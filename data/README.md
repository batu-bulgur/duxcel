# Downloaded reference data

`downloads/` holds optional public reference datasets such as NYC Yellow Cab.
It is ignored and is never deleted by `make clean` or `make distclean`.
It is input data, separate from authored tiny fixtures and generated test output.

Download explicitly with `python3 tools/download_test_data.py`.
`--roundtrip` additionally uses the existing DuckDB CLI to create disposable
CSV/Parquet derivatives in `build/tests/generated/public-data/`.
No build or test target downloads data automatically. User workbooks may live
anywhere selected by the user; this directory is not the application's storage.
