#include "workbook/primary_source_draft.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL primary source draft:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static Workbook book(void) {
    Workbook b; workbook_init(&b); Analysis source={.kind=SOURCE_CSV,.path="/current.csv"};
    CHECK(workbook_add(&b,"Database",&source,NULL)); CHECK(workbook_prepare_command_session(&b,0,NULL)); b.dirty=false; return b;
}
static size_t reservation(PrimarySourceDraftInfo info) { return info.owned+info.validation_scratch+info.naming_scratch; }
static PrimarySourceDraft *begin(PrimarySourceDraftContext c,const Analysis *source) {
    PrimarySourceDraftInfo info={0}; PrimarySourceDraft *draft=NULL;
    CHECK(primary_source_draft_measure(c,source,"  Database  ",&info,NULL)==PRIMARY_SOURCE_READY && info.owned && info.naming_scratch);
    Workbook before=*c.book; Worksheet sheet=c.book->sheets[0];
    CHECK(primary_source_draft_begin(c,source,"  Database  ",reservation(info)-1,&draft,NULL)==PRIMARY_SOURCE_LIMIT && !draft);
    CHECK(primary_source_draft_begin(c,source,"  Database  ",reservation(info),&draft,NULL)==PRIMARY_SOURCE_READY && draft);
    CHECK(!memcmp(&before,c.book,sizeof(before)) && !memcmp(&sheet,c.book->sheets,sizeof(sheet)) && !primary_source_draft_source(draft));
    CHECK(!strcmp(primary_source_draft_name(draft),"Database (2)") && primary_source_draft_owned(draft)<=info.owned);
    PrimarySourceCandidate pending={0}; CHECK(primary_source_draft_take(c,&draft,&pending,NULL)==PRIMARY_SOURCE_PROTECTED && draft && !pending.name);
    return draft;
}
static void basics(void) {
    Workbook b=book(); Analysis app=analysis_copy(&b.sheets[0].analysis); uint64_t request=7,epoch=9;
    PrimarySourceDraftContext c={&b,&app,&request,&epoch,true,false}; Analysis source={.kind=SOURCE_DATABASE,.path="/new.duckdb",.schema="chosen"};
    PrimarySourceDraft *draft=begin(c,&source); Workbook before=b; Worksheet sheet=b.sheets[0];
    Analysis reply=source; reply.schema="chosen"; reply.table="Sales"; PrimarySourceDraftReply actual={request,&reply,NULL}; PrimarySourceDraftInfo info={0};
    CHECK(primary_source_draft_reply_measure(c,draft,actual,&info,NULL)==PRIMARY_SOURCE_READY && info.owned && !info.naming_scratch);
    CHECK(primary_source_draft_resolve(c,draft,actual,reservation(info)-1,NULL)==PRIMARY_SOURCE_LIMIT && !primary_source_draft_source(draft));
    CHECK(primary_source_draft_resolve(c,draft,actual,reservation(info),NULL)==PRIMARY_SOURCE_RESOLVED);
    const Analysis *resolved=primary_source_draft_source(draft);
    CHECK(resolved && resolved->path!=source.path && resolved->schema!=reply.schema && resolved->table!=reply.table && !strcmp(resolved->table,"Sales"));
    CHECK(primary_source_draft_resolve(c,draft,actual,SIZE_MAX,NULL)==PRIMARY_SOURCE_PROTECTED);
    CHECK(!memcmp(&before,&b,sizeof(b)) && !memcmp(&sheet,b.sheets,sizeof(sheet)));
    PrimarySourceCandidate candidate={0}; CHECK(primary_source_draft_take(c,&draft,&candidate,NULL)==PRIMARY_SOURCE_TAKEN && !draft);
    CHECK(candidate.source.table && !strcmp(candidate.name,"Database (2)") && !memcmp(&before,&b,sizeof(b)));
    primary_source_candidate_free(&candidate); CHECK(!candidate.name && !candidate.source.path);
    analysis_free(&app); workbook_free(&b);
}
static void reply_failures(void) {
    for (unsigned mode=0;mode<10;mode++) {
        Workbook b=book(); uint64_t request=7,epoch=9; PrimarySourceDraftContext c={&b,&b.sheets[0].analysis,&request,&epoch,true,false};
        Analysis source={.kind=SOURCE_DATABASE,.path="/new.duckdb",.schema="chosen"}; PrimarySourceDraft *draft=begin(c,&source);
        Analysis reply=source; reply.schema="chosen"; reply.table="Sales"; PrimarySourceDraftReply actual={request,&reply,NULL};
        if (mode==0) actual.request++;
        if (mode==1) actual.error="DuckDB probe failure";
        if (mode==2) reply.table=NULL;
        if (mode==3) reply.table="";
        if (mode==4) reply.schema="other";
        if (mode==5) reply.path="/other.duckdb";
        if (mode==6) reply.unique_rows=true;
        if (mode==7) reply.source_version=1;
        if (mode==8) reply.filter_number=(NumberType){NUM_DECIMAL,39,0};
        if (mode==9) reply.query="SELECT changed FROM input";
        PrimarySourceDraftStatus expected=mode==1 ? PRIMARY_SOURCE_FAILED : mode==2 || mode==3 || mode==8 ? PRIMARY_SOURCE_INVALID : PRIMARY_SOURCE_STALE;
        PrimarySourceDraftInfo info={1,2,3},old=info; Workbook before=b; size_t bytes=primary_source_draft_owned(draft);
        CHECK(primary_source_draft_reply_measure(c,draft,actual,&info,NULL)==expected && !memcmp(&info,&old,sizeof(info)));
        CHECK(primary_source_draft_resolve(c,draft,actual,SIZE_MAX,NULL)==expected && !primary_source_draft_source(draft) && primary_source_draft_owned(draft)==bytes && !memcmp(&before,&b,sizeof(b)));
        actual=(PrimarySourceDraftReply){request,&source,NULL}; source.table="Sales";
        CHECK(primary_source_draft_resolve(c,draft,actual,SIZE_MAX,NULL)==PRIMARY_SOURCE_RESOLVED); source.table=NULL;
        primary_source_draft_free(&draft); CHECK(!draft); workbook_free(&b);
    }
}
static void fences(void) {
    for (unsigned mode=0;mode<12;mode++) {
        Workbook b=book(); Analysis app=analysis_copy(&b.sheets[0].analysis); uint64_t request=7,epoch=9;
        PrimarySourceDraftContext c={&b,&app,&request,&epoch,true,false}; Analysis source={.kind=SOURCE_DATABASE,.path="/new.duckdb"}; PrimarySourceDraft *draft=begin(c,&source);
        Analysis reply=source; reply.schema="main"; reply.table="Sales"; CHECK(primary_source_draft_resolve(c,draft,(PrimarySourceDraftReply){request,&reply,NULL},SIZE_MAX,NULL)==PRIMARY_SOURCE_RESOLVED);
        if (mode==0) request++;
        if (mode==1) epoch++;
        if (mode==2) b.command_revision++;
        if (mode==3) b.edit_session_id++;
        if (mode==4) b.sheet_high_water++;
        if (mode==5) b.history_branch++;
        if (mode==6) b.sheets[0].cells.revision++;
        if (mode==7) b.sheets[0].formats.revision++;
        if (mode==8) b.sheets[0].history_last++;
        if (mode==9) { char *old=b.sheets[0].name; b.sheets[0].name=dx_strdup(old); free(old); }
        if (mode==10) { char *old=app.path; app.path=dx_strdup(old); free(old); }
        if (mode==11) b.sheets[0].hidden=true;
        CHECK(primary_source_draft_check(c,draft,NULL)==PRIMARY_SOURCE_STALE);
        Workbook before=b; PrimarySourceCandidate out={0};
        CHECK(primary_source_draft_take(c,&draft,&out,NULL)==PRIMARY_SOURCE_STALE && draft && !out.name && !memcmp(&before,&b,sizeof(b)));
        primary_source_draft_free(&draft); analysis_free(&app); workbook_free(&b);
    }
}
static void grammar_and_cancel(void) {
    Workbook b=book(); uint64_t request=7,epoch=9; PrimarySourceDraftContext c={&b,&b.sheets[0].analysis,&request,&epoch,true,false};
    Analysis source={.kind=SOURCE_DATABASE,.path="/new.duckdb"}; CHECK(analysis_filter_set(&source,"Id",FILTER_BETWEEN,"[\"1\",\"2\"]",NULL));
    PrimarySourceDraftInfo info={0}; CHECK(primary_source_draft_measure(c,&source,"New",&info,NULL)==PRIMARY_SOURCE_READY && info.validation_scratch);
    PrimarySourceDraft *draft=NULL; CHECK(primary_source_draft_begin(c,&source,"New",info.owned+info.naming_scratch,&draft,NULL)==PRIMARY_SOURCE_LIMIT && !draft);
    CHECK(primary_source_draft_begin(c,&source,"New",reservation(info),&draft,NULL)==PRIMARY_SOURCE_READY); size_t bytes=primary_source_draft_owned(draft);
    Analysis reply=source; reply.schema="main"; reply.table="Sales"; PrimarySourceDraftReply actual={request,&reply,NULL};
    CHECK(primary_source_draft_reply_measure(c,draft,actual,&info,NULL)==PRIMARY_SOURCE_READY && info.validation_scratch);
    CHECK(primary_source_draft_resolve(c,draft,actual,info.owned,NULL)==PRIMARY_SOURCE_LIMIT && primary_source_draft_owned(draft)==bytes);
    CHECK(primary_source_draft_resolve(c,draft,actual,reservation(info),NULL)==PRIMARY_SOURCE_RESOLVED);
    PrimarySourceCandidate out={0}; c.read_only=true; CHECK(primary_source_draft_take(c,&draft,&out,NULL)==PRIMARY_SOURCE_PROTECTED && draft && !out.name);
    c.read_only=false; CHECK(primary_source_draft_take(c,&draft,&out,NULL)==PRIMARY_SOURCE_TAKEN); primary_source_candidate_free(&out);
    analysis_filters_clear(&source); CHECK(primary_source_draft_begin(c,&source,"bad\nname",SIZE_MAX,&draft,NULL)==PRIMARY_SOURCE_INVALID && !draft);
    CHECK(primary_source_draft_begin(c,&source,"   ",SIZE_MAX,&draft,NULL)==PRIMARY_SOURCE_INVALID && !draft);
    c.ready=false; CHECK(primary_source_draft_begin(c,&source,"New",SIZE_MAX,&draft,NULL)==PRIMARY_SOURCE_PROTECTED && !draft);
    c.ready=true; Workbook before=b;
    Analysis bad={.kind=SOURCE_DATABASE,.path="/new.duckdb",.filter_column="Id",.filter_value="[bad",.filter_op=FILTER_BETWEEN};
    CHECK(primary_source_draft_measure(c,&bad,"New",&info,NULL)==PRIMARY_SOURCE_READY && info.validation_scratch);
    CHECK(primary_source_draft_begin(c,&bad,"New",SIZE_MAX,&draft,NULL)==PRIMARY_SOURCE_INVALID && !draft && !memcmp(&before,&b,sizeof(b)));
    b.sheet_high_water=UINT64_MAX; CHECK(primary_source_draft_measure(c,&source,"New",&info,NULL)==PRIMARY_SOURCE_LIMIT && !draft); b.sheet_high_water=before.sheet_high_water;
    source.table="already resolved"; CHECK(primary_source_draft_measure(c,&source,"New",&info,NULL)==PRIMARY_SOURCE_INVALID); source.table=NULL;
    source.source_version=1; CHECK(primary_source_draft_measure(c,&source,"New",&info,NULL)==PRIMARY_SOURCE_INVALID); source.source_version=0;
    primary_source_draft_free(NULL); primary_source_candidate_free(NULL); workbook_free(&b);
}
static void final_numeric_slot(void) {
    Workbook b=book(); uint64_t request=7,epoch=9; PrimarySourceDraftContext c={&b,&b.sheets[0].analysis,&request,&epoch,true,false};
    Analysis source={.kind=SOURCE_DATABASE,.path=dx_strdup("/new.duckdb")};
    for (size_t i=0;i<DX_ANALYSIS_FILTERS;i++) { char name[24]; snprintf(name,sizeof(name),"Field %zu",i); CHECK(analysis_filter_set(&source,name,FILTER_GREATER,"1",NULL)); }
    CHECK(source.filter_count==DX_ANALYSIS_FILTERS-1);
    size_t last=sizeof(source.filters)/sizeof(*source.filters)-1; CHECK(last==DX_ANALYSIS_FILTERS-2);
    source.filters[last].number=(NumberType){NUM_DECIMAL,39,0}; PrimarySourceDraftInfo info={0};
    CHECK(primary_source_draft_measure(c,&source,"New",&info,NULL)==PRIMARY_SOURCE_INVALID);
    source.filters[last].number=(NumberType){NUM_U64,0,0}; PrimarySourceDraft *draft=begin(c,&source);
    Analysis reply=analysis_copy(&source); reply.schema=dx_strdup("main"); reply.table=dx_strdup("Sales"); reply.filters[last].number=(NumberType){NUM_FLOAT,1,0};
    CHECK(primary_source_draft_reply_measure(c,draft,(PrimarySourceDraftReply){request,&reply,NULL},&info,NULL)==PRIMARY_SOURCE_INVALID);
    reply.filters[last].number=(NumberType){NUM_U64,0,0};
    CHECK(primary_source_draft_resolve(c,draft,(PrimarySourceDraftReply){request,&reply,NULL},SIZE_MAX,NULL)==PRIMARY_SOURCE_RESOLVED);
    primary_source_draft_free(&draft); analysis_free(&source); analysis_free(&reply); workbook_free(&b);
}
static void file_and_demo_replies(void) {
    const SourceKind kinds[]={SOURCE_CSV,SOURCE_PARQUET,SOURCE_DEMO,SOURCE_DEMO};
    for (size_t k=0;k<4;k++) for (unsigned mode=0;mode<10;mode++) {
        Workbook b=book(); Analysis app=analysis_copy(&b.sheets[0].analysis); uint64_t request=7,epoch=9;
        PrimarySourceDraftContext c={&b,&app,&request,&epoch,true,false};
        Analysis proposal={.kind=kinds[k],.path=k==0 ? "/new.csv" : k==1 ? "/new.parquet" : k==2 ? "" : NULL,
            .filter_column="Amount",.filter_value="10",.filter_op=FILTER_GREATER};
        PrimarySourceDraftInfo info={0}; PrimarySourceDraft *draft=NULL;
        CHECK(primary_source_draft_measure(c,&proposal,"File",&info,NULL)==PRIMARY_SOURCE_READY);
        CHECK(primary_source_draft_begin(c,&proposal,"File",reservation(info)-1,&draft,NULL)==PRIMARY_SOURCE_LIMIT && !draft);
        CHECK(primary_source_draft_begin(c,&proposal,"File",reservation(info),&draft,NULL)==PRIMARY_SOURCE_READY);
        Analysis reply=proposal; reply.filter_number=(NumberType){NUM_DECIMAL,12,2};
        if (mode==1) reply.path="/changed.csv";
        if (mode==2) reply.schema="invented";
        if (mode==3) reply.table="invented";
        if (mode==4) reply.query="SELECT changed FROM input";
        if (mode==5) reply.unique_rows=true;
        if (mode==6) reply.kind=SOURCE_DATABASE;
        if (mode==7) reply.filter_number=(NumberType){NUM_DECIMAL,39,0};
        if (mode==8) reply.sort_column="Amount";
        if (mode==9) reply.filter_value="20";
        PrimarySourceDraftReply actual={request,&reply,NULL}; Workbook before=b;
        PrimarySourceDraftStatus expected=mode==0 ? PRIMARY_SOURCE_READY : mode==7 ? PRIMARY_SOURCE_INVALID : PRIMARY_SOURCE_STALE;
        CHECK(primary_source_draft_reply_measure(c,draft,actual,&info,NULL)==expected);
        if (mode) {
            CHECK(primary_source_draft_resolve(c,draft,actual,SIZE_MAX,NULL)==expected && !primary_source_draft_source(draft));
            reply=proposal; reply.filter_number=(NumberType){NUM_DECIMAL,12,2};
        }
        CHECK(primary_source_draft_resolve(c,draft,actual,SIZE_MAX,NULL)==PRIMARY_SOURCE_RESOLVED && !memcmp(&before,&b,sizeof(b)));
        const Analysis *source=primary_source_draft_source(draft);
        CHECK(source && source->kind==kinds[k] && source->filter_number.kind==NUM_DECIMAL && !source->schema && !source->table);
        CHECK((source->path!=NULL)==(proposal.path!=NULL) && (!source->path || !strcmp(source->path,proposal.path)));
        primary_source_draft_free(&draft); analysis_free(&app); workbook_free(&b);
    }
    Workbook b=book(); uint64_t request=1,epoch=1; PrimarySourceDraftContext c={&b,&b.sheets[0].analysis,&request,&epoch,true,false}; PrimarySourceDraftInfo info={0};
    const Analysis invalid[]={ {.kind=SOURCE_NONE}, {.kind=SOURCE_CSV}, {.kind=SOURCE_PARQUET,.path=""},
        {.kind=SOURCE_DEMO,.path="/invented"}, {.kind=SOURCE_CSV,.path="/file.csv",.schema="main"},
        {.kind=SOURCE_PARQUET,.path="/file.parquet",.table="invented"} };
    for (size_t i=0;i<sizeof(invalid)/sizeof(*invalid);i++) CHECK(primary_source_draft_measure(c,invalid+i,"File",&info,NULL)==PRIMARY_SOURCE_INVALID);
    workbook_free(&b);
}
int main(void) { basics(); reply_failures(); fences(); grammar_and_cancel(); final_numeric_slot(); file_and_demo_replies(); printf("PASS: %u pending primary source ownership checks\n",checks); return 0; }
