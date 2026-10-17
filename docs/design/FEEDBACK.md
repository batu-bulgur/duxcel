# Pinpoint UI feedback and design changes

Survey and workflow established 17 October 2026. This is a parallel design-review lane; the production architecture remains C99 + raylib + DuckDB. The current executable reference stays read-only. The review sidecar lives in `tools/ui-review/` and is not a product dependency.

The subsequent [native review and WebAssembly feasibility analysis](NATIVE_REVIEW_FEASIBILITY.md) led to the [approved native review implementation plan](NATIVE_REVIEW_PLAN.md), now the first engineering priority. It captures feedback inside the real C99/raylib application and shares widget identities with the selected Wasm browser UI. The browser engine starts with the official DuckDB-Wasm Worker bridge. These native/browser capabilities are not yet implemented; the running 4174 workflow described below still reviews the JavaScript reference.

## Recommended methods

**Use Agentation pins for a specific control, menu, cell or region; add a short recording for a sequence, timing or drag problem.** Keep issue IDs stable across discussion, design decisions and native acceptance. This combination is our assessment for Duxcel, rather than a measured comparative benchmark. Vendor capabilities below were checked against primary documentation; only the Agentation workflow was installed and exercised here. No paid service was connected.

| Option | Strongest use | Context and agent handoff | Fit / tradeoff for Duxcel |
| --- | --- | --- | --- |
| [Agentation](https://www.agentation.com/features), [API](https://www.agentation.com/api), [MCP](https://www.agentation.com/mcp) | Pin a live element, text or area; pause motion; batch comments | Selectors, bounds, styles, best-effort React source; copy text or use local HTTP/MCP statuses/threads | Selected for the local mock. React 18+ / desktop browser; no screenshot/video output; does not instrument raylib. Source-available PolyForm Shield, not MIT. Our separate sidecar adds Duxcel state and IDs. |
| [React Grab](https://github.com/aidenybai/react-grab) | “This exact component” during conversation | Copies selected element/component stack and source locations into the agent | Small, MIT-licensed, useful for a quick one-off reference. Our assessment: less suited to an ongoing review queue; React locations do not map directly to native controls. |
| [Jam](https://jam.dev/docs/introduction) | Demonstrate a sequence, premature dismissal, animation or drag failure | Screenshots/recordings, metadata and browser diagnostic context; documentation also describes MCP debugging | Best complementary capture candidate. Browser extension and Jam for Mac exist; a Mac video can show the native app. Browser DOM/network diagnostics do not automatically describe native raylib internals. External service/account workflow; not installed here. |
| [Marker.io](https://marker.io/) | Website feedback with screenshots, collaboration and issue-tracker integration | Visual annotations, technical metadata and session replay | Good for a broader review team. More service/workflow overhead than this local design chat needs; native controls still need external capture. Not installed. |
| [BugHerd](https://bugherd.com/) | Client-style pinned feedback and task-board review | Browser/device details, project/task tracking and advertised MCP integration | Useful if multiple reviewers need a managed queue. Browser-oriented hosted workflow; not selected or installed. |
| [Pastel](https://help.usepastel.com/en/articles/1996011-reviewing-websites-in-pastel) | Share a live web canvas for browse/comment review | Comments on live website elements; browse/comment modes | Friendly for nontechnical reviewers. Its documented proxy model is a weaker fit for an unpublished localhost mock and native C UI; not selected. |
| [BugPin](https://github.com/aranticlabs/bugpin) | Self-hosted screenshot reporting and triage | Annotated screenshots, diagnostics, Markdown/JSON/AI-prompt exports | Worth revisiting if we need a private multi-project review service. Requires running its backend/admin system; community/enterprise licensing needs evaluating for that use. More operational work than the current sidecar. Not installed. |
| Screenshot + short screen recording + issue ID | Native app, canvas/WebGL, transient states and precise timing | Pixels and a narrated/action-based reproduction, plus the tested native build identity | Universal fallback and the bridge to native. Lacks automatic element/source identity; add the control's name, expected behavior and build/scale manually. It is evidence, not proof of cause or exact replay. |

No current pricing or paid plan is assumed in this recommendation. Product claims about speed/accuracy were not used as proof.

## Giving actionable feedback

Open <http://localhost:4174/>. Reach the state in normal interaction mode, then click **Pin feedback**. It preserves the currently open menu. **Cmd/Ctrl+Shift+.** is the review-only alternative; Duxcel's existing Focus shortcut is preserved. Click the relevant control, describe the issue, then **Add**. The confirmation supplies an `UX-…` ID. The queue at <http://localhost:4174/__review/> exposes the target, captured state and discussion history.

One issue should describe one behavior or decision. For example:

> [Mock bug] Did: open Amount's filter and click inside its search box. Happened: the menu closed. Expected: keep it open until Apply, Cancel, Escape, or an outside click.

That is a sample report format, not a claim that this particular bug was observed. For a gesture or animation, record the smallest sequence that shows the problem and reference the ID in chat. The note can be short; it need not prescribe the fix. Unknown classification is fine.

The sidecar saves observable UI state and a bounded action trail. It cannot restore arbitrary React state, guarantee an exact replay, capture a frame automatically, or prove calculation correctness. Its recorded selection list is capped at 12 mounted cell IDs and is a context hint, not the entire selection model. Timing/frame-rate acceptance needs its own test.

## Triage and handoff

1. **Observe.** Read the target and state, reproduce it, and keep the original note. Acknowledge the queue item when work begins. Ask a targeted follow-up only when the intended behavior remains ambiguous.
2. **Classify.** A *mock defect* contradicts already accepted interaction rules; a *design change* intentionally changes the reference behavior; a *native gap* differs from accepted behavior in the C app. A problem may need linked work in both lanes. Capture priority separately from category.
3. **Decide.** Record the desired trigger/result, keyboard equivalent, dismissal/focus rule, viewport/scale constraints and a small acceptance checklist. Keep decisions in `docs/design/`; execution order belongs only in the current `docs/ROADMAP.md`. Do not copy whole queues or checkpoint narratives into status documents.
4. **Preserve the reference.** Collecting notes does not mutate `design/prototype/`. Trial design changes in an explicitly versioned experiment and promote an accepted batch deliberately with the integration owner. Record the baseline fingerprint and new design revision so native implementation can target a known version.
5. **Verify separately.** Track design acceptance, mock interaction verification, native visual parity, native pointer/keyboard behavior and analytical semantics independently. For native changes, provide source/binary revision, fixture, viewport/UI scale and focused acceptance evidence. Do not close native work merely because a browser note was addressed.
6. **Close or retain.** Reply with the decision/evidence. Mark an annotation resolved only when its stated scope is verified; leave a linked native task open if still pending. Dismiss with a reason, preserving history. These are explicit review actions; adding a note does not authorize an automatic source rewrite or background implementation.

The local queue persists while this chat is idle. The agent reads it when asked; it does not continuously watch or wake itself. Any chat in this workspace can use the project-scoped reader documented in [the sidecar README](../../tools/ui-review/README.md). No cross-chat messages, account connection, public publishing or native desktop takeover are part of this setup.

## Future native review support

Prefer a small native debug identity/capture feature over inserting web tooling into the app: stable widget/command IDs; current view, focused control and menu path; window/framebuffer/UI/grid scales; source/binary version; fixture identity; bounded input trace; and an explicit screenshot export. Keep data values/SQL out of capture unless required and intentionally included. This is a proposed engineering task, not an implemented native feature. Route it through the App/platform integration owner and existing desktop lock.
