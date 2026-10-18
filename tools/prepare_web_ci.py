#!/usr/bin/env python3
"""Fetch only DuckDB's pinned C type header for engine-free browser CI."""
import hashlib
from pathlib import Path
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
URL = 'https://raw.githubusercontent.com/duckdb/duckdb/v1.4.3/src/include/duckdb.h'
SHA256 = 'bf840147dc70bb8238631023e95fa99c987a7744605a008bbb5b53a0446ae6ea'

if __name__ == '__main__':
    with urllib.request.urlopen(URL, timeout=60) as response:
        content = response.read(2 * 1024 * 1024)
    if hashlib.sha256(content).hexdigest() != SHA256:
        raise SystemExit('DuckDB C header checksum mismatch')
    destination = ROOT / 'build/ci/include/duckdb.h'
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(content)
    print('Verified DuckDB v1.4.3 C header; no native engine installed.')
