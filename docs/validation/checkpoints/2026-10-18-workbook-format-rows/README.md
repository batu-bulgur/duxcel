# Base format property rows

This continuation owns the thirteen scalar properties stored in native
`formats` rows. It exposes ordered, borrowed POD views; it does not expose a
complete FormatStore or publish a Workbook. Selectors, source-aware mode,
scope/guard payloads, history, migration, rendering and browser Open/Save remain
outside this slice. Existing dormant bindings and RAW packets retain their own
[earlier acceptance](../2026-10-18-workbook-source-bindings/).

| Gate | Tested scope | Result |
| --- | --- | --- |
| [Private pure](pure/manifest.json) | `f6a92f2`, exact25 project inputs plus type header, no engine | 48,288/configuration: prior38,778 +9,510; 17.533 s build/run |
| [Root pure](root-pure/manifest.json) | `689f0a7`, same private preprocessor bytes | 48,288/configuration; 24.520 s total, including20.386 s build |
| [Native comparison](native/manifest.json) | `f3f7328`, native DuckDB v1.5.6, exact55 project inputs plus SDK | 280,343/configuration; 297 metadata executions /411 feeds; optimized8.809 s, O1 ASan/UBSan63.405 s |
| [Native publication](native/publication.json) | Four immutable writer archives per configuration | optimized2.391 s /O1 13.039 s; exact metadata, readonly hashes, refusal and owned cleanup |
| [Stream memory tests](node/manifest.json) | `ccebe09`, one policy entry; chunker/helper unchanged | 205 groups:193 retained +12 appended; 4.377 s wrapper /4.191 s Node |
| [Controller](static-controller/manifest.json) | `dd114c4`,183 lines /69,867 bytes | Static-only; this receipt claims no compilation or runtime |
| [Harness](harness/manifest.json) | Pre-integration `0b90063` | 24 codec guard tests /32 runner tests and syntax checks pass |
| [Initial actual run](initial-runner-failure/summary-extract.json) | `519fa4d`, complete C result; outer runner fails | C reports364,221 checks /207 requests; ordinary-held expectation was wrong |
| [Corrected actual Worker](actual-worker/manifest.json) | Artifact built at `519fa4d`; runner `ceb53e5` | 364,221 C /906 external checks,207 requests; 17.030 s CLI /10.166 s browser |
| [Adjacent bindings](adjacent-bindings/manifest.json) | Same artifact /`ceb53e5` runner | 174,408 C /717 external checks,166 requests; 10.649 s CLI /8.467 s browser |
| [Fresh shipping](shipping/manifest.json) | Normal/review at `ceb53e5` | Builds and format-controller exclusion pass; no App runtime |

Private pure, native and stream test gates pass on their first attempts. The
original pure definition prefix312,824 bytes and6,382-byte main statements are
preserved. Native source-preservation witnesses retain the earlier result
adapter, eleven prior includes and original main path. The
[root native comparison](native/root-source-comparison.json) checks all55
project inputs plus installed header/library against `c43bb14`. A
[second Git-object witness](runtime-correction-source-comparison.json) matches
the55 project inputs against the original driver build and corrected runner;
it also proves the C driver, host and stream modules are unchanged across the
three tool-only corrections. Neither witness claims another native execution.
No earlier maximum-family native gate is
relabeled as current. LSan is disabled on the macOS runtime; O1 ASan/UBSan halt
on errors.

## Scalar continuation and ownership

The C-built request is:

```sql
SELECT sheet_id,position,r0,r1,c0,c1,mask,flags,alignment,color,font_size,numeric_format,decimals
FROM _duxcel_workspace.formats ORDER BY sheet_id,position LIMIT 32769
```

The original types are INTEGER/INTEGER/UBIGINT/UBIGINT followed by nine INTEGERs,
all required and non-NULL. The table is required in writer versions12–14.
Version14 arming also requires complete dormant source bindings and ledger.
At most512 rows per worksheet and32,768 across64 sheets are accepted;32,769 is
an overflow sentinel. Worksheet order and each worksheet's contiguous position
are checked before publishing a batch. Ranges/masks/styles are captured without
narrowing loss, then checked through existing shared `format_valid` using a
nonowning one-patch spatial view. Row order, unmasked property bytes and
`specified=0` are preserved; maximum row/column coordinates remain exact.

