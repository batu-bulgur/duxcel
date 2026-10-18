# Independent source-version allocation counters

Historical checkpoint. Machine-generated payloads were removed from the current checkout on 18 October 2026; they remain recoverable at Git ref `archive/pre-cleanup-20261018`. Paths and measurements below describe that original run. See the [retention policy](../../README.md).

The shared C partial Header now retains source-version high-water observations separately from object allocation counters. Actual official Worker acceptance at `3e98de2` passes **49,485 C checks / 290 external checks**, four immutable native archives, five completed Headers and 64 metadata requests. This adds family **18**, not a source ledger, source binding, version issuer, source activation or browser Workbook Open/Save feature.

## Source-scoped results

| Layer | Tested source | Result / elapsed scope | Receipt |
| --- | --- | --- | --- |
| Pure source-counter reader | `a3cb8c5` | 24,739 checks/configuration, optimized/O1 ASan+UBSan; 13.341 s including builds | manifest (archived `pure/manifest.json`), original seal (archived `pure/SHA256.json`), 31-input inventory (archived `pure/source-inventory.json`) |
| Integrated pure reader | `682864e`, after comment-only boundary correction | 24,739/configuration; 13.663 s including builds | manifest (archived `root-pure/manifest.json`), summary (archived `root-pure/summary.json`), comment patch (archived `pure/header-boundary-comment.patch`) |
| Native metadata comparison | private `5d275e7` → root `de780b1` | 44,937 checks / 79 cursor executions / 106 typed feeds per configuration; 2.599 s optimized / 11.293 s O1 sanitizer runtime | manifest (archived `native/manifest.json`), 52-file closure (archived `native/preprocessor-closure.json`) |
| Harness | private `5bed02f` | Fifteen pure mocks, 1.035 s; prior twelve definitions unchanged, 24 corrupted-report checks | manifest (archived `harness/manifest.json`), log (archived `harness/runner.txt`) |
| C workflow, static only | private `72562fd` | Approved inventory/source checks; this receipt makes no runtime claim | manifest (archived `worker-static/manifest.json`), original seal (archived `worker-static/SHA256.json`) |
| Actual official Worker + C | `3e98de2` | 49,485 C / 290 external; 7.035 s browser, 20.599 s CLI, 20.934 s enclosing invocation | C report (archived `actual-worker/c-report.extract.json`), summary (archived `actual-worker/summary.extract.json`), query transport (archived `actual-worker/codec-read-transport.json`) |

The accepted enclosing batch takes **26.067 s**, including fifteen harness tests, 32 runner tests and a cached exclusion check. Its commands and times (archived `actual-worker/manifest.json`) remain distinct from browser runtime. The standalone driver artifact is `c452125e95a56cf71fc9d57448ccbadbae154c77affe613349417a58f649b69a`; its build receipt reports 4.849 s. The wrapper's 11.896 s setup-build field includes prerequisite work and is a different measurement.

All new pure, native, publication and actual Worker gates pass on their first run. There is no failure/recovery-of-implementation narrative invented for this slice; deliberately invalid input/rejection cases remain part of the passing tests. Earlier object-counter test corrections retain their separate [checkpoint](../2026-10-18-workbook-object-counters/).

## Counter continuation contract

The once-only `workbook_codec_read_add_source_version_counters()` continuation requires an unclaimed Header with complete object descriptors and object counters. Readiness and `workbook_codec_header_source_version_high_water()` expose only borrowed Header observations. An invalid/unready getter preserves caller output. No `SourceVersions` store, ledger entry, Analysis binding or fake runtime revision is constructed.

Writer versions 12/13 forbid a present source-version-counter table, even if empty; absence completes with zero for every worksheet and **no query**. Version 14 requires one exact INTEGER/UBIGINT row per worksheet:

```sql
SELECT sheet_id,high_water
FROM _duxcel_workspace.source_version_counters ORDER BY sheet_id LIMIT 65
```

