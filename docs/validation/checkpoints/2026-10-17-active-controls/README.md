# Active selection, value retention, reference controls and hidden QA

Integration source **413b25b**. The manifests retain the dirty-state/source and binary SHA-256 for each run. The native application was deliberately rebuilt with **O0 ASan/UBSan** for iteration; `build-o0.txt` preserves the exact command (7.344s). This is distinct from the default O1 sanitizer target and does not establish optimized input latency. GPU/background helpers use O1 sanitizers. No dependencies or global macOS/Git preferences were changed.

| Accepted scope | Native stages/checks | Runtime (preparation excluded) |
| --- | --- | --- |
| Early layout/selection assertion prefix | 45 stages | 23.270s |
| Active origin, drag/Shift/reversal/Tab/Enter, readout eviction and invalidation | 41 stages | 19.973s |
| Criteria cache, real filter Apply/Cancel/Undo, closed selectors and 150% narrow clipping | 24 stages | 13.052s |
| Complete independent-table workflow, including corrected ordered navigation | 73 stages | 32.989s |
| Real framebuffer GPU paint probe | 110 checks | 1.944s |
| Background-window probe, before each native gate | 477 checks each | 0.729–1.647s |

The **183 application stages are focused acceptance**, not a fresh full 1,053-stage pass. The earlier frozen 988-stage pass remains associated only with its own source/binary. The 2-frame export negative test returned 1 with unfinished-stage diagnostics in 1.147s, proving hidden startup does not manufacture completion.

Current pure grid/readout/label coverage is 16,860 /133 /238 assertions per configuration. `selection-fast.json` records the combined grid/readout gate before the label boundary follow-up (then 234 label checks); `filterlabel-fast.json` covers the final 238-check helper. The helper accepts valid 8 MiB pivot predicates; upstream source-filter limits remain 4 MiB. Native scenarios preserve real DuckDB PIVOT, source/result ownership, exact DECIMAL text and SQL NULL versus empty/literal text. The readout remains bounded and does not request data to repaint. Filter JSON is parsed on definition changes, with idle cache counters stable.

## Non-disruptive automation

Automated application/GPU windows are hidden and unfocused **before InitWindow**, with always-run rendering. The Cocoa regression asserts invisibility/non-key state, NSApp inactivity and frontmost PID never equal to the test process, while permitting the user to switch applications. Actual Retina pixels survive both pre/post-swap capture and resize. No Space movement/pinning/private API is used. The runner discards inherited visibility overrides; `--visible` is explicit and recorded. Hidden real-file-panel requests are refused before entering AppKit's modal loop.

Actual system panels, OS responder/IME routing, physical gestures/DPI/fullscreen/Space lifecycle remain deliberately visible QA. Existing clipboard scenarios still use the system clipboard, so the desktop lock remains necessary. These tests do not claim physical input delivery merely because the renderer/controller paths accepted queued events.

## UI acceptance and exceptions

The read-only reference was exercised at 1280×820 /100% and 640×640 /150%. Local matched reference/native captures live in `tests/results/active-selection/` (`criteria-comparison.png`, `criteria-narrow-comparison.png`, `ui-reference/`); original native Retina frames remain in `tests/results/workspace-layout/`. Their capture states differ in pane scroll position, retained focus, field display aliases and transient native engine feedback; these are not pixel-difference scores or whole-interface sign-off.

Implemented controls: criteria text with the reference filter icon/text-only anatomy, full-pill hit target and Space access; measured closed representation/Plot/font/number fields; explicit select focus and release of other editing regions. Applying a filter now reveals its pill. Undo and replacement edit occur within one test stage to exercise reused revisions without an intervening paint. Opening a real distinct-value editor legitimately queries; idle/cancel assertions start after that publication.

Remaining exact-UI work includes collapsed ribbon geometry/icons at narrow widths (identified by the new comparison), selector option-popup anatomy, shared dialogs, and the rest of the matching-state matrix. Analytical future work remains separately tracked in the current roadmap. No prototype source was modified.

Initial failed harness runs are retained locally: the first exposed a real hidden pill/reveal issue; the second exposed an incorrect no-query assumption while opening the real distinct editor. Their failures are not counted as passes. The active-value lane's earlier table assertion failure and corrected follow-up evidence were also preserved before cleanup under `tests/results/active-selection/lanes/`.
