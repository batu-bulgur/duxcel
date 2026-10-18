# Containing selections, addresses and source ownership, 18 October 2026

Current integration `9c882ac`, source SHA256 `27cf40268b86ca57026c972e14361fab0cfa5bbaa1d7e8519a4187fe8ef09c28` (258 files). Each retained run pins its own earlier/current source, harness and binaries. This checkpoint combines separately scoped gates; it does not relabel older captures as the current binary or claim complete UI parity.

## Working interactions

- Progressive Select All uses current metadata for body → headers/body → object footprint → used sheet → whole canvas. Missing/equal/non-containing/unresolved stages are skipped. It preserves active cell, axis direction and viewport; leaving an analytical object returns to worksheet selection without restoring an unrelated bookmark. Rows/columns expand to the sheet; blank cells outside used bounds do not jump to an unrelated range. No new query or allocation is involved in the fixed-candidate helper.
- Native sanitizer 27 stages at `86f1566` pass in 14.543 s; optimized current `9c882ac` passes the same 27 in 13.999 s. The earlier pure-test marker lacked the runner's literal `PASS:`; the strict runner rejected it, then the marker was normalized with assertions unchanged.
- Actual transformed DPR2 browser selection passes 502 checks in 48.014 s at `d458293`. Real primary/Table/Pivot/manual controls build the fixture, disable metrics through actual checkboxes, then exercise trusted Control/Command+A and preserve jobs/pages/history/manual/dirty ownership. An earlier actual Meta+A attempt left the primary body unchanged: the Wasm C handler accepted Control only. The shared C browser shortcut handler now accepts Control or Super; native platform rules remain unchanged.
- Source-header row numbering matches the frozen prototype (+2); blank sheets use +1. Move, context menus, inspector and readout now share display conversion. The old Move spelling was one row below the displayed destination. Native sanitizer 45 stages at `5417aeb` pass in 25.020 s; U62:V63 → $Y$72 retains the intended model destination and Undo/Redo. Pure grid 36,801 and scope 8,835 checks pass; source A1 is header chrome, while final display row DX_SHEET_ROWS+1 round-trips.
- Current actual browser controls pass 1,185 checks in 28.126 s, including review picking/cancel, width and blank-sheet Move drafts, source/manual ownership and narrow150% clipping. This is not browser source-Move Apply or complete OS-input acceptance.

The browser fixture narrative uses conventional **model row +1** labels such as A1:C6/Table B13:D20/Pivot I1:K8. Those describe stored bounds, not the source-header UI's displayed row numbers. The retained snapshots contain the actual zero-based model coordinates; screenshots retain the approved +2 display convention.

## Model and verification prerequisites

At `9c882ac`, the focused combined gate took 53.492 s including builds. Optimized and ASan/UBSan both pass: format 43,479; canonical scope 8,835; Page 177; native record Pages 81,635; source-version ownership236; existing workspace-format635; worker verifier1,249. Exact terminal summaries were checked in addition to exit codes.

- Native raw Pages capture exact file metadata around preparation/execution/count; later digest binding can compare every named field. Between-Page metadata change or raw identity loss invalidates publication even when the old cache stamp is unchanged. This detects observations, not hostile transactional mutation.
- Strict known-type raw/result packets preserve exact schema, reader/engine version, SHA256, physical IDs and typed filter definitions. Derived numeric filter bindings must match the captured schema. Owned ledgers retain lifetime counters outside Analysis Undo and deep-copy through worksheet snapshot/duplicate.
- Record edits can be planned without modifying live formatting/history and committed after all owners preflight. Unsafe mixed moves remain explicitly rejected.
- Current v12/v13 saves reject source-bound state before touching a destination. A v14 codec, guarded legacy migration, global App history/staging reservations, verification cache and live renderer/command producer remain open. **Ordinary App formatting still uses coordinates; these prerequisites do not make styles follow records.**

Current official Worker:123,713 checks/187 stages; 11.511 s browser,18.629 s including build/setup. The three workload inventories remain192 jobs/402 SQL submissions/21 counts. Native verification is explicitly unsupported in browser without changing cached query state. No browser source verification/persistence claim.

## Integrated review and runnable artifacts

Current hidden native sanitizer base+workspace passes10 stages in8.625 s total. Current optimized native review passes400 stages in17.613 s and saves three frame-bound reports. The earlier402a03d sanitizer400 pass retains its own scope; no new sanitizer400 pass is claimed here.

Current linked native release/sanitizer and normal/review Wasm production isolation passes, including absence of the isolated source-digest checkpoint. The signed build/Duxcel.app was refreshed without launching; package hashes/signature checks are retained. The only persistent project origin127.0.0.1:8767 now serves accepted review artifact b211d4fe5acd72155290 (normal86f7360d3214a00b00ea), retaining the persistent queue. No user tab was refreshed. Private browser processes/ports were cleaned up.

Physical gestures, OS menus/panels/IME, broader browsers and full matching-state UI parity remain separate. The next ownership audit found old ObjectView allocations retained across opening a new source; that fix follows this checkpoint and is not covered here.
