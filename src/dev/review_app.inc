/* Review-only App adapter. Included after the normal controllers so targets
   use their actual geometry/cache, with no database access or retained cells. */
#include "review_capture_layout.h"
#ifdef DUXCEL_BROWSER_HOST
#include <emscripten/heap.h>
#include <malloc.h>
#endif
static DxReviewRect review_rect(Rectangle r) { return (DxReviewRect){r.x,r.y,r.width,r.height}; }
static bool review_armed(App *a) { return a->review && a->review->phase==DX_REPORT_ARMED; }
static DxReviewDataIdentity review_scope(App *a) {
    DxReviewDataIdentity d={.present=true,.sheet_id=a->book.active+1,.schema_generation=a->field_schema_generation,.query_generation=a->analysis_revision,.source_id=0};
    snprintf(d.kind,sizeof(d.kind),"query-position");
    snprintf(d.document_id,sizeof(d.document_id),"%s",a->review->frame.document_id);
    return d;
}
static void review_control(App *a,const char *id,const char *action,const char *path,const char *role,const char *label,Rectangle bounds,int layer,bool enabled,bool focused,bool container,const char *file,int line) {
    if (!review_armed(a) || bounds.width<=0 || bounds.height<=0) return;
    DxReviewWidget w; dx_review_widget_init(&w,id,action,path,role,label);
    w.bounds=review_rect(bounds); w.clip=a->ui.clipping ? review_rect(a->ui.clip) : (DxReviewRect){0,0,GetScreenWidth(),GetScreenHeight()};
    w.layer=layer; w.enabled=enabled; w.input_eligible=enabled && a->modal==MODAL_NONE && strcmp(role,"text") && strcmp(role,"region") && strcmp(role,"banner") && strcmp(role,"tablist"); w.focused=focused;
    if (!strncmp(id,"chrome.",7) && strcmp(id,"chrome.title_bar") && strcmp(id,"chrome.help")) snprintf(w.parent_id,sizeof(w.parent_id),"chrome.title_bar");
    if (!strcmp(id,"ribbon.tabs.background")) snprintf(w.parent_id,sizeof(w.parent_id),"ribbon.tabs");
    w.container=container; w.source_line=(uint32_t)line; snprintf(w.source_file,sizeof(w.source_file),"%s",file);
    if (a->popup_capturing) { w.transform.ty=a->popup_presence.offset*a->ui.scale; w.bounds.y+=w.transform.ty; if (a->ui.clipping) w.clip.y+=w.transform.ty; w.layer+=100; }
    DxReviewStatus result=dx_review_add(a->review->registry,&w);
    if (result!=DX_REVIEW_OK) fprintf(stderr,"Review registration %s: %s\n",id,dx_review_status_text(result));
}
static void review_begin_frame(App *a) {
    DxReviewFrame f={.frame_id=a->frames+1,.timestamp_ns=(uint64_t)(dx_millis()*1000000.0),.window_width=GetScreenWidth(),.window_height=GetScreenHeight(),.framebuffer_width=GetRenderWidth(),.framebuffer_height=GetRenderHeight(),.ui_scale=a->ui.scale,.grid_zoom=active_canvas_zoom(a)};
    Vector2 dpi=GetWindowScaleDPI(); f.dpi_x=dpi.x; f.dpi_y=dpi.y;
    /* Workbook identity is session-scoped, never a filesystem path or pointer. */
    snprintf(f.document_id,sizeof(f.document_id),"%s-document-%u",a->review->frame.session_id,a->review->document_serial);
    snprintf(f.fonts,sizeof(f.fonts),"%s; IBMPlexSans-Medium.otf; IBMPlexSans-SemiBold.otf; %s",getenv("DUXCEL_FONT") ? getenv("DUXCEL_FONT") : "IBMPlexSans-Regular.otf",getenv("DUXCEL_MONO_FONT") ? getenv("DUXCEL_MONO_FONT") : "JetBrainsMono-Regular.ttf");
    snprintf(f.state_text,sizeof(f.state_text),"sheet=%zu modal=%d ribbon=%d/%d focus=%d/%d/%d filter_field=%zu filter_focus=%d filter_menu=%d search_cursor=%zu search_anchor=%zu search_revision=%llu active=%llu,%zu anchor=%llu,%zu extent=%llu,%zu top=%llu horizontal=%.3f page=%llu/%zu spare=%llu/%zu source_revision=%llu schema=%llu request=%llu busy=%d io_busy=%d dirty=%d",a->book.active,(int)a->modal,a->ribbon_tab,a->ribbon_mode,a->chrome_focus,a->pivot_focus,a->ribbon_focus,a->field,a->filter_focus,a->filter_condition_menu,a->input.cursor,a->input.anchor,(unsigned long long)a->input.revision,(unsigned long long)a->grid.row,a->grid.col,(unsigned long long)a->grid.anchor_row,a->grid.anchor_col,(unsigned long long)a->grid.end_row,a->grid.end_col,(unsigned long long)a->grid.top,a->grid.horizontal,(unsigned long long)a->page.offset,a->page.row_count,(unsigned long long)a->spare.offset,a->spare.row_count,(unsigned long long)a->analysis_revision,(unsigned long long)a->field_schema_generation,(unsigned long long)a->request_id,a->busy,a->io_busy,a->book.dirty);
    size_t used=strlen(f.state_text);
    snprintf(f.state_text+used,sizeof(f.state_text)-used," fraction=%.9f row_height=%.6f elastic=%.6f,%.6f velocity=%.6f,%.6f body=%.3f,%.3f,%.3f,%.3f keytips=%.6f",a->grid.fraction,a->grid.row_height>=18 ? a->grid.row_height : 27,a->grid.elastic.x,a->grid.elastic.y,a->grid.spring_velocity.x,a->grid.spring_velocity.y,a->grid.body.x,a->grid.body.y,a->grid.body.width,a->grid.body.height,a->keytip_alpha);
    used=strlen(f.state_text);
    float cell_scale=a->ui.scale*a->grid.zoom;
    snprintf(f.state_text+used,sizeof(f.state_text)-used," grid_rect=%.3f,%.3f,%.3f,%.3f grid_gutter=%.3f grid_header=%.3f grid_cell=%.3f,%.3f source_header=%d grid_selection=%d",a->grid.rect.x,a->grid.rect.y,a->grid.rect.width,a->grid.rect.height,a->grid.body.x-a->grid.rect.x,a->grid.body.y-a->grid.rect.y,a->grid.width_count ? a->grid.widths[0]*cell_scale : 0,(a->grid.row_height>=18 ? a->grid.row_height : 27)*cell_scale,a->grid.source_header,(int)a->grid.selection);
    used=strlen(f.state_text);
    Worksheet *sheet=active_sheet(a); CellFormat format=active_format(a);
    snprintf(f.state_text+used,sizeof(f.state_text)-used," keytip_tab=%d keytip_prefix=%s file_menu=%d active_format_flags=%u manual_cells=%zu sheet_count=%zu",a->keytip_state.tab,a->keytip_state.prefix[0] ? a->keytip_state.prefix : "-",a->file_menu,format.flags,sheet ? sheet->cells.count : 0,a->book.count);
    UIChoiceListLayout file=file_menu_layout(a); used=strlen(f.state_text);
    snprintf(f.state_text+used,sizeof(f.state_text)-used," file_focus=%d file_scroll=%d file_visible=%d file_panel=%.3f,%.3f,%.3f,%.3f",a->file_menu_focus,a->file_menu_scroll,file.visible,file.frame.panel.x,file.frame.panel.y,file.frame.panel.width,file.frame.panel.height);
    int last_command=-1;
    for (unsigned i=0;i<a->review->trace_count;i++) {
        const char *line=a->review->trace[(a->review->trace_head+i)%DX_REVIEW_EVENTS];
        const char *command=strstr(line," command.");
        if (command) { const char *value=strrchr(command,'='); if (value) last_command=(int)strtol(value+1,NULL,10); }
    }
    used=strlen(f.state_text); snprintf(f.state_text+used,sizeof(f.state_text)-used," last_command=%d",last_command);
#ifdef DUXCEL_BROWSER_HOST
    used=strlen(f.state_text);
    snprintf(f.state_text+used,sizeof(f.state_text)-used," ui_heap=%zu ui_allocated=%u",emscripten_get_heap_size(),(unsigned)mallinfo().uordblks);
#endif
    dx_review_capture_begin(a->review,&f);
}
static const char *review_action_id(UIAction action) {
    static const char *const actions[]={
        [ACTION_COPY]="command.copy",
        [ACTION_PASTE]="command.paste",
        [ACTION_CUT]="command.cut",
        [ACTION_FONT]="command.font",
        [ACTION_FONT_SIZE]="command.font_size",
        [ACTION_BOLD]="command.bold",
        [ACTION_ITALIC]="command.italic",
        [ACTION_UNDERLINE]="command.underline",
        [ACTION_TEXT_COLOR]="command.text_color",
        [ACTION_ALIGN_LEFT]="command.align_left",
        [ACTION_ALIGN_CENTER]="command.align_center",
        [ACTION_ALIGN_RIGHT]="command.align_right",
        [ACTION_WRAP]="command.wrap",
        [ACTION_NUMBER_FORMAT]="command.number_format",
        [ACTION_ACCOUNTING]="command.accounting",
        [ACTION_DECIMAL_LESS]="command.decimal_less",
        [ACTION_DECIMAL_MORE]="command.decimal_more",
        [ACTION_SORT_ASC]="command.sort_asc",
        [ACTION_SORT_DESC]="command.sort_desc",
        [ACTION_FILTER]="command.filter",
        [ACTION_CLEAR_FILTERS]="command.clear_filters",
        [ACTION_CREATE_VIEW]="command.create_view",
        [ACTION_TRANSFORM]="command.transform",
        [ACTION_REFRESH]="command.refresh",
        [ACTION_MATRIX]="command.matrix",
        [ACTION_BAR]="command.bar",
        [ACTION_LINE]="command.line",
        [ACTION_NEW_SHEET]="command.new_sheet",
        [ACTION_SOURCE_ORDER]="command.source_order",
        [ACTION_SQL]="command.sql",
        [ACTION_ADD_ROWS]="command.add_rows",
        [ACTION_ADD_COLUMNS]="command.add_columns",
        [ACTION_SWAP_SHELVES]="command.swap_shelves",
        [ACTION_ADD_MEASURE]="command.add_measure",
        [ACTION_BROWSE]="command.browse",
        [ACTION_WORKBOOK]="command.workbook",
        [ACTION_FIELDS]="command.fields",
        [ACTION_FOCUS]="command.focus",
        [ACTION_DENSITY]="command.density",
        [ACTION_SCALE]="command.scale",
        [ACTION_GRIDLINES]="command.gridlines",
        [ACTION_FREEZE]="command.freeze",
        [ACTION_RESET_ZOOM]="command.reset_zoom",
        [ACTION_RESET_LAYOUT]="command.reset_layout",
        [ACTION_GUIDE]="command.guide",
    };
    return (unsigned)action<sizeof(actions)/sizeof(actions[0]) ? actions[action] : "command.unknown";
}
static void review_ribbon_control(App *a,int command,Rectangle bounds) {
    if (!review_armed(a) || command<0 || command>=ribbon_counts[a->ribbon_tab]) return;
    const UIRibbonCommand *c=&ribbon_commands[a->ribbon_tab][command];
    const char *action=review_action_id(c->action); char id[128],path[192];
    snprintf(id,sizeof(id),"ribbon.%d.%s.%s",a->ribbon_tab,a->popup_capturing ? "flyout" : "command",action+8);
    static const char *const modes[]={"full","compact","icons","focus"};
    snprintf(path,sizeof(path),"ribbon/%s/%s/%s",ui_ribbon_tabs[a->ribbon_tab],modes[a->ribbon_mode],a->popup_capturing ? "flyout" : "commands");
    review_control(a,id,action,path,"button",c->label,bounds,20,command_enabled(a,command),a->ribbon_focus && a->ribbon_command==command,false,__FILE__,__LINE__);
}
static void review_ribbon_tab(App *a,int tab,Rectangle bounds) {
    if (!review_armed(a) || tab<0 || tab>=RIBBON_TABS) return;
    static const char *const names[]={"home","insert","data","analyze","view","visualization"};
    char id[64],action[64],path[96];
    snprintf(id,sizeof(id),"ribbon.tab.%s",names[tab]);
    snprintf(action,sizeof(action),"ribbon.select.%s",names[tab]);
    snprintf(path,sizeof(path),"ribbon/tabs/%s",names[tab]);
    review_control(a,id,action,path,"tab",ui_ribbon_tabs[tab],bounds,20,!a->io_busy,a->ribbon_focus && a->ribbon_command==-1 && a->ribbon_tab==tab,false,__FILE__,__LINE__);
    DxReviewWidget *w=(DxReviewWidget *)dx_review_find(a->review->registry,id);
    if (w) { w->checked=a->ribbon_tab==tab; snprintf(w->parent_id,sizeof(w->parent_id),"ribbon.tabs"); }
}
static DxReviewWidget *review_file_item(App *a,const char *id,const char *action,const char *role,const char *label,Rectangle bounds,Rectangle clip,int layer,bool enabled,bool focused,bool container) {
    review_control(a,id,action,"file/menu",role,label,bounds,layer,enabled,focused,container,__FILE__,__LINE__);
    DxReviewWidget *w=(DxReviewWidget *)dx_review_find(a->review->registry,id);
    if (w) { w->clip=review_rect(clip); if (strcmp(id,"file.menu")) snprintf(w->parent_id,sizeof(w->parent_id),"file.menu"); }
    return w;
}
static void review_file_menu(App *a) {
    if (!review_armed(a) || !a->file_menu || a->modal!=MODAL_NONE) return;
    UIChoiceListLayout list=file_menu_layout(a); if (!list.frame.valid) return;
    Rectangle panel=list.frame.panel; float s=a->ui.scale;
    /* A top-level container blocks covered chrome even in the panel's blank
       margins. Disabled rows stay pickable without becoming input eligible. */
    DxReviewWidget *menu=review_file_item(a,"file.menu","file.inspect","menu","File",panel,panel,200,true,false,true);
    if (menu) { menu->expanded=true; menu->input_eligible=false; }
    review_file_item(a,"file.background","file.inspect","region","File menu background",panel,panel,200,true,false,false);
    review_file_item(a,"file.heading","file.inspect","text","File",list.frame.title,panel,201,true,false,false);
    static const char *const suffix[]={"new","open","save","save_as","close","quit"};
    static const char *const actions[]={"command.new_workbook","command.open","command.save","command.save_as","command.close","command.quit"};
    bool scrolling=list.visible<UI_KEYTIP_FILE_COUNT;
    for (int row=0;row<list.visible;row++) {
        int action=row+a->file_menu_scroll; if (action<0 || action>=UI_KEYTIP_FILE_COUNT) continue;
        Rectangle item=list.frame.rows[row]; if (scrolling) item.width-=9*s;
        char id[64]; snprintf(id,sizeof(id),"file.action.%s",suffix[action]);
        review_file_item(a,id,actions[action],"menuitem",ui_keytips_file_name((UIKeytipFileAction)action),item,GetCollisionRec(list.items,panel),201,file_action_available(a,action),a->file_menu_focus==action,false);
    }
    if (scrolling) {
        Rectangle track=box(list.items.x+list.items.width-8*s,list.items.y,8*s,list.items.height);
        float size=fminf(track.height,fmaxf(26*s,track.height*list.visible/UI_KEYTIP_FILE_COUNT));
        float offset=fmaxf(0,fminf(UI_KEYTIP_FILE_COUNT-list.visible,a->file_menu_scroll));
        Rectangle thumb=box(track.x+3*s,track.y+(track.height-size)*offset/(UI_KEYTIP_FILE_COUNT-list.visible),fmaxf(s,track.width-6*s),size);
        review_file_item(a,"file.scrollbar","file.scroll","scrollbar","File menu scroll",track,panel,202,true,false,false);
        DxReviewWidget *w=review_file_item(a,"file.scrollbar.thumb","file.scroll","scrollbar","File menu scroll thumb",thumb,track,203,true,a->file_menu_drag,false);
        if (w) snprintf(w->parent_id,sizeof(w->parent_id),"file.scrollbar");
    }
}
static void review_choice_menu(App *a,const UIChoiceLayout *layout,const char *title,const char *const *names,int count,int selected,int focus) {
    if (!review_armed(a) || !layout || !layout->valid) return;
    int kind=a->modal==MODAL_RIBBON ? 0 : a->modal==MODAL_INTERFACE_SCALE ? 1 : a->modal==MODAL_DENSITY ? 2 : a->modal==MODAL_FORMAT && a->format_action==ACTION_TEXT_COLOR ? 3 : -1;
    if (kind<0 || count<1 || count>4 || count!=layout->count) return;
    static const char *const roots[]={"ribbon_display","interface_scale","row_density","text_color"};
    static const char *const options[][4]={{"full","compact","icons","focus"},{"compact","standard","large","extra_large"},{"compact","default","comfortable",NULL},{"automatic","blue","teal","red"}};
    char id[128],parent[96],path[96];
    snprintf(parent,sizeof(parent),"choice.%s",roots[kind]); snprintf(path,sizeof(path),"popup/%s",roots[kind]);
    Rectangle clip=a->ui.clipping ? GetCollisionRec(layout->panel,a->ui.clip) : layout->panel;
    float offset=a->popup_capturing ? a->popup_presence.offset*a->ui.scale : 0; clip.y+=offset;
    int layer=a->popup_capturing ? 30 : 130;
    /* The modal itself owns input. Its rows are above any retained parent
       flyout, while File's separate top-level surface keeps its higher layer. */
    review_control(a,parent,"choice.inspect",path,"menu",title,layout->panel,layer,true,false,true,__FILE__,__LINE__);
    DxReviewWidget *w=(DxReviewWidget *)dx_review_find(a->review->registry,parent);
    if (w) { w->clip=review_rect(clip); w->expanded=true; w->input_eligible=false; }
    snprintf(id,sizeof(id),"%s.background",parent);
    review_control(a,id,"choice.inspect",path,"region","Menu background",layout->panel,layer,true,false,false,__FILE__,__LINE__);
    w=(DxReviewWidget *)dx_review_find(a->review->registry,id);
    if (w) { w->clip=review_rect(clip); snprintf(w->parent_id,sizeof(w->parent_id),"%s",parent); }
    snprintf(id,sizeof(id),"%s.heading",parent);
    review_control(a,id,"choice.inspect",path,"text",title,layout->title,layer+1,true,false,false,__FILE__,__LINE__);
    w=(DxReviewWidget *)dx_review_find(a->review->registry,id);
    if (w) { w->clip=review_rect(clip); snprintf(w->parent_id,sizeof(w->parent_id),"%s",parent); }
    for (int i=0;i<count;i++) {
        if (!options[kind][i]) continue;
        snprintf(id,sizeof(id),"%s.option.%s",parent,options[kind][i]);
        review_control(a,id,"choice.select",path,"menuitemradio",names[i],layout->rows[i],layer+1,true,i==focus,false,__FILE__,__LINE__);
        w=(DxReviewWidget *)dx_review_find(a->review->registry,id);
        if (w) { w->clip=review_rect(clip); snprintf(w->parent_id,sizeof(w->parent_id),"%s",parent); w->checked=i==selected; w->input_eligible=true; }
    }
}
static DxReviewWidget *review_filter_item(App *a,const char *id,const char *action,const char *role,const char *label,Rectangle bounds,Rectangle clip,int layer,int focus,bool enabled) {
    DxReviewWidget w; dx_review_widget_init(&w,id,action,"popup/column-filter",role,label);
    snprintf(w.parent_id,sizeof(w.parent_id),"filter.panel");
    w.bounds=review_rect(bounds); float offset=a->popup_presence.offset*a->ui.scale;
    w.transform.ty=offset; w.bounds.y+=offset; clip.y+=offset; w.clip=review_rect(clip); w.layer=layer;
    w.enabled=enabled; w.input_eligible=enabled; w.focused=a->filter_focus==focus;
    w.data=review_scope(a); SheetObject *o=filter_object(a); if (o) { w.data.object_id=o->id; w.data.source_id=o->source_id; w.data.query_generation=o->revision; w.data.schema_generation=object_source_revision(a,o); }
    Page *fields=filter_fields(a); if (a->field<fields->column_count) snprintf(w.data.field,sizeof(w.data.field),"%s",fields->columns[a->field].name);
    snprintf(w.source_file,sizeof(w.source_file),"%s",__FILE__); w.source_line=__LINE__;
    DxReviewRegistry *registry=a->review->registry;
    if (dx_review_add(registry,&w)!=DX_REVIEW_OK) return NULL;
    return &registry->widgets[registry->count-1];
}
static const char *review_filter_op(FilterOp op) {
    switch ((int)op) {
    case -1: return "any"; case FILTER_CONTAINS: return "contains"; case FILTER_EQUAL: return "equal";
    case FILTER_GREATER: return "greater"; case FILTER_LESS: return "less"; case FILTER_BETWEEN: return "between";
    case FILTER_NULL: return "is_null"; case FILTER_NOT_NULL: return "not_null"; default: return "unsupported";
    }
}
static void review_filter(App *a) {
    if (!review_armed(a) || a->modal!=MODAL_FILTER || !a->filter_layout.valid) return;
    UIFilterLayout r=a->filter_layout; float s=a->ui.scale; bool allow=!a->filter_condition_menu && !a->values_selecting;
    review_control(a,"filter.panel","filter.inspect","popup/column-filter","dialog","Column filter",r.panel,0,true,false,true,__FILE__,__LINE__);
#define FILTER_ITEM(id,action,role,label,rect,focus,enabled) review_filter_item(a,id,action,role,label,rect,r.panel,101,focus,enabled)
    FILTER_ITEM("filter.close","filter.close","button","Close",box(r.title.x+r.title.width-33*s,r.title.y+6.5f*s,23*s,23*s),0,!a->filter_condition_menu);
    FILTER_ITEM("filter.sort.ascending","command.sort_asc","button","Sort ascending",r.sort[0],1,allow && !selected_pivot(a));
    FILTER_ITEM("filter.sort.descending","command.sort_desc","button","Sort descending",r.sort[1],2,allow && !selected_pivot(a));
    FILTER_ITEM("filter.search","filter.search","textbox","Search available values",r.search,3,allow);
    FILTER_ITEM("filter.all","filter.select_all","checkbox","Select all",r.all,4,allow);
    DxReviewWidget *all=(DxReviewWidget *)dx_review_find(a->review->registry,"filter.all"); if (all) { all->checked=a->value_checks_all; all->indeterminate=a->value_checks_mixed; }
    unsigned visible=(unsigned)fmaxf(1,floorf((r.list.height-8*s)/(25*s)));
    for (unsigned row=0;row<visible && a->values_total>0 && a->values_scroll+row<(uint64_t)a->values_total;row++) {
        uint64_t index=a->values_scroll+row; DistinctPage *page=cached_values(a,index/DX_DISTINCT_ROWS*DX_DISTINCT_ROWS);
        if (!page || index-page->offset>=page->count) continue;
        DistinctValue *v=&page->values[index-page->offset]; char id[128];
        snprintf(id,sizeof(id),"filter.value.%llu.%zu.%d.%llu",(unsigned long long)a->request_id,a->field,a->date_level,(unsigned long long)index);
        DxReviewWidget *w=review_filter_item(a,id,"filter.toggle_value","checkbox",v->label,box(r.list.x,r.list.y+(3+row*25)*s,r.list.width-10*s,25*s),r.list,101,a->filter_focus==5 && a->values_focus==(int64_t)index ? 5 : -1,allow);
        if (!w) continue; bool partial=false;
        w->checked=value_checked(a,v->key,&partial); w->indeterminate=partial;
        dx_review_value_set(&w->value,filter_fields(a)->columns[a->field].type_name,v->is_null,v->value);
    }
    if (a->date_level>0) FILTER_ITEM("filter.date.parent","filter.date_parent","button","Parent",box(r.dates.x+r.dates.width-79*s,r.dates.y+2*s,73*s,23*s),-1,allow);
    FILTER_ITEM("filter.condition","filter.condition","combobox","Condition",r.condition,6,allow);
    DxReviewWidget *condition=(DxReviewWidget *)dx_review_find(a->review->registry,"filter.condition");
    if (condition) { condition->expanded=a->filter_condition_menu; dx_review_value_set(&condition->value,"condition",false,filter_condition_label(a,a->filter_condition_enabled ? a->filter_op : (FilterOp)-1)); }
    DxReviewWidget *search=(DxReviewWidget *)dx_review_find(a->review->registry,"filter.search"); if (search) dx_review_value_set(&search->value,"VARCHAR",false,a->input.text);
    if (a->filter_condition_enabled && a->filter_op!=FILTER_NULL && a->filter_op!=FILTER_NOT_NULL) { FILTER_ITEM("filter.term","filter.term","textbox","Value",r.term,7,allow); if (a->filter_op==FILTER_BETWEEN) FILTER_ITEM("filter.end","filter.end","textbox","To",r.end,8,allow); }
    FILTER_ITEM("filter.clear","filter.clear","button","Clear filter",r.clear,9,allow);
    FILTER_ITEM("filter.cancel","filter.close","button","Cancel",r.cancel,10,!a->filter_condition_menu);
    FILTER_ITEM("filter.apply","filter.apply","button","Apply",r.apply,11,allow);
    if (a->filter_condition_menu) {
        FilterOp choices[8]; int count=filter_conditions(a,choices);
        UISelectMenuLayout m=ui_select_menu_layout((Vector2){r.condition.x,r.condition.y+r.condition.height},(Vector2){GetScreenWidth(),GetScreenHeight()},s,r.condition.width/s,count);
        review_control(a,"filter.conditions","filter.conditions","popup/column-filter/conditions","menu","Conditions",m.panel,10,true,false,true,__FILE__,__LINE__);
        for (int row=0;row<m.visible;row++) { int i=row+a->filter_condition_scroll; if (i<0 || i>=count) continue; char id[64]; snprintf(id,sizeof(id),"filter.condition.option.%s",review_filter_op(choices[i])); DxReviewWidget *w=review_filter_item(a,id,"filter.choose_condition","menuitem",filter_condition_label(a,choices[i]),ui_select_menu_row(&m,row),m.panel,111,-1,true); if (w) { w->focused=i==a->filter_condition_index; w->checked=a->filter_condition_enabled ? choices[i]==a->filter_op : (int)choices[i]<0; snprintf(w->parent_id,sizeof(w->parent_id),"filter.conditions"); } }
    }
#undef FILTER_ITEM
}
static void review_grid(App *a) {
    if (!review_armed(a)) return;
    if (selected_pivot(a) && !a->step_preview.active && a->ribbon_mode!=3) {
        /* Authoring results have different coordinates/grain from worksheet
           cells. Expose the actual visible surfaces, never invented cell values. */
        review_control(a,"analysis.source_preview","analysis.inspect_source","workspace/analysis/source","grid","Source preview",a->authoring_layout.preview,5,true,false,false,__FILE__,__LINE__);
        review_control(a,"analysis.matrix","analysis.inspect_matrix","workspace/analysis/matrix","grid","Matrix result",a->authoring_layout.matrix,5,true,false,false,__FILE__,__LINE__);
        review_control(a,"analysis.chart","analysis.inspect_chart","workspace/analysis/chart","figure","Chart",a->authoring_layout.chart,5,true,false,false,__FILE__,__LINE__);
        return;
    }
    Grid *g=a->step_preview.active ? &a->step_preview.grid : &a->grid;
    review_control(a,"worksheet.cells","worksheet.inspect_cell","workspace/canvas","grid","Worksheet cells",g->body,5,true,false,false,__FILE__,__LINE__);
    /* The header/scrollbars are separate targets; cells are resolved only at the picked point. */
    review_control(a,"worksheet.header","worksheet.inspect_header","workspace/canvas","row","Column headers",box(g->rect.x,g->rect.y,g->rect.width,g->body.y-g->rect.y),6,true,false,false,__FILE__,__LINE__);
    review_control(a,"worksheet.vertical_scroll","worksheet.scroll","workspace/canvas","scrollbar","Vertical scroll",g->vtrack,7,true,false,false,__FILE__,__LINE__);
    review_control(a,"worksheet.horizontal_scroll","worksheet.scroll","workspace/canvas","scrollbar","Horizontal scroll",g->htrack,7,true,false,false,__FILE__,__LINE__);
}
static void review_resolve_grid(App *a) {
    DxReviewCapture *r=a->review;
    if (!r || r->phase!=DX_REPORT_NOTE || !r->target.found || strcmp(r->target.widget.id,"worksheet.cells")) return;
    Grid *g=a->step_preview.active ? &a->step_preview.grid : &a->grid;
    uint64_t row; size_t col; float scale=a->ui.scale*g->zoom;
    if (!grid_cell_at(g,scale,r->pick_point,&row,&col)) return;
    DxReviewWidget w; char id[128]; snprintf(id,sizeof(id),"worksheet.cell.%zu.%llu.%llu.%zu",a->book.active,(unsigned long long)a->request_id,(unsigned long long)row,col);
    dx_review_widget_init(&w,id,"worksheet.inspect_cell","workspace/canvas/cell","gridcell","Worksheet cell");
    w.bounds=review_rect(grid_range_rect(g,scale,row,col,1,1)); w.clip=review_rect(grid_column_clip(g,scale,col)); w.layer=5;
    w.data=review_scope(a); w.data.has_cell=true; w.data.row=row; w.data.column=col;
    const Page *p=a->step_preview.active ? &a->step_preview.page : &a->page,*spare=a->step_preview.active ? &a->step_preview.spare : &a->spare;
    Worksheet *sheet=active_sheet(a); int hit=!a->step_preview.active && sheet ? objects_hit(&sheet->objects,row,col) : -1;
    uint64_t local=row; size_t column=col; const char *synthetic=NULL; bool sparse=false,stale=a->page_stale;
    if (hit>=0) {
        SheetObject *o=&sheet->objects.items[hit]; ObjectView *v=&a->objects[hit]; p=&v->page; spare=NULL;
        w.data.object_id=o->id; w.data.source_id=o->source_id; w.data.schema_generation=object_source_revision(a,o); w.data.query_generation=o->revision;
        column-=o->col; local=row>=o->row+2 ? row-o->row-2 : UINT64_MAX;
        stale=v->dirty || v->failed || v->revision!=o->revision || v->source_revision!=object_source_revision(a,o);
        if (row==o->row && col==o->col) synthetic=o->name;
        else if (row==o->row+1 && column<p->column_count) synthetic=p->columns[column].name;
        else if (o->kind==OBJECT_CHART || row<o->row+2) sparse=true;
    } else if (!a->step_preview.active && sheet && (a->analysis.kind==SOURCE_NONE || row>=source_rows(a) || col>=a->page.column_count)) { synthetic=cells_get(&sheet->cells,row,col); sparse=!synthetic || !*synthetic; snprintf(w.data.kind,sizeof(w.data.kind),"sparse-cell"); w.data.schema_generation=sheet->cells.revision; }
    if (a->step_preview.active) { stale=a->step_preview.loading; snprintf(w.data.kind,sizeof(w.data.kind),"step-preview"); w.data.source_id=a->step_preview.source_id; w.data.schema_generation=a->step_preview.source_revision; w.data.query_generation=a->step_preview.request; }
    if (synthetic || sparse) dx_review_value_set(&w.value,sparse ? "NULL" : "VARCHAR",sparse,synthetic);
    else if (!stale) {
        const Cell *cell=copy_cached_cell(p,local,column); if (!cell && spare) { cell=copy_cached_cell(spare,local,column); if (cell) p=spare; }
        if (cell) { dx_review_value_set(&w.value,p->columns[column].type_name,cell->is_null,cell->text); snprintf(w.data.field,sizeof(w.data.field),"%s",p->columns[column].name); }
        else if (column>=p->column_count || (!p->count_pending && local>=p->total)) dx_review_value_set(&w.value,"NULL",true,NULL);
    }
    snprintf(w.source_file,sizeof(w.source_file),"%s",__FILE__); w.source_line=__LINE__;
    r->target.widget=w; r->target.bounds=review_rect(clip_rect((Rectangle){w.bounds.x,w.bounds.y,w.bounds.width,w.bounds.height},(Rectangle){w.clip.x,w.clip.y,w.clip.width,w.clip.height}));
}
static bool review_grid_gesture(const Grid *g) {
    return g->dragging || g->moving || g->resizing>=0 || g->scrollbar;
}
static bool review_can_capture(App *a) {
    /* Finish the current gesture before freezing input. Otherwise the release
       can be consumed by reporting and leave a thumb/selection dragging when
       the workbook resumes. Check nested surfaces as well as the main grid. */
    return a->review && a->review->phase==DX_REPORT_IDLE &&
        !IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && !IsMouseButtonDown(MOUSE_BUTTON_MIDDLE) &&
        !review_grid_gesture(&a->grid) && !review_grid_gesture(&a->source_preview) &&
        !(a->step_preview.initialized && review_grid_gesture(&a->step_preview.grid)) &&
        !a->analysis_vbar.dragging && !a->matrix_hbar.dragging && !a->matrix_vbar.dragging &&
        !a->dock_scrollbars[0].dragging && !a->dock_scrollbars[1].dragging && !a->author_scrollbar.dragging &&
        !a->chart_resizing && a->pivot_drag_field<0 && a->pivot_drag_role<0 && !a->matrix_selecting &&
        !(a->modal==MODAL_GROUP && a->summary_drag>=0) && !a->split_side && !a->split_pipeline &&
        !a->steps_drag && !a->workbook_drag && !a->field_drag && !a->values_drag && !a->context_drag &&
        !(a->file_menu && a->file_menu_drag) &&
        !a->inspector_drag && !a->table_relation_drag && !a->input.dragging && !a->filter_term.dragging &&
        !a->filter_end.dragging && !a->pivot_search.dragging && !a->chart_columns.dragging && !a->sort_editor.search.dragging;
}

