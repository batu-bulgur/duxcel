-- Finite codec-v1 container for the complete transactions v15 core.
-- Consumer: tools/create_workbook.py; native closure: tests/test_fixture.c.
BEGIN TRANSACTION;
UPDATE _duxcel_workspace.meta SET version=16;
CREATE SCHEMA _duxcel_versions;
CREATE TABLE _duxcel_versions.meta(codec INTEGER,document_high UBIGINT,document_low UBIGINT,generation UBIGINT,saved_generation UBIGINT,saved_present BOOLEAN,saved_key BLOB);
CREATE TABLE _duxcel_versions.roots(position INTEGER,key BLOB,bytes UBIGINT,encoding INTEGER,PRIMARY KEY(position),UNIQUE(key));
CREATE TABLE _duxcel_versions.tags(position INTEGER,label VARCHAR,root BLOB,created UBIGINT,PRIMARY KEY(position),UNIQUE(label));
CREATE TABLE _duxcel_versions.chunks(root BLOB,position UBIGINT,data BLOB,PRIMARY KEY(root,position));
INSERT INTO _duxcel_versions.meta SELECT 1,high,low,1,1,true,from_hex('__ROOT_SHA256__') FROM _duxcel_workspace.workbook_identity;
INSERT INTO _duxcel_versions.roots VALUES(0,from_hex('__ROOT_SHA256__'),__ROOT_BYTES__,15);
INSERT INTO _duxcel_versions.chunks
SELECT from_hex('__ROOT_SHA256__'),position,from_hex(substr(hex(content),position*131072+1,131072))
FROM read_blob('__CORE_PATH__'),range(__ROOT_CHUNKS__) AS positions(position);
COMMIT;
