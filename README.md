# Duxcel

A native analytical spreadsheet in **C99 + raylib + DuckDB**. CSV, Parquet and read-only DuckDB sources, sparse manual cells, same-sheet pivots and charts share a workbook. Native workbooks use `.dxls`.

```sh
make
./build/bin/duxcel
./build/bin/duxcel /path/to/data.parquet
make bundle
open build/Duxcel.app
```

The Mac build uses Apple Clang, raylib, DuckDB and pkg-config. The bundle currently links Homebrew libraries; standalone distribution remains unfinished. `make` builds the native app; browser builds, tests and review services are explicit separate targets. No server starts automatically.

The former JavaScript mock is retired. The current native UI and [written requirements](docs/REQUIREMENTS.md) define the baseline. The optional browser product compiles this same C UI into Wasm; `web/` contains its platform/Worker glue and tests, not another spreadsheet interface.

- [Documentation](docs/README.md)
- [Current status](docs/STATUS.md) and [roadmap](docs/ROADMAP.md)
- [Architecture](docs/ARCHITECTURE.md) and [repository audit](docs/REPOSITORY.md)
- [Testing](docs/TESTING.md), [user guide](docs/USER_GUIDE.md), [contributing](docs/CONTRIBUTING.md)

`make clean` removes all of `build/`. `make distclean` also removes dependency caches (`.cache/`, `web/node_modules`). Downloaded reference data in `data/downloads/`, authored fixtures, assets/licenses and user workbooks remain. See [repository boundaries](docs/REPOSITORY.md) for language and test ownership.
