# Unified native/Wasm automatic feedback — 17 October 2026

Accepted implementation source **`5cd61fc`**, source manifest
`f030ad791cb41396afde777e03c21d4ddde05fef9407cfdaf3646e793be67759`.
The only preexisting untracked tree was `tools/ui-review/`; it was not staged or
used. Its mock/Agentation servers were stopped. Production UI/engine semantics
were not changed by this feedback slice.

The development service serves one loopback Wasm review origin and automatically
discovers native bundles in the same project outbox. Browser saves first commit
an immutable archive into IndexedDB, then POST it with a same-origin token. Local
pending and shared-queue acknowledgment have different real C confirmations.
The shared outbox/SQLite queue survive process shutdown; no model or listener is
linked into production. Agent dispatch uses exclusive expiring leases and a
single external read-only Codex CLI worker. Results are triage, not fixes or
design/implementation/verification approval.

## Actual end-to-end evidence

- `python3 tools/test_native_review.py --no-build`: both **26-stage** optimized
  and ASan/UBSan reporting flows passed in **2.067s /2.123s**, preceded by the
  **477-check** hidden-window safety probe (**1.372s**). Entire gate **5.807s**.
  The two native reports per configuration retained their original frame/build
  hashes. [Native source/binary manifest](native_acceptance.json).
- The real browser **offline→save→reload→reconnect→queue** gate passed
  **39 checks**, **12.653s total**, at DPR2 with an inset80% canvas. The same
  **233,472-byte archive and SHA256** survived actual IndexedDB reload, one shared
  bundle was published, and its pending browser archive retired only after ACK.
  The test blocked only this owned page's review requests. The C app continued to
  use its real official Worker/source fixture. [Manifest](browser_acceptance.json),
  [full browser result](browser-offline-result.json), [pending confirmation](browser-pending.png).
- The online real browser queue path passed **32 checks**, **10.635s total**,
  with no archive download. [Result](browser-online-result.json),
  [shared-queue confirmation](browser-queued.png).
- An already running automatic worker discovered a real browser report and an
  atomically copied original native report, without a per-report chat prompt.
  The installed authenticated Codex CLI attached each actual PNG, inspected the
  target/context and wrote schema-validated triage. Both completed on attempt1.
  [Results and approximate CLI invocation timings](agent-triage.json),
  [durable queue/lifecycle assertions](loop-summary.json). A new queue instance
  read both outcomes after processing; native lifecycle matched its source copy,
  and browser design/implementation/verification remained pending/not-started/unverified.
  Both synthetic test notes correctly yielded uncertain triage rather than an
  invented defect. The private test worker was stopped afterward.

## Focused contracts and isolation

`make test-review-queue` passed **28 queue groups** (0.551s), **8 service/worker
groups** (5.126s) and **29 Node cases** (0.109s). These include competing real
processes, abrupt consumer exit, restart/expiry, intentional-stop requeue,
failed-consumer recovery, atomic publication, malformed/path/link/project/hash
rejection, ID conflicts, HTTP origin/token/restart and retained immutable bytes.
Cached unchanged idle syncs perform zero PNG reads; claims fully revalidate.
[Output](pure-tests.txt).

Existing registry/store release and ASan/UBSan helpers, reader and build-checker
fixtures passed in [review-core.txt](review-core.txt). Current normal/review Wasm
exports/assets/hosts and both optimized/sanitizer native shipping binaries passed
linked review absence. The new browser uploader is absent from production.
[Browser/native shipping artifact hashes](shipping-absence.json).

All actual native windows were hidden/unfocused before creation. Browser runs used
owned headless Chromium profiles and the shared desktop lock. No user app,
browser/profile, desktop Space or global agent configuration was changed.

## Limits

Delivery is at least once after a lease expires. Automatic triage consumes the
existing Codex account only when a report is available; no continuous video,
deterministic replay, automatic source edits or global MCP registration is
claimed. Browser storage is origin-scoped and can be deleted/evicted before a
shared ACK. Keep the review origin stable. Queue scans are bounded to1024 reports;
browser pending storage is bounded to32 reports/256MiB. Additional widget
enumeration, browser analytical/persistence parity, **blurry1× Retina backing and
trackpad panning** remain open. These passing feedback tests do not accept those
product behaviors or full UI parity.
