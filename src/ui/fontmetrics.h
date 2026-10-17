#ifndef DX_FONT_METRICS_H
#define DX_FONT_METRICS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Borrowed SFNT bytes. Units let the native renderer use CSS em sizes rather
   than stb's ascent-to-descent pixel height, without a font-library dependency. */
typedef struct {
    const unsigned char *data;
    size_t size,hmtx,cmap4,cmap12,cmap4_end,cmap12_end;
    unsigned units,metrics,glyphs;
    int ascent,descent;
    float height_per_em;
    bool valid;
} UIFontMetrics;
bool ui_font_metrics_init(UIFontMetrics *metrics,const unsigned char *data,size_t size);
unsigned ui_font_glyph(const UIFontMetrics *metrics,unsigned codepoint);
float ui_font_advance_em(const UIFontMetrics *metrics,unsigned codepoint);
#endif
