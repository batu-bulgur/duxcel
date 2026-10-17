#!/usr/bin/env python3
"""Exercise the real Make build graph in an isolated, tiny C99 checkout."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    checks = 0
    started = time.monotonic()

    def check(condition, message):
        nonlocal checks
        checks += 1
        if not condition:
            raise AssertionError(message)

    with tempfile.TemporaryDirectory(prefix="duxcel-build-contract-") as temporary:
        tree = Path(temporary)
        for directory in ("src", "tools", "generated", "inputs"):
            (tree / directory).mkdir()
        shutil.copyfile(ROOT / "Makefile", tree / "Makefile")
        (tree / "src/common.h").write_text("int answer(void);\n")
        original = '#include "common.h"\n#include "brand.h"\nint answer(void) { return BRAND_INT; }\n'
        (tree / "src/first.c").write_text(original)
        (tree / "src/main.c").write_text('#include "common.h"\nint main(void) { return answer()!=17; }\n')
        (tree / "tools/create_design_workbook.c").write_text('#include "common.h"\nint main(void) { return answer()!=17; }\n')
        (tree / "inputs/source.svg").write_text("#define BRAND_INT 17\n")
        (tree / "generated/brand.h").write_text("#define BRAND_INT 17\n")
        (tree / "generated/icon.bin").write_text("#define BRAND_INT 17\n")
        (tree / "tools/generate_icons.py").write_text('''import pathlib, sys
root=pathlib.Path('.')
expected=(root/'inputs/source.svg').read_bytes()
outputs=[root/'generated/brand.h',root/'generated/icon.bin']
if '--check' in sys.argv:
    sys.exit(0 if all(p.is_file() and p.read_bytes()==expected for p in outputs) else 1)
with (root/'generations.log').open('a') as log: log.write('generated\\n')
for p in outputs: p.write_bytes(expected)
''')
        (tree / "tools/compiler.py").write_text('''import fcntl, json, pathlib, shutil, subprocess, sys
with pathlib.Path('compiler.lock').open('a') as lock:
    fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
    with pathlib.Path('compiler.jsonl').open('a') as log:
        log.write(json.dumps(sys.argv[1:])+'\\n')
    sys.exit(subprocess.call([shutil.which('clang'),*sys.argv[1:]]))
''')
        settings = ["BREW_PREFIX=", "RAYLIB_CFLAGS=", "RAYLIB_LIBS=", "DUCKDB_CFLAGS=-Igenerated -Isrc",
                    "DUCKDB_LIBS=-lm", "CPPFLAGS=-Igenerated -Isrc", "LDLIBS=-lm", "THREAD_LIBS=",
                    "CC=python3 tools/compiler.py", "SRC=src/first.c src/main.c", "CORE=src/first.c",
                    "ICON_INPUTS=inputs/source.svg tools/generate_icons.py",
                    "ICON_OUTPUTS=generated/brand.h generated/icon.bin"]

        def commands():
            log = tree / "compiler.jsonl"
            return [json.loads(line) for line in log.read_text().splitlines()] if log.exists() else []

        previous_options = {}

        def next_timestamp():
            # Apple's bundled Make 3.81 compares whole-second mtimes. Keep
            # deliberate fixture edits/flag changes distinct from the last
            # artifact rather than accepting a false incremental-test pass.
            time.sleep(1.02 - time.time() % 1)

        def edit(path, text):
            next_timestamp()
            path.write_text(text)

        def build(target="duxcel", options=(), success=True):
            if target in previous_options and tuple(options) != previous_options[target]:
                next_timestamp()
            previous_options[target] = tuple(options)
            before = len(commands())
            result = subprocess.run(["make", "-j4", target, *settings, *options], cwd=tree,
                                    text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            check((result.returncode == 0) == success, result.stdout)
            calls = commands()[before:]
            return [call[call.index("-c") + 1] for call in calls if "-c" in call], sum("-c" not in call for call in calls)

        compiled, links = build()
        check(sorted(compiled) == ["src/first.c", "src/main.c"] and links == 1, "cold app compilation")
        check(not (tree / "generations.log").exists(), "valid checked-in icons must not regenerate")
        check(build() == ([], 0), "warm app must not invoke the compiler/linker")
        check((tree / "build/obj/release/app/src/first.d").is_file(), "compiler dependency output")
        (tree / "build/obj/release/app/src/first.d").unlink()
        check(build() == (["src/first.c"], 1), "missing dependency sidecars must be recreated")
        check(build() == ([], 0), "repaired dependency graph returns to a warm no-op")

        edit(tree / "src/unrelated.h", "/* unrelated */\n")
        check(build() == ([], 0), "unrelated headers must not invalidate the application")
        edit(tree / "src/first.c", original + "/* source edit */\n")
        state = build()
        check(state == (["src/first.c"], 1), "only the changed translation unit should compile: " + repr(state))
        edit(tree / "src/common.h", "int answer(void); /* shared header edit */\n")
        compiled, links = build()
        check(sorted(compiled) == ["src/first.c", "src/main.c"] and links == 1, "shared header dependencies")

        compiled, links = build(options=["CFLAGS=-O0 -g"])
        check(len(compiled) == 2 and links == 1, "compile flags must invalidate their objects")
        check(build(options=["CFLAGS=-O0 -g"]) == ([], 0), "unchanged compile flags are a no-op")
        check(build(options=["CFLAGS=-O0 -g", "LDFLAGS=-lm"]) == ([], 1), "link flags must only relink")
        compiled, links = build("duxcel-debug")
        check(len(compiled) == 2 and links == 1, "independent sanitizer objects")
        check((tree / "build/obj/sanitize/app/src/first.o").is_file(), "sanitizer object path")
        check(build(options=["CFLAGS=-O0 -g", "LDFLAGS=-lm"]) == ([], 0), "sanitizers must not alter release objects")
        compiled, links = build("build/create-design-workbook")
        check(sorted(compiled) == ["src/first.c", "tools/create_design_workbook.c"] and links == 1,
              "helper must not reuse application objects with different include flags")
        check(build("build/create-design-workbook") == ([], 0), "warm helper build")

        before = digest(tree / "duxcel")
        first_object = tree / "build/obj/release/app/src/first.o"
        old_object = digest(first_object)
        edit(tree / "src/first.c", "invalid C for a deliberate failure;\n")
        build(options=["CFLAGS=-O0 -g", "LDFLAGS=-lm"], success=False)
        check(digest(tree / "duxcel") == before and digest(first_object) == old_object,
              "failed compilation must retain the old binary and object")
        check(not list(tree.rglob("*.new")), "failed compilation temporary files are removed")
        edit(tree / "src/first.c", original)
        build(options=["CFLAGS=-O0 -g", "LDFLAGS=-lm"])
        before = digest(tree / "duxcel")
        build(options=["CFLAGS=-O0 -g", "LDFLAGS=-Wl,--duxcel-intentional-build-failure"], success=False)
        check(digest(tree / "duxcel") == before and not (tree / "duxcel.new").exists(),
              "failed linking must retain the accepted binary and remove its temporary output")

        for name in ("brand.h", "icon.bin"):
            (tree / "generated" / name).unlink()
        build(options=["CFLAGS=-O0 -g", "LDFLAGS=-lm"])
        check((tree / "generations.log").read_text().splitlines() == ["generated"],
              "multiple missing derivatives must cause one coordinated regeneration")
        edit(tree / "inputs/source.svg", "#define BRAND_INT 17\n/* canonical edit */\n")
        compiled, links = build(options=["CFLAGS=-O0 -g", "LDFLAGS=-lm"])
        check(compiled == ["src/first.c"] and links == 1, "generated header changes rebuild its actual consumer")
        check(len((tree / "generations.log").read_text().splitlines()) == 2, "canonical change regenerates once")
        check(build(options=["CFLAGS=-O0 -g", "LDFLAGS=-lm"]) == ([], 0), "repaired icon graph returns to a warm no-op")
        subprocess.run([str(tree / "duxcel")], cwd=tree, check=True)
    print(f"PASS: {checks} incremental build checks in {time.monotonic()-started:.3f}s")


if __name__ == "__main__":
    main()
