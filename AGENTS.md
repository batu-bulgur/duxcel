# Working on Duxcel

## Product and source of truth

- Production is C99 + raylib + DuckDB, currently macOS only. Use the existing toolchain; do not add a web runtime or dependency for the UI port.
- `design/prototype/` is the read-only, executable **exact UI definition**. Its original study instructions describe its creation, not a requirement to redesign or await approval again. Do not replace real native calculations with mock data or JSON saves.
- Start with `docs/README.md`, `docs/STATUS.md`, and `docs/ROADMAP.md`. Detailed requirements live in `docs/design/`; historical checkpoint narratives in `docs/archive/` are evidence, not the current task list.
- Preserve typed SQL NULL versus zero-length text versus literal `NULL`. Missing worksheet cells are sparse and display their distinct NULL mark. Preserve exact integer/DECIMAL text and explicit memory ownership.

## Implementation and parallel work

- Prefer focused changes in existing modules. `docs/ARCHITECTURE.md` maps ownership. Keep one integration owner for `src/app/app.c`, shared UI/platform interfaces, and the build.
- For independent parallel tasks, use agents in separate Git branches/worktrees with explicit file ownership. Initially allow at most two simultaneous compiler/engine processes on this 8 GB Mac. Read-only review can proceed concurrently.
- Native GUI, clipboard, file-panel, preferences, signing and bundle tests have a single desktop owner across worktrees. Use the test runner's desktop lock. Do not interact with or terminate a user's running app to perform tests.
- Do not share writable generated databases between worktrees. Preserve core-before-workbook fixture ordering. Never overwrite an executable while a test suite is using it.
- Review and integrate small commits; do not use broad refactors to create artificial parallelism. Never force-reset another agent's work. Do not modify global Git identity/configuration.

## Verification and reporting

- Use the smallest affected pure tests during edits, then a focused native profile for visible behavior. Run full regression at integration/release boundaries or when shared ownership/rendering changes justify it; do not repeat it after every cosmetic adjustment.
- See `docs/TESTING.md` for runnable tiers, data requirements, serialization, and provenance. Keep sanitizer coverage. Do not remove assertions or shorten animation waits merely to report faster tests.
- A helper passing tests is not an integrated feature. Report visual parity, working interaction, and analytical semantics separately. Associate evidence with the tested binary/source revision; the pre-Git frozen pass does not cover newer source changes.
- Update the one current status/roadmap, rather than copying a checkpoint into several files. Record meaningful limitations and elapsed time, not only assertion totals.
- Generated icons/brand headers are intentional checked-in derivatives. Do not hand-edit them or use `make -B`; the existing multi-output icon rule is not safe for arbitrary parallel regeneration.
