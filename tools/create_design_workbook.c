/* Offline acceptance workbook. All definitions, filters and PIVOT results use
   the production C99 document/engine code, rather than browser calculations. */
#include "workbook/workbook.h"
#include "data/database.h"
#include "platform/platform.h"
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

static const char *names[]={"Date","Transaction ID","Counterparty","Category","Account","Amount (USD)","Status","Currency","Month","Region","Reference","Memo"};

/* Deterministic transaction inputs belong to the native acceptance fixture.
   Generate data only: filtering, aggregation and saving use production C below. */
static void csv_text(FILE *file,const char *text) {
    if (!text) return; /* Unquoted missing field is SQL NULL; quoted "" is text. */
    fputc('"',file);
    for (;*text;text++) { if (*text=='"') fputc('"',file); fputc(*text,file); }
    fputc('"',file);
}
static void transaction(FILE *file,unsigned i,bool force_usd) {
    static const struct { const char *date,*id,*partner,*category; int64_t cents; } first[]={
        {"2026-09-14","TX-0914-1042","Northstar Labs","Customer receipt",12845000},
        {"2026-09-14","TX-0914-1043","Atlas Supply Co.","Inventory",-2486000},
        {"2026-09-14","TX-0914-1044","Harbor Cloud","Software",-485000},
        {"2026-09-15","TX-0915-1045","Payroll batch","Payroll",-9642000},
        {"2026-09-15","TX-0915-1046","Meridian Retail","Customer receipt",7420000},
        {"2026-09-15","TX-0915-1047","City Properties","Rent",-1250000},
        {"2026-09-16","TX-0916-1048","Acme Components","Inventory",-1834000},
        {"2026-09-16","TX-0916-1049","Juniper Studio","Customer receipt",3280000},
        {"2026-09-16","TX-0916-1050","Metro Utilities","Utilities",-218462},
        {"2026-09-17","TX-0917-1051","Summit Logistics","Freight",-764000},
        {"2026-09-17","TX-0917-1052","Bluepeak Media","Software",-312000},
        {"2026-09-18","TX-0918-1053","Pinecrest Capital","Customer receipt",4790000}
    };
    static const char *partners[]={"Northstar Labs","Atlas Supply Co.","Harbor Cloud","Meridian Retail","City Properties","Acme Components","Juniper Studio","Metro Utilities","Summit Logistics","Bluepeak Media","Pinecrest Capital","Redwood Systems","Lakeview Solutions","Clearwater Inc.","Crescent Manufacturing","Oakridge Ventures","Vertex Technology","Maple Office Supply","Ridgeway Telecom","Westlake Industries"};
    static const char *categories[]={"Customer receipt","Inventory","Software","Payroll","Rent","Utilities","Freight","Equipment","Marketing","Interest"};
    static const char *regions[]={"North America","Europe","Asia Pacific"};
    static const char *memos[]={"Quarterly settlement","Bank transfer","Recurring payment","Invoice cleared","Batch settlement"};
    bool seed=i<sizeof(first)/sizeof(first[0]);
    char date[11],id[32],month[8],reference[32],amount[32];
    if (seed) { snprintf(date,sizeof(date),"%s",first[i].date); snprintf(id,sizeof(id),"%s",first[i].id); }
    else { snprintf(date,sizeof(date),"2026-%02u-%02u",7+i*7%3,1+i*11%28); snprintf(id,sizeof(id),"TX-26%02u-%06u",7+i*7%3,i+1); }
    memcpy(month,date,7); month[7]=0;
    const char *category=seed ? first[i].category : categories[i*7%10];
    bool positive=!strcmp(category,"Customer receipt") || !strcmp(category,"Interest");
    int64_t cents=seed ? first[i].cents : (positive ? 1 : -1)*(int64_t)(25000+i*7919%(positive ? 12500000 : 2400000));
    int64_t magnitude=cents<0 ? -cents : cents;
    snprintf(amount,sizeof(amount),"%s%" PRId64 ".%02" PRId64,cents<0 ? "-" : "",magnitude/100,magnitude%100);
    snprintf(reference,sizeof(reference),"INV-%06u",100000+i);
    const char *fields[]={date,id,seed ? first[i].partner : partners[i%20],category,
        i<12 || i%7<5 ? "Operating" : i%7==5 ? "Reserve" : "Payroll",amount,
        i<38450 ? "Posted" : i%2 ? "Pending" : "Reversed",
        force_usd || i<36984 ? "USD" : i%3 ? "EUR" : "GBP",month,regions[i%3],
        i%23 ? reference : NULL,i%19==0 ? "" : i%31==0 ? NULL : memos[i%5]};
    for (size_t c=0;c<sizeof(fields)/sizeof(fields[0]);c++) {
        if (c) fputc(',',file);
        if (c==5) fputs(amount,file); else csv_text(file,fields[c]);
    }
    fputc('\n',file);
}
static bool create_input(Database *db,const char *prefix,bool large,char **error) {
    char csv[160],parquet[160];
    snprintf(csv,sizeof(csv),"tests/generated/%s.csv",prefix);
    snprintf(parquet,sizeof(parquet),"tests/generated/%s.parquet",prefix);
#ifdef _WIN32
    int created=_mkdir("tests/generated");
#else
    int created=mkdir("tests/generated",0700);
#endif
    if (created && errno!=EEXIST) { *error=dx_strdup("Cannot create fixture directory."); return false; }
    char *temporary=dx_temp_sibling(csv);
    FILE *file=temporary ? fopen(temporary,"wb") : NULL;
    if (!file) { if (temporary) remove(temporary); free(temporary); *error=dx_strdup("Cannot create transaction fixture."); return false; }
    for (size_t c=0;c<sizeof(names)/sizeof(names[0]);c++) { if (c) fputc(',',file); csv_text(file,names[c]); }
    fputc('\n',file);
    unsigned rows=large ? 48216 : 310;
    for (unsigned row=0;row<rows;row++) {
        unsigned i=large || row<240 ? row : row<280 ? 48176+row-240 : 37000+row-280;
        transaction(file,i,!large && row>=240 && row<280);
    }
    bool written=!ferror(file); if (fclose(file)!=0) written=false;
    bool published=written && dx_replace_file(temporary,csv);
    if (!published) { remove(temporary); free(temporary); *error=dx_strdup("Cannot publish transaction fixture."); return false; }
    free(temporary);
    temporary=dx_temp_sibling(parquet);
    if (!temporary) { *error=dx_strdup("Cannot create Parquet fixture."); return false; }
    char *quoted_csv=dx_quote(csv,'\''),*quoted_parquet=dx_quote(temporary,'\'');
    char *sql=dx_printf("COPY (SELECT * REPLACE (\"Amount (USD)\"::DECIMAL(18,2) AS \"Amount (USD)\") FROM read_csv(%s,header=true,allow_quoted_nulls=false)) TO %s (FORMAT PARQUET)",quoted_csv,quoted_parquet);
    duckdb_result result={0}; bool converted=database_execute(db,sql,&result,error);
    if (converted) duckdb_destroy_result(&result);
    free(sql);
    if (converted) {
        sql=dx_printf("SELECT count(*),count(*) FILTER (WHERE \"Memo\" IS NULL)>0,count(*) FILTER (WHERE \"Memo\"='')>0,bool_and(typeof(\"Amount (USD)\")='DECIMAL(18,2)') FROM read_parquet(%s)",quoted_parquet);
        converted=database_execute(db,sql,&result,error);
        if (converted) {
            converted=duckdb_row_count(&result)==1 && duckdb_value_uint64(&result,0,0)==rows &&
                duckdb_value_boolean(&result,1,0) && duckdb_value_boolean(&result,2,0) && duckdb_value_boolean(&result,3,0);
            duckdb_destroy_result(&result);
            if (!converted) *error=dx_strdup("Transaction fixture lost its rows, NULL/text distinction or exact DECIMAL type.");
        }
    } else sql=NULL;
    free(sql); free(quoted_csv); free(quoted_parquet);
    published=converted && dx_replace_file(temporary,parquet);
    if (!published) { remove(temporary); if (converted) *error=dx_strdup("Cannot publish Parquet fixture."); }
    free(temporary); return published;
}

int main(int argc,char **argv) {
    bool large=argc>1 && !strcmp(argv[1],"--large"); const char *prefix=large ? "design-large" : "design-definition";
    Workbook book; workbook_init(&book); Database db; bool opened=false,ok=false;
    char *error=NULL; Page page={0}; PivotDefinition pivot={0};
    char input[160],output[160]; snprintf(input,sizeof(input),"tests/generated/%s.parquet",prefix); snprintf(output,sizeof(output),"tests/generated/%s.dxls",prefix);
    Analysis source={.kind=SOURCE_PARQUET};
    opened=database_open(&db,&error);
    if (!opened || !create_input(&db,prefix,large,&error)) goto done;
    source.path=dx_absolute_path(input);
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
    if (!database_pivot(&db,&source,&pivot,0,-1,&page,&error)) goto done;
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
