-- Finite v12/24-column manual sheet for W05b native migration GUI acceptance.
-- Consumer: tools/create_workbook.py --identity-only; workbook identity App fixture.
CREATE SCHEMA _duxcel_workspace;
CREATE TABLE _duxcel_workspace.meta(version INTEGER,active_sheet UBIGINT,user_scale DOUBLE);
INSERT INTO _duxcel_workspace.meta VALUES(12,0,1);
CREATE TABLE _duxcel_workspace.sheets(id INTEGER,name VARCHAR,kind INTEGER,path VARCHAR,source_schema VARCHAR,source_table VARCHAR,filter_column VARCHAR,filter_value VARCHAR,filter_op INTEGER,group_column VARCHAR,measure_column VARCHAR,aggregate INTEGER,sort_column VARCHAR,descending BOOLEAN,active_row UBIGINT,active_col UBIGINT,anchor_row UBIGINT,anchor_col UBIGINT,end_row UBIGINT,end_col UBIGINT,top_row UBIGINT,horizontal DOUBLE,fraction DOUBLE,selection INTEGER);
INSERT INTO _duxcel_workspace.sheets VALUES(0,'Legacy',0,'',NULL,NULL,NULL,NULL,0,NULL,NULL,0,NULL,false,1,1,1,1,1,1,0,0,0,0);
CREATE TABLE _duxcel_workspace.columns(sheet_id INTEGER,column_id INTEGER,name VARCHAR,width DOUBLE);
CREATE TABLE _duxcel_workspace.cells(sheet_id INTEGER,row_id UBIGINT,column_id INTEGER,text VARCHAR);
INSERT INTO _duxcel_workspace.cells VALUES(0,1,1,'Legacy original');
CREATE TABLE _duxcel_workspace.formats(sheet_id INTEGER,position INTEGER,r0 UBIGINT,r1 UBIGINT,c0 INTEGER,c1 INTEGER,mask INTEGER,flags INTEGER,alignment INTEGER,color INTEGER,font_size INTEGER,numeric_format INTEGER,decimals INTEGER);
CREATE TABLE _duxcel_workspace.queries(sheet_id INTEGER,object_id UBIGINT,query VARCHAR);
CREATE TABLE _duxcel_workspace.source_options(sheet_id INTEGER,object_id UBIGINT,unique_rows BOOLEAN);
