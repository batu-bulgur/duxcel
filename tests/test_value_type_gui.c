#define _POSIX_C_SOURCE 200809L
#include "ui/value_type_view.h"
#include "platform/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL value-type GUI:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
/* Stage actions enter the following frame after ui_update clears old input. */
static bool pointer_pending;
static DxPointerGesture pointer;
static DxKeyModifiers key_pending;
static void click(Rectangle r) { float x=r.x+r.width/2,y=r.y+r.height/2; pointer=(DxPointerGesture){x,y,x,y,0,false}; pointer_pending=true; }
static void press(int key,unsigned modifiers) { key_pending=(DxKeyModifiers){key,modifiers,false}; }
static void dispatch(void) {
    if (pointer_pending) { ui_note_pointer_gesture(pointer); pointer_pending=false; }
    if (key_pending.key) { ui_note_key_press(key_pending.key,key_pending.modifiers); key_pending=(DxKeyModifiers){0}; }
}
static unsigned ink(Image image,Rectangle r) {
    Color *pixels=LoadImageColors(image); CHECK(pixels); unsigned count=0;
    float sx=(float)image.width/GetScreenWidth(),sy=(float)image.height/GetScreenHeight();
    for (int y=(int)(r.y*sy);y<(int)((r.y+r.height)*sy) && y<image.height;y++) for (int x=(int)(r.x*sx);x<(int)((r.x+r.width)*sx) && x<image.width;x++) if (x>=0 && y>=0) { Color c=pixels[y*image.width+x]; if (c.a>240 && c.r+c.g+c.b<420) count++; }
    UnloadImageColors(pixels); return count;
}
int main(void) {
    CHECK(getenv("DUXCEL_TEST_INSTANCE") && !strcmp(getenv("DUXCEL_TEST_INSTANCE"),"1"));
    dx_native_prepare(); SetConfigFlags(FLAG_WINDOW_HIDDEN|FLAG_WINDOW_RESIZABLE|FLAG_MSAA_4X_HINT); InitWindow(960,640,"Duxcel value type [TEST]"); CHECK(IsWindowReady());
    dx_native_setup(GetWindowHandle()); dx_native_test_identity(true); dx_native_window_visible(false); SetTargetFPS(60); SetExitKey(KEY_NULL);
    UIContext ui={0}; CHECK(ui_init(&ui,"assets/fonts/IBMPlexSans-Regular.otf","assets/fonts/IBMPlexSans-Medium.otf","assets/fonts/IBMPlexSans-SemiBold.otf","assets/fonts/JetBrainsMono-Regular.ttf"));
    UIValueTypeChooser chooser; CHECK(ui_value_type_begin((ManualValueView){.present=true,.kind=MANUAL_VALUE_TEXT},&chooser));
    UIValueType result={.present=true,.kind=MANUAL_VALUE_SQL_NULL}; unsigned stage=0,settled=0; UIValueTypeViewEvent terminal=UI_VALUE_TYPE_VIEW_IDLE;
    for (unsigned frame=0;frame<800 && stage<24 && !WindowShouldClose();frame++) {
        dx_input_begin_frame(); ui_update(&ui); dispatch(); Rectangle viewport={0,0,GetScreenWidth(),GetScreenHeight()}; UIValueTypeLayout l=ui_value_type_layout(viewport,ui.scale); CHECK(l.valid);
        BeginDrawing(); ClearBackground(UI_BG); UIValueTypeViewEvent event=ui_value_type_view_draw(&chooser,&ui,viewport,true,&result); if (event!=UI_VALUE_TYPE_VIEW_IDLE) terminal=event;
        if (!frame) {
            UIValueType original=result; UIValueType value=chooser.value; UIValueTypeFocus original_focus=chooser.focus;
            ui_note_key_press(KEY_DOWN,0); CHECK(ui_value_type_view_draw(&chooser,&ui,viewport,false,&result)==UI_VALUE_TYPE_VIEW_IDLE);
            DxKeyModifiers key; CHECK(ui_take_navigation_key(&key) && key.key==KEY_DOWN);
            CHECK(chooser.value.kind==value.kind && chooser.focus==original_focus && result.kind==original.kind);
        }
        EndDrawing(); CHECK(!ui.scissor_depth && !ui.paint_depth);
        if (++settled<24) continue; settled=0;
        switch (stage) {
        case 0: { Image image=LoadImageFromScreen(); CHECK(ink(image,l.title)>20); UnloadImage(image); click(l.family[UI_VALUE_TYPE_INTEGER]); break; }
        case 1: CHECK(chooser.value.number.kind==NUM_I64); click(l.sign[1]); break;
        case 2: CHECK(chooser.value.number.kind==NUM_U64); click(l.bits[4]); break;
        case 3: CHECK(chooser.value.number.kind==NUM_U128); press(KEY_TAB,0); break;
        case 4: CHECK(chooser.focus==UI_VALUE_TYPE_APPLY); press(KEY_ENTER,0); break;
        case 5: CHECK(terminal==UI_VALUE_TYPE_VIEW_APPLY && !chooser.active && result.present && result.number.kind==NUM_U128); CHECK(ui_value_type_begin((ManualValueView){.present=true,.kind=MANUAL_VALUE_TEXT},&chooser)); terminal=UI_VALUE_TYPE_VIEW_IDLE; click(l.family[UI_VALUE_TYPE_DECIMAL]); break;
        case 6: CHECK(chooser.value.number.kind==NUM_DECIMAL && chooser.value.number.precision==38 && !chooser.value.number.scale); click(l.minus[0]); break;
        case 7: CHECK(chooser.value.number.precision==37); click(l.plus[1]); break;
        case 8: CHECK(chooser.value.number.scale==1 && chooser.focus==UI_VALUE_TYPE_SCALE); press(KEY_TAB,DX_MOD_SHIFT); break;
        case 9: CHECK(chooser.focus==UI_VALUE_TYPE_PRECISION); press(KEY_RIGHT,0); break;
        case 10: CHECK(chooser.value.number.precision==38 && chooser.value.number.scale==1); press(KEY_ENTER,0); break;
        case 11: CHECK(terminal==UI_VALUE_TYPE_VIEW_APPLY && result.number.kind==NUM_DECIMAL && result.number.precision==38 && result.number.scale==1); CHECK(ui_value_type_begin((ManualValueView){0},&chooser)); terminal=UI_VALUE_TYPE_VIEW_IDLE; ui_set_scale(&ui,1.5f); SetWindowSize(720,480); break;
        case 12: { UIValueTypeFamily family; CHECK(ui_value_type_family(chooser.value,&family) && family==UI_VALUE_TYPE_CLEAR); Image image=LoadImageFromScreen(); CHECK(ink(image,l.title)>20); UnloadImage(image); click(l.family[UI_VALUE_TYPE_TEXT]); break; }
        case 13: CHECK(chooser.value.present && chooser.value.kind==MANUAL_VALUE_TEXT); click(l.apply); break;
        case 14: CHECK(terminal==UI_VALUE_TYPE_VIEW_APPLY && result.present && result.kind==MANUAL_VALUE_TEXT); CHECK(ui_value_type_begin((ManualValueView){.present=true,.kind=MANUAL_VALUE_TEXT},&chooser)); terminal=UI_VALUE_TYPE_VIEW_IDLE; click(l.family[UI_VALUE_TYPE_NULL]); break;
        case 15: CHECK(chooser.value.present && chooser.value.kind==MANUAL_VALUE_SQL_NULL); press(KEY_ENTER,0); break;
        case 16: CHECK(terminal==UI_VALUE_TYPE_VIEW_APPLY && result.present && result.kind==MANUAL_VALUE_SQL_NULL); CHECK(ui_value_type_begin((ManualValueView){.present=true,.kind=MANUAL_VALUE_TEXT},&chooser)); terminal=UI_VALUE_TYPE_VIEW_IDLE; click(l.family[UI_VALUE_TYPE_INTEGER]); break;
        case 17: CHECK(chooser.value.number.kind==NUM_I64); press(KEY_ESCAPE,0); break;
        case 18: CHECK(terminal==UI_VALUE_TYPE_VIEW_CANCEL && result.present && result.kind==MANUAL_VALUE_SQL_NULL); CHECK(ui_value_type_begin((ManualValueView){.present=true,.kind=MANUAL_VALUE_TEXT},&chooser)); terminal=UI_VALUE_TYPE_VIEW_IDLE; click(l.family[UI_VALUE_TYPE_CLEAR]); break;
        case 19: CHECK(!chooser.value.present && chooser.value.kind==MANUAL_VALUE_SQL_NULL); click(l.apply); break;
        case 20: CHECK(terminal==UI_VALUE_TYPE_VIEW_APPLY && !result.present); CHECK(ui_value_type_begin((ManualValueView){.present=true,.kind=MANUAL_VALUE_TEXT},&chooser)); chooser.value=(UIValueType){.present=true,.kind=MANUAL_VALUE_NUMBER,.number={NUM_FLOAT,0,0}}; terminal=UI_VALUE_TYPE_VIEW_IDLE; click(l.apply); break;
        case 21: CHECK(terminal==UI_VALUE_TYPE_VIEW_IDLE && chooser.active && !ui_value_type_can_apply(&chooser) && !result.present); press(KEY_TAB,0); break;
        case 22: CHECK(chooser.focus==UI_VALUE_TYPE_CANCEL); press(KEY_ENTER,0); break;
        case 23: CHECK(terminal==UI_VALUE_TYPE_VIEW_CANCEL && !chooser.active && !result.present); break;
        }
        stage++;
    }
    CHECK(stage==24); ui_destroy(&ui); dx_native_shutdown(); CloseWindow(); printf("PASS: native explicit value-type workflow (24 stages, %u checks)\n",checks); return 0;
}
