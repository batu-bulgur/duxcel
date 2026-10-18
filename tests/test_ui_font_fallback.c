/* Actual ui.c + bundled font bytes + raylib CPU glyph rasterization. Font
   textures, drawing and window/input queries are bounded stubs: no GPU,
   window, engine, clipboard or OS-input coverage. Link with -Wl,-dead_strip
   on macOS so unused UI/platform functions need no production dependencies. */
#include "ui/ui.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL font fallback:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static struct {
    unsigned loads,omit_on_call,next_texture,live,unloads,draws;
    bool owned[128];
    Rectangle sources[8];
    float density;
    double time;
} state={.next_texture=2,.density=2};
static GlyphInfo default_glyph={.value='?',.advanceX=5};
static Rectangle default_rect={0,0,5,10};

Font GetFontDefault(void) {
    return (Font){.baseSize=10,.glyphCount=1,.texture={.id=1},.glyphs=&default_glyph,.recs=&default_rect};
}
bool IsFontValid(Font font) { return font.baseSize>0 && font.glyphCount>0 && font.glyphs && font.recs && font.texture.id; }
int GetGlyphIndex(Font font,int cp) {
    int fallback=0;
    for (int i=0;i<font.glyphCount;i++) {
        if (font.glyphs[i].value==cp) return i;
        if (font.glyphs[i].value=='?') fallback=i;
    }
    return fallback; /* Match raylib's silent '?' substitution. */
}
Font LoadFontFromMemory(const char *type,const unsigned char *data,int bytes,int size,const int *codes,int count) {
    (void)type; state.loads++; int filtered[1024],n=0;
    CHECK(count<=1024);
    for (int i=0;i<count;i++) if (!(state.loads==state.omit_on_call && codes[i]==0xfffd)) filtered[n++]=codes[i];
    Font font={.baseSize=size};
    font.glyphs=LoadFontData(data,bytes,size,filtered,n,FONT_DEFAULT,&font.glyphCount);
    CHECK(font.glyphs && font.glyphCount>0);
    font.recs=calloc((size_t)font.glyphCount,sizeof(*font.recs)); CHECK(font.recs);
    for (int i=0;i<font.glyphCount;i++) font.recs[i]=(Rectangle){i*128.0f,0,(float)font.glyphs[i].image.width,(float)font.glyphs[i].image.height};
    font.texture.id=++state.next_texture; CHECK(font.texture.id<128 && !state.owned[font.texture.id]);
    state.owned[font.texture.id]=true; state.live++; return font;
}
void UnloadFont(Font font) {
    CHECK(font.texture.id>2 && font.texture.id<128 && state.owned[font.texture.id]);
    state.owned[font.texture.id]=false; state.live--; state.unloads++;
    UnloadFontData(font.glyphs,font.glyphCount); free(font.recs);
}
void SetTextureFilter(Texture2D texture,int filter) { (void)texture; (void)filter; }
Texture2D LoadTexture(const char *path) { (void)path; return (Texture2D){.id=2}; }
void UnloadTexture(Texture2D texture) { CHECK(texture.id==2); }
const char *GetApplicationDirectory(void) { return "./"; }
int GetScreenWidth(void) { return 800; }
int GetScreenHeight(void) { return 600; }
int GetRenderWidth(void) { return (int)(800*state.density); }
int GetRenderHeight(void) { return (int)(600*state.density); }
Vector2 GetWindowScaleDPI(void) { return (Vector2){state.density,state.density}; }
int GetKeyPressed(void) { return 0; }
bool IsMouseButtonPressed(int button) { (void)button; return false; }
double GetTime(void) { return state.time; }
void ui_paint_destroy(UIContext *ui) { CHECK(!ui->paint_bytes && !ui->paint_allocations); }
void DrawTexturePro(Texture2D texture,Rectangle source,Rectangle target,Vector2 origin,float rotation,Color color) {
    (void)texture; (void)target; (void)origin; (void)rotation; (void)color;
    CHECK(state.draws<8); state.sources[state.draws++]=source;
}

