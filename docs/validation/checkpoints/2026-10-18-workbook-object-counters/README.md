# Exact object allocation counters in the partial Header

The counter continuation is accepted through the shared C reader, native DuckDB and the pinned official DuckDB-Wasm Worker. The actual counter run at `c7dbe35` passes **45,403 C checks / 244 external checks**, four immutable native archives, five completed Headers and 54 metadata requests. It preserves exact worksheet-local high-water values, including retired IDs above 2^53 and exhausted `UINT64_MAX` values. This is an object-counter observation, not an ObjectStore adoption, browser ID issuer, complete Workbook or browser Open/Save feature.

## Source and evidence boundaries

| Layer | Tested source | Result / elapsed scope | Receipt |
| --- | --- | --- | --- |
| Initial pure component | `e34f9c7` | 23,220 checks/configuration, 13.782 s including builds; its exact-pressure success claim is superseded below | [original manifest](pure-original/manifest.json) |
| Corrected concrete-pressure test | `cdc666b`, production unchanged | 23,220/configuration, optimized/O1 ASan+UBSan, 11.763 s including builds | [corrected manifest](pure-owned-pressure/manifest.json) |
| Integrated pure reader | `fe1d272`, docs dirty as recorded | 23,220/configuration, 14.803 s total | [manifest](root-pure/manifest.json), [summary](root-pure/summary.json) |
| Native comparison and issuer control | private `adb1054` → root `b409b64` | 30,993 checks / 65 cursor metadata executions / 202 typed feeds per configuration; 1.752 s optimized / 9.862 s O1 sanitizer runtime | [manifest](native/manifest.json), [exact 51-file closure](native/preprocessor-closure.json) |
| Harness | private `fe14309` → root `bc357ee` | Twelve pure mock tests, 1.130 s; previous nine test definitions unchanged | [manifest](harness/manifest.json), [log](harness/runner.txt) |
| C workflow, static | private `bffdb10` plus root observation | Planned counter inventory only; no runtime claim from this receipt | [static receipt](worker-static/manifest.json), [root observation patch](worker-static/root-observation.patch) |
| Actual official Worker + C | `c7dbe35` | 45,403 C / 244 external; 7.421 s browser, 15.883 s CLI, 16.263 s enclosing invocation | [C report](actual-worker/c-report.extract.json), [summary](actual-worker/summary.extract.json), [transport](actual-worker/codec-read-transport.json) |

The enclosing accepted counter batch takes **20.268 s**, including twelve harness tests, 32 runner tests and a cached production-exclusion check. [Commands and times](actual-worker/manifest.json) distinguish those costs from browser runtime. The driver artifact signature is `77c02477e21cbb3b6b55f8cd67eac30dc3a6656aa113e01c7af0040d33983aa4`; its build receipt reports 5.198 s. The wrapper's 7.253 s setup-build field includes prerequisite work and is a different measurement.

Native builds take 6.226/11.320 s. Unchanged ordinary Header controls pass 1,611 checks / 21 metadata executions / 35 feeds in 1.447/3.292 s. Native/build runtime plus emitter checks totals 38.532 s; dependency capture adds 0.850 s separately. Native SDK DuckDB is 1.5.6; the official Worker reports its own engine version in the retained transport. Cursor metadata execution counts exclude native fixture/save/load/inspection SQL and Worker attachment administration.

## Bounded continuation and ownership

`WORKBOOK_CODEC_OBJECT_COUNTERS` appends family ID **17**. It can be armed once after the object descriptor registry completes, before claiming the Header, independently of other optional families. `workbook_codec_header_object_counters_ready()` distinguishes completed observations; `workbook_codec_header_object_high_water()` returns an exact `uint64_t` and preserves caller output on invalid/unready access. No runtime revision, next-ID issue or full SheetObject is fabricated.

