# Native tests and inputs

C/Objective-C `test_*` sources exercise the real native product API, ownership,
engine and renderer. They are compiled only by test targets. Product sources
stay in `src/`; generated programs/databases/reports stay in `build/tests/`.

| Location | Role | Retention |
| --- | --- | --- |
| `test_*.c`, `test_*.inc`, `test_*.m` | Native suites and their implementation fragments | Tracked source |
| `support/create_workbook.c` | Production-linked canonical/large workbook fixture generator and assertions | Tracked C test helper |
| `fixtures/*.csv` | Seven tiny input datasets: NULL/text/quoting/sort/chart cases | Tracked authored inputs |
| `fixtures/ui/*.json` | Versioned expected UI geometry/scroll/paint contracts | Tracked finite regression inputs |
| `fixtures/rendering/*.h` | C pixel-array oracles used by native paint tests | Tracked test input; preserve values |
| `../tools/tests/` | Python tests for automation, queues, packaging and cleanup | Tracked tool tests |
| `../web/tests/` | Browser adapter tests, drivers and probe/host support | Tracked browser tests |

A fixture is an input with a named consumer, not a synonym for a previous run.
Session manifests, commands, elapsed-time reports, captures and event dumps are
outputs and never belong in fixtures. They are ignored under `build/tests/`.

The workbook helper is C because it links the same Workbook/DuckDB API and must
preserve exact DECIMAL, NULL/empty text, PIVOT and memory ownership. Python runs
it as a separate executable; no Python runtime or FFI is injected into the app.
The public `make test-fixture` target verifies both optimized and sanitizer builds.
Legacy dataset basenames remain stable so existing scenario inputs do not change.
