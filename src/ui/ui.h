#ifndef DUXCEL_UI_H
#define DUXCEL_UI_H
#include <raylib.h>
#include "../common.h"
#include "../platform/platform.h"
#include "carbon_icons.h"
#include "fontmetrics.h"
#include "paintlayer.h"
#include "motion.h"
#include "../workbook/format.h"
typedef struct {
    uint64_t key; double seen; UIMotion motion;
    Color current[3],from[3],target[3];
} UIControlFeedback;
typedef struct { unsigned char *data; int size; UIFontMetrics metrics; } UIFontSource;
typedef struct {
    float scale, requested_scale,canvas_font_scale;
    double paint_time;
    Vector2 dpi;
    Font body,small,medium,emphasis,italic,bold_italic,mono; /* bundled faces at display density */
    Texture2D icons; /* Original Carbon artwork, shared 64-pixel atlas. */
    int marker_glyphs[5];
    char *font_path,*medium_path,*emphasis_path,*italic_path,*bold_italic_path,*mono_path;
    bool fonts_loaded,mono_owned;
    int raster_size, small_raster,mono_raster;
    double reload_after;
    float digit_width; /* maximum digit advance at size 11, before UI/canvas scale */
    int glyph_index[6][0x2300], fallback[6];
    UIFontSource font_sources[6]; /* regular, medium, semibold, italics, mono */
    float em_advance[6][1024],em_height[6],command_advance,shift_advance,mono_em_height;
    int command_glyph,shift_glyph;
    bool advances_ready;
    Vector2 render_scale;
    Rectangle clip;
    bool clipping;
    bool pointer_blocked;
    Rectangle scissor_rects[8],saved_clips[8];
    bool saved_clipping[8];
    unsigned scissor_depth;
    UIPaintBuffer paint_buffers[UI_PAINT_BUFFERS];
    UIPaintFrame paint_frames[UI_PAINT_DEPTH];
    unsigned paint_depth,paint_peak_depth;
    size_t paint_bytes;
    uint64_t paint_allocations,paint_fallbacks;
    UIControlFeedback feedback[128];
    bool reduced_motion;
    int cursor;
} UIContext;
extern const Color UI_BG, UI_SURFACE, UI_TEXT, UI_MUTED, UI_BORDER, UI_ACCENT, UI_TINT;
extern const Color UI_FOCUS;
/* Shared deterministic glyph coverage for native atlases and CPU profiling. */
size_t ui_font_codepoints(int codepoints[1024]);
bool ui_init(UIContext *ui,const char *font_path,const char *medium_path,const char *emphasis_path,const char *mono_path);
void ui_update(UIContext *ui);
void ui_set_scale(UIContext *ui, float scale);
float ui_canvas_font_scale(float zoom);
void ui_set_canvas_zoom(UIContext *ui,float zoom);
void ui_destroy(UIContext *ui);
void ui_text(UIContext *ui, const char *text, float x, float y, float size, Color color);
float ui_measure(UIContext *ui, const char *text, float size);
float ui_measure_limit(UIContext *ui, const char *text, float size, float limit);
/* Fit complete middle-dot-separated status metrics, retaining original order.
   Skipped values remain in the application's customization menu. No allocation. */
size_t ui_fit_metrics(UIContext *ui,const char *text,float width,char output[2048]);
float ui_codepoint_width(UIContext *ui,int codepoint,float size);
void ui_set_clip(UIContext *ui, Rectangle clip);
void ui_clear_clip(UIContext *ui);
/* Nested pane viewports own both GPU clipping and pointer/text bounds. */
bool ui_scissor_begin(UIContext *ui,Rectangle clip);
void ui_scissor_end(UIContext *ui);
bool ui_paint_begin(UIContext *ui,Rectangle bounds,Vector2 reserve,float opacity);
void ui_paint_end(UIContext *ui);
/* Dedicated cache slots retain a complete picture after its controller closes. */
bool ui_paint_cache_begin(UIContext *ui,unsigned slot,Rectangle bounds,Vector2 reserve);
UIPaintImage ui_paint_cache_end(UIContext *ui);
void ui_paint_image(UIContext *ui,UIPaintImage image,float opacity,Vector2 offset);
/* Scoped screen-to-popup input mapping; reset before returning to the canvas. */
void ui_pointer_offset(Vector2 offset);
Vector2 ui_mouse_position(void);
void ui_paint_destroy(UIContext *ui);
void ui_paint_apply_clip(UIContext *ui);
void ui_border(Rectangle rectangle,Rectangle clip,float width,Color color);
/* Focus is retained for keyboard use; pointer activation does not paint a
   keyboard ring. Selection and editable-input outlines remain independent. */
