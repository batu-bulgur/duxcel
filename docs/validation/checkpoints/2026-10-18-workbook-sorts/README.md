# Bounded primary-sheet sort metadata

The opt-in shared C sort continuation passes pure/native tests and the actual official Worker boundary at `de6e15e`. Original Header/Worker and transformed App Columns regressions plus fresh shipping exclusion also pass. This is an owned **partial Header**, not a complete Workbook, production loader, browser Open/Save or distribution promotion. Counts and timings below belong to separate gates and are not summed.

## Component and native evidence

| Gate | Source and actual acceptance | Timing scope |
| --- | --- | --- |
| [Integrated components](integrated/manifest.json) | `b041729`, unchanged fingerprint; pure3,220/config, C wire2,220/config,55 JS tests and four original harness tests | 12.417s including builds |
| [Pure runner](integrated/pure-summary.json) | `072329-fast-86286`; all1,391 prior Header checks plus1,829 optional-sort checks per optimized/O1 configuration | 3.912s build;0.543/0.719s runs;5.318s runner total (containing command5.523s) |
| [Private pure](pure/manifest.json) | `5676bc4` → root `fa1a4c3`; exact inputs, commands, SDK-header and binary hashes | 3.376s build/run total |
| [Private native](native-private/manifest.json) | `b55c5c7`; actual SDK v1.5.6;15,913 optional-sort checks/config and unchanged1,611 ordinary Header checks/config | Sorts1.467/7.255s; builds3.635/7.999s; default1.105/3.140s;24.607s command total |
| [Root native](native-root/manifest.json) | unchanged `de6e15e`; same15,913 and1,611 checks/config; exact optimized/O1 helper hashes | Header0.745/3.144s; sorts1.521/10.281s;10.219s build;26.069s total |

The [source comparison](component-source-comparison.json) verifies exact integrated implementation/test hashes against private manifests. The later root getter-lifetime documentation differs from the private native header by a comment; that difference is explicit, not called byte-identical. O1 uses ASan/UBSan with macOS `detect_leaks=0`; no LSan claim is made. Pure tests use DuckDB type declarations without an engine; the native comparison executes the actual SDK.

The original meta/sheets path remains independently claimable and unchanged. Explicit continuation validates the primary sort, permits an absent `sorts` table in v12/v13, requires it in v14 and deep-owns secondary names. Its fixed named projection reads `sheet_id,position,name,descending`, ordered by sheet/position with `LIMIT 449`. At most64 sheets×7 secondary entries are accepted; the449th is a rejection sentinel. Feeds remain≤256 rows.

Validation preserves native eight-sort ordering, nonempty≤4,096-byte UTF-8 names and ASCII-insensitive duplicates, including against the primary. Eight names cannot exceed the native32,768-byte name cap. NULL/empty names, malformed UTF-8/NUL, wrong schema, gaps, duplicates, descending sheet order and stale exact keys reject. Prospective `Analysis` values borrow old/staged strings; publication happens only after the whole batch passes. Rejection frees new names and preserves cursor/earlier batches; final Header ownership moves once. Tests cover256/192 splits, retry/cleanup, independent prior output and exact byte thresholds.

Native fixtures contain64 sheets and448 secondary entries in four actual packages: v12, v13, v14 and physically reordered v14. The sort-only29 metadata executions/35 typed feeds describe decoder work, **not all native-process SQL**. Native comparison also accepts the v12 missing-table default and rejects v14 absence/sentinel, retaining owned prior output through fault/budget/stale recovery. Source definitions intentionally reference unavailable data and never execute.

The [supplementary emitter receipt](native-private/emitter-sorts.json) selects original sort records from both configurations: exact file hashes/sizes, read-only CLI metadata, no-overwrite refusal and late-conflict owned cleanup. Its11.691s makes36.299s combined private command time. Unchanged seven-file Header emission remains referenced by its original digest. CLI and SDK have separate hashes. Fresh DuckDB archives differ across invocations; preservation is proved **within** an invocation, not as deterministic serialization.