Geometric Header arrays, old/new array coexistence and caller-retained input
are charged before allocation. A separate known-native reservation uses
`max(sizeof(FormatPatch),96)` per row. Version14 combines format reservations
with both archived and restored ledger categories at1MiB per sheet /8MiB per
workbook. Raw reference, independently resolved locator and exact packet bytes
remain separate. Logical categories do not measure allocator/RSS/engine peaks.
Pure full-domain probes use an explicit64MiB test allowance; native combined
boundary probes use32MiB. Neither allowance widens the representative2MiB
Header /4MiB input policy planned for the actual Worker.

Feeds are transactional at the batch boundary. Native negative tests preserve
an accepted256-row prefix, or512 for a per-sheet overflow, along with the prior
Header/POD/query/Table/ledger graph, pointers and bytes. A stale key rejects
before borrowed input is read. Required NULL, scalar capture, mask/style/range,
position/order/identity, original type and concrete allocated pressure errors
must leave a corrected same-cursor retry possible.

## Writer fixtures and transport

The files are `format-rows-v12.dxls`, `format-rows-v13.dxls`,
`format-rows-v14.dxls` and `format-rows-v14-reordered.dxls`. They extend the
earlier64-sheet binding fixtures. Each small file has seven format rows:
three on sheet1 and four on sheet2. The reordered file has285 rows:257 on
sheet0 and28 on sheet1, so one immutable result crosses256+29. The final
physical format column order is reversed while named schema checks and C
projection preserve the canonical order. Broad, single-cell and maximum-edge
ranges plus alternating masks retain their exact property bytes. Source SQL
sentinels are stored metadata and never execute.

The dedicated stream family is `format-rows` under
`purpose:'workbook-metadata-stream'`: exactly13 dense original sidecars, one
captured immutable archive registration, offset0, unknown total and at most
32,769 result rows. Ordinary Page policies remain unchanged. One engine reader
delivers at most256 rows /4MiB combined Page buffers /6MiB wire per chunk,
awaiting ACK before advancing. A genuinely empty final Page is delivered only
after EOF, strict reader retirement and binding checks. The test-only callback
helper retires temporary C borrows before its synchronous numeric1 ACK.

Memory tests cover exact U64 values, malformed numeric NULL/empty/literal atoms
preserved for later C rejection, dense/arity/cross-family denial before ticket
or send, a transport-only32,769-row sentinel, byte/row limits, suspended ACK,
held second-chunk cancellation/stale replay, fresh recovery, allocation/final
ACK failure and fail-closed reader cleanup. The original six-query transport
probe is byte-identical. These memory tests are neither an actual C ABI nor an
official Worker execution.

## Initial runner correction

The first actual run finishes the C/Worker workflow with `ok:true`, all207
queries and the full counter/proof inventory. The outer runner then fails at
32 checks with ["Held actual callback transport/release receipt changed"](initial-runner-failure/result.json).
The profile correctly reports zero ordinary held replies and one held stream
chunk; the shared outer guard incorrectly expected an ordinary held reply.
This is an observed runner failure, not accepted overall first-pass success or
a C/host/fixture failure. The bounded full-C-result extract and original SHA
preserve that distinction without copying the browser event dump.

The [correction note](harness-correction/correction-note.json) and
[source diff](harness-correction/first-correction-source.diff) retain a separate
test-edit mistake: a global replacement briefly appended two format-only
mutations to an older binding test. Its guard rejected that edit; the binding
test was restored byte-for-byte before continuing. Corrected24 guard tests
include36 corrupted format receipts, including the ordinary-held misuse.
The [receipt replay](harness-correction/replay.mjs) exercises the completed
original result against corrected outer guards; it is a receipt-only replay,
not another Worker execution. The fresh corrected actual run below is the
accepted overall gate.

## Actual Worker and retained ownership

The accepted no-window workflow uses cached artifact `7ea1bdedce558f5e10a7`,
originally built in13.375 s at `519fa4d`. Its runner is `ceb53e5`. Both exact
source snapshots, including the preserved untracked `tools/ui-review/` status,
are retained in the [summary extract](actual-worker/summary-extract.json).
This is not a freshly rebuilt `ceb53e5` binary. The first failed runner and
accepted rerun use unchanged C, host, stream and fixture-generator bytes.

