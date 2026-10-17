#!/bin/sh
# Run from the repository root. Downloads stay ignored by source control.
set -eu
mkdir -p tests/downloads
fetch() {
    if [ ! -s "$2" ]; then
        curl -fL --retry 2 "$1" -o "$2.part"
        mv "$2.part" "$2"
    fi
}
fetch 'https://d37ci6vzurychx.cloudfront.net/trip-data/yellow_tripdata_2025-01.parquet' 'tests/downloads/yellow_tripdata_2025-01.parquet'
fetch 'https://raw.githubusercontent.com/mwaskom/seaborn-data/master/taxis.csv' 'tests/downloads/taxis.csv'
if [ ! -s tests/downloads/yellow_tripdata_2025-01.csv ]; then
    duckdb -c "COPY (SELECT * FROM read_parquet('tests/downloads/yellow_tripdata_2025-01.parquet')) TO 'tests/downloads/yellow_tripdata_2025-01.csv' (HEADER, DELIMITER ',');"
fi
if [ ! -s tests/downloads/yellow_roundtrip.parquet ]; then
    duckdb -c "COPY (SELECT * FROM read_csv_auto('tests/downloads/yellow_tripdata_2025-01.csv')) TO 'tests/downloads/yellow_roundtrip.parquet' (FORMAT PARQUET, COMPRESSION ZSTD);"
fi
printf '%s\n' 'Public fixtures ready in tests/downloads (approximately 460 MiB).'
