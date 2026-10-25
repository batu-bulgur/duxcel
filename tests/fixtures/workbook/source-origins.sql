-- Finite current-core metadata replacement after transactions.sql creates the
-- exact current schema. Consumer: tests/support/table_origin_app.inc.
BEGIN TRANSACTION;
DELETE FROM _duxcel_workspace.cells;
DELETE FROM _duxcel_workspace."columns";
DELETE FROM _duxcel_workspace.filters;
DELETE FROM _duxcel_workspace.formats;
DELETE FROM _duxcel_workspace.marks;
DELETE FROM _duxcel_workspace.objects;
DELETE FROM _duxcel_workspace.pivot_fields;
DELETE FROM _duxcel_workspace.queries;
DELETE FROM _duxcel_workspace.sorts;
DELETE FROM _duxcel_workspace.source_options;
DELETE FROM _duxcel_workspace.table_filters;
DELETE FROM _duxcel_workspace.table_sorts;
DELETE FROM _duxcel_workspace.table_sources;
DELETE FROM _duxcel_workspace.sheet_identity;
DELETE FROM _duxcel_workspace.object_counters;
DELETE FROM _duxcel_workspace.format_scopes;
DELETE FROM _duxcel_workspace.format_selectors;
DELETE FROM _duxcel_workspace.format_guards;
DELETE FROM _duxcel_workspace.format_state;
DELETE FROM _duxcel_workspace.source_version_counters;
DELETE FROM _duxcel_workspace.source_versions;
DELETE FROM _duxcel_workspace.source_bindings;
DELETE FROM _duxcel_workspace.format_records;
DELETE FROM _duxcel_workspace.format_fields;
DELETE FROM _duxcel_workspace.sheets WHERE id<>0;
UPDATE _duxcel_workspace.sheets SET name='Loaded origins',kind=1,path=@PRIMARY@,
    source_schema=NULL,source_table=NULL,filter_column=NULL,filter_value=NULL,filter_op=0,
    group_column=NULL,measure_column=NULL,"aggregate"=0,sort_column=NULL,descending=false,
    active_row=0,active_col=0,anchor_row=0,anchor_col=0,end_row=0,end_col=0,top_row=0,
    horizontal=0,fraction=0,selection=0,canvas_zoom=1;
INSERT INTO _duxcel_workspace.sheet_identity VALUES(0,1);
INSERT INTO _duxcel_workspace.object_counters VALUES(0,3);
INSERT INTO _duxcel_workspace.format_state VALUES(0,true);
INSERT INTO _duxcel_workspace.source_version_counters VALUES(0,0);
INSERT INTO _duxcel_workspace.source_bindings VALUES(0,0,0),(0,1,0),(0,2,0);
INSERT INTO _duxcel_workspace.objects VALUES
    (0,1,'Pending bound source',10000,4,5,4,2,0,0,0),
    (0,2,'Pending counted source',10000,16,5,4,2,0,0,0),
    (0,3,'Bound Pivot',10000,28,5,4,0,0,1,0);
INSERT INTO _duxcel_workspace.table_sources VALUES
    (0,1,3,@DATABASE@,'main',NULL,'Amount','10',2,NULL,NULL,0,NULL,false),
    (0,2,3,@DATABASE@,NULL,NULL,'Amount','10',2,NULL,NULL,0,NULL,false);
INSERT INTO _duxcel_workspace.queries VALUES(0,1,
    'SELECT ''Group ''||range::VARCHAR AS City, (range+1)::DECIMAL(12,2) AS Amount FROM range(24)');
INSERT INTO _duxcel_workspace.queries VALUES(0,2,
    'SELECT CASE WHEN range%2=0 THEN ''Ankara'' ELSE ''Bursa'' END AS City, (range%100)::DECIMAL(12,2) AS Amount FROM range(1024)');
INSERT INTO _duxcel_workspace.pivot_fields VALUES
    (0,3,0,0,'City',0,5,NULL,0),(0,3,2,0,'Amount',0,5,NULL,0);
-- Viewport fits; final count collides until the native manual clear command.
INSERT INTO _duxcel_workspace.cells VALUES(0,10800,16,'Count growth guard');
COMMIT;
