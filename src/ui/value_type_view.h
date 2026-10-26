#ifndef DUXCEL_UI_VALUE_TYPE_VIEW_H
#define DUXCEL_UI_VALUE_TYPE_VIEW_H
#include "value_type.h"
#include "ui.h"
typedef struct {
    Rectangle panel,title,family[5],sign[2],bits[5],details[2],minus[2],value[2],plus[2],help,apply,cancel;
    bool valid;
} UIValueTypeLayout;
typedef enum { UI_VALUE_TYPE_VIEW_IDLE,UI_VALUE_TYPE_VIEW_APPLY,UI_VALUE_TYPE_VIEW_CANCEL } UIValueTypeViewEvent;
UIValueTypeLayout ui_value_type_layout(Rectangle viewport,float scale);
/* Standalone metadata-only modal. Reuses existing buttons/labels/focus rings;
   no raw text, editor/model owner, callback or type conversion is accepted.
   Input=false never dequeues keys/gestures or changes chooser/output. Enter
   applies (except Cancel focus); Escape cancels. Invalid metadata disables Apply.
   Only a terminal event changes out/active; IDLE leaves output untouched. */
UIValueTypeViewEvent ui_value_type_view_draw(UIValueTypeChooser *chooser,UIContext *ui,
    Rectangle viewport,bool input,UIValueType *out);
#endif
