#include "ui.h"
#include "../platform/platform.h"
#include "../transforms/number.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
const Color UI_BG={244,246,248,255}, UI_SURFACE={255,255,255,255};
const Color UI_TEXT={32,39,46,255}, UI_MUTED={99,113,125,255};
const Color UI_BORDER={202,210,218,255}, UI_ACCENT={40,95,134,255}, UI_TINT={228,238,246,255};
const Color UI_FOCUS={121,82,179,255};
static float density(UIContext *u) { return fmaxf(1,fmaxf(u->render_scale.x,u->render_scale.y)); }
float ui_canvas_font_scale(float zoom) {
    if (!isfinite(zoom) || zoom<=1) return 1;
    return zoom<=1.25f ? 1.25f : zoom<=1.5f ? 1.5f : zoom<=2 ? 2 : 2.5f;
}
void ui_set_canvas_zoom(UIContext *u,float zoom) {
    float next=ui_canvas_font_scale(zoom);
    if (u->canvas_font_scale!=next) { u->canvas_font_scale=next; u->reload_after=GetTime()+.12; }
}
static float advance(Font f,int i) { return f.glyphs[i].advanceX ? (float)f.glyphs[i].advanceX : f.recs[i].width; }
static int source_index(int face) { return face<2 ? 0 : face-1; }
static float source_height(UIContext *u,int source) { return u->font_sources[source].metrics.valid ? u->font_sources[source].metrics.height_per_em : 1; }
static bool load_source(UIContext *u,int index,const char *path) {
    UIFontSource *source=&u->font_sources[index]; if (source->data) return true;
    if (!path) return false;
    source->data=LoadFileData(path,&source->size);
    if (!source->data || !ui_font_metrics_init(&source->metrics,source->data,(size_t)source->size)) { if (source->data) UnloadFileData(source->data); memset(source,0,sizeof(*source)); return false; }
    return true;
}
static Font source_font(UIContext *u,int index,int size,int *codes,int count) {
    UIFontSource *source=&u->font_sources[index];
    return LoadFontFromMemory(".ttf",source->data,source->size,size,codes,count);
}
static void release_font(Font font) {
    if (font.glyphs && font.glyphs!=GetFontDefault().glyphs) UnloadFont(font);
}
size_t ui_font_codepoints(int codes[1024]) {
    size_t n=0;
    for (int c=32;c<384;c++) codes[n++]=c;
    for (int c=0x2000;c<=0x206f;c++) codes[n++]=c;
    for (int c=0x2100;c<=0x22ff;c++) codes[n++]=c;
    codes[n++]=0x20ac; codes[n++]=0x20ba; codes[n++]=0x20b9; /* €, ₺, ₹ */
    return n;
}
static bool load_fonts(UIContext *u) {
    const char *paths[]={u->font_path,u->medium_path,u->emphasis_path,u->italic_path,u->bold_italic_path,u->mono_path};
    for (int i=0;i<6;i++) if ((i<5 || paths[i]) && !load_source(u,i,paths[i])) return false;
    int codes[1024],n=(int)ui_font_codepoints(codes);
    float canvas=fmaxf(1,u->canvas_font_scale);
    int body_size=(int)ceilf(16*u->scale*density(u)*canvas*source_height(u,0)), small_size=(int)ceilf(11*u->scale*density(u)*canvas*source_height(u,0));
    Font body=source_font(u,0,body_size,codes,n),small=source_font(u,0,small_size,codes,n);
    Font medium=source_font(u,1,body_size,codes,n),emphasis=source_font(u,2,body_size,codes,n);
    Font italic=source_font(u,3,body_size,codes,n),bold_italic=source_font(u,4,body_size,codes,n);
    unsigned fallback=GetFontDefault().texture.id;
    if (!IsFontValid(body) || !IsFontValid(small) || !IsFontValid(medium) || !IsFontValid(emphasis) || !IsFontValid(italic) || !IsFontValid(bold_italic) || body.texture.id==fallback || small.texture.id==fallback || medium.texture.id==fallback || emphasis.texture.id==fallback || italic.texture.id==fallback || bold_italic.texture.id==fallback) {
        release_font(body); release_font(small); release_font(medium); release_font(emphasis); release_font(italic); release_font(bold_italic);
        return false;
    }
    int mono_size=(int)ceilf(10*u->scale*density(u)*canvas*source_height(u,5)); Font mono=GetFontDefault(); bool owned=false;
    if (u->mono_path) {
        int marks[]={'N','U','L','"',' ',0x2318,0x21e7}; Font loaded=source_font(u,5,mono_size,marks,7);
        if (IsFontValid(loaded) && loaded.texture.id!=fallback) { mono=loaded; owned=true; SetTextureFilter(mono.texture,TEXTURE_FILTER_BILINEAR); }
        else { release_font(loaded); release_font(body); release_font(small); release_font(medium); release_font(emphasis); release_font(italic); release_font(bold_italic); return false; }
    }
    /* Publish the entire family together. A failed DPI reload preserves every
       previous atlas, including the font that distinguishes typed NULL marks. */
    if (u->fonts_loaded) { release_font(u->body); release_font(u->small); release_font(u->medium); release_font(u->emphasis); release_font(u->italic); release_font(u->bold_italic); }
    u->body=body; u->small=small; u->medium=medium; u->emphasis=emphasis; u->italic=italic; u->bold_italic=bold_italic; u->raster_size=body_size; u->small_raster=small_size; u->fonts_loaded=true;
    if (u->mono_owned) release_font(u->mono);
    u->mono=mono; u->mono_owned=owned; u->mono_raster=mono_size;
    u->mono_em_height=source_height(u,5); u->command_glyph=owned ? GetGlyphIndex(mono,0x2318) : -1; u->command_advance=ui_font_advance_em(&u->font_sources[5].metrics,0x2318);
    u->shift_glyph=owned ? GetGlyphIndex(mono,0x21e7) : -1; u->shift_advance=ui_font_advance_em(&u->font_sources[5].metrics,0x21e7);
    const char *marks="NUL\" "; for (int i=0;i<5;i++) u->marker_glyphs[i]=GetGlyphIndex(u->mono,marks[i]);
    for (int f=0;f<6;f++) {
        Font font=f==0 ? body : f==1 ? small : f==2 ? medium : f==3 ? emphasis : f==4 ? italic : bold_italic; u->fallback[f]=GetGlyphIndex(font,'?');
        UIFontMetrics *metrics=&u->font_sources[source_index(f)].metrics; u->em_height[f]=metrics->height_per_em;
        for (int c=0;c<0x2300;c++) u->glyph_index[f][c]=u->fallback[f];
        for (int i=0;i<font.glyphCount;i++) {
            int cp=font.glyphs[i].value; if (cp>=0 && cp<0x2300) u->glyph_index[f][cp]=i;
            if (i<1024) u->em_advance[f][i]=ui_font_glyph(metrics,(unsigned)cp) ? ui_font_advance_em(metrics,(unsigned)cp) : advance(font,i)*metrics->height_per_em/font.baseSize;
        }
        SetTextureFilter(font.texture,TEXTURE_FILTER_BILINEAR);
    }
    u->advances_ready=true; u->digit_width=0;
    for (int c='0';c<='9';c++) u->digit_width=fmaxf(u->digit_width,u->em_advance[1][u->glyph_index[1][c]]*11);
    return true;
}
static void update_density(UIContext *u) {
    u->dpi=GetWindowScaleDPI();
    u->render_scale=(Vector2){(float)GetRenderWidth()/fmaxf(1,(float)GetScreenWidth()),(float)GetRenderHeight()/fmaxf(1,(float)GetScreenHeight())};
}
bool ui_init(UIContext *u,const char *font,const char *medium,const char *emphasis,const char *mono) {
    memset(u,0,sizeof(*u)); u->mono=GetFontDefault();
    const char *marks="NUL\" "; for (int i=0;i<5;i++) u->marker_glyphs[i]=GetGlyphIndex(u->mono,marks[i]);
    u->scale=u->requested_scale=u->canvas_font_scale=1; update_density(u); u->font_path=dx_strdup(font); u->medium_path=dx_strdup(medium); u->emphasis_path=dx_strdup(emphasis); u->mono_path=mono ? dx_strdup(mono) : NULL;
    const char *name=dx_basename(font); size_t directory=(size_t)(name-font);
    u->italic_path=dx_printf("%.*sIBMPlexSans-Italic.otf",(int)directory,font); u->bold_italic_path=dx_printf("%.*sIBMPlexSans-SemiBoldItalic.otf",(int)directory,font);
    char path[4096]; const char *roots[]={"assets/ui/carbon-atlas.png","../Resources/assets/ui/carbon-atlas.png","../assets/ui/carbon-atlas.png"};
    for (size_t i=0;i<sizeof(roots)/sizeof(*roots);i++) {
        int bytes=snprintf(path,sizeof(path),"%s%s",GetApplicationDirectory(),roots[i]);
        if (bytes>0 && (size_t)bytes<sizeof(path) && FileExists(path)) { u->icons=LoadTexture(path); break; }
    }
    if (u->icons.id) SetTextureFilter(u->icons,TEXTURE_FILTER_BILINEAR);
    return u->icons.id && load_fonts(u);
}
void ui_set_scale(UIContext *u,float scale) { u->requested_scale=fminf(1.5f,fmaxf(.8f,scale)); }
static bool queued_presses[512];
static unsigned char key_modifiers[512];
static DxKeyModifiers navigation_keys[128];
static unsigned navigation_head,navigation_count;
static bool native_navigation;
static bool keyboard_focus_visible;
static bool navigation_key(int key) {
    return key==KEY_UP || key==KEY_DOWN || key==KEY_LEFT || key==KEY_RIGHT || key==KEY_HOME || key==KEY_END ||
        key==KEY_PAGE_UP || key==KEY_PAGE_DOWN || key==KEY_TAB || key==KEY_ENTER || key==KEY_KP_ENTER;
}
static bool focus_key(int key) {
    return navigation_key(key) || key==KEY_SPACE || key==KEY_ESCAPE || key==KEY_KB_MENU ||
        key==KEY_LEFT_ALT || key==KEY_RIGHT_ALT || (key>=KEY_F1 && key<=KEY_F12);
}
bool ui_focus_visible(void) { return keyboard_focus_visible; }
#ifdef DUXCEL_DEV_REVIEW
void ui_review_restore_focus(bool keyboard) { keyboard_focus_visible=keyboard; }

