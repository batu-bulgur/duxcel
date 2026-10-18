# Git and parallel development

**Current priority, 18 October 2026:** audit/cleanup and deliver a clean native bundle, then reproduce remaining physical pan/selection bugs in the actual user-launchable app before more metadata components. Browser Open/Save and complete UI/physical-input parity remain unfinished. Follow the current delivery note in [STATUS.md](STATUS.md), rather than treating an earlier test or service pointer as the latest app.

The repository starts locally on `main`. `snapshot/pre-audit-2026-10-17` preserves the initial source/design state; it is not a release. No remote has been configured or anything published. Commits made by the assistant use a command-scoped Codex author identity, leaving the user's global configuration untouched.

Use short branches such as `ui/filter-labels`, `grid/active-readout`, `engine/chart-other`, or `tooling/incremental-build`. A branch is an independent history; a **worktree** is a separate checkout where an agent can edit/build it without sharing another agent's working files.

```sh
git status --short
git worktree add -b ui/filter-labels ../duxcel-filter-labels main
# Work, verify and commit inside that checkout.
git log --oneline main..ui/filter-labels
git merge --no-ff ui/filter-labels
# After integration, remove a clean manual worktree:
git worktree remove ../duxcel-filter-labels
```

Codex-managed worktrees should be created/archived with the app's worktree tools so their recoverable snapshots and attachment metadata stay intact. The first two independent lanes were exercised for test tooling and an optional-error fix. Source/design audit could run in parallel read-only without a checkout.

Before dispatch, agree on files owned, feature acceptance, base commit and verification scope. Keep at most two active implementation lanes on this Mac. App, shared UI interfaces, Makefile and integration documents have one owner; changes there require coordination. A feature needing the same App section should queue integration, not race another agent's edit. Remove an integrated clean checkout promptly once its commits are reachable through retained refs; keep unfinished or dirty work isolated and recoverable.

Build outputs, mutable test fixtures and logs stay inside each worktree's ignored directories. Do not symlink writable `tests/generated` between them. Large downloaded inputs can be shared read-only. Native windows still share the desktop, clipboard and preferences: serialize their tests using the runner's lock even across branches. Bundle/signing/manual UI acceptance also belongs to that owner.

Keep evidence concise and source-scoped: describe the behavior, tested revision/binary and remaining limits. Do not commit a full payload capsule for every metadata slice. Generated databases, binaries, event dumps and repeated machine reports remain ignored. Cleanup preserves checkpoint README history, the four pinned runtime-reference providers and the recoverable Git archive ref `archive/pre-cleanup-20261018`; it does not duplicate those archived payloads into new capsules.

Integrate a reviewed commit, run its affected gate on the combined source, and update current status once. A clean merge does not prove behavior. Full regression belongs at a coherent integration boundary; its results must identify the source and binary tested. Preserve work before cleanup, never force-reset another branch, and never use `git clean -xfd` as routine tidying: ignored data/results may be valuable.
