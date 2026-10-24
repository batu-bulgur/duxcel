#include "workbook/object_import_draft.h"
#include "workbook/history.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL object import draft:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static Workbook book(void) { Workbook b; workbook_init(&b); Analysis empty={0}; CHECK(workbook_add(&b,"Manual",&empty,NULL)); b.dirty=false; return b; }
int main(void) {
    Workbook b=book(); uint64_t request=7,epoch=1,rows=0; size_t columns=0; bool pending=false;
    ObjectCreateContext c={.command={&b,&request,SIZE_MAX,SIZE_MAX,true,false},.primary=&b.sheets[0].analysis,.primary_epoch=&epoch,.source_rows=&rows,.source_columns=&columns,.source_count_pending=&pending,.manual_formats=true};
    ObjectImportDraft draft={0}; Analysis source={.kind=SOURCE_DATABASE,.path="/database.duckdb"};
    CHECK(!object_import_draft_begin(&c,0,6,20,10,&source,true,&draft,NULL) && !draft.active);
    CHECK(object_import_draft_begin(&c,0,7,20,10,&source,true,&draft,NULL));
    CHECK(draft.active && draft.source.path!=source.path && !strcmp(draft.source.path,source.path) && draft.target.owner.request_epoch==7 &&
        !b.sheets[0].objects.count && !b.sheets[0].objects.next_id && !b.sheets[0].undo_actions_count && !b.command_revision && !b.dirty);
    Analysis resolved=source; resolved.schema="main"; resolved.table="Sales"; ObjectCreateCommand command={.row=123};
    CHECK(!object_import_draft_command(&draft,6,20,10,&resolved,&command,NULL) && command.row==123);
    CHECK(!object_import_draft_command(&draft,7,21,10,&resolved,&command,NULL) && command.row==123);
    CHECK(!object_import_draft_command(&draft,7,20,11,&resolved,&command,NULL) && command.row==123);
    CHECK(!object_import_draft_command(&draft,7,20,10,&source,&command,NULL) && command.row==123);
    resolved.path="/other.duckdb"; CHECK(!object_import_draft_command(&draft,7,20,10,&resolved,&command,NULL)); resolved.path=source.path;
    resolved.unique_rows=true; CHECK(!object_import_draft_command(&draft,7,20,10,&resolved,&command,NULL)); resolved.unique_rows=false;
    resolved.table=""; CHECK(!object_import_draft_command(&draft,7,20,10,&resolved,&command,NULL)); resolved.table="Sales";
    CHECK(object_import_draft_command(&draft,7,20,10,&resolved,&command,NULL) && command.kind==OBJECT_TABLE && command.source==&resolved && command.row==20 && command.column==10 && !strcmp(command.name,"Table 1"));
    CHECK(workbook_apply_object_create_recorded(&c,&draft.target,command,NULL,NULL).status==OBJECT_CREATE_APPLIED);
    CHECK(b.command_revision==1 && b.sheets[0].objects.count==1 && b.sheets[0].objects.next_id==1 && b.sheets[0].formats.source_aware && b.sheets[0].undo_actions[0]==EDIT_OBJECTS_FORMAT &&
        !strcmp(b.sheets[0].objects.items[0].source.table,"Sales") && b.sheets[0].objects.items[0].source.table!=resolved.table);
    object_import_draft_free(&draft); CHECK(!draft.active && !draft.source.path && b.sheets[0].objects.count==1); workbook_free(&b);
    b=book(); c.command.book=&b; c.primary=&b.sheets[0].analysis;
    CHECK(object_import_draft_begin(&c,0,7,20,10,&source,true,&draft,NULL));
    CHECK(object_import_draft_command(&draft,7,20,10,&resolved,&command,NULL));
    request++; CHECK(workbook_apply_object_create_recorded(&c,&draft.target,command,NULL,NULL).status==OBJECT_CREATE_STALE && !b.sheets[0].objects.count && !b.dirty);
    object_import_draft_free(&draft); CHECK(!b.sheets[0].undo_actions_count); workbook_free(&b);
    printf("PASS: %u pending object import checks\n",checks); return 0;
}