Sheet IDs must be contiguous from zero, both fields non-NULL, with at most 64 rows plus the rejected 65th sentinel. Whole-batch/EOF validation occurs before publishing POD scalars. High-water is an exact uint64, including zero and MAX. It is deliberately independent of an object's maximum/live/retired ID: object MAX with source zero is valid in this component. Consistency with unparsed source-version ledger entries remains deferred.

The family adds one fixed Header-owned scalar per worksheet and allocates nothing during feeds. Admission includes retained caller input before publication; errors preserve cursor, owned bytes and the independent previous Header graph/text identities. Stale keys are ignored before borrowing callback wire/scalars. The actual stale callback reserves 1,468 wire bytes and 21 total-string bytes numerically before any borrowed read. These are declared owned-buffer reservations, not allocator/RSS measurements.

The pure component retains all **23,220** previous assertion definitions and adds 1,519. Tests cover both completion/default domains, 64/65 bounds, split feeds, original typed shape, explicit EOF, required NULL, foreign/missing/duplicate sheet identity, stale-before-read, exact U64, output retention and Columns/Cells composition. An exact 8 MiB admission case owns a real 8,258,204-byte pressure allocation beside reader65,026 / old Header62,226 / borrowed fixture3,152 bytes. A real +1 allocation is rejected. This is a pure logical C boundary; it is not the actual representative Worker peak or a whole-file memory claim.

The `682864e` correction changes only the Header completion comment, explicitly retaining source ledger/bindings/formats/final validation outside the boundary. Root pure acceptance follows that comment change; the production implementation and pure test bytes match the component. Selected committed hashes (archived `source-witness.json`) distinguish those scopes.

## Native archives and comparison

Four files, `source-counters-v12/v13/v14/v14-reordered.dxls`, contain 64 worksheets and 56 Pivot/Chart/Table descriptors. The [object-counter pattern](../2026-10-18-workbook-object-counters/README.md) supplies the independent object ID domain, including retired and exhausted values. Source counters are zero for versions 12/13; v14 repeats `[0,1,9007199254741043,UINT64_MAX]`. The ledger is empty, Analysis source-version bindings are zero, and `formats.source_aware` stays **false**. Native minimum-version selection writes v14 because of its nonzero source high-water alone; no forced version rewrite or format activation is used.

Primary and Table metadata retain exact `missing-source.csv` references, empty schema, literal `NULL` table/name text and stored `SELECT * FROM __BP1_SOURCE_COUNTERS_MUST_NOT_EXECUTE` definitions. The source file does not exist and stored source SQL never runs. Native Save/Load comparisons inspect the actual `source_versions.high_water`, independently of `objects.next_id`; the C Header stores neither full model. A zero source counter beneath object MAX is explicitly tested without a fictitious lower bound from the other domain.

The reordered emitted v14 archive swaps source-counter columns plus Meta/Sheets physical order. The focused native comparison separately reorders object/counter columns too. Named projections validate original schemas; actual SDK vectors supply exact U64 atoms without double conversion. Counter results are executed once and split into bounded feeds. Native faults cover both older forbidden presence, v14 absence, required NULL/type/order/count/65th sentinel, explicit EOF, independent zero/MAX domains, concrete budget rejection, stale callbacks and valid retries.

Ordinary Header controls retain 1,611 checks / 21 metadata executions / 35 feeds in 0.568/2.858 s. Native builds take 6.834/11.681 s. Native builds/runs plus read-only publication checks total **40.937 s**; dependency capture adds 0.985 s separately. Four-file emission alone takes 0.405/2.582 s. Publication evidence (archived `native/emitter-results.json`) checks exact values, NULL/empty/literal source metadata, physical schemas, unchanged hashes, immutable modes/inodes, no-overwrite/source/symlink refusal and late-conflict owned cleanup. No archives or binaries are retained here.

