# Wasm object cache

Historical checkpoint. Machine-generated payloads were removed from the current checkout on 18 October 2026; they remain recoverable at Git ref `archive/pre-cleanup-20261018`. Paths and measurements below describe that original run. See the [retention policy](../../README.md).

The build helper reuses an object only after a fresh compiler dependency scan,
exact dependency hashes and resolved targets, valid object/sidecar hashes, and
the caller's source-freeze checks agree. App, review, Worker and codec objects
have separate mode namespaces and locks. This checkpoint covers build tooling;
it establishes no new application, UI, engine or persistence runtime behavior.

The helper is `e9c6e12` (root `ab3b225`), integrated into the browser builder at
`e7c75be`. Source scope (archived `source-scope.json`) records the three exact tool hashes.
The broad artifact signature and final source/input guards remain in place.
Review-header generation occurs under the review mode lock; the lock remains
held through object compilation and linking.

| Evidence | Result | Scope |
| --- | --- | --- |
| Helper tests (archived `helper/manifest.json`) | 18 groups, 8.974 s unittest / 9.073 s subprocess | Parser and existing local Clang tiny-C compilation/linking; no emcc performance inference |
| Actual emcc scans (archived `actual/scan-probe.json`) | App 0.497 s; codec reader 0.143 s; review capture 0.126 s | Three dependency scans, not complete build timings |
| Cold normal build (archived `actual/cold-app-build-extract.json`) | 24.679 s internal / 26.319 s command (archived `actual/cold-app-command.json`); all 58 objects rebuilt | Actual pinned emcc 6.0.12, source `e7c75be` |
| Cold review build (archived `actual/cold-review-build-extract.json`) | 27.130 s internal / 28.796 s command (archived `actual/cold-review-command.json`); all 62 objects rebuilt | Separate review namespace at `e7c75be` |
| Warm normal build (archived `actual/warm-app-build-extract.json`) | 16.456 s internal / 19.050 s command (archived `actual/warm-app-command.json`); 58 reused, zero rebuilt | Real format metadata change, source `689f0a7` |
| Warm review build (archived `actual/warm-review-build-extract.json`) | 13.899 s internal / 15.547 s command (archived `actual/warm-review-command.json`); 61 reused, one rebuilt | Generated review provenance correctly rebuilds `review_capture.c` |
| Warm exclusion (archived `actual/warm-exclusion.json`) | Passed for fresh normal `6df84fe6a4b627d1d145` / review `67165312f8d33d7a93d5` | Compile/link exclusion, no application launch |

Cold normal object work takes 21.911 s, including 1.513 s of broad caller input
validation. Cold review object work takes 24.126 s, including 1.530 s validation.
This additional scan/check work has a cold-build cost. Warm normal/review object
phases take 13.102/10.641 s, including 1.875/1.771 s validation. Warm normal ran
alongside a native O1 SQL test and is slightly slower than the unmatched earlier
15.243 s normal build; the earlier review build took 16.756 s. These are different
sources and loads, not a historical speed benchmark. Avoided physical compiles
are demonstrated; a universal latency improvement is not.

The real warm change adds format-row reader/header definitions, pure tests and
Makefile validator linkage. A Git-object comparison (archived `actual/translation-unit-comparison.json`)
checks all 58 normal and 62 review translation-unit C files byte-for-byte against
the cold revision. Those files are unchanged. Review provenance intentionally
hashes the broader source/revision and therefore changes its generated header.
The warm normal Wasm SHA is identical to the cold normal SHA; review Wasm changes
with its provenance. The original integration summary (archived `actual/manifest.json`)
keeps both cache configurations and timings.

The initial helper run passes 15 of 16 groups, then fails closed when Clang
normalizes a literal POSIX backslash filename into a nonexistent slash path.
[Original test sources and log](helper/first-failure/) and the
raw depfile diagnostic (archived `helper/backslash-dep-diagnostic.json`) retain that
failure. The corrected suite requires rejection of this ambiguous spelling;
synthetic valid Make escaping remains tested separately. No filesystem mapping
is guessed. The current helper also checks same-timestamp edits, transitive and
shared headers, include shadowing, symlink targets, generated headers, corrupt
sidecars/objects, compile/source-freeze failures and interrupted publication.

Repeated broad validation remains intentional. Checking only the current C
source and generated review header would miss a dependency that temporarily
changes before its scan/compile and returns to its initial bytes before final
artifact validation. An optional callback using the helper's already-hashed
dependency snapshot is deferred; the measured callback cost does not justify a
new seam in this slice. The helper still performs `-MM` on every object lookup.

`actual/*-extract.json` is a bounded selection from the original build JSON,
with its original path, byte count and SHA256. Vendor inventories, binaries and
generated artifacts are omitted. The helper directory preserves its original
small manifest and seal. `SHA256SUMS` seals this capsule's payloads. No binaries,
vendor files, SDK files, application windows, service/queue changes or package
promotion are included. Native symbol observations in the exclusion receipt
refer to the existing native binaries, not a fresh native build or execution.
