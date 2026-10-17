#!/usr/bin/env python3
"""Copy locked same-origin browser dependencies without executing/bundling them."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import tempfile

ROOT = Path(__file__).resolve().parents[1]
PACKAGES = {"@duckdb/duckdb-wasm": "duckdb", "apache-arrow": "apache-arrow", "flatbuffers": "flatbuffers", "tslib": "tslib"}
REQUIRED_LICENSES = {"apache-arrow": ("LICENSE.txt", "NOTICE.txt"), "flatbuffers": ("LICENSE",), "tslib": ("LICENSE.txt", "CopyrightNotice.txt")}
LICENSE_NAME = re.compile(r"(?:licen[cs]e|notice|copying|copyright(?:notice)?)(?:\.[a-z0-9_-]+)?$", re.IGNORECASE)


class VendorError(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise VendorError(message)


def regular(path):
    info = path.lstat()
    require(stat.S_ISREG(info.st_mode) and 0 < info.st_size <= 128 * 1024 * 1024, "Expected ordinary nonempty vendor file of at most 128 MiB: " + str(path))
    return info


def read_json(path):
    require(regular(path).st_size <= 2 * 1024 * 1024, "Oversized dependency metadata: " + str(path))
    value = json.loads(path.read_text())
    require(isinstance(value, dict), "Dependency metadata must be an object: " + str(path))
    return value


def digest(path, output=None):
    before = regular(path)
    result, size = hashlib.sha256(), 0
    with path.open("rb") as source:
        for data in iter(lambda: source.read(1024 * 1024), b""):
            result.update(data); size += len(data)
            if output is not None:
                output.write(data)
        after = os.fstat(source.fileno())
    require(size == before.st_size == after.st_size and before.st_mtime_ns == after.st_mtime_ns, "Vendor input changed during packaging: " + str(path))
    return {"sha256": result.hexdigest(), "bytes": size}


def inventory(root):
    lock = read_json(root / "web/package-lock.json")
    pins = read_json(root / "web/toolchain.lock.json")
    require(pins.get("schema") == "duxcel.web-toolchain" and pins.get("version") == 1, "Unsupported toolchain pins")
    files, packages, missing = {}, {}, []
    installed = root / "web/node_modules"
    for name, folder in PACKAGES.items():
        path = installed / name
        require(path.is_dir() and not path.is_symlink(), "Missing ordinary installed package: " + name)
        package = read_json(path / "package.json")
        entry = lock.get("packages", {}).get("node_modules/" + name, {})
        version = entry.get("version", "")
        require(re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", version) and package.get("name") == name and package.get("version") == version, "Installed package version disagrees with package-lock: " + name)
        expected_url = "https://registry.npmjs.org/" + name + "/-/" + name.rsplit("/", 1)[-1] + "-" + version + ".tgz"
        require(entry.get("resolved") == expected_url and isinstance(entry.get("integrity"), str) and entry["integrity"].startswith("sha512-"), "Missing official locked package provenance: " + name)
        if name in ("@duckdb/duckdb-wasm", "apache-arrow"):
            pin = pins.get("browser_packages", {}).get(name, {})
            require(pin.get("version") == version and pin.get("integrity") == entry["integrity"] and pin.get("tarball") == expected_url, "Package-lock disagrees with toolchain pins: " + name)
        if name == "@duckdb/duckdb-wasm":
            selected = [path / "dist" / filename for filename in ("duckdb-browser.mjs", "duckdb-browser-eh.worker.js", "duckdb-eh.wasm")]
        elif name == "apache-arrow":
            selected = sorted(path.rglob("*.mjs"))
            require(path / "Arrow.dom.mjs" in selected, "Missing Arrow browser entry")
        elif name == "flatbuffers":
            selected = sorted((path / "mjs").rglob("*.js"))
            require(path / "mjs/flatbuffers.js" in selected, "Missing flatbuffers ESM entry")
        else:
            selected = [path / "tslib.es6.mjs"]
        licenses = sorted(file for file in path.iterdir() if LICENSE_NAME.fullmatch(file.name))
        for required in REQUIRED_LICENSES.get(name, ()):
            require(path / required in licenses, "Missing supplied package license/notice: " + name + "/" + required)
        if not licenses:
            missing.append(name)  # official DuckDB-Wasm 1.32.0 npm package supplies no standalone license
        for file in selected + licenses:
            relative = file.relative_to(path)
            parent = path
            for component in relative.parts[:-1]:
                parent /= component
                require(not parent.is_symlink(), "Vendor directories cannot redirect through symlinks: " + str(parent))
            regular(file)
            files[(Path("vendor") / folder / relative).as_posix()] = file
        packages[name] = {"version": version, "integrity": entry["integrity"], "license": package.get("license"),
                          "license_files": [(Path("vendor") / folder / file.name).as_posix() for file in licenses]}
    return files, packages, missing


def package_vendor(root: Path, destination: Path):
    """Publish destination/vendor; return exact assets, hashes, sizes and import map.

    Does no downloads, rewriting, compilation, npm work, app or engine execution.
    An identical vendor subtree is reusable; a different existing one is retained
    and rejected. The integration owner serializes publication of build folders.
    """
    root, destination = Path(root).resolve(), Path(destination).resolve()
    files, packages, missing = inventory(root)
    vendor = destination / "vendor"
    inputs = root / "web/node_modules"
    require(not vendor.is_relative_to(inputs) and not inputs.is_relative_to(vendor), "Vendor output must not overlap installed dependencies")
    require(not vendor.is_symlink(), "Vendor output cannot be a symlink")
    destination.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".vendor-", dir=destination) as directory:
        stage = Path(directory)
        manifest = {}
        for relative, source in sorted(files.items()):
            target = stage / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            with target.open("xb") as output:
                manifest[relative] = digest(source, output)
        if vendor.exists():
            existing = {}
            for path in vendor.rglob("*"):
                require(not path.is_symlink(), "Existing vendor output contains a symlink")
                if not path.is_dir():
                    existing[path.relative_to(destination).as_posix()] = digest(path)
            require(existing == manifest, "Existing vendor output differs; use a new build destination")
        else:
            (stage / "vendor").rename(vendor)
    browser = "./vendor/duckdb/dist/duckdb-browser.mjs"
    imports = {"apache-arrow": "./vendor/apache-arrow/Arrow.dom.mjs",
               "flatbuffers": "./vendor/flatbuffers/mjs/flatbuffers.js", "tslib": "./vendor/tslib/tslib.es6.mjs",
               "@duckdb/duckdb-wasm": browser, "@duckdb/duckdb-wasm/dist/duckdb-browser.mjs": browser}
    return {"schema": "duxcel.web-vendor", "version": 1, "packages": packages, "files": manifest,
            "bytes": sum(item["bytes"] for item in manifest.values()), "import_map": {"imports": imports},
            "worker": {"moduleURL": "./vendor/duckdb/dist/duckdb-eh.wasm", "workerURL": "./vendor/duckdb/dist/duckdb-browser-eh.worker.js"},
            "missing_standalone_licenses": missing}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("destination", type=Path)
    parser.add_argument("--root", type=Path, default=ROOT)
    args = parser.parse_args()
    try:
        print(json.dumps(package_vendor(args.root, args.destination), indent=2))
        return 0
    except (OSError, ValueError, KeyError) as error:
        print(json.dumps({"error": str(error)}))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
