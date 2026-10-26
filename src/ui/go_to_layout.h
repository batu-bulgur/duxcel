#ifndef DUXCEL_UI_GO_TO_LAYOUT_H
#define DUXCEL_UI_GO_TO_LAYOUT_H
#include <raylib.h>
#include <stdbool.h>
typedef struct { Rectangle panel,title,label,input,message,cancel,go; bool valid; } UIGoToLayout;
UIGoToLayout ui_go_to_layout(Rectangle viewport,float scale);
#endif
