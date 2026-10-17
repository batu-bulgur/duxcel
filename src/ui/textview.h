#ifndef DUXCEL_TEXTVIEW_H
#define DUXCEL_TEXTVIEW_H
#include "ui.h"
#define DX_TEXT_LINES 65536
/* Only line offsets are owned here; the inspected value is a separate snapshot. */
typedef struct { size_t *offsets,count,capacity; float width,scale,font_size; int raster_size; bool limited; } TextLines;
void text_lines_free(TextLines *lines);
void text_lines_layout(TextLines *lines,UIContext *ui,const char *text,float width);
void text_lines_draw(const TextLines *lines,UIContext *ui,const char *text,size_t first,Rectangle rect);
void text_lines_layout_sized(TextLines *lines,UIContext *ui,const char *text,float width,float size);
void text_lines_draw_sized(const TextLines *lines,UIContext *ui,const char *text,size_t first,Rectangle rect,float size,float line_height,Color color);
typedef struct {
    TextLines lines;
    uint64_t revision,revealed_revision;
    size_t revealed_cursor;
    int scroll;
    double wheel;
    bool dragging_scroll;
    float scroll_grab;
} TextEditor;
void text_editor_free(TextEditor *editor);
bool text_editor_draw(TextEditor *editor,TextInput *input,UIContext *ui,Rectangle rect,bool active);
#endif
