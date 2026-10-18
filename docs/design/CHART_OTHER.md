# Source-derived chart Other component

`src/transforms/chart_other.*` provides a bounded ranking and SQL-planning
component. It is not yet connected to App, Worker, Page ownership or rendering.
The existing chart's AVG/distinct limitation remains until that integration.

The three largest **loaded** Row tuples use the existing chart ranking: first
assigned SUM when present, otherwise the chosen Plot measure, finite scaled
scores across categories, absolute magnitude, and loaded-row order for ties.
The planner aggregates the exact union of the remaining loaded tuples from
`pivot_input_sql`, retaining source/Pivot filters and date grains. It never
subtracts global totals or folds unloaded groups into Other. Every assigned
category/measure result appears in the captured matrix's existing order.

This gives a weighted source AVG and a distinct union: group averages 0 and 10
over one and three rows produce 7.5, while distinct sets `{x}` and `{x,y,z}`
produce 3. SUM/COUNT/MIN/MAX use the same source relation. SQL NULL, empty text
and literal `NULL` remain separate Row and Column keys. Escaped key literals
use `IS NOT DISTINCT FROM` and `cast_to_type` against the projected field or
date-grain expression; type-name text is never interpolated as SQL.

Ranking allocates nothing on success. Planning supports one rank variant, up
to 256 loaded rows, 128 captured result columns and eight fields per shelf. The
owned SQL is bounded to 4 MiB including its terminator; temporary existing SQL
helpers and DuckDB memory are separate costs. Success replaces an owned plan;
failure preserves it. Three or fewer loaded groups need no Other query.

The caller must supply the matching captured source definition, Pivot Page
and original Column-key SQL types in shelf order. Complex key types and
non-temporal date-grain sources are rejected. No describe query is performed.
Request/source/version freshness, cancellation and reply publication remain
caller responsibilities; this component does not certify a Page's provenance.

Future integration should keep the result in its owning Pivot job/Page cache,
with the same latest-request and source fences as the matrix. To retain a
query-free Plot switch, a SUM ranking can share one variant; without SUM, at
most eight assigned-measure variants are needed. An optional Other failure
must preserve the valid matrix/totals and report unavailable state. No query
should begin from painting or an idle retry. Derived results need no new file
format field; aggregate, shelf order and Plot choice are already persisted.

The focused real-DuckDB tests are `tests/test_chart_other.c`; they compare
ranking with the existing chart helper and check weighted AVG, overlapping
distinct values, missing/empty/literal keys, date grains, exact DECIMAL text,
filtered sources, nonzero loaded offsets, ownership and bounded failures.
This is component evidence, not integrated analytical or UI acceptance.
