# Incremental builds and the narrow ribbon

Scope ends at **09ce3de**, 17 October 2026. Later More-menu work is outside this checkpoint. No fresh full-inventory, whole-interface or physical-input acceptance is claimed.

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
