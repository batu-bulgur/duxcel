# Duxcel

A native analytical spreadsheet in **C99 + raylib + DuckDB**. CSV, Parquet and read-only DuckDB sources, manual cells, pivots and charts coexist on a worksheet. Workbooks save as `.dxls` through native macOS file panels.

The [web prototype](design/prototype/) defines the **exact interface** to implement in native code. It is a design reference, never a production dependency. The port is substantial but unfinished; [current status](docs/STATUS.md) distinguishes working features, remaining gaps and the source actually tested.

```sh
make
./duxcel
./duxcel /path/to/data.parquet
make bundle
open build/Duxcel.app
```

The current Mac already has Apple Clang, raylib, DuckDB and pkg-config. No additional installation is needed. The native bundle currently links the Homebrew libraries; it is not a standalone distribution.

- [Documentation index](docs/README.md) — all specifications, plans and evidence.
- [Roadmap](docs/ROADMAP.md) — ordered, bounded delivery milestones.
- [User guide](docs/USER_GUIDE.md) — controls, shortcuts, persistence and limits.
- [Testing](docs/TESTING.md) — fast edit loop, affected native checks and full regression.
- [Contributing](docs/CONTRIBUTING.md) — Git branches, worktrees and parallel ownership.
- [Project audit](docs/audit/2026-10-17.md) — file classification, quality findings and proposed course correction.

Sources live in `src/`, tests in `tests/`, runtime assets in `assets/`, packaging in `packaging/`, and developer utilities in `tools/`. Generated binaries, datasets, local results and prototype dependencies are ignored. Related earlier design explorations are preserved under `docs/`, with their licenses and provenance.
