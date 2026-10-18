#include "chartview.h"
#include "../transforms/chart_other.h"
#include <math.h>
#include <float.h>
#include <string.h>
void ui_chart_ticks(double low,double high,double ticks[5]) {
    double rough=(high-low)/4;
    if (isfinite(rough) && rough>0) {
        double digits=floor(log10(rough))+1,magnitude=pow(10,digits),ratio=digits==1 ? .1 : .05;
        for (unsigned correction=0;correction<64;correction++) {
            double step=(ceil(rough/magnitude/ratio)+correction)*ratio*magnitude;
            if (!isfinite(step) || step<=0) break;
            double below=ceil(-low/step),above=ceil(high/step),count=below+above+1;
            if (count>5) continue; if (count<5) { if (high>0) above+=5-count; else below+=5-count; }
            double first=-below*step,last=above*step;
            if (isfinite(first) && isfinite(last) && first<=low && last>=high) { for (size_t i=0;i<5;i++) ticks[i]=first+i*step; return; }
            break;
        }
    }
    for (size_t i=0;i<5;i++) ticks[i]=low*(1-i/4.0)+high*(i/4.0);
}
bool ui_chart_view(const Page *p,const PivotDefinition *definition,size_t measure,UIChartView *out) {
    memset(out,0,sizeof(*out)); size_t keys=definition->counts[PIVOT_ROWS],measures=definition->counts[PIVOT_VALUES];
    if (!measures || measures>DX_PIVOT_FIELDS || keys>DX_PIVOT_FIELDS || measure>=measures || !p->row_count || p->row_count>DX_PAGE_ROWS || p->column_count<=keys || (p->column_count-keys)%measures || !p->cells || !p->columns) return false;
    size_t categories=(p->column_count-keys)/measures; if (categories>DX_CHART_CATEGORIES) return false;
    for (size_t c=0;c<categories;c++) if (!p->columns[keys+c*measures+measure].numeric) return false;
    out->categories=categories; out->measure=measure; out->keys=keys; out->measures=measures; out->truncated=p->offset || p->total>p->row_count;
    /* The source aggregation and paint must select the same loaded tuples. */
    ChartOtherRank rank={0}; if (!chart_other_rank(p,definition,measure,&rank,NULL)) return false;
    size_t top=rank.top_count; memcpy(out->rows,rank.top_rows,sizeof(out->rows));
    const Page *derived=page_other_values(p,measure);
    if (derived) {
        const PageOther *other=p->other; size_t variant=other->measure_variant[measure];
        if (other->keys!=keys || other->measures!=measures || other->top_count!=top ||
            other->rank_measure[variant]!=rank.rank_measure || memcmp(other->top_rows[variant],rank.top_rows,sizeof(rank.top_rows))) derived=NULL;
    }
    out->other_values=derived;
    Aggregate aggregate=definition->fields[PIVOT_VALUES][measure].aggregate;
    bool combinable=aggregate==AGG_SUM || aggregate==AGG_COUNT || aggregate==AGG_MIN || aggregate==AGG_MAX;
    out->non_additive=!combinable && !derived && p->row_count>top;
    out->other=(combinable || derived) && p->row_count>top; out->series=top+(out->other ? 1 : 0);
    for (size_t r=0;r<p->row_count;r++) {
        size_t series=top; for (size_t i=0;i<top;i++) if (out->rows[i]==r) series=i;
        if (series==top && (!out->other || derived)) continue;
        for (size_t c=0;c<categories;c++) {
            const Cell *cell=&p->cells[r*p->column_count+keys+c*measures+measure]; if (cell->is_null) continue;
            if (!isfinite(cell->number)) { out->non_finite=true; continue; }
            double value=cell->number;
            if (series==top && out->present[series][c]) {
                if (aggregate==AGG_MIN) value=fmin(out->values[series][c],value);
                else if (aggregate==AGG_MAX) value=fmax(out->values[series][c],value);
                else value+=out->values[series][c];
            }
            if (!isfinite(value)) { out->non_finite=true; out->values[series][c]=NAN; out->present[series][c]=false; }
            else if (!isnan(out->values[series][c])) { out->values[series][c]=value; out->present[series][c]=true; }
        }
    }
    if (derived) for (size_t c=0;c<categories;c++) {
        const Cell *cell=&derived->cells[c*measures+measure]; if (cell->is_null) continue;
        if (!isfinite(cell->number)) { out->non_finite=true; continue; }
        out->values[top][c]=cell->number; out->present[top][c]=true;
    }
    for (size_t i=0;i<out->series;i++) { out->legend[i]=i;
        for (size_t c=0;c<categories;c++) if (out->present[i][c]) { out->low=fmin(out->low,out->values[i][c]); out->high=fmax(out->high,out->values[i][c]); }
    }
    /* The mock's legend orders row keys separately from ranked paint colors. */
    for (size_t i=1;i<top;i++) for (size_t j=i;j && out->rows[out->legend[j]]<out->rows[out->legend[j-1]];j--) { size_t a=out->legend[j]; out->legend[j]=out->legend[j-1]; out->legend[j-1]=a; }
    if (out->low==out->high) out->high=out->low+1;
    out->valid=true; return true;
}
