# Owned source-version ledger metadata

This checkpoint records the dependency-ready source-version ledger continuation of the shared C **partial Header**. It preserves pending entries and exact immutable RAW packet bytes independently of archived references and optional resolved locators. It does not activate a source, restore bindings/formats, publish a Workbook or enable browser workbook Open/Save.

Actual no-window acceptance at `2bb5bf4` passes **176,738 C /460 external checks**, four archives, five completed Headers and98 metadata queries. The adjacent four-column Grid gate also passes on the same immutable driver.

## Source and evidence boundaries

| Layer | Tested source | Result and elapsed scope | Receipt |
| --- | --- | --- | --- |
| Pure family19 reader | private `ad36d3b` | 35,033 checks/configuration: 24,739 unchanged +10,294; optimized/O1 ASan+UBSan; 17.881 s builds/runs | [manifest](pure/manifest.json), [preservation](pure/original-test-preservation.json) |
| Root pure linkage | `f661434` | 35,033/configuration; 15.367 s focused runner | [manifest](root-pure/manifest.json), [summary](root-pure/summary.json) |
| Seven-column stream/helper | private `c626f6c` | 47 focused /196 combined Node groups; 0.813/4.049 s enclosing commands; all181 earlier groups byte-preserved | [manifest](node/manifest.json), [prefix witness](node/test-prefixes.json) |
| Worker C controller authoring | private `4201dc8` → root `0e04723` | static checks only; planned98-query workflow, no compilation/runtime in that receipt | [static receipt](static-controller/manifest.json) |
| Native SDK comparison | private `e9193f9`, pure dependency `ad36d3b` | 184,182 checks /194 cursor executions /288 typed feeds per configuration; optimized4.260 s /O1 sanitizer25.632 s runtime | [manifest](native/manifest.json), [exact53-input closure](native/preprocessor-closure.json) |
| Actual official Worker + C | `2bb5bf4`, driver `2da8e6eb4988d1d11894` | 176,738 C /460 external;8.066 s browser /18.252 s CLI | [C report](actual-worker/result.extract.json), [wrapper](actual-worker/summary.extract.json), [transport](actual-worker/codec-read-transport.json) |
| Adjacent Columns/Cells streams | same `2bb5bf4` /same driver | 79,583 C /282 external;62 queries;5.611 s browser /6.756 s CLI | [manifest](adjacent-grid/manifest.json), [C report](adjacent-grid/result.extract.json) |
| Root harness guards | original pending integration scopes as recorded | 18 harness /32 runner tests; corrected exact command inventory;33 corrupted source-ledger reports rejected | [passed guards](harness-corrected/manifest.json), [preceding failure](harness-initial/manifest.json) |

The root pure comparison records76 C/header inputs using a conservative all-project-header inventory. It is **not** an exact preprocessor closure. The native comparison separately retains actual `clang -MM`53-project-input closure and SDK identities. The native [root comparison](native/root-source-comparison.json) matches every one of those53 compiled project inputs plus SDK files between private `e9193f9` and actual `2bb5bf4`; its scope is native only. [Selected committed-source witnesses](source-witness.json) separately show unchanged pure/component and transport/controller bytes in the actual integration, without substituting for that closure.

The driver build takes1.379 s with no C object rebuilt; the actual wrapper's8.632 s setup-build field also includes native-helper preparation and is not that driver-link duration. The native194 cursor executions and actual98 requests exclude other native Save/Load/inspection or browser attachment/detachment administration SQL.

The Node tests use the actual adapter, Arrow and callback helper with mock connections/readers/C exports; real `WebAssembly.Memory` grows. They execute no DuckDB, browser or C packet decoder. A helper passing is not integrated acceptance. Native tests use actual SDK DuckDB `v1.5.6`, not the official Worker's separately observed engine version.

## Owned ledger continuation

Family19 follows complete object descriptors, object counters and source-version counters, once before Header claim. Versions12/13 forbid a present ledger table and default an absent ledger to ready-empty without SQL; version14 requires it. Original named types are INTEGER/UBIGINT/UBIGINT/INTEGER/VARCHAR/INTEGER/BLOB, with only definition nullable:

