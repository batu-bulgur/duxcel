#!/usr/bin/env python3
"""Loopback/consumer contract checks; no model, desktop or browser required."""
import contextlib
import http.client
import io
import json
from pathlib import Path
import tempfile
import threading
import time
import unittest

from native_review import ReviewError
from review_queue import ReviewQueue
from tools.tests.test_review_queue import archive, files
from review_service import ReviewServer, review_artifact
from review_worker import CodexTriage, ReviewWorker, validate_triage


def triage(report_id):
    return {"report_id": report_id, "category": "defect", "summary": "Filter control needs review",
            "requested_behavior": "Retain focus", "evidence": ["Actual target filter.apply"],
            "implementation_targets": ["src/app/app.c"], "acceptance_checks": ["Check focus after dismissal"],
            "needs_clarification": False}


class ServiceTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="duxcel-review-service-")
        self.root = Path(self.temporary.name)
        self.queue = ReviewQueue(self.root / "outbox", "duxcel")
        self.server = ReviewServer(("127.0.0.1", 0), self.queue)
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()

    def tearDown(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(2)
        self.temporary.cleanup()

    def request(self, route, data=None, **headers):
        connection = http.client.HTTPConnection("127.0.0.1", self.server.server_port, timeout=3)
        try:
            connection.request("POST" if data is not None else "GET", route, body=data, headers=headers)
            response = connection.getresponse()
            payload = response.read()
            return response.status, json.loads(payload) if response.getheader("Content-Type") == "application/json" else payload
        finally:
            connection.close()

    def post(self, report_id="DXR-browser", **headers):
        defaults = {"Content-Type": "application/x-tar", "X-Duxcel-Review-Token": self.server.token}
        defaults.update(headers)
        return self.request("/__review/reports", archive(report_id=report_id), **defaults)

    def native(self, report_id="DXR-native"):
        folder = self.queue.root / report_id
        folder.mkdir()
        payload = files(report_id=report_id)
        for name in ("manifest.json", "frame.png"):
            (folder / name).write_bytes(payload[name])

    def test_same_origin_token_and_bounded_upload_are_required(self):
        self.assertEqual(self.request("/__review/config")[1]["project_id"], "duxcel")
        for headers in ({"Origin": "https://foreign.invalid"}, {"Host": "evil.invalid"}, {"Sec-Fetch-Site": "cross-site"}):
            self.assertEqual(self.request("/__review/config", **headers)[0], 403)
            self.assertEqual(self.post(**headers)[0], 403)
        self.assertEqual(self.post(**{"X-Duxcel-Review-Token": "wrong"})[0], 403)
        self.assertEqual(self.post(**{"X-Duxcel-Review-Token": "é"})[0], 403)
        self.assertEqual(self.post(**{"Content-Type": "application/json"})[0], 413)
        self.assertEqual(self.post(**{"Content-Length": str(200 * 1024 * 1024)})[0], 413)
        self.assertEqual(self.queue.list()["reports"], [])

    def test_http_receipt_survives_restart_and_returns_original_evidence(self):
        status, receipt = self.post()
        self.assertEqual(status, 200)
        self.assertEqual((receipt["durable"], receipt["queued"], receipt["duplicate"]), (True, True, False))
        self.assertTrue(self.post()[1]["duplicate"])
        restarted = ReviewQueue(self.queue.root, "duxcel")
        self.assertEqual(restarted.list()["reports"][0]["state"], "pending")
        self.assertEqual(self.request("/__review/bundle/DXR-browser/frame.png")[1], files()["frame.png"])
        self.assertEqual(self.request("/__review/bundle/DXR-browser/lifecycle.json")[0], 400)
        self.assertEqual(self.request("/__review/report/../outside")[0], 400)
        self.assertEqual(self.request("/anything")[0], 404)

    def test_service_restart_rotates_token_without_losing_reports(self):
        self.post()
        old_token, port = self.server.token, self.server.server_port
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(2)
        self.server = ReviewServer(("127.0.0.1", port), ReviewQueue(self.queue.root, "duxcel"))
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.assertEqual(self.post(**{"X-Duxcel-Review-Token": old_token})[0], 403)
        self.assertTrue(self.post()[1]["duplicate"])
        self.assertEqual(len(self.queue.list()["reports"]), 1)

    def test_automatic_worker_picks_native_and_browser_and_preserves_design_status(self):
        seen = []
        def runner(claim, queue, stop):
            seen.append((claim["report_id"], claim["report"]["manifest"]["target"]["widget"]["id"]))
            return triage(claim["report_id"])
        worker = ReviewWorker(self.queue, runner, interval=.01)
        with contextlib.redirect_stdout(io.StringIO()):
            thread = threading.Thread(target=worker.run)
            thread.start()
            try:
                self.native()
                self.post()
                deadline = time.monotonic() + 4
                while time.monotonic() < deadline:
                    listing = self.queue.list()["reports"]
                    if len(listing) == 2 and all(row["state"] == "completed" for row in listing):
                        break
                    time.sleep(.01)
                self.assertEqual(len(seen), 2)
                self.assertEqual({item[0] for item in seen}, {"DXR-native", "DXR-browser"})
                self.assertTrue(all(item[1] == "filter.apply" for item in seen))
                for row in self.queue.list()["reports"]:
                    self.assertEqual(row["state"], "completed")
                    self.assertEqual(row["result"], "triaged")
                    self.assertEqual(json.loads(row["message"])["report_id"], row["report_id"])
                    lifecycle = self.queue.store.read(row["report_id"])["lifecycle"]
                    self.assertEqual((lifecycle["design"], lifecycle["implementation"], lifecycle["verification"]), ("pending", "not-started", "unverified"))
                self.assertIsNone(worker.step())
            finally:
                worker.stop.set()
                thread.join(3)

    def test_failed_agent_retries_then_retains_terminal_failure(self):
        self.native()
        def fail(*_):
            raise ReviewError("Agent unavailable")
        worker = ReviewWorker(self.queue, fail, max_attempts=2)
        self.assertEqual(worker.step()["state"], "retry")
        self.assertEqual(worker.step()["state"], "failed")
        self.assertIsNone(worker.step())
        report = self.queue.list()["reports"][0]
        self.assertEqual(report["state"], "failed")
        self.assertIn("Agent unavailable", report["error"])
        self.assertTrue((self.queue.root / "DXR-native/frame.png").is_file())

    def test_intentional_worker_stop_requeues_instead_of_stranding_report(self):
        self.native()
        def interrupted(claim, queue, stop):
            stop.set()
            raise ReviewError("Stopped intentionally")
        worker = ReviewWorker(self.queue, interrupted, max_attempts=1)
        self.assertEqual(worker.step()["state"], "stopped")
        self.assertEqual(self.queue.list()["reports"][0]["state"], "pending")
        self.assertIsNone(worker.step())

    def test_codex_adapter_uses_read_only_existing_cli_and_attaches_frame(self):
        self.native()
        claim = self.queue.claim("test")
        executable = self.root / "fixture-agent"
        executable.write_text('#!/usr/bin/env python3\nimport sys,json,pathlib\n'
            'args=sys.argv[1:];prompt=sys.stdin.read();out=pathlib.Path(args[args.index("--output-last-message")+1]);'
            'out.with_name("invocation.json").write_text(json.dumps({"args":args,"prompt":prompt}));'
            'out.write_text(' + repr(json.dumps(triage("DXR-native"))) + ')\n')
        executable.chmod(0o700)
        result = CodexTriage(executable=executable)(claim, self.queue, threading.Event())
        self.assertEqual(result["report_id"], "DXR-native")
        invocation = json.loads(next((self.queue.root / ".agent-runs").rglob("invocation.json")).read_text())
        self.assertIn("read-only", invocation["args"])
        self.assertIn("--ignore-user-config", invocation["args"])
        self.assertIn("--image", invocation["args"])
        self.assertIn("untrusted DATA", invocation["prompt"])
        self.assertNotIn("--dangerously-bypass-approvals-and-sandbox", invocation["args"])
        with self.assertRaises(ReviewError):
            validate_triage(triage("foreign"), "DXR-native")

    def test_service_rejects_production_artifacts_and_non_loopback(self):
        artifact = self.root / "artifact"
        artifact.mkdir()
        (artifact / "build.json").write_text('{"kind":"application"}')
        with self.assertRaises(ReviewError):
            review_artifact(artifact)
        with self.assertRaises(ReviewError):
            ReviewServer(("0.0.0.0", 0), self.queue)


if __name__ == "__main__":
    unittest.main()
