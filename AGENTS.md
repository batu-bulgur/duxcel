# Working on Duxcel

## Product and current work

- Product UI, interactions and analytical logic use C99 + raylib + DuckDB. Native macOS is the validated platform; preserve portable native boundaries. Objective-C is confined to the macOS adapter and its integration tests. No web runtime belongs in the native app.
- The browser target compiles the same C UI to Wasm. Keep the official asynchronous DuckDB-Wasm Worker and the minimum DOM/file/clipboard/WebGL bridge in `web/`; no second spreadsheet UI. Browser workbook Open/Save remains incomplete.
- The JavaScript mock and visual explorations are retired and removed. Current native behavior plus explicit requirements in `docs/` define the baseline. Do not restore an executable mock or require pixel matching against it. Small accepted regression inputs under `tests/fixtures/ui/` are tests, not a design authority.
- Continue the roadmap, including persistence internals and tests. Native pan/snap-back is user-confirmed resolved on the current Mac; revisit only with a new reproduction. Broader browser/pinch/Space coverage remains separate.
- Start with `docs/README.md`, `docs/STATUS.md`, `docs/ROADMAP.md` and `docs/ARCHITECTURE.md`. Keep active decisions there, not in dated audit/checkpoint copies.
- Preserve SQL NULL, zero-length text and literal `NULL` distinctly, including sparse missing cells and exact integer/DECIMAL text. Memory ownership must be explicit.
- Keep review widgets/capture/queue bridges compile/link excluded from production. Native review is implemented infrastructure, not a reason to postpone all other product work. See `docs/design/NATIVE_REVIEW_PLAN.md` for its ownership boundaries.

## Layout and ownership

- All executables, object files, generated databases, captures and logs belong under ignored `build/`. Authored tests and small fixtures stay in `tests/`. Ordinary `make` produces `build/bin/duxcel`; `make bundle` produces `build/Duxcel.app`.
- `make clean` removes disposable outputs while retaining the installed web SDK and public-data cache. `make distclean` also removes downloaded caches/dependencies. Neither may delete authored inputs or user workbooks. Do not recreate legacy root/test executables.
- Prefer existing modules. Keep one integration owner for App/shared UI interfaces/build. At most two active implementation lanes in isolated branches/worktrees and two simultaneous compiler/engine processes on this 8 GB Mac. Read-only review may run concurrently.
- Review small commits, integrate accepted work and remove completed clean checkouts while retaining refs. Preserve unfinished work. Never force-reset another branch or change global Git identity.
- Attribute Codex commits to `Codex <codex@openai.com>` as author and committer using environment variables only on the commit command.

## Verification

- Use focused C tests during edits, then the relevant real native/browser workflow. Retain sanitizer coverage, meaningful assertions, completion checks and animation waits. Full regression belongs at integration boundaries, not after each cosmetic edit.
- Native desktop/file-panel/clipboard/preferences/signing tests have one owner across worktrees and use the runner's desktop lock. Keep automated windows hidden. Do not interact with or terminate the user's app. Never overwrite a binary being tested.
- Generated databases are private to each worktree; preserve core-before-workbook fixture ordering. Never share writable fixtures across worktrees.
- A component pass does not establish an integrated feature. Distinguish visual behavior, actual input, analytical correctness and persistence; record tested source/binary and elapsed time with explicit limits.
- Preserve testing and persistence work. Remove obsolete generated artifacts rather than reducing coverage. Default evidence belongs in ignored `build/tests/results/`; retain concise current results, not full tracked screenshot/log capsules. Git history holds retired material.
- Prefer C for product semantics, fixtures and native test assertions. Migrate orchestration only with equivalent timeout, locking, failure and recovery coverage. Keep genuine browser/platform boundary tests.
- Icons/brand headers are intentional checked-in derivatives with licenses/provenance. Do not hand-edit them or use `make -B`; regenerate only through the existing explicit generator.
