#include "grid.h"
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))
static const Color GRID_LINE={228,232,236,255};
static float clamp(float x,float lo,float hi) { return fmaxf(lo,fminf(hi,x)); }
static uint64_t after(uint64_t row,uint64_t margin) { return row>=DX_SHEET_ROWS-margin ? DX_SHEET_ROWS : row+margin; }
static uint64_t canvas_rows(const Grid *g,const Page *p) {
    if (g->bounded) return MAX(UINT64_C(1),p->total);
    if (g->scrollbar && g->scroll_rows) return g->scroll_rows;
    uint64_t used=MAX(p->total,g->used_rows);
    used=MAX(used,after(g->row,1)); used=MAX(used,after(g->anchor_row,1)); used=MAX(used,after(g->end_row,1));
    used=MAX(used,after(g->top,g->render_rows));
    uint64_t margin=MAX(UINT64_C(100),UINT64_C(4)*g->visible_rows);
    return MAX(DX_CANVAS_MIN_ROWS,after(used,margin));
}
static size_t column_at(const Grid *g,float x) {
    size_t lo=0,hi=g->width_count;
    while (lo<hi) { size_t mid=lo+(hi-lo)/2; if (g->offsets[mid+1]<=x) lo=mid+1; else hi=mid; }
    return lo;
}
float grid_column_x(const Grid *g,float s,size_t column) {
    if (column>g->width_count) return g->body.x;
    return g->body.x+(g->offsets[column]+(column<g->frozen_columns ? 0 : g->elastic.x-g->horizontal))*s;
}
Rectangle grid_column_clip(const Grid *g,float s,size_t column) {
    Rectangle clip=g->body;
    if (g->frozen_columns) {
        if (column<g->frozen_columns) clip.width=fminf(clip.width,g->frozen_width*s);
        else { clip.x+=g->frozen_width*s; clip.width=fmaxf(0,clip.width-g->frozen_width*s); }
    }
    return clip;
}
static float content_at(const Grid *g,float s,float x) {
    float local=(x-g->body.x)/s;
    return g->frozen_columns && local<g->frozen_width ? local : local+g->horizontal-g->elastic.x;
}
static float row_height(const Grid *g) { return g->row_height>=18 && g->row_height<=48 ? g->row_height : 27; }
void grid_init(Grid *g) { *g=(Grid){0}; g->zoom=1; g->elastic_enabled=true; g->resizing=g->last_edge=-1; g->zebra=true; }
GridLocation grid_location(const Grid *g) {
    return (GridLocation){.row=g->row,.anchor_row=g->anchor_row,.end_row=g->end_row,.top=g->top,
        .col=g->col,.anchor_col=g->anchor_col,.end_col=g->end_col,.horizontal=g->horizontal,
        .fraction=g->fraction,.selection=g->selection,.valid=true};
}
bool grid_restore_location(Grid *g,const GridLocation *v) {
    if (!v || !v->valid || v->row>=DX_SHEET_ROWS || v->anchor_row>=DX_SHEET_ROWS || v->end_row>=DX_SHEET_ROWS || v->top>=DX_SHEET_ROWS ||
        v->col>=16384 || v->anchor_col>=16384 || v->end_col>=16384 || !isfinite(v->horizontal) || v->horizontal<0 ||
        !isfinite(v->fraction) || v->fraction<0 || v->fraction>=1 || v->selection<SELECT_CELLS || v->selection>SELECT_ALL) return false;
    grid_ensure_columns(g,MAX(v->col,MAX(v->anchor_col,v->end_col))+1);
    g->row=v->row; g->anchor_row=v->anchor_row; g->end_row=v->end_row; g->top=v->top;
    g->col=v->col; g->anchor_col=v->anchor_col; g->end_col=v->end_col;
    g->horizontal=v->horizontal; g->fraction=v->fraction; g->selection=v->selection;
    g->elastic=g->spring_velocity=(Vector2){0}; g->dragging=g->moving=g->context_requested=g->move_requested=false;
    g->resizing=-1; g->scrollbar=0; return true;
}
void grid_destroy(Grid *g) {
    for (size_t c=0;c<g->width_count;c++) free(g->column_names[c]);
    free(g->column_names); free(g->widths); free(g->offsets); grid_init(g);
}
void grid_set_width(Grid *g,size_t col,float value) {
    if (col>=g->width_count) return;
    float next=clamp(value,52,1200), delta=next-g->widths[col]; if (!delta) return; g->widths[col]=next; g->width_revision++;
    for (size_t c=col+1;c<=g->width_count;c++) g->offsets[c]+=delta;
}
void grid_ensure_columns(Grid *g,size_t count) {
    count=MIN(count,16384); if (g->width_count>=count) return;
    size_t old=g->width_count,next=MIN(16384,old+((count-old+51)/52)*52);
    g->widths=dx_realloc(g->widths,next*sizeof(float)); g->offsets=dx_realloc(g->offsets,(next+1)*sizeof(float));
    g->column_names=dx_realloc(g->column_names,next*sizeof(char *));
    if (!old) g->offsets[0]=0;
    for (size_t c=old;c<next;c++) { g->column_names[c]=NULL; g->widths[c]=96; g->offsets[c+1]=g->offsets[c]+96; }
    g->width_count=next;
}
static void extend_columns(Grid *g) { grid_ensure_columns(g,g->width_count+52); }
void grid_schema(Grid *g,const Page *p,bool reset) {
    bool same=g->data_columns==p->column_count && g->width_count>=(g->bounded ? p->column_count : 52);
    for (size_t c=0;same && c<p->column_count;c++) same=!strcmp(g->column_names[c],p->columns[c].name);
    if (reset || !same) {
        float zoom=g->zoom,density=g->row_height; bool hidden=g->hide_gridlines,bounded=g->bounded,selection=g->hide_selection,freeze=g->freeze; grid_destroy(g); g->zoom=zoom; g->row_height=density; g->hide_gridlines=hidden; g->bounded=bounded; g->hide_selection=selection; g->freeze=freeze; g->data_columns=p->column_count; g->width_count=MIN(16384,bounded ? MAX(1,p->column_count) : MAX(52,p->column_count+16));
        g->widths=dx_alloc(g->width_count*sizeof(float)); g->offsets=dx_alloc((g->width_count+1)*sizeof(float));
        g->column_names=dx_alloc(g->width_count*sizeof(char *));
        for (size_t c=0;c<g->width_count;c++) {
            g->column_names[c]=c<p->column_count ? dx_strdup(p->columns[c].name) : NULL;
            g->widths[c]=c<p->column_count ? (p->columns[c].type==DUCKDB_TYPE_TIMESTAMP ? 190 : 128) : 96;
            g->offsets[c+1]=g->offsets[c]+g->widths[c];
        }
    }
    g->source_header=p->column_count>0;
    if (g->width_count) {
        g->col=MIN(g->col,g->width_count-1); g->anchor_col=MIN(g->anchor_col,g->width_count-1); g->end_col=MIN(g->end_col,g->width_count-1);
    }
    uint64_t rows=canvas_rows(g,p);
    g->row=MIN(g->row,rows-1); g->anchor_row=MIN(g->anchor_row,rows-1); g->end_row=MIN(g->end_row,rows-1);
}
static void keep_active_in_selection(Grid *g) {
    if (g->selection!=SELECT_COLUMNS && g->selection!=SELECT_ALL && (g->row<MIN(g->anchor_row,g->end_row) || g->row>MAX(g->anchor_row,g->end_row))) g->row=g->anchor_row;
    if (g->selection!=SELECT_ROWS && g->selection!=SELECT_ALL && (g->col<MIN(g->anchor_col,g->end_col) || g->col>MAX(g->anchor_col,g->end_col))) g->col=g->anchor_col;
}
void grid_select_cell(Grid *g,uint64_t row,size_t col,bool extend) {
    g->end_row=row; g->end_col=col; g->selection=SELECT_CELLS;
    if (!extend) { g->row=g->anchor_row=row; g->col=g->anchor_col=col; }
    else keep_active_in_selection(g);
}
bool grid_parse_address(const char *text,uint64_t *row,size_t *column) {
    if (!text || !row || !column) return false;
    const unsigned char *p=(const unsigned char *)text; while (isspace(*p)) p++;
    if (*p=='$') p++;
    size_t col=0; unsigned letters=0;
    while ((*p>='A' && *p<='Z') || (*p>='a' && *p<='z')) {
        unsigned digit=(unsigned)(toupper(*p++)-'A'+1);
        if (col>(16384-digit)/26) return false;
        col=col*26+digit; letters++;
    }
    if (!letters || !col) return false;
    if (*p=='$') p++;
    if (!isdigit(*p)) return false;
    uint64_t number=0;
    while (isdigit(*p)) { unsigned digit=*p++-'0'; if (number>(DX_SHEET_ROWS-digit)/10) return false; number=number*10+digit; }
    while (isspace(*p)) p++;
    if (*p || !number) return false;
    *row=number-1; *column=col-1; return true;
}
void grid_bounds(const Grid *g,uint64_t *r0,uint64_t *r1,size_t *c0,size_t *c1) {
    *r0=MIN(g->anchor_row,g->end_row); *r1=MAX(g->anchor_row,g->end_row);
    *c0=MIN(g->anchor_col,g->end_col); *c1=MAX(g->anchor_col,g->end_col);
    /* UINT64_MAX/SIZE_MAX are clamped to the relation by callers. */
    if (g->selection==SELECT_COLUMNS || g->selection==SELECT_ALL) { *r0=0; *r1=UINT64_MAX; }
    if (g->selection==SELECT_ROWS || g->selection==SELECT_ALL) { *c0=0; *c1=SIZE_MAX; }
}
bool grid_cached(const Grid *g,const Page *p) {
    if (!p->column_count || (!p->count_pending && g->top>=p->total) || (!g->frozen_columns && g->first_col>=p->column_count)) return true;
    if (p->count_pending && p->row_count<DX_PAGE_ROWS && g->top>=p->offset) return true;
    uint64_t last=p->count_pending ? g->top+g->render_rows : MIN(g->top+g->render_rows,p->total);
    return p->column_count && g->top>=p->offset && last<=p->offset+p->row_count;
}
static void scroll_to(Grid *g,const Page *p,double row) {
    uint64_t rows=canvas_rows(g,p);
    double max=rows>g->visible_rows ? (double)(rows-g->visible_rows) : 0;
    row=fmin(max,fmax(0,row)); g->top=(uint64_t)row; g->fraction=row-(double)g->top;
}
void grid_layout(Grid *g,float s,const Page *p,Rectangle r) {
    uint64_t label=after(g->top,(uint64_t)ceilf(r.height/fmaxf(1,row_height(g)*s))+2); unsigned digits=1;
    while (label>=10) { label/=10; digits++; }
    g->gutter=fmaxf(40,16+ceilf((g->digit_width>0 ? g->digit_width : 8)*digits));
    g->chrome_scale=s/(g->zoom>0 ? g->zoom : 1);
    g->rect=r; float gutter=g->gutter*s,sb=12*g->chrome_scale,hh=(g->source_header ? 48 : 24)*s;
    g->body=(Rectangle){r.x+gutter,r.y+hh,fmaxf(1,r.width-gutter-sb),fmaxf(1,r.height-hh-sb)};
    g->frozen_columns=g->freeze ? MIN(2,MIN(p->column_count,g->width_count)) : 0;
    g->frozen_width=g->frozen_columns ? g->offsets[g->frozen_columns] : 0;
    g->freeze_blocked=g->frozen_columns && g->body.width-g->frozen_width*s<72*g->chrome_scale;
    if (g->freeze_blocked) g->body.height=fmaxf(1,g->body.height-28*g->chrome_scale);
    g->visible_rows=(unsigned)MAX(1,floorf(g->body.height/(row_height(g)*s)));
    g->render_rows=(unsigned)ceilf(g->body.height/(row_height(g)*s)+g->fraction);
    scroll_to(g,p,(double)g->top+g->fraction);
    if (g->bounded) g->render_rows=(unsigned)MIN(g->render_rows,p->total-g->top);
    float total=g->width_count ? g->offsets[g->width_count] : 0;
    g->horizontal=clamp(g->horizontal,0,fmaxf(0,total-g->body.width/s));
    g->first_col=MAX(g->frozen_columns,column_at(g,g->frozen_width+fmaxf(0,g->horizontal-g->elastic.x)));
    g->vtrack=(Rectangle){r.x+r.width-sb,g->body.y,sb,g->body.height};
    g->htrack=(Rectangle){g->body.x+fminf(g->body.width,g->frozen_width*s),r.y+r.height-sb-(g->freeze_blocked ? 28*g->chrome_scale : 0),fmaxf(0,g->body.width-g->frozen_width*s),sb};
    uint64_t rows=canvas_rows(g,p);
    float vh=fminf(g->vtrack.height,fmaxf(26*g->chrome_scale,g->vtrack.height*g->visible_rows/(float)rows));
    double max=rows>g->visible_rows ? (double)(rows-g->visible_rows) : 0;
    float vy=max ? (float)(((double)g->top+g->fraction)/max)*(g->vtrack.height-vh) : 0;
    g->vthumb=(Rectangle){g->vtrack.x+2*g->chrome_scale,g->vtrack.y+vy,sb-4*g->chrome_scale,vh};
    float scrollable=total-g->frozen_width;
    float hw=fminf(g->htrack.width,fmaxf(26*g->chrome_scale,scrollable>0 ? g->htrack.width*g->htrack.width/(scrollable*s) : g->htrack.width));
    float max_x=fmaxf(0,total-g->body.width/s);
    float hx=max_x ? g->horizontal/max_x*(g->htrack.width-hw) : 0;
    g->hthumb=(Rectangle){g->htrack.x+hx,g->htrack.y+2*g->chrome_scale,hw,sb-4*g->chrome_scale};
}
static void reveal_cell_at(Grid *g,const Page *p,float s,uint64_t row,size_t column) {
    if (column>=g->width_count || !g->visible_rows || s<=0) return;
    g->elastic=g->spring_velocity=(Vector2){0};
    if ((double)row<(double)g->top+g->fraction) scroll_to(g,p,(double)row);
    if (row>=g->top+g->visible_rows) scroll_to(g,p,(double)(row-g->visible_rows+1));
    if (column<g->frozen_columns || g->freeze_blocked) return;
    float left=g->offsets[column]-g->frozen_width, view=g->body.width/s-g->frozen_width;
    if (left<g->horizontal) g->horizontal=left;
    if (left+g->widths[column]>g->horizontal+view) g->horizontal=left+g->widths[column]-view;
}
void grid_reveal_cell(Grid *g,const Page *p,float s) { reveal_cell_at(g,p,s,g->row,g->col); }
void grid_reveal_column(Grid *g,const Page *p,float s,size_t column,bool center) {
    if (column>=g->width_count || s<=0) return;
    if (column<g->frozen_columns || g->freeze_blocked) return;
    float left=g->offsets[column]-g->frozen_width,right=g->offsets[column+1]-g->frozen_width,view=g->body.width/s-g->frozen_width;
    if (center) g->horizontal=fmaxf(0,(left+right-view)*.5f);
    else if (left<g->horizontal || right>g->horizontal+view) g->horizontal=left;
    g->elastic.x=g->spring_velocity.x=0; grid_layout(g,s,p,g->rect);
}
void grid_zoom_at(Grid *g,const Page *p,float ui_scale,float zoom,Vector2 pointer) {
    zoom=clamp(zoom,.5f,2.5f);
    float old=ui_scale*g->zoom,next=ui_scale*zoom;
    Vector2 origin=grid_origin(g,old);
    Vector2 anchor={clamp(pointer.x,g->body.x,g->body.x+g->body.width),clamp(pointer.y,g->body.y,g->body.y+g->body.height)};
    bool left_edge=anchor.x==g->body.x,top_edge=anchor.y==g->body.y;
    double row=(double)g->top+g->fraction+(anchor.y-origin.y)/(row_height(g)*old);
    bool frozen_anchor=g->frozen_columns && anchor.x<g->body.x+g->frozen_width*old;
    float col=content_at(g,old,anchor.x);
    g->elastic=g->spring_velocity=(Vector2){0};
    g->zoom=zoom; grid_layout(g,next,p,g->rect);
    /* A larger gutter/header can cover the previous body corner. Keep that
       data anchor at the new body edge instead of scrolling it under chrome. */
    anchor=(Vector2){left_edge ? g->body.x : clamp(pointer.x,g->body.x,g->body.x+g->body.width),top_edge ? g->body.y : clamp(pointer.y,g->body.y,g->body.y+g->body.height)};
    if (!frozen_anchor) g->horizontal=col-(anchor.x-g->body.x)/next;
    scroll_to(g,p,row-(anchor.y-g->body.y)/(row_height(g)*next)); grid_layout(g,next,p,g->rect);
}
void grid_fit_column(Grid *g,UIContext *u,const Page *p,size_t c) {
    if (c>=g->width_count || u->scale<=0) return;
    if (c>=p->column_count) { grid_set_width(g,c,96); return; }
    float w=MAX(ui_measure_weight(u,p->columns[c].name,13,UI_MEDIUM),ui_measure(u,p->columns[c].type_name,10));
    for (size_t r=0;r<p->row_count;r++) {
        Cell *cell=&p->cells[r*p->column_count+c];
        w=fmaxf(w,ui_value_width(u,cell->text,cell->is_null,13,1180*u->scale));
    }
    grid_set_width(g,c,w/u->scale+20);
}
static void navigate(Grid *g,const Page *p,const GridInput *in,float scale) {
    Page canvas=*p; canvas.total=canvas_rows(g,p); canvas.column_count=g->width_count; canvas.records=(PageRecords){0};
    const Page *data=p; p=&canvas;
    int key=in->key; if (!key || !p->column_count) return;
    if (key==KEY_KB_MENU || (key==KEY_F10 && in->shift)) { if (!g->lock_structure) g->context_requested=true; return; }
    if (key==KEY_A && in->shortcut) {
        bool entire_data=g->selection==SELECT_CELLS && g->anchor_row==0 && g->anchor_col==0 &&
            g->end_row+1==data->total && g->end_col+1==data->column_count;
        if (data->total && g->row<data->total && g->col<data->column_count && !entire_data) {
            g->selection=SELECT_CELLS; g->anchor_row=0; g->anchor_col=0;
            g->end_row=data->total-1; g->end_col=data->column_count-1;
        } else g->selection=SELECT_ALL;
        return;
    }
    if (key==KEY_SPACE && (in->shift || in->shortcut)) { g->selection=in->shortcut ? SELECT_COLUMNS : SELECT_ROWS; return; }
    uint64_t r0,r1; size_t c0,c1; grid_bounds(g,&r0,&r1,&c0,&c1);
    r1=MIN(r1,p->total-1); c1=MIN(c1,p->column_count-1);
    if ((key==KEY_TAB || key==KEY_ENTER) && g->selection==SELECT_CELLS && (r0!=r1 || c0!=c1)) {
        if (key==KEY_TAB) {
            if (in->shift) { if (g->col>c0) g->col--; else { g->col=c1; g->row=g->row>r0 ? g->row-1 : r1; } }
            else if (g->col<c1) g->col++; else { g->col=c0; g->row=g->row<r1 ? g->row+1 : r0; }
        } else if (in->shift) {
            if (g->row>r0) g->row--; else { g->row=r1; g->col=g->col>c0 ? g->col-1 : c1; }
        } else if (g->row<r1) g->row++; else { g->row=r0; g->col=g->col<c1 ? g->col+1 : c0; }
        grid_reveal_cell(g,data,scale); return;
    }
    bool extending=in->shift && key!=KEY_TAB && key!=KEY_ENTER;
    uint64_t row=extending ? g->end_row : g->row; size_t col=extending ? g->end_col : g->col;
    GridRegion region=g->navigation;
    bool contextual=region.active && region.r0<=region.r1 && region.r1<DX_SHEET_ROWS && region.c0<=region.c1 && region.c1<g->width_count;
    switch (key) {
    case KEY_UP: case KEY_DOWN:
        if (in->alt && key==KEY_DOWN && g->col<data->column_count) { if (!g->lock_structure) g->filter_requested=true; return; }
        if (in->shortcut) row=contextual ? (key==KEY_UP ? region.r0 : region.r1) : key==KEY_UP ? 0 : (data->total ? data->total-1 : 0);
        else if (key==KEY_UP) row=row ? row-1 : 0; else row=MIN(row+1,p->total-1);
        break;
    case KEY_LEFT: case KEY_RIGHT:
        if (in->alt) { if (!g->lock_structure) grid_set_width(g,g->col,g->widths[g->col]+(key==KEY_LEFT ? -12 : 12)); return; }
        if (in->shortcut) col=contextual ? (key==KEY_LEFT ? region.c0 : region.c1) : key==KEY_LEFT ? 0 : (data->column_count ? data->column_count-1 : 0);
        else if (key==KEY_LEFT) col=col ? col-1 : 0; else col=MIN(col+1,p->column_count-1);
        break;
    case KEY_HOME: col=contextual && !in->shortcut ? region.c0 : 0; if (in->shortcut) row=0; break;
    case KEY_END: {
        size_t columns=MAX(data->column_count,g->used_columns); col=contextual && !in->shortcut ? region.c1 : columns ? columns-1 : 0;
        if (in->shortcut) { uint64_t rows=MAX(data->total,g->used_rows); row=rows ? rows-1 : 0; }
        break;
    }
    case KEY_PAGE_DOWN: row=MIN(row+g->visible_rows,p->total-1); break;
    case KEY_PAGE_UP: row=row>g->visible_rows ? row-g->visible_rows : 0; break;
    case KEY_TAB:
        if (in->shift) { if (col) col--; else if (row) { row--; col=p->column_count-1; } }
        else if (col+1<p->column_count) col++; else if (row+1<p->total) { row++; col=0; }
        break;
    case KEY_ENTER: row=in->shift ? (row ? row-1 : 0) : MIN(row+1,p->total-1); break;
    default: return;
    }
    if (in->shift && key!=KEY_TAB && key!=KEY_ENTER &&
        ((g->selection==SELECT_ROWS && (key==KEY_UP || key==KEY_DOWN)) ||
         (g->selection==SELECT_COLUMNS && (key==KEY_LEFT || key==KEY_RIGHT)))) {
        g->end_row=row; g->end_col=col; keep_active_in_selection(g);
    } else grid_select_cell(g,row,col,extending);
    reveal_cell_at(g,data,scale,row,col);
}
Vector2 grid_origin(const Grid *g,float s) { return (Vector2){g->body.x+g->elastic.x*s,g->body.y+g->elastic.y*s}; }
Rectangle grid_range_rect(const Grid *g,float s,uint64_t row,size_t col,uint64_t rows,size_t columns) {
    if (col>=g->width_count || columns>g->width_count-col) return (Rectangle){0}; Vector2 origin=grid_origin(g,s);
    return (Rectangle){grid_column_x(g,s,col),origin.y+(float)((double)row-g->top-g->fraction)*row_height(g)*s,(g->offsets[col+columns]-g->offsets[col])*s,(float)rows*row_height(g)*s};
}
Rectangle grid_range_outline(const Grid *g,float s,uint64_t row,size_t col,uint64_t rows,size_t columns,Rectangle clip) {
    if (!(s>0) || !isfinite(s) || !rows || row>=DX_SHEET_ROWS || rows>DX_SHEET_ROWS-row || !columns || col>=g->width_count || columns>g->width_count-col || clip.width<=0 || clip.height<=0) return (Rectangle){0};
    Vector2 origin=grid_origin(g,s);
    double left=g->body.x+((double)g->offsets[col]+(col<g->frozen_columns ? 0 : (double)g->elastic.x-g->horizontal))*s,right=left+((double)g->offsets[col+columns]-g->offsets[col])*s;
    double top=origin.y+((double)row-g->top-g->fraction)*row_height(g)*s,bottom=origin.y+((double)(row+rows)-g->top-g->fraction)*row_height(g)*s;
    if (right<clip.x || left>clip.x+clip.width || bottom<clip.y || top>clip.y+clip.height) return (Rectangle){0};
    double margin=8*s;
    left=fmax(left,clip.x-margin); right=fmin(right,clip.x+clip.width+margin);
    top=fmax(top,clip.y-margin); bottom=fmin(bottom,clip.y+clip.height+margin);
    return (Rectangle){(float)left,(float)top,(float)(right-left),(float)(bottom-top)};
}
static uint64_t row_at(const Grid *g,float s,float y) { return (uint64_t)fmax(0,(double)g->top+g->fraction+(y-grid_origin(g,s).y)/(row_height(g)*s)); }
bool grid_cell_at(const Grid *g,float s,Vector2 point,uint64_t *row,size_t *column) {
    if (s<=0 || !CheckCollisionPointRec(point,g->body) || !g->width_count) return false;
    Vector2 origin=grid_origin(g,s); float x=content_at(g,s,point.x);
    if (x<0 || (double)g->top+g->fraction+(point.y-origin.y)/(row_height(g)*s)<0) return false;
    *row=row_at(g,s,point.y); *column=column_at(g,x); return *column<g->width_count;
}
static Rectangle selection_rect(const Grid *g,float s,int64_t dr,int dc) {
    uint64_t r0,r1; size_t c0,c1; grid_bounds(g,&r0,&r1,&c0,&c1);
    if (c1>=g->width_count || r1==UINT64_MAX) return (Rectangle){0};
    c0=(size_t)((int)c0+dc); c1=(size_t)((int)c1+dc);
    return grid_range_outline(g,s,(uint64_t)((int64_t)r0+dr),c0,r1-r0+1,c1-c0+1,g->body);
}
static void spring_axis(float *position,float *velocity,float dt) {
    float decay=expf(-16*dt),step=(*velocity+16*(*position))*dt;
    *position=(*position+step)*decay; *velocity=(*velocity-16*step)*decay;
    if (fabsf(*position)<.02f && fabsf(*velocity)<.05f) *position=*velocity=0;
}
static void spring(Grid *g,float dt) {
    if (!g->elastic_enabled) { g->elastic=g->spring_velocity=(Vector2){0}; return; }
    if (g->dragging || g->moving || g->resizing>=0) return;
    dt=clamp(dt,0,.05f); spring_axis(&g->elastic.x,&g->spring_velocity.x,dt); spring_axis(&g->elastic.y,&g->spring_velocity.y,dt);
}
static double wheel_axis(Grid *g,double position,double maximum,double delta,float *visual,float *velocity) {
    if (!delta) return position;
    if (*visual*delta>0) { double consume=fmin(fabs(*visual),fabs(delta)); consume=delta<0 ? -consume : consume; *visual-=(float)consume; delta-=consume; }
    double wanted=position+delta,bounded=fmax(0,fmin(maximum,wanted));
    if (g->elastic_enabled) *visual=clamp(*visual+(float)(bounded-wanted)*.35f/(1+fabsf(*visual)/32),-64,64);
    *velocity=0; return bounded;
}
void grid_handle_input(Grid *g,UIContext *u,const Page *p,const GridInput *in) {
    GridInput masked;
    if (g->block_pointer && !g->dragging && !g->moving && g->resizing<0 && !g->scrollbar) {
        masked=*in; masked.mouse=(Vector2){-1000000,-1000000}; masked.wheel=(Vector2){0}; masked.pressed=masked.right=false; in=&masked;
    }
    GridInput keyboard=*in;
    if (in->separate_keyboard) { keyboard.shift=in->key_shift; keyboard.shortcut=in->key_shortcut; keyboard.alt=in->key_alt; }
    if (keyboard.key==KEY_KP_ENTER) keyboard.key=KEY_ENTER;
    if (g->lock_structure) { g->resizing=-1; g->moving=false; g->move_requested=g->context_requested=g->filter_requested=g->fit_requested=false; }
    if (in->key==KEY_ESCAPE) {
        if (g->resizing>=0) grid_set_width(g,(size_t)g->resizing,g->width_start);
        g->resizing=-1; g->last_edge=-1; g->moving=g->dragging=g->move_requested=false; grid_layout(g,u->scale,p,g->rect); return;
    }
    float cs=g->chrome_scale;
    if (g->freeze_blocked && !g->lock_structure && in->pressed && CheckCollisionPointRec(in->mouse,(Rectangle){g->rect.x+g->rect.width-68*cs,g->rect.y+g->rect.height-25*cs,61*cs,23*cs})) {
        g->freeze=false; g->elastic.x=g->spring_velocity.x=0; grid_layout(g,u->scale,p,g->rect); return;
    }
    size_t key_column=keyboard.shift ? g->end_col : g->col;
    if (!g->bounded && g->width_count && ((keyboard.key==KEY_RIGHT && !keyboard.alt && !keyboard.shortcut && key_column+1==g->width_count) ||
        ((in->wheel.x<0 || (in->shift && in->wheel.y<0)) && g->horizontal+g->body.width/u->scale>g->offsets[g->width_count]-96))) extend_columns(g);
    Page canvas=*p; canvas.total=canvas_rows(g,p); canvas.column_count=g->width_count; canvas.records=(PageRecords){0};
    const Page *data=p; p=&canvas;
    float s=u->scale; Vector2 m=in->mouse; g->clock+=in->dt; g->cursor=MOUSE_CURSOR_DEFAULT; spring(g,in->dt);
    if (in->key) g->elastic=g->spring_velocity=(Vector2){0};
    if (!g->lock_structure && g->allow_move && g->selection==SELECT_CELLS && CheckCollisionPointRec(m,g->body)) {
        Rectangle selection=selection_rect(g,s,0,0);
        bool edge=selection.width>0 && m.x>=selection.x-4*s && m.x<=selection.x+selection.width+4*s &&
            m.y>=selection.y-4*s && m.y<=selection.y+selection.height+4*s &&
            (fabsf(m.x-selection.x)<4*s || fabsf(m.x-selection.x-selection.width)<4*s || fabsf(m.y-selection.y)<4*s || fabsf(m.y-selection.y-selection.height)<4*s);
        if (edge) {
            g->cursor=MOUSE_CURSOR_RESIZE_ALL;
            if (in->pressed) { g->moving=true; g->move_dr=0; g->move_dc=0; g->move_row=row_at(g,s,m.y); g->move_col=column_at(g,content_at(g,s,m.x)); }
        }
    }
    if (g->moving) {
        uint64_t r0,r1; size_t c0,c1; grid_bounds(g,&r0,&r1,&c0,&c1);
        float edge=0, speed=fmaxf(1,in->dt*40);
        if (m.y<g->body.y+edge) scroll_to(g,data,(double)g->top+g->fraction-speed);
        else if (m.y>g->body.y+g->body.height-edge) scroll_to(g,data,(double)g->top+g->fraction+speed);
        if (m.x<g->body.x+edge) g->horizontal=fmaxf(0,g->horizontal-speed*24);
        else if (m.x>g->body.x+g->body.width-edge) { g->horizontal+=speed*24; if (g->horizontal+g->body.width/s>g->offsets[g->width_count]-96) extend_columns(g); }
        double row=(double)row_at(g,s,m.y);
        size_t col=column_at(g,fmaxf(0,content_at(g,s,m.x)));
        int64_t dr=(int64_t)fmax(0,row)-(int64_t)g->move_row; int dc=(int)col-(int)g->move_col;
        g->move_dr=MAX(-(int64_t)r0,MIN(dr,(int64_t)p->total-1-(int64_t)r1));
        g->move_dc=MAX(-(int)c0,MIN(dc,(int)p->column_count-1-(int)c1)); g->cursor=MOUSE_CURSOR_RESIZE_ALL;
        if (in->released || !in->down) { g->moving=false; g->move_requested=g->move_dr || g->move_dc; }
        grid_layout(g,s,data,g->rect); return;
    }
    bool hover=CheckCollisionPointRec(m,g->rect);
    if (hover && g->resizing<0 && !g->scrollbar) {
        double maximum=(double)(p->total>g->visible_rows ? p->total-g->visible_rows : 0)*row_height(g);
        if (!in->shift) {
            double position=wheel_axis(g,((double)g->top+g->fraction)*row_height(g),maximum,-in->wheel.y*3*row_height(g),&g->elastic.y,&g->spring_velocity.y);
            scroll_to(g,data,position/row_height(g));
        }
        float max_x=g->width_count ? fmaxf(0,g->offsets[g->width_count]-g->body.width/s) : 0;
        g->horizontal=(float)wheel_axis(g,g->horizontal,max_x,-(in->shift ? in->wheel.y : in->wheel.x)*52,&g->elastic.x,&g->spring_velocity.x);
    }
    /* Hit testing and drawing use the exact same thumb geometry, including grab offset. */
    if (in->pressed && CheckCollisionPointRec(m,g->vtrack)) {
        g->elastic=g->spring_velocity=(Vector2){0};
        g->scroll_rows=p->total; g->scrollbar=1; g->grab_offset=CheckCollisionPointRec(m,g->vthumb) ? m.y-g->vthumb.y : g->vthumb.height/2;
    } else if (in->pressed && CheckCollisionPointRec(m,g->htrack)) {
        g->elastic=g->spring_velocity=(Vector2){0};
        g->scroll_rows=p->total; g->scrollbar=2; g->grab_offset=CheckCollisionPointRec(m,g->hthumb) ? m.x-g->hthumb.x : g->hthumb.width/2;
    }
    if (g->scrollbar && in->down) {
        if (g->scrollbar==1) {
            float travel=g->vtrack.height-g->vthumb.height;
            double max=p->total>g->visible_rows ? (double)(p->total-g->visible_rows) : 0;
            scroll_to(g,data,travel>0 ? clamp((m.y-g->vtrack.y-g->grab_offset)/travel,0,1)*max : 0);
        } else {
            float travel=g->htrack.width-g->hthumb.width;
            float max=g->width_count ? fmaxf(0,g->offsets[g->width_count]-g->body.width/s) : 0;
            g->horizontal=travel>0 ? clamp((m.x-g->htrack.x-g->grab_offset)/travel,0,1)*max : 0;
        }
    }
    float content_x=content_at(g,s,m.x);
    size_t hit=column_at(g,content_x); int edge=-1;
    bool header=m.y>=g->rect.y && m.y<g->body.y && m.x>=g->body.x && m.x<g->body.x+g->body.width;
    if (header && g->width_count && !g->lock_structure) {
        if (hit<g->width_count && fabsf(m.x-grid_column_x(g,s,hit)-g->widths[hit]*s)<5*cs) edge=(int)hit;
        if (hit && hit!=g->frozen_columns && fabsf(m.x-grid_column_x(g,s,hit))<5*cs) edge=(int)hit-1;
        if (g->frozen_columns && fabsf(m.x-g->body.x-g->frozen_width*s)<5*cs) edge=(int)g->frozen_columns-1;
    }
    if (edge>=0) g->cursor=MOUSE_CURSOR_RESIZE_EW;
    if (edge>=0 && in->pressed) {
        if (g->last_edge==edge && g->clock-g->last_click<.32) {
            grid_fit_column(g,u,data,(size_t)edge); g->fit_requested=true; g->fit_column=(size_t)edge;
            g->resizing=-1; g->last_edge=-1;
        }
        else { g->resizing=edge; g->drag_start=m.x; g->width_start=g->widths[edge]; g->last_edge=edge; g->last_click=g->clock; }
    }
    if (g->resizing>=0 && in->down) {
        if (fabsf(m.x-g->drag_start)>3*s) g->last_edge=-1;
        grid_set_width(g,(size_t)g->resizing,g->width_start+(m.x-g->drag_start)/s); g->cursor=MOUSE_CURSOR_RESIZE_EW;
    }
    bool gutter=m.x>=g->rect.x && m.x<g->body.x && m.y>=g->body.y && m.y<g->body.y+g->body.height;
    bool body=CheckCollisionPointRec(m,g->body) && content_x>=0 && (double)g->top+g->fraction+(m.y-grid_origin(g,s).y)/(row_height(g)*s)>=0;
    if ((in->pressed || in->right) && !g->scrollbar && g->resizing<0 && p->column_count) {
        if (!g->lock_structure && in->right && gutter) {
            uint64_t row=row_at(g,s,m.y),r0,r1; size_t c0,c1; grid_bounds(g,&r0,&r1,&c0,&c1);
            if (g->selection!=SELECT_ROWS || row<r0 || row>r1) { grid_select_cell(g,row,g->col,false); g->selection=SELECT_ROWS; }
            g->context_requested=true; return;
        }
        if (!g->lock_structure && in->right && (header || body) && hit<p->column_count) {
            uint64_t row=body ? row_at(g,s,m.y) : g->row,r0,r1; size_t c0,c1; grid_bounds(g,&r0,&r1,&c0,&c1);
            if (header) { if (g->selection!=SELECT_COLUMNS || hit<c0 || hit>c1) { grid_select_cell(g,g->row,hit,false); g->selection=SELECT_COLUMNS; } }
            else if (row<r0 || row>r1 || hit<c0 || hit>c1) grid_select_cell(g,MIN(row,p->total-1),hit,false);
            g->context_requested=true; return;
        }
        if (header && hit<p->column_count) {
            float right=grid_column_x(g,s,hit)+g->widths[hit]*s;
            if (!g->lock_structure && hit<data->column_count && m.y>g->rect.y+(g->source_header ? 22 : 24)*s && m.x>right-22*cs) {
                grid_select_cell(g,g->row,hit,false); g->filter_requested=true; g->dragging=false; return;
            }
            if (!in->shift || g->selection!=SELECT_COLUMNS) g->col=g->anchor_col=hit;
            g->end_col=hit; g->selection=SELECT_COLUMNS; keep_active_in_selection(g); g->dragging=in->pressed;
        } else if (gutter && p->total) {
            uint64_t row=MIN(p->total-1,row_at(g,s,m.y));
            if (!in->shift || g->selection!=SELECT_ROWS) g->row=g->anchor_row=row;
            g->end_row=row; g->selection=SELECT_ROWS; keep_active_in_selection(g); g->dragging=in->pressed;
        } else if (body && hit<p->column_count && p->total) {
            uint64_t row=row_at(g,s,m.y);
            if (row<p->total) { grid_select_cell(g,row,hit,in->shift); g->dragging=in->pressed; }
        } else if (in->pressed && hover && m.x<g->body.x && m.y<g->body.y) g->selection=SELECT_ALL;
    }
    if (g->dragging && in->down && p->total && p->column_count) {
        /* Auto-scroll continues during a range drag outside the viewport. */
        g->auto_scroll+=in->dt;
        if (g->auto_scroll>=.035f) {
            g->auto_scroll=0;
            if (g->selection!=SELECT_COLUMNS && m.y<g->body.y) scroll_to(g,data,(double)g->top-1);
            if (g->selection!=SELECT_COLUMNS && m.y>=g->body.y+g->body.height) scroll_to(g,data,(double)g->top+1);
            if (g->selection!=SELECT_ROWS && m.x<g->body.x) g->horizontal-=16;
            if (g->selection!=SELECT_ROWS && m.x>=g->body.x+g->body.width) {
                g->horizontal+=16;
                if (!g->bounded && g->horizontal+g->body.width/s>g->offsets[g->width_count]-96) extend_columns(g);
            }
        }
        if (g->selection!=SELECT_COLUMNS) {
            float y=clamp(m.y,g->body.y,g->body.y+g->body.height-1);
            g->end_row=MIN(p->total-1,row_at(g,s,y));
        }
        if (g->selection!=SELECT_ROWS) {
            float x=clamp(m.x,g->body.x,g->body.x+g->body.width-1);
            size_t c=column_at(g,content_at(g,s,x)); g->end_col=MIN(c,p->column_count-1);
        }
        keep_active_in_selection(g);
    }
    if (in->released || !in->down) { g->dragging=false; g->resizing=-1; g->scrollbar=0; g->scroll_rows=0; }
    navigate(g,data,&keyboard,s); grid_layout(g,s,data,g->rect);
}
void grid_replay_gesture(Grid *g,UIContext *u,const Page *p,DxPointerGesture gesture) {
    GridInput in={.mouse={gesture.x0,gesture.y0},.pressed=true,.down=true,.shift=(gesture.modifiers&DX_MOD_SHIFT)!=0,.alt=(gesture.modifiers&DX_MOD_ALT)!=0};
#ifdef __APPLE__
    in.shortcut=(gesture.modifiers&DX_MOD_SUPER)!=0;
#else
    in.shortcut=(gesture.modifiers&DX_MOD_CONTROL)!=0;
#endif
    if (gesture.right) { in.pressed=in.down=false; in.right=true; grid_handle_input(g,u,p,&in); return; }
    grid_handle_input(g,u,p,&in);
    in.pressed=false; in.mouse=(Vector2){gesture.x1,gesture.y1}; grid_handle_input(g,u,p,&in);
    in.down=false; in.released=true; grid_handle_input(g,u,p,&in);
}
void grid_update(Grid *g,UIContext *u,const Page *p,Rectangle r,bool enabled) {
#ifdef DUXCEL_DEV_REVIEW
    if (dx_review_input_blocked()) { grid_layout(g,u->scale,p,r); return; }
#endif
    g->controls_enabled=enabled && !g->lock_structure && !g->block_pointer;
    grid_layout(g,u->scale,p,r);
    DxPointerGesture gesture;
    if (!enabled) { g->dragging=false; g->moving=false; g->resizing=-1; g->scrollbar=0; g->scroll_rows=0; g->last_edge=-1; spring(g,GetFrameTime()); return; }
    GridInput in={.mouse=GetMousePosition(),.wheel=GetMouseWheelMoveV(),.pressed=IsMouseButtonPressed(MOUSE_BUTTON_LEFT),
        .released=IsMouseButtonReleased(MOUSE_BUTTON_LEFT),.down=IsMouseButtonDown(MOUSE_BUTTON_LEFT),
        .right=IsMouseButtonPressed(MOUSE_BUTTON_RIGHT),.shift=ui_shift(),.shortcut=ui_shortcut(),
        .alt=IsKeyDown(KEY_LEFT_ALT)||IsKeyDown(KEY_RIGHT_ALT),.dt=GetFrameTime()};
    const int navigation[]={KEY_UP,KEY_DOWN,KEY_LEFT,KEY_RIGHT,KEY_HOME,KEY_END,KEY_PAGE_UP,KEY_PAGE_DOWN,KEY_TAB,KEY_ENTER,KEY_KP_ENTER};
    if (!ui_navigation_captured()) for (size_t i=0;i<sizeof(navigation)/sizeof(*navigation);i++) if (ui_key(navigation[i])) { in.key=navigation[i]; break; }
    const int commands[]={KEY_A,KEY_SPACE,KEY_KB_MENU,KEY_F10,KEY_ESCAPE};
    if (!in.key) for (size_t i=0;i<sizeof(commands)/sizeof(*commands);i++) if (ui_key(commands[i])) { in.key=commands[i]; break; }
    in.separate_keyboard=true; in.key_shift=ui_key_shift(in.key); in.key_shortcut=ui_key_shortcut(in.key); in.key_alt=ui_key_alt(in.key);
    /* Only gestures beginning on this grid belong to it. Later-drawn chrome
       (including the status bar) must retain its completed clicks. */
    while (!g->block_pointer && ui_take_gesture_from(r,&gesture)) if (!in.pressed && !in.released && !in.down && !in.right && !IsMouseButtonReleased(MOUSE_BUTTON_RIGHT) && !IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && !g->move_requested && !g->context_requested && !g->filter_requested && !g->fit_requested) grid_replay_gesture(g,u,p,gesture);
    grid_handle_input(g,u,p,&in); if (g->cursor!=MOUSE_CURSOR_DEFAULT) u->cursor=g->cursor;
}
static void letter(size_t c,char *out) {
    char temp[32]; size_t n=0; c++;
    do { c--; temp[n++]=(char)('A'+c%26); c/=26; } while (c && n<sizeof(temp)-1);
    for (size_t i=0;i<n;i++) out[i]=temp[n-i-1]; out[n]=0;
}
static void draw_grid_region(Grid *g,UIContext *u,const Page *p,bool frozen) {
    float s=u->scale,rh=row_height(g)*s,hh=g->body.y-g->rect.y;
    size_t start=frozen ? 0 : g->first_col,end=frozen ? g->frozen_columns : g->width_count;
    Rectangle clip=grid_column_clip(g,s,frozen ? 0 : g->frozen_columns); if (clip.width<=0) return;
    Vector2 origin=grid_origin(g,s); float y0=origin.y-(float)g->fraction*rh;
    uint64_t r0,r1; size_t c0,c1; grid_bounds(g,&r0,&r1,&c0,&c1);
    /* Selection endpoints are calculated in each plane before clipping. This
       keeps a spanning range continuous without inventing a border at Freeze. */
    Grid plane=*g; plane.frozen_columns=0; if (frozen) { plane.horizontal=0; plane.elastic.x=0; }
    if (!g->hide_selection && c0<g->width_count && g->render_rows && r1>=g->top && r0<g->top+g->render_rows) {
        size_t last=MIN(c1,g->width_count-1); uint64_t first_row=MAX(r0,g->top),last_row=MIN(MIN(r1,g->top+g->render_rows-1),DX_SHEET_ROWS-1);
        Rectangle area=grid_range_rect(&plane,s,first_row,c0,last_row-first_row+1,last-c0+1);
        area=GetCollisionRec(area,clip); if (area.width>0 && area.height>0) DrawRectangleRec(area,UI_TINT);
    }
    if (!g->hide_gridlines) for (unsigned vr=0;vr<g->render_rows;vr++) {
        float bottom=y0+(vr+1)*rh;
        if (bottom>=clip.y && bottom<=clip.y+clip.height) DrawRectangleRec((Rectangle){clip.x,bottom-g->chrome_scale,clip.width,g->chrome_scale},GRID_LINE);
    }
    for (size_t c=start;c<end;c++) {
        float x=grid_column_x(g,s,c),right=fminf(x+g->widths[c]*s,clip.x+clip.width);
        if (x>=clip.x+clip.width) break;
        if (!g->hide_gridlines && right>clip.x) DrawRectangleRec((Rectangle){right-g->chrome_scale,clip.y,g->chrome_scale,clip.height},GRID_LINE);
    }
    ui_set_clip(u,clip);
    for (size_t c=start;c<end;c++) {
        float x=grid_column_x(g,s,c),w=g->widths[c]*s; if (x>=clip.x+clip.width) break;
        if (x+w<=clip.x) continue;
        for (unsigned vr=0;vr<g->render_rows;vr++) {
            uint64_t row=g->top+vr; if (row>=DX_SHEET_ROWS) break;
            bool in_data=c<p->column_count && (p->count_pending || row<p->total); if (!in_data) continue;
            const Cell *value=row>=p->offset && row<p->offset+p->row_count ? &p->cells[(row-p->offset)*p->column_count+c] : NULL;
            bool null=value && value->is_null; const char *text=value ? value->text : "…";
            bool numeric=p->columns[c].numeric,floating=p->columns[c].type==DUCKDB_TYPE_FLOAT || p->columns[c].type==DUCKDB_TYPE_DOUBLE;
            CellFormat format=g->formats ? format_at(g->formats,g->format_row+row,g->format_col+c) : format_default();
            Color color=numeric && value && !null && value->number<0 ? (Color){164,51,55,255} : UI_TEXT;
            ui_formatted_value(u,text,null,numeric,floating,value ? value->number : 0,(Rectangle){x+3*s,y0+vr*rh,w-4*s,rh},13,color,format);
        }
    }
    if (!g->hide_selection && g->col>=start && g->col<end && g->row>=g->top && g->row-g->top<g->render_rows) ui_border(grid_range_rect(g,s,g->row,g->col,1,1),clip,2*g->chrome_scale,UI_ACCENT);
    if (!g->hide_selection && g->selection==SELECT_CELLS) {
        Rectangle outline=selection_rect(&plane,s,g->moving ? g->move_dr : 0,g->moving ? g->move_dc : 0);
        ui_border(outline,clip,1.5f*g->chrome_scale,UI_ACCENT);
        Vector2 corner={outline.x+outline.width,outline.y+outline.height};
        float handle=3*g->chrome_scale;
        if (g->allow_move && corner.x<clip.x+clip.width && corner.y<clip.y+clip.height && CheckCollisionPointRec(corner,clip)) DrawRectangleRec(GetCollisionRec((Rectangle){corner.x-handle,corner.y-handle,2*handle,2*handle},clip),UI_ACCENT);
    }
    float cs=g->chrome_scale; u->scale=cs;
    Rectangle header={clip.x,g->rect.y,clip.width,hh}; ui_set_clip(u,header);
    for (size_t c=start;c<end;c++) {
        float x=grid_column_x(g,s,c),w=g->widths[c]*s; if (x>=clip.x+clip.width) break;
        float left=fmaxf(x,clip.x),right=fminf(x+w,clip.x+clip.width); if (right<=left) continue;
        if (!g->hide_selection && c>=c0 && c<=c1) DrawRectangleRec((Rectangle){left,g->rect.y,right-left,hh},UI_TINT);
        if (c<p->column_count) {
            DrawRectangleRec((Rectangle){left,g->rect.y+22*s,right-left,26*s},(Color){242,245,247,255});
            float arrow_x=x+w-12*cs,arrow_y=g->rect.y+35*s;
            if (arrow_x>clip.x+9*cs && arrow_x<clip.x+clip.width-9*cs) {
                Rectangle filter={arrow_x-8*cs,arrow_y-9*cs,16*cs,18*cs}; bool hover=CheckCollisionPointRec(GetMousePosition(),filter),filtered=p->columns[c].filtered;
                if (filtered || hover) { DrawRectangleRec(filter,UI_TINT); DrawRectangleLinesEx(filter,cs,filtered ? UI_ACCENT : UI_BORDER); }
                ui_carbon(u,filtered ? CARBON_FILTER : CARBON_CHEVRON_DOWN,(Rectangle){arrow_x-6*cs,arrow_y-6*cs,12*cs,12*cs},filtered ? UI_ACCENT : UI_MUTED);
            }
        }
        DrawRectangleRec((Rectangle){right-cs,g->rect.y,cs,hh},(Color){216,224,230,255});
    }
    for (size_t c=start;c<end;c++) {
        float x=grid_column_x(g,s,c),w=g->widths[c]*s; if (x>=clip.x+clip.width) break;
        char code[32]; letter(c,code); bool sorted=c<p->column_count && p->columns[c].sort_priority && w>=74*cs;
        float height=(g->source_header ? 22 : 24)*s,width=ui_measure(u,code,11);
        ui_label(u,code,(Rectangle){x+(w-width)*.5f,g->rect.y,width+1,height},11,UI_MUTED,false,UI_REGULAR);
        if (sorted) { char order[16]; snprintf(order,sizeof(order),"%s%u",p->columns[c].sort_descending ? "↓" : "↑",p->columns[c].sort_priority); ui_text(u,order,x+w-30*cs,g->rect.y+(height-10*cs)*.5f,10,UI_ACCENT); }
    }
    for (size_t c=start;c<MIN(end,p->column_count);c++) {
        float x=grid_column_x(g,s,c),w=g->widths[c]*s; if (x>=clip.x+clip.width) break;
        ui_text_clip_weight(u,p->columns[c].name,(Rectangle){x+3*cs,g->rect.y+22*s,w-25*cs,26*s},12,UI_TEXT,false,UI_MEDIUM);
    }
    ui_clear_clip(u); u->scale=s;
}
void grid_draw(Grid *g,UIContext *u,const Page *p,bool busy) {
    Rectangle r=g->rect; float s=u->scale,rh=row_height(g)*s,hh=g->body.y-r.y; DrawRectangleRec(r,WHITE);
    uint64_t r0,r1; size_t c0,c1; grid_bounds(g,&r0,&r1,&c0,&c1); (void)c0; (void)c1;
    Vector2 origin=grid_origin(g,s); float y0=origin.y-(float)g->fraction*rh;
    DrawRectangleRec((Rectangle){r.x,r.y,r.width-12*g->chrome_scale,hh},(Color){242,245,247,255});
    for (unsigned vr=0;vr<g->render_rows;vr++) {
        float y=y0+vr*rh,top=fmaxf(y,g->body.y),bottom=fminf(y+rh,g->body.y+g->body.height); if (bottom<=top) continue;
        DrawLineV((Vector2){r.x,bottom},(Vector2){g->body.x,bottom},GRID_LINE);
    }
    draw_grid_region(g,u,p,false); if (g->frozen_columns) draw_grid_region(g,u,p,true);
    if (g->frozen_columns && g->frozen_width*s<g->body.width) { float boundary=g->body.x+g->frozen_width*s; DrawLineEx((Vector2){boundary,r.y},(Vector2){boundary,g->body.y+g->body.height},s,(Color){155,174,190,255}); }
    DrawRectangleRec((Rectangle){r.x,g->body.y,g->gutter*s,g->body.height},UI_BG);
    if (!g->hide_selection && g->render_rows && r1>=g->top && r0<g->top+g->render_rows) {
        uint64_t first=MAX(r0,g->top),last=MIN(r1,g->top+g->render_rows-1);
        float top=fmaxf(y0+(float)(first-g->top)*rh,g->body.y),bottom=fminf(y0+(float)(last-g->top+1)*rh,g->body.y+g->body.height);
        if (bottom>top) DrawRectangleRec((Rectangle){r.x,top,g->gutter*s,bottom-top},UI_TINT);
    }
    float cs=g->chrome_scale; u->scale=cs;
    ui_set_clip(u,(Rectangle){r.x,g->body.y,g->gutter*s,g->body.height});
    for (unsigned vr=0;vr<g->render_rows;vr++) {
        uint64_t row=g->top+vr; if (row>=DX_SHEET_ROWS) break;
        float y=y0+vr*rh,top=fmaxf(y,g->body.y),bottom=fminf(y+rh,g->body.y+g->body.height); if (bottom<=top) continue;
        char text[32]; snprintf(text,sizeof(text),"%" PRIu64,row+(g->source_header ? 2 : 1));
        ui_text_clip(u,text,(Rectangle){r.x,y,g->gutter*s,rh},11,UI_MUTED,true);
    }
    ui_clear_clip(u);
    DrawLineV((Vector2){g->body.x,r.y},(Vector2){g->body.x,g->body.y+g->body.height},GRID_LINE);
    DrawLineV((Vector2){r.x,g->body.y},(Vector2){g->body.x+g->body.width,g->body.y},(Color){197,208,217,255});
    DrawRectangleRec(g->vtrack,UI_BG); DrawRectangleRec(g->htrack,UI_BG);
    Color thumb={180,191,201,255};
    if (g->vthumb.height<g->vtrack.height-.5f) DrawRectangleRounded(g->vthumb,.6f,4,thumb);
    if (g->hthumb.width<g->htrack.width-.5f) DrawRectangleRounded(g->hthumb,.6f,4,thumb);
    if (g->resizing>=0) {
        char label[64]; snprintf(label,sizeof(label),"Width %.1f · Esc cancels",g->widths[g->resizing]);
        float width=ui_measure(u,label,11)+16*cs,x=grid_column_x(g,s,(size_t)g->resizing)+g->widths[g->resizing]*s;
        Rectangle tip={clamp(x-width*.5f,g->body.x,fmaxf(g->body.x,g->body.x+g->body.width-width)),g->rect.y+2*cs,width,22*cs};
        DrawRectangleRec(tip,UI_TEXT); ui_text_clip(u,label,tip,11,WHITE,false);
    }
    if (busy) ui_text(u,"Fetching…",r.x+r.width-112*cs,r.y+5*cs,11,UI_ACCENT);
    if (g->freeze_blocked) {
        Rectangle hint={r.x,r.y+r.height-28*cs,r.width,28*cs}; DrawRectangleRec(hint,(Color){238,244,248,255});
        DrawLineV((Vector2){hint.x,hint.y},(Vector2){hint.x+hint.width,hint.y},(Color){191,208,222,255});
        ui_text_clip(u,"Frozen columns fill this view.",(Rectangle){hint.x+7*cs,hint.y,fmaxf(0,hint.width-80*cs),hint.height},11,(Color){61,93,115,255},false);
        if (ui_button(u,(Rectangle){hint.x+hint.width-68*cs,hint.y+3*cs,61*cs,23*cs},"Unfreeze",false,g->controls_enabled)) { g->freeze=false; grid_layout(g,s,p,r); }
    }
    u->scale=s;
}