Writer v12 forbids a present counter table. Its absent-table continuation performs no SQL and reconstructs the maximum live descriptor ID across every object kind, or zero for an empty sheet. Versions 13/14 require the table. One original named INTEGER/UBIGINT projection is used:

```sql
SELECT sheet_id,high_water
FROM _duxcel_workspace.object_counters ORDER BY sheet_id LIMIT 65
```

There must be exactly one non-NULL row per worksheet, in contiguous sheet order, with high-water at least every live descriptor ID. Zero is valid for an empty sheet; positive retired counters and exhausted MAX are valid even with no live objects. The 65th row is a sentinel, not an extra worksheet. The adapter checks original schema/types before a C-built VARCHAR projection and parses exact U64 text; `Cell.number` is not used for integer identity.

The inline Header POD values add no per-counter allocation. Whole-batch validation and retained-input admission precede publication; rejected batches preserve cursor, owned bytes and prior independent Header graph/string identities. A stale cookie/ticket is rejected before reading the borrowed wire or total. The stale callback reserves its numeric wire size (1,708 bytes observed) and 21 bytes for the maximum canonical total before any borrowed read. These are ownership reservations, not allocator/RSS measurements.

The pure component preserves the previous **21,980** assertion definitions and adds 1,240. Its corrected pressure test owns exactly the charged allocation on each exact/+1 attempt. The original version had retained one unreported byte during a claimed exact-boundary success; only the new test changed. Both receipts are preserved. Earlier new fixtures also used a CSV without a locator resolver and expected the wrong existing column name. Their corrections are [retained with the original pure evidence](pure-original/).

## Native archives and actual workflow

All four archives contain 64 worksheets and 56 descriptors across Pivot, Chart and Table kinds. The repeated eight-sheet pattern covers never-issued empty, empty retired, empty exhausted, live Pivot ID1, live Chart above 2^53, live Table with retired high-water, mixed kinds with rotating maximum kind, and live MAX. V12 intentionally has no retired gap; v13 preserves retired values; v14 additionally uses source-aware format state to select its writer version, without decoding that format state here. Stored primary/Table SQL is a sentinel and **never executes**. Exact `missing-source.csv` references, empty schema and literal `NULL` table/name text survive.

The physically reordered archive swaps counter columns as well as Meta/Sheets layout. Native comparison also exercises reordered object descriptor columns. The factory's actual closed archives are checked read-only for every expected field and exact U64 text. [Emitter receipts](native/emitter-results.json) preserve hashes, modes, independent inodes, existing-input/symlink/source refusal, no-overwrite and late-conflict cleanup. Emission alone takes 0.419/2.306 s. No database, binary or source data is copied into this capsule.

The native gate compares each Header high-water against actual `workbook_load().objects.next_id`, then separately tests native post-load allocation at high-water+1 and honest MAX rejection. It does not activate a browser issuer. Counter results use one SDK execution with 32+32 or single-row feeds. Native faults cover required NULLs, wrong type, duplicate/missing/foreign worksheet rows, below-live counters, the 65th sentinel, forbidden v12 presence, missing v13/v14 packages, concrete budget rejection, stale-before-read and valid recovery.

The [actual clang `-MM` closure](native/preprocessor-closure.json) contains 51 project inputs plus explicit installed SDK/header identities. Both configurations have identical project closures. [Private](native/root-source-comparison.json), [root integration](native/root-integration-comparison.json) and [actual-recovery](native/actual-source-comparison.json) witnesses match those bytes without rerunning native tests. [Selected committed hashes](source-witness.json) distinguish codec, pure-test, native, harness and C-driver changes; they are not a substitute for that compilation closure. The native test wrapper leaves all existing result-feed and family definitions unchanged; only the ordinary Header control is rerun in this slice.

The actual Worker inventory is:

