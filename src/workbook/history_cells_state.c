#include "history_cells_state.h"
#include <string.h>
static const Worksheet *sheet(const Workbook *book,uint64_t id) {
    for (size_t i=0;i<book->count;i++) if (book->sheets[i].id==id) return book->sheets+i;
    return NULL;
}
static WorkbookHistoryCellsStateStatus measure_path(const WorkbookHistoryProfileView *v,const Workbook *book,size_t *bytes) {
    if (!v || !book || !bytes || !workbook_identity_valid(book) || v->document.high!=book->id.high || v->document.low!=book->id.low)
        return HISTORY_CELLS_STATE_INVALID;
    if (v->kind!=HISTORY_PROFILE_RETAINED || !v->current.path) return HISTORY_CELLS_STATE_UNSUPPORTED;
    if (v->current.count>DX_HISTORY_PROFILE_RECORDS || v->current.count>SIZE_MAX/sizeof(WorkbookHistoryCellStateEntry)) return HISTORY_CELLS_STATE_LIMIT;
    for (size_t i=0;i<v->current.count;i++) {
        const WorkbookHistoryProfileRecord *r=v->records+v->current.records[i];
        if (r->event->family!=DX_HISTORY_CELL_FAMILY) return HISTORY_CELLS_STATE_UNSUPPORTED;
        if (!sheet(book,r->sheet)) return HISTORY_CELLS_STATE_MISMATCH;
    }
    *bytes=v->current.count*sizeof(WorkbookHistoryCellStateEntry); return HISTORY_CELLS_STATE_OK;
}
WorkbookHistoryCellsStateStatus workbook_history_cells_state_measure(const WorkbookHistoryProfile *profile,const Workbook *book,size_t *bytes) {
    return measure_path(workbook_history_profile_view(profile),book,bytes);
}
static int key_compare(uint64_t sheet_id,uint64_t row,size_t column,const WorkbookHistoryCellStateEntry *entry) {
    if (sheet_id!=entry->sheet) return sheet_id<entry->sheet ? -1 : 1;
    if (row!=entry->row) return row<entry->row ? -1 : 1;
    return column==entry->column ? 0 : column<entry->column ? -1 : 1;
}
static size_t locate(const WorkbookHistoryCellStateEntry *entries,size_t count,uint64_t sheet_id,uint64_t row,size_t column) {
    size_t lo=0,hi=count;
    while (lo<hi) { size_t m=lo+(hi-lo)/2; if (key_compare(sheet_id,row,column,entries+m)>0) lo=m+1; else hi=m; }
    return lo;
}
static bool walk(const WorkbookHistoryProfileView *v,const Workbook *book,WorkbookHistoryCellStateEntry *scratch,
    size_t from,size_t to,bool redo,size_t *touched) {
    size_t count=0;
    for (size_t step=from;redo ? step<to : step>to;) {
        size_t i=redo ? step++ : --step;
        const WorkbookHistoryProfileRecord *r=v->records+v->current.records[i]; CellDelta d={0};
        if (cell_delta_decode(r->data,r->bytes,&d)!=CELL_DELTA_CODEC_OK) return false;
        size_t found=locate(scratch,count,d.sheet,d.row,d.column);
        bool present=found<count && !key_compare(d.sheet,d.row,d.column,scratch+found);
        const char *value; size_t bytes;
        if (present) { value=scratch[found].value; bytes=scratch[found].bytes; }
        else { value=cells_get(&sheet(book,d.sheet)->cells,d.row,d.column); bytes=strlen(value); }
        const char *expected=redo ? d.before : d.after; size_t expected_bytes=redo ? d.before_bytes : d.after_bytes;
        if (bytes!=expected_bytes || (bytes && memcmp(value,expected,bytes))) return false;
        if (!present) { memmove(scratch+found+1,scratch+found,(count-found)*sizeof(*scratch)); count++; }
        scratch[found]=(WorkbookHistoryCellStateEntry){d.sheet,d.row,d.column,redo ? d.after_bytes : d.before_bytes,redo ? d.after : d.before};
    }
    *touched=count; return true;
}
WorkbookHistoryCellsStateStatus workbook_history_cells_state_check(const WorkbookHistoryProfile *profile,const Workbook *actual,
    WorkbookHistoryCellStateEntry *scratch,size_t reserved,WorkbookHistoryCellsStateReport *out) {
    if (!out) return HISTORY_CELLS_STATE_INVALID;
    size_t bytes=0; WorkbookHistoryCellsStateStatus status=workbook_history_cells_state_measure(profile,actual,&bytes);
    if (status!=HISTORY_CELLS_STATE_OK) return status;
    if (reserved<bytes) return HISTORY_CELLS_STATE_LIMIT;
    if (bytes && !scratch) return HISTORY_CELLS_STATE_INVALID;
    const WorkbookHistoryProfileView *v=workbook_history_profile_view(profile); const WorkbookHistoryPath *p=v->current.path;
    size_t cursor=(size_t)(p->cursor-p->available);
    WorkbookHistoryCellsStateReport next={.undo=cursor,.redo=v->current.count-cursor};
    if (!walk(v,actual,scratch,cursor,0,false,&next.undo_cells) ||
        !walk(v,actual,scratch,cursor,v->current.count,true,&next.redo_cells)) return HISTORY_CELLS_STATE_MISMATCH;
    *out=next; return HISTORY_CELLS_STATE_OK;
}

