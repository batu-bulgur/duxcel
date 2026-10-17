# Source ownership and integration boundaries

The existing module split is useful and stays in place. This audit does not propose an engine or UI-framework rewrite.

| Area | Responsibility / ownership | Suitable independent lane |
| --- | --- | --- |
| `src/data/` | DuckDB connection/results, worker jobs/replies, bounded pages, counts/distinct/statistics | Engine correctness; avoid GUI work |
| `src/transforms/` | Owned analysis/pivot/Marks definitions and SQL generation | Analytical behavior plus pure/engine tests |
| `src/workbook/` | Sparse cells, formats, object placement/history, clipboard and `.dxls` persistence | Workbook invariants/migrations |
| `src/grid/` | Logical cell geometry, input, range/active state, viewport and visible drawing | Grid interaction with focused tests |
| `src/ui/` | Font metrics, widgets, measured presentation/layout, motion, paint layers, matrix/chart presentation | Pure widget/layout feature lanes |
| `src/platform/` | Native macOS file panels/menu/gestures; clocks, threads, writable temporary paths | Platform owner; serial desktop acceptance |
| `src/app/` | Command dispatch, selections, source identity, async routing, contextual views and lifecycle | **Single integrator** until focused controllers are extracted |
| `tests/` | Authored C tests and small fixtures; ignored generated DBs/downloads/results | Owner of the corresponding module |
| `tools/` | Asset generation, design fixture export, inventory and test orchestration | Build/test tooling lane |
| `assets/`, `packaging/` | Runtime fonts/icons/licenses, intentional prebuilt derivatives, platform metadata | Asset owner; generated files follow their generator |

The worker owns its DuckDB connection; jobs/replies transfer owned values, and stale IDs are rejected. Pages own schema/cell strings. UI snapshots must copy bounded data they retain; they cannot keep pointers into evictable pages. Per-frame format stores are borrowed. GPU paint layers own bounded textures and must restore framebuffer/scissor state. Keep this ownership explicit when adding or extracting code.

At audit baseline, `src/app/app.c` has approximately 11,000 lines and roughly 3,900 lines of embedded native scenario code. It is both the integration hotspot and the main merge-conflict risk. Extract the harness first without changing behavior, then extract narrowly cohesive controllers when a feature benefits. Do not move all UI into a new abstraction layer to enable parallel work.

Logical UI units, interface scale, per-sheet canvas zoom and framebuffer density are distinct. Geometry modules should remain pure where possible. Native render/input controllers consume that geometry; tests should verify invariants and meaningful behavior, not copy implementation arithmetic.

`design/prototype/` remains executable design material with its own JavaScript dependencies. It is never part of native `SRC`, linked into the app, or copied into its bundle. Dormant Windows/Linux packaging remains related project source but has no current execution claim.
