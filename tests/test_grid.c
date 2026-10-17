#include "grid/grid.h"
#include "ui/textview.h"
#include "ui/ribbon.h"
#include "ui/ribbon_spec.h"
#include "ui/flyout.h"
#include "ui/dockview.h"
#include "ui/statusview.h"
#include "ui/workspace.h"
#include "platform/platform.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <float.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL grid:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void event(Grid *g,UIContext *u,Page *p,GridInput in) {
    if (!in.dt) in.dt=.016f;
    grid_handle_input(g,u,p,&in);
}
static void test_focus_modality(void) {
    DxPointerGesture gesture; DxKeyModifiers key;
    ui_note_pointer_gesture((DxPointerGesture){0}); CHECK(!ui_focus_visible());
    ui_note_key_press(KEY_TAB,0); CHECK(ui_focus_visible());
    ui_note_pointer_gesture((DxPointerGesture){0}); CHECK(!ui_focus_visible());
    ui_note_key_modifiers(KEY_DOWN,0); CHECK(ui_focus_visible());
    ui_note_pointer_gesture((DxPointerGesture){0}); CHECK(!ui_focus_visible());
    ui_note_key_modifiers(KEY_S,DX_MOD_SUPER); CHECK(!ui_focus_visible());
    ui_note_key_modifiers(-1,DX_MOD_ALT); ui_note_key_modifiers(512,DX_MOD_ALT); CHECK(!ui_focus_visible());
    ui_note_key_modifiers(KEY_H,DX_MOD_ALT); CHECK(ui_focus_visible());
    ui_note_pointer_gesture((DxPointerGesture){0}); CHECK(!ui_focus_visible());
    ui_note_key_press(KEY_ESCAPE,0); CHECK(ui_focus_visible());
    ui_note_pointer_gesture((DxPointerGesture){0}); CHECK(!ui_focus_visible());
    ui_note_key_modifiers(KEY_F6,DX_MOD_CONTROL); CHECK(ui_focus_visible());
    ui_note_pointer_gesture((DxPointerGesture){0}); CHECK(!ui_focus_visible());
    while (ui_take_pointer_gesture(&gesture)) { }
    while (ui_take_navigation_key(&key)) { }
}
static void test_active_range_cell(void) {
    Grid g; grid_init(&g); UIContext ui={.scale=1}; Column columns[12]={0}; char names[12][8];
    for (size_t i=0;i<12;i++) { snprintf(names[i],sizeof(names[i]),"F%zu",i); columns[i].name=names[i]; }
    Page p={.columns=columns,.column_count=12,.total=1000}; grid_schema(&g,&p,true); g.row_height=24;
    grid_layout(&g,1,&p,(Rectangle){0,0,900,600}); grid_select_cell(&g,5,5,false); grid_reveal_cell(&g,&p,1);
    Rectangle first=grid_range_rect(&g,1,5,5,1,1),last=grid_range_rect(&g,1,8,5,1,1);
    grid_replay_gesture(&g,&ui,&p,(DxPointerGesture){first.x+first.width*.5f,first.y+12,last.x+last.width*.5f,last.y+12,0,false});
    CHECK(g.row==5 && g.col==5 && g.anchor_row==5 && g.end_row==8 && g.end_col==5 && !g.dragging);
    event(&g,&ui,&p,(GridInput){.key=KEY_DOWN,.shift=true}); CHECK(g.row==5 && g.col==5 && g.end_row==9);
    event(&g,&ui,&p,(GridInput){.key=KEY_UP,.shift=true}); CHECK(g.row==5 && g.end_row==8);
    event(&g,&ui,&p,(GridInput){.key=KEY_RIGHT,.shift=true}); CHECK(g.col==5 && g.end_col==6);
    event(&g,&ui,&p,(GridInput){.key=KEY_HOME,.shift=true}); CHECK(g.row==5 && g.col==5 && !g.end_col);
    for (int i=0;i<4;i++) event(&g,&ui,&p,(GridInput){.key=KEY_UP,.shift=true});
    CHECK(g.row==5 && g.col==5 && g.end_row==4 && !g.end_col);
    event(&g,&ui,&p,(GridInput){.key=KEY_TAB}); CHECK(g.row==4 && !g.col && g.anchor_row==5 && g.end_row==4);
    event(&g,&ui,&p,(GridInput){.key=KEY_RIGHT,.shift=true}); CHECK(g.row==4 && g.col==5 && g.end_col==1);
    event(&g,&ui,&p,(GridInput){.key=KEY_DOWN,.shift=true}); CHECK(g.row==5 && g.col==5 && g.end_row==5);
    event(&g,&ui,&p,(GridInput){.key=KEY_DOWN}); CHECK(g.row==6 && g.col==5 && g.anchor_row==6 && g.end_row==6 && g.end_col==5);
    grid_select_cell(&g,2,2,false); grid_select_cell(&g,3,3,true);
    event(&g,&ui,&p,(GridInput){.key=KEY_TAB}); CHECK(g.row==2 && g.col==3 && g.end_row==3 && g.end_col==3);
    event(&g,&ui,&p,(GridInput){.key=KEY_ENTER}); CHECK(g.row==3 && g.col==3 && g.anchor_row==2);
    event(&g,&ui,&p,(GridInput){.key=KEY_TAB,.shift=true}); CHECK(g.row==3 && g.col==2 && g.end_col==3);
    event(&g,&ui,&p,(GridInput){.key=KEY_ENTER,.shift=true}); CHECK(g.row==2 && g.col==2 && g.end_row==3);
    grid_select_cell(&g,0,0,false); event(&g,&ui,&p,(GridInput){.key=KEY_END,.shift=true,.shortcut=true});
    CHECK(!g.row && !g.col && g.end_row==999 && g.end_col==11);
    CHECK(g.top>0 && g.top+g.visible_rows>999 && g.horizontal>0);
    event(&g,&ui,&p,(GridInput){.key=KEY_HOME,.shift=true,.shortcut=true}); CHECK(!g.row && !g.col && !g.end_row && !g.end_col && !g.top && !g.horizontal);
    grid_select_cell(&g,8,6,false); grid_reveal_cell(&g,&p,1);
    first=grid_range_rect(&g,1,8,6,1,1); last=grid_range_rect(&g,1,5,5,1,1);
    grid_replay_gesture(&g,&ui,&p,(DxPointerGesture){first.x+first.width*.5f,first.y+12,last.x+last.width*.5f,last.y+12,0,false});
    CHECK(g.row==8 && g.col==6 && g.anchor_row==8 && g.end_row==5 && g.anchor_col==6 && g.end_col==5);
    grid_select_cell(&g,5,5,false); g.selection=SELECT_COLUMNS;
    event(&g,&ui,&p,(GridInput){.key=KEY_RIGHT,.shift=true}); CHECK(g.row==5 && g.col==5 && g.end_col==6 && g.selection==SELECT_COLUMNS);
    grid_select_cell(&g,5,5,false); g.selection=SELECT_ROWS;
    event(&g,&ui,&p,(GridInput){.key=KEY_DOWN,.shift=true}); CHECK(g.row==5 && g.col==5 && g.end_row==6 && g.selection==SELECT_ROWS);
    grid_destroy(&g);
}
static void test_active_extent_canvas_growth(void) {
    for (int variant=0;variant<2;variant++) {
        float scale=variant ? 1.5f : 1;
        Grid g; grid_init(&g); UIContext ui={.scale=scale}; Page empty={0};
        grid_schema(&g,&empty,true); grid_layout(&g,scale,&empty,(Rectangle){0,0,900,600});
        grid_select_cell(&g,22,49,false); grid_select_cell(&g,22,51,true);
        float active_width=g.widths[49],extent_width=g.widths[51];
        event(&g,&ui,&empty,(GridInput){.key=KEY_RIGHT,.separate_keyboard=true,.key_shift=true,.key_alt=true});
        CHECK(g.width_count==52 && g.widths[49]==active_width+12 && g.widths[51]==extent_width);
        CHECK(g.row==22 && g.col==49 && g.end_col==51);
        /* The native key snapshot must extend the extent, even if Shift was
           released before the pointer snapshot for this rendered frame. */
        event(&g,&ui,&empty,(GridInput){.key=KEY_RIGHT,.separate_keyboard=true,.key_shift=true});
        CHECK(g.width_count==104 && g.row==22 && g.col==49 && g.anchor_col==49 && g.end_col==52);
        Rectangle extent=grid_range_rect(&g,scale,22,52,1,1);
        CHECK(extent.x>=g.body.x-.01f && extent.x+extent.width<=g.body.x+g.body.width+.01f);
        event(&g,&ui,&empty,(GridInput){.key=KEY_LEFT,.separate_keyboard=true,.key_shift=true});
        CHECK(g.col==49 && g.end_col==51 && g.width_count==104);

        grid_schema(&g,&empty,true); g.top=20; grid_layout(&g,scale,&empty,(Rectangle){0,0,900,600});
        g.horizontal=g.offsets[g.width_count]-g.body.width/scale; grid_layout(&g,scale,&empty,g.rect);
        Rectangle origin=grid_range_rect(&g,scale,22,49,1,1);
        Vector2 pointer={origin.x+origin.width*.5f,origin.y+origin.height*.5f};
        event(&g,&ui,&empty,(GridInput){.mouse=pointer,.pressed=true,.down=true});
        CHECK(g.dragging && g.row==22 && g.col==49);
        pointer.x=g.body.x+g.body.width+24;
        for (int frame=0;frame<12;frame++) event(&g,&ui,&empty,(GridInput){.mouse=pointer,.down=true,.dt=.05f});
        CHECK(g.width_count==104 && g.row==22 && g.col==49 && g.end_col>51 && g.end_row==22);
        CHECK(g.horizontal>g.offsets[52]-g.body.width/scale);
        event(&g,&ui,&empty,(GridInput){.mouse=pointer,.released=true});
        CHECK(!g.dragging && g.col==49 && g.end_col>51);
        event(&g,&ui,&empty,(GridInput){.key=KEY_TAB,.shift=true});
        CHECK(g.row==22 && g.col==g.end_col && g.anchor_col==49);
        size_t previous=g.col;
        event(&g,&ui,&empty,(GridInput){.key=KEY_RIGHT});
        CHECK(g.col==previous+1 && g.col==g.end_col && g.anchor_col==g.col && g.row==g.end_row);

        grid_schema(&g,&empty,true); grid_select_cell(&g,22,49,false); grid_select_cell(&g,22,51,true);
        event(&g,&ui,&empty,(GridInput){.key=KEY_RIGHT,.shift=true,.separate_keyboard=true});
        CHECK(g.width_count==52 && g.col==50 && g.anchor_col==50 && g.end_col==50);
        grid_select_cell(&g,22,49,false); grid_select_cell(&g,22,51,true);
        event(&g,&ui,&empty,(GridInput){.key=KEY_RIGHT,.shift=true,.shortcut=true});
        CHECK(g.width_count==52 && g.col==49 && !g.end_col);
        grid_destroy(&g);
    }
    /* Embedded result grids retain a finite schema under the same gesture. */
    Grid g; grid_init(&g); g.bounded=true; UIContext ui={.scale=1};
    Column columns[2]={{.name="A"},{.name="B"}}; Page p={.columns=columns,.column_count=2,.total=20};
    grid_schema(&g,&p,true); grid_layout(&g,1,&p,(Rectangle){0,0,250,300});
    grid_select_cell(&g,1,0,false); grid_select_cell(&g,1,1,true);
    event(&g,&ui,&p,(GridInput){.key=KEY_RIGHT,.shift=true});
    CHECK(g.width_count==2 && !g.col && g.end_col==1);
    grid_reveal_cell(&g,&p,1); Rectangle origin=grid_range_rect(&g,1,1,0,1,1);
    event(&g,&ui,&p,(GridInput){.mouse={origin.x+20,origin.y+12},.pressed=true,.down=true});
    for (int frame=0;frame<12;frame++) event(&g,&ui,&p,(GridInput){.mouse={g.body.x+g.body.width+24,origin.y+12},.down=true,.dt=.05f});
    CHECK(g.width_count==2 && !g.col && g.end_col==1);
    event(&g,&ui,&p,(GridInput){.released=true}); grid_destroy(&g);
}
static void test_active_header_ranges(void) {
    for (int variant=0;variant<2;variant++) {
        float scale=variant ? 1.5f : 1;
        Grid g; grid_init(&g); UIContext ui={.scale=scale}; Column columns[6]={0}; char names[6][8];
        for (size_t c=0;c<6;c++) { snprintf(names[c],sizeof(names[c]),"F%zu",c); columns[c].name=names[c]; }
        Page p={.columns=columns,.column_count=6,.total=10000}; grid_schema(&g,&p,true);
        for (size_t c=0;c<6;c++) grid_set_width(&g,c,64);
        grid_layout(&g,scale,&p,(Rectangle){0,0,900,600}); grid_select_cell(&g,3,1,false);
        float y=g.rect.y+10*scale;
        float x1=grid_column_x(&g,scale,1)+20*scale,x4=grid_column_x(&g,scale,4)+20*scale;
        grid_replay_gesture(&g,&ui,&p,(DxPointerGesture){x1,y,x4,y,0,false});
        CHECK(g.selection==SELECT_COLUMNS && g.row==3 && g.col==1 && g.anchor_col==1 && g.end_col==4);
        float x0=grid_column_x(&g,scale,0)+20*scale;
        grid_replay_gesture(&g,&ui,&p,(DxPointerGesture){x0,y,x0,y,DX_MOD_SHIFT,false});
        CHECK(g.selection==SELECT_COLUMNS && g.row==3 && g.col==1 && g.anchor_col==1 && !g.end_col);
        event(&g,&ui,&p,(GridInput){.key=KEY_RIGHT,.shift=true});
        CHECK(g.selection==SELECT_COLUMNS && g.col==1 && g.end_col==1);
        event(&g,&ui,&p,(GridInput){.key=KEY_RIGHT});
        CHECK(g.selection==SELECT_CELLS && g.row==3 && g.col==2 && g.anchor_col==2 && g.end_col==2);

        grid_select_cell(&g,3,1,false); g.top=0; grid_layout(&g,scale,&p,g.rect);
        Rectangle row3=grid_range_rect(&g,scale,3,1,1,1),row6=grid_range_rect(&g,scale,6,1,1,1);
        float gutter=g.rect.x+10*scale;
        grid_replay_gesture(&g,&ui,&p,(DxPointerGesture){gutter,row3.y+10*scale,gutter,row6.y+10*scale,0,false});
        CHECK(g.selection==SELECT_ROWS && g.row==3 && g.col==1 && g.anchor_row==3 && g.end_row==6);
        Rectangle row1=grid_range_rect(&g,scale,1,1,1,1);
        grid_replay_gesture(&g,&ui,&p,(DxPointerGesture){gutter,row1.y+10*scale,gutter,row1.y+10*scale,DX_MOD_SHIFT,false});
        CHECK(g.selection==SELECT_ROWS && g.row==3 && g.col==1 && g.anchor_row==3 && g.end_row==1);
        event(&g,&ui,&p,(GridInput){.key=KEY_DOWN,.shift=true});
        CHECK(g.row==3 && g.end_row==2 && g.selection==SELECT_ROWS);
        event(&g,&ui,&p,(GridInput){.key=KEY_UP});
        CHECK(g.selection==SELECT_CELLS && g.row==2 && g.col==1 && g.anchor_row==2 && g.end_row==2);

        grid_select_cell(&g,3,1,false); grid_reveal_cell(&g,&p,scale); row3=grid_range_rect(&g,scale,3,1,1,1);
        event(&g,&ui,&p,(GridInput){.mouse={row3.x+10*scale,row3.y+10*scale},.pressed=true,.down=true});
        for (int frame=0;frame<12;frame++) event(&g,&ui,&p,(GridInput){.mouse={row3.x+10*scale,g.body.y+g.body.height+24},.down=true,.dt=.05f});
        CHECK(g.row==3 && g.col==1 && g.anchor_row==3 && g.end_row>g.visible_rows && g.top>0);
        uint64_t end=g.end_row;
        event(&g,&ui,&p,(GridInput){.released=true});
        event(&g,&ui,&p,(GridInput){.key=KEY_UP,.shift=true});
        CHECK(g.row==3 && g.col==1 && g.end_row==end-1 && g.top+g.visible_rows>g.end_row);
        grid_destroy(&g);
    }
}
static void test_source_context(void) {
    test_active_range_cell();
    test_active_extent_canvas_growth();
    test_active_header_ranges();
    UICellContextLayout v=ui_cell_context_layout((Rectangle){228,137,772,28},1280,false,false,43);
    CHECK(v.valid && v.address.x==228 && v.address.y==139.5f && v.address.width==88 && v.address.height==22);
    CHECK(v.address_text.x==237 && v.chevron.x==295 && v.function.x==325 && v.function.y==142.5f);
    CHECK(v.value.x==350 && v.value.y==139.5f && v.value.width==581 && v.value.height==22);
    CHECK(v.type.x==940 && v.type.width==52);
    v=ui_cell_context_layout((Rectangle){0,137,427,28},427,true,false,43);
    CHECK(v.valid && v.address.x==36 && v.address.width==62 && v.address_text.x==42 && v.chevron.x==80);
    CHECK(v.function.x==103 && v.value.x==124 && v.value.width==295 && !v.type.width);
    v=ui_cell_context_layout((Rectangle){0,137,680,28},680,true,false,43);
    CHECK(v.type.width==52 && v.value.x==124 && v.value.width==491 && v.type.x==620);
    v=ui_cell_context_layout((Rectangle){0,0,1280,28},1280,false,true,100);
    CHECK(v.value.x==122 && v.type.x==1035 && v.type.width==100 && v.value.width==904);
    CHECK(!ui_cell_context_layout((Rectangle){0,0,NAN,28},1280,false,false,0).valid);
    CHECK(!ui_cell_context_layout((Rectangle){0,0,772,28},1280,false,false,INFINITY).valid);
    Grid g; grid_init(&g); grid_ensure_columns(&g,12);
    g.row=g.anchor_row=5; g.end_row=8; g.col=g.anchor_col=g.end_col=5; g.top=3; g.fraction=.25; g.horizontal=273.75f;
    g.zoom=1.3f; g.formats=(const FormatStore *)&g; g.dragging=true;
    GridLocation location=grid_location(&g); float *widths=g.widths,*offsets=g.offsets; char **names=g.column_names;
    grid_select_cell(&g,100,30,false); g.top=80; g.fraction=.9; g.horizontal=900;
    /* Ensure columns once before checking that restore retains owned arrays. */
    widths=g.widths; offsets=g.offsets; names=g.column_names;
    CHECK(grid_restore_location(&g,&location));
    CHECK(g.row==5 && g.col==5 && g.anchor_row==5 && g.end_row==8 && g.end_col==5);
    CHECK(g.top==3 && g.fraction==.25 && g.horizontal==273.75f && g.selection==SELECT_CELLS);
    CHECK(g.widths==widths && g.offsets==offsets && g.column_names==names && g.formats==(const FormatStore *)&g && g.zoom==1.3f);
    CHECK(!g.dragging && !g.moving && g.resizing==-1 && !g.scrollbar);
    GridLocation bad=location; bad.fraction=NAN; CHECK(!grid_restore_location(&g,&bad) && g.row==5 && g.horizontal==273.75f);
    bad=location; bad.end_col=16384; CHECK(!grid_restore_location(&g,&bad) && g.end_row==8);
    bad=location; bad.selection=(SelectionMode)-1; CHECK(!grid_restore_location(&g,&bad));
    bad=location; bad.row=DX_SHEET_ROWS; CHECK(!grid_restore_location(&g,&bad));
    bad=location; bad.horizontal=-1; CHECK(!grid_restore_location(&g,&bad));
    g.formats=NULL; grid_destroy(&g);
}
static void test_shelf_wrap(void) {
    test_source_context();
    UIContextTitleLayout title=ui_context_title_layout((Rectangle){228,137,772,26},false,125.2421875f,60.1875f,76.078125f);
    CHECK(title.valid && title.icon.x==239 && title.icon.y==141.5f && title.icon.width==16);
    CHECK(title.title.x==263 && title.title.width==125.2421875f && title.source.x==401.2421875f);
    CHECK(title.browse.x==886.921875f && title.browse.width==102.078125f && title.browse.y==139.25f);
    title=ui_context_title_layout((Rectangle){0,137,427,26},true,125.2421875f,60.1875f,76.078125f);
    CHECK(title.valid && title.icon.x==39 && title.title.x==63 && title.title.width==242.921875f && !title.source.width);
    CHECK(title.browse.x==313.921875f && title.browse.x+title.browse.width==416);
    CHECK(!ui_context_title_layout((Rectangle){0,0,NAN,26},true,1,1,1).valid);
    CHECK(!ui_context_title_layout((Rectangle){0,0,427,26},true,1,1,INFINITY).valid);
    const float date[]={109.515625f,131.15625f,121.0546875f};
    UIShelfLayout l=ui_shelf_layout((Rectangle){0,163,427,0},date,3,true,false);
    CHECK(l.valid && l.panel.height==58 && l.fields.x==84 && l.fields.width==335 && l.fields.height==51);
    CHECK(l.tokens[0].y==166 && l.tokens[1].y==166 && l.tokens[2].y==194);
    CHECK(l.add.y==194 && l.add.x==210.0546875f);
    CHECK(ui_shelf_insertion(l.tokens,3,(Vector2){200,170})==1);
    CHECK(ui_shelf_insertion(l.tokens,3,(Vector2){90,200})==2);
    CHECK(ui_shelf_insertion(l.tokens,3,(Vector2){400,200})==3);
    CHECK(ui_shelf_insertion(l.tokens,3,(Vector2){400,160})==0);
    CHECK(ui_shelf_neighbor(l.tokens,3,0,1)==2);
    CHECK(ui_shelf_neighbor(l.tokens,3,1,1)==2);
    CHECK(ui_shelf_neighbor(l.tokens,3,2,-1)==0);
    CHECK(ui_shelf_neighbor(l.tokens,3,2,1)==2);
    CHECK(ui_shelf_neighbor(NULL,3,2,1)==2);
    const float row[]={92.5078125f,96.1015625f};
    l=ui_shelf_layout((Rectangle){0,221,427,0},row,2,true,true);
    CHECK(l.valid && l.panel.height==30 && l.tokens[0].y==224.5f && l.add.y==224.5f);
    for (int width=240;width<=1920;width+=35) for (size_t count=0;count<=8;count++) {
        float widths[8]; for (size_t i=0;i<8;i++) widths[i]=80+17*i;
        l=ui_shelf_layout((Rectangle){0,137,width,0},widths,count,width<700,false);
        CHECK(l.valid && l.count==count && l.panel.height>=31);
        CHECK(l.add.y+l.add.height<=l.panel.y+l.panel.height-3);
        for (size_t i=0;i<count;i++) {
            CHECK(l.tokens[i].x>=l.fields.x && l.tokens[i].x+l.tokens[i].width<=l.fields.x+fmaxf(l.fields.width,210)+.001f);
            CHECK(l.tokens[i].y+l.tokens[i].height<=l.panel.y+l.panel.height-3);
            if (i) CHECK(l.tokens[i].y>l.tokens[i-1].y || l.tokens[i].x>=l.tokens[i-1].x+l.tokens[i-1].width+5);
        }
    }
    UIWorkspaceLayout w=ui_workspace_layout(427,640,137,228,280,true,false,true,false);
    ui_workspace_context_height(&w,115);
    CHECK(w.context.height==115 && w.canvas.y==252 && w.canvas.y+w.canvas.height==618);
    UIAuthoringLayout a=ui_authoring_layout(w.canvas,0,115);
    CHECK(a.valid && fabsf(a.preview.height-(640-22-137)*.4f)<.001f);
    CHECK(!ui_shelf_layout((Rectangle){0,0,NAN,0},date,3,true,false).valid);
    CHECK(!ui_shelf_layout((Rectangle){0,0,427,0},date,9,true,false).valid);
}
static void test_workspace_layout(void) {
    test_shelf_wrap();
    UIMenuLayout action=ui_action_menu_layout((Vector2){10,834},11,240,1,(Vector2){1440,988});
    CHECK(action.panel.x==10 && action.panel.y==602 && action.panel.width==240 && action.panel.height==348);
    CHECK(action.items.x==15 && action.items.y==637 && action.items.width==230 && action.visible==11);
    action=ui_action_menu_layout((Vector2){630,630},16384,240,1.5f,(Vector2){640,640});
    CHECK(action.visible>0 && action.panel.x>=0 && action.panel.y>=0 && action.panel.x+action.panel.width<=640 && action.panel.y+action.panel.height<=640);
    CHECK(!ui_action_menu_layout((Vector2){NAN,0},11,240,1,(Vector2){1440,988}).visible);
    CHECK(!ui_action_menu_layout((Vector2){0,0},0,240,1,(Vector2){1440,988}).visible);
    UIFilterLayout filter=ui_filter_layout((Vector2){500,294},(Vector2){1440,988},1,0,false);
    CHECK(filter.valid && filter.panel.width==288 && filter.panel.height==512.5f && filter.panel.x==500 && filter.panel.y==294);
    CHECK(filter.title.height==36 && filter.search.height==28 && filter.list.height==230 && filter.condition.height==25 && filter.footer.height==44);
    CHECK(filter.search.y-filter.panel.y==115 && filter.all.y-filter.panel.y==148 && filter.list.y-filter.panel.y==175 && filter.scope.y-filter.panel.y==445);
    for (int terms=0;terms<3;terms++) for (int dates=0;dates<2;dates++) {
        filter=ui_filter_layout((Vector2){620,620},(Vector2){640,640},1.5f,terms,dates);
        CHECK(filter.valid && filter.panel.x>=0 && filter.panel.y>=0 && filter.panel.x+filter.panel.width<=640 && filter.panel.y+filter.panel.height<=640);
        CHECK(filter.list.height>=25*1.5f && filter.apply.x>=filter.clear.x+filter.clear.width && filter.cancel.x+filter.cancel.width<filter.apply.x);
        CHECK(filter.scope.y>=filter.condition.y+filter.condition.height && filter.footer.y>=filter.scope.y+filter.scope.height);
    }
    CHECK(!ui_filter_layout((Vector2){NAN,0},(Vector2){640,640},1,0,false).valid);
    CHECK(!ui_filter_layout((Vector2){0,0},(Vector2){640,640},0,0,false).valid);
    CHECK(ui_matrix_column_width(0,2)==110 && ui_matrix_column_width(2,2)==108);
    CHECK(ui_matrix_column_left(2,2)==220 && ui_matrix_column_left(10,2)==1084);
    const float positions[]={-1,0,109.9f,110,219.9f,220,327.9f,328,1e20f,NAN,INFINITY};
    const size_t hit[]={0,0,0,1,1,2,2,3,9,0,0};
    for (size_t i=0;i<11;i++) CHECK(ui_matrix_column_at(positions[i],10,2)==hit[i]);
    CHECK(ui_matrix_column_at(999,0,0)==0 && ui_matrix_column_at(110,2,8)==1 && ui_matrix_column_left(SIZE_MAX,SIZE_MAX)>0);
    const float widths[]={426.6667f,640,699,700,1020,1040,1280,1920};
    for (size_t i=0;i<sizeof(widths)/sizeof(*widths);i++) for (int author=0;author<2;author++) for (int drawer=0;drawer<2;drawer++) {
        float width=widths[i]; UIWorkspaceLayout w=ui_workspace_layout(width,480,108,236,300,true,drawer,author,false);
        CHECK(w.valid && w.narrow==(width<700) && w.right_overlay==(width<1040));
        CHECK(w.left_overlay==(width<700 && drawer));
        CHECK(w.canvas.x==(width<700 ? 0 : 236) && w.canvas.y==(author ? 196 : 136));
        CHECK(w.canvas.y+w.canvas.height==458 && w.canvas.width>0);
        CHECK(w.canvas.x+w.canvas.width==(width<1040 ? width : width-300));
        CHECK(w.left.width==(width>=700 || drawer ? 236 : 0));
        CHECK(w.inspector.x+w.inspector.width==width && w.inspector.y==108);
    }
    UIWorkspaceLayout hidden=ui_workspace_layout(640,480,66,236,0,false,true,false,false);
    CHECK(hidden.valid && !hidden.left_overlay && !hidden.left.width && hidden.canvas.width==640);
    UIWorkspaceLayout focus=ui_workspace_layout(640,480,0,236,300,true,true,true,true);
    CHECK(focus.valid && !focus.left.width && !focus.inspector.width && focus.canvas.y==28 && focus.canvas.width==640);
    CHECK(!ui_workspace_layout(NAN,480,108,236,300,true,false,true,false).valid);
    CHECK(!ui_workspace_layout(640,480,INFINITY,236,300,true,false,true,false).valid);
    CHECK(!ui_workspace_layout(640,240,200,236,300,true,false,true,false).valid);
    const float author_widths[]={416,640,880,881,1024,1212};
    for (size_t i=0;i<6;i++) for (int h=320;h<=1024;h+=352) {
        Rectangle canvas={228,224,author_widths[i],h};
        UIAuthoringLayout wide=ui_authoring_layout(canvas,0,88),bottom=ui_authoring_layout(canvas,1e6f,88);
        CHECK(wide.valid && wide.stacked==(wide.content_width<=880));
        CHECK(wide.content_width==canvas.width-(wide.scrollable ? 9 : 0));
        CHECK(wide.preview.x+wide.preview.width==canvas.x+wide.content_width);
        CHECK(wide.label.height==21 && wide.preview.height>=150 && wide.preview.height<=336);
        CHECK(wide.matrix.height==318 && wide.chart.height==318);
        CHECK(wide.matrix.x>=canvas.x && wide.chart.x+wide.chart.width<=canvas.x+canvas.width);
        CHECK(wide.stacked ? wide.chart.y>wide.matrix.y+wide.matrix.height : wide.chart.x>wide.matrix.x+wide.matrix.width);
        CHECK(bottom.chart.y+bottom.chart.height<=canvas.y+canvas.height);
    }
    CHECK(!ui_authoring_layout((Rectangle){0,0,NAN,800},0,88).valid);
    UIAuthoringLayout lane=ui_authoring_layout((Rectangle){228,225,772,573},0,88);
    CHECK(lane.scrollable && lane.content_width==763 && lane.preview.width==763 && lane.matrix.width==739);
    CHECK(lane.matrix.x+lane.matrix.width==979);
    lane=ui_authoring_layout((Rectangle){0,0,881,600},0,88);
    CHECK(lane.scrollable && lane.stacked && lane.content_width==872);
    lane=ui_authoring_layout((Rectangle){0,0,881,1100},0,88);
    CHECK(!lane.scrollable && !lane.stacked && lane.matrix.width==480 && lane.chart.width>=340);
}
static void test_dock_scroll_and_clips(void) {
    const float scales[]={.8f,1,1.5f,2.5f},heights[]={235,480,830}; const size_t sheets[]={1,4,100};
    for (int k=0;k<4;k++) for (int h=0;h<3;h++) for (int n=0;n<3;n++) for (int author=0;author<2;author++) for (int fields=0;fields<2;fields++) {
        float scale=scales[k]; Rectangle viewport={0,137*scale,228*scale,heights[h]*scale};
        UIDockLayout l=ui_dock_layout(viewport,scale,sheets[n],true,false,fields,false,author,296,10000);
        CHECK(l.valid); CHECK(l.compact==(heights[h]<=700)); CHECK(l.maximum>=0 && l.offset==l.maximum);
        CHECK(l.content.height>=viewport.height); CHECK(fabsf(l.content.y+l.offset*scale-viewport.y)<.01f);
        CHECK(l.content.width==viewport.width-(l.maximum>0 ? 9*scale : 0)); CHECK(fabsf(l.maximum-(l.content.height-viewport.height)/scale)<.001f);
        UIDockLayout start=ui_dock_layout(viewport,scale,sheets[n],true,false,fields,false,author,296,0);
        CHECK(start.offset==0); CHECK(ui_dock_reveal(start,(Rectangle){0,viewport.y,10,24*scale},scale)==0);
        float next=ui_dock_reveal(start,(Rectangle){0,start.content.y+start.content.height-24*scale,10,24*scale},scale);
        CHECK(fabsf(next-start.maximum)<.01f);
    }
    UIDockLayout compact=ui_dock_layout((Rectangle){0,0,228,235},1,4,true,false,true,false,true,296,0);
    CHECK(compact.content.height==555 && compact.maximum==320);
    UIDockLayout source=ui_dock_layout((Rectangle){0,0,228,235},1,4,true,false,true,false,false,0,0);
    CHECK(source.content.height==310 && source.maximum==75);
    for (int i=0;i<=100;i++) {
        float fraction=i/100.0f;
        UIDockLayout animated=ui_dock_layout_fraction((Rectangle){0,0,228,235},1,4,true,fraction,true,false,false,0,1000);
        CHECK(animated.valid && fabsf(animated.content.height-fmaxf(235,196+114*fraction))<.001f);
        CHECK(fabsf(animated.maximum-fmaxf(0,114*fraction-39))<.001f && animated.offset==animated.maximum);
    }
    CHECK(!ui_dock_layout_fraction((Rectangle){0,0,228,235},1,4,true,NAN,true,false,false,0,0).valid);
    CHECK(!ui_dock_layout_fraction((Rectangle){0,0,228,235},1,4,true,1.1f,true,false,false,0,0).valid);
    CHECK(!ui_dock_layout((Rectangle){0,0,228,235},0,4,true,false,true,false,true,296,0).valid);
    CHECK(!ui_dock_layout((Rectangle){0,NAN,228,235},1,4,true,false,true,false,true,296,0).valid);
    UIContext *ui=calloc(1,sizeof(*ui)); CHECK(ui);
    ui->clip=(Rectangle){10,10,200,100}; ui->clipping=true;
    CHECK(ui_scissor_begin(ui,(Rectangle){0,0,100,100})); CHECK(ui->scissor_depth==1 && ui->clip.x==10 && ui->clip.width==90 && ui->clip.height==90);
    ui_set_clip(ui,(Rectangle){0,0,30,30}); CHECK(ui->clip.x==10 && ui->clip.y==10 && ui->clip.width==20 && ui->clip.height==20);
    ui_clear_clip(ui); CHECK(ui->clipping && ui->clip.width==90);
    CHECK(ui_scissor_begin(ui,(Rectangle){50,0,150,50})); CHECK(ui->clip.x==50 && ui->clip.y==10 && ui->clip.width==50 && ui->clip.height==40);
    ui_clear_clip(ui); CHECK(ui->clip.x==50 && ui->clip.width==50);
    ui_scissor_end(ui); CHECK(ui->scissor_depth==1 && ui->clipping && ui->clip.x==10 && ui->clip.width==90);
    ui_scissor_end(ui); CHECK(!ui->scissor_depth && ui->clipping && ui->clip.width==200 && ui->clip.height==100);
    ui_clear_clip(ui); CHECK(!ui->clipping);
    for (int i=0;i<8;i++) CHECK(ui_scissor_begin(ui,(Rectangle){i,i,100-i,100-i}));
    CHECK(!ui_scissor_begin(ui,(Rectangle){0,0,10,10}) && ui->scissor_depth==8);
    for (int i=0;i<8;i++) ui_scissor_end(ui);
    CHECK(!ui->clipping && !ui->scissor_depth); ui_scissor_end(ui); CHECK(!ui->scissor_depth);
    CHECK(!ui_scissor_begin(ui,(Rectangle){NAN,0,10,10})); free(ui);
}
static void test_animated_workspace_extent(void) {
    UIWorkbookList compact=ui_workbook_list(84,4,true),full=ui_workbook_list(115,4,false);
    CHECK(compact.valid && compact.visible==4 && compact.height==84 && compact.row_height==20.25f && compact.stride==21.25f);
    CHECK(full.valid && full.visible==4 && full.height==115 && full.row_height==28 && full.stride==29);
    CHECK(ui_workbook_list(84,100,true).visible==4 && ui_workbook_list(84,100,true).row_height==20);
    CHECK(!ui_workbook_list(NAN,4,true).valid && !ui_workbook_list(1e30f,4,true).valid);
    const size_t counts[]={0,1,2,3,4,5,10,100,16384,SIZE_MAX}; const float heights[]={28,84,115,124,200};
    for (int n=0;n<10;n++) for (int compact_mode=0;compact_mode<2;compact_mode++) for (int h=0;h<5;h++) {
        size_t count=counts[n]; float height=heights[h]; UIWorkbookList list=ui_workbook_list(height,count,compact_mode);
        CHECK(list.valid && list.visible<=count && list.height<=height && list.height>=0 && list.row_height>=20 && list.row_height<=28);
        CHECK(list.stride==list.row_height+1 && (count || !list.visible));
    }
    for (int i=0;i<=228;i++) {
        UIWorkspaceLayout l=ui_workspace_layout(1440,988,137,(float)i,280,true,false,false,false);
        CHECK(l.valid && l.left.width==i && l.canvas.x==i && l.context.x==i);
        CHECK(l.canvas.width==1160-i && l.canvas.x+l.canvas.width==1160);
    }
}
static void test_ribbon_flyouts(void) {
    const float scales[]={.8f,1,1.5f,2.5f}; const Vector2 viewports[]={{1440,988},{640,640}};
    for (int tab=0;tab<RIBBON_TABS;tab++) for (int group=0;group<ribbon_group_counts[tab];group++) for (int k=0;k<4;k++) for (int v=0;v<2;v++) {
        float labels[UI_RIBBON_COMMANDS]={0}; for (int c=0;c<ribbon_counts[tab];c++) labels[c]=strlen(ribbon_commands[tab][c].label)*6;
        Vector2 viewport=viewports[v]; float scale=scales[k];
        UIFlyoutLayout layout=ui_flyout_layout((Vector2){viewport.x,viewport.y},viewport,scale,tab,group,labels);
        CHECK(layout.valid); CHECK(layout.panel.width==fminf(280,viewport.x/scale-12)*scale);
        CHECK(layout.panel.x>=0 && layout.panel.y>=0); CHECK(layout.panel.x+layout.panel.width<=viewport.x+.01f); CHECK(layout.panel.y+layout.panel.height<=viewport.y+.01f);
        CHECK(layout.close.width==23*scale && layout.close.height==30*scale); CHECK(layout.title.height==36*scale);
        const UIRibbonGroup *spec=&ribbon_groups[tab][group];
        for (int c=spec->first;c<spec->first+spec->count;c++) {
            Rectangle r=layout.commands[c]; CHECK(r.width>0 && r.height>0); CHECK(r.x>=layout.body.x && r.x+r.width<=layout.body.x+layout.body.width+.01f); CHECK(r.y>=layout.body.y);
            for (int previous=spec->first;previous<c;previous++) { Rectangle p=layout.commands[previous]; if (p.y<r.y+r.height-.01f && r.y<p.y+p.height-.01f) { if (!(p.x+p.width<=r.x+.01f || r.x+r.width<=p.x+.01f)) fprintf(stderr,"flyout overlap tab=%d group=%d scale=%f controls=%d,%d\n",tab,group,scale,previous,c); CHECK(p.x+p.width<=r.x+.01f || r.x+r.width<=p.x+.01f); } }
        }
        if (tab==RIBBON_HOME && group==1) { CHECK(layout.content_height==125); CHECK(layout.commands[4].width==53*scale); CHECK(layout.commands[3].width==((layout.panel.width/scale)-84)*scale); }
        if (tab==RIBBON_HOME && group==2) CHECK(layout.content_height==102);
        if (tab==RIBBON_HOME && group==3) CHECK(layout.content_height==125);
    }
    float labels[UI_RIBBON_COMMANDS]={0};
    CHECK(!ui_flyout_layout((Vector2){0,0},(Vector2){1440,988},0,0,0,labels).valid);
    CHECK(!ui_flyout_layout((Vector2){0,0},(Vector2){1440,988},1,RIBBON_TABS,0,labels).valid);
    CHECK(!ui_flyout_layout((Vector2){0,0},(Vector2){1440,988},1,0,6,labels).valid);
    labels[0]=NAN; CHECK(!ui_flyout_layout((Vector2){0,0},(Vector2){1440,988},1,0,0,labels).valid);
}
static void test_ribbon_reference_collapse(void) {
    /* Measured executable reference: 640 physical / 150% UI has clientWidth
       427. The six rounded collapsed buttons occupy 398 with JS's +8 slots. */
    float labels[UI_RIBBON_COMMANDS]={0};
    float names[UI_RIBBON_GROUPS]={50,23,52.901f,41.906f,25,42.828f};
    const float buttons[]={64,53,66.901f,55.906f,53,56.828f};
    for (int c=0;c<ribbon_counts[RIBBON_HOME];c++) labels[c]=strlen(ribbon_commands[RIBBON_HOME][c].label)*6;
    const float widths[]={640/1.5f,427};
    for (size_t k=0;k<sizeof(widths)/sizeof(*widths);k++) {
        UIRibbonLayout l=ui_ribbon_spec_layout(widths[k],0,RIBBON_HOME,labels,names);
        CHECK(l.valid && l.overflow_count==1 && l.states[0]==UI_RIBBON_OVERFLOW);
        CHECK(l.overflow.x==widths[k]-34 && l.overflow.width==28 && l.overflow.y==27 && l.overflow.height==28);
        float x=10;
        for (int g=1;g<6;g++) {
            Rectangle r=l.groups[g]; CHECK(l.states[g]==UI_RIBBON_COLLAPSED);
            CHECK(fabsf(r.x-x)<.001f && fabsf(r.width-buttons[g])<.001f && r.y==10 && r.height==62);
            CHECK(r.x+r.width<l.overflow.x && !l.launchers[g].width);
            for (int c=ribbon_groups[0][g].first;c<ribbon_groups[0][g].first+ribbon_groups[0][g].count;c++) CHECK(!l.commands[c].width);
            x+=buttons[g]+9; /* 4px side padding plus actual 1px slot border. */
        }
        CHECK(fabsf(l.groups[2].x-72)<.001f); /* Alignment remains directly after Font. */
    }
    UIRibbonLayout all=ui_ribbon_spec_layout(448,0,RIBBON_HOME,labels,names);
    CHECK(all.valid && !all.overflow_count); for (int g=0;g<6;g++) CHECK(all.states[g]==UI_RIBBON_COLLAPSED);
    /* History is reachable from More below 700, including Icons with no
       hidden groups. Wide layouts use their separate history dock instead. */
    for (int mode=0;mode<3;mode++) {
        UIRibbonLayout narrow=ui_ribbon_spec_layout(699,mode,RIBBON_HOME,labels,names);
        CHECK(narrow.valid && narrow.overflow.width==28 && narrow.overflow.x==665-(mode==0 ? 0 : 32));
        if (mode) CHECK(narrow.overflow.x+narrow.overflow.width<=699-32-6);
        UIRibbonLayout wide=ui_ribbon_spec_layout(1440,mode,RIBBON_HOME,labels,names);
        CHECK(wide.valid && !wide.overflow_count && !wide.overflow.width);
    }
    UIRibbonLayout icons_layout=ui_ribbon_spec_layout(640/1.5f,2,RIBBON_HOME,labels,names);
    CHECK(icons_layout.valid && !icons_layout.overflow_count && icons_layout.overflow.width==28);
    UIRibbonLayout edge=ui_ribbon_spec_layout(447.99f,0,RIBBON_HOME,labels,names);
    CHECK(edge.valid && edge.overflow_count==1 && edge.states[0]==UI_RIBBON_OVERFLOW && edge.states[2]==UI_RIBBON_COLLAPSED);
    edge=ui_ribbon_spec_layout(376,0,RIBBON_HOME,labels,names); CHECK(edge.valid && edge.overflow_count==1);
    edge=ui_ribbon_spec_layout(375.99f,0,RIBBON_HOME,labels,names); CHECK(edge.valid && edge.overflow_count==2 && edge.states[2]==UI_RIBBON_OVERFLOW);
    /* The reserved dock is independent of whether overflow becomes necessary;
       crossing that state must not charge for the same control a second time. */
    UIRibbonGroup group={"Analysis",0,1,10,false}; float full[UI_RIBBON_GROUPS]={100}; Rectangle cells[UI_RIBBON_COMMANDS]={{0,0,56,28}};
    float one_name[UI_RIBBON_GROUPS]={43};
    UIRibbonLayout fit=ui_ribbon_layout_docked(115,0,&group,1,labels,one_name,full,cells,38);
    CHECK(fit.valid && !fit.overflow_count && fit.states[0]==UI_RIBBON_COLLAPSED && fit.groups[0].width==57);
    UIRibbonLayout hidden=ui_ribbon_layout_docked(114.99f,0,&group,1,labels,one_name,full,cells,38);
    CHECK(hidden.valid && hidden.overflow_count==1 && hidden.states[0]==UI_RIBBON_OVERFLOW && !hidden.groups[0].width);
    CHECK(ui_ribbon_layout_custom(115,0,&group,1,labels,one_name,full,cells).states[0]==UI_RIBBON_EXPANDED);
    CHECK(!ui_ribbon_layout_docked(115,0,&group,1,labels,one_name,full,cells,NAN).valid);
    CHECK(!ui_ribbon_layout_docked(115,0,&group,1,labels,one_name,full,cells,20).valid);
    CHECK(!ui_ribbon_layout_docked(115,0,&group,1,labels,one_name,full,cells,104).valid);
    const UICarbonIcon icons[RIBBON_TABS][UI_RIBBON_GROUPS]={
        {CARBON_COPY,CARBON_TEXT_BOLD,CARBON_TEXT_ALIGN_LEFT,CARBON_CURRENCY_DOLLAR,CARBON_FILTER,CARBON_DATA_VIEW},
        {CARBON_DATA_VIEW,CARBON_TABLE}, {CARBON_SORT_ASCENDING,CARBON_FILTER,CARBON_DATA_STRUCTURED},
        {CARBON_CHART_BAR,CARBON_ROW,CARBON_SQL,CARBON_TABLE},
        {CARBON_DATA_STRUCTURED,CARBON_SETTINGS_ADJUST,CARBON_GRID,CARBON_KEYBOARD},
        {CARBON_CHART_BAR,CARBON_ROW,CARBON_SQL,CARBON_TABLE}
    };
    for (int tab=0;tab<RIBBON_TABS;tab++) {
        for (int g=0;g<ribbon_group_counts[tab];g++) CHECK(ui_ribbon_group_icon(tab,g)==icons[tab][g]);
        CHECK(ui_ribbon_group_icon(tab,-1)==CARBON_COUNT && ui_ribbon_group_icon(tab,ribbon_group_counts[tab])==CARBON_COUNT);
    }
    CHECK(ui_ribbon_group_icon(-1,0)==CARBON_COUNT && ui_ribbon_group_icon(RIBBON_TABS,0)==CARBON_COUNT);
}
static void test_ribbon_spec(void) {
    test_ribbon_reference_collapse();
    const float widths[]={240,376,416,640/1.5f,448,640,699.9f,700,900,1280,1440,1920};
    for (int tab=0;tab<RIBBON_TABS;tab++) for (int mode=0;mode<4;mode++) for (size_t w=0;w<sizeof(widths)/sizeof(*widths);w++) {
        float labels[UI_RIBBON_COMMANDS]={0},names[UI_RIBBON_GROUPS]={0};
        for (int c=0;c<ribbon_counts[tab];c++) labels[c]=strlen(ribbon_commands[tab][c].label)*6;
        for (int g=0;g<ribbon_group_counts[tab];g++) names[g]=strlen(ribbon_groups[tab][g].name)*6;
        UIRibbonLayout layout=ui_ribbon_spec_layout(widths[w],mode,tab,labels,names);
        CHECK(layout.valid && layout.height==ui_ribbon_height(mode));
        if (mode==3) continue;
        for (int g=0;g<layout.group_count;g++) {
            Rectangle group=layout.groups[g];
            if (layout.states[g]==UI_RIBBON_OVERFLOW) { CHECK(!group.width); continue; }
            CHECK(group.x>=0 && group.x+group.width<widths[w]-(widths[w]>=700 ? 62 : 0));
            CHECK(group.x+group.width<widths[w]-(widths[w]>=700 ? 98 : 38)-(mode==0 ? 0 : 32));
            if (mode) CHECK(group.x+group.width<widths[w]-32);
            if (layout.overflow_count) CHECK(group.x+group.width<layout.overflow.x);
            for (int before=0;before<g;before++) if (layout.groups[before].width) CHECK(layout.groups[before].x+layout.groups[before].width<=group.x);
            if (layout.states[g]!=UI_RIBBON_EXPANDED) continue;
            for (int c=ribbon_groups[tab][g].first;c<ribbon_groups[tab][g].first+ribbon_groups[tab][g].count;c++) {
                Rectangle cell=layout.commands[c];
                if (!ui_ribbon_command_visible(tab,mode,c)) { CHECK(!cell.width); continue; }
                CHECK(cell.width>=18 && cell.height>=18 && cell.x>=group.x && cell.x+cell.width<=group.x+group.width+.01f);
                CHECK(ui_ribbon_action_index(tab,ribbon_commands[tab][c].action)==c);
                if (tab==RIBBON_HOME && mode==0 && c>=17 && c<=20) CHECK(cell.width>=labels[c]+39);
                for (int other=ribbon_groups[tab][g].first;other<c;other++) {
                    Rectangle previous=layout.commands[other];
                    if (previous.width && previous.height && previous.y<cell.y+cell.height && cell.y<previous.y+previous.height) CHECK(previous.x+previous.width<=cell.x+.01f || cell.x+cell.width<=previous.x+.01f);
                }
            }
        }
        if (mode && layout.overflow.width) CHECK(layout.overflow.x+layout.overflow.width<=widths[w]-32-6+.001f);
    }
    CHECK(ui_ribbon_action_index(RIBBON_HOME,ACTION_CREATE_VIEW)==21);
    CHECK(ui_ribbon_action_index(RIBBON_DATA,ACTION_FILTER)==3);
    CHECK(ui_ribbon_action_index(RIBBON_VIEW,ACTION_DENSITY)==5);
    CHECK(ui_ribbon_action_index(-1,ACTION_COPY)==-1);
}
static void test_grid_density(void) {
    const float densities[]={22,24,28},scales[]={.8f,1,1.5f};
    Column columns[]={{.name="ID"},{.name="Amount"}}; Page page={.columns=columns,.column_count=2,.total=1000};
    for (size_t d=0;d<3;d++) for (size_t i=0;i<3;i++) {
        Grid grid; grid_init(&grid); grid.row_height=densities[d]; grid_schema(&grid,&page,true);
        float scale=scales[i]; grid.top=10; grid.fraction=.25;
        grid_layout(&grid,scale,&page,(Rectangle){0,0,800,640});
        Rectangle cell=grid_range_rect(&grid,scale,13,1,2,1); uint64_t row; size_t col;
        CHECK(fabsf(cell.height-2*densities[d]*scale)<.01f);
        CHECK(grid_cell_at(&grid,scale,(Vector2){cell.x+4*scale,cell.y+3*scale},&row,&col) && row==13 && col==1);
        Vector2 pointer={grid.body.x+100,grid.body.y+130};
        double before=(double)grid.top+grid.fraction+(pointer.y-grid.body.y)/(densities[d]*scale);
        grid_zoom_at(&grid,&page,scale,1.2f,pointer);
        double after=(double)grid.top+grid.fraction+(pointer.y-grid.body.y)/(densities[d]*scale*grid.zoom);
        CHECK(fabs(before-after)<.001 && grid.row_height==densities[d]);
        grid_destroy(&grid);
    }
}
static void test_bounded_preview(void) {
    Column columns[]={{.name="ID"},{.name="Amount"}}; Page page={.columns=columns,.column_count=2,.total=12};
    Grid grid; grid_init(&grid); grid.bounded=true; grid.row_height=24; grid_schema(&grid,&page,true);
    grid.top=1000; grid_layout(&grid,1,&page,(Rectangle){0,0,800,240});
    CHECK(grid.bounded && grid.width_count==2 && grid.data_columns==2 && grid.top<12);
    CHECK(grid.top+grid.render_rows<=12 && grid.render_rows<=12);
    grid_schema(&grid,&page,true); CHECK(grid.bounded && grid.row_height==24);
    page.total=0; grid_layout(&grid,1,&page,(Rectangle){0,0,800,240}); CHECK(!grid.render_rows && !grid.top);
    grid_destroy(&grid);
}
static void test_carbon_assets(void) {
    Image atlas=LoadImage("assets/ui/carbon-atlas.png");
    CHECK(atlas.data && atlas.width==512 && atlas.height==((CARBON_COUNT+7)/8)*64);
    Color *pixels=LoadImageColors(atlas); CHECK(pixels && FileExists("assets/ui/CARBON-LICENSE.txt"));
    for (int icon=0;icon<CARBON_COUNT;icon++) {
        size_t covered=0;
        for (int y=0;y<64;y++) for (int x=0;x<64;x++) covered+=pixels[((icon/8)*64+y)*atlas.width+(icon%8)*64+x].a>0;
        CHECK(covered>16 && covered<64*64);
    }
    UnloadImageColors(pixels); UnloadImage(atlas);
}
static void test_ribbon_layout(void) {
    const UIRibbonGroup groups[]={{"Document",0,3,100,false},{"Workbook",3,1,80,false},{"History",4,2,40,false},{"Clipboard",6,1,60,false},{"Export",7,1,20,false}};
    float labels[UI_RIBBON_COMMANDS]={28,27,41,57,30,29,28,86},names[UI_RIBBON_GROUPS]={54,58,40,53,34};
    const float widths[]={240,360,480,680,1020,1280,1920};
    for (int mode=0;mode<3;mode++) for (size_t w=0;w<sizeof(widths)/sizeof(*widths);w++) {
        float width=widths[w]; UIRibbonLayout layout=ui_ribbon_layout(width,mode,groups,5,labels,names);
        CHECK(layout.valid && layout.group_count==5 && layout.height==ui_ribbon_height(mode));
        int hidden=0; float previous=0;
        for (int g=0;g<5;g++) {
            Rectangle r=layout.groups[g]; hidden+=layout.states[g]==UI_RIBBON_OVERFLOW;
            if (layout.states[g]==UI_RIBBON_OVERFLOW) { CHECK(r.width==0); continue; }
            CHECK(r.x>=previous && r.x+r.width<=width-6+.01f); previous=r.x+r.width;
            if (layout.overflow_count) CHECK(r.x+r.width<layout.overflow.x);
            for (int c=groups[g].first;c<groups[g].first+groups[g].count;c++) {
                Rectangle button=layout.commands[c];
                CHECK(ui_ribbon_group_for(groups,5,c)==g);
                if (layout.states[g]==UI_RIBBON_EXPANDED) CHECK(button.width>=28 && button.x>=r.x && button.x+button.width<=r.x+r.width+.01f && button.height>=28);
                else CHECK(button.width==0);
            }
        }
        CHECK(hidden==layout.overflow_count);
        if (hidden) CHECK(layout.overflow.width==32 && layout.overflow.x+layout.overflow.width<=width-6+.01f);
        if (width==1920) CHECK(!hidden && layout.states[0]==UI_RIBBON_EXPANDED);
    }
    UIRibbonLayout small=ui_ribbon_layout(360,0,groups,5,labels,names);
    CHECK(small.overflow_count && small.states[0]!=UI_RIBBON_OVERFLOW && small.states[4]==UI_RIBBON_OVERFLOW);
    CHECK(ui_ribbon_layout(1280,3,groups,5,labels,names).height==0);
    CHECK(!ui_ribbon_layout(NAN,0,groups,5,labels,names).valid);
    CHECK(!ui_ribbon_layout(1280,4,groups,5,labels,names).valid);
    CHECK(!ui_ribbon_layout(1280,0,NULL,5,labels,names).valid);
    CHECK(!ui_ribbon_layout(1280,0,groups,7,labels,names).valid);
    labels[0]=INFINITY; CHECK(!ui_ribbon_layout(1280,0,groups,5,labels,names).valid); labels[0]=28;
    UIRibbonGroup bad[]={{"Bad",UI_RIBBON_COMMANDS-1,2,0,false}}; CHECK(!ui_ribbon_layout(1280,0,bad,1,labels,names).valid);
    UIRibbonGroup primary[]={{"Analyze",0,1,100,true}};
    UIRibbonLayout prominent=ui_ribbon_layout(400,0,primary,1,labels,names);
    CHECK(prominent.valid && prominent.commands[0].height==52 && prominent.commands[0].y==5);
    CHECK(ui_ribbon_layout(400,1,primary,1,labels,names).commands[0].height==28);
    CHECK(ui_ribbon_group_for(groups,5,8)==-1 && ui_ribbon_group_for(groups,5,-1)==-1);
}
static void test_status_geometry(void) {
    const float scales[]={.8f,1,1.5f,2.5f}; const Vector2 viewports[]={{1440,988},{1280,820},{640,640}};
    float widths[]={30,90,95};
    for (int k=0;k<4;k++) for (int v=0;v<3;v++) {
        float scale=scales[k]; Vector2 viewport=viewports[v]; UIStatusLayout l=ui_status_layout(viewport,scale,widths,44.46875f);
        CHECK(l.valid); CHECK(l.panel.height==22*scale); CHECK(l.panel.y+l.panel.height==viewport.y);
        CHECK(l.zoom.width==92*scale && l.zoom.height==19*scale); CHECK(l.customize.width==23*scale);
        CHECK(l.zoom.x+l.zoom.width<=viewport.x); CHECK(l.context[0].x>=0); CHECK(l.context[1].width==0 || viewport.x/scale>=620); CHECK(l.context[2].width==0 || viewport.x/scale>=900);
        CHECK(l.metrics.width>=0 && l.metrics.x+l.metrics.width<=l.customize.x+.01f);
        CHECK(l.scale.x+l.scale.width<=l.zoom.x); CHECK(fabsf(l.minus.x+l.minus.width-l.level.x)<.001f); CHECK(fabsf(l.level.x+l.level.width-l.plus.x)<.001f);
        CHECK(l.scale_text==(viewport.x/scale>=620));
    }
    UIStatusLayout wide=ui_status_layout((Vector2){1440,988},1,widths,44.46875f);
    CHECK(wide.zoom.x==1343 && wide.scale.x==1265.53125f && wide.customize.x==1242.53125f);
    CHECK(wide.panel.y==966 && wide.minus.y==969 && wide.minus.width==23 && wide.level.width==44);
    CHECK(!ui_status_layout((Vector2){1440,988},0,widths,44).valid); CHECK(!ui_status_layout((Vector2){1440,NAN},1,widths,44).valid);
}
static void test_status_metrics(void) {
    UIContext *ui=calloc(1,sizeof(*ui)); CHECK(ui!=NULL);
    GlyphInfo glyph={.advanceX=6}; ui->scale=1; ui->body=ui->small=(Font){.baseSize=12,.glyphs=&glyph}; char output[2048];
    CHECK(ui_fit_metrics(ui,"Cells 4 · Sum 100 · Min 0",100,output)==2 && !strcmp(output,"Cells 4 · Sum 100"));
    CHECK(ui_fit_metrics(ui,"Cells 4 · Sum 100 · Min 0",42,output)==1 && !strcmp(output,"Cells 4"));
    CHECK(ui_fit_metrics(ui,"Cells 4 · Sum 100 · Min 0",29,output)==0 && !*output);
    CHECK(ui_fit_metrics(ui,"Cells 4 · Sum 100 · Min 0",150,output)==3 && !strcmp(output,"Cells 4 · Sum 100 · Min 0"));
    ui->scale=1.5f; CHECK(ui_fit_metrics(ui,"Cells 4 · Sum 100 · Min 0",150,output)==2 && !strcmp(output,"Cells 4 · Sum 100"));
    CHECK(!ui_fit_metrics(ui,NULL,100,output) && !*output);
    CHECK(!ui_fit_metrics(ui,"Cells 4",NAN,output) && !*output);
    char large[2100]; memset(large,'A',2047); memcpy(large+2047," · B",6);
    ui->scale=1; CHECK(ui_fit_metrics(ui,large,20000,output)==1 && strlen(output)==2047);
    UIStatusMetrics layout; ui_status_metrics_layout(ui,"Cells 4 · Sum 100",&layout);
    CHECK(layout.count==2 && layout.width==99); CHECK(!strcmp(layout.labels+layout.offsets[0],"Cells 4") && !strcmp(layout.labels+layout.offsets[1],"Sum 100"));
    ui_status_metrics_layout(ui,NULL,&layout); CHECK(!layout.count && !layout.width);
    ui_status_metrics_layout(ui,output,&layout); CHECK(layout.count==1 && strlen(layout.labels)==2047);
    free(ui);
}
static void test_menu_bounds(void) {
    const float scales[]={.8f,1,1.25f,1.5f};
    for (size_t i=0;i<sizeof(scales)/sizeof(*scales);i++) for (int count=7;count<=11;count++) {
        float s=scales[i]; UIMenuLayout menu=ui_menu_layout((Vector2){1010,630},count,38,244,s,(Vector2){1020,640});
        CHECK(menu.visible>=1 && menu.visible<=count);
        CHECK(menu.panel.x>=8*s && menu.panel.y>=8*s && menu.panel.x+menu.panel.width<=1020-8*s+.01f && menu.panel.y+menu.panel.height<=640-8*s+.01f);
        CHECK(menu.items.y+menu.items.height<=menu.panel.y+menu.panel.height-8*s+.01f);
        if (s==1.5f && count==11) CHECK(menu.visible==10);
    }
    UIMenuLayout menu=ui_menu_layout((Vector2){-100,-100},11,38,244,1.5f,(Vector2){1020,640});
    CHECK(menu.panel.x==12 && menu.panel.y==12 && menu.visible==10);
    CHECK(!ui_menu_layout((Vector2){0},0,38,244,1,(Vector2){1020,640}).visible);
    CHECK(!ui_menu_layout((Vector2){0},11,NAN,244,1,(Vector2){1020,640}).visible);
}
static void test_text_lines(void) {
    GlyphInfo glyphs[]={{.advanceX=1}}; Rectangle recs[]={{.width=1}};
    UIContext u={.scale=1,.body={.baseSize=1,.glyphCount=1,.glyphs=glyphs,.recs=recs}}; TextLines l={0};
    const char *value="İx界z"; text_lines_layout(&l,&u,value,26);
    CHECK(l.count==2 && l.offsets[1]==3 && l.offsets[2]==7 && !l.limited);
    size_t *retained=l.offsets;
    text_lines_layout(&l,&u,"ab cd",52); CHECK(l.count==2 && l.offsets[1]==3);
    CHECK(l.offsets==retained && l.capacity==256);
    text_lines_layout_sized(&l,&u,"abcd",48,12); CHECK(l.count==1 && l.font_size==12 && l.offsets==retained);
    text_lines_layout_sized(&l,&u,value,24,12); CHECK(l.count==2 && l.offsets[1]==3 && l.offsets[2]==7 && l.font_size==12);
    text_lines_layout(&l,&u,"ab cd",52); CHECK(l.font_size==13 && l.count==2 && l.offsets[1]==3);
    text_lines_layout(&l,&u,"a\r\nb\rc\n",100); CHECK(l.count==4 && l.offsets[1]==3 && l.offsets[2]==5 && l.offsets[3]==7);
    text_lines_layout(&l,&u,"a\tb",52); CHECK(l.count==3 && l.offsets[1]==1 && l.offsets[2]==2);
    text_lines_layout(&l,&u,"",0); CHECK(l.count==1 && !l.offsets[0] && !l.offsets[1]);
    text_lines_layout(&l,&u,"界x",1); CHECK(l.count==2 && l.offsets[1]==3); /* Never split UTF-8 or stall on a wide glyph. */
    char *many=dx_alloc(DX_TEXT_LINES+2); memset(many,'\n',DX_TEXT_LINES+1);
    text_lines_layout(&l,&u,many,100); CHECK(l.count==DX_TEXT_LINES && l.limited && l.offsets[l.count]==DX_TEXT_LINES-1);
    retained=l.offsets; text_lines_layout(&l,&u,"short",100); CHECK(l.offsets==retained && l.count==1 && !l.limited);
    free(many); text_lines_free(&l); CHECK(!l.offsets && !l.count);
}
static void test_text_editing(void) {
    TextInput in={0}; input_insert(&in,"İ🙂x");
    CHECK(in.length==7 && in.cursor==7 && in.anchor==7 && !strcmp(in.text,"İ🙂x"));
    uint64_t revision=in.revision;
    input_key(&in,KEY_LEFT,0); CHECK(in.cursor==6 && in.revision==revision);
    input_insert(&in,"A"); CHECK(!strcmp(in.text,"İ🙂Ax") && in.cursor==7);
    input_key(&in,KEY_BACKSPACE,0); CHECK(!strcmp(in.text,"İ🙂x") && in.cursor==6);
    input_key(&in,KEY_BACKSPACE,0); CHECK(!strcmp(in.text,"İx") && in.cursor==2);
    input_key(&in,KEY_DELETE,0); CHECK(!strcmp(in.text,"İ") && in.cursor==2);
    input_key(&in,KEY_HOME,0); input_key(&in,KEY_DELETE,0); CHECK(!*in.text && !in.length && !in.cursor);
    input_set(&in,"İstanbul"); input_key(&in,KEY_RIGHT,0); CHECK(in.cursor==9 && in.anchor==9 && !in.select_all);
    input_key(&in,KEY_LEFT,DX_MOD_SHIFT); input_key(&in,KEY_LEFT,DX_MOD_SHIFT);
    char *copy=input_selection_text(&in); CHECK(copy && !strcmp(copy,"ul")); free(copy);
    input_insert(&in,"az"); CHECK(!strcmp(in.text,"İstanbaz") && in.cursor==9 && in.anchor==9);
    input_key(&in,KEY_HOME,0); input_key(&in,KEY_RIGHT,DX_MOD_SHIFT); input_key(&in,KEY_RIGHT,DX_MOD_SHIFT);
    copy=input_selection_text(&in); CHECK(copy && !strcmp(copy,"İs")); free(copy);
    input_insert(&in,in.text+3); CHECK(!strcmp(in.text,"tanbaztanbaz") && in.cursor==6);
    input_key(&in,KEY_END,0); input_key(&in,KEY_HOME,DX_MOD_SHIFT); revision=in.revision;
    input_insert(&in,in.text); CHECK(!strcmp(in.text,"tanbaztanbaz") && in.cursor==12 && in.revision==revision);
    CHECK(!input_selection_text(&in));
    input_set(&in,"alpha βeta.gamma"); in.select_all=false;
#ifdef __APPLE__
    unsigned word=DX_MOD_ALT,line=DX_MOD_SUPER;
#else
    unsigned word=DX_MOD_CONTROL,line=0;
#endif
    input_key(&in,KEY_LEFT,word); CHECK(in.cursor==12);
    input_key(&in,KEY_LEFT,word); CHECK(in.cursor==6);
    input_key(&in,KEY_LEFT,word|DX_MOD_SHIFT); copy=input_selection_text(&in); CHECK(copy && !strcmp(copy,"alpha ")); free(copy);
    input_insert(&in,"X"); CHECK(!strcmp(in.text,"Xβeta.gamma"));
    input_key(&in,KEY_HOME,0); input_key(&in,KEY_RIGHT,word); CHECK(in.cursor==6);
    input_key(&in,KEY_HOME,0); input_key(&in,KEY_END,line); CHECK(in.cursor==in.length);
#ifdef __APPLE__
    input_key(&in,KEY_BACKSPACE,line); CHECK(!in.length);
#endif
    input_set(&in,"İx"); in.select_all=false; in.cursor=in.anchor=1;
    input_key(&in,KEY_DELETE,0); CHECK(!strcmp(in.text,"x") && !in.cursor);
    input_key(&in,KEY_BACKSPACE,0); CHECK(!strcmp(in.text,"x"));
    input_key(&in,KEY_END,DX_MOD_SHIFT); input_insert(&in,""); CHECK(!in.length && !*in.text);
    input_set(&in,"İstanbul\nSELECT 'βeta'\nFROM input"); in.multiline=true; in.select_all=false;
    input_key(&in,KEY_HOME,0); CHECK(in.cursor==25); input_key(&in,KEY_END,0); CHECK(in.cursor==35);
    in.cursor=in.anchor=15; input_key(&in,KEY_HOME,0); CHECK(in.cursor==10); input_key(&in,KEY_END,DX_MOD_SHIFT);
    copy=input_selection_text(&in); CHECK(copy && !strcmp(copy,"SELECT 'βeta'")); free(copy);
    input_insert(&in,"SELECT City"); CHECK(!strcmp(in.text,"İstanbul\nSELECT City\nFROM input"));
    CHECK(input_history(&in,false) && in.multiline && !strcmp(in.text,"İstanbul\nSELECT 'βeta'\nFROM input"));
    input_key(&in,KEY_HOME,0); input_key(&in,KEY_BACKSPACE,0); CHECK(strstr(in.text,"İstanbulSELECT"));
    input_free(&in);
}
static size_t history_bytes(const TextInputState *states,size_t count) {
    size_t bytes=0; for (size_t i=0;i<count;i++) bytes+=states[i].capacity; return bytes;
}
static void test_text_history(void) {
    TextInput in={0};
    CHECK(!input_history(&in,false) && !input_history(&in,true) && !in.revision);
    input_insert(&in,"İstanbul"); uint64_t revision=in.revision;
    CHECK(input_history(&in,false) && in.text && !*in.text && in.revision==revision+1);
    CHECK(input_history(&in,true) && !strcmp(in.text,"İstanbul") && in.cursor==9 && in.anchor==9);
    input_key(&in,KEY_LEFT,DX_MOD_SHIFT); input_key(&in,KEY_LEFT,DX_MOD_SHIFT); in.view_start=2;
    input_insert(&in,"az"); CHECK(!strcmp(in.text,"İstanbaz"));
    CHECK(input_history(&in,false) && !strcmp(in.text,"İstanbul") && in.cursor==7 && in.anchor==9 && in.view_start==2);
    char *selected=input_selection_text(&in); CHECK(selected && !strcmp(selected,"ul")); free(selected);
    CHECK(input_history(&in,true) && !strcmp(in.text,"İstanbaz") && in.cursor==9 && in.anchor==9 && !in.select_all);
    CHECK(input_history(&in,false)); input_insert(&in,"kent");
    CHECK(!strcmp(in.text,"İstanbkent") && !input_history(&in,true)); /* A branch discards redo. */
    in.select_all=true; revision=in.revision; size_t count=in.undo_count;
    input_insert(&in,in.text);
    CHECK(in.revision==revision && in.undo_count==count && !in.select_all); /* No-op replacement. */
    input_set(&in,"İ🙂x"); in.select_all=false; in.view_start=2;
    input_key(&in,KEY_BACKSPACE,0); input_key(&in,KEY_BACKSPACE,0);
    CHECK(!strcmp(in.text,"İ") && input_history(&in,false) && !strcmp(in.text,"İ🙂"));
    CHECK(input_history(&in,false) && !strcmp(in.text,"İ🙂x") && in.cursor==7 && in.view_start==2);
    in.select_all=true; input_key(&in,KEY_DELETE,0);
    CHECK(!in.length && input_history(&in,false) && in.select_all && !strcmp(in.text,"İ🙂x"));
    CHECK(input_history(&in,true) && !in.length);
    input_set(&in,"alpha βeta.gamma"); in.select_all=false;
#ifdef __APPLE__
    input_key(&in,KEY_BACKSPACE,DX_MOD_ALT);
    CHECK(!strcmp(in.text,"alpha βeta.") && input_history(&in,false) && !strcmp(in.text,"alpha βeta.gamma"));
    input_key(&in,KEY_BACKSPACE,DX_MOD_SUPER);
    CHECK(!in.length && input_history(&in,false) && !strcmp(in.text,"alpha βeta.gamma"));
#else
    input_key(&in,KEY_BACKSPACE,DX_MOD_CONTROL);
    CHECK(!strcmp(in.text,"alpha βeta.") && input_history(&in,false) && !strcmp(in.text,"alpha βeta.gamma"));
#endif
    /* Payloads may alias current text or a state freed by branching/eviction. */
    input_set(&in,"old"); input_insert(&in,"İnew"); CHECK(input_history(&in,false));
    const char *payload=in.redo[in.redo_count-1].text+2;
    input_insert(&in,payload); CHECK(!strcmp(in.text,"new") && !in.redo_count);
    CHECK(input_history(&in,false) && !strcmp(in.text,"old"));
    input_set(&in,"a"); in.select_all=false;
    for (int i=0;i<32;i++) input_insert(&in,"b");
    CHECK(in.undo_count==32 && in.length==33);
    in.select_all=true; input_insert(&in,in.undo[0].text);
    CHECK(!strcmp(in.text,"a") && in.undo_count==32);
    for (int i=0;i<32;i++) CHECK(input_history(&in,false));
    CHECK(in.length==2 && !strcmp(in.text,"ab") && !input_history(&in,false) && in.redo_count==32);
    for (int i=0;i<32;i++) CHECK(input_history(&in,true));
    CHECK(!strcmp(in.text,"a") && !input_history(&in,true));
    input_set(&in,"abcdef"); input_insert(&in,"replacement"); revision=in.revision;
    input_set(&in,in.undo[in.undo_count-1].text+2);
    CHECK(!strcmp(in.text,"cdef") && !in.undo_count && !in.redo_count && in.revision==revision+1);
    char *large=dx_alloc(70001); memset(large,'x',70000); input_set(&in,large); free(large);
    char *value=dx_alloc(65537); memset(value,'x',65536);
    for (int i=0;i<40;i++) { value[0]=(char)('a'+i%26); in.select_all=true; input_insert(&in,value); }
    free(value);
    CHECK(in.undo_count>0 && in.undo_count<32 && history_bytes(in.undo,in.undo_count)<=1024*1024);
    count=in.undo_count; for (size_t i=0;i<count;i++) CHECK(input_history(&in,false));
    CHECK(!input_history(&in,false) && history_bytes(in.redo,in.redo_count)<=1024*1024);
    for (size_t i=0;i<count;i++) CHECK(input_history(&in,true));
    CHECK(!input_history(&in,true) && history_bytes(in.undo,in.undo_count)<=1024*1024);
    large=dx_alloc(1024*1024+2); memset(large,'x',1024*1024+1); input_set(&in,large); free(large);
    input_insert(&in,"small");
    CHECK(!strcmp(in.text,"small") && !in.undo_count && !input_history(&in,false));
    /* Restoring a tiny snapshot also frees an oversized current allocation. */
    input_set(&in,"before"); input_insert(&in,"after");
    in.text=dx_realloc(in.text,1024*1024+1); in.capacity=1024*1024+1;
    CHECK(input_history(&in,false) && !strcmp(in.text,"before") && !in.redo_count);
    input_free(&in); TextInput empty={0}; CHECK(!memcmp(&in,&empty,sizeof(in))); input_free(&in);
}
static int font_benchmark(void) {
    const char *paths[]={"assets/fonts/IBMPlexSans-Regular.otf","assets/fonts/IBMPlexSans-Regular.otf","assets/fonts/IBMPlexSans-Medium.otf","assets/fonts/IBMPlexSans-SemiBold.otf","assets/fonts/IBMPlexSans-Italic.otf","assets/fonts/IBMPlexSans-SemiBoldItalic.otf","assets/fonts/JetBrainsMono-Regular.ttf"};
    const int sizes[]={32,22,32,32,32,32,20}; int codes[1024],marks[]={'N','U','L','"',' '}; int n=(int)ui_font_codepoints(codes);
    printf("CPU font decoding/raster only; 100 percent UI, 2x display; %d primary codepoints; no packing, GPU, window or filesystem-cache flush\n",n);
    for (int pass=0;pass<5;pass++) {
        double total=0; size_t bytes=0;
        for (int face=0;face<7;face++) {
            clock_t start=clock(); int length=0,count=0; unsigned char *data=LoadFileData(paths[face],&length);
            if (!data) return 1;
            GlyphInfo *glyphs=LoadFontData(data,length,sizes[face],face==6 ? marks : codes,face==6 ? 5 : n,FONT_DEFAULT,&count);
            if (!glyphs || count<=0 || count>(face==6 ? 5 : n)) { UnloadFileData(data); if (glyphs) UnloadFontData(glyphs,count); return 1; }
            double elapsed=1000.0*(clock()-start)/CLOCKS_PER_SEC; size_t payload=0;
            for (int i=0;i<count;i++) if (glyphs[i].image.data && glyphs[i].image.width>0 && glyphs[i].image.height>0) payload+=(size_t)GetPixelDataSize(glyphs[i].image.width,glyphs[i].image.height,glyphs[i].image.format);
            printf("pass=%d face=%d raster=%d requested=%d glyphs=%d cpu_ms=%.3f glyph_pixels=%zu\n",pass+1,face,sizes[face],face==6 ? 5 : n,count,elapsed,payload);
            total+=elapsed; bytes+=payload; UnloadFontData(glyphs,count); UnloadFileData(data);
        }
        printf("pass=%d family_cpu_ms=%.3f decoded_glyph_pixels=%zu\n",pass+1,total,bytes);
    }
    return 0;
}
static int column_benchmark(void) {
    Page blank={0};
    puts("CPU column geometry allocation/growth only; blank sheet to 16,384 columns; no window, fonts, queries or rendering");
    for (int pass=0;pass<5;pass++) {
        clock_t started=clock(); double checksum=0;
        for (int iteration=0;iteration<500;iteration++) {
            Grid grid; grid_init(&grid); grid_schema(&grid,&blank,true); grid_ensure_columns(&grid,16384);
            checksum+=grid.offsets[16384]; grid_destroy(&grid);
        }
        printf("pass=%d repetitions=500 cpu_ms=%.3f checksum=%.0f\n",pass+1,1000.0*(clock()-started)/CLOCKS_PER_SEC,checksum);
    }
    return 0;
}
static void test_long_range_outlines(void) {
    Grid g; grid_init(&g); Page page={0}; UIContext ui={.scale=1}; Rectangle viewport={150,100,850,600};
    grid_schema(&g,&page,true); grid_ensure_columns(&g,52);
    const uint64_t ends[]={3475267,DX_SHEET_ROWS-21}; const float scales[]={.8f,1,1.5f};
    for (size_t i=0;i<2;i++) for (size_t j=0;j<3;j++) {
        float scale=scales[j]; ui.scale=scale; g.used_rows=ends[i]+1; grid_layout(&g,scale,&page,viewport);
        g.top=ends[i]-g.visible_rows+2; g.fraction=.375; g.horizontal=g.offsets[10]; grid_layout(&g,scale,&page,viewport);
        Rectangle last=grid_range_rect(&g,scale,ends[i],10,1,1),outline=grid_range_outline(&g,scale,40,10,ends[i]-39,3,g.body);
        CHECK(outline.width>0 && fabsf(outline.y+outline.height-last.y-last.height)<.001f);
        CHECK(outline.y<g.body.y && outline.height<=g.body.height+16*scale);
        CHECK(fabsf(outline.x-last.x)<.001f && fabsf(outline.width-288*scale)<.001f);
        grid_select_cell(&g,40,10,false); g.end_row=ends[i]; g.end_col=12; g.allow_move=true;
        Vector2 bottom={outline.x+outline.width*.5f,outline.y+outline.height-scale};
        event(&g,&ui,&page,(GridInput){.mouse=bottom,.pressed=true,.down=true}); CHECK(g.moving);
        event(&g,&ui,&page,(GridInput){.key=KEY_ESCAPE}); CHECK(!g.moving && g.anchor_row==40 && g.end_row==ends[i]);
    }
    CHECK(grid_range_outline(&g,1,0,0,0,1,g.body).height==0);
    CHECK(grid_range_outline(&g,1,DX_SHEET_ROWS,0,1,1,g.body).height==0);
    CHECK(grid_range_outline(&g,1,DX_SHEET_ROWS-1,0,2,1,g.body).height==0);
    CHECK(grid_range_outline(&g,1,40,g.width_count,1,1,g.body).width==0);
    CHECK(grid_range_outline(&g,NAN,40,10,1,1,g.body).width==0);
    CHECK(grid_range_outline(&g,1,0,10,5,1,g.body).height==0);
    grid_destroy(&g);
}
static void test_region_navigation(void) {
    Grid g; grid_init(&g); UIContext ui={.scale=1}; Page page={0};
    grid_schema(&g,&page,true); grid_ensure_columns(&g,52); grid_layout(&g,1,&page,(Rectangle){0,0,900,600});
    g.used_rows=1027; g.used_columns=48;
    g.navigation=(GridRegion){21,1026,36,41,true}; grid_select_cell(&g,22,37,false);
    event(&g,&ui,&page,(GridInput){.key=KEY_DOWN,.shortcut=true}); CHECK(g.row==1026 && g.col==37);
    event(&g,&ui,&page,(GridInput){.key=KEY_UP,.shortcut=true}); CHECK(g.row==21 && g.col==37);
    event(&g,&ui,&page,(GridInput){.key=KEY_RIGHT,.shortcut=true}); CHECK(g.row==21 && g.col==41);
    event(&g,&ui,&page,(GridInput){.key=KEY_LEFT,.shortcut=true}); CHECK(g.row==21 && g.col==36);
    grid_select_cell(&g,22,38,false); event(&g,&ui,&page,(GridInput){.key=KEY_DOWN,.shortcut=true,.shift=true});
    CHECK(g.anchor_row==22 && g.end_row==1026 && g.row==22 && g.anchor_col==38 && g.top+g.visible_rows>1026);
    event(&g,&ui,&page,(GridInput){.key=KEY_LEFT,.shortcut=true,.shift=true}); CHECK(g.anchor_col==38 && g.end_col==36 && g.anchor_row==22);
    grid_select_cell(&g,25,38,false); event(&g,&ui,&page,(GridInput){.key=KEY_HOME}); CHECK(g.row==25 && g.col==36);
    event(&g,&ui,&page,(GridInput){.key=KEY_END}); CHECK(g.row==25 && g.col==41);
    event(&g,&ui,&page,(GridInput){.key=KEY_HOME,.shortcut=true}); CHECK(g.row==0 && g.col==0);
    event(&g,&ui,&page,(GridInput){.key=KEY_END,.shortcut=true}); CHECK(g.row==1026 && g.col==47);
    grid_select_cell(&g,25,38,false); g.selection=SELECT_ROWS;
    event(&g,&ui,&page,(GridInput){.key=KEY_DOWN,.shortcut=true,.shift=true}); CHECK(g.selection==SELECT_ROWS && g.anchor_row==25 && g.end_row==1026);
    grid_select_cell(&g,25,38,false); g.selection=SELECT_COLUMNS;
    event(&g,&ui,&page,(GridInput){.key=KEY_RIGHT,.shortcut=true,.shift=true}); CHECK(g.selection==SELECT_COLUMNS && g.anchor_col==38 && g.end_col==41);
    g.navigation.r1=g.navigation.r0; grid_select_cell(&g,21,36,false);
    event(&g,&ui,&page,(GridInput){.key=KEY_DOWN,.separate_keyboard=true,.key_shortcut=true}); CHECK(g.row==21 && g.col==36);
    g.navigation.active=false; grid_select_cell(&g,25,38,false);
    event(&g,&ui,&page,(GridInput){.key=KEY_DOWN,.shortcut=true}); CHECK(g.row==0 && g.col==38);
    g.navigation=(GridRegion){21,DX_SHEET_ROWS,36,41,true}; grid_select_cell(&g,25,38,false);
    event(&g,&ui,&page,(GridInput){.key=KEY_DOWN,.shortcut=true}); CHECK(g.row==0);
    g.navigation=(GridRegion){21,26,41,36,true}; grid_select_cell(&g,25,38,false);
    event(&g,&ui,&page,(GridInput){.key=KEY_RIGHT,.shortcut=true}); CHECK(g.col==0);
    grid_destroy(&g);
}
static void test_addresses(void) {
    struct { const char *text; uint64_t row; size_t column; } valid[]={
        {"A1",0,0},{"  h12 \t",11,7},{"$XFD$1099511627776",DX_SHEET_ROWS-1,16383},
        {"aa001",0,26},{"Z$1048576",1048575,25},{"$b7",6,1}
    };
    for (size_t i=0;i<sizeof(valid)/sizeof(*valid);i++) {
        uint64_t row=9; size_t col=9;
        CHECK(grid_parse_address(valid[i].text,&row,&col)); CHECK(row==valid[i].row && col==valid[i].column);
    }
    const char *invalid[]={NULL,"","A0","A","1","XFE1","A1099511627777","A18446744073709551616","AAAAAAAA1","A1:B2","Sheet1!A1","A-1","A1+1","A 1","$$A1","A$$1","İ1"};
    for (size_t i=0;i<sizeof(invalid)/sizeof(*invalid);i++) {
        uint64_t row=9; size_t col=9;
        CHECK(!grid_parse_address(invalid[i],&row,&col)); CHECK(row==9 && col==9);
    }
    uint64_t row; size_t col; CHECK(!grid_parse_address("A1",NULL,&col) && !grid_parse_address("A1",&row,NULL));
}
static void test_navigation_queue(void) {
    DxKeyModifiers event; while (ui_take_navigation_key(&event)) {}
    DxKeyModifiers expected[]={{.key=KEY_RIGHT},{.key=KEY_RIGHT,.modifiers=DX_MOD_SHIFT},{.key=KEY_DOWN,.modifiers=DX_MOD_SUPER},{.key=KEY_LEFT,.modifiers=DX_MOD_CONTROL},{.key=KEY_ENTER,.modifiers=DX_MOD_SHIFT}};
    for (size_t i=0;i<sizeof(expected)/sizeof(*expected);i++) ui_note_key_modifiers(expected[i].key,expected[i].modifiers);
    CHECK(ui_navigation_captured() && !ui_take_navigation_key(NULL));
    for (size_t i=0;i<sizeof(expected)/sizeof(*expected);i++) {
        CHECK(ui_take_navigation_key(&event)); CHECK(event.key==expected[i].key && event.modifiers==expected[i].modifiers);
    }
    CHECK(!ui_take_navigation_key(&event) && ui_navigation_captured());
    ui_note_key_modifiers(KEY_ESCAPE,0); CHECK(!ui_take_navigation_key(&event));
    for (unsigned i=0;i<140;i++) ui_note_key_modifiers(i%2 ? KEY_LEFT : KEY_RIGHT,i%16);
    for (unsigned i=0;i<128;i++) { CHECK(ui_take_navigation_key(&event)); CHECK(event.key==(i%2 ? KEY_LEFT : KEY_RIGHT) && event.modifiers==i%16); }
    CHECK(!ui_take_navigation_key(&event));
    UIContext context={0}; ui_destroy(&context); CHECK(!ui_navigation_captured());
}
static void test_keytip_queue(void) {
    UIContext context={0}; ui_destroy(&context); DxKeyModifiers press;
    const int keys[]={KEY_LEFT_ALT,KEY_H,KEY_C,KEY_C,KEY_ESCAPE};
    for (unsigned i=0;i<5;i++) ui_note_keytip_press(keys[i],i ? 0 : DX_MOD_ALT,false,false);
    CHECK(!ui_take_keytip_press(NULL));
    for (unsigned i=0;i<5;i++) { CHECK(ui_take_keytip_press(&press)); CHECK(press.key==keys[i]); }
    CHECK(!ui_take_keytip_press(&press));
    ui_note_keytip_press(KEY_F,0,false,false); /* duplicate GLFW stream */
    ui_note_keytip_press(KEY_H,DX_MOD_ALT,true,false);
    ui_note_keytip_press(KEY_H,DX_MOD_ALT,true,true); /* repeat excluded */
    ui_note_keytip_press(KEY_C,0,false,false); /* native stream owns this frame */
    ui_note_keytip_press(KEY_C,DX_MOD_SHIFT,true,false);
    ui_note_keytip_press(KEY_C,0,true,false);
    CHECK(ui_take_keytip_press(&press) && press.key==KEY_H && press.modifiers==DX_MOD_ALT);
    CHECK(ui_take_keytip_press(&press) && press.key==KEY_C && press.modifiers==DX_MOD_SHIFT);
    CHECK(ui_take_keytip_press(&press) && press.key==KEY_C && !press.modifiers);
    CHECK(!ui_take_keytip_press(&press));
    for (unsigned i=0;i<140;i++) ui_note_keytip_press(KEY_A+i%26,i%16,true,false);
    for (unsigned i=0;i<128;i++) { CHECK(ui_take_keytip_press(&press)); CHECK(press.key==KEY_A+(int)(i%26) && press.modifiers==i%16); }
    CHECK(!ui_take_keytip_press(&press));
    ui_note_keytip_press(KEY_J,0,true,false); ui_destroy(&context); CHECK(!ui_take_keytip_press(&press));
}
static void test_frozen_columns(void) {
    CHECK(ui_canvas_font_scale(.5f)==1 && ui_canvas_font_scale(1)==1 && ui_canvas_font_scale(NAN)==1);
    CHECK(ui_canvas_font_scale(1.1f)==1.25f && ui_canvas_font_scale(1.25f)==1.25f && ui_canvas_font_scale(1.5f)==1.5f);
    CHECK(ui_canvas_font_scale(1.6f)==2 && ui_canvas_font_scale(2)==2 && ui_canvas_font_scale(2.5f)==2.5f);
    Column columns[6]={{.name="Date"},{.name="ID"},{.name="City"},{.name="Amount"},{.name="Status"},{.name="Memo"}};
    Page p={.columns=columns,.column_count=6,.total=1000,.row_count=DX_PAGE_ROWS};
    const float scales[]={.8f,1,1.5f,2};
    for (size_t i=0;i<4;i++) {
        float scale=scales[i]; UIContext u={.scale=scale}; Grid g; grid_init(&g); g.freeze=true;
        grid_schema(&g,&p,true); CHECK(g.freeze); grid_set_width(&g,0,110); grid_set_width(&g,1,150); g.row_height=24;
        Rectangle rect={20,40,900*scale,500*scale}; grid_layout(&g,scale,&p,rect);
        CHECK(g.frozen_columns==2 && g.frozen_width==260 && !g.freeze_blocked);
        float fixed=grid_column_x(&g,scale,1); g.horizontal=200; grid_layout(&g,scale,&p,rect);
        CHECK(grid_column_x(&g,scale,1)==fixed && fabsf(g.htrack.x-g.body.x-260*scale)<.01f && g.first_col>=2);
        Rectangle frozen=grid_column_clip(&g,scale,0),scroll=grid_column_clip(&g,scale,2);
        CHECK(fabsf(frozen.width-260*scale)<.01f && scroll.x==frozen.x+frozen.width && fabsf(scroll.width+frozen.width-g.body.width)<.01f);
        for (size_t col=0;col<2;col++) {
            uint64_t row; size_t hit; Rectangle cell=grid_range_rect(&g,scale,3,col,1,1);
            CHECK(grid_cell_at(&g,scale,(Vector2){cell.x+cell.width/2,cell.y+cell.height/2},&row,&hit) && row==3 && hit==col);
            event(&g,&u,&p,(GridInput){.mouse={cell.x+cell.width/2,cell.y+cell.height/2},.pressed=true,.down=true});
            CHECK(g.col==col && g.row==3); event(&g,&u,&p,(GridInput){.released=true});
        }
        uint64_t row; size_t hit; Vector2 point={scroll.x+30*scale,g.body.y+10*scale};
        CHECK(grid_cell_at(&g,scale,point,&row,&hit) && hit>=2);
        float before=g.horizontal; grid_reveal_column(&g,&p,scale,0,true); CHECK(g.horizontal==before);
        grid_select_cell(&g,3,1,false); grid_reveal_cell(&g,&p,scale); CHECK(g.horizontal==before);
        grid_reveal_column(&g,&p,scale,5,false); CHECK(grid_column_x(&g,scale,5)>=scroll.x-.01f && grid_column_x(&g,scale,5)+g.widths[5]*scale<=scroll.x+scroll.width+.01f);
        float right=grid_column_x(&g,scale,1)+g.widths[1]*scale;
        event(&g,&u,&p,(GridInput){.mouse={right-12*scale,g.rect.y+35*scale},.pressed=true,.down=true});
        CHECK(g.filter_requested && g.col==1); g.filter_requested=false; event(&g,&u,&p,(GridInput){.released=true});
        event(&g,&u,&p,(GridInput){.mouse={right,g.rect.y+10*scale},.pressed=true,.down=true}); CHECK(g.resizing==1);
        event(&g,&u,&p,(GridInput){.mouse={right+20*scale,g.rect.y+10*scale},.down=true}); CHECK(fabsf(g.widths[1]-170)<.01f);
        event(&g,&u,&p,(GridInput){.key=KEY_ESCAPE}); CHECK(g.resizing==-1 && g.widths[1]==150);
        before=g.horizontal; grid_zoom_at(&g,&p,scale,1.2f,(Vector2){g.body.x+50*scale,g.body.y+60*scale}); CHECK(g.horizontal==before && g.freeze && g.frozen_columns==2);
        CHECK(fabsf(g.chrome_scale-scale)<.001f && fabsf(g.vtrack.width-12*scale)<.001f && fabsf(g.htrack.height-12*scale)<.001f);
        g.zoom=1; grid_layout(&g,scale,&p,(Rectangle){20,40,320*scale,500*scale});
        CHECK(g.freeze && g.freeze_blocked && g.body.height/scale<500-48-12 && g.htrack.width>=0);
        CHECK(grid_cell_at(&g,scale,(Vector2){g.body.x+30*scale,g.body.y+10*scale},&row,&hit) && hit==0);
        grid_layout(&g,scale,&p,rect); CHECK(g.freeze && !g.freeze_blocked);
        g.horizontal=g.offsets[20]; g.top=500; grid_layout(&g,scale,&p,rect); CHECK(g.first_col>=6 && !grid_cached(&g,&p));
        g.freeze=false; grid_layout(&g,scale,&p,rect); CHECK(grid_cached(&g,&p) && !g.frozen_columns && g.htrack.x==g.body.x);
        grid_destroy(&g);
    }
}
static Vector2 overscroll_setup(Grid *g,UIContext *u,const Page *page,float scale,bool far_edge) {
    grid_init(g); g->bounded=true; g->row_height=24; grid_schema(g,page,true);
    u->scale=scale; grid_layout(g,scale,page,(Rectangle){20,40,900*scale,500*scale});
    grid_select_cell(g,4,2,false);
    if (far_edge) {
        g->top=page->total-g->visible_rows; g->horizontal=g->offsets[g->width_count]-g->body.width/scale;
        grid_layout(g,scale,page,g->rect);
    }
    return (Vector2){g->body.x+g->body.width*.5f,g->body.y+g->body.height*.5f};
}
static void test_overscroll_return(void) {
    Column columns[12]; for (size_t i=0;i<12;i++) columns[i]=(Column){.name="Field",.type=DUCKDB_TYPE_VARCHAR};
    Page page={.columns=columns,.column_count=12,.total=500};
    const float scales[]={1,1.5f,2.5f}; Vector2 reference[2]={{0}};
    for (size_t edge=0;edge<2;edge++) for (size_t factor=0;factor<3;factor++) {
        float scale=scales[factor],outward=edge ? 1.f : -1.f,sign=-outward;
        UIContext u={0}; Grid one,packets; Vector2 pointer=overscroll_setup(&one,&u,&page,scale,edge!=0);
        overscroll_setup(&packets,&u,&page,scale,edge!=0);
        uint64_t top=one.top; float horizontal=one.horizontal;
        /* No elapsed time between packets: coalescing an identical physical
           pull must not change resistance or consume real source positions.
           Call the controller directly: event() replaces dt=0 with .016. */
        grid_handle_input(&one,&u,&page,&(GridInput){.mouse=pointer,.pixel_scroll=true,.pixel_pan={80*outward,120*outward}});
        for (int n=0;n<8;n++) grid_handle_input(&packets,&u,&page,&(GridInput){.mouse=pointer,.pixel_scroll=true,.pixel_pan={10*outward,15*outward}});
        CHECK(fabsf((one.elastic.x-packets.elastic.x)*scale)<.001f && fabsf((one.elastic.y-packets.elastic.y)*scale)<.001f);
        CHECK(one.elastic.x*sign>0 && one.elastic.y*sign>0);
        CHECK(fabsf(one.elastic.x*scale)<=32.001f && fabsf(one.elastic.y*scale)<=32.001f);
        CHECK(one.top==top && !one.fraction && one.horizontal==horizontal && packets.top==top && !packets.fraction && packets.horizontal==horizontal);
        CHECK(one.row==4 && one.col==2 && packets.row==4 && packets.col==2);
        if (!factor) reference[edge]=(Vector2){one.elastic.x*scale,one.elastic.y*scale};
        else CHECK(fabsf(one.elastic.x*scale-reference[edge].x)<.001f && fabsf(one.elastic.y*scale-reference[edge].y)<.001f);
        grid_handle_input(&packets,&u,&page,&(GridInput){.mouse=pointer,.pixel_scroll=true,.pixel_pan={1000000*outward,1000000*outward}});
        CHECK(isfinite(packets.elastic.x) && isfinite(packets.elastic.y) && fabsf(packets.elastic.x*scale)<=32.001f && fabsf(packets.elastic.y*scale)<=32.001f);
        /* Real trackpad momentum includes many subpixel tails. Returning
           velocity survives each packet and the edge settles promptly rather
           than restarting a slow return 120 times each second. */
        for (int n=0;n<36;n++) {
            float before_x=fabsf(one.elastic.x),before_y=fabsf(one.elastic.y);
            grid_handle_input(&one,&u,&page,&(GridInput){.mouse=pointer,.pixel_scroll=true,.pixel_pan={.01f*outward,.01f*outward},.dt=1.f/120});
            CHECK(one.elastic.x*sign>=0 && one.elastic.y*sign>=0);
            CHECK(fabsf(one.elastic.x)<=before_x+.00001f && fabsf(one.elastic.y)<=before_y+.00001f);
            if (n<8) CHECK(one.spring_velocity.x*sign<0 && one.spring_velocity.y*sign<0);
        }
        CHECK(fabsf(one.elastic.x*scale)<.25f && fabsf(one.elastic.y*scale)<.25f);
        for (int n=0;n<24;n++) grid_handle_input(&one,&u,&page,&(GridInput){.mouse=pointer,.dt=1.f/120});
        CHECK(!one.elastic.x && !one.elastic.y && !one.spring_velocity.x && !one.spring_velocity.y);
        CHECK(one.top==top && !one.fraction && one.horizontal==horizontal && one.row==4 && one.col==2);
        /* Equivalent elapsed time, including a slow frame, produces the same
           recovery. This catches throwing away all but .05s of a frame. */
        one.elastic=packets.elastic=(Vector2){40*sign/scale,24*sign/scale};
        one.spring_velocity=packets.spring_velocity=(Vector2){-100*sign/scale,-60*sign/scale};
        one.pan_x=one.pan_y=packets.pan_x=packets.pan_y=(GridPanAxis){0};
        grid_handle_input(&one,&u,&page,&(GridInput){.mouse=pointer,.dt=.2f});
        for (int n=0;n<24;n++) grid_handle_input(&packets,&u,&page,&(GridInput){.mouse=pointer,.dt=1.f/120});
        CHECK(fabsf((one.elastic.x-packets.elastic.x)*scale)<.001f && fabsf((one.elastic.y-packets.elastic.y)*scale)<.001f);
        CHECK(fabsf((one.spring_velocity.x-packets.spring_velocity.x)*scale)<.01f && fabsf((one.spring_velocity.y-packets.spring_velocity.y)*scale)<.01f);
        CHECK(fabsf(one.elastic.x*scale)<1 && fabsf(one.elastic.y*scale)<1);
        grid_handle_input(&one,&u,&page,&(GridInput){.mouse=pointer,.dt=.6f});
        CHECK(!one.elastic.x && !one.elastic.y && !one.spring_velocity.x && !one.spring_velocity.y);
        /* Reversal consumes visible stretch first. A strong existing return
           must not carry a shortened stretch across zero into the other edge. */
        one.elastic=(Vector2){12*sign/scale,12*sign/scale}; one.spring_velocity=(Vector2){-300*sign/scale,-300*sign/scale};
        grid_handle_input(&one,&u,&page,&(GridInput){.mouse=pointer,.pixel_scroll=true,.pixel_pan={5*sign,5*sign}});
        CHECK(fabsf(one.elastic.x*scale-7*sign)<.001f && fabsf(one.elastic.y*scale-7*sign)<.001f);
        CHECK(one.top==top && !one.fraction && one.horizontal==horizontal);
        float before_x=fabsf(one.elastic.x),before_y=fabsf(one.elastic.y);
        for (int n=0;n<48;n++) {
            grid_handle_input(&one,&u,&page,&(GridInput){.mouse=pointer,.dt=1.f/120});
            CHECK(one.elastic.x*sign>=0 && one.elastic.y*sign>=0);
            CHECK(fabsf(one.elastic.x)<=before_x+.00001f && fabsf(one.elastic.y)<=before_y+.00001f);
            before_x=fabsf(one.elastic.x); before_y=fabsf(one.elastic.y);
        }
        CHECK(!one.elastic.x && !one.elastic.y && one.top==top && !one.fraction && one.horizontal==horizontal);
        one.elastic=(Vector2){12*sign/scale,12*sign/scale}; one.spring_velocity=(Vector2){-300*sign/scale,-300*sign/scale};
        grid_handle_input(&one,&u,&page,&(GridInput){.mouse=pointer,.pixel_scroll=true,.pixel_pan={15*sign,15*sign}});
        CHECK(!one.elastic.x && !one.elastic.y && !one.spring_velocity.x && !one.spring_velocity.y);
        CHECK(fabs(((double)one.top+one.fraction-top)*24*scale-3*sign)<.001);
        CHECK(fabsf((one.horizontal-horizontal)*scale-3*sign)<.005f && one.row==4 && one.col==2);
        grid_destroy(&one); grid_destroy(&packets);
    }
    /* An infinite sheet grows before the allocated right edge is reached.
       A shelf/overlay owning that same wheel cannot allocate sheet columns. */
    for (size_t factor=0;factor<3;factor++) {
        float scale=scales[factor]; UIContext u={.scale=scale}; Grid g; grid_init(&g); grid_schema(&g,&page,true);
        grid_layout(&g,scale,&page,(Rectangle){20,40,900*scale,500*scale});
        g.horizontal=g.offsets[g.width_count]-g.body.width/scale-10/scale; grid_layout(&g,scale,&page,g.rect);
        size_t count=g.width_count; float start=g.horizontal; Vector2 pointer={g.body.x+100,g.body.y+100};
        grid_handle_input(&g,&u,&page,&(GridInput){.mouse={-100,-100},.pixel_scroll=true,.pixel_pan={160,0}});
        CHECK(g.width_count==count && g.horizontal==start && !g.elastic.x);
        g.block_pointer=true; grid_handle_input(&g,&u,&page,&(GridInput){.mouse=pointer,.pixel_scroll=true,.pixel_pan={160,0}});
        CHECK(g.width_count==count && g.horizontal==start && !g.elastic.x); g.block_pointer=false;
        grid_handle_input(&g,&u,&page,&(GridInput){.mouse=pointer,.pixel_scroll=true,.pixel_pan={160,0}});
        CHECK(g.width_count>count && fabsf((g.horizontal-start)*scale-160)<.01f && !g.elastic.x);
        CHECK(!g.top && !g.fraction && !g.row && !g.col); grid_destroy(&g);
    }
}
static void test_overscroll_gestures(void) {
    Column columns[12]; for (size_t i=0;i<12;i++) columns[i]=(Column){.name="Field",.type=DUCKDB_TYPE_VARCHAR};
    Page page={.columns=columns,.column_count=12,.total=500};
    for (int edge=0;edge<2;edge++) for (int factor=0;factor<2;factor++) {
        float scale=factor ? 2.5f : 1,sign=edge ? -1.f : 1.f;
        UIContext u={0}; Grid one,packets; Vector2 pointer=overscroll_setup(&one,&u,&page,scale,edge!=0);
        overscroll_setup(&packets,&u,&page,scale,edge!=0);
        uint64_t top=one.top; float horizontal=one.horizontal;
        /* Native finger-held pulls follow displacement, without a return
           fighting each later frame. Packetization and empty held frames
           cannot change the amount of pull. */
        grid_handle_input(&one,&u,&page,&(GridInput){.mouse=pointer,.pixel_scroll=true,.pixel_pan={-80*sign,-120*sign},.pan_phase=GRID_PAN_BEGIN,.dt=.05f});
        for (int n=0;n<6;n++) grid_handle_input(&packets,&u,&page,&(GridInput){.mouse=pointer,.pixel_scroll=true,.pixel_pan={-80*sign/6,-20*sign},.pan_phase=n ? GRID_PAN_ACTIVE : GRID_PAN_BEGIN,.dt=1.f/120});
        CHECK(fabsf((one.elastic.x-packets.elastic.x)*scale)<.001f && fabsf((one.elastic.y-packets.elastic.y)*scale)<.001f);
        Vector2 held=one.elastic;
        for (int n=0;n<30;n++) grid_handle_input(&one,&u,&page,&(GridInput){.mouse=pointer,.pixel_scroll=true,.pan_phase=GRID_PAN_ACTIVE,.dt=1.f/60});
        CHECK(one.elastic.x==held.x && one.elastic.y==held.y && !one.spring_velocity.x && !one.spring_velocity.y);
        /* Momentum may keep scrolling inside a sheet, but must not pull an
           exposed edge back out after release, even with large late packets. */
        grid_handle_input(&one,&u,&page,&(GridInput){.mouse=pointer,.pixel_scroll=true,.pan_phase=GRID_PAN_RELEASE});
        float before_x=fabsf(one.elastic.x),before_y=fabsf(one.elastic.y);
        for (int n=0;n<60;n++) {
            float dt=n%3==0 ? 1.f/60 : 1.f/120;
            grid_handle_input(&one,&u,&page,&(GridInput){.mouse=pointer,.pixel_scroll=true,.pixel_pan={-sign*(n%4 ? 8 : 30),-sign*(n%5 ? 5 : 24)},.pan_phase=GRID_PAN_MOMENTUM,.dt=dt});
            CHECK(fabsf(one.elastic.x)<=before_x+.00001f && fabsf(one.elastic.y)<=before_y+.00001f);
            CHECK(one.elastic.x*sign>=0 && one.elastic.y*sign>=0);
            before_x=fabsf(one.elastic.x); before_y=fabsf(one.elastic.y);
            if (n==26) CHECK(before_x*scale<.25f && before_y*scale<.25f);
        }
        CHECK(!one.elastic.x && !one.elastic.y && !one.spring_velocity.x && !one.spring_velocity.y);
        CHECK(one.top==top && !one.fraction && one.horizontal==horizontal && one.row==4 && one.col==2);
        grid_handle_input(&one,&u,&page,&(GridInput){.mouse=pointer,.pixel_scroll=true,.pixel_pan={-40*sign,-40*sign},.pan_phase=GRID_PAN_BEGIN,.dt=1.f/120});
        CHECK(one.elastic.x*sign>0 && one.elastic.y*sign>0 && one.pan_x.active && one.pan_y.active);
        float consume=fmaxf(fabsf(one.elastic.x),fabsf(one.elastic.y))*scale+20;
        grid_handle_input(&one,&u,&page,&(GridInput){.mouse=pointer,.pixel_scroll=true,.pixel_pan={consume*sign,consume*sign},.pan_phase=GRID_PAN_MOMENTUM});
        CHECK(!one.elastic.x && !one.elastic.y && !one.spring_velocity.x && !one.spring_velocity.y);
        CHECK(fabsf(one.horizontal-horizontal)>1 && fabs(((double)one.top+one.fraction-top)*24)>1);
        grid_destroy(&one); grid_destroy(&packets);
        /* A large single-axis160-point trackpad pull is only a small cue.
           The untouched axis stays fixed at both boundaries and zooms, and
           substantial momentum cannot prolong or re-open the cue. */
        for (int axis=0;axis<2;axis++) {
            Grid g; pointer=overscroll_setup(&g,&u,&page,scale,edge!=0);
            uint64_t start_top=g.top; float start_horizontal=g.horizontal;
            Vector2 pull=axis ? (Vector2){0,-160*sign} : (Vector2){-160*sign,0};
            grid_handle_input(&g,&u,&page,&(GridInput){.mouse=pointer,.pixel_scroll=true,.pixel_pan=pull,.pan_phase=GRID_PAN_BEGIN,.dt=1.f/120});
            float cue=(axis ? g.elastic.y : g.elastic.x)*scale;
            CHECK(fabsf(cue)<=32.001f && fabsf(cue)>=31.999f && cue*sign>0);
            CHECK(!(axis ? g.elastic.x : g.elastic.y));
            grid_handle_input(&g,&u,&page,&(GridInput){.mouse=pointer,.pixel_scroll=true,.pan_phase=GRID_PAN_RELEASE});
            for (int n=0;n<24;n++) {
                pull=axis ? (Vector2){0,-24*sign} : (Vector2){-24*sign,0};
                grid_handle_input(&g,&u,&page,&(GridInput){.mouse=pointer,.pixel_scroll=true,.pixel_pan=pull,.pan_phase=GRID_PAN_MOMENTUM,.dt=1.f/120});
                float next=(axis ? g.elastic.y : g.elastic.x)*scale;
                CHECK(fabsf(next)<=fabsf(cue)+.00001f && next*sign>=0);
                CHECK(!(axis ? g.elastic.x : g.elastic.y)); cue=next;
            }
            CHECK(fabsf(cue)<.25f && g.top==start_top && !g.fraction && g.horizontal==start_horizontal);
            grid_destroy(&g);
        }
    }
    /* No portable WheelEvent phase exists. A realistic declining tail and
       intermittent delivery trigger one return, never sawtooth outward jumps
       or repeated tiny bounces after rest. Keep X deliberately steady while Y
       decays to verify independent gesture ownership. */
    const float periods[]={1.f/60,1.f/120,1.f/90},uneven[]={.008f,.008f,.024f,.004f,.012f}; float returned_at[4]={0};
    for (int rate=0;rate<4;rate++) {
        UIContext u={0}; Grid g; Vector2 pointer=overscroll_setup(&g,&u,&page,1,false);
        float elapsed=0,before=0; bool returning=false; unsigned frame=0;
        while (elapsed<.6f) {
            float dt=rate==3 ? uneven[frame++%5] : periods[rate],tail=elapsed<.06f ? 1200 : 1200*expf(-24*(elapsed-.06f));
            /* Same 30ms delivery gap at every rate. Late coalesced tails
               cannot restart the released edge. */
            bool gap=elapsed>=.16f && elapsed<.19f;
            grid_handle_input(&g,&u,&page,&(GridInput){.mouse=pointer,.pixel_scroll=true,.pixel_pan={-800*dt,gap ? 0 : -tail*dt},.dt=dt});
            elapsed+=dt;
            if (g.pan_y.returning) {
                if (!returning) { returning=true; returned_at[rate]=elapsed; }
                else CHECK(g.elastic.y<=before+.00001f);
            }
            if (elapsed>.4f) CHECK(fabsf(g.elastic.y)<.25f);
            CHECK(g.elastic.x>0 && g.pan_x.active && !g.pan_x.returning);
            CHECK(!g.top && !g.fraction && !g.horizontal && g.row==4 && g.col==2);
            before=g.elastic.y;
        }
        CHECK(returning && !g.elastic.y);
        CHECK(returned_at[rate]>=.06f && returned_at[rate]<.14f);
        for (int n=0;n<24;n++) {
            grid_handle_input(&g,&u,&page,&(GridInput){.mouse=pointer,.pixel_scroll=true,.pixel_pan={-8,n%2 ? -.1f : .1f},.dt=.01f});
            CHECK(!g.elastic.y && g.pan_y.returning && !g.pan_y.active);
            CHECK(!g.top && !g.fraction && !g.horizontal);
        }
        grid_handle_input(&g,&u,&page,&(GridInput){.mouse=pointer,.dt=.15f});
        grid_handle_input(&g,&u,&page,&(GridInput){.mouse=pointer,.pixel_scroll=true,.pixel_pan={-40,-40},.dt=.01f});
        CHECK(g.pan_x.active && g.pan_y.active && !g.pan_x.returning && !g.pan_y.returning && g.elastic.y>0);
        grid_destroy(&g);
    }
    CHECK(fabsf(returned_at[0]-returned_at[1])<=1.f/60+.00001f && fabsf(returned_at[0]-returned_at[2])<=1.f/60+.00001f);
    CHECK(fabsf(returned_at[0]-returned_at[3])<=.02401f);
}
static void test_boundary_tail_noise(void) {
    Column columns[12]; for (size_t i=0;i<12;i++) columns[i]=(Column){.name="Field",.type=DUCKDB_TYPE_VARCHAR};
    Page page={.columns=columns,.column_count=12,.total=500};
    const float uneven[]={.004f,.012f,.024f,.008f,.016f};
    for (int native=0;native<2;native++) for (int edge=0;edge<2;edge++) for (int zoom=0;zoom<2;zoom++) for (int rate=0;rate<3;rate++) {
        float scale=zoom ? 2.5f : 1,sign=edge ? -1.f : 1.f;
        UIContext u={0}; Grid g; Vector2 pointer=overscroll_setup(&g,&u,&page,scale,edge!=0);
        uint64_t top=g.top; float horizontal=g.horizontal;
        GridPanPhase tail=native ? GRID_PAN_MOMENTUM : GRID_PAN_NONE;
        grid_handle_input(&g,&u,&page,&(GridInput){.mouse=pointer,.pixel_scroll=true,.pixel_pan={-160*sign,-160*sign},.pan_phase=native ? GRID_PAN_BEGIN : GRID_PAN_NONE,.dt=1.f/120});
        grid_handle_input(&g,&u,&page,&(GridInput){.mouse=pointer,.pixel_scroll=true,.pan_phase=native ? GRID_PAN_RELEASE : GRID_PAN_NONE,.dt=.04f});
        CHECK(g.pan_x.returning && g.pan_y.returning);
        float before_x=fabsf(g.elastic.x),before_y=fabsf(g.elastic.y),elapsed=0; unsigned tiny=0;
        for (int frame=0;frame<90;frame++) {
            float dt=rate==0 ? 1.f/60 : rate==1 ? 1.f/120 : uneven[frame%5];
            Vector2 pan={0}; bool gap=frame%17>=7 && frame%17<=10;
            if (!gap) {
                if (frame%13==0) pan=(Vector2){-64*sign,-48*sign};
                else { float noise=tiny++%2 ? -.6f : .6f; pan=(Vector2){noise*sign,-noise*sign}; }
            }
            grid_handle_input(&g,&u,&page,&(GridInput){.mouse=pointer,.pixel_scroll=true,.pixel_pan=pan,.pan_phase=tail,.dt=dt});
            if (frame%13==0) {
                /* Several large tail packets can be coalesced into one frame;
                   none may restart the returning paper. */
                for (int packet=0;packet<3;packet++) grid_handle_input(&g,&u,&page,&(GridInput){.mouse=pointer,.pixel_scroll=true,.pixel_pan={-24*sign,-32*sign},.pan_phase=tail});
            }
            elapsed+=dt;
            CHECK(g.top==top && !g.fraction && g.horizontal==horizontal);
            CHECK(fabsf(g.elastic.x)<=before_x+.00001f && fabsf(g.elastic.y)<=before_y+.00001f);
            CHECK(g.elastic.x*sign>=0 && g.elastic.y*sign>=0);
            CHECK(g.row==4 && g.col==2 && g.pan_x.returning && g.pan_y.returning);
            if (elapsed>.25f) CHECK(fabsf(g.elastic.x*scale)<.25f && fabsf(g.elastic.y*scale)<.25f);
            before_x=fabsf(g.elastic.x); before_y=fabsf(g.elastic.y);
        }
        CHECK(!g.elastic.x && !g.elastic.y);
        /* Deliberate2/3-point reversals remain immediate. Alternating packets
           that large are intentionally not claimed as distinguishable noise. */
        grid_handle_input(&g,&u,&page,&(GridInput){.mouse=pointer,.pixel_scroll=true,.pixel_pan={-.1f*sign,-.1f*sign},.pan_phase=tail});
        grid_handle_input(&g,&u,&page,&(GridInput){.mouse=pointer,.pixel_scroll=true,.pixel_pan={2*sign,3*sign},.pan_phase=tail});
        CHECK(fabsf((g.horizontal-horizontal)*scale-2*sign)<.005f);
        CHECK(fabs(((double)g.top+g.fraction-top)*24*scale-3*sign)<.001);
        grid_handle_input(&g,&u,&page,&(GridInput){.mouse=pointer,.pixel_scroll=true,.pixel_pan={.125f*sign,.125f*sign},.pan_phase=tail});
        CHECK(fabsf((g.horizontal-horizontal)*scale-2.125f*sign)<.005f);
        CHECK(fabs(((double)g.top+g.fraction-top)*24*scale-3.125f*sign)<.001);
        /* Pull/release again, then reverse with a coherent subpixel stream.
           Its total is retained across the one-point boundary hysteresis. */
        grid_handle_input(&g,&u,&page,&(GridInput){.mouse=pointer,.pixel_scroll=true,.pixel_pan={-180*sign,-180*sign},.pan_phase=native ? GRID_PAN_BEGIN : GRID_PAN_NONE,.dt=.15f});
        grid_handle_input(&g,&u,&page,&(GridInput){.mouse=pointer,.pixel_scroll=true,.pan_phase=native ? GRID_PAN_RELEASE : GRID_PAN_NONE,.dt=.04f});
        for (int frame=0;frame<20;frame++) grid_handle_input(&g,&u,&page,&(GridInput){.mouse=pointer,.pixel_scroll=true,.pixel_pan={-.1f*sign,-.1f*sign},.pan_phase=tail,.dt=.02f});
        CHECK(!g.elastic.x && !g.elastic.y && g.top==top && !g.fraction && g.horizontal==horizontal);
        for (int frame=0;frame<12;frame++) {
            grid_handle_input(&g,&u,&page,&(GridInput){.mouse=pointer,.pixel_scroll=true,.pixel_pan={.125f*sign,.125f*sign},.pan_phase=tail,.dt=.008f});
            if (frame<8) {
                if (g.top!=top || g.fraction || g.horizontal!=horizontal) fprintf(stderr,"Boundary stream native=%d edge=%d scale=%g rate=%d frame=%d x=%g row=%g returning=%d edge=%d pending=%g\n",native,edge,scale,rate,frame,(g.horizontal-horizontal)*scale,((double)g.top+g.fraction-top)*24*scale,g.pan_x.returning,g.pan_x.return_edge,g.pan_x.reversal);
                CHECK(g.top==top && !g.fraction && g.horizontal==horizontal);
            }
        }
        CHECK(fabsf((g.horizontal-horizontal)*scale-1.5f*sign)<.005f);
        CHECK(fabs(((double)g.top+g.fraction-top)*24*scale-1.5f*sign)<.001);
        grid_destroy(&g);
    }
}
int main(int argc,char **argv) {
    if (argc==2 && !strcmp(argv[1],"--column-benchmark")) return column_benchmark();
    if (argc==2 && !strcmp(argv[1],"--font-benchmark")) return font_benchmark();
    if (argc==2 && !strcmp(argv[1],"--overscroll")) { test_overscroll_return(); test_overscroll_gestures(); test_boundary_tail_noise(); printf("PASS: %u precise overscroll/return checks\n",checks); return 0; }
    test_overscroll_return(); test_overscroll_gestures(); test_boundary_tail_noise(); test_keytip_queue();
    test_animated_workspace_extent(); test_dock_scroll_and_clips(); test_ribbon_flyouts(); test_workspace_layout(); test_frozen_columns(); test_ribbon_spec(); test_grid_density(); test_bounded_preview(); test_carbon_assets(); test_status_geometry(); test_status_metrics(); test_ribbon_layout(); test_menu_bounds(); test_text_lines(); test_text_editing(); test_text_history(); test_region_navigation(); test_long_range_outlines(); test_addresses(); test_navigation_queue();
    CHECK(ui_chart_fraction(0,-DBL_MAX,DBL_MAX)==.5);
    CHECK(ui_chart_fraction(DBL_MAX,-DBL_MAX,DBL_MAX)==1);
    CHECK(ui_chart_fraction(-DBL_MAX,-DBL_MAX,DBL_MAX)==0);
    CHECK(fabs(ui_chart_fraction(DBL_MAX/2,-DBL_MAX,DBL_MAX)-.75)<1e-15);
    CHECK(ui_chart_fraction(5,0,10)==.5 && ui_chart_fraction(-1,0,10)==0 && ui_chart_fraction(11,0,10)==1);
    CHECK(ui_chart_fraction(0,0,0)==.5 && ui_chart_fraction(NAN,0,1)==.5);
    CHECK(ui_chart_fraction(nextafter(DBL_MAX,0),nextafter(DBL_MAX,0),DBL_MAX)==0);
    CHECK(ui_chart_fraction(1e-310,0,2e-310)==.5);
    { int size=0,codes[]={'N','U','L','"',' '}; unsigned char *data=LoadFileData("assets/fonts/JetBrainsMono-Regular.ttf",&size); CHECK(data && size>0);
      int count=0; GlyphInfo *marks=LoadFontData(data,size,20,codes,5,FONT_DEFAULT,&count); CHECK(marks && count==5 && marks[0].advanceX>0);
      for (int i=1;i<5;i++) CHECK(marks[i].advanceX==marks[0].advanceX);
      UnloadFontData(marks,count); UnloadFileData(data); }
    const char *faces[]={"assets/fonts/IBMPlexSans-Regular.otf","assets/fonts/IBMPlexSans-Medium.otf","assets/fonts/IBMPlexSans-SemiBold.otf"};
    for (size_t face=0;face<sizeof(faces)/sizeof(*faces);face++) {
      int size=0,codes[]={'0','1','2','3','4','5','6','7','8','9'}; unsigned char *data=LoadFileData(faces[face],&size); CHECK(data && size>0);
      const int rasters[]={9,11,18,22,26,28,33,39};
      for (size_t raster=0;raster<sizeof(rasters)/sizeof(*rasters);raster++) {
          int count=0; GlyphInfo *digits=LoadFontData(data,size,rasters[raster],codes,10,FONT_DEFAULT,&count); CHECK(digits && count==10 && digits[0].advanceX>0);
          for (int i=1;i<10;i++) CHECK(digits[i].advanceX==digits[0].advanceX);
          UnloadFontData(digits,count);
      }
      int required[]={0x130,0x131,0x11e,0x11f,0x15e,0x15f,0xc7,0xe7,0xd6,0xf6,0xdc,0xfc,0x20ac,0x20ba,0x20b9,0x2190,0x2191,0x2192,0x2193,0x2026,0x2212,0xd7,0xb7},found=0;
      GlyphInfo *coverage=LoadFontData(data,size,26,required,23,FONT_DEFAULT,&found); CHECK(coverage && found==23); UnloadFontData(coverage,found);
      UnloadFileData(data);
    }
    TextInput field={0}; input_set(&field,"İstanbul"); input_set(&field,field.text+2); CHECK(!strcmp(field.text,"stanbul"));
    field.select_all=false; input_insert(&field,field.text); CHECK(!strcmp(field.text,"stanbulstanbul") && field.length==14);
    field.select_all=true; input_insert(&field,field.text+7); CHECK(!strcmp(field.text,"stanbul") && field.length==7);
    input_set(&field,NULL); CHECK(!field.length && !*field.text); input_free(&field);
    field=(TextInput){0}; char *long_text=dx_alloc(70001); memset(long_text,'x',70000); input_set(&field,long_text); free(long_text);
    field.select_all=false; input_insert(&field,"y"); CHECK(field.length==70000);
    field.select_all=true; input_insert(&field,"replacement"); CHECK(!strcmp(field.text,"replacement")); input_free(&field);
    UIContext u={.scale=1}; Column cols[8]={0};
    char *names[]={"ID","City","Amount","Date","Approved","Note","Other","Final"};
    for (size_t i=0;i<8;i++) cols[i]=(Column){.name=names[i],.type_name="VARCHAR"};
    Page p={.columns=cols,.column_count=8,.total=1000000,.offset=0,.row_count=256};
    Grid g; grid_init(&g); grid_schema(&g,&p,true); Rectangle rect={150,100,850,600}; grid_layout(&g,1,&p,rect);
    CHECK(g.column_names[0] && !strcmp(g.column_names[0],"ID") && !g.column_names[8]);
    CHECK(g.visible_rows==20 && g.render_rows==20 && g.body.y==rect.y+48 && g.gutter==40); CHECK(grid_cached(&g,&p));
    g.top=100; g.fraction=.25; g.horizontal=220; grid_layout(&g,1,&p,rect);
    Vector2 corner={g.body.x,g.body.y}; grid_zoom_at(&g,&p,1,1.5f,corner);
    CHECK(g.top==100 && fabs(g.fraction-.25)<.00001 && fabsf(g.horizontal-220)<.001f);
    corner=(Vector2){g.body.x,g.body.y}; grid_zoom_at(&g,&p,1,1,corner);
    CHECK(g.top==100 && fabs(g.fraction-.25)<.00001 && fabsf(g.horizontal-220)<.001f);
    g.top=0; g.fraction=0; g.horizontal=0; grid_layout(&g,1,&p,rect);

    ui_note_pointer_gesture((DxPointerGesture){1,2,3,4,DX_MOD_SHIFT,false}); DxPointerGesture gesture;
    CHECK(ui_take_pointer_gesture(&gesture) && gesture.x0==1 && gesture.y1==4 && gesture.modifiers==DX_MOD_SHIFT);
    CHECK(!ui_take_pointer_gesture(&gesture));
    ui_note_pointer_gesture((DxPointerGesture){1,2,3,4,0,false}); ui_note_pointer_gesture((DxPointerGesture){20,30,40,50,0,true});
    CHECK(ui_gesture_starts_in((Rectangle){15,25,20,20})); CHECK(ui_gesture_starts_in((Rectangle){0,0,10,10}));
    CHECK(!ui_gesture_starts_in((Rectangle){40,45,20,20})); CHECK(!ui_gesture_starts_in((Rectangle){0,0,0,10}));
    CHECK(ui_take_gesture_from((Rectangle){15,25,20,20},&gesture) && gesture.right && gesture.x1==40);
    CHECK(!ui_gesture_starts_in((Rectangle){15,25,20,20}) && ui_gesture_starts_in((Rectangle){0,0,10,10}));
    CHECK(ui_take_pointer_gesture(&gesture) && !gesture.right && gesture.x0==1 && !ui_take_pointer_gesture(&gesture));
    ui_note_pointer_gesture((DxPointerGesture){1,2,3,4,0,true}); ui_note_pointer_gesture((DxPointerGesture){1,2,5,6,0,false});
    CHECK(ui_take_left_gesture_from((Rectangle){0,0,10,10},&gesture) && !gesture.right && gesture.x1==5);
    CHECK(!ui_take_left_gesture_from((Rectangle){0,0,10,10},&gesture));
    CHECK(ui_take_pointer_gesture(&gesture) && gesture.right && gesture.x1==3);
    ui_note_pointer_gesture((DxPointerGesture){1,2,5,6,0,false});
    CHECK(ui_take_click_from((Rectangle){0,0,10,10}) && !ui_take_pointer_gesture(&gesture));
    ui_note_pointer_gesture((DxPointerGesture){1,2,15,6,0,false});
    CHECK(!ui_take_click_from((Rectangle){0,0,10,10}) && !ui_take_pointer_gesture(&gesture));
    ui_note_pointer_gesture((DxPointerGesture){1,2,5,6,0,true});
    CHECK(!ui_take_click_from((Rectangle){0,0,10,10}) && ui_take_pointer_gesture(&gesture) && gesture.right);
    ui_note_pointer_gesture((DxPointerGesture){1,2,5,6,0,false});
    CHECK(!ui_take_click_from((Rectangle){0,0,0,10}) && ui_take_pointer_gesture(&gesture));
    grid_select_cell(&g,3,2,false); g.top=100; g.fraction=.25; g.horizontal=500;
    g.elastic=(Vector2){1,2}; g.spring_velocity=(Vector2){3,4}; grid_reveal_cell(&g,&p,1);
    CHECK(g.row==3 && g.col==2 && g.top==3 && !g.fraction && g.horizontal==g.offsets[2]);
    CHECK(!g.elastic.x && !g.elastic.y && !g.spring_velocity.x && !g.spring_velocity.y);
    grid_select_cell(&g,100,2,false); grid_reveal_cell(&g,&p,1);
    CHECK(g.top==100-g.visible_rows+1 && g.row==100);
    float horizontal=g.horizontal; grid_reveal_cell(&g,&p,0); CHECK(g.horizontal==horizontal);
    grid_select_cell(&g,0,0,false); g.top=0; g.horizontal=0;

    grid_replay_gesture(&g,&u,&p,(DxPointerGesture){g.body.x+10,g.body.y+10,g.body.x+g.widths[0]+10,g.body.y+60,0,false});
    CHECK(!g.anchor_col && g.end_col==1 && !g.anchor_row && g.end_row==2 && !g.dragging);
    g.allow_move=true; grid_select_cell(&g,3,2,false);
    Rectangle selected=grid_range_rect(&g,1,3,2,1,1);
    grid_replay_gesture(&g,&u,&p,(DxPointerGesture){selected.x+10,selected.y+1,selected.x+g.widths[2]+10,selected.y+55,0,false});
    CHECK(g.move_requested && g.move_dr==2 && g.move_dc==1 && !g.moving);
    g.move_requested=false; g.allow_move=false; grid_select_cell(&g,0,0,false);

    grid_replay_gesture(&g,&u,&p,(DxPointerGesture){g.rect.x+3,g.body.y+30,g.rect.x+3,g.body.y+30,0,true});
    CHECK(g.context_requested && g.selection==SELECT_ROWS && g.row==1 && g.anchor_row==1 && g.end_row==1);
    g.context_requested=false; g.end_row=3;
    grid_replay_gesture(&g,&u,&p,(DxPointerGesture){g.rect.x+3,g.body.y+60,g.rect.x+3,g.body.y+60,0,true});
    CHECK(g.context_requested && g.row==1 && g.anchor_row==1 && g.end_row==3);
    g.context_requested=false; g.lock_structure=true;
    grid_replay_gesture(&g,&u,&p,(DxPointerGesture){g.rect.x+3,g.body.y+90,g.rect.x+3,g.body.y+90,0,true});
    CHECK(!g.context_requested); g.lock_structure=false; grid_select_cell(&g,0,0,false);

    /* Keyboard modifiers belong to their queued press, independently of pointer state. */
    ui_note_key_modifiers(KEY_DOWN,DX_MOD_SHIFT); ui_note_key_modifiers(KEY_RIGHT,0);
    CHECK(ui_key_shift(KEY_DOWN) && !ui_key_shift(KEY_RIGHT));
    ui_note_key_modifiers(KEY_DOWN,0); CHECK(!ui_key_shift(KEY_DOWN));
    ui_note_key_modifiers(KEY_A,DX_MOD_SUPER|DX_MOD_ALT); CHECK(ui_key_alt(KEY_A) && !ui_key_control(KEY_A));
#ifdef __APPLE__
    CHECK(ui_key_shortcut(KEY_A));
#else
    CHECK(!ui_key_shortcut(KEY_A));
#endif
    ui_note_key_modifiers(KEY_A,DX_MOD_CONTROL); CHECK(ui_key_control(KEY_A) && !ui_key_alt(KEY_A));
#ifdef __APPLE__
    CHECK(!ui_key_shortcut(KEY_A));
#else
    CHECK(ui_key_shortcut(KEY_A));
#endif
    ui_note_key_modifiers(-1,15); ui_note_key_modifiers(512,15);
    ui_note_key_modifiers(KEY_A,0); CHECK(!ui_key_shortcut(KEY_A));
    grid_select_cell(&g,3,2,false);
    event(&g,&u,&p,(GridInput){.key=KEY_DOWN,.separate_keyboard=true,.key_shift=true});
    CHECK(g.row==3 && g.anchor_row==3 && g.end_row==4 && g.col==2);
    event(&g,&u,&p,(GridInput){.key=KEY_RIGHT,.shift=true,.separate_keyboard=true});
    CHECK(g.row==3 && g.col==3 && g.anchor_row==3 && g.anchor_col==3);
    event(&g,&u,&p,(GridInput){.key=KEY_KP_ENTER,.separate_keyboard=true,.key_shift=true});
    CHECK(g.row==2 && g.col==3 && g.anchor_row==2);
    grid_select_cell(&g,0,0,false);

    Page unknown=p; unknown.count_pending=true; unknown.total=256;
    CHECK(grid_cached(&g,&unknown)); g.top=250; CHECK(!grid_cached(&g,&unknown)); g.top=256; CHECK(!grid_cached(&g,&unknown));
    unknown.offset=500; unknown.row_count=0; unknown.total=0; g.top=510; CHECK(grid_cached(&g,&unknown));
    g.top=100; CHECK(!grid_cached(&g,&unknown)); g.top=0;
    for (int i=0;i<40;i++) event(&g,&u,&p,(GridInput){.mouse={400,300},.wheel={0,-.01f}});
    CHECK(g.top==1 && fabs(g.fraction-.2)<.00001);
    /* Preserve pixel tails, both axes and equal screen-space speed at zoom. */
    uint64_t pan_active_row=g.row; size_t pan_active_col=g.col;
    for (int scale=1;scale<=3;scale++) {
        g.top=10; g.fraction=0; g.horizontal=100; u.scale=(float)scale;
        grid_layout(&g,u.scale,&p,rect);
        Vector2 pointer={g.body.x+50,g.body.y+50};
        for (int i=0;i<40;i++) event(&g,&u,&p,(GridInput){.mouse=pointer,.pixel_scroll=true,.pixel_pan={.125f,.25f},.wheel={99,99},.shift=true});
        CHECK(fabs(((double)g.top+g.fraction-10)*(g.row_height>=18 ? g.row_height : 27)*scale-10)<.00001);
        CHECK(fabs((g.horizontal-100)*scale-5)<.001);
        CHECK(g.row==pan_active_row && g.col==pan_active_col);
        g.block_pointer=true;
        event(&g,&u,&p,(GridInput){.mouse=pointer,.pixel_scroll=true,.pixel_pan={100,100}});
        CHECK(fabs((g.horizontal-100)*scale-5)<.001); g.block_pointer=false;
        for (int i=0;i<40;i++) event(&g,&u,&p,(GridInput){.mouse=pointer,.pixel_scroll=true,.pixel_pan={-.125f,-.25f}});
        CHECK(fabs((double)g.top+g.fraction-10)<.00001 && fabs(g.horizontal-100)<.001);
    }
    u.scale=1; g.top=1; g.fraction=.2; g.horizontal=0; grid_layout(&g,1,&p,rect);
    grid_select_cell(&g,3,1,false);
    event(&g,&u,&p,(GridInput){.key=KEY_RIGHT,.shift=true}); event(&g,&u,&p,(GridInput){.key=KEY_DOWN,.shift=true});
    uint64_t r0,r1; size_t c0,c1; grid_bounds(&g,&r0,&r1,&c0,&c1);
    CHECK(r0==3 && r1==4 && c0==1 && c1==2); CHECK(g.row==3 && g.col==1);
    /* Tab cycles from the original active cell through the retained range. */
    event(&g,&u,&p,(GridInput){.key=KEY_TAB}); CHECK(g.row==3 && g.col==2);
    grid_bounds(&g,&r0,&r1,&c0,&c1); CHECK(r1==4 && c1==2);
    event(&g,&u,&p,(GridInput){.key=KEY_ENTER}); CHECK(g.row==4 && g.col==2);
    event(&g,&u,&p,(GridInput){.key=KEY_ENTER,.shift=true}); CHECK(g.row==3 && g.col==2);
    grid_select_cell(&g,100,2,false); g.top=100; g.fraction=0; grid_layout(&g,1,&p,rect);
    event(&g,&u,&p,(GridInput){.mouse={g.body.x+40,g.rect.y+10},.pressed=true,.down=true});
    event(&g,&u,&p,(GridInput){.released=true});
    CHECK(g.selection==SELECT_COLUMNS && g.row==100 && g.col==0);
    grid_bounds(&g,&r0,&r1,&c0,&c1); CHECK(r0==0 && r1==UINT64_MAX && c0==0 && c1==0);
    event(&g,&u,&p,(GridInput){.key=KEY_DOWN}); CHECK(g.row==101 && g.selection==SELECT_CELLS);
    event(&g,&u,&p,(GridInput){.key=KEY_A,.shortcut=true}); CHECK(g.row==101 && g.col==0 && g.selection==SELECT_CELLS && g.end_row==999999 && g.end_col==7);
    event(&g,&u,&p,(GridInput){.key=KEY_A,.shortcut=true}); CHECK(g.selection==SELECT_ALL);
    event(&g,&u,&p,(GridInput){.key=KEY_HOME,.shortcut=true}); CHECK(g.row==0 && g.col==0);
    event(&g,&u,&p,(GridInput){.key=KEY_END,.shortcut=true}); CHECK(g.row==999999 && g.col==7);
    event(&g,&u,&p,(GridInput){.key=KEY_PAGE_UP}); CHECK(g.row==999979);
    g.top=500000; g.fraction=0; grid_layout(&g,1,&p,rect);
    Vector2 grab={g.vthumb.x+3,g.vthumb.y+7};
    event(&g,&u,&p,(GridInput){.mouse=grab,.pressed=true,.down=true});
    CHECK(llabs((long long)g.top-500000)<=1); CHECK(g.scrollbar==1);
    event(&g,&u,&p,(GridInput){.mouse={grab.x,grab.y+60},.down=true}); CHECK(g.top>500000);
    event(&g,&u,&p,(GridInput){.released=true}); CHECK(g.scrollbar==0);
    g.horizontal=0; g.top=0; g.fraction=0; grid_layout(&g,1,&p,rect);
    float edge=g.body.x+g.widths[0],before=g.widths[0];
    event(&g,&u,&p,(GridInput){.mouse={edge,g.rect.y+25},.pressed=true,.down=true,.dt=1});
    CHECK(g.resizing==0);
    event(&g,&u,&p,(GridInput){.mouse={edge+100,g.rect.y+25},.down=true}); CHECK(fabsf(g.widths[0]-before-100)<.001f);
    CHECK(fabsf(g.offsets[1]-g.widths[0])<.001f); event(&g,&u,&p,(GridInput){.released=true});
    uint64_t widths_revision=g.width_revision; grid_set_width(&g,0,g.widths[0]); CHECK(g.width_revision==widths_revision);
    grid_layout(&g,1,&p,rect); edge=g.body.x+g.widths[0]; before=g.widths[0];
    event(&g,&u,&p,(GridInput){.mouse={edge,g.rect.y+25},.pressed=true,.down=true,.dt=1});
    event(&g,&u,&p,(GridInput){.mouse={edge+80,g.rect.y+25},.down=true}); CHECK(g.widths[0]==before+80);
    event(&g,&u,&p,(GridInput){.key=KEY_ESCAPE,.mouse={edge+80,g.rect.y+25},.down=true}); CHECK(g.resizing==-1 && g.widths[0]==before);
    CHECK(g.offsets[1]==g.widths[0]); event(&g,&u,&p,(GridInput){.released=true});
    grid_select_cell(&g,0,0,false); g.end_row=2; g.end_col=1;
    event(&g,&u,&p,(GridInput){.right=true,.mouse={g.body.x+20,g.body.y+30}});
    CHECK(g.context_requested && !g.anchor_row && g.end_row==2 && g.end_col==1); g.context_requested=false;
    event(&g,&u,&p,(GridInput){.key=KEY_F10,.separate_keyboard=true,.key_shift=true}); CHECK(g.context_requested); g.context_requested=false;
    event(&g,&u,&p,(GridInput){.right=true,.mouse={g.body.x+20,g.rect.y+15}});
    CHECK(g.context_requested && g.selection==SELECT_COLUMNS && !g.col); g.context_requested=false;

    u.scale=1.5f; grid_layout(&g,u.scale,&p,rect);
    event(&g,&u,&p,(GridInput){.mouse={g.body.x+g.widths[0]*1.5f+25,g.body.y+10},.pressed=true,.down=true});
    CHECK(g.col==1 && g.row==0); event(&g,&u,&p,(GridInput){.released=true});
    float old=g.widths[1]; event(&g,&u,&p,(GridInput){.key=KEY_RIGHT,.alt=true}); CHECK(g.widths[1]==old+12);
    g.top=0; g.fraction=0; grid_layout(&g,u.scale,&p,rect);
    event(&g,&u,&p,(GridInput){.mouse={g.body.x+20,g.body.y+20},.pressed=true,.down=true});
    event(&g,&u,&p,(GridInput){.mouse={g.body.x+200,g.body.y+g.body.height+100},.down=true,.dt=.1f});
    CHECK(g.top>0 && g.end_row>0); event(&g,&u,&p,(GridInput){.released=true});
    /* Empty relations, zero-size windows and schema replacement must remain safe. */
    p.total=0; grid_select_cell(&g,100,10,false); grid_layout(&g,u.scale,&p,(Rectangle){0,0,1,1});
    event(&g,&u,&p,(GridInput){.key=KEY_DOWN}); grid_schema(&g,&p,false); CHECK(g.row==101 && g.col==10);
    p.total=100; cols[0].name="Changed schema"; grid_schema(&g,&p,false); CHECK(g.widths[0]==128);
    u.scale=1; grid_layout(&g,1,&p,rect);
    g.filter_requested=false;
    event(&g,&u,&p,(GridInput){.mouse={g.body.x+g.widths[0]-12,g.rect.y+38},.pressed=true,.down=true});
    CHECK(g.filter_requested && g.col==0 && g.anchor_col==0 && g.end_col==0 && !g.dragging);
    g.filter_requested=false; event(&g,&u,&p,(GridInput){.released=true});
    event(&g,&u,&p,(GridInput){.key=KEY_DOWN,.alt=true}); CHECK(g.filter_requested);
    grid_select_cell(&g,10,51,false); event(&g,&u,&p,(GridInput){.key=KEY_RIGHT});
    CHECK(g.width_count==104 && g.col==52 && g.offsets[104]>g.offsets[103]);
    grid_set_width(&g,80,177); grid_schema(&g,&p,false); CHECK(g.width_count==104 && g.widths[80]==177);
    g.top=500; grid_layout(&g,1,&p,rect); CHECK(grid_cached(&g,&p));
    g.top=0; g.horizontal=g.offsets[20]; grid_layout(&g,1,&p,rect); CHECK(grid_cached(&g,&p));
    Page blank={0}; grid_schema(&g,&blank,true); grid_layout(&g,1,&blank,rect);
    CHECK(g.body.y==rect.y+24 && grid_cached(&g,&blank));
    CHECK(g.vthumb.height>26); /* A new workbook does not map the thumb to a million empty rows. */
    g.allow_move=true; grid_select_cell(&g,1,1,false); grid_select_cell(&g,2,2,true);
    Vector2 border={g.body.x+g.offsets[1]+30,g.body.y+27};
    event(&g,&u,&blank,(GridInput){.mouse=border,.pressed=true,.down=true}); CHECK(g.moving && !g.dragging);
    Vector2 target={border.x+g.widths[1],border.y+54};
    event(&g,&u,&blank,(GridInput){.mouse=target,.down=true}); CHECK(g.move_dr==2 && g.move_dc==1);
    event(&g,&u,&blank,(GridInput){.mouse=target,.released=true}); CHECK(!g.moving && g.move_requested);
    grid_bounds(&g,&r0,&r1,&c0,&c1); CHECK(r0==1 && r1==2 && c0==1 && c1==2);
    event(&g,&u,&blank,(GridInput){.mouse={g.body.x+10,g.body.y+100},.right=true});
    CHECK(g.context_requested && g.row==3 && g.col==0 && g.anchor_row==3 && g.anchor_col==0);
    grid_ensure_columns(&g,16384); CHECK(g.width_count==16384);
    CHECK(!g.column_names[16383] && g.offsets[16384]>g.offsets[16383]);
    grid_select_cell(&g,DX_SHEET_ROWS-1,16383,false); event(&g,&u,&blank,(GridInput){.key=KEY_DOWN});
    event(&g,&u,&blank,(GridInput){.key=KEY_RIGHT}); CHECK(g.row==DX_SHEET_ROWS-1 && g.col==16383);
    grid_select_cell(&g,100,10,false); g.top=90; g.horizontal=800; grid_layout(&g,1,&blank,rect);
    Vector2 pointer={g.body.x+200,g.body.y+150};
    double row_before=(double)g.top+g.fraction+(pointer.y-g.body.y)/27;
    float x_before=g.horizontal+pointer.x-g.body.x;
    grid_zoom_at(&g,&blank,1,1.5f,pointer);
    CHECK(fabs(((double)g.top+g.fraction+(pointer.y-g.body.y)/40.5)-row_before)<.001);
    CHECK(fabsf(g.horizontal+(pointer.x-g.body.x)/1.5f-x_before)<.001f);
    grid_schema(&g,&p,true); CHECK(g.zoom==1.5f);
    grid_schema(&g,&blank,true); u.scale=1.5f; grid_layout(&g,u.scale,&blank,rect); g.allow_move=false;
    Vector2 over={g.body.x+200,g.body.y+180}; unsigned render_rows=g.render_rows;
    event(&g,&u,&blank,(GridInput){.mouse=over,.wheel={1,1},.dt=1.f/60});
    CHECK(g.top==0 && g.fraction==0 && g.horizontal==0 && g.elastic.x>0 && g.elastic.y>0);
    CHECK(g.render_rows==render_rows && grid_cached(&g,&blank));
    Rectangle cell=grid_range_rect(&g,u.scale,2,1,1,1); uint64_t hit_row; size_t hit_col;
    CHECK(grid_cell_at(&g,u.scale,(Vector2){cell.x+10,cell.y+10},&hit_row,&hit_col) && hit_row==2 && hit_col==1);
    CHECK(!grid_cell_at(&g,u.scale,(Vector2){g.body.x+1,g.body.y+1},&hit_row,&hit_col));
    event(&g,&u,&blank,(GridInput){.mouse={cell.x+10,cell.y+10},.pressed=true,.down=true}); CHECK(g.row==2 && g.col==1);
    event(&g,&u,&blank,(GridInput){.released=true});
    for (int i=0;i<90;i++) event(&g,&u,&blank,(GridInput){.dt=1.f/60});
    CHECK(g.elastic.x==0 && g.elastic.y==0 && g.top==0 && g.horizontal==0);
    g.elastic_enabled=false; event(&g,&u,&blank,(GridInput){.mouse=over,.wheel={1,1},.dt=1.f/60}); CHECK(g.elastic.x==0 && g.elastic.y==0);
    g.elastic_enabled=true; grid_ensure_columns(&g,16384); grid_layout(&g,u.scale,&blank,rect);
    g.top=DX_SHEET_ROWS-g.visible_rows; grid_layout(&g,u.scale,&blank,rect);
    g.horizontal=g.offsets[16384]-g.body.width/u.scale; grid_layout(&g,u.scale,&blank,rect);
    uint64_t bottom=g.top; float right=g.horizontal;
    event(&g,&u,&blank,(GridInput){.mouse=over,.wheel={-1,-1},.dt=1.f/60});
    CHECK(g.top==bottom && g.horizontal==right && g.elastic.x<0 && g.elastic.y<0);
    event(&g,&u,&blank,(GridInput){.key=KEY_HOME}); CHECK(g.elastic.x==0 && g.elastic.y==0);
    grid_schema(&g,&blank,true); u.scale=1; grid_ensure_columns(&g,104); g.used_rows=4000005; g.used_columns=16; grid_layout(&g,1,&blank,rect);
    event(&g,&u,&blank,(GridInput){.key=KEY_END,.shortcut=true}); CHECK(g.row==4000004 && g.col==15 && g.top>3900000);
    grid_layout(&g,u.scale,&blank,rect); CHECK(g.gutter>=65 && grid_cached(&g,&blank));
    Rectangle deep=grid_range_rect(&g,u.scale,g.top+2,15,1,1);
    CHECK(grid_cell_at(&g,u.scale,(Vector2){deep.x+5,deep.y+5},&hit_row,&hit_col) && hit_row==g.top+2 && hit_col==15);
    for (int scale=0;scale<3;scale++) {
        u.scale=scale==0 ? .8f : scale==1 ? 1 : 1.5f; grid_layout(&g,u.scale,&blank,rect);
        size_t column=g.first_col+1; deep=grid_range_rect(&g,u.scale,g.top+2,column,1,1);
        CHECK(grid_cell_at(&g,u.scale,(Vector2){deep.x+5,deep.y+5},&hit_row,&hit_col) && hit_row==g.top+2 && hit_col==column);
    }
    grid_select_cell(&g,1048575,0,false); g.top=1048560; g.used_rows=0; g.used_columns=0; u.scale=1; grid_layout(&g,1,&blank,rect);
    event(&g,&u,&blank,(GridInput){.key=KEY_DOWN}); CHECK(g.row==1048576);
    Vector2 thumb={g.vthumb.x+2,g.vthumb.y+g.vthumb.height/2};
    event(&g,&u,&blank,(GridInput){.mouse=thumb,.pressed=true,.down=true}); CHECK(g.scrollbar==1 && g.scroll_rows>=1048576);
    uint64_t extent=g.scroll_rows;
    Vector2 end={thumb.x,g.vtrack.y+g.vtrack.height}; event(&g,&u,&blank,(GridInput){.mouse=end,.down=true}); uint64_t at=g.top;
    for (int i=0;i<8;i++) event(&g,&u,&blank,(GridInput){.mouse=end,.down=true});
    CHECK(g.scroll_rows==extent && g.top==at); event(&g,&u,&blank,(GridInput){.released=true}); CHECK(!g.scrollbar && !g.scroll_rows && g.top==at);
    grid_schema(&g,&p,true); g.source_header=true; grid_layout(&g,1,&p,rect); g.lock_structure=true;
    float width=g.widths[0]; Vector2 resize_point={g.body.x+width,g.rect.y+12};
    event(&g,&u,&p,(GridInput){.mouse=resize_point,.pressed=true,.down=true}); CHECK(g.resizing==-1);
    event(&g,&u,&p,(GridInput){.key=KEY_RIGHT,.alt=true}); CHECK(g.widths[0]==width);
    event(&g,&u,&p,(GridInput){.key=KEY_DOWN,.alt=true}); CHECK(!g.filter_requested);
    event(&g,&u,&p,(GridInput){.mouse={g.body.x+width-12,g.rect.y+39},.pressed=true,.down=true}); CHECK(!g.filter_requested);
    event(&g,&u,&p,(GridInput){.mouse={g.body.x+10,g.body.y+10},.right=true}); CHECK(!g.context_requested);
    g.allow_move=true; grid_select_cell(&g,0,0,false); grid_select_cell(&g,1,1,true);
    event(&g,&u,&p,(GridInput){.mouse={g.body.x+20,g.body.y},.pressed=true,.down=true}); CHECK(!g.moving);
    event(&g,&u,&p,(GridInput){.released=true}); event(&g,&u,&p,(GridInput){.mouse={g.body.x+100,g.body.y+100},.wheel={0,-1}}); CHECK(g.top>0);
    g.lock_structure=false; g.block_pointer=true; grid_select_cell(&g,0,0,false); g.top=0; grid_layout(&g,1,&p,rect);
    ui_note_pointer_gesture((DxPointerGesture){900,500,900,500,0,false});
    grid_update(&g,&u,&p,rect,true); DxPointerGesture overlay;
    CHECK(ui_take_pointer_gesture(&overlay) && overlay.x0==900 && overlay.y0==500);
    event(&g,&u,&p,(GridInput){.mouse={g.body.x+100,g.body.y+100},.pressed=true,.down=true,.wheel={0,-4}});
    CHECK(g.row==0 && g.col==0 && g.top==0 && !g.dragging && !g.moving);
    event(&g,&u,&p,(GridInput){.key=KEY_DOWN}); CHECK(g.row==1);
    event(&g,&u,&p,(GridInput){.mouse={g.body.x+10,g.body.y+10},.right=true}); CHECK(!g.context_requested);
    g.block_pointer=false; g.allow_move=false;
    float chrome_x=rect.x+rect.width+10;
    ui_note_pointer_gesture((DxPointerGesture){chrome_x,500,chrome_x,500,0,false});
    grid_update(&g,&u,&p,rect,true);
    CHECK(ui_take_pointer_gesture(&overlay) && overlay.x0==chrome_x && overlay.y0==500);
    event(&g,&u,&p,(GridInput){.mouse={g.body.x+10,g.body.y+10},.pressed=true,.down=true}); CHECK(g.dragging);
    g.block_pointer=true; event(&g,&u,&p,(GridInput){.mouse={g.body.x+130,g.body.y+60},.down=true}); CHECK(g.dragging && g.end_row>=2 && g.end_col>=1);
    event(&g,&u,&p,(GridInput){.released=true}); CHECK(!g.dragging);
    grid_ensure_columns(&g,104); grid_select_cell(&g,7,2,false); grid_select_cell(&g,10,4,true); g.top=3; g.fraction=.25;
    uint64_t active_row=g.row,anchor=g.anchor_row,end_row=g.end_row; size_t active_col=g.col,end_col=g.end_col;
    grid_layout(&g,1,&p,rect); grid_reveal_column(&g,&p,1,70,false);
    CHECK(g.horizontal==g.offsets[70] && g.row==active_row && g.col==active_col && g.anchor_row==anchor && g.end_row==end_row && g.end_col==end_col && g.top==3 && g.fraction==.25);
    float revealed=g.horizontal; grid_reveal_column(&g,&p,1,71,false); CHECK(g.horizontal==revealed);
    grid_reveal_column(&g,&p,1,70,true); CHECK(g.horizontal<revealed && g.horizontal+g.body.width/2>g.offsets[70]);
    for (int scale=0;scale<3;scale++) {
        float factor=scale==0 ? .8f : scale==1 ? 1 : 1.5f; grid_layout(&g,factor,&p,rect); grid_reveal_column(&g,&p,factor,103,false);
        CHECK(g.horizontal>=0 && g.horizontal<=g.offsets[g.width_count]-g.body.width/factor+.01f && g.row==active_row && g.col==active_col && g.top==3);
    }
    Column *wide=dx_alloc(16384*sizeof(*wide)); for (size_t i=0;i<16384;i++) wide[i]=(Column){.name="Wide column",.type=DUCKDB_TYPE_VARCHAR};
    Page wide_page={.column_count=16384,.columns=wide}; Grid wide_grid; grid_init(&wide_grid); grid_schema(&wide_grid,&wide_page,true);
    CHECK(wide_grid.width_count==16384 && wide_grid.data_columns==16384);
    grid_set_width(&wide_grid,16383,240); float *widths=wide_grid.widths; grid_schema(&wide_grid,&wide_page,false);
    CHECK(wide_grid.widths==widths && wide_grid.widths[16383]==240);
    grid_ensure_columns(&wide_grid,20000); CHECK(wide_grid.width_count==16384); grid_destroy(&wide_grid); free(wide);
    char formatted[64];
    CHECK(ui_format_float(1574460.870000001,10,formatted) && !strcmp(formatted,"1574460.87"));
    CHECK(ui_format_float(-15484.970000000003,10,formatted) && !strcmp(formatted,"-15484.97"));
    CHECK(ui_format_float(3.198079666666667,10,formatted) && !strcmp(formatted,"3.198079667"));
    CHECK(ui_format_float(1.2345678901234567,15,formatted) && !strcmp(formatted,"1.23456789012346"));
    CHECK(ui_format_float(1e300,10,formatted) && !strcmp(formatted,"1e+300"));
    CHECK(ui_format_float(-0.0,10,formatted) && !strcmp(formatted,"-0"));
    CHECK(!ui_format_float(INFINITY,10,formatted) && !ui_format_float(NAN,10,formatted));
    CHECK(!ui_format_float(1,0,formatted) && !ui_format_float(1,16,formatted));
    double remainder=0; int steps=0;
    for (int i=0;i<100;i++) steps+=ui_scroll_steps(&remainder,.09);
    CHECK(steps==9 && fabs(remainder)<.000001);
    for (int i=0;i<100;i++) steps+=ui_scroll_steps(&remainder,-.09);
    CHECK(steps==0 && fabs(remainder)<.000001);
    CHECK(ui_scroll_steps(&remainder,.2)==0 && ui_scroll_steps(&remainder,.2)==0 && ui_scroll_steps(&remainder,-.4)==0 && fabs(remainder)<.000001);
    CHECK(ui_scroll_steps(&remainder,.75)==0 && ui_scroll_steps(&remainder,.75)==1 && fabs(remainder-.5)<.000001);
    CHECK(ui_scroll_steps(&remainder,-.5)==0 && remainder==0);
    CHECK(ui_scroll_steps(&remainder,1e100)==128 && remainder==0);
    CHECK(ui_scroll_steps(&remainder,-1e100)==-128 && remainder==0);
    CHECK(ui_scroll_steps(&remainder,NAN)==0 && remainder==0);
    remainder=NAN; CHECK(ui_scroll_steps(&remainder,1)==0 && remainder==0);
    /* Actual retained button feedback, including interrupted exit and snapping. */
    Color base_feedback[]={WHITE,WHITE,WHITE},feedback_colors[]={RED,BLUE,BLACK}; Rectangle feedback_rect={600,400,50,25};
    u.paint_time=1; ui_feedback_colors(&u,feedback_rect,77,base_feedback,feedback_colors); CHECK(feedback_colors[0].r==255 && feedback_colors[0].g==255);
    u.paint_time=1.045; feedback_colors[0]=RED; feedback_colors[1]=BLUE; feedback_colors[2]=BLACK; ui_feedback_colors(&u,feedback_rect,77,base_feedback,feedback_colors);
    CHECK(feedback_colors[0].g>RED.g && feedback_colors[0].g<255 && feedback_colors[1].b>BLUE.b && feedback_colors[1].b<255);
    u.paint_time=1.091; feedback_colors[0]=RED; feedback_colors[1]=BLUE; feedback_colors[2]=BLACK; ui_feedback_colors(&u,feedback_rect,77,base_feedback,feedback_colors); CHECK(!memcmp(feedback_colors,(Color[]){RED,BLUE,BLACK},sizeof(feedback_colors)));
    feedback_colors[0]=WHITE; feedback_colors[1]=WHITE; feedback_colors[2]=WHITE; ui_feedback_colors(&u,feedback_rect,77,base_feedback,feedback_colors); CHECK(feedback_colors[0].g==RED.g);
    u.paint_time=1.12; feedback_colors[0]=WHITE; feedback_colors[1]=WHITE; feedback_colors[2]=WHITE; ui_feedback_colors(&u,feedback_rect,77,base_feedback,feedback_colors); CHECK(feedback_colors[0].g>RED.g && feedback_colors[0].g<255);
    u.reduced_motion=true; feedback_colors[0]=WHITE; feedback_colors[1]=WHITE; feedback_colors[2]=WHITE; ui_feedback_colors(&u,feedback_rect,77,base_feedback,feedback_colors); CHECK(!memcmp(feedback_colors,base_feedback,sizeof(feedback_colors))); u.reduced_motion=false;
    /* A moving popup maps queued physical gestures without stealing a canvas click. */
    ui_pointer_offset((Vector2){0,-3});
    ui_note_pointer_gesture((DxPointerGesture){.x0=25,.y0=48,.x1=26,.y1=49});
    CHECK(ui_gesture_starts_in((Rectangle){20,50,20,20}));
    DxPointerGesture moved_popup={0};
    CHECK(ui_take_left_gesture_from((Rectangle){20,50,20,20},&moved_popup));
    CHECK(moved_popup.y0==51 && moved_popup.y1==52 && moved_popup.x0==25);
    ui_pointer_offset((Vector2){0});
    ui_note_pointer_gesture((DxPointerGesture){.x0=25,.y0=48,.x1=26,.y1=49});
    CHECK(!ui_take_left_gesture_from((Rectangle){20,50,20,20},&moved_popup));
    CHECK(ui_take_pointer_gesture(&moved_popup) && moved_popup.y0==48);
    test_focus_modality();
    grid_destroy(&g); puts("PASS: grid input, geometry, selection, fractional scrolling, thumb dragging, resizing, zoom and empty relations");
    printf("%u grid checks\n",checks); return 0;
}