static void review_native_command(App *a,DxCommand command) {
    DxReviewCapture *r=a->review;
    /* Native menu commands are consumed in every reporting phase, but only
       the focused editable composer may mutate its owned draft. */
    if (!r || (r->phase!=DX_REPORT_NOTE && r->phase!=DX_REPORT_ERROR) || r->focus!=0) return;
    r->note.native_edit=true;
    if (command==DX_COMMAND_SELECT_ALL) r->note.select_all=true;
    else if (command==DX_COMMAND_PASTE) { const char *text=GetClipboardText(); if (text && strlen(text)<DX_REVIEW_NOTE_BYTES) input_insert(&r->note,text); }
    else if (command==DX_COMMAND_UNDO || command==DX_COMMAND_REDO) input_history(&r->note,command==DX_COMMAND_REDO);
    else if (command==DX_COMMAND_COPY) { char *text=input_selection_text(&r->note); if (text) { SetClipboardText(text); free(text); } }
}

/* Focused native acceptance; never enabled in shipping, uses the same queued
   input/controller routes as other App scenarios and private outbox fixtures. */
#ifndef DUXCEL_BROWSER_HOST
#define REVIEW_SMOKE_STAGES 71
typedef struct {
    int stage; uint64_t request,jobs; GridLocation selection; Cell *page;
    size_t cursor,anchor,history; bool dirty; int file_focus,file_scroll,diagnostic_stage; char root[4096];
    Modal choice_modal; int choice_focus,choice_ribbon_mode; float choice_scale,choice_row_height; uint64_t choice_formats;
} ReviewSmoke;
static void review_smoke_click(Rectangle b) { float x=b.x+b.width*.5f,y=b.y+b.height*.5f; ui_note_pointer_gesture((DxPointerGesture){x,y,x,y,0,false}); }
static void review_smoke_pick(App *a,const char *id) {
    const DxReviewWidget *w=dx_review_find(a->review->registry,id);
    if (w) review_smoke_click(GetCollisionRec((Rectangle){w->bounds.x,w->bounds.y,w->bounds.width,w->bounds.height},(Rectangle){w->clip.x,w->clip.y,w->clip.width,w->clip.height}));
}
static bool review_smoke_chrome(App *a) {
    DxReviewRegistry *registry=a->review->registry;
    const char *ids[]={"chrome.brand","chrome.document_title","chrome.save","chrome.help","ribbon.display",
        "ribbon.tab.home","ribbon.tab.insert","ribbon.tab.data","ribbon.tab.analyze","ribbon.tab.view"};
    if (registry->duplicate_count || registry->invalid_count) return false;
    for (size_t i=0;i<sizeof(ids)/sizeof(*ids);i++) {
        const DxReviewWidget *w=dx_review_find(registry,ids[i]); DxReviewTarget picked;
        if (!w) { fprintf(stderr,"FAIL: missing review chrome target %s\n",ids[i]); return false; }
        Rectangle visible=GetCollisionRec((Rectangle){w->bounds.x,w->bounds.y,w->bounds.width,w->bounds.height},(Rectangle){w->clip.x,w->clip.y,w->clip.width,w->clip.height});
        if (visible.width>0 && visible.height>0 && (!dx_review_pick(registry,visible.x+visible.width*.5f,visible.y+visible.height*.5f,DX_REVIEW_PICK_CONTROL,&picked) || strcmp(picked.widget.id,ids[i]))) { fprintf(stderr,"FAIL: review chrome target %s picked %s at %.2f,%.2f\n",ids[i],picked.widget.id,visible.x+visible.width*.5f,visible.y+visible.height*.5f); return false; }
    }
    const DxReviewWidget *home=dx_review_find(registry,"ribbon.tab.home"),*brand=dx_review_find(registry,"chrome.brand");
    DxReviewTarget picked; float s=a->ui.scale;
    return home->checked && !brand->input_eligible &&
        dx_review_pick(registry,2*s,14*s,DX_REVIEW_PICK_CONTROL,&picked) && !strcmp(picked.widget.id,"chrome.title_bar.background") &&
        dx_review_pick(registry,2*s,14*s,DX_REVIEW_PICK_CONTAINER,&picked) && !strcmp(picked.widget.id,"chrome.title_bar") &&
        dx_review_pick(registry,2*s,42*s,DX_REVIEW_PICK_CONTROL,&picked) && !strcmp(picked.widget.id,"ribbon.tabs.background") &&
        dx_review_pick(registry,2*s,42*s,DX_REVIEW_PICK_CONTAINER,&picked) && !strcmp(picked.widget.id,"ribbon.tabs");
}
static bool review_smoke_same(App *a,ReviewSmoke *t) {
    GridLocation g=grid_location(&a->grid);
    return a->modal==MODAL_FILTER && a->filter_focus==3 && a->input.cursor==t->cursor && a->input.anchor==t->anchor && a->request_id==t->request && a->page.cells==t->page && a->book.dirty==t->dirty && active_sheet(a)->undo_actions_count==t->history && g.row==t->selection.row && g.col==t->selection.col && g.end_row==t->selection.end_row && g.end_col==t->selection.end_col;
}
static bool review_smoke_file_same(App *a,ReviewSmoke *t) {
    GridLocation g=grid_location(&a->grid);
    dx_lock(&a->worker.mutex); bool jobs=a->worker.jobs_started==t->jobs; dx_unlock(&a->worker.mutex);
    return a->file_menu && a->modal==MODAL_NONE && a->file_menu_focus==t->file_focus && a->file_menu_scroll==t->file_scroll &&
        a->request_id==t->request && a->page.cells==t->page && a->book.dirty==t->dirty && active_sheet(a)->undo_actions_count==t->history && jobs &&
        g.row==t->selection.row && g.col==t->selection.col && g.end_row==t->selection.end_row && g.end_col==t->selection.end_col;
}
static bool review_smoke_file_targets(App *a,bool scrolling) {
    DxReviewRegistry *registry=a->review->registry; UIChoiceListLayout list=file_menu_layout(a); DxReviewTarget picked;
    const DxReviewWidget *menu=dx_review_find(registry,"file.menu"),*heading=dx_review_find(registry,"file.heading");
    if (!menu || !heading || !list.frame.valid || registry->duplicate_count || registry->invalid_count || !menu->container || !menu->expanded || menu->input_eligible || heading->input_eligible) return false;
    if (menu->bounds.x!=list.frame.panel.x || menu->bounds.y!=list.frame.panel.y || menu->bounds.width!=list.frame.panel.width || menu->bounds.height!=list.frame.panel.height) return false;
    if (!dx_review_pick(registry,menu->bounds.x+2*a->ui.scale,menu->bounds.y+2*a->ui.scale,DX_REVIEW_PICK_CONTROL,&picked) || strcmp(picked.widget.id,"file.background")) return false;
    if (!dx_review_pick(registry,heading->bounds.x+heading->bounds.width*.5f,heading->bounds.y+heading->bounds.height*.5f,DX_REVIEW_PICK_CONTROL,&picked) || strcmp(picked.widget.id,"file.heading")) return false;
    static const char *const suffix[]={"new","open","save","save_as","close","quit"};
    for (int action=0;action<UI_KEYTIP_FILE_COUNT;action++) {
        char id[64]; snprintf(id,sizeof(id),"file.action.%s",suffix[action]);
        const DxReviewWidget *w=dx_review_find(registry,id); bool visible=action>=a->file_menu_scroll && action<a->file_menu_scroll+list.visible;
        if (!visible) { if (w) return false; continue; }
        if (!w || strcmp(w->parent_id,"file.menu") || w->enabled!=file_action_available(a,action) || w->input_eligible!=w->enabled || w->focused!=(a->file_menu_focus==action)) return false;
        float x=w->bounds.x+w->bounds.width*.5f,y=w->bounds.y+w->bounds.height*.5f;
        if (!dx_review_pick(registry,x,y,DX_REVIEW_PICK_CONTROL,&picked) || strcmp(picked.widget.id,id) || !dx_review_pick(registry,x,y,DX_REVIEW_PICK_CONTAINER,&picked) || strcmp(picked.widget.id,"file.menu")) return false;
    }
    const DxReviewWidget *track=dx_review_find(registry,"file.scrollbar"),*thumb=dx_review_find(registry,"file.scrollbar.thumb");
    if (!scrolling) return !track && !thumb;
    if (!track || !thumb || strcmp(thumb->parent_id,"file.scrollbar") || track->bounds.width!=8*a->ui.scale || thumb->bounds.y<=track->bounds.y) return false;
    return dx_review_pick(registry,thumb->bounds.x+thumb->bounds.width*.5f,thumb->bounds.y+thumb->bounds.height*.5f,DX_REVIEW_PICK_CONTROL,&picked) && !strcmp(picked.widget.id,"file.scrollbar.thumb") &&
        dx_review_pick(registry,track->bounds.x+a->ui.scale,track->bounds.y+a->ui.scale,DX_REVIEW_PICK_CONTROL,&picked) && !strcmp(picked.widget.id,"file.scrollbar");
}
static void review_smoke_choice_remember(App *a,ReviewSmoke *t) {
    t->request=a->request_id; t->page=a->page.cells; t->selection=grid_location(&a->grid); t->dirty=a->book.dirty; t->history=active_sheet(a)->undo_actions_count;
    t->choice_modal=a->modal; t->choice_focus=a->modal==MODAL_RIBBON ? a->ribbon_menu_focus : a->context_focus;
    t->choice_ribbon_mode=a->ribbon_mode; t->choice_scale=a->ui.scale; t->choice_row_height=a->grid.row_height; t->choice_formats=active_sheet(a)->formats.revision;
    dx_lock(&a->worker.mutex); t->jobs=a->worker.jobs_started; dx_unlock(&a->worker.mutex);
}
static bool review_smoke_choice_same(App *a,ReviewSmoke *t) {
    GridLocation g=grid_location(&a->grid); int focus=a->modal==MODAL_RIBBON ? a->ribbon_menu_focus : a->context_focus;
    dx_lock(&a->worker.mutex); bool jobs=a->worker.jobs_started==t->jobs; dx_unlock(&a->worker.mutex);
    return a->modal==t->choice_modal && focus==t->choice_focus && a->ribbon_mode==t->choice_ribbon_mode && a->ui.scale==t->choice_scale && a->grid.row_height==t->choice_row_height &&
        a->request_id==t->request && a->page.cells==t->page && a->book.dirty==t->dirty && active_sheet(a)->undo_actions_count==t->history && active_sheet(a)->formats.revision==t->choice_formats && jobs &&
        g.row==t->selection.row && g.col==t->selection.col && g.end_row==t->selection.end_row && g.end_col==t->selection.end_col;
}
static bool review_smoke_choice_targets(App *a,const char *root,const char *const *options,int count,int selected,int focus) {
    DxReviewRegistry *registry=a->review->registry; DxReviewTarget picked; char id[128];
    const DxReviewWidget *menu=dx_review_find(registry,root);
    if (!menu || !menu->container || !menu->expanded || menu->input_eligible || registry->duplicate_count || registry->invalid_count) return false;
    snprintf(id,sizeof(id),"%s.background",root);
    if (!dx_review_pick(registry,menu->bounds.x+2*a->ui.scale,menu->bounds.y+2*a->ui.scale,DX_REVIEW_PICK_CONTROL,&picked) || strcmp(picked.widget.id,id)) return false;
    snprintf(id,sizeof(id),"%s.heading",root); const DxReviewWidget *heading=dx_review_find(registry,id);
    if (!heading || heading->input_eligible || strcmp(heading->parent_id,root) || !dx_review_pick(registry,heading->bounds.x+heading->bounds.width*.5f,heading->bounds.y+heading->bounds.height*.5f,DX_REVIEW_PICK_CONTROL,&picked) || strcmp(picked.widget.id,id)) return false;
    for (int i=0;i<count;i++) {
        snprintf(id,sizeof(id),"%s.option.%s",root,options[i]); const DxReviewWidget *w=dx_review_find(registry,id);
        if (!w || !w->enabled || !w->input_eligible || w->checked!=(i==selected) || w->focused!=(i==focus) || strcmp(w->role,"menuitemradio") || strcmp(w->parent_id,root) || w->layer<=menu->layer) return false;
        if (w->transform.ty!=menu->transform.ty || w->clip.x<menu->bounds.x || w->clip.y<menu->bounds.y || w->clip.x+w->clip.width>menu->bounds.x+menu->bounds.width || w->clip.y+w->clip.height>menu->bounds.y+menu->bounds.height) return false;
        float x=w->bounds.x+w->bounds.width*.5f,y=w->bounds.y+w->bounds.height*.5f;
        if (!dx_review_pick(registry,x,y,DX_REVIEW_PICK_CONTROL,&picked) || strcmp(picked.widget.id,id) || !dx_review_pick(registry,x,y,DX_REVIEW_PICK_CONTAINER,&picked) || strcmp(picked.widget.id,root)) return false;
    }
    return !dx_review_find(registry,"file.menu");
}
static void review_smoke_tick(App *a,ReviewSmoke *t) {
    DxReviewCapture *r=a->review; if (!r || t->stage<0 || t->stage>=REVIEW_SMOKE_STAGES) return;
    if (t->diagnostic_stage!=t->stage) {
        fprintf(stderr,"Review smoke stage=%d phase=%d file=%d/%d/%d ui_scale=%.2f window=%dx%d target=%s\n",t->stage,(int)r->phase,a->file_menu,a->file_menu_focus,a->file_menu_scroll,a->ui.scale,GetScreenWidth(),GetScreenHeight(),r->target.widget.id);
        t->diagnostic_stage=t->stage;
    }
    switch (t->stage) {
    case 0:
        if (a->busy || a->page.count_pending || a->page.column_count!=3) return;
        if (!review_can_capture(a)) return;
        {
            bool *drags[]={&a->analysis_vbar.dragging,&a->matrix_hbar.dragging,&a->matrix_vbar.dragging,
                &a->dock_scrollbars[0].dragging,&a->dock_scrollbars[1].dragging,&a->author_scrollbar.dragging,
                &a->source_preview.dragging,&a->context_drag,&a->inspector_drag,&a->table_relation_drag,
                &a->pivot_search.dragging,&a->chart_columns.dragging,&a->sort_editor.search.dragging};
            for (size_t i=0;i<sizeof(drags)/sizeof(*drags);i++) {
                *drags[i]=true; bool captured=review_can_capture(a); *drags[i]=false;
                if (captured) goto failed;
            }
            a->file_menu_drag=true;
            if (!review_can_capture(a)) goto failed; /* A closed menu owns no active gesture. */
            a->file_menu=true; if (review_can_capture(a)) goto failed;
            a->file_menu=a->file_menu_drag=false;
        }
        grid_select_cell(&a->grid,1,1,false); a->grid.end_row=3; open_modal(a,MODAL_FILTER); break;
    case 1:
        if (a->values_loading || a->values_total<0 || a->popup_presence.offset || a->popup_presence.alpha!=1) return;
        a->filter_focus=3; input_set(&a->input,""); a->input.cursor=a->input.anchor=0;
        t->request=a->request_id; t->page=a->page.cells; t->selection=grid_location(&a->grid); t->cursor=a->input.cursor; t->anchor=a->input.anchor; t->dirty=a->book.dirty; t->history=active_sheet(a)->undo_actions_count;
        snprintf(t->root,sizeof(t->root),"%s",r->root); dx_lock(&a->worker.mutex); t->jobs=a->worker.jobs_started; dx_unlock(&a->worker.mutex);
        ui_note_key_press(KEY_PERIOD,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 2: {
        if (r->phase==DX_REPORT_ARMED) return;
        if (r->phase!=DX_REPORT_PICK || !review_smoke_same(a,t) || !r->image.data || r->image.width!=GetRenderWidth() || r->registry->duplicate_count || r->registry->invalid_count || !dx_review_find(r->registry,"filter.search")) goto failed;
        printf("Review captured frame=%llu widgets=%zu pixels=%dx%d capture_ms=%.3f\n",(unsigned long long)r->frame.frame_id,r->registry->count,r->image.width,r->image.height,r->capture_ms);
        review_smoke_pick(a,"filter.search"); break;
    }
    case 3:
        if (r->phase!=DX_REPORT_NOTE) return;
        if (strcmp(r->target.widget.id,"filter.search") || !review_smoke_same(a,t)) goto failed;
        input_insert(&r->note,"Native filter review: Türkçe, NULL, empty text \"\".\nKeep the menu and selection.");
        r->focus=1;
        review_smoke_click((Rectangle){GetScreenWidth()*.5f,GetScreenHeight()*.5f,1,1});
        ui_note_key_press(KEY_ENTER,DX_MOD_SUPER); break;
    case 4:
        if (r->phase==DX_REPORT_WRITING) {
            uint64_t revision=r->note.revision; bool selected=r->note.select_all;
            review_native_command(a,DX_COMMAND_SELECT_ALL); review_native_command(a,DX_COMMAND_UNDO);
            if (r->note.revision!=revision || r->note.select_all!=selected || r->focus!=0) goto failed;
            return;
        }
        if (r->phase==DX_REPORT_NOTE) return;
        if (r->phase!=DX_REPORT_SAVED || !r->last_report_id[0] || !review_smoke_same(a,t)) goto failed;
        printf("Review report one: %s capture_ms=%.3f write_ms=%.3f\n",r->last_report_id,r->capture_ms,r->write_ms);
        ui_note_key_press(KEY_ENTER,0); break;
    case 5:
        if (r->phase!=DX_REPORT_IDLE) return;
        if (!review_smoke_same(a,t)) goto failed;
        dx_lock(&a->worker.mutex); bool same_jobs=a->worker.jobs_started==t->jobs; dx_unlock(&a->worker.mutex); if (!same_jobs) goto failed;
        ui_set_scale(&a->ui,1.5f); SetWindowSize(760,640); a->filter_condition_menu=true; a->filter_condition_index=1;
        setenv("DUXCEL_REVIEW_TEST_FAIL_CAPTURE","1",1); ui_note_key_press(KEY_PERIOD,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 6:
        if (r->phase==DX_REPORT_ARMED) return;
        if (r->phase!=DX_REPORT_ERROR || r->image.data || !a->filter_condition_menu) goto failed;
        unsetenv("DUXCEL_REVIEW_TEST_FAIL_CAPTURE"); r->focus=2; ui_note_key_press(KEY_ENTER,0); break;
    case 7:
        if (r->phase==DX_REPORT_ERROR || r->phase==DX_REPORT_ARMED) return;
        if (r->phase!=DX_REPORT_PICK || r->frame.ui_scale!=1.5f || !a->filter_condition_menu) goto failed;
        review_smoke_pick(a,"filter.condition.option.equal"); break;
    case 8:
        if (r->phase!=DX_REPORT_NOTE) return;
        if (strcmp(r->target.widget.id,"filter.condition.option.equal") || !a->filter_condition_menu) goto failed;
        input_insert(&r->note,"Narrow 150% submenu. Retry must retain this note."); setenv("DUXCEL_REVIEW_OUTBOX","/dev/null/outbox",1); ui_note_key_press(KEY_ENTER,DX_MOD_SUPER); break;
    case 9:
        if (r->phase==DX_REPORT_NOTE || r->phase==DX_REPORT_WRITING) return;
        if (r->phase!=DX_REPORT_ERROR || !strstr(r->note.text,"retain this note")) goto failed;
        setenv("DUXCEL_REVIEW_OUTBOX",t->root,1); ui_note_key_press(KEY_ENTER,DX_MOD_SUPER); break;
    case 10:
        if (r->phase==DX_REPORT_WRITING || r->phase==DX_REPORT_ERROR) return;
        if (r->phase!=DX_REPORT_SAVED || !a->filter_condition_menu) goto failed;
        printf("Review report two: %s capture_ms=%.3f write_ms=%.3f\n",r->last_report_id,r->capture_ms,r->write_ms);
        ui_note_key_press(KEY_ENTER,0); break;
    case 11:
        if (r->phase!=DX_REPORT_IDLE) return;
        if (!review_smoke_same(a,t) || !a->filter_condition_menu) goto failed;
        close_filter(a); ui_set_scale(&a->ui,1); SetWindowSize(1280,820); a->grid.freeze=true; a->grid.top=1; a->grid.horizontal=120;
        ui_note_key_press(KEY_PERIOD,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 12: {
        if (r->phase!=DX_REPORT_PICK) return;
        Vector2 p={grid_column_x(&a->grid,a->ui.scale*a->grid.zoom,1)+12,a->grid.body.y+12};
        review_smoke_click((Rectangle){p.x,p.y,1,1}); break;
    }
    case 13:
        if (r->phase!=DX_REPORT_NOTE) return;
        if (!r->target.widget.data.has_cell || r->target.widget.data.row!=1 || r->target.widget.data.column!=1 || !r->target.widget.value.present || r->target.widget.value.is_null || strcmp(r->target.widget.value.exact_text,"NULL")) goto failed;
        ui_note_key_press(KEY_ESCAPE,0); break;
    case 14:
        if (r->phase!=DX_REPORT_IDLE) return;
        if (a->modal!=MODAL_NONE || a->grid.top!=1) goto failed;
        a->ribbon_mode=0; a->ribbon_tab=RIBBON_HOME; ui_note_key_press(KEY_PERIOD,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 15:
        if (r->phase!=DX_REPORT_PICK) return;
        review_smoke_pick(a,"ribbon.0.command.font"); break;
    case 16:
        if (r->phase!=DX_REPORT_NOTE) return;
        if (r->target.widget.enabled || strcmp(r->target.widget.action_id,"command.font")) goto failed;
        ui_note_key_press(KEY_ESCAPE,0); break;
    case 17:
        if (r->phase!=DX_REPORT_IDLE) return;
        /* A reply may finish while reporting; its owned Page remains queued. */
        request_page(a,false); ui_note_key_press(KEY_PERIOD,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 18: {
        if (r->phase!=DX_REPORT_PICK) return;
        dx_lock(&a->worker.mutex); bool ready=a->worker.reply!=NULL; dx_unlock(&a->worker.mutex);
        if (!ready) return;
        if (!a->busy) goto failed;
        ui_note_key_press(KEY_ESCAPE,0);
        break;
    }
    case 19:
        if (r->phase!=DX_REPORT_IDLE || a->busy) return;
        ui_note_key_press(KEY_PERIOD,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 20:
        if (r->phase!=DX_REPORT_PICK) return;
        if (!dx_review_find(r->registry,"pane.fields.close") || !dx_review_find(r->registry,"pane.workbook.add_sheet")) goto failed;
        review_smoke_pick(a,"pane.fields.close"); break;
    case 21:
        if (r->phase!=DX_REPORT_NOTE) return;
        if (strcmp(r->target.widget.id,"pane.fields.close") || !a->fields_visible) goto failed;
        ui_note_key_press(KEY_ESCAPE,0); break;
    case 22:
        if (r->phase!=DX_REPORT_IDLE) return;
        /* History lives in More only below 700 logical units. At the wide
           fixture size there is no More menu to capture. */
        SetWindowSize(640,640); a->ribbon_mode=2;
        open_command_group(a,-1,0); ui_note_key_press(KEY_PERIOD,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 23:
        if (r->phase!=DX_REPORT_PICK) return;
        if (!dx_review_find(r->registry,"ribbon.more.panel")) goto failed;
        review_smoke_pick(a,"ribbon.more.undo"); break;
    case 24:
        if (r->phase!=DX_REPORT_NOTE) return;
        if (r->target.widget.enabled || strcmp(r->target.widget.action_id,"command.undo") || a->modal!=MODAL_COMMANDS) goto failed;
        ui_note_key_press(KEY_ESCAPE,0); break;
    case 25: {
        if (r->phase!=DX_REPORT_IDLE) return;
        if (a->modal!=MODAL_COMMANDS || !a->fields_visible) goto failed;
        /* Frozen/capture frames must contribute real samples, not zeros in
           the denominator of frame-tail and mean measurements. */
        for (unsigned i=0;i<a->frames && i<600;i++)
            if (a->frame_samples[i]<=0 || a->render_ms[i]<=0) goto failed;
        close_command_group(a);
        break;
    }
    case 26:
        if (r->phase!=DX_REPORT_IDLE) return;
        ui_set_scale(&a->ui,1); SetWindowSize(1280,820); a->ribbon_mode=0; a->ribbon_tab=RIBBON_HOME;
        ui_note_key_press(KEY_PERIOD,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 27:
        if (r->phase!=DX_REPORT_PICK) return;
        if (!review_smoke_chrome(a)) goto failed;
        review_smoke_pick(a,"ribbon.tab.home"); break;
    case 28:
        if (r->phase!=DX_REPORT_NOTE) return;
        if (strcmp(r->target.widget.id,"ribbon.tab.home") || !r->target.widget.checked || a->ribbon_tab!=RIBBON_HOME) goto failed;
        ui_note_key_press(KEY_ESCAPE,0); break;
    case 29:
        if (r->phase!=DX_REPORT_IDLE) return;
        ui_set_scale(&a->ui,1.5f);
        if (a->ui.scale!=1.5f) return; /* ui_update applies requested scale next frame. */
        SetWindowMinSize(400,640); SetWindowSize(400,640); a->ribbon_tabs_scroll=1000000; a->ribbon_focus=false;
        ui_note_key_press(KEY_PERIOD,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 30: {
        if (r->phase!=DX_REPORT_PICK) return;
        const DxReviewWidget *home=dx_review_find(r->registry,"ribbon.tab.home");
        if (r->frame.ui_scale!=1.5f || !review_smoke_chrome(a) || a->ribbon_tabs_scroll<=0 || !home || home->bounds.x>=home->clip.x) { fprintf(stderr,"FAIL: review tab clipping scale=%.2f scroll=%.2f home=%.2f clip=%.2f\n",r->frame.ui_scale,a->ribbon_tabs_scroll,home ? home->bounds.x : 0,home ? home->clip.x : 0); goto failed; }
        review_smoke_pick(a,"ribbon.display"); break;
    }
    case 31:
        if (r->phase!=DX_REPORT_NOTE) return;
        if (strcmp(r->target.widget.id,"ribbon.display") || a->modal!=MODAL_NONE) goto failed;
        ui_note_key_press(KEY_ESCAPE,0); break;
    case 32:
        if (r->phase!=DX_REPORT_IDLE) return;
        ui_note_key_press(KEY_PERIOD,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 33: {
        if (r->phase!=DX_REPORT_PICK) return;
        const DxReviewWidget *brand=dx_review_find(r->registry,"chrome.brand");
        if (!brand) goto failed;
        r->pick_index=(int)(brand-r->registry->widgets)-1; ui_note_key_press(KEY_TAB,0); break;
    }
    case 34:
        if (r->phase!=DX_REPORT_PICK || !r->keyboard_pick || strcmp(r->target.widget.id,"chrome.brand")) goto failed;
        ui_note_key_press(KEY_ENTER,0); break;
    case 35:
        if (r->phase!=DX_REPORT_NOTE) return;
        if (strcmp(r->target.widget.id,"chrome.brand") || r->target.widget.input_eligible) goto failed;
        ui_note_key_press(KEY_ESCAPE,0); break;
    case 36:
        if (r->phase!=DX_REPORT_IDLE) return;
        SetWindowMinSize(640,640); SetWindowSize(1280,820); ui_set_scale(&a->ui,1);
        break;
    case 37:
        if (a->ui.scale!=1 || r->phase!=DX_REPORT_IDLE) return;
        a->file_menu=true; a->file_menu_focus=UI_KEYTIP_FILE_SAVE; a->file_menu_scroll=0; a->file_menu_frame=a->frames; a->io_busy=true;
        t->request=a->request_id; t->page=a->page.cells; t->selection=grid_location(&a->grid); t->dirty=a->book.dirty; t->history=active_sheet(a)->undo_actions_count;
        t->file_focus=a->file_menu_focus; t->file_scroll=a->file_menu_scroll;
        dx_lock(&a->worker.mutex); t->jobs=a->worker.jobs_started; dx_unlock(&a->worker.mutex);
        ui_note_key_press(KEY_PERIOD,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 38:
        if (r->phase!=DX_REPORT_PICK) return;
        if (!review_smoke_file_targets(a,false) || !review_smoke_file_same(a,t)) goto failed;
        review_smoke_pick(a,"file.action.save"); break;
    case 39:
        if (r->phase!=DX_REPORT_NOTE) return;
        if (strcmp(r->target.widget.id,"file.action.save") || r->target.widget.enabled || r->target.widget.input_eligible || !r->target.widget.focused || !review_smoke_file_same(a,t)) goto failed;
        ui_note_key_press(KEY_ESCAPE,0); break;
    case 40:
        if (r->phase!=DX_REPORT_IDLE) return;
        if (!review_smoke_file_same(a,t)) goto failed;
        a->io_busy=false; ui_set_scale(&a->ui,1.5f); SetWindowMinSize(400,180); SetWindowSize(640,200); break;
    case 41:
        if (a->ui.scale!=1.5f || r->phase!=DX_REPORT_IDLE) return;
        {
            UIChoiceListLayout list=file_menu_layout(a); if (!list.frame.valid || list.visible!=2) goto failed;
            a->file_menu_scroll=UI_KEYTIP_FILE_COUNT-list.visible; a->file_menu_focus=UI_KEYTIP_FILE_CLOSE; a->file_menu_frame=a->frames-1;
            t->file_focus=a->file_menu_focus; t->file_scroll=a->file_menu_scroll;
        }
        ui_note_key_press(KEY_PERIOD,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 42:
        if (r->phase!=DX_REPORT_PICK) return;
        if (r->frame.ui_scale!=1.5f || !review_smoke_file_targets(a,true) || !review_smoke_file_same(a,t)) goto failed;
        /* The bottom picker bar covers this short menu's thumb. Move it by
           the real Space shortcut before picking the exposed frozen target. */
        if (!r->toolbar_top) ui_note_key_press(KEY_SPACE,0);
        review_smoke_pick(a,"file.scrollbar.thumb"); break;
    case 43:
        if (r->phase!=DX_REPORT_NOTE) return;
        if (strcmp(r->target.widget.id,"file.scrollbar.thumb") || !review_smoke_file_same(a,t)) goto failed;
        ui_note_key_press(KEY_ESCAPE,0); break;
    case 44:
        if (r->phase!=DX_REPORT_IDLE) return;
        if (!review_smoke_file_same(a,t)) goto failed;
        a->file_menu=a->key_tips=a->ribbon_focus=false; ui_keytips_cancel(&a->keytip_state);
        ui_set_scale(&a->ui,1); SetWindowMinSize(640,640); SetWindowSize(1280,820); break;
    case 45:
        if (r->phase!=DX_REPORT_IDLE || a->ui.scale!=1) return;
        if (a->file_menu || a->io_busy || a->file_menu_drag) goto failed;
        break;
    case 46: {
        printf("PASS: native review filter/selection isolation, typed frozen cell, disabled ribbon/overflow, pane controls, 100/150 percent, capture/write retry, durable reports, queued query reply, title/ribbon pointer/keyboard targets and bounded File menu targets\n"); break;
    }
    case 47:
        if (r->phase!=DX_REPORT_IDLE) return;
        a->modal=MODAL_RIBBON; a->ribbon_menu_focus=a->ribbon_mode;
        if (!a->popup_presence.active || a->popup_presence.alpha!=1 || a->popup_presence.offset) return;
        review_smoke_choice_remember(a,t); ui_note_key_press(KEY_PERIOD,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 48: {
        if (r->phase!=DX_REPORT_PICK) return;
        const char *options[]={"full","compact","icons","focus"};
        if (!review_smoke_choice_same(a,t) || !review_smoke_choice_targets(a,"choice.ribbon_display",options,4,a->ribbon_mode,a->ribbon_menu_focus)) goto failed;
        review_smoke_pick(a,"choice.ribbon_display.option.full"); break;
    }
    case 49:
        if (r->phase!=DX_REPORT_NOTE) return;
        if (strcmp(r->target.widget.id,"choice.ribbon_display.option.full") || !r->target.widget.checked || !r->target.widget.focused || !review_smoke_choice_same(a,t)) goto failed;
        ui_note_key_press(KEY_ESCAPE,0); break;
    case 50:
        if (r->phase!=DX_REPORT_IDLE) return;
        if (!review_smoke_choice_same(a,t)) goto failed;
        a->modal=MODAL_NONE; ui_set_scale(&a->ui,1.5f); SetWindowSize(760,640); break;
    case 51:
        if (r->phase!=DX_REPORT_IDLE || a->ui.scale!=1.5f) return;
        if (a->modal!=MODAL_INTERFACE_SCALE) { begin_scale_menu(a,(Vector2){300,280}); a->context_focus=3; }
        if (!a->popup_presence.active || a->popup_presence.alpha!=1 || a->popup_presence.offset) return;
        review_smoke_choice_remember(a,t); ui_note_key_press(KEY_PERIOD,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 52: {
        if (r->phase!=DX_REPORT_PICK) return;
        const char *options[]={"compact","standard","large","extra_large"};
        if (r->frame.ui_scale!=1.5f || !review_smoke_choice_same(a,t) || !review_smoke_choice_targets(a,"choice.interface_scale",options,4,3,3)) goto failed;
        review_smoke_pick(a,"choice.interface_scale.option.extra_large"); break;
    }
    case 53:
        if (r->phase!=DX_REPORT_NOTE) return;
        if (strcmp(r->target.widget.id,"choice.interface_scale.option.extra_large") || !r->target.widget.checked || !r->target.widget.focused || !review_smoke_choice_same(a,t)) goto failed;
        ui_note_key_press(KEY_ESCAPE,0); break;
    case 54:
        if (r->phase!=DX_REPORT_IDLE) return;
        if (a->modal==MODAL_INTERFACE_SCALE) { if (!review_smoke_choice_same(a,t)) goto failed; a->modal=MODAL_DENSITY; a->context_origin=(Vector2){300,280}; a->context_focus=1; }
        if (!a->popup_presence.active || a->popup_presence.alpha!=1 || a->popup_presence.offset) return;
        review_smoke_choice_remember(a,t); ui_note_key_press(KEY_PERIOD,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 55: {
        if (r->phase!=DX_REPORT_PICK) return;
        const char *options[]={"compact","default","comfortable"};
        if (!review_smoke_choice_same(a,t) || !review_smoke_choice_targets(a,"choice.row_density",options,3,1,1)) goto failed;
        review_smoke_pick(a,"choice.row_density.option.default"); break;
    }
    case 56:
        if (r->phase!=DX_REPORT_NOTE) return;
        if (strcmp(r->target.widget.id,"choice.row_density.option.default") || !r->target.widget.checked || !r->target.widget.focused || !review_smoke_choice_same(a,t)) goto failed;
        ui_note_key_press(KEY_ESCAPE,0); break;
    case 57:
        if (r->phase!=DX_REPORT_IDLE) return;
        if (a->modal==MODAL_DENSITY) {
            if (!review_smoke_choice_same(a,t)) goto failed;
            a->modal=MODAL_FORMAT; a->format_action=ACTION_TEXT_COLOR; a->format_origin=(Vector2){300,280}; a->format_return_group=a->format_return_command=-1; a->context_focus=3;
        }
        if (!a->popup_presence.active || a->popup_presence.alpha!=1 || a->popup_presence.offset) return;
        review_smoke_choice_remember(a,t); ui_note_key_press(KEY_PERIOD,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 58: {
        if (r->phase!=DX_REPORT_PICK) return;
        const char *options[]={"automatic","blue","teal","red"};
        if (!review_smoke_choice_same(a,t) || !review_smoke_choice_targets(a,"choice.text_color",options,4,-1,3)) goto failed;
        review_smoke_pick(a,"choice.text_color.option.red"); break;
    }
    case 59:
        if (r->phase!=DX_REPORT_NOTE) return;
        if (strcmp(r->target.widget.id,"choice.text_color.option.red") || r->target.widget.checked || !r->target.widget.focused || !review_smoke_choice_same(a,t)) goto failed;
        ui_note_key_press(KEY_ESCAPE,0); break;
    case 60:
        if (r->phase!=DX_REPORT_IDLE) return;
        if (!review_smoke_choice_same(a,t)) goto failed;
        close_format_menu(a); ui_set_scale(&a->ui,1); SetWindowSize(1280,820); break;
    case 61: {
        if (r->phase!=DX_REPORT_IDLE || a->ui.scale!=1) return;
        if (a->modal!=MODAL_NONE || a->ui.scissor_depth || a->ui.paint_depth) goto failed;
        printf("PASS: native headed choice targets retain checked/focused state, popup clipping and frozen ribbon, UI scale, row density and text color\n"); break;
    }
    case 62:
        if (r->phase!=DX_REPORT_IDLE) return;
        SetWindowMinSize(400,180); SetWindowSize(640,200); ui_set_scale(&a->ui,1.5f); break;
    case 63:
        if (r->phase!=DX_REPORT_IDLE || a->ui.scale!=1.5f) return;
        a->file_menu=true; a->file_menu_focus=UI_KEYTIP_FILE_CLOSE; a->file_menu_scroll=4; a->file_menu_frame=a->frames-1;
        t->request=a->request_id; t->page=a->page.cells; t->selection=grid_location(&a->grid); t->dirty=a->book.dirty; t->history=active_sheet(a)->undo_actions_count;
        t->file_focus=a->file_menu_focus; t->file_scroll=a->file_menu_scroll;
        dx_lock(&a->worker.mutex); t->jobs=a->worker.jobs_started; dx_unlock(&a->worker.mutex);
        ui_note_key_press(KEY_PERIOD,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 64:
        if (r->phase!=DX_REPORT_PICK) return;
        if (!review_smoke_file_targets(a,true) || !review_smoke_file_same(a,t)) goto failed;
        if (!r->toolbar_top) ui_note_key_press(KEY_SPACE,0);
        review_smoke_pick(a,"file.scrollbar.thumb"); break;
    case 65: {
        if (r->phase!=DX_REPORT_NOTE) return;
        DxReviewCaptureLayout layout=dx_review_capture_layout(GetScreenWidth(),GetScreenHeight(),a->ui.scale,r->toolbar_top);
        if (strcmp(r->target.widget.id,"file.scrollbar.thumb") || r->frame.ui_scale!=1.5f || !layout.compact || layout.scale>=a->ui.scale || layout.note.height+.01f<37*layout.scale || !review_smoke_file_same(a,t)) goto failed;
        Rectangle actions[]={layout.cancel,layout.pick,layout.save};
        for (size_t i=0;i<sizeof(actions)/sizeof(*actions);i++) if (actions[i].x<0 || actions[i].y<0 || actions[i].x+actions[i].width>GetScreenWidth() || actions[i].y+actions[i].height>GetScreenHeight()) goto failed;
        r->focus=3; review_smoke_click(layout.note); break;
    }
    case 66:
        if (r->phase!=DX_REPORT_NOTE) return;
        if (r->focus!=0 || !review_smoke_file_same(a,t)) goto failed;
        input_insert(&r->note,"Short native composer.\nNULL, empty text, literal NULL.\nTailx");
        ui_note_key_press(KEY_BACKSPACE,0); break;
    case 67: {
        if (r->phase!=DX_REPORT_NOTE) return;
        if (strcmp(r->note.text,"Short native composer.\nNULL, empty text, literal NULL.\nTail") || r->editor.lines.count<3 || r->editor.scroll<=0 || !r->note.multiline || !review_smoke_file_same(a,t)) goto failed;
        DxReviewCaptureLayout layout=dx_review_capture_layout(GetScreenWidth(),GetScreenHeight(),a->ui.scale,r->toolbar_top);
        review_smoke_click(layout.save); break;
    }
    case 68:
        if (r->phase==DX_REPORT_NOTE || r->phase==DX_REPORT_WRITING) return;
        if (r->phase!=DX_REPORT_SAVED || !r->last_report_id[0] || !review_smoke_file_same(a,t)) goto failed;
        printf("Review report three: %s capture_ms=%.3f write_ms=%.3f compact_note_lines=%zu\n",r->last_report_id,r->capture_ms,r->write_ms,r->editor.lines.count);
        ui_note_key_press(KEY_ENTER,0); break;
    case 69:
        if (r->phase!=DX_REPORT_IDLE) return;
        if (!review_smoke_file_same(a,t)) goto failed;
        a->file_menu=false; ui_set_scale(&a->ui,1); SetWindowMinSize(640,640); SetWindowSize(1280,820); break;
    case 70: {
        if (r->phase!=DX_REPORT_IDLE || a->ui.scale!=1) return;
        if (a->modal!=MODAL_NONE || a->ui.scissor_depth || a->ui.paint_depth || a->file_menu || a->io_busy) goto failed;
        printf("PASS: short 150 percent review composer has a visible multiline editor, pointer focus, actual Backspace and pointer Save with retained frozen state\nReview smoke completed [0,%d)\n",REVIEW_SMOKE_STAGES); break;
    }
    }
    t->stage++; return;
failed:
    fprintf(stderr,"FAIL: native review stage=%d phase=%d target=%s error=%s\n",t->stage,(int)r->phase,r->target.widget.id,r->error); t->stage=-1;
}
#endif
static void review_group(App *a,int group,Rectangle bounds,bool overflow) {
    if (!review_armed(a) || group<0 || group>=ribbon_group_counts[a->ribbon_tab]) return;
    static const char *const tabs[]={"home","insert","data","analyze","view","visualization"};
    static const char *const groups[RIBBON_TABS][UI_RIBBON_GROUPS]={
        {"clipboard","font","alignment","number","data","analysis"},{"analysis","workbook"},
        {"order","filter","steps"},{"visuals","structure","inspect","return"},
        {"panes","layout","worksheet","help"},{"visuals","structure","inspect","return"}};
    char id[128],action[96],path[192];
    snprintf(action,sizeof(action),"ribbon.group.%s.%s",tabs[a->ribbon_tab],groups[a->ribbon_tab][group]);
    snprintf(id,sizeof(id),"%s.%s",action,overflow ? "overflow" : "collapsed");
    snprintf(path,sizeof(path),"ribbon/%s/%s",tabs[a->ribbon_tab],overflow ? "more" : "collapsed");
    review_control(a,id,action,path,overflow ? "menuitem" : "button",ribbon_groups[a->ribbon_tab][group].name,bounds,overflow ? 1 : 20,true,a->ribbon_focus && ui_ribbon_group_for(ribbon_groups[a->ribbon_tab],ribbon_group_counts[a->ribbon_tab],a->ribbon_command)==group,false,__FILE__,__LINE__);
}