## Separate bounded transport

The [private wire manifest](wire/manifest.json) and integrated [C](integrated/wire-release.txt)/[O1](integrated/wire-sanitize.txt)/[JS](integrated/js.txt) logs retain2,220 C checks/config and55 real Arrow/adapter/wire tests. No DuckDB Worker/App runs in these module tests. The [private loader shim](wire/pinned-arrow-loader.mjs) resolves the existing pinned Arrow installation; no dependency or spreadsheet runtime is installed.

`submitWorkbookSorts` admits only explicit `workbook-sorts`, offset zero, unknown total, four typed columns and one captured immutable-file registration. It bounds results to449 rows; ordinary submit/decoding remains256 and Copy remains20,000 cells. Existing wire/text/owned limits apply. Frozen requests/descriptors/tokens own their values. Invalid/revoked bindings reject before consuming latestId; checks also run before send and after reader cleanup, and ordinary recovery remains available.

DXP1 itself grants no larger envelope. The trusted C SORTS cursor selects the separate decoder, canonical SQL and≤256-row feeds; it must reject the sentinel and charge simultaneously retained wire/Page/atom buffers. Tokens guard session registration freshness only, not persistent/content identity or arbitrary SQL authorization.

## Actual official Worker sort gate

[Run `20261018T072940-browser-88321`](worker/summary.json) uses exact `de6e15e` and dedicated artifact `d49d8c8e71a31a5d330959c9b8aa6972d7cd6aa2a6db7df982ef9f03ce96f17d`:15.041s CLI including6.580s reported build and6.860s browser. The artifact has no App/window/raylib renderer or product Open/Save activation.

The [C report](worker/c-report.json) passes **77,148 checks**, four archives, six claimed Headers,49 C-submitted requests, six reasoned atomic rejections and one actual superseded completed callback. The separate [outer harness](worker/driver.txt) passes190 assertions. Family requests are12 meta/12 sheets/13 sorts/12 DESCRIBEs; normal work is six per archive. Native fixture generation and ATTACH/DETACH administration are outside these counters.

Six valid448-row results split256/192. The seventh accepted256-row prefix belongs to the rejected449-row sentinel, not another valid Header. Required-NULL/name-NULL, ordering, case-duplicate, sentinel and owned-budget reasons reject without changing the independent prior Header. A held real reply is copied before callback buffers retire; superseding its key rejects before those pointers can be read, then a fresh request recovers. Deliberate isolated callback holding is not physical cancellation or an App workflow.

The [creation protocol](worker/codec-native-create.txt), [four-file manifest](worker/codec-fixture-manifest.json) and [preservation receipt](worker/codec-native-provenance.json) bind read-only emitted bytes/helper hash before and after. The private server serves only that allowlist; browser SHA verifies fetched bytes. The [transport receipt](worker/transport.json) retains every49 ticket, SQL byte length/hash, deduplicated exact SQL and source-binding references. Typed schema precedes canonical C projection; physical reordering preserves named semantics. Final [cleanup](worker/cleanup.json) retires the closed test session.

Observed `peakOwnedInput` is2,252,570 bytes at recorded C callback/feed boundaries; Header limit2,097,152 and input admission4,194,304. These exclude JS/engine/RSS and are not continuous process/all-allocation peaks. Worker fixtures all contain sorts; missing-table acceptance remains pure/native scope.

## Adjacent regressions and shipping

The [adjacent manifest](adjacent/manifest.json) binds unchanged `de6e15e`,76.838s across four serialized commands. Its wrapper times include process overhead; individual run summaries retain distinct timings.