#endif
static void queue_navigation(int key,unsigned modifiers) {
    if (navigation_count<128) { navigation_keys[(navigation_head+navigation_count)%128]=(DxKeyModifiers){key,modifiers&15}; navigation_count++; }
}
bool ui_navigation_captured(void) { return native_navigation || navigation_count!=0; }
bool ui_take_navigation_key(DxKeyModifiers *event) {
#ifdef DUXCEL_DEV_REVIEW
    if (dx_review_input_blocked()) return false;
#endif
    if (!event || !navigation_count) return false;
    *event=navigation_keys[navigation_head]; navigation_head=(navigation_head+1)%128; navigation_count--; return true;
}
static DxPointerGesture pointer_gestures[16];
static unsigned pointer_count,pointer_head;
static uint64_t input_frame;
static Vector2 pointer_offset;
#ifdef DUXCEL_DEV_REVIEW
void ui_review_discard_input(void) {
    pointer_count=pointer_head=navigation_count=navigation_head=0;
    memset(queued_presses,0,sizeof(queued_presses));
    while (GetKeyPressed()) {} while (GetCharPressed()) {}
}
#endif
void ui_pointer_offset(Vector2 offset) { pointer_offset=offset; }
Vector2 ui_mouse_position(void) { Vector2 p=GetMousePosition(); return (Vector2){p.x-pointer_offset.x,p.y-pointer_offset.y}; }
void ui_note_pointer_gesture(DxPointerGesture gesture) { keyboard_focus_visible=false; if (pointer_count<16) { pointer_gestures[(pointer_head+pointer_count)%16]=gesture; pointer_count++; } }
bool ui_has_pointer_gesture(void) {
#ifdef DUXCEL_DEV_REVIEW
    if (dx_review_input_blocked()) return false;
#endif
    return pointer_count!=0;
}
bool ui_take_pointer_gesture(DxPointerGesture *gesture) {
#ifdef DUXCEL_DEV_REVIEW
    if (dx_review_input_blocked()) return false;
#endif
    if (!pointer_count) return false;
    *gesture=pointer_gestures[pointer_head]; pointer_head=(pointer_head+1)%16; pointer_count--; return true;
}
static bool take_gesture_from(Rectangle area,DxPointerGesture *gesture,bool left_only) {
#ifdef DUXCEL_DEV_REVIEW
    if (dx_review_input_blocked()) return false;
#endif
    if (!pointer_count) return false;
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)||IsMouseButtonReleased(MOUSE_BUTTON_LEFT)||IsMouseButtonDown(MOUSE_BUTTON_LEFT)||IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)||IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)||IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) return false;
    for (unsigned i=0;i<pointer_count;i++) {
        DxPointerGesture *candidate=&pointer_gestures[(pointer_head+i)%16];
        if (left_only && candidate->right) continue;
        if (!CheckCollisionPointRec((Vector2){candidate->x0-pointer_offset.x,candidate->y0-pointer_offset.y},area)) continue;
        *gesture=*candidate;
        gesture->x0-=pointer_offset.x; gesture->x1-=pointer_offset.x; gesture->y0-=pointer_offset.y; gesture->y1-=pointer_offset.y;
        for (unsigned j=i;j+1<pointer_count;j++) pointer_gestures[(pointer_head+j)%16]=pointer_gestures[(pointer_head+j+1)%16];
        pointer_count--; return true;
    }
    return false;
}
bool ui_take_gesture_from(Rectangle area,DxPointerGesture *gesture) { return take_gesture_from(area,gesture,false); }
bool ui_gesture_starts_in(Rectangle area) {
#ifdef DUXCEL_DEV_REVIEW
    if (dx_review_input_blocked()) return false;
#endif
    if (area.width<=0 || area.height<=0) return false;
    for (unsigned i=0;i<pointer_count;i++) { const DxPointerGesture *gesture=&pointer_gestures[(pointer_head+i)%16]; if (CheckCollisionPointRec((Vector2){gesture->x0-pointer_offset.x,gesture->y0-pointer_offset.y},area)) return true; }
    return false;
}
bool ui_take_left_gesture_from(Rectangle area,DxPointerGesture *gesture) { return take_gesture_from(area,gesture,true); }
bool ui_take_click_from(Rectangle area) {
    if (area.width<=0 || area.height<=0) return false;
    DxPointerGesture gesture;
    return ui_take_left_gesture_from(area,&gesture) && CheckCollisionPointRec((Vector2){gesture.x1,gesture.y1},area);
}
void ui_update(UIContext *u) {
    u->pointer_blocked=false; pointer_offset=(Vector2){0};
    input_frame++;
    pointer_count=pointer_head=0;
    navigation_count=navigation_head=0;
    memset(queued_presses,0,sizeof(queued_presses));
    memset(key_modifiers,0,sizeof(key_modifiers));
    int key; while ((key=GetKeyPressed())!=0) if (key>0 && key<512) {
        if (focus_key(key)) keyboard_focus_visible=true;
        queued_presses[key]=true;
        if (!native_navigation && navigation_key(key)) {
            unsigned modifiers=(ui_key_shift(key) ? DX_MOD_SHIFT : 0) | (ui_key_control(key) ? DX_MOD_CONTROL : 0) | (ui_key_alt(key) ? DX_MOD_ALT : 0);
#ifdef __APPLE__
            if (ui_key_shortcut(key)) modifiers|=DX_MOD_SUPER;
#endif
            queue_navigation(key,modifiers);
        }
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)||IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)||IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE)) keyboard_focus_visible=false;
    u->cursor=MOUSE_CURSOR_DEFAULT;
    u->scale=u->requested_scale;
    update_density(u);
    int body=(int)ceilf(16*u->scale*density(u)*fmaxf(1,u->canvas_font_scale)*source_height(u,0)), small=(int)ceilf(11*u->scale*density(u)*fmaxf(1,u->canvas_font_scale)*source_height(u,0)),mono=(int)ceilf(10*u->scale*density(u)*fmaxf(1,u->canvas_font_scale)*source_height(u,5));
    if ((body!=u->raster_size || small!=u->small_raster || mono!=u->mono_raster) && GetTime()>=u->reload_after) {
        if (!load_fonts(u)) { u->reload_after=GetTime()+2; TraceLog(LOG_WARNING,"Keeping previous font atlases after a reload failure"); }
        else u->reload_after=0;
    }
}
void ui_destroy(UIContext *u) {
    navigation_count=navigation_head=0; native_navigation=false;
    ui_paint_destroy(u);
    if (u->fonts_loaded) { release_font(u->body); release_font(u->small); release_font(u->medium); release_font(u->emphasis); release_font(u->italic); release_font(u->bold_italic); }
    if (u->mono_owned) release_font(u->mono);
    if (u->icons.id) UnloadTexture(u->icons);
    for (int i=0;i<6;i++) if (u->font_sources[i].data) UnloadFileData(u->font_sources[i].data);
    free(u->font_path); free(u->medium_path); free(u->emphasis_path); free(u->italic_path); free(u->bold_italic_path); free(u->mono_path); memset(u,0,sizeof(*u));
}
static int glyph(UIContext *u,int f,int cp) { return cp>=0 && cp<0x2300 ? u->glyph_index[f][cp] : u->fallback[f]; }
static int next_codepoint(const char *p,int *bytes) {
    int cp=GetCodepointNext(p,bytes); if (cp=='\n' || cp=='\r' || cp=='\t') cp=' '; return cp;
}
static Font face_font(UIContext *u,int face) { return face==0 ? u->body : face==1 ? u->small : face==2 ? u->medium : face==3 ? u->emphasis : face==4 ? u->italic : u->bold_italic; }
static float face_height(UIContext *u,int face) { return u->em_height[face]>0 ? u->em_height[face] : 1; }
static float text_advance(UIContext *u,int face,int cp,float size) {
    if (cp==0x2318 && u->mono_owned && u->command_advance>0) return u->command_advance*size*u->scale;
    if (cp==0x21e7 && u->mono_owned && u->shift_advance>0) return u->shift_advance*size*u->scale;
    Font font=face_font(u,face); if (!font.baseSize) return 0; int index=glyph(u,face,cp);
    float em=u->advances_ready && index<1024 ? u->em_advance[face][index] : advance(font,index)*face_height(u,face)/font.baseSize;
    return em*size*u->scale;
}
float ui_measure_limit(UIContext *u,const char *text,float size,float limit) {
    int f=size<=11; Font font=f ? u->small : u->body;
    if (!text || !font.baseSize) return 0;
    float width=0;
    while (*text && width<limit) {
        int bytes=0,cp=next_codepoint(text,&bytes); if (bytes<=0) break;
        width+=text_advance(u,f,cp,size); text+=bytes;
    }
    return width;
}
float ui_measure(UIContext *u,const char *text,float size) { return ui_measure_limit(u,text,size,INFINITY); }
size_t ui_fit_metrics(UIContext *u,const char *text,float width,char output[2048]) {
    output[0]=0; if (!text || !isfinite(width) || width<=0) return 0;
    size_t used=0,count=0; float occupied=0;
    while (*text) {
        const char *next=strstr(text," · "); size_t length=next ? (size_t)(next-text) : strlen(text),separator=used ? strlen(" · ") : 0;
        if (length && separator<2048-used && length<2048-used-separator) {
            if (separator) memcpy(output+used," · ",separator);
            memcpy(output+used+separator,text,length); output[used+separator+length]=0;
            float extra=ui_measure_limit(u,output+used+separator,12,width+1)+(count ? 15*u->scale : 0);
            if (occupied+extra<=width+.001f*u->scale) { used+=separator+length; count++; occupied+=extra; }
            else output[used]=0;
        }
        if (!next) break; text=next+strlen(" · ");
    }
    return count;
}
static float measure_weight_limit(UIContext *u,const char *text,float size,UIFontWeight weight,float limit) {
    if (weight==UI_REGULAR) return ui_measure_limit(u,text,size,limit);
    int f=weight==UI_MEDIUM ? 2 : weight==UI_SEMIBOLD ? 3 : weight==UI_ITALIC ? 4 : 5; Font font=f==2 ? u->medium : f==3 ? u->emphasis : f==4 ? u->italic : u->bold_italic; if (!font.baseSize || !text) return 0;
    float width=0;
    while (*text && width<limit) { int bytes=0,cp=next_codepoint(text,&bytes); if (bytes<=0) break; width+=text_advance(u,f,cp,size); text+=bytes; }
    return width;
}
float ui_measure_weight(UIContext *u,const char *text,float size,UIFontWeight weight) { return measure_weight_limit(u,text,size,weight,INFINITY); }
float ui_codepoint_width(UIContext *u,int cp,float size) {
    int f=size<=11; Font font=f ? u->small : u->body;
    return font.baseSize ? text_advance(u,f,cp,size) : 0;
}
static Rectangle intersect(Rectangle a,Rectangle b);
void ui_set_clip(UIContext *u,Rectangle clip) {
    u->clip=u->scissor_depth ? intersect(clip,u->scissor_rects[u->scissor_depth-1]) : clip;
    if (u->paint_depth) u->clip=intersect(u->clip,u->paint_frames[u->paint_depth-1].layout.bounds);
    u->clipping=true;
}
void ui_clear_clip(UIContext *u) {
    u->clipping=u->scissor_depth!=0 || u->paint_depth!=0;
    if (u->scissor_depth) u->clip=u->scissor_rects[u->scissor_depth-1];
    else if (u->paint_depth) u->clip=u->paint_frames[u->paint_depth-1].layout.bounds;
    if (u->scissor_depth && u->paint_depth) u->clip=intersect(u->clip,u->paint_frames[u->paint_depth-1].layout.bounds);
}
static Rectangle intersect(Rectangle a,Rectangle b) {
    float x=fmaxf(a.x,b.x),y=fmaxf(a.y,b.y);
    return (Rectangle){x,y,fmaxf(0,fminf(a.x+a.width,b.x+b.width)-x),fmaxf(0,fminf(a.y+a.height,b.y+b.height)-y)};
}
bool ui_scissor_begin(UIContext *u,Rectangle clip) {
    if (!u || u->scissor_depth>=8 || !isfinite(clip.x)||!isfinite(clip.y)||!isfinite(clip.width)||!isfinite(clip.height)||clip.width<0||clip.height<0) return false;
    unsigned at=u->scissor_depth; u->saved_clips[at]=u->clip; u->saved_clipping[at]=u->clipping;
    if (u->clipping) clip=intersect(clip,u->clip);
    if (at) clip=intersect(clip,u->scissor_rects[at-1]);
    u->scissor_rects[at]=clip; u->scissor_depth++; u->clip=clip; u->clipping=true;
    ui_paint_apply_clip(u);
    return true;
}
void ui_scissor_end(UIContext *u) {
    if (!u || !u->scissor_depth) return;
    unsigned at=--u->scissor_depth; u->clip=u->saved_clips[at]; u->clipping=u->saved_clipping[at];
    ui_paint_apply_clip(u);
}
void ui_border(Rectangle r,Rectangle clip,float width,Color color) {
    Rectangle edges[]={{r.x,r.y,r.width,width},{r.x,r.y+r.height-width,r.width,width},{r.x,r.y,width,r.height},{r.x+r.width-width,r.y,width,r.height}};
    for (size_t i=0;i<4;i++) { Rectangle cut=intersect(edges[i],clip); if (cut.width>0 && cut.height>0) DrawRectangleRec(cut,color); }
}
void ui_focus_border(Rectangle r,Rectangle clip,float width) { if (keyboard_focus_visible) ui_border(r,clip,width,UI_FOCUS); }
void ui_dashed_border(Rectangle r,Rectangle clip,float width,float dash,float gap,Color color) {
    if (!isfinite(r.x)||!isfinite(r.y)||!isfinite(r.width)||!isfinite(r.height)||r.width<0||r.height<0||!isfinite(width)||width<=0||!isfinite(dash)||dash<=0||!isfinite(gap)||gap<0) return;
    float step=dash+gap; if (!isfinite(step)) return;
    Rectangle sides[]={{r.x,r.y,r.width,width},{r.x,r.y+r.height-width,r.width,width},{r.x,r.y,width,r.height},{r.x+r.width-width,r.y,width,r.height}};
    for (int side=0;side<4;side++) {
        float length=side<2 ? r.width : r.height;
        for (float at=0;at<length && at/step<8192;at+=step) {
            Rectangle piece=sides[side];
            if (side<2) { piece.x+=at; piece.width=fminf(dash,length-at); }
            else { piece.y+=at; piece.height=fminf(dash,length-at); }
            Rectangle cut=intersect(piece,clip); if (cut.width>0 && cut.height>0) DrawRectangleRec(cut,color);
        }
    }
}
static void draw_glyph(Font f,int index,float x,float y,float factor,Rectangle clip,Color color) {
    GlyphInfo info=f.glyphs[index]; Rectangle src=f.recs[index];
    src.x-=f.glyphPadding; src.y-=f.glyphPadding; src.width+=2*f.glyphPadding; src.height+=2*f.glyphPadding;
    Rectangle dest={x+(info.offsetX-f.glyphPadding)*factor,y+(info.offsetY-f.glyphPadding)*factor,src.width*factor,src.height*factor};
    Rectangle cut=intersect(dest,clip);
    if (cut.width<=0 || cut.height<=0) return;
    src.x+=(cut.x-dest.x)/factor; src.y+=(cut.y-dest.y)/factor;
    src.width=cut.width/factor; src.height=cut.height/factor;
    DrawTexturePro(f.texture,src,cut,(Vector2){0,0},0,color);
}
static void draw_codepoint(UIContext *u,int face,int cp,float x,float y,float size,Rectangle clip,Color color) {
    Font font=face_font(u,face); float height=face_height(u,face);
    if (((cp==0x2318 && u->command_advance>0) || (cp==0x21e7 && u->shift_advance>0)) && u->mono_owned) {
        const UIFontMetrics *main=&u->font_sources[source_index(face)].metrics,*mono=&u->font_sources[5].metrics;
        if (main->valid && mono->valid) y+=(main->ascent/(float)main->units-mono->ascent/(float)mono->units)*size*u->scale;
        draw_glyph(u->mono,cp==0x2318 ? u->command_glyph : u->shift_glyph,x,y,size*u->scale*u->mono_em_height/u->mono.baseSize,clip,color);
    } else if (cp!=' ') draw_glyph(font,glyph(u,face,cp),x,y,size*u->scale*height/font.baseSize,clip,color);
}
void ui_text(UIContext *u,const char *text,float x,float y,float size,Color color) {
    int f=size<=11; Font font=face_font(u,f); if (!font.baseSize || !text) return;
    float dx=fmaxf(1,u->render_scale.x),dy=fmaxf(1,u->render_scale.y);
    x=roundf(x*dx)/dx; y=roundf(y*dy)/dy;
    Rectangle clip=u->clipping ? u->clip : (Rectangle){-100000,-100000,200000,200000};
    while (*text) {
        int bytes=0,cp=next_codepoint(text,&bytes); if (bytes<=0) break;
        draw_codepoint(u,f,cp,x,y,size,clip,color); x+=text_advance(u,f,cp,size); text+=bytes;
    }
}
void ui_text_clip(UIContext *u,const char *text,Rectangle r,float size,Color color,bool right) {
    ui_text_clip_weight(u,text,r,size,color,right,UI_REGULAR);
}
static void text_clip_weight(UIContext *u,const char *text,Rectangle r,float size,Color color,bool right,UIFontWeight weight,float padding) {
    if (!text || r.width<=2*padding*u->scale || r.height<=0) return;
    int f=weight==UI_MEDIUM ? 2 : weight==UI_SEMIBOLD ? 3 : weight==UI_ITALIC ? 4 : weight==UI_SEMIBOLD_ITALIC ? 5 : size<=11;
    Font font=f==0 ? u->body : f==1 ? u->small : f==2 ? u->medium : f==3 ? u->emphasis : f==4 ? u->italic : u->bold_italic; if (!font.baseSize) return;
    /* Fractional em advances can differ by an ulp after a parent adds and
       removes padding. That rounding must not replace a fitting word with … . */
    float available=r.width-2*padding*u->scale+.001f*u->scale, width=0;
    /* No allocations, duplicate UTF-8 walks, linear glyph searches, or GL scissor flushes. */
    int indices[1024]; size_t count=0; const char *p=text;
    while (*p && count<1023) {
        int bytes=0,cp=next_codepoint(p,&bytes); if (bytes<=0) break;
        float w=text_advance(u,f,cp,size);
        if (width+w>available) break;
        indices[count++]=cp; width+=w; p+=bytes;
    }
    if (*p) {
        int ellipsis=0x2026; float w=text_advance(u,f,ellipsis,size);
        while (count && width+w>available) width-=text_advance(u,f,indices[--count],size);
        if (width+w<=available) { indices[count++]=ellipsis; width+=w; }
    }
    float x=r.x+(padding>0 ? padding-1 : 0)*u->scale;
    if (right) x=fmaxf(x,r.x+r.width-width-(padding>0 ? padding+1 : 0)*u->scale);
    float dx=fmaxf(1,u->render_scale.x),dy=fmaxf(1,u->render_scale.y);
    x=roundf(x*dx)/dx; float y=roundf((r.y+(r.height-size*u->scale*face_height(u,f))/2)*dy)/dy;
    Rectangle clip=u->clipping ? intersect(r,u->clip) : r;
    for (size_t i=0;i<count;i++) { draw_codepoint(u,f,indices[i],x,y,size,clip,color); x+=text_advance(u,f,indices[i],size); }
}
void ui_text_clip_weight(UIContext *u,const char *text,Rectangle r,float size,Color color,bool right,UIFontWeight weight) {
    text_clip_weight(u,text,r,size,color,right,weight,8);
}
void ui_label(UIContext *u,const char *text,Rectangle r,float size,Color color,bool right,UIFontWeight weight) {
    text_clip_weight(u,text,r,size,color,right,weight,0);
}
double ui_chart_fraction(double value,double lower,double upper) {
    if (!isfinite(value) || !isfinite(lower) || !isfinite(upper) || lower>=upper) return .5;
    if (value<=lower) return 0; if (value>=upper) return 1;
    double span=upper-lower;
    if (isfinite(span)) return (value-lower)/span;
    double scale=fmax(fabs(lower),fabs(upper));
    return (value/scale-lower/scale)/(upper/scale-lower/scale);
}
bool ui_format_float(double number,unsigned digits,char output[64]) {
    if (!isfinite(number) || digits<1 || digits>15) return false;
    int length=snprintf(output,64,"%.*g",(int)digits,number); return length>0 && length<64;
}
void ui_float_value(UIContext *u,const char *raw,bool is_null,double number,Rectangle r,float size,Color color,bool right) {
    char text[64]; float available=r.width-16*u->scale;
    int digits=(int)floorf(available/fmaxf(1,u->digit_width*(size/11)*u->scale))-2;
    if (digits<3) digits=3; if (digits>15) digits=15;
    if (!is_null && raw && *raw && ui_format_float(number,(unsigned)digits,text)) {
        while (digits>1 && ui_measure_limit(u,text,size,available+1)>available) ui_format_float(number,(unsigned)--digits,text);
        ui_value(u,text,false,r,size,color,right);
    } else ui_value(u,raw,is_null,r,size,color,right);
}
float ui_value_width(UIContext *u,const char *text,bool is_null,float size,float limit) {
    if (is_null || (text && !*text)) return ((is_null ? 4 : 2)*6+6)*fminf(1,size/11)*u->scale;
    return ui_measure_limit(u,text,size,limit);
}
void ui_value(UIContext *u,const char *text,bool is_null,Rectangle r,float size,Color color,bool right) {
    if (!is_null && (!text || *text)) { ui_text_clip(u,text,r,size,color,right); return; }
    if (r.width<16*u->scale || r.height<=0) return;
    float k=fminf(1,size/11)*u->scale,w=ui_value_width(u,text,is_null,size,0),h=fminf(14*k,r.height-2*k);
    float x=right ? fmaxf(r.x+7*u->scale,r.x+r.width-w-9*u->scale) : r.x+7*u->scale;
    float y=r.y+(r.height-h)/2;
    float dx=fmaxf(1,u->render_scale.x),dy=fmaxf(1,u->render_scale.y);
    x=roundf(x*dx)/dx; y=roundf(y*dy)/dy;
    Rectangle clip=u->clipping ? intersect(r,u->clip) : r;
    /* Fixed glyph advances and smaller type distinguish missing data from literal
       proportional text. No button-like box or per-cell allocation/texture break. */
    if (!u->mono.baseSize) return;
    int null_indices[]={0,1,2,2},empty_indices[]={3,3};
    int count=is_null ? 4 : 2; float factor=10*k*(u->mono_em_height>0 ? u->mono_em_height : 1)/u->mono.baseSize;
    for (int i=0;i<count;i++) draw_glyph(u->mono,u->marker_glyphs[is_null ? null_indices[i] : empty_indices[i]],x+(3+i*6)*k,y+(h-10*k*(u->mono_em_height>0 ? u->mono_em_height : 1))/2,factor,clip,(Color){91,103,122,255});
}
void ui_formatted_value_weight(UIContext *u,const char *raw,bool is_null,bool numeric,bool floating,double number,
    Rectangle r,float default_size,Color color,CellFormat format,UIFontWeight base_weight) {
    float size=format.specified&FORMAT_SIZE ? format.size : default_size;
    bool right=format.align==FORMAT_RIGHT || (format.align==FORMAT_AUTO && numeric);
    char display[512]; const char *text=raw;
    if (!is_null && numeric && raw && *raw && (format.number!=FORMAT_GENERAL || (format.specified&FORMAT_DECIMALS)) && number_display(raw,format.decimals,format.number!=FORMAT_NUMBER,display)) text=display;
    else if (!is_null && numeric && floating && raw && *raw) {
        float available=r.width-16*u->scale; int digits=(int)floorf(available/fmaxf(1,u->digit_width*(size/11)*u->scale))-2;
        if (digits<3) digits=3; if (digits>15) digits=15;
        if (ui_format_float(number,(unsigned)digits,display)) { while (digits>1 && ui_measure_limit(u,display,size,available+1)>available) ui_format_float(number,(unsigned)--digits,display); text=display; }
    }
    static const Color colors[]={{32,39,46,255},{40,95,134,255},{57,124,117,255},{164,51,55,255}};
    if (format.specified&FORMAT_COLOR) color=colors[format.color];
    bool marker=is_null || (text && !*text);
    UIFontWeight weight=format.flags&FORMAT_ITALIC ? format.flags&FORMAT_BOLD ? UI_SEMIBOLD_ITALIC : UI_ITALIC : format.flags&FORMAT_BOLD ? UI_SEMIBOLD : base_weight;
    float available=fmaxf(0,r.width-16*u->scale),width=0;
    if (format.align==FORMAT_CENTER || (format.flags&FORMAT_UNDERLINE)) width=marker ? ui_value_width(u,text,is_null,size,r.width) : measure_weight_limit(u,text,size,weight,available+1);
    if (format.align==FORMAT_CENTER && width<available) { r.x+=(available-width)*.5f; r.width=width+16*u->scale; }
    if (marker) { ui_value(u,text,is_null,r,size,color,right); return; }
    ui_text_clip_weight(u,text,r,size,color,right,weight);
    if ((format.flags&FORMAT_UNDERLINE) && text && *text) {
        float shown=fminf(width,available),x=right ? r.x+r.width-shown-9*u->scale : r.x+7*u->scale;
        Rectangle underline={x,r.y+(r.height+size*u->scale)*.5f-1*u->scale,shown,fmaxf(1,u->scale)};
        Rectangle visible=u->clipping ? intersect(underline,u->clip) : underline; if (visible.width>0 && visible.height>0) DrawRectangleRec(visible,color);
    }
}
void ui_formatted_value(UIContext *u,const char *raw,bool is_null,bool numeric,bool floating,double number,
    Rectangle r,float default_size,Color color,CellFormat format) {
    ui_formatted_value_weight(u,raw,is_null,numeric,floating,number,r,default_size,color,format,UI_REGULAR);
}
bool ui_control_input(UIContext *u,Rectangle r,bool enabled,bool *hover) {
#ifdef DUXCEL_DEV_REVIEW
    if (dx_review_input_blocked()) { *hover=false; return false; }
#endif
    enabled=enabled && !u->pointer_blocked;
    *hover=enabled && CheckCollisionPointRec(ui_mouse_position(),r) && (!u->clipping || CheckCollisionPointRec(ui_mouse_position(),u->clip));
    bool completed=enabled && ui_take_click_from(u->clipping ? intersect(r,u->clip) : r);
    if (*hover) u->cursor=MOUSE_CURSOR_POINTING_HAND;
    return completed || (*hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT));
}
static Color feedback_mix(Color a,Color b,float progress) {
    float alpha=a.a*(1-progress)+b.a*progress; Color out={0}; out.a=(unsigned char)lroundf(alpha);
    if (alpha>0) {
        out.r=(unsigned char)lroundf((a.r*a.a*(1-progress)+b.r*b.a*progress)/alpha);
        out.g=(unsigned char)lroundf((a.g*a.a*(1-progress)+b.g*b.a*progress)/alpha);
        out.b=(unsigned char)lroundf((a.b*a.a*(1-progress)+b.b*b.a*progress)/alpha);
    }
    return out;
}
void ui_feedback_colors(UIContext *u,Rectangle r,unsigned kind,const Color base[3],Color colors[3]) {
    if (!u || !base || !colors || !isfinite(u->scale) || u->scale<=0 || !isfinite(u->paint_time) || !isfinite(r.x) || !isfinite(r.y) || !isfinite(r.width) || !isfinite(r.height) || fabsf(r.x)>65536 || fabsf(r.y)>65536 || fabsf(r.width)>65536 || fabsf(r.height)>65536) return;
    uint64_t hash=1469598103934665603ULL;
    const float dimensions[]={r.x,r.y,r.width,r.height};
    for (int i=0;i<4;i++) { hash^=(uint32_t)(int32_t)lroundf(dimensions[i]/u->scale*8); hash*=1099511628211ULL; }
    hash^=kind; if (!hash) hash=1;
    UIControlFeedback *entry=NULL,*oldest=NULL;
    for (unsigned probe=0;probe<8;probe++) {
        UIControlFeedback *candidate=&u->feedback[(hash+probe)&127];
        if (candidate->key==hash) { entry=candidate; break; }
        if (!oldest || !candidate->key || candidate->seen<oldest->seen) oldest=candidate;
    }
    if (!entry) entry=oldest;
    if (entry->key!=hash || u->paint_time-entry->seen>.25 || u->paint_time<entry->seen) {
        *entry=(UIControlFeedback){.key=hash}; entry->motion.standard_ease=true;
        memcpy(entry->current,base,sizeof(entry->current)); memcpy(entry->from,base,sizeof(entry->from)); memcpy(entry->target,base,sizeof(entry->target));
        ui_motion_update_at(&entry->motion,1,u->paint_time,.09f,true);
    }
    entry->seen=u->paint_time;
    float progress=ui_motion_update_at(&entry->motion,1,u->paint_time,.09f,u->reduced_motion);
    for (int i=0;i<3;i++) entry->current[i]=feedback_mix(entry->from[i],entry->target[i],progress);
    if (memcmp(entry->target,colors,sizeof(entry->target))) {
        memcpy(entry->from,entry->current,sizeof(entry->from)); memcpy(entry->target,colors,sizeof(entry->target));
        ui_motion_update_at(&entry->motion,0,u->paint_time,.09f,true);
        progress=ui_motion_update_at(&entry->motion,1,u->paint_time,.09f,u->reduced_motion);
        for (int i=0;i<3;i++) entry->current[i]=feedback_mix(entry->from[i],entry->target[i],progress);
    }
    memcpy(colors,entry->current,sizeof(entry->current));
}
bool ui_list_row(UIContext *u,Rectangle r,const char *label,bool selected,bool enabled) {
    bool hover=false,clicked=ui_control_input(u,r,enabled,&hover);
    Rectangle visible=u->clipping ? intersect(r,u->clip) : r;
    Color ink=!enabled ? UI_MUTED : selected ? UI_ACCENT : UI_TEXT;
    Color base[]={selected ? UI_TINT : (Color){237,241,247,0},selected ? (Color){177,194,216,255} : (Color){202,210,218,0},ink};
    Color colors[]={selected ? UI_TINT : hover ? (Color){237,241,247,255} : base[0],selected ? base[1] : hover ? UI_BORDER : base[1],ink};
    ui_feedback_colors(u,r,3,base,colors);
    if (colors[0].a) DrawRectangleRec(visible,colors[0]);
    if (colors[1].a) ui_border(r,visible,u->scale,colors[1]);
    if (selected) DrawRectangleRec(intersect((Rectangle){r.x,r.y,2*u->scale,r.height},visible),UI_ACCENT);
    ui_text_clip_weight(u,label,r,12,colors[2],false,selected ? UI_MEDIUM : UI_REGULAR);
    return clicked;
}

