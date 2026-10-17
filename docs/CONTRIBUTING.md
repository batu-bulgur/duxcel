# Git and parallel development

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

Before dispatch, agree on files owned, feature acceptance, base commit and verification scope. Start with two active implementation/build lanes on this Mac. App, shared UI interfaces, Makefile and integration documents have one owner; changes there require coordination. Pure widgets, transforms and workbook fixes can proceed independently. A feature needing the same App section should queue integration, not race another agent's edit.

Build outputs, mutable test fixtures and logs stay inside each worktree's ignored directories. Do not symlink writable `tests/generated` between them. Large downloaded inputs can be shared read-only. Native windows still share the desktop, clipboard and preferences: serialize their tests using the runner's lock even across branches. Bundle/signing/manual UI acceptance also belongs to that owner.

Integrate a reviewed commit, run its affected gate on the combined source, and update current status once. A clean merge does not prove behavior. Full regression belongs at a coherent integration boundary; its results must identify the source and binary tested. Preserve work before cleanup, never force-reset another branch, and never use `git clean -xfd` as routine tidying: ignored data/results may be valuable.
