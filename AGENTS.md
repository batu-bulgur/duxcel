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

- Build products, generated databases, captures and logs belong under ignored `build/`. Keep downloaded dependencies in `.cache/`, reference data in ignored `data/downloads/`, and checkout locks in `.state/`. Ordinary `make` produces `build/bin/duxcel`; `make bundle` produces `build/Duxcel.app`.
- `make clean` removes all of `build/`; `make distclean` also removes dependency caches and `web/node_modules`. Both preserve downloaded reference data, authored inputs, lock inodes and user workbooks. Migrate legacy inputs safely; refuse conflicting copies. Do not recreate root/test executables.
- Prefer existing modules. Keep one integration owner for App/shared UI interfaces/build. At most two active implementation lanes in isolated branches/worktrees. Compile independent native translation units with eight parallel jobs by default (`BUILD_JOBS=8`); recursive makes share that jobserver. This supersedes the old two-compiler cap. Keep mutable engine suites serial, at most two already-built read-only tests, and one desktop owner on this 8 GB Mac. Read-only review may run concurrently.
- Review small commits, integrate accepted work and remove completed clean checkouts while retaining refs. Preserve unfinished work. Never force-reset another branch or change global Git identity.
- Attribute Codex commits to `Codex <codex@openai.com>` as author and committer using environment variables only on the commit command.

## Verification

- Use focused C tests during edits, then the relevant real native/browser workflow. Retain sanitizer coverage, meaningful assertions, completion checks and animation waits. Full regression belongs at integration boundaries, not after each cosmetic edit.
- Native desktop/file-panel/clipboard/preferences/signing tests have one owner across worktrees and use the runner's desktop lock. Keep automated windows hidden. Do not interact with or terminate the user's app. Never overwrite a binary being tested.
- Generated databases are private to each worktree; preserve core-before-workbook fixture ordering. Never share writable fixtures across worktrees.
- A component pass does not establish an integrated feature. Distinguish visual behavior, actual input, analytical correctness and persistence; record tested source/binary and elapsed time with explicit limits.
- Preserve testing and persistence work. Remove obsolete generated artifacts rather than reducing coverage. Default evidence belongs in ignored `build/tests/results/`; retain concise current results, not full tracked screenshot/log capsules. Git history holds retired material.
- Use C99 for product semantics and test helpers linking product APIs (`tests/support/`); Python for external process/file/network/build/review orchestration (`tools/`); JS for browser platform/Worker glue (`web/`). Tool tests belong in `tools/tests/`, browser tests/drivers/probes in `web/tests/`. Native tests/finite fixture inputs belong in `tests/`. Do not choose languages or locations by session; exceptions require a concrete API/platform/performance reason. Preserve existing meaningful coverage rather than rewriting to change language percentages.
- Fixtures must have a schema/purpose and a named consumer. Historical commands, manifests, elapsed-time/pass reports and captures are disposable outputs, not fixture inputs. See `docs/REPOSITORY.md` and `tests/README.md`.
- Icons/brand headers are intentional checked-in derivatives with licenses/provenance. Do not hand-edit them or use `make -B`; regenerate only through the existing explicit generator.