bool ui_button(UIContext *u,Rectangle r,const char *label,bool active,bool enabled) {
    if (label && !strcmp(label,"×")) return ui_icon_button(u,r,CARBON_CLOSE,active,enabled);
    if (label && !strcmp(label,"+")) return ui_icon_button(u,r,CARBON_ADD,active,enabled);
    if (label && !strcmp(label,"↑")) return ui_icon_button(u,r,CARBON_ARROW_UP,active,enabled);
    if (label && !strcmp(label,"↓")) return ui_icon_button(u,r,CARBON_ARROW_DOWN,active,enabled);
    if (label && !strcmp(label,"›")) return ui_icon_button(u,r,CARBON_CHEVRON_RIGHT,active,enabled);
    bool hover=false,completed=ui_control_input(u,r,enabled,&hover);
    Color fill=active ? UI_TINT : hover ? (Color){237,241,247,255} : WHITE;
    if (completed || (hover && IsMouseButtonDown(MOUSE_BUTTON_LEFT))) fill=(Color){218,226,238,255};
    Color border=active ? UI_ACCENT : enabled ? (Color){177,187,201,255} : UI_BORDER;
    Color ink=!enabled ? (Color){153,160,172,255} : active ? UI_ACCENT : UI_TEXT;
    Color base[]={active ? UI_TINT : WHITE,border,ink},colors[]={fill,border,ink};
    ui_feedback_colors(u,r,2,base,colors); fill=colors[0]; border=colors[1];
    Rectangle visible=u->clipping ? intersect(r,u->clip) : r;
    if (visible.width>0 && visible.height>0) {
        if (!u->clipping || (r.x>=u->clip.x && r.y>=u->clip.y && r.x+r.width<=u->clip.x+u->clip.width && r.y+r.height<=u->clip.y+u->clip.height)) {
            DrawRectangleRounded(r,.15f,4,fill); DrawRectangleRoundedLinesEx(r,.15f,4,u->scale,border);
        } else { DrawRectangleRec(visible,fill); ui_border(r,u->clip,u->scale,border); }
    }
    Color color=colors[2];
    ui_text_clip_weight(u,label,r,12,color,false,active ? UI_MEDIUM : UI_REGULAR);
    return completed;
}
bool ui_tool_button(UIContext *u,Rectangle r,const char *label,bool active,bool enabled) {
    bool hover=false,clicked=ui_control_input(u,r,enabled,&hover);
    Rectangle visible=u->clipping ? intersect(r,u->clip) : r;
    Color ink=enabled ? active ? UI_ACCENT : UI_TEXT : (Color){153,160,172,255};
    Color base[]={active ? (Color){216,229,238,255} : (Color){228,234,240,0},active ? (Color){175,197,214,255} : (Color){197,208,218,0},ink};
    bool pressed=clicked || (hover && IsMouseButtonDown(MOUSE_BUTTON_LEFT));
    Color colors[]={pressed ? (Color){212,228,238,255} : active ? base[0] : hover ? (Color){228,234,240,255} : base[0],active ? base[1] : hover || pressed ? (Color){197,208,218,255} : base[1],ink};
    ui_feedback_colors(u,r,1,base,colors);
    if (colors[0].a) DrawRectangleRec(visible,colors[0]);
    if (colors[1].a) ui_border(r,visible,u->scale,colors[1]);
    ui_text_clip_weight(u,label,r,12,colors[2],false,active ? UI_MEDIUM : UI_REGULAR);
    return clicked;
}

