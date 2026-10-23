import os
from pathlib import Path
import tempfile
import unittest
from unittest import mock
import build_info


class BuildInfoTests(unittest.TestCase):
    def test_archive_does_not_require_git(self):
        with mock.patch.object(build_info.subprocess, "run", side_effect=FileNotFoundError):
            data = build_info.metadata(Path("/source"))
        self.assertFalse(data["KNOWN"])
        self.assertIn("source archive", data["REVISION"])

    def test_stable_payload_does_not_rewrite_or_recompile(self):
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / "info.h"
            with mock.patch.object(build_info, "metadata", return_value={"REVISION": "abc", "KNOWN": 1}):
                self.assertTrue(build_info.generate(Path(directory), target))
                stamp = target.stat().st_mtime_ns
                self.assertFalse(build_info.generate(Path(directory), target))
                self.assertEqual(stamp, target.stat().st_mtime_ns)
            with mock.patch.object(build_info, "metadata", return_value={"REVISION": "def", "KNOWN": 1}):
                self.assertTrue(build_info.generate(Path(directory), target))
                self.assertIn('"def"', target.read_text())

    def test_dirty_head_and_changelog_are_captured(self):
        def git(root, *args):
            if args[0] == "rev-parse": return "a" * 40
            if args[0] == "status": return " M src/example.c"
            return "quoted \"subject\" */ and İ\nnext line"
        with mock.patch.object(build_info, "git", side_effect=git):
            data = build_info.metadata(Path("/source"))
        self.assertTrue(data["DIRTY"])
        self.assertTrue(data["KNOWN"])
        self.assertIn("next line", data["LOG"])
        escaped = build_info.c_string(data["LOG"])
        self.assertNotIn("\n", escaped)
        self.assertIn("\\042", escaped)
        self.assertIn("\\304\\260", escaped)

    def test_reproducible_timestamp(self):
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / "info.h"
            with mock.patch.dict(os.environ, {"SOURCE_DATE_EPOCH": "0"}), mock.patch.object(build_info, "metadata", return_value={}):
                build_info.generate(Path(directory), target)
            self.assertIn("1970-01-01", target.read_text())

    def test_uncommitted_code_change_refreshes_identity(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "src").mkdir()
            source = root / "src/example.c"
            source.write_text("old code")
            target = root / "build/info.h"
            with mock.patch.object(build_info, "metadata", return_value={"DIRTY": 1}):
                build_info.generate(root, target)
                before = target.read_text()
                source.write_text("new code")
                self.assertTrue(build_info.generate(root, target))
                self.assertNotEqual(before, target.read_text())

    def test_only_metadata_object_receives_generated_header(self):
        makefile = Path(__file__).resolve().parents[2].joinpath("Makefile").read_text()
        self.assertIn("$(BUILD_INFO_OBJECTS): OBJECT_CPPFLAGS = -include $(BUILD_INFO_HEADER)", makefile)
        self.assertNotIn("CPPFLAGS += -include $(BUILD_INFO_HEADER)", makefile)
