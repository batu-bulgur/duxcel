# Browser DPR, precise pan and workbook file foundation

Historical checkpoint. Machine-generated payloads were removed from the current checkout on 18 October 2026; they remain recoverable at Git ref `archive/pre-cleanup-20261018`. Paths and measurements below describe that original run. See the [retention policy](../../README.md).

17 October 2026. Production stays shared C99/raylib with the official asynchronous DuckDB-Wasm Worker; there is no new UI/runtime dependency or prototype edit.

## Scope and provenance

The normal artifact is `aa76f526317d62fd37b8` (Wasm SHA-256 `109dd098e9182955ad4abf1d3bbbbce22804c2d1159530ec93af5b1c0d97ba84`); review is `bd5c2fc0a11604247ce4` (`30b553aafa05c11c1764215df4568d8dd8c934818b519ac1aa44a9f63793e3d2`). Both record build source `6bfebb4`. Later commits change the test-only persistence probe/fixture, not these renderer/engine artifacts. Final compatibility acceptance uses `de681f1`; each run preserves its own source/hash boundary. The unrelated preexisting untracked `tools/ui-review/` is left untouched.

`fe1882e` implements actual backing/input integration; `abe640f` adds density-only transition detection; `95468d3` adds review-only callback measurements. Native active-cell acceptance uses `fe1882e` and the unchanged native grid implementation. This is focused acceptance, not a new full regression or complete exact UI-port sign-off.

## Implemented platform repairs

The previous web framebuffer stayed at1× despite a Retina browser DPR. C/raylib now synchronizes logical window, actual backing/render/current-FBO dimensions, screen transform and scissors. Existing fonts load at actual render density. Pure sizing caps backing at8192 per dimension/16Mi color pixels/max4× and the actual GL viewport/renderbuffer limit; these limits do not shrink logical UI. The adapter repairs only a private copied pinned raylib source, with its input hashes in the dependency stamp; SDK/Homebrew/native sources are unchanged. GLFW preserves host CSS sizes, and its existing mouse mapping uses synchronized logical dimensions. One scalar DPR comparison at the existing frame boundary covers missing resize/MQL notifications without polling layout each frame. Screenshot readback avoids double density scaling.

Pinned GLFW quantized tiny vertical wheel input to a full detent while horizontal input used different units; raylib also replaced earlier events in the same frame. A bounded reusable host accumulator claims canvas wheel input before GLFW, retains pixel fractions and both axes, normalizes line/page units and separates Ctrl-wheel zoom. C drains it once per frame, including blocked/modal frames. The grid consumes logical pixel pan divided by its existing UI/canvas scale. Legacy native wheel behavior remains unchanged. Generic chrome retains non-consuming normalized wheel input and C hit-test ownership.

## Accepted checks

| Gate | Result and elapsed time | Scope |
| --- | --- | --- |
| Pure browser input/surface/probe |46 Node groups; see `node-tests.txt` | Fractions, tails, modes, bounds, transforms, lifecycle and configuration; no engine/desktop |
| Native grid release + ASan/UBSan |20,384 checks each;9.938s total including8.585s build | Pixel-distance invariance, both axes, blocked input, retained active cell and existing grid coverage |
| Hidden native active-cell |41 App stages +477 desktop checks;20.905s App /60.428s total including37.542s build | Real sanitizer renderer/input; no visible window or Space activation |
| Combined browser platform/filter/review |101 checks;21.812s total | Actual C pan/reversal/Shift/trusted wheel, zoom, DPR1/1.25/2, explicit CSS sizing, inset transform, value/date filters,12 header picks, real report and reader |
| Normal manual App, DPR1 |12 checks;7.342s total | Exact manual integer, selection/Shift navigation, wide/narrow render |
| Normal public Parquet App, DPR2 |18 checks;6.406s total |59,158,238-byte source;256-row/20-column page and3,475,226 rows; real ribbon input/capture before source reply |
| BP0 native→Worker→native |13 driver checks;7.938s total;986.3ms probe | Real v12 file rewrite/export/reopen; both native configurations pass118 checks |
| Shipping exclusion |Passed actual normal/review Wasm + native binaries | No review bridge/uploader/probe packaged with normal UI; device adapter shared |

The platform test reports **zero query jobs** during pan/zoom/density checks. All pointer selections still reach the intended C targets at DPR2 with the CSS transform. The browser-generated wheel assertion uses the actual trusted DOM delta: headless CDP at DPR2 delivered half its requested protocol delta, so the test records/compares delivered units rather than silently changing product speed. Earlier failed probes/test assumptions remain in ignored run logs; successful manifests above are the accepted boundaries.

Matching blank state: DPR1 (archived `blank-dpr1.png`), DPR2 (archived `blank-dpr2.png`). Actual Retina C filter frame (archived `retina-filter-frame.png`) retains its full framebuffer resolution rather than the CSS-scaled browser presentation.