The actual native `clang -MM` closure has **52 project inputs**, with explicit SDK/header identities. Both configurations match; root's integration witness (archived `native/root-source-comparison.json`) and the independent actual-source comparison (archived `native/actual-source-comparison.json`) match every input at `3e98de2`, without another native run. Native uses SDK DuckDB1.5.6; the retained official Worker transport reports `v1.4.3`. The 79 cursor executions are not every native fixture/save/load/inspection SQL statement.

## Actual official Worker evidence

The Worker checks original I/U types before C-built VARCHAR projections, preserving exact U64 text rather than `Cell.number`. Each request freezes the archive's original registration epoch/ID. The recorded inventory is:

- Ten Meta, ten Sheets, ten Objects, nine Object Counters, eight Source Counters and seventeen DESCRIBE requests: **64** total, with 63 observed callbacks. Normal workloads are `[6,8,10,10]`; actual per-file totals including faults/recovery are `[6,8,10,40]`.
- Ten accepted56-object feeds, eighteen accepted32-object-counter feeds and nine accepted32-source-counter feeds. Nine object-counter / three source-counter results complete and are poisoned after copying. Defaults are one object-counter family and two source-counter families.
- Four rejected source feeds at prefixes `[32,32,32,0]`: required source high-water NULL, foreign sheet64, missing final row63 at actual premature EOF and concrete retained-input pressure. Prior Header graph/string ownership and accepted prefixes survive. One actual completed reply is held and replayed stale, then a fresh complete candidate recovers.
- All nine proof flags pass, including `independentDomains`, `exactSourceMaximum` and no older source-counter query. Both counter projections use the ordinary65-row policy, named canonical casts, ordered inner SQL and no OFFSET; Objects retains its existing explicit2049 policy. Existing one-shot Meta/Sheets OFFSET0 envelopes are unchanged.

Independent receipt checks (archived `receipt-audit.json`) verify the terminal inventories, each captured binding, version-specific query absence, canonical SQL and absence of source SQL among recorded requests. The sealed four-file inventory and unchanged served-file provenance (archived `actual-worker/codec-native-provenance.json`) bind this run to its own closed native archive bytes, without copying those files.

Maximum completed Header is **46,772 bytes**, within the 2 MiB limit. Peak accounted C-boundary coexistence is **2,097,216 bytes**, including deliberate pressure, within the separate 4 MiB input limit. This counts declared C graphs/SQL/schema, wire/Page/atoms and observed diagnostics; it excludes Arrow/engine/browser/allocator/RSS and does not establish full-limit workbook loading.

## Shipping and remaining scope

The cached exclusion command checks the existing `7a1c7d3` normal/review artifacts `7f7de0534d342fa08f73` / `ba727756ed340db23ba6`. It is not a fresh `3e98de2` App build. Original fresh shipping scope (archived `../2026-10-18-workbook-grid-streams/initial-integration/shipping-exclusion.json`). No broad old-profile matrix, native App/GUI, original Worker247, App172, package/service/tab refresh, clipboard/file panel or queue delivery is repeated here. The actual source-counter flow composes the object-counter family; pure Columns/Cells composition is retained. The common catalog-schema helper is unchanged from the preceding [accepted counter/Grid retention](../2026-10-18-workbook-object-counters/README.md).

Ledger entries, counter/ledger consistency, bindings, canonical source proofs, format payloads, historical migration, final whole-candidate validation and writer/output adapters remain open. Optional Header readiness is not Workbook publication. Browser Open/Save and UI parity remain unclaimed.

## Retention

provenance.json (archived `provenance.json`) records each original path/byte count/hash and distinguishes exact copies, bounded JSON extracts and git witnesses. Original component seals retain their original receipt-relative meaning. hashes.json (archived `hashes.json`) seals this capsule's selected layout. Full vendor inventories, duplicated transport, binary/database assets, screenshots and browser event streams are omitted.
