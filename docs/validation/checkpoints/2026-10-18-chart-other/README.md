# Source-derived chart Other

This checkpoint accepts bounded source-derived Other values through the native engine, official DuckDB-Wasm Worker, and actual shared-C native/browser UI. It preserves each tested source and artifact separately. Component checks, analytical semantics, interaction coverage and visual evidence are not one combined regression total. Current status/roadmap/testing documents are maintained by the integration owner.

## Owned computation and presentation

The C planner ranks the captured, loaded Pivot groups and aggregates the exact union of their non-top Row-key tuples from the captured source relation, after source/Pivot filters and grains. It does not fold already-aggregated averages or sum distinct counts. With a SUM measure, its rank is shared; without SUM, numeric Plot measures can select independent bounded rank variants. Every variant contains all category/measure values, so Plot and Marks can read different assigned measures from the same owned result.

`PageOther` owns independent typed one-row Pages; source pointers are not retained. Its parent fingerprints bind to the owned cells, columns, executed SQL and loaded extent. A complete parent Page can move while retaining the graph; a borrowed shortened/reshaped prefix cannot reuse it. Successful adoption consumes ownership; failed adoption preserves both operands. Partial/free paths visit every variant slot, including a not-yet-counted conversion, and each Page retains its own cell-text release callback.

Native collection validates original result-vector key bytes before C-string conversion and observes supported file identity/stamp before/after the complete operation. Worker jobs own captured definitions; matching owner/revision/request publication transfers the complete Page once. Chart/Marks presentation borrows it and performs no query per paint or Plot change. Other is an ephemeral result cache; Save/Open restores definitions and recomputes it, rather than archiving data values.

The logical 4 MiB owned admission includes retained Page/totals, captured definition text, returned source/key/plan strings and readout variants at checked construction boundaries. It is not a process/RSS, parser/reallocator, transient DuckDB-result or engine-memory cap. Captured source/session routing and typed-key proof are separate from the planner's ownership contract; a C-string or scalar type enum alone does not establish source identity or complete key bytes.

## Retained component evidence

| Evidence | Tested source boundary | Accepted scope and time |
| --- | --- | --- |
| [Original retained planner](helper/manifest.json) | Private `b2e7ddb` base with exact file hashes | 396 checks/configuration; optimized/O1 builds1.269/2.538s, ordinary runs0.451/0.713s; no App activation |
| [Actual UI mapping](ui/manifest.json) | Private `7641724` plus tests later committed as `0807ddb`/integrated `07277b7`; exact inputs retained | chartview187 +encoding188 per optimized/O1;8.412s total including6.444s build; no engine/GPU/App |
| [Root native integration](integration840/summary.json) | Exact `84072b1`, source fingerprint `d44e399e…` | planner396/DB383/Worker310/UI187+188 per configuration; ten runs5.968s, build24.322s, total30.356s; no App window |
| [Native fatal-result/source follow-up](native-followup/manifest.json) | Private tested commit `717c64c`, merged as `361aaf8`; manifest confirms exact file hashes | DB474/configuration; optimized/O1 builds1.756/2.740s and runs0.628/1.194s,6.719s total including scoped shipping-object check |
| [Final native engine/Worker](native-final/summary.json) | Exact `b7abbb1`, fingerprint `683185e8…` | DB474 +Worker310 per optimized/O1 configuration; four ordinary runs2.569s, build25.804s, total28.403s; no App window |
| [Native App40](app-release/summary.json) | Exact optimized `0fd8c57`, fingerprint `b5f98351…`; executable unchanged across run | App22.071s, background1.195s, build6.392s, total29.832s; owned hidden native window, actual source/PIVOT/chart flow |
| [Native O1 App40](app-sanitize/summary.json) | Wrapper `24daeb0`; executable `80a16032…` was compiled from c3 C inputs, as recorded in the separate rebuild | App22.818s, background1.446s, incremental build1.317s, total25.741s; all40 stages |
| [Native review400](review/summary.json) | Exact `1082c1f`, fingerprint `e54f86e0…`; release/O1 binaries identified separately | App17.876s /23.089s; separate76.232s rebuild and1.580s background preflight;400 stages and three saved reports per configuration |

The mapping fixtures distinguish source AVG7.5 from displayed-row fold5 and source distinct3 from folded4; they retain large `DECIMAL(38,4)` text in Marks bindings despite double projection losing low digits. NULL, numeric zero and non-finite values stay distinct; stale extents/pointers/ranks reject source-derived cache reuse. Two no-SUM variants switch Plot without altering the graph, and independent parent/readout free callbacks are checked. These are actual C consumers with owned fixtures, not GPU pixels or a live source query.