static Font face(const UIContext *ui,int f) {
    return f==0 ? ui->body : f==1 ? ui->small : f==2 ? ui->medium : f==3 ? ui->emphasis : f==4 ? ui->italic : ui->bold_italic;
}
static bool init(UIContext *ui) {
    return ui_init(ui,"assets/fonts/IBMPlexSans-Regular.otf","assets/fonts/IBMPlexSans-Medium.otf","assets/fonts/IBMPlexSans-SemiBold.otf","assets/fonts/JetBrainsMono-Regular.ttf");
}
static bool close_float(float a,float b) { return fabsf(a-b)<.0001f; }
static void family(Font fonts[7],const UIContext *ui) { for (int f=0;f<6;f++) fonts[f]=face(ui,f); fonts[6]=ui->mono; }
static void same_family(const UIContext *ui,const Font old[7]) {
    Font current[7]; family(current,ui); CHECK(!memcmp(current,old,sizeof(current)));
    for (int f=0;f<7;f++) CHECK(state.owned[current[f].texture.id]);
}
static void expect_text(UIContext *ui) {
    static const char *unknown[]={"\xd8\xa7","\xe8\xa1\xa8","\xf0\x9f\xa6\x86"}; /* Unsupported BMP below/above table cap, supplementary. */
    for (int f=0;f<6;f++) {
        Font font=face(ui,f); int replacement=ui->fallback[f],question=GetGlyphIndex(font,'?');
        CHECK(font.glyphs[replacement].value==0xfffd && font.glyphs[question].value=='?' && replacement!=question);
        UIFontWeight weight=f<2 ? UI_REGULAR : f==2 ? UI_MEDIUM : f==3 ? UI_SEMIBOLD : f==4 ? UI_ITALIC : UI_SEMIBOLD_ITALIC;
        float size=f==1 ? 11 : 13,replacement_width=ui_measure_weight(ui,"\xef\xbf\xbd",size,weight);
        CHECK(replacement_width>0);
        for (size_t i=0;i<sizeof(unknown)/sizeof(*unknown);i++) {
            CHECK(close_float(ui_measure_weight(ui,unknown[i],size,weight),replacement_width));
            state.draws=0; ui_text_clip_weight(ui,unknown[i],(Rectangle){20,20,200,80},size,UI_TEXT,false,weight);
            CHECK(state.draws==1 && state.sources[0].x==font.recs[replacement].x);
        }
        state.draws=0; ui_text_clip_weight(ui,"?",(Rectangle){20,20,200,80},size,UI_TEXT,false,weight);
        CHECK(state.draws==1 && state.sources[0].x==font.recs[question].x);
        state.draws=0; ui_text_clip_weight(ui,unknown[1],(Rectangle){20,20,8*ui->scale,80},size,UI_TEXT,false,weight); CHECK(!state.draws);
    }
    char text[]="?\xe8\xa1\xa8\xf0\x9f\xa6\x86",saved[sizeof(text)]; memcpy(saved,text,sizeof(text));
    unsigned loads=state.loads;
    for (int i=0;i<100;i++) {
        CHECK(close_float(ui_measure(ui,text,13),ui_measure(ui,"?",13)+2*ui_measure(ui,"\xef\xbf\xbd",13)));
        state.draws=0; ui_text(ui,text,20,20,13,UI_TEXT); CHECK(state.draws==3);
        CHECK(state.sources[0].x!=state.sources[1].x && state.sources[1].x==state.sources[2].x);
    }
    CHECK(!memcmp(saved,text,sizeof(text)) && state.loads==loads && state.live==7);
    CHECK(ui->mono.glyphs[ui->marker_glyphs[0]].value=='N' && ui->mono.glyphs[ui->marker_glyphs[3]].value=='"');
}
int main(void) {
    SetTraceLogLevel(LOG_NONE);
    int codes[1024]; size_t count=ui_font_codepoints(codes); CHECK(count==980 && codes[count-1]==0xfffd);
    unsigned matches=0; for (size_t i=0;i<count;i++) matches+=codes[i]==0xfffd; CHECK(matches==1);
    /* Missing replacement in any temporary atlas must reject the entire
       family, including on initial/custom-font setup: '?' is not sufficient. */
    for (unsigned bad=1;bad<=6;bad++) {
        UIContext ui; state.omit_on_call=state.loads+bad; CHECK(!init(&ui));
        CHECK(!ui.fonts_loaded && !ui.mono_owned && !state.live); ui_destroy(&ui); CHECK(!state.live);
    }
    state.omit_on_call=0; UIContext ui; CHECK(init(&ui) && state.live==7); expect_text(&ui);
    Font old[7]; family(old,&ui); int fallback[6]; memcpy(fallback,ui.fallback,sizeof(fallback));
    float old_digit=ui.digit_width; unsigned loads=state.loads,unloads=state.unloads;
    ui_set_scale(&ui,1.5f); state.omit_on_call=state.loads+4; state.time=1; ui_update(&ui);
    CHECK(state.loads==loads+6 && state.unloads==unloads+6 && state.live==7 && ui.reload_after==3);
    same_family(&ui,old); CHECK(!memcmp(fallback,ui.fallback,sizeof(fallback)) && old_digit==ui.digit_width);
    loads=state.loads; state.time=2; ui_update(&ui); CHECK(state.loads==loads); same_family(&ui,old);
    state.omit_on_call=0; state.time=3; ui_update(&ui); CHECK(state.live==7 && !ui.reload_after && ui.raster_size!=old[0].baseSize);
    for (int f=0;f<7;f++) CHECK(!state.owned[old[f].texture.id]);
    expect_text(&ui); ui_destroy(&ui); CHECK(!state.live);
    printf("PASS: %u font fallback checks (actual ui.c, bundled CPU glyphs; no GPU/window)\n",checks); return 0;
}
