#!/usr/bin/env python3
"""Generate finite workbook/source acceptance inputs using the existing DuckDB CLI.

Python owns synthetic data and fixture files. Finite SQL authors the current
v15 core and v16 container; it is not a general application serializer. Native
product assertions stay in tests/test_fixture.c and exercise the real loader,
PIVOT and save/reload path.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

from workspace import FileLock

ROOT = Path(__file__).resolve().parents[1]
TEMPLATE = ROOT / "tests/fixtures/workbook/transactions.sql"
NAMES = ("Date", "Transaction ID", "Counterparty", "Category", "Account", "Amount (USD)",
         "Status", "Currency", "Month", "Region", "Reference", "Memo")
FIRST = (
    ("2026-09-14", "TX-0914-1042", "Northstar Labs", "Customer receipt", 12845000),
    ("2026-09-14", "TX-0914-1043", "Atlas Supply Co.", "Inventory", -2486000),
    ("2026-09-14", "TX-0914-1044", "Harbor Cloud", "Software", -485000),
    ("2026-09-15", "TX-0915-1045", "Payroll batch", "Payroll", -9642000),
    ("2026-09-15", "TX-0915-1046", "Meridian Retail", "Customer receipt", 7420000),
    ("2026-09-15", "TX-0915-1047", "City Properties", "Rent", -1250000),
    ("2026-09-16", "TX-0916-1048", "Acme Components", "Inventory", -1834000),
    ("2026-09-16", "TX-0916-1049", "Juniper Studio", "Customer receipt", 3280000),
    ("2026-09-16", "TX-0916-1050", "Metro Utilities", "Utilities", -218462),
    ("2026-09-17", "TX-0917-1051", "Summit Logistics", "Freight", -764000),
    ("2026-09-17", "TX-0917-1052", "Bluepeak Media", "Software", -312000),
    ("2026-09-18", "TX-0918-1053", "Pinecrest Capital", "Customer receipt", 4790000),
)
PARTNERS = ("Northstar Labs", "Atlas Supply Co.", "Harbor Cloud", "Meridian Retail", "City Properties",
            "Acme Components", "Juniper Studio", "Metro Utilities", "Summit Logistics", "Bluepeak Media",
            "Pinecrest Capital", "Redwood Systems", "Lakeview Solutions", "Clearwater Inc.",
            "Crescent Manufacturing", "Oakridge Ventures", "Vertex Technology", "Maple Office Supply",
            "Ridgeway Telecom", "Westlake Industries")
CATEGORIES = ("Customer receipt", "Inventory", "Software", "Payroll", "Rent", "Utilities", "Freight",
              "Equipment", "Marketing", "Interest")
MEMOS = ("Quarterly settlement", "Bank transfer", "Recurring payment", "Invoice cleared", "Batch settlement")


def quote(value):
    return "'" + str(value).replace("'", "''") + "'"


def transaction(index, force_usd=False):
    if index < len(FIRST):
        date, identifier, partner, category, cents = FIRST[index]
    else:
        month = 7 + index * 7 % 3
        date, identifier = f"2026-{month:02}-{1 + index * 11 % 28:02}", f"TX-26{month:02}-{index + 1:06}"
        partner, category = PARTNERS[index % 20], CATEGORIES[index * 7 % 10]
        positive = category in ("Customer receipt", "Interest")
        cents = (1 if positive else -1) * (25000 + index * 7919 % (12500000 if positive else 2400000))
    amount = f"{'-' if cents < 0 else ''}{abs(cents) // 100}.{abs(cents) % 100:02}"
    return (date, identifier, partner, category,
            "Operating" if index < 12 or index % 7 < 5 else "Reserve" if index % 7 == 5 else "Payroll",
            amount, "Posted" if index < 38450 else "Pending" if index % 2 else "Reversed",
            "USD" if force_usd or index < 36984 else "EUR" if index % 3 else "GBP", date[:7],
            ("North America", "Europe", "Asia Pacific")[index % 3],
            f"INV-{100000 + index:06}" if index % 23 else None,
            "" if index % 19 == 0 else None if index % 31 == 0 else "NULL" if index % 47 == 0 else MEMOS[index % 5])


def write_csv(path, large):
    def text(value):
        return "" if value is None else '"' + value.replace('"', '""') + '"'
    with path.open("w", encoding="utf-8", newline="") as output:
        output.write(",".join(map(text, NAMES)) + "\n")
        for row in range(48216 if large else 310):
            index = row if large or row < 240 else 48176 + row - 240 if row < 280 else 37000 + row - 280
            fields = transaction(index, not large and 240 <= row < 280)
            output.write(",".join(value if column == 5 else text(value) for column, value in enumerate(fields)) + "\n")


def execute(executable, database, sql):
    result = subprocess.run([str(executable), str(database), "-init", os.devnull, "-batch", "-bail", "-json"],
                            input=sql, text=True, capture_output=True, timeout=120)
    if result.returncode:
        raise RuntimeError("DuckDB fixture generation failed: " + result.stderr.strip())
    return json.loads(result.stdout) if result.stdout.strip() else None


def create_journal_paste(output, *, duckdb=None):
    """Finite TSV input; C owns paste, checkpoint and recovery assertions."""
    executable = duckdb or shutil.which("duckdb")
    if not executable:
        raise RuntimeError("The existing DuckDB CLI is required to generate fixtures")
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".journal-paste-", dir=output) as temporary:
        stage = Path(temporary) / "journal-paste.tsv"
        sql = (ROOT / "tests/fixtures/workbook/journal-paste.sql").read_text()
        execute(executable, ":memory:", sql.replace("@OUTPUT_TSV@", quote(stage)))
        target = output / stage.name
        os.replace(stage, target)
    return target


def create_source_connections(output, *, duckdb=None):
    """Finite resolved relations; native C owns connection/history acceptance."""
    executable = duckdb or shutil.which("duckdb")
    if not executable:
        raise RuntimeError("The existing DuckDB CLI is required to generate fixtures")
    output.mkdir(parents=True, exist_ok=True)
    sql = (ROOT / "tests/fixtures/workbook/source-connections.sql").read_text()
    targets = []
    with tempfile.TemporaryDirectory(prefix=".source-connections-", dir=output) as temporary:
        stages = []
        for label, scale in (("a", 1), ("b", 10)):
            stage = Path(temporary) / ("source-connection-" + label + ".duckdb")
            execute(executable, stage, sql.replace("@SCALE@", str(scale)))
            stages.append(stage)
        for stage in stages:
            target = output / stage.name
            os.replace(stage, target)
            targets.append(target)
    return targets


def create_source_opens(output, *, duckdb=None):
    """Finite file probes, including genuine malformed gzip/Parquet failures."""
    executable = duckdb or shutil.which("duckdb")
    if not executable:
        raise RuntimeError("The existing DuckDB CLI is required to generate fixtures")
    output.mkdir(parents=True, exist_ok=True)
    targets = []
    with tempfile.TemporaryDirectory(prefix=".source-opens-", dir=output) as temporary:
        stage = Path(temporary)
        database = create_source_connections(stage, duckdb=executable)[0]
        csv, parquet = stage / "source-open.csv", stage / "source-open.parquet"
        sql = (ROOT / "tests/fixtures/workbook/source-opens.sql").read_text()
        execute(executable, database, sql.replace("@CSV@", quote(csv)).replace("@PARQUET@", quote(parquet)))
        bad_csv, bad_parquet = stage / "source-open-bad.csv.gz", stage / "source-open-bad.parquet"
        bad_csv.write_bytes(b"not a gzip stream\n")
        bad_parquet.write_bytes(b"not a parquet file\n")
        for candidate in (csv, parquet, bad_csv, bad_parquet):
            target = output / candidate.name
            os.replace(candidate, target)
            targets.append(target)
    return targets


def create_source_origins(output, *, duckdb=None):
    """Loaded NULL defaults, typed count and unrelated ready-primary fixture."""
    executable = duckdb or shutil.which("duckdb")
    if not executable:
        raise RuntimeError("The existing DuckDB CLI is required to generate fixtures")
    connections = create_source_connections(output, duckdb=executable)
    with tempfile.TemporaryDirectory(prefix=".source-origins-", dir=output) as temporary:
        stage = Path(temporary)
        core, book = stage / "core.dxls", stage / "source-origins.dxls"
        primary = ROOT / "tests/fixtures/duplicates.csv"
        schema = TEMPLATE.read_text().replace("'__TRANSACTIONS_SOURCE__'", quote(primary))
        schema = schema.replace("__DOCUMENT_LOW__", "33")
        data = (ROOT / "tests/fixtures/workbook/source-origins.sql").read_text()
        data = data.replace("@PRIMARY@", quote(primary)).replace("@DATABASE@", quote(connections[0]))
        execute(executable, core, schema + "\n" + data)
        payload = core.read_bytes()
        shutil.copyfile(core, book)
        container = (ROOT / "tests/fixtures/workbook/current-container.sql").read_text()
        container = container.replace("__ROOT_SHA256__", hashlib.sha256(payload).hexdigest())
        container = container.replace("__ROOT_BYTES__", str(len(payload)))
        container = container.replace("__ROOT_CHUNKS__", str((len(payload) + 65535) // 65536))
        execute(executable, book, container.replace("'__CORE_PATH__'", quote(core)))
        target = output / book.name
        os.replace(book, target)
    return target


def create(output, *, large=False, duckdb=None):
    executable = duckdb or shutil.which("duckdb")
    if not executable:
        raise RuntimeError("The existing DuckDB CLI is required to generate fixtures")
    prefix = "design-large" if large else "design-definition"
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".transactions-", dir=output) as temporary:
        stage = Path(temporary)
        csv, parquet, book = (stage / (prefix + suffix) for suffix in (".csv", ".parquet", ".dxls"))
        write_csv(csv, large)
        execute(executable, ":memory:", f'COPY (SELECT * REPLACE ("Amount (USD)"::DECIMAL(18,2) AS "Amount (USD)") '
                f'FROM read_csv({quote(csv)},header=true,allow_quoted_nulls=false)) TO {quote(parquet)} (FORMAT PARQUET);')
        stats = execute(executable, ":memory:", f'''SELECT count(*) AS rows,
            count(*) FILTER (WHERE "Memo" IS NULL) AS nulls,
            count(*) FILTER (WHERE "Memo"='') AS empty_text,
            count(*) FILTER (WHERE "Memo"='NULL') AS literal_null,
            bool_and(typeof("Amount (USD)")='DECIMAL(18,2)') AS exact_decimal
            FROM read_parquet({quote(parquet)});''')[0]
        if stats["rows"] != (48216 if large else 310) or not all(stats[key] for key in ("nulls", "empty_text", "literal_null", "exact_decimal")):
            raise RuntimeError("Transaction fixture lost rows, typed NULL/text or exact DECIMAL")
        core = stage / "current-core.dxls"
        sql = TEMPLATE.read_text().replace("'__TRANSACTIONS_SOURCE__'", quote((output / parquet.name).resolve()))
        execute(executable, core, sql.replace("__DOCUMENT_LOW__", "2" if large else "1"))
        payload = core.read_bytes()
        digest = hashlib.sha256(payload).hexdigest()
        shutil.copyfile(core, book)
        sql = (ROOT / "tests/fixtures/workbook/current-container.sql").read_text()
        sql = sql.replace("__ROOT_SHA256__", digest).replace("__ROOT_BYTES__", str(len(payload)))
        sql = sql.replace("__ROOT_CHUNKS__", str((len(payload) + 65535) // 65536))
        execute(executable, book, sql.replace("'__CORE_PATH__'", quote(core)))
        # Nothing reaches the accepted paths until every candidate is complete.
        for candidate in (csv, parquet, book):
            candidate.replace(output / candidate.name)
    return stats


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--large", action="store_true")
    mode.add_argument("--journal-paste-only", action="store_true")
    mode.add_argument("--source-connections-only", action="store_true")
    mode.add_argument("--source-origins-only", action="store_true")
    mode.add_argument("--source-opens-only", action="store_true")
    parser.add_argument("--output-dir", type=Path, default=ROOT / "build/tests/generated")
    options = parser.parse_args()
    with FileLock(ROOT / "build/locks/fixture-inputs.lock", "Fixture generation"):
        if options.source_opens_only:
            targets = create_source_opens(options.output_dir.resolve())
            print(f"PASS: source open fixtures: {len(targets)} finite files")
            return
        if options.source_origins_only:
            target = create_source_origins(options.output_dir.resolve())
            print(f"PASS: source origin fixture: {target}")
            return
        if options.source_connections_only:
            targets = create_source_connections(options.output_dir.resolve())
            print(f"PASS: source connection fixtures: {targets[0]}, {targets[1]}")
            return
        if options.journal_paste_only:
            target = create_journal_paste(options.output_dir.resolve())
            print(f"PASS: journal paste fixture: {target}")
            return
        stats = create(options.output_dir.resolve(), large=options.large)
    print(f"PASS: Python transaction fixture, {stats['rows']} rows; NULL/empty/literal NULL and DECIMAL preserved")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        raise SystemExit(str(error)) from error
