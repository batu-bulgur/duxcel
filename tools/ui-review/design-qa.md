# Local review sidecar acceptance

final result: passed

17 October 2026. Scope: feedback capture around the unchanged browser reference. This is not a new native visual/interaction/engine acceptance pass.

## Source and comparison

- Reference: directly served `design/prototype/`, SHA-256 `d30782b2df41819031822c8d158ca603e62346096d8e7cea4a68fa5ce602e0bd` covering source, public assets, HTML and lockfile. Git baseline at first startup: `c89a4ddcb02f89abbbf7c7658131e60c51cbd60f`; after the persistence restart: `10a2351cd75dce6ee506576c8feb70e9c3b2ccea`. The reference fingerprint stayed identical. Native work continued concurrently and is outside this evidence.
- Implementation: reference plus injected Agentation and a separate review strip at `http://localhost:4174/`. Pinned tool versions: Agentation 3.1.2 / MCP server 1.3.2. Tested sidecar source hashes are in `.local/evidence/source-manifest.json`.
- Same source fixture, Home/full ribbon, Transactions, selection F7:F10, UI/grid scales 100%, viewport 745 × 924, DPR 2. Browser overrides were not applied.
- Full source and implementation were composed into **one comparison input** and viewed: `.local/evidence/comparison-final.jpg`. Focused control/status comparison was separately composed and viewed: `review-controls-comparison.jpg`.
- Upper 760 px of the final/reference screenshots were byte-identical after RGB decoding. The intentional lower-right delta is the review strip and official draggable toolbar. Neither changes the layout or status-bar geometry.

Five fidelity surfaces: layout/geometry unchanged; IBM Plex typography unchanged; existing color/borders/spacing unchanged; real source icons/assets unchanged; content/state unchanged. The added review controls are visibly external tools. They occlude a small part of the lower-right grid, an intentional review-only tradeoff; the clean 4173 view remains available for unobstructed UX inspection. No broad visual sign-off for other viewports is claimed.

## Exercised workflow

1. Pin **Transform** in feedback mode without executing it. Add a clearly labeled setup note. Confirm exact target, bounds, styles and best-effort source location arrive in the official persistent store and project queue as UX-001.
2. Confirm Duxcel-specific context: viewport/DPR, Home/full, Transactions, active F7 and four mounted selection IDs, 228 px left pane, 100% UI/grid scales, scroll and bounded event trail.
3. Open Date filter in normal mode, activate **Pin feedback**, and verify the menu stays open. Pin its Sort ascending control and add UX-002; saved state retains `Sort and filter Date` in open dialogs.
4. Acknowledge UX-001 and add a setup-verification thread reply to UX-002 through the project CLI. Read both in the rendered queue. Reload the review page and confirm rejoin.
5. Restart only the owned review server, detached for handoff. Both notes, IDs, acknowledgment, context and the thread survive the server restart. Official server reports SQLite storage, not an in-memory fallback.
6. Clear only the two authored setup annotations through the toolbar. The active queue is empty; setup evidence remains under `.local/evidence/`, and deletion events retain the local mirror. First real feedback will use UX-003 or later.
7. Fix an instrumentation bug found during acceptance: Agentation initially attempted to annotate the custom Pin feedback strip when exiting. Mark the strip with the vendor's toolbar exclusion attribute. On a fresh load, verify Pin toggles **on and off** while an open Date filter stays visible.
8. Verify review-only **Ctrl+Shift+.** activates feedback from the Date search field without dismissing the menu. Verify Duxcel's **Ctrl+Shift+F** still enters Focus mode while feedback stays off, and Exit focus restores the UI. Transform opens/dismisses its real mock inspector in normal mode.

The focused browser/server verification window was approximately five minutes, including persistence restart and corrections. Survey/install/documentation time is additional. Evidence includes `reference.jpg`, `review-final.jpg`, `menu-pin.jpg`, `queue-verified.jpg`, `menu-note.json`, comparisons and source manifest. Setup images/notes are labeled tests and are not actual user design feedback.

## Limits and checks

- The reference has no tracked diff. Native source/build files were not edited by this task; concurrent native edits are unrelated. No native process was controlled and no native test binary was built or run.
- Sidecar server/CLI syntax checks and documentation whitespace checks pass. Vite successfully compiled the injected JSX. A production prototype build was not run because this is a dev-only sidecar around a read-only reference.
- No automatic screen/video capture, exact state restoration, continuous agent watching, external service, account connection or MCP registration is claimed. The vendor's “MCP Connected” UI denotes HTTP connectivity here.
- Agentation source metadata can point to an owning call site rather than the literal control implementation; selectors and surrounding context remain necessary. Official local SQLite and project context files both need backup. Browser localStorage alone expires and is not the durable record.
- Motion/timing, drag feel, calculation correctness and native parity require separate evidence. The short action trail is a reproduction aid, not deterministic replay.

final result: passed
