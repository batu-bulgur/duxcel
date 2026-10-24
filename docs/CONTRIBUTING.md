# Linear development and parallel work

`main` is the ordinary development line. Work on it directly for solo tasks and
make small commits after the affected checks pass. Do not create a branch for
an audit, a document edit, a test run or every individual implementation step.
Codex commits use command-scoped author and committer environment variables:
`Codex <codex@openai.com>`. Never change global Git identity.

## Daily verification contract

Start with `make test-feature FEATURE=<id>`; inspect its plan with
`python3 tools/test.py feature --feature <id> --plan`. Maintain the explicit
ownership/local/interaction map in `tools/test_catalog.py` with the slice. The
normal edit loop runs selected logic and one focused GUI workflow. Broader
cross-feature tests remain implemented but run only for a changed shared
boundary or an explicit milestone. `make test-milestone` retains native full
regression; browser and platform lifecycle/Open gates remain explicit. Do not
repeat all broad gates for each review revision. See [TESTING.md](TESTING.md).

Extract cohesive controllers incrementally: borrowed typed snapshots in,
explicit outcomes out; App owns readiness, jobs, focus and history. Keep product
logic in C99 and native-only AppKit ownership in the adapter. A standalone test
must link actual product modules and declare its acceptance limit. TEST-badged
launches and compiled About metadata distinguish QA from the user's instance.

## What a worktree adds

A branch is a named pointer to a commit. A worktree is another directory with
its own checked-out files, index and working state; it shares the repository’s
objects/history. A worktree can use a detached HEAD, so another permanent branch
is not required. Separate directories prevent simultaneous edits of the same
working files; branches alone do not provide that isolation.

Use at most two implementation lanes, only when actual simultaneous work needs
separate files/builds. Agree on owned files, base commit, acceptance and the
integration owner first. App/shared interfaces/Makefile have one owner. A lane
that needs the same interfaces queues its integration instead of racing them.
An explicit user-requested larger team may override the two-lane default. Record
its ownership/dependency map in ROADMAP before execution. Keep compilation and
mutable suites serialized across that team's worktrees, retain the global desktop
lock, and label every automated instance with its lane/checkout. The coordinator
alone integrates shared App/build/catalog/docs changes at verified milestones.
Read-only review needs neither a new branch nor a worktree.

For a manual parallel checkout, `git worktree add --detach <path> main` is enough.
If a Codex worktree tool requires a branch, use a temporary `codex/<feature>`
branch; remove it after integration instead of accumulating old branch pointers.
Use the app’s archive tool for worktrees attached to the current chat. For manual
checkouts, preserve work before `git worktree remove`; never force-reset another
lane or use `git clean -xfd` as routine cleanup.

After removing a checkout, remove its empty outer worktree directory too,
including a leftover zero-byte `.codex-worktree-name` marker. Check the actual
`~/.codex/worktrees/` directory as well as `git worktree list`. Refuse unexpected
contents; preserve active checkouts and other projects. Empty retired Duxcel
wrappers are unfinished cleanup.

## Integration and unfinished work

One owner reviews the exact lane commits, then fast-forwards main if possible,
otherwise cherry-picks the accepted commits. Recheck the combined source. A
component test does not establish integrated UI, engine or persistence behavior.
Keep main linear; do not create merge commits just to retain a branch name.
Remove a completed checkout and its temporary branch promptly.

Before retiring unfinished work, commit its authored changes in Git, including
any intentionally ignored source inputs, and keep the commit reachable. Record
in `ROADMAP.md` what is implemented, what passed, the remaining acceptance and
the exact Git commit/tag. Do not merge
an old whole tree merely to rescue one feature; it may restore a retired mock,
obsolete layout or previously fixed input behavior. Generated reports/captures
are not fixture inputs or active design requirements.

Git is the history store. Do not keep duplicate bundles, tarballs, patch backups
or filesystem archive directories for code/history already retained in Git.
Disposable audit/run output may be removed; it does not need another archive.

Builds/fixtures/reports are private to each checkout. Never share writable test
databases across worktrees. Independent native translation units use eight
compilation jobs by default; recursive Make shares the jobserver. Mutable engine
suites remain serial, already-built read-only tests use at most two processes,
and desktop/file-panel/clipboard/preferences/signing tests have one hidden owner.

## Recovering the retired branch inventory

`archive/streamline-20261020` is a historical tag, not a development branch or a
product integration. Its commit retains all 231 old branch tips and five dirty
worktree snapshots as ancestors. `branches.json` maps old names to commits;
`worktrees.json` names the saved snapshots and original paths. Read them with
`git show archive/streamline-20261020:worktrees.json`, then recover a particular
snapshot into a detached worktree when needed. These records and source contents
are in Git; no external backup directory is required.

The older pre-layout/pre-cleanup/legacy-review archive names are historical tags.
Keep the historical mock and old reports there; do not restore them to main.