bool ui_focus_visible(void);
void ui_focus_border(Rectangle rectangle,Rectangle clip,float width);
void ui_dashed_border(Rectangle rectangle,Rectangle clip,float width,float dash,float gap,Color color);
typedef struct { Rectangle panel,items; int visible; } UIMenuLayout;
UIMenuLayout ui_menu_layout(Vector2 origin,int count,float row_height,float width,float scale,Vector2 window);
UIMenuLayout ui_action_menu_layout(Vector2 origin,int count,float width,float scale,Vector2 window);
void ui_text_clip(UIContext *ui, const char *text, Rectangle box, float size, Color color, bool right);
typedef enum { UI_REGULAR,UI_MEDIUM,UI_SEMIBOLD,UI_ITALIC,UI_SEMIBOLD_ITALIC } UIFontWeight;
float ui_measure_weight(UIContext *ui,const char *text,float size,UIFontWeight weight);
void ui_text_clip_weight(UIContext *ui,const char *text,Rectangle box,float size,Color color,bool right,UIFontWeight weight);
/* Labels with padding supplied by their measured parent layout. */
void ui_label(UIContext *ui,const char *text,Rectangle box,float size,Color color,bool right,UIFontWeight weight);
/* Typed database values: NULL and empty text have non-interactive monospace marks. */
void ui_value(UIContext *ui,const char *text,bool is_null,Rectangle box,float size,Color color,bool right);
void ui_formatted_value(UIContext *ui,const char *raw,bool is_null,bool numeric,bool floating,double number,
    Rectangle box,float default_size,Color color,CellFormat format);
void ui_formatted_value_weight(UIContext *ui,const char *raw,bool is_null,bool numeric,bool floating,double number,
    Rectangle box,float default_size,Color color,CellFormat format,UIFontWeight weight);
/* General display only: caller opts in for FLOAT/DOUBLE; exact decimal/integer
   values, inspection, clipboard and exports retain their original text/type. */
bool ui_format_float(double number,unsigned digits,char output[64]);
/* Stable chart position, including finite opposite extremes whose span overflows. */
double ui_chart_fraction(double value,double lower,double upper);
void ui_float_value(UIContext *ui,const char *raw,bool is_null,double number,Rectangle box,float size,Color color,bool right);
float ui_value_width(UIContext *ui,const char *text,bool is_null,float size,float limit);
/* Bounded retained 90 ms CSS-ease color feedback; colors are fill/border/ink. */
void ui_feedback_colors(UIContext *ui,Rectangle rect,unsigned kind,const Color base[3],Color colors[3]);
bool ui_control_input(UIContext *ui,Rectangle rect,bool enabled,bool *hover);
bool ui_button(UIContext *ui, Rectangle r, const char *label, bool active, bool enabled);
bool ui_tool_button(UIContext *ui,Rectangle r,const char *label,bool active,bool enabled);
bool ui_popup_button(UIContext *ui,Rectangle r,const char *label,bool primary,bool text_action,bool enabled);
bool ui_popup_row(UIContext *ui,Rectangle r,const char *label,UICarbonIcon icon,bool enabled);
bool ui_popup_row_input(UIContext *ui,Rectangle r,const char *label,UICarbonIcon icon,bool enabled,bool pointer);
void ui_carbon(UIContext *ui,UICarbonIcon icon,Rectangle rect,Color color);
bool ui_icon_button(UIContext *ui,Rectangle r,UICarbonIcon icon,bool active,bool enabled);
bool ui_menu_item(UIContext *ui,Rectangle r,const char *label,UICarbonIcon icon,bool checked,bool enabled);
typedef struct { bool dragging; float grab; } UIScrollbar;
/* Content, viewport and offset share caller units; track/grab are display
   coordinates. Captures complete between-frame gestures without allocation. */