All browser runs used a temporary headless Chromium150 profile under the shared desktop lock. Native windows were hidden before creation. Tests never accessed/reloaded the user's open tab, user profile, workbook or app. Temporary servers/profiles were closed; only the owned review origin on127.0.0.1:8767 remains. Old mock/Agentation ports4173/4174/4747/8768 remain closed. The service now serves the accepted review artifact and preserves its existing persistent queue; the user's page updates only on manual reload.

## Resource observations

Fresh owned profiles, loopback HTTP and warm OS file caches were used. These single local observations are not input-latency or general startup guarantees.

| Observation | Measured value / interpretation |
| --- | --- |
| Navigation → first completed C frame |281.6ms normal DPR1;267.8ms normal DPR2;280.4ms review |
| Normal App assets |Wasm2,969,421 bytes; JS391,872; font/UI preload1,104,374 |
| Lazy official engine assets |EH Wasm34,242,586 bytes; Worker772,759; additional Arrow/module assets in hashed manifests |
| Actual parent resource bodies |Normal manual149 resources/5,179,929 encoded bytes; Parquet151/5,984,665; review156/6,300,564. No HTTP compression in this local server. Worker-owned resource transfer is **not** included or inferred from parent Resource Timing. |
| UI linear memory |64MiB capacity throughout the small source/platform snapshots; C allocated approximately7.63MB at DPR1,7.86MB at1.25 and8.93MB at2. This excludes GL textures, engine Worker and process RSS. |
| Parent JS heap |Approximately6.56MB used in the integrated review observation; browser estimate, excludes Worker/GPU/RSS |
| Framebuffer at1280×820 |DPR1:1280×820;1.25:1600×1025;2:2560×1640. DPR2 color backing alone is16,793,600 bytes, excluding other browser/GPU buffers. |
| First CSV C job |Two callbacks;1,058 wire bytes,2,431 aggregate decoded Page allocation bytes,1,203 retained Page bytes;55.6ms end-to-end C job,0.8ms C wire validation/allocation/copy. Later callbacks have timing quantization; these do not isolate engine execution or JS encoding. |
| Large source response |File selection→C publication/count observed in1,537ms. A View input+capture completed in197ms **before** its source reply; this establishes responsive scheduling, not input latency. |

The copy ledger remains explicit: file `arrayBuffer` plus adapter-owned byte clone (two59,158,238-byte buffers for this Parquet before transfer/lifetime release); Worker Arrow values→per-cell UTF-8 chunks→owned Page arrays; Page→contiguous bounded wire; wire→UI linear memory; C validation/deep-owned Page strings/arrays before temporary transport release. Source bytes do not enter the C UI heap. Page limits remain256 rows/256 columns/4MiB payload and6MiB wire. C callbacks now measure their own decode/allocation work only in review builds, with no idle tracing, polling serialization or new SQL. End-to-end Worker memory/peak copy RSS and worst-case conversion stalls still need measurement. The current eager Arrow import also increases bootstrap request count; future handle/loader optimization must retain the measured ownership/semantics.

## BP0 findings and next boundaries

The real native v12 fixture is8,400,896 bytes, storage version64, with2 sheets,9 sparse cells,2 nondefault formats,3 bound table/pivot/chart objects, source/query/filter/sort options and Marks. Object IDs begin at9,007,199,254,740,993. The official package1.32.0/engine1.4.3 reads it read-only and rejects writes. A private registered valid database copy has its16 metadata tables recreated from engine-described DDL; typed rows are compared using `EXCEPT ALL` both ways. Exact Worker-written BIGINT/UBIGINT/DECIMAL(38,10), SQL NULL, empty VARCHAR, literal NULL and UTF-8 sentinels persist. The copy is committed/checkpointed/flushed, exported while its private writer is quiescent, reopened in the Worker, then accepted by native engine1.5.6 through the actual C `workbook_load` in release and ASan/UBSan. Original bytes/hash stay unchanged. Fixture/export hashes and exact timings are in `persistence-probe.json`; no database binary is checked in.

The export is17,051,648 bytes: metadata rewriting of a cloned baseline grows the file. This proves file/metadata compatibility, not efficient storage or product Save. Newly created unregistered database files failed the pinned copy API before and after detach; an empty registered buffer failed ATTACH; the declared `registerEmptyFileBuffer` is a no-op in the installed package. Direct Arrow catalog-boolean getters produced invalid values; the probe uses canonical SQL VARCHAR nullability, as shared source pages already do. No dependency was upgraded to conceal these limitations.

Blank workbook file creation/export, preservation of unrelated user tables, bounded shared C metadata codec/migrations, actual asynchronous Open/Save/dirty lifecycle, external source handles/relink, OPFS/quota/crash recovery and browser analytical operations remain incomplete. The [persistence plan](../../../design/BROWSER_PERSISTENCE.md) orders those slices. Physical trackpad momentum/pinch/browser zoom/monitor transitions, Safari/Firefox/IME/clipboard/context-loss and the broader exact-UI state matrix remain unaccepted. Hidden synthetic input cannot establish physical gesture feel.
