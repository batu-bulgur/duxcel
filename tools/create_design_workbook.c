/* Offline acceptance workbook. All definitions, filters and PIVOT results use
   the production C99 document/engine code, rather than browser calculations. */
#include "workbook/workbook.h"
#include "data/database.h"
#include "platform/platform.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc,char **argv) {
    bool large=argc>1 && !strcmp(argv[1],"--large"); const char *prefix=large ? "design-large" : "design-definition";
    Workbook book; workbook_init(&book); Database db; bool opened=false,ok=false;
    char *error=NULL; Page page={0}; PivotDefinition pivot={0};
    char input[160],output[160]; snprintf(input,sizeof(input),"tests/generated/%s.parquet",prefix); snprintf(output,sizeof(output),"tests/generated/%s.dxls",prefix);
    Analysis source={.kind=SOURCE_PARQUET,.path=dx_absolute_path(input)};
    if (!source.path || !analysis_filter_set(&source,"Status",FILTER_IN,"[\"Posted\"]",&error) ||
        !analysis_filter_set(&source,"Currency",FILTER_IN,"[\"USD\"]",&error) ||
        !analysis_sort_set(&source,"Transaction ID",false,false,&error)) goto done;
    if (!workbook_add(&book,"Transactions",&source,&error) || !workbook_add(&book,"Monthly cash flow",&source,&error) || !workbook_add(&book,"Counterparties",&source,&error) || !workbook_add(&book,"Reconciliation",&source,&error)) goto done;
    book.active=0; Worksheet *sheet=&book.sheets[0];
    if (!pivot_assign(&pivot,PIVOT_ROWS,"Account",AGG_SUM,&error) || !pivot_assign(&pivot,PIVOT_ROWS,"Category",AGG_SUM,&error) ||
        !pivot_assign_grain(&pivot,PIVOT_COLUMNS,"Date",AGG_SUM,PIVOT_DATE_YEAR,&error) ||
        !pivot_assign_grain(&pivot,PIVOT_COLUMNS,"Date",AGG_SUM,PIVOT_DATE_QUARTER,&error) ||
        !pivot_assign_grain(&pivot,PIVOT_COLUMNS,"Date",AGG_SUM,PIVOT_DATE_MONTH,&error) ||
        !pivot_assign(&pivot,PIVOT_VALUES,"Amount (USD)",AGG_SUM,&error) || !pivot_assign(&pivot,PIVOT_VALUES,"Amount (USD)",AGG_COUNT,&error)) goto done;
    opened=database_open(&db,&error); if (!opened || !database_pivot(&db,&source,&pivot,0,-1,&page,&error)) goto done;
    int64_t cents=0,count=0;
    for (size_t r=0;r<page.row_count;r++) for (size_t c=2;c<page.column_count;c++) {
        Cell *cell=&page.cells[r*page.column_count+c]; if (cell->is_null) continue;
        if ((c-2)%2) count+=(int64_t)cell->number; else cents+=(int64_t)llround(cell->number*100);
    }
    if (page.total!=30 || page.column_count!=8 || cents!=(large ? INT64_C(9686461450) : INT64_C(-128883430)) || count!=(large ? 36984 : 240)) { error=dx_strdup("Native aggregates differ from the canonical design fixture."); goto done; }
    int index=objects_add(&sheet->objects,0,12,"Cash flow by category",&error);
    if (index<0 || !objects_update(&sheet->objects,(size_t)index,&pivot,&error)) goto done;
    MarkDefinition marks={0};
    MarkField label={.name="Amount (USD)",.aggregate=AGG_SUM,.measure=true},detail={.name="Account"};
    bool marked=marks_assign(&marks,MARKS_LABEL,&label,&error) && marks_assign(&marks,MARKS_DETAIL,&detail,&error) && objects_marks(&sheet->objects,(size_t)index,&marks,&error);
    marks_free(&marks); if (!marked) goto done;
    sheet->objects.items[index].rows=page.total+2; sheet->objects.items[index].columns=page.column_count;
    CellFormat format=format_default(); format.number=FORMAT_ACCOUNTING;
    if (!format_apply(&sheet->formats,(FormatRange){0,DX_SHEET_ROWS-1,5,5},FORMAT_NUMERIC,format,&error)) goto done;
    for (size_t c=2;c<page.column_count;c++) {
        format.number=(c-2)%2 ? FORMAT_NUMBER : FORMAT_ACCOUNTING; format.decimals=(c-2)%2 ? 0 : 2;
        if (!format_apply(&sheet->formats,(FormatRange){2,page.total+1,12+c,12+c},FORMAT_NUMERIC|FORMAT_DECIMALS,format,&error)) goto done;
    }
    static const char *names[]={"Date","Transaction ID","Counterparty","Category","Account","Amount (USD)","Status","Currency","Month","Region","Reference","Memo"};
    static const float widths[]={112,146,184,150,116,146,100,94,118,132,156,220};
    sheet->view.column_count=12; sheet->view.columns=dx_alloc(12*sizeof(char *)); sheet->view.widths=dx_alloc(sizeof(widths));
    memcpy(sheet->view.widths,widths,sizeof(widths)); for (size_t c=0;c<12;c++) sheet->view.columns[c]=dx_strdup(names[c]);
    sheet->view.row=sheet->view.anchor_row=sheet->view.end_row=2;
    sheet->view.col=sheet->view.anchor_col=sheet->view.end_col=12; sheet->view.canvas_zoom=1;
    ok=workbook_save(&book,output,&error);
done:
    if (!ok) fprintf(stderr,"Design workbook: %s\n",error ? error : "fixture unavailable");
    else printf("PASS: %s fixture, %d filtered transactions, 30 native PIVOT groups and saved source-bound authoring workbook\n",large ? "large" : "canonical 310-row",large ? 36984 : 240);
    page_free(&page); if (opened) database_close(&db); free(error); pivot_free(&pivot); analysis_free(&source); workbook_free(&book); return ok ? 0 : 1;
}