Four privately emitted immutable archives complete five Headers over207
metadata requests. Normal workloads are `[18,20,26,26]`; retries extend only
the last archive. There are13 each of Meta, Sheets, Objects, Queries, Source
Options, Table Sources/Filters/Sorts and format-row executions;12 object-counter,
11 source-counter,11 ledger,11 binding and45 original-schema DESCRIBEs.
These count only C metadata requests, excluding engine initialization,
attach/detach and native fixture Save/Load/inspection SQL. Every request retains
its original immutable registration token. Source query sentinels never execute.

Thirteen format streams deliver22 matched data callbacks and five matched empty
EOFs. A further held chunk replays stale, giving28 physical format callbacks;
20 numeric1 ACKs and13 done callbacks include five successes, seven errors and
one cancellation. The dependency ledger adds11 streams /32 callbacks /32 ACKs
/11 done. Host aggregate counters are24 submissions /60 chunks /52 ACKs
/24 done with one held/released/cancelled stream and zero ordinary held replies.
All physical format chunks are canonical Utf8 carrying the thirteen original
SQL sidecars; C checks original schema before canonical projection. Neither
Arrow nor JavaScript evaluates format or packet semantics.

Seven negatives preserve prefixes `[256,256,256,256,256,512,0]`: required NULL,
invalid mask, flags that cannot narrow losslessly, out-of-range coordinates,
wrong position, sheet row overflow and actual allocated pressure. A held second
format chunk is cancelled after256 rows; old done then old chunk reject stale
before pointer adaptation, followed by a whole-candidate retry and fresh
recovery. All22 matched format data chunks are poisoned/freed after adaptation.
Old Header identities, exact RAW bytes, Table/query/POD properties and independent
empty locators survive. The five nonzero dormant bindings remain deferred and
all Header `Analysis.source_version` values remain zero.

The completed Header maximum is151,862 bytes under2MiB; peak accounted input is
2,097,216 bytes under4MiB, including forced pressure. Stale wire reservation is
2,951 bytes, with42 scalar and2,070 terminal reservation bytes. These are owned
logical categories, not measured allocator/stack/engine/Arrow/browser/RSS peaks.
Native RAW `v1.5.6` declarations remain exact dormant bytes under observed
Worker `v1.4.3`; compatible engine/file proof or activation is not established.

The adjacent166-request bindings flow keeps its original five negatives,
ordinary held stale reply, complete source definitions and seven-column ledger
dispatch. Its completed Header maximum is117,046 bytes. It does not repeat
Grid, Worker247, App UI, native GUI or full native maximum-family acceptance.

## Evidence boundaries

[Fresh normal/review shipping](shipping/manifest.json) produces
`2628959ad40a01a0f325` /`36dc5fd2f17267bc91a6` at `ceb53e5` in18.431/14.637 s
internally,22.722/18.076 s command time. Normal reuses58 objects; review reuses61
and correctly recompiles its generated-provenance consumer. The
[exclusion receipt](shipping/exclusion.json) verifies the new format controller,
mode and codec callback markers are absent from production App packages.
Existing native symbol observations retain their separately hashed binary
scope; no native rebuild or execution occurs here.

Always scanning dependencies has a material build cost even with cache hits;
the normal object phase takes14.691 s, including3.414 s broad input validation.
Avoided physical compilation is not a universal latency improvement. The
[separate cache checkpoint](../2026-10-18-wasm-object-cache/) keeps its original
cold/warm source scopes. A faster reuse policy is not accepted by this capsule.

[Provenance](provenance.json) records original paths, lengths and hashes. The
native directory retains its original small47-payload seal; pure manifests
inventory their bounded receipts. The accepted actual C report is retained once;
result/summary extracts remove duplicate reports/vendor inventories with an
explicit recipe and original SHA. The adjacent and initial-failure extracts
retain complete C scalars/proofs and transport counters while omitting their
repeated query lists. No binaries, archives, SDK/vendor data, browser event dumps
or previous large capsules are copied. Fresh native
distribution, App UI/GUI/visual parity, service/queue delivery and full browser
workbook Open/Save are not claimed. Selectors, mode, scopes/guards/membership,
historical migration and complete candidate model validation remain open.
`SHA256SUMS` seals the curated payloads; the receipt audit verifies sources,
counter contracts, immutable bindings and the retained original hashes without
running compilers or engines.
