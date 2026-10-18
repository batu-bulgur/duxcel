/* Definition-only native comparison; stored source SQL is never executed. */
#define PM_BASE UINT64_C(9007199254740993)
#define PM_LIMIT (2u*1024u*1024u)
static uint64_t pm_id(unsigned version,bool large,size_t sheet,size_t object) {
    return version==14&&((large&&sheet==1&&object==16)||(!large&&object==3))?UINT64_MAX:PM_BASE+object;
}
static char *pm_query(size_t sheet,bool table) {return dx_printf("SELECT * FROM __BP1_PM_%s_MUST_NOT_EXECUTE /* 'İ' %zu */;  \n",table?"TABLE":"PRIMARY",sheet);}
static PivotDefinition pm_definition(void) {
    PivotDefinition p={.counts={2,1,2,4}};
    p.fields[PIVOT_ROWS][0]=(PivotField){.name=dx_strdup("Date 'İ'"),.grain=PIVOT_DATE_YEAR,.filter=FILTER_NOT_NULL};
    p.fields[PIVOT_ROWS][1]=(PivotField){.name=dx_strdup("Date 'İ'"),.grain=PIVOT_DATE_QUARTER,.filter=FILTER_NOT_NULL};
    p.fields[PIVOT_COLUMNS][0]=(PivotField){.name=dx_strdup("Column 'İ'"),.grain=PIVOT_DATE_MONTH,.filter=FILTER_NOT_NULL};
    p.fields[PIVOT_VALUES][0]=(PivotField){.name=dx_strdup("Amount 'İ'"),.aggregate=AGG_AVG,.filter=FILTER_NOT_NULL};
    p.fields[PIVOT_VALUES][1]=(PivotField){.name=dx_strdup("Amount 'İ'"),.aggregate=AGG_DISTINCT,.filter=FILTER_NOT_NULL};
    const char *values[]={NULL,"","NULL","[\"10\",\"20\"]"};
    for(size_t f=0;f<4;f++)p.fields[PIVOT_FILTERS][f]=(PivotField){.name=dx_printf("Filter 'İ' %zu",f),.value=values[f]?dx_strdup(values[f]):NULL,.filter=f==3?FILTER_BETWEEN:FILTER_CONTAINS};
    CHECK(!p.fields[PIVOT_FILTERS][0].value&&p.fields[PIVOT_FILTERS][1].value&&!*p.fields[PIVOT_FILTERS][1].value&&!strcmp(p.fields[PIVOT_FILTERS][2].value,"NULL"));
    return p;
}
static MarkDefinition pm_marks(void) {
    MarkDefinition m={.counts={2,1,2,2,2,1}};
    for(int role=0;role<DX_MARK_ROLES;role++)for(size_t f=0;f<m.counts[role];f++)m.fields[role][f]=(MarkField){
        .name=dx_printf("Mark 'İ' %d:%zu",role,f),.measure=f!=0,.aggregate=f?AGG_DISTINCT:AGG_AVG,.grain=f?PIVOT_DATE_EXACT:PIVOT_DATE_YEAR};
    return m;
}
static void pm_fixture(const char *path,const char *missing,unsigned version,bool large,bool empty) {
    Workbook b;workbook_init(&b);char *error=NULL;size_t sheets=large?2:1,objects=empty?0:large?17:4;
    for(size_t s=0;s<sheets;s++){
        Analysis a={.kind=SOURCE_CSV,.path=dx_strdup(missing),.schema=dx_strdup(""),.table=dx_strdup("NULL"),.query=pm_query(s,false),.unique_rows=s%2!=0};
        char *name=dx_printf("Pivot 'İ' %zu",s);CHECK(workbook_add(&b,name,&a,&error));free(name);analysis_free(&a);Worksheet *w=b.sheets+s;
        w->view=(SheetView){.row=DX_SHEET_ROWS-1,.col=16383,.anchor_row=1,.anchor_col=2,.end_row=3,.end_col=4,.top=5,.horizontal=120.25f,.fraction=.25,.selection=2,.canvas_zoom=1.5f};
        w->objects.count=objects;w->objects.items=objects?dx_alloc(objects*sizeof(*w->objects.items)):NULL;
        for(size_t j=0;j<objects;j++){
            bool table=j==objects-1,full=large||j!=0;ObjectKind kind=table?OBJECT_TABLE:large?(j%2?OBJECT_CHART:OBJECT_PIVOT):j==2?OBJECT_CHART:OBJECT_PIVOT;
            SheetObject *o=w->objects.items+j;*o=(SheetObject){.id=pm_id(version,large,s,j),.name=j?dx_printf("Pivot 'İ' %zu:%zu",s,j):dx_strdup("NULL"),.row=10+j*12,.rows=8,.col=8,.columns=4,.revision=1,.kind=kind,.mark=kind==OBJECT_CHART?MARK_LINE:MARK_BAR};
            if(table)o->source=(Analysis){.kind=SOURCE_CSV,.path=dx_strdup(missing),.schema=dx_strdup(""),.table=dx_strdup("NULL"),.query=pm_query(s,true),.unique_rows=(s+j+1)%2!=0};
            else {if(full){o->pivot=pm_definition();o->marks=pm_marks();o->chart_measure=1;}if((large&&j%2==0)||(!large&&j==1))o->source_id=pm_id(version,large,s,objects-1);}
        }
        w->objects.next_id=objects?pm_id(version,large,s,objects-1):0;
    }
    b.active=sheets-1;b.user_scale=1.25f;if(version==13)b.sheets[0].objects.next_id=PM_BASE+100;if(version==14)b.sheets[0].formats.source_aware=true;
    CHECK(workbook_save(&b,path,&error)&&!error);workbook_free(&b);
}
static const EmitFixture emit_pm_fixtures[]={
    {"pivot-marks-v12.dxls",12,25,false},{"pivot-marks-v13.dxls",13,25,false},
    {"pivot-marks-v14.dxls",14,25,false},{"pivot-marks-v14-reordered.dxls",14,25,true}
};
static const char pm_reorder_sql[]=
    "CREATE TABLE _duxcel_workspace.pm_fields AS SELECT grain,value,filter_op,aggregate,name,position,role,object_id,sheet_id FROM _duxcel_workspace.pivot_fields; DROP TABLE _duxcel_workspace.pivot_fields; ALTER TABLE _duxcel_workspace.pm_fields RENAME TO pivot_fields;"
    "CREATE TABLE _duxcel_workspace.pm_marks AS SELECT grain,aggregate,is_measure,name,position,role,object_id,sheet_id FROM _duxcel_workspace.marks; DROP TABLE _duxcel_workspace.marks; ALTER TABLE _duxcel_workspace.pm_marks RENAME TO marks";
