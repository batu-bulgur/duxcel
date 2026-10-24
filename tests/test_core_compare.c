#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "workbook/core_compare.h"
#include "workbook/workbook_codec.h"
#include "platform/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL core equality:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
typedef struct { bool cancel; WorkbookCoreComparePhase phase; size_t table; uint64_t seen; unsigned phases; } Probe;
static bool probe(void *context,WorkbookCoreComparePhase phase,size_t table) {
    Probe *p=context; p->phases|=1u<<phase;
    if (phase==CORE_COMPARE_ROWS) { CHECK(table<workbook_codec_table_count(15)); p->seen|=UINT64_C(1)<<table; }
    return p->cancel && phase==p->phase && table>=p->table;
}
static WorkbookCorePayload *capture(const Workbook *book,const char *parent) {
    WorkbookCorePayload *out=NULL; char *error=NULL;
    WorkbookCorePayloadResult result=workbook_core_payload_capture(book,parent,(WorkbookCorePayloadControl){0},&out,&error);
    if (result!=CORE_PAYLOAD_READY) fprintf(stderr,"Capture detail: %s\n",error ? error : "none");
    CHECK(result==CORE_PAYLOAD_READY && !error && out); return out;
}
static void compare(const WorkbookCorePayload *a,const WorkbookCorePayload *b,bool expected) {
    char *error=NULL; bool same=!expected;
    CHECK(workbook_core_payload_equal(a,b,(WorkbookCoreCompareControl){0},&same,&error)==CORE_PAYLOAD_READY && !error && same==expected);
}
static void changed(const Workbook *book,const char *parent,const WorkbookCorePayload *base) {
    WorkbookCorePayload *other=capture(book,parent); compare(base,other,false); compare(other,base,false);
    CHECK(workbook_core_payload_retire(other,NULL)); workbook_core_payload_free(other);
}
int main(void) {
    char parent[]="build/tests/core-compare-XXXXXX"; CHECK(mkdtemp(parent));
    Workbook book; workbook_init(&book); char *error=NULL;
    Analysis source={.kind=SOURCE_CSV,.path="/offline/sales' source.csv",.filter_column="Optional",.filter_op=FILTER_NULL};
    CHECK(workbook_add(&book,"Complete core",&source,&error)); Worksheet *s=book.sheets;
    CHECK(cells_set(&s->cells,50,30,"9007199254740993.0123456789",&error));
    CHECK(objects_add_table(&s->objects,0,8,"External table",&(Analysis){.kind=SOURCE_PARQUET,.path="/offline/table.parquet"},&error)==0);
    CHECK(objects_add(&s->objects,0,20,"Summary",&error)==1);
    CHECK(pivot_assign(&s->objects.items[1].pivot,PIVOT_ROWS,"City",AGG_SUM,&error));
    CHECK(pivot_assign(&s->objects.items[1].pivot,PIVOT_VALUES,"Amount",AGG_SUM,&error));
    SourceVersions versions={0}; SourceVersion entry={.version=1,.kind=source.kind,.path=source.path};
    CHECK(source_versions_plan(&s->source_versions,&entry,true,&versions,&error)); s->source_versions=versions;
    s->formats.source_aware=true; s->analysis.source_version=1;
    CellFormat style=format_default(); style.flags=FORMAT_BOLD; style.color=3;
    CHECK(format_apply(&s->formats,(FormatRange){50,50,30,30},FORMAT_BOLD|FORMAT_COLOR,style,&error));
    s->view.column_count=1; s->view.columns=dx_alloc(sizeof(char *)); s->view.columns[0]=dx_strdup("Amount");
    s->view.widths=dx_alloc(sizeof(float)); s->view.widths[0]=120;
    WorkbookCorePayload *base=NULL,*other=NULL;
#if !defined(__APPLE__) || defined(__EMSCRIPTEN__)
    CHECK(workbook_core_payload_capture(&book,parent,(WorkbookCorePayloadControl){0},&base,&error)==CORE_PAYLOAD_UNSUPPORTED && !base);
    free(error); error=NULL;
#else
    base=capture(&book,parent); other=capture(&book,parent);
    CHECK(strcmp(workbook_core_payload_path(base),workbook_core_payload_path(other)));
    Probe observed={0}; bool same=false;
    CHECK(workbook_core_payload_equal(base,other,(WorkbookCoreCompareControl){probe,&observed},&same,&error)==CORE_PAYLOAD_READY && same && !error);
    CHECK(observed.seen==(UINT64_C(1)<<workbook_codec_table_count(15))-1 && observed.phases==7);
    compare(base,base,true); compare(other,base,true);
    for (unsigned phase=0;phase<3;phase++) {
        Probe stop={.cancel=true,.phase=(WorkbookCoreComparePhase)phase,.table=phase==CORE_COMPARE_ROWS ? 20 : 0}; same=true;
        CHECK(workbook_core_payload_equal(base,other,(WorkbookCoreCompareControl){probe,&stop},&same,&error)==CORE_PAYLOAD_CANCELLED && same && error);
        free(error); error=NULL;
        compare(base,other,true);
    }
    /* These edits intentionally retain command/query/format/title clocks.
       Equality must compare complete persisted values instead of those clocks. */
    uint64_t revision=s->cells.revision;
    CHECK(cells_set(&s->cells,50,30,"9007199254740993.0123456788",&error)); s->cells.revision=revision; changed(&book,parent,base);
    CHECK(cells_set(&s->cells,50,30,"9007199254740993.0123456789",&error)); s->cells.revision=revision;
    s->view.widths[0]=121; changed(&book,parent,base); s->view.widths[0]=120;
    s->formats.items[0].value.color=2; changed(&book,parent,base); s->formats.items[0].value.color=3;
    s->analysis.filter_value=dx_strdup(""); changed(&book,parent,base);
    WorkbookCorePayload *empty=capture(&book,parent); free(s->analysis.filter_value);
    s->analysis.filter_value=dx_strdup("NULL"); changed(&book,parent,base);
    WorkbookCorePayload *literal=capture(&book,parent); compare(empty,literal,false);
    workbook_core_payload_free(empty); workbook_core_payload_free(literal);
    free(s->analysis.filter_value); s->analysis.filter_value=NULL;
    s->analysis.query=dx_strdup("SELECT 1 AS changed"); changed(&book,parent,base); free(s->analysis.query); s->analysis.query=NULL;
    s->objects.items[0].source.query=dx_strdup("SELECT 2 AS changed"); changed(&book,parent,base); free(s->objects.items[0].source.query); s->objects.items[0].source.query=NULL;
    char *name=s->objects.items[1].name; s->objects.items[1].name="Changed title"; changed(&book,parent,base); s->objects.items[1].name=name;
    char *field=s->objects.items[1].pivot.fields[PIVOT_ROWS][0].name; s->objects.items[1].pivot.fields[PIVOT_ROWS][0].name="Region";
    changed(&book,parent,base); s->objects.items[1].pivot.fields[PIVOT_ROWS][0].name=field;
    s->source_versions.high_water=2; changed(&book,parent,base); s->source_versions.high_water=1;
    book.origin.present=true; book.origin.document=(WorkbookId){1,2}; book.origin.root[0]=7;
    changed(&book,parent,base); WorkbookCorePayload *origin=capture(&book,parent);
    WorkbookCorePayload *same_origin=capture(&book,parent); compare(origin,same_origin,true); workbook_core_payload_free(same_origin);
    book.origin.root[31]=1; changed(&book,parent,origin); memset(&book.origin,0,sizeof(book.origin));
    workbook_core_payload_free(origin);
    book.command_revision=UINT64_C(9007199254740993); changed(&book,parent,base); book.command_revision=0;
    book.user_scale=1.25f; changed(&book,parent,base); book.user_scale=1;
    WorkbookId id=book.id; book.id.low^=1; changed(&book,parent,base); book.id=id;
    WorkbookCorePayload *restored=capture(&book,parent); compare(base,restored,true); workbook_core_payload_free(restored);
    /* A changed/retired payload cannot publish even a plausible equality bit. */
    const char *path=workbook_core_payload_path(other); FILE *f=fopen(path,"ab"); CHECK(f && fputc('x',f)!=EOF && !fclose(f));
    same=true; CHECK(workbook_core_payload_equal(base,other,(WorkbookCoreCompareControl){0},&same,&error)==CORE_PAYLOAD_FAILED && same && error);
    free(error); error=NULL;
    char *directory=dx_strdup(workbook_core_payload_directory(other)),*owned_path=dx_strdup(path),*lease=dx_printf("%s/lease.lock",directory);
    workbook_core_payload_free(other); other=NULL;
    CHECK(!unlink(owned_path) && !unlink(lease) && !rmdir(directory)); free(directory); free(owned_path); free(lease);
    CHECK(workbook_core_payload_retire(base,NULL));
    same=false; CHECK(workbook_core_payload_equal(base,base,(WorkbookCoreCompareControl){0},&same,&error)==CORE_PAYLOAD_FAILED && !same && error);
    free(error); error=NULL; workbook_core_payload_free(base); base=NULL;
#endif
    bool output=true;
    CHECK(workbook_core_payload_equal(NULL,NULL,(WorkbookCoreCompareControl){0},&output,&error)==CORE_PAYLOAD_FAILED && output && error);
    free(error); workbook_free(&book); CHECK(!rmdir(parent));
    printf("PASS: %u complete core equality checks\n",checks); return 0;
}
