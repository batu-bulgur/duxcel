-- Finite CSV/Parquet conversion for tools/create_workbook.py --source-opens-only.
-- Consumer: tests/support/primary_source_kinds_app.inc, actual bounded Worker IO.
COPY (SELECT * FROM alpha ORDER BY City,Amount) TO @CSV@ (FORMAT CSV, HEADER TRUE);
COPY (SELECT * FROM alpha ORDER BY City,Amount) TO @PARQUET@ (FORMAT PARQUET);