- Ten Meta, ten Sheets, ten Objects, nine Object Counters and fifteen DESCRIBE requests: **54** total. Normal fixture workloads are `[6,8,8,8]`, with no v12 counter query. Original captured registration bindings remain fixed on every request. Counter/descriptor projections contain no OFFSET; ordinary Meta/Sheets keep their existing one-shot OFFSET0 envelopes.
- Ten accepted 56-descriptor feeds, eleven accepted 32-counter feeds, four completed counter groups and one v12 default. Four adopted counter result buffers are poisoned after copying.
- Four rejected feeds at prefixes `[32,32,32,0]`: below-live high-water, required NULL, duplicate worksheet identity and concrete retained-input pressure. Each preserves old Header ownership. One actual completed callback is held, superseded and replayed stale, followed by fresh valid recovery.

The completed Header maximum is **46,260 bytes** within the 2 MiB Header limit. Peak accounted C-boundary coexistence is **2,097,216 bytes**, including forced pressure, within the separate 4 MiB retained-input limit. This covers declared owned C graphs, wire/Page/atoms/schema/SQL and diagnostics at observed boundaries; it is not engine/Arrow/browser/allocator/RSS peak or a whole-file load guarantee.

## Natural failures, corrections and remaining scope

The initial native new test incorrectly required `finish` to fail after an optional counter arm had itself been rejected. The unchanged Header remains claimable in that state; only an armed pending family prevents finish. The new assertion was corrected without changing the reader or feed/error checks. [Original failure](native/initial-arm-assumption/manifest.json). Supplementary inspection-script syntax/reserved-alias corrections are recorded separately and are not product failures. The initial harness mock likewise expected “Incomplete” rather than the actual “Unexpected” wrong-inventory diagnostic; [its original failure](harness/initial-inventory-assumption/manifest.json) is retained.

The first real Worker run at `ec3e44e` fails before its first counter execution, after 8,585 C checks / 13 requests. The test driver's canonical-schema helper still assumed 25 columns for the two-column counter table. Its projection admission correctly refuses. The [original report](initial-worker/c-report.extract.json) and [22.123 s CLI summary](initial-worker/summary.extract.json) retain that failure. The [test-driver correction](initial-worker/driver-correction.patch) derives schema width from the catalog, guards its fixed bound, and replaces generic Page SQL paging with the dedicated two-column canonical projection. This is a test-driver correction, not a production decoder/engine failure. Recovery at `c7dbe35` passes all unchanged counter assertions.

The adjacent actual Grid check at `c7dbe35` passes **79,583 C / 282 external checks**, retaining its original 62-query / 53-chunk / 47-ACK / 22-completion workload after the common schema-helper correction. It takes 6.797 s browser / 9.190 s CLI / 9.589 s enclosing invocation and reuses the same counter-driver artifact. Maximum Header/input observations are 170,096 / 2,097,214 bytes. The added schema guards and inline counter storage distinguish these counts/sizes from the earlier Grid checkpoint. [Adjacent scope and receipt](adjacent-grid/manifest.json), [independent evidence audit](receipt-audit.json). The cached exclusion check covers the previously built `7a1c7d3` normal/review artifacts `7f7de0534d342fa08f73` / `ba727756ed340db23ba6`; it is not a fresh `c7dbe35` App build. [Original fresh artifact scope](../2026-10-18-workbook-grid-streams/initial-integration/shipping-exclusion.json). No broad old-family matrix, original Worker247, App UI/Columns172, native GUI, package/service refresh, user-tab reload, clipboard/file panel or queue delivery is claimed here.

Source-version counters, source ledger/bindings, format payloads, historical migration, final whole-candidate validation and output adapters remain incomplete. A ready optional counter family does not imply a complete Workbook or browser Save/Open. This checkpoint introduces no visual-parity claim.

## Retention

[provenance.json](provenance.json) distinguishes exact original copies, bounded JSON extracts and git-object witnesses with original byte counts and hashes. Extracts omit duplicate transport, full vendor inventory and event payloads. [hashes.json](hashes.json) seals this final bounded payload. No binaries, databases, vendor assets, screenshots or full browser event streams are retained.