```sql
SELECT sheet_id,version,owner,source_kind,source_reference,encoding,definition
FROM _duxcel_workspace.source_versions
ORDER BY sheet_id,version LIMIT 65537
```

One immutable result admits at most65,536 entries, at most1,024 per sheet, through atomic feeds of at most256 rows and explicit EOF. `(sheet,version)` is strictly ordered; version is positive and bounded by its sheet's source high-water. Owner is bounded by the independent object high-water. Archived retired owners remain valid; a live analytical object cannot become a source owner. Primary owner0 remains an archival slot even when the current primary is SOURCE_NONE. CSV and Parquet are the supported ledger kinds.

Pending entries require encoding0 + SQL NULL; their RAW pointer/length remain NULL/zero. Complete entries require encoding1 + a canonical RAW BLOB32..262,143 bytes matching exact owner/version/kind. Empty non-NULL BLOB is distinct and rejects. RAW is checked before the shared packet validator, so a RESULT discriminator cannot enter result-filter JSON parsing. This validates binary metadata syntax, not file content or publication provenance.

Raw archive references are independently owned nonempty UTF-8, at most64KiB, and may contain archived glob spelling. Optional resolution produces a separate owned locator. NULL resolver or an accepted empty result stays unresolved; `locator_resolved` reports nonempty mapping, not filesystem existence, content identity or capability. Nonempty resolved locators receive native length/UTF-8/no-glob checks. An empty browser locator is never passed off as a valid native SourceVersion path or compared to the archive reference as identity.

Archive and known-restored ledger categories each reserve at least `max(sizeof(SourceVersion),48)` per entry, with1MiB/sheet and8MiB/workbook limits. They remain lower bounds for later complete model admission: final unavailable native locator lengths, format payloads and other families are separate. Actual Header arrays, both strings and packet copies have their own budget, including old/new/staged coexistence and caller-retained input. Complete batches reserve128KiB RAW sorting scratch before validation; pending-only batches do not. Failures preserve accepted prefix, SQL/cursor, bytes, prior output and input ownership. No fake SourceVersion/Analysis is fabricated to imply complete model validity.

## Closed native files and packet fixture

The four immutable files are `source-versions-v12.dxls`, `source-versions-v13.dxls`, `source-versions-v14.dxls`, and `source-versions-v14-reordered.dxls`. Each has64 sheets named `Source versions 'İ' s` and the existing56-object counter pattern, including live, retired and exhausted object domains. All current primaries are SOURCE_NONE. Stored Table source metadata remains CSV/`missing-source.csv`/empty schema/literal `NULL` table, with `SELECT * FROM __BP1_SOURCE_VERSIONS_MUST_NOT_EXECUTE`; this stored SQL never executes and remains unarmed in the Header. Current source bindings are zero and source-aware formatting is false.

Older files have empty ledgers and source high-waters0. The small14 file has5 entries (2 pending/3 complete). The reordered14 file has285 (95 pending/190 complete), one actual result split256+29. It includes primary owner0, retired owner9007199254741023, owner/versionUINT64_MAX and a live Table owner9007199254740993. Source and object allocation domains remain independent. References are exact `missing-source.csv` /`.parquet`, alternating CSV/Parquet by global entry ordinal. Pending status is ordinal divisible by3.

Each complete147-byte native RAW packet owns the exact schema `Id UBIGINT`, `City 'İ' VARCHAR`, `Amount DECIMAL(38,4)`, encoded native engine `v1.5.6`, file-byte declaration9007199254741043 and deterministic SHA bytes `(7*i+3)&255`. These are fixture declarations built with the existing codec, **not** observed source-file hashes. The C Worker controller has a bounded independent exact-byte oracle for those declarations; it cannot establish compatibility or activation under the Worker's different engine. Native result/connection closure precedes the owned Header/native comparison; a further graph-retention check follows native model cleanup. Archived packet bytes survive input poisoning and retirement.

