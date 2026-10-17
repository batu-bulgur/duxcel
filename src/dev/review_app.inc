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
static uint64_t review_sheet_order(App *a) {
    uint64_t hash=UINT64_C(14695981039346656037);
    for (size_t i=0;i<a->book.count && i<DX_MAX_SHEETS;i++) {
        const unsigned char *name=(const unsigned char *)a->book.sheets[i].name;
        for (size_t n=0;name && n<4096 && name[n];n++) { hash^=name[n]; hash*=UINT64_C(1099511628211); }
        hash^=0; hash*=UINT64_C(1099511628211); hash^=a->book.sheets[i].hidden; hash*=UINT64_C(1099511628211);
    }
    hash^=a->book.count; return hash*UINT64_C(1099511628211);
}
static DxReviewDataIdentity review_sheet_scope(App *a,size_t sheet) {
    if (sheet>=a->book.count) return (DxReviewDataIdentity){0};
    DxReviewDataIdentity data=review_scope(a); snprintf(data.kind,sizeof(data.kind),"worksheet-position"); data.sheet_id=sheet+1;
    data.source_id=data.object_id=data.schema_generation=0; data.field[0]=0;
    /* Worksheet reorder physically moves the array. This is an owned position
       scoped to the captured document/order, never a persistent worksheet ID.
       The bounded digest was computed once at this armed frame's beginning. */
    const char *order=strstr(a->review->frame.state_text," workbook_order=");
    data.query_generation=order ? (uint64_t)strtoull(order+16,NULL,16) : review_sheet_order(a);
    return data;
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
    used=strlen(f.state_text); snprintf(f.state_text+used,sizeof(f.state_text)-used," workbook_order=%016llx",(unsigned long long)review_sheet_order(a));
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
/* Authoring controllers supply their actual drawn rectangles and states. Only
   the capture frame derives semantic identity here; no layout is recomputed,
   borrowed field text retained or query made by this adapter. */
static uint64_t review_field_hash(const char *text) {
    uint64_t hash=UINT64_C(14695981039346656037);
    for (const unsigned char *p=(const unsigned char *)(text ? text : "");*p;p++) { hash^=*p; hash*=UINT64_C(1099511628211); }
    return hash;
}
static bool review_author_dialog(const char *kind) {
    return !strcmp(kind,"column_width") || !strcmp(kind,"move_range") || !strcmp(kind,"object_name") ||
        !strcmp(kind,"chart_size") || !strcmp(kind,"unsaved") || !strcmp(kind,"source_query") || !strcmp(kind,"source_relation") || !strcmp(kind,"sort") || !strcmp(kind,"sheet_rename");
}
static bool review_author_field(char output[128],const char *field) {
    size_t length=strlen(field);
    if (length<128) { memcpy(output,field,length+1); return false; }
    size_t end=124; while (end && ((unsigned char)field[end]&0xc0)==0x80) end--;
    memcpy(output,field,end); memcpy(output+end,"…",4); return true;
}
static const char *review_author_identity(App *a,const char *kind,char parent[128],DxReviewDataIdentity *data,bool *truncated) {
    SheetObject *object=selected_object(a); const char *field=NULL,*scope=NULL;
    static const char *const roles[]={"rows","columns","values","filters"};
    static const char *const marks[]={"color","size","label","detail","tooltip","shape"};
    if (!strcmp(kind,"sheet_menu") || !strcmp(kind,"sheet_rename") || !strcmp(kind,"sheet_unhide")) {
        *data=review_sheet_scope(a,a->sheet_menu_index); snprintf(parent,128,"author.%s",kind); return NULL;
    }
    if (!strcmp(kind,"source_query")) {
        *data=review_scope(a); snprintf(data->kind,sizeof(data->kind),"source-query");
        data->source_id=data->object_id=data->schema_generation=0; data->query_generation=a->query_source_revision;
        data->has_cell=false; data->row=data->column=0; data->field[0]=0;
        ObjectStore *store=&active_sheet(a)->objects;
        if (a->query_object>=0 && (size_t)a->query_object<store->count && store->items[a->query_object].kind==OBJECT_TABLE) data->source_id=data->object_id=store->items[a->query_object].id;
        snprintf(parent,128,"author.%s",kind); return NULL;
    }
    if (!strcmp(kind,"source_relation")) {
        *data=review_scope(a); snprintf(data->kind,sizeof(data->kind),"source-relation");
        data->source_id=data->object_id=data->schema_generation=data->query_generation=0; data->has_cell=false; data->row=data->column=0; data->field[0]=0;
        ObjectStore *store=&active_sheet(a)->objects;
        if (a->table_relation_object>=0 && (size_t)a->table_relation_object<store->count && store->items[a->table_relation_object].kind==OBJECT_TABLE) {
            SheetObject *table=&store->items[a->table_relation_object]; data->source_id=data->object_id=table->id; data->query_generation=table->revision;
        }
        snprintf(parent,128,"author.%s",kind); return NULL;
    }
    if (!strcmp(kind,"sort")) {
        *data=review_scope(a); snprintf(data->kind,sizeof(data->kind),"sort-draft");
        data->source_id=data->object_id=data->schema_generation=0; data->query_generation=a->analysis_revision;
        data->has_cell=false; data->row=data->column=0; data->field[0]=0; ObjectStore *store=&active_sheet(a)->objects;
        if (a->sort_object>=0 && (size_t)a->sort_object<store->count && store->items[a->sort_object].kind==OBJECT_TABLE) {
            SheetObject *table=&store->items[a->sort_object]; data->source_id=data->object_id=table->id; data->query_generation=table->revision;
        }
        snprintf(parent,128,"author.%s",kind); return NULL;
    }
    *data=review_scope(a);
    if (object) {
        data->object_id=object->id; data->source_id=object->kind==OBJECT_TABLE ? object->id : object->source_id;
        data->query_generation=object->revision; data->schema_generation=object_source_revision(a,object);
    }
    if (!strcmp(kind,"pivot_field")) {
        int role=a->pivot_drag_role,index=a->pivot_drag_index;
        if (object && role>=0 && role<4 && index>=0 && (size_t)index<object->pivot.counts[role]) { scope=roles[role]; field=object->pivot.fields[role][index].name; }
        else if (a->pivot_drag_field>=0 && (size_t)a->pivot_drag_field<context_fields(a)->column_count) field=context_fields(a)->columns[a->pivot_drag_field].name;
    } else if (!strcmp(kind,"field") && a->pivot_field>=0 && (size_t)a->pivot_field<context_fields(a)->column_count) field=context_fields(a)->columns[a->pivot_field].name;
    else if (!strcmp(kind,"marks") && (unsigned)a->mark_role<DX_MARK_ROLES) scope=marks[a->mark_role];
    else if (!strcmp(kind,"assign_field") && (unsigned)a->pivot_role<4) scope=roles[a->pivot_role];
    if (!strcmp(kind,"step") && a->step_menu_index>=0 && (size_t)a->step_menu_index<a->pipeline_row_count) {
        static const char *const kinds[]={"source","unique","filter","group","sort"};
        const UIStep *step=&a->pipeline_rows[a->step_menu_index]; scope=(unsigned)step->kind<5 ? kinds[step->kind] : "unknown";
        field=step->column; data->row=step->index;
    } else if (!strcmp(kind,"context") || !strcmp(kind,"source_context")) {
        scope=a->grid.selection==SELECT_ROWS ? "rows" : a->grid.selection==SELECT_COLUMNS ? "columns" : "cells";
        data->has_cell=true; data->row=a->grid.row; data->column=a->grid.col;
        if (!strcmp(kind,"context")) snprintf(data->kind,sizeof(data->kind),"worksheet-position");
        else {
            Page *page=&a->page;
            if (object && object->kind==OBJECT_TABLE) { page=&a->objects[a->selected_object].page; data->row=a->grid.row-object->row-2; data->column=a->grid.col-object->col; }
            if (data->column<page->column_count) field=page->columns[data->column].name;
        }
    }
    snprintf(parent,128,"author.%s",kind);
    if (field && review_author_field(data->field,field) && truncated) *truncated=true;
    return scope;
}
static DxReviewWidget *review_author_widget(App *a,const char *id,const char *parent,const char *kind,const char *action,const char *role,const char *label,Rectangle bounds,Rectangle clip,int layer,bool enabled,bool checked,bool focused,bool container,const char *field,const char *file,int line) {
    if (!review_armed(a) || bounds.width<=0 || bounds.height<=0 || clip.width<=0 || clip.height<=0) return NULL;
    DxReviewWidget w; dx_review_widget_init(&w,id,action,"",role,label);
    const char *surface=review_author_dialog(kind) ? "dialog" : "popup";
    snprintf(w.path,sizeof(w.path),"%s/author/%s",surface,kind); snprintf(w.parent_id,sizeof(w.parent_id),"%s",parent ? parent : "");
    char identity[128]; const char *scope=review_author_identity(a,kind,identity,&w.data,&w.truncated);
    if (scope) snprintf(w.path,sizeof(w.path),"%s/author/%s/%s",surface,kind,scope);
    if (field) w.truncated|=review_author_field(w.data.field,field);
    w.bounds=review_rect(bounds); w.clip=review_rect(clip); w.layer=layer;
    if (a->popup_capturing) { w.transform.ty=a->popup_presence.offset*a->ui.scale; w.bounds.y+=w.transform.ty; w.clip.y+=w.transform.ty; }
    w.enabled=enabled; w.checked=checked; w.focused=focused; w.container=container; w.expanded=container;
    w.input_eligible=enabled && !container && strcmp(role,"text") && strcmp(role,"region");
    snprintf(w.source_file,sizeof(w.source_file),"%s",file); w.source_line=(uint32_t)line;
    DxReviewStatus status=dx_review_add(a->review->registry,&w);
    if (status!=DX_REVIEW_OK) { fprintf(stderr,"Review authoring registration %s: %s\n",id,dx_review_status_text(status)); return NULL; }
    return (DxReviewWidget *)dx_review_find(a->review->registry,id);
}
static void review_author_capture_state(App *a,const char *text) {
    /* Capture and registry each own their frame. Keep the diagnostic console
       and serialized archive identical without adding App state or idle work. */
    DxReviewFrame *frames[]={&a->review->frame,&a->review->registry->frame};
    for (size_t i=0;i<sizeof(frames)/sizeof(*frames);i++) {
        size_t used=strlen(frames[i]->state_text); int n=snprintf(frames[i]->state_text+used,sizeof(frames[i]->state_text)-used,"%s",text);
        if (n<0 || (size_t)n>=sizeof(frames[i]->state_text)-used) {
            frames[i]->truncated=true; a->review->registry->truncated=true;
            char *overflow=strstr(frames[i]->state_text," author_overflow="); if (overflow) overflow[17]='1';
        }
    }
}
static void review_author_capture_count(App *a,int count) {
    DxReviewFrame *frames[]={&a->review->frame,&a->review->registry->frame};
    for (size_t i=0;i<sizeof(frames)/sizeof(*frames);i++) {
        char *value=strstr(frames[i]->state_text," author_count=");
        if (value) { unsigned n=(unsigned)(count>16 ? 16 : count); value+=14; value[0]=(char)('0'+n/10); value[1]=(char)('0'+n%10); }
        value=strstr(frames[i]->state_text," author_overflow=");
        if (value) value[17]=count>16 || frames[i]->truncated ? '1' : '0';
    }
}
static void review_author_panel(App *a,const char *kind,const char *title,Rectangle panel,Rectangle heading,const char *file,int line) {
    if (!review_armed(a)) return;
    char parent[128],id[128]; DxReviewDataIdentity data; review_author_identity(a,kind,parent,&data,NULL);
    Rectangle clip=a->ui.clipping ? GetCollisionRec(panel,a->ui.clip) : panel;
    bool dialog=review_author_dialog(kind);
    if (dialog) {
        Rectangle viewport=box(0,0,GetScreenWidth(),GetScreenHeight()); char backdrop[128];
        snprintf(backdrop,sizeof(backdrop),"%s.backdrop",parent);
        review_author_widget(a,backdrop,NULL,kind,"author.inspect","region","Modal backdrop",viewport,viewport,129,true,false,false,true,NULL,file,line);
        snprintf(id,sizeof(id),"%s.background",backdrop);
        review_author_widget(a,id,backdrop,kind,"author.inspect","region","Modal backdrop",viewport,viewport,129,true,false,false,false,NULL,file,line);
    }
    review_author_widget(a,parent,NULL,kind,"author.inspect",dialog ? "dialog" : "menu",title,panel,clip,130,true,false,false,true,NULL,file,line);
    snprintf(id,sizeof(id),"%s.background",parent); review_author_widget(a,id,parent,kind,"author.inspect","region",dialog ? "Dialog background" : "Menu background",panel,clip,130,true,false,false,false,NULL,file,line);
    snprintf(id,sizeof(id),"%s.heading",parent); review_author_widget(a,id,parent,kind,"author.inspect","text",title,heading,clip,131,true,false,false,false,NULL,file,line);
    float offset=a->popup_capturing ? a->popup_presence.offset*a->ui.scale : 0;
    char state[480]; snprintf(state,sizeof(state)," author_kind=%s author_parent=%s author_count=00 author_overflow=0 author_panel=%.3f,%.3f,%.3f,%.3f author_heading=%.3f,%.3f,%.3f,%.3f",kind,parent,panel.x,panel.y+offset,panel.width,panel.height,heading.x,heading.y+offset,heading.width,heading.height);
    review_author_capture_state(a,state);
    if (!strcmp(kind,"source_query")) {
        char query[320]; snprintf(query,sizeof(query)," query_object=%d query_source_revision=%llu query_draft_revision=%llu query_focus=%d query_loading=%d query_cursor=%zu query_anchor=%zu query_scroll=%d",a->query_object,(unsigned long long)a->query_source_revision,(unsigned long long)a->input.revision,a->query_focus,a->query_loading,a->input.cursor,a->input.anchor,a->query_editor.scroll);
        review_author_capture_state(a,query);
    }
    if (!strcmp(kind,"source_relation")) {
        size_t count=a->table_relation_object>=0 && (size_t)a->table_relation_object<active_sheet(a)->objects.count ? a->objects[a->table_relation_object].page.relation_count : 0;
        char relation[192]; snprintf(relation,sizeof(relation)," relation_object=%d relation_source_revision=%llu relation_focus=%d relation_scroll=%d relation_count=%zu",a->table_relation_object,(unsigned long long)data.query_generation,a->table_relation_focus,a->table_relation_scroll,count);
        review_author_capture_state(a,relation);
    }
}
static void review_author_item(App *a,const char *kind,const char *key,const char *action,const char *role,const char *label,Rectangle bounds,Rectangle clip,bool enabled,bool checked,bool focused,const char *field,const char *file,int line) {
    if (!review_armed(a)) return;
    char parent[128],id[128]; DxReviewDataIdentity data; review_author_identity(a,kind,parent,&data,NULL);
    bool field_list=field && (!strcmp(kind,"marks") || !strcmp(kind,"assign_field") || !strcmp(kind,"plot"));
    int length=field_list ? snprintf(id,sizeof(id),"%s.action.%s.f%016llx",parent,key,(unsigned long long)review_field_hash(field)) : snprintf(id,sizeof(id),"%s.action.%s",parent,key);
    int row=0; DxReviewRegistry *registry=a->review->registry;
    for (size_t i=0;i<registry->count;i++) if (!strcmp(registry->widgets[i].parent_id,parent) && strstr(registry->widgets[i].id,".action.")) row++;
    DxReviewWidget *w=review_author_widget(a,id,parent,kind,action,role,label,bounds,clip,131,enabled,checked,focused,false,field,file,line);
    if (w && (length<0 || (size_t)length>=sizeof(id))) w->identity_truncated=true;
    if (w && row<16) {
        Rectangle visible=GetCollisionRec((Rectangle){w->bounds.x,w->bounds.y,w->bounds.width,w->bounds.height},(Rectangle){w->clip.x,w->clip.y,w->clip.width,w->clip.height});
        char state[512]; snprintf(state,sizeof(state)," author_row%d=%.3f,%.3f,%.3f,%.3f author_key%d=%s author_id%d=%s author_flags%d=%d,%d,%d",row,visible.x,visible.y,visible.width,visible.height,row,key,row,id,row,enabled,checked,focused);
        review_author_capture_state(a,state);
    }
    if (w) review_author_capture_count(a,row+1);
}
static void review_author_scroll(App *a,const char *kind,Rectangle track,Rectangle thumb,Rectangle clip,bool dragging,const char *file,int line) {
    if (!review_armed(a)) return;
    char parent[128],id[128],track_id[128]; DxReviewDataIdentity data; review_author_identity(a,kind,parent,&data,NULL);
    snprintf(track_id,sizeof(track_id),"%s.scrollbar",parent);
    review_author_widget(a,track_id,parent,kind,"author.scroll","scrollbar","Menu scroll",track,clip,132,true,false,false,false,NULL,file,line);
    snprintf(id,sizeof(id),"%s.thumb",track_id);
    review_author_widget(a,id,track_id,kind,"author.scroll","scrollbar","Menu scroll thumb",thumb,GetCollisionRec(track,clip),133,true,false,dragging,false,NULL,file,line);
}
static void review_author_input(App *a,const char *kind,const char *key,const char *action,const char *label,Rectangle bounds,Rectangle clip,bool enabled,bool focused,const TextInput *input,const char *file,int line) {
    if (!review_armed(a)) return;
    review_author_item(a,kind,key,action,"textbox",label,bounds,clip,enabled,false,focused,NULL,file,line);
    char id[128]; snprintf(id,sizeof(id),"author.%s.action.%s",kind,key);
    DxReviewWidget *w=(DxReviewWidget *)dx_review_find(a->review->registry,id);
    if (w) { dx_review_value_set(&w->value,"VARCHAR",false,input && input->text ? input->text : ""); w->truncated|=w->value.truncated; a->review->registry->truncated|=w->truncated; }
}
static void review_relation_choice(App *a,const Relation *relation,size_t index,Rectangle bounds,Rectangle clip,bool checked,bool focused,const char *file,int line) {
    if (!review_armed(a) || !relation || !relation->schema || !relation->name) return;
    char key[80],id[128]; snprintf(key,sizeof(key),"select.s%016llx.n%016llx",(unsigned long long)review_field_hash(relation->schema),(unsigned long long)review_field_hash(relation->name));
    review_author_item(a,"source_relation",key,"source.select_relation","menuitemradio",relation->label,bounds,clip,true,checked,focused,NULL,file,line);
    snprintf(id,sizeof(id),"author.source_relation.action.%s",key); DxReviewWidget *widget=(DxReviewWidget *)dx_review_find(a->review->registry,id);
    if (!widget) return;
    /* Own a qualified SQL identifier, with components quoted independently so
       dots and quotes in a schema/name cannot conflate different relations. */
    char qualified[sizeof(widget->value.exact_text)]={0}; size_t used=0; bool truncated=false;
    const char *parts[]={relation->schema,relation->name};
    for (size_t part=0;part<2 && !truncated;part++) {
        if (used+3>=sizeof(qualified)) { truncated=true; break; }
        if (part) qualified[used++]='.'; qualified[used++]='"';
        for (const char *text=parts[part];*text;) {
            int bytes=0; GetCodepointNext(text,&bytes); if (bytes<1) bytes=1;
            size_t need=*text=='"' ? 2 : (size_t)bytes;
            if (used+need+2>=sizeof(qualified)) { truncated=true; break; }
            if (*text=='"') { qualified[used++]='"'; qualified[used++]='"'; }
            else { memcpy(qualified+used,text,(size_t)bytes); used+=(size_t)bytes; }
            text+=bytes;
        }
        if (!truncated) qualified[used++]='"';
    }
    qualified[used]=0; dx_review_value_set(&widget->value,"VARCHAR",false,qualified); widget->value.truncated|=truncated;
    widget->data.row=index; widget->truncated|=widget->value.truncated; a->review->registry->truncated|=widget->truncated;
}
static void review_sort(App *a,const SortReviewFrame *frame) {
    if (!review_armed(a) || !frame || !frame->armed) return;
    SortEditor *editor=&a->sort_editor;
    char scope[128],state[512]; DxReviewDataIdentity identity; review_author_identity(a,"sort",scope,&identity,NULL);
    snprintf(state,sizeof(state)," author_kind=sort author_parent=author.sort sort_targets=%zu sort_overflow=%d sort_object=%d sort_source_revision=%llu sort_focus=%d sort_choice=%zu sort_level=%d sort_field_scroll=%d sort_order_scroll=%d sort_search_revision=%llu sort_draft_count=%zu",frame->count,frame->overflow,a->sort_object,(unsigned long long)identity.query_generation,editor->focus,editor->choice,editor->level,editor->field_scroll,editor->order_scroll,(unsigned long long)editor->search.revision,editor->draft.sort_count+(editor->draft.sort_column ? 1 : 0));
    review_author_capture_state(a,state);
    for (size_t i=0;i<editor->draft.sort_count+(editor->draft.sort_column ? 1 : 0);i++) {
        AnalysisSort sort=analysis_sort_at(&editor->draft,i); char level[96]; snprintf(level,sizeof(level)," sort_draft%zu=%016llx,%d",i,(unsigned long long)review_field_hash(sort.column),sort.descending); review_author_capture_state(a,level);
    }
    if (frame->overflow) { a->review->registry->truncated=true; a->review->registry->frame.truncated=true; a->review->frame.truncated=true; }
    for (size_t i=0;i<frame->count && i<SORT_REVIEW_TARGETS;i++) {
        const SortReviewTarget *target=&frame->targets[i]; char id[128],parent[128]="author.sort",key[64]="",capture_key[48]=""; const char *role="button",*action="sort.inspect"; int layer=132;
        bool priority=target->kind>=SORT_REVIEW_LEVEL && target->kind<=SORT_REVIEW_DESCENDING;
        if (priority) snprintf(parent,sizeof(parent),"author.sort.priority.f%016llx",(unsigned long long)target->field_hash);
        switch (target->kind) {
        case SORT_REVIEW_PANEL: snprintf(id,sizeof(id),"author.sort"); parent[0]=0; role="dialog"; layer=130; break;
        case SORT_REVIEW_HEADING: snprintf(key,sizeof(key),"heading"); role="text"; layer=131; break;
        case SORT_REVIEW_FIELDS: snprintf(key,sizeof(key),"fields"); role="group"; layer=131; break;
        case SORT_REVIEW_PRIORITIES: snprintf(key,sizeof(key),"priorities"); role="group"; layer=131; break;
        case SORT_REVIEW_SEARCH: snprintf(key,sizeof(key),"search"); snprintf(parent,sizeof(parent),"author.sort.fields"); snprintf(capture_key,sizeof(capture_key),"search"); role="textbox"; action="sort.search_fields"; break;
        case SORT_REVIEW_FIELD: snprintf(key,sizeof(key),"field.c%zu.f%016llx",target->column+1,(unsigned long long)target->field_hash); snprintf(parent,sizeof(parent),"author.sort.fields"); action="sort.add_field"; if (!target->column) snprintf(capture_key,sizeof(capture_key),"field0"); break;
        case SORT_REVIEW_LEVEL: snprintf(id,sizeof(id),"%s",parent); snprintf(parent,sizeof(parent),"author.sort.priorities"); role="group"; break;
        case SORT_REVIEW_LEVEL_SELECT: snprintf(key,sizeof(key),"select"); action="sort.select_priority"; layer=133; break;
        case SORT_REVIEW_LEVEL_TITLE: snprintf(key,sizeof(key),"heading"); role="text"; layer=134; break;
        case SORT_REVIEW_UP: snprintf(key,sizeof(key),"move_up"); action="sort.move_up"; layer=135; if (target->priority<2) snprintf(capture_key,sizeof(capture_key),"up%zu",target->priority); break;
        case SORT_REVIEW_DOWN: snprintf(key,sizeof(key),"move_down"); action="sort.move_down"; layer=135; break;
        case SORT_REVIEW_REMOVE: snprintf(key,sizeof(key),"remove"); action="sort.remove_level"; layer=135; break;
        case SORT_REVIEW_ASCENDING: snprintf(key,sizeof(key),"ascending"); action="sort.ascending"; layer=135; break;
        case SORT_REVIEW_DESCENDING: snprintf(key,sizeof(key),"descending"); action="sort.descending"; layer=135; if (target->priority<2) snprintf(capture_key,sizeof(capture_key),"descending%zu",target->priority); break;
        case SORT_REVIEW_FIELD_SCROLL: snprintf(key,sizeof(key),"scrollbar"); snprintf(parent,sizeof(parent),"author.sort.fields"); action="sort.scroll_fields"; role="scrollbar"; layer=136; break;
        case SORT_REVIEW_FIELD_THUMB: snprintf(key,sizeof(key),"thumb"); snprintf(parent,sizeof(parent),"author.sort.fields.scrollbar"); action="sort.scroll_fields"; role="scrollbar"; layer=137; break;
        case SORT_REVIEW_ORDER_SCROLL: snprintf(key,sizeof(key),"scrollbar"); snprintf(parent,sizeof(parent),"author.sort.priorities"); action="sort.scroll_priorities"; role="scrollbar"; layer=136; break;
        case SORT_REVIEW_ORDER_THUMB: snprintf(key,sizeof(key),"thumb"); snprintf(parent,sizeof(parent),"author.sort.priorities.scrollbar"); action="sort.scroll_priorities"; role="scrollbar"; layer=137; break;
        case SORT_REVIEW_STATUS: snprintf(key,sizeof(key),"status"); role="text"; break;
        case SORT_REVIEW_CLOSE: snprintf(key,sizeof(key),"close"); action="dialog.cancel"; break;
        case SORT_REVIEW_CLEAR: snprintf(key,sizeof(key),"clear"); action="sort.clear_levels"; break;
        case SORT_REVIEW_CANCEL: snprintf(key,sizeof(key),"cancel"); snprintf(capture_key,sizeof(capture_key),"cancel"); action="dialog.cancel"; break;
        case SORT_REVIEW_APPLY: snprintf(key,sizeof(key),"apply"); snprintf(capture_key,sizeof(capture_key),"apply"); action="sort.apply"; break;
        default: continue;
        }
        if (target->kind==SORT_REVIEW_PANEL) {
            Rectangle viewport=box(0,0,GetScreenWidth(),GetScreenHeight());
            review_author_widget(a,"author.sort.backdrop",NULL,"sort","sort.inspect","region","Modal backdrop",viewport,viewport,129,true,false,false,true,NULL,target->file,target->line);
            review_author_widget(a,"author.sort.backdrop.background","author.sort.backdrop","sort","sort.inspect","region","Modal backdrop",viewport,viewport,129,true,false,false,false,NULL,target->file,target->line);
        }
        if (key[0]) snprintf(id,sizeof(id),"%s.%s",parent,key);
        DxReviewWidget *widget=review_author_widget(a,id,parent,"sort",action,role,target->label,target->bounds,target->clip,layer,target->enabled,target->checked,target->focused,target->container,target->field[0] ? target->field : NULL,target->file,target->line);
        if (!widget) continue;
        if (priority) widget->data.row=target->priority;
        if (target->kind==SORT_REVIEW_FIELD) widget->data.column=target->column;
        widget->truncated|=target->truncated; a->review->registry->truncated|=widget->truncated;
        if (target->kind==SORT_REVIEW_SEARCH) { dx_review_value_set(&widget->value,"VARCHAR",false,editor->search.text ? editor->search.text : ""); widget->truncated|=widget->value.truncated; a->review->registry->truncated|=widget->truncated; }
        if (target->kind==SORT_REVIEW_PANEL) review_author_widget(a,"author.sort.background","author.sort","sort","sort.inspect","region","Dialog background",target->bounds,target->clip,130,true,false,false,false,NULL,target->file,target->line);
        if (capture_key[0]) {
            Rectangle visible=GetCollisionRec((Rectangle){widget->bounds.x,widget->bounds.y,widget->bounds.width,widget->bounds.height},(Rectangle){widget->clip.x,widget->clip.y,widget->clip.width,widget->clip.height}); char geometry[240];
            snprintf(geometry,sizeof(geometry)," sort_%s=%.3f,%.3f,%.3f,%.3f sort_%s_id=%s",capture_key,visible.x,visible.y,visible.width,visible.height,capture_key,widget->id); review_author_capture_state(a,geometry);
        }
    }
}
static void review_sheet_row(App *a,size_t sheet,const char *label,Rectangle bounds,Rectangle clip,bool enabled,bool checked,bool focused,const char *file,int line) {
    if (!review_armed(a) || sheet>=a->book.count || bounds.width<=0 || bounds.height<=0 || clip.width<=0 || clip.height<=0) return;
    char id[128]; snprintf(id,sizeof(id),"pane.workbook.sheet.%zu",sheet+1);
    DxReviewWidget w; dx_review_widget_init(&w,id,"workbook.activate_sheet","workspace/left/workbook/sheets","tab",label);
    snprintf(w.parent_id,sizeof(w.parent_id),"pane.workbook"); w.bounds=review_rect(bounds); w.clip=review_rect(clip); w.layer=52;
    w.enabled=w.input_eligible=enabled; w.checked=checked; w.focused=focused; w.data=review_sheet_scope(a,sheet);
    snprintf(w.source_file,sizeof(w.source_file),"%s",file); w.source_line=(uint32_t)line;
    DxReviewStatus result=dx_review_add(a->review->registry,&w);
    if (result!=DX_REVIEW_OK) fprintf(stderr,"Review worksheet registration %s: %s\n",id,dx_review_status_text(result));
}
static void review_sheet_choice(App *a,const char *kind,const char *key,const char *action,const char *label,Rectangle bounds,Rectangle clip,bool enabled,bool checked,bool focused,size_t sheet,const char *file,int line) {
    if (!review_armed(a) || sheet>=a->book.count) return;
    char semantic[96],id[128]; snprintf(semantic,sizeof(semantic),"%s.sheet%zu",key,sheet+1);
    review_author_item(a,kind,semantic,action,"menuitem",label,bounds,clip,enabled,checked,focused,NULL,file,line);
    snprintf(id,sizeof(id),"author.%s.action.%s",kind,semantic);
    DxReviewWidget *w=(DxReviewWidget *)dx_review_find(a->review->registry,id); if (w) w->data=review_sheet_scope(a,sheet);
}
static void review_sheet_scroll(App *a,Rectangle track,Rectangle thumb,Rectangle clip,bool dragging,const char *file,int line) {
    if (!review_armed(a) || track.width<=0 || track.height<=0 || clip.width<=0 || clip.height<=0) return;
    const Rectangle bounds[]={track,thumb}; const char *const ids[]={"pane.workbook.scrollbar","pane.workbook.scrollbar.thumb"};
    bool enabled=!a->ui.pointer_blocked && a->modal==MODAL_NONE && !a->io_busy && a->sidebar && !a->workbook_collapsed;
    for (size_t i=0;i<2;i++) {
        DxReviewWidget w; dx_review_widget_init(&w,ids[i],"workbook.scroll","workspace/left/workbook/scrollbar","scrollbar",i ? "Worksheet scroll thumb" : "Worksheet scroll");
        snprintf(w.parent_id,sizeof(w.parent_id),"%s",i ? ids[0] : "pane.workbook"); w.bounds=review_rect(bounds[i]); w.clip=review_rect(i ? GetCollisionRec(track,clip) : clip); w.layer=(int)(53+i);
        w.enabled=w.input_eligible=enabled; w.focused=i && dragging; w.data=review_sheet_scope(a,a->book.active); snprintf(w.data.kind,sizeof(w.data.kind),"workbook-position");
        snprintf(w.source_file,sizeof(w.source_file),"%s",file); w.source_line=(uint32_t)line;
        DxReviewStatus status=dx_review_add(a->review->registry,&w);
        if (status!=DX_REVIEW_OK) fprintf(stderr,"Review worksheet scroll registration %s: %s\n",ids[i],dx_review_status_text(status));
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
static void review_matrix_area(App *a,Rectangle rows,Rectangle clip,Rectangle measure) {
    if (!review_armed(a)) return;
    Rectangle visible=GetCollisionRec(rows,clip),cell=GetCollisionRec(measure,clip);
    char state[320]; snprintf(state,sizeof(state)," matrix_data_rect=%.3f,%.3f,%.3f,%.3f matrix_first_measure_rect=%.3f,%.3f,%.3f,%.3f matrix_data_row=%d",visible.x,visible.y,visible.width,visible.height,cell.x,cell.y,cell.width,cell.height,a->matrix_row);
    review_author_capture_state(a,state);
}
static void review_statistics(App *a) {
    if (!review_armed(a) || a->file_menu || (a->modal!=MODAL_NONE && a->modal!=MODAL_STATISTICS)) return;
    const StatsKey *key=&a->statistics_key; const SelectionStats *stats=&a->statistics;
    char state[768];
    snprintf(state,sizeof(state)," stats_state=%d stats_request=%llu stats_object=%d stats_cells=%llu stats_key=%llu,%llu,%zu,%zu,%llu,%zu,%d stats_counts=%llu,%llu,%llu,%llu stats_flags=%d,%d,%d,%d stats_error=%d",
        a->statistics_state,(unsigned long long)a->statistics_request,a->statistics_object,(unsigned long long)a->statistics_cells,
        (unsigned long long)key->r0,(unsigned long long)key->r1,key->c0,key->c1,(unsigned long long)key->source_revision,key->sheet,key->active,
        (unsigned long long)stats->cells,(unsigned long long)stats->filled,(unsigned long long)stats->numeric,(unsigned long long)stats->distinct,
        stats->approximate,stats->non_finite,stats->sum_overflow,stats->average_overflow,a->statistics_error!=NULL);
    review_author_capture_state(a,state);
    const char *names[]={"sum","average","minimum","maximum"},*values[]={stats->sum,stats->average,stats->minimum,stats->maximum};
    for (size_t i=0;i<4;i++) {
        const char *raw=values[i]; char encoded[129]="-"; size_t n=0;
        if (raw && *raw) {
            static const char hex[]="0123456789abcdef";
            for (;n<64 && raw[n];n++) { unsigned char ch=(unsigned char)raw[n]; encoded[2*n]=hex[ch>>4]; encoded[2*n+1]=hex[ch&15]; }
            encoded[2*n]=0;
        }
        snprintf(state,sizeof(state)," stats_%s_null=%d stats_%s_hex=%s stats_%s_truncated=%d",names[i],!raw,names[i],encoded,names[i],raw && raw[n]);
        review_author_capture_state(a,state);
    }
}
static void review_grid(App *a) {
    if (!review_armed(a)) return;
    review_statistics(a);
    /* Existing cached analytical state, owned by both captured frames. Layout
       is read after this frame's draw; no SQL, heap work or test controller. */
    Worksheet *sheet=active_sheet(a); SheetObject *object=selected_object(a); ObjectView *view=object ? &a->objects[a->selected_object] : NULL;
    char state[1536],grand[129]="-"; bool grand_null=true,grand_truncated=false;
    PivotTotals *totals=view ? view->page.totals : NULL;
    if (totals && totals->columns.row_count && totals->columns.column_count && totals->columns.cells) {
        size_t measures=object ? object->pivot.counts[PIVOT_VALUES] : 0;
        size_t at=totals->has_columns && measures && totals->columns.column_count>=measures ? totals->columns.column_count-measures : 0;
        const Cell *cell=&totals->columns.cells[at]; grand_null=cell->is_null;
        if (!cell->is_null && cell->text && *cell->text) {
            static const char hex[]="0123456789abcdef"; size_t n=0;
            for (;n<64 && cell->text[n];n++) { unsigned char ch=(unsigned char)cell->text[n]; grand[2*n]=hex[ch>>4]; grand[2*n+1]=hex[ch&15]; }
            grand[2*n]=0; grand_truncated=cell->text[n]!=0;
        }
    }
    if (!object) snprintf(state,sizeof(state)," selected_object=-1 object_count=%zu",sheet ? sheet->objects.count : 0);
    else snprintf(state,sizeof(state)," selected_object=%d object_count=%zu object=%llu,%u,%llu,%zu,%llu,%zu object_page=%llu,%zu,%zu,%llu,%d,%d pivot_counts=%zu,%zu,%zu,%zu matrix_rect=%.3f,%.3f,%.3f,%.3f preview_rect=%.3f,%.3f,%.3f,%.3f totals_count=%zu,%zu totals_shape=%zu,%zu,%zu,%zu,%d object_grand0_null=%d object_grand0_hex=%s object_grand0_truncated=%d",
        object ? a->selected_object : -1,sheet ? sheet->objects.count : 0,(unsigned long long)(object ? object->id : 0),object ? (unsigned)object->kind : 0,(unsigned long long)(object ? object->row : 0),object ? object->col : 0,(unsigned long long)(object ? object->rows : 0),object ? object->columns : 0,
        (unsigned long long)(view ? view->page.offset : 0),view ? view->page.row_count : 0,view ? view->page.column_count : 0,(unsigned long long)(view ? view->page.total : 0),view && view->dirty,view && view->failed,
        object ? object->pivot.counts[PIVOT_ROWS] : 0,object ? object->pivot.counts[PIVOT_COLUMNS] : 0,object ? object->pivot.counts[PIVOT_VALUES] : 0,object ? object->pivot.counts[PIVOT_FILTERS] : 0,
        a->authoring_layout.matrix.x,a->authoring_layout.matrix.y,a->authoring_layout.matrix.width,a->authoring_layout.matrix.height,a->authoring_layout.preview.x,a->authoring_layout.preview.y,a->authoring_layout.preview.width,a->authoring_layout.preview.height,
        totals ? totals->rows.row_count : 0,totals ? totals->columns.row_count : 0,
        totals ? totals->rows.row_count : 0,totals ? totals->rows.column_count : 0,totals ? totals->columns.row_count : 0,totals ? totals->columns.column_count : 0,totals && totals->has_columns,grand_null,grand,grand_truncated);
    review_author_capture_state(a,state);
    if (object) { snprintf(state,sizeof(state)," object_revision=%llu",(unsigned long long)object->revision); review_author_capture_state(a,state); }
    if (object && view && object->pivot.counts[PIVOT_COLUMNS]) for (size_t c=0;c<2;c++) {
        size_t at=object->pivot.counts[PIVOT_ROWS]+c; const Column *column=at<view->page.column_count ? &view->page.columns[at] : NULL;
        bool present=column && column->category_count && column->categories;
        const char *raw=present ? column->categories[0] : NULL; char encoded[129]="-"; size_t n=0;
        if (raw && *raw) {
            static const char hex[]="0123456789abcdef";
            for (;n<64 && raw[n];n++) { unsigned char ch=(unsigned char)raw[n]; encoded[2*n]=hex[ch>>4]; encoded[2*n+1]=hex[ch&15]; } encoded[2*n]=0;
        }
        snprintf(state,sizeof(state)," object_category%zu_present=%d object_category%zu_null=%d object_category%zu_hex=%s object_category%zu_truncated=%d",c,present,c,!raw,c,encoded,c,raw && raw[n]);
        review_author_capture_state(a,state);
    }
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
        !a->chart_resizing && (a->modal==MODAL_PIVOT_FIELD || (a->pivot_drag_field<0 && a->pivot_drag_role<0)) && !a->matrix_selecting &&
        !(a->modal==MODAL_GROUP && a->summary_drag>=0) && !a->split_side && !a->split_pipeline &&
        !a->steps_drag && !a->workbook_drag && !a->field_drag && !a->values_drag && !a->context_drag &&
        !(a->file_menu && a->file_menu_drag) &&
        !a->inspector_drag && !a->table_relation_drag && !a->sort_editor.dragging && !a->input.dragging && !a->filter_term.dragging &&
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
#define REVIEW_AUTHOR_CASES 14
#define REVIEW_AUTHOR_END (71+REVIEW_AUTHOR_CASES*5)
#define REVIEW_DIALOG_END (REVIEW_AUTHOR_END+5*6)
#define REVIEW_PLOT_END (REVIEW_DIALOG_END+5)
#define REVIEW_SHEET_END (REVIEW_PLOT_END+4*6)
#define REVIEW_QUERY_END (REVIEW_SHEET_END+2*6)
#define REVIEW_RELATION_END (REVIEW_QUERY_END+6)
#define REVIEW_SMOKE_STAGES (REVIEW_RELATION_END+2*6)
typedef struct {
    int stage; unsigned note_frame,wait_frame; uint64_t request,jobs; GridLocation selection; Cell *page;
    size_t cursor,anchor,history; bool dirty; int file_focus,file_scroll,diagnostic_stage; char root[4096];
    Modal choice_modal; int choice_focus,choice_ribbon_mode; float choice_scale,choice_row_height; uint64_t choice_formats;
    Analysis author_source; PivotDefinition author_pivot; MarkDefinition author_marks; bool author_owned;
    uint64_t author_revision,author_title_revision,author_marks_revision; size_t author_count; int author_selected;
    uint64_t author_rows; size_t author_columns; ObjectKind author_kind; ChartMark author_mark; char *author_name;
    Cell *author_page; int author_field,author_field_focus,author_drag_field,author_drag_role,author_drag_index,author_mark_role;
    char author_target[128];
    char *dialog_text,*dialog_chart_text; size_t dialog_cursor,dialog_anchor,dialog_chart_cursor,dialog_chart_anchor;
    uint64_t dialog_revision,dialog_chart_revision; int dialog_focus; DocumentAction dialog_action;
    bool sheet_fixture; size_t sheet_original,sheet_other,sheet_count,sheet_active,sheet_index;
    char *sheet_names[DX_MAX_SHEETS]; bool sheet_hidden[DX_MAX_SHEETS]; uint64_t sheet_generation;
    int sheet_focus,sheet_scroll,sheet_unhide_scroll,sheet_pick_diagnostic_stage; Rectangle sheet_navigation;
    bool query_table_added,query_pivot_added,query_pending,query_complete; int query_table,query_pivot,query_object,query_focus;
    Analysis query_source; uint64_t query_source_revision,query_draft_revision,query_edit_revision,query_owner;
    uint64_t query_jobs,query_sql,query_counts,query_prepares,query_latest,query_complete_sql,query_complete_counts,query_complete_prepares; QueryReply *query_reply;
    TextEditor query_editor;
    struct { Cell *cells; uint64_t id,revision,source_id,total; size_t rows,columns; } query_views[DX_MAX_OBJECTS];
    bool relation_added; int relation_object,relation_focus,relation_scroll; Analysis relation_source;
    uint64_t relation_id,relation_revision; Relation *relation_rows; size_t relation_count;
    Analysis sort_source,sort_draft; int sort_object,sort_focus,sort_level,sort_field_scroll,sort_order_scroll;
    size_t sort_choice,sort_count,*sort_matches; uint64_t sort_owner,sort_revision,sort_search_revision; char sort_city_up[128];
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
static bool review_smoke_text_same(const char *left,const char *right) { return left && right ? !strcmp(left,right) : left==right; }
static bool review_smoke_pivot_same(const PivotDefinition *left,const PivotDefinition *right) {
    for (size_t role=0;role<4;role++) {
        if (left->counts[role]!=right->counts[role]) return false;
        for (size_t i=0;i<left->counts[role];i++) {
            const PivotField *x=&left->fields[role][i],*y=&right->fields[role][i];
            if (!review_smoke_text_same(x->name,y->name) || !review_smoke_text_same(x->value,y->value) || x->aggregate!=y->aggregate || x->grain!=y->grain || x->filter!=y->filter || x->number.kind!=y->number.kind || x->number.precision!=y->number.precision || x->number.scale!=y->number.scale) return false;
        }
    }
    return true;
}
static void review_smoke_author_release(ReviewSmoke *t) {
    if (t->author_owned) { analysis_free(&t->author_source); pivot_free(&t->author_pivot); marks_free(&t->author_marks); free(t->author_name); t->author_name=NULL; t->author_owned=false; }
    analysis_free(&t->query_source);
    analysis_free(&t->relation_source);
    analysis_free(&t->sort_source); analysis_free(&t->sort_draft);
    free(t->dialog_text); t->dialog_text=NULL; free(t->dialog_chart_text); t->dialog_chart_text=NULL;
    for (size_t i=0;i<DX_MAX_SHEETS;i++) { free(t->sheet_names[i]); t->sheet_names[i]=NULL; }
}
static void review_smoke_author_remember(App *a,ReviewSmoke *t) {
    review_smoke_choice_remember(a,t); review_smoke_author_release(t);
    t->author_owned=true; t->author_source=analysis_copy(&a->analysis);
    SheetObject *object=selected_object(a); t->author_pivot=object ? pivot_copy(&object->pivot) : (PivotDefinition){0}; t->author_marks=object ? marks_copy(&object->marks) : (MarkDefinition){0};
    t->author_selected=a->selected_object; t->author_count=active_sheet(a)->objects.count; t->author_revision=object ? object->revision : 0;
    t->author_title_revision=active_sheet(a)->objects.title_revision; t->author_marks_revision=active_sheet(a)->objects.marks_revision;
    t->author_rows=object ? object->rows : 0; t->author_columns=object ? object->columns : 0; t->author_kind=object ? object->kind : OBJECT_PIVOT;
    t->author_mark=object ? object->mark : MARK_BAR; t->author_name=object ? dx_strdup(object->name) : NULL;
    t->author_page=object ? a->objects[a->selected_object].page.cells : NULL;
    t->author_field=a->pivot_field; t->author_field_focus=a->field_menu_focus;
    t->author_drag_field=a->pivot_drag_field; t->author_drag_role=a->pivot_drag_role; t->author_drag_index=a->pivot_drag_index; t->author_mark_role=a->mark_role;
}
static bool review_smoke_author_models_same(App *a,ReviewSmoke *t) {
    SheetObject *object=selected_object(a);
    return analysis_same_result(&a->analysis,&t->author_source) && analysis_sorts_equal(&a->analysis,&t->author_source) &&
        a->selected_object==t->author_selected && active_sheet(a)->objects.count==t->author_count && active_sheet(a)->objects.title_revision==t->author_title_revision && active_sheet(a)->objects.marks_revision==t->author_marks_revision &&
        (!object || (object->revision==t->author_revision && object->kind==t->author_kind && object->mark==t->author_mark && object->rows==t->author_rows && object->columns==t->author_columns && review_smoke_text_same(object->name,t->author_name) && a->objects[a->selected_object].page.cells==t->author_page && review_smoke_pivot_same(&object->pivot,&t->author_pivot) && marks_equal(&object->marks,&t->author_marks))) &&
        a->pivot_field==t->author_field && a->field_menu_focus==t->author_field_focus && a->pivot_drag_field==t->author_drag_field && a->pivot_drag_role==t->author_drag_role && a->pivot_drag_index==t->author_drag_index && a->mark_role==t->author_mark_role;
}
static bool review_smoke_author_same(App *a,ReviewSmoke *t) { return review_smoke_choice_same(a,t) && review_smoke_author_models_same(a,t); }
static bool review_smoke_author_targets(App *a,const char *kind,const char *target,ReviewSmoke *t) {
    DxReviewRegistry *registry=a->review->registry; DxReviewTarget picked; char parent[128],id[128];
    snprintf(parent,sizeof(parent),"author.%s",kind); const DxReviewWidget *menu=dx_review_find(registry,parent);
    if (!menu || !menu->container || !menu->expanded || menu->input_eligible || registry->duplicate_count || registry->invalid_count || registry->overflow_count) return false;
    bool compact=!strcmp(kind,"representation") || !strcmp(kind,"plot");
    snprintf(id,sizeof(id),"%s.background",parent);
    float margin=(compact ? .5f : 2)*a->ui.scale,x=menu->bounds.x+margin,y=menu->bounds.y+margin;
    if (!dx_review_pick(registry,x,y,DX_REVIEW_PICK_CONTROL,&picked) || strcmp(picked.widget.id,id)) { fprintf(stderr,"FAIL: authoring background expected=%s actual=%s point=%.2f,%.2f\n",id,picked.widget.id,x,y); return false; }
    snprintf(id,sizeof(id),"%s.heading",parent); const DxReviewWidget *heading=dx_review_find(registry,id);
    if (compact ? heading!=NULL : (!heading || heading->input_eligible || !dx_review_pick(registry,heading->bounds.x+heading->bounds.width*.5f,heading->bounds.y+heading->bounds.height*.5f,DX_REVIEW_PICK_CONTROL,&picked) || strcmp(picked.widget.id,id))) return false;
    size_t rows=0; const DxReviewWidget *chosen=NULL;
    for (size_t i=0;i<registry->count;i++) {
        const DxReviewWidget *w=&registry->widgets[i]; if (strcmp(w->parent_id,parent) || !strstr(w->id,".action.")) continue;
        if (w->input_eligible!=w->enabled || w->layer<=menu->layer || !w->data.present || strcmp(w->data.document_id,menu->data.document_id) || w->data.object_id!=menu->data.object_id || w->data.query_generation!=menu->data.query_generation || w->transform.ty!=menu->transform.ty) return false;
        Rectangle visible=GetCollisionRec((Rectangle){w->bounds.x,w->bounds.y,w->bounds.width,w->bounds.height},(Rectangle){w->clip.x,w->clip.y,w->clip.width,w->clip.height});
        if (visible.width<=0 || visible.height<=0) continue;
        float x=visible.x+visible.width*.5f,y=visible.y+visible.height*.5f;
        if (!dx_review_pick(registry,x,y,DX_REVIEW_PICK_CONTROL,&picked) || strcmp(picked.widget.id,w->id) || !dx_review_pick(registry,x,y,DX_REVIEW_PICK_CONTAINER,&picked) || strcmp(picked.widget.id,parent)) return false;
        rows++; if ((!target && !chosen) || (target && (strstr(w->id,target) || strstr(w->label,target)))) chosen=w;
    }
    static const size_t expected[]={6,6,8,9,5,7,4,7,4,6,6,3,2,4};
    bool extra_plot=t->stage>=REVIEW_DIALOG_END; size_t item=extra_plot ? 12 : (size_t)(t->stage-71)/5;
    if (item>=sizeof(expected)/sizeof(*expected) || rows!=(extra_plot ? 2 : expected[item]) || !chosen || !strstr(a->review->frame.state_text," author_count=") || strstr(a->review->frame.state_text," author_overflow=1")) return false;
    if (item==7) {
        const DxReviewWidget *earlier=dx_review_find(registry,"author.pivot_field.action.move_earlier"),*later=dx_review_find(registry,"author.pivot_field.action.move_later");
        if (!earlier || !later || earlier->enabled || later->enabled) return false;
    }
    if (item==8) {
        const DxReviewWidget *sum=dx_review_find(registry,"author.pivot_field.action.aggregate.sum"),*remove=dx_review_find(registry,"author.pivot_field.action.remove");
        if (!sum || !sum->checked || !remove || remove->enabled) return false;
    }
    if (extra_plot ? chosen->checked : ((item==11 || item==12 || item==13) && !chosen->checked)) return false;
    snprintf(t->author_target,sizeof(t->author_target),"%s",chosen->id);
    return true;
}
static bool review_smoke_author_select_ready(App *a,Modal modal) {
    Rectangle field=author_select_field(a,modal);
    Rectangle clip=GetCollisionRec(a->author_marks_layout.viewport,box(0,0,GetScreenWidth(),GetScreenHeight()));
    Rectangle visible=GetCollisionRec(field,clip);
    return field.width>0 && field.height>0 && visible.width+.01f>=field.width && visible.height+.01f>=field.height;
}
/* Five stages per case keep captures tied to settled, real controllers while
   preserving the original report/write-retry inventory above. No reports are
   created here; each native picker enters and cancels its owned composer. */
static int review_smoke_author_tick(App *a,ReviewSmoke *t) {
    static const char *const kinds[]={"source_context","context","context","field","step","object","pivot_field","pivot_field","pivot_field","marks","assign_field","representation","plot","view"};
    int item=(t->stage-71)/5,phase=(t->stage-71)%5; DxReviewCapture *r=a->review;
    if (phase==0) {
        if (r->phase!=DX_REPORT_IDLE || !smoke_work_idle(a)) return 0;
        if (item==0 && (!a->analysis.path || !strstr(a->analysis.path,"transactions.csv"))) { open_source(a,"tests/fixtures/transactions.csv",false); return 0; }
        if (a->page.column_count!=6) return -1;
        ui_set_scale(&a->ui,item>=3 ? 1.5f : 1); SetWindowSize(item>=3 ? 760 : 1280,item>=3 ? 640 : 820);
        a->modal=MODAL_NONE; a->file_menu=false; a->consume_grid_input=true; a->context_focus=a->context_scroll=0; a->context_origin=(Vector2){620,300};
        a->pivot_drag_field=a->pivot_drag_role=-1;
        if (item==0) { a->selected_object=-1; grid_select_cell(&a->grid,1,1,false); a->modal=MODAL_CONTEXT; }
        else if (item==1) { a->selected_object=-1; a->grid.selection=SELECT_ROWS; a->grid.row=a->grid.anchor_row=a->grid.end_row=1; a->modal=MODAL_CONTEXT; }
        else if (item==2) { a->selected_object=-1; grid_select_cell(&a->grid,20,8,false); a->modal=MODAL_CONTEXT; }
        else if (item==3) { a->selected_object=-1; begin_field_menu(a,1,a->context_origin); a->field_menu_focus=3; a->field_menu_keyboard=true; }
        else if (item==4) { a->selected_object=-1; pipeline_rows_cache(a); begin_step_menu(a,0,a->context_origin); }
        else {
            if (!active_sheet(a)->objects.count) {
                grid_select_cell(&a->grid,0,0,false); create_pivot(a); SheetObject *object=selected_pivot(a); if (!object) return -1;
                PivotDefinition definition={0}; char *error=NULL;
                bool ok=pivot_assign(&definition,PIVOT_ROWS,"Date",AGG_SUM,&error) && pivot_assign(&definition,PIVOT_VALUES,"Amount",AGG_SUM,&error);
                free(error); if (!ok) { pivot_free(&definition); return -1; }
                if (!set_pivot_definition(a,&definition)) return -1;
                return 0;
            }
            a->selected_object=0; SheetObject *object=selected_pivot(a); if (!object) return -1;
            if (GetScreenWidth()/a->ui.scale<700) show_workspace(a);
            if (item==5) a->modal=MODAL_OBJECT;
            else if (item==6) { a->pivot_drag_field=2; a->modal=MODAL_PIVOT_FIELD; }
            else if (item==7 || item==8) { a->pivot_drag_role=item==7 ? PIVOT_ROWS : PIVOT_VALUES; a->pivot_drag_index=0; a->modal=MODAL_PIVOT_FIELD; }
            else if (item==9) begin_marks(a,MARKS_COLOR,box(620,280,100,20),false);
            else if (item==10) begin_assign_field(a,PIVOT_COLUMNS,box(620,280,100,20));
            else if (item==11) { a->modal=MODAL_REPRESENTATION; a->context_focus=0; a->pivot_focus=6; a->author_reveal=true; }
            else if (item==12) {
                if (object->kind!=OBJECT_CHART) { if (!set_representation(a,1)) return -1; return 0; }
                if (object->pivot.counts[PIVOT_VALUES]==1) {
                    PivotDefinition definition=pivot_copy(&object->pivot); char *error=NULL; bool ok=pivot_assign(&definition,PIVOT_VALUES,"Amount",AGG_AVG,&error); free(error);
                    if (!ok) { pivot_free(&definition); return -1; } if (!set_pivot_definition(a,&definition)) return -1; return 0;
                }
                if (object->pivot.counts[PIVOT_VALUES]!=2) return -1;
                a->pivot_focus=7; a->author_reveal=true;
                if (!review_smoke_author_select_ready(a,MODAL_CHART_MEASURE)) return 0;
                begin_author_select(a,MODAL_CHART_MEASURE); if (a->modal!=MODAL_CHART_MEASURE) return -1;
            }
            else { a->modal=MODAL_VIEW; a->view_menu_chart=true; a->context_focus=1; }
        }
        return 1;
    }
    if (phase==1) {
        if (a->modal==MODAL_NONE) { fprintf(stderr,"FAIL: authoring setup closed kind=%s case=%d selected=%d\n",kinds[item],item,a->selected_object); return -1; }
        if (r->phase!=DX_REPORT_IDLE || !smoke_work_idle(a) || a->popup_presence.offset || a->popup_presence.alpha!=1 || !review_can_capture(a) || a->ui.scale!=(item>=3 ? 1.5f : 1)) return 0;
        review_smoke_author_remember(a,t); ui_note_key_press(KEY_PERIOD,DX_MOD_SUPER|DX_MOD_SHIFT); return 1;
    }
    if (phase==2) {
        if (r->phase!=DX_REPORT_PICK) return 0;
        const char *target=item==1 ? "Edit cell" : item==7 ? ".action.grain.year" : item==8 ? ".action.aggregate.sum" : item==13 ? "Bar chart" : NULL;
        if (!review_smoke_author_same(a,t) || !review_smoke_author_targets(a,kinds[item],target,t)) return -1;
        r->toolbar_top=true; review_smoke_pick(a,t->author_target); return 1;
    }
    if (phase==3) {
        if (r->phase!=DX_REPORT_NOTE) return 0;
        if (strcmp(r->target.widget.id,t->author_target) || !review_smoke_author_same(a,t) || (item==1 && r->target.widget.enabled)) return -1;
        if (item==3 && strcmp(r->target.widget.data.field,"City")) return -1;
        if ((item==7 || item==8) && strcmp(r->target.widget.data.field,item==7 ? "Date" : "Amount")) return -1;
        ui_note_key_press(KEY_ESCAPE,0); return 1;
    }
    if (r->phase!=DX_REPORT_IDLE) return 0;
    if (!review_smoke_author_same(a,t)) return -1;
    printf("PASS: native authoring review kind=%s case=%d retains definitions, focus, owned pages, history and query jobs\n",kinds[item],item);
    a->modal=MODAL_NONE; a->pivot_drag_field=a->pivot_drag_role=-1; a->pivot_search_focus=false;
    review_smoke_author_release(t);
    return 1;
}
static int review_smoke_dialog_focus(App *a,int item) {
    return item==0 ? a->width_focus : item==1 ? a->range_move_focus : item==2 ? a->object_name_focus : item==3 ? a->chart_size_focus : a->document_focus;
}
static void review_smoke_dialog_remember(App *a,ReviewSmoke *t,int item) {
    review_smoke_author_remember(a,t);
    t->dialog_text=dx_strdup(a->input.text ? a->input.text : ""); t->dialog_chart_text=dx_strdup(a->chart_columns.text ? a->chart_columns.text : "");
    t->dialog_cursor=a->input.cursor; t->dialog_anchor=a->input.anchor; t->dialog_revision=a->input.revision;
    t->dialog_chart_cursor=a->chart_columns.cursor; t->dialog_chart_anchor=a->chart_columns.anchor; t->dialog_chart_revision=a->chart_columns.revision;
    t->dialog_focus=review_smoke_dialog_focus(a,item); t->dialog_action=a->document_action;
}
static bool review_smoke_dialog_same(App *a,ReviewSmoke *t,int item,bool pending) {
    return review_smoke_author_same(a,t) && !strcmp(a->input.text ? a->input.text : "",t->dialog_text) && !strcmp(a->chart_columns.text ? a->chart_columns.text : "",t->dialog_chart_text) &&
        a->input.cursor==t->dialog_cursor && a->input.anchor==t->dialog_anchor && a->input.revision==t->dialog_revision &&
        a->chart_columns.cursor==t->dialog_chart_cursor && a->chart_columns.anchor==t->dialog_chart_anchor && a->chart_columns.revision==t->dialog_chart_revision &&
        review_smoke_dialog_focus(a,item)==t->dialog_focus && a->document_action==(pending ? t->dialog_action : DOCUMENT_NONE) && !a->window_closed && !a->exit_ready;
}
static bool review_smoke_dialog_targets(App *a,ReviewSmoke *t,const char *kind,int item) {
    DxReviewRegistry *registry=a->review->registry; DxReviewTarget picked; char parent[128],id[128]; snprintf(parent,sizeof(parent),"author.%s",kind);
    const DxReviewWidget *dialog=dx_review_find(registry,parent);
    if (!dialog || strcmp(dialog->role,"dialog") || !dialog->container || dialog->input_eligible || registry->duplicate_count || registry->invalid_count || registry->overflow_count) return false;
    float x=2*a->ui.scale,y=GetScreenHeight()*.5f; snprintf(id,sizeof(id),"%s.backdrop.background",parent);
    if (!dx_review_pick(registry,x,y,DX_REVIEW_PICK_CONTROL,&picked) || strcmp(picked.widget.id,id)) return false;
    snprintf(id,sizeof(id),"%s.backdrop",parent);
    if (!dx_review_pick(registry,x,y,DX_REVIEW_PICK_CONTAINER,&picked) || strcmp(picked.widget.id,id)) return false;
    snprintf(id,sizeof(id),"%s.heading",parent); const DxReviewWidget *heading=dx_review_find(registry,id);
    if (!heading || heading->input_eligible || !dx_review_pick(registry,heading->bounds.x+heading->bounds.width*.5f,heading->bounds.y+heading->bounds.height*.5f,DX_REVIEW_PICK_CONTROL,&picked) || strcmp(picked.widget.id,id)) return false;
    size_t rows=0;
    for (size_t i=0;i<registry->count;i++) {
        const DxReviewWidget *w=&registry->widgets[i]; if (strcmp(w->parent_id,parent) || !strstr(w->id,".action.")) continue;
        if (w->input_eligible!=w->enabled || w->layer<=dialog->layer || w->bounds.x<dialog->bounds.x || w->bounds.y<dialog->bounds.y || w->bounds.x+w->bounds.width>dialog->bounds.x+dialog->bounds.width || w->bounds.y+w->bounds.height>dialog->bounds.y+dialog->bounds.height) return false;
        x=w->bounds.x+w->bounds.width*.5f; y=w->bounds.y+w->bounds.height*.5f;
        if (!dx_review_pick(registry,x,y,DX_REVIEW_PICK_CONTROL,&picked) || strcmp(picked.widget.id,w->id) || !dx_review_pick(registry,x,y,DX_REVIEW_PICK_CONTAINER,&picked) || strcmp(picked.widget.id,parent)) return false;
        if (!strcmp(w->role,"textbox")) {
            const char *text=item==3 && strstr(w->id,".columns") ? t->dialog_chart_text : t->dialog_text;
            if (!w->value.present || w->value.is_null || strcmp(w->value.type,"VARCHAR") || strcmp(w->value.exact_text,text) || w->value.truncated) return false;
        }
        rows++;
    }
    if (rows!=(item==3 ? 4 : 3) || strstr(a->review->frame.state_text," author_overflow=1")) return false;
    const char *keys[]={"width","destination","name","rows","save"}; snprintf(t->author_target,sizeof(t->author_target),"%s.action.%s",parent,keys[item]);
    return dx_review_find(registry,t->author_target)!=NULL;
}
static bool review_smoke_dialog_settled(App *a,int item) {
    if (item!=4) return smoke_work_idle(a);
    /* This real guard intentionally retains a pending Close. Keep every
       engine/UI ownership condition while allowing that expected request. */
    if (a->modal!=MODAL_UNSAVED || a->document_action!=DOCUMENT_CLOSE || a->document_saving ||
        a->busy || a->io_busy || a->values_loading || a->values_selecting || a->query_loading || a->step_preview.loading ||
        a->count_deferred || a->pending_sheet || a->pending_new_sheet || a->pending_relation || a->sheet_action!=SHEET_ACTION_NONE) return false;
    dx_lock(&a->worker.mutex); bool idle=!a->worker.running && !a->worker.pending && !a->worker.reply; dx_unlock(&a->worker.mutex);
    return idle;
}
static int review_smoke_dialog_tick(App *a,ReviewSmoke *t) {
    static const char *const kinds[]={"column_width","move_range","object_name","chart_size","unsaved"};
    int item=(t->stage-REVIEW_AUTHOR_END)/6,phase=(t->stage-REVIEW_AUTHOR_END)%6; DxReviewCapture *r=a->review;
    float scale=item==0 || item==2 ? 1 : 1.5f;
    if (phase==0) {
        if (r->phase!=DX_REPORT_IDLE || !smoke_work_idle(a)) return 0;
        a->modal=MODAL_NONE; a->consume_grid_input=true; a->pivot_drag_field=a->pivot_drag_role=-1;
        ui_set_scale(&a->ui,scale); SetWindowSize(scale==1 ? 1280 : 760,scale==1 ? 820 : 640);
        if (item==0) { a->selected_object=-1; grid_select_cell(&a->grid,20,8,false); column_action(a,COLUMN_WIDTH); input_set(&a->input,""); }
        else if (item==1) { a->selected_object=-1; grid_select_cell(&a->grid,20,8,false); begin_move_range(a,false); input_set(&a->input,"AA101"); a->input.cursor=2; a->input.anchor=1; }
        else if (item==2) { a->selected_object=0; begin_object_name(a); input_set(&a->input,"NULL"); a->input.cursor=4; a->input.anchor=1; }
        else if (item==3) {
            a->selected_object=0; if (!set_representation(a,1)) return -1; begin_chart_size(a);
            input_set(&a->chart_columns,"6"); input_set(&a->input,"12"); a->chart_columns.cursor=1; a->chart_columns.anchor=0; a->input.cursor=2; a->input.anchor=1; a->chart_size_focus=1;
        } else { a->book.dirty=true; request_document_action(a,DOCUMENT_CLOSE,NULL); a->document_focus=2; }
        return a->modal==MODAL_NONE ? -1 : 1;
    }
    if (phase==1) {
        if (r->phase!=DX_REPORT_IDLE || !review_smoke_dialog_settled(a,item) || a->ui.scale!=scale || !review_can_capture(a)) return 0;
        review_smoke_dialog_remember(a,t,item); ui_note_key_press(KEY_PERIOD,DX_MOD_SUPER|DX_MOD_SHIFT); return 1;
    }
    if (phase==2) {
        if (r->phase!=DX_REPORT_PICK) return 0;
        if (!review_smoke_dialog_same(a,t,item,true) || !review_smoke_dialog_targets(a,t,kinds[item],item)) return -1;
        r->toolbar_top=true; review_smoke_pick(a,t->author_target); return 1;
    }
    if (phase==3) {
        if (r->phase!=DX_REPORT_NOTE) return 0;
        if (strcmp(r->target.widget.id,t->author_target) || !review_smoke_dialog_same(a,t,item,true)) return -1;
        ui_note_key_press(KEY_ESCAPE,0); return 1;
    }
    if (r->phase!=DX_REPORT_IDLE) return 0;
    if (phase==4) {
        if (!review_smoke_dialog_same(a,t,item,true)) return -1;
        ui_note_key_press(KEY_ESCAPE,0); return 1;
    }
    if (a->modal!=MODAL_NONE) return -1;
    t->choice_modal=MODAL_NONE;
    if (!review_smoke_dialog_same(a,t,item,false)) return -1;
    printf("PASS: native dialog review kind=%s ui_scale=%.1f retains draft text/cursor/anchor, modal ownership, models and jobs\n",kinds[item],scale);
    review_smoke_author_release(t); return 1;
}
static int review_smoke_plot_identity_tick(App *a,ReviewSmoke *t) {
    int phase=t->stage-REVIEW_DIALOG_END; DxReviewCapture *r=a->review;
    if (phase==0) {
        if (r->phase!=DX_REPORT_IDLE || !smoke_work_idle(a)) return 0;
        a->selected_object=0; SheetObject *object=selected_pivot(a); if (!object) return -1;
        if (object->pivot.counts[PIVOT_VALUES]==1) {
            PivotDefinition definition=pivot_copy(&object->pivot); char *error=NULL; bool ok=pivot_assign(&definition,PIVOT_VALUES,"Amount",AGG_AVG,&error); free(error);
            if (!ok) { pivot_free(&definition); return -1; } if (!set_pivot_definition(a,&definition)) return -1; return 0;
        }
        if (object->pivot.counts[PIVOT_VALUES]!=2) return -1;
        show_workspace(a); a->pivot_focus=7; a->author_reveal=true;
        if (!review_smoke_author_select_ready(a,MODAL_CHART_MEASURE)) return 0;
        begin_author_select(a,MODAL_CHART_MEASURE); return a->modal==MODAL_CHART_MEASURE ? 1 : -1;
    }
    if (phase==1) {
        if (a->modal==MODAL_NONE) { fprintf(stderr,"FAIL: duplicate Plot controller closed before review capture\n"); return -1; }
        if (r->phase!=DX_REPORT_IDLE || !smoke_work_idle(a) || a->popup_presence.offset || a->popup_presence.alpha!=1 || !review_can_capture(a)) return 0;
        review_smoke_author_remember(a,t); ui_note_key_press(KEY_PERIOD,DX_MOD_SUPER|DX_MOD_SHIFT); return 1;
    }
    if (phase==2) {
        if (r->phase!=DX_REPORT_PICK) return 0;
        if (!review_smoke_author_same(a,t) || !review_smoke_author_targets(a,"plot","AVG(",t)) return -1;
        r->toolbar_top=true; review_smoke_pick(a,t->author_target); return 1;
    }
    if (phase==3) {
        if (r->phase!=DX_REPORT_NOTE) return 0;
        if (strcmp(r->target.widget.id,t->author_target) || strcmp(r->target.widget.data.field,"Amount") || r->target.widget.checked || !review_smoke_author_same(a,t)) return -1;
        ui_note_key_press(KEY_ESCAPE,0); return 1;
    }
    if (r->phase!=DX_REPORT_IDLE) return 0;
    if (!review_smoke_author_same(a,t)) return -1;
    a->modal=MODAL_NONE; review_smoke_author_release(t);
    printf("PASS: native Plot distinguishes SUM and AVG of the same raw field with separate semantic IDs and retained checked state\n"); return 1;
}
static bool review_smoke_sheet_fixture(App *a,ReviewSmoke *t) {
    if (t->sheet_fixture) return true;
    t->sheet_original=a->book.active; capture_sheet(a); char *error=NULL; size_t existing=a->book.count;
    for (size_t i=0;i<existing;i++) if (i!=t->sheet_original && !a->book.sheets[i].hidden && !workbook_hide(&a->book,i,true,&error)) goto failed;
    Analysis empty={0};
    if (!workbook_add(&a->book,"Review navigation β",&empty,&error)) goto failed;
    t->sheet_other=a->book.active;
    if (!cells_set(&a->book.sheets[t->sheet_other].cells,0,0,"NULL",&error)) goto failed;
    for (size_t i=0;i<15;i++) {
        char name[96]; snprintf(name,sizeof(name),"Hidden review worksheet %zu · İ",i+1);
        if (!workbook_add(&a->book,name,&empty,&error) || !workbook_hide(&a->book,a->book.active,true,&error)) goto failed;
    }
    if (a->book.active!=t->sheet_original) activate_sheet(a,t->sheet_original,false);
    uint64_t order=review_sheet_order(a);
    if (!workbook_reorder(&a->book,t->sheet_original,t->sheet_other,&error) || review_sheet_order(a)==order ||
        !workbook_reorder(&a->book,t->sheet_other,t->sheet_original,&error) || review_sheet_order(a)!=order) goto failed;
    size_t hidden=a->book.count-1;
    if (!workbook_hide(&a->book,hidden,false,&error) || review_sheet_order(a)==order || !workbook_hide(&a->book,hidden,true,&error) || review_sheet_order(a)!=order) goto failed;
    t->sheet_fixture=true; free(error); return true;
failed:
    fprintf(stderr,"FAIL: review worksheet fixture: %s\n",error ? error : "unknown workbook error"); free(error); return false;
}
static void review_smoke_sheet_remember(App *a,ReviewSmoke *t) {
    review_smoke_author_remember(a,t); t->sheet_count=a->book.count; t->sheet_active=a->book.active; t->sheet_index=a->sheet_menu_index;
    t->sheet_focus=a->sheet_menu_focus; t->sheet_scroll=a->sheet_scroll; t->sheet_unhide_scroll=a->unhide_scroll; t->sheet_generation=review_sheet_order(a);
    for (size_t i=0;i<a->book.count;i++) { t->sheet_names[i]=dx_strdup(a->book.sheets[i].name); t->sheet_hidden[i]=a->book.sheets[i].hidden; }
    t->dialog_text=dx_strdup(a->input.text ? a->input.text : ""); t->dialog_cursor=a->input.cursor; t->dialog_anchor=a->input.anchor; t->dialog_revision=a->input.revision;
}
static bool review_smoke_sheet_same(App *a,ReviewSmoke *t) {
    if (!review_smoke_author_same(a,t) || a->book.count!=t->sheet_count || a->book.active!=t->sheet_active || a->sheet_menu_index!=t->sheet_index || a->sheet_menu_focus!=t->sheet_focus ||
        a->sheet_scroll!=t->sheet_scroll || a->unhide_scroll!=t->sheet_unhide_scroll || a->sheet_action!=SHEET_ACTION_NONE || a->pending_sheet || a->pending_new_sheet ||
        strcmp(a->input.text ? a->input.text : "",t->dialog_text) || a->input.cursor!=t->dialog_cursor || a->input.anchor!=t->dialog_anchor || a->input.revision!=t->dialog_revision) return false;
    for (size_t i=0;i<a->book.count;i++) if (strcmp(a->book.sheets[i].name,t->sheet_names[i]) || a->book.sheets[i].hidden!=t->sheet_hidden[i]) return false;
    return true;
}
static bool review_smoke_sheet_data(const DxReviewWidget *widget,size_t sheet,uint64_t generation) {
    const DxReviewDataIdentity *data=&widget->data;
    return data->present && !strcmp(data->kind,"worksheet-position") && data->sheet_id==sheet+1 && data->query_generation==generation && !data->schema_generation && !data->source_id && !data->object_id && !data->field[0] && !data->has_cell;
}
static bool review_smoke_sheet_pick(App *a,const DxReviewWidget *widget,const char *parent) {
    if (!widget) return false; DxReviewTarget picked;
    Rectangle visible=GetCollisionRec((Rectangle){widget->bounds.x,widget->bounds.y,widget->bounds.width,widget->bounds.height},(Rectangle){widget->clip.x,widget->clip.y,widget->clip.width,widget->clip.height});
    if (visible.width<=0 || visible.height<=0) return false; float x=visible.x+visible.width*.5f,y=visible.y+visible.height*.5f;
    return dx_review_pick(a->review->registry,x,y,DX_REVIEW_PICK_CONTROL,&picked) && !strcmp(picked.widget.id,widget->id) &&
        dx_review_pick(a->review->registry,x,y,DX_REVIEW_PICK_CONTAINER,&picked) && !strcmp(picked.widget.id,parent);
}
static bool review_smoke_sheet_targets(App *a,ReviewSmoke *t,int item) {
    DxReviewRegistry *registry=a->review->registry; char id[128];
    t->author_target[0]=0;
    const char *kind=item==0 ? "sheet_menu" : item==3 ? "sheet_unhide" : "sheet_rename";
    char parent[128]; snprintf(parent,sizeof(parent),"author.%s",kind); const DxReviewWidget *panel=dx_review_find(registry,parent);
    if (!panel || !panel->container || panel->input_eligible || !review_smoke_sheet_data(panel,a->sheet_menu_index,t->sheet_generation) || registry->duplicate_count || registry->invalid_count || registry->overflow_count || strstr(a->review->frame.state_text," author_overflow=1")) return false;
    if (item==0) {
        static const char *const keys[]={"rename","duplicate","move.up","move.down","hide","unhide"};
        for (size_t i=0;i<6;i++) {
            snprintf(id,sizeof(id),"%s.action.%s",parent,keys[i]); const DxReviewWidget *row=dx_review_find(registry,id);
            if (!row || !review_smoke_sheet_data(row,a->sheet_menu_index,t->sheet_generation) || row->input_eligible!=row->enabled || !review_smoke_sheet_pick(a,row,parent)) return false;
        }
        const DxReviewWidget *up=dx_review_find(registry,"author.sheet_menu.action.move.up"); if (!up || up->enabled) return false;
        snprintf(t->author_target,sizeof(t->author_target),"%s",up->id);
        for (size_t i=0;i<a->book.count;i++) {
            snprintf(id,sizeof(id),"pane.workbook.sheet.%zu",i+1); const DxReviewWidget *row=dx_review_find(registry,id);
            if (a->book.sheets[i].hidden) { if (row) return false; continue; }
            if (!row || strcmp(row->role,"tab") || !review_smoke_sheet_data(row,i,t->sheet_generation) || row->checked!=(i==a->book.active) || row->input_eligible || !review_smoke_sheet_pick(a,row,"pane.workbook")) return false;
            if (i==t->sheet_other) t->sheet_navigation=(Rectangle){row->bounds.x,row->bounds.y,row->bounds.width,row->bounds.height};
        }
        return t->sheet_navigation.width>0;
    }
    if (item==1 || item==2) {
        snprintf(id,sizeof(id),"%s.action.name",parent); const DxReviewWidget *input=dx_review_find(registry,id);
        if (!input || strcmp(input->role,"textbox") || !review_smoke_sheet_data(input,a->sheet_menu_index,t->sheet_generation) || !input->value.present || input->value.is_null || strcmp(input->value.type,"VARCHAR") || strcmp(input->value.exact_text,item==1 ? "" : "NULL") || !review_smoke_sheet_pick(a,input,parent)) return false;
        snprintf(t->author_target,sizeof(t->author_target),"%s",input->id); return true;
    }
    UIChoiceListLayout layout=ui_choice_list_layout(a->context_origin,(Vector2){GetScreenWidth(),GetScreenHeight()},a->ui.scale,(int)(a->book.count-workbook_visible_count(&a->book)));
    size_t hidden[DX_MAX_SHEETS],count=0; for (size_t i=0;i<a->book.count;i++) if (a->book.sheets[i].hidden) hidden[count++]=i;
    if (!layout.frame.valid || count<=(size_t)layout.visible || a->unhide_scroll<=0 || a->sheet_menu_focus!=(int)count-1) return false;
    for (size_t i=0;i<count;i++) {
        snprintf(id,sizeof(id),"author.sheet_unhide.action.show.sheet%zu",hidden[i]+1); const DxReviewWidget *row=dx_review_find(registry,id);
        bool visible=i>=(size_t)a->unhide_scroll && i<(size_t)a->unhide_scroll+(size_t)layout.visible;
        if (!visible) { if (row) return false; continue; }
        if (!row || !review_smoke_sheet_data(row,hidden[i],t->sheet_generation) || !row->enabled || !row->input_eligible || row->focused!=((int)i==a->sheet_menu_focus) || !review_smoke_sheet_pick(a,row,parent)) return false;
        if (i==count-1) snprintf(t->author_target,sizeof(t->author_target),"%s",row->id);
    }
    const DxReviewWidget *track=dx_review_find(registry,"author.sheet_unhide.scrollbar"),*thumb=dx_review_find(registry,"author.sheet_unhide.scrollbar.thumb");
    return t->author_target[0] && track && thumb && thumb->bounds.y>track->bounds.y && !strcmp(thumb->parent_id,track->id) && review_smoke_sheet_pick(a,thumb,parent);
}
static void review_smoke_sheet_diagnostic(App *a,ReviewSmoke *t,int item,bool same,bool targets) {
    DxReviewRegistry *registry=a->review->registry;
    fprintf(stderr,"Review worksheet mismatch case=%d same=%d targets=%d modal=%d/%d active=%zu/%zu count=%zu/%zu index=%zu/%zu focus=%d/%d scroll=%d/%d unhide=%d/%d expected_order=%016llx actual_order=%016llx duplicates=%zu invalid=%zu overflow=%zu\n",item,same,targets,(int)a->modal,(int)t->choice_modal,a->book.active,t->sheet_active,a->book.count,t->sheet_count,a->sheet_menu_index,t->sheet_index,a->sheet_menu_focus,t->sheet_focus,a->sheet_scroll,t->sheet_scroll,a->unhide_scroll,t->sheet_unhide_scroll,(unsigned long long)t->sheet_generation,(unsigned long long)review_sheet_order(a),registry->duplicate_count,registry->invalid_count,registry->overflow_count);
    fprintf(stderr,"Review worksheet capture state: %s\n",a->review->frame.state_text);
    size_t shown=0;
    for (size_t i=0;i<registry->count && shown<80;i++) {
        const DxReviewWidget *w=&registry->widgets[i];
        if (strncmp(w->id,"author.sheet_",13) && strncmp(w->id,"pane.workbook",13)) continue;
        Rectangle visible=GetCollisionRec((Rectangle){w->bounds.x,w->bounds.y,w->bounds.width,w->bounds.height},(Rectangle){w->clip.x,w->clip.y,w->clip.width,w->clip.height});
        DxReviewTarget control={0},container={0}; float x=visible.x+visible.width*.5f,y=visible.y+visible.height*.5f;
        dx_review_pick(registry,x,y,DX_REVIEW_PICK_CONTROL,&control); dx_review_pick(registry,x,y,DX_REVIEW_PICK_CONTAINER,&container);
        fprintf(stderr,"Review worksheet widget id=%s parent=%s role=%s label=%s enabled=%d eligible=%d checked=%d focus=%d container=%d layer=%d kind=%s sheet=%llu source=%llu object=%llu field=%s schema=%llu order=%016llx bounds=%.2f,%.2f,%.2f,%.2f clip=%.2f,%.2f,%.2f,%.2f picked=%s owner=%s\n",w->id,w->parent_id,w->role,w->label,w->enabled,w->input_eligible,w->checked,w->focused,w->container,w->layer,w->data.kind,(unsigned long long)w->data.sheet_id,(unsigned long long)w->data.source_id,(unsigned long long)w->data.object_id,w->data.field,(unsigned long long)w->data.schema_generation,(unsigned long long)w->data.query_generation,w->bounds.x,w->bounds.y,w->bounds.width,w->bounds.height,w->clip.x,w->clip.y,w->clip.width,w->clip.height,control.found ? control.widget.id : "<none>",container.found ? container.widget.id : "<none>");
        shown++;
    }
}
static void review_smoke_sheet_pointer_diagnostic(App *a,ReviewSmoke *t,const char *when) {
    DxReviewCapture *r=a->review; const DxReviewWidget *widget=dx_review_find(r->registry,t->author_target);
    Rectangle bounds={0},clip={0},visible={0},display={0};
    if (widget) {
        bounds=(Rectangle){widget->bounds.x,widget->bounds.y,widget->bounds.width,widget->bounds.height};
        clip=(Rectangle){widget->clip.x,widget->clip.y,widget->clip.width,widget->clip.height}; visible=GetCollisionRec(bounds,clip);
    }
    float x=visible.x+visible.width*.5f,y=visible.y+visible.height*.5f;
    if (r->image.width>0 && r->image.height>0) {
        float w=GetScreenWidth(),h=GetScreenHeight(),aspect=(float)r->image.width/r->image.height;
        display=(Rectangle){0,0,w,h};
        if (w/h>aspect) { display.width=h*aspect; display.x=(w-display.width)*.5f; }
        else { display.height=w/aspect; display.y=(h-display.height)*.5f; }
    }
    DxReviewCaptureLayout picker=dx_review_capture_layout(GetScreenWidth(),GetScreenHeight(),a->ui.scale,r->toolbar_top);
    Vector2 mapped={0};
    if (display.width>0 && display.height>0) mapped=(Vector2){(x-display.x)*r->frame.window_width/display.width,(y-display.y)*r->frame.window_height/display.height};
    DxReviewTarget direct={0},transformed={0}; dx_review_pick(r->registry,x,y,r->mode,&direct); dx_review_pick(r->registry,mapped.x,mapped.y,r->mode,&transformed);
    fprintf(stderr,"Review worksheet pointer %s stage=%d target=%s bounds=%.2f,%.2f,%.2f,%.2f clip=%.2f,%.2f,%.2f,%.2f queued_point=%.2f,%.2f window=%dx%d render=%dx%d frame=%ux%u framebuffer=%ux%u image=%dx%d texture=%dx%d display=%.2f,%.2f,%.2f,%.2f mapped=%.2f,%.2f bar=%.2f,%.2f,%.2f,%.2f top=%d mode=%d phase=%d pending=%d blocked=%d mouse=%d/%d/%d,%d/%d/%d direct=%s transformed=%s actual=%s keyboard=%d dragging=%d\n",when,t->stage,t->author_target,bounds.x,bounds.y,bounds.width,bounds.height,clip.x,clip.y,clip.width,clip.height,x,y,GetScreenWidth(),GetScreenHeight(),GetRenderWidth(),GetRenderHeight(),r->frame.window_width,r->frame.window_height,r->frame.framebuffer_width,r->frame.framebuffer_height,r->image.width,r->image.height,r->texture.width,r->texture.height,display.x,display.y,display.width,display.height,mapped.x,mapped.y,picker.picker_bar.x,picker.picker_bar.y,picker.picker_bar.width,picker.picker_bar.height,r->toolbar_top,(int)r->mode,(int)r->phase,ui_has_pointer_gesture(),dx_review_input_blocked(),IsMouseButtonPressed(MOUSE_BUTTON_LEFT),IsMouseButtonReleased(MOUSE_BUTTON_LEFT),IsMouseButtonDown(MOUSE_BUTTON_LEFT),IsMouseButtonPressed(MOUSE_BUTTON_RIGHT),IsMouseButtonReleased(MOUSE_BUTTON_RIGHT),IsMouseButtonDown(MOUSE_BUTTON_RIGHT),direct.found ? direct.widget.id : "<none>",transformed.found ? transformed.widget.id : "<none>",r->target.found ? r->target.widget.id : "<none>",r->keyboard_pick,r->dragging);
}
static int review_smoke_sheet_tick(App *a,ReviewSmoke *t) {
    int item=(t->stage-REVIEW_PLOT_END)/6,phase=(t->stage-REVIEW_PLOT_END)%6; DxReviewCapture *r=a->review; float scale=item==3 ? 1.5f : 1;
    if (phase==0) {
        if (r->phase!=DX_REPORT_IDLE || !smoke_work_idle(a)) return 0;
        if (!review_smoke_sheet_fixture(a,t)) return -1;
        if (item==1) { const char *value=cells_get(&active_sheet(a)->cells,0,0); if (a->book.active!=t->sheet_other || !value || strcmp(value,"NULL")) return -1; }
        ui_set_scale(&a->ui,scale); SetWindowMinSize(400,180); SetWindowSize(item==3 ? 640 : 1280,item==3 ? 400 : 820);
        a->sidebar=true; a->workbook_collapsed=false; show_workspace(a); a->context_origin=(Vector2){item==3 ? 630 : 900,item==3 ? 390 : 300};
        a->pivot_drag_field=a->pivot_drag_role=-1; a->consume_grid_input=true;
        if (item==0) begin_sheet_menu(a,t->sheet_original,a->context_origin);
        else if (item==1 || item==2) {
            begin_sheet_rename(a,t->sheet_other); input_set(&a->input,item==1 ? "" : "NULL");
            if (item==2) { a->input.cursor=2; a->input.anchor=1; }
        } else { a->sheet_menu_index=t->sheet_other; a->modal=MODAL_UNHIDE; a->sheet_menu_focus=a->unhide_scroll=0; a->context_drag=false; ui_note_key_press(KEY_END,0); }
        return 1;
    }
    if (phase==1) {
        if (a->modal==MODAL_NONE) return -1;
        if (r->phase!=DX_REPORT_IDLE || !smoke_work_idle(a) || a->ui.scale!=scale || !review_can_capture(a) || (item!=1 && item!=2 && (a->popup_presence.offset || a->popup_presence.alpha!=1)) || a->workbook_open<.999f) return 0;
        if (item==3) {
            if (GetScreenWidth()!=640 || GetScreenHeight()!=400) return 0;
            int count=(int)(a->book.count-workbook_visible_count(&a->book));
            UIChoiceListLayout menu=ui_choice_list_layout(a->context_origin,(Vector2){GetScreenWidth(),GetScreenHeight()},a->ui.scale,count);
            if (!menu.frame.valid) return -1;
            /* The native resize notification may arrive after the first End
               gesture. Exercise End again in the actual short viewport so the
               focused last row is visible before freezing its real control. */
            if (a->sheet_menu_focus!=count-1 || a->sheet_menu_focus<a->unhide_scroll || a->sheet_menu_focus>=a->unhide_scroll+menu.visible) { ui_note_key_press(KEY_END,0); return 0; }
        }
        review_smoke_sheet_remember(a,t); ui_note_key_press(KEY_PERIOD,DX_MOD_SUPER|DX_MOD_SHIFT); return 1;
    }
    if (phase==2) {
        if (r->phase!=DX_REPORT_PICK) return 0;
        bool same=review_smoke_sheet_same(a,t),targets=same && review_smoke_sheet_targets(a,t,item);
        if (!same || !targets) { review_smoke_sheet_diagnostic(a,t,item,same,targets); return -1; }
        r->toolbar_top=true; review_smoke_pick(a,t->author_target);
        if (item==3) review_smoke_sheet_pointer_diagnostic(a,t,"queued"); return 1;
    }
    if (phase==3) {
        if (r->phase!=DX_REPORT_NOTE) {
            if (t->sheet_pick_diagnostic_stage!=t->stage) { review_smoke_sheet_pointer_diagnostic(a,t,"after picker draw"); t->sheet_pick_diagnostic_stage=t->stage; }
            return 0;
        }
        if (strcmp(r->target.widget.id,t->author_target) || !review_smoke_sheet_same(a,t)) return -1;
        ui_note_key_press(KEY_ESCAPE,0); return 1;
    }
    if (r->phase!=DX_REPORT_IDLE) return 0;
    if (phase==4) { if (!review_smoke_sheet_same(a,t)) return -1; ui_note_key_press(KEY_ESCAPE,0); return 1; }
    if (a->modal!=MODAL_NONE) return -1; t->choice_modal=MODAL_NONE;
    if (!review_smoke_sheet_same(a,t)) return -1;
    if (item==0) review_smoke_click(t->sheet_navigation);
    printf("PASS: native worksheet review case=%d retains captured position/order, names, hidden state, draft/selection and pages/jobs\n",item);
    review_smoke_author_release(t);
    if (item==3) { ui_set_scale(&a->ui,1); SetWindowMinSize(640,640); SetWindowSize(1280,820); }
    return 1;
}
static void review_smoke_query_remember(App *a,ReviewSmoke *t,bool pending) {
    review_smoke_author_remember(a,t); t->query_source=analysis_copy(query_source(a));
    t->query_object=a->query_object; t->query_focus=a->query_focus; t->query_source_revision=a->query_source_revision;
    t->query_draft_revision=a->query_draft_revision; t->query_edit_revision=a->query_edit_revision; t->query_editor=a->query_editor;
    t->query_owner=a->query_object>=0 ? active_sheet(a)->objects.items[a->query_object].id : 0;
    t->query_pending=pending; t->query_complete=false; t->query_reply=NULL;
    t->dialog_text=dx_strdup(a->input.text); t->dialog_cursor=a->input.cursor; t->dialog_anchor=a->input.anchor; t->dialog_revision=a->input.revision;
    for (size_t i=0;i<active_sheet(a)->objects.count;i++) {
        SheetObject *object=&active_sheet(a)->objects.items[i]; Page *page=&a->objects[i].page;
        t->query_views[i].cells=page->cells; t->query_views[i].id=object->id; t->query_views[i].revision=object->revision; t->query_views[i].source_id=object->source_id;
        t->query_views[i].total=page->total; t->query_views[i].rows=page->row_count; t->query_views[i].columns=page->column_count;
    }
}
static bool review_smoke_query_worker(App *a,ReviewSmoke *t,bool closed) {
    QueryWorker *worker=&a->worker; bool valid;
    dx_lock(&worker->mutex);
    if (!t->query_pending) valid=!worker->running && !worker->pending && !worker->reply && worker->jobs_started==t->query_jobs && worker->latest_id==t->query_latest && worker->db.query_count==t->query_sql && worker->db.count_queries==t->query_counts && worker->db.relation_prepares==t->query_prepares;
    else if (closed) valid=t->query_complete && !worker->running && !worker->pending && !worker->reply && worker->latest_id==t->request+1 && worker->jobs_started==t->query_jobs+1 && worker->db.query_count==t->query_complete_sql && worker->db.count_queries==t->query_complete_counts && worker->db.relation_prepares==t->query_complete_prepares;
    else {
        valid=worker->latest_id==t->request && worker->jobs_started>=t->query_jobs && worker->jobs_started<=t->query_jobs+1;
        if (worker->pending) valid&=worker->pending->id==t->request && worker->pending->kind==JOB_VALIDATE_SQL && worker->pending->object_id==t->query_owner && worker->pending->revision==t->query_source_revision;
        if (worker->running) valid&=worker->active_id==t->request && worker->jobs_started==t->query_jobs+1;
        if (worker->reply) {
            QueryReply *reply=worker->reply;
            valid&=!worker->running && !worker->pending && worker->jobs_started==t->query_jobs+1 && reply->id==t->request && reply->kind==JOB_VALIDATE_SQL && reply->object_id==t->query_owner && reply->revision==t->query_source_revision && !reply->error && reply->analysis.query && !strcmp(reply->analysis.query,t->dialog_text);
            if (valid && !t->query_complete) {
                t->query_complete=true; t->query_reply=reply; t->query_complete_sql=worker->db.query_count; t->query_complete_counts=worker->db.count_queries; t->query_complete_prepares=worker->db.relation_prepares;
            }
        }
        /* This fresh CSV draft is bound with duckdb_prepare, not executed.
           Preparing proves validation work; unchanged execution/count counters
           prove capture did not run an additional source query. */
        if (t->query_complete) valid&=worker->reply==t->query_reply && !worker->running && !worker->pending && worker->db.query_count==t->query_complete_sql && worker->db.count_queries==t->query_complete_counts && worker->db.relation_prepares==t->query_complete_prepares && t->query_complete_prepares==t->query_prepares+1 && t->query_complete_sql==t->query_sql && t->query_complete_counts==t->query_counts;
        else valid&=worker->pending || worker->running;
    }
    dx_unlock(&worker->mutex); return valid;
}
static bool review_smoke_query_same(App *a,ReviewSmoke *t,bool closed) {
    GridLocation location=grid_location(&a->grid); const Analysis *source=query_source(a); TextEditor *editor=&a->query_editor;
    if (a->modal!=(closed ? MODAL_NONE : MODAL_SOURCE_QUERY) || a->selected_object!=t->author_selected || a->query_object!=t->query_object || a->query_focus!=t->query_focus || a->query_source_revision!=t->query_source_revision ||
        a->query_draft_revision!=t->query_draft_revision || a->query_edit_revision!=t->query_edit_revision || a->query_loading!=(t->query_pending && !closed) || a->busy!=(t->query_pending && !closed) || a->query_validated || a->query_error || a->io_busy ||
        a->request_id!=t->request+(closed && t->query_pending ? 1 : 0) || a->page.cells!=t->page || a->book.dirty!=t->dirty || active_sheet(a)->undo_actions_count!=t->history || active_sheet(a)->formats.revision!=t->choice_formats ||
        a->ribbon_mode!=t->choice_ribbon_mode || a->ui.scale!=t->choice_scale || a->grid.row_height!=t->choice_row_height || location.row!=t->selection.row || location.col!=t->selection.col || location.end_row!=t->selection.end_row || location.end_col!=t->selection.end_col ||
        strcmp(a->input.text,t->dialog_text) || a->input.cursor!=t->dialog_cursor || a->input.anchor!=t->dialog_anchor || a->input.revision!=t->dialog_revision ||
        editor->scroll!=t->query_editor.scroll || editor->revision!=t->query_editor.revision || editor->revealed_revision!=t->query_editor.revealed_revision || editor->revealed_cursor!=t->query_editor.revealed_cursor || editor->lines.offsets!=t->query_editor.lines.offsets ||
        !source || !analysis_same_result(source,&t->query_source) || !analysis_sorts_equal(source,&t->query_source) || !review_smoke_author_models_same(a,t)) return false;
    for (size_t i=0;i<active_sheet(a)->objects.count;i++) {
        SheetObject *object=&active_sheet(a)->objects.items[i]; Page *page=&a->objects[i].page;
        if (page->cells!=t->query_views[i].cells || object->id!=t->query_views[i].id || object->revision!=t->query_views[i].revision || object->source_id!=t->query_views[i].source_id || page->total!=t->query_views[i].total || page->row_count!=t->query_views[i].rows || page->column_count!=t->query_views[i].columns) return false;
    }
    return review_smoke_query_worker(a,t,closed);
}
static bool review_smoke_query_targets(App *a,ReviewSmoke *t) {
    DxReviewRegistry *registry=a->review->registry;
    const char *parent="author.source_query",*keys[]={"sql","original","cancel","apply"};
    const DxReviewWidget *panel=dx_review_find(registry,parent),*backdrop=dx_review_find(registry,"author.source_query.backdrop");
    if (!panel || strcmp(panel->role,"dialog") || !panel->container || panel->input_eligible || !backdrop || !backdrop->container || registry->duplicate_count || registry->invalid_count || registry->overflow_count || strstr(a->review->frame.state_text," author_overflow=1")) return false;
    for (size_t i=0;i<registry->count;i++) {
        const DxReviewWidget *widget=&registry->widgets[i]; if (strncmp(widget->id,"author.source_query",19)) continue;
        const DxReviewDataIdentity *data=&widget->data;
        if (!data->present || strcmp(data->kind,"source-query") || data->sheet_id!=a->book.active+1 || data->source_id!=t->query_owner || data->object_id!=t->query_owner || data->query_generation!=t->query_source_revision || data->schema_generation || data->field[0] || data->has_cell || data->row || data->column) return false;
    }
    for (size_t i=0;i<4;i++) {
        char id[128]; snprintf(id,sizeof(id),"%s.action.%s",parent,keys[i]); const DxReviewWidget *widget=dx_review_find(registry,id);
        bool enabled=i!=3 || !t->query_pending;
        if (!widget || strcmp(widget->role,i ? "button" : "textbox") || widget->enabled!=enabled || widget->input_eligible!=enabled || widget->focused!=(a->query_focus==(int)i) || !review_smoke_sheet_pick(a,widget,parent)) return false;
        if (!i && (!widget->value.present || widget->value.is_null || strcmp(widget->value.type,"VARCHAR") || strcmp(widget->value.exact_text,t->dialog_text))) return false;
    }
    const DxReviewWidget *heading=dx_review_find(registry,"author.source_query.heading"); DxReviewTarget picked;
    if (!heading || heading->input_eligible || !review_smoke_sheet_pick(a,heading,parent) ||
        !dx_review_pick(registry,2*a->ui.scale,2*a->ui.scale,DX_REVIEW_PICK_CONTROL,&picked) || strcmp(picked.widget.id,"author.source_query.backdrop.background")) return false;
    snprintf(t->author_target,sizeof(t->author_target),"%s.action.%s",parent,t->query_pending ? "apply" : "sql"); return true;
}
static void review_smoke_query_diagnostic(App *a,ReviewSmoke *t,const char *reason) {
    fprintf(stderr,"Review source-query mismatch %s stage=%d modal=%d selected=%d/%d query_object=%d/%d focus=%d/%d loading=%d busy=%d pending=%d complete=%d request=%llu/%llu source_revision=%llu/%llu draft_revision=%llu/%llu edit_revision=%llu/%llu input_revision=%llu/%llu cursor=%zu/%zu anchor=%zu/%zu editor_scroll=%d/%d editor_revision=%llu/%llu target=%s actual=%s\n",reason,t->stage,(int)a->modal,a->selected_object,t->author_selected,a->query_object,t->query_object,a->query_focus,t->query_focus,a->query_loading,a->busy,t->query_pending,t->query_complete,(unsigned long long)a->request_id,(unsigned long long)t->request,(unsigned long long)a->query_source_revision,(unsigned long long)t->query_source_revision,(unsigned long long)a->query_draft_revision,(unsigned long long)t->query_draft_revision,(unsigned long long)a->query_edit_revision,(unsigned long long)t->query_edit_revision,(unsigned long long)a->input.revision,(unsigned long long)t->dialog_revision,a->input.cursor,t->dialog_cursor,a->input.anchor,t->dialog_anchor,a->query_editor.scroll,t->query_editor.scroll,(unsigned long long)a->query_editor.revision,(unsigned long long)t->query_editor.revision,t->author_target,a->review->target.widget.id);
    dx_lock(&a->worker.mutex); QueryWorker *worker=&a->worker;
    fprintf(stderr,"Review source-query worker running=%d latest=%llu active=%llu jobs=%llu/%llu pending=%d/%llu reply=%d/%llu owned_reply=%d sql=%llu base=%llu completed=%llu\n",worker->running,(unsigned long long)worker->latest_id,(unsigned long long)worker->active_id,(unsigned long long)worker->jobs_started,(unsigned long long)t->query_jobs,worker->pending ? (int)worker->pending->kind : -1,(unsigned long long)(worker->pending ? worker->pending->id : 0),worker->reply ? (int)worker->reply->kind : -1,(unsigned long long)(worker->reply ? worker->reply->id : 0),worker->reply==t->query_reply,(unsigned long long)(worker->running ? 0 : worker->db.query_count),(unsigned long long)t->query_sql,(unsigned long long)t->query_complete_sql);
    dx_unlock(&a->worker.mutex);
    for (size_t i=0;i<a->review->registry->count;i++) {
        const DxReviewWidget *widget=&a->review->registry->widgets[i]; if (strncmp(widget->id,"author.source_query",19)) continue;
        fprintf(stderr,"Review source-query widget %s role=%s enabled=%d eligible=%d focused=%d kind=%s owner=%llu source=%llu schema=%llu query=%llu bounds=%.2f,%.2f,%.2f,%.2f clip=%.2f,%.2f,%.2f,%.2f\n",widget->id,widget->role,widget->enabled,widget->input_eligible,widget->focused,widget->data.kind,(unsigned long long)widget->data.object_id,(unsigned long long)widget->data.source_id,(unsigned long long)widget->data.schema_generation,(unsigned long long)widget->data.query_generation,widget->bounds.x,widget->bounds.y,widget->bounds.width,widget->bounds.height,widget->clip.x,widget->clip.y,widget->clip.width,widget->clip.height);
    }
}
static int review_smoke_query_tick(App *a,ReviewSmoke *t) {
    int item=(t->stage-REVIEW_SHEET_END)/6,phase=(t->stage-REVIEW_SHEET_END)%6; bool pending=item==1; DxReviewCapture *r=a->review;
    if (!phase) {
        if (r->phase!=DX_REPORT_IDLE || !smoke_work_idle(a)) return 0;
        if (a->book.active!=t->sheet_original) { activate_sheet(a,t->sheet_original,false); return 0; }
        if (pending) {
            if (!t->query_table_added) {
                a->selected_object=-1; grid_select_cell(&a->grid,30,10,false); if (!import_table(a,"tests/fixtures/transactions.csv")) return -1;
                t->query_table=a->selected_object; t->query_table_added=true; return 0;
            }
            if (!object_ready(a,(size_t)t->query_table)) return 0;
            if (!t->query_pivot_added) {
                a->selected_object=t->query_table; create_pivot(a); if (!selected_pivot(a)) return -1;
                t->query_pivot=a->selected_object; t->query_pivot_added=true; PivotDefinition pivot={0}; char *error=NULL;
                bool ok=pivot_assign(&pivot,PIVOT_ROWS,"City",AGG_SUM,&error) && pivot_assign(&pivot,PIVOT_VALUES,"Amount",AGG_SUM,&error); free(error);
                if (!ok) { pivot_free(&pivot); return -1; } if (!set_pivot_definition(a,&pivot)) return -1; return 0;
            }
            if (!object_ready(a,(size_t)t->query_pivot)) return 0;
        } else if (!object_ready(a,0)) return 0;
        a->selected_object=pending ? t->query_pivot : 0; show_workspace(a); edit_source_query(a);
        if (a->modal!=MODAL_SOURCE_QUERY || a->query_object!=(pending ? t->query_table : -1) || !selected_pivot(a)) return -1;
        ui_set_scale(&a->ui,pending ? 1.5f : 1); SetWindowSize(pending ? 760 : 1280,pending ? 640 : 820);
        input_set(&a->input,pending ? "SELECT ID, City, Amount::DECIMAL(12,2) AS Amount,\n       Date, Approved, Note\nFROM input\nWHERE Approved\nORDER BY City" : "SELECT City,\n       Amount::DECIMAL(12,2) AS \"Tutar β\"\nFROM input\nORDER BY City");
        a->input.select_all=false; a->input.cursor=23; a->input.anchor=12; a->query_edit_revision=a->input.revision; a->query_focus=pending ? 3 : 0; return 1;
    }
    if (phase==1) {
        if (a->modal!=MODAL_SOURCE_QUERY) return -1;
        if (r->phase!=DX_REPORT_IDLE || !smoke_work_idle(a) || !review_can_capture(a) || GetScreenWidth()!=(pending ? 760 : 1280) || GetScreenHeight()!=(pending ? 640 : 820)) return 0;
        dx_lock(&a->worker.mutex); uint64_t jobs=a->worker.jobs_started,sql=a->worker.db.query_count,counts=a->worker.db.count_queries,prepares=a->worker.db.relation_prepares,latest=a->worker.latest_id; dx_unlock(&a->worker.mutex);
        if (pending) { submit_source_query(a); if (!a->query_loading || !a->busy || a->pending_kind!=JOB_VALIDATE_SQL) return -1; }
        review_smoke_query_remember(a,t,pending); t->query_jobs=jobs; t->query_sql=sql; t->query_counts=counts; t->query_prepares=prepares; t->query_latest=latest;
        ui_note_key_press(KEY_PERIOD,DX_MOD_SUPER|DX_MOD_SHIFT); return 1;
    }
    if (phase==2) {
        if (r->phase!=DX_REPORT_PICK) return 0;
        bool same=review_smoke_query_same(a,t,false),targets=same && review_smoke_query_targets(a,t);
        if (!same || !targets) { review_smoke_query_diagnostic(a,t,same ? "targets" : "retained state"); return -1; }
        r->toolbar_top=true; review_smoke_pick(a,t->author_target); return 1;
    }
    if (phase==3) {
        if (r->phase!=DX_REPORT_NOTE) return 0;
        bool already_complete=t->query_complete;
        if (strcmp(r->target.widget.id,t->author_target) || !review_smoke_query_same(a,t,false) || (pending && r->target.widget.enabled)) { review_smoke_query_diagnostic(a,t,"actual picker target/state"); return -1; }
        if (pending && (!already_complete || !t->query_complete)) return 0;
        ui_note_key_press(KEY_ESCAPE,0); return 1;
    }
    if (r->phase!=DX_REPORT_IDLE) return 0;
    if (phase==4) { if (!review_smoke_query_same(a,t,false)) { review_smoke_query_diagnostic(a,t,"report cancellation"); return -1; } ui_note_key_press(KEY_ESCAPE,0); return 1; }
    if (!review_smoke_query_same(a,t,true)) { review_smoke_query_diagnostic(a,t,"normal editor cancellation"); return -1; }
    printf("PASS: native source-query review owner=%s pending=%d retains SQL/cursor/focus, source and dependent pages; validation jobs=%llu prepares=%llu executed SQL=%llu\n",pending ? "upstream table" : "primary",pending,(unsigned long long)(pending ? 1 : 0),(unsigned long long)(pending ? t->query_complete_prepares-t->query_prepares : 0),(unsigned long long)(pending ? t->query_complete_sql-t->query_sql : 0));
    review_smoke_author_release(t);
    if (pending) { ui_set_scale(&a->ui,1); SetWindowSize(1280,820); } return 1;
}
static void review_smoke_relation_remember(App *a,ReviewSmoke *t) {
    review_smoke_author_remember(a,t); SheetObject *table=&active_sheet(a)->objects.items[a->table_relation_object]; Page *page=&a->objects[a->table_relation_object].page;
    t->relation_source=analysis_copy(&table->source); t->relation_id=table->id; t->relation_revision=table->revision;
    t->relation_object=a->table_relation_object; t->relation_focus=a->table_relation_focus; t->relation_scroll=a->table_relation_scroll; t->relation_rows=page->relations; t->relation_count=page->relation_count;
    t->dialog_text=dx_strdup(a->input.text); t->dialog_cursor=a->input.cursor; t->dialog_anchor=a->input.anchor; t->dialog_revision=a->input.revision;
    for (size_t i=0;i<active_sheet(a)->objects.count;i++) {
        SheetObject *object=&active_sheet(a)->objects.items[i]; Page *cached=&a->objects[i].page;
        t->query_views[i].cells=cached->cells; t->query_views[i].id=object->id; t->query_views[i].revision=object->revision; t->query_views[i].source_id=object->source_id;
        t->query_views[i].total=cached->total; t->query_views[i].rows=cached->row_count; t->query_views[i].columns=cached->column_count;
    }
    t->query_pending=false;
    dx_lock(&a->worker.mutex); t->query_jobs=a->worker.jobs_started; t->query_sql=a->worker.db.query_count; t->query_counts=a->worker.db.count_queries; t->query_prepares=a->worker.db.relation_prepares; t->query_latest=a->worker.latest_id; dx_unlock(&a->worker.mutex);
}
static bool review_smoke_relation_same(App *a,ReviewSmoke *t) {
    if (!review_smoke_author_same(a,t) || a->table_relation_object!=t->relation_object || a->table_relation_focus!=t->relation_focus || a->table_relation_scroll!=t->relation_scroll || a->table_relation_drag ||
        strcmp(a->input.text,t->dialog_text) || a->input.cursor!=t->dialog_cursor || a->input.anchor!=t->dialog_anchor || a->input.revision!=t->dialog_revision) return false;
    SheetObject *table=&active_sheet(a)->objects.items[a->table_relation_object]; Page *page=&a->objects[a->table_relation_object].page;
    if (table->id!=t->relation_id || table->revision!=t->relation_revision || !analysis_same_result(&table->source,&t->relation_source) || !analysis_sorts_equal(&table->source,&t->relation_source) || page->relations!=t->relation_rows || page->relation_count!=t->relation_count || page->relation_count!=2 ||
        strcmp(page->relations[0].schema,"main") || strcmp(page->relations[0].name,"a view") || strcmp(page->relations[1].schema,"main") || strcmp(page->relations[1].name,"table with spaces")) return false;
    for (size_t i=0;i<active_sheet(a)->objects.count;i++) {
        SheetObject *object=&active_sheet(a)->objects.items[i]; Page *cached=&a->objects[i].page;
        if (cached->cells!=t->query_views[i].cells || object->id!=t->query_views[i].id || object->revision!=t->query_views[i].revision || object->source_id!=t->query_views[i].source_id || cached->total!=t->query_views[i].total || cached->row_count!=t->query_views[i].rows || cached->column_count!=t->query_views[i].columns) return false;
    }
    return review_smoke_query_worker(a,t,false);
}
static bool review_smoke_relation_targets(App *a,ReviewSmoke *t) {
    DxReviewRegistry *registry=a->review->registry; const char *parent="author.source_relation"; const DxReviewWidget *rows[2]={0};
    const DxReviewWidget *panel=dx_review_find(registry,parent),*cancel=dx_review_find(registry,"author.source_relation.action.cancel");
    if (!panel || strcmp(panel->role,"dialog") || !panel->container || panel->input_eligible || !cancel || strcmp(cancel->role,"button") || !cancel->enabled || !cancel->input_eligible || cancel->focused || !review_smoke_sheet_pick(a,cancel,parent) || registry->duplicate_count || registry->invalid_count || registry->overflow_count || strstr(a->review->frame.state_text," author_overflow=1")) return false;
    for (size_t i=0;i<registry->count;i++) {
        const DxReviewWidget *widget=&registry->widgets[i]; if (strncmp(widget->id,"author.source_relation",22)) continue;
        const DxReviewDataIdentity *data=&widget->data;
        if (!data->present || strcmp(data->kind,"source-relation") || data->sheet_id!=a->book.active+1 || data->source_id!=t->relation_id || data->object_id!=t->relation_id || data->query_generation!=t->relation_revision || data->schema_generation || data->field[0] || data->has_cell || data->column) return false;
        if (strcmp(widget->role,"menuitemradio")) continue;
        if (data->row>1 || rows[data->row] || !strstr(widget->id,".action.select.s") || !widget->enabled || !widget->input_eligible || widget->checked!=(data->row==0) || widget->focused!=(data->row==1) ||
            !widget->value.present || widget->value.is_null || widget->value.truncated || strcmp(widget->value.type,"VARCHAR") || strcmp(widget->value.exact_text,data->row ? "\"main\".\"table with spaces\"" : "\"main\".\"a view\"") || !review_smoke_sheet_pick(a,widget,parent)) return false;
        rows[data->row]=widget;
    }
    if (!rows[0] || !rows[1] || !strcmp(rows[0]->id,rows[1]->id) || dx_review_find(registry,"author.source_relation.scrollbar")) return false;
    const DxReviewWidget *heading=dx_review_find(registry,"author.source_relation.heading"); DxReviewTarget picked;
    if (!heading || heading->input_eligible || !review_smoke_sheet_pick(a,heading,parent) || !dx_review_pick(registry,2*a->ui.scale,2*a->ui.scale,DX_REVIEW_PICK_CONTROL,&picked) || strcmp(picked.widget.id,"author.source_relation.backdrop.background")) return false;
    snprintf(t->author_target,sizeof(t->author_target),"%s",rows[1]->id); return true;
}
static void review_smoke_relation_diagnostic(App *a,ReviewSmoke *t,const char *reason) {
    fprintf(stderr,"Review source-relation mismatch %s stage=%d modal=%d selected=%d expected_selected=%d owner_index=%d/%d focus=%d/%d scroll=%d/%d owner=%llu revision=%llu target=%s actual=%s\n",reason,t->stage,(int)a->modal,a->selected_object,t->author_selected,a->table_relation_object,t->relation_object,a->table_relation_focus,t->relation_focus,a->table_relation_scroll,t->relation_scroll,(unsigned long long)t->relation_id,(unsigned long long)t->relation_revision,t->author_target,a->review->target.widget.id);
    for (size_t i=0;i<a->review->registry->count;i++) {
        const DxReviewWidget *widget=&a->review->registry->widgets[i]; if (strncmp(widget->id,"author.source_relation",22)) continue;
        fprintf(stderr,"Review source-relation widget %s role=%s enabled=%d eligible=%d checked=%d focused=%d kind=%s owner=%llu source=%llu row=%llu schema=%llu query=%llu value=%s bounds=%.2f,%.2f,%.2f,%.2f clip=%.2f,%.2f,%.2f,%.2f\n",widget->id,widget->role,widget->enabled,widget->input_eligible,widget->checked,widget->focused,widget->data.kind,(unsigned long long)widget->data.object_id,(unsigned long long)widget->data.source_id,(unsigned long long)widget->data.row,(unsigned long long)widget->data.schema_generation,(unsigned long long)widget->data.query_generation,widget->value.exact_text,widget->bounds.x,widget->bounds.y,widget->bounds.width,widget->bounds.height,widget->clip.x,widget->clip.y,widget->clip.width,widget->clip.height);
    }
}
static int review_smoke_relation_tick(App *a,ReviewSmoke *t) {
    int phase=t->stage-REVIEW_QUERY_END; DxReviewCapture *r=a->review;
    if (!phase) {
        if (r->phase!=DX_REPORT_IDLE || !smoke_work_idle(a)) return 0;
        if (!t->relation_added) {
            if (!FileExists("tests/generated/fixture.duckdb")) { fprintf(stderr,"FAIL: relation review requires the core-generated tests/generated/fixture.duckdb\n"); return -1; }
            a->selected_object=-1; grid_select_cell(&a->grid,60,10,false); if (!import_table(a,"tests/generated/fixture.duckdb")) return -1;
            t->relation_object=a->selected_object; t->relation_added=true; return 0;
        }
        if (!object_ready(a,(size_t)t->relation_object)) return 0;
        SheetObject *table=&active_sheet(a)->objects.items[t->relation_object]; Page *page=&a->objects[t->relation_object].page;
        if (table->source.kind!=SOURCE_DATABASE || !table->source.table || strcmp(table->source.table,"a view") || page->relation_count!=2) return -1;
        a->selected_object=t->relation_object; begin_table_relation(a); if (a->modal!=MODAL_TABLE_RELATION) return -1;
        a->selected_object=t->query_pivot; if (!selected_pivot(a) || selected_object(a)->id==table->id) return -1;
        ui_set_scale(&a->ui,1.5f); SetWindowSize(760,640); ui_note_key_press(KEY_END,0); return 1;
    }
    if (phase==1) {
        if (a->modal!=MODAL_TABLE_RELATION) return -1;
        if (r->phase!=DX_REPORT_IDLE || !smoke_work_idle(a) || !review_can_capture(a) || a->ui.scale!=1.5f || GetScreenWidth()!=760 || GetScreenHeight()!=640) return 0;
        if (a->table_relation_focus!=1) { ui_note_key_press(KEY_END,0); return 0; }
        review_smoke_relation_remember(a,t); ui_note_key_press(KEY_PERIOD,DX_MOD_SUPER|DX_MOD_SHIFT); return 1;
    }
    if (phase==2) {
        if (r->phase!=DX_REPORT_PICK) return 0;
        bool same=review_smoke_relation_same(a,t),targets=same && review_smoke_relation_targets(a,t);
        if (!same || !targets) { review_smoke_relation_diagnostic(a,t,same ? "targets" : "retained state"); return -1; }
        r->toolbar_top=true; review_smoke_pick(a,t->author_target); return 1;
    }
    if (phase==3) {
        if (r->phase!=DX_REPORT_NOTE) return 0;
        if (strcmp(r->target.widget.id,t->author_target) || r->target.widget.checked || strcmp(r->target.widget.value.exact_text,"\"main\".\"table with spaces\"") || !review_smoke_relation_same(a,t)) { review_smoke_relation_diagnostic(a,t,"actual picker target/state"); return -1; }
        ui_note_key_press(KEY_ESCAPE,0); return 1;
    }
    if (r->phase!=DX_REPORT_IDLE) return 0;
    if (phase==4) { if (!review_smoke_relation_same(a,t)) { review_smoke_relation_diagnostic(a,t,"report cancellation"); return -1; } ui_note_key_press(KEY_ESCAPE,0); return 1; }
    t->choice_modal=MODAL_NONE;
    if (!review_smoke_relation_same(a,t)) { review_smoke_relation_diagnostic(a,t,"normal chooser cancellation"); return -1; }
    printf("PASS: native source-relation review retains actual table ownership, quoted cached names, source/model/pages and SQL/job counts\n");
    review_smoke_author_release(t); ui_set_scale(&a->ui,1); SetWindowSize(1280,820); return 1;
}
static const Analysis *review_smoke_sort_source(App *a,int index) { return index>=0 ? &active_sheet(a)->objects.items[index].source : &a->analysis; }
static void review_smoke_sort_remember(App *a,ReviewSmoke *t) {
    review_smoke_author_remember(a,t); SortEditor *editor=&a->sort_editor;
    t->sort_source=analysis_copy(review_smoke_sort_source(a,a->sort_object)); t->sort_draft=analysis_copy(&editor->draft); t->sort_object=a->sort_object;
    t->sort_owner=a->sort_object>=0 ? active_sheet(a)->objects.items[a->sort_object].id : 0; t->sort_revision=a->sort_object>=0 ? active_sheet(a)->objects.items[a->sort_object].revision : a->analysis_revision;
    t->sort_focus=editor->focus; t->sort_level=editor->level; t->sort_choice=editor->choice; t->sort_field_scroll=editor->field_scroll; t->sort_order_scroll=editor->order_scroll; t->sort_count=editor->count; t->sort_matches=editor->matches; t->sort_search_revision=editor->search_revision;
    t->dialog_text=dx_strdup(editor->search.text); t->dialog_cursor=editor->search.cursor; t->dialog_anchor=editor->search.anchor; t->dialog_revision=editor->search.revision;
    for (size_t i=0;i<active_sheet(a)->objects.count;i++) {
        SheetObject *object=&active_sheet(a)->objects.items[i]; Page *page=&a->objects[i].page;
        t->query_views[i].cells=page->cells; t->query_views[i].id=object->id; t->query_views[i].revision=object->revision; t->query_views[i].source_id=object->source_id;
        t->query_views[i].total=page->total; t->query_views[i].rows=page->row_count; t->query_views[i].columns=page->column_count;
    }
    t->query_pending=false;
    dx_lock(&a->worker.mutex); t->query_jobs=a->worker.jobs_started; t->query_sql=a->worker.db.query_count; t->query_counts=a->worker.db.count_queries; t->query_prepares=a->worker.db.relation_prepares; t->query_latest=a->worker.latest_id; dx_unlock(&a->worker.mutex);
}
static bool review_smoke_sort_same(App *a,ReviewSmoke *t,bool closed) {
    SortEditor *editor=&a->sort_editor; const Analysis *source=review_smoke_sort_source(a,t->sort_object);
    if (!review_smoke_author_same(a,t) || a->sort_object!=(closed ? -1 : t->sort_object) || !analysis_same_result(source,&t->sort_source) || !analysis_sorts_equal(source,&t->sort_source) || !analysis_sorts_equal(&editor->draft,&t->sort_draft) ||
        editor->focus!=t->sort_focus || editor->level!=t->sort_level || editor->choice!=t->sort_choice || editor->count!=t->sort_count || editor->matches!=t->sort_matches || editor->field_scroll!=t->sort_field_scroll || editor->order_scroll!=t->sort_order_scroll || editor->search_revision!=t->sort_search_revision ||
        editor->just_opened || editor->dragging || editor->error || strcmp(editor->search.text,t->dialog_text) || editor->search.cursor!=t->dialog_cursor || editor->search.anchor!=t->dialog_anchor || editor->search.revision!=t->dialog_revision) return false;
    for (size_t i=0;i<active_sheet(a)->objects.count;i++) {
        SheetObject *object=&active_sheet(a)->objects.items[i]; Page *page=&a->objects[i].page;
        if (page->cells!=t->query_views[i].cells || object->id!=t->query_views[i].id || object->revision!=t->query_views[i].revision || object->source_id!=t->query_views[i].source_id || page->total!=t->query_views[i].total || page->row_count!=t->query_views[i].rows || page->column_count!=t->query_views[i].columns) return false;
    }
    return review_smoke_query_worker(a,t,false);
}
static bool review_smoke_sort_targets(App *a,ReviewSmoke *t,int item) {
    DxReviewRegistry *registry=a->review->registry; const DxReviewWidget *panel=dx_review_find(registry,"author.sort"),*search=dx_review_find(registry,"author.sort.fields.search");
    if (!panel || strcmp(panel->role,"dialog") || !panel->container || panel->input_eligible || !search || strcmp(search->role,"textbox") || !search->value.present || search->value.is_null || strcmp(search->value.type,"VARCHAR") || strcmp(search->value.exact_text,t->dialog_text) || search->focused || !review_smoke_sheet_pick(a,search,"author.sort.fields") ||
        registry->duplicate_count || registry->invalid_count || registry->overflow_count || a->sort_editor.review.overflow || !strstr(a->review->frame.state_text," sort_overflow=0")) return false;
    size_t targets=0,fields=0;
    for (size_t i=0;i<registry->count;i++) {
        const DxReviewWidget *widget=&registry->widgets[i]; if (strncmp(widget->id,"author.sort",11)) continue; targets++;
        const DxReviewDataIdentity *data=&widget->data;
        if (!data->present || strcmp(data->kind,"sort-draft") || data->sheet_id!=a->book.active+1 || data->source_id!=t->sort_owner || data->object_id!=t->sort_owner || data->query_generation!=t->sort_revision || data->schema_generation || data->has_cell || !strstr(widget->source_file,"src/ui/sortview.c") || !widget->source_line) return false;
        if (!strncmp(widget->id,"author.sort.fields.field.",25)) {
            fields++; const Page *page=t->sort_object>=0 ? &a->objects[t->sort_object].page : &a->page;
            if (data->column>=page->column_count || strcmp(data->field,page->columns[data->column].name) || !widget->enabled || !widget->input_eligible || !review_smoke_sheet_pick(a,widget,"author.sort.fields")) return false;
        }
    }
    if (targets!=a->sort_editor.review.count+3 || fields!=t->sort_count) return false;
    for (size_t i=0;i<3;i++) {
        AnalysisSort sort=analysis_sort_at(&a->sort_editor.draft,i); char parent[128],id[128]; snprintf(parent,sizeof(parent),"author.sort.priority.f%016llx",(unsigned long long)review_field_hash(sort.column));
        const DxReviewWidget *level=dx_review_find(registry,parent);
        if (!level || !level->container || strcmp(level->parent_id,"author.sort.priorities") || strcmp(level->data.field,sort.column) || level->data.row!=i || level->focused!=(a->sort_editor.focus==2 && a->sort_editor.level==(int)i)) return false;
        const char *keys[]={"heading","move_up","move_down","remove","ascending","descending"};
        for (size_t j=0;j<6;j++) {
            snprintf(id,sizeof(id),"%s.%s",parent,keys[j]); const DxReviewWidget *widget=dx_review_find(registry,id);
            bool enabled=j==1 ? i>0 : j==2 ? i+1<3 : true,checked=j==4 ? !sort.descending : j==5 ? sort.descending : false;
            if (!widget || widget->enabled!=enabled || widget->checked!=checked || widget->input_eligible!=(enabled && j!=0) || widget->data.row!=i || strcmp(widget->data.field,sort.column) || !review_smoke_sheet_pick(a,widget,parent)) return false;
        }
        snprintf(id,sizeof(id),"%s.select",parent); const DxReviewWidget *select=dx_review_find(registry,id); DxReviewTarget picked;
        if (!select || !select->input_eligible || !dx_review_pick(registry,select->bounds.x+select->bounds.width-a->ui.scale,select->bounds.y+select->bounds.height-a->ui.scale,DX_REVIEW_PICK_CONTROL,&picked) || strcmp(picked.widget.id,id)) return false;
    }
    char city[128]; snprintf(city,sizeof(city),"author.sort.priority.f%016llx.move_up",(unsigned long long)review_field_hash("City"));
    const DxReviewWidget *up=dx_review_find(registry,city);
    if (!up || up->enabled!=(item==1) || up->data.row!=(uint64_t)item) return false;
    if (!item) { snprintf(t->sort_city_up,sizeof(t->sort_city_up),"%s",up->id); snprintf(t->author_target,sizeof(t->author_target),"%s",up->id); }
    else {
        if (strcmp(t->sort_city_up,up->id)) return false;
        snprintf(t->author_target,sizeof(t->author_target),"author.sort.priority.f%016llx.descending",(unsigned long long)review_field_hash("Amount"));
        const DxReviewWidget *descending=dx_review_find(registry,t->author_target); if (!descending || !descending->checked || strcmp(descending->data.field,"Amount") || descending->data.row) return false;
    }
    const char *buttons[]={"close","clear","cancel","apply"};
    for (size_t i=0;i<4;i++) { char id[128]; snprintf(id,sizeof(id),"author.sort.%s",buttons[i]); const DxReviewWidget *widget=dx_review_find(registry,id); if (!widget || !widget->enabled || !widget->input_eligible || !review_smoke_sheet_pick(a,widget,"author.sort")) return false; }
    return true;
}
static void review_smoke_sort_diagnostic(App *a,ReviewSmoke *t,const char *reason) {
    fprintf(stderr,"Review multi-sort mismatch %s stage=%d modal=%d selected=%d/%d owner_index=%d/%d owner=%llu focus=%d/%d level=%d/%d fields=%zu/%zu scroll=%d/%d,%d/%d snapshot=%zu overflow=%d target=%s actual=%s\n",reason,t->stage,(int)a->modal,a->selected_object,t->author_selected,a->sort_object,t->sort_object,(unsigned long long)t->sort_owner,a->sort_editor.focus,t->sort_focus,a->sort_editor.level,t->sort_level,a->sort_editor.count,t->sort_count,a->sort_editor.field_scroll,t->sort_field_scroll,a->sort_editor.order_scroll,t->sort_order_scroll,a->sort_editor.review.count,a->sort_editor.review.overflow,t->author_target,a->review->target.widget.id);
    for (size_t i=0;i<a->review->registry->count;i++) {
        const DxReviewWidget *widget=&a->review->registry->widgets[i]; if (strncmp(widget->id,"author.sort",11)) continue;
        fprintf(stderr,"Review multi-sort widget %s parent=%s role=%s enabled=%d checked=%d focus=%d row=%llu field=%s bounds=%.2f,%.2f,%.2f,%.2f clip=%.2f,%.2f,%.2f,%.2f\n",widget->id,widget->parent_id,widget->role,widget->enabled,widget->checked,widget->focused,(unsigned long long)widget->data.row,widget->data.field,widget->bounds.x,widget->bounds.y,widget->bounds.width,widget->bounds.height,widget->clip.x,widget->clip.y,widget->clip.width,widget->clip.height);
    }
}
static int review_smoke_sort_tick(App *a,ReviewSmoke *t) {
    int item=(t->stage-REVIEW_RELATION_END)/6,phase=(t->stage-REVIEW_RELATION_END)%6; DxReviewCapture *r=a->review;
    if (!phase) {
        if (r->phase!=DX_REPORT_IDLE || !smoke_work_idle(a)) return 0;
        int selected=item ? t->query_pivot : 0; a->selected_object=item ? t->query_table : -1; open_modal(a,MODAL_SORT); a->selected_object=selected;
        if (a->modal!=MODAL_SORT || a->sort_object!=(item ? t->query_table : -1) || !selected_pivot(a)) return -1;
        char *error=NULL; Analysis *draft=&a->sort_editor.draft;
        bool ok=analysis_sort_set(draft,"City",false,false,&error) && analysis_sort_set(draft,"Amount",true,true,&error) && analysis_sort_set(draft,"ID",false,true,&error); free(error); if (!ok) return -1;
        input_set(&a->sort_editor.search,item ? "ity" : ""); a->sort_editor.search.select_all=false; a->sort_editor.search.cursor=item ? 2 : 0; a->sort_editor.search.anchor=item ? 1 : 0; a->sort_editor.focus=2; a->sort_editor.level=0;
        ui_set_scale(&a->ui,item ? 1.5f : 1); SetWindowSize(item ? 760 : 1280,820); return 1;
    }
    if (phase==1) {
        if (a->modal!=MODAL_SORT) return -1;
        if (r->phase!=DX_REPORT_IDLE || !smoke_work_idle(a) || !review_can_capture(a) || a->sort_editor.just_opened || a->ui.scale!=(item ? 1.5f : 1) || GetScreenWidth()!=(item ? 760 : 1280) || GetScreenHeight()!=820) return 0;
        if (item && !strcmp(analysis_sort_at(&a->sort_editor.draft,0).column,"City")) { ui_note_key_press(KEY_DOWN,DX_MOD_ALT); return 0; }
        if (a->sort_editor.count!=(item ? 1 : 6) || (item && strcmp(analysis_sort_at(&a->sort_editor.draft,1).column,"City"))) return -1;
        review_smoke_sort_remember(a,t); ui_note_key_press(KEY_PERIOD,DX_MOD_SUPER|DX_MOD_SHIFT); return 1;
    }
    if (phase==2) {
        if (r->phase!=DX_REPORT_PICK) return 0;
        bool same=review_smoke_sort_same(a,t,false),targets=same && review_smoke_sort_targets(a,t,item);
        if (!same || !targets) { review_smoke_sort_diagnostic(a,t,same ? "targets" : "retained state"); return -1; }
        r->toolbar_top=true; review_smoke_pick(a,t->author_target); return 1;
    }
    if (phase==3) {
        if (r->phase!=DX_REPORT_NOTE) return 0;
        if (strcmp(r->target.widget.id,t->author_target) || r->target.widget.enabled!=(item==1) || !review_smoke_sort_same(a,t,false)) { review_smoke_sort_diagnostic(a,t,"actual picker target/state"); return -1; }
        ui_note_key_press(KEY_ESCAPE,0); return 1;
    }
    if (r->phase!=DX_REPORT_IDLE) return 0;
    if (phase==4) { if (!review_smoke_sort_same(a,t,false)) { review_smoke_sort_diagnostic(a,t,"report cancellation"); return -1; } ui_note_key_press(KEY_ESCAPE,0); return 1; }
    t->choice_modal=MODAL_NONE; if (!review_smoke_sort_same(a,t,true)) { review_smoke_sort_diagnostic(a,t,"normal editor cancellation"); return -1; }
    printf("PASS: native multi-sort review owner=%s ui_scale=%.1f retains draft order, search selection, stable field priorities, source/pages and query jobs\n",item ? "upstream table" : "primary",item ? 1.5 : 1.0);
    review_smoke_author_release(t); if (item) { ui_set_scale(&a->ui,1); SetWindowSize(1280,820); printf("Review smoke completed [0,%d)\n",REVIEW_SMOKE_STAGES); } return 1;
}
static void review_smoke_tick(App *a,ReviewSmoke *t) {
    DxReviewCapture *r=a->review; if (!r || t->stage<0 || t->stage>=REVIEW_SMOKE_STAGES) return;
    if (t->diagnostic_stage!=t->stage) {
        fprintf(stderr,"Review smoke stage=%d phase=%d file=%d/%d/%d ui_scale=%.2f window=%dx%d target=%s\n",t->stage,(int)r->phase,a->file_menu,a->file_menu_focus,a->file_menu_scroll,a->ui.scale,GetScreenWidth(),GetScreenHeight(),r->target.widget.id);
        t->diagnostic_stage=t->stage; t->wait_frame=a->frames;
    }
    if (a->frames-t->wait_frame>=60) {
        Rectangle plot=author_select_field(a,MODAL_CHART_MEASURE); SheetObject *object=selected_object(a);
        fprintf(stderr,"Review smoke waiting stage=%d phase=%d modal=%d object=%d ui_scale=%.2f busy=%d/%d idle=%d capture=%d popup=%.3f/%.3f plot=%.2f,%.2f,%.2f,%.2f request=%llu\n",t->stage,(int)r->phase,(int)a->modal,object ? (int)object->kind : -1,a->ui.scale,a->busy,a->io_busy,smoke_work_idle(a),review_can_capture(a),a->popup_presence.alpha,a->popup_presence.offset,plot.x,plot.y,plot.width,plot.height,(unsigned long long)a->request_id);
        t->wait_frame=a->frames;
    }
    if (t->stage>=71) {
        int progress=t->stage>=REVIEW_RELATION_END ? review_smoke_sort_tick(a,t) : t->stage>=REVIEW_QUERY_END ? review_smoke_relation_tick(a,t) : t->stage>=REVIEW_SHEET_END ? review_smoke_query_tick(a,t) : t->stage>=REVIEW_PLOT_END ? review_smoke_sheet_tick(a,t) : t->stage>=REVIEW_DIALOG_END ? review_smoke_plot_identity_tick(a,t) : t->stage>=REVIEW_AUTHOR_END ? review_smoke_dialog_tick(a,t) : review_smoke_author_tick(a,t);
        if (progress<0) goto failed; if (!progress) return;
        t->stage++; return;
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
            for (int drag=1;drag<=2;drag++) { a->sort_editor.dragging=drag; bool captured=review_can_capture(a); a->sort_editor.dragging=0; if (captured) goto failed; }
            a->file_menu_drag=true;
            if (!review_can_capture(a)) goto failed; /* A closed menu owns no active gesture. */
            a->file_menu=true; if (review_can_capture(a)) goto failed;
            a->file_menu=a->file_menu_drag=false;
            a->pivot_drag_field=0; if (review_can_capture(a)) goto failed;
            a->modal=MODAL_PIVOT_FIELD; if (!review_can_capture(a)) goto failed;
            a->pivot_drag_field=-1; a->pivot_drag_role=PIVOT_ROWS;
            if (!review_can_capture(a)) goto failed;
            a->modal=MODAL_NONE; if (review_can_capture(a)) goto failed;
            a->pivot_drag_role=-1;
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
        /* Both hidden swap buffers must contain the edited note before readback. */
        if (!t->note_frame) { t->note_frame=a->frames; return; }
        if (a->frames-t->note_frame<2) return;
        smoke_snapshot("tests/results/native-review/compact-composer-150.png");
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
        printf("PASS: short 150 percent review composer has a visible multiline editor, pointer focus, actual Backspace and pointer Save with retained frozen state\n"); break;
    }
    }
    t->stage++; return;
failed:
    fprintf(stderr,"FAIL: native review stage=%d phase=%d target=%s error=%s\n",t->stage,(int)r->phase,r->target.widget.id,r->error); review_smoke_author_release(t); t->stage=-1;
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
