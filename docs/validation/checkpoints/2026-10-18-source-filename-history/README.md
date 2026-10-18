# Source filenames and pane history acceptance

Original browser filenames now remain separate from immutable executable registration keys in Steps, source details and owned preview titles. Renaming a sheet or Table changes its own name. The shared UI and calculations remain C99/raylib and DuckDB; the browser host only carries the original label across the existing import boundary.

| Gate | Product / runner | Result | Seconds |
| --- | --- | --- | ---: |
| Focused Steps/preview modules | `6808680` |128 Steps and319 full preview checks per optimized/O1 ASan+UBSan configuration |18.877 total, including16.335 build |
| Memory-backed browser host | Root retained Node log |37 groups; no real Worker |0.175 TAP duration |
| Natural pane-history product red | `6808680` |Fails at primary-filter Undo after446 prior checks |34.667 driver /37.318 total |
| Table-anchor fixture retry | `ca2b227` |Runner incorrectly navigates while Steps owns focus |22.860 driver /25.638 total |
| Actual DPR2/transformed browser | `ca2b227` / `eedc5bb` |1,035 checks /20 stages |45.397 driver /47.770 browser /48.015 total |
| Hidden native review | `ca2b227` binary / `eedc5bb` runner |400 stages and three reports per configuration |18.055 optimized /22.786 sanitizer |
| Signed local bundle | `ca2b227` code / `eedc5bb` source snapshot |Signature/source verified; not launched |0.905 |

The private files share the original basename `import-7-O'Brien-İstanbul-表-🦆.csv`. Their keys are independently registered as `import-1-…` and `import-2-…`; no prefix is stripped from user text. The five-row primary retains its results while a three-row Table with IDs101–103 is imported at B13. The normal UI renames them Ledger/Sales, filters City, opens raw source previews, applies `SELECT * FROM input WHERE ID > 0`, and exercises query, rename and import Undo/Redo. Source previews retain five and three rows respectively, independently of the current filter. The final cached revisits and idle sample retain16 submitted jobs; the initial setup includes one ordinary Statistics job, so the whole run is not query-free.

The first run exposed a product defect: Steps keyboard handling consumed Command/Control Z/Y before global history dispatch. The [natural failed frame](images/history-shortcut-red.png) and measured timeout retain that control. The fix lets those shortcuts reach existing global history without clearing pane focus or changing local SQL Copy/arrows/modal guards. A later Table-import failure was a fixture defect: its anchor navigation ran while Steps still owned focus. The corrected runner first clicks the canvas normally and asserts B13; it does not route pane arrows globally or weaken the product assertion.

Current unmodified compositor captures: [renamed primary](images/renamed-primary.png), [filtered source preview](images/filtered-preview.png), [renamed Table](images/renamed-table.png), [query detail](images/query-detail.png). **CJK/emoji glyph coverage remains open:** the Table image visibly renders those filename characters as `?`, although captured UTF-8 title/key/detail text is exact. These images do not establish full international visual parity, native physical input/pixels or full reference-state matching.

The focused modules preserve the old basename APIs and unchanged Analysis/path/query/source-version behavior; optional named APIs borrow the label only during construction and deep-own preview titles. The host rejects NUL/unpaired UTF-16 before file read, engine access, name reservation or name/label allocation. It still emits one normal owned diagnostic on failure. Valid Unicode, apostrophes, whitespace and prefix-looking originals retain their text; the existing sanitized200-byte registration bound is unchanged.

`package.json` records the signed local bundle. `service.json` records the accepted same-C artifact `bca930d9d464f78976eb` at8767, with the same project/queue and zero reports before/after refresh. No user browser reload or native launch occurred. This proves accepted UI serving and queue retention, not a new end-to-end report-delivery run. Native optimized/sanitizer and normal/review Wasm shipping-exclusion logs pass. Official Worker192 retains its earlier source scope; it was not rerun for this presentation change.

Friendly names are session UI metadata. Browser `.dxls` Save/Open, persistent labels, source registration epochs/replacement identity and glyph fallback remain unfinished. Formatting admission retry and broader migration/parity retain their existing open tasks.

`measurements.json` keeps selected source/artifact/binary hashes, phase times, exact label captures, job/history observations and original-file hashes. The five PNGs and small logs are copied unchanged; no new capture, compiler, engine or browser was launched during curation. Large event streams, mutable CSVs, duplicate frames and native report bundles remain in their original private runs. `files.sha256.json` hashes retained files except itself.
