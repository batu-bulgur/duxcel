#include "versionpreviewview.h"
#include "../workbook/version_index.h"
#include <inttypes.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct { char *text; size_t length; } Definition;
static void append(Definition *d,const char *format,...) {
    va_list args,copy; va_start(args,format); va_copy(copy,args); int bytes=vsnprintf(NULL,0,format,copy); va_end(copy);
    if (bytes>=0) { d->text=dx_realloc(d->text,d->length+(size_t)bytes+1); vsnprintf(d->text+d->length,(size_t)bytes+1,format,args); d->length+=(size_t)bytes; }
    va_end(args);
}
static void source_definition(Definition *d,const Analysis *a) {
    static const char *kinds[]={"Authored worksheet cells","CSV / TSV","Parquet","DuckDB","Demo relation"};
    append(d,"Source: %s\n",(unsigned)a->kind<5 ? kinds[a->kind] : "Unavailable");
    if (a->path && *a->path) append(d,"Location: %s\n",a->path);
    if (a->table && *a->table) append(d,"Relation: %s%s%s\n",a->schema ? a->schema : "",a->schema && *a->schema ? "." : "",a->table);
    if (a->query && *a->query) append(d,"Query:\n%s\n",a->query);
    if (a->unique_rows) append(d,"Unique rows: enabled\n");
    if (a->filter_column) append(d,"Filter: %s · %s · %s\n",a->filter_column,filter_label(a->filter_op),a->filter_value ? a->filter_value : "");
    for (size_t i=0;i<a->filter_count;i++) append(d,"Filter: %s · %s · %s\n",a->filters[i].column,filter_label(a->filters[i].op),a->filters[i].value ? a->filters[i].value : "");
    if (a->sort_column) append(d,"Sort: %s · %s\n",a->sort_column,a->descending ? "descending" : "ascending");
    for (size_t i=0;i<a->sort_count;i++) append(d,"Sort: %s · %s\n",a->sorts[i].column,a->sorts[i].descending ? "descending" : "ascending");
    if (a->group_column) append(d,"Summary: %s · %s(%s)\n",a->group_column,aggregate_caption(a->aggregate),a->measure_column ? a->measure_column : "rows");
}
static char *definitions(const Worksheet *sheet) {
    Definition d={0}; append(&d,"Worksheet: %s%s\n\n",sheet->name,sheet->hidden ? " (hidden)" : ""); source_definition(&d,&sheet->analysis);
    append(&d,"\n%zu analytical object%s\n",sheet->objects.count,sheet->objects.count==1 ? "" : "s");
    const char *roles[]={"Rows","Columns","Values","Filters"};
    for (size_t i=0;i<sheet->objects.count;i++) {
        const SheetObject *o=sheet->objects.items+i;
        append(&d,"\n%s: %s\nPosition: row %" PRIu64 ", column %zu · %" PRIu64 " rows × %zu columns\n",o->kind==OBJECT_TABLE ? "Table" : o->kind==OBJECT_CHART ? "Chart" : "Pivot",o->name,o->row+1,o->col+1,o->rows,o->columns);
        if (o->kind==OBJECT_TABLE) source_definition(&d,&o->source);
        else {
            const SheetObject *table=objects_source(&sheet->objects,o);
            append(&d,"Based on: %s\n",table ? table->name : "worksheet source");
            for (size_t role=0;role<4;role++) for (size_t field=0;field<o->pivot.counts[role];field++) {
                const PivotField *f=o->pivot.fields[role]+field; char *label=pivot_field_label(f);
                if (role==PIVOT_VALUES) append(&d,"%s: %s(%s)\n",roles[role],aggregate_caption(f->aggregate),label);
                else if (role==PIVOT_FILTERS) append(&d,"%s: %s · %s · %s\n",roles[role],label,filter_label(f->filter),f->value ? f->value : "");
                else append(&d,"%s: %s\n",roles[role],label);
                free(label);
            }
            if (o->kind==OBJECT_CHART) {
                static const char *marks[]={"Bar","Line","Dot","Scatter"}; append(&d,"Marks: %s\n",(unsigned)o->mark<4 ? marks[o->mark] : "Unavailable");
                for (size_t role=0;role<DX_MARK_ROLES;role++) for (size_t j=0;j<o->marks.counts[role];j++) append(&d,"%s: %s\n",marks_label((MarkRole)role),o->marks.fields[role][j].name);
            }
        }
    }
    append(&d,"\nThese stored definitions are not executed in historical preview. Linked rows and analytical output values are not retained by the current version profile.\n"); return d.text;
}
void ui_history_free(UIVersionPreviewView *v) {
    if (!v) return;
    if (v->ready) grid_destroy(&v->grid);
    page_free(&v->empty); text_lines_free(&v->lines); free(v->definition_text); *v=(UIVersionPreviewView){0};
}
static void select_sheet(UIVersionPreviewView *v,const Workbook *book,size_t sheet) {
    bool show=v->definitions; ui_history_free(v); grid_init(&v->grid); v->ready=true; v->definitions=show; v->sheet=sheet;
    const Worksheet *s=book->sheets+sheet; const SheetView *saved=&s->view;
    grid_schema(&v->grid,&v->empty,true);
    size_t columns=saved->column_count>26 ? saved->column_count : 26;
    for (size_t i=0;i<s->cells.count;i++) {
        const SheetCell *cell=s->cells.cells+i;
        if (cell->col+1>columns) columns=cell->col+1;
        if (cell->row+1>v->grid.used_rows) v->grid.used_rows=cell->row+1;
    }
    for (size_t i=0;i<s->objects.count;i++) {
        const SheetObject *o=s->objects.items+i;
        if (o->col+o->columns>columns) columns=o->col+o->columns;
        if (o->row+o->rows>v->grid.used_rows) v->grid.used_rows=o->row+o->rows;
    }
    if (columns>16384) columns=16384;
    grid_ensure_columns(&v->grid,columns); v->grid.used_columns=columns;
    for (size_t i=0;i<saved->column_count;i++) grid_set_width(&v->grid,i,saved->widths[i]);
    v->grid.row=saved->row; v->grid.col=saved->col; v->grid.anchor_row=saved->anchor_row; v->grid.anchor_col=saved->anchor_col;
    v->grid.end_row=saved->end_row; v->grid.end_col=saved->end_col; v->grid.selection=(SelectionMode)saved->selection;
    v->grid.top=saved->top; v->grid.horizontal=saved->horizontal; v->grid.fraction=saved->fraction; v->grid.zoom=saved->canvas_zoom;
    v->grid.lock_structure=true; v->grid.allow_move=false; v->definition_text=definitions(s);
}
void ui_history_begin(UIVersionPreviewView *v,const Workbook *book) {
    ui_history_free(v); if (book && book->count) select_sheet(v,book,book->active);
}
static void authored_cells(UIContext *u,Grid *g,const Worksheet *sheet) {
    float scale=u->scale;
    for (int pass=0;pass<(g->frozen_columns ? 2 : 1);pass++) {
        size_t first=pass ? 0 : g->first_col,last=pass ? g->frozen_columns : g->width_count;
        Rectangle clip=grid_column_clip(g,scale,pass ? 0 : g->frozen_columns); ui_set_clip(u,clip);
        for (size_t column=first;column<last;column++) {
            float x=grid_column_x(g,scale,column); if (x>=clip.x+clip.width) break; if (x+g->widths[column]*scale<=clip.x) continue;
            for (unsigned row=0;row<g->render_rows && g->top+row<DX_SHEET_ROWS;row++) {
                if (objects_hit(&sheet->objects,g->top+row,column)>=0) continue;
                const char *text=cells_get(&sheet->cells,g->top+row,column); if (!*text) continue;
                Rectangle r=grid_range_rect(g,scale,g->top+row,column,1,1);
                ui_formatted_value(u,text,false,false,false,0,r,13,UI_TEXT,format_at(&sheet->formats,g->top+row,column));
            }
        }
    }
    ui_clear_clip(u);
}
static void object_placeholders(UIContext *u,Grid *grid,const Worksheet *sheet) {
    ui_set_clip(u,grid->body);
    for (size_t i=0;i<sheet->objects.count;i++) {
        const SheetObject *o=sheet->objects.items+i; Rectangle r=grid_range_rect(grid,u->scale,o->row,o->col,o->rows,o->columns);
        Rectangle visible=GetCollisionRec(r,grid->body); if (visible.width<=0 || visible.height<=0) continue;
        DrawRectangleRec(visible,UI_TINT); ui_border(r,grid->body,u->scale,UI_BORDER);
        Rectangle title={r.x+8*u->scale,r.y+5*u->scale,r.width-16*u->scale,20*u->scale};
        ui_label(u,o->name,title,12,UI_TEXT,false,UI_MEDIUM); title.y+=22*u->scale;
        ui_label(u,"Output unavailable · stored definition retained",title,10,UI_MUTED,false,UI_REGULAR);
    }
    ui_clear_clip(u);
}
UIHistoryAction ui_history_draw(UIContext *u,UIVersionPreviewView *v,const Workbook *book,const char *label,const char *message,bool input) {
    if (!v->ready || !book || !book->count || v->sheet>=book->count) return UI_HISTORY_NONE;
    float s=u->scale,w=GetScreenWidth(),h=GetScreenHeight(); const Worksheet *sheet=book->sheets+v->sheet;
    ClearBackground(UI_BG);
    Rectangle heading={16*s,10*s,fmaxf(40*s,w-304*s),30*s}; char title[DX_VERSION_LABEL_BYTES+64];
    snprintf(title,sizeof(title),"Preview · %s",label ? label : "Saved version"); ui_label(u,title,heading,16,UI_TEXT,false,UI_SEMIBOLD);
    v->back=(Rectangle){w-180*s,12*s,164*s,28*s}; v->versions=(Rectangle){w-280*s,12*s,92*s,28*s};
    bool blocked=u->pointer_blocked; u->pointer_blocked=blocked || !input;
    bool back=ui_button(u,v->back,"Working Head",true,true),versions=ui_button(u,v->versions,"Versions…",false,true);
    v->previous=(Rectangle){16*s,52*s,28*s,28*s}; v->next=(Rectangle){52*s,52*s,28*s,28*s};
    bool previous=ui_button(u,v->previous,"‹",false,book->count>1),next=ui_button(u,v->next,"›",false,book->count>1);
    v->definition=(Rectangle){w-132*s,52*s,116*s,28*s};
    if (ui_button(u,v->definition,v->definitions ? "Worksheet" : "Definitions",false,true)) v->definitions=!v->definitions;
    char name[4300]; snprintf(name,sizeof(name),"%s%s · %zu/%zu",sheet->name,sheet->hidden ? " (hidden)" : "",v->sheet+1,book->count);
    ui_label(u,name,(Rectangle){92*s,52*s,fmaxf(20*s,w-236*s),28*s},12,UI_TEXT,false,UI_MEDIUM);
    ui_label(u,"Read only · Your Working Head is kept unchanged.",(Rectangle){16*s,86*s,w-32*s,18*s},11,UI_MUTED,false,UI_REGULAR);
    v->canvas=(Rectangle){8*s,112*s,w-16*s,fmaxf(1,h-144*s)};
    if (previous || next) { size_t at=(v->sheet+book->count+(previous ? -1 : 1))%book->count; select_sheet(v,book,at); sheet=book->sheets+v->sheet; }
    if (v->definitions) {
        Rectangle area=v->canvas; area.x+=10*s; area.width-=20*s; DrawRectangleRec(v->canvas,UI_SURFACE);
        text_lines_layout_sized(&v->lines,u,v->definition_text,area.width,12);
        int visible=(int)fmaxf(1,floorf(area.height/(18*s))),maximum=v->lines.count>(size_t)visible ? (int)v->lines.count-visible : 0;
        if (input && CheckCollisionPointRec(ui_mouse_position(),area)) v->text_scroll+=ui_scroll_steps(&v->wheel,-GetMouseWheelMoveV().y*3);
        if (input && ui_pressed(KEY_DOWN)) v->text_scroll++;
        if (input && ui_pressed(KEY_UP)) v->text_scroll--;
        if (input && ui_pressed(KEY_HOME)) v->text_scroll=0;
        if (input && ui_pressed(KEY_END)) v->text_scroll=maximum;
        if (v->text_scroll<0) v->text_scroll=0;
        if (v->text_scroll>maximum) v->text_scroll=maximum;
        text_lines_draw_sized(&v->lines,u,v->definition_text,(size_t)v->text_scroll,area,12,18,UI_TEXT);
    } else {
        u->scale=s*v->grid.zoom; v->grid.formats=&sheet->formats; v->grid.format_row=v->grid.format_col=0;
        v->grid.lock_structure=true; v->grid.allow_move=false;
        grid_update(&v->grid,u,&v->empty,v->canvas,input); grid_draw(&v->grid,u,&v->empty,false);
        authored_cells(u,&v->grid,sheet); object_placeholders(u,&v->grid,sheet); u->scale=s;
    }
    const char *detail=message && *message ? message : sheet->analysis.kind!=SOURCE_NONE || sheet->objects.count ? "Linked rows / analytical outputs unavailable; see stored Definitions." : "Authored cells and formatting from the retained version.";
    ui_label(u,detail,(Rectangle){16*s,h-26*s,w-32*s,22*s},10,UI_MUTED,false,UI_REGULAR);
    u->pointer_blocked=blocked;
    return back ? UI_HISTORY_RETURN : versions ? UI_HISTORY_VERSIONS : UI_HISTORY_NONE;
}
