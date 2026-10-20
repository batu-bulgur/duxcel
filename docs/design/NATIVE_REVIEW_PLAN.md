# Development review architecture

Native and Wasm feedback share the C semantic registry and one persistent external queue. This workflow is implemented infrastructure; it does not continually take precedence over product correctness or persistence work. Current capability and remaining work live in [status](../STATUS.md) and [roadmap](../ROADMAP.md).

## Product isolation

- `DUXCEL_DEV_REVIEW` is independent of optimization, sanitizers and `NDEBUG`. Review release/sanitizer objects and binaries are separate from shipping builds.
- Shipping binaries/packages exclude new review modules, fields, strings, commands, input hooks, listeners and browser exports/assets at compile/link/package time. A disabled runtime switch is insufficient.
- The native app requires no Python, Node, model or service. External queue/agent processes start only through explicit development commands. Browser product persistence must not depend on the review service.
- Inspect actual linked artifacts for exclusion. A source/Make graph check alone cannot prove absence.

## Portable C registry and capture

Use stable action IDs, widget-instance IDs, presentation paths and data targets separately. Labels, pointers, geometry hashes and enum ordinals alone are not durable identity. Register actual bounds/clipping/layers, eligibility and enabled/focused state. Detect duplicates and bounded overflow. Resolve visible grid targets on demand rather than enumerating the infinite sheet.

A capture owns one sealed frame registry, image and state, including source/build/binary identity, window/framebuffer/DPI/UI scale/sheet zoom and the selected target. Keep query/source/object/schema generations with positional cell targets; invalidate them after reindexing. Preserve exact integer/DECIMAL text, typed NULL/empty/literal text and safely bounded UTF-8/JSON. Never retain evictable Page/Cell pointers or mutable worker references.

Capture pixels on the graphics thread before report decoration; transfer owned buffers to encoding/writing. Keep capture independent of DuckDB connections. Use bounded event traces and explicit captures, with no idle SQL, per-frame filesystem/network work, periodic screenshots or whole-grid trees.

## Composer and document lifetime

The review command owns input before normal shortcut/click dispatch. Picking and note composition preserve the original menu, focus and selection; there is no click-through or accidental worksheet typing. Cancellation, failed capture/write, retry and document/window shutdown have explicit cleanup. Background queries may continue, but the report retains its captured state. Verify eviction and asynchronous replies without publishing stale data.

The native outbox atomically publishes immutable uniquely identified bundles containing image, target/state, manifest and bounded trace. Use a platform-selected writable root. Local reporting works while the external service/agent is stopped.

## Persistent queue and agent adapter

The browser review host first commits its immutable archive to IndexedDB, then submits it to the shared project outbox through an explicitly started same-origin loopback service. Local-pending and shared durable acknowledgment are different states. Reload retries retain IDs/bytes; local pending data remains subject to origin eviction/deletion until shared acknowledgment.

The external queue validates bounded uploads, rejects conflicting IDs and uses transactional leases, bounded retries and restart recovery. An optional worker invokes the existing Codex CLI read-only, supplies the actual frame and records structured triage, source targets and acceptance checks. Captured text/notes are data, never command authority. Pickup/triage does not imply implementation, verification or design approval. Preserve acknowledgment/reply history without modifying immutable evidence. No global agent configuration changes are required.

Remote application control, arbitrary SQL/shell access, continuous video and general deterministic replay are outside this reporting contract.

## Verification and ownership

- Pure C/ASan/UBSan tests cover identity, scope, clipping/layers, overflow, stale targets, ownership and serialization.
- Focused real native/browser tests cover capture/composer isolation, actual images, cancellation, write failure/retry, durable queue/reload behavior, malformed uploads, concurrent consumers and expired leases.
- Cover disabled/closed widgets, frozen surfaces, popups, ribbon overflow, panes and narrow/short/150% layouts. Physical input, IME, clipboard, non-Chromium and OS acceptance remain separately scoped.
- Keep tested source/binary/fixture/viewport and elapsed time. Rendering, analytical correctness, feedback transport and user design acceptance are separate claims.
- One desktop owner uses the shared test lock and hidden windows. Never control/terminate the user's app or overwrite a running test binary. Native compilation defaults to eight jobs; keep mutable engine checks serial and use per-worktree mutable fixtures.

The Wasm target uses the same C App/registry with browser capture/outbox adapters and the official asynchronous DuckDB-Wasm Worker. Request identities, cancellation, stale replies, typed transport and independently owned memory remain mandatory. Complete browser workbook Open/Save remains separate unfinished work.
