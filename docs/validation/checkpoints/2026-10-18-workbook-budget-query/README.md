# Workbook budget query

Historical checkpoint. Machine-generated payloads were removed from the current checkout on 18 October 2026; they remain recoverable at Git ref `archive/pre-cleanup-20261018`. Paths and measurements below describe that original run. See the [retention policy](../../README.md).

The query rewrite, focused native codec runs and corrected browser-conditioned model build/run are accepted within the separate source scopes below. This checkpoint does not promote an App bundle or service, activate browser persistence, or include later header-decoder work.

## Same limits, independent scalar checks

Commit `554b591` (archived `production-change.txt`) changes the scalar metadata-limit query in native `source_package_budget`: nine `EXISTS` expressions previously joined by `OR` become nine `UNION ALL` Boolean rows reduced by `bool_or(invalid)`. The nine predicate texts and their order are byte-identical in the extraction (archived `probe/extraction.json`), old query (archived `probe/old.sql`) and new query (archived `probe/union.sql`). Count/text bounds, NULL/NUL decisions and the surrounding owned-budget checks are not relaxed.

Each `EXISTS` produces one non-NULL Boolean even when its table is empty; therefore the reduction still returns one non-NULL Boolean. Missing tables or incompatible column types remain query errors. This is an SQL shape correction at archive validation, not a new cache, skipped validation, per-frame query, or architectural change.

## Matched isolated query measurements

The private probe source (archived `probe/probe.c`) and manifest (archived `probe/manifest.json`) retain exact compiler flags, binary hashes and the linked Homebrew library hash. Actual `duckdb_library_version()` reports **v1.5.6**. Both configurations use that same library, two threads, a256MB engine setting and two equivalent private in-memory seven-table metadata schemas; no application, GUI or persistence file is executed by this probe.

| Configuration | Old OR first /repeat | New UNION +bool_or first /repeat |
| --- | --- | --- |
| Optimized `-O2` |26.629 /19.901ms |1.395 /1.277ms |
| O1 ASan/UBSan caller |73.563001 /72.362ms |5.451 /4.771ms |

The exact optimized (archived `probe/release.jsonl`) and O1 (archived `probe/sanitize.jsonl`) logs retain the measurements. “Cold” means the first timed query in that variant's separately initialized in-memory database; it does not mean cold filesystem or process caches. The order is old-first, new-first, new-repeat, old-repeat. Each cell is one observation, not a percentile, repeated statistical benchmark or claim of a universal engine speedup. The installed library is identified by hash; its own build is not claimed to be sanitizer-instrumented.

Each configuration passes **1,009 checks over30 correctness cases plus four expected query errors**. For the30 cases, the new combined result is checked against the explicit expected Boolean and the OR of all nine unchanged standalone predicates. This does not claim that the expensive old combined query was rerun for every boundary case. The four errors run both combined forms against a missing table and an incompatible definition column.

Cases retain accepted/rejected boundaries for1,024 source versions,65,536-byte references/paths,262,144-byte definitions,512 scopes,33 guards,20,000 records and16,384 fields, plus primary/Table path NULL/NUL, source-reference NULL/NUL, allowed pending raw-definition NULL, empty tables and benign populated metadata. Predicate files are retained individually so the comparator does not reproduce the new combined shape.

## Native codec integration is a separate workload

The before manifest (archived `before/manifest.json`) is exact source `efd7adf`, running five existing workbook subsets per configuration. The after manifest (archived `after/manifest.json`) is exact `554b591`, rerunning the unchanged `--source-formats-only` subset with **1,287 checks per optimized/O1 configuration**. Both record source unchanged, identical original core-fixture hashes and the binaries used by their own runs.

| Source-format workload | Optimized | O1 ASan/UBSan |
| --- | --- | --- |
| Before `efd7adf` |18.535s |119.750s |
| After `554b591` |15.329s |93.838s |

The after build takes15.566s and the two runs total124.981s including build/runner overhead. Before/after use the same focused workload, but these serial integration runs are not a strictly controlled whole-suite benchmark. Only the isolated old/new SQL probe is the matched query comparison. The1287 codec assertions retain typed dormant scopes, source ledgers, exact uint64 values, budgets and transactional rejection; they are helper-level archive acceptance, not a fresh whole-App or UI run.

## Browser-conditioned model target boundary

Two natural build failures are retained, rather than presented as model passes:

- At `554b591`, the initial target (archived `model-failures/initial.json`) fails after3.747s because native clang receives browser-conditioned `platform.c`, which includes the Emscripten header. The compiler log (archived `model-failures/initial-build.txt`) records the actual failure.
- At `96e68b1`, filtering that file from this pure model target exposes a second link failure (archived `model-failures/intermediate.json`) after3.432s: real workbook/model paths require `dx_absolute_path`. The linker log (archived `model-failures/intermediate-build.txt`) is retained. These are target-boundary defects, not failure of the measured SQL rewrite or permission to replace the real path helper with a mock.

The final correction `e142142` compiles the real native platform path boundary in separately owned optimized/O1 objects without the browser-JavaScript define, then links it with browser-conditioned model CORE. The manifest (archived `model-final/manifest.json`) records source unchanged and passes **87 model checks/configuration**: build7.781s, optimized0.554s, O1 ASan/UBSan0.486s, total8.821s. The actual compiler commands (archived `model-final/build.txt`) and optimized (archived `model-final/release.txt`)/O1 (archived `model-final/sanitize.txt`) output are retained. Real locator assertions stay enabled; no path helper is replaced with a mock. No browser Worker, browser save/open, UI or physical platform execution acceptance is implied by this native compiler/model gate.

## Scope and evidence

The accepted distribution remains the prior native bundle `0fd8c57` and review service product `1082c1f`; this checkpoint makes no new delivery claim. The original process sampling file is not copied or used to claim a global RSS/memory improvement. Engine settings and elapsed query times do not establish a process-memory cap.

Only compact sources, SQL, manifests and text/JSONL logs are retained. No executable, dylib, generated database, temporary engine directory, input/event stream or report archive is copied. Provenance (archived `provenance.json`) records original paths/hashes; the hash manifest (archived `hashes.json`) seals every retained payload. Independent policy/header-decoder work remains outside this checkpoint.