bool ui_scrollbar(UIContext *ui,Rectangle track,float content,float viewport,float *offset,bool horizontal,UIScrollbar *state,bool enabled);
/* Containers can update before arranging children, then paint the thumb once,
   so a drag moves both content and thumb during the same frame. */
bool ui_scrollbar_update(UIContext *ui,Rectangle track,float content,float viewport,float *offset,bool horizontal,UIScrollbar *state,bool enabled);
void ui_scrollbar_paint(UIContext *ui,Rectangle track,float content,float viewport,float offset,bool horizontal);
/* Dense navigation row: selection/hover surfaces, rather than a raised command. */
bool ui_list_row(UIContext *ui,Rectangle r,const char *label,bool selected,bool enabled);
typedef enum { ICON_OPEN, ICON_SAVE, ICON_ADD, ICON_UNDO, ICON_REDO, ICON_COPY, ICON_TABLE, ICON_SORT, ICON_SUM, ICON_CLEAR, ICON_CODE, ICON_SETTINGS } UIIcon;
void ui_icon(UIContext *ui,UIIcon icon,Vector2 origin,Color color);
bool ui_command(UIContext *ui, Rectangle r, UIIcon icon, const char *label, bool active, bool enabled);
bool ui_carbon_command(UIContext *ui,Rectangle r,UICarbonIcon icon,const char *label,bool active,bool enabled);
bool ui_shortcut(void);
bool ui_shift(void);
/* Preserve a queued press even when key-down and key-up share one event poll. */
bool ui_pressed(int key);
bool ui_key(int key);
void ui_note_key_modifiers(int key,unsigned modifiers); /* DX_MOD_* after ui_update */
/* Complete captured press, including its modifiers. Input adapters/tests use
   this when a press did not enter raylib's GetKeyPressed queue. */
void ui_note_key_press(int key,unsigned modifiers);
/* Ordered navigation, bounded to 128 presses per frame. Native capture replaces
   GLFW's shorter queue and generated repeats rather than duplicating them. */
bool ui_navigation_captured(void);
bool ui_take_navigation_key(DxKeyModifiers *event);
bool ui_key_shift(int key);
bool ui_key_shortcut(int key);
bool ui_key_alt(int key);
bool ui_key_control(int key);
void ui_note_pointer_gesture(DxPointerGesture gesture);
bool ui_has_pointer_gesture(void);
bool ui_take_pointer_gesture(DxPointerGesture *gesture);
bool ui_take_gesture_from(Rectangle area,DxPointerGesture *gesture);
bool ui_gesture_starts_in(Rectangle area); /* inspect ownership without consuming */
bool ui_take_left_gesture_from(Rectangle area,DxPointerGesture *gesture);
bool ui_take_click_from(Rectangle area); /* completed left click; release stays inside */
/* Discrete list rows retain fractional trackpad motion between frames. */
int ui_scroll_steps(double *remainder,double delta);
/* One focused text field per modal; owns UTF-8 and both history stacks.
   New edits are limited to 64 KiB; each stack retains at most 32 states / 1 MiB.
   Use input_free, rather than freeing text separately. */
typedef struct {
    char *text; size_t length,capacity,cursor,anchor,view_start;
    bool select_all;
} TextInputState;
typedef struct {
    char *text; size_t length,capacity,cursor,anchor,view_start;
    bool select_all,native_edit,dragging,multiline;
    uint64_t revision,active_frame;
    double last_click; float click_x; unsigned clicks;
    TextInputState undo[32],redo[32];
    size_t undo_count,redo_count;
    unsigned edit_kind; double edit_at;
} TextInput;
void input_free(TextInput *input);
void input_set(TextInput *input, const char *text);
void input_insert(TextInput *input, const char *text);
void input_key(TextInput *input,int key,unsigned modifiers);
bool input_history(TextInput *input,bool redo);
char *input_selection_text(const TextInput *input); /* NULL without a selection; caller frees. */
void input_update(TextInput *input);
void input_draw(UIContext *ui, TextInput *input, Rectangle box);
void input_draw_sized(UIContext *ui,TextInput *input,Rectangle box,float size,bool frame);
#endif
