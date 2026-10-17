#include "guideview.h"
#include <math.h>
#include <string.h>
static const char *names[]={"Grid","Fields and shelves","Panes","Scaling","Status"};
static const char *descriptions[]={
    "Click or drag cells. Shift extends selection. Drag a header boundary; double-click to fit.",
    "Create view, then drag fields to Rows, Columns, Measures, or Marks. Field menus provide keyboard alternatives.",
    "Transform opens on the right. Collapse retains an entry; Close removes it entirely. Resize either dock by its edge. Narrow windows tuck Workbook and Fields behind the workspace button.",
    "UI scale changes the whole interface. The adjacent zoom control changes the grid only. View offers row density and focus mode.",
    "Right-click the bottom bar to choose which selection statistics appear."
};
static const char *labels[]={"Move / extend selection","Cycle visible regions","Filter selected column","Next / previous sheet","Transformations / Fields","Focus mode"};
static const char *keys[]={"Arrows / Shift+Arrows","F6 / Shift+F6","Alt+↓","Ctrl+Tab / Ctrl+Shift+Tab","⌘/Ctrl+Alt+T / F","⌘/Ctrl+Shift+F"};
static const char *intro="Explore the workspace, transitions, and interaction details.";
static const char *note="Each view uses its own source and filters. Preview historical steps without changing your workbook; inspect SQL to see how a result is calculated.";
#ifdef DUXCEL_DEV_REVIEW
static const char *section_keys[]={"grid","fields","panes","scaling","status"};
static const char *shortcut_keys[]={"selection","regions","filter","sheets","panes","focus"};
static bool guide_review_text(char *to,size_t capacity,const char *raw) {
    size_t length=strlen(raw),used=length<capacity ? length : capacity-1;
    while (used && ((unsigned char)raw[used]&0xc0)==0x80) used--;
    memcpy(to,raw,used); to[used]=0; return used!=length;
}
static void guide_review_record(UIGuide *guide,GuideReviewKind kind,const char *key,const char *raw,size_t text_line,Rectangle bounds,Rectangle clip,bool focused,bool container,const char *file,uint32_t line) {
    if (!guide->review.armed) return;
    Rectangle visible=GetCollisionRec(bounds,clip); if (visible.width<=0 || visible.height<=0) return;
    if (guide->review.count>=GUIDE_REVIEW_TARGETS) { guide->review.overflow=true; return; }
    GuideReviewTarget *target=&guide->review.targets[guide->review.count++];
    *target=(GuideReviewTarget){.kind=kind,.bounds=bounds,.clip=clip,.text_line=text_line,.enabled=true,.focused=focused,.container=container,.file=file,.source_line=line};
    target->truncated=guide_review_text(target->key,sizeof(target->key),key);
    target->truncated|=guide_review_text(target->text,sizeof(target->text),raw);
}
#define GUIDE_REVIEW(g,...) do { if ((g)->review.armed) guide_review_record((g),__VA_ARGS__,__FILE__,__LINE__); } while (0)
#define GUIDE_TEXT_CONTEXT UIGuide *guide,GuideReviewKind review_kind,const char *review_key,
#define GUIDE_TEXT(g,kind,key,...) text((g),(kind),(key),__VA_ARGS__)
#else
#define GUIDE_REVIEW(...) ((void)0)
#define GUIDE_TEXT_CONTEXT
#define GUIDE_TEXT(g,kind,key,...) text(__VA_ARGS__)
#endif
UIGuideLayout ui_guide_layout(Vector2 viewport,float scale,const float heights[5],const float shortcut_heights[6],float intro_height,float note_height,float key_width) {
    UIGuideLayout out={0};
    if (!isfinite(scale) || scale<=0 || !isfinite(viewport.x) || !isfinite(viewport.y) || !heights || !shortcut_heights) return out;
    float width=fminf(520,viewport.x/scale-30),maximum=viewport.y/scale-40;
    if (width<180 || maximum<180 || !isfinite(intro_height) || !isfinite(note_height) || !isfinite(key_width) || intro_height<0 || note_height<0 || key_width<0) return out;
    float y=50+intro_height+13,list=y;
    for (int i=0;i<5;i++) { if (!isfinite(heights[i]) || heights[i]<17) return out; y+=heights[i]+(i<4 ? 11 : 0); }
    float shortcut_y=y+12,shortcut_height=20;
    for (int i=0;i<6;i++) { if (!isfinite(shortcut_heights[i]) || shortcut_heights[i]<14.5f) return out; shortcut_height+=shortcut_heights[i]+(i<5 ? 7 : 0); }
    float note_y=shortcut_y+shortcut_height+13,start_y=note_y+note_height+13;
    out.content_height=start_y+29+16; float height=fminf(out.content_height,maximum),x=(viewport.x/scale-width)*.5f,top=(viewport.y/scale-height)*.5f;
    out.panel=(Rectangle){x*scale,top*scale,width*scale,height*scale}; out.clip=(Rectangle){(x+1)*scale,(top+1)*scale,(width-2)*scale,(height-2)*scale};
    out.header=(Rectangle){(x+11)*scale,(top+1)*scale,(width-54)*scale,36*scale}; out.close=(Rectangle){(x+width-34)*scale,(top+7.5f)*scale,23*scale,23*scale};
    out.intro=(Rectangle){(x+19)*scale,(top+50)*scale,(width-38)*scale,intro_height*scale};
    y=list;
    for (int i=0;i<5;i++) { out.names[i]=(Rectangle){(x+19)*scale,(top+y)*scale,90*scale,heights[i]*scale}; out.descriptions[i]=(Rectangle){(x+119)*scale,(top+y)*scale,(width-138)*scale,heights[i]*scale}; y+=heights[i]+11; }
    out.shortcuts=(Rectangle){(x+1)*scale,(top+shortcut_y)*scale,(width-2)*scale,shortcut_height*scale};
    key_width=fminf(key_width,(width-53)*.5f); y=shortcut_y+10;
    for (int i=0;i<6;i++) { out.labels[i]=(Rectangle){(x+19)*scale,(top+y)*scale,(width-53-key_width)*scale,shortcut_heights[i]*scale}; out.keys[i]=(Rectangle){(x+width-19-key_width)*scale,(top+y)*scale,key_width*scale,shortcut_heights[i]*scale}; y+=shortcut_heights[i]+7; }
    out.note=(Rectangle){(x+19)*scale,(top+note_y)*scale,(width-38)*scale,note_height*scale}; out.start=(Rectangle){(x+width-135)*scale,(top+start_y)*scale,116*scale,29*scale}; out.valid=true; return out;
}
static void wrap(TextLines *lines,UIContext *ui,const char *text,float width,float size) {
    if (!lines->offsets || lines->width!=width || lines->scale!=ui->scale || lines->raster_size!=ui->raster_size || lines->font_size!=size) text_lines_layout_sized(lines,ui,text,width,size);
}
static Rectangle shifted(Rectangle rect,float offset) { rect.y-=offset; return rect; }
static void text(GUIDE_TEXT_CONTEXT UIContext *ui,const TextLines *lines,const char *raw,Rectangle rect,Rectangle clip,float size,float line_height,Color color,UIFontWeight weight) {
    ui_set_clip(ui,GetCollisionRec(rect,clip));
    for (size_t i=0;i<lines->count;i++) {
        Rectangle line={rect.x,rect.y+i*line_height*ui->scale,rect.width,line_height*ui->scale};
        if (line.y+line.height<=clip.y || line.y>=clip.y+clip.height) continue;
        size_t start=lines->offsets[i],length=lines->offsets[i+1]-start; char label[1024]; if (length>=sizeof(label)) length=sizeof(label)-1;
        memcpy(label,raw+start,length); label[length]=0; ui_label(ui,label,line,size,color,false,weight);
        GUIDE_REVIEW(guide,review_kind,review_key,label,i,line,ui->clip,false,false);
    }
    ui_clear_clip(ui);
}
bool ui_guide_draw(UIContext *ui,UIGuide *guide,bool pointer) {
#ifdef DUXCEL_DEV_REVIEW
    if (guide->review.armed) { guide->review.count=0; guide->review.overflow=false; }
#endif
    float s=ui->scale,width=fminf(520,GetScreenWidth()/s-30),key_width=0,heights[5],shortcut_heights[6];
    for (int i=0;i<6;i++) key_width=fmaxf(key_width,ui_measure(ui,keys[i],11)/s);
    key_width=fminf(key_width,(width-53)*.5f);
    for (int i=0;i<5;i++) { wrap(&guide->names[i],ui,names[i],90*s,12); wrap(&guide->descriptions[i],ui,descriptions[i],(width-138)*s,12); heights[i]=17*fmaxf(guide->names[i].count,guide->descriptions[i].count); }
    for (int i=0;i<6;i++) { wrap(&guide->labels[i],ui,labels[i],(width-53-key_width)*s,11); shortcut_heights[i]=14.5f*guide->labels[i].count; }
    wrap(&guide->intro,ui,intro,(width-38)*s,12); wrap(&guide->note,ui,note,(width-38)*s,10.5f);
    UIGuideLayout layout=ui_guide_layout((Vector2){GetScreenWidth(),GetScreenHeight()},s,heights,shortcut_heights,guide->intro.count*18,guide->note.count*16,key_width); guide->layout=layout;
    if (!layout.valid) return false;
    float maximum=fmaxf(0,layout.content_height-layout.panel.height/s);
    if (pointer && ui_pressed(KEY_ESCAPE)) return true;
    if (pointer && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(GetMousePosition(),layout.panel)) return true;
    if (pointer && CheckCollisionPointRec(GetMousePosition(),layout.panel)) guide->scroll-=GetMouseWheelMoveV().y*42;
    if (pointer && ui_key(KEY_UP)) guide->scroll-=24; if (pointer && ui_key(KEY_DOWN)) guide->scroll+=24;
    if (pointer && ui_key(KEY_PAGE_UP)) guide->scroll-=layout.clip.height/s-30; if (pointer && ui_key(KEY_PAGE_DOWN)) guide->scroll+=layout.clip.height/s-30;
    if (pointer && ui_pressed(KEY_HOME)) guide->scroll=0; if (pointer && ui_pressed(KEY_END)) guide->scroll=maximum;
    if (pointer && ui_pressed(KEY_TAB)) {
        guide->focus=1-guide->focus; Rectangle target=guide->focus ? layout.start : layout.close;
        if (target.y-guide->scroll*s<layout.clip.y) guide->scroll=(target.y-layout.clip.y)/s;
        if (target.y+target.height-guide->scroll*s>layout.clip.y+layout.clip.height) guide->scroll=(target.y+target.height-layout.clip.y-layout.clip.height)/s;
    }
    guide->scroll=fmaxf(0,fminf(maximum,guide->scroll)); float scroll=guide->scroll*s;
#ifdef DUXCEL_DEV_REVIEW
    if (guide->review.armed) { guide->review.scroll=guide->scroll; guide->review.maximum=maximum; guide->review.focus=guide->focus; }
#endif
    DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),(Color){35,52,66,64});
    for (int i=12;i>0;i--) DrawRectangleRounded((Rectangle){layout.panel.x-i*s,layout.panel.y+(9-i)*s,layout.panel.width+2*i*s,layout.panel.height+2*i*s},.015f,4,(Color){38,59,78,3});
    DrawRectangleRounded(layout.panel,.015f,4,WHITE); DrawRectangleRoundedLinesEx(layout.panel,.015f,4,s,(Color){183,201,215,255});
    GUIDE_REVIEW(guide,GUIDE_REVIEW_PANEL,"panel","Explore the Duxcel design",0,layout.panel,((Rectangle){0,0,GetScreenWidth(),GetScreenHeight()}),false,true);
    ui_set_clip(ui,layout.clip); Rectangle heading=shifted(layout.header,scroll),close=shifted(layout.close,scroll);
    ui_label(ui,"Explore the Duxcel design",heading,12.5f,UI_TEXT,false,UI_MEDIUM);
    GUIDE_REVIEW(guide,GUIDE_REVIEW_HEADING,"heading","Explore the Duxcel design",0,heading,ui->clip,false,false);
    Rectangle rule={layout.clip.x,layout.panel.y+37*s-scroll,layout.clip.width,s}; DrawRectangleRec(GetCollisionRec(rule,layout.clip),(Color){224,231,237,255});
    bool done=ui_icon_button(ui,close,CARBON_CLOSE,false,pointer);
    GUIDE_REVIEW(guide,GUIDE_REVIEW_CLOSE,"close","Close",0,close,ui->clip,guide->focus==0,false);
    if (!guide->focus) ui_border(close,layout.clip,s,UI_FOCUS); ui_clear_clip(ui);
    GUIDE_TEXT(guide,GUIDE_REVIEW_INTRO,"intro",ui,&guide->intro,intro,shifted(layout.intro,scroll),layout.clip,12,18,UI_MUTED,UI_REGULAR);
    for (int i=0;i<5;i++) {
        GUIDE_TEXT(guide,GUIDE_REVIEW_NAME,section_keys[i],ui,&guide->names[i],names[i],shifted(layout.names[i],scroll),layout.clip,12,17,((Color){47,90,116,255}),UI_MEDIUM);
        GUIDE_TEXT(guide,GUIDE_REVIEW_DESCRIPTION,section_keys[i],ui,&guide->descriptions[i],descriptions[i],shifted(layout.descriptions[i],scroll),layout.clip,12,17,((Color){92,114,130,255}),UI_REGULAR);
    }
    DrawRectangleRec(GetCollisionRec(shifted(layout.shortcuts,scroll),layout.clip),(Color){244,247,249,255});
    GUIDE_REVIEW(guide,GUIDE_REVIEW_SHORTCUTS,"shortcuts","Keyboard shortcuts",0,shifted(layout.shortcuts,scroll),layout.clip,false,true);
    for (int i=0;i<6;i++) {
        GUIDE_TEXT(guide,GUIDE_REVIEW_LABEL,shortcut_keys[i],ui,&guide->labels[i],labels[i],shifted(layout.labels[i],scroll),layout.clip,11,14.5f,UI_TEXT,UI_REGULAR);
        ui_set_clip(ui,layout.clip); ui_label(ui,keys[i],shifted(layout.keys[i],scroll),11,UI_MUTED,false,UI_REGULAR); ui_clear_clip(ui);
        GUIDE_REVIEW(guide,GUIDE_REVIEW_KEY,shortcut_keys[i],keys[i],0,shifted(layout.keys[i],scroll),layout.clip,false,false);
    }
    GUIDE_TEXT(guide,GUIDE_REVIEW_NOTE,"note",ui,&guide->note,note,shifted(layout.note,scroll),layout.clip,10.5f,16,UI_MUTED,UI_REGULAR);
    ui_set_clip(ui,layout.clip); Rectangle start=shifted(layout.start,scroll);
    done|=ui_popup_button(ui,start,"",true,false,pointer); float label=ui_measure(ui,"Start exploring",13);
    ui_label(ui,"Start exploring",(Rectangle){start.x+(start.width-label)*.5f,start.y,label,start.height},13,WHITE,false,UI_REGULAR);
    GUIDE_REVIEW(guide,GUIDE_REVIEW_START,"start","Start exploring",0,start,ui->clip,guide->focus!=0,false);
    if (guide->focus) ui_border(start,layout.clip,s,UI_FOCUS); ui_clear_clip(ui);
    if (maximum) {
        Rectangle track={layout.panel.x+layout.panel.width-8*s,layout.clip.y+2*s,6*s,layout.clip.height-4*s};
        ui_scrollbar(ui,track,layout.content_height,layout.panel.height/s,&guide->scroll,false,&guide->scrollbar,pointer);
        GUIDE_REVIEW(guide,GUIDE_REVIEW_SCROLL,"scrollbar","Scroll guide",0,track,((Rectangle){0,0,GetScreenWidth(),GetScreenHeight()}),false,false);
        GUIDE_REVIEW(guide,GUIDE_REVIEW_THUMB,"thumb","Scroll guide",0,ui_scrollbar_thumb(track,layout.content_height,layout.panel.height/s,guide->scroll,false,s),((Rectangle){0,0,GetScreenWidth(),GetScreenHeight()}),guide->scrollbar.dragging,false);
    }
    return done || (pointer && (ui_pressed(KEY_ENTER) || ui_pressed(KEY_KP_ENTER) || ui_pressed(KEY_SPACE)));
}
void ui_guide_free(UIGuide *guide) {
    for (int i=0;i<5;i++) { text_lines_free(&guide->names[i]); text_lines_free(&guide->descriptions[i]); }
    for (int i=0;i<6;i++) text_lines_free(&guide->labels[i]); text_lines_free(&guide->intro); text_lines_free(&guide->note); memset(guide,0,sizeof(*guide));
}
