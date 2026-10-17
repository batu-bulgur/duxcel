#include "ui/stepview.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(condition) do { checks++; if (!(condition)) { fprintf(stderr,"FAIL stepview:%d: %s\n",__LINE__,#condition); exit(1); } } while (0)
int main(void) {
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
    printf("PASS: %u native step checks (source scope, exact filter labels, typed NULL/empty/literal keys, owned prefix previews, sort/group order and bounds)\n",checks); return 0;
}
