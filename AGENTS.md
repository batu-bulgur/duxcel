# Working on Duxcel

## Current human priority — 18 October 2026

- Repository/worktree cleanup and a clean native delivery are complete. Continue the full roadmap, including persistence internals and tests; they remain essential work even when changes are not visible in the GUI.
- Address the remaining physical pan/selection bugs alongside persistence correctness in independent bounded lanes where ownership permits. Keep focused sanitizer and integration coverage. Automated callbacks, hidden tests and browser simulations do not establish physical-input parity.
- Browser workbook Open/Save remains unavailable; complete UI and physical-input parity have not been achieved.

## Product and source of truth

- Product UI and interaction logic remain C99 + raylib, with DuckDB for computation. macOS is the currently validated native platform; retain portable native boundaries for Linux/Windows. Do not add a web runtime to the native app.
- The approved browser target compiles the same C99/raylib UI to WebAssembly. Start browser engine integration with the official DuckDB-Wasm asynchronous Worker behind a thin typed bridge; minimal browser loader/platform glue is allowed, a second JavaScript spreadsheet UI is not. Pin browser/toolchain dependencies in isolated development tooling when that milestone starts.
- Prioritize the native agentic review build before further broad GUI refinement. Follow `docs/design/NATIVE_REVIEW_PLAN.md`: portable semantic widget IDs/registry, same-frame capture and owned bounded state/trace, native feedback composer, and external agent/queue adapter. Exclude all new review code/data/commands/bridges from production at compile/link time, rather than relying on a runtime switch. Keep review/sanitizer/release objects and binaries isolated.
- `design/prototype/` is the read-only, executable **exact UI definition**. Its original study instructions describe its creation, not a requirement to redesign or await approval again. Do not replace real native calculations with mock data or JSON saves.
- Start with `docs/README.md`, `docs/STATUS.md`, and `docs/ROADMAP.md`. Detailed requirements live in `docs/design/`; historical checkpoint narratives in `docs/archive/` are evidence, not the current task list.
- Preserve typed SQL NULL versus zero-length text versus literal `NULL`. Missing worksheet cells are sparse and display their distinct NULL mark. Preserve exact integer/DECIMAL text and explicit memory ownership.

## Implementation and parallel work

- Prefer focused changes in existing modules. `docs/ARCHITECTURE.md` maps ownership. Keep one integration owner for `src/app/app.c`, shared UI/platform interfaces, and the build.
- Keep at most two active implementation lanes in separate Git branches/worktrees with explicit file ownership, and at most two simultaneous compiler/engine processes on this 8 GB Mac. Read-only review can proceed concurrently. Remove integrated clean checkouts promptly while keeping their refs; preserve unfinished or dirty work separately.
- Native GUI, clipboard, file-panel, preferences, signing and bundle tests have a single desktop owner across worktrees. Use the test runner's desktop lock. Do not interact with or terminate a user's running app to perform tests.
- Do not share writable generated databases between worktrees. Preserve core-before-workbook fixture ordering. Never overwrite an executable while a test suite is using it.
- Review and integrate small commits; do not use broad refactors to create artificial parallelism. Never force-reset another agent's work. Do not modify global Git identity/configuration.
- Attribute every Codex-created commit to `Codex <codex@openai.com>` as both author and committer. Set `GIT_AUTHOR_NAME`, `GIT_AUTHOR_EMAIL`, `GIT_COMMITTER_NAME`, and `GIT_COMMITTER_EMAIL` only for the commit command; preserve the user's configured identity for their own commits.

## Verification and reporting

- Use the smallest affected pure tests during edits, then a focused native profile for visible behavior. Run full regression at integration/release boundaries or when shared ownership/rendering changes justify it; do not repeat it after every cosmetic adjustment.
- See `docs/TESTING.md` for runnable tiers, data requirements, serialization, and provenance. Keep sanitizer coverage. Do not remove assertions or shorten animation waits merely to report faster tests.
- A helper passing tests is not an integrated feature. Report visual parity, working interaction, and analytical semantics separately. Associate evidence with the tested binary/source revision; the pre-Git frozen pass does not cover newer source changes.
- Update the one current status/roadmap, rather than copying a checkpoint into several files. Record meaningful limitations and elapsed time, not only assertion totals.
- Keep generated output ignored. Do not commit a full machine-payload capsule for each metadata slice; retain concise source-scoped summaries and only the small evidence needed to explain a concrete result or failure. Preserve historical README records, archive refs and pinned runtime-reference providers during cleanup.
- Generated icons/brand headers are intentional checked-in derivatives. Do not hand-edit them or use `make -B`; the existing multi-output icon rule is not safe for arbitrary parallel regeneration.
