# Local UI review sidecar

The unmodified reference runs at <http://localhost:4173/>. The feedback-enabled view runs at <http://localhost:4174/>; its queue is <http://localhost:4174/__review/>. Both render `design/prototype/` directly. This tool injects Agentation alongside the reference at serve time. No prototype files, native build dependencies or native processes are changed.

## Giving feedback

1. Use the UI normally to reach the problem. Leave feedback mode off while trying gestures.
2. Click **Pin feedback** in the small review strip. This control preserves open menus. **Cmd/Ctrl+Shift+.** also toggles pinpoint mode without an outside click. The black Agentation button works for ordinary elements too.
3. Click the element and write a note. For behavior: **Did → Happened → Expected**. Prefix `[Mock bug]`, `[Design change]`, or `[Native gap]` if useful; classification is optional.
4. Click **Add**. The strip confirms an ID such as `UX-003 saved`. Open **Queue** to see the saved target and state. Marker numbers are session-local; `UX-…` is the durable reference to use in chat.
5. Exit pinpoint mode to resume testing. Say “review the pending feedback” in this chat, or name an issue ID. This session does not automatically wake or modify code when a note arrives.

For motion/drag issues, add a short recording with the same ID. Agentation is text/DOM annotation, not video or deterministic replay. Screenshot/recording attachments are manual in this version. No annotation tool here can inspect native raylib widgets.

## Agent operation

From the repository root, using the existing Node runtime:

```sh
node tools/ui-review/feedback.mjs list
node tools/ui-review/feedback.mjs show UX-003
node tools/ui-review/feedback.mjs status UX-003 acknowledged
node tools/ui-review/feedback.mjs reply UX-003 "Observed; native acceptance is still pending."
```

Before changing a design, read the note, surrounding UI and captured state, then distinguish a mock defect from an intended design change and a native implementation gap. The source line is best-effort metadata; verify the control by selector, accessibility name and context before editing. Marking an annotation resolved does not establish native parity. Apply the [feedback policy](../../docs/design/FEEDBACK.md).

## Running / storage

```sh
cd tools/ui-review
npm ci
npm start
```

The server uses Vite/React from the prototype's existing installation; install its existing lockfile if that installation is missing. Agentation `3.1.2` and Agentation MCP server `1.3.2` are separately locked in this tool. The MCP package uses a local SQLite binding. This is development tooling, never part of Duxcel's C99 build or bundle. Both packages declare `PolyForm-Shield-1.0.0`; React Grab's MIT license is a different license. See their shipped licenses.

- UI review binds to loopback port 4174. Official Agentation HTTP API binds to loopback port 4747. Do not expose either publicly.
- Official annotations, statuses and threads persist in `~/.agentation/store.db`; this is the vendor's local storage default, shared by local Agentation sessions. This queue filters to the review origin. No global Codex/MCP configuration is modified.
- Duxcel-specific issue IDs, state, the last 24 actions and a local annotation mirror live in `.local/feedback.json`. `.local/` is Git-ignored. Back up **both** stores to preserve notes and their Duxcel context. Browser storage is a convenience, not the only copy.
- Captured context: viewport/DPR, source fingerprint/revision, active sheet/ribbon/tab, UI/grid scales, visible selection IDs, panes, shelves, scroll position, open dialogs, bounded click/navigation/command trail. No typed text, clipboard contents, network payloads, continuous pointer trace or screen video are recorded by the sidecar. Agentation itself reads target DOM text/styles to identify the element. Use synthetic data.
- Source fingerprint covers `src/`, `public/`, `index.html` and the reference lockfile at server start. Restart after changing that source; reference immutability is the default. HMR is only a convenience for sidecar development.
- This HTTP connection is available to agents through the project CLI now. The official toolbar's “MCP Connected” label means its server is reachable; **no MCP tools have been registered in this Codex session and no watch loop is running**. Official MCP registration is optional later, after validating the useful capture workflow.
- To connect an MCP client later, use the pinned local CLI's `server --mcp-only --http-url http://127.0.0.1:4747` command. The agent starts only the stdio bridge, not a second HTTP server. Do not edit a user's global MCP setup automatically.

No native tests are run by this sidecar; the native desktop owner and test lock remain unchanged.
