#include "app/selection.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL selection:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static Grid seed(uint64_t row,size_t col) {
    Grid g={0}; g.row=g.anchor_row=g.end_row=row; g.col=g.anchor_col=g.end_col=col;
    g.selection=SELECT_CELLS; g.top=17; g.horizontal=22; g.zoom=1.25f; return g;
}
static SelectionOutcome expand(Grid *g,SelectionContext *context) {
    Grid before=*g; SelectionContext snapshot=*context;
    SelectionOutcome out=selection_expand(g,context);
    CHECK(!memcmp(&snapshot,context,sizeof(snapshot)));
    Grid after=*g; after.selection=before.selection;
    after.anchor_row=before.anchor_row; after.end_row=before.end_row;
    after.anchor_col=before.anchor_col; after.end_col=before.end_col;
    CHECK(!memcmp(&after,&before,sizeof(after)));
    return out;
}
int main(void) {
    CellStore cells={0}; char *error=NULL;
    CHECK(cells_set(&cells,1,1,"NULL",&error)); CHECK(cells_set(&cells,1,2,"",&error));
    CHECK(cells_set(&cells,2,1,"12345678901234567890",&error)); free(error);
    SelectionContext context={.cells=&cells,.used={1,20000,1,25,true}};
    Grid g=seed(1,1); SelectionOutcome out=expand(&g,&context);
    CHECK(g.anchor_row==1 && g.end_row==2 && g.anchor_col==1 && g.end_col==1);
    CHECK(!out.within_owner); expand(&g,&context);
    CHECK(g.end_row==20000 && g.end_col==25); expand(&g,&context); CHECK(g.selection==SELECT_ALL);
    expand(&g,&context); CHECK(g.selection==SELECT_ALL);
    CHECK(cells.count==2 && !strcmp(cells_get(&cells,1,1),"NULL") && !*cells_get(&cells,1,2));
    context.owner=(GridRegion){10,18,2,5,true}; context.body=(GridRegion){12,15,2,4,true};
    context.headers=(GridRegion){11,15,2,4,true}; context.used=(GridRegion){0,20000,0,25,true};
    g=seed(13,3); out=expand(&g,&context); CHECK(out.within_owner && !out.whole_owner);
    CHECK(g.anchor_row==12 && g.end_row==15); out=expand(&g,&context);
    CHECK(out.within_owner && !out.whole_owner && g.anchor_row==11);
    out=expand(&g,&context); CHECK(out.within_owner && out.whole_owner && g.anchor_row==10 && g.end_col==5);
    out=expand(&g,&context); CHECK(!out.within_owner && !out.whole_owner && g.end_row==20000);
    /* An unavailable/stale analytical result contributes its frame only. */
    context.body.active=context.headers.active=false; g=seed(13,3);
    out=expand(&g,&context); CHECK(out.whole_owner && g.end_row==18);
    /* A provisional primary result cannot manufacture a used rectangle. */
    context=(SelectionContext){.cells=&cells}; g=seed(10,10); expand(&g,&context); CHECK(g.selection==SELECT_ALL);
    context.primary=(GridRegion){0,99,0,4,true}; g=seed(20,3); expand(&g,&context); CHECK(g.end_row==99 && g.end_col==4);
    g=seed(100,5); expand(&g,&context); CHECK(g.selection==SELECT_ALL);
    g=seed(1,1); g.selection=SELECT_ROWS; expand(&g,&context); CHECK(g.selection==SELECT_ALL);
    g=seed(1,1); g.selection=SELECT_COLUMNS; expand(&g,&context); CHECK(g.selection==SELECT_ALL);
    CHECK(!selection_expand(NULL,&context).within_owner); CHECK(!selection_expand(&g,NULL).within_owner);
    cells_free(&cells); printf("PASS: %u selection controller checks\n",checks); return 0;
}
