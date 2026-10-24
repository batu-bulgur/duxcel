#include "ui/choiceview.h"
#include "ui/selectview.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"choice geometry at %d: %s\n",__LINE__,#x); exit(1); } } while (0)

/* This executable links choiceview/selectview only. Record real painter calls
   without constructing a GPU window, font atlas or engine. These stubs never
   enter an App/test executable which links the actual UI implementation. */
const Color UI_BG={244,246,248,255},UI_SURFACE={255,255,255,255},UI_TEXT={32,39,46,255},UI_MUTED={99,113,125,255};
const Color UI_BORDER={202,210,218,255},UI_ACCENT={40,95,134,255},UI_TINT={228,238,246,255},UI_FOCUS={121,82,179,255};
typedef enum { PAINT_RECT,PAINT_ROUND,PAINT_ROUND_LINE,PAINT_BORDER,PAINT_LABEL,PAINT_ICON,PAINT_FOCUS } PaintKind;
typedef struct { PaintKind kind; Rectangle rect,clip; Color color; float size; bool clipping; UICarbonIcon icon; UIFontWeight weight; char text[64]; } Paint;
static struct {
    UIContext *ui; Paint calls[32]; unsigned count,inputs,begins,ends,feedback_kind;
    bool click,hover,down,scissor,input_enabled; Rectangle input;
} paint;
static void reset_paint(UIContext *ui) { memset(&paint,0,sizeof(paint)); paint.ui=ui; paint.scissor=true; }
static Paint *record_paint(PaintKind kind,Rectangle rect,Color color,float size) {
    if (paint.count==sizeof(paint.calls)/sizeof(*paint.calls)) { fputs("Paint recorder overflow\n",stderr); exit(1); }
    Paint *p=&paint.calls[paint.count++]; *p=(Paint){.kind=kind,.rect=rect,.color=color,.size=size,.clip=paint.ui->clip,.clipping=paint.ui->clipping}; return p;
}
static bool same_rect(Rectangle a,Rectangle b) { return fabsf(a.x-b.x)<.001f && fabsf(a.y-b.y)<.001f && fabsf(a.width-b.width)<.001f && fabsf(a.height-b.height)<.001f; }
static bool same_color(Color a,Color b) { return a.r==b.r && a.g==b.g && a.b==b.b && a.a==b.a; }
static unsigned painted(PaintKind kind) { unsigned count=0; for (unsigned i=0;i<paint.count;i++) count+=paint.calls[i].kind==kind; return count; }
static Paint *find_paint(PaintKind kind) { for (unsigned i=0;i<paint.count;i++) if (paint.calls[i].kind==kind) return &paint.calls[i]; fputs("Missing painter call\n",stderr); exit(1); }
void DrawRectangleRec(Rectangle r,Color color) { record_paint(PAINT_RECT,r,color,0); }
void DrawRectangleRounded(Rectangle r,float roundness,int segments,Color color) { (void)segments; record_paint(PAINT_ROUND,r,color,roundness); }
void DrawRectangleRoundedLinesEx(Rectangle r,float roundness,int segments,float width,Color color) { (void)roundness; (void)segments; record_paint(PAINT_ROUND_LINE,r,color,width); }
bool IsMouseButtonDown(int button) { return button==MOUSE_BUTTON_LEFT && paint.down; }
bool ui_control_input(UIContext *u,Rectangle r,bool enabled,bool *hover) { paint.inputs++; paint.input=r; paint.input_enabled=enabled && !u->pointer_blocked; *hover=paint.input_enabled && paint.hover; return paint.input_enabled && paint.click; }
bool ui_tool_button(UIContext *u,Rectangle r,const char *label,bool active,bool enabled) { (void)label; (void)active; bool hover=false; return ui_control_input(u,r,enabled,&hover); }
void ui_feedback_colors(UIContext *u,Rectangle r,unsigned kind,const Color base[3],Color colors[3]) { (void)u; (void)r; (void)base; (void)colors; paint.feedback_kind=kind; }
float ui_measure(UIContext *u,const char *text,float size) { return (float)strlen(text)*size*u->scale*.5f; }
void ui_label(UIContext *u,const char *text,Rectangle r,float size,Color color,bool right,UIFontWeight weight) { (void)u; (void)right; Paint *p=record_paint(PAINT_LABEL,r,color,size); p->weight=weight; snprintf(p->text,sizeof(p->text),"%s",text); }
void ui_carbon(UIContext *u,UICarbonIcon icon,Rectangle r,Color color) { (void)u; record_paint(PAINT_ICON,r,color,0)->icon=icon; }
void ui_border(Rectangle r,Rectangle clip,float width,Color color) { Paint *p=record_paint(PAINT_BORDER,r,color,width); p->clip=clip; p->clipping=true; }
void ui_focus_border(Rectangle r,Rectangle clip,float width) { Paint *p=record_paint(PAINT_FOCUS,r,UI_FOCUS,width); p->clip=clip; p->clipping=true; }
bool ui_scissor_begin(UIContext *u,Rectangle clip) {
    paint.begins++; if (!paint.scissor || u->scissor_depth>=8) return false;
    unsigned depth=u->scissor_depth++; u->saved_clips[depth]=u->clip; u->saved_clipping[depth]=u->clipping;
    u->clip=u->clipping ? GetCollisionRec(u->clip,clip) : clip; u->clipping=true; return true;
}
void ui_scissor_end(UIContext *u) { paint.ends++; if (!u->scissor_depth) abort(); unsigned depth=--u->scissor_depth; u->clip=u->saved_clips[depth]; u->clipping=u->saved_clipping[depth]; }

