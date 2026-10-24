#include "workbook/definition_baseline.h"
#include <stdio.h>
#include <stdlib.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL definition baseline:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
int main(void) {
    Analysis a={0}; DefinitionDependencyView view={.primary=&a};
    CHECK(definition_baseline_status((DefinitionDependencyView){0})==DEFINITION_BASELINE_READY);
    for (unsigned kind=SOURCE_NONE;kind<=SOURCE_DEMO;kind++) {
        a.kind=kind; a.path="/source";
        CHECK(definition_baseline_status(view)==(kind==SOURCE_DATABASE ? DEFINITION_BASELINE_UNRESOLVED : DEFINITION_BASELINE_READY));
    }
    a.kind=SOURCE_DATABASE; a.path=NULL; CHECK(definition_baseline_status(view)==DEFINITION_BASELINE_INVALID);
    a.path=""; CHECK(definition_baseline_status(view)==DEFINITION_BASELINE_INVALID);
    a.path="/source"; a.schema="placeholder"; CHECK(definition_baseline_status(view)==DEFINITION_BASELINE_UNRESOLVED);
    a.table=""; CHECK(definition_baseline_status(view)==DEFINITION_BASELINE_INVALID);
    a.table="NULL"; a.schema=NULL; CHECK(definition_baseline_status(view)==DEFINITION_BASELINE_INVALID);
    a.schema=""; CHECK(definition_baseline_status(view)==DEFINITION_BASELINE_INVALID);
    a.schema="main"; CHECK(definition_baseline_status(view)==DEFINITION_BASELINE_READY);
    a.filter_number=(NumberType){NUM_DECIMAL,38,4}; CHECK(definition_baseline_status(view)==DEFINITION_BASELINE_READY);
    a.kind=(SourceKind)-1; CHECK(definition_baseline_status(view)==DEFINITION_BASELINE_INVALID);
    a.kind=SOURCE_DEMO;
    SheetObject items[3]={{.kind=OBJECT_TABLE,.source={.kind=SOURCE_DATABASE,.path="/one"}},
        {.kind=OBJECT_PIVOT,.rows=UINT64_MAX,.columns=SIZE_MAX}, {.kind=OBJECT_TABLE,.source={.kind=SOURCE_PARQUET,.path="/two"}}};
    ObjectSnapshot objects={items,3}; view.objects=&objects;
    CHECK(definition_baseline_status(view)==DEFINITION_BASELINE_UNRESOLVED);
    items[0].source.schema="main"; items[0].source.table="Sales";
    CHECK(definition_baseline_status(view)==DEFINITION_BASELINE_READY);
    a.kind=SOURCE_DATABASE; a.table=NULL; CHECK(definition_baseline_status(view)==DEFINITION_BASELINE_UNRESOLVED);
    items[2].source.kind=(SourceKind)99; CHECK(definition_baseline_status(view)==DEFINITION_BASELINE_INVALID);
    items[2].source.kind=SOURCE_PARQUET; a.kind=SOURCE_DEMO;
    items[1].kind=(ObjectKind)-1; CHECK(definition_baseline_status(view)==DEFINITION_BASELINE_INVALID);
    items[1].kind=OBJECT_CHART; CHECK(definition_baseline_status(view)==DEFINITION_BASELINE_READY);
    objects.count=DX_MAX_OBJECTS+1; CHECK(definition_baseline_status(view)==DEFINITION_BASELINE_INVALID);
    objects.count=1; objects.items=NULL; CHECK(definition_baseline_status(view)==DEFINITION_BASELINE_INVALID);
    objects.count=0; CHECK(definition_baseline_status(view)==DEFINITION_BASELINE_READY);
    printf("PASS: %u definition baseline readiness checks\n",checks); return 0;
}
