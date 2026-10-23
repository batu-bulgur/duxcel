#!/usr/bin/env python3
"""Exercise the real Make build graph in an isolated, tiny C99 checkout."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[2]


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
        for directory in ("src", "tools", "tests", "generated", "inputs", "assets"):
            (tree / directory).mkdir(parents=True)
        shutil.copyfile(ROOT / "Makefile", tree / "Makefile")
        (tree / "src/common.h").write_text("int answer(void);\n")
        original = '#include "common.h"\n#include "brand.h"\nint answer(void) { return BRAND_INT; }\n'
        (tree / "src/first.c").write_text(original)
        (tree / "src/main.c").write_text('#include "common.h"\nint main(void) { return answer()!=17; }\n')
        (tree / "tests/test_fixture.c").write_text('#include "common.h"\nint main(void) { return answer()!=17; }\n')
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
        (tree / "tools/compiler.py").write_text('''import fcntl, json, os, pathlib, shutil, subprocess, sys, time
state_path=pathlib.Path('compiler-state.json')
def activity(delta):
    with pathlib.Path('compiler.lock').open('a') as lock:
        fcntl.flock(lock,fcntl.LOCK_EX)
        state=json.loads(state_path.read_text()) if state_path.exists() else dict(active=0,peak=0,started=0)
        state['active']+=delta
        if delta>0:
            state['started']+=1
            state['peak']=max(state['peak'],state['active'])
            with pathlib.Path('compiler.jsonl').open('a') as log:
                log.write(json.dumps(sys.argv[1:])+'\\n')
        state_path.write_text(json.dumps(state))
        return state
activity(1)
try:
    # A bounded barrier proves eight simultaneous compiler invocations rather
    # than merely observing -j8 in command text or relying on timing luck.
    expected=int(os.environ.get('DUXCEL_BUILD_BARRIER','0'))
    deadline=time.monotonic()+15
    while expected and activity(0)['started']<expected:
        if time.monotonic()>deadline: raise RuntimeError('parallel compiler barrier timed out')
        time.sleep(.01)
    result=subprocess.call([shutil.which('clang'),*sys.argv[1:]])
finally:
    activity(-1)
sys.exit(result)
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

        def build(target="build/bin/duxcel", options=(), success=True, env=None):
            if target in previous_options and tuple(options) != previous_options[target]:
                next_timestamp()
            previous_options[target] = tuple(options)
            before = len(commands())
            targets = [target] if isinstance(target, str) else list(target)
            result = subprocess.run(["make", *targets, *settings, *options], cwd=tree, env=env,
                                    text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=60)
            check((result.returncode == 0) == success, result.stdout)
            calls = commands()[before:]
            return [call[call.index("-c") + 1] for call in calls if "-c" in call], sum("-c" not in call for call in calls)

        probe_sources = [f"src/parallel_{index}.c" for index in range(8)]
        probe_targets = tuple("build/obj/release/app/" + source[:-2] + ".o" for source in probe_sources)
        for index, source in enumerate(probe_sources):
            (tree / source).write_text(f"int parallel_{index}(void) {{ return {index}; }}\n")
        compiled, links = build(probe_targets, env=dict(os.environ, DUXCEL_BUILD_BARRIER="8"))
        check(sorted(compiled) == probe_sources and links == 0, "parallel object targets compile their own sources")
        activity = json.loads((tree / "compiler-state.json").read_text())
        check(activity["peak"] == 8 and activity["active"] == 0, "ordinary make must admit eight compiler jobs")

        compiled, links = build()
        check(sorted(compiled) == ["src/first.c", "src/main.c"] and links == 1, "cold app compilation")
        check(not (tree / "duxcel").exists() and not (tree / "duxcel-debug").exists(), "native outputs never occupy the source root")
        check((tree / "build/bin/assets").resolve() == (tree / "assets").resolve(), "native binaries retain executable-relative runtime assets")
        check(not (tree / "generations.log").exists(), "valid checked-in icons must not regenerate")
        check(build() == ([], 0), "warm app must not invoke the compiler/linker")
        check((tree / "build/obj/release/app/src/first.d").is_file(), "compiler dependency output")
        next_timestamp()  # Sidecar removal is a deliberate edit on Make 3.81 too.
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
        compiled, links = build("build/bin/duxcel-debug")
        check(len(compiled) == 2 and links == 1, "independent sanitizer objects")
        check((tree / "build/obj/sanitize/app/src/first.o").is_file(), "sanitizer object path")
        check(build(options=["CFLAGS=-O0 -g", "LDFLAGS=-lm"]) == ([], 0), "sanitizers must not alter release objects")
        accepted = {name: digest(tree / name) for name in ("build/bin/duxcel", "build/bin/duxcel-debug")}
        edit_target = "build/edit-sanitize/duxcel"
        compiled, links = build(edit_target)
        check(sorted(compiled) == ["src/first.c", "src/main.c"] and links == 1, "independent edit sanitizer objects")
        edit_object = tree / "build/obj/edit-sanitize/app/src/first.o"
        check(edit_object.is_file() and (tree / "build/edit-sanitize/assets").resolve() == (tree / "assets").resolve(), "edit objects and executable-relative assets")
        edit_calls = [call for call in commands() if "-c" in call and "build/obj/edit-sanitize/" in call[call.index("-o") + 1]]
        check(len(edit_calls) == 2 and all("-O0" in call and "-fsanitize=address,undefined" in call for call in edit_calls), "edit compilation keeps both sanitizers at O0")
        subprocess.run([str(tree / edit_target)], cwd=tree, check=True)
        check(build(edit_target) == ([], 0), "warm edit build")
        next_timestamp()
        (tree / "build/obj/edit-sanitize/app/src/first.d").unlink()
        check(build(edit_target) == (["src/first.c"], 1), "edit missing dependency recovery")
        edit_before, edit_object_before = digest(tree / edit_target), digest(edit_object)
        build(edit_target, options=["APP_EDIT_FLAGS=-invalid-duxcel-edit-flag"], success=False)
        check(digest(tree / edit_target) == edit_before and digest(edit_object) == edit_object_before, "failed edit compile preserves accepted output")
        check(not list(tree.rglob("*.new")), "failed edit compilation cleans private temporary files")
        check(build(edit_target)[1] == 1, "edit flag recovery relinks only after successful compilation")
        check(all(digest(tree / name) == value for name, value in accepted.items()), "edit mode never replaces release or O1 sanitizer artifacts")
        compiled, links = build("build/tests/test_fixture")
        check(sorted(compiled) == ["src/first.c", "tests/test_fixture.c"] and links == 1,
              "helper must not reuse application objects with different include flags")
        check(build("build/tests/test_fixture") == ([], 0), "warm helper build")

        before = digest(tree / "build/bin/duxcel")
        first_object = tree / "build/obj/release/app/src/first.o"
        old_object = digest(first_object)
        edit(tree / "src/first.c", "invalid C for a deliberate failure;\n")
        build(options=["CFLAGS=-O0 -g", "LDFLAGS=-lm"], success=False)
        check(digest(tree / "build/bin/duxcel") == before and digest(first_object) == old_object,
              "failed compilation must retain the old binary and object")
        check(not list(tree.rglob("*.new")), "failed compilation temporary files are removed")
        edit(tree / "src/first.c", original)
        build(options=["CFLAGS=-O0 -g", "LDFLAGS=-lm"])
        before = digest(tree / "build/bin/duxcel")
        build(options=["CFLAGS=-O0 -g", "LDFLAGS=-Wl,--duxcel-intentional-build-failure"], success=False)
        check(digest(tree / "build/bin/duxcel") == before and not (tree / "build/bin/duxcel.new").exists(),
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
        subprocess.run([str(tree / "build/bin/duxcel")], cwd=tree, check=True)

        for target in probe_targets:
            (tree / target).unlink()
        (tree / "compiler-state.json").unlink()
        compiled, links = build(probe_targets, options=["BUILD_JOBS=1"])
        check(len(compiled) == 8 and links == 0, "serial budget still compiles every object")
        check(json.loads((tree / "compiler-state.json").read_text())["peak"] == 1, "BUILD_JOBS override controls actual concurrency")

        # Legacy fixture/execution goals must retain serialization even though
        # their admitted object prerequisites can be compiled in parallel alone.
        with (tree / "Makefile").open("a") as makefile:
            makefile.write("\nserial-probe: " + " ".join(probe_targets) + "\n")
        for target in probe_targets:
            (tree / target).unlink()
        (tree / "compiler-state.json").unlink()
        compiled, links = build("serial-probe")
        check(len(compiled) == 8 and links == 0, "serial execution goals retain their prerequisites")
        check(json.loads((tree / "compiler-state.json").read_text())["peak"] == 1, "legacy execution graphs remain serial")

        build()  # The application is warm before probing inherited job slots.
        with (tree / "Makefile").open("a") as makefile:
            makefile.write("\nall: recursive-probe\nrecursive-probe:\n\t+$(MAKE) BUILD_JOBS=8 " + " ".join(probe_targets) + "\n")
        for target in probe_targets:
            (tree / target).unlink()
        (tree / "compiler-state.json").unlink()
        compiled, links = build("all", options=["BUILD_JOBS=2"], env=dict(os.environ, DUXCEL_BUILD_BARRIER="2"))
        check(len(compiled) == 8 and links == 0, "recursive builds complete their object inventory")
        check(json.loads((tree / "compiler-state.json").read_text())["peak"] == 2,
              "recursive make shares the parent jobserver even when its local budget is eight")
        # The real Python runner must preserve the inherited jobserver through
        # its Make subprocess, rather than silently degrading to one compiler.
        (tree / "tools/python_make.py").write_text(
            "import subprocess,sys\n"+
            "sys.path.insert(0,"+repr(str(ROOT / "tools"))+")\n"+
            "from workspace import make_jobserver_fds\n"+
            "subprocess.run(['make',*sys.argv[1:]],check=True,pass_fds=make_jobserver_fds())\n")
        makefile=(tree / "Makefile").read_text().replace("all: recursive-probe", "")
        makefile+="\nall: python-probe\npython-probe:\n\t+python3 tools/python_make.py BUILD_JOBS=8 "+" ".join(probe_targets)+"\n"
        (tree / "Makefile").write_text(makefile)
        for target in probe_targets:
            (tree / target).unlink()
        (tree / "compiler-state.json").unlink()
        compiled,links=build("all",options=["BUILD_JOBS=2"],env=dict(os.environ,DUXCEL_BUILD_BARRIER="2"))
        check(len(compiled)==8 and links==0,"Python recursive builds complete their object inventory")
        check(json.loads((tree / "compiler-state.json").read_text())["peak"]==2,
              "Python runner passes the parent two-slot jobserver without serial fallback or oversubscription")
        # The workbook archive suite shares native engine objects, with only
        # actual headers in its dependency closure rather than every UI header.
        build()
        (tree / "tests/test_workbook.c").write_text('#include "common.h"\nint main(void) { return answer()!=17; }\n')
        compiled,links=build("build/tests/test_workbook")
        check(compiled==["tests/test_workbook.c"] and links==1,"workbook test reuses warm native engine objects")
        edit(tree / "src/unrelated.h","#define UNRELATED_UI_VALUE 1\n")
        check(build("build/tests/test_workbook")==([],0),"unrelated UI headers do not rebuild workbook tests")
        edit(tree / "tests/test_workbook.c",'#include "common.h"\nint main(void) { return answer()!=17; }\n/* local assertion edit */\n')
        compiled,links=build("build/tests/test_workbook")
        check(compiled==["tests/test_workbook.c"] and links==1,"workbook assertion changes rebuild only their test translation unit")
        edit(tree / "src/common.h","int answer(void);\n/* actual shared dependency */\n")
        compiled,links=build("build/tests/test_workbook")
        check(set(compiled)=={"src/first.c","tests/test_workbook.c"} and links==1,"real workbook dependencies still invalidate both consumers")
        subprocess.run([str(tree / "build/tests/test_workbook")],cwd=tree,check=True)
        (tree / "tests/test_core.c").write_text('#include "common.h"\nint main(void) { return answer()!=17; }\n')
        compiled,links=build("build/tests/test_core")
        check(compiled==["tests/test_core.c"] and links==1,"core fixture/assertion suite also reuses warm native engine objects")
        edit(tree / "src/unrelated.h","#define UNRELATED_UI_VALUE 2\n")
        check(build("build/tests/test_core")==([],0),"unrelated UI headers do not rebuild core engine tests")
        subprocess.run([str(tree / "build/tests/test_core")],cwd=tree,check=True)
    print(f"PASS: {checks} incremental build checks in {time.monotonic()-started:.3f}s")


if __name__ == "__main__":
    main()