Native ordinary Header controls retain1,611 checks /21 cursor executions /35 feeds, optimized0.617/O12.866 s runtime. The final native batch takes49.288 s including a cached optimized build check and15.429 s O1 build. A separate actual optimized compile takes6.331 s. Four-file emission alone takes0.490/1.978 s; emission, read-only inspection, refusal and late-conflict cleanup take4.249 s. The SDK's largest observed copied batch is1,092,237 bytes in pathological archive tests; engine-materialized results and allocator/RSS are excluded. Some isolated archive-budget probes explicitly use32MiB; they do not promise every allowed archive fits the representative2MiB Header/4MiB input driver.

## Native adapter and harness failures

The [initial native failure](native/first-failure/initial-manifest.json) exposed test-adapter truncation: an actual `a || chr(0) || b` reference became `a` through the SDK's deprecated NUL-terminated VARCHAR row accessor before reaching the decoder. Accepting the supplied valid `a` atom was correct; the native full loader rejected the real NUL. The corrected test first checks original schema, then executes one encoded-reference BLOB projection and uses its exact byte length to feed TEXT atoms. The final shared decoder therefore sees and rejects all three bytes. No production loader or JavaScript parser change is involved.

An inserted diagnostic then dereferenced a NULL request after that unexpectedly accepted feed completed. Its [receipt](native/diagnostic-crash/receipt.json) explicitly records reconstructed source and an uncaptured original binary hash. The [retained delta](native/diagnostic-crash/reconstructed-source.patch) reconstructs that diagnostic from the captured first-failure include; the [safe diagnostic](native/diagnostic-3.txt) records the actual truncation. This is a test-only print crash, not a decoder/engine bug or an invented hashed-binary claim.

The [first supplementary inspection](native/initial-emitter-inspection/manifest.json) incorrectly expected stored NULL query rows for blank primaries. Native Save correctly omits absent queries;18 Table sentinel rows are the correct actual inventory. Only the Python oracle changed; native C and emitted formulas stayed unchanged.

The root's [initial runner guard](harness-initial/2.txt) omitted the new source-ledger mode from the full expected command list. Updating that exact list retained core-before-workbook fixture ordering and every old mode. A preceding `python3 -m unittest` import-command mistake is a transient observation in the original manifest, without a separately retained stdout/hash; invoking the test script directly fixed it. Neither is production acceptance or a product failure.

## Actual Worker, adjacent retention and shipping

The actual first-pass run uses one owned headless official Worker and four freshly emitted, sealed native archives. [Native creation protocol](actual-worker/codec-native-create.txt), [file inventory](actual-worker/codec-fixture-manifest.json) and [unchanged served-file proof](actual-worker/codec-native-provenance.json) bind that invocation to its own original bytes; database container hashes need not equal the private emission. No archive is copied here.

Original seven-column schemas are checked before the C SQL projection. `hex(definition)` supplies nullable UTF-8 text while preserving the original BLOB sidecar. JavaScript never decodes packet bytes or stringifies raw Binary; all25 observed chunks have actual physical Utf8 and explicit original types. C validates every non-NULL hex length/parity/digit before allocating an admitted whole-chunk atom vector/decoded arena. Empty hex reaches the shared decoder as a non-NULL zero-byte BLOB; SQL NULL remains distinct. The canonical projection orders by qualified original numeric sheet/version columns, avoiding VARCHAR-alias lexical ordering.

The actual inventory is:

