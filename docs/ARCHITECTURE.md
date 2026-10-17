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

## Approved review and browser boundaries

The native agentic review build is the next integration priority; [NATIVE_REVIEW_PLAN.md](design/NATIVE_REVIEW_PLAN.md) defines its feature slices and acceptance. The App/build/shared-interface integrator remains the owner. A portable C99 review core should define semantic widget identities, clipped/layered geometry, bounded frame records, state snapshots and serialization without OS handles, live DuckDB handles or borrowed page pointers. Native rendering/composer/input hooks and capture/file adapters consume those records. New review modules, data, commands and transport exports are absent from production source/object lists and conditional fields/hooks; feature flags are part of isolated build signatures.

Agent/queue communication runs outside the app, initially through atomic local bundles and a reader. A later optional local IPC/MCP adapter consumes the same versioned schema; its availability is not required to save feedback. Preserve same-frame image/state provenance, worker ownership and the desktop lock. Logical widget identity is separate from current ribbon/overflow placement and from action identity.

The browser product compiles shared C99/raylib UI and interactions through Emscripten. The selected starting engine is official DuckDB-Wasm in a Worker, connected by bounded asynchronous typed requests/pages. Keep browser file/clipboard/composition/lifetime glue behind platform adapters. Preserve SQL generation and transformations in shared C where applicable; JavaScript marshals rather than reimplements analytical behavior. Browser persistence must account for the direct DuckDB C API in `workbook.c`, not just replace `worker.c`.

Native/browser review use the same registry and schema, with platform-specific capture/transport adapters. A canvas exposes no automatic HTML button identities. Review snapshots may enumerate visible controls and compute cell targets on demand; never export an entire worksheet each frame. This debug infrastructure is not a completed native/browser accessibility implementation.

### Native review ownership

`src/dev/review_registry.*` owns bounded portable records and serialization; `review_store.*` publishes immutable bundles atomically. `review_capture.*` owns the frozen CPU image/GPU texture, separate note/editor/history, bounded event ring and writer lifetime. Encoding/storage run on its writer without GL or database access; cancellation/shutdown joins before freeing those buffers. `review_app.inc` is the compile-time-only App adapter for semantic IDs, current controller geometry and selected cached cell values. It never retains a Page/Cell pointer in a report. Registry bounds and clips are resolved window logical coordinates; the recorded transform describes presentation and must not be applied again to those bounds.

Only an armed capture render enumerates controls. Normal review frames perform no registry serialization, GPU readback, filesystem polling or review SQL. The report ownership check precedes count/statistics/query publication. The frozen overlay does not run normal App controllers or schedule jobs; one completed worker reply stays owned by the existing worker mutex until ordinary dispatch resumes. Raw/queued key, pointer, wheel and pinch inputs are consumed/quarantined at capture, and composer commands cannot leak back to the workbook. Separate Make configurations force-include the review input adapter; shipping hooks omit argument evaluation and all review fields/modules.

The local Python reader (`tools/native_review.py`) validates complete project-scoped native bundles and writes separately locked lifecycle state. It neither changes the immutable capture nor runs remote app actions. No daemon or network listener is required for the initial feedback loop.

The shared App has one owned `AppSession` with `app_start`, `app_frame` and `app_finish`. Native `app_run` drives those calls synchronously; the browser host will schedule the same frame call. Options are copied, the render batch remains at a stable address, and finish joins workers before freeing state. macOS hooks detach before GLFW/window teardown. `src/data/page.*` owns result/snapshot cleanup with an explicit cell-text release callback; native pages retain DuckDB allocation ownership and browser-owned pages use C allocation.
