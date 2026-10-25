#include "ui/sourceview.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(c) do { checks++; if (!(c)) { fprintf(stderr,"FAIL sourceview:%d: %s\n",__LINE__,#c); exit(1); } } while (0)
int main(void) {
    char path[]="/data/O'Brien-İstanbul-表.csv",schema[]="",table[]="NULL",query[]="SELECT '' AS empty, NULL AS absent, 'NULL' AS literal";
    for (SourceKind kind=SOURCE_CSV;kind<=SOURCE_DATABASE;kind++) {
        Analysis source={.kind=kind,.path=path,.schema=schema,.table=table,.query=query,.source_version=UINT64_MAX};
        Analysis before=source;
        for (SourceViewState state=SOURCE_VIEW_LOADING;state<=SOURCE_VIEW_UNAVAILABLE;state++) {
            SourceProvenanceView view={0};
            CHECK(ui_source_provenance(&source,state,&view));
            CHECK(view.definition==&source && view.kind==kind && view.state==state && view.external && view.read_only);
            CHECK(view.path==path && view.schema==schema && view.table==table && view.query==query);
            CHECK(!memcmp(&source,&before,sizeof(source)) && !strcmp(view.path,"/data/O'Brien-İstanbul-表.csv"));
            CHECK(!strcmp(view.schema,"") && !strcmp(view.table,"NULL") && !strcmp(view.query,query));
        }
        source.schema=source.table=source.query=NULL;
        SourceProvenanceView view={0}; CHECK(ui_source_provenance(&source,SOURCE_VIEW_UNAVAILABLE,&view));
        CHECK(!view.schema && !view.table && !view.query && view.path==path);
        /* This is a borrowed projection, never a retained second definition. */
        path[1]='D'; CHECK(view.path[1]=='D'); path[1]='d';
    }
    char long_path[8192],long_query[8192];
    size_t bytes=0;
    while (bytes+sizeof("İstanbul-表-🦆/")<sizeof(long_path)) { memcpy(long_path+bytes,"İstanbul-表-🦆/",sizeof("İstanbul-表-🦆/")-1); bytes+=sizeof("İstanbul-表-🦆/")-1; }
    long_path[bytes]=0; memcpy(long_query,long_path,bytes+1);
    Analysis long_source={.kind=SOURCE_PARQUET,.path=long_path,.query=long_query};
    SourceProvenanceView long_view={0}; CHECK(ui_source_provenance(&long_source,SOURCE_VIEW_UNAVAILABLE,&long_view));
    CHECK(long_view.path==long_path && long_view.query==long_query && strlen(long_view.path)==bytes && !strcmp(long_view.query,long_query));
    SourceProvenanceView sentinel={.kind=SOURCE_DATABASE,.path="preserve",.external=true},out=sentinel;
    Analysis invalid={0};
    CHECK(!ui_source_provenance(NULL,SOURCE_VIEW_READY,&out)); CHECK(!memcmp(&out,&sentinel,sizeof(out)));
    CHECK(!ui_source_provenance(&invalid,SOURCE_VIEW_READY,&out));
    invalid=(Analysis){.kind=SOURCE_DEMO,.path=path}; CHECK(!ui_source_provenance(&invalid,SOURCE_VIEW_READY,&out));
    invalid.kind=SOURCE_CSV; invalid.path=""; CHECK(!ui_source_provenance(&invalid,SOURCE_VIEW_READY,&out));
    invalid.path=NULL; CHECK(!ui_source_provenance(&invalid,SOURCE_VIEW_READY,&out));
    invalid.path=path; CHECK(!ui_source_provenance(&invalid,(SourceViewState)-1,&out));
    CHECK(!ui_source_provenance(&invalid,SOURCE_VIEW_READY,NULL)); CHECK(!memcmp(&out,&sentinel,sizeof(out)));
    CHECK(!strcmp(ui_source_kind_label(SOURCE_CSV),"Delimited text"));
    CHECK(!strcmp(ui_source_kind_label(SOURCE_PARQUET),"Parquet"));
    CHECK(!strcmp(ui_source_kind_label(SOURCE_DATABASE),"DuckDB"));
    CHECK(!strcmp(ui_source_state_label(SOURCE_VIEW_LOADING),"Loading"));
    CHECK(!strcmp(ui_source_state_label(SOURCE_VIEW_READY),"Ready"));
    CHECK(!strcmp(ui_source_state_label(SOURCE_VIEW_UNAVAILABLE),"Unavailable"));
    printf("PASS: %u source provenance projection checks\n",checks); return 0;
}