The root Worker310 gate uses a real native engine and owned source-derived replies, checks 60 query-free Plot/Marks changes, latest-only supersession and retained readouts after Worker shutdown. Immediate double-submit may replace a pending request before a partial Other graph exists; it is not deterministic proof of cancellation after partial graph construction. The final native follow-up makes unexpected shape/type/conversion/adoption failures fatal while explicit unsupported/budget outcomes retain safe fallback. Its `DUXCEL_TEST_CHART_OTHER` fault seam is scoped to the focused test target; [symbol evidence](native-followup/shipping-object-nm.txt) covers the no-macro database object, not an unrecorded whole-App link check. macOS follow-up sanitization sets `detect_leaks=0`; no LSan claim is made.

## Actual native source and chart flow

The optimized40-stage [App log](app-release/native-chart.txt) covers a real same-sheet source, Pivot matrix and chart. Its seven-row fixture ranks A/B/C above D/E: the non-top source rows yield weighted AVG7.5 and distinct3, whereas folding the two displayed aggregates would produce5 and4. Root visually inspected the retained [average](captures/other-average.png) and [distinct](captures/other-distinct.png) captures; their [provenance](captures/provenance.json) binds them to this optimized run, before later runs could overwrite the original paths. These screenshots show the actual native state, not a frozen-reference parity comparison.

The same run includes Other-cache Undo/Redo for representation, Plot and Marks (stages35/37), query-free return to the matrix, and Save/Open recomputation from definitions. The O1 [App log](app-sanitize/native-chart.txt) independently passes the same40-stage chart scenario, including the earlier naming/dialog/history assertions. Its wrapper source is `24daeb0`; the unchanged executable hash binds it to the separately recorded c3 C-input build, rather than implying a new native compilation for a later JavaScript receipt change.

The separate native review400 run retains its existing renderer/controller, composer and ownership checks. Its [release](review/release-extract.json) and [O1](review/sanitize-extract.json) extracts preserve exact PASS/performance lines; the summary retains six saved report hashes without copying report archives. These two profiles are not the whole native App inventory or all visual states. Their477-check background preflights verify the owned hidden non-key window boundary. They do not use the user's clipboard/preferences or deliver physical OS file panels, input or IME.

## Deliberately unsupported NUL counterexample

The [diagnostic](cause/unproved-nul-diagnostic.json), with [source/build provenance](cause/unproved-nul-manifest.json), intentionally bypasses the planner's complete-key prerequisite. Original key `a\0b` is **3 bytes** in the DuckDB vector, but the legacy accessor and converted C string expose **1 byte**. Reusing that truncated key produces **Other1 instead of42**. The process exits successfully because it asserts the known counterexample; this is not accepted unsafe-input behavior or a failed production run. The then-current producer was not enabled. The later native collector's key-byte proof must reject this unsafe path; it must never infer proof from `strlen` or the legacy accessor's already-truncated length.

## Real official Worker and retained baseline

The [earlier accepted Worker221 baseline](baseline-worker221.json) remains **225 jobs /441 submitted SQL /21 count submissions**, with its original192-stage workload intact. The [accepted new Worker run](worker/summary.json), exact source/runner `c3a4a99` and artifact signature `33d20558b2798d2d2eca7da2309d6cc1249ee199f712a6be32570b83034f0e5e`, passes125,639 C checks over247 stages. Its26-stage Other slice adds24 jobs/88 submitted SQL/0 counts: **249 total jobs /529 submitted SQL /21 count submissions across five sequential Workers**. These counters record submissions and may include requests superseded before engine execution; they do not count only executed SQL.

The run takes16.590s CLI /16.375s browser; the6.871s driver build is separate. The [original workload receipts](worker/worker-workloads.json) and [binding receipt](worker/binding-workloads.json) retain the221-stage counters unchanged. A bounded [Other console extraction](worker/other-console-extract.json) preserves exact messages and the original event-file hash without copying that stream. The [postvalidation](integration/chart-other-workload-postvalidation.json) strictly rechecks those already-retained messages; it is not another engine run. Root added a standalone Other-receipt writer afterward (`24daeb0`); that later writer did not produce the retained c3 run's receipt.

These real C/official DuckDB-Wasm tests cover exact loaded-key membership, typed domains, true source aggregates, no-SUM variants, captured CSV binding retention, local native-stamp rejection and source-drop rejection. Cancellation between two actual variants releases staged readouts and ignores the delayed variant; a normal flagged Pivot then recovers without aliasing the retained graph. This deterministic Worker cancellation coverage is separate from the native310 latest-only pending-request test. It is not an actual App/browser chart interaction or pixel gate.

The [native O1 rebuild](integration/chart-other-sanitize-build.json) takes39.031s at c3. The [linked native symbol exclusion](integration/chart-other-native-symbol-exclusion.json) finds no recorded test/review/browser symbols in the two hashed executables. Their exact hashes remain authoritative; this check itself is not a runtime result.

## Actual browser App742

