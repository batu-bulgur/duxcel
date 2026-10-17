#!/usr/bin/env python3
"""One development-only Wasm origin, native/browser queue and agent consumer.

No web or model runtime is linked into native/shipping targets. A stopped
service cannot lose a native outbox report or a browser's committed local report.
"""
import argparse
import contextlib
import functools
import hmac
import http.server
import json
import os
from pathlib import Path
import secrets
import signal
import socketserver
import threading
import time
from urllib.parse import unquote, urlsplit

from native_review import ReviewError, ReviewStore, default_project, default_root
from review_queue import ReviewQueue

ROOT = Path(__file__).resolve().parents[1]
UPLOAD_LIMIT = 134 * 1024 * 1024


def review_artifact(path=None):
    artifact = Path(path) if path else Path(json.loads((ROOT / "build/web/review/latest.json").read_text())["directory"])
    artifact = artifact.resolve(strict=True)
    manifest = json.loads((artifact / "build.json").read_text())
    if manifest.get("kind") != "review" or not (artifact / "review_host.mjs").is_file():
        raise ReviewError("Serve only a separate Wasm review artifact, never a shipping artifact")
    return artifact


class ReviewServer(http.server.ThreadingHTTPServer):
    daemon_threads = True
    def __init__(self, address, queue, artifact=None):
        if address[0] != "127.0.0.1":
            raise ReviewError("Review service must bind to IPv4 loopback")
        self.queue, self.artifact = queue, artifact
        self.token = secrets.token_urlsafe(32)
        self.upload_lock = threading.Lock()
        super().__init__(address, ReviewHandler)

    def server_bind(self):
        # Avoid this Mac's slow reverse DNS lookup; address is explicit loopback.
        socketserver.TCPServer.server_bind(self)
        self.server_name, self.server_port = self.server_address


class ReviewHandler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(args[2].artifact or ROOT / "build/nonexistent"), **kwargs)

    def log_message(self, *args):
        pass

    def end_headers(self):
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("Referrer-Policy", "no-referrer")
        super().end_headers()

    def json(self, status, payload):
        data = json.dumps(payload, ensure_ascii=False).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(data)

    def same_origin(self):
        host = self.headers.get("Host", "")
        if host not in (f"127.0.0.1:{self.server.server_port}", f"localhost:{self.server.server_port}"):
            return False
        origin = self.headers.get("Origin")
        return (origin is None or origin == "http://" + host) and self.headers.get("Sec-Fetch-Site") != "cross-site"

    def do_GET(self):
        if not self.same_origin():
            self.json(403, {"error": "Review service requires the same loopback origin"})
            return
        route = urlsplit(self.path).path
        try:
            if route == "/__review/config":
                self.json(200, {"schema": "duxcel.review-service", "version": 1,
                                "project_id": self.server.queue.project_id, "token": self.server.token})
            elif route == "/__review/queue":
                self.json(200, self.server.queue.list())
            elif route.startswith("/__review/bundle/"):
                fields = route.removeprefix("/__review/bundle/").split("/")
                if len(fields) != 2 or fields[1] not in ("manifest.json", "frame.png", "browser.json"):
                    raise ReviewError("Unknown report member")
                report_id, name = unquote(fields[0]), fields[1]
                self.server.queue.store.read(report_id)
                with self.server.queue.store.bundle(report_id) as bundle:
                    data = bundle.read(name, UPLOAD_LIMIT if name == "frame.png" else 4 * 1024 * 1024)
                self.send_response(200)
                self.send_header("Content-Type", "image/png" if name == "frame.png" else "application/json")
                self.send_header("Content-Length", str(len(data)))
                self.end_headers()
                if self.command != "HEAD":
                    self.wfile.write(data)
            elif route.startswith("/__review/report/"):
                report_id = unquote(route.removeprefix("/__review/report/"))
                self.json(200, self.server.queue.store.read(report_id))
            elif self.server.artifact:
                # No directory listing, source checkout or private outbox exposure.
                relative = unquote(route).lstrip("/") or "index.html"
                candidate = (self.server.artifact / relative).resolve()
                if not candidate.is_relative_to(self.server.artifact) or not candidate.is_file():
                    self.json(404, {"error": "Not found"})
                else:
                    super().do_HEAD() if self.command == "HEAD" else super().do_GET()
            else:
                self.json(404, {"error": "No Wasm artifact configured"})
        except (OSError, ValueError, ReviewError) as error:
            self.json(400, {"error": str(error)})

    def do_HEAD(self):
        # HEAD follows exactly the same routing/containment as GET.
        self.do_GET()

    def do_POST(self):
        self.close_connection = True
        token = self.headers.get("X-Duxcel-Review-Token", "")
        if not self.same_origin() or not token.isascii() or not hmac.compare_digest(token, self.server.token):
            self.json(403, {"error": "Invalid review origin/token"})
            return
        if self.path != "/__review/reports":
            self.json(404, {"error": "Not found"})
            return
        try:
            length = int(self.headers.get("Content-Length", "0"))
            if length < 1 or length > UPLOAD_LIMIT or self.headers.get("Transfer-Encoding") or self.headers.get("Content-Type", "").split(";")[0] != "application/x-tar":
                self.json(413, {"error": "A bounded report archive is required"})
                return
            if not self.server.upload_lock.acquire(blocking=False):
                self.json(503, {"error": "Another report is being committed; retry shortly"})
                return
            try:
                self.connection.settimeout(15)
                data = self.rfile.read(length)
                if len(data) != length:
                    raise ReviewError("Incomplete report upload")
                receipt = self.server.queue.import_archive(data)
                self.server.queue.sync()
                self.json(200, {"report_id": receipt["report_id"], "durable": True, "queued": True,
                                "duplicate": receipt["duplicate"]})
            finally:
                self.server.upload_lock.release()
        except (OSError, ValueError, ReviewError) as error:
            self.json(400, {"error": str(error)})


