# Typed workbook header

The bounded component, native comparisons and corrected runner inventory pass within the separate source scopes below. This decoder is **not linked into the production App or loader**. Its opaque partial Header is never a Workbook, and none of these gates activates native/browser Open/Save through it.

## Component boundary and ownership

The new `workbook_codec_read` component accepts exact typed meta/sheets families for well-formed writer versions12/13/14. A caller supplies an already-checked version/layout, typed atoms and a trusted locator resolver; it does not execute source definitions. Existing SQL identifiers select named columns, preserving physical-column independence and the explicit24-column legacy sheets layout by projecting the25-column header view.

Three nonzero key scalars—document epoch, operation ticket and snapshot generation—are compared exactly. Stale feeds are ignored before reading the batch or changing state/error; these keys do not themselves issue tickets or prevent wrap. Each family has one SQL execution with successive owned-result batches; a feed cursor is not permission to rerun the query for every slice. The bound is256 rows/feed,65,536 bytes/text and64 sheets with a65th sentinel rejected.

Input atoms/descriptors are borrowed for one feed. Accepted values, text and resolved paths become independently owned; malformed/budget/resolver failure preserves the prior reader. The resolver must return its own precisely described allocation, including on a rejected result, and the reader retires it. `finish` moves only a complete meta/sheets Header once; failure preserves independent owned operands. Cells, objects, formatting, source ledger, query definitions and preferences are not decoded or published as a complete workbook here.

The logical owned budget includes cursor/Header, temporary batch/copies, resolved paths and explicit caller-retained input. It is not an allocator, parser, engine or process/RSS peak. The native adapter supplies original chunk VARCHAR lengths and validity, preserving SQL NULL, empty text, literal `NULL`, exact uint64 and embedded-NUL rejection. No C-string guess is used as a source-byte proof.

## Source-scoped pure evidence

| Gate | Exact tested scope | Results |
| --- | --- | --- |
| [Initial pure component](component404/manifest.json) | Private `404e460`, tested-file hashes match commit |1,379 checks/configuration;2.839s including optimized/O1 builds, runs and symbol check |
| [Strict DOUBLE boundary follow-up](component08/manifest.json) | Private `08ebebc`, tested-file hashes match commit |1,391 checks/configuration;3.544s including optimized/O1 builds and runs |
| [Integrated root pure gate](pure-root/summary.json) | `6599c23`, fingerprint `a8ea1c20…`; exact binaries unchanged across runs |1,391 checks/configuration; build2.745s, optimized0.373s, O1 ASan/UBSan0.547s, total3.756s |

The [404](component404/release-run.txt) and [08](component08/release-run.txt) logs retain original counts rather than treating commit prefixes as assertion totals. Tests cover schema/type/null/text/viewport validation, exact keys, stale feeds, deep ownership and atomic budget failure. The strict follow-up validates original DOUBLE bounds before float narrowing, including tiny negative horizontal offsets and values just outside canvas limits. The [pure binary imports](component404/release-nm.txt) contain no DuckDB engine imports; this scope is a C component, not an engine/GPU/browser gate. macOS component ASan uses `detect_leaks=0`; no LSan claim is made.

## Actual native metadata comparison

The private [manifest](native-private/manifest.json), exact tested commit `10ed08b`, and [optimized](native-private/release-run.txt)/[O1](native-private/sanitize-run.txt) logs pass **1,611 checks per configuration** in15.624s total including build. They create private native v12/v13/v14 archive fixtures, compare the partial typed Header against the existing native loader, then verify every archive byte is unchanged by reading. The actual SDK reports v1.5.6, with header/library hashes retained.

Seven comparisons cover each version, physical meta/sheets column reorder for each, and v12 sheets without `canvas_zoom` (24 columns). The adapter records **21 metadata executions /35 typed feeds per configuration**, including malformed schema/result attempts. These are adapter counters, not every DuckDB query in fixture creation, archive mutation or the legacy loader comparator.

Prepared-statement schema and owned chunk/vector APIs provide exact atoms; no legacy `duckdb_value_*`/row-count/column-data API is mixed into the same result path. Result resources can be freed before the completed Header is inspected. Cases retain missing source paths, a deliberately unexecutable source-query sentinel, NULL/empty/literal text, high uint64 keys, stale feed rejection, real length-three embedded NUL and budget denial. Native source definitions never execute and no UI, OS file panel or clipboard is used.

The retained [initial resolver fixture failure](native-private/initial-noncanonical-resolver-setup.txt) used caller `/tmp` spelling as resolver context while native loading canonicalized to `/private/tmp`. The test adapter was corrected to the actual `native.path`; no product code or assertion was weakened. This is a fixture setup correction, not acceptance of inconsistent document identity.

Fresh root comparison source `6766749` passes the same1,611 checks in both configurations: [optimized](native-root/release.txt)1.226s and [O1](native-root/sanitize.txt)2.870s after11.238s build. The [initial manifest](native-root/initial-manifest.json) also records a failed runner unit expectation after those passes. Its [failure log](native-root/initial-runner.txt) expected the old full-tier command list, omitting the newly added native-header test in each configuration. That initial combined invocation is not counted as an all-green gate.

The [final acceptance receipt](native-root/final-acceptance.json) at `95a2d9b` updates only that expectation, records32 [passing runner unit tests](native-root/final-runner.txt) in2.150s, and retains the original676 native source scope. The final receipt also records the native comparison binaries' hashes; it does not pretend those engine runs occurred again at95a2 or replace the initial failure.

## Adjacent existing App and distribution are separate

Existing four-stage workspace App checks pass at `6599c23`: [optimized](adjacent-app/release/summary.json) App3.005s /total8.000s and [O1](adjacent-app/sanitize/summary.json) App3.110s /total7.255s. These owned hidden native runs check the existing production path after the separate policy/query changes. Their477-check background preflight, binary hashes and runtime logs are retained. They do not use the new header decoder and are not a fresh chart40, review400 or full-App regression.

The adjacent [quiet signed package](delivery-adjacent/workbook-policy-package.json) takes1.694s at659, with [packaging log](delivery-adjacent/workbook-policy-package.txt), signature verification and exact optimized App input hash. [Native release](delivery-adjacent/workbook-policy-exclusion-native.txt) /[O1 exclusion](delivery-adjacent/workbook-policy-exclusion-native-sanitize.txt) take0.576s /0.341s. This is the policy/query package refresh, not decoder-linked product activation. The browser service remains accepted product1082 unchanged; no new browser runtime, persistence or feedback-delivery promotion is claimed.

## Remaining integration

Further workbook families, complete Workbook construction, atomic App publication, production native loader adoption, Worker/browser decoding, browser Open/Save, broader historical coercions and full UI remain outside this checkpoint. The current native production source-format implementation remains separate, including the already accepted `554b591` budget-query correction. Helper and comparison success does not complete those milestones.

Only compact original manifests/logs and ownership/source provenance are retained. No binaries, archive fixtures/generated databases, screenshots, report archives or event/input streams are copied. [Provenance](provenance.json) records each original artifact; [the hash manifest](hashes.json) seals every retained payload. Current STATUS/ROADMAP/TESTING documents belong to the integration owner.