static void test_select_geometry(void) {
    const float scales[]={.8f,1,1.5f,2.5f},widths[]={48,53,136,196,208};
    for (size_t s=0;s<sizeof(scales)/sizeof(*scales);s++) for (size_t w=0;w<sizeof(widths)/sizeof(*widths);w++) for (int count=1;count<=8;count++) {
        float factor=scales[s]; UISelectMenuLayout l=ui_select_menu_layout((Vector2){640,640},(Vector2){640,640},factor,widths[w],count);
        CHECK(l.valid && l.count==count && l.visible==count);
        CHECK(l.panel.width==widths[w]*factor && l.panel.height==(2+26*count)*factor && l.row_height==26*factor);
        CHECK(l.panel.x>=5*factor && l.panel.y>=5*factor && l.panel.x+l.panel.width<=640-5*factor+.001f && l.panel.y+l.panel.height<=640-5*factor+.001f);
        CHECK(l.items.x==l.panel.x+factor && l.items.y==l.panel.y+factor && l.items.width==l.panel.width-2*factor);
        for (int row=0;row<count;row++) { Rectangle r=ui_select_menu_row(&l,row); CHECK(same_rect(r,(Rectangle){l.items.x,l.items.y+row*26*factor,l.items.width,26*factor})); CHECK(r.y+r.height<=l.panel.y+l.panel.height-factor+.001f); }
        CHECK(ui_select_menu_row(&l,-1).width==0 && ui_select_menu_row(&l,count).width==0);
    }
    UISelectMenuLayout l=ui_select_menu_layout((Vector2){1210,988},(Vector2){1440,988},1,48,6);
    CHECK(l.valid && same_rect(l.panel,(Rectangle){1210,825,48,158}) && same_rect(l.items,(Rectangle){1211,826,46,156}));
    l=ui_select_menu_layout((Vector2){640,200},(Vector2){640,200},1.5f,196,64);
    CHECK(l.valid && l.visible==4 && l.count==64 && same_rect(l.panel,(Rectangle){338.5f,33.5f,294,159}));
    l=ui_select_menu_layout((Vector2){FLT_MAX,-FLT_MAX},(Vector2){640,640},1,FLT_MAX,64);
    CHECK(l.valid && l.visible==24 && same_rect(l.panel,(Rectangle){5,5,630,626}));
    const float invalid[]={NAN,INFINITY,-INFINITY,0,-1};
    for (size_t i=0;i<sizeof(invalid)/sizeof(*invalid);i++) CHECK(!ui_select_menu_layout((Vector2){0},(Vector2){640,640},invalid[i],196,6).valid);
    CHECK(!ui_select_menu_layout((Vector2){NAN,0},(Vector2){640,640},1,196,6).valid);
    CHECK(!ui_select_menu_layout((Vector2){0},(Vector2){640,INFINITY},1,196,6).valid);
    CHECK(!ui_select_menu_layout((Vector2){0},(Vector2){12,12},1,196,6).valid);
    CHECK(!ui_select_menu_layout((Vector2){0},(Vector2){640,640},1,196,0).valid);
    CHECK(!ui_select_menu_layout((Vector2){0},(Vector2){640,640},1,196,65).valid);
    CHECK(!ui_select_menu_layout((Vector2){0},(Vector2){640,640},1,NAN,6).valid);
    CHECK(ui_select_menu_row(NULL,0).width==0);
}
static void test_select_paint(void) {
    UIContext u={.scale=1}; Rectangle field={10,20,196,25};
    const float sizes[]={12,10.5f,12,12,11};
    for (int style=UI_SELECT_REPRESENTATION;style<=UI_SELECT_FILTER_CONDITION;style++) for (int scale=0;scale<2;scale++) {
        u.scale=scale ? 1.5f : 1; float s=u.scale; Rectangle r={10,20,196*s,25*s};
        reset_paint(&u); paint.hover=paint.down=paint.click=true;
        CHECK(ui_select_field(&u,r,"Between",(UISelectStyle)style,false,true));
        CHECK(paint.inputs==1 && same_rect(paint.input,r) && paint.input_enabled && paint.count==4);
        CHECK(paint.calls[0].kind==PAINT_ROUND && same_color(paint.calls[0].color,UI_BORDER) && same_rect(paint.calls[0].rect,r));
        CHECK(paint.calls[1].kind==PAINT_ROUND && same_color(paint.calls[1].color,WHITE) && same_rect(paint.calls[1].rect,(Rectangle){r.x+s,r.y+s,r.width-2*s,r.height-2*s}));
        Paint *label=find_paint(PAINT_LABEL),*icon=find_paint(PAINT_ICON); float padding=style==UI_SELECT_FONT_SIZE ? 4*s : 8*s;
        CHECK(label->size==sizes[style] && label->weight==UI_REGULAR && !strcmp(label->text,"Between") && same_color(label->color,UI_TEXT));
        CHECK(same_rect(label->rect,(Rectangle){r.x+padding,r.y,r.width-2*padding-14*s,r.height}));
        CHECK(icon->icon==CARBON_CHEVRON_DOWN && same_rect(icon->rect,(Rectangle){r.x+r.width-padding-11*s,r.y+7*s,11*s,11*s}));
        reset_paint(&u); CHECK(!ui_select_field(&u,r,"Between",(UISelectStyle)style,true,true));
        CHECK(same_color(paint.calls[0].color,UI_FOCUS) && paint.calls[1].kind==PAINT_RECT && same_color(paint.calls[1].color,WHITE));
        CHECK(same_rect(paint.calls[1].rect,(Rectangle){r.x+2*s,r.y+2*s,r.width-4*s,r.height-4*s}));
        reset_paint(&u); paint.click=paint.hover=true; CHECK(!ui_select_field(&u,r,"Between",(UISelectStyle)style,true,false));
        CHECK(!paint.input_enabled && same_color(paint.calls[0].color,UI_BORDER) && paint.calls[1].kind==PAINT_ROUND);
        CHECK(same_color(find_paint(PAINT_LABEL)->color,(Color){153,160,172,255}));
    }
    u.scale=1; UISelectMenuLayout menu=ui_select_menu_layout((Vector2){10,47},(Vector2){640,640},1,196,6);
    for (int style=UI_SELECT_REPRESENTATION;style<=UI_SELECT_FILTER_CONDITION;style++) {
        reset_paint(&u); paint.click=true; CHECK(ui_select_menu_option(&u,&menu,2,"Between",(UISelectStyle)style,true,true,true,true));
        Rectangle row=ui_select_menu_row(&menu,2); Paint *label=find_paint(PAINT_LABEL),*icon=find_paint(PAINT_ICON);
        CHECK(paint.count==4 && label->size==sizes[style] && label->weight==UI_REGULAR && !strcmp(label->text,"Between"));
        CHECK(icon->icon==CARBON_CHECKMARK && same_rect(icon->rect,(Rectangle){row.x+4,row.y+7,12,12}));
        CHECK(same_rect(label->rect,(Rectangle){row.x+20,row.y,row.width-24,26}) && same_color(label->color,UI_TEXT));
        CHECK(painted(PAINT_FOCUS)==1 && label->clipping && same_rect(label->clip,row) && !u.clipping && !u.scissor_depth);
    }
    reset_paint(&u); CHECK(!ui_select_menu_option(&u,&menu,0,"Any value",UI_SELECT_FILTER_CONDITION,false,false,true,true));
    CHECK(paint.count==1 && painted(PAINT_ICON)==0 && find_paint(PAINT_LABEL)->size==11);
    reset_paint(&u); paint.click=paint.hover=paint.down=true; CHECK(!ui_select_menu_option(&u,&menu,0,"Any value",UI_SELECT_FILTER_CONDITION,true,true,true,false));
    CHECK(!paint.input_enabled && painted(PAINT_ICON)==1 && painted(PAINT_FOCUS)==1 && same_color(find_paint(PAINT_LABEL)->color,UI_TEXT));
    reset_paint(&u); paint.click=paint.hover=true; CHECK(!ui_select_menu_option(&u,&menu,0,"Any value",UI_SELECT_FILTER_CONDITION,true,true,false,true));
    CHECK(!paint.input_enabled && paint.count==2 && painted(PAINT_ICON)==1 && !painted(PAINT_FOCUS) && !painted(PAINT_RECT));
    CHECK(same_color(find_paint(PAINT_LABEL)->color,(Color){153,160,172,255}));
    u.pointer_blocked=true; reset_paint(&u); paint.click=true; CHECK(!ui_select_field(&u,field,"Any value",UI_SELECT_FILTER_CONDITION,false,true)); CHECK(!paint.input_enabled); u.pointer_blocked=false;
    reset_paint(&u); CHECK(!ui_select_field(&u,field,NULL,UI_SELECT_FILTER_CONDITION,false,true)); CHECK(!*find_paint(PAINT_LABEL)->text);
    reset_paint(&u); ui_select_menu_frame(&u,&menu); CHECK(paint.count==3 && painted(PAINT_RECT)==2 && painted(PAINT_BORDER)==1 && same_rect(paint.calls[1].rect,menu.panel) && same_color(paint.calls[1].color,WHITE));
}
static void test_select_bounds_and_clip(void) {
    UIContext u={.scale=1,.clip={10,20,90,25},.clipping=true}; Rectangle field={10,20,196,25},original=u.clip;
    reset_paint(&u); CHECK(!ui_select_field(&u,field,"Contains",UI_SELECT_FILTER_CONDITION,true,true));
    CHECK(paint.begins==1 && paint.ends==1 && !u.scissor_depth && u.clipping && same_rect(u.clip,original));
    for (unsigned i=0;i<paint.count;i++) CHECK(paint.calls[i].clipping && same_rect(paint.calls[i].clip,original));
    reset_paint(&u); paint.scissor=false; CHECK(!ui_select_field(&u,field,"Contains",UI_SELECT_FILTER_CONDITION,true,true));
    CHECK(paint.begins==1 && !paint.ends && !u.scissor_depth && same_rect(u.clip,original));
    CHECK(paint.calls[0].kind==PAINT_RECT && same_rect(paint.calls[0].rect,original) && painted(PAINT_ROUND)==0 && painted(PAINT_BORDER)==1);
    UISelectMenuLayout menu=ui_select_menu_layout((Vector2){10,47},(Vector2){640,640},1,196,6);
    Rectangle row=ui_select_menu_row(&menu,0); u.clip=(Rectangle){row.x+20,row.y+2,40,10}; original=u.clip;
    reset_paint(&u); CHECK(!ui_select_menu_option(&u,&menu,0,"Contains",UI_SELECT_FILTER_CONDITION,true,true,true,true));
    CHECK(u.clipping && same_rect(u.clip,original) && !u.scissor_depth && !paint.begins && !paint.ends);
    for (unsigned i=0;i<paint.count;i++) CHECK(paint.calls[i].clipping && same_rect(paint.calls[i].clip,original));
    u.clip=(Rectangle){-100,-100,10,10}; reset_paint(&u);
    CHECK(!ui_select_field(&u,field,"Contains",UI_SELECT_FILTER_CONDITION,true,true)); CHECK(!ui_select_menu_option(&u,&menu,0,"Contains",UI_SELECT_FILTER_CONDITION,true,true,true,true));
    CHECK(!paint.count && !paint.inputs); u.clipping=false;
    for (int style=-1;style<=UI_SELECT_FILTER_CONDITION+1;style+=UI_SELECT_FILTER_CONDITION+2) {
        reset_paint(&u); CHECK(!ui_select_field(&u,field,"Contains",(UISelectStyle)style,true,true));
        CHECK(!ui_select_menu_option(&u,&menu,0,"Contains",(UISelectStyle)style,true,true,true,true)); CHECK(!paint.count && !paint.inputs);
    }
    const float invalid[]={NAN,INFINITY,-INFINITY,0,-1};
    for (size_t i=0;i<sizeof(invalid)/sizeof(*invalid);i++) {
        u.scale=invalid[i]; reset_paint(&u); CHECK(!ui_select_field(&u,field,"Contains",UI_SELECT_FILTER_CONDITION,true,true));
        CHECK(!ui_select_menu_option(&u,&menu,0,"Contains",UI_SELECT_FILTER_CONDITION,true,true,true,true)); CHECK(!paint.count && !paint.inputs);
    }
    u.scale=1; const Rectangle invalid_rects[]={{NAN,0,196,25},{0,INFINITY,196,25},{0,0,INFINITY,25},{0,0,196,NAN},{0,0,2,25},{0,0,196,2}};
    for (size_t i=0;i<sizeof(invalid_rects)/sizeof(*invalid_rects);i++) { reset_paint(&u); CHECK(!ui_select_field(&u,invalid_rects[i],"Contains",UI_SELECT_FILTER_CONDITION,true,true)); CHECK(!paint.count && !paint.inputs); }
    reset_paint(&u); CHECK(!ui_select_field(NULL,field,"Contains",UI_SELECT_FILTER_CONDITION,true,true));
    CHECK(!ui_select_menu_option(NULL,&menu,0,"Contains",UI_SELECT_FILTER_CONDITION,true,true,true,true));
    CHECK(!ui_select_menu_option(&u,&menu,-1,"Contains",UI_SELECT_FILTER_CONDITION,true,true,true,true));
    CHECK(!ui_select_menu_option(&u,&menu,menu.visible,"Contains",UI_SELECT_FILTER_CONDITION,true,true,true,true)); CHECK(!paint.count && !paint.inputs);
}
static void test_choice_row_paint(void) {
    const float scales[]={1,1.5f};
    for (unsigned i=0;i<sizeof(scales)/sizeof(*scales);i++) {
        float s=scales[i]; UIContext u={.scale=s}; Rectangle row={10,20,230*s,28*s};
        reset_paint(&u); CHECK(!ui_choice_row(&u,row,"Open workbook","O",CARBON_FOLDER_OPEN,false,false,true));
        CHECK(paint.inputs==1 && paint.input_enabled && paint.feedback_kind==8 && !painted(PAINT_RECT) && !painted(PAINT_ROUND) && !painted(PAINT_BORDER));
        Paint *icon=find_paint(PAINT_ICON),*label=find_paint(PAINT_LABEL);
        CHECK(icon->icon==CARBON_FOLDER_OPEN && same_color(icon->color,(Color){65,110,138,255}));
        CHECK(same_rect(icon->rect,(Rectangle){row.x+8.5f*s,row.y+6*s,16*s,16*s}));
        CHECK(label->size==11.5f && same_color(label->color,(Color){39,63,81,255}) && !strcmp(label->text,"Open workbook"));
        CHECK(paint.calls[2].kind==PAINT_LABEL && paint.calls[2].size==11 && same_color(paint.calls[2].color,(Color){94,110,124,255}));
        CHECK(same_rect(paint.calls[2].rect,(Rectangle){row.x+row.width-20.5f*s,row.y,13.5f*s,row.height}));
        for (unsigned c=0;c<paint.count;c++) CHECK(paint.calls[c].clipping && same_rect(paint.calls[c].clip,row));
        CHECK(!u.clipping && !u.scissor_depth);

        reset_paint(&u); paint.hover=true; CHECK(!ui_choice_row(&u,row,"Open",NULL,CARBON_COUNT,false,false,true));
        CHECK(painted(PAINT_ROUND)==1 && !painted(PAINT_BORDER) && same_color(find_paint(PAINT_ROUND)->color,(Color){231,240,246,255}));
        CHECK(fabsf(find_paint(PAINT_ROUND)->size-4*s/row.height)<.0001f);
        reset_paint(&u); paint.hover=paint.down=true; CHECK(!ui_choice_row(&u,row,"Open",NULL,CARBON_COUNT,false,false,true));
        CHECK(same_color(find_paint(PAINT_ROUND)->color,(Color){212,228,238,255}));
        reset_paint(&u); paint.click=true; CHECK(ui_choice_row(&u,row,"Open",NULL,CARBON_COUNT,false,false,true));
        CHECK(same_color(find_paint(PAINT_ROUND)->color,(Color){212,228,238,255}));
        reset_paint(&u); CHECK(!ui_choice_row(&u,row,"Visible","V",CARBON_CLOSE,true,true,true));
        CHECK(find_paint(PAINT_ICON)->icon==CARBON_CHECKMARK && painted(PAINT_FOCUS)==1 && same_rect(find_paint(PAINT_FOCUS)->clip,row));

        reset_paint(&u); paint.hover=paint.click=paint.down=true; CHECK(!ui_choice_row(&u,row,"Disabled","D",CARBON_CLOSE,false,true,false));
        CHECK(!paint.input_enabled && !painted(PAINT_ROUND) && !painted(PAINT_RECT) && !painted(PAINT_FOCUS));
        CHECK(same_color(find_paint(PAINT_LABEL)->color,Fade((Color){39,63,81,255},.4f)));
        CHECK(same_color(find_paint(PAINT_ICON)->color,Fade((Color){65,110,138,255},.4f)));
        CHECK(same_color(paint.calls[2].color,Fade((Color){94,110,124,255},.4f)));
        reset_paint(&u); paint.hover=paint.click=true; CHECK(!ui_choice_row_input(&u,row,"Blocked",NULL,CARBON_CLOSE,false,true,true,false));
        CHECK(!paint.input_enabled && !u.pointer_blocked && painted(PAINT_FOCUS)==1 && same_color(find_paint(PAINT_LABEL)->color,(Color){39,63,81,255}));
        u.pointer_blocked=true; reset_paint(&u); CHECK(!ui_choice_row_input(&u,row,"Blocked",NULL,CARBON_COUNT,false,false,true,true)); CHECK(u.pointer_blocked && !paint.input_enabled); u.pointer_blocked=false;
    }
}
static void test_choice_row_bounds_and_clip(void) {
    UIContext u={.scale=1,.clip={40,22,90,18},.clipping=true}; Rectangle original=u.clip,row={10,20,230,28};
    reset_paint(&u); paint.hover=true; CHECK(!ui_choice_row(&u,row,"Very long action name","Shortcut",CARBON_CLOSE,false,true,true));
    CHECK(paint.begins==1 && paint.ends==1 && !u.scissor_depth && u.clipping && same_rect(u.clip,original));
    for (unsigned i=0;i<paint.count;i++) CHECK(paint.calls[i].clipping && same_rect(paint.calls[i].clip,original));
    CHECK(same_rect(find_paint(PAINT_FOCUS)->rect,row) && same_rect(find_paint(PAINT_FOCUS)->clip,original));
    reset_paint(&u); paint.scissor=false; paint.hover=true; CHECK(!ui_choice_row(&u,row,"Open",NULL,CARBON_COUNT,false,true,true));
    CHECK(paint.begins==1 && !paint.ends && !painted(PAINT_ROUND) && painted(PAINT_RECT)==1 && same_rect(find_paint(PAINT_RECT)->rect,original));
    CHECK(!u.scissor_depth && same_rect(u.clip,original));
    u.clip=(Rectangle){-100,-100,20,20}; reset_paint(&u); paint.click=true;
    CHECK(!ui_choice_row(&u,row,"Invisible",NULL,CARBON_COUNT,false,true,true)); CHECK(!paint.count && !paint.inputs);
    u.clipping=false;
    const Rectangle invalid[]={{NAN,0,230,28},{0,INFINITY,230,28},{0,0,INFINITY,28},{0,0,230,NAN},{0,0,0,28},{0,0,230,-1}};
    for (unsigned i=0;i<sizeof(invalid)/sizeof(*invalid);i++) { reset_paint(&u); CHECK(!ui_choice_row(&u,invalid[i],"Invalid",NULL,CARBON_COUNT,false,true,true)); CHECK(!paint.count && !paint.inputs); }
    const float scales[]={NAN,INFINITY,-1,0};
    for (unsigned i=0;i<sizeof(scales)/sizeof(*scales);i++) { u.scale=scales[i]; reset_paint(&u); CHECK(!ui_choice_row(&u,row,"Invalid",NULL,CARBON_COUNT,false,true,true)); CHECK(!paint.count && !paint.inputs); }
    CHECK(!ui_choice_row(NULL,row,"Invalid",NULL,CARBON_COUNT,false,true,true));
    CHECK(!ui_choice_row_input(NULL,row,"Invalid",NULL,CARBON_COUNT,false,true,true,true));
    u.scale=1; reset_paint(&u); CHECK(!ui_choice_row(&u,row,NULL,NULL,CARBON_COUNT,false,false,true)); CHECK(!*find_paint(PAINT_LABEL)->text);
}
int main(void) {
    const float scales[]={.8f,1,1.5f,2.5f}; const Vector2 windows[]={{1440,988},{640,640},{1280,820}};
    for (int s=0;s<4;s++) for (int w=0;w<3;w++) for (int count=3;count<=4;count++) for (int sep=-1;sep<count;sep+=count) for (int corner=0;corner<4;corner++) {
        Vector2 viewport=windows[w],origin={corner&1 ? viewport.x : 0,corner&2 ? viewport.y : 0}; float scale=scales[s];
        UIChoiceLayout l=ui_choice_layout(origin,viewport,scale,count,sep);
        CHECK(l.valid); CHECK(l.count==count); CHECK(l.panel.width==fminf(240,viewport.x/scale-12)*scale);
        CHECK(l.panel.height==(40+28*count+(sep>=0 ? 9 : 0))*scale);
        CHECK(l.panel.x>=0 && l.panel.y>=0); CHECK(l.panel.x+l.panel.width<=viewport.x); CHECK(l.panel.y+l.panel.height<=viewport.y);
        CHECK(l.title.height==30*scale);
        for (int i=0;i<count;i++) { CHECK(l.rows[i].height==28*scale); CHECK(l.rows[i].width==l.panel.width-10*scale); CHECK(l.rows[i].y>=l.title.y+l.title.height); CHECK(l.rows[i].y+l.rows[i].height<=l.panel.y+l.panel.height); }
        CHECK((l.separator.height>0)==(sep>=0));
    }
    UIChoiceLayout scale=ui_choice_layout((Vector2){1210,988},(Vector2){1440,988},1,4,-1);
    CHECK(scale.panel.x==1194); CHECK(scale.panel.y==812); CHECK(scale.panel.height==152);
    UIChoiceLayout ribbon=ui_choice_layout((Vector2){1210,58},(Vector2){1440,988},1,4,3);
    CHECK(ribbon.panel.height==161); CHECK(ribbon.rows[3].y-ribbon.rows[2].y==37);
    CHECK(!ui_choice_layout((Vector2){NAN,0},(Vector2){1440,988},1,4,-1).valid);
    CHECK(!ui_choice_layout((Vector2){0,0},(Vector2){1440,988},0,4,-1).valid);
    CHECK(!ui_choice_layout((Vector2){0,0},(Vector2){1440,988},1,UI_CHOICE_MAX+1,-1).valid);
    for (int s=0;s<3;s++) for (int w=0;w<3;w++) for (int corner=0;corner<4;corner++) {
        Vector2 viewport=windows[w],origin={corner&1 ? viewport.x : 0,corner&2 ? viewport.y : 0}; float factor=scales[s];
        UIChoiceLayout fields=ui_choice_layout(origin,viewport,factor,9,3);
        CHECK(fields.valid && fields.count==9 && fields.panel.height==301*factor);
        CHECK(fields.panel.x>=0 && fields.panel.x+fields.panel.width<=viewport.x);
        CHECK(fields.panel.y>=0 && fields.panel.y+fields.panel.height<=viewport.y);
        for (int i=0;i<9;i++) { CHECK(fields.rows[i].height==28*factor); CHECK(fields.rows[i].y+fields.rows[i].height<=fields.panel.y+fields.panel.height+.001f); }
        CHECK(fabsf(fields.rows[3].y-fields.rows[2].y-37*factor)<.001f);
    }
    CHECK(!ui_choice_layout((Vector2){0,0},(Vector2){1440,988},1,4,4).valid);
    CHECK(!ui_choice_layout((Vector2){0,0},(Vector2){160,100},1,4,-1).valid);
    for (int s=0;s<4;s++) for (int w=0;w<3;w++) for (int count=1;count<=16384;count=count<12 ? count+1 : count*8) {
        UIChoiceListLayout list=ui_choice_list_layout((Vector2){windows[w].x,windows[w].y},windows[w],scales[s],count);
        CHECK(list.frame.valid && list.visible>=1 && list.visible<=count && list.visible<=UI_CHOICE_MAX);
        CHECK(list.frame.panel.width==240*scales[s]);
        CHECK(list.frame.panel.y+list.frame.panel.height<=windows[w].y);
        CHECK(list.items.height==list.visible*28*scales[s]);
        CHECK(list.items.y==list.frame.title.y+list.frame.title.height);
        CHECK(list.items.y+list.items.height<=list.frame.panel.y+list.frame.panel.height);
    }
    UIChoiceListLayout list=ui_choice_list_layout((Vector2){1000,620},(Vector2){1020,640},1.5f,11);
    CHECK(list.frame.valid && list.visible==10 && list.frame.panel.width==360 && list.frame.panel.height==480);
    CHECK(!ui_choice_list_layout((Vector2){0},(Vector2){1280,820},NAN,11).frame.valid);
    CHECK(!ui_choice_list_layout((Vector2){0},(Vector2){1280,820},1,0).frame.valid);
    CHECK(!ui_choice_list_layout((Vector2){0},(Vector2){1280,100},2,11).frame.valid);
    test_select_geometry(); test_select_paint(); test_select_bounds_and_clip(); test_choice_row_paint(); test_choice_row_bounds_and_clip();
    printf("PASS: %d choice-menu checks (exact scale/density/ribbon geometry, compact select and action-row paints, condition type, ownership guards, clipping and 80/100/150/250 percent)\n",checks); return 0;
}