def parser():
    result = argparse.ArgumentParser(description=__doc__)
    result.add_argument("--root", type=Path, help="shared project outbox; defaults to the native outbox")
    result.add_argument("--project", default=default_project())
    commands = result.add_subparsers(dest="command", required=True)
    serve = commands.add_parser("serve", help="one loopback origin and native report collector")
    serve.add_argument("--port", type=int, default=8767)
    serve.add_argument("--artifact", type=Path)
    serve.add_argument("--agent", choices=("none", "codex"), default="none", help="automatic read-only triage, never automatic source edits")
    commands.add_parser("list")
    retry = commands.add_parser("retry", help="explicitly retry a failed report after fixing the consumer")
    retry.add_argument("report_id")
    read = commands.add_parser("read")
    read.add_argument("report_id")
    worker = commands.add_parser("worker", help="automatically claim and triage new reports")
    worker.add_argument("--once", action="store_true")
    worker.add_argument("--codex", help="existing CLI executable; no installation/config changes")
    next_report = commands.add_parser("next", help="wait for and atomically lease the next report")
    next_report.add_argument("--worker", required=True)
    next_report.add_argument("--wait", type=float, default=0)
    for name in ("finish", "release", "renew"):
        child = commands.add_parser(name)
        child.add_argument("report_id")
        child.add_argument("token")
        if name != "renew":
            child.add_argument("--message", default="")
    return result


def main(argv=None):
    args = parser().parse_args(argv)
    queue = ReviewQueue(args.root or default_root(args.project), args.project)
    if args.command == "list":
        print(json.dumps(queue.list(), indent=2))
    elif args.command == "read":
        print(json.dumps(queue.store.read(args.report_id), ensure_ascii=False, indent=2))
    elif args.command == "retry":
        print(json.dumps(queue.retry(args.report_id)))
    elif args.command == "next":
        deadline = time.monotonic() + min(3600, max(0, args.wait))
        while True:
            claim = queue.claim(args.worker)
            if claim or time.monotonic() >= deadline:
                print(json.dumps(claim, ensure_ascii=False))
                break
            time.sleep(.5)
    elif args.command in ("finish", "release", "renew"):
        kwargs = {"message": args.message} if args.command == "finish" else {"error": args.message} if args.command == "release" else {}
        print(json.dumps(getattr(queue, args.command)(args.report_id, args.token, **kwargs)))
    elif args.command == "worker":
        from review_worker import CodexTriage, ReviewWorker
        worker = ReviewWorker(queue, CodexTriage(executable=args.codex))
        if args.once:
            print(json.dumps(worker.step(), ensure_ascii=False))
        else:
            signal.signal(signal.SIGTERM, lambda *_: worker.stop.set())
            signal.signal(signal.SIGINT, lambda *_: worker.stop.set())
            worker.run()
    else:
        artifact = review_artifact(args.artifact)
        server = ReviewServer(("127.0.0.1", args.port), queue, artifact)
        stop = threading.Event()
        worker = worker_thread = None
        def collect():
            while not stop.is_set():
                try:
                    queue.sync()
                except Exception as error:
                    print(json.dumps({"collector_error": str(error)}), flush=True)
                stop.wait(1)
        if args.agent == "codex":
            from review_worker import CodexTriage, ReviewWorker
            worker = ReviewWorker(queue, CodexTriage())
            worker_thread = threading.Thread(target=worker.run, daemon=True)
            worker_thread.start()
        threading.Thread(target=collect, daemon=True).start()
        def shutdown(*_):
            stop.set()
            if worker:
                worker.stop.set()
            threading.Thread(target=server.shutdown, daemon=True).start()
        signal.signal(signal.SIGTERM, shutdown)
        signal.signal(signal.SIGINT, shutdown)
        print(json.dumps({"url": f"http://127.0.0.1:{server.server_port}/", "project_id": queue.project_id,
                          "outbox": str(queue.root), "agent": args.agent, "artifact": str(artifact)}), flush=True)
        try:
            server.serve_forever(poll_interval=.2)
        finally:
            shutdown()
            server.server_close()
            if worker_thread:
                worker_thread.join(timeout=8)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, ReviewError) as error:
        print(json.dumps({"error": str(error)}))
        raise SystemExit(1)
