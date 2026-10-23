# Transaction workbook fixture

`transactions.sql` is a finite v12 workbook input: four named sheets, a
same-sheet PIVOT at M1, its Rows/Columns/Values/Marks, value filters, widths and
seven numeric format ranges. It is not a reusable serializer or a historical
execution report. Its only substitution is the quoted relative Parquet filename
`__TRANSACTIONS_SOURCE__`.

Consumer: `tools/create_workbook.py` creates canonical (310-row) and large
(48,216-row) CSV/Parquet/workbook files under `build/tests/generated/` using the
already installed DuckDB CLI. Native scenarios consume those basenames.
`tests/test_fixture.c` validates the product loader, real PIVOT and save/reload
path against both inputs in optimized and ASan/UBSan configurations. Python tool
tests validate generation, missing CLI and failure-before-publication.

Change the finite SQL only when intentionally changing this acceptance input.
General `.dxls` persistence remains owned by the C workbook implementation.

`identity-legacy.sql` is a finite v12,24-column manual workbook containing one
sheet and a B2 text value. `tools/create_workbook.py --identity-only` generates
`build/tests/generated/identity-legacy.dxls`. Its named consumer is the private
workbook-identity App fixture (`tests/support/workbook_identity_app.inc`), which
uses the real asynchronous native Open/Save pipeline to migrate and reopen it.
