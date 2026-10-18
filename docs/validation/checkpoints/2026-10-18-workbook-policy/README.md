# Shared workbook policy foundation — 18 October 2026

This checkpoint accepts BP1's shared C inventory and minimum write-version policy at `efd7adf` (implementation `9c74ccb`, isolated `c2e7c5e`). It does not implement a row decoder, browser workbook Open/Save, asynchronous operation keys, source relinking or recovery. Native production loading remains the existing adapter.

The immutable descriptors preserve all26 native v14 table names, order, columns, type codes and supported NULL values. Package16/20/26 counts describe current writers12/13/14, not historical required-table contracts. Native legacy optional tables, coercion/default paths, relative locators, archive/model/target validation and existing error strings remain separate. The minimum policy preserves ordinary12, retired owner high-water13 and source/history/ledger/mode14; existing13/14 archives never downgrade. Exact uint64 comparisons preserve values above2^53 and UINT64_MAX. Descriptors and model access are borrowed; the module allocates and executes nothing.

## Focused component checks

The root [fast manifest](pure/summary.json) passes2,274 checks per optimized/O1 ASan+UBSan configuration in3.455s including1.832s build. Its source is `efd7adf`; the [isolated run](isolated/manifest.json) and [mechanical extraction comparison](isolated/extraction-control.json) have their own source/command hashes. Both require the existing DuckDB type header but link no engine library. A first isolated no-header command failed before compilation and was corrected to use the installed include path; it introduced no dependency. The existing32 runner tests pass in2.136s.

## Native engine acceptance

The [native manifest](native/manifest.json) retains five existing subsets per configuration, in serial: formats635, owners393, source versions207, source formats1,287 and guarded formats1,003. All original assertions remain. It verifies immutable test binaries and unchanged original core fixtures; no core fixture was regenerated or shared across worktrees. No GUI was launched and no clipboard/file panel was used.

Build takes16.501s. Optimized subset times are2.546/2.457/0.780/18.535/9.625s; O1 sanitizer takes8.593/10.782/4.212/119.750/60.296s. Total254.364s is retained honestly: this is an integration gate rather than an edit-loop target. Sampling the long malformed-source run identified existing DuckDB join-order planning in the nine-EXISTS budget query; a separate measured follow-up is required before any speed claim. These subsets establish their named native writer/loader boundaries, not a new full v1–14 regression or whole-App pass.

## Browser build boundary

The [normal shared-C Wasm build](browser/build.json) succeeds at `efd7adf` in16.808s, artifact `615b25925559451100a3`. This proves linkage only. Browser workbook persistence remains unavailable; it is not a browser runtime acceptance, staged decoder, download, OPFS or source-identity proof. The served review artifact and signed native bundle retain the separately accepted chart Other source scopes.

## Retention

Only focused logs/manifests and extraction evidence are retained. No executable, generated database, user report archive or UI input stream is copied. `hashes.json` seals these files; original paths and hashes are in `provenance.json`.
