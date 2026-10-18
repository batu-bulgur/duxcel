# Incremental builds and the narrow ribbon

Historical checkpoint. Machine-generated payloads were removed from the current checkout on 18 October 2026; they remain recoverable at Git ref `archive/pre-cleanup-20261018`. Paths and measurements below describe that original run. See the [retention policy](../../README.md).

Build/collapse acceptance ends at **09ce3de**, followed by separately recorded More-menu integration **ff70638**, 17 October 2026. No fresh full-inventory, whole-interface or physical-input acceptance is claimed.

## Build acceptance

Incremental implementation `785b5d5` isolates application release/sanitizer objects and fixture-helper objects. Compiler dependencies and command signatures, missing-sidecar repair, atomic artifact replacement and one coordinated icon repair passed **44 isolated build-contract checks**: **12.705s** on integration; **12.540s** in the implementation worktree. The integration result is an explicitly transcribed execution record in `root-build-contract.json`, not a retained raw log. That record and `incremental-build.json` include the exact Makefile/helper hashes.

Module test commands and fixture execution order are unchanged; `.NOTPARALLEL` remains. The contract invokes `make -j4` with a nonblocking compiler lock to detect accidental overlap. This validates retained serialization, not arbitrary simultaneous Make processes in one worktree. No native application or DuckDB fixture runs were needed for these build checks.

| Build observation | Previous monolithic | Incremental |
| --- | ---: | ---: |
| Development app cold, `-O0 -g` | 3.159s | 5.317s |
| Development app warm | 0.123s | 0.199s |
| Development app widget edit | 2.734s | 0.259s |
| Sanitizer app cold, O1 ASan/UBSan | — | 31.424s |
| Sanitizer app warm / widget | — | 0.206s /0.263s |
| Isolated fixture helper cold / warm, O0 | — | 1.279s /0.177s |

These are individual local observations using Apple clang 21.0.0 and GNU Make 3.81. Cold means missing build objects, not a cold OS cache or idle-machine benchmark. A widget edit was forced with `-W src/ui/selectview.c`; only that object's timestamp changed in the incremental release tree. Initial compiler startup per file makes the cold build slower. Valid canonical/generated asset bytes were unchanged; the 56-derivative icon check passed. The benchmark manifest preserves commands and configurations; no O2 performance result is implied.

Installed Make 3.81 compares whole-second mtimes. Same-second source/header/flag changes can be missed; contract mutations cross timestamp boundaries. Explicitly forcing an affected source or cleaning its configuration remains necessary when freshness is uncertain. Configuration signatures do not resolve this underlying limitation.

## Narrow-ribbon acceptance

`ribbon-collapse.json` preserves the original hidden affected-run manifest: **18 application stages [620,638)** passed in **10.877s**, **13.171s total**. Its **477-check** Cocoa/background probe took **1.541s**. The application used default O1 ASan/UBSan, confirmed by the integration owner and retained compile signature; this differs from the earlier O0 application checkpoint.

The run began on dirty `a104116`; its App/controller edits were then committed as `09ce3de`. The preserved manifest, rather than a reconstructed clean-commit claim, records source SHA-256 **6adb40ff79b4a1bb34091fbec0e2ccc16b75f1615749ce47de30e9b47f644e03** and application binary SHA-256 **802f88388f6fd691c03ea251aed94acd000cd4af69df509eddfd3a0b13c21563**, unchanged after execution. `ribbon-grid.json` records **20,362 assertions per release/sanitizer configuration**; the same sanitizer grid binary was reused in the native gate.

Accepted scope: reserved dock layout, collapsed group geometry/icons/type and Alignment retention, pointer flyouts, Escape focus, live ribbon/status above the narrow drawer, and Full/Compact/Icons/Focus resizing with unchanged source/result caches and no idle SQL changes. `ribbon-evidence.txt` contains verbatim terminal evidence and hashes of the complete local logs. Full Retina captures remain under ignored `tests/results/`; they are not archived wholesale here.

Hidden renderer/controller checks do not establish physical trackpad/IME delivery or native file-panel behavior. Selector option popups, More-menu anatomy, shared dialogs and the remaining matching-state matrix are still open. The **1,071-stage inventory through 09ce3de** has not passed as one coherent new full run; the earlier 183-stage selection/control acceptance retains its own source/binary boundary.

## More-menu integration

The 240-unit ActionMenu uses the reference title, group-command labels/icons, conditional separator and history rows; it has no Close X. More remains present below 700 logical units in Icons mode even when all groups fit. Real Undo/Redo retains source/PIVOT pages and exits historical preview before changing live history. A More-origin group returns to the list when its anchor is clicked; Escape focuses More, and Enter/Space reopens it. Matching reference/native captures were inspected at 640×640 with 150% UI scale. The file title and enabled history reflect each real fixture/session; they are not copied mock values.

`ribbon-overflow.json` records the fresh default O1 ASan/UBSan binary at **ff70638** passing **27 stages [638,665)** in **13.789s**, **16.520s total**, including the 477-check hidden probe. The exact runtime completion range is now mandatory for layout acceptance. All **16 runner checks** passed in **0.380s**; `range-guard.json` also records rejection of the actual stale-binary log.

A prior no-build run (`20261017T120950-affected-18937`) reused an older binary, so its expected-27 summary is **not accepted**. Its raw artifacts and explicit invalidation note remain local. A fresh rebuild passed the new 27 stages before the stricter guard was integrated, then passed again with the new guard. `--no-build` still requires caller verification of other source freshness.

This closes the More-menu interaction slice, not the whole exact UI port. Group flyouts still differ in initial Close-button focus; direct group-anchor replacement and one-click transitions to other chrome need further matching-state checks. Selector popups, shared dialogs, minor shared-menu box/shadow details and the remaining full state matrix remain open. Analytical semantics were not expanded by this presentation work.

Final adjacent acceptance uses the **same ff70638 source digest and application binary** across five nonoverlapping slices: More27 **13.789s**, original selection/ribbon64 **33.292s**, criteria/selects24 **12.650s**, source controls22 **12.081s**, and narrow collapse18 **9.729s**. This is **155 focused application stages**; their individual complete gate times were **16.520 /35.145 /14.561 /13.440 /11.312s**. Each also passed the 477-check background probe. These are scoped integration records, not a fresh 1,098-stage full pass or physical OS input acceptance.

Common source digest: `dd9f34ad772796c39cbf70166eb3bff957dc9baab9ccf99c9c19df4956597603`; application SHA-256: `27c04b9279d3576aeddf82e0b628fcc2e9fc2b6b4c17e587e046f6884147fb64`. Source, binary, exact runtime ranges and shutdown checks were stable in every retained manifest.