The [real App profile](browser-app/summary.json) passes742 checks at DPR2 with a1280×820 logical surface. Runner `d097f5c` uses the unchanged review artifact `7d229ee094909d11deb0` built from product `1082c1f`; its full signature/source/flags and top-level file hashes are retained. Time is30.040s CLI /29.805s browser /27.504s driver. This is distinct from Worker247's backend-only artifact and run.

Normal shared-C authoring creates a same-sheet empty H1 Pivot beside the five-column CSV, then Rows Group, Columns Bucket, SUM Rank, AVG Value and COUNT DISTINCT Customer. The [bounded readout receipt](browser-app/readouts.json) retains the actual source/matrix/Other generations, typed values, sparse G10 sentinel, model/history state and idle counters. Bar SUM5, keyboard Plot AVG7.5, pointer Plot distinct3, and separately assigned AVG Tooltip7.5 read the same owned cache. Plot and Marks each Undo/Redo one ordinary action; representation changes, capture/cancel, idle, source return and manual-cell return submit no jobs after publication.

Root visually inspected the actual [AVG](browser-app/chart-other-average.png) and [distinct with assigned AVG Tooltip](browser-app/chart-other-distinct-tooltip.png) compositor captures. The Tooltip assertion observes its cached encoding readout; it does not claim the hover balloon was visible. There is no mocked analytical state, second JavaScript spreadsheet, clipboard activation, saved/uploaded browser report, browser persistence, physical input or full pixel-parity claim.

Three earlier runs failed because the new harness reused incompatible fixture assumptions. The [compact history](fixture-corrections/summary.json) retains their exact failed driver errors and source/artifact hashes:

- `064408-browser-74046`:272 checks /11.831s; six-column I1 expectation corrected to this fixture's five-column H1 anchor (`da3dc7a`).
- `064450-browser-74222`:457 checks /20.499s; category/grand footer expectation corrected from four to six measure columns for three Values (`bb0698e`).
- `064533-browser-74432`:511 checks /20.096s; sparse H10 correctly blocked the expanded Bar footprint H1:M12. The friendly collision preserved matrix/request/Other state; moving the sentinel to G10 made the intended interaction reachable (`d097f5c`).

These are genuine test failures and corrected harness setup; they are not natural analytical product failures or evidence that a guard was waived. All use the same product review artifact as the final passing run.

## Build isolation and quiet delivery

Normal/review Wasm initially build at `24daeb0` in14.824s /19.296s with a0.345s exclusion check, recorded in [build timing](integration/chart-other-web-build.json). The [normal artifact](integration/normal-wasm-build.json) remains `4bd26622b9ebbd322882`; passive bounded review-only metadata at `1082c1f` produces the separately [recorded review artifact](integration/review-wasm-build.json) in18.758s. No analytical engine change is inferred from that metadata addition.

The later [exclusion run](integration/chart-other-exclusion.json) at tool wrapper `da3dc7a` passes native release0.242s, O1 native0.306s and Wasm0.537s. Retained [release](integration/chart-other-exclusion-native.txt), [O1](integration/chart-other-exclusion-native-sanitize.txt) and [Wasm export/marker](integration/web-review-absence.json) evidence ties the checks to their hashed shipping/review binaries. Review and fault seams remain excluded from shipping; build/exclusion checks are separate from runtime behavior.

The quiet [signed package](delivery/chart-other-package.json) takes0.856s. Its native code revision is `0fd8c57`, and unsigned input SHA `e22c1cc3828cb1787a89f611bda443ff7b9529b1c4ab6776cedcdd7a1fb4c063` exactly matches optimized App40. The [package log](delivery/chart-other-package.txt) and receipt record signing verification, without launching the user's app. They do not relabel native code as the newer browser-tool source.

The owned [8767 service refresh](delivery/chart-other-service.json) takes0.199s, PID75024, serving the accepted1082 review artifact. The [queue receipt](delivery/chart-other-service-retention.json) records the same project/IDs and zero reports before/after. This proves retention, not new feedback delivery. No user browser reload or native launch occurs. These are historical delivery records at this checkpoint, not a claim about later independent BP1 work.

## Limits and retained evidence

Other is derived from the exact union of non-top **loaded** groups, not an unbounded global/unloaded-group calculation. Unsupported source/key/type/budget outcomes retain the honest unavailable result; unexpected conversion/result-shape failures fail the job. Source/session verification and future persistence policies retain their separate scope. Arbitrary SQL dependencies, universal source identity, full visual parity, physical OS interactions and whole-application regression are not completed here. Independent later BP1 changes are excluded.

No executable, generated database, report archive or full input/event stream is copied. [Provenance](provenance.json) records exact original paths/hashes and explains bounded derivations. [The hash manifest](hashes.json) seals every retained payload file; its own hash is verified separately at publication. The four retained native/browser Other PNGs support actual state review, not a new visual reference oracle.
