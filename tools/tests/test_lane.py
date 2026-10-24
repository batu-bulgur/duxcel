import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import tempfile
import time
import unittest

import lane
import workspace


class LaneTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.base = Path(self.temporary.name).resolve()
        self.main = self.base / "main"
        self.other = self.base / "other"
        self.main.mkdir()
        self.git(self.main, "init", "-q")
        self.git(self.main, "-c", "user.name=Test", "-c", "user.email=test@example.invalid",
                 "commit", "--allow-empty", "-qm", "base")
        self.git(self.main, "worktree", "add", "--detach", str(self.other), "HEAD")
        for root in (self.main, self.other):
            (root / "tools").mkdir()
            for name in ("lane.py", "workspace.py"):
                shutil.copyfile(Path(lane.__file__).parent / name, root / "tools" / name)
            self.run_lane(root, "--init", check=True)

    def git(self, root, *args):
        return subprocess.run(["git", "-C", str(root), *args], check=True, capture_output=True)

    def argv(self, root, *args):
        return [sys.executable, str(root / "tools/lane.py"), "--checkout", str(root),
                "--lane", root.name, *args]

    def run_lane(self, root, *args, **kwargs):
        return subprocess.run(self.argv(root, *args), capture_output=True, text=True, **kwargs)

    def test_worktree_commands_serialize_on_one_repository_lock(self):
        self.assertEqual(workspace.repository_lock_path(self.main), workspace.repository_lock_path(self.other))
        entered, release, second = [self.base / name for name in ("entered", "release", "second")]
        program = ("import pathlib,time,sys; pathlib.Path(sys.argv[1]).touch(); "
                   "\nwhile not pathlib.Path(sys.argv[2]).exists(): time.sleep(.01)")
        first = subprocess.Popen(self.argv(self.main, "--", sys.executable, "-c", program, str(entered), str(release)),
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self.addCleanup(lambda: first.poll() is None and first.kill())
        deadline = time.monotonic() + 5
        while not entered.exists() and time.monotonic() < deadline:
            time.sleep(.01)
        self.assertTrue(entered.exists())
        follower = subprocess.Popen(self.argv(self.other, "--", sys.executable, "-c",
                                             "import pathlib,sys; pathlib.Path(sys.argv[1]).touch()", str(second)),
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self.addCleanup(lambda: follower.poll() is None and follower.kill())
        with self.assertRaises(subprocess.TimeoutExpired):
            follower.communicate(timeout=.3)
        self.assertFalse(second.exists())
        release.touch()
        first.communicate(timeout=5)
        follower.communicate(timeout=5)
        self.assertEqual((first.returncode, follower.returncode), (0, 0))
        self.assertTrue(second.exists())
        receipts = list(self.other.glob("build/tests/results/lanes/*.json"))
        self.assertEqual(json.loads(receipts[0].read_text())["checkout"], str(self.other))

    def test_cancellation_preserves_lock_until_child_exits(self):
        ready, cancelled, release = [self.base / name for name in ("ready", "cancelled", "release")]
        program = ("import pathlib,signal,sys,time; "
                   "signal.signal(signal.SIGTERM,lambda *a:pathlib.Path(sys.argv[2]).touch()); "
                   "pathlib.Path(sys.argv[1]).touch(); "
                   "\nwhile not pathlib.Path(sys.argv[3]).exists(): time.sleep(.01)")
        process = subprocess.Popen(self.argv(self.main, "--", sys.executable, "-c", program,
                                              str(ready), str(cancelled), str(release)),
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self.addCleanup(lambda: process.poll() is None and process.kill())
        deadline = time.monotonic() + 5
        while not ready.exists() and time.monotonic() < deadline:
            time.sleep(.01)
        self.assertTrue(ready.exists())
        process.send_signal(signal.SIGTERM)
        deadline = time.monotonic() + 5
        while not cancelled.exists() and time.monotonic() < deadline:
            time.sleep(.01)
        self.assertTrue(cancelled.exists())
        with self.assertRaises(RuntimeError):
            with workspace.FileLock(workspace.repository_lock_path(self.other), "test"):
                pass
        release.touch()
        process.communicate(timeout=5)
        self.assertEqual(process.returncode, 0)

    def test_shell_free_cwd_environment_and_receipt(self):
        payload = "$(touch SHOULD_NOT_EXIST); `touch ALSO_NOT`"
        program = ("import json,os,sys,pathlib; pathlib.Path('observed.json').write_text("
                   "json.dumps([os.getcwd(),os.environ['DUXCEL_LANE'],os.environ['DUXCEL_TEST_INSTANCE'],"
                   "os.environ['DUXCEL_TEST_VISIBLE'],os.environ['BUILD_JOBS'],sys.argv[1]]))")
        self.run_lane(self.other, "--", sys.executable, "-c", program, payload, check=True)
        self.assertEqual(json.loads((self.other / "observed.json").read_text()),
                         [str(self.other), "other", "1", "0", "8", payload])
        receipt = json.loads(next(self.other.glob("build/tests/results/lanes/*.json")).read_text())
        self.assertEqual(receipt["returncode"], 0)
        self.assertEqual(receipt["lane"], "other")
        self.assertIn("elapsed_seconds", receipt)

    def test_wrong_checkout_marker_and_shared_output_are_rejected(self):
        command = ["--", sys.executable, "-c", "raise AssertionError('must not launch')"]
        result = subprocess.run([sys.executable, str(self.main / "tools/lane.py"), "--checkout", str(self.other),
                                 "--lane", "main", *command], capture_output=True, text=True)
        self.assertIn("does not match", result.stderr)
        marker = self.main / "build/lane.json"
        marker.write_text('{}')
        self.assertIn("mismatched", self.run_lane(self.main, *command).stderr)
        (self.other / ".cache").symlink_to(self.main / "build", target_is_directory=True)
        self.assertIn("not a symlink", self.run_lane(self.other, *command).stderr)
