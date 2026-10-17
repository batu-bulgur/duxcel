#include "workspace.h"
#include <math.h>

UIWorkspaceLayout ui_workspace_layout(float width,float height,float chrome,
    float left_width,float inspector_width,bool left_visible,bool drawer,bool authoring,bool focus) {
    UIWorkspaceLayout out={0};
    if (!isfinite(width)||!isfinite(height)||!isfinite(chrome)||!isfinite(left_width)||!isfinite(inspector_width) ||
        width<240 || height<240 || chrome<0 || chrome>height-100 || left_width<0 || inspector_width<0) return out;
    float bottom=height-22;
    out.narrow=width<700; out.right_overlay=width<1040;
    /* The caller retains the user's full pane width. This is its animated
       wrapper extent, which may legitimately pass through zero. */
    left_width=fminf(left_width,fminf(360,width-24));
    inspector_width=focus ? 0 : fminf(inspector_width,width-24);
    out.left_overlay=!focus && out.narrow && drawer && left_visible;
    float side=!focus && left_visible && !out.narrow ? left_width : 0;
    float right=width-(out.right_overlay ? 0 : inspector_width);
    float context_height=authoring && !focus ? 88 : 28;
    out.context=(Rectangle){side,chrome,fmaxf(0,right-side),context_height};
    out.canvas=(Rectangle){side,chrome+context_height,fmaxf(0,right-side),fmaxf(0,bottom-chrome-context_height)};
    out.left=(Rectangle){0,chrome,side ? side : out.left_overlay ? left_width : 0,bottom-chrome};
    out.inspector=(Rectangle){width-inspector_width,chrome,inspector_width,bottom-chrome};
    out.valid=true;
    return out;
}
void ui_workspace_context_height(UIWorkspaceLayout *workspace,float height) {
    if (!workspace || !workspace->valid || !isfinite(height) || height<0) return;
    float bottom=workspace->canvas.y+workspace->canvas.height;
    workspace->context.height=height;
    workspace->canvas.y=workspace->context.y+height;
    workspace->canvas.height=fmaxf(0,bottom-workspace->canvas.y);
}
UICellContextLayout ui_cell_context_layout(Rectangle bounds,float viewport_width,bool workspace_button,bool focus,float type_width) {
    UICellContextLayout out={0};
    if (!isfinite(bounds.x)||!isfinite(bounds.y)||!isfinite(bounds.width)||bounds.width<160 || !isfinite(viewport_width)||viewport_width<240 || !isfinite(type_width)||type_width<0) return out;
    bool compact=viewport_width<700;
    float gap=compact ? 5 : 9,left=bounds.x+(workspace_button ? 36 : 0),width=compact ? 62 : 88,padding=compact ? 6 : 9;
    out.address=(Rectangle){left,bounds.y+2.5f,width,22};
    out.address_text=(Rectangle){left+padding,bounds.y+2.5f,fmaxf(0,width-2*padding-12),22};
    out.chevron=(Rectangle){left+width-padding-12,bounds.y+7.5f,12,12};
    out.function=(Rectangle){left+width+gap,bounds.y+5.5f,16,16};
    float start=out.function.x+16+gap,right=bounds.x+bounds.width-(focus ? 145 : 8);
    float type=viewport_width>620 ? fmaxf(52,type_width) : 0;
    out.type=(Rectangle){right-type,bounds.y+2.5f,type,22};
    out.value=(Rectangle){start,bounds.y+2.5f,fmaxf(0,right-start-(type ? type+gap : 0)),22};
    out.valid=true; return out;
}
UIContextTitleLayout ui_context_title_layout(Rectangle bounds,bool compact,float title_width,float source_width,float browse_text_width) {
    UIContextTitleLayout out={0};
    if (!isfinite(bounds.x)||!isfinite(bounds.y)||!isfinite(bounds.width)||bounds.width<200 || !isfinite(title_width)||title_width<0 || !isfinite(source_width)||source_width<0 || !isfinite(browse_text_width)||browse_text_width<0) return out;
    float start=bounds.x+(compact ? 39 : 11),browse=fminf(browse_text_width+26,bounds.width-80);
    out.icon=(Rectangle){start,bounds.y+4.5f,16,16};
    out.browse=(Rectangle){bounds.x+bounds.width-11-browse,bounds.y+2.25f,browse,20.5f};
    float available=fmaxf(0,out.browse.x-8-start-24);
    out.title=(Rectangle){start+24,bounds.y,compact ? available : fminf(available,title_width),25};
    if (!compact) out.source=(Rectangle){out.title.x+out.title.width+13,bounds.y,fminf(source_width,fmaxf(0,out.browse.x-8-out.title.x-out.title.width-13)),25};
    out.browse_text=(Rectangle){out.browse.x+5,bounds.y+2.25f,fmaxf(0,browse-26),20.5f};
    out.chevron=(Rectangle){out.browse.x+browse-17,bounds.y+6.5f,12,12};
    out.valid=true; return out;
}
UIShelfLayout ui_shelf_layout(Rectangle bounds,const float *widths,size_t count,bool compact,bool last) {
    UIShelfLayout out={0};
    if (!isfinite(bounds.x)||!isfinite(bounds.y)||!isfinite(bounds.width)||bounds.width<120 || count>UI_SHELF_FIELDS || (count&&!widths)) return out;
    float padding=compact ? 7 : 8,label=compact ? 72 : 88;
    float start=bounds.x+padding+label+5,available=bounds.width-padding-label-5-8;
    float x=0,y=0;
    for (size_t i=0;i<=count;i++) {
        float w=i==count ? 108 : widths[i];
        if (!isfinite(w)||w<=0) return (UIShelfLayout){0};
        w=fminf(w,210);
        if (x>0 && x+w>available+.001f) { x=0; y+=28; }
        Rectangle token={start+x,bounds.y+3+y,w,23};
        if (i==count) out.add=token; else out.tokens[i]=token;
        x+=w+5;
    }
    float height=fmaxf(24,y+23);
    if (!y) {
        for (size_t i=0;i<count;i++) out.tokens[i].y+=.5f;
        out.add.y+=.5f;
    }
    out.panel=(Rectangle){bounds.x,bounds.y,bounds.width,height+6+(last ? 0 : 1)};
    out.label=(Rectangle){bounds.x+padding,bounds.y+3,label,24};
    out.fields=(Rectangle){start,bounds.y+3,available,height};
    out.count=count; out.valid=true; return out;
}
size_t ui_shelf_insertion(const Rectangle *tokens,size_t count,Vector2 point) {
    if (!tokens || count>UI_SHELF_FIELDS || !isfinite(point.x)||!isfinite(point.y)) return count;
    for (size_t i=0;i<count;i++) {
        Rectangle token=tokens[i];
        if (point.y<token.y || (point.y<token.y+token.height && point.x<token.x+token.width*.5f)) return i;
    }
    return count;
}
size_t ui_shelf_neighbor(const Rectangle *tokens,size_t count,size_t current,int direction) {
    if (!tokens || count>UI_SHELF_FIELDS || current>=count || !direction) return current;
    float x=tokens[current].x+tokens[current].width*.5f,y=tokens[current].y;
    float nearest_row=INFINITY,nearest_x=INFINITY; size_t next=current;
    for (size_t i=0;i<count;i++) {
        float row=(tokens[i].y-y)*(direction<0 ? -1 : 1);
        float distance=fabsf(tokens[i].x+tokens[i].width*.5f-x);
        if (row<=.001f) continue;
        if (row<nearest_row-.001f || (fabsf(row-nearest_row)<.001f && distance<nearest_x)) {
            nearest_row=row; nearest_x=distance; next=i;
        }
    }
    return next;
}
UIAuthoringLayout ui_authoring_layout(Rectangle canvas,float scroll,float context_height) {
    UIAuthoringLayout out={0};
    if (!isfinite(canvas.x)||!isfinite(canvas.y)||!isfinite(canvas.width)||!isfinite(canvas.height)||!isfinite(scroll)||!isfinite(context_height)||context_height<0 || canvas.width<120 || canvas.height<80) return out;
    float preview=fminf(336,fmaxf(150,(canvas.height+context_height)*.4f));
    out.stacked=canvas.width<=880;
    out.height=21+preview+15+318+(out.stacked ? 330 : 0)+20;
    out.scrollable=out.height>canvas.height;
    out.content_width=canvas.width-(out.scrollable ? 9 : 0);
    /* A scrollable CSS container reserves its own lane before laying out the
       preview's independent scrollbar and the two analysis objects. */
    out.stacked=out.content_width<=880;
    out.height=21+preview+15+318+(out.stacked ? 330 : 0)+20;
    scroll=fmaxf(0,fminf(fmaxf(0,out.height-canvas.height),scroll));
    float y=canvas.y-scroll;
    out.label=(Rectangle){canvas.x,y,out.content_width,21};
    out.preview=(Rectangle){canvas.x,y+21,out.content_width,preview};
    float available=out.content_width-24-(out.stacked ? 0 : 12);
    float matrix=out.stacked ? available : fmaxf(480,fminf(available-340,available*1.3f/2.3f));
    out.matrix=(Rectangle){canvas.x+12,y+21+preview+15,matrix,318};
    out.chart=(Rectangle){out.stacked ? canvas.x+12 : out.matrix.x+matrix+12,out.matrix.y+(out.stacked ? 330 : 0),out.stacked ? matrix : available-matrix,318};
    out.valid=true; return out;
}
float ui_matrix_column_width(size_t column,size_t dimensions) { return column<dimensions ? 110 : 108; }
float ui_matrix_column_left(size_t column,size_t dimensions) { return (float)((double)column*108+(double)(column<dimensions ? column : dimensions)*2); }
size_t ui_matrix_column_at(float x,size_t columns,size_t dimensions) {
    if (!columns || !isfinite(x)) return 0;
    dimensions=dimensions<columns ? dimensions : columns; x=fmaxf(0,x);
    float width=dimensions*110.0f;
    double index=x<width ? floor(x/110) : dimensions+floor((x-width)/108);
    return index>=columns ? columns-1 : (size_t)index;
}
UIFilterLayout ui_filter_layout(Vector2 anchor,Vector2 window,float scale,int terms,bool dates) {
    UIFilterLayout r={0};
    if (!isfinite(scale)||scale<=0||!isfinite(anchor.x)||!isfinite(anchor.y)||!isfinite(window.x)||!isfinite(window.y)||window.x<180*scale||window.y<300*scale||terms<0||terms>2) return r;
    float width=fminf(288,window.x/scale-12),fixed=282.5f+terms*30+(dates ? 27 : 0);
    float list=fminf(230,fmaxf(25,window.y/scale-20-fixed)),height=fixed+list;
    float x=fmaxf(5,fminf(anchor.x/scale,window.x/scale-width-6));
    /* The web definition reserves 570 for anchor placement even with a shorter checklist. */
    float y=fmaxf(5,fminf(anchor.y/scale,window.y/scale-fmaxf(570,height)-10));
    r.panel=(Rectangle){x,y,width,height}; r.title=(Rectangle){x+1,y+1,width-2,36};
    for (int i=0;i<2;i++) r.sort[i]=(Rectangle){x+7,y+42+i*29,width-14,29};
    r.search=(Rectangle){x+11,y+115,width-22,28}; r.all=(Rectangle){x+4,y+148,width-8,27};
    r.dates=(Rectangle){x+4,y+175,width-8,dates ? 27 : 0};
    r.list=(Rectangle){x+4,y+175+r.dates.height,width-8,list};
    float cy=r.list.y+r.list.height;
    r.condition=(Rectangle){x+81,cy+7,width-92,25};
    r.term=(Rectangle){r.condition.x,cy+37,r.condition.width,terms ? 25 : 0};
    r.end=(Rectangle){r.condition.x,cy+67,r.condition.width,terms==2 ? 25 : 0};
    r.scope=(Rectangle){x+11,cy+40+terms*30,width-22,22.5f};
    r.footer=(Rectangle){x+1,r.scope.y+22.5f,width-2,44};
    r.clear=(Rectangle){x+10,r.footer.y+8.5f,51.4375f,27};
    r.apply=(Rectangle){x+width-60.1875f,r.footer.y+8,50.1875f,28};
    r.cancel=(Rectangle){r.apply.x-61.453125f,r.footer.y+8.5f,55.453125f,27}; r.valid=true;
    Rectangle *rects[]={&r.panel,&r.title,&r.sort[0],&r.sort[1],&r.search,&r.all,&r.dates,&r.list,&r.condition,&r.term,&r.end,&r.scope,&r.footer,&r.clear,&r.cancel,&r.apply};
    for (size_t i=0;i<sizeof(rects)/sizeof(*rects);i++) { rects[i]->x*=scale; rects[i]->y*=scale; rects[i]->width*=scale; rects[i]->height*=scale; }
    return r;
}
