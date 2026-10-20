# Project documentation

The native C99 application and explicit written requirements define the product. The retired JavaScript mock, visual explorations and historical checkpoint trees are no longer part of the checkout or agent instructions. Recover historical material from Git at `archive/pre-layout-cleanup-20261018` when investigating an actual regression.

All project documentation, source code, code comments and developer-facing text must be in English, regardless of the language used in conversation.

## Scope of design documents

`docs/design/` means detailed software and product design, including data models, persistence, identity, algorithms, interaction and visual contracts. It is not restricted to UI appearance. `DESIGN.md` and `INTERACTIONS.md` own the visual/interaction subset; `WORKBOOK_MODEL_TRD.md`, `BROWSER_PERSISTENCE.md` and `SOURCE_FORMAT_IDENTITY.md` own detailed technical designs. `ARCHITECTURE.md` remains the system-wide ownership and implementation overview, `REQUIREMENTS.md` the concise human decision record, and `ROADMAP.md` the scheduled/dependency work. A document's folder does not indicate whether its proposals are implemented or accepted.

## Where each kind of information belongs

Start with STATUS → ROADMAP → the relevant feature/design document → ARCHITECTURE and source code. For Excel behavior, find the feature or shortcut ID in [EXCEL_SHORTCUTS.md](EXCEL_SHORTCUTS.md). Each document owns the question below; other documents should link to it instead of duplicating its details.

| Document | Question answered / information owned | Information that belongs elsewhere |
| --- | --- | --- |
| [STATUS.md](STATUS.md) | Which capabilities exist now, on which platform, with what limits? | Detailed feature checklists or future design decisions |
| [ROADMAP.md](ROADMAP.md) | What comes next, with which dependencies and batch acceptance gates? | Every shortcut record or test-run journal |
| [EXCEL_SHORTCUTS.md](EXCEL_SHORTCUTS.md) | Which intent/key maps to which feature? Feature status, platform binding, UI placement, icon/appearance notes and behavior cards | A commitment to implement every Excel feature or the entire product roadmap |
| [REQUIREMENTS.md](REQUIREMENTS.md) | Which product behavior has the user requested or accepted? | Treating an existing implementation as human acceptance |
| [ARCHITECTURE.md](ARCHITECTURE.md) | How do UI, model, worker/SQL, result memory, rendering and persistence connect? What exists and what is proposed? | Daily work queues or presenting proposals as implemented capabilities |
| [REPOSITORY.md](REPOSITORY.md) | Where do source, tools, fixtures and outputs belong? What are the cleanup and language boundaries? | A second complete copy of test commands |
| [TESTING.md](TESTING.md) | How are checks run? What are fixture ordering, sanitizer, desktop ownership and evidence limits? | Product usage instructions or screenshot/log archives |
| [USER_GUIDE.md](USER_GUIDE.md) | How does the current application work, including controls and user-visible limits? | Future shortcut targets, binary hashes or running-service journals |
| [CONTRIBUTING.md](CONTRIBUTING.md) | What are the main-line, parallel-worktree, review and integration rules? | Dated product priorities or assumptions about current Git/service state |
| [design/DESIGN.md](design/DESIGN.md) | What are the workspace, ribbon/group, visual-language, measurement and responsive-layout targets? | Implementation/test status for individual shortcuts |
| [design/INTERACTIONS.md](design/INTERACTIONS.md) | How should focus, selection, editing, popups, panes and analytical actions behave? | A second complete shortcut inventory |
| [design/NATIVE_REVIEW_PLAN.md](design/NATIVE_REVIEW_PLAN.md) | What are the development-only capture/outbox/queue boundaries and report lifecycle? | Copies of the actual feedback queue or batch records |
| [design/BROWSER_PERSISTENCE.md](design/BROWSER_PERSISTENCE.md) | Which gates connect the shared C codec, Worker and browser Open/Save/recovery? | Treating partial components as complete browser IO |
| [design/SOURCE_FORMAT_IDENTITY.md](design/SOURCE_FORMAT_IDENTITY.md) | Which source record owns a format, under which identity, migration, budget and suspension rules? | Assuming source-edit overlays or data time travel are implemented |
| [design/WORKBOOK_MODEL_TRD.md](design/WORKBOOK_MODEL_TRD.md) | Technical draft incorporating accepted Import/Connect/Data Model, sheet composition, reference, history and Save decisions; remaining implementation questions | A second requirement summary, scheduled roadmap work or implemented capability |
| [design/CHART_OTHER.md](design/CHART_OTHER.md) | At which aggregate/grain is bounded chart Other correct? | The entire chart/Marks backlog or a universal top-N guarantee |
| [../tests/README.md](../tests/README.md) | Which native test/helper consumes each fixture? | Run output |
| [../tools/README.md](../tools/README.md) | What does each external tool do, and where is the product/orchestration boundary? | Adding tool dependencies to the native runtime |