- [Original Header](adjacent/header-summary.json), `073043-browser-89106`:6,294 C checks plus204 external,7.929s CLI/6.818s browser, same dedicated artifact. Seven fresh files,54 requests,11 rejections, one stale callback and exact uint64/tiny-negative/sentinel guards remain. [Fixture/helper preservation](adjacent/header-codec-native-provenance.json) and [C receipt](adjacent/header-c-report.json) stay separate from sort counters.
- [Original Worker247](adjacent/worker247-summary.json), `073059-browser-89434`:125,633 C checks/247 stages,24.466s CLI/15.844s browser including8.497s build. [Baseline/export/verification](adjacent/worker247-worker-workloads.json), [bindings](adjacent/worker247-binding-workloads.json) and [Other](adjacent/worker247-other-workloads.json) preserve original221-stage225 jobs/441 submitted SQL/21 counts; total249 jobs/529 submitted SQL/21 counts across five sequential Workers, including24 Other jobs/88 SQL. Submitted packets are not all executed engine SQL. Sort-driver requests are not added into these totals.
- [Transformed DPR2 App Columns](adjacent/app-columns-summary.json), `073145-browser-90641`:172 assertions,13.634s CLI/13.500s browser. Real C controls publish4×3 Rows City/Columns Approved/SUM Amount with false/true headers, source-derived footer shapes and145.25 grand sum. [Cached readouts](adjacent/app-columns-readouts.json) retain source/manual/request/history and a measured298ms idle interval without another job. Root viewed the original matrix/footer PNG; no PNG is duplicated and no full pixel/UI parity, physical-input or report-upload claim is made.

Five [codec harness tests](harness-unit-five.txt) and32 [runner tests](runner-unit.txt) pass separately. The original four-test harness remains scoped to the b041 component gate; parser/runner tests add no engine coverage.

Fresh [normal/review builds](shipping-builds.json) and [shipping absence](shipping-absence.json) pass at `de6e15e`,29.919s command. Artifacts `32d1a3abd580afa04bb5` /`75d9683306000b2c15df` retain distinct Wasm hashes/exports/configurations. Normal shipping excludes review/test markers and acceptance assets. Both App artifacts exclude the codec acceptance source/callbacks/loader/workflow; review retains its intended review callback. Optimized/O1 native linked-symbol checks find no browser/review bridge and retain exact binary hashes.

This is **not distribution promotion**: the signed package remains prior accepted659;8767/PID75024 still serves prior review1082. No package/service replacement, user-tab reload, user-app launch or new report/queue delivery is claimed.

## Setup diagnostics and remaining limits

The [initial pure receipts](cause/pure-initial-receipts.json) retain commands/results and differing test hashes while factoring repeated source maps. Original `running` labels reflect interrupted manifest writes; nonzero result codes are authoritative. First compile [omitted the SDK include](cause/pure-missing-include.txt); adding its path fixed only the command. A new test then [reused a freed output Header variable](cause/pure-output-reset.txt); resetting it to NULL fixed fixture ownership, retaining assertions and production code.

The two [wire observations](cause/wire-transient-observations.json) are explicitly transient: correct dedicated arity rejection was `PAGE_LIMIT` before schema validation, and a supposed4MiB fixture actually included four extra literal `NULL` bytes. Expectations changed to the correct error and exact4MiB/+1 aggregate sizes. The failed log was overwritten by the passing run; no original failure artifact/hash or product correction is claimed.

The native [temporary-alias receipt](cause/native-tmp-alias.json)/[log](cause/native-tmp-alias.txt) retains a fixture failure where `/tmp` differed from native canonical path resolution. Canonicalizing the owned directory fixed the fixture only. A separate [CLI assertion](cause/native-cli-uint64-string.json) changed integer expectations to actual exact UBIGINT JSON strings. Neither changed production semantics.

The [prior Header checkpoint](../2026-10-18-workbook-header-worker/README.md) keeps its earlier scope. Table sorts, remaining families, legacy migrations and source payloads remain undecoded; no Workbook construction/publication, persisted source execution, browser Open/Save, durable recovery or persistent/content identity is enabled. Full UI parity and later preference-family work remain outside this acceptance.

Only bounded manifests/logs/derived provenance are retained—no executable, archive/database fixture, PNG, full event/input stream, report store or live-session capability. [Provenance](provenance.json) records original hashes and every derivation; [hashes](hashes.json) seal all retained payloads.
