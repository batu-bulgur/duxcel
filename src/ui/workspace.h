#ifndef DX_WORKSPACE_UI_H
#define DX_WORKSPACE_UI_H
#include <raylib.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    Rectangle canvas,left,context,inspector;
    bool narrow,left_overlay,right_overlay,valid;
} UIWorkspaceLayout;
/* Logical coordinates, independent of display density and worksheet zoom.
   The temporary left overlay never changes the section visibility preferences. */
UIWorkspaceLayout ui_workspace_layout(float width,float height,float chrome,
    float left_width,float inspector_width,bool left_visible,bool drawer,bool authoring,bool focus);
#define UI_SHELF_FIELDS 8
typedef struct {
    Rectangle panel,label,fields,tokens[UI_SHELF_FIELDS],add;
    size_t count;
    bool valid;
} UIShelfLayout;
/* The two axes use CSS-style wrapping, including the Add field button. All
   coordinates and intrinsic token widths are logical, before display scale. */
UIShelfLayout ui_shelf_layout(Rectangle bounds,const float *widths,size_t count,bool compact,bool last);
size_t ui_shelf_insertion(const Rectangle *tokens,size_t count,Vector2 point);
size_t ui_shelf_neighbor(const Rectangle *tokens,size_t count,size_t current,int vertical_direction);
void ui_workspace_context_height(UIWorkspaceLayout *workspace,float height);
typedef struct { Rectangle icon,title,source,browse,browse_text,chevron; bool valid; } UIContextTitleLayout;
UIContextTitleLayout ui_context_title_layout(Rectangle bounds,bool compact,float title_width,float source_width,float browse_text_width);
typedef struct { Rectangle address,address_text,chevron,function,value,type; bool valid; } UICellContextLayout;
/* Logical measurements of the source value strip. The viewport, rather than
   the remaining worksheet width, controls the compact and type-label states. */
UICellContextLayout ui_cell_context_layout(Rectangle bounds,float viewport_width,bool workspace_button,bool focus,float type_width);
typedef struct { Rectangle label,preview,matrix,chart; float height,content_width; bool stacked,scrollable,valid; } UIAuthoringLayout;
UIAuthoringLayout ui_authoring_layout(Rectangle canvas,float scroll,float context_height);
float ui_matrix_column_width(size_t column,size_t dimensions);
float ui_matrix_column_left(size_t column,size_t dimensions);
size_t ui_matrix_column_at(float x,size_t columns,size_t dimensions);
typedef struct { Rectangle panel,title,sort[2],search,all,dates,list,condition,term,end,scope,footer,clear,cancel,apply; bool valid; } UIFilterLayout;
/* Popup measurements match the design definition; only the value viewport shrinks. */
UIFilterLayout ui_filter_layout(Vector2 anchor,Vector2 window,float scale,int terms,bool dates);
#endif
