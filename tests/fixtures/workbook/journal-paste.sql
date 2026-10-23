-- Finite maximum-size clipboard input for test_journal_performance.c.
-- All values are nonempty; quoted tabs/newlines exercise the real TSV parser.
COPY (SELECT CASE WHEN i=0 THEN '9007199254740993.0123456789'
    WHEN i=1 THEN 'NULL' WHEN i=2 THEN 'İstanbul' || chr(9) || 'line' || chr(10) || 'two'
    WHEN i=19999 THEN 'last-row' ELSE 'row-' || i::VARCHAR END AS value
    FROM range(20000) t(i) ORDER BY i)
TO @OUTPUT_TSV@ (FORMAT CSV, DELIMITER '\t', HEADER false);
