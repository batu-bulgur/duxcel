#!/usr/bin/env python3
"""Explicit public-data download; never invoked by make/test automatically."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
from urllib.request import urlopen

ROOT = Path(__file__).resolve().parents[1]
SOURCES = (
    ('yellow_tripdata_2025-01.parquet', 'https://d37ci6vzurychx.cloudfront.net/trip-data/yellow_tripdata_2025-01.parquet'),
    ('taxis.csv', 'https://raw.githubusercontent.com/mwaskom/seaborn-data/master/taxis.csv'),
)


def fetch(url, destination):
    """Reuse a nonempty file; publish a complete response through a sibling."""
    if destination.is_file() and destination.stat().st_size:
        return False
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with urlopen(url, timeout=60) as response, tempfile.NamedTemporaryFile(
                prefix='.' + destination.name + '-', dir=destination.parent, delete=False) as output:
            temporary = Path(output.name)
            shutil.copyfileobj(response, output, length=1024 * 1024)
            size = output.tell()
            expected = response.headers.get('Content-Length')
            if not size or (expected is not None and size != int(expected)):
                raise ValueError('Incomplete public-data response: ' + destination.name)
            output.flush()
            os.fsync(output.fileno())
        temporary.replace(destination)
        return True
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--roundtrip', action='store_true', help='Use the existing DuckDB CLI for optional CSV/Parquet conversion under build/tests/generated')
    args = parser.parse_args(argv)
    directory = ROOT / 'data/downloads'
    for name, url in SOURCES:
        path = directory / name
        print(('Downloaded ' if fetch(url, path) else 'Reused ') + str(path))
    if args.roundtrip:
        # Process orchestration only. Native acceptance/semantics use C tests.
        output = ROOT / 'build/tests/generated/public-data'; output.mkdir(parents=True, exist_ok=True)
        quote = lambda p: "'" + str(p).replace("'", "''") + "'"
        csv, parquet = output / 'yellow.csv', output / 'yellow_roundtrip.parquet'
        with tempfile.TemporaryDirectory(prefix='.roundtrip-', dir=output) as temporary:
            staged_csv, staged_parquet = Path(temporary) / csv.name, Path(temporary) / parquet.name
            sql = f'COPY (SELECT * FROM read_parquet({quote(directory / SOURCES[0][0])})) TO {quote(staged_csv)} (HEADER);'
            sql += f'COPY (SELECT * FROM read_csv_auto({quote(staged_csv)})) TO {quote(staged_parquet)} (FORMAT PARQUET, COMPRESSION ZSTD);'
            subprocess.run(['duckdb', '-c', sql], check=True)
            if any(not p.is_file() or not p.stat().st_size for p in (staged_csv, staged_parquet)):
                raise ValueError('DuckDB did not produce complete conversion outputs')
            staged_csv.replace(csv)
            staged_parquet.replace(parquet)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