static Color popup_available(Color color,bool enabled) {
    return enabled ? color : (Color){(unsigned char)((color.r*2+255*3)/5),(unsigned char)((color.g*2+255*3)/5),(unsigned char)((color.b*2+255*3)/5),color.a};
}
bool ui_popup_button(UIContext *u,Rectangle r,const char *label,bool primary,bool text_action,bool enabled) {
    bool hover=false,clicked=ui_control_input(u,r,enabled,&hover); float s=u->scale;
    Color ink=popup_available(primary ? WHITE : text_action ? UI_ACCENT : (Color){39,63,81,255},enabled);
    Color base[]={popup_available(primary ? (Color){40,95,134,255} : text_action ? (Color){237,243,247,0} : WHITE,enabled),popup_available(primary ? (Color){40,95,134,255} : (Color){196,211,223,text_action ? 0 : 255},enabled),ink};
    bool pressed=clicked || (hover && IsMouseButtonDown(MOUSE_BUTTON_LEFT));
    Color colors[]={pressed ? (Color){212,228,238,255} : hover ? popup_available(primary ? (Color){33,82,118,255} : (Color){237,243,247,255},enabled) : base[0],base[1],ink};
    ui_feedback_colors(u,r,4,base,colors);
    if (!text_action) {
        DrawRectangleRounded(r,.15f,4,colors[1]);
        DrawRectangleRounded((Rectangle){r.x+s,r.y+s,r.width-2*s,r.height-2*s},.15f,4,colors[0]);
    } else if (colors[0].a) DrawRectangleRounded(r,.15f,4,colors[0]);
    float width=ui_measure(u,label,11); ui_label(u,label,(Rectangle){text_action ? r.x : r.x+(r.width-width)*.5f,r.y,text_action ? r.width : width+1*s,r.height},11,colors[2],false,UI_REGULAR);
    return clicked;
}
bool ui_popup_row_input(UIContext *u,Rectangle r,const char *label,UICarbonIcon icon,bool enabled,bool pointer) {
    bool hover=false,clicked=ui_control_input(u,r,enabled && pointer,&hover); float s=u->scale;
    Color ink=popup_available((Color){39,63,81,255},enabled);
    Color base[]={{237,243,247,0},{0,0,0,0},ink},colors[]={hover || clicked ? (Color){237,243,247,255} : base[0],base[1],ink};
    ui_feedback_colors(u,r,5,base,colors);
    if (colors[0].a) DrawRectangleRounded(r,.12f,4,colors[0]);
    if ((unsigned)icon<CARBON_COUNT) {
        float press=hover && IsMouseButtonDown(MOUSE_BUTTON_LEFT) ? .5f*s : 0;
        ui_carbon(u,icon,(Rectangle){r.x+7*s,r.y+(r.height-16*s)*.5f+press,16*s,16*s},colors[2]);
    }
    ui_label(u,label,(Rectangle){r.x+31*s,r.y,r.width-38*s,r.height},11.5f,colors[2],false,UI_REGULAR); return clicked;
}

