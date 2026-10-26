#ifndef DUXCEL_UI_VALUE_TYPE_H
#define DUXCEL_UI_VALUE_TYPE_H
#include "../workbook/manual_value.h"
/* Metadata only. No raw/canonical text pointer, copied value or model owner. */
typedef struct { bool present; ManualValueKind kind; NumberType number; } UIValueType;
typedef enum { UI_VALUE_TYPE_TEXT,UI_VALUE_TYPE_NULL,UI_VALUE_TYPE_INTEGER,UI_VALUE_TYPE_DECIMAL,UI_VALUE_TYPE_CLEAR } UIValueTypeFamily;
typedef enum { UI_VALUE_TYPE_FAMILY,UI_VALUE_TYPE_SIGN,UI_VALUE_TYPE_BITS,UI_VALUE_TYPE_PRECISION,UI_VALUE_TYPE_SCALE,UI_VALUE_TYPE_APPLY,UI_VALUE_TYPE_CANCEL,UI_VALUE_TYPE_FOCUS_COUNT } UIValueTypeFocus;
typedef struct { UIValueType value; UIValueTypeFocus focus; bool active; } UIValueTypeChooser;
typedef enum { UI_VALUE_TYPE_APPLIED,UI_VALUE_TYPE_CANCELLED,UI_VALUE_TYPE_INVALID } UIValueTypeOutcome;
/* Only the view's present/kind/number scalars are inspected. Invalid metadata
   preserves the prior chooser; this is never raw-value validation/admission. */
bool ui_value_type_begin(ManualValueView view,UIValueTypeChooser *out);
bool ui_value_type_valid(UIValueType value);
bool ui_value_type_can_apply(const UIValueTypeChooser *chooser);
bool ui_value_type_family(UIValueType value,UIValueTypeFamily *out);
bool ui_value_type_select(UIValueTypeChooser *chooser,UIValueTypeFamily family);
bool ui_value_type_integer(UIValueTypeChooser *chooser,bool unsigned_value,unsigned bits);
bool ui_value_type_decimal(UIValueTypeChooser *chooser,unsigned precision,unsigned scale);
bool ui_value_type_focus_enabled(const UIValueTypeChooser *chooser,UIValueTypeFocus focus);
bool ui_value_type_tab(UIValueTypeChooser *chooser,bool reverse);
bool ui_value_type_adjust(UIValueTypeChooser *chooser,int direction);
/* Apply always emits the chosen metadata, even when its type is unchanged:
   only the caller can judge an edit's raw text/no-op. Cancel/invalid preserve
   out. Successful finish closes this chooser without touching any model. */
UIValueTypeOutcome ui_value_type_finish(UIValueTypeChooser *chooser,bool apply,UIValueType *out);
#endif
