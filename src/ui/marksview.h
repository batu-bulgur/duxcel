#ifndef DUXCEL_MARKSVIEW_H
#define DUXCEL_MARKSVIEW_H
#include <raylib.h>
#include <stdbool.h>
#define UI_MARKS_FIELDS 8
#define UI_MARKS_ROLES 6
typedef struct {
    unsigned measures,filters;
    bool assigned[UI_MARKS_ROLES],plot;
    float filter_widths[UI_MARKS_FIELDS]; /* intrinsic badge widths, logical units */
} UIMarksContent;
typedef struct {
    Rectangle panel,viewport,heading,representation,measure_heading,measure_list,plot;
    Rectangle measures[UI_MARKS_FIELDS],targets[UI_MARKS_ROLES],filter_heading,filters[UI_MARKS_FIELDS],empty_filter,inherited,scrollbar;
    float content_height,maximum,offset,content_width;
    bool valid,scrollable;
} UIMarksLayout;
/* Matches the definition's complete scrollable Marks section, including its
   heading, 10px padding, 9px scrollbar reservation and intrinsic filter wrap. */
UIMarksLayout ui_marks_layout(Rectangle panel,float scale,const UIMarksContent *content,float offset);
float ui_marks_content_height(float width,const UIMarksContent *content);
#endif
