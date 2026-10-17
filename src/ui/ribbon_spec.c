#include "ribbon_spec.h"
#include <math.h>
const char *const ui_ribbon_tabs[]={"Home","Insert","Data","Analyze","View","Visualization"};
const int ribbon_counts[]={24,5,8,11,12,11},ribbon_group_counts[]={6,2,3,4,4,4};
#define C(label,icon,action) {label,CARBON_##icon,ACTION_##action}
#define VISUALS C("Edit analysis",DATA_VIEW,CREATE_VIEW),C("Matrix",TABLE,MATRIX),C("Bar chart",CHART_BAR,BAR),C("Line chart",CHART_LINE,LINE),C("Add to Rows",ROW,ADD_ROWS),C("Add to Columns",COLUMN,ADD_COLUMNS),C("Swap shelves",ARROWS_HORIZONTAL,SWAP_SHELVES),C("Add measure",FUNCTION,ADD_MEASURE),C("Transformations",DATA_STRUCTURED,TRANSFORM),C("Inspect SQL",SQL,SQL),C("Browse data",TABLE,BROWSE)
const UIRibbonCommand ribbon_commands[RIBBON_TABS][UI_RIBBON_COMMANDS]={
    {C("Copy",COPY,COPY),C("Paste",PASTE,PASTE),C("Cut",CUT,CUT),C("IBM Plex Sans",STRING_TEXT,FONT),C("13",STRING_TEXT,FONT_SIZE),C("Bold",TEXT_BOLD,BOLD),C("Italic",TEXT_ITALIC,ITALIC),C("Underline",TEXT_UNDERLINE,UNDERLINE),C("Text color",TEXT_COLOR,TEXT_COLOR),C("Align left",TEXT_ALIGN_LEFT,ALIGN_LEFT),C("Align center",TEXT_ALIGN_CENTER,ALIGN_CENTER),C("Align right",TEXT_ALIGN_RIGHT,ALIGN_RIGHT),C("Wrap text",TEXT_WRAP,WRAP),C("Accounting",CURRENCY_DOLLAR,NUMBER_FORMAT),C("Accounting format",CURRENCY_DOLLAR,ACCOUNTING),C("Decrease decimal places",ARROW_LEFT,DECIMAL_LESS),C("Increase decimal places",ARROW_RIGHT,DECIMAL_MORE),C("Sort A → Z",SORT_ASCENDING,SORT_ASC),C("Sort Z → A",SORT_DESCENDING,SORT_DESC),C("Filter",FILTER,FILTER),C("Clear",FILTER_REMOVE,CLEAR_FILTERS),C("Create view",DATA_VIEW,CREATE_VIEW),C("Transform",DATA_STRUCTURED,TRANSFORM),C("Refresh",RENEW,REFRESH)},
    {C("Analysis view",DATA_VIEW,CREATE_VIEW),C("Matrix",TABLE,MATRIX),C("Bar chart",CHART_BAR,BAR),C("Line chart",CHART_LINE,LINE),C("New sheet",DOCUMENT_BLANK,NEW_SHEET)},
    {C("Sort ascending",SORT_ASCENDING,SORT_ASC),C("Sort descending",SORT_DESCENDING,SORT_DESC),C("Source order",ARROWS_HORIZONTAL,SOURCE_ORDER),C("Column filter",FILTER,FILTER),C("Clear all filters",FILTER_REMOVE,CLEAR_FILTERS),C("Transformations",DATA_STRUCTURED,TRANSFORM),C("Inspect SQL",SQL,SQL),C("Refresh result",RENEW,REFRESH)},
    {VISUALS},
    {C("Workbook",TABLE,WORKBOOK),C("Fields",LIST,FIELDS),C("Transformations",DATA_STRUCTURED,TRANSFORM),C("Inspect SQL",SQL,SQL),C("Focus view",MAXIMIZE,FOCUS),C("Row density",ARROWS_HORIZONTAL,DENSITY),C("Interface scale",SETTINGS_ADJUST,SCALE),C("Gridlines",GRID,GRIDLINES),C("Freeze first 2 columns",COLUMN,FREEZE),C("Reset zoom",ZOOM_IN,RESET_ZOOM),C("Reset layout",RENEW,RESET_LAYOUT),C("Keyboard guide",KEYBOARD,GUIDE)},
    {VISUALS}
};
#undef VISUALS
#undef C
/* Lower priorities degrade first, the inverse of the prototype's CSS helper. */
const UIRibbonGroup ribbon_groups[RIBBON_TABS][UI_RIBBON_GROUPS]={
    {{"Clipboard",0,3,10,false},{"Font",3,6,50,false},{"Alignment",9,4,20,false},{"Number",13,4,30,false},{"Data",17,4,80,false},{"Analysis",21,3,90,true}},
    {{"Analysis",0,4,90,false},{"Workbook",4,1,50,false}},
    {{"Order",0,3,70,false},{"Filter",3,2,90,false},{"Steps",5,3,80,false}},
    {{"Visuals",0,4,90,false},{"Structure",4,4,80,false},{"Inspect",8,2,40,false},{"Return",10,1,10,false}},
    {{"Panes",0,4,90,false},{"Layout",4,3,80,false},{"Worksheet",7,3,40,false},{"Help",10,2,10,false}},
    {{"Visuals",0,4,90,false},{"Structure",4,4,80,false},{"Inspect",8,2,40,false},{"Return",10,1,10,false}}
};
UICarbonIcon ui_ribbon_group_icon(int tab,int group) {
    static const UICarbonIcon icons[RIBBON_TABS][UI_RIBBON_GROUPS]={
        {CARBON_COPY,CARBON_TEXT_BOLD,CARBON_TEXT_ALIGN_LEFT,CARBON_CURRENCY_DOLLAR,CARBON_FILTER,CARBON_DATA_VIEW},
        {CARBON_DATA_VIEW,CARBON_TABLE},
        {CARBON_SORT_ASCENDING,CARBON_FILTER,CARBON_DATA_STRUCTURED},
        {CARBON_CHART_BAR,CARBON_ROW,CARBON_SQL,CARBON_TABLE},
        {CARBON_DATA_STRUCTURED,CARBON_SETTINGS_ADJUST,CARBON_GRID,CARBON_KEYBOARD},
        {CARBON_CHART_BAR,CARBON_ROW,CARBON_SQL,CARBON_TABLE}
    };
    if (tab<0 || tab>=RIBBON_TABS || group<0 || group>=ribbon_group_counts[tab]) return CARBON_COUNT;
    return icons[tab][group];
}
UIRibbonLayout ui_ribbon_spec_layout(float width,int mode,int tab,const float labels[UI_RIBBON_COMMANDS],const float names[UI_RIBBON_GROUPS]) {
    if (tab<0 || tab>=RIBBON_TABS) return (UIRibbonLayout){0};
    if (mode==3) return ui_ribbon_layout(width,mode,ribbon_groups[tab],ribbon_group_counts[tab],labels,names);
    float expanded[]={100,204,208,173,270,254};
    Rectangle cells[UI_RIBBON_COMMANDS]={
        {12,1,74,18},{12,19,74,18},{12,37,74,18},
        {6,0,127,26},{138,0,48,26},{44,28,27,27},{72,28,27,27},{100,28,27,27},{128,28,27,27},
        {6,17,27,28},{34,17,27,28},{62,17,27,28},{93,17,100,28},
        {6,0,148,26},{28,28,27,27},{56,28,33,27},{90,28,33,27},
        {6,3,102,26},{6,29,102,26},{113,16,66,28},{183,16,69,28},
        {6,0,80,54},{91,0,80,54},{176,0,62,54}
    };
    if (width<1400) {
        const float regular[]={93,200,200,166,257,242};
        for (int i=0;i<6;i++) expanded[i]=regular[i];
        for (int i=0;i<3;i++) cells[i].x=6;
        cells[12].width=94; cells[20].x=180; cells[20].width=69;
        cells[21].width=78; cells[22].x=86; cells[22].width=78; cells[23].x=168; cells[23].width=60;
    }
    /* Intrinsic text widths, as in the definition's flex row. Fixed targets
       clipped Filter/Clear after correcting em metrics, then overlapped. */
    float sort=fmaxf(labels[17],labels[18])+39,filter=labels[19]+39,clear=labels[20]+39;
    cells[17]=(Rectangle){6,0,sort,28}; cells[18]=(Rectangle){6,28,sort,28};
    cells[19]=(Rectangle){6+sort+8,14,filter,28};
    cells[20]=(Rectangle){cells[19].x+filter+8,14,clear,28};
    expanded[4]=cells[20].x+clear+18;
    /* The action dock already includes overflow. Reserve it once, including
       on wide windows before any group actually needs the overflow button. */
    float reserve=(width>=700 ? 98 : 38)+(mode==0 ? 0 : 32);
    if (tab==RIBBON_HOME && (mode==1 || mode==2)) {
        UIRibbonGroup groups[UI_RIBBON_GROUPS]; float measures[UI_RIBBON_COMMANDS]={0},sizes[UI_RIBBON_GROUPS]={0};
        Rectangle targets[UI_RIBBON_COMMANDS]={0}; int mapping[UI_RIBBON_COMMANDS],count=0;
        for (int g=0;g<6;g++) {
            groups[g]=ribbon_groups[tab][g]; groups[g].first=count; groups[g].count=0;
            float x=mode==1 ? 7 : 5;
            for (int c=ribbon_groups[tab][g].first;c<ribbon_groups[tab][g].first+ribbon_groups[tab][g].count;c++) {
                if (!ui_ribbon_command_visible(tab,mode,c)) continue;
                float size=mode==2 ? 27 : c==3 ? 127 : c==4 ? 48 : c==13 ? 118 : (c>=5 && c<=11) ? 28 : labels[c]+39;
                mapping[count]=c; measures[count]=labels[c]; targets[count]=(Rectangle){x,0,size,28};
                x+=size+(mode==1 ? 2 : 0); groups[g].count++; count++;
            }
            sizes[g]=x+7+21+12; /* group padding + disclosure + occupied gutter */
        }
        UIRibbonLayout result=ui_ribbon_layout_docked(width,mode,groups,6,measures,names,sizes,targets,reserve);
        Rectangle remapped[UI_RIBBON_COMMANDS]={0};
        for (int c=0;c<count;c++) remapped[mapping[c]]=result.commands[c];
        for (int c=0;c<UI_RIBBON_COMMANDS;c++) result.commands[c]=remapped[c];
        if (width<700 && !result.overflow_count) result.overflow=(Rectangle){width-reserve+4,mode==1 ? 4 : 2,28,28};
        return result;
    }
    UIRibbonLayout result=ui_ribbon_layout_docked(width,mode,ribbon_groups[tab],ribbon_group_counts[tab],labels,names,
        tab==RIBBON_HOME ? expanded : NULL,tab==RIBBON_HOME ? cells : NULL,reserve);
    if (width<700 && !result.overflow_count) result.overflow=(Rectangle){width-reserve+4,mode==0 ? 27 : mode==1 ? 4 : 2,28,28};
    return result;
}
bool ui_ribbon_command_visible(int tab,int mode,int command) {
    if (tab<0 || tab>=RIBBON_TABS || command<0 || command>=ribbon_counts[tab]) return false;
    if (tab!=RIBBON_HOME || mode==0 || mode==3) return true;
    const int compact[]={0,3,4,5,6,9,10,11,13,17,19,21,22},icons[]={0,5,6,8,9,10,11,14,17,19,21,22};
    const int *visible=mode==1 ? compact : icons; size_t count=mode==1 ? sizeof(compact)/sizeof(*compact) : sizeof(icons)/sizeof(*icons);
    for (size_t i=0;i<count;i++) if (visible[i]==command) return true;
    return false;
}
int ui_ribbon_action_index(int tab,UIAction action) {
    if (tab<0 || tab>=RIBBON_TABS) return -1;
    for (int i=0;i<ribbon_counts[tab];i++) if (ribbon_commands[tab][i].action==action) return i;
    return -1;
}
