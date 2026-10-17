#ifndef DX_CHART_ENCODING_H
#define DX_CHART_ENCODING_H
#include "chartview.h"
#include "../transforms/marks.h"
typedef enum { UI_BIND_NONE,UI_BIND_ROW,UI_BIND_COLUMN,UI_BIND_MEASURE } UIEncodingKind;
typedef struct { UIEncodingKind kind; size_t index; } UIEncodingBinding;
typedef struct { const char *raw; double number; bool available,is_null,numeric,approximate; } UIEncodingValue;
typedef struct {
    UIEncodingBinding bindings[DX_MARK_ROLES][DX_MARK_FIELDS];
    size_t counts[DX_MARK_ROLES];
    bool active[DX_MARK_ROLES],partial[DX_MARK_ROLES];
    uint64_t colors[4],shapes[4][DX_CHART_CATEGORIES];
    double sizes[4][DX_CHART_CATEGORIES],size_max;
    bool sized[4][DX_CHART_CATEGORIES];
} UIChartEncoding;
UIEncodingBinding ui_encoding_binding(const PivotDefinition *definition,const MarkField *field);
UIEncodingValue ui_encoding_value(const Page *page,const PivotDefinition *definition,const UIChartView *view,UIEncodingBinding binding,size_t series,size_t category);
bool ui_chart_encoding(const Page *page,const PivotDefinition *definition,const MarkDefinition *marks,const UIChartView *view,bool line,UIChartEncoding *out);
#endif
