#include "workbook/source_format_baseline.h"
#include "workbook/history.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL source format baseline:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static Workbook book(void) {
    Workbook b; workbook_init(&b); Analysis a={.kind=SOURCE_CSV,.path="/source"}; CHECK(workbook_add(&b,"Loaded",&a,NULL)); Worksheet *s=b.sheets;
    CellFormat style=format_default(); style.flags=FORMAT_BOLD;
    CHECK(format_apply(&s->formats,(FormatRange){2,2,0,0},FORMAT_BOLD,style,NULL)); worksheet_record(s,EDIT_FORMAT);
    CHECK(cells_set(&s->cells,8,5,"NULL",NULL)); worksheet_record(s,EDIT_CELLS);
    CHECK(cells_set(&s->cells,9,5,"9007199254740993",NULL)); worksheet_record(s,EDIT_CELLS);
    CHECK(cells_set(&s->cells,10,5,"12345678901234567890.123456789",NULL)); worksheet_record(s,EDIT_CELLS);
    style.flags=FORMAT_ITALIC; CHECK(format_apply(&s->formats,(FormatRange){3,3,0,0},FORMAT_ITALIC,style,NULL)); worksheet_record(s,EDIT_FORMAT);
    workbook_history_note_edit(&b); CHECK(format_history(&s->formats,false)); worksheet_transfer_history(s,false); b.dirty=false;
    CellEditTarget target; CHECK(workbook_prepare_cell_edit(&b,0,0,0,&target,NULL)); return b;
}
static SourceVersion entry(SourceFormatBaselineContext *c,SourcePacket *result) {
    Analysis *a=c->command.primary; SourceVersion e={.version=c->command.book->sheets[0].source_versions.high_water+1,.kind=a->kind,.path=a->path};
    Column col={.name="Id",.type=DUCKDB_TYPE_BIGINT,.type_name="BIGINT",.numeric=true,.number_type={NUM_I64,0,0}};
    Page schema={.columns=&col,.column_count=1}; SourceDigest digest={.bytes=42}; digest.identity.size=42; digest.sha256[0]=1;
    CHECK(source_scope_raw_build(0,e.version,a,&schema,&digest,"engine",&e.raw,NULL)); CHECK(source_scope_result_build(&e.raw,a,result,NULL)); return e;
}
static void basic(bool external) {
    Workbook b=book(); Worksheet *s=b.sheets; Analysis live=analysis_copy(&s->analysis); uint64_t request=10,origin=50;
    SourceFormatBaselineContext c={{&b,external ? &live : &s->analysis,&request,SIZE_MAX,true,false},&origin,true}; SourcePacket result={0}; SourceVersion e=entry(&c,&result);
    Workbook *saved=workbook_snapshot(&b); CHECK(saved && !saved->sheets[0].formats.source_aware);
    Workbook before=b; Worksheet sheet=*s; CellStore cells=s->cells; Analysis primary=*c.command.primary; SourceFormatBaselinePlan *p=NULL; SourceFormatBaselineInfo info={0};
    CHECK(workbook_prepare_source_format_baseline(&c,0,&e,&result,source_format_baseline_reservation(),&p,&info,NULL)==SOURCE_FORMAT_COMMAND_READY && p && info.version==1 && info.owned<source_format_baseline_reservation() && info.validation_scratch>0);
    CHECK(!memcmp(&before,&b,sizeof(b)) && !memcmp(&sheet,s,sizeof(sheet)) && !memcmp(&primary,c.command.primary,sizeof(primary)));
    source_packet_free(&e.raw); source_packet_free(&result);
    CHECK(workbook_apply_source_format_baseline(&c,&p,NULL)==SOURCE_FORMAT_COMMAND_APPLIED && !p);
    CHECK(b.dirty && b.command_revision==before.command_revision && b.history_branch==before.history_branch && s->history_last==sheet.history_last &&
        s->history_floor==sheet.history_floor && s->history_ceiling==sheet.history_ceiling && s->undo_actions_count==sheet.undo_actions_count && s->redo_actions_count==sheet.redo_actions_count &&
        !memcmp(s->undo_actions,sheet.undo_actions,sizeof(s->undo_actions)) && !memcmp(s->redo_actions,sheet.redo_actions,sizeof(s->redo_actions)) &&
        !memcmp(s->undo_order,sheet.undo_order,sizeof(s->undo_order)) && !memcmp(s->redo_order,sheet.redo_order,sizeof(s->redo_order)) && !memcmp(&s->cells,&cells,sizeof(cells)));
    CHECK(s->formats.revision==sheet.formats.revision+1 && s->formats.source_aware && s->formats.undo_count==sheet.formats.undo_count && s->formats.redo_count==sheet.formats.redo_count &&
        s->source_versions.high_water==1 && s->source_versions.count==1 && c.command.primary->source_version==1 && c.command.primary->path==primary.path);
    if (external) CHECK(!s->analysis.source_version);
    CHECK(s->formats.items[0].selector==FORMAT_GUARDED && s->formats.redo[0].items[1].selector==FORMAT_GUARDED && s->formats.redo[0].source_aware && s->formats.undo[0].source_aware);
    FormatScopeView scope={0,s->formats.scopes[0].definition,s->formats.scopes[0].bytes}; CHECK(format_resolve(&s->formats,2,0,&scope).flags&FORMAT_BOLD);
    CHECK(!(format_resolve_token(&s->formats,2,0,SIZE_MAX).flags&FORMAT_BOLD) && (format_at(&s->formats,2,0).flags&FORMAT_BOLD));
    CHECK(!saved->sheets[0].formats.source_aware && saved->sheets[0].formats.items[0].selector==FORMAT_SPATIAL && !saved->sheets[0].analysis.source_version && !saved->sheets[0].source_versions.count);
    CHECK(workbook_history_step(&b,true,NULL).status==WORKBOOK_HISTORY_APPLIED && (format_resolve(&s->formats,3,0,&scope).flags&FORMAT_ITALIC));
    source_packet_free(&e.raw); source_packet_free(&result); analysis_free(&live); workbook_free(saved); free(saved); workbook_free(&b);
}
static void reject_prepare(SourceFormatBaselineContext *c,const SourceVersion *e,const SourcePacket *result,size_t reserved,SourceFormatCommandStatus expected) {
    Workbook before=*c->command.book; Worksheet sheet=c->command.book->sheets[0]; Analysis primary=*c->command.primary; SourceFormatBaselinePlan *p=NULL; SourceFormatBaselineInfo info={7,8,9},original=info; char *error=NULL;
    CHECK(workbook_prepare_source_format_baseline(c,0,e,result,reserved,&p,&info,&error)==expected && !p && error && !memcmp(&info,&original,sizeof(info)) &&
        !memcmp(&before,c->command.book,sizeof(before)) && !memcmp(&sheet,c->command.book->sheets,sizeof(sheet)) && !memcmp(&primary,c->command.primary,sizeof(primary))); free(error);
}
static void failures(void) {
    Workbook b=book(); uint64_t request=1,origin=5; SourceFormatBaselineContext c={{&b,&b.sheets[0].analysis,&request,SIZE_MAX,true,false},&origin,true}; SourcePacket result={0}; SourceVersion e=entry(&c,&result);
    c.origin_open=false; reject_prepare(&c,&e,&result,SIZE_MAX,SOURCE_FORMAT_COMMAND_PROTECTED); c.origin_open=true;
    c.command.validation_reserved=0; reject_prepare(&c,&e,&result,SIZE_MAX,SOURCE_FORMAT_COMMAND_LIMIT); c.command.validation_reserved=SIZE_MAX;
    reject_prepare(&c,&e,&result,source_format_baseline_reservation()-1,SOURCE_FORMAT_COMMAND_LIMIT);
    e.owner=1; reject_prepare(&c,&e,&result,SIZE_MAX,SOURCE_FORMAT_COMMAND_INVALID); e.owner=0;
    e.version++; reject_prepare(&c,&e,&result,SIZE_MAX,SOURCE_FORMAT_COMMAND_INVALID); e.version--;
    e.path="/other"; reject_prepare(&c,&e,&result,SIZE_MAX,SOURCE_FORMAT_COMMAND_INVALID); e.path=c.command.primary->path;
    result.data[0]='X'; reject_prepare(&c,&e,&result,SIZE_MAX,SOURCE_FORMAT_COMMAND_INVALID); result.data[0]='D';
    uint64_t format_revision=b.sheets[0].formats.revision; b.sheets[0].formats.revision=UINT64_MAX; reject_prepare(&c,&e,&result,SIZE_MAX,SOURCE_FORMAT_COMMAND_LIMIT); b.sheets[0].formats.revision=format_revision;
    source_packet_free(&e.raw); source_packet_free(&result); workbook_free(&b);
}
static void apply_fences(void) {
    for (unsigned mode=0;mode<6;mode++) {
        Workbook b=book(); uint64_t request=1,origin=5; SourceFormatBaselineContext c={{&b,&b.sheets[0].analysis,&request,SIZE_MAX,true,false},&origin,true}; SourcePacket result={0}; SourceVersion e=entry(&c,&result);
        SourceFormatBaselinePlan *p=NULL; SourceFormatBaselineInfo info={0}; CHECK(workbook_prepare_source_format_baseline(&c,0,&e,&result,SIZE_MAX,&p,&info,NULL)==SOURCE_FORMAT_COMMAND_READY);
        if (mode==0) origin++;
        if (mode==1) c.origin_open=false;
        if (mode==2) request++;
        if (mode==3) b.sheets[0].formats.revision++;
        if (mode==4) c.command.ready=false;
        if (mode==5) b.command_revision++;
        Workbook before=b; Worksheet sheet=b.sheets[0]; SourceFormatBaselinePlan *identity=p; char *error=NULL;
        CHECK(workbook_apply_source_format_baseline(&c,&p,&error)==(mode==1 || mode==4 ? SOURCE_FORMAT_COMMAND_PROTECTED : SOURCE_FORMAT_COMMAND_STALE) && p==identity && error &&
            !memcmp(&before,&b,sizeof(before)) && !memcmp(&sheet,b.sheets,sizeof(sheet))); free(error);
        if (mode==4) { c.command.ready=true; CHECK(workbook_apply_source_format_baseline(&c,&p,NULL)==SOURCE_FORMAT_COMMAND_APPLIED && !p); }
        source_format_baseline_plan_free(&p); source_packet_free(&e.raw); source_packet_free(&result); workbook_free(&b);
    }
}
int main(void) { basic(false); basic(true); failures(); apply_fences(); source_format_baseline_plan_free(NULL); printf("PASS: %u atomic source format baseline checks\n",checks); return 0; }