static const char *projected(const Worksheet *s,const WorkbookHistoryCellStateEntry *entries,size_t count,uint64_t row,size_t column,size_t *bytes) {
    size_t at=locate(entries,count,s->id,row,column);
    if (at<count && !key_compare(s->id,row,column,entries+at)) { *bytes=entries[at].bytes; return entries[at].value; }
    const char *value=cells_get(&s->cells,row,column); *bytes=strlen(value); return value;
}
static bool coordinate(const Worksheet *a,const Worksheet *b,const WorkbookHistoryCellStateEntry *left,size_t ln,
    const WorkbookHistoryCellStateEntry *right,size_t rn,uint64_t row,size_t column) {
    size_t an=0,bn=0; const char *av=projected(a,left,ln,row,column,&an),*bv=projected(b,right,rn,row,column,&bn);
    return an==bn && (!an || !memcmp(av,bv,an));
}
static WorkbookHistoryCellsStateStatus saved_views(const WorkbookHistoryProfile *profile,const Workbook *current,const Workbook *saved,
    WorkbookHistoryProfileView *cv,WorkbookHistoryProfileView *sv,uint64_t *saved_position,uint64_t *fork,size_t *bytes) {
    const WorkbookHistoryProfileView *v=workbook_history_profile_view(profile);
    if (!v || !v->versions->saved.present || !v->saved.path || !bytes) return HISTORY_CELLS_STATE_UNSUPPORTED;
    *cv=*v; *sv=*v; sv->current=v->saved;
    size_t cn=0,sn=0; WorkbookHistoryCellsStateStatus status=measure_path(cv,current,&cn);
    if (status!=HISTORY_CELLS_STATE_OK) return status;
    status=measure_path(sv,saved,&sn); if (status!=HISTORY_CELLS_STATE_OK) return status;
    if (cn>SIZE_MAX-sn) return HISTORY_CELLS_STATE_LIMIT;
    bool found=false;
    for (size_t i=0;i<v->anchors->count;i++) if (v->anchors->items[i].boundary.anchor==v->anchors->saved_anchor) {
        *saved_position=v->anchors->items[i].boundary.position; found=true; break;
    }
    *fork=workbook_history_profile_protection(profile)->shared_through;
    const WorkbookHistoryPath *p=sv->current.path;
    if (!found || *saved_position<p->available || *saved_position>p->tail || *fork<p->available || *fork>p->tail ||
        *fork<cv->current.path->available || *fork>cv->current.path->tail) return HISTORY_CELLS_STATE_INVALID;
    *bytes=cn+sn; return HISTORY_CELLS_STATE_OK;
}
WorkbookHistoryCellsStateStatus workbook_history_cells_saved_measure(const WorkbookHistoryProfile *profile,const Workbook *current,const Workbook *saved,size_t *bytes) {
    WorkbookHistoryProfileView cv,sv; uint64_t position=0,fork=0; size_t n=0;
    WorkbookHistoryCellsStateStatus status=saved_views(profile,current,saved,&cv,&sv,&position,&fork,&n);
    if (status==HISTORY_CELLS_STATE_OK) { if (!bytes) return HISTORY_CELLS_STATE_INVALID; *bytes=n; }
    return status;
}
WorkbookHistoryCellsStateStatus workbook_history_cells_saved_check(const WorkbookHistoryProfile *profile,const Workbook *current,const Workbook *saved,
    WorkbookHistoryCellStateEntry *scratch,size_t reserved,WorkbookHistoryCellsSavedReport *out) {
    if (!out) return HISTORY_CELLS_STATE_INVALID;
    WorkbookHistoryProfileView cv,sv; uint64_t position=0,fork=0; size_t bytes=0;
    WorkbookHistoryCellsStateStatus status=saved_views(profile,current,saved,&cv,&sv,&position,&fork,&bytes);
    if (status!=HISTORY_CELLS_STATE_OK) return status;
    if (reserved<bytes) return HISTORY_CELLS_STATE_LIMIT;
    if (bytes && !scratch) return HISTORY_CELLS_STATE_INVALID;
    WorkbookHistoryCellStateEntry *right=scratch ? scratch+cv.current.count : NULL;
    size_t cc=(size_t)(cv.current.path->cursor-cv.current.path->available),sc=(size_t)(position-sv.current.path->available);
    WorkbookHistoryCellsSavedReport next={.shared_position=fork,.current={.undo=cc,.redo=cv.current.count-cc},.saved={.undo=sc,.redo=sv.current.count-sc}};
    if (!walk(&cv,current,scratch,cc,0,false,&next.current.undo_cells) || !walk(&cv,current,scratch,cc,cv.current.count,true,&next.current.redo_cells) ||
        !walk(&sv,saved,right,sc,0,false,&next.saved.undo_cells) || !walk(&sv,saved,right,sc,sv.current.count,true,&next.saved.redo_cells)) return HISTORY_CELLS_STATE_MISMATCH;
    size_t ln=0,rn=0,cf=(size_t)(fork-cv.current.path->available),sf=(size_t)(fork-sv.current.path->available);
    if (!walk(&cv,current,scratch,cc,cf,cc<cf,&ln) || !walk(&sv,saved,right,sc,sf,sc<sf,&rn)) return HISTORY_CELLS_STATE_MISMATCH;
    if (current->count!=saved->count) return HISTORY_CELLS_STATE_MISMATCH;
    for (size_t i=0;i<current->count;i++) {
        const Worksheet *a=current->sheets+i,*b=sheet(saved,a->id); if (!b) return HISTORY_CELLS_STATE_MISMATCH;
        const CellStore *stores[2]={&a->cells,&b->cells};
        for (unsigned side=0;side<2;side++) for (size_t j=0;j<stores[side]->count;j++) {
            const SheetCell *cell=stores[side]->cells+j;
            if (!coordinate(a,b,scratch,ln,right,rn,cell->row,cell->col)) return HISTORY_CELLS_STATE_MISMATCH;
            next.coordinate_checks++;
        }
        const WorkbookHistoryCellStateEntry *entries[2]={scratch,right}; size_t counts[2]={ln,rn};
        for (unsigned side=0;side<2;side++) for (size_t j=0;j<counts[side];j++) if (entries[side][j].sheet==a->id) {
            if (!coordinate(a,b,scratch,ln,right,rn,entries[side][j].row,entries[side][j].column)) return HISTORY_CELLS_STATE_MISMATCH;
            next.coordinate_checks++;
        }
    }
    *out=next; return HISTORY_CELLS_STATE_OK;
}
