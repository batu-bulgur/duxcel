# Project documentation

The latest user instruction establishes the browser interface as the exact design definition. Preserve its anatomy and demonstrated interactions while connecting them to real native commands and DuckDB results. Production remains C99/raylib; the reference's mock calculations, sample counts and design JSON are not production behavior.

| Document | Purpose |
| --- | --- |
| [STATUS.md](STATUS.md) | One current capability matrix, limitations and evidence scope |
| [ROADMAP.md](ROADMAP.md) | Ordered delivery work and acceptance criteria |
| [REQUIREMENTS.md](REQUIREMENTS.md) | Original brief and later user decisions reconciled |
| [design/DESIGN.md](design/DESIGN.md) | Product/visual requirements |
| [design/INTERACTIONS.md](design/INTERACTIONS.md) | Detailed behavior, motion, selection and pane rules |
| [../design/prototype/](../design/prototype/) | Read-only executable reference, source, fonts and browser QA |
| [USER_GUIDE.md](USER_GUIDE.md) | Current controls, keyboard reference, file format and limits |
| [ARCHITECTURE.md](ARCHITECTURE.md) | Source ownership, memory boundaries and parallel lanes |
| [TESTING.md](TESTING.md) | Short development checks and comprehensive regression |
| [CONTRIBUTING.md](CONTRIBUTING.md) | Git workflow, worktrees and integration discipline |
| [validation/README.md](validation/README.md) | Versioned acceptance records and performance interpretation |
| [audit/2026-10-17.md](audit/2026-10-17.md) | Repository/implementation audit and cleanup rationale |
| [archive/README.md](archive/README.md) | Earlier plans, test narratives and superseded assets |

Resolve conflicts in this order: latest human requirements; the rendered prototype for demonstrated UI; explicit product/interaction requirements beyond its scope; current native acceptance matrix; historical records. The prototype's earlier “disposable study / awaiting approval” wording records its creation. It does not revoke the later instruction to implement it exactly. Missing native capabilities need honest disabled states or completed real actions, never pretend calculations.

All paths in code spans are repository-relative unless stated otherwise. Small root/design entry-point files preserve older links; the substantive written specifications and plans live here. Windows/Linux validation and WebAssembly remain outside the current macOS work.
