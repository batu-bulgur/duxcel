# Project documentation

**Current priority, 18 October 2026:** audit/cleanup and deliver a clean native bundle, then reproduce the remaining physical pan/selection bugs in the actual user-launchable app before expanding metadata components. Browser workbook Open/Save is still unavailable; complete UI and physical-input parity are not achieved. [STATUS.md](STATUS.md) distinguishes earlier tested revisions from the next bundle delivery.

The JavaScript prototype remains the frozen design reference. The established architecture uses a native agentic review build and a browser proof compiling the same C99/raylib UI to WebAssembly with the official DuckDB-Wasm Worker behind a thin typed bridge. Refine actual application behavior through native evidence and versioned design decisions; the reference's mock calculations, sample counts and design JSON are not production behavior. Native distribution gains no web runtime.

| Document | Purpose |
| --- | --- |
| [STATUS.md](STATUS.md) | One current capability matrix, limitations and evidence scope |
| [ROADMAP.md](ROADMAP.md) | Ordered delivery work and acceptance criteria |
| [REQUIREMENTS.md](REQUIREMENTS.md) | Original brief and later user decisions reconciled |
| [design/DESIGN.md](design/DESIGN.md) | Product/visual requirements |
| [design/INTERACTIONS.md](design/INTERACTIONS.md) | Detailed behavior, motion, selection and pane rules |
| [design/FEEDBACK.md](design/FEEDBACK.md) | Pinpoint review tools, local feedback queue and design/native handoff rules |
| [design/NATIVE_REVIEW_FEASIBILITY.md](design/NATIVE_REVIEW_FEASIBILITY.md) | Proposed native review build, production exclusion and shared Wasm/browser path |
| [design/NATIVE_REVIEW_PLAN.md](design/NATIVE_REVIEW_PLAN.md) | Approved native feedback priority, selected Wasm architecture and implementation acceptance |
| [design/SOURCE_FORMAT_IDENTITY.md](design/SOURCE_FORMAT_IDENTITY.md) | Proposed source-record formatting semantics, bounded implementation slices and persistence gates |
| [design/BROWSER_PERSISTENCE.md](design/BROWSER_PERSISTENCE.md) | Shared C workbook/Worker persistence slices, file/handle ownership and pinned API constraints |
| [../design/prototype/](../design/prototype/) | Read-only executable reference, source, fonts and browser QA |
| [USER_GUIDE.md](USER_GUIDE.md) | Current controls, keyboard reference, file format and limits |
| [ARCHITECTURE.md](ARCHITECTURE.md) | Source ownership, memory boundaries and parallel lanes |
| [TESTING.md](TESTING.md) | Short development checks and comprehensive regression |
| [CONTRIBUTING.md](CONTRIBUTING.md) | Git workflow, worktrees and integration discipline |
| [validation/README.md](validation/README.md) | Versioned acceptance records and performance interpretation |
| [audit/2026-10-18-cleanup.md](audit/2026-10-18-cleanup.md) | Current worktree audit, cleanup and native delivery |
| [audit/2026-10-17.md](audit/2026-10-17.md) | Initial repository/implementation audit |
| [archive/README.md](archive/README.md) | Earlier plans, test narratives and superseded assets |

Resolve conflicts in this order: latest human requirements; the rendered prototype for demonstrated UI; explicit product/interaction requirements beyond its scope; current native acceptance matrix; historical records. The prototype's earlier “disposable study / awaiting approval” wording records its creation. It does not revoke the later instruction to implement it exactly. Missing native capabilities need honest disabled states or completed real actions, never pretend calculations.

All paths in code spans are repository-relative unless stated otherwise. Small root/design entry-point files preserve older links; the substantive written specifications and plans live here. macOS is the current execution platform. Linux/Windows remain portability targets without new execution claims; Wasm is an approved subsequent milestone with its own browser verification.