bool ui_popup_row(UIContext *u,Rectangle r,const char *label,UICarbonIcon icon,bool enabled) { return ui_popup_row_input(u,r,label,icon,enabled,true); }
void ui_carbon(UIContext *u,UICarbonIcon icon,Rectangle rect,Color color) {
    if (!u->icons.id || (unsigned)icon>=CARBON_COUNT || rect.width<=0 || rect.height<=0) return;
    float dx=fmaxf(1,u->render_scale.x),dy=fmaxf(1,u->render_scale.y);
    rect.x=roundf(rect.x*dx)/dx; rect.y=roundf(rect.y*dy)/dy;
    Rectangle target=u->clipping ? intersect(rect,u->clip) : rect;
    if (target.width<=0 || target.height<=0) return;
    Rectangle source={(icon%8)*64.0f,(icon/8)*64.0f,64,64};
    source.x+=(target.x-rect.x)/rect.width*64; source.y+=(target.y-rect.y)/rect.height*64;
    source.width=target.width/rect.width*64; source.height=target.height/rect.height*64;
    DrawTexturePro(u->icons,source,target,(Vector2){0,0},0,color);
}
bool ui_icon_button(UIContext *u,Rectangle r,UICarbonIcon icon,bool active,bool enabled) {
    bool clicked=ui_tool_button(u,r,"",active,enabled); float size=16*u->scale;
    float press=enabled && !u->pointer_blocked && IsMouseButtonDown(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(ui_mouse_position(),r) ? .5f*u->scale : 0;
    ui_carbon(u,icon,(Rectangle){r.x+(r.width-size)*.5f,r.y+(r.height-size)*.5f+press,size,size},enabled ? active ? UI_ACCENT : UI_MUTED : (Color){153,160,172,255});
    return clicked;
}
bool ui_menu_item(UIContext *u,Rectangle r,const char *label,UICarbonIcon icon,bool checked,bool enabled) {
    bool clicked=ui_tool_button(u,r,"",false,enabled); float s=u->scale;
    Color ink=enabled ? UI_TEXT : (Color){153,160,172,255};
    ui_carbon(u,checked ? CARBON_CHECKMARK : icon,(Rectangle){r.x+6*s,r.y+(r.height-16*s)*.5f,16*s,16*s},ink);
    ui_text_clip(u,label,(Rectangle){r.x+25*s,r.y,r.width-25*s,r.height},12,ink,false);
    return clicked;
}
bool ui_scrollbar_update(UIContext *u,Rectangle track,float content,float viewport,float *offset,bool horizontal,UIScrollbar *state,bool enabled) {
    enabled=enabled && !u->pointer_blocked;
    if (!offset || !state || !isfinite(content) || !isfinite(viewport) || !isfinite(*offset) || content<=viewport || viewport<=0 || track.width<=0 || track.height<=0) { if (state) state->dragging=false; return false; }
    float maximum=content-viewport,length=horizontal ? track.width : track.height;
    float size=fminf(length,fmaxf(26*u->scale,length*viewport/content)),travel=length-size;
    *offset=fmaxf(0,fminf(maximum,*offset));
    Rectangle thumb=track; if (horizontal) { thumb.x+=travel*(*offset)/maximum; thumb.width=size; } else { thumb.y+=travel*(*offset)/maximum; thumb.height=size; }
    Rectangle visible=u->clipping ? intersect(track,u->clip) : track;
    Vector2 mouse=ui_mouse_position(); bool consumed=false; DxPointerGesture gesture;
    if (enabled && ui_take_left_gesture_from(visible,&gesture)) {
        Vector2 start={gesture.x0,gesture.y0};
        float grab=CheckCollisionPointRec(start,thumb) ? (horizontal ? start.x-thumb.x : start.y-thumb.y) : size*.5f;
        if (travel>0) *offset=fmaxf(0,fminf(1,((horizontal ? gesture.x1-track.x : gesture.y1-track.y)-grab)/travel))*maximum;
        consumed=true; state->dragging=false;
    } else if (enabled && CheckCollisionPointRec(mouse,visible) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        state->grab=CheckCollisionPointRec(mouse,thumb) ? (horizontal ? mouse.x-thumb.x : mouse.y-thumb.y) : size*.5f;
        state->dragging=true; consumed=true;
    }
    if (state->dragging && enabled && IsMouseButtonDown(MOUSE_BUTTON_LEFT) && travel>0) { *offset=fmaxf(0,fminf(1,((horizontal ? mouse.x-track.x : mouse.y-track.y)-state->grab)/travel))*maximum; consumed=true; }
    if (!enabled || !IsMouseButtonDown(MOUSE_BUTTON_LEFT)) state->dragging=false;
    return consumed;
}
void ui_scrollbar_paint(UIContext *u,Rectangle track,float content,float viewport,float offset,bool horizontal) {
    if (!isfinite(content)||!isfinite(viewport)||!isfinite(offset)||content<=viewport||viewport<=0||track.width<=0||track.height<=0) return;
    float maximum=content-viewport,length=horizontal ? track.width : track.height;
    float size=fminf(length,fmaxf(26*u->scale,length*viewport/content)),travel=length-size;
    offset=fmaxf(0,fminf(maximum,offset));
    Rectangle thumb=track,visible=u->clipping ? intersect(track,u->clip) : track;
    if (horizontal) { thumb.x+=travel*offset/maximum; thumb.width=size; thumb.y+=3*u->scale; thumb.height=fmaxf(u->scale,track.height-6*u->scale); }
    else { thumb.y+=travel*offset/maximum; thumb.height=size; thumb.x+=3*u->scale; thumb.width=fmaxf(u->scale,track.width-6*u->scale); }
    DrawRectangleRec(visible,UI_BG);
    DrawRectangleRec(u->clipping ? intersect(thumb,u->clip) : thumb,(Color){173,184,199,255});
}
bool ui_scrollbar(UIContext *u,Rectangle track,float content,float viewport,float *offset,bool horizontal,UIScrollbar *state,bool enabled) {
    bool changed=ui_scrollbar_update(u,track,content,viewport,offset,horizontal,state,enabled);
    if (offset && state) ui_scrollbar_paint(u,track,content,viewport,*offset,horizontal);
    return changed;
}

static UICarbonIcon carbon_for(UIIcon icon) {
    static const UICarbonIcon icons[]={CARBON_FOLDER_OPEN,CARBON_SAVE,CARBON_ADD,CARBON_UNDO,CARBON_REDO,CARBON_COPY,CARBON_TABLE,CARBON_SORT_ASCENDING,CARBON_CHART_BAR,CARBON_CLOSE,CARBON_SQL,CARBON_SETTINGS_ADJUST};
    return (unsigned)icon<sizeof(icons)/sizeof(*icons) ? icons[icon] : CARBON_TABLE;
}
bool ui_carbon_command(UIContext *u,Rectangle r,UICarbonIcon icon,const char *label,bool active,bool enabled) {
    bool clicked=ui_tool_button(u,r,"",active,enabled); float s=u->scale;
    bool prominent=r.height>40*s; float size=(prominent ? 20 : 16)*s;
    Rectangle image={r.x+7*s,r.y+(r.height-size)*.5f,size,size};
    Rectangle caption={r.x+31*s,r.y,fmaxf(0,r.width-39*s),r.height};
    if (prominent) { image=(Rectangle){r.x+(r.width-size)*.5f,r.y+5*s,size,size}; caption=(Rectangle){r.x,r.y+r.height-24*s,r.width,23*s}; }
    if (!label) image.x=r.x+(r.width-size)*.5f;
    if (enabled && !u->pointer_blocked && IsMouseButtonDown(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(ui_mouse_position(),r)) image.y+=.5f*s;
    ui_carbon(u,icon,image,enabled ? UI_TEXT : (Color){153,160,172,255});
    if (prominent && label) { float width=ui_measure_weight(u,label,11.5f,active ? UI_MEDIUM : UI_REGULAR); caption.x+=fmaxf(0,(caption.width-width)*.5f); caption.width=fminf(caption.width,width+1); }
    ui_label(u,label,caption,prominent ? 11.5f : 12,enabled ? active ? UI_ACCENT : UI_TEXT : (Color){153,160,172,255},false,active ? UI_MEDIUM : UI_REGULAR);
    return clicked;
}
bool ui_command(UIContext *u,Rectangle r,UIIcon icon,const char *label,bool active,bool enabled) {
    return ui_carbon_command(u,r,carbon_for(icon),label,active,enabled);
}
void ui_icon(UIContext *u,UIIcon icon,Vector2 origin,Color color) {
    ui_carbon(u,carbon_for(icon),(Rectangle){origin.x,origin.y,16*u->scale,16*u->scale},color);
}
bool ui_shortcut(void) {
#ifdef __APPLE__
    return IsKeyDown(KEY_LEFT_SUPER)||IsKeyDown(KEY_RIGHT_SUPER);
#else
    return IsKeyDown(KEY_LEFT_CONTROL)||IsKeyDown(KEY_RIGHT_CONTROL);
#endif
}
bool ui_shift(void) { return IsKeyDown(KEY_LEFT_SHIFT)||IsKeyDown(KEY_RIGHT_SHIFT); }
bool ui_pressed(int key) {
#ifdef DUXCEL_DEV_REVIEW
    if (dx_review_input_blocked()) return false;
#endif
    return key>0 && key<512 && (queued_presses[key] || IsKeyPressed(key)); }
bool ui_key(int key) { return ui_pressed(key)||IsKeyPressedRepeat(key); }
void ui_note_key_modifiers(int key,unsigned modifiers) {
    if (focus_key(key) || (key>0 && key<512 && (modifiers&DX_MOD_ALT))) keyboard_focus_visible=true;
    if (key>0 && key<512) key_modifiers[key]=(unsigned char)(128 | (modifiers&15));
    if (navigation_key(key)) {
        if (!native_navigation) { navigation_count=navigation_head=0; native_navigation=true; }
        queue_navigation(key,modifiers);
    }
}
void ui_note_key_press(int key,unsigned modifiers) {
    if (key>0 && key<512) queued_presses[key]=true; ui_note_key_modifiers(key,modifiers);
}
static unsigned modifiers_for(int key) {
    if (key>0 && key<512 && (key_modifiers[key]&128)) return key_modifiers[key]&15;
    return (ui_shift() ? DX_MOD_SHIFT : 0) | ((IsKeyDown(KEY_LEFT_CONTROL)||IsKeyDown(KEY_RIGHT_CONTROL)) ? DX_MOD_CONTROL : 0) | ((IsKeyDown(KEY_LEFT_ALT)||IsKeyDown(KEY_RIGHT_ALT)) ? DX_MOD_ALT : 0) | ((IsKeyDown(KEY_LEFT_SUPER)||IsKeyDown(KEY_RIGHT_SUPER)) ? DX_MOD_SUPER : 0);
}
bool ui_key_shift(int key) { return (modifiers_for(key)&DX_MOD_SHIFT)!=0; }
bool ui_key_alt(int key) { return (modifiers_for(key)&DX_MOD_ALT)!=0; }
bool ui_key_control(int key) { return (modifiers_for(key)&DX_MOD_CONTROL)!=0; }
bool ui_key_shortcut(int key) {
#ifdef __APPLE__
    return (modifiers_for(key)&DX_MOD_SUPER)!=0;
#else
    return (modifiers_for(key)&DX_MOD_CONTROL)!=0;
#endif
}
int ui_scroll_steps(double *remainder,double delta) {
    if (!isfinite(delta) || !isfinite(*remainder)) { *remainder=0; return 0; }
    double total=*remainder+fmax(-128,fmin(128,delta)),nearest=round(total);
    if (fabs(total-nearest)<.000001) total=nearest;
    int steps=(int)total; *remainder=total-steps; return steps;
}
static size_t input_boundary(const TextInput *in,size_t position) {
    if (position>in->length) position=in->length;
    while (position && position<in->length && ((unsigned char)in->text[position]&0xc0)==0x80) position--;
    return position;
}
static void input_bounds(const TextInput *in,size_t *start,size_t *end) {
    size_t cursor=input_boundary(in,in->cursor),anchor=input_boundary(in,in->anchor);
    *start=in->select_all ? 0 : cursor<anchor ? cursor : anchor;
    *end=in->select_all ? in->length : cursor>anchor ? cursor : anchor;
}
static size_t input_next(const TextInput *in,size_t at) {
    if (at>=in->length) return in->length;
    at++; while (at<in->length && ((unsigned char)in->text[at]&0xc0)==0x80) at++; return at;
}
static bool input_word(const TextInput *in,size_t at) {
    unsigned char c=(unsigned char)in->text[at]; return c>=128 || (c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='_';
}
static size_t input_word_left(const TextInput *in,size_t at) {
    while (at && !input_word(in,dx_utf8_prev(in->text,at))) at=dx_utf8_prev(in->text,at);
    while (at && input_word(in,dx_utf8_prev(in->text,at))) at=dx_utf8_prev(in->text,at);
    return at;
}
static size_t input_word_right(const TextInput *in,size_t at) {
    while (at<in->length && !input_word(in,at)) at=input_next(in,at);
    while (at<in->length && input_word(in,at)) at=input_next(in,at);
    return at;
}
static void input_clear_history(TextInputState *states,size_t *count) {
    for (size_t i=0;i<*count;i++) free(states[i].text);
    memset(states,0,32*sizeof(*states)); *count=0;
}
void input_free(TextInput *in) {
    free(in->text); input_clear_history(in->undo,&in->undo_count); input_clear_history(in->redo,&in->redo_count); memset(in,0,sizeof(*in));
}
static TextInputState input_state(const TextInput *in) {
    return (TextInputState){in->text,in->length,in->capacity,in->cursor,in->anchor,in->view_start,in->select_all};
}
static void input_push(TextInputState *states,size_t *count,TextInputState state) {
    if (state.capacity>1024*1024) { input_clear_history(states,count); free(state.text); return; }
    size_t bytes=state.capacity; for (size_t i=0;i<*count;i++) bytes+=states[i].capacity;
    while (*count && (*count==32 || bytes>1024*1024)) { bytes-=states[0].capacity; free(states[0].text); memmove(states,states+1,(--*count)*sizeof(*states)); }
    states[(*count)++]=state;
}
static void input_remember(TextInput *in,unsigned kind,double now,bool selection) {
    input_clear_history(in->redo,&in->redo_count);
    bool combine=kind && in->edit_kind==kind && !selection && now>=in->edit_at && now-in->edit_at<=.7;
    if (!combine) {
        if (in->length>=1024*1024) { input_clear_history(in->undo,&in->undo_count); in->edit_kind=0; return; }
        TextInputState state=input_state(in); state.text=dx_strdup(in->text); state.capacity=in->length+1;
        input_push(in->undo,&in->undo_count,state);
    }
    in->edit_kind=kind; in->edit_at=now;
}
bool input_history(TextInput *in,bool redo) {
    in->edit_kind=0;
    TextInputState *from=redo ? in->redo : in->undo,*to=redo ? in->undo : in->redo;
    size_t *fc=redo ? &in->redo_count : &in->undo_count,*tc=redo ? &in->undo_count : &in->redo_count;
    if (!*fc) return false;
    TextInputState state=from[--*fc]; memset(from+*fc,0,sizeof(*from)); input_push(to,tc,input_state(in));
    in->text=state.text; in->length=state.length; in->capacity=state.capacity; in->cursor=state.cursor; in->anchor=state.anchor; in->view_start=state.view_start;
    in->select_all=state.select_all; in->dragging=false; in->revision++; return true;
}
void input_set(TextInput *in,const char *text) {
    char *owned=dx_strdup(text); uint64_t revision=in->revision; input_free(in); in->revision=revision;
    in->text=owned; in->length=strlen(owned); in->capacity=in->length+1;
    in->cursor=in->anchor=in->length; in->view_start=0; in->select_all=true; in->dragging=false; in->clicks=0; in->last_click=-1; in->revision++;
}
static void input_insert_at(TextInput *in,const char *s,unsigned kind,double now) {
    if (!in->text) { in->length=in->capacity=in->cursor=in->anchor=in->view_start=0; }
    size_t start,end; input_bounds(in,&start,&end); size_t n=strlen(s),remaining=in->length-(end-start);
    if (remaining>65536 || n>65536-remaining) return;
    bool changed=n!=end-start || (n && memcmp(in->text+start,s,n));
    if (changed) {
        char *owned=NULL;
        bool alias=in->text && (uintptr_t)s>=(uintptr_t)in->text && (uintptr_t)s<=(uintptr_t)in->text+in->length;
        if (!kind) for (int stack=0;stack<2 && !alias;stack++) {
            TextInputState *states=stack ? in->redo : in->undo; size_t count=stack ? in->redo_count : in->undo_count;
            for (size_t i=0;i<count && !alias;i++) alias=states[i].text && (uintptr_t)s>=(uintptr_t)states[i].text && (uintptr_t)s<=(uintptr_t)states[i].text+states[i].length;
        }
        if (alias) { owned=dx_strdup(s); s=owned; }
        input_remember(in,kind,now,start!=end);
        size_t length=remaining+n;
        if (length+1>in->capacity) { in->capacity=(length+1)*2; if (in->capacity<64) in->capacity=64; in->text=dx_realloc(in->text,in->capacity); }
        memmove(in->text+start+n,in->text+end,in->length-end); memcpy(in->text+start,s,n); in->text[length]=0; free(owned);
        in->length=length; in->revision++;
    } else in->edit_kind=0;
    in->cursor=in->anchor=start+n; in->select_all=false; in->dragging=false;
}
UIMenuLayout ui_menu_layout(Vector2 origin,int count,float row_height,float width,float scale,Vector2 window) {
    if (count<=0 || !isfinite(scale) || scale<=0 || !isfinite(row_height) || row_height<=0 || !isfinite(width) || width<=0 || !isfinite(window.x) || !isfinite(window.y) || window.x<=0 || window.y<=0) return (UIMenuLayout){0};
    float margin=8*scale,padding=16*scale,step=row_height*scale;
    double capacity=floor((window.y-2*margin-padding)/step);
    if (!isfinite(step) || step<=0 || !isfinite(capacity) || capacity<1 || window.x<=2*margin) return (UIMenuLayout){0};
    int visible=capacity>=count ? count : (int)capacity;
    float w=fminf(width*scale,fmaxf(0,window.x-2*margin)),h=visible*step+padding;
    float x=fmaxf(margin,fminf(origin.x,window.x-w-margin)),y=fmaxf(margin,fminf(origin.y,window.y-h-margin));
    return (UIMenuLayout){.panel={x,y,w,h},.items={x+margin,y+margin,w-2*margin,visible*step},.visible=visible};
}
UIMenuLayout ui_action_menu_layout(Vector2 origin,int count,float width,float scale,Vector2 window) {
    if (count<1 || !isfinite(origin.x)||!isfinite(origin.y)||!isfinite(scale)||scale<=0||!isfinite(width)||width<=0||!isfinite(window.x)||!isfinite(window.y)||window.x/scale<170 || window.y/scale<100) return (UIMenuLayout){0};
    float w=fminf(width,window.x/scale-12),capacity=floorf((fminf(620,window.y/scale-35)-40)/28);
    int visible=(int)fminf(count,fmaxf(1,capacity)); float height=visible*28+40;
    float place=fmaxf(height,fminf(560,count*30.0f+46));
    float x=fmaxf(5,fminf(origin.x/scale,window.x/scale-w-6)),y=fmaxf(5,fminf(origin.y/scale,window.y/scale-place-10));
    return (UIMenuLayout){.panel={x*scale,y*scale,w*scale,height*scale},.items={(x+5)*scale,(y+35)*scale,(w-10)*scale,visible*28*scale},.visible=visible};
}
void input_insert(TextInput *in,const char *text) { input_insert_at(in,text,0,0); }
char *input_selection_text(const TextInput *in) {
    size_t start,end; input_bounds(in,&start,&end); if (start==end) return NULL;
    char *text=dx_alloc(end-start+1); memcpy(text,in->text+start,end-start); text[end-start]=0; return text;
}
static void input_key_at(TextInput *in,int key,unsigned modifiers,double now) {
    size_t start,end; input_bounds(in,&start,&end);
    size_t cursor=in->select_all ? in->length : input_boundary(in,in->cursor),anchor=in->select_all ? 0 : input_boundary(in,in->anchor),next=cursor;
    bool shift=(modifiers&DX_MOD_SHIFT)!=0;
    size_t line_start=0,line_end=in->length;
    if (in->multiline) {
        line_start=cursor; while (line_start && in->text[line_start-1]!='\n') line_start--;
        line_end=cursor; while (line_end<in->length && in->text[line_end]!='\n') line_end++;
    }
#ifdef __APPLE__
    bool line=(modifiers&DX_MOD_SUPER)!=0,word=(modifiers&DX_MOD_ALT)!=0;
#else
    bool line=false,word=(modifiers&DX_MOD_CONTROL)!=0;
#endif
    if (key==KEY_BACKSPACE || key==KEY_DELETE) {
        if (start==end) {
            if (key==KEY_BACKSPACE) start=line ? line_start : word ? input_word_left(in,cursor) : dx_utf8_prev(in->text,cursor);
            else end=line ? line_end : word ? input_word_right(in,cursor) : input_next(in,cursor);
        }
        if (start<end) { input_remember(in,now<0 ? 0 : key==KEY_BACKSPACE ? 2 : 3,now,cursor!=anchor); memmove(in->text+start,in->text+end,in->length-end+1); in->length-=end-start; in->revision++; }
        in->cursor=in->anchor=start; in->select_all=false; in->dragging=false; return;
    }
    if (key==KEY_LEFT) next=!shift && !line && !word && start!=end ? start : line ? line_start : word ? input_word_left(in,cursor) : dx_utf8_prev(in->text,cursor);
    else if (key==KEY_RIGHT) next=!shift && !line && !word && start!=end ? end : line ? line_end : word ? input_word_right(in,cursor) : input_next(in,cursor);
    else if (key==KEY_HOME) next=line_start;
    else if (key==KEY_END) next=line_end;
    else return;
    in->cursor=next; in->anchor=shift ? anchor : next; in->select_all=false; in->dragging=false; in->edit_kind=0;
}
void input_key(TextInput *in,int key,unsigned modifiers) { input_key_at(in,key,modifiers,-1); }
static void input_copy_selection(TextInput *in) { char *text=input_selection_text(in); if (text) { SetClipboardText(text); free(text); } }
void input_update(TextInput *in) {
#ifdef DUXCEL_DEV_REVIEW
    if (dx_review_input_blocked()) return;
#endif
    in->active_frame=input_frame;
    bool shortcut=!in->native_edit; in->native_edit=false;
    if (shortcut && ((ui_key_shortcut(KEY_Z) && ui_pressed(KEY_Z)) || (ui_key_shortcut(KEY_Y) && ui_pressed(KEY_Y)))) {
        input_history(in,ui_pressed(KEY_Y) || ui_key_shift(KEY_Z)); while (GetCharPressed()) {} return;
    }
    if (shortcut && ui_key_shortcut(KEY_A) && ui_pressed(KEY_A)) in->select_all=true;
    if (shortcut && ui_key_shortcut(KEY_C) && ui_pressed(KEY_C)) input_copy_selection(in);
    if (shortcut && ui_key_shortcut(KEY_V) && ui_pressed(KEY_V)) { const char *s=GetClipboardText(); if (s && strlen(s)<=65536 && (in->multiline || !strpbrk(s,"\r\n"))) input_insert(in,s); }
    if (in->multiline && ((ui_pressed(KEY_ENTER) && !ui_key_shortcut(KEY_ENTER)) || (ui_pressed(KEY_KP_ENTER) && !ui_key_shortcut(KEY_KP_ENTER)))) input_insert(in,"\n");
    const int keys[]={KEY_LEFT,KEY_RIGHT,KEY_HOME,KEY_END,KEY_BACKSPACE,KEY_DELETE};
    for (size_t i=0;i<sizeof(keys)/sizeof(*keys);i++) if (ui_key(keys[i])) { input_key_at(in,keys[i],modifiers_for(keys[i]),GetTime()); break; }
    int cp;
    while ((cp=GetCharPressed())!=0) if (cp>=32 && !ui_shortcut()) {
        int n=0; const char *encoded=CodepointToUTF8(cp,&n); char text[8]={0};
        if (n>0 && n<8) { memcpy(text,encoded,(size_t)n); input_insert_at(in,text,1,GetTime()); }
    }
}
static float input_width(UIContext *u,const TextInput *in,size_t start,size_t end,float size) {
    int face=size<=11; float width=0;
    for (size_t at=start;at<end;) { int bytes=0,cp=next_codepoint(in->text+at,&bytes); if (bytes<=0) break; width+=text_advance(u,face,cp,size); at+=(size_t)bytes; }
    return width;
}
static size_t input_hit(UIContext *u,const TextInput *in,Rectangle r,Vector2 mouse,float size) {
    int face=size<=11; float x=r.x+8*u->scale;
    for (size_t at=in->view_start;at<in->length;) {
        int bytes=0,cp=next_codepoint(in->text+at,&bytes); if (bytes<=0) break;
        float width=text_advance(u,face,cp,size); if (mouse.x<x+width*.5f) return at;
        x+=width; at+=(size_t)bytes;
    }
    return in->length;
}
void input_draw_sized(UIContext *u,TextInput *in,Rectangle r,float size,bool frame) {
#ifdef DUXCEL_DEV_REVIEW
    TextInput frozen;
    if (dx_review_input_blocked()) { frozen=*in; if (frozen.active_frame+1==input_frame) frozen.active_frame=input_frame; in=&frozen; }
#endif
    bool active=in->active_frame==input_frame; Vector2 mouse=ui_mouse_position(); Rectangle visible=u->pointer_blocked ? (Rectangle){0} : u->clipping ? intersect(r,u->clip) : r;
    if (active && CheckCollisionPointRec(mouse,visible)) u->cursor=MOUSE_CURSOR_IBEAM;
    size_t cursor=in->select_all ? in->length : input_boundary(in,in->cursor); in->view_start=input_boundary(in,in->view_start);
    if (in->view_start>cursor) in->view_start=cursor;
    float available=fmaxf(0,r.width-16*u->scale),width=input_width(u,in,in->view_start,cursor,size);
    while (in->view_start<cursor && width>available) { size_t next=input_next(in,in->view_start); width-=input_width(u,in,in->view_start,next,size); in->view_start=next; }
    while (in->view_start) { size_t previous=dx_utf8_prev(in->text,in->view_start); float extra=input_width(u,in,previous,in->view_start,size); if (width+extra>available) break; in->view_start=previous; width+=extra; }
    DxPointerGesture gesture={0}; bool replay=active && ui_take_left_gesture_from(visible,&gesture);
    Vector2 pressed=replay ? (Vector2){gesture.x0,gesture.y0} : mouse;
    if (active && (replay || IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) && CheckCollisionPointRec(pressed,visible)) {
        in->edit_kind=0;
        size_t at=input_hit(u,in,r,pressed,size); double now=GetTime();
        in->clicks=now-in->last_click<.32 && fabsf(pressed.x-in->click_x)<5*u->scale ? in->clicks+1 : 1; in->last_click=now; in->click_x=pressed.x;
        if (in->clicks>=3) { in->select_all=true; in->cursor=in->length; in->anchor=0; }
        else if (in->clicks==2) {
            if (at==in->length && at) at=dx_utf8_prev(in->text,at);
            in->select_all=false; in->anchor=at; in->cursor=input_next(in,at);
            if (at<in->length && input_word(in,at)) { while (in->anchor && input_word(in,dx_utf8_prev(in->text,in->anchor))) in->anchor=dx_utf8_prev(in->text,in->anchor); while (in->cursor<in->length && input_word(in,in->cursor)) in->cursor=input_next(in,in->cursor); }
        }
        else { if (!(replay ? gesture.modifiers&DX_MOD_SHIFT : ui_shift())) in->anchor=at; else if (in->select_all) in->anchor=0; in->cursor=at; in->select_all=false; }
        in->dragging=in->clicks==1;
    }
    Vector2 dragged=replay ? (Vector2){gesture.x1,gesture.y1} : mouse;
    if (active && in->dragging && (replay || IsMouseButtonDown(MOUSE_BUTTON_LEFT))) { if (dragged.x<r.x && in->view_start) in->view_start=dx_utf8_prev(in->text,in->view_start); in->cursor=input_hit(u,in,r,dragged,size); in->select_all=false; }
    if (!active || !IsMouseButtonDown(MOUSE_BUTTON_LEFT)) in->dragging=false;
    size_t start,end; input_bounds(in,&start,&end); cursor=in->select_all ? in->length : in->cursor;
    if (frame) { DrawRectangleRec(r,WHITE); DrawRectangleLinesEx(r,1*u->scale,active ? UI_ACCENT : UI_BORDER); }
    if (active && end>start) {
        float left=start<=in->view_start ? 0 : input_width(u,in,in->view_start,start,size),right=end<=in->view_start ? 0 : input_width(u,in,in->view_start,end,size);
        left=fminf(available,left); right=fminf(available,right);
        if (right>left) DrawRectangleRec((Rectangle){r.x+8*u->scale+left,r.y+4*u->scale,right-left,r.height-8*u->scale},active ? UI_TINT : UI_BG);
    }
    ui_text_clip(u,in->text ? in->text+in->view_start : "",r,size,UI_TEXT,false);
    if (active && start==end && ((int)(GetTime()*2)%2)==0) {
        float x=r.x+8*u->scale+input_width(u,in,in->view_start,cursor,size); x=fminf(r.x+r.width-8*u->scale,fmaxf(r.x+8*u->scale,x));
        DrawLineV((Vector2){x,r.y+7*u->scale},(Vector2){x,r.y+r.height-7*u->scale},UI_ACCENT);
    }
}

void input_draw(UIContext *u,TextInput *in,Rectangle r) { input_draw_sized(u,in,r,14,true); }