static size_t pm_emit_index;
static void pm_emit_fixture(const char *path,const char *missing,unsigned version) {CHECK(pm_emit_index<4&&emit_pm_fixtures[pm_emit_index].version==version);pm_fixture(path,missing,version,pm_emit_index++==3,false);}
static int emit_pivot_marks(const char *directory) {
    char reorder[8192];int n=snprintf(reorder,sizeof(reorder),"%s;%s;%s;%s;%s",objects_reorder_sql,queries_reorder_sql,options_reorder_sql,tables_reorder_sql,pm_reorder_sql);CHECK(n>0&&(size_t)n<sizeof(reorder));
    pm_emit_index=0;return emit_fixture_files(directory,emit_pm_fixtures,4,pm_emit_fixture,reorder,NULL);
}
typedef struct {TableHeaderStamp tables;uint64_t digest;} PMStamp;
static PMStamp pm_stamp(WorkbookCodecHeader *h) {
    PMStamp stamp={tables_stamp(h),UINT64_C(1469598103934665603)};bool ready=workbook_codec_header_pivot_marks_ready(h);stamp.digest=object_hash_word(stamp.digest,ready);
    if(ready)for(size_t s=0;s<workbook_codec_header_sheet_count(h);s++)for(size_t j=0;j<workbook_codec_header_object_count(h,s);j++){
        WorkbookCodecObjectView o;CHECK(workbook_codec_header_object_at(h,s,j,&o));if(o.kind==OBJECT_TABLE)continue;const PivotDefinition *p=NULL;const MarkDefinition *m=NULL;
        CHECK(workbook_codec_header_pivot_definition(h,s,o.id,&p)&&workbook_codec_header_marks_definition(h,s,o.id,&m));
        for(int r=0;r<4;r++){stamp.digest=object_hash_word(stamp.digest,p->counts[r]);for(size_t i=0;i<p->counts[r];i++){const PivotField *f=&p->fields[r][i];stamp.digest=table_hash_text(stamp.digest,f->name);stamp.digest=table_hash_text(stamp.digest,f->value);stamp.digest=object_hash_word(stamp.digest,f->aggregate);stamp.digest=object_hash_word(stamp.digest,f->filter);stamp.digest=object_hash_word(stamp.digest,f->grain);}}
        for(int r=0;r<DX_MARK_ROLES;r++){stamp.digest=object_hash_word(stamp.digest,m->counts[r]);for(size_t i=0;i<m->counts[r];i++){const MarkField *f=&m->fields[r][i];stamp.digest=table_hash_text(stamp.digest,f->name);stamp.digest=object_hash_word(stamp.digest,f->aggregate);stamp.digest=object_hash_word(stamp.digest,f->grain);stamp.digest=object_hash_word(stamp.digest,f->measure);}}
    }
    return stamp;
}
static void pm_kept(WorkbookCodecHeader *h,PMStamp before) {kept_tables_header(h,before.tables);PMStamp now=pm_stamp(h);CHECK(now.digest==before.digest);}
static WorkbookCodecRead *pm_begin(duckdb_connection c,unsigned version,const char *target,size_t retained) {
    WorkbookCodecRead *r=NULL;char *error=NULL;CHECK(workbook_codec_read_begin(&r,key,version,WORKBOOK_CODEC_SHEETS_25,PM_LIMIT,resolve,(void *)target,&error));
    for(size_t i=0;i<2;i++)CHECK(result_feed(c,r,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);
    CHECK(workbook_codec_read_add_objects(r,true,&error)&&result_feed(c,r,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);
    CHECK(workbook_codec_read_add_queries(r,true,&error)&&result_feed(c,r,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);
    CHECK(workbook_codec_read_add_source_options(r,true,&error)&&result_feed(c,r,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);
    CHECK(workbook_codec_read_add_table_sources(r,(WorkbookCodecTableSourcePresence){true,true,true},&error));for(size_t i=0;i<3;i++)CHECK(result_feed(c,r,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);return r;
}
static void pm_fields_equal(const PivotDefinition *p,const PivotDefinition *q,const MarkDefinition *m,const MarkDefinition *n) {
    char *error=NULL;CHECK(pivot_valid(p,&error)&&marks_valid(m,&error)&&!error);
    for(int r=0;r<4;r++){CHECK(p->counts[r]==q->counts[r]);for(size_t i=0;i<p->counts[r];i++){const PivotField *a=p->fields[r]+i,*b=q->fields[r]+i;CHECK(text_same(a->name,b->name)&&text_same(a->value,b->value)&&a->aggregate==b->aggregate&&a->filter==b->filter&&a->grain==b->grain&&!a->number.kind&&!a->number.precision&&!a->number.scale);}}
    for(int r=0;r<DX_MARK_ROLES;r++){CHECK(m->counts[r]==n->counts[r]);for(size_t i=0;i<m->counts[r];i++){const MarkField *a=m->fields[r]+i,*b=n->fields[r]+i;CHECK(text_same(a->name,b->name)&&a->aggregate==b->aggregate&&a->grain==b->grain&&a->measure==b->measure);}}
}
static void pm_compare(const char *path,unsigned version,bool large,WorkbookCodecHeader **out) {
    FileImage image=file_image(path);Workbook native;workbook_init(&native);char *error=NULL;CHECK(workbook_load(&native,path,&error)&&!error);duckdb_database db=NULL;duckdb_connection c=NULL;open_readonly(path,&db,&c);
    size_t retained=workbook_codec_header_owned_bytes(*out);WorkbookCodecRead *r=pm_begin(c,version,native.path,retained);PMStamp old=pm_stamp(*out);
    CHECK(workbook_codec_read_add_pivot_marks(r,(WorkbookCodecPivotMarksPresence){true,true},&error));
    const WorkbookCodecFamily families[]={WORKBOOK_CODEC_PIVOT_FIELDS,WORKBOOK_CODEC_MARKS};const size_t columns[]={9,8},limits[]={65537,98305};
    for(size_t i=0;i<2;i++){const WorkbookCodecRequest *request=workbook_codec_read_request(r);CHECK(request&&request->family==families[i]&&request->column_count==columns[i]&&request->row_limit==limits[i]);CHECK(!workbook_codec_read_finish(r,out,&error));clear_error(&error);pm_kept(*out,old);CHECK(result_feed(c,r,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);}
    CHECK(pivot_result_rows[0]==(large?288u:18u)&&pivot_result_rows[1]==(large?320u:20u));if(large)CHECK(pivot_batches[0]==2&&pivot_batches[1]==2&&pivot_batches256[0]==1&&pivot_batches256[1]==1);
    CHECK(workbook_codec_read_finish(r,out,&error)&&!error);workbook_codec_read_free(r);duckdb_disconnect(&c);duckdb_close(&db);CHECK(workbook_codec_header_pivot_marks_ready(*out));
    CHECK(native.count==(large?2u:1u)&&native.active==native.count-1&&workbook_codec_header_sheet_count(*out)==native.count&&workbook_codec_header_version(*out)==version&&workbook_codec_header_active_sheet(*out)==native.active&&workbook_codec_header_user_scale(*out)==native.user_scale);
    for(size_t s=0;s<native.count;s++){
        const Worksheet *w=native.sheets+s;const SheetView *v=workbook_codec_header_view(*out,s),*n=&w->view;CHECK(v&&v->row==n->row&&v->col==n->col&&v->anchor_row==n->anchor_row&&v->anchor_col==n->anchor_col&&v->end_row==n->end_row&&v->end_col==n->end_col&&v->top==n->top&&v->horizontal==n->horizontal&&v->fraction==n->fraction&&v->selection==n->selection&&v->canvas_zoom==n->canvas_zoom);CHECK(!strcmp(workbook_codec_header_name(*out,s),w->name));table_analysis_compare(workbook_codec_header_analysis(*out,s),&w->analysis);CHECK(workbook_codec_header_object_count(*out,s)==w->objects.count);
        for(size_t j=0;j<w->objects.count;j++){
            const SheetObject *o=w->objects.items+j;WorkbookCodecObjectView v;CHECK(workbook_codec_header_object_at(*out,s,j,&v)&&v.id==o->id&&v.kind==o->kind&&v.mark==o->mark&&v.chart_measure==o->chart_measure&&v.source_id==o->source_id&&v.row==o->row&&v.height==o->rows&&v.column==o->col&&v.width==o->columns&&!strcmp(v.name,o->name));
            if(o->kind==OBJECT_TABLE){WorkbookCodecTableSourceView source;CHECK(workbook_codec_header_table_source(*out,s,o->id,&source)&&source.locator_available&&!strcmp(source.reference,"missing-source.csv"));table_analysis_compare(source.analysis,&o->source);const char *query=NULL;CHECK(workbook_codec_header_query(*out,s,o->id,&query)&&query==source.analysis->query);continue;}
            const PivotDefinition *p=NULL;const MarkDefinition *m=NULL;CHECK(workbook_codec_header_pivot_definition(*out,s,o->id,&p)&&workbook_codec_header_marks_definition(*out,s,o->id,&m));pm_fields_equal(p,&o->pivot,m,&o->marks);
        }
    }
    const PivotDefinition *p=(const PivotDefinition *)*out,*prior=p;CHECK(!workbook_codec_header_pivot_definition(*out,0,0,&p)&&p==prior);const MarkDefinition *m=(const MarkDefinition *)*out,*before=m;CHECK(!workbook_codec_header_marks_definition(*out,0,pm_id(version,large,0,large?16:3),&m)&&m==before);
    PMStamp final=pm_stamp(*out);workbook_free(&native);pm_kept(*out,final);FileImage after=file_image(path);CHECK(image.bytes==after.bytes&&!memcmp(image.data,after.data,image.bytes));free(image.data);free(after.data);
}
static void pm_rejected(const char *path,unsigned version,WorkbookCodecPivotMarksPresence presence,WorkbookCodecFamily failed,size_t first,WorkbookCodecHeader **out) {
    FileImage original=file_image(path);Workbook b;workbook_init(&b);Analysis blank={0};char *error=NULL;CHECK(workbook_add(&b,"Retained native",&blank,&error));Workbook before=b;
    CHECK(!workbook_load(&b,path,&error)&&error&&!memcmp(&b,&before,sizeof(b))&&!strcmp(b.sheets[0].name,"Retained native"));clear_error(&error);char *target=dx_absolute_path(path);CHECK(target);
    duckdb_database db=NULL;duckdb_connection c=NULL;open_readonly(path,&db,&c);size_t retained=workbook_codec_header_owned_bytes(*out);WorkbookCodecRead *r=pm_begin(c,version,target,retained);PMStamp old=pm_stamp(*out);size_t owned=workbook_codec_read_owned_bytes(r);
    if(!presence.fields||!presence.marks){CHECK(!workbook_codec_read_add_pivot_marks(r,presence,&error)&&error&&!workbook_codec_read_request(r)&&workbook_codec_read_owned_bytes(r)==owned);clear_error(&error);}
    else {
        CHECK(workbook_codec_read_add_pivot_marks(r,presence,&error));while(workbook_codec_read_request(r)->family!=failed)CHECK(result_feed(c,r,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);
        unsigned count=executions;CHECK(result_feed(c,r,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ERROR&&error);CHECK(executions==count+1&&pivot_last_family==failed&&pivot_last_first==first&&workbook_codec_read_owned_bytes(r)==pivot_last_owned&&workbook_codec_read_request(r)->first_row==first);clear_error(&error);
        CHECK(!workbook_codec_read_finish(r,out,&error));clear_error(&error);
    }
    pm_kept(*out,old);workbook_codec_read_free(r);duckdb_disconnect(&c);duckdb_close(&db);free(target);workbook_free(&b);pm_kept(*out,old);
    FileImage after=file_image(path);CHECK(original.bytes==after.bytes&&!memcmp(original.data,after.data,original.bytes));free(original.data);free(after.data);
}
static void pm_input_faults(const char *path,WorkbookCodecHeader **out) {
    FileImage image=file_image(path);char *target=dx_absolute_path(path);CHECK(target);duckdb_database db=NULL;duckdb_connection c=NULL;open_readonly(path,&db,&c);char *error=NULL;
    PMStamp old=pm_stamp(*out);size_t retained=workbook_codec_header_owned_bytes(*out);WorkbookCodecRead *r=pm_begin(c,14,target,retained);CHECK(workbook_codec_read_add_pivot_marks(r,(WorkbookCodecPivotMarksPresence){true,true},&error));
    const char *changes[]={"NULL::VARCHAR AS name","aggregate::VARCHAR AS aggregate","NULL::INTEGER AS grain"};
    for(size_t i=0;i<3;i++){
        const char *sql=workbook_codec_read_request(r)->sql;char *override=dx_printf("SELECT * REPLACE (%s) FROM (%s)",changes[i],sql);size_t owned=workbook_codec_read_owned_bytes(r);
        CHECK(result_feed(c,r,override,256,false,retained,&error)==WORKBOOK_CODEC_READ_ERROR&&error&&pivot_last_first==0&&workbook_codec_read_owned_bytes(r)==owned);clear_error(&error);free(override);pm_kept(*out,old);
    }
    size_t owned=workbook_codec_read_owned_bytes(r);CHECK(owned+retained+1<PM_LIMIT);size_t bytes=PM_LIMIT-owned-retained-1;unsigned char *pressure=dx_alloc(bytes);memset(pressure,0,bytes);
    CHECK(result_feed(c,r,NULL,256,true,retained+bytes,&error)==WORKBOOK_CODEC_READ_ERROR&&error&&strstr(error,"budget")&&pivot_last_first==0&&workbook_codec_read_owned_bytes(r)==owned);clear_error(&error);free(pressure);pm_kept(*out,old);
    CHECK(result_feed(c,r,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);owned=workbook_codec_read_owned_bytes(r);
    const char *sql=workbook_codec_read_request(r)->sql;char *override=dx_printf("SELECT * REPLACE (is_measure::INTEGER AS is_measure) FROM (%s)",sql);
    CHECK(result_feed(c,r,override,256,false,retained,&error)==WORKBOOK_CODEC_READ_ERROR&&error&&workbook_codec_read_owned_bytes(r)==owned);clear_error(&error);free(override);pm_kept(*out,old);
    CHECK(result_feed(c,r,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error&&workbook_codec_read_finish(r,out,&error));workbook_codec_read_free(r);duckdb_disconnect(&c);duckdb_close(&db);free(target);
    FileImage after=file_image(path);CHECK(image.bytes==after.bytes&&!memcmp(image.data,after.data,image.bytes));free(image.data);free(after.data);
}
static void pm_empty(const char *path,const char *missing,unsigned version,bool absent,WorkbookCodecHeader **out) {
    pm_fixture(path,missing,version,false,true);if(absent)mutate(path,"DROP TABLE _duxcel_workspace.objects; DROP TABLE _duxcel_workspace.pivot_fields; DROP TABLE _duxcel_workspace.marks; DROP TABLE _duxcel_workspace.table_sources; DROP TABLE _duxcel_workspace.table_filters; DROP TABLE _duxcel_workspace.table_sorts");
    Workbook b;workbook_init(&b);char *error=NULL;CHECK(workbook_load(&b,path,&error)&&!error&&!b.sheets[0].objects.count);FileImage image=file_image(path);duckdb_database db=NULL;duckdb_connection c=NULL;open_readonly(path,&db,&c);
    WorkbookCodecRead *r=NULL;size_t retained=workbook_codec_header_owned_bytes(*out);CHECK(workbook_codec_read_begin(&r,key,version,WORKBOOK_CODEC_SHEETS_25,PM_LIMIT,resolve,b.path,&error));for(size_t i=0;i<2;i++)CHECK(result_feed(c,r,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);
    CHECK(workbook_codec_read_add_objects(r,!absent,&error));if(!absent)CHECK(result_feed(c,r,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);
    CHECK(workbook_codec_read_add_queries(r,true,&error)&&result_feed(c,r,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);
    CHECK(workbook_codec_read_add_source_options(r,true,&error)&&result_feed(c,r,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);
    CHECK(workbook_codec_read_add_table_sources(r,(WorkbookCodecTableSourcePresence){!absent,!absent,!absent},&error));if(!absent)for(size_t i=0;i<3;i++)CHECK(result_feed(c,r,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);
    unsigned count=executions;CHECK(workbook_codec_read_add_pivot_marks(r,(WorkbookCodecPivotMarksPresence){!absent,!absent},&error));if(!absent)for(size_t i=0;i<2;i++)CHECK(result_feed(c,r,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);
    CHECK(executions==count+(absent?0u:2u)&&workbook_codec_read_finish(r,out,&error)&&workbook_codec_header_pivot_marks_ready(*out)&&!workbook_codec_header_object_count(*out,0));workbook_codec_read_free(r);duckdb_disconnect(&c);duckdb_close(&db);workbook_free(&b);
    FileImage after=file_image(path);CHECK(image.bytes==after.bytes&&!memcmp(image.data,after.data,image.bytes));free(image.data);free(after.data);
}
static int pivot_marks_only(void) {
    char temporary[]="/tmp/duxcel-codec-pivot-marks-XXXXXX";CHECK(mkdtemp(temporary));char *directory=dx_absolute_path(temporary);CHECK(directory);char *missing=dx_printf("%s/missing-source.csv",directory);CHECK(access(missing,F_OK)!=0);WorkbookCodecHeader *header=NULL;char *paths[4];
    for(size_t i=0;i<4;i++){const EmitFixture *entry=emit_pm_fixtures+i;paths[i]=dx_printf("%s/%s",directory,entry->name);pm_fixture(paths[i],missing,entry->version,i==3,false);if(entry->reordered){mutate(paths[i],reorder_sql);mutate(paths[i],objects_reorder_sql);mutate(paths[i],queries_reorder_sql);mutate(paths[i],options_reorder_sql);mutate(paths[i],tables_reorder_sql);mutate(paths[i],pm_reorder_sql);}pm_compare(paths[i],entry->version,i==3,&header);}
    pm_input_faults(paths[2],&header);FileImage small=file_image(paths[2]),large=file_image(paths[3]);char *path=dx_printf("%s/private.dxls",directory);
    for(size_t v=0;v<3;v++)for(size_t child=0;child<2;child++){FileImage image=file_image(paths[v]);options_copy_image(path,image);free(image.data);mutate(path,child?"DROP TABLE _duxcel_workspace.marks":"DROP TABLE _duxcel_workspace.pivot_fields");pm_rejected(path,(unsigned)v+12,(WorkbookCodecPivotMarksPresence){child!=0,child!=1},WORKBOOK_CODEC_PIVOT_FIELDS,0,&header);CHECK(!unlink(path));}
    const char *invalid[]={
        "UPDATE _duxcel_workspace.pivot_fields SET name=NULL WHERE role=0 AND position=0",
        "UPDATE _duxcel_workspace.pivot_fields SET value='a'||chr(0)||'b' WHERE role=3 AND position=0 AND object_id=9007199254740994",
        "UPDATE _duxcel_workspace.pivot_fields SET grain=1 WHERE role=2 AND position=0",
        "UPDATE _duxcel_workspace.pivot_fields SET object_id=18446744073709551615::UBIGINT WHERE object_id=9007199254740994",
        "UPDATE _duxcel_workspace.marks SET name=NULL WHERE role=0 AND position=0",
        "UPDATE _duxcel_workspace.marks SET object_id=18446744073709551615::UBIGINT WHERE object_id=9007199254740994"
    };
    for(size_t i=0;i<sizeof(invalid)/sizeof(*invalid);i++){options_copy_image(path,small);mutate(path,invalid[i]);pm_rejected(path,14,(WorkbookCodecPivotMarksPresence){true,true},i<4?WORKBOOK_CODEC_PIVOT_FIELDS:WORKBOOK_CODEC_MARKS,0,&header);CHECK(!unlink(path));}
    const char *late[]={
        "UPDATE _duxcel_workspace.pivot_fields SET position=4 WHERE sheet_id=1 AND object_id=9007199254741008 AND role=3 AND position=3",
        "UPDATE _duxcel_workspace.pivot_fields SET name='Date ''İ''',grain=1 WHERE sheet_id=1 AND object_id=9007199254741008 AND role=1",
        "UPDATE _duxcel_workspace.marks SET position=2 WHERE sheet_id=1 AND object_id=9007199254741008 AND role=0 AND position=1",
        "UPDATE _duxcel_workspace.marks SET name='Mark ''İ'' 0:0',is_measure=false,grain=1 WHERE sheet_id=1 AND object_id=9007199254741008 AND role=0 AND position=1",
        "UPDATE _duxcel_workspace.objects SET chart_measure=2 WHERE sheet_id=1 AND id=9007199254741008"
    };
    for(size_t i=0;i<sizeof(late)/sizeof(*late);i++){options_copy_image(path,large);mutate(path,late[i]);pm_rejected(path,14,(WorkbookCodecPivotMarksPresence){true,true},(i<2||i==4)?WORKBOOK_CODEC_PIVOT_FIELDS:WORKBOOK_CODEC_MARKS,256,&header);CHECK(!unlink(path));}
    pm_compare(paths[0],12,false,&header);free(small.data);free(large.data);for(unsigned v=12;v<=14;v++){pm_empty(path,missing,v,false,&header);CHECK(!unlink(path));}pm_empty(path,missing,12,true,&header);CHECK(!unlink(path));
    workbook_codec_header_free(header);free(path);for(size_t i=0;i<4;i++){CHECK(!unlink(paths[i]));free(paths[i]);}CHECK(nul_cells==1&&access(missing,F_OK)!=0);free(missing);CHECK(!rmdir(directory));free(directory);
    printf("PASS: native Pivot Marks %u checks; %u cursor metadata executions/%u typed feeds; exact complete definitions and 288 Pivot/320 Marks cross256 composition, NULL-empty-literal, native closed-handle parity, empty defaults and atomic error/budget/stale/recovery; source SQL never executed; native comparison only, no Worker/App Open claim; engine %s\n",checks,executions,feeds,duckdb_library_version());return 0;
}
#undef PM_BASE
#undef PM_LIMIT
