#ifndef DUXCEL_CHARTVIEW_H
#define DUXCEL_CHARTVIEW_H
#include "../data/database.h"
#define DX_CHART_CATEGORIES 128
/* Bounded presentation of the cached matrix: Columns are the X categories,
   three ranked Rows are the series, and compatible aggregates combine Other.
   No pages/strings are owned and no database work is performed here. */
typedef struct {
    double values[4][DX_CHART_CATEGORIES],low,high;
    bool present[4][DX_CHART_CATEGORIES];
    size_t rows[3],legend[4],categories,series,measure,keys,measures;
    const Page *other_values; /* Borrowed from this immutable parent Page. */
    bool valid,other,truncated,non_finite,non_additive;
} UIChartView;
bool ui_chart_view(const Page *page,const PivotDefinition *definition,size_t measure,UIChartView *out);
void ui_chart_ticks(double low,double high,double ticks[5]);
#endif
