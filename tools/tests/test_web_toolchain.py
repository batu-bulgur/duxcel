#!/usr/bin/env python3
"""Offline toolchain-bootstrap contracts; no real downloads, compiler or browser."""
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import subprocess
import tarfile
import tempfile
import unittest
from unittest import mock

SPEC = importlib.util.spec_from_file_location("web_toolchain", Path(__file__).resolve().parents[1].joinpath("web_toolchain.py"))
tool = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(tool)


class ToolchainTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="duxcel-web-tools-test-")
        self.addCleanup(self.temporary.cleanup)
        self.project = Path(self.temporary.name).resolve()
        self.root = self.project / ".cache/web/toolchain"
        self.lock = tool.load_lock(tool.PROJECT / "web/toolchain.lock.json")
        self.state = {"python": {"path": "/existing/python", "version": "3.13.15"},
                      "node": {"path": "/existing/node", "version": "26.7.0"}}

    def archive(self, path, files, extra=None):
        path.parent.mkdir(parents=True, exist_ok=True)
        with tarfile.open(path, "w:gz") as output:
            for name, text in files.items():
                payload = text.encode()
                member = tarfile.TarInfo("source/" + name)
                member.size, member.mode = len(payload), 0o644
                output.addfile(member, io.BytesIO(payload))
            if extra:
                output.addfile(extra)

    def cached_fixture(self):
        for name in ("emsdk", "raylib"):
            item = self.lock[name]
            path = self.root / "downloads" / (name + "-" + item["commit"] + ".tar.gz")
            files = {"emsdk.py": "# frozen fixture", "emscripten-releases-tags.json": json.dumps({"releases": {item["version"]: item["release_hash"]}})} if name == "emsdk" else {"src/raylib.h": '#define RAYLIB_VERSION "6.0"\n'}
            self.archive(path, files)
            item["sha256"] = tool.sha256(path)
        artifact = self.lock["emsdk"]["artifacts"]["darwin-arm64"]
        path = self.root / "downloads" / artifact["filename"]
        path.write_bytes(b"verified compiler archive fixture")
        artifact["sha256"] = tool.sha256(path)

    def fake_command(self, arguments, env=None, **kwargs):
        if "install" in arguments:
            self.assertEqual(arguments[-1], "releases-" + self.lock["emsdk"]["release_hash"] + "-64bit")
            self.assertNotIn("activate", arguments)
            path = self.root / "emsdk/upstream/emscripten/emcc.py"
            path.parent.mkdir(parents=True)
            path.write_text("# installed compiler fixture")
            return "Installed compiler-only fixture\n"
        self.assertEqual(arguments[-1], "--version")
        return "emcc (Emscripten gcc/clang-like replacement) 6.0.12 (fixture)\n"

    @mock.patch.object(tool.platform, "system", return_value="Darwin")
    @mock.patch.object(tool.platform, "machine", return_value="arm64")
    def install_fixture(self, *mocks):
        with mock.patch.object(tool, "runtime", side_effect=[self.state["python"], self.state["node"]]), \
                mock.patch.object(tool, "command", side_effect=self.fake_command), \
                mock.patch.object(tool.urllib.request, "urlopen", side_effect=AssertionError("network forbidden")):
            return tool.bootstrap(self.root, self.lock, "fixture-lock")

    def test_checked_in_lock_has_immutable_official_pins(self):
        self.assertEqual(self.lock["emsdk"]["version"], "6.0.12")
        self.assertEqual(self.lock["raylib"]["version"], "6.0")
        self.assertEqual(self.lock["browser_packages"]["@duckdb/duckdb-wasm"]["version"], "1.32.0")
        for edit in ({"version": True}, {"emsdk": dict(self.lock["emsdk"], version="latest")},
                     {"raylib": dict(self.lock["raylib"], commit="../escape")},
                     {"raylib": dict(self.lock["raylib"], repository="unofficial/fork")}):
            path = self.project / "bad-lock.json"
            path.write_text(json.dumps(dict(self.lock, **edit)))
            with self.subTest(edit=list(edit)), self.assertRaises(tool.ToolchainError):
                tool.load_lock(path)

    def test_root_rejects_external_parent_traversal_and_symlink(self):
        self.assertEqual(tool.local_root(self.project), self.root)
        self.assertEqual(tool.local_root(self.project, ".cache/web/other"), self.project / ".cache/web/other")
        for path in (self.project / ".cache/web", self.project / ".cache/web/../external", self.project / "outside"):
            with self.subTest(path=path), self.assertRaises(tool.ToolchainError):
                tool.local_root(self.project, path)
        (self.project / ".cache").symlink_to(self.project / "external", target_is_directory=True)
        with self.assertRaises(tool.ToolchainError):
            tool.local_root(self.project)

    def test_cached_offline_fetch_checks_integrity_and_never_uses_network(self):
        path = self.project / "archive"
        path.write_bytes(b"fixed")
        item = {"sha256": hashlib.sha256(b"fixed").hexdigest(), "url": "https://example.invalid/archive"}
        with mock.patch.object(tool.urllib.request, "urlopen", side_effect=AssertionError("network forbidden")):
            self.assertEqual(tool.fetch(item, path, offline=True), 0)
            path.write_bytes(b"corrupt")
            with self.assertRaises(tool.ToolchainError):
                tool.fetch(item, path, offline=True)
            with self.assertRaises(tool.ToolchainError):
                tool.fetch(item, self.project / "missing", offline=True)
        self.assertEqual(path.read_bytes(), b"corrupt")

    def test_download_checksum_and_network_failures_leave_no_partial_or_foreign_file(self):
        item = {"sha256": "0" * 64, "url": "https://example.invalid/archive"}
        response = io.BytesIO(b"wrong checksum")
        response.geturl = lambda: item["url"]
        path = self.project / "downloads/archive"
        with mock.patch.object(tool.urllib.request, "urlopen", return_value=response), self.assertRaises(tool.ToolchainError):
            tool.fetch(item, path)
        self.assertEqual(list(path.parent.iterdir()), [])
        foreign = path.parent / ".archive.part-foreign"
        foreign.write_bytes(b"leave me")
        with mock.patch.object(tool.urllib.request, "urlopen", side_effect=OSError("offline")), self.assertRaises(OSError):
            tool.fetch(item, path)
        self.assertEqual(foreign.read_bytes(), b"leave me")

    def test_sources_extract_atomically_and_reject_traversal_or_links(self):
        archive = self.project / "source.tar.gz"
        self.archive(archive, {"src/header.h": "ordinary source"})
        destination = self.project / "source"
        item = {"version": "1.0", "commit": "a" * 40, "sha256": tool.sha256(archive)}
        tool.unpack_source(archive, destination, item)
        self.assertEqual((destination / "src/header.h").read_text(), "ordinary source")
        tool.unpack_source(archive, destination, item)
        with self.assertRaises(tool.ToolchainError):
            tool.unpack_source(archive, destination, dict(item, commit="b" * 40))
        for kind in (tarfile.REGTYPE, tarfile.SYMTYPE, tarfile.LNKTYPE):
            member = tarfile.TarInfo("source/../../escape" if kind == tarfile.REGTYPE else "source/link")
            member.type, member.linkname = kind, "/outside"
            self.archive(archive, {}, member)
            with self.subTest(kind=kind), self.assertRaises(tool.ToolchainError):
                tool.unpack_source(archive, self.project / "bad-source", item)
            self.assertFalse((self.project / "bad-source").exists())
            self.assertFalse((self.project / "escape").exists())

    def test_runtime_selection_handles_path_names_old_versions_and_errors(self):
        with mock.patch.object(tool.shutil, "which", return_value="/existing/python3.13"), \
                mock.patch.object(tool, "command", return_value="Python 3.13.15\n"):
            self.assertEqual(tool.runtime(["python3.13"], [3, 10, 0], "Python")["path"], "/existing/python3.13")
        for response in ("Python 3.9.6", "v18.2.0", "unexpected"):
            with self.subTest(response=response), mock.patch.object(tool, "command", return_value=response), self.assertRaises(tool.ToolchainError):
                tool.runtime(["/existing/program"], [18, 3, 0] if response.startswith("v") else [3, 10, 0], "runtime")
        with mock.patch.object(tool, "command", side_effect=OSError("missing")), self.assertRaises(tool.ToolchainError):
            tool.runtime(["/missing"], [3, 10, 0], "Python")

    def test_child_environment_preserves_global_path_home_and_owns_config_cache(self):
        with mock.patch.dict(os.environ, {"EM_CONFIG": "/global/config", "EM_NODE_JS": "other-node", "EMCC_CFLAGS": "-DGLOBAL"}):
            before = dict(os.environ)
            child = tool.environment(self.root, self.state)
            self.assertEqual(dict(os.environ), before)
            for name in ("PATH", "HOME"):
                self.assertEqual(child.get(name), before.get(name))
            self.assertEqual(child["EM_CONFIG"], str(self.root / ".emscripten"))
            self.assertEqual(child["EM_CACHE"], str(self.root / "cache"))
            self.assertNotIn("EM_NODE_JS", child)
            self.assertNotIn("EMCC_CFLAGS", child)
            self.assertEqual(child["EMCC_CORES"], "1")

    def test_full_cached_install_and_offline_reopen_without_global_activation(self):
        self.cached_fixture()
        state = self.install_fixture()
        self.assertTrue(state["ready"])
        self.assertEqual(state["downloaded_bytes"], 0)
        self.assertFalse((self.root / ".bootstrap-lock").exists())
        with mock.patch.object(tool, "command", side_effect=self.fake_command), \
                mock.patch.object(tool.urllib.request, "urlopen", side_effect=AssertionError("network forbidden")):
            self.assertTrue(tool.status(self.root, self.lock, "fixture-lock")["ready"])
            with self.assertRaises(tool.ToolchainError):
                tool.status(self.root, self.lock, "different-lock")
            (self.root / ".emscripten").write_text("NODE_JS = '/global'\n")
            with self.assertRaises(tool.ToolchainError):
                tool.status(self.root, self.lock, "fixture-lock")

    def test_offline_missing_compiler_never_invokes_installer_and_releases_lock(self):
        self.cached_fixture()
        with mock.patch.object(tool.platform, "system", return_value="Darwin"), mock.patch.object(tool.platform, "machine", return_value="arm64"), \
                mock.patch.object(tool, "runtime", side_effect=[self.state["python"], self.state["node"]]), \
                mock.patch.object(tool, "command", side_effect=AssertionError("installer forbidden")), self.assertRaisesRegex(tool.ToolchainError, "Offline: SDK compiler"):
            tool.bootstrap(self.root, self.lock, "fixture-lock", offline=True)
        self.assertFalse((self.root / "state.json").exists())
        self.assertFalse((self.root / ".bootstrap-lock").exists())

    def test_wrong_emcc_version_and_failed_commands_are_not_accepted(self):
        with mock.patch.object(tool, "command", return_value="emcc (fixture) 6.0.120\n"), self.assertRaises(tool.ToolchainError):
            tool.compiler_version(self.root, self.state, "6.0.12")
        with mock.patch.object(tool.subprocess, "run", return_value=subprocess.CompletedProcess([], 1, "broken install")), self.assertRaisesRegex(tool.ToolchainError, "broken install"):
            tool.command(["installer"])

    def test_unknown_host_existing_lock_and_redirected_state_are_rejected(self):
        self.root.mkdir(parents=True)
        with mock.patch.object(tool.platform, "system", return_value="Other"), self.assertRaisesRegex(tool.ToolchainError, "No pinned SDK"):
            tool.bootstrap(self.root, self.lock, "fixture-lock")
        (self.root / ".bootstrap-lock").mkdir()
        with self.assertRaisesRegex(tool.ToolchainError, "Bootstrap lock exists"):
            tool.bootstrap(self.root, self.lock, "fixture-lock")
        self.assertTrue((self.root / ".bootstrap-lock").is_dir())
        (self.root / ".bootstrap-lock").rmdir()
        (self.root / "downloads").symlink_to(self.project / "outside")
        with self.assertRaisesRegex(tool.ToolchainError, "symlinks"):
            tool.bootstrap(self.root, self.lock, "fixture-lock")


if __name__ == "__main__":
    unittest.main(verbosity=2)
