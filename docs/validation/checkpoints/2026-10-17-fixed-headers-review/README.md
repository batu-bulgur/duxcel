# Fixed sheet headers and review coverage — 17 October 2026

Runtime and focused harness: `41095d0`. The latest human zoom requirement overrides the prototype's earlier scaling of sheet headers. All new production behavior remains C99/raylib; the browser runs the same grid code.

Row-number gutter width, both source-header tier heights and header typography now follow interface scale. Data rows/columns and cell text follow canvas zoom; row-header height and column-header width follow their corresponding cells. Header filtering, resizing, frozen dividers, selection handles and fit calculations use the appropriate units. Source and historical previews now share the actual header/data fit calculation.

Native review now enumerates the topmost File surface, heading, clipped actions and scrollbar, plus Ribbon display, Interface scale, Row density and Text color choice menus. Disabled Save remains reportable. Capture/cancel retains focus, selection, caches, history and requests. Short report composition preserves a full editor line and visible actions; its compact presentation scale does not change captured UI-scale provenance. A real 640×200/150% note was focused, edited with Backspace, scrolled and saved through the pointer button, then read back exactly as the third immutable report.

The new Wasm acceptance also found an actual WebGL1 error: raylib's framebuffer deletion queried the name of a missing depth attachment. Our paint caches explicitly own only a color texture and depthless framebuffer. Browser cleanup now deletes those owned resources directly, keeping the native path intact. Final popup growth/shrink/scale/filter interactions produce no rendering diagnostics.

| Gate | Accepted scope | Elapsed |
| --- | --- | --- |
| Pure grid/choice/preview/paint/popup, optimized + ASan/UBSan | Grid 36,562 checks/configuration; all five modules pass | 25.109s including 21.159s build |
| Pure review geometry/core | 38,600 layout  + 2,905 registry  + 131 store checks/configuration; reader/build fixtures | See retained log |
| Native zoom, ASan/UBSan | 12 stages; 50/100/200% canvas, 150% interface, main/source/history grids, fit dispatch and retained query/cache/history | 8.850s app; 10.011s warm runner |
| Native review, optimized + ASan/UBSan | 71 stages each; three exact durable reports each; File/four choice trees/short note | 3.620s /3.806s app |
| Native release | Existing 6-stage engine/application scenario | 3.639s app;4.779s runner |
| Native GPU, ASan/UBSan | 110 real paint/clip/resource checks under desktop lock | 1.882s |
| Wasm zoom, review artifact | 191 checks; real imported five-row CSV +blank grid, actual compositor glyph bounds at DPR2 with inset CSS transform, trusted header selection/resize/filter, independent UI125/100, zero zoom/scale jobs | 18.031s |
| Wasm access/review | 80 checks; real disabled File Save/background/heading/scrolled thumb and Ribbon-display option picking, cancellation, zero report uploads or engine jobs | 17.089s |
| Wasm shipping | Existing12 startup/manual-interaction checks | 7.441s |

Each native runner also accepts the 477-check hidden-window probe. All automated windows were hidden or belonged to an owned headless Chromium profile; tests never activated or closed the user's app. The native bundle is ad-hoc signed and verified. Current native and Wasm shipping/review exclusion checks pass. Runner 27 and CDP 8 tests pass; no new dependencies were installed.

Visual evidence, actual interactions and analytical semantics remain separate: the fixed-header requirement is accepted at the tested states, not the complete UI port. Zoom/UI scale submit no Worker jobs. Source/blank range selection can still request existing browser-unsupported statistics (kind 10); browser analytical completeness is not claimed. Physical pinch/trackpad/Space/IME delivery, other browsers, full reference-state parity, record-following formats and browser workbook saving remain open in the roadmap. The full 1,120-stage native inventory was not rerun.

Earlier failed runs are not accepted evidence: the initial anchor test ignored default frozen columns and origin clamping; transformed pointer motion delivered 23.75 rather than 24 points; direct WebGL readback could sample a discarded buffer; the actual late WebGL attachment error was fixed. Short-picker tests now move the reporting toolbar using its real Space action. Saved reports are verified in captured-frame order rather than lexicographic IDs (`17` precedes `2`). The compact note screenshot waits for both hidden swap buffers to present.

[Provenance](provenance.json) binds source, immutable browser artifacts, native binaries and retained evidence hashes. The only persistent project origin remains `http://127.0.0.1:8767/`, serving accepted review artifact `33cd6ba6708fccf4417a`; the automatic external queue consumer and existing durable outbox are retained. No user browser was refreshed. Managed-worktree archival was requested through the application and returned queued; physical removal is not claimed.
