#!/usr/bin/env python3
"""Vendor packaging fixtures; no npm, compiler, browser or engine."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

SPEC = importlib.util.spec_from_file_location("package_web_vendor", Path(__file__).with_name("package_web_vendor.py"))
vendor = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(vendor)


class VendorTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="duxcel-web-vendor-test-")
        self.addCleanup(self.temporary.cleanup)
        self.base = Path(self.temporary.name).resolve()
        self.root, self.destination = self.base / "project", self.base / "output"
        self.web = self.root / "web"
        versions = {"@duckdb/duckdb-wasm": "1.32.0", "apache-arrow": "17.0.0", "flatbuffers": "24.12.23", "tslib": "2.8.1"}
        packages, pins = {}, {}
        for name, version in versions.items():
            path = self.web / "node_modules" / name
            self.write(path / "package.json", json.dumps({"name": name, "version": version, "license": "MIT"}))
            entry = {"version": version, "integrity": "sha512-" + "A" * 86 + "==",
                     "resolved": "https://registry.npmjs.org/" + name + "/-/" + name.rsplit("/", 1)[-1] + "-" + version + ".tgz"}
            packages["node_modules/" + name] = entry
            if name in ("@duckdb/duckdb-wasm", "apache-arrow"):
                pins[name] = {"version": version, "integrity": entry["integrity"], "tarball": entry["resolved"]}
            for required in vendor.REQUIRED_LICENSES.get(name, ()):
                self.write(path / required, "fixture license/notice")
        self.write(self.web / "package-lock.json", json.dumps({"packages": packages}))
        self.write(self.web / "toolchain.lock.json", json.dumps({"schema": "duxcel.web-toolchain", "version": 1, "browser_packages": pins}))
        self.sources = {
            "@duckdb/duckdb-wasm/dist/duckdb-browser.mjs": 'import * as arrow from "apache-arrow";',
            "@duckdb/duckdb-wasm/dist/duckdb-browser-mvp.worker.js": "// official worker fixture",
            "@duckdb/duckdb-wasm/dist/duckdb-mvp.wasm": "fixture Wasm bytes",
            "apache-arrow/Arrow.dom.mjs": 'export * from "./sub/child.mjs";',
            "apache-arrow/sub/child.mjs": 'import * as f from "flatbuffers"; import { __extends } from "tslib";',
            "apache-arrow/Arrow.node.mjs": "// unused node entry, retained .mjs tree",
            "flatbuffers/mjs/flatbuffers.js": 'export * from "./constants.js";',
            "flatbuffers/mjs/constants.js": "export const SIZEOF_INT=4;",
            "tslib/tslib.es6.mjs": "export const __extends=()=>{};",
        }
        for path, text in self.sources.items():
            self.write(self.web / "node_modules" / path, text)
        for path in ("@duckdb/duckdb-wasm/dist/duckdb-coi.wasm", "@duckdb/duckdb-wasm/dist/duckdb-eh.wasm",
                     "@duckdb/duckdb-wasm/dist/duckdb-node.cjs", "apache-arrow/sub/child.mjs.map",
                     "apache-arrow/sub/child.d.ts", "flatbuffers/js/node.js", "tslib/tslib.js"):
            self.write(self.web / "node_modules" / path, "not browser distribution input")

    def write(self, path, text):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)

    def test_whitelist_preserves_paths_hashes_licenses_and_import_targets(self):
        result = vendor.package_vendor(self.root, self.destination)
        files = result["files"]
        for path, entry in files.items():
            data = (self.destination / path).read_bytes()
            self.assertEqual(entry, {"sha256": hashlib.sha256(data).hexdigest(), "bytes": len(data)})
            self.assertFalse(path.endswith((".map", ".d.ts", ".cjs")))
            self.assertNotIn("-coi", path)
            self.assertNotIn("-eh", path)
        self.assertEqual(result["bytes"], sum(item["bytes"] for item in files.values()))
        self.assertIn("vendor/apache-arrow/sub/child.mjs", files)
        self.assertIn("vendor/flatbuffers/mjs/constants.js", files)
        self.assertIn("vendor/tslib/CopyrightNotice.txt", files)
        self.assertEqual(result["missing_standalone_licenses"], ["@duckdb/duckdb-wasm"])
        for target in result["import_map"]["imports"].values():
            self.assertTrue((self.destination / target).is_file())
        for target in result["worker"].values():
            self.assertTrue((self.destination / target).is_file())

    def test_mismatched_installed_and_direct_pin_versions_fail_before_publication(self):
        package = self.web / "node_modules/apache-arrow/package.json"
        body = json.loads(package.read_text()); body["version"] = "18.0.0"
        package.write_text(json.dumps(body))
        with self.assertRaisesRegex(vendor.VendorError, "package-lock"):
            vendor.package_vendor(self.root, self.destination)
        self.assertFalse(self.destination.exists())
        body["version"] = "17.0.0"; package.write_text(json.dumps(body))
        pins = self.web / "toolchain.lock.json"
        body = json.loads(pins.read_text()); body["browser_packages"]["apache-arrow"]["integrity"] = "wrong"
        pins.write_text(json.dumps(body))
        with self.assertRaisesRegex(vendor.VendorError, "toolchain pins"):
            vendor.package_vendor(self.root, self.destination)

    def test_missing_required_asset_or_license_never_publishes(self):
        asset = self.web / "node_modules/@duckdb/duckdb-wasm/dist/duckdb-mvp.wasm"
        asset.unlink()
        with self.assertRaises(OSError):
            vendor.package_vendor(self.root, self.destination)
        self.write(asset, "restored")
        (self.web / "node_modules/flatbuffers/LICENSE").unlink()
        with self.assertRaisesRegex(vendor.VendorError, "license/notice"):
            vendor.package_vendor(self.root, self.destination)
        self.assertFalse(self.destination.exists())

    def test_identical_output_is_reusable_but_different_output_is_retained(self):
        first = vendor.package_vendor(self.root, self.destination)
        self.assertEqual(vendor.package_vendor(self.root, self.destination), first)
        keep = self.destination / "vendor/unowned.txt"
        keep.write_text("keep existing output")
        with self.assertRaisesRegex(vendor.VendorError, "Existing vendor output differs"):
            vendor.package_vendor(self.root, self.destination)
        self.assertEqual(keep.read_text(), "keep existing output")
        self.assertEqual(list(self.destination.glob(".vendor-*")), [])

    def test_source_and_destination_symlinks_and_source_overlap_are_rejected(self):
        outside = self.base / "outside"
        outside.write_text("outside")
        asset = self.web / "node_modules/@duckdb/duckdb-wasm/dist/duckdb-mvp.wasm"
        asset.unlink(); asset.symlink_to(outside)
        with self.assertRaises(vendor.VendorError):
            vendor.package_vendor(self.root, self.destination)
        asset.unlink(); self.write(asset, "restored")
        self.destination.mkdir()
        (self.destination / "vendor").symlink_to(self.web)
        with self.assertRaises(vendor.VendorError):
            vendor.package_vendor(self.root, self.destination)
        with self.assertRaisesRegex(vendor.VendorError, "overlap"):
            vendor.package_vendor(self.root, self.web / "node_modules")

    def test_copy_failure_cleans_only_its_stage_and_preserves_other_build_files(self):
        self.destination.mkdir()
        keep = self.destination / "duxcel.wasm"
        keep.write_text("accepted shared renderer")
        with mock.patch.object(vendor, "digest", side_effect=OSError("disk full")), self.assertRaises(OSError):
            vendor.package_vendor(self.root, self.destination)
        self.assertEqual(keep.read_text(), "accepted shared renderer")
        self.assertFalse((self.destination / "vendor").exists())
        self.assertEqual(list(self.destination.glob(".vendor-*")), [])


if __name__ == "__main__":
    unittest.main(verbosity=2)
