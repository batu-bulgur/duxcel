# Native tests and inputs

C/Objective-C `test_*` sources exercise the real native product API, ownership,
engine and renderer. They are compiled only by test targets. Product sources
stay in `src/`; generated programs/databases/reports stay in `build/tests/`.

| Location | Role | Retention |
| --- | --- | --- |
| `test_*.c`, `test_*.inc`, `test_*.m` | Native suites and their implementation fragments | Tracked source |
| `test_fixture.c` | Native load/PIVOT/typed data/save/reload acceptance of canonical/large inputs | Tracked C test |
| `fixtures/workbook/transactions.sql` | Finite v12 four-sheet workbook input for the Python generator | Tracked SQL test input |
| `fixtures/*.csv` | Seven tiny input datasets: NULL/text/quoting/sort/chart cases | Tracked authored inputs |
| `fixtures/ui/*.json` | Versioned expected UI geometry/scroll/paint contracts | Tracked finite regression inputs |
| `fixtures/rendering/*.h` | C pixel-array oracles used by native paint tests | Tracked test input; preserve values |
| `../tools/tests/` | Python tests for automation, queues, packaging and cleanup | Tracked tool tests |
| `../web/tests/` | Browser adapter tests, drivers and probe/host support | Tracked browser tests |

A fixture is an input with a named consumer, not a synonym for a previous run.
Session manifests, commands, elapsed-time reports, captures and event dumps are
outputs and never belong in fixtures. They are ignored under `build/tests/`.

`tools/create_workbook.py` generates synthetic CSV, converts it to Parquet with
the existing DuckDB CLI and creates the fixed workbook from the finite SQL input.
There is no C generator or Python package installation. The native test loads
those files, checks real product PIVOT/NULL/empty/literal text/exact DECIMAL, then
saves and reloads the workbook. `make test-fixture` checks optimized and sanitizer
paths; generation and product acceptance are distinct. Dataset basenames stay
stable for the existing native/browser scenarios.