- 14 Meta,14 Sheets,14 Objects,13 Object Counters,12 Source Counters,12 Source Versions and19 DESCRIBE requests: **98** total and86 ordinary callbacks. Normal workloads are `[6,8,12,12]`; including failure/recovery the files receive `[6,8,12,72]`. Every request pins its original registration epoch/ID; no stored source query is executed.
- Dependency feeds are14×56 objects,26×32 object counters and24×32 source counters. Defaults are one object-counter, two source-counter and two empty-ledger families. No forbidden older-version ledger SQL is submitted.
- Ledger delivery has25 chunk callbacks,16 numeric-1 ACKs and12 done callbacks: three successful streams, eight errors and one stale old completion. Accepted data feeds are ten256-row, two29-row and one5-row chunks, followed by three explicit empty EOFs. Temporary C buffers retire before ACK; Header claim follows completion/reader retirement.
- Six shared-decoder rejections and two hex-adaptation rejections preserve rejected prefixes `[256,256,256,256,256,256,256,0]`: required reference NULL, pending encoding with nonempty BLOB, malformed RAW, RESULT discriminator, odd hex, nonhex, empty non-NULL BLOB and concrete **allocated** retained-input pressure at2MiB+1. Odd/nonhex errors allocate no atoms/decoded arena. Nineteen adapted nonempty chunks, including rejected decoder input, are poisoned and freed after feeds.
- The second ledger chunk is genuinely held after a256-row accepted prefix. Cancel drains old completion, then one old chunk replays; both reject cookie/ticket before borrowed-pointer reads. The stale chunk reserves8,770 wire bytes +42 scalar bytes; the stale terminal reserves2,070 bytes. Only then does the new cancellation acknowledgement allow a whole-candidate retry and a fresh completed Header. Prior graph pointers/scalars/reference/packet hashes survive failures, poisoned inputs and final catalog detach.

[Independent retained-receipt validation](receipt-audit.json) checks the exact counters, original binding tokens, per-file queries, canonical numeric ordering, no source SQL and unchanged archive hashes without another runtime. The actual Header maximum is103,826 bytes within2MiB; peak accounted C-boundary category is2,097,216 bytes within4MiB. The bridge counts wire/Page/atoms/decoded copies, prior Header/read/SQL/diagnostics and a conservative128KiB RAW category. The reader separately admits old/new arrays and staged copies/scratch under2MiB. This is not a precise transient allocator/stack measurement, Arrow/engine/browser bound or RSS claim, and does not establish that every full-limit ledger fits the representative driver.

The Worker reports `v1.4.3`; native packets preserve `v1.5.6` as dormant metadata. Exact packet transport/validation does **not** certify cross-engine compatibility, current file identity or active source publication. The new source-ledger driver is not browser App Open.

Adjacent Grid passes the original four-column Columns/Cells path using the same artifact: six Headers,62 requests,53 chunk callbacks/47 ACKs,22 done callbacks, five rejections and held cancellation/stale recovery. Its observed C count is79,583 and Header maximum172,152 bytes (peak2,097,214), separately scoped from the earlier Grid checkpoint. It establishes retention of that existing workflow, not actual App UI/Worker247 or full model publication.

Fresh normal/review Wasm builds at `2bb5bf4` take15.243/16.756 s, artifacts `99baff5ac3d835b69e2b` /`c356b47b51b3935928e8`. [Source-scoped build records](shipping/manifest.json) and the [stronger exclusion receipt](shipping/exclusion.json) check both fresh packages, including the new source-ledger test name/mode and all four stream callbacks. The copied exclusion equals the original `build/web/review-absence.json`. Existing native binary symbol checks also pass with their retained hashes; this is not a fresh native App runtime. There is no actual App UI/Worker247 rerun, native package/service promotion, user-window/tab reload or new queue delivery.

## Remaining scope and retention

Source bindings, format scopes/payloads, full restored-candidate validity and historical migration, writer/output adapters, issuer/publication fences, browser file capabilities/content identity and product Open/Save remain unfinished. Complete packet syntax does not authorize a query, file or active format token. This slice launches no user window/tab, clipboard/file panel, package/service refresh or review-queue delivery.

[provenance.json](provenance.json) records original paths/bytes/hashes and distinguishes exact copies, bounded deltas and committed-source equality witnesses. Original component seals keep their original receipt-relative meaning; this capsule deliberately omits redundant payloads referenced by those original seals. [hashes.json](hashes.json) and [SHA256SUMS](SHA256SUMS) seal this capsule's selected relative layout. No archives, binaries, SDK/vendor payloads, screenshots, full browser event streams, report databases or process/RSS claims are retained.