## Overlap, stale information and maintenance

The 20 October 2026 review found a distinct purpose for each existing document. Deleting files or creating another dated audit/checkpoint tree is unnecessary. The main problem is **repeating the same information at different detail levels and points in time**. Apply these maintenance rules:

- **STATUS ↔ ROADMAP:** One describes current capability; the other remaining work. Both may briefly mention the same missing capability, but detailed checklists live only in the roadmap/feature record. Tooling acceptance paragraphs in STATUS are bounded integration evidence, not complete UI acceptance. Update the relevant current paragraph instead of appending old run capsules.
- **REQUIREMENTS ↔ DESIGN ↔ INTERACTIONS:** Human decisions, visual layout and interaction contracts are distinct. Keep REQUIREMENTS as a concise decision record, linking details to the two design documents. EXCEL_SHORTCUTS owns individual feature/binding status; shared focus/selection rules stay in INTERACTIONS.
- **USER_GUIDE ↔ architecture/testing:** The guide contains extensive format/engine details. Retain user-visible limits; place new internal codec slices in ARCHITECTURE or the relevant design document. The stale local bundle/service hash paragraph was removed, and vague “validation records” references now link to current TESTING/STATUS documents.
- **CONTRIBUTING ↔ status documents:** Separate process rules from transient Git/product state. The current guide describes linear work on main and worktrees for actual parallelism; document/review tasks do not require another branch. Query Git and service state when needed.
- **Proposals in ARCHITECTURE:** The operational walkthrough is useful; preserve proposal labels. Link accepted and scheduled slices into ROADMAP instead of giving every proposal a second work queue here.
- **Detailed persistence/identity/Other designs:** These are necessary; codec budgets and aggregate semantics do not fit a general roadmap. Old source-scoped test/commit notes are not current acceptance. New evidence goes into ignored build output; current capability goes into STATUS.

Open consistency issue: [INTERACTIONS selection statistics](design/INTERACTIONS.md#scope-and-definitions) treats empty text as blank for Filled/Distinct; [USER_GUIDE current behavior](USER_GUIDE.md) counts empty VARCHAR as a value. This **expected/current behavior difference** remains unresolved. Decide the product rule and test acceptance through ROADMAP rather than silently changing one document to match the other. Similarly, the guide's current Cmd+F/Focus/F5/F12 bindings can differ from the Excel inventory targets; a target list does not change a working control.

For a feature change, update the relevant EXCEL_SHORTCUTS ID/card → ROADMAP batch → STATUS if general capability changed → USER_GUIDE if the user's controls changed. New human decisions update REQUIREMENTS/the relevant design contract. Do not maintain the same detailed to-do list in three places.

Resolve conflicts using the latest human instruction, current written requirements, then verified native behavior. Tests record bounded acceptance; they do not override intended product changes. Update one current status and roadmap instead of accumulating duplicate journals.
