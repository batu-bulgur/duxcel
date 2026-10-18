#include "ui/stepview.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(condition) do { checks++; if (!(condition)) { fprintf(stderr,"FAIL stepview:%d: %s\n",__LINE__,#condition); exit(1); } } while (0)
static bool valid_utf8(const char *text) {
    const unsigned char *s=(const unsigned char *)text;
    while (*s) {
        unsigned char lead=*s++; unsigned count; uint32_t code;
        if (lead<128) continue;
        if (lead>=0xc2 && lead<=0xdf) { count=1; code=lead&31; }
        else if (lead>=0xe0 && lead<=0xef) { count=2; code=lead&15; }
        else if (lead>=0xf0 && lead<=0xf4) { count=3; code=lead&7; }
        else return false;
        for (unsigned i=0;i<count;i++) { if ((*s&0xc0)!=0x80) return false; code=(code<<6)|(*s++&63); }
        if ((count==2 && code<0x800) || (count==3 && code<0x10000) || code>0x10ffff || (code>=0xd800 && code<=0xdfff)) return false;
    }
    return true;
}
static void named_steps(void) {
    Analysis source={.kind=SOURCE_CSV,.path=dx_strdup("import-1-import-7-O'Brien-İstanbul-表.csv"),.source_version=19},prefix={0};
    char *path=source.path,*error=NULL; UIStep ordinary[DX_UI_STEPS],named[DX_UI_STEPS];
    CHECK(analysis_filter_set(&source,"City",FILTER_EQUAL,"İstanbul",&error));
    CHECK(analysis_sort_set(&source,"Amount",true,false,&error));
    size_t count=ui_source_steps(&source,6,ordinary); CHECK(count==3);
    CHECK(ui_source_steps_named(&source,6,NULL,named)==count && !memcmp(ordinary,named,sizeof(named)));
    const char *filename="import-7-O'Brien-İstanbul-表.csv";
    CHECK(ui_source_steps_named(&source,6,filename,named)==count);
    CHECK(!strcmp(named[0].title,"Import import-7-O'Brien-İstanbul-表.csv"));
    CHECK(!strcmp(named[0].detail,ordinary[0].detail) && !memcmp(named+1,ordinary+1,(DX_UI_STEPS-1)*sizeof(*named)));
    CHECK(source.path==path && !strcmp(path,"import-1-import-7-O'Brien-İstanbul-表.csv") && source.source_version==19);
    CHECK(ui_step_source(&source,0,&prefix));
    CHECK(prefix.path!=path && !strcmp(prefix.path,path) && prefix.source_version==19 && !prefix.filter_column && !prefix.sort_column);
    Analysis another={.kind=SOURCE_CSV,.path=dx_strdup("import-2-import-7-O'Brien-İstanbul-表.csv")};
    CHECK(ui_source_steps_named(&another,6,filename,named)==1 && !strcmp(named[0].title,"Import import-7-O'Brien-İstanbul-表.csv"));
    CHECK(strcmp(source.path,another.path) && !strcmp(another.path,"import-2-import-7-O'Brien-İstanbul-表.csv"));
    CHECK(ui_source_steps_named(&source,6,"literal/path\\name.csv",named)==count && !strcmp(named[0].title,"Import literal/path\\name.csv"));
    CHECK(ui_source_steps_named(&source,6,"",named)==count && !strcmp(named[0].title,"Import "));
    CHECK(ui_source_steps_named(&source,6,"one\ntwo\tthree.csv",named)==count && !strcmp(named[0].title,"Import one two three.csv"));
    source.query=dx_strdup("SELECT * FROM read_csv_auto('import-1-import-7-O''Brien-İstanbul-表.csv')"); char *query=source.query;
    CHECK(ui_source_steps_named(&source,6,filename,named)==count && !strcmp(named[0].title,"Source query"));
    CHECK(!strcmp(named[0].detail,"SELECT over import-7-O'Brien-İstanbul-表.csv"));
    CHECK(ui_step_source(&source,0,&prefix) && prefix.query!=query && !strcmp(prefix.query,query) && !strcmp(prefix.path,path));
    CHECK(source.query==query && source.path==path);
    char long_name[4096];
    for (size_t offset=0;offset<12;offset++) {
        memset(long_name,'x',offset); size_t bytes=offset;
        while (bytes+sizeof("ğ表🦆")<sizeof(long_name)) { memcpy(long_name+bytes,"ğ表🦆",sizeof("ğ表🦆")-1); bytes+=sizeof("ğ表🦆")-1; }
        long_name[bytes]=0; CHECK(bytes>200 && valid_utf8(long_name));
        CHECK(ui_source_steps_named(&source,6,long_name,named)==count && strlen(named[0].detail)<sizeof(named[0].detail) && valid_utf8(named[0].detail));
        free(source.query); source.query=NULL;
        CHECK(ui_source_steps_named(&source,6,long_name,named)==count && strlen(named[0].title)<sizeof(named[0].title) && valid_utf8(named[0].title));
        CHECK(!strncmp(named[0].title,"Import ",7) && !strncmp(named[0].title+7,long_name,strlen(named[0].title)-7));
        source.query=dx_strdup("SELECT * FROM source");
    }
    Analysis blank={0},demo={.kind=SOURCE_DEMO},database={.kind=SOURCE_DATABASE,.path=dx_strdup("/native/data.duckdb"),.table=dx_strdup("items")};
    CHECK(ui_source_steps_named(&blank,0,filename,named)==1 && !strcmp(named[0].title,"Blank worksheet"));
    CHECK(ui_source_steps_named(&demo,0,filename,named)==1 && !strcmp(named[0].title,"Import sample data"));
    CHECK(ui_source_steps_named(&database,3,filename,named)==1 && !strcmp(named[0].title,"Connect import-7-O'Brien-İstanbul-表.csv") && !strcmp(named[0].detail,"main.items · 3 fields"));
    CHECK(ui_source_steps(&database,3,ordinary)==1 && !strcmp(ordinary[0].title,"Connect data.duckdb"));
    analysis_free(&database); analysis_free(&another); analysis_free(&prefix); analysis_free(&source); free(error);
}
int main(void) {
    named_steps();
    Analysis source={.kind=SOURCE_PARQUET,.path=dx_strdup("/data/transactions.parquet")},preview={0}; UIStep steps[DX_UI_STEPS]; char *error=NULL;
    CHECK(analysis_filter_set(&source,"Status",FILTER_IN,"[\"Posted\"]",&error)); CHECK(analysis_filter_set(&source,"Currency",FILTER_EQUAL,"USD",&error));
    CHECK(analysis_sort_set(&source,"Transaction ID",false,false,&error));
    CHECK(ui_source_steps(&source,12,steps)==4); CHECK(!strcmp(steps[0].title,"Import transactions.parquet")); CHECK(!strcmp(steps[0].detail,"Parquet · 12 fields"));
    CHECK(!strcmp(steps[1].title,"Filter Status = Posted")); CHECK(!strcmp(steps[1].detail,"Status equals 'Posted'")); CHECK(!strcmp(steps[2].title,"Filter Currency = USD"));
    CHECK(steps[3].kind==UI_STEP_SORT && !strcmp(steps[3].title,"Sort Transaction ID ascending"));
    CHECK(ui_step_source(&source,1,&preview)); CHECK(preview.filter_column && !preview.filter_count && !preview.sort_column && strcmp(preview.filter_column,"Status")==0);
    CHECK(source.filter_count==1 && source.sort_column && source.filter_column!=preview.filter_column);
    CHECK(ui_step_source(&source,0,&preview)); CHECK(!preview.filter_column && !preview.sort_column && preview.path && preview.path!=source.path);
    CHECK(!ui_step_source(&source,100,&preview)); CHECK(preview.path && !preview.filter_column);
    source.unique_rows=true; source.group_column=dx_strdup("Account"); source.measure_column=dx_strdup("Amount"); source.aggregate=AGG_AVG;
    CHECK(analysis_sort_set(&source,"Amount",true,true,&error)); CHECK(ui_source_steps(&source,12,steps)==7);
    CHECK(steps[1].kind==UI_STEP_UNIQUE && steps[4].kind==UI_STEP_GROUP && steps[6].index==1);
    CHECK(ui_step_source(&source,1,&preview)); CHECK(preview.unique_rows && !preview.filter_column && !preview.group_column && !preview.sort_column);
    CHECK(ui_step_source(&source,4,&preview)); CHECK(preview.unique_rows && preview.filter_count==1 && preview.group_column && preview.aggregate==AGG_AVG && !preview.sort_column);
    CHECK(ui_step_source(&source,5,&preview)); CHECK(preview.sort_column && !preview.sort_count && preview.group_column);
    CHECK(ui_step_source(&source,6,&preview)); CHECK(preview.sort_count==1 && preview.sorts[0].descending);
    CHECK(analysis_filter_set(&source,"Status",FILTER_IN,"[null]",&error)); CHECK(ui_source_steps(&source,12,steps)==7); CHECK(!strcmp(steps[2].detail,"Status is SQL NULL"));
    CHECK(analysis_filter_set(&source,"Status",FILTER_IN,"[\"NULL\"]",&error)); ui_source_steps(&source,12,steps); CHECK(!strcmp(steps[2].detail,"Status equals 'NULL'"));
    CHECK(analysis_filter_set(&source,"Status",FILTER_IN,"[\"\"]",&error)); ui_source_steps(&source,12,steps); CHECK(!strcmp(steps[2].title,"Filter Status = \"\""));
    CHECK(analysis_filter_set(&source,"Status",FILTER_IN,"[\"a\",\"b\"]",&error)); ui_source_steps(&source,12,steps); CHECK(!strcmp(steps[2].detail,"2 selected values"));
    CHECK(analysis_filter_set(&source,"Status",FILTER_NOT_IN,"[\"Posted\"]",&error)); ui_source_steps(&source,12,steps); CHECK(strstr(steps[2].title,"≠ Posted"));
    char long_name[4096]; for (size_t i=0;i<1365;i++) memcpy(long_name+3*i,"ğ",2),long_name[3*i+2]='x'; long_name[4095]=0;
    CHECK(analysis_filter_set(&source,long_name,FILTER_CONTAINS,"a\nb",&error)); ui_source_steps(&source,12,steps);
    for (size_t i=0;i<8;i++) { CHECK(strlen(steps[i].title)<320 && strlen(steps[i].detail)<512); CHECK(!strchr(steps[i].detail,'\n')); }
    source.filter_count=DX_ANALYSIS_FILTERS; CHECK(!ui_source_steps(&source,12,steps)); source.filter_count=2;
    analysis_free(&preview); analysis_free(&source); free(error);
    printf("PASS: %u native step checks (named source presentation, unchanged identity, exact filter labels, typed NULL/empty/literal keys, owned prefix previews, sort/group order and UTF-8 bounds)\n",checks); return 0;
}
