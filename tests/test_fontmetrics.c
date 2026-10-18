#include "ui/fontmetrics.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL font metrics:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static unsigned char *read_font(const char *path,size_t *size) {
    FILE *file=fopen(path,"rb"); CHECK(file); CHECK(!fseek(file,0,SEEK_END)); long length=ftell(file); CHECK(length>0); rewind(file);
    *size=(size_t)length; unsigned char *data=malloc(*size); CHECK(data && fread(data,1,*size,file)==*size); fclose(file); return data;
}
int main(void) {
    const char *paths[]={"assets/fonts/IBMPlexSans-Regular.otf","assets/fonts/IBMPlexSans-Medium.otf","assets/fonts/IBMPlexSans-SemiBold.otf","assets/fonts/IBMPlexSans-Italic.otf","assets/fonts/IBMPlexSans-SemiBoldItalic.otf","assets/fonts/JetBrainsMono-Regular.ttf"};
    for (size_t face=0;face<6;face++) {
        size_t size; unsigned char *data=read_font(paths[face],&size); UIFontMetrics metrics;
        CHECK(ui_font_metrics_init(&metrics,data,size) && metrics.valid && metrics.units==1000);
        CHECK(fabsf(metrics.height_per_em-(face==5 ? 1.32f : 1.3f))<.00001f);
        float digit=ui_font_advance_em(&metrics,'0'); CHECK(digit>0 && digit<1);
        for (unsigned cp='0';cp<='9';cp++) CHECK(fabsf(ui_font_advance_em(&metrics,cp)-digit)<.00001f);
        CHECK(ui_font_glyph(&metrics,'A') && ui_font_glyph(&metrics,0x131) && ui_font_glyph(&metrics,' ') && ui_font_advance_em(&metrics,' ')>0);
        CHECK(ui_font_glyph(&metrics,0xfffd) && ui_font_glyph(&metrics,0xfffd)!=ui_font_glyph(&metrics,'?') && ui_font_advance_em(&metrics,0xfffd)>0);
        CHECK(!ui_font_glyph(&metrics,0x8868) && !ui_font_glyph(&metrics,0x1f986)); /* No bundled CJK/emoji fallback. */
        CHECK((ui_font_glyph(&metrics,0x2318)!=0)==(face==5));
        CHECK((ui_font_glyph(&metrics,0x21e7)!=0)==(face==5));
        CHECK(!ui_font_glyph(&metrics,0x110000) && !ui_font_advance_em(&metrics,0x110000));
        for (size_t length=0;length<512;length+=7) { UIFontMetrics truncated; CHECK(!ui_font_metrics_init(&truncated,data,length)); }
        unsigned char *bad=malloc(size); CHECK(bad); memcpy(bad,data,size); bad[4]=255;bad[5]=255; CHECK(!ui_font_metrics_init(&metrics,bad,size));
        memcpy(bad,data,size); bad[0]='b'; CHECK(!ui_font_metrics_init(&metrics,bad,size)); free(bad); free(data);
    }
    UIFontMetrics empty; CHECK(!ui_font_metrics_init(&empty,NULL,0)); CHECK(!ui_font_glyph(&empty,'A') && !ui_font_advance_em(&empty,'A'));
    printf("PASS: %u font metric checks (bundled em calibration, exact tabular advances, Unicode cmap, Command fallback and malformed/truncated bounds)\n",checks); return 0;
}
