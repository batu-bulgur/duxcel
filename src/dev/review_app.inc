/* Review-only App adapter. Included after the normal controllers so targets
   use their actual geometry/cache, with no database access or retained cells. */
#include "review_capture_layout.h"
#ifdef DUXCEL_BROWSER_HOST
#include <emscripten/heap.h>
#include <malloc.h>
#endif
static DxReviewRect review_rect(Rectangle r) { return (DxReviewRect){r.x,r.y,r.width,r.height}; }
static bool review_armed(App *a) { return a->review && a->review->phase==DX_REPORT_ARMED; }
static bool review_author_hex64(char output[129],const char *text);
static void review_document_id(App *a,char output[128]) {
    snprintf(output,128,"%s-document-%u",a->review->frame.session_id,a->review->document_serial);
}
static DxReviewDataIdentity review_scope(App *a) {
    DxReviewDataIdentity d={.present=true,.sheet_id=a->book.active+1,.schema_generation=a->field_schema_generation,.query_generation=a->analysis_revision,.source_id=0};
    snprintf(d.kind,sizeof(d.kind),"query-position");
    review_document_id(a,d.document_id);
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
    review_document_id(a,f.document_id);
    snprintf(f.fonts,sizeof(f.fonts),"%s; IBMPlexSans-Medium.otf; IBMPlexSans-SemiBold.otf; %s",getenv("DUXCEL_FONT") ? getenv("DUXCEL_FONT") : "IBMPlexSans-Regular.otf",getenv("DUXCEL_MONO_FONT") ? getenv("DUXCEL_MONO_FONT") : "JetBrainsMono-Regular.ttf");
    snprintf(f.state_text,sizeof(f.state_text),"sheet=%zu modal=%d ribbon=%d/%d focus=%d/%d/%d filter_field=%zu filter_focus=%d filter_menu=%d search_cursor=%zu search_anchor=%zu search_revision=%llu active=%llu,%zu anchor=%llu,%zu extent=%llu,%zu top=%llu horizontal=%.3f page=%llu/%zu spare=%llu/%zu source_revision=%llu schema=%llu request=%llu busy=%d io_busy=%d dirty=%d",a->book.active,(int)a->modal,a->ribbon_tab,a->ribbon_mode,a->chrome_focus,a->pivot_focus,a->ribbon_focus,a->field,a->filter_focus,a->filter_condition_menu,a->input.cursor,a->input.anchor,(unsigned long long)a->input.revision,(unsigned long long)a->grid.row,a->grid.col,(unsigned long long)a->grid.anchor_row,a->grid.anchor_col,(unsigned long long)a->grid.end_row,a->grid.end_col,(unsigned long long)a->grid.top,a->grid.horizontal,(unsigned long long)a->page.offset,a->page.row_count,(unsigned long long)a->spare.offset,a->spare.row_count,(unsigned long long)a->analysis_revision,(unsigned long long)a->field_schema_generation,(unsigned long long)a->request_id,a->busy,a->io_busy,a->book.dirty);
    size_t used=strlen(f.state_text);
    snprintf(f.state_text+used,sizeof(f.state_text)-used," fraction=%.9f row_height=%.6f elastic=%.6f,%.6f velocity=%.6f,%.6f body=%.3f,%.3f,%.3f,%.3f keytips=%.6f",a->grid.fraction,a->grid.row_height>=18 ? a->grid.row_height : 27,a->grid.elastic.x,a->grid.elastic.y,a->grid.spring_velocity.x,a->grid.spring_velocity.y,a->grid.body.x,a->grid.body.y,a->grid.body.width,a->grid.body.height,a->keytip_alpha);
    used=strlen(f.state_text);
    float cell_scale=a->ui.scale*a->grid.zoom;
    snprintf(f.state_text+used,sizeof(f.state_text)-used," grid_rect=%.3f,%.3f,%.3f,%.3f grid_gutter=%.3f grid_header=%.3f grid_cell=%.3f,%.3f source_header=%d grid_selection=%d",a->grid.rect.x,a->grid.rect.y,a->grid.rect.width,a->grid.rect.height,a->grid.body.x-a->grid.rect.x,a->grid.body.y-a->grid.rect.y,a->grid.width_count ? a->grid.widths[0]*cell_scale : 0,(a->grid.row_height>=18 ? a->grid.row_height : 27)*cell_scale,a->grid.source_header,(int)a->grid.selection);
    used=strlen(f.state_text);
    Worksheet *sheet=active_sheet(a); CellFormat format=active_format(a);
    snprintf(f.state_text+used,sizeof(f.state_text)-used," keytip_tab=%d keytip_prefix=%s file_menu=%d active_format_flags=%u manual_cells=%zu sheet_count=%zu selection_history=%zu,%zu selection_matrix=%d",a->keytip_state.tab,a->keytip_state.prefix[0] ? a->keytip_state.prefix : "-",a->file_menu,format.flags,sheet ? sheet->cells.count : 0,a->book.count,sheet ? sheet->undo_actions_count : 0,sheet ? sheet->redo_actions_count : 0,a->matrix_cells_active);
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
    used=strlen(f.state_text);
    snprintf(f.state_text+used,sizeof(f.state_text)-used," browser_picker=%u,%d,%llu,%zu,%llu,%zu",a->browser_file_intent,a->browser_file_path!=NULL,(unsigned long long)a->browser_file_revision,a->browser_file_sheet,(unsigned long long)a->browser_file_row,a->browser_file_col);
    char clipboard_message[129]; bool clipboard_truncated=review_author_hex64(clipboard_message,a->message);
    used=strlen(f.state_text);
    snprintf(f.state_text+used,sizeof(f.state_text)-used," clipboard_pending=%u clipboard_completed=%u clipboard_status=%u clipboard_message_hex=%s clipboard_message_truncated=%d",a->browser_clipboard_pending,a->browser_clipboard_completed,a->browser_clipboard_status,clipboard_message,clipboard_truncated);
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
    static const char *const suffix[]={"new","open","save","save_as","close","import","export","quit"};
    static const char *const actions[]={"command.new_workbook","command.open","command.save","command.save_as","command.close","command.import_table","command.export","command.quit"};
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
        !strcmp(kind,"chart_size") || !strcmp(kind,"unsaved") || !strcmp(kind,"source_query") || !strcmp(kind,"source_relation") || !strcmp(kind,"sort") || !strcmp(kind,"sheet_rename") || !strcmp(kind,"cell_inspector") || !strcmp(kind,"settings") || !strcmp(kind,"generated_sql") || !strcmp(kind,"guide") || !strcmp(kind,"error");
}
static bool review_author_field(char output[128],const char *field) {
    size_t length=strlen(field);
    if (length<128) { memcpy(output,field,length+1); return false; }
    size_t end=124; while (end && ((unsigned char)field[end]&0xc0)==0x80) end--;
    memcpy(output,field,end); memcpy(output+end,"…",4); return true;
}
/* Called only after inspect_value has resolved and copied its actual value.
   The origin outlives Page publication/eviction just as inspector_text does. */
static void review_inspector_pin(App *a,const Page *page,const SheetObject *object,uint64_t row,size_t column,bool manual) {
    DxReviewDataIdentity data=review_scope(a);
    data.source_id=data.object_id=data.schema_generation=data.query_generation=0;
    data.has_cell=true; data.row=row; data.column=column; data.field[0]=0;
    bool stale=false,truncated=false;
    if (manual) {
        snprintf(data.kind,sizeof(data.kind),"inspector-manual"); data.query_generation=active_sheet(a)->cells.revision;
    } else {
        snprintf(data.kind,sizeof(data.kind),"inspector-source");
        if (page && column<page->column_count) truncated=review_author_field(data.field,page->columns[column].name);
        if (a->step_preview.active) {
            snprintf(data.kind,sizeof(data.kind),"inspector-preview"); stale=a->step_preview.loading;
            data.source_id=a->step_preview.source_id;
            if (!stale) data.query_generation=a->step_preview.source_revision;
        } else if (object) {
            snprintf(data.kind,sizeof(data.kind),"inspector-result"); data.object_id=object->id;
            ObjectStore *store=&active_sheet(a)->objects;
            for (size_t i=0;i<store->count;i++) if (&store->items[i]==object) {
                const ObjectView *view=&a->objects[i];
                data.query_generation=view->revision; data.schema_generation=view->source_revision;
                stale=view->dirty || view->failed || view->revision!=object->revision || view->source_revision!=object_source_revision(a,object);
                /* Cached revisions cannot recover an old upstream binding.
                   Do not relabel its owned value with the new source ID. */
                if (!stale) data.source_id=object->kind==OBJECT_TABLE ? object->id : object->source_id;
                break;
            }
        } else { stale=a->page_stale; if (!stale) data.query_generation=a->analysis_revision; }
    }
    a->inspector_origin=data; a->inspector_origin_valid=true; a->inspector_origin_stale=stale; a->inspector_origin_truncated=truncated;
}
/* SQL is an owned definition/result snapshot. A source inspector may restore
   a selected Pivot afterwards, so only its producing controller pins origin. */
static void review_sql_pin(App *a,const SheetObject *owner,bool cached) {
    a->sql_origin=(DxReviewDataIdentity){0}; a->sql_origin_valid=a->sql_origin_stale=false;
    if (!a->sql_text) return;
    DxReviewDataIdentity data=review_scope(a); snprintf(data.kind,sizeof(data.kind),"generated-source-sql");
    data.source_id=data.object_id=data.schema_generation=0; data.query_generation=a->analysis_revision;
    data.has_cell=false; data.row=data.column=0; data.field[0]=0; bool stale=false;
    if (owner) {
        const ObjectStore *store=&active_sheet(a)->objects; const SheetObject *source=objects_source(store,owner);
        data.object_id=owner->id; data.source_id=source ? source->id : 0; data.query_generation=owner->revision;
        if (cached) {
            snprintf(data.kind,sizeof(data.kind),"generated-result-sql"); stale=true;
            for (size_t i=0;i<store->count;i++) if (&store->items[i]==owner) {
                const ObjectView *view=&a->objects[i]; data.query_generation=view->revision; data.schema_generation=view->source_revision;
                stale=view->dirty || view->failed || view->revision!=owner->revision || view->source_revision!=object_source_revision(a,owner); break;
            }
            if (stale) data.source_id=0;
        } else if (owner->kind!=OBJECT_TABLE) {
            snprintf(data.kind,sizeof(data.kind),owner->pivot.counts[PIVOT_VALUES] ? "generated-pivot-template-sql" : "generated-empty-pivot-sql");
            data.schema_generation=object_source_revision(a,owner);
        }
    }
    a->sql_origin=data; a->sql_origin_valid=true; a->sql_origin_stale=stale;
}
static const char *review_author_identity(App *a,const char *kind,char parent[128],DxReviewDataIdentity *data,bool *truncated) {
    SheetObject *object=selected_object(a); const char *field=NULL,*scope=NULL;
    static const char *const roles[]={"rows","columns","values","filters"};
    static const char *const marks[]={"color","size","label","detail","tooltip","shape"};
    if (!strcmp(kind,"error")) {
        *data=(DxReviewDataIdentity){.present=true}; review_document_id(a,data->document_id); snprintf(data->kind,sizeof(data->kind),"document-error");
        snprintf(parent,128,"author.error"); return NULL;
    }
    if (!strcmp(kind,"guide")) {
        *data=(DxReviewDataIdentity){.present=true}; review_document_id(a,data->document_id); snprintf(data->kind,sizeof(data->kind),"guide-document");
        snprintf(parent,128,"author.guide"); return NULL;
    }
    if (!strcmp(kind,"generated_sql")) {
        if (a->sql_origin_valid) *data=a->sql_origin;
        else { *data=review_scope(a); data->source_id=data->object_id=data->schema_generation=data->query_generation=0; data->has_cell=false; data->row=data->column=0; data->field[0]=0; snprintf(data->kind,sizeof(data->kind),"generated-unknown-sql"); }
        snprintf(parent,128,"author.%s",kind); return NULL;
    }
    if (!strcmp(kind,"settings")) {
        /* Application preferences do not target the selected worksheet/data. */
        *data=(DxReviewDataIdentity){0}; snprintf(data->kind,sizeof(data->kind),"app-settings");
        snprintf(parent,128,"author.%s",kind); return NULL;
    }
    if (!strcmp(kind,"font_size") || !strcmp(kind,"number_format")) {
        *data=review_scope(a); snprintf(data->kind,sizeof(data->kind),"worksheet-format");
        data->source_id=data->object_id=data->schema_generation=0; data->query_generation=active_sheet(a)->formats.revision;
        uint64_t r0,r1; size_t c0,c1; grid_bounds(&a->grid,&r0,&r1,&c0,&c1); data->row=r0; data->column=c0; data->has_cell=true; data->field[0]=0;
        snprintf(parent,128,"author.%s",kind); return NULL;
    }
    if (!strcmp(kind,"cell_inspector")) {
        if (a->inspector_origin_valid) *data=a->inspector_origin;
        else { *data=review_scope(a); data->source_id=data->object_id=data->schema_generation=data->query_generation=0; data->has_cell=false; data->row=data->column=0; data->field[0]=0; snprintf(data->kind,sizeof(data->kind),"inspector-unknown"); }
        if (truncated) *truncated|=a->inspector_origin_truncated;
        snprintf(parent,128,"author.%s",kind); return NULL;
    }
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
    if (!strcmp(kind,"statistics")) {
        *data=review_scope(a); data->source_id=data->object_id=data->query_generation=data->schema_generation=0; data->has_cell=false; data->field[0]=0;
        const Page *page=&a->page; bool one_column=a->statistics_columns==1;
        if (a->step_preview.active) {
            StepPreview *preview=&a->step_preview; snprintf(data->kind,sizeof(data->kind),"step-statistics");
            data->source_id=preview->source_id; data->schema_generation=preview->source_revision; data->query_generation=preview->request;
            data->row=preview->stats_bounds.r0; data->column=preview->stats_bounds.c0; one_column=preview->stats_bounds.c0==preview->stats_bounds.c1; page=&preview->page;
        } else {
            snprintf(data->kind,sizeof(data->kind),a->statistics_frame ? "statistics-frame" : a->statistics_object==-2 ? "statistics-mixed" : "statistics-range");
            data->row=a->statistics_first; data->column=a->statistics_column; data->query_generation=a->statistics_key.source_revision;
            ObjectStore *store=&active_sheet(a)->objects;
            if (a->statistics_object>=0 && (size_t)a->statistics_object<store->count) {
                SheetObject *owner=&store->items[a->statistics_object]; data->object_id=owner->id; data->source_id=owner->kind==OBJECT_TABLE ? owner->id : owner->source_id;
                data->query_generation=owner->revision; data->schema_generation=owner->kind==OBJECT_TABLE ? 0 : object_source_revision(a,owner); page=&a->objects[a->statistics_object].page;
            }
            if (a->statistics_frame || a->statistics_object==-2) { uint64_t r0,r1; size_t c0,c1; grid_bounds(&a->grid,&r0,&r1,&c0,&c1); data->row=r0; data->column=c0; one_column=false; }
        }
        if (one_column && data->column<page->column_count && review_author_field(data->field,page->columns[data->column].name) && truncated) *truncated=true;
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
static void review_shadow_state(App *a) {
    if (!review_armed(a)) return;
    const UIShadowLayout *layout=&a->pipeline_shadow_layout;
    char state[256]; snprintf(state,sizeof(state)," feedback_pipeline_shadow=%d,%d,%llu,%zu,%d,%d,%.6f,%.6f,%.6f",a->pipeline_shadow_ready,a->pipeline_shadow_active,(unsigned long long)a->pipeline_shadow_uploads,a->pipeline_shadow_ready ? layout->bytes : (size_t)0,a->pipeline_shadow_texture.width,a->pipeline_shadow_texture.height,layout->scale,layout->density.x,layout->density.y);
    review_author_capture_state(a,state);
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
    if (!strcmp(kind,"error")) {
        const DxReviewWidget *widget=dx_review_find(a->review->registry,parent);
        if (widget) { const DxReviewDataIdentity *origin=&widget->data; char identity[256]; snprintf(identity,sizeof(identity)," error_kind=%s error_origin=%llu,%llu,%llu,%llu,%llu,%d,%llu,%llu",origin->kind,(unsigned long long)origin->sheet_id,(unsigned long long)origin->object_id,(unsigned long long)origin->source_id,(unsigned long long)origin->schema_generation,(unsigned long long)origin->query_generation,origin->has_cell,(unsigned long long)origin->row,(unsigned long long)origin->column); review_author_capture_state(a,identity); }
    }
    if (!strcmp(kind,"source_query")) {
        char query[320]; snprintf(query,sizeof(query)," query_object=%d query_source_revision=%llu query_draft_revision=%llu query_focus=%d query_loading=%d query_cursor=%zu query_anchor=%zu query_scroll=%d",a->query_object,(unsigned long long)a->query_source_revision,(unsigned long long)a->input.revision,a->query_focus,a->query_loading,a->input.cursor,a->input.anchor,a->query_editor.scroll);
        review_author_capture_state(a,query);
    }
    if (!strcmp(kind,"source_relation")) {
        size_t count=a->table_relation_object>=0 && (size_t)a->table_relation_object<active_sheet(a)->objects.count ? a->objects[a->table_relation_object].page.relation_count : 0;
        char relation[192]; snprintf(relation,sizeof(relation)," relation_object=%d relation_source_revision=%llu relation_focus=%d relation_scroll=%d relation_count=%zu",a->table_relation_object,(unsigned long long)data.query_generation,a->table_relation_focus,a->table_relation_scroll,count);
        review_author_capture_state(a,relation);
    }
    if (!strcmp(kind,"statistics")) {
        uint64_t r0,r1; size_t c0,c1; Grid *grid=a->step_preview.active ? &a->step_preview.grid : &a->grid; grid_bounds(grid,&r0,&r1,&c0,&c1);
        const SelectionStats *cached=a->step_preview.active ? &a->step_preview.statistics : &a->statistics;
        char stats[512]; snprintf(stats,sizeof(stats)," stats_menu_mask=%u stats_menu_focus=%d stats_menu_frame=%d stats_menu_enabled=%d stats_menu_preview=%d stats_menu_range=%llu,%llu,%zu,%zu stats_menu_object=%d stats_menu_state=%d stats_menu_flags=%d,%d,%d,%d",a->statistics_mask,a->statistics_focus,!a->step_preview.active && a->statistics_frame,a->statistics_enabled,a->step_preview.active,(unsigned long long)r0,(unsigned long long)r1,c0,c1,a->statistics_object,a->step_preview.active ? (int)a->step_preview.stats_state : (int)a->statistics_state,cached->approximate,cached->non_finite,cached->sum_overflow,cached->average_overflow);
        review_author_capture_state(a,stats);
    }
    if (!strcmp(kind,"cell_inspector")) {
        char inspector[320]; snprintf(inspector,sizeof(inspector)," inspector_bytes=%zu inspector_null=%d inspector_scroll=%zu inspector_lines=%zu inspector_limited=%d inspector_origin_valid=%d inspector_origin_stale=%d inspector_origin_truncated=%d inspector_value_truncated=%d",a->inspector_bytes,a->inspector_null,a->inspector_scroll,a->inspector_lines.count,a->inspector_lines.limited,a->inspector_origin_valid,a->inspector_origin_stale,a->inspector_origin_truncated,a->inspector_bytes>=sizeof(((DxReviewValue *)0)->exact_text));
        review_author_capture_state(a,inspector);
    }
    if (!strcmp(kind,"settings")) {
        char settings[160]; snprintf(settings,sizeof(settings)," settings_scale=%.3f settings_requested_scale=%.3f settings_reduced_motion=%d",a->ui.scale,a->ui.requested_scale,a->reduced_motion);
        review_author_capture_state(a,settings);
    }
    if (!strcmp(kind,"font_size") || !strcmp(kind,"number_format")) {
        uint64_t r0,r1; size_t c0,c1; grid_bounds(&a->grid,&r0,&r1,&c0,&c1); CellFormat current=active_format(a);
        char format[512]; snprintf(format,sizeof(format)," format_range=%llu,%llu,%zu,%zu format_revision=%llu format_value=%u,%u,%u,%u,%u,%u format_parent=%d,%d format_focus=%d format_scroll=%d format_origin=%.3f,%.3f format_width=%.3f",(unsigned long long)r0,(unsigned long long)r1,c0,c1,(unsigned long long)active_sheet(a)->formats.revision,current.flags,current.align,current.color,current.size,current.number,current.decimals,a->format_return_group,a->format_return_command,a->context_focus,a->context_scroll,a->format_origin.x,a->format_origin.y,a->format_width);
        review_author_capture_state(a,format);
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
static bool review_author_hex64(char output[129],const char *text) {
    if (!text || !*text) { memcpy(output,"-",2); return false; }
    static const char hex[]="0123456789abcdef"; size_t used=0;
    while (text[used] && used<64) {
        int bytes=0; GetCodepointNext(text+used,&bytes); if (bytes<1) bytes=1;
        if ((size_t)bytes>64-used) break;
        for (int i=0;i<bytes;i++) { unsigned char ch=(unsigned char)text[used]; output[2*used]=hex[ch>>4]; output[2*used+1]=hex[ch&15]; used++; }
    }
    output[2*used]=0; return text[used]!=0;
}
static const char *review_workspace_role(int role) {
    static const char *const roles[]={"rows","columns","values","filters"}; return (unsigned)role<4 ? roles[role] : "context";
}
static DxReviewDataIdentity review_workspace_scope(App *a,bool fields) {
    DxReviewDataIdentity data=review_scope(a); const SheetObject *object=selected_object(a),*source=objects_source(&active_sheet(a)->objects,object);
    data.has_cell=false; data.row=data.column=0; data.field[0]=0;
    data.source_id=source ? source->id : 0; data.object_id=fields ? data.source_id : object ? object->id : 0;
    data.schema_generation=fields ? a->field_schema_generation : object ? object_source_revision(a,object) : a->analysis_revision;
    data.query_generation=fields ? source ? source->revision : a->analysis_revision : object ? object->revision : a->analysis_revision;
    snprintf(data.kind,sizeof(data.kind),fields ? "source-fields" : "pivot-authoring");
    if (fields) {
        bool stale=!source && a->page_stale;
        if (source) {
            const ObjectView *cached=&a->objects[(size_t)(source-active_sheet(a)->objects.items)];
            stale=cached->dirty || cached->failed || cached->waiting_count || cached->revision!=source->revision;
            if (stale) data.query_generation=cached->revision;
        } else if (stale) data.query_generation=0; /* An old primary Page has no pinned analysis revision. */
        if (stale) snprintf(data.kind,sizeof(data.kind),"source-fields-stale");
    }
    return data;
}
static void review_workspace_geometry(App *a,const DxReviewWidget *widget,Rectangle visible) {
    if (!review_armed(a) || a->modal!=MODAL_NONE || a->file_menu || !widget || visible.width<=0 || visible.height<=0) return;
    static const struct { const char *id,*part; } targets[]={
        {"pane.fields.search","fields_search"},{"analysis.context.browse","context_browse"},
        {"analysis.shelf.rows.add","rows_add"},{"analysis.shelf.columns.add","columns_add"},
        {"analysis.marks.representation","representation"},{"analysis.shelf.values.add","values_add"},
        {"analysis.shelf.filters.add","filters_add"},{"analysis.marks.target.color","color"},
    };
    const char *part=NULL; bool dynamic=false;
    for (size_t i=0;i<sizeof(targets)/sizeof(*targets);i++) if (!strcmp(widget->id,targets[i].id)) { part=targets[i].part; break; }
    if (!part) {
        if (strstr(widget->id,"pane.fields.field.")==widget->id && strstr(widget->id,".more")) part="field_more";
        else if (strstr(widget->id,"analysis.shelf.rows.token.")==widget->id && !strstr(widget->id,".remove")) part="rows_token";
        else if (strstr(widget->id,"analysis.shelf.columns.token.")==widget->id && !strstr(widget->id,".remove")) part="columns_token";
        else if (strstr(widget->id,"analysis.shelf.values.token.")==widget->id && strstr(widget->id,".remove")) part="values_remove";
        dynamic=part!=NULL;
    }
    if (!part) return;
    char key[64],state[384]; snprintf(key,sizeof(key)," workspace_%s_rect=",part);
    if (strstr(a->review->frame.state_text,key)) return; /* One actual visible representative per family. */
    snprintf(state,sizeof(state),"%s%.3f,%.3f,%.3f,%.3f",key,visible.x,visible.y,visible.width,visible.height);
    review_author_capture_state(a,state);
    if (dynamic) { snprintf(state,sizeof(state)," workspace_%s_id=%s",part,widget->id); review_author_capture_state(a,state); }
    if (!strcmp(part,"values_remove")) { snprintf(state,sizeof(state)," workspace_values_remove_enabled=%d",widget->enabled); review_author_capture_state(a,state); }
}
static DxReviewWidget *review_workspace_widget(App *a,const char *id,const char *parent,const char *action,const char *role,const char *label,Rectangle bounds,Rectangle clip,bool enabled,bool checked,bool focused,const char *field,int pivot_role,int index,int layer,const char *file,int line) {
    if (!review_armed(a)) return NULL;
    if (a->ui.clipping) clip=GetCollisionRec(clip,a->ui.clip);
    Rectangle visible=GetCollisionRec(GetCollisionRec(bounds,clip),box(0,0,GetScreenWidth(),GetScreenHeight()));
    if (visible.width<=0 || visible.height<=0) return NULL;
    bool fields=!strncmp(id,"pane.fields",11); DxReviewWidget widget;
    dx_review_widget_init(&widget,id,action,fields ? "workspace/left/fields" : "workspace/analysis/authoring",role,label);
    snprintf(widget.parent_id,sizeof(widget.parent_id),"%s",parent ? parent : ""); widget.bounds=review_rect(bounds); widget.clip=review_rect(clip); widget.layer=layer;
    widget.container=!strcmp(role,"group") || !strcmp(role,"listbox") || !strcmp(role,"container"); widget.expanded=widget.container;
    widget.enabled=enabled; widget.checked=checked; widget.focused=focused; widget.input_eligible=enabled && !widget.container && strcmp(role,"text") && strcmp(role,"region");
    widget.data=review_workspace_scope(a,fields);
    if (field && review_author_field(widget.data.field,field)) {
        /* The complete widget ID and captured source/column occurrence remain
           intact. Only this optional human-readable descriptor is bounded. */
        widget.truncated=true; bool stale=strstr(widget.data.kind,"-stale")!=NULL;
        snprintf(widget.data.kind,sizeof(widget.data.kind),fields ? stale ? "source-fields-stale-truncated" : "source-fields-truncated" : "pivot-authoring-truncated");
    }
    if (pivot_role>=0) widget.data.column=(uint64_t)pivot_role; if (index>=0) widget.data.row=(uint64_t)index;
    if (!strcmp(role,"text") || !strcmp(role,"combobox")) dx_review_value_set(&widget.value,"VARCHAR",false,label ? label : "");
    if (!strcmp(id,"pane.fields.search")) dx_review_value_set(&widget.value,"VARCHAR",false,a->pivot_search.text ? a->pivot_search.text : "");
    snprintf(widget.source_file,sizeof(widget.source_file),"%s",file); widget.source_line=(uint32_t)line; widget.truncated|=widget.value.truncated;
    DxReviewStatus status=dx_review_add(a->review->registry,&widget);
    if (status!=DX_REVIEW_OK) { fprintf(stderr,"Review workspace registration %s: %s\n",id,dx_review_status_text(status)); return NULL; }
    review_workspace_geometry(a,&widget,visible);
    a->review->registry->truncated|=widget.truncated; return (DxReviewWidget *)dx_review_find(a->review->registry,id);
}
static void review_workspace_control(App *a,const char *id,const char *action,const char *role,const char *label,Rectangle bounds,Rectangle clip,bool enabled,bool checked,bool focused,const char *field,int pivot_role,int index,const char *file,int line) {
    if (!review_armed(a)) return;
    char parent[128]; int layer=!strncmp(id,"pane.fields",11) || !strncmp(id,"analysis.marks",14) || !strncmp(id,"analysis.shelf.values",21) || !strncmp(id,"analysis.shelf.filters",22) ? 52 : 22;
    if (!strcmp(id,"pane.fields") || !strcmp(id,"analysis.marks")) snprintf(parent,sizeof(parent),"pane.workbook");
    else if (!strcmp(id,"analysis.context")) parent[0]=0;
    else if (!strncmp(id,"pane.fields",11)) snprintf(parent,sizeof(parent),"pane.fields");
    else if (!strncmp(id,"analysis.shelf.",15)) {
        snprintf(parent,sizeof(parent),"%s",id); char *end=strchr(parent+15,'.');
        if (end) *end=0; else snprintf(parent,sizeof(parent),"%s",pivot_role>=PIVOT_VALUES ? "analysis.marks" : "analysis.context");
    } else snprintf(parent,sizeof(parent),"%s",!strncmp(id,"analysis.marks",14) ? "analysis.marks" : "analysis.context");
    DxReviewWidget *widget=review_workspace_widget(a,id,parent,action,role,label,bounds,clip,enabled,checked,focused,field,pivot_role,index,layer,file,line);
    if (widget && !strncmp(id,"analysis.marks.target.",22) && checked && (unsigned)index<DX_MARK_ROLES) {
        dx_review_value_set(&widget->value,"VARCHAR",false,a->mark_labels[index]); widget->truncated|=widget->value.truncated; a->review->registry->truncated|=widget->truncated;
    }
}
static void review_workspace_field(App *a,const Column *field,size_t index,Rectangle bounds,Rectangle clip,bool enabled,bool focused,bool more,const char *file,int line) {
    if (!review_armed(a) || !field || !field->name) return;
    char id[128],parent[128]; snprintf(parent,sizeof(parent),"pane.fields.field.f%016llx",(unsigned long long)review_field_hash(field->name));
    snprintf(id,sizeof(id),"%s%s",parent,more ? ".more" : "");
    DxReviewWidget *widget=review_workspace_widget(a,id,more ? parent : "pane.fields.list",more ? "field.open_actions" : selected_pivot(a) ? "field.select_drag" : "field.reveal","button",more ? "Field actions" : field->name,bounds,clip,enabled,!more && focused,focused,field->name,-1,(int)index,more ? 54 : 53,file,line);
    if (widget) { bool stale=strstr(widget->data.kind,"-stale")!=NULL,cut=strstr(widget->data.kind,"-truncated")!=NULL; snprintf(widget->data.kind,sizeof(widget->data.kind),cut ? stale ? "available-field-stale-truncated" : "available-field-truncated" : stale ? "available-field-stale" : "available-field"); dx_review_value_set(&widget->value,"VARCHAR",false,field->name); widget->truncated|=widget->value.truncated; a->review->registry->truncated|=widget->truncated; }
}
static bool review_workspace_token_id(char output[128],int role,const PivotField *field,bool remove) {
    static const char *const aggregates[]={"sum","count","distinct","average","minimum","maximum"},*const grains[]={"exact","year","quarter","month"};
    if ((unsigned)role>=4 || !field || !field->name || (unsigned)field->aggregate>=6 || (unsigned)field->grain>=4) return false;
    snprintf(output,128,"analysis.shelf.%s.token.f%016llx.%s.%s%s",review_workspace_role(role),(unsigned long long)review_field_hash(field->name),aggregates[field->aggregate],grains[field->grain],remove ? ".remove" : ""); return true;
}
static void review_workspace_token(App *a,int role,size_t index,const PivotField *field,const char *label,Rectangle bounds,Rectangle clip,bool enabled,bool focused,bool remove,const char *file,int line) {
    if (!review_armed(a)) return;
    char id[128],parent[128]; if (!review_workspace_token_id(id,role,field,remove)) return;
    if (remove) review_workspace_token_id(parent,role,field,false); else snprintf(parent,sizeof(parent),"analysis.shelf.%s",review_workspace_role(role));
    DxReviewWidget *widget=review_workspace_widget(a,id,parent,remove ? "pivot.remove_field" : "pivot.inspect_field","button",label,bounds,clip,enabled,false,focused,field->name,role,(int)index,(role>=PIVOT_VALUES ? 53 : 23)+(remove ? 1 : 0),file,line);
    if (widget) { bool cut=strstr(widget->data.kind,"-truncated")!=NULL; snprintf(widget->data.kind,sizeof(widget->data.kind),cut ? "pivot-token-truncated" : "pivot-token"); dx_review_value_set(&widget->value,"VARCHAR",false,label); widget->truncated|=widget->value.truncated; a->review->registry->truncated|=widget->truncated; }
}
static void review_workspace_scroll(App *a,const char *part,Rectangle track,Rectangle thumb,Rectangle clip,bool enabled,bool dragging,const char *file,int line) {
    if (!review_armed(a)) return;
    const char *parent=!strcmp(part,"fields") ? "pane.fields" : "analysis.marks"; char id[128],thumb_id[128]; snprintf(id,sizeof(id),"%s.scrollbar",parent); snprintf(thumb_id,sizeof(thumb_id),"%s.thumb",id);
    review_workspace_widget(a,id,parent,"workspace.scroll","scrollbar","Scroll authoring controls",track,clip,enabled,false,false,NULL,-1,-1,55,file,line);
    review_workspace_widget(a,thumb_id,id,"workspace.scroll","scrollbar","Scroll authoring controls",thumb,GetCollisionRec(track,clip),enabled,false,dragging,NULL,-1,-1,56,file,line);
}
static DxReviewDataIdentity review_feedback_origin(App *a,const char *producer,const SheetObject *owner) {
    DxReviewDataIdentity data={0}; if (!review_armed(a)) return data;
    data.present=true; review_document_id(a,data.document_id); snprintf(data.kind,sizeof(data.kind),"document-feedback");
    if (!strcmp(producer,"task") || !strcmp(producer,"toast")) { snprintf(data.kind,sizeof(data.kind),!strcmp(producer,"toast") ? "document-notification" : "document-task"); return data; }
    data.sheet_id=a->book.active+1;
    if (!strcmp(producer,"interface")) { snprintf(data.kind,sizeof(data.kind),"canvas-presentation"); return data; }
    if (!strcmp(producer,"pipeline_sql")) {
        snprintf(data.kind,sizeof(data.kind),a->inspector_sql_valid ? "transformations-sql" : "transformations-sql-stale");
        data.sheet_id=a->inspector_sql_sheet+1; data.source_id=data.object_id=a->inspector_sql_id; data.query_generation=a->inspector_sql_revision; return data;
    }
    if (!strcmp(producer,"preview") || (!strcmp(producer,"source_footer") && a->step_preview.active) || (!strcmp(producer,"metrics") && a->step_preview.active)) {
        const StepPreview *preview=&a->step_preview; data.source_id=preview->source_id; data.schema_generation=preview->source_revision; data.query_generation=preview->request; data.row=preview->step;
        snprintf(data.kind,sizeof(data.kind),!strcmp(producer,"metrics") ? "preview-statistics" : !strcmp(producer,"source_footer") ? "preview-source-status" : "step-preview");
        if (!strcmp(producer,"metrics")) { data.row=preview->stats_bounds.r0; data.column=preview->stats_bounds.c0; } return data;
    }
    if (!strcmp(producer,"metrics")) {
        const StatsKey *key=&a->statistics_key; data.sheet_id=key->sheet+1; data.query_generation=key->source_revision; data.row=key->r0; data.column=key->c0;
        snprintf(data.kind,sizeof(data.kind),a->statistics_frame ? "statistics-frame" : a->statistics_object==-2 ? "statistics-mixed" : "selection-statistics");
        ObjectStore *store=&active_sheet(a)->objects;
        if (a->statistics_object>=0 && (size_t)a->statistics_object<store->count) {
            const SheetObject *object=&store->items[a->statistics_object]; data.object_id=object->id; data.source_id=object->kind==OBJECT_TABLE ? object->id : object->source_id;
            if (a->statistics_frame) { data.sheet_id=a->book.active+1; data.query_generation=object->revision; data.schema_generation=object_source_revision(a,object); data.row=data.column=0; }
        }
        if (a->diagnostics) { data=(DxReviewDataIdentity){.present=true}; review_document_id(a,data.document_id); snprintf(data.kind,sizeof(data.kind),"document-diagnostics"); } return data;
    }
    if (!strcmp(producer,"readout")) {
        const ActiveReadout *key=&a->active_readout; data.sheet_id=key->sheet+1; data.has_cell=true; data.row=key->row; data.column=key->column;
        snprintf(data.kind,sizeof(data.kind),key->value.valid ? "active-value" : "active-value-loading");
        if (key->preview) { data.source_id=key->source; data.schema_generation=key->revision; data.query_generation=a->step_preview.request; return data; }
        if (key->manual) { data.query_generation=key->manual; snprintf(data.kind,sizeof(data.kind),"manual-readout"); return data; }
        data.query_generation=key->revision;
        if (key->kind) {
            data.object_id=key->source; data.schema_generation=key->revision; data.query_generation=key->object_revision; bool found=false;
            ObjectStore *store=&active_sheet(a)->objects;
            for (size_t i=0;i<store->count;i++) if (store->items[i].id==key->source) {
                const SheetObject *object=&store->items[i]; const ObjectView *view=&a->objects[i];
                found=object->revision==key->object_revision && object_source_revision(a,object)==key->revision && object->row==key->origin_row && object->col==key->origin_column;
                if (found) data.source_id=object->kind==OBJECT_TABLE ? object->id : object->source_id;
                if (view->dirty || view->failed || view->revision!=key->object_revision || view->source_revision!=key->revision) found=false;
                break;
            }
            if (!found && key->value.valid) snprintf(data.kind,sizeof(data.kind),"active-value-stale");
        } else if (a->page_stale && key->value.valid) snprintf(data.kind,sizeof(data.kind),"active-value-stale");
        return data;
    }
    data.source_id=owner ? owner->kind==OBJECT_TABLE ? owner->id : owner->source_id : 0; data.object_id=owner ? owner->id : 0;
    data.query_generation=owner ? owner->revision : a->analysis_revision;
    if (!strcmp(producer,"filter")) {
        snprintf(data.kind,sizeof(data.kind),"filter-domain"); data.schema_generation=object_source_revision(a,owner); data.column=a->field;
        const Page *fields=filter_fields(a); if (a->field<fields->column_count && review_author_field(data.field,fields->columns[a->field].name)) snprintf(data.kind,sizeof(data.kind),"filter-domain-truncated"); return data;
    }
    if (!strcmp(producer,"source_footer")) {
        snprintf(data.kind,sizeof(data.kind),"source-status");
        if (owner && owner->kind==OBJECT_TABLE) { const ObjectView *view=&a->objects[(size_t)(owner-active_sheet(a)->objects.items)]; data.query_generation=view->revision; if (view->dirty || view->failed || view->waiting_count || view->revision!=owner->revision) snprintf(data.kind,sizeof(data.kind),"source-status-stale"); }
        else if (a->page_stale) { data.query_generation=0; snprintf(data.kind,sizeof(data.kind),"source-status-stale"); }
    } else snprintf(data.kind,sizeof(data.kind),"transformations-source");
    return data;
}
static void review_feedback_metadata(App *a,const DxReviewWidget *widget) {
    if (!review_armed(a) || !widget) return;
    char state[1024]; const char *id=widget->id;
    static const struct { const char *id,*key; } geometry[]={
        {"pane.transformations.inspect","pipeline_inspect"},{"pane.transformations.sql","pipeline_sql"},
        {"pane.transformations.copy","pipeline_copy"},{"pane.transformations.back","pipeline_back"},
        {"pane.transformations.preview","pipeline_preview"},{"preview.status","preview_status"},
        {"preview.return","preview_return"},{"filter.values.status","filter_status"},
        {"filter.values.scrollbar.thumb","filter_thumb"},{"status.source_count","source_count"},
        {"cell.readout.value","readout_value"},{"toast.content","toast"}
    };
    Rectangle shown=GetCollisionRec((Rectangle){widget->bounds.x,widget->bounds.y,widget->bounds.width,widget->bounds.height},(Rectangle){widget->clip.x,widget->clip.y,widget->clip.width,widget->clip.height});
    if (!strcmp(id,"pane.transformations.body") && !a->pipeline_sql) {
        const UIInspectorLayout *layout=&a->pipeline_layout;
        snprintf(state,sizeof(state)," feedback_pipeline_scroll=%.6f,%.6f,%.6f,%.6f,%zu,%d feedback_pipeline_header_box=%.3f,%.3f,%.3f,%.3f feedback_pipeline_viewport_box=%.3f,%.3f,%.3f,%.3f feedback_pipeline_list_box=%.3f,%.3f,%.3f,%.3f feedback_pipeline_edit_box=%.3f,%.3f,%.3f,%.3f feedback_pipeline_footer_box=%.3f,%.3f,%.3f,%.3f feedback_pipeline_outer_track=%.3f,%.3f,%.3f,%.3f feedback_pipeline_inner_track=%.3f,%.3f,%.3f,%.3f",
            layout->outer_offset,layout->outer_max,layout->inner_offset,layout->inner_max,a->pipeline_row_count,a->pipeline_focus_index,
            layout->header.x,layout->header.y,layout->header.width,layout->header.height,
            layout->body_clip.x,layout->body_clip.y,layout->body_clip.width,layout->body_clip.height,
            layout->list_frame.x,layout->list_frame.y,layout->list_frame.width,layout->list_frame.height,
            layout->edit.x,layout->edit.y,layout->edit.width,layout->edit.height,
            layout->footer.x,layout->footer.y,layout->footer.width,layout->footer.height,
            layout->outer_scrollbar.x,layout->outer_scrollbar.y,layout->outer_scrollbar.width,layout->outer_scrollbar.height,
            layout->list_scrollbar.x,layout->list_scrollbar.y,layout->list_scrollbar.width,layout->list_scrollbar.height);
        review_author_capture_state(a,state);
    }
    if (!strncmp(id,"pane.transformations.step.",26) && !strstr(a->review->frame.state_text," feedback_pipeline_step_rect=")) {
        snprintf(state,sizeof(state)," feedback_pipeline_step_rect=%.3f,%.3f,%.3f,%.3f feedback_pipeline_step_id=%s",shown.x,shown.y,shown.width,shown.height,id); review_author_capture_state(a,state);
    }
    for (size_t i=0;i<sizeof(geometry)/sizeof(*geometry);i++) if (!strcmp(id,geometry[i].id)) {
        snprintf(state,sizeof(state)," feedback_%s_rect=%.3f,%.3f,%.3f,%.3f",geometry[i].key,shown.x,shown.y,shown.width,shown.height); review_author_capture_state(a,state); break;
    }
    if (!strcmp(id,"preview.status") || !strcmp(id,"filter.values.status")) {
        char hex[129]; bool cut=review_author_hex64(hex,widget->value.present ? widget->value.exact_text : NULL);
        const char *key=!strcmp(id,"preview.status") ? "preview_status" : "filter_status";
        snprintf(state,sizeof(state)," feedback_%s_hex=%s feedback_%s_truncated=%d",key,hex,key,cut || widget->value.truncated); review_author_capture_state(a,state);
    }
#ifdef DUXCEL_BROWSER_HOST
    if (!strcmp(id,"preview.title")) {
        char key[129],title[129]; bool key_cut=review_author_hex64(key,a->step_preview.source.path);
        bool title_cut=review_author_hex64(title,widget->value.present ? widget->value.exact_text : NULL);
        snprintf(state,sizeof(state)," label_preview_key_hex=%s label_preview_title_hex=%s label_preview_cut=%d,%d",key,title,key_cut,title_cut || widget->value.truncated); review_author_capture_state(a,state);
    }
#endif
    if (!strcmp(id,"pane.transformations.sql") && a->modal==MODAL_NONE) {
        snprintf(state,sizeof(state)," feedback_sql_cache=%llu,%llu,%zu,%llu,%llu feedback_sql_layout=%d,%zu,%d",(unsigned long long)a->inspector_sql_id,(unsigned long long)a->inspector_sql_revision,a->inspector_sql_sheet,(unsigned long long)a->inspector_sql_generation,(unsigned long long)a->inspector_sql_steps_key,a->inspector_sql_scroll,a->inspector_sql_lines.count,a->inspector_sql_lines.limited); review_author_capture_state(a,state);
    } else if (!strcmp(id,"status.metrics") && a->modal==MODAL_NONE && !a->file_menu) {
        const StatsKey *key=&a->statistics_key;
        snprintf(state,sizeof(state)," feedback_stats_key=%llu,%llu,%zu,%zu,%llu,%llu,%llu,%llu,%zu,%zu,%d,%d feedback_stats_state=%d,%d,%d,%llu",(unsigned long long)key->r0,(unsigned long long)key->r1,key->c0,key->c1,(unsigned long long)key->source_revision,(unsigned long long)key->cell_revision,(unsigned long long)key->objects,(unsigned long long)key->total,key->sheet,key->columns,key->count_pending,key->active,a->statistics_state,a->statistics_object,a->statistics_frame,(unsigned long long)a->statistics_request); review_author_capture_state(a,state);
    } else if (!strcmp(id,"filter.values.status")) {
        char context[129],pending[129]; bool context_cut=review_author_hex64(context,a->values_context),pending_cut=review_author_hex64(pending,a->value_pending_key);
        snprintf(state,sizeof(state)," feedback_filter=%llu,%d,%lld,%d,%d feedback_filter_context_hex=%s feedback_filter_pending_hex=%s feedback_filter_truncated=%d,%d",(unsigned long long)a->values_search_revision,a->date_level,(long long)a->values_total,a->values_loading,a->values_failed,context,pending,context_cut,pending_cut); review_author_capture_state(a,state);
    } else if (!strcmp(id,"toast.content")) {
        snprintf(state,sizeof(state)," feedback_toast=%llu,%.3f,%zu,%d",(unsigned long long)a->message_revision,a->message_since,a->toast_lines.count,a->toast_lines.limited); review_author_capture_state(a,state);
    }
}
static void review_feedback_target(App *a,DxReviewDataIdentity origin,const char *id,const char *parent,const char *action,const char *role,const char *label,Rectangle bounds,Rectangle clip,bool enabled,bool checked,bool focused,const char *type,bool is_null,const char *text,const char *file,int line) {
    if (!review_armed(a)) return;
    bool filter=!strncmp(id,"filter.",7);
    if (filter ? a->modal!=MODAL_FILTER : a->modal!=MODAL_NONE || a->file_menu) return;
    if (a->ui.clipping) clip=GetCollisionRec(clip,a->ui.clip);
    float offset=filter ? a->popup_presence.offset*a->ui.scale : 0;
    bounds.y+=offset; clip.y+=offset;
    Rectangle visible=GetCollisionRec(GetCollisionRec(bounds,clip),box(0,0,GetScreenWidth(),GetScreenHeight())); if (visible.width<=0 || visible.height<=0) return;
    DxReviewWidget widget; dx_review_widget_init(&widget,id,action,"workspace/feedback",role,label); widget.data=origin;
    widget.truncated|=strstr(origin.kind,"-truncated")!=NULL;
    snprintf(widget.parent_id,sizeof(widget.parent_id),"%s",parent ? parent : ""); widget.bounds=review_rect(bounds); widget.clip=review_rect(clip); widget.transform.ty=offset;
    int layer=filter ? 103 : !strncmp(id,"toast",5) ? 161 : !strncmp(id,"status",6) ? 62 : !strncmp(id,"cell.readout.",13) ? 18 : !strncmp(id,"preview",7) ? 12 : 44;
    widget.container=!strcmp(role,"group") || !strcmp(role,"listbox") || !strcmp(role,"container"); widget.expanded=widget.container; widget.layer=layer-(widget.container ? 1 : 0);
    if (filter && widget.container) widget.layer=100; /* Preserve actual value children at101 above their group. */
    widget.enabled=enabled; widget.checked=checked; widget.focused=focused; widget.input_eligible=enabled && !widget.container && strcmp(role,"text") && strcmp(role,"region");
    if (type) dx_review_value_set(&widget.value,type,is_null,text);
    if (type && !strcmp(id,"cell.readout.value")) widget.value.truncated|=a->active_readout.value.truncated;
    widget.truncated|=widget.value.truncated;
    snprintf(widget.source_file,sizeof(widget.source_file),"%s",file); widget.source_line=(uint32_t)line;
    DxReviewStatus result=dx_review_add(a->review->registry,&widget);
    if (result!=DX_REVIEW_OK) { fprintf(stderr,"Review feedback registration %s: %s\n",id,dx_review_status_text(result)); return; }
    a->review->registry->truncated|=widget.truncated; review_feedback_metadata(a,&widget);
}
static void review_feedback_scroll(App *a,DxReviewDataIdentity origin,const char *id,const char *parent,Rectangle track,Rectangle thumb,Rectangle clip,bool enabled,bool dragging,const char *file,int line) {
    if (!review_armed(a)) return;
    review_feedback_target(a,origin,id,parent,"feedback.scroll","scrollbar","Scroll feedback content",track,clip,enabled,false,false,NULL,false,NULL,file,line);
    char child[128]; snprintf(child,sizeof(child),"%s.thumb",id); review_feedback_target(a,origin,child,id,"feedback.scroll","scrollbar","Scroll feedback content",thumb,GetCollisionRec(track,clip),enabled,false,dragging,NULL,false,NULL,file,line);
    DxReviewWidget *widget=(DxReviewWidget *)dx_review_find(a->review->registry,child); if (widget) widget->layer++;
}
static void review_feedback_step(App *a,const SheetObject *owner,const UIStep *step,size_t index,Rectangle bounds,Rectangle clip,bool enabled,bool checked,bool focused,const char *file,int line) {
    if (!review_armed(a) || a->modal!=MODAL_NONE || a->file_menu || !step || (unsigned)step->kind>UI_STEP_SORT) return;
    static const char *const kinds[]={"source","unique","filter","group","sort"}; char id[128];
    snprintf(id,sizeof(id),"pane.transformations.step.%s.f%016llx.n%zu",kinds[step->kind],(unsigned long long)review_field_hash(step->column),step->index);
    DxReviewDataIdentity origin=review_feedback_origin(a,"pipeline",owner); origin.row=index; origin.column=(uint64_t)step->kind;
    bool truncated=step->column && review_author_field(origin.field,step->column);
    review_feedback_target(a,origin,id,"pane.transformations.steps","pipeline.select_step","button",step->title,bounds,clip,enabled,checked,focused,"VARCHAR",false,step->detail,file,line);
    if (truncated) { DxReviewWidget *widget=(DxReviewWidget *)dx_review_find(a->review->registry,id); if (widget) { widget->truncated=true; snprintf(widget->data.kind,sizeof(widget->data.kind),"transformations-step-truncated"); } a->review->registry->truncated=true; }
#ifdef DUXCEL_BROWSER_HOST
    const DxReviewWidget *widget=dx_review_find(a->review->registry,id);
    if (step->kind==UI_STEP_SOURCE && widget) {
        const Analysis *source=owner ? &owner->source : &a->analysis;
        char key[129],title[129],detail[129],sheet[129],object[129],state[1024];
        bool key_cut=review_author_hex64(key,source->path),title_cut=review_author_hex64(title,widget->label);
        bool detail_cut=review_author_hex64(detail,widget->value.present ? widget->value.exact_text : NULL);
        bool sheet_cut=review_author_hex64(sheet,active_sheet(a)->name),object_cut=review_author_hex64(object,owner ? owner->name : NULL);
        snprintf(state,sizeof(state)," label_source_owner=%llu label_source_key_hex=%s label_source_title_hex=%s label_source_detail_hex=%s label_source_cut=%d,%d,%d label_sheet_hex=%s label_object_hex=%s label_names_cut=%d,%d",(unsigned long long)(owner ? owner->id : 0),key,title,detail,key_cut,title_cut || widget->truncated,detail_cut || widget->value.truncated,sheet,object,sheet_cut,object_cut);
        review_author_capture_state(a,state);
    }
#endif
}
static void review_feedback_preview(App *a,const UIPreviewReviewFrame *frame) {
    if (!review_armed(a) || !frame || !frame->armed || a->modal!=MODAL_NONE || a->file_menu) return;
    DxReviewDataIdentity origin=review_feedback_origin(a,"preview",NULL); char metadata[256];
    snprintf(metadata,sizeof(metadata)," feedback_preview=%llu,%llu,%llu,%zu,%d,%d feedback_preview_records=%zu,%d",(unsigned long long)a->step_preview.source_id,(unsigned long long)a->step_preview.source_revision,(unsigned long long)a->step_preview.request,a->step_preview.step,a->step_preview.loading,a->step_preview.error!=NULL,frame->count,frame->overflow); review_author_capture_state(a,metadata);
    const Grid *grid=&a->step_preview.grid;
    snprintf(metadata,sizeof(metadata)," feedback_preview_selection=%llu,%zu,%llu,%zu,%llu,%zu",
        (unsigned long long)grid->row,grid->col,(unsigned long long)grid->anchor_row,grid->anchor_col,
        (unsigned long long)grid->end_row,grid->end_col); review_author_capture_state(a,metadata);
    snprintf(metadata,sizeof(metadata)," feedback_preview_body=%.3f,%.3f,%.3f,%.3f",grid->body.x,grid->body.y,grid->body.width,grid->body.height); review_author_capture_state(a,metadata);
    if (frame->overflow) a->review->registry->truncated=true;
    for (size_t i=0;i<frame->count;i++) {
        const UIPreviewReviewTarget *target=&frame->targets[i]; char id[128]; snprintf(id,sizeof(id),"preview.%s",target->key);
        const char *parent=!strcmp(target->key,"panel") ? "" : !strcmp(target->key,"banner") || !strcmp(target->key,"status") ? "preview.panel" : "preview.banner";
        const char *label=!strcmp(target->key,"status") ? "Preview status" : !strcmp(target->key,"title") ? "Historical step title" : target->text;
        review_feedback_target(a,origin,id,parent,!strcmp(target->key,"return") ? "preview.return" : "preview.inspect",target->role,label,target->bounds,target->clip,target->enabled,false,false,!strcmp(target->role,"text") ? "VARCHAR" : NULL,false,target->text,target->source_file,(int)target->source_line);
        DxReviewWidget *widget=(DxReviewWidget *)dx_review_find(a->review->registry,id); if (widget) { widget->truncated|=target->truncated; widget->value.truncated|=target->truncated; a->review->registry->truncated|=widget->truncated; }
    }
}
static void review_query_status(App *a,const char *text,Rectangle bounds,const char *file,int line) {
    if (!review_armed(a) || a->modal!=MODAL_SOURCE_QUERY) return;
    Rectangle clip=a->ui.clipping ? a->ui.clip : box(0,0,GetScreenWidth(),GetScreenHeight());
    DxReviewWidget *widget=review_author_widget(a,"author.source_query.status","author.source_query","source_query","source.query.inspect_feedback","text","Query status",bounds,clip,131,true,false,false,false,NULL,file,line);
    if (!widget) return;
    dx_review_value_set(&widget->value,"VARCHAR",false,text ? text : ""); widget->truncated|=widget->value.truncated; a->review->registry->truncated|=widget->truncated;
    Rectangle shown=GetCollisionRec((Rectangle){widget->bounds.x,widget->bounds.y,widget->bounds.width,widget->bounds.height},(Rectangle){widget->clip.x,widget->clip.y,widget->clip.width,widget->clip.height});
    char hex[129],state[384]; bool truncated=review_author_hex64(hex,text);
    snprintf(state,sizeof(state)," query_status_rect=%.3f,%.3f,%.3f,%.3f query_status_loading=%d query_status_hex=%s query_status_truncated=%d query_status_value_truncated=%d",shown.x,shown.y,shown.width,shown.height,a->query_loading,hex,truncated,widget->value.truncated);
    review_author_capture_state(a,state);
}
static void review_query_error(App *a,Rectangle bounds,const char *file,int line) {
    if (!review_armed(a) || a->modal!=MODAL_SOURCE_QUERY || !a->query_error) return;
    Rectangle clip=a->ui.clipping ? a->ui.clip : box(0,0,GetScreenWidth(),GetScreenHeight());
    DxReviewWidget *widget=review_author_widget(a,"author.source_query.validation_error","author.source_query","source_query","source.query.inspect_feedback","region","Query validation error",bounds,clip,131,true,false,false,false,NULL,file,line);
    if (!widget) return;
    dx_review_value_set(&widget->value,"VARCHAR",false,a->query_error); widget->truncated|=widget->value.truncated; a->review->registry->truncated|=widget->truncated;
    Rectangle shown=GetCollisionRec((Rectangle){widget->bounds.x,widget->bounds.y,widget->bounds.width,widget->bounds.height},(Rectangle){widget->clip.x,widget->clip.y,widget->clip.width,widget->clip.height});
    char hex[129],state[512]; bool truncated=review_author_hex64(hex,a->query_error);
    snprintf(state,sizeof(state)," query_error_rect=%.3f,%.3f,%.3f,%.3f query_error_bytes=%zu query_error_lines=%zu query_error_scroll=%d query_error_limited=%d query_error_hex=%s query_error_truncated=%d query_error_value_truncated=%d",shown.x,shown.y,shown.width,shown.height,strlen(a->query_error),a->query_errors.count,a->query_error_scroll,a->query_errors.limited,hex,truncated,widget->value.truncated);
    review_author_capture_state(a,state);
}
static void review_cell_editor(App *a,Rectangle bounds,Rectangle clip,const char *file,int line) {
    if (!review_armed(a) || a->modal!=MODAL_CELL || bounds.width<=0 || bounds.height<=0) return;
    Rectangle shown=GetCollisionRec(bounds,clip); if (shown.width<=0 || shown.height<=0) return;
    const TextInput *input=&a->input; DxReviewWidget widget;
    dx_review_widget_init(&widget,"author.cell_editor.input","cell.edit_draft","workspace/canvas/cell_editor","textbox","Cell editor");
    snprintf(widget.parent_id,sizeof(widget.parent_id),"worksheet.cells"); widget.bounds=review_rect(bounds); widget.clip=review_rect(clip); widget.layer=130;
    widget.enabled=widget.input_eligible=widget.focused=true; widget.data=review_scope(a);
    snprintf(widget.data.kind,sizeof(widget.data.kind),"manual-cell-draft"); widget.data.object_id=widget.data.source_id=widget.data.schema_generation=0;
    widget.data.query_generation=active_sheet(a)->cells.revision; widget.data.has_cell=true; widget.data.row=a->grid.row; widget.data.column=a->grid.col; widget.data.field[0]=0;
    dx_review_value_set(&widget.value,"VARCHAR",false,input->text ? input->text : ""); widget.truncated=widget.value.truncated;
    snprintf(widget.source_file,sizeof(widget.source_file),"%s",file); widget.source_line=(uint32_t)line;
    DxReviewStatus result=dx_review_add(a->review->registry,&widget);
    if (result!=DX_REVIEW_OK) { fprintf(stderr,"Review inline editor registration: %s\n",dx_review_status_text(result)); return; }
    a->review->registry->truncated|=widget.truncated;
    size_t undo_bytes=0,redo_bytes=0;
    for (size_t i=0;i<input->undo_count && i<32;i++) undo_bytes+=input->undo[i].capacity;
    for (size_t i=0;i<input->redo_count && i<32;i++) redo_bytes+=input->redo[i].capacity;
    char hex[129],state[1024]; bool truncated=review_author_hex64(hex,input->text);
    snprintf(state,sizeof(state)," author_kind=cell_editor cell_editor_rect=%.3f,%.3f,%.3f,%.3f cell_editor_cursor=%zu cell_editor_anchor=%zu cell_editor_select_all=%d cell_editor_revision=%llu cell_editor_view_start=%zu cell_editor_bytes=%zu cell_editor_history=%zu,%zu cell_editor_history_bytes=%zu,%zu cell_editor_origin=%llu,0,0,0,%llu,%llu,%llu,1 cell_editor_text_hex=%s cell_editor_text_truncated=%d cell_editor_value_truncated=%d",shown.x,shown.y,shown.width,shown.height,input->cursor,input->anchor,input->select_all,(unsigned long long)input->revision,input->view_start,input->length,input->undo_count,input->redo_count,undo_bytes,redo_bytes,(unsigned long long)widget.data.sheet_id,(unsigned long long)widget.data.query_generation,(unsigned long long)widget.data.row,(unsigned long long)widget.data.column,hex,truncated,widget.value.truncated);
    review_author_capture_state(a,state);
}
static void review_error_content(App *a,Rectangle bounds,size_t painted_lines,bool clipped,const char *file,int line) {
    if (!review_armed(a) || a->modal!=MODAL_ERROR) return;
    Rectangle clip=a->ui.clipping ? a->ui.clip : box(0,0,GetScreenWidth(),GetScreenHeight());
    DxReviewWidget *widget=review_author_widget(a,"author.error.content","author.error","error","error.inspect","region","Error message",bounds,clip,131,true,false,false,false,NULL,file,line);
    if (!widget) return;
    dx_review_value_set(&widget->value,"VARCHAR",false,a->message ? a->message : ""); widget->truncated|=widget->value.truncated; a->review->registry->truncated|=widget->truncated;
    Rectangle shown=GetCollisionRec(bounds,clip); char text[129],state[640]; bool truncated=review_author_hex64(text,a->message);
    snprintf(state,sizeof(state)," error_content_rect=%.3f,%.3f,%.3f,%.3f error_bytes=%zu error_lines=%zu error_clipped=%d error_message_since=%.3f error_request=%llu error_pending_kind=%d error_busy=%d error_io_busy=%d error_text_hex=%s error_text_truncated=%d error_value_truncated=%d",shown.x,shown.y,shown.width,shown.height,a->message ? strlen(a->message) : 0,painted_lines,clipped,a->message_since,(unsigned long long)a->request_id,(int)a->pending_kind,a->busy,a->io_busy,text,truncated,widget->value.truncated);
    review_author_capture_state(a,state);
}
static void review_sql_content(App *a,const char *sql,Rectangle bounds,Rectangle clip,int scroll,int lines,int visible,const char *file,int line) {
    if (!review_armed(a)) return;
    DxReviewWidget *widget=review_author_widget(a,"author.generated_sql.content","author.generated_sql","generated_sql","sql.inspect","region","Generated SQL",bounds,clip,131,true,false,false,false,NULL,file,line);
    if (!widget) return;
    dx_review_value_set(&widget->value,"VARCHAR",false,sql ? sql : ""); widget->truncated|=widget->value.truncated; a->review->registry->truncated|=widget->truncated;
    Rectangle shown=GetCollisionRec((Rectangle){widget->bounds.x,widget->bounds.y,widget->bounds.width,widget->bounds.height},(Rectangle){widget->clip.x,widget->clip.y,widget->clip.width,widget->clip.height});
    char text[129],state[640]; bool truncated=review_author_hex64(text,sql); const DxReviewDataIdentity *origin=&a->sql_origin;
    snprintf(state,sizeof(state)," sql_content_rect=%.3f,%.3f,%.3f,%.3f sql_bytes=%zu sql_scroll=%d sql_lines=%d sql_visible=%d sql_origin_valid=%d sql_origin_stale=%d sql_origin=%llu,%llu,%llu,%llu,%llu sql_text_hex=%s sql_text_truncated=%d sql_value_truncated=%d",shown.x,shown.y,shown.width,shown.height,sql ? strlen(sql) : 0,scroll,lines,visible,a->sql_origin_valid,a->sql_origin_stale,(unsigned long long)origin->sheet_id,(unsigned long long)origin->object_id,(unsigned long long)origin->source_id,(unsigned long long)origin->schema_generation,(unsigned long long)origin->query_generation,text,truncated,widget->value.truncated);
    review_author_capture_state(a,state);
}
static void review_inspector_value(App *a,Rectangle bounds,Rectangle clip,const char *file,int line) {
    if (!review_armed(a)) return;
    DxReviewWidget *widget=review_author_widget(a,"author.cell_inspector.value","author.cell_inspector","cell_inspector","cell.inspect_value","region","Cell value",bounds,clip,131,true,false,false,false,NULL,file,line);
    if (!widget) return;
    dx_review_value_set(&widget->value,a->inspector_type,a->inspector_null,a->inspector_text);
    widget->truncated|=widget->value.truncated; a->review->registry->truncated|=widget->truncated;
    Rectangle visible=GetCollisionRec((Rectangle){widget->bounds.x,widget->bounds.y,widget->bounds.width,widget->bounds.height},(Rectangle){widget->clip.x,widget->clip.y,widget->clip.width,widget->clip.height});
    char type[129],text[129],field[129],state[1024];
    bool type_truncated=review_author_hex64(type,a->inspector_type),text_truncated=review_author_hex64(text,a->inspector_text),field_truncated=review_author_hex64(field,a->inspector_origin.field);
    const DxReviewDataIdentity *origin=&a->inspector_origin;
    snprintf(state,sizeof(state)," inspector_value_rect=%.3f,%.3f,%.3f,%.3f inspector_type_hex=%s inspector_type_truncated=%d inspector_text_hex=%s inspector_text_truncated=%d inspector_origin=%d,%d,%llu,%llu,%llu,%llu,%llu,%llu,%llu inspector_field_hex=%s inspector_field_truncated=%d",visible.x,visible.y,visible.width,visible.height,type,type_truncated,text,text_truncated,a->inspector_origin_valid,a->inspector_origin_stale,(unsigned long long)origin->sheet_id,(unsigned long long)origin->object_id,(unsigned long long)origin->source_id,(unsigned long long)origin->schema_generation,(unsigned long long)origin->query_generation,(unsigned long long)origin->row,(unsigned long long)origin->column,field,field_truncated);
    review_author_capture_state(a,state);
}
static void review_statistics_row(App *a,int metric,const char *label,Rectangle bounds,Rectangle clip,bool checked,bool focused,const char *file,int line) {
    if (!review_armed(a) || metric<0 || metric>=8) return;
    static const char *const keys[]={"cells","filled","numeric","distinct","sum","average","minimum","maximum"};
    review_author_item(a,"statistics",keys[metric],"statistics.toggle_metric","checkbox",label,bounds,clip,true,checked,focused,NULL,file,line);
    char id[128]; snprintf(id,sizeof(id),"author.statistics.action.%s",keys[metric]); DxReviewWidget *widget=(DxReviewWidget *)dx_review_find(a->review->registry,id);
    bool ready=a->step_preview.active ? a->step_preview.stats_state==PREVIEW_STATS_READY : !a->statistics_frame && a->statistics_state==STATS_READY;
    if (!widget || !ready) return;
    const SelectionStats *stats=a->step_preview.active ? &a->step_preview.statistics : &a->statistics;
    if (metric<4) {
        uint64_t cells=a->step_preview.active ? a->step_preview.stats_bounds.active ? (a->step_preview.stats_bounds.r1-a->step_preview.stats_bounds.r0+1)*(a->step_preview.stats_bounds.c1-a->step_preview.stats_bounds.c0+1) : 0 : a->statistics_cells;
        uint64_t count=metric==0 ? cells : metric==1 ? stats->filled : metric==2 ? stats->numeric : stats->distinct; char exact[32]; snprintf(exact,sizeof(exact),"%llu",(unsigned long long)count);
        dx_review_value_set(&widget->value,"UBIGINT",false,exact);
    } else if (!stats->non_finite && !(metric==4 && stats->sum_overflow) && !(metric==5 && stats->average_overflow)) {
        const char *raw=metric==4 ? stats->sum : metric==5 ? stats->average : metric==6 ? stats->minimum : stats->maximum;
        dx_review_value_set(&widget->value,stats->approximate || metric==5 ? "DOUBLE" : "exact-numeric",!raw,raw);
    }
    widget->truncated|=widget->value.truncated; a->review->registry->truncated|=widget->truncated;
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
static bool review_guide_id(const GuideReviewTarget *target,char id[128]) {
    switch (target->kind) {
    case GUIDE_REVIEW_PANEL: snprintf(id,128,"author.guide"); break;
    case GUIDE_REVIEW_HEADING: snprintf(id,128,"author.guide.heading"); break;
    case GUIDE_REVIEW_CLOSE: case GUIDE_REVIEW_START: snprintf(id,128,"author.guide.action.%s",target->key); break;
    case GUIDE_REVIEW_INTRO: case GUIDE_REVIEW_NOTE: snprintf(id,128,"author.guide.%s.line%zu",target->key,target->text_line); break;
    case GUIDE_REVIEW_NAME: case GUIDE_REVIEW_DESCRIPTION: snprintf(id,128,"author.guide.section.%s.%s.line%zu",target->key,target->kind==GUIDE_REVIEW_NAME ? "name" : "description",target->text_line); break;
    case GUIDE_REVIEW_SHORTCUTS: snprintf(id,128,"author.guide.shortcuts"); break;
    case GUIDE_REVIEW_LABEL: case GUIDE_REVIEW_KEY: snprintf(id,128,"author.guide.shortcut.%s.%s.line%zu",target->key,target->kind==GUIDE_REVIEW_LABEL ? "label" : "key",target->text_line); break;
    case GUIDE_REVIEW_SCROLL: snprintf(id,128,"author.guide.scrollbar"); break;
    case GUIDE_REVIEW_THUMB: snprintf(id,128,"author.guide.scrollbar.thumb"); break;
    default: return false;
    }
    return true;
}
static void review_guide(App *a,const GuideReviewFrame *frame) {
    if (!review_armed(a) || !frame || !frame->armed) return;
    bool body_captured=false,key_captured=false;
    char state[256]; snprintf(state,sizeof(state)," author_kind=guide author_parent=author.guide guide_count=%zu guide_overflow=%d guide_scroll=%.3f guide_maximum=%.3f guide_focus=%d",frame->count,frame->overflow,frame->scroll,frame->maximum,frame->focus); review_author_capture_state(a,state);
    if (frame->overflow) { a->review->registry->truncated=true; a->review->registry->frame.truncated=true; a->review->frame.truncated=true; }
    for (size_t i=0;i<frame->count && i<GUIDE_REVIEW_TARGETS;i++) {
        const GuideReviewTarget *target=&frame->targets[i]; char id[128]; if (!review_guide_id(target,id)) continue;
        const char *parent="author.guide",*role="text",*action="guide.inspect",*capture_key=NULL; int layer=132;
        switch (target->kind) {
        case GUIDE_REVIEW_PANEL: parent=NULL; role="dialog"; layer=130; capture_key="panel"; break;
        case GUIDE_REVIEW_HEADING: capture_key="heading"; break;
        case GUIDE_REVIEW_CLOSE: case GUIDE_REVIEW_START: role="button"; action="guide.close"; capture_key=target->key; layer=133; break;
        case GUIDE_REVIEW_SHORTCUTS: role="group"; layer=131; break;
        case GUIDE_REVIEW_LABEL: case GUIDE_REVIEW_KEY: parent="author.guide.shortcuts"; break;
        case GUIDE_REVIEW_SCROLL: role="scrollbar"; action="guide.scroll"; layer=136; capture_key="scrollbar"; break;
        case GUIDE_REVIEW_THUMB: parent="author.guide.scrollbar"; role="scrollbar"; action="guide.scroll"; layer=137; capture_key="thumb"; break;
        default: break;
        }
        if (target->kind==GUIDE_REVIEW_PANEL) {
            Rectangle viewport=box(0,0,GetScreenWidth(),GetScreenHeight());
            review_author_widget(a,"author.guide.backdrop",NULL,"guide","guide.inspect","region","Modal backdrop",viewport,viewport,129,true,false,false,true,NULL,target->file,target->source_line);
            review_author_widget(a,"author.guide.backdrop.background","author.guide.backdrop","guide","guide.inspect","region","Modal backdrop",viewport,viewport,129,true,false,false,false,NULL,target->file,target->source_line);
        }
        DxReviewWidget *widget=review_author_widget(a,id,parent,"guide",action,role,target->text,target->bounds,target->clip,layer,target->enabled,false,target->focused,target->container,NULL,target->file,target->source_line);
        if (!widget) continue;
        if (!strcmp(role,"text")) { dx_review_value_set(&widget->value,"VARCHAR",false,target->text); widget->truncated|=widget->value.truncated; }
        widget->truncated|=target->truncated; a->review->registry->truncated|=widget->truncated;
        if (target->kind==GUIDE_REVIEW_PANEL) {
            const DxReviewDataIdentity *origin=&widget->data; char identity[256];
            snprintf(identity,sizeof(identity)," guide_kind=%s guide_origin=%llu,%llu,%llu,%llu,%llu,%d,%llu,%llu",origin->kind,(unsigned long long)origin->sheet_id,(unsigned long long)origin->object_id,(unsigned long long)origin->source_id,(unsigned long long)origin->schema_generation,(unsigned long long)origin->query_generation,origin->has_cell,(unsigned long long)origin->row,(unsigned long long)origin->column); review_author_capture_state(a,identity);
            review_author_widget(a,"author.guide.background","author.guide","guide","guide.inspect","region","Dialog background",target->bounds,target->clip,130,true,false,false,false,NULL,target->file,target->source_line);
        }
        if (capture_key) {
            Rectangle visible=GetCollisionRec((Rectangle){widget->bounds.x,widget->bounds.y,widget->bounds.width,widget->bounds.height},(Rectangle){widget->clip.x,widget->clip.y,widget->clip.width,widget->clip.height});
            char geometry[192]; snprintf(geometry,sizeof(geometry)," guide_%s_rect=%.3f,%.3f,%.3f,%.3f",capture_key,visible.x,visible.y,visible.width,visible.height); review_author_capture_state(a,geometry);
        }
        bool body=target->kind==GUIDE_REVIEW_INTRO || target->kind==GUIDE_REVIEW_NAME || target->kind==GUIDE_REVIEW_DESCRIPTION || target->kind==GUIDE_REVIEW_NOTE;
        if ((body && !body_captured) || (target->kind==GUIDE_REVIEW_KEY && !key_captured)) {
            const char *key=body ? "text" : "key"; char hex[129],metadata[512]; bool truncated=review_author_hex64(hex,target->text);
            Rectangle visible=GetCollisionRec(target->bounds,target->clip);
            snprintf(metadata,sizeof(metadata)," guide_%s_rect=%.3f,%.3f,%.3f,%.3f guide_%s_id=%s guide_%s_hex=%s guide_%s_truncated=%d",key,visible.x,visible.y,visible.width,visible.height,key,widget->id,key,hex,key,truncated || target->truncated); review_author_capture_state(a,metadata);
            if (body) body_captured=true; else key_captured=true;
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
    w.data=review_feedback_origin(a,"filter",filter_object(a));
    w.truncated|=strstr(w.data.kind,"-truncated")!=NULL;
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
    DxReviewWidget *panel=(DxReviewWidget *)dx_review_find(a->review->registry,"filter.panel"); if (panel) { panel->data=review_feedback_origin(a,"filter",filter_object(a)); panel->truncated|=strstr(panel->data.kind,"-truncated")!=NULL; a->review->registry->truncated|=panel->truncated; }
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
        w->checked=value_checked(a,v->key,&partial); w->indeterminate=partial; w->data.row=index;
        snprintf(w->parent_id,sizeof(w->parent_id),"filter.values");
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
        DxReviewWidget *conditions=(DxReviewWidget *)dx_review_find(a->review->registry,"filter.conditions"); if (conditions) { conditions->data=review_feedback_origin(a,"filter",filter_object(a)); conditions->truncated|=strstr(conditions->data.kind,"-truncated")!=NULL; a->review->registry->truncated|=conditions->truncated; }
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
    else if (command==DX_COMMAND_COPY) {
        char *text=input_selection_text(&r->note);
        if (text) {
#ifdef DUXCEL_BROWSER_HOST
            dx_web_clipboard_begin(text);
#else
            SetClipboardText(text);
#endif
            free(text);
        }
    }
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
#define REVIEW_SORT_END (REVIEW_RELATION_END+2*6)
#define REVIEW_STATS_END (REVIEW_SORT_END+3*6)
#define REVIEW_INSPECTOR_END (REVIEW_STATS_END+4*6)
#define REVIEW_SETTINGS_END (REVIEW_INSPECTOR_END+2*6)
#define REVIEW_FORMAT_END (REVIEW_SETTINGS_END+2*6)
#define REVIEW_SQL_END (REVIEW_FORMAT_END+2*6)
#define REVIEW_OWNER_END (REVIEW_SQL_END+14)
#define REVIEW_GUIDE_END (REVIEW_OWNER_END+2*6)
#define REVIEW_CELL_END (REVIEW_GUIDE_END+2*6)
#define REVIEW_ERROR_END (REVIEW_CELL_END+2*6)
#define REVIEW_QUERY_FEEDBACK_END (REVIEW_ERROR_END+6)
#define REVIEW_WORKSPACE_END (REVIEW_QUERY_FEEDBACK_END+2*4)
#define REVIEW_FEEDBACK_END (REVIEW_WORKSPACE_END+6*4)
#define REVIEW_SMOKE_STAGES (REVIEW_FEEDBACK_END+4)
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
    char *query_error_text,*query_error_pointer; TextLines query_error_lines; size_t *query_error_offsets,*query_editor_offsets;
    int query_error_scroll; double query_error_wheel; bool query_failure_started;
    uint64_t query_failure_jobs,query_failure_sql,query_failure_counts,query_failure_prepares,query_failure_request;
    struct { Cell *cells; uint64_t id,revision,source_id,total; size_t rows,columns; } query_views[DX_MAX_OBJECTS];
    bool relation_added; int relation_object,relation_focus,relation_scroll; Analysis relation_source;
    uint64_t relation_id,relation_revision; Relation *relation_rows; size_t relation_count;
    Analysis sort_source,sort_draft; int sort_object,sort_focus,sort_level,sort_field_scroll,sort_order_scroll;
    size_t sort_choice,sort_count,*sort_matches; uint64_t sort_owner,sort_revision,sort_search_revision; char sort_city_up[128];
    SelectionStats stats_cache; StatsKey stats_key; StatsState stats_state; Analysis stats_source; char *stats_text[4],*stats_error;
    uint64_t stats_request,stats_cells,stats_first,stats_rows; size_t stats_column,stats_columns; unsigned stats_mask; bool stats_enabled,stats_frame; int stats_object,stats_focus;
    bool inspector_added; int inspector_object; Analysis inspector_source;
    char *inspector_text,*inspector_type,*inspector_column,*inspector_pointers[3]; char inspector_address[48];
    TextLines inspector_layout; size_t *inspector_offsets,inspector_bytes,inspector_scroll;
    DxReviewDataIdentity inspector_origin; bool inspector_null,inspector_drag,inspector_valid,inspector_stale,inspector_truncated;
    float inspector_grab; double inspector_layout_ms;
    float settings_requested_scale; bool settings_reduced_motion;
    FormatStore format_copy,format_borrowed; CellFormat format_value; UIFlyoutLayout format_flyout;
    UIAction format_action; Vector2 format_origin; float format_width; double format_wheel;
    int format_parent_group,format_parent_command,format_focus,format_scroll,format_popup_group,format_popup_focus,format_setup_item;
    bool format_setup_started,format_parent_closing,format_more_anchor;
    Analysis sql_source; char *sql_text,*sql_pointer,*sql_expected,*sql_pane_text,*sql_pane_pointer; size_t *sql_pane_offsets;
    DxReviewDataIdentity sql_origin; TextLines sql_pane_lines; int sql_owner,sql_scroll,sql_pane_scroll,sql_setup_item,sql_pane_focus,sql_pane_step;
    double sql_wheel; uint64_t sql_revision,sql_owner_id; bool sql_valid,sql_stale,sql_setup_started,sql_pane_visible,sql_pane_collapsed,sql_pane_mode;
    char owner_fixture[4096],owner_copy[4096];
    UIGuideLayout guide_layout; TextLines guide_lines[18]; size_t *guide_offsets[18];
    UIScrollbar guide_scrollbar; float guide_scroll; int guide_focus; bool guide_navigation_sent,guide_button_focus;
    TextInput cell_input; char *cell_text,*cell_history[64]; CellStore cell_model,cell_borrowed; SheetCell *cell_pointers;
    size_t cell_worksheet_redo; uint64_t cell_object_next; int cell_setup_item;
    bool cell_setup_started,cell_clicked,cell_f2_sent,cell_inserted,cell_undo_sent;
    char error_directory[4096],error_book_path[4096]; char *error_text,*error_pointer;
    double error_since; uint64_t error_status_counter,error_request,error_jobs,error_object_next;
    size_t error_sheets,error_active; int error_setup_item; bool error_setup_started;
    bool workspace_started,workspace_search_focus; int workspace_setup_item,workspace_object,workspace_focus,workspace_role,workspace_value,workspace_field_scroll;
    size_t *workspace_matches,*workspace_match_pointer,workspace_match_count,workspace_match_capacity,workspace_schema_count;
    uint64_t workspace_source_id,workspace_source_revision,workspace_match_generation,workspace_search_revision;
    float workspace_author_scroll,workspace_dock_scroll[2]; UIMarksLayout workspace_marks; UIShelfLayout workspace_axes[2];
    UIContextTitleLayout workspace_title; Rectangle workspace_fields,workspace_tokens[4][DX_PIVOT_FIELDS];
    char workspace_mark_labels[DX_MARK_ROLES][512];
    bool feedback_fixture,feedback_table_added,feedback_transformed,feedback_pivot_added,feedback_started,feedback_end_sent,feedback_hidden,feedback_resume;
    int feedback_table,feedback_pivot,feedback_item,feedback_scroll,feedback_chrome,feedback_filter_focus; float feedback_width;
    char feedback_path[4096],feedback_backup[4096]; Analysis feedback_source,feedback_preview_source;
    StepPreview feedback_preview; GridLocation feedback_preview_selection; DistinctPage feedback_values[4]; char *feedback_keys[4],*feedback_context,*feedback_pending,*feedback_message;
    int64_t feedback_total,feedback_value_focus; uint64_t feedback_value_scroll,feedback_search_revision; bool feedback_loading,feedback_failed,feedback_all;
    char *feedback_message_pointer; double feedback_message_since; uint64_t feedback_message_revision;
    bool long_sheet_added,long_table_added,long_query_submitted,long_filter_opened;
    char long_field[192]; TextInput long_search; size_t long_sheet;
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
    static const char *const suffix[]={"new","open","save","save_as","close","import","export","quit"};
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
    free(t->query_error_text); free(t->query_error_offsets); free(t->query_editor_offsets);
    t->query_error_text=NULL; t->query_error_offsets=t->query_editor_offsets=NULL;
    analysis_free(&t->relation_source);
    analysis_free(&t->sort_source); analysis_free(&t->sort_draft);
    analysis_free(&t->stats_source); free(t->stats_error); t->stats_error=NULL; for (size_t i=0;i<4;i++) { free(t->stats_text[i]); t->stats_text[i]=NULL; }
    analysis_free(&t->inspector_source); free(t->inspector_text); free(t->inspector_type); free(t->inspector_column); free(t->inspector_offsets);
    t->inspector_text=t->inspector_type=t->inspector_column=NULL; t->inspector_offsets=NULL;
    format_free(&t->format_copy); /* format_borrowed only observes App ownership. */
    analysis_free(&t->sql_source); free(t->sql_text); free(t->sql_expected); free(t->sql_pane_text); free(t->sql_pane_offsets);
    t->sql_text=t->sql_expected=t->sql_pane_text=NULL; t->sql_pane_offsets=NULL;
    for (size_t i=0;i<18;i++) { free(t->guide_offsets[i]); t->guide_offsets[i]=NULL; }
    free(t->cell_text); t->cell_text=NULL; free(t->cell_pointers); t->cell_pointers=NULL; cells_free(&t->cell_model);
    for (size_t i=0;i<64;i++) { free(t->cell_history[i]); t->cell_history[i]=NULL; }
    free(t->error_text); t->error_text=NULL;
    free(t->workspace_matches); t->workspace_matches=NULL;
    analysis_free(&t->feedback_source); analysis_free(&t->feedback_preview_source);
    free(t->feedback_context); free(t->feedback_pending); free(t->feedback_message); t->feedback_context=t->feedback_pending=t->feedback_message=NULL;
    for (size_t i=0;i<4;i++) { free(t->feedback_keys[i]); t->feedback_keys[i]=NULL; }
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
static void review_smoke_query_state_remember(App *a,ReviewSmoke *t,bool pending) {
    t->query_source=analysis_copy(query_source(a));
    t->query_object=a->query_object; t->query_focus=a->query_focus; t->query_source_revision=a->query_source_revision;
    t->query_draft_revision=a->query_draft_revision; t->query_edit_revision=a->query_edit_revision; t->query_editor=a->query_editor;
    t->query_owner=a->query_object>=0 ? active_sheet(a)->objects.items[a->query_object].id : 0;
    t->query_pending=pending; t->query_complete=false; t->query_reply=NULL;
    t->dialog_text=dx_strdup(a->input.text); t->dialog_cursor=a->input.cursor; t->dialog_anchor=a->input.anchor; t->dialog_revision=a->input.revision;
    t->query_error_text=a->query_error ? dx_strdup(a->query_error) : NULL; t->query_error_pointer=a->query_error; t->query_error_lines=a->query_errors;
    t->query_error_scroll=a->query_error_scroll; t->query_error_wheel=a->query_error_wheel;
    if (a->query_errors.offsets) { size_t bytes=(a->query_errors.count+1)*sizeof(size_t); t->query_error_offsets=dx_alloc(bytes); memcpy(t->query_error_offsets,a->query_errors.offsets,bytes); }
    if (a->query_editor.lines.offsets) { size_t bytes=(a->query_editor.lines.count+1)*sizeof(size_t); t->query_editor_offsets=dx_alloc(bytes); memcpy(t->query_editor_offsets,a->query_editor.lines.offsets,bytes); }
    for (size_t i=0;i<active_sheet(a)->objects.count;i++) {
        SheetObject *object=&active_sheet(a)->objects.items[i]; Page *page=&a->objects[i].page;
        t->query_views[i].cells=page->cells; t->query_views[i].id=object->id; t->query_views[i].revision=object->revision; t->query_views[i].source_id=object->source_id;
        t->query_views[i].total=page->total; t->query_views[i].rows=page->row_count; t->query_views[i].columns=page->column_count;
    }
}
static void review_smoke_query_remember(App *a,ReviewSmoke *t,bool pending) {
    review_smoke_author_remember(a,t); review_smoke_query_state_remember(a,t,pending);
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
        a->query_draft_revision!=t->query_draft_revision || a->query_edit_revision!=t->query_edit_revision || a->query_loading!=(t->query_pending && !closed) || a->busy!=(t->query_pending && !closed) || a->query_validated || a->query_error!=t->query_error_pointer || !review_smoke_text_same(a->query_error,t->query_error_text) || a->io_busy ||
        a->request_id!=t->request+(closed && t->query_pending ? 1 : 0) || a->page.cells!=t->page || a->book.dirty!=t->dirty || active_sheet(a)->undo_actions_count!=t->history || active_sheet(a)->formats.revision!=t->choice_formats ||
        a->ribbon_mode!=t->choice_ribbon_mode || a->ui.scale!=t->choice_scale || a->grid.row_height!=t->choice_row_height || location.row!=t->selection.row || location.col!=t->selection.col || location.end_row!=t->selection.end_row || location.end_col!=t->selection.end_col ||
        strcmp(a->input.text,t->dialog_text) || a->input.cursor!=t->dialog_cursor || a->input.anchor!=t->dialog_anchor || a->input.revision!=t->dialog_revision ||
        editor->scroll!=t->query_editor.scroll || editor->revision!=t->query_editor.revision || editor->revealed_revision!=t->query_editor.revealed_revision || editor->revealed_cursor!=t->query_editor.revealed_cursor || editor->lines.offsets!=t->query_editor.lines.offsets ||
        !source || !analysis_same_result(source,&t->query_source) || !analysis_sorts_equal(source,&t->query_source) || !review_smoke_author_models_same(a,t)) return false;
    if (memcmp(&a->query_errors,&t->query_error_lines,sizeof(TextLines)) || a->query_error_scroll!=t->query_error_scroll || a->query_error_wheel!=t->query_error_wheel ||
        (t->query_error_offsets && memcmp(a->query_errors.offsets,t->query_error_offsets,(a->query_errors.count+1)*sizeof(size_t))) ||
        (t->query_editor_offsets && (editor->lines.count!=t->query_editor.lines.count || memcmp(editor->lines.offsets,t->query_editor_offsets,(editor->lines.count+1)*sizeof(size_t))))) return false;
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
    char document[128]; review_document_id(a,document);
    for (size_t i=0;i<registry->count;i++) {
        const DxReviewWidget *widget=&registry->widgets[i]; if (strncmp(widget->id,"author.source_query",19)) continue;
        const DxReviewDataIdentity *data=&widget->data;
        if (!data->present || strcmp(data->kind,"source-query") || strcmp(data->document_id,document) || strcmp(data->document_id,registry->frame.document_id) || data->sheet_id!=a->book.active+1 || data->source_id!=t->query_owner || data->object_id!=t->query_owner || data->query_generation!=t->query_source_revision || data->schema_generation || data->field[0] || data->has_cell || data->row || data->column) return false;
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
    const DxReviewWidget *status=dx_review_find(registry,"author.source_query.status"),*error=dx_review_find(registry,"author.source_query.validation_error");
    const char *expected=t->query_pending ? "Checking the query… Escape cancels; the displayed source stays intact." : "Apply clears source filters, sort and summary in one undoable action.";
    if (!status || strcmp(status->role,"text") || status->input_eligible || status->focused || !status->enabled || !status->value.present || status->value.is_null || status->value.truncated || strcmp(status->value.type,"VARCHAR") || strcmp(status->value.exact_text,expected) || !review_smoke_sheet_pick(a,status,parent) || !strstr(a->review->frame.state_text,t->query_pending ? " query_status_loading=1" : " query_status_loading=0")) return false;
    if (t->query_error_text ? !error || strcmp(error->role,"region") || error->input_eligible || error->focused || !error->enabled || !error->value.present || error->value.is_null || error->value.truncated || strcmp(error->value.type,"VARCHAR") || strcmp(error->value.exact_text,t->query_error_text) || !review_smoke_sheet_pick(a,error,parent) : error!=NULL) return false;
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
    review_smoke_author_release(t); if (item) { ui_set_scale(&a->ui,1); SetWindowSize(1280,820); } return 1;
}
static const Analysis *review_smoke_stats_source(App *a,int index) { return index>=0 ? object_source(a,&active_sheet(a)->objects.items[index]) : &a->analysis; }
static void review_smoke_stats_remember(App *a,ReviewSmoke *t) {
    review_smoke_author_remember(a,t); t->stats_source=analysis_copy(review_smoke_stats_source(a,a->statistics_object)); t->stats_cache=a->statistics; t->stats_key=a->statistics_key;
    t->stats_state=a->statistics_state; t->stats_request=a->statistics_request; t->stats_cells=a->statistics_cells; t->stats_first=a->statistics_first; t->stats_rows=a->statistics_rows; t->stats_column=a->statistics_column; t->stats_columns=a->statistics_columns;
    t->stats_mask=a->statistics_mask; t->stats_enabled=a->statistics_enabled; t->stats_frame=a->statistics_frame; t->stats_object=a->statistics_object; t->stats_focus=a->statistics_focus; t->stats_error=a->statistics_error ? dx_strdup(a->statistics_error) : NULL;
    const char *texts[]={a->statistics.sum,a->statistics.average,a->statistics.minimum,a->statistics.maximum}; for (size_t i=0;i<4;i++) t->stats_text[i]=texts[i] ? dx_strdup(texts[i]) : NULL;
    for (size_t i=0;i<active_sheet(a)->objects.count;i++) {
        SheetObject *object=&active_sheet(a)->objects.items[i]; Page *page=&a->objects[i].page;
        t->query_views[i].cells=page->cells; t->query_views[i].id=object->id; t->query_views[i].revision=object->revision; t->query_views[i].source_id=object->source_id;
        t->query_views[i].total=page->total; t->query_views[i].rows=page->row_count; t->query_views[i].columns=page->column_count;
    }
    t->query_pending=false;
    dx_lock(&a->worker.mutex); t->query_jobs=a->worker.jobs_started; t->query_sql=a->worker.db.query_count; t->query_counts=a->worker.db.count_queries; t->query_prepares=a->worker.db.relation_prepares; t->query_latest=a->worker.latest_id; dx_unlock(&a->worker.mutex);
}
static bool review_smoke_stats_same(App *a,ReviewSmoke *t) {
    const SelectionStats *cache=&a->statistics,*previous=&t->stats_cache; const StatsKey *key=&a->statistics_key,*old=&t->stats_key;
    if (!review_smoke_author_same(a,t) || !analysis_same_result(review_smoke_stats_source(a,t->stats_object),&t->stats_source) || !analysis_sorts_equal(review_smoke_stats_source(a,t->stats_object),&t->stats_source) ||
        a->statistics_state!=t->stats_state || a->statistics_request!=t->stats_request || a->statistics_cells!=t->stats_cells || a->statistics_first!=t->stats_first || a->statistics_rows!=t->stats_rows || a->statistics_column!=t->stats_column || a->statistics_columns!=t->stats_columns ||
        a->statistics_mask!=t->stats_mask || a->statistics_enabled!=t->stats_enabled || a->statistics_frame!=t->stats_frame || a->statistics_object!=t->stats_object || a->statistics_focus!=t->stats_focus || !review_smoke_text_same(a->statistics_error,t->stats_error) ||
        key->r0!=old->r0 || key->r1!=old->r1 || key->c0!=old->c0 || key->c1!=old->c1 || key->source_revision!=old->source_revision || key->cell_revision!=old->cell_revision || key->objects!=old->objects || key->total!=old->total || key->sheet!=old->sheet || key->columns!=old->columns || key->count_pending!=old->count_pending || key->active!=old->active ||
        cache->cells!=previous->cells || cache->filled!=previous->filled || cache->numeric!=previous->numeric || cache->distinct!=previous->distinct || cache->approximate!=previous->approximate || cache->non_finite!=previous->non_finite || cache->sum_overflow!=previous->sum_overflow || cache->average_overflow!=previous->average_overflow || cache->query_ms!=previous->query_ms ||
        cache->sum!=previous->sum || cache->average!=previous->average || cache->minimum!=previous->minimum || cache->maximum!=previous->maximum) return false;
    const char *texts[]={cache->sum,cache->average,cache->minimum,cache->maximum}; for (size_t i=0;i<4;i++) if (!review_smoke_text_same(texts[i],t->stats_text[i])) return false;
    for (size_t i=0;i<active_sheet(a)->objects.count;i++) {
        SheetObject *object=&active_sheet(a)->objects.items[i]; Page *page=&a->objects[i].page;
        if (page->cells!=t->query_views[i].cells || object->id!=t->query_views[i].id || object->revision!=t->query_views[i].revision || object->source_id!=t->query_views[i].source_id || page->total!=t->query_views[i].total || page->row_count!=t->query_views[i].rows || page->column_count!=t->query_views[i].columns) return false;
    }
    return review_smoke_query_worker(a,t,false);
}
static bool review_smoke_stats_targets(App *a,ReviewSmoke *t,int item) {
    DxReviewRegistry *registry=a->review->registry; const char *parent="author.statistics"; const DxReviewWidget *panel=dx_review_find(registry,parent),*done=dx_review_find(registry,"author.statistics.action.done");
    bool frame=item==1; SheetObject *owner=t->stats_object>=0 ? &active_sheet(a)->objects.items[t->stats_object] : NULL;
    uint64_t object=owner ? owner->id : 0,source=owner ? owner->kind==OBJECT_TABLE ? owner->id : owner->source_id : 0,schema=owner && owner->kind!=OBJECT_TABLE ? object_source_revision(a,owner) : 0,revision=owner ? owner->revision : t->stats_key.source_revision;
    if (!panel || !panel->container || panel->input_eligible || !done || !done->enabled || !done->input_eligible || done->focused || !review_smoke_sheet_pick(a,done,parent) || registry->duplicate_count || registry->invalid_count || registry->overflow_count || strstr(a->review->frame.state_text," author_overflow=1")) return false;
    size_t rows=0;
    for (size_t i=0;i<registry->count;i++) {
        const DxReviewWidget *widget=&registry->widgets[i]; if (strncmp(widget->id,"author.statistics",17)) continue;
        const DxReviewDataIdentity *data=&widget->data;
        if (!data->present || strcmp(data->kind,frame ? "statistics-frame" : "statistics-range") || data->sheet_id!=a->book.active+1 || data->object_id!=object || data->source_id!=source || data->query_generation!=revision || data->schema_generation!=schema || data->has_cell || strcmp(data->field,frame ? "" : "Amount") || data->row!=(frame ? owner->row : 0) || data->column!=(frame ? owner->col : 2)) return false;
        if (!strcmp(widget->role,"checkbox")) rows++;
    }
    if (rows!=8) return false;
    const char *keys[]={"cells","filled","numeric","distinct","sum","average","minimum","maximum"};
    const char *exact[]={"5","5","5","5","145.25","29.05","-4.5","100"};
    for (size_t i=0;i<8;i++) {
        char id[128]; snprintf(id,sizeof(id),"%s.action.%s",parent,keys[i]); const DxReviewWidget *widget=dx_review_find(registry,id);
        if (!widget || strcmp(widget->role,"checkbox") || !widget->enabled || !widget->input_eligible || widget->checked!=((t->stats_mask&(1u<<i))!=0) || widget->focused!=(i==(size_t)t->stats_focus) || !review_smoke_sheet_pick(a,widget,parent)) return false;
        if (frame ? widget->value.present : !widget->value.present || widget->value.is_null || strcmp(widget->value.type,i<4 ? "UBIGINT" : i==5 || item==2 ? "DOUBLE" : "exact-numeric") || strcmp(widget->value.exact_text,exact[i])) return false;
    }
    snprintf(t->author_target,sizeof(t->author_target),"author.statistics.action.sum"); return true;
}
static void review_smoke_stats_diagnostic(App *a,ReviewSmoke *t,const char *reason) {
    fprintf(stderr,"Review statistics mismatch %s stage=%d modal=%d state=%d/%d object=%d/%d frame=%d/%d mask=%u/%u focus=%d/%d request=%llu/%llu cells=%llu/%llu target=%s actual=%s flags=%d,%d,%d,%d\n",reason,t->stage,(int)a->modal,(int)a->statistics_state,(int)t->stats_state,a->statistics_object,t->stats_object,a->statistics_frame,t->stats_frame,a->statistics_mask,t->stats_mask,a->statistics_focus,t->stats_focus,(unsigned long long)a->statistics_request,(unsigned long long)t->stats_request,(unsigned long long)a->statistics_cells,(unsigned long long)t->stats_cells,t->author_target,a->review->target.widget.id,a->statistics.approximate,a->statistics.non_finite,a->statistics.sum_overflow,a->statistics.average_overflow);
    for (size_t i=0;i<a->review->registry->count;i++) {
        const DxReviewWidget *widget=&a->review->registry->widgets[i]; if (strncmp(widget->id,"author.statistics",17)) continue;
        fprintf(stderr,"Review statistics widget %s role=%s enabled=%d checked=%d focus=%d kind=%s owner=%llu source=%llu row=%llu column=%llu field=%s value=%d/%d/%s/%s\n",widget->id,widget->role,widget->enabled,widget->checked,widget->focused,widget->data.kind,(unsigned long long)widget->data.object_id,(unsigned long long)widget->data.source_id,(unsigned long long)widget->data.row,(unsigned long long)widget->data.column,widget->data.field,widget->value.present,widget->value.is_null,widget->value.type,widget->value.exact_text);
    }
}
static int review_smoke_stats_tick(App *a,ReviewSmoke *t) {
    int item=(t->stage-REVIEW_SORT_END)/6,phase=(t->stage-REVIEW_SORT_END)%6; bool frame=item==1; DxReviewCapture *r=a->review;
    if (!phase) {
        if (r->phase!=DX_REPORT_IDLE || !smoke_work_idle(a)) return 0;
        a->statistics_enabled=true; a->statistics_mask=frame ? 0xaa : 0x55; a->selected_object=frame ? t->query_pivot : item==2 ? 0 : t->relation_object;
        if (!object_ready(a,(size_t)a->selected_object)) return 0;
        SheetObject *object=selected_object(a); a->matrix_cells_active=false;
        if (frame) { if (object->kind!=OBJECT_PIVOT) return -1; grid_select_cell(&a->grid,object->row,object->col,false); a->grid.end_row=object->row+object->rows-1; a->grid.end_col=object->col+object->columns-1; }
        else if (item==2) { if (!selected_pivot(a) || a->page.column_count!=6) return -1; grid_select_cell(&a->grid,0,2,false); a->grid.end_row=4; if (authoring_frame_selected(a)) return -1; }
        else { if (object->kind!=OBJECT_TABLE || a->objects[a->selected_object].page.column_count<3 || a->objects[a->selected_object].page.columns[2].number_type.kind!=NUM_DECIMAL) return -1; grid_select_cell(&a->grid,object->row+2,object->col+2,false); a->grid.end_row=object->row+6; }
        ui_set_scale(&a->ui,frame ? 1.5f : 1); SetWindowSize(frame ? 760 : 1280,frame ? 640 : 820); show_workspace(a); open_statistics(a); a->statistics_focus=4; return 1;
    }
    if (phase==1) {
        if (a->modal!=MODAL_STATISTICS) return -1;
        if (r->phase!=DX_REPORT_IDLE || !smoke_work_idle(a) || !review_can_capture(a) || a->popup_presence.alpha!=1 || a->popup_presence.offset || a->ui.scale!=(frame ? 1.5f : 1) || GetScreenWidth()!=(frame ? 760 : 1280) || GetScreenHeight()!=(frame ? 640 : 820)) return 0;
        if (frame) { if (!a->statistics_frame || a->statistics_state!=STATS_IDLE || a->statistics_request || strcmp(statistics_scope(a),"Matrix frame")) return -1; }
        else {
            if (a->statistics_state!=STATS_READY) return 0;
            if (a->statistics_frame || a->statistics_object!=(item==2 ? -1 : t->relation_object) || a->statistics_cells!=5 || a->statistics.numeric!=5 || a->statistics.approximate!=(item==2) || !a->statistics.sum || strcmp(a->statistics.sum,"145.25") || (item==2 && !selected_pivot(a))) return -1;
        }
        review_smoke_stats_remember(a,t); ui_note_key_press(KEY_PERIOD,DX_MOD_SUPER|DX_MOD_SHIFT); return 1;
    }
    if (phase==2) {
        if (r->phase!=DX_REPORT_PICK) return 0;
        bool same=review_smoke_stats_same(a,t),targets=same && review_smoke_stats_targets(a,t,item);
        if (!same || !targets) { review_smoke_stats_diagnostic(a,t,same ? "targets" : "retained state"); return -1; }
        r->toolbar_top=true; review_smoke_pick(a,t->author_target); return 1;
    }
    if (phase==3) {
        if (r->phase!=DX_REPORT_NOTE) return 0;
        if (strcmp(r->target.widget.id,t->author_target) || r->target.widget.checked!=!frame || !review_smoke_stats_same(a,t)) { review_smoke_stats_diagnostic(a,t,"actual picker target/state"); return -1; }
        ui_note_key_press(KEY_ESCAPE,0); return 1;
    }
    if (r->phase!=DX_REPORT_IDLE) return 0;
    if (phase==4) { if (!review_smoke_stats_same(a,t)) { review_smoke_stats_diagnostic(a,t,"report cancellation"); return -1; } ui_note_key_press(KEY_ESCAPE,0); return 1; }
    t->choice_modal=MODAL_NONE; if (!review_smoke_stats_same(a,t)) { review_smoke_stats_diagnostic(a,t,"normal statistics cancellation"); return -1; }
    printf("PASS: native statistics review scope=%s retains checklist/focus, exact cached values/flags, source/model/pages and query jobs\n",frame ? "matrix frame" : item==2 ? "primary range with Chart selected" : "DECIMAL source range");
    review_smoke_author_release(t); if (frame) { ui_set_scale(&a->ui,1); SetWindowSize(1280,820); } return 1;
}
static bool review_smoke_inspector_identity(const DxReviewDataIdentity *left,const DxReviewDataIdentity *right) {
    return left->present==right->present && left->has_cell==right->has_cell && !strcmp(left->kind,right->kind) && !strcmp(left->document_id,right->document_id) && !strcmp(left->field,right->field) &&
        left->sheet_id==right->sheet_id && left->source_id==right->source_id && left->object_id==right->object_id && left->schema_generation==right->schema_generation && left->query_generation==right->query_generation && left->row==right->row && left->column==right->column;
}
static void review_smoke_inspector_remember(App *a,ReviewSmoke *t) {
    review_smoke_stats_remember(a,t); t->inspector_source=analysis_copy(&selected_object(a)->source);
    t->inspector_text=dx_strdup(a->inspector_text); t->inspector_type=dx_strdup(a->inspector_type); t->inspector_column=dx_strdup(a->inspector_column);
    t->inspector_pointers[0]=a->inspector_text; t->inspector_pointers[1]=a->inspector_type; t->inspector_pointers[2]=a->inspector_column;
    snprintf(t->inspector_address,sizeof(t->inspector_address),"%s",a->inspector_address);
    t->inspector_layout=a->inspector_lines;
    if (a->inspector_lines.offsets) { size_t bytes=(a->inspector_lines.count+1)*sizeof(size_t); t->inspector_offsets=dx_alloc(bytes); memcpy(t->inspector_offsets,a->inspector_lines.offsets,bytes); }
    t->inspector_bytes=a->inspector_bytes; t->inspector_scroll=a->inspector_scroll; t->inspector_null=a->inspector_null; t->inspector_drag=a->inspector_drag; t->inspector_grab=a->inspector_grab; t->inspector_layout_ms=a->inspector_layout_ms;
    t->inspector_origin=a->inspector_origin; t->inspector_valid=a->inspector_origin_valid; t->inspector_stale=a->inspector_origin_stale; t->inspector_truncated=a->inspector_origin_truncated;
}
static bool review_smoke_inspector_same(App *a,ReviewSmoke *t,bool closed) {
    SheetObject *object=selected_object(a);
    if (!review_smoke_stats_same(a,t) || !object || !analysis_same_result(&object->source,&t->inspector_source) || !analysis_sorts_equal(&object->source,&t->inspector_source)) return false;
    if (closed) { const DxReviewDataIdentity empty={0}; return !a->inspector_text && !a->inspector_type && !a->inspector_column && !a->inspector_lines.offsets && !a->inspector_lines.count && !a->inspector_origin_valid && !a->inspector_origin_stale && !a->inspector_origin_truncated && review_smoke_inspector_identity(&a->inspector_origin,&empty); }
    const TextLines *lines=&a->inspector_lines,*old=&t->inspector_layout;
    return a->inspector_text==t->inspector_pointers[0] && a->inspector_type==t->inspector_pointers[1] && a->inspector_column==t->inspector_pointers[2] &&
        !strcmp(a->inspector_text,t->inspector_text) && !strcmp(a->inspector_type,t->inspector_type) && !strcmp(a->inspector_column,t->inspector_column) && !strcmp(a->inspector_address,t->inspector_address) &&
        a->inspector_bytes==t->inspector_bytes && a->inspector_scroll==t->inspector_scroll && a->inspector_null==t->inspector_null && a->inspector_drag==t->inspector_drag && a->inspector_grab==t->inspector_grab && a->inspector_layout_ms==t->inspector_layout_ms &&
        a->inspector_origin_valid==t->inspector_valid && a->inspector_origin_stale==t->inspector_stale && a->inspector_origin_truncated==t->inspector_truncated && review_smoke_inspector_identity(&a->inspector_origin,&t->inspector_origin) &&
        lines->offsets==old->offsets && lines->count==old->count && lines->capacity==old->capacity && lines->width==old->width && lines->scale==old->scale && lines->font_size==old->font_size && lines->raster_size==old->raster_size && lines->limited==old->limited &&
        (!lines->offsets || !memcmp(lines->offsets,t->inspector_offsets,(lines->count+1)*sizeof(size_t)));
}
static bool review_smoke_inspector_targets(App *a,ReviewSmoke *t,int item) {
    const char *parent="author.cell_inspector"; DxReviewRegistry *registry=a->review->registry;
    const DxReviewWidget *panel=dx_review_find(registry,parent),*heading=dx_review_find(registry,"author.cell_inspector.heading"),*value=dx_review_find(registry,"author.cell_inspector.value"),*copy=dx_review_find(registry,"author.cell_inspector.action.copy"),*close=dx_review_find(registry,"author.cell_inspector.action.close");
    if (!panel || !panel->container || panel->input_eligible || !heading || heading->input_eligible || !review_smoke_sheet_pick(a,heading,parent) || !value || strcmp(value->role,"region") || value->input_eligible || !review_smoke_sheet_pick(a,value,parent) ||
        !copy || strcmp(copy->role,"button") || copy->enabled!=(item!=0) || copy->input_eligible!=(item!=0) || copy->checked || copy->focused || !review_smoke_sheet_pick(a,copy,parent) || !close || !close->enabled || !close->input_eligible || close->checked || close->focused || !review_smoke_sheet_pick(a,close,parent) ||
        registry->duplicate_count || registry->invalid_count || registry->overflow_count || strstr(a->review->frame.state_text," author_overflow=1")) return false;
    SheetObject *owner=selected_object(a); ObjectView *view=&a->objects[a->selected_object];
    size_t row=item==0 ? 0 : item==1 ? 2 : item==2 ? 1 : 3,column=item==3 ? 5 : 1;
    if (!t->inspector_valid || t->inspector_stale || t->inspector_truncated || !t->inspector_origin.present || !t->inspector_origin.has_cell || strcmp(t->inspector_origin.kind,"inspector-result") || t->inspector_origin.object_id!=owner->id || t->inspector_origin.source_id!=owner->id || t->inspector_origin.query_generation!=owner->revision || t->inspector_origin.schema_generation!=view->source_revision || t->inspector_origin.sheet_id!=a->book.active+1 || t->inspector_origin.row!=row || t->inspector_origin.column!=column || strcmp(t->inspector_origin.field,view->page.columns[column].name)) return false;
    for (size_t i=0;i<registry->count;i++) {
        const DxReviewWidget *widget=&registry->widgets[i]; if (strncmp(widget->id,parent,strlen(parent))) continue;
        if (!review_smoke_inspector_identity(&widget->data,&t->inspector_origin) || widget->identity_truncated) return false;
    }
    if (!value->value.present || strcmp(value->value.type,"VARCHAR") || value->value.is_null!=(item==0) || value->value.truncated!=(item==3) || value->truncated!=(item==3)) return false;
    if (item<3 ? strcmp(value->value.exact_text,item==2 ? "NULL" : "") : strlen(value->value.exact_text)!=sizeof(value->value.exact_text)-1 || strncmp(value->value.exact_text,t->inspector_text,sizeof(value->value.exact_text)-4) || strcmp(value->value.exact_text+sizeof(value->value.exact_text)-4,"…") || !registry->truncated || !strstr(a->review->frame.state_text," inspector_value_truncated=1")) return false;
    const DxReviewWidget *track=dx_review_find(registry,"author.cell_inspector.scrollbar"),*thumb=dx_review_find(registry,"author.cell_inspector.scrollbar.thumb");
    if (item==3) {
        if (!track || !thumb || !thumb->enabled || !thumb->input_eligible || thumb->focused || t->inspector_bytes!=50000 || !t->inspector_scroll || thumb->bounds.y<=track->bounds.y || !review_smoke_sheet_pick(a,thumb,parent)) return false;
        snprintf(t->author_target,sizeof(t->author_target),"%s",thumb->id);
    } else { if (track || thumb || registry->truncated) return false; snprintf(t->author_target,sizeof(t->author_target),"%s",item==0 ? copy->id : value->id); }
    DxReviewTarget picked;
    return dx_review_pick(registry,2*a->ui.scale,2*a->ui.scale,DX_REVIEW_PICK_CONTROL,&picked) && !strcmp(picked.widget.id,"author.cell_inspector.backdrop.background");
}
static void review_smoke_inspector_diagnostic(App *a,ReviewSmoke *t,const char *reason) {
    fprintf(stderr,"Review inspector mismatch %s stage=%d modal=%d selected=%d/%d bytes=%zu/%zu null=%d/%d scroll=%zu/%zu lines=%zu/%zu origin=%d,%d,%d owner=%llu source=%llu row=%llu column=%llu field=%s target=%s actual=%s\n",reason,t->stage,(int)a->modal,a->selected_object,t->author_selected,a->inspector_bytes,t->inspector_bytes,a->inspector_null,t->inspector_null,a->inspector_scroll,t->inspector_scroll,a->inspector_lines.count,t->inspector_layout.count,a->inspector_origin_valid,a->inspector_origin_stale,a->inspector_origin_truncated,(unsigned long long)a->inspector_origin.object_id,(unsigned long long)a->inspector_origin.source_id,(unsigned long long)a->inspector_origin.row,(unsigned long long)a->inspector_origin.column,a->inspector_origin.field,t->author_target,a->review->target.widget.id);
    for (size_t i=0;i<a->review->registry->count;i++) { const DxReviewWidget *widget=&a->review->registry->widgets[i]; if (strncmp(widget->id,"author.cell_inspector",strlen("author.cell_inspector"))) continue; fprintf(stderr,"Review inspector widget %s parent=%s role=%s enabled=%d eligible=%d value=%d/%d/%s length=%zu truncated=%d/%d bounds=%.2f,%.2f,%.2f,%.2f clip=%.2f,%.2f,%.2f,%.2f\n",widget->id,widget->parent_id,widget->role,widget->enabled,widget->input_eligible,widget->value.present,widget->value.is_null,widget->value.type,strlen(widget->value.exact_text),widget->truncated,widget->identity_truncated,widget->bounds.x,widget->bounds.y,widget->bounds.width,widget->bounds.height,widget->clip.x,widget->clip.y,widget->clip.width,widget->clip.height); }
}
static int review_smoke_inspector_tick(App *a,ReviewSmoke *t) {
    int item=(t->stage-REVIEW_STATS_END)/6,phase=(t->stage-REVIEW_STATS_END)%6; bool long_value=item==3; DxReviewCapture *r=a->review;
    if (!phase) {
        if (r->phase!=DX_REPORT_IDLE || !smoke_work_idle(a)) return 0;
        if (!t->inspector_added) {
            a->selected_object=-1; grid_select_cell(&a->grid,90,10,false); if (!import_table(a,"tests/fixtures/null-text.csv")) return -1;
            t->inspector_object=a->selected_object; t->inspector_added=true; return 0;
        }
        a->selected_object=long_value ? t->relation_object : t->inspector_object;
        if (!object_ready(a,(size_t)a->selected_object)) return 0;
        SheetObject *owner=selected_object(a); Page *page=&a->objects[a->selected_object].page;
        uint64_t row=item==0 ? 0 : item==1 ? 2 : item==2 ? 1 : 3; size_t column=long_value ? 5 : 1;
        const Cell *cell=copy_cached_cell(page,row,column);
        if (!cell || owner->kind!=OBJECT_TABLE || cell->is_null!=(item==0) || strcmp(page->columns[column].type_name,"VARCHAR") || (item!=0 && (!cell->text || (long_value ? strlen(cell->text)!=50000 : strcmp(cell->text,item==2 ? "NULL" : ""))))) return -1;
        ui_set_scale(&a->ui,long_value ? 1.5f : 1); SetWindowSize(long_value ? 760 : 1280,long_value ? 640 : 820); show_workspace(a); a->matrix_cells_active=false;
        grid_select_cell(&a->grid,owner->row+2+row,owner->col+column,false); inspect_value(a); if (a->modal!=MODAL_INSPECT) return -1; return 1;
    }
    if (phase==1) {
        if (a->modal!=MODAL_INSPECT) return -1;
        if (r->phase!=DX_REPORT_IDLE || !smoke_work_idle(a) || !review_can_capture(a) || a->ui.scale!=(long_value ? 1.5f : 1) || GetScreenWidth()!=(long_value ? 760 : 1280) || GetScreenHeight()!=(long_value ? 640 : 820)) return 0;
        if (long_value) {
            TextLines *lines=&a->inspector_lines;
            if (!lines->offsets || lines->scale!=a->ui.scale || lines->raster_size!=a->ui.raster_size) return 0;
            if (!a->inspector_scroll) { ui_note_key_press(KEY_END,0); return 0; }
            if (lines->limited || lines->offsets[lines->count]!=a->inspector_bytes) return -1;
        }
        review_smoke_inspector_remember(a,t); ui_note_key_press(KEY_PERIOD,DX_MOD_SUPER|DX_MOD_SHIFT); return 1;
    }
    if (phase==2) {
        if (r->phase!=DX_REPORT_PICK) return 0;
        bool same=review_smoke_inspector_same(a,t,false),targets=same && review_smoke_inspector_targets(a,t,item);
        if (!same || !targets) { review_smoke_inspector_diagnostic(a,t,same ? "targets" : "retained state"); return -1; }
        r->toolbar_top=true; review_smoke_pick(a,t->author_target); return 1;
    }
    if (phase==3) {
        if (r->phase!=DX_REPORT_NOTE) return 0;
        if (strcmp(r->target.widget.id,t->author_target) || !review_smoke_inspector_same(a,t,false) || (item==0 && r->target.widget.enabled)) { review_smoke_inspector_diagnostic(a,t,"actual picker target/state"); return -1; }
        ui_note_key_press(KEY_ESCAPE,0); return 1;
    }
    if (r->phase!=DX_REPORT_IDLE) return 0;
    if (phase==4) { if (!review_smoke_inspector_same(a,t,false)) { review_smoke_inspector_diagnostic(a,t,"report cancellation"); return -1; } ui_note_key_press(KEY_ESCAPE,0); return 1; }
    t->choice_modal=MODAL_NONE; if (!review_smoke_inspector_same(a,t,true)) { review_smoke_inspector_diagnostic(a,t,"normal inspector cancellation"); return -1; }
    printf("PASS: native cell-inspector review value=%s retains owned full text/type, pinned cell origin, layout/scroll, statistics, source/models/pages and query jobs; Copy never activated\n",item==0 ? "SQL NULL" : item==1 ? "empty VARCHAR" : item==2 ? "literal NULL" : "50000-byte VARCHAR");
    review_smoke_author_release(t); if (long_value) { ui_set_scale(&a->ui,1); SetWindowSize(1280,820); } return 1;
}
static bool review_smoke_settings_same(App *a,ReviewSmoke *t) {
    return a->ui.requested_scale==t->settings_requested_scale && a->reduced_motion==t->settings_reduced_motion && review_smoke_stats_same(a,t);
}
static bool review_smoke_settings_targets(App *a,ReviewSmoke *t,int item) {
    DxReviewRegistry *registry=a->review->registry; const char *parent="author.settings";
    const DxReviewWidget *panel=dx_review_find(registry,parent),*heading=dx_review_find(registry,"author.settings.heading");
    if (!panel || !panel->container || panel->input_eligible || !heading || heading->input_eligible || !review_smoke_sheet_pick(a,heading,parent) || registry->duplicate_count || registry->invalid_count || registry->overflow_count || registry->truncated || !strstr(a->review->frame.state_text," author_count=06 author_overflow=0")) return false;
    static const char *const keys[]={"close","interface.compact","interface.standard","interface.large","interface.extra_large","reduce_motion"};
    const DxReviewWidget *buttons[6]; char id[128];
    for (size_t i=0;i<6;i++) {
        snprintf(id,sizeof(id),"%s.action.%s",parent,keys[i]); const DxReviewWidget *widget=dx_review_find(registry,id); buttons[i]=widget;
        bool checked=i==5 ? item==1 : i==(size_t)(item ? 4 : 2);
        if (!widget || strcmp(widget->role,"button") || !widget->enabled || !widget->input_eligible || widget->checked!=checked || widget->focused || !review_smoke_sheet_pick(a,widget,parent)) return false;
    }
    size_t count=0;
    for (size_t i=0;i<registry->count;i++) {
        const DxReviewWidget *widget=&registry->widgets[i]; if (strncmp(widget->id,parent,strlen(parent))) continue;
        const DxReviewDataIdentity *data=&widget->data;
        if (widget->truncated || data->present || strcmp(data->kind,"app-settings") || data->document_id[0] || data->sheet_id || data->source_id || data->object_id || data->schema_generation || data->query_generation || data->has_cell || data->row || data->column || data->field[0]) return false;
        if (strstr(widget->id,".action.")) count++;
    }
    if (count!=6) return false;
    if (item ? buttons[1]->bounds.x!=buttons[2]->bounds.x || buttons[2]->bounds.x!=buttons[3]->bounds.x || buttons[3]->bounds.x!=buttons[4]->bounds.x || buttons[1]->bounds.y>=buttons[2]->bounds.y || buttons[2]->bounds.y>=buttons[3]->bounds.y || buttons[3]->bounds.y>=buttons[4]->bounds.y :
        buttons[1]->bounds.y!=buttons[2]->bounds.y || buttons[3]->bounds.y!=buttons[4]->bounds.y || buttons[1]->bounds.x>=buttons[2]->bounds.x || buttons[1]->bounds.y>=buttons[3]->bounds.y) return false;
    DxReviewTarget picked;
    if (!dx_review_pick(registry,panel->bounds.x+2*a->ui.scale,panel->bounds.y+2*a->ui.scale,DX_REVIEW_PICK_CONTROL,&picked) || strcmp(picked.widget.id,"author.settings.background") || !dx_review_pick(registry,2*a->ui.scale,2*a->ui.scale,DX_REVIEW_PICK_CONTROL,&picked) || strcmp(picked.widget.id,"author.settings.backdrop.background")) return false;
    snprintf(t->author_target,sizeof(t->author_target),"%s",buttons[item ? 5 : 2]->id); return true;
}
static void review_smoke_settings_diagnostic(App *a,ReviewSmoke *t,const char *reason) {
    fprintf(stderr,"Review settings mismatch %s stage=%d modal=%d scale=%.3f/%.3f requested=%.3f/%.3f reduced_motion=%d/%d target=%s actual=%s\n",reason,t->stage,(int)a->modal,a->ui.scale,t->choice_scale,a->ui.requested_scale,t->settings_requested_scale,a->reduced_motion,t->settings_reduced_motion,t->author_target,a->review->target.widget.id);
    for (size_t i=0;i<a->review->registry->count;i++) { const DxReviewWidget *widget=&a->review->registry->widgets[i]; if (strncmp(widget->id,"author.settings",strlen("author.settings"))) continue; fprintf(stderr,"Review settings widget %s parent=%s role=%s enabled=%d checked=%d focus=%d data=%d/%s owner=%llu source=%llu bounds=%.2f,%.2f,%.2f,%.2f clip=%.2f,%.2f,%.2f,%.2f\n",widget->id,widget->parent_id,widget->role,widget->enabled,widget->checked,widget->focused,widget->data.present,widget->data.kind,(unsigned long long)widget->data.object_id,(unsigned long long)widget->data.source_id,widget->bounds.x,widget->bounds.y,widget->bounds.width,widget->bounds.height,widget->clip.x,widget->clip.y,widget->clip.width,widget->clip.height); }
}
static int review_smoke_settings_tick(App *a,ReviewSmoke *t) {
    int item=(t->stage-REVIEW_INSPECTOR_END)/6,phase=(t->stage-REVIEW_INSPECTOR_END)%6; DxReviewCapture *r=a->review;
    if (!phase) {
        if (r->phase!=DX_REPORT_IDLE || !smoke_work_idle(a)) return 0;
        ui_set_scale(&a->ui,item ? 1.5f : 1); SetWindowSize(item ? 760 : 1280,item ? 640 : 820); a->reduced_motion=item==1; open_modal(a,MODAL_SETTINGS);
        return a->modal==MODAL_SETTINGS ? 1 : -1;
    }
    if (phase==1) {
        if (a->modal!=MODAL_SETTINGS) return -1;
        if (r->phase!=DX_REPORT_IDLE || !smoke_work_idle(a) || !review_can_capture(a) || a->ui.scale!=(item ? 1.5f : 1) || GetScreenWidth()!=(item ? 760 : 1280) || GetScreenHeight()!=(item ? 640 : 820)) return 0;
        review_smoke_stats_remember(a,t); t->settings_requested_scale=a->ui.requested_scale; t->settings_reduced_motion=a->reduced_motion;
        ui_note_key_press(KEY_PERIOD,DX_MOD_SUPER|DX_MOD_SHIFT); return 1;
    }
    if (phase==2) {
        if (r->phase!=DX_REPORT_PICK) return 0;
        bool same=review_smoke_settings_same(a,t),targets=same && review_smoke_settings_targets(a,t,item);
        if (!same || !targets) { review_smoke_settings_diagnostic(a,t,same ? "targets" : "retained state"); return -1; }
        r->toolbar_top=true; review_smoke_pick(a,t->author_target); return 1;
    }
    if (phase==3) {
        if (r->phase!=DX_REPORT_NOTE) return 0;
        if (strcmp(r->target.widget.id,t->author_target) || !r->target.widget.checked || !review_smoke_settings_same(a,t)) { review_smoke_settings_diagnostic(a,t,"actual picker target/state"); return -1; }
        ui_note_key_press(KEY_ESCAPE,0); return 1;
    }
    if (r->phase!=DX_REPORT_IDLE) return 0;
    if (phase==4) { if (!review_smoke_settings_same(a,t)) { review_smoke_settings_diagnostic(a,t,"report cancellation"); return -1; } ui_note_key_press(KEY_ESCAPE,0); return 1; }
    t->choice_modal=MODAL_NONE; if (!review_smoke_settings_same(a,t)) { review_smoke_settings_diagnostic(a,t,"normal settings cancellation"); return -1; }
    printf("PASS: native Settings review layout=%s retains actual/requested UI scale, reduced motion, source/models/pages/statistics and query jobs\n",item ? "narrow single column at150%" : "wide two columns at100%");
    review_smoke_author_release(t); if (item) { ui_set_scale(&a->ui,1); SetWindowSize(1280,820); } return 1;
}
static bool review_smoke_format_values(const FormatStore *live,const FormatStore *copy) {
    if (live->count!=copy->count || live->scope_count!=copy->scope_count || live->revision!=copy->revision) return false;
    for (size_t i=0;i<live->count;i++) {
        const FormatPatch *a=&live->items[i],*b=&copy->items[i];
        if (a->range.r0!=b->range.r0 || a->range.r1!=b->range.r1 || a->range.c0!=b->range.c0 || a->range.c1!=b->range.c1 || a->mask!=b->mask || memcmp(&a->value,&b->value,sizeof(a->value)) || a->selector!=b->selector || a->guard_count!=b->guard_count || (a->guard_count && memcmp(a->guard_ids,b->guard_ids,a->guard_count*sizeof(size_t)))) return false;
    }
    for (size_t i=0;i<live->scope_count;i++) if (live->scopes[i].owner!=copy->scopes[i].owner || live->scopes[i].bytes!=copy->scopes[i].bytes || memcmp(live->scopes[i].definition,copy->scopes[i].definition,live->scopes[i].bytes)) return false;
    return true;
}
static void review_smoke_format_remember(App *a,ReviewSmoke *t) {
    review_smoke_stats_remember(a,t); t->format_borrowed=active_sheet(a)->formats; format_copy(&t->format_copy,&active_sheet(a)->formats); t->format_value=active_format(a);
    t->format_flyout=a->ribbon_flyout; t->format_action=a->format_action; t->format_origin=a->format_origin; t->format_width=a->format_width; t->format_wheel=a->wheel_select;
    t->format_parent_group=a->format_return_group; t->format_parent_command=a->format_return_command; t->format_focus=a->context_focus; t->format_scroll=a->context_scroll;
    t->format_popup_group=a->ribbon_popup_group; t->format_popup_focus=a->ribbon_popup_focus; t->format_more_anchor=a->ribbon_popup_more_anchor;
}
static bool review_smoke_format_same(App *a,ReviewSmoke *t) {
    CellFormat value=active_format(a);
    return review_smoke_stats_same(a,t) && !memcmp(&active_sheet(a)->formats,&t->format_borrowed,sizeof(t->format_borrowed)) && review_smoke_format_values(&active_sheet(a)->formats,&t->format_copy) && !memcmp(&value,&t->format_value,sizeof(value)) &&
        a->format_action==t->format_action && a->format_origin.x==t->format_origin.x && a->format_origin.y==t->format_origin.y && a->format_width==t->format_width && a->wheel_select==t->format_wheel &&
        a->format_return_group==t->format_parent_group && a->format_return_command==t->format_parent_command && a->context_focus==t->format_focus && a->context_scroll==t->format_scroll &&
        (t->format_parent_group<0 || (a->ribbon_popup_group==t->format_popup_group && a->ribbon_popup_focus==t->format_popup_focus && a->ribbon_popup_more_anchor==t->format_more_anchor && !memcmp(&a->ribbon_flyout,&t->format_flyout,sizeof(t->format_flyout))));
}
static bool review_smoke_format_targets(App *a,ReviewSmoke *t,int item) {
    DxReviewRegistry *registry=a->review->registry; char parent[128],id[128]; snprintf(parent,sizeof(parent),"author.%s",item ? "number_format" : "font_size");
    const DxReviewWidget *panel=dx_review_find(registry,parent); snprintf(id,sizeof(id),"%s.heading",parent);
    if (!panel || !panel->container || panel->input_eligible || dx_review_find(registry,id) || registry->duplicate_count || registry->invalid_count || registry->overflow_count || registry->truncated || strstr(a->review->frame.state_text," author_overflow=1")) return false;
    uint64_t r0,r1; size_t c0,c1; grid_bounds(&a->grid,&r0,&r1,&c0,&c1); size_t rows=0,checks=0,focused=0;
    for (size_t i=0;i<registry->count;i++) {
        const DxReviewWidget *widget=&registry->widgets[i]; if (strncmp(widget->id,parent,strlen(parent))) continue;
        const DxReviewDataIdentity *data=&widget->data;
        if (!data->present || strcmp(data->kind,"worksheet-format") || data->sheet_id!=a->book.active+1 || data->object_id || data->source_id || data->schema_generation || data->query_generation!=t->format_borrowed.revision || !data->has_cell || data->row!=r0 || data->column!=c0 || data->field[0]) return false;
        if (!strstr(widget->id,".action.")) continue;
        char key[32],label[32]; if (item) { snprintf(key,sizeof(key),"%s",rows ? "number" : "accounting"); snprintf(label,sizeof(label),"%s",rows ? "Number" : "Accounting"); }
        else { snprintf(key,sizeof(key),"size.%zu",11+rows); snprintf(label,sizeof(label),"%zu",11+rows); }
        snprintf(id,sizeof(id),"%s.action.%s",parent,key); bool checked=item ? (rows ? t->format_value.number==FORMAT_NUMBER : t->format_value.number!=FORMAT_NUMBER) : t->format_value.size==11+rows;
        if (strcmp(widget->id,id) || strcmp(widget->label,label) || strcmp(widget->role,"menuitemradio") || !widget->enabled || !widget->input_eligible || widget->checked!=checked || widget->focused!=(rows==(size_t)t->format_focus) || !review_smoke_sheet_pick(a,widget,parent)) return false;
        rows++; checks+=widget->checked; focused+=widget->focused;
    }
    DxReviewTarget picked;
    if (rows!=(item ? 2 : 6) || checks!=1 || focused!=1 || !dx_review_pick(registry,panel->bounds.x+.5f*a->ui.scale,panel->bounds.y+.5f*a->ui.scale,DX_REVIEW_PICK_CONTROL,&picked)) return false;
    snprintf(id,sizeof(id),"%s.background",parent); if (strcmp(picked.widget.id,id) || fabsf(panel->bounds.width-a->format_width*a->ui.scale)>.01f) return false;
    if (item) {
        char opener[128]; snprintf(opener,sizeof(opener),"ribbon.0.flyout.%s",review_action_id(ACTION_NUMBER_FORMAT)+8);
        const DxReviewWidget *anchor=dx_review_find(registry,opener); Rectangle expected=t->format_flyout.commands[t->format_parent_command];
        if (!anchor || anchor->bounds.x!=expected.x || anchor->bounds.y!=expected.y || anchor->bounds.width!=expected.width || anchor->bounds.height!=expected.height || t->format_parent_group<0 || !t->format_flyout.valid) return false;
    }
    snprintf(t->author_target,sizeof(t->author_target),"%s.action.%s",parent,item ? "number" : "size.13"); return true;
}
static void review_smoke_format_diagnostic(App *a,ReviewSmoke *t,const char *reason) {
    fprintf(stderr,"Review format mismatch %s stage=%d modal=%d action=%d/%d parent=%d,%d/%d,%d focus=%d/%d scroll=%d/%d width=%.3f/%.3f revision=%llu/%llu target=%s actual=%s\n",reason,t->stage,(int)a->modal,(int)a->format_action,(int)t->format_action,a->format_return_group,a->format_return_command,t->format_parent_group,t->format_parent_command,a->context_focus,t->format_focus,a->context_scroll,t->format_scroll,a->format_width,t->format_width,(unsigned long long)active_sheet(a)->formats.revision,(unsigned long long)t->format_borrowed.revision,t->author_target,a->review->target.widget.id);
    for (size_t i=0;i<a->review->registry->count;i++) { const DxReviewWidget *widget=&a->review->registry->widgets[i]; if (strncmp(widget->id,"author.font_size",strlen("author.font_size")) && strncmp(widget->id,"author.number_format",strlen("author.number_format"))) continue; fprintf(stderr,"Review format widget %s parent=%s role=%s enabled=%d checked=%d focus=%d kind=%s owner=%llu source=%llu row=%llu column=%llu generation=%llu bounds=%.2f,%.2f,%.2f,%.2f clip=%.2f,%.2f,%.2f,%.2f\n",widget->id,widget->parent_id,widget->role,widget->enabled,widget->checked,widget->focused,widget->data.kind,(unsigned long long)widget->data.object_id,(unsigned long long)widget->data.source_id,(unsigned long long)widget->data.row,(unsigned long long)widget->data.column,(unsigned long long)widget->data.query_generation,widget->bounds.x,widget->bounds.y,widget->bounds.width,widget->bounds.height,widget->clip.x,widget->clip.y,widget->clip.width,widget->clip.height); }
}
static int review_smoke_format_tick(App *a,ReviewSmoke *t) {
    int item=(t->stage-REVIEW_SETTINGS_END)/6,phase=(t->stage-REVIEW_SETTINGS_END)%6; DxReviewCapture *r=a->review;
    UIAction action=item ? ACTION_NUMBER_FORMAT : ACTION_FONT_SIZE; int command=-1;
    for (int i=0;i<ribbon_counts[RIBBON_HOME];i++) if (ribbon_commands[RIBBON_HOME][i].action==action) command=i;
    if (command<0) return -1;
    if (!phase) {
        if (r->phase!=DX_REPORT_IDLE || !smoke_work_idle(a)) return 0;
        if (!t->format_setup_started || t->format_setup_item!=item) {
            t->format_setup_started=true; t->format_setup_item=item; t->format_parent_closing=false;
            ui_set_scale(&a->ui,item ? 1.5f : 1); SetWindowSize(item ? 760 : 1280,item ? 640 : 820); a->ribbon_tab=RIBBON_HOME; set_ribbon_mode(a,0); a->ribbon_focus=false;
            a->selected_object=0; a->matrix_cells_active=false; grid_select_cell(&a->grid,120,24,false); a->grid.end_row=121; a->grid.end_col=26; return 0;
        }
        if (GetScreenWidth()!=(item ? 760 : 1280) || GetScreenHeight()!=(item ? 640 : 820) || a->ui.scale!=(item ? 1.5f : 1)) return 0;
        /* Let the real range watcher finish before a popup intentionally
           suspends statistics scheduling; cancellation must add no job. */
        if (a->statistics_state==STATS_WAITING || a->statistics_state==STATS_CALCULATING) return 0;
        update_ribbon_layout(a);
        if (item) {
            int group=ui_ribbon_group_for(ribbon_groups[RIBBON_HOME],ribbon_group_counts[RIBBON_HOME],command);
            if (group<0) return -1;
            if (a->modal==MODAL_NONE) { open_command_group(a,group,command-ribbon_groups[RIBBON_HOME][group].first); return 0; }
            if (a->modal!=MODAL_COMMANDS || a->ribbon_popup_group!=group) return -1;
            Rectangle opener=a->ribbon_flyout.commands[command]; if (!a->ribbon_flyout.valid || opener.width<=0 || opener.height<=0) return 0;
        } else if (a->ribbon_layout.commands[command].width<=0) return -1;
        invoke_command(a,command); return a->modal==MODAL_FORMAT && a->format_action==action ? 1 : -1;
    }
    if (phase==1) {
        if (a->modal!=MODAL_FORMAT || a->format_action!=action) return -1;
        if (r->phase!=DX_REPORT_IDLE || !smoke_work_idle(a) || !review_can_capture(a) || a->popup_presence.alpha!=1 || a->popup_presence.offset) return 0;
        review_smoke_format_remember(a,t); ui_note_key_press(KEY_PERIOD,DX_MOD_SUPER|DX_MOD_SHIFT); return 1;
    }
    if (phase==2) {
        if (r->phase!=DX_REPORT_PICK) return 0;
        bool same=review_smoke_format_same(a,t),targets=same && review_smoke_format_targets(a,t,item);
        if (!same || !targets) { review_smoke_format_diagnostic(a,t,same ? "targets" : "retained state"); return -1; }
        r->toolbar_top=false; review_smoke_pick(a,t->author_target); return 1;
    }
    if (phase==3) {
        if (r->phase!=DX_REPORT_NOTE) return 0;
        if (strcmp(r->target.widget.id,t->author_target) || r->target.widget.checked!=(item==0) || !review_smoke_format_same(a,t)) { review_smoke_format_diagnostic(a,t,"actual picker target/state"); return -1; }
        ui_note_key_press(KEY_ESCAPE,0); return 1;
    }
    if (r->phase!=DX_REPORT_IDLE) return 0;
    if (phase==4) { if (!review_smoke_format_same(a,t)) { review_smoke_format_diagnostic(a,t,"report cancellation"); return -1; } ui_note_key_press(KEY_ESCAPE,0); return 1; }
    t->choice_modal=item && !t->format_parent_closing ? MODAL_COMMANDS : MODAL_NONE;
    if (!review_smoke_format_same(a,t)) { review_smoke_format_diagnostic(a,t,"normal child/parent cancellation"); return -1; }
    if (item && !t->format_parent_closing) { t->format_parent_closing=true; ui_note_key_press(KEY_ESCAPE,0); return 0; }
    if (item && (!a->ribbon_focus || a->ribbon_command!=(t->format_more_anchor && ribbon_more_available(a) ? RIBBON_MORE_COMMAND : command))) return -1;
    printf("PASS: native compact format review kind=%s retains worksheet range/revision, full format ownership/content/history, source/models/pages/statistics and query jobs%s\n",item ? "number" : "font size",item ? "; actual retained parent flyout restored and closed" : "");
    review_smoke_author_release(t); if (item) { ui_set_scale(&a->ui,1); SetWindowSize(1280,820); } return 1;
}
static const Analysis *review_smoke_sql_source(App *a,int owner) { return owner<0 ? &a->analysis : &active_sheet(a)->objects.items[owner].source; }
static void review_smoke_sql_remember(App *a,ReviewSmoke *t,int item) {
    review_smoke_stats_remember(a,t); t->sql_owner=item ? t->query_table : -1;
    t->sql_source=analysis_copy(review_smoke_sql_source(a,t->sql_owner));
    t->sql_owner_id=item ? active_sheet(a)->objects.items[t->sql_owner].id : 0; t->sql_revision=item ? active_sheet(a)->objects.items[t->sql_owner].revision : a->analysis_revision;
    char *definition=analysis_sql(&t->sql_source,false); t->sql_expected=dx_printf("-- Current source result\n%s",definition); free(definition);
    t->sql_text=dx_strdup(a->sql_text); t->sql_pointer=a->sql_text; t->sql_scroll=a->sql_scroll; t->sql_wheel=a->wheel_sql; t->sql_origin=a->sql_origin; t->sql_valid=a->sql_origin_valid; t->sql_stale=a->sql_origin_stale;
    t->sql_pane_text=a->inspector_sql_text ? dx_strdup(a->inspector_sql_text) : NULL; t->sql_pane_pointer=a->inspector_sql_text; t->sql_pane_lines=a->inspector_sql_lines; t->sql_pane_scroll=a->inspector_sql_scroll;
    if (a->inspector_sql_lines.offsets) { size_t bytes=(a->inspector_sql_lines.count+1)*sizeof(size_t); t->sql_pane_offsets=dx_alloc(bytes); memcpy(t->sql_pane_offsets,a->inspector_sql_lines.offsets,bytes); }
    t->sql_pane_visible=a->pipeline; t->sql_pane_collapsed=a->pipeline_collapsed; t->sql_pane_mode=a->pipeline_sql; t->sql_pane_focus=a->pipeline_focus_index; t->sql_pane_step=a->pipeline_selected_step;
}
static bool review_smoke_sql_same(App *a,ReviewSmoke *t) {
    const Analysis *source=review_smoke_sql_source(a,t->sql_owner); const TextLines *lines=&a->inspector_sql_lines,*old=&t->sql_pane_lines;
    return review_smoke_stats_same(a,t) && analysis_same_result(source,&t->sql_source) && analysis_sorts_equal(source,&t->sql_source) &&
        a->sql_text==t->sql_pointer && review_smoke_text_same(a->sql_text,t->sql_text) && !strcmp(a->sql_text,t->sql_expected) && a->sql_scroll==t->sql_scroll && a->wheel_sql==t->sql_wheel && a->sql_origin_valid==t->sql_valid && a->sql_origin_stale==t->sql_stale && review_smoke_inspector_identity(&a->sql_origin,&t->sql_origin) &&
        a->inspector_sql_text==t->sql_pane_pointer && review_smoke_text_same(a->inspector_sql_text,t->sql_pane_text) && a->inspector_sql_scroll==t->sql_pane_scroll && a->pipeline==t->sql_pane_visible && a->pipeline_collapsed==t->sql_pane_collapsed && a->pipeline_sql==t->sql_pane_mode && a->pipeline_focus_index==t->sql_pane_focus && a->pipeline_selected_step==t->sql_pane_step &&
        lines->offsets==old->offsets && lines->count==old->count && lines->capacity==old->capacity && lines->width==old->width && lines->scale==old->scale && lines->font_size==old->font_size && lines->raster_size==old->raster_size && lines->limited==old->limited &&
        (!lines->offsets || !memcmp(lines->offsets,t->sql_pane_offsets,(lines->count+1)*sizeof(size_t)));
}
static bool review_smoke_sql_targets(App *a,ReviewSmoke *t,int item) {
    DxReviewRegistry *registry=a->review->registry; const char *parent="author.generated_sql";
    const DxReviewWidget *panel=dx_review_find(registry,parent),*heading=dx_review_find(registry,"author.generated_sql.heading"),*content=dx_review_find(registry,"author.generated_sql.content"),*copy=dx_review_find(registry,"author.generated_sql.action.copy"),*close=dx_review_find(registry,"author.generated_sql.action.close");
    if (!panel || !panel->container || strcmp(panel->role,"dialog") || panel->input_eligible || !heading || heading->input_eligible || !review_smoke_sheet_pick(a,heading,parent) || !content || strcmp(content->role,"region") || content->input_eligible || !review_smoke_sheet_pick(a,content,parent) ||
        !copy || !copy->enabled || !copy->input_eligible || copy->checked || copy->focused || !review_smoke_sheet_pick(a,copy,parent) || !close || !close->enabled || !close->input_eligible || close->checked || close->focused || !review_smoke_sheet_pick(a,close,parent) || registry->duplicate_count || registry->invalid_count || registry->overflow_count || registry->truncated || strstr(a->review->frame.state_text," author_overflow=1")) return false;
    SheetObject *selected=selected_object(a);
    if (!t->sql_valid || t->sql_stale || !selected || selected->kind!=OBJECT_PIVOT || selected->id==t->sql_owner_id || t->sql_scroll || !t->sql_pane_text || !t->sql_pane_lines.offsets || t->sql_pane_pointer==t->sql_pointer || t->sql_origin.object_id!=t->sql_owner_id || t->sql_origin.source_id!=t->sql_owner_id || t->sql_origin.query_generation!=t->sql_revision || t->sql_origin.schema_generation || strcmp(t->sql_origin.kind,"generated-source-sql") || strcmp(t->sql_origin.document_id,a->review->frame.document_id)) return false;
    for (size_t i=0;i<registry->count;i++) {
        const DxReviewWidget *widget=&registry->widgets[i]; if (strncmp(widget->id,parent,strlen(parent))) continue;
        const DxReviewDataIdentity *data=&widget->data;
        if (!review_smoke_inspector_identity(data,&t->sql_origin) || !data->present || data->has_cell || data->field[0] || data->row || data->column || data->sheet_id!=a->book.active+1 || strcmp(data->document_id,registry->frame.document_id)) return false;
    }
    if (!content->value.present || content->value.is_null || content->value.truncated || strcmp(content->value.type,"VARCHAR") || strcmp(content->value.exact_text,t->sql_text) || dx_review_find(registry,"author.generated_sql.scrollbar") || !strstr(a->review->frame.state_text," sql_scroll=0 ") || !strstr(a->review->frame.state_text," sql_origin_valid=1 sql_origin_stale=0 ") || !strstr(a->review->frame.state_text," sql_text_hex=")) return false;
    DxReviewTarget picked;
    if (!dx_review_pick(registry,2*a->ui.scale,2*a->ui.scale,DX_REVIEW_PICK_CONTROL,&picked) || strcmp(picked.widget.id,"author.generated_sql.backdrop.background")) return false;
    snprintf(t->author_target,sizeof(t->author_target),"%s",item ? copy->id : content->id); return true;
}
static void review_smoke_sql_diagnostic(App *a,ReviewSmoke *t,const char *reason) {
    fprintf(stderr,"Review Generated SQL mismatch %s stage=%d modal=%d selected=%d/%d owner=%d/%llu revision=%llu origin=%d,%d/%d,%d owner=%llu source=%llu query=%llu document=%s frame_document=%s scroll=%d/%d pane=%d,%d,%d/%d,%d,%d target=%s actual=%s\n",reason,t->stage,(int)a->modal,a->selected_object,t->author_selected,t->sql_owner,(unsigned long long)t->sql_owner_id,(unsigned long long)t->sql_revision,a->sql_origin_valid,a->sql_origin_stale,t->sql_valid,t->sql_stale,(unsigned long long)a->sql_origin.object_id,(unsigned long long)a->sql_origin.source_id,(unsigned long long)a->sql_origin.query_generation,a->sql_origin.document_id,a->review->frame.document_id,a->sql_scroll,t->sql_scroll,a->pipeline,a->pipeline_collapsed,a->pipeline_sql,t->sql_pane_visible,t->sql_pane_collapsed,t->sql_pane_mode,t->author_target,a->review->target.widget.id);
    for (size_t i=0;i<a->review->registry->count;i++) { const DxReviewWidget *widget=&a->review->registry->widgets[i]; if (strncmp(widget->id,"author.generated_sql",strlen("author.generated_sql"))) continue; fprintf(stderr,"Review Generated SQL widget %s role=%s owner=%llu source=%llu kind=%s value=%d/%d/%s/%zu truncated=%d bounds=%.2f,%.2f,%.2f,%.2f clip=%.2f,%.2f,%.2f,%.2f\n",widget->id,widget->role,(unsigned long long)widget->data.object_id,(unsigned long long)widget->data.source_id,widget->data.kind,widget->value.present,widget->value.is_null,widget->value.type,strlen(widget->value.exact_text),widget->truncated,widget->bounds.x,widget->bounds.y,widget->bounds.width,widget->bounds.height,widget->clip.x,widget->clip.y,widget->clip.width,widget->clip.height); }
}
static int review_smoke_sql_tick(App *a,ReviewSmoke *t) {
    int item=(t->stage-REVIEW_FORMAT_END)/6,phase=(t->stage-REVIEW_FORMAT_END)%6; DxReviewCapture *r=a->review;
    if (!phase) {
        if (r->phase!=DX_REPORT_IDLE || !smoke_work_idle(a)) return 0;
        a->selected_object=item ? t->query_pivot : 0;
        if (!selected_pivot(a) || !object_ready(a,(size_t)a->selected_object)) return 0;
        if (selected_pivot(a)->kind!=OBJECT_PIVOT) { if (!set_representation(a,0)) return -1; return 0; }
        if (!t->sql_setup_started || t->sql_setup_item!=item) {
            t->sql_setup_started=true; t->sql_setup_item=item;
            ui_set_scale(&a->ui,item ? 1.5f : 1); SetWindowSize(item ? 760 : 1280,item ? 640 : 820); a->pipeline_sql=true; present_pipeline(a,true,false); return 0;
        }
        if (GetScreenWidth()!=(item ? 760 : 1280) || GetScreenHeight()!=(item ? 640 : 820) || a->ui.scale!=(item ? 1.5f : 1) || !a->pipeline_layout.valid || !a->inspector_sql_text || !a->inspector_sql_lines.offsets || a->inspector_sql_lines.scale!=a->ui.scale || a->inspector_sql_lines.raster_size!=a->ui.raster_size || a->statistics_state==STATS_WAITING || a->statistics_state==STATS_CALCULATING) return 0;
        inspect_source_sql(a); return a->modal==MODAL_SQL ? 1 : -1;
    }
    if (phase==1) {
        if (a->modal!=MODAL_SQL) return -1;
        if (r->phase!=DX_REPORT_IDLE || !smoke_work_idle(a) || !review_can_capture(a)) return 0;
        review_smoke_sql_remember(a,t,item);
        if (!review_smoke_sql_same(a,t) || strlen(t->sql_text)>=sizeof(((DxReviewValue *)0)->exact_text) || t->sql_scroll) { review_smoke_sql_diagnostic(a,t,"real source setup"); return -1; }
        ui_note_key_press(KEY_PERIOD,DX_MOD_SUPER|DX_MOD_SHIFT); return 1;
    }
    if (phase==2) {
        if (r->phase!=DX_REPORT_PICK) return 0;
        bool same=review_smoke_sql_same(a,t),targets=same && review_smoke_sql_targets(a,t,item);
        if (!same || !targets) { review_smoke_sql_diagnostic(a,t,same ? "targets" : "retained state"); return -1; }
        r->toolbar_top=true; review_smoke_pick(a,t->author_target); return 1;
    }
    if (phase==3) {
        if (r->phase!=DX_REPORT_NOTE) return 0;
        if (strcmp(r->target.widget.id,t->author_target) || !review_smoke_sql_same(a,t)) { review_smoke_sql_diagnostic(a,t,"actual picker target/state"); return -1; }
        ui_note_key_press(KEY_ESCAPE,0); return 1;
    }
    if (r->phase!=DX_REPORT_IDLE) return 0;
    if (phase==4) { if (!review_smoke_sql_same(a,t)) { review_smoke_sql_diagnostic(a,t,"report cancellation"); return -1; } ui_note_key_press(KEY_ESCAPE,0); return 1; }
    t->choice_modal=MODAL_NONE; if (!review_smoke_sql_same(a,t)) { review_smoke_sql_diagnostic(a,t,"normal SQL close retains owned definition"); return -1; }
    printf("PASS: native Generated SQL review owner=%s while another Pivot stays selected retains owned SQL/pinned origin, independent Transformations SQL, scroll0 and source/models/pages/statistics/jobs; Copy never activated\n",item ? "upstream Table" : "primary");
    review_smoke_author_release(t); if (item) { ui_set_scale(&a->ui,1); SetWindowSize(1280,820); } return 1;
}
static bool review_smoke_owner_format(const FormatStore *store,FormatRange range) {
    if (store->count!=1 || store->scope_count) return false;
    const FormatPatch *patch=&store->items[0]; CellFormat expected=format_default(); expected.flags=FORMAT_BOLD;
    return patch->range.r0==range.r0 && patch->range.r1==range.r1 && patch->range.c0==range.c0 && patch->range.c1==range.c1 &&
        patch->selector==FORMAT_SPATIAL && !patch->guard_count && patch->mask==FORMAT_BOLD && !memcmp(&patch->value,&expected,sizeof(expected));
}
static bool review_smoke_owner_model(App *a,size_t count,uint64_t high_water,bool original) {
    if (a->book.count!=3 || a->analysis.kind!=SOURCE_NONE || a->page.column_count) return false;
    const Worksheet *live=&a->book.sheets[0],*exhausted=&a->book.sheets[1],*empty=&a->book.sheets[2];
    const char *names[]={"Owner lifetime","Exhausted owner","Never allocated"};
    for (size_t i=0;i<3;i++) if (a->book.sheets[i].analysis.kind!=SOURCE_NONE || a->book.sheets[i].hidden || strcmp(a->book.sheets[i].name,names[i])) return false;
    if (live->objects.count!=count || live->objects.next_id!=high_water || exhausted->objects.count || exhausted->objects.next_id!=UINT64_MAX || empty->objects.count || empty->objects.next_id || exhausted->cells.count || empty->cells.count || exhausted->formats.count || exhausted->formats.scope_count ||
        live->cells.count!=2 || strcmp(cells_get(&live->cells,20,0),"9007199254740993.1234567890") || strcmp(cells_get(&live->cells,21,0),"NULL") || *cells_get(&live->cells,22,0) ||
        !review_smoke_owner_format(&live->formats,(FormatRange){20,21,0,0}) || !review_smoke_owner_format(&empty->formats,(FormatRange){0,DX_SHEET_ROWS-1,1,1}) ||
        format_at(&live->formats,20,0).flags!=FORMAT_BOLD || format_at(&live->formats,21,0).flags!=FORMAT_BOLD || format_at(&live->formats,19,0).flags || format_at(&live->formats,22,0).flags || format_at(&empty->formats,500,1).flags!=FORMAT_BOLD || format_at(&empty->formats,500,0).flags) return false;
    if (!count) return true;
    const SheetObject *first=&live->objects.items[0];
    if (original) {
        if (count!=1 || first->id!=UINT64_C(9007199254740993) || first->kind!=OBJECT_PIVOT || first->source_id || first->row || first->col!=8 || first->rows!=5 || first->columns!=4) return false;
        for (size_t role=0;role<4;role++) if (first->pivot.counts[role]) return false;
        return true;
    }
    if (first->id!=UINT64_C(9007199254740995) || first->kind!=OBJECT_TABLE || first->source_id || first->row || first->col!=8 || first->source.kind!=SOURCE_CSV || !first->source.path || strcmp(dx_basename(first->source.path),"transactions.csv")) return false;
    if (count>1) {
        const SheetObject *pivot=&live->objects.items[1];
        if (count!=2 || pivot->kind!=OBJECT_PIVOT || pivot->id!=high_water || pivot->source_id!=first->id) return false;
        for (size_t role=0;role<4;role++) if (pivot->pivot.counts[role]) return false;
    }
    return true;
}
static bool review_smoke_owner_table(App *a) {
    const ObjectView *view=&a->objects[0]; const SheetObject *table=&active_sheet(a)->objects.items[0];
    return a->book.active==0 && object_ready(a,0) && table->rows==7 && table->columns==6 && view->id==table->id && view->page.offset==0 && view->page.total==5 && view->page.row_count==5 && view->page.column_count==6 &&
        !strcmp(view->page.columns[1].name,"City") && !view->page.cells[1].is_null && !strcmp(view->page.cells[1].text,"İstanbul");
}
static bool review_smoke_owner_target(App *a,ReviewSmoke *t) {
    const char *parent="author.cell_inspector"; DxReviewRegistry *registry=a->review->registry;
    const DxReviewWidget *value=dx_review_find(registry,"author.cell_inspector.value"); const DxReviewDataIdentity *origin=&t->inspector_origin; const SheetObject *table=&active_sheet(a)->objects.items[0];
    char document[128]; review_document_id(a,document);
    if (!review_smoke_owner_model(a,2,UINT64_C(9007199254740997),false) || !review_smoke_owner_table(a) || !review_smoke_inspector_same(a,t,false) ||
        !t->inspector_valid || t->inspector_stale || t->inspector_truncated || t->inspector_null || strcmp(t->inspector_text,"İstanbul") || strcmp(t->inspector_type,"VARCHAR") ||
        !origin->present || !origin->has_cell || strcmp(origin->kind,"inspector-result") || strcmp(origin->document_id,document) || strcmp(origin->document_id,registry->frame.document_id) || origin->sheet_id!=1 || origin->object_id!=UINT64_C(9007199254740995) || origin->source_id!=origin->object_id || origin->query_generation!=table->revision || origin->schema_generation!=a->objects[0].source_revision || origin->row || origin->column!=1 || strcmp(origin->field,"City") ||
        !value || strcmp(value->role,"region") || value->input_eligible || !review_smoke_sheet_pick(a,value,parent) || !value->value.present || value->value.is_null || value->value.truncated || strcmp(value->value.type,"VARCHAR") || strcmp(value->value.exact_text,t->inspector_text) || registry->duplicate_count || registry->invalid_count || registry->overflow_count || registry->truncated || strstr(a->review->frame.state_text," author_overflow=1")) return false;
    for (size_t i=0;i<registry->count;i++) if (!strncmp(registry->widgets[i].id,parent,strlen(parent)) && (!review_smoke_inspector_identity(&registry->widgets[i].data,origin) || registry->widgets[i].identity_truncated)) return false;
    snprintf(t->author_target,sizeof(t->author_target),"%s",value->id); return true;
}
static int review_smoke_owner_tick(App *a,ReviewSmoke *t) {
    const int phase=t->stage-REVIEW_SQL_END; DxReviewCapture *r=a->review;
    if (a->modal==MODAL_ERROR || a->fetch_failed || (a->book.active==0 && a->book.count==3 && a->book.sheets[0].objects.count && a->objects[0].failed)) goto failed;
    if (phase<=8) {
        if (r->phase!=DX_REPORT_IDLE || !smoke_work_idle(a)) return 0;
        switch (phase) {
        case 0: {
            const char *fixture=getenv("DUXCEL_REVIEW_OWNER_FIXTURE"); char *absolute=fixture && *fixture ? dx_absolute_path(fixture) : NULL;
            if (!absolute) { fprintf(stderr,"Owner lifetime review requires private DUXCEL_REVIEW_OWNER_FIXTURE from native create-owners\n"); goto failed; }
            int length=snprintf(t->owner_fixture,sizeof(t->owner_fixture),"%s",absolute); free(absolute);
            if (length<0 || (size_t)length>=sizeof(t->owner_fixture)) goto failed;
            length=snprintf(t->owner_copy,sizeof(t->owner_copy),"%s.native-owner-roundtrip.dxls",t->owner_fixture);
            if (length<0 || (size_t)length>=sizeof(t->owner_copy) || FileExists(t->owner_copy) || DirectoryExists(t->owner_copy)) { fprintf(stderr,"Owner lifetime review refuses an existing or oversized copy destination\n"); goto failed; }
            workspace_job(a,JOB_LOAD,t->owner_fixture); if (!a->io_busy || a->pending_kind!=JOB_LOAD) goto failed; return 1;
        }
        case 1:
            if (a->book.active || !a->book.path || strcmp(a->book.path,t->owner_fixture) || !review_smoke_owner_model(a,1,UINT64_C(9007199254740994),true)) goto failed;
            a->selected_object=0; remove_pivot(a);
            if (!review_smoke_owner_model(a,0,UINT64_C(9007199254740994),false)) goto failed;
            grid_select_cell(&a->grid,0,8,false); if (!import_table(a,"tests/fixtures/transactions.csv") || !review_smoke_owner_model(a,1,UINT64_C(9007199254740995),false)) goto failed; return 1;
        case 2:
            if (!review_smoke_owner_model(a,1,UINT64_C(9007199254740995),false)) goto failed;
            if (!object_ready(a,0)) return 0;
            if (!review_smoke_owner_table(a)) goto failed;
            a->selected_object=0; create_pivot(a);
            if (a->selected_object!=1 || !review_smoke_owner_model(a,2,UINT64_C(9007199254740996),false)) goto failed; return 1;
        case 3:
            if (!review_smoke_owner_model(a,2,UINT64_C(9007199254740996),false) || !review_smoke_owner_table(a) || a->selected_object!=1) goto failed;
            remove_pivot(a);
            if (!review_smoke_owner_model(a,1,UINT64_C(9007199254740996),false)) goto failed;
            workspace_job(a,JOB_SAVE,t->owner_copy); if (!a->io_busy || a->pending_kind!=JOB_SAVE) goto failed; return 1;
        case 4:
            if (!a->book.path || strcmp(a->book.path,t->owner_copy) || a->book.dirty || !FileExists(t->owner_copy) || !review_smoke_owner_model(a,1,UINT64_C(9007199254740996),false)) goto failed;
            workspace_job(a,JOB_LOAD,t->owner_copy); if (!a->io_busy || a->pending_kind!=JOB_LOAD) goto failed; return 1;
        case 5:
            if (a->book.active || !a->book.path || strcmp(a->book.path,t->owner_copy) || a->book.dirty || !review_smoke_owner_model(a,1,UINT64_C(9007199254740996),false)) goto failed;
            if (!object_ready(a,0)) return 0;
            if (!review_smoke_owner_table(a)) goto failed;
            a->selected_object=0; create_pivot(a);
            if (a->selected_object!=1 || !review_smoke_owner_model(a,2,UINT64_C(9007199254740997),false)) goto failed; return 1;
        case 6:
            if (!review_smoke_owner_model(a,2,UINT64_C(9007199254740997),false) || !review_smoke_owner_table(a)) goto failed;
            activate_sheet(a,1,true); if (a->book.active!=1) goto failed; return 1;
        case 7: {
            if (a->book.active!=1 || !review_smoke_owner_model(a,2,UINT64_C(9007199254740997),false)) goto failed;
            uint64_t request=a->request_id,jobs,queries,counts,prepares; size_t history=active_sheet(a)->undo_actions_count;
            dx_lock(&a->worker.mutex); jobs=a->worker.jobs_started; queries=a->worker.db.query_count; counts=a->worker.db.count_queries; prepares=a->worker.db.relation_prepares; dx_unlock(&a->worker.mutex);
            grid_select_cell(&a->grid,0,8,false); bool accepted=import_table(a,"tests/fixtures/transactions.csv");
            dx_lock(&a->worker.mutex); bool same=jobs==a->worker.jobs_started && queries==a->worker.db.query_count && counts==a->worker.db.count_queries && prepares==a->worker.db.relation_prepares; dx_unlock(&a->worker.mutex);
            if (accepted || !same || a->request_id!=request || active_sheet(a)->undo_actions_count!=history || !a->message || !strstr(a->message,"exhausted its object IDs") || !review_smoke_owner_model(a,2,UINT64_C(9007199254740997),false)) goto failed;
            activate_sheet(a,0,true); if (a->book.active) goto failed; return 1;
        }
        case 8:
            if (!review_smoke_owner_model(a,2,UINT64_C(9007199254740997),false)) goto failed;
            if (!object_ready(a,0)) return 0;
            if (!review_smoke_owner_table(a)) goto failed;
            a->selected_object=0; show_workspace(a); a->matrix_cells_active=false; grid_select_cell(&a->grid,2,9,false); watch_statistics(a);
            /* Finish this selection through the real scheduler before opening
               the inspector, which pauses statistics scheduling. */
            if (a->statistics_state==STATS_WAITING || a->statistics_state==STATS_CALCULATING) return 0;
            inspect_value(a); if (a->modal!=MODAL_INSPECT) goto failed; return 1;
        }
    }
    if (phase==9) {
        if (a->modal!=MODAL_INSPECT) goto failed;
        if (r->phase!=DX_REPORT_IDLE || !smoke_work_idle(a) || !review_can_capture(a) || a->statistics_state==STATS_WAITING || a->statistics_state==STATS_CALCULATING) return 0;
        review_smoke_inspector_remember(a,t); ui_note_key_press(KEY_PERIOD,DX_MOD_SUPER|DX_MOD_SHIFT); return 1;
    }
    if (phase==10) {
        if (r->phase!=DX_REPORT_PICK) return 0;
        if (!review_smoke_owner_target(a,t)) goto failed;
        r->toolbar_top=true; review_smoke_pick(a,t->author_target); return 1;
    }
    if (phase==11) {
        if (r->phase!=DX_REPORT_NOTE) return 0;
        if (strcmp(r->target.widget.id,t->author_target) || !review_smoke_inspector_same(a,t,false) || r->target.widget.data.object_id!=UINT64_C(9007199254740995) || r->target.widget.data.source_id!=UINT64_C(9007199254740995)) goto failed;
        ui_note_key_press(KEY_ESCAPE,0); return 1;
    }
    if (r->phase!=DX_REPORT_IDLE) return 0;
    if (phase==12) { if (!review_smoke_inspector_same(a,t,false)) goto failed; ui_note_key_press(KEY_ESCAPE,0); return 1; }
    t->choice_modal=MODAL_NONE;
    if (!review_smoke_inspector_same(a,t,true) || !review_smoke_owner_model(a,2,UINT64_C(9007199254740997),false) || !review_smoke_owner_table(a)) goto failed;
    review_smoke_author_release(t);
    puts("PASS: native owner lifetime load/delete/import/bind/delete/save/reopen retains retired uint64 IDs above2^53, exact sparse cells/spatial formats, exhausted and never-allocated sheets; exhausted import adds no object/query; reopened Table inspector capture/cancel retains exact owner and Pages without clipboard/panels");
    return 1;
failed:
    fprintf(stderr,"Review owner lifetime mismatch stage=%d phase=%d modal=%d active=%zu sheets=%zu objects=%zu high_water=%llu path=%s message=%s\n",t->stage,phase,(int)a->modal,a->book.active,a->book.count,active_sheet(a) ? active_sheet(a)->objects.count : 0,(unsigned long long)(active_sheet(a) ? active_sheet(a)->objects.next_id : 0),a->book.path ? a->book.path : "",a->message ? a->message : "");
    for (size_t i=0;i<a->book.count && i<3;i++) { const Worksheet *sheet=&a->book.sheets[i]; fprintf(stderr,"Review owner sheet=%zu kind=%d objects=%zu next=%llu cells=%zu formats=%zu/%zu\n",i,(int)sheet->analysis.kind,sheet->objects.count,(unsigned long long)sheet->objects.next_id,sheet->cells.count,sheet->formats.count,sheet->formats.scope_count); }
    fprintf(stderr,"Review owner Table cache ready=%d id=%llu revision=%llu source=%llu dirty=%d failed=%d count=%llu/%zu columns=%zu statistics=%d enabled=%d\n",a->book.active==0 && object_ready(a,0),(unsigned long long)a->objects[0].id,(unsigned long long)a->objects[0].revision,(unsigned long long)a->objects[0].source_revision,a->objects[0].dirty,a->objects[0].failed,(unsigned long long)a->objects[0].page.total,a->objects[0].page.row_count,a->objects[0].page.column_count,(int)a->statistics_state,a->statistics_enabled);
    if (phase>=9) review_smoke_inspector_diagnostic(a,t,"owner lifetime"); return -1;
}
static const TextLines *review_smoke_guide_lines(const UIGuide *guide,size_t index) {
    return index<5 ? &guide->names[index] : index<10 ? &guide->descriptions[index-5] : index<16 ? &guide->labels[index-10] : index==16 ? &guide->intro : &guide->note;
}
static void review_smoke_guide_remember(App *a,ReviewSmoke *t) {
    review_smoke_stats_remember(a,t); t->guide_layout=a->guide.layout; t->guide_scroll=a->guide.scroll; t->guide_focus=a->guide.focus; t->guide_scrollbar=a->guide.scrollbar; t->guide_button_focus=a->guide_button_focus;
    for (size_t i=0;i<18;i++) {
        const TextLines *lines=review_smoke_guide_lines(&a->guide,i); t->guide_lines[i]=*lines;
        if (lines->offsets) { size_t bytes=(lines->count+1)*sizeof(size_t); t->guide_offsets[i]=dx_alloc(bytes); memcpy(t->guide_offsets[i],lines->offsets,bytes); }
    }
}
static bool review_smoke_guide_rect(Rectangle a,Rectangle b) { return a.x==b.x && a.y==b.y && a.width==b.width && a.height==b.height; }
static bool review_smoke_guide_same(App *a,ReviewSmoke *t,bool closed) {
    const UIGuideLayout *layout=&a->guide.layout,*old=&t->guide_layout;
    if (!review_smoke_stats_same(a,t) || a->guide.scroll!=t->guide_scroll || a->guide.focus!=t->guide_focus || a->guide.scrollbar.dragging!=t->guide_scrollbar.dragging || a->guide.scrollbar.grab!=t->guide_scrollbar.grab || a->guide_button_focus!=(closed ? true : t->guide_button_focus) || layout->valid!=old->valid || layout->content_height!=old->content_height) return false;
    const Rectangle current[]={layout->panel,layout->clip,layout->header,layout->close,layout->intro,layout->shortcuts,layout->note,layout->start},previous[]={old->panel,old->clip,old->header,old->close,old->intro,old->shortcuts,old->note,old->start};
    for (size_t i=0;i<sizeof(current)/sizeof(*current);i++) if (!review_smoke_guide_rect(current[i],previous[i])) return false;
    for (size_t i=0;i<5;i++) if (!review_smoke_guide_rect(layout->names[i],old->names[i]) || !review_smoke_guide_rect(layout->descriptions[i],old->descriptions[i])) return false;
    for (size_t i=0;i<6;i++) if (!review_smoke_guide_rect(layout->labels[i],old->labels[i]) || !review_smoke_guide_rect(layout->keys[i],old->keys[i])) return false;
    for (size_t i=0;i<18;i++) {
        const TextLines *lines=review_smoke_guide_lines(&a->guide,i),*before=&t->guide_lines[i];
        if (lines->offsets!=before->offsets || lines->count!=before->count || lines->capacity!=before->capacity || lines->width!=before->width || lines->scale!=before->scale || lines->font_size!=before->font_size || lines->raster_size!=before->raster_size || lines->limited!=before->limited || (lines->offsets && memcmp(lines->offsets,t->guide_offsets[i],(lines->count+1)*sizeof(size_t)))) return false;
    }
    return true;
}
static bool review_smoke_guide_targets(App *a,ReviewSmoke *t,int item) {
    const GuideReviewFrame *frame=&a->guide.review; DxReviewRegistry *registry=a->review->registry;
    const DxReviewWidget *panel=dx_review_find(registry,"author.guide"),*close=dx_review_find(registry,"author.guide.action.close"),*start=dx_review_find(registry,"author.guide.action.start"),*track=dx_review_find(registry,"author.guide.scrollbar"),*thumb=dx_review_find(registry,"author.guide.scrollbar.thumb");
    if (!panel || !panel->container || strcmp(panel->role,"dialog") || panel->input_eligible || frame->overflow || !frame->count || frame->count>GUIDE_REVIEW_TARGETS || registry->duplicate_count || registry->invalid_count || registry->overflow_count || registry->truncated || !strstr(a->review->frame.state_text," guide_overflow=0 ") || !strstr(a->review->frame.state_text," guide_kind=guide-document guide_origin=0,0,0,0,0,0,0,0")) return false;
    char document[128]; review_document_id(a,document); size_t registered=0,texts=0,shortcuts=0;
    for (size_t i=0;i<registry->count;i++) {
        const DxReviewWidget *widget=&registry->widgets[i]; if (strncmp(widget->id,"author.guide",strlen("author.guide"))) continue; registered++;
        const DxReviewDataIdentity *data=&widget->data;
        if (!data->present || strcmp(data->kind,"guide-document") || strcmp(data->document_id,document) || strcmp(data->document_id,registry->frame.document_id) || data->sheet_id || data->object_id || data->source_id || data->schema_generation || data->query_generation || data->has_cell || data->row || data->column || data->field[0]) return false;
    }
    if (registered!=frame->count+3) return false;
    for (size_t i=0;i<frame->count;i++) {
        const GuideReviewTarget *target=&frame->targets[i]; char id[128]; if (!review_guide_id(target,id)) return false;
        const DxReviewWidget *widget=dx_review_find(registry,id); Rectangle visible=GetCollisionRec(target->bounds,target->clip);
        if (!widget || target->truncated || visible.width<=0 || visible.height<=0 || widget->bounds.x!=target->bounds.x || widget->bounds.y!=target->bounds.y || widget->bounds.width!=target->bounds.width || widget->bounds.height!=target->bounds.height || widget->clip.x!=target->clip.x || widget->clip.y!=target->clip.y || widget->clip.width!=target->clip.width || widget->clip.height!=target->clip.height || widget->enabled!=target->enabled || widget->focused!=target->focused || !strstr(widget->source_file,"src/ui/guideview.c") || !widget->source_line) return false;
        if (!strcmp(widget->role,"text")) {
            texts++; if (widget->input_eligible || !widget->value.present || widget->value.is_null || widget->value.truncated || strcmp(widget->value.type,"VARCHAR") || strcmp(widget->value.exact_text,target->text)) return false;
        }
        if (target->kind==GUIDE_REVIEW_KEY) { shortcuts++; if (strncmp(widget->id,"author.guide.shortcut.",strlen("author.guide.shortcut."))) return false; }
    }
    if (!texts || !shortcuts || !start || !start->enabled || !start->input_eligible || strcmp(start->action_id,"guide.close") || start->focused!=(item!=0) || !review_smoke_sheet_pick(a,start,"author.guide")) return false;
    if (item) {
        if (close || dx_review_find(registry,"author.guide.heading") || !track || !thumb || !track->enabled || !thumb->enabled || !thumb->input_eligible || thumb->focused || frame->scroll<=0 || frame->scroll!=frame->maximum || t->guide_focus!=1 || !review_smoke_sheet_pick(a,thumb,"author.guide")) return false;
        snprintf(t->author_target,sizeof(t->author_target),"%s",thumb->id);
    } else {
        if (!close || !close->enabled || !close->input_eligible || !close->focused || strcmp(close->action_id,"guide.close") || track || thumb || frame->scroll || t->guide_focus || !review_smoke_sheet_pick(a,close,"author.guide") || !dx_review_find(registry,"author.guide.section.grid.name.line0") || !dx_review_find(registry,"author.guide.shortcut.selection.key.line0")) return false;
        snprintf(t->author_target,sizeof(t->author_target),"%s",close->id);
    }
    DxReviewTarget picked; return dx_review_pick(registry,2*a->ui.scale,2*a->ui.scale,DX_REVIEW_PICK_CONTROL,&picked) && !strcmp(picked.widget.id,"author.guide.backdrop.background");
}
static int review_smoke_guide_tick(App *a,ReviewSmoke *t) {
    int item=(t->stage-REVIEW_OWNER_END)/6,phase=(t->stage-REVIEW_OWNER_END)%6; DxReviewCapture *r=a->review;
    if (!phase) {
        if (r->phase!=DX_REPORT_IDLE || !smoke_work_idle(a)) return 0;
        a->selected_object=1; if (!selected_pivot(a) || !review_smoke_owner_table(a)) goto failed;
        ui_set_scale(&a->ui,item ? 1.5f : 1); SetWindowSize(item ? 640 : 1280,item ? 400 : 820); run_action(a,ACTION_GUIDE);
        t->guide_navigation_sent=false; return a->modal==MODAL_GUIDE ? 1 : -1;
    }
    if (phase==1) {
        if (a->modal!=MODAL_GUIDE) goto failed;
        if (r->phase!=DX_REPORT_IDLE || !smoke_work_idle(a) || !review_can_capture(a) || GetScreenWidth()!=(item ? 640 : 1280) || GetScreenHeight()!=(item ? 400 : 820) || a->ui.scale!=(item ? 1.5f : 1) || !a->guide.layout.valid || !a->guide.intro.offsets || a->guide.intro.scale!=a->ui.scale || a->guide.intro.raster_size!=a->ui.raster_size) return 0;
        if (item && !t->guide_navigation_sent) { t->guide_navigation_sent=true; ui_note_key_press(KEY_END,0); ui_note_key_press(KEY_TAB,0); return 0; }
        if (item ? a->guide.scroll<=0 || a->guide.focus!=1 : a->guide.scroll || a->guide.focus) goto failed;
        review_smoke_guide_remember(a,t); ui_note_key_press(KEY_PERIOD,DX_MOD_SUPER|DX_MOD_SHIFT); return 1;
    }
    if (phase==2) {
        if (r->phase!=DX_REPORT_PICK) return 0;
        if (!review_smoke_guide_same(a,t,false) || !review_smoke_guide_targets(a,t,item)) goto failed;
        r->toolbar_top=true; review_smoke_pick(a,t->author_target); return 1;
    }
    if (phase==3) {
        if (r->phase!=DX_REPORT_NOTE) return 0;
        if (strcmp(r->target.widget.id,t->author_target) || !review_smoke_guide_same(a,t,false)) goto failed;
        ui_note_key_press(KEY_ESCAPE,0); return 1;
    }
    if (r->phase!=DX_REPORT_IDLE) return 0;
    if (phase==4) { if (!review_smoke_guide_same(a,t,false)) goto failed; ui_note_key_press(KEY_ESCAPE,0); return 1; }
    t->choice_modal=MODAL_NONE; if (!review_smoke_guide_same(a,t,true)) goto failed;
    printf("PASS: native Guide review %s retains actual visible text/shortcuts, logical buttons, clipping, focus/scroll, all owned line caches, source/Pivot/Pages/statistics/jobs and document-only identity; normal Escape returns Help focus\n",item ? "short150% at real End/Tab footer" : "wide100% at top");
    review_smoke_author_release(t); if (item) { ui_set_scale(&a->ui,1); SetWindowSize(1280,820); } return 1;
failed:
    fprintf(stderr,"Review Guide mismatch stage=%d phase=%d modal=%d report=%d focus=%d/%d scroll=%.3f/%.3f maximum=%.3f records=%zu overflow=%d target=%s actual=%s retained=%d\n",t->stage,phase,(int)a->modal,(int)r->phase,a->guide.focus,t->guide_focus,a->guide.scroll,t->guide_scroll,a->guide.review.maximum,a->guide.review.count,a->guide.review.overflow,t->author_target,r->target.widget.id,phase>1 && review_smoke_guide_same(a,t,false));
    for (size_t i=0;i<r->registry->count;i++) { const DxReviewWidget *widget=&r->registry->widgets[i]; if (strncmp(widget->id,"author.guide",strlen("author.guide"))) continue; fprintf(stderr,"Review Guide widget %s role=%s enabled=%d focus=%d kind=%s object=%llu source=%llu value=%s bounds=%.2f,%.2f,%.2f,%.2f clip=%.2f,%.2f,%.2f,%.2f\n",widget->id,widget->role,widget->enabled,widget->focused,widget->data.kind,(unsigned long long)widget->data.object_id,(unsigned long long)widget->data.source_id,widget->value.exact_text,widget->bounds.x,widget->bounds.y,widget->bounds.width,widget->bounds.height,widget->clip.x,widget->clip.y,widget->clip.width,widget->clip.height); } return -1;
}
static const char review_cell_draft[]="İstanbul \"NULL\" — a long draft with quoted \"text\" and retained UTF-8: ç";
static bool review_smoke_cell_content(const CellStore *cells,const CellStore *before) {
    if (cells->count!=before->count || cells->bytes!=before->bytes) return false;
    for (size_t i=0;i<cells->count;i++) if (cells->cells[i].row!=before->cells[i].row || cells->cells[i].col!=before->cells[i].col || strcmp(cells->cells[i].text,before->cells[i].text)) return false;
    return true;
}
static void review_smoke_cell_remember(App *a,ReviewSmoke *t) {
    review_smoke_stats_remember(a,t); t->cell_input=a->input; t->cell_text=dx_strdup(a->input.text);
    for (size_t stack=0;stack<2;stack++) {
        const TextInputState *states=stack ? a->input.redo : a->input.undo; size_t count=stack ? a->input.redo_count : a->input.undo_count;
        for (size_t i=0;i<count;i++) t->cell_history[32*stack+i]=dx_strdup(states[i].text);
    }
    const Worksheet *sheet=active_sheet(a); t->cell_borrowed=sheet->cells; cells_copy(&t->cell_model,&sheet->cells);
    if (sheet->cells.count) { size_t bytes=sheet->cells.count*sizeof(SheetCell); t->cell_pointers=dx_alloc(bytes); memcpy(t->cell_pointers,sheet->cells.cells,bytes); }
    t->cell_worksheet_redo=sheet->redo_actions_count; t->cell_object_next=sheet->objects.next_id;
}
static bool review_smoke_cell_input_same(const TextInput *input,ReviewSmoke *t) {
    const TextInput *old=&t->cell_input;
    if (input->text!=old->text || strcmp(input->text,t->cell_text) || input->length!=old->length || input->capacity!=old->capacity || input->cursor!=old->cursor || input->anchor!=old->anchor || input->view_start!=old->view_start || input->select_all!=old->select_all || input->native_edit!=old->native_edit || input->dragging!=old->dragging || input->multiline!=old->multiline || input->revision!=old->revision || input->undo_count!=old->undo_count || input->redo_count!=old->redo_count || input->edit_kind!=old->edit_kind || input->edit_at!=old->edit_at || input->last_click!=old->last_click || input->click_x!=old->click_x || input->clicks!=old->clicks) return false;
    for (size_t stack=0;stack<2;stack++) {
        const TextInputState *states=stack ? input->redo : input->undo,*before=stack ? old->redo : old->undo; size_t count=stack ? input->redo_count : input->undo_count;
        for (size_t i=0;i<count;i++) if (states[i].text!=before[i].text || strcmp(states[i].text,t->cell_history[32*stack+i]) || states[i].length!=before[i].length || states[i].capacity!=before[i].capacity || states[i].cursor!=before[i].cursor || states[i].anchor!=before[i].anchor || states[i].view_start!=before[i].view_start || states[i].select_all!=before[i].select_all) return false;
    }
    return true;
}
static bool review_smoke_cell_same(App *a,ReviewSmoke *t) {
    const Worksheet *sheet=active_sheet(a); const CellStore *cells=&sheet->cells,*old=&t->cell_borrowed;
    if (!review_smoke_stats_same(a,t) || !review_smoke_cell_input_same(&a->input,t) || !review_smoke_cell_content(cells,&t->cell_model) || cells->cells!=old->cells || cells->capacity!=old->capacity || cells->revision!=old->revision || cells->undo_count!=old->undo_count || cells->redo_count!=old->redo_count || sheet->redo_actions_count!=t->cell_worksheet_redo || sheet->objects.next_id!=t->cell_object_next) return false;
    for (size_t i=0;i<cells->count;i++) if (cells->cells[i].text!=t->cell_pointers[i].text) return false;
    return true;
}
static bool review_smoke_cell_targets(App *a,ReviewSmoke *t,int item) {
    DxReviewRegistry *registry=a->review->registry; const DxReviewWidget *widget=dx_review_find(registry,"author.cell_editor.input");
    if (!widget || strcmp(widget->role,"textbox") || strcmp(widget->action_id,"cell.edit_draft") || strcmp(widget->path,"workspace/canvas/cell_editor") || !widget->enabled || !widget->input_eligible || !widget->focused || widget->container || !widget->value.present || widget->value.is_null || widget->value.truncated || strcmp(widget->value.type,"VARCHAR") || strcmp(widget->value.exact_text,review_cell_draft) || registry->duplicate_count || registry->invalid_count || registry->overflow_count || registry->truncated) return false;
    const DxReviewDataIdentity *data=&widget->data; char document[128]; review_document_id(a,document); Rectangle body=a->grid.body;
    if (!data->present || !data->has_cell || strcmp(data->kind,"manual-cell-draft") || strcmp(data->document_id,document) || strcmp(data->document_id,registry->frame.document_id) || data->sheet_id!=a->book.active+1 || data->object_id || data->source_id || data->schema_generation || data->query_generation!=t->cell_borrowed.revision || data->row!=2 || data->column!=(size_t)(1+item) || data->field[0] || widget->clip.x!=body.x || widget->clip.y!=body.y || widget->clip.width!=body.width || widget->clip.height!=body.height || !strstr(widget->source_file,"src/app/app.c") || !widget->source_line) return false;
    for (size_t i=0;i<registry->count;i++) if (!strncmp(registry->widgets[i].id,"author.cell_editor",strlen("author.cell_editor")) && &registry->widgets[i]!=widget) return false;
    if (!t->cell_input.view_start || !t->cell_input.undo_count || t->cell_input.select_all!=(item!=0) || (!item && (t->cell_input.cursor!=dx_utf8_prev(t->cell_input.text,t->cell_input.length) || t->cell_input.anchor!=t->cell_input.length))) return false;
    DxReviewTarget picked; Rectangle visible=GetCollisionRec((Rectangle){widget->bounds.x,widget->bounds.y,widget->bounds.width,widget->bounds.height},body);
    if (visible.width<=0 || visible.height<=0 || !dx_review_pick(registry,visible.x+visible.width*.5f,visible.y+visible.height*.5f,DX_REVIEW_PICK_CONTROL,&picked) || strcmp(picked.widget.id,widget->id)) return false;
    snprintf(t->author_target,sizeof(t->author_target),"%s",widget->id); return true;
}
static bool review_smoke_cell_pages(App *a,ReviewSmoke *t) {
    const ObjectStore *objects=&active_sheet(a)->objects;
    if (!analysis_same_result(&a->analysis,&t->author_source) || !analysis_sorts_equal(&a->analysis,&t->author_source) || objects->count!=t->author_count || objects->next_id!=t->cell_object_next || a->page.cells!=t->page) return false;
    for (size_t i=0;i<objects->count;i++) {
        const SheetObject *object=&objects->items[i]; const Page *page=&a->objects[i].page;
        if (page->cells!=t->query_views[i].cells || object->id!=t->query_views[i].id || object->revision!=t->query_views[i].revision || object->source_id!=t->query_views[i].source_id || page->total!=t->query_views[i].total || page->row_count!=t->query_views[i].rows || page->column_count!=t->query_views[i].columns) return false;
    }
    return true;
}
static int review_smoke_cell_tick(App *a,ReviewSmoke *t) {
    int item=(t->stage-REVIEW_GUIDE_END)/6,phase=(t->stage-REVIEW_GUIDE_END)%6; DxReviewCapture *r=a->review;
    if (!phase) {
        if (r->phase!=DX_REPORT_IDLE || !smoke_work_idle(a)) return 0;
        if (!t->cell_setup_started || t->cell_setup_item!=item) {
            t->cell_setup_started=true; t->cell_setup_item=item; t->cell_clicked=t->cell_f2_sent=t->cell_inserted=t->cell_undo_sent=false;
            ui_set_scale(&a->ui,item ? 1.5f : 1); SetWindowSize(item ? 760 : 1280,item ? 640 : 820); browse_source(a); close_workspace(a); present_pipeline(a,false,false); grid_select_cell(&a->grid,2,1+item,false); keyboard_object(a); grid_reveal_cell(&a->grid,&a->page,a->ui.scale*a->grid.zoom); return 0;
        }
        if (a->modal!=MODAL_NONE) goto failed;
        if (GetScreenWidth()!=(item ? 760 : 1280) || GetScreenHeight()!=(item ? 640 : 820) || a->ui.scale!=(item ? 1.5f : 1)) return 0;
        if (!t->cell_clicked) {
            grid_reveal_cell(&a->grid,&a->page,a->ui.scale*a->grid.zoom);
            Rectangle cell=grid_range_rect(&a->grid,a->ui.scale*a->grid.zoom,2,1+item,1,1),visible=GetCollisionRec(cell,a->grid.body);
            if (visible.width<=0 || visible.height<=0) return 0;
            review_smoke_click(visible); t->cell_clicked=true; return 0;
        }
        if (!manual_cell(a) || a->grid.row!=2 || a->grid.col!=(size_t)(1+item) || *cells_get(&active_sheet(a)->cells,2,1+item)) goto failed;
        ui_note_key_press(KEY_F2,0); t->cell_f2_sent=true; return 1;
    }
    if (phase==1) {
        if (a->modal!=MODAL_CELL) goto failed;
        if (r->phase!=DX_REPORT_IDLE || !smoke_work_idle(a) || !review_can_capture(a)) return 0;
        if (!t->cell_inserted) {
            if (!t->cell_f2_sent || a->input.length) goto failed;
            input_insert(&a->input,"İstanbul \"NULL\" — "); input_insert(&a->input,"a long draft with quoted \"text\" and retained UTF-8: ç");
            ui_note_key_press(item ? KEY_A : KEY_LEFT,item ? DX_MOD_SUPER : DX_MOD_SHIFT); t->cell_inserted=true; return 0;
        }
        if (strcmp(a->input.text,review_cell_draft) || !a->input.view_start || *cells_get(&active_sheet(a)->cells,2,1+item)) goto failed;
        review_smoke_cell_remember(a,t); ui_note_key_press(KEY_PERIOD,DX_MOD_SUPER|DX_MOD_SHIFT); return 1;
    }
    if (phase==2) {
        if (r->phase!=DX_REPORT_PICK) return 0;
        if (!review_smoke_cell_same(a,t) || !review_smoke_cell_targets(a,t,item)) goto failed;
        r->toolbar_top=true; review_smoke_pick(a,t->author_target); return 1;
    }
    if (phase==3) {
        if (r->phase!=DX_REPORT_NOTE) return 0;
        if (strcmp(r->target.widget.id,t->author_target) || !review_smoke_cell_same(a,t)) goto failed;
        ui_note_key_press(KEY_ESCAPE,0); return 1;
    }
    if (r->phase!=DX_REPORT_IDLE) return 0;
    if (phase==4) { if (!review_smoke_cell_same(a,t)) goto failed; ui_note_key_press(item ? KEY_ENTER : KEY_ESCAPE,0); return 1; }
    t->choice_modal=MODAL_NONE;
    if (!item) { if (!review_smoke_cell_same(a,t)) goto failed; }
    else {
        const Worksheet *sheet=active_sheet(a); const CellStore *cells=&sheet->cells,*before=&t->cell_borrowed;
        if (a->modal!=MODAL_NONE || !review_smoke_cell_pages(a,t)) goto failed;
        if (!t->cell_undo_sent) {
            if (strcmp(cells_get(cells,2,2),review_cell_draft) || cells->count!=before->count+1 || cells->revision!=before->revision+1 || cells->undo_count!=before->undo_count+1 || cells->redo_count || sheet->undo_actions_count!=t->history+1 || sheet->redo_actions_count || sheet->undo_actions[sheet->undo_actions_count-1]!=EDIT_CELLS || !a->book.dirty) goto failed;
            ui_note_key_press(KEY_Z,DX_MOD_SUPER); t->cell_undo_sent=true; return 0;
        }
        if (!review_smoke_cell_content(cells,&t->cell_model) || cells->revision!=before->revision+2 || cells->undo_count!=before->undo_count || cells->redo_count!=1 || sheet->undo_actions_count!=t->history || sheet->redo_actions_count!=1 || sheet->redo_actions[0]!=EDIT_CELLS) goto failed;
    }
    printf("PASS: native inline cell editor %s uses normal blank-cell click/F2, UTF8 quoted editing primitive and queued navigation; capture/pick/cancel retains draft/selection/hscroll/owned histories/model/Pages/jobs with manual-cell scope; %s\n",item ? "150% select-all" : "100% partial UTF8 selection",item ? "normal Enter creates one cell Undo action; normal Undo restores sparse NULL" : "normal Escape leaves sparse NULL unchanged");
    review_smoke_author_release(t); if (item) { ui_set_scale(&a->ui,1); SetWindowSize(1280,820); } return 1;
failed:
    fprintf(stderr,"Review inline cell mismatch stage=%d phase=%d modal=%d report=%d grid=%llu,%zu input=%zu cursor=%zu anchor=%zu all=%d view=%zu revision=%llu history=%zu/%zu worksheet=%zu/%zu target=%s actual=%s\n",t->stage,phase,(int)a->modal,(int)r->phase,(unsigned long long)a->grid.row,a->grid.col,a->input.length,a->input.cursor,a->input.anchor,a->input.select_all,a->input.view_start,(unsigned long long)a->input.revision,a->input.undo_count,a->input.redo_count,active_sheet(a)->undo_actions_count,active_sheet(a)->redo_actions_count,t->author_target,r->target.widget.id);
    const DxReviewWidget *widget=dx_review_find(r->registry,"author.cell_editor.input");
    if (widget) fprintf(stderr,"Review inline widget role=%s path=%s kind=%s sheet=%llu owner=%llu source=%llu schema=%llu revision=%llu cell=%llu,%llu value=%d/%d/%s/%s bounds=%.2f,%.2f,%.2f,%.2f clip=%.2f,%.2f,%.2f,%.2f\n",widget->role,widget->path,widget->data.kind,(unsigned long long)widget->data.sheet_id,(unsigned long long)widget->data.object_id,(unsigned long long)widget->data.source_id,(unsigned long long)widget->data.schema_generation,(unsigned long long)widget->data.query_generation,(unsigned long long)widget->data.row,(unsigned long long)widget->data.column,widget->value.present,widget->value.is_null,widget->value.type,widget->value.exact_text,widget->bounds.x,widget->bounds.y,widget->bounds.width,widget->bounds.height,widget->clip.x,widget->clip.y,widget->clip.width,widget->clip.height);
    return -1;
}
static bool review_smoke_error_pages(App *a,ReviewSmoke *t) {
    const ObjectStore *objects=&active_sheet(a)->objects;
    if (!review_smoke_author_models_same(a,t) || objects->next_id!=t->error_object_next || a->page.cells!=t->page) return false;
    for (size_t i=0;i<objects->count;i++) {
        const SheetObject *object=&objects->items[i]; const Page *page=&a->objects[i].page;
        if (object->id!=t->query_views[i].id || object->revision!=t->query_views[i].revision || object->source_id!=t->query_views[i].source_id || page->cells!=t->query_views[i].cells || page->total!=t->query_views[i].total || page->row_count!=t->query_views[i].rows || page->column_count!=t->query_views[i].columns) return false;
    }
    return true;
}
static bool review_smoke_error_same(App *a,ReviewSmoke *t) {
    return review_smoke_stats_same(a,t) && a->message==t->error_pointer && !strcmp(a->message,t->error_text) && a->message_since==t->error_since && a->message_revision==t->error_status_counter &&
        a->request_id==t->error_request && a->pending_kind==JOB_SAVE && !a->busy && !a->io_busy && !a->fetch_failed && a->document_action==DOCUMENT_NONE && !a->document_saving &&
        a->book.count==t->error_sheets && a->book.active==t->error_active && a->book.path && !strcmp(a->book.path,t->error_book_path) && active_sheet(a)->objects.next_id==t->error_object_next;
}
static bool review_smoke_error_targets(App *a,ReviewSmoke *t,int item) {
    const char *parent="author.error"; DxReviewRegistry *registry=a->review->registry;
    const DxReviewWidget *panel=dx_review_find(registry,parent),*heading=dx_review_find(registry,"author.error.heading"),*content=dx_review_find(registry,"author.error.content"),*relink=dx_review_find(registry,"author.error.action.relink");
    if (!panel || !panel->container || strcmp(panel->role,"dialog") || panel->input_eligible || !heading || heading->input_eligible || !review_smoke_sheet_pick(a,heading,parent) || !content || strcmp(content->role,"region") || content->input_eligible || !review_smoke_sheet_pick(a,content,parent) || !content->value.present || content->value.is_null || content->value.truncated || strcmp(content->value.type,"VARCHAR") || strcmp(content->value.exact_text,t->error_text) || registry->duplicate_count || registry->invalid_count || registry->overflow_count || registry->truncated) return false;
    char document[128]; review_document_id(a,document);
    for (size_t i=0;i<registry->count;i++) {
        const DxReviewWidget *widget=&registry->widgets[i]; if (strncmp(widget->id,parent,strlen(parent))) continue;
        const DxReviewDataIdentity *data=&widget->data;
        if (!data->present || strcmp(data->kind,"document-error") || strcmp(data->document_id,document) || strcmp(data->document_id,registry->frame.document_id) || data->sheet_id || data->object_id || data->source_id || data->schema_generation || data->query_generation || data->has_cell || data->row || data->column || data->field[0]) return false;
    }
    const char *keys[]={"x_close","copy_error","close"};
    for (size_t i=0;i<3;i++) { char id[128]; snprintf(id,sizeof(id),"%s.action.%s",parent,keys[i]); const DxReviewWidget *widget=dx_review_find(registry,id); if (!widget || !widget->enabled || !widget->input_eligible || widget->focused || widget->checked || strcmp(widget->role,"button") || !review_smoke_sheet_pick(a,widget,parent)) return false; }
    if (item ? !relink || !relink->enabled || !relink->input_eligible || !review_smoke_sheet_pick(a,relink,parent) : relink!=NULL) return false;
    const char *lines=strstr(a->review->frame.state_text," error_lines="),*clipped=strstr(a->review->frame.state_text," error_clipped=");
    if (!lines || !strtoul(lines+13,NULL,10) || !clipped || (strtol(clipped+15,NULL,10)!=0)!=(item!=0) || !strstr(a->review->frame.state_text," error_message_since=") || !strstr(a->review->frame.state_text," error_value_truncated=0")) return false;
    DxReviewTarget picked; if (!dx_review_pick(registry,2*a->ui.scale,2*a->ui.scale,DX_REVIEW_PICK_CONTROL,&picked) || strcmp(picked.widget.id,"author.error.backdrop.background")) return false;
    snprintf(t->author_target,sizeof(t->author_target),"%s",item ? relink->id : content->id); return true;
}
static int review_smoke_error_tick(App *a,ReviewSmoke *t) {
    int item=(t->stage-REVIEW_CELL_END)/6,phase=(t->stage-REVIEW_CELL_END)%6; DxReviewCapture *r=a->review;
    if (!phase) {
        if (r->phase!=DX_REPORT_IDLE || !smoke_work_idle(a)) return 0;
        if (!t->error_setup_started || t->error_setup_item!=item) {
            t->error_setup_started=true; t->error_setup_item=item; t->error_sheets=a->book.count+(item ? 1 : 0);
            ui_set_scale(&a->ui,item ? 1.5f : 1); SetWindowSize(item ? 640 : 1280,item ? 400 : 820);
            if (item) open_source(a,"tests/fixtures/transactions.csv",false); else a->selected_object=1;
            return 0;
        }
        if (a->modal!=MODAL_NONE || a->book.count!=t->error_sheets) goto failed;
        if (GetScreenWidth()!=(item ? 640 : 1280) || GetScreenHeight()!=(item ? 400 : 820) || a->ui.scale!=(item ? 1.5f : 1) || a->statistics_state==STATS_WAITING || a->statistics_state==STATS_CALCULATING) return 0;
        if (item ? a->analysis.kind!=SOURCE_CSV || a->page.column_count!=6 || a->page.total!=5 || a->page.count_pending : a->analysis.kind!=SOURCE_NONE || !selected_pivot(a)) goto failed;
        char suffix[129]; memset(suffix,'x',sizeof(suffix)-1); suffix[sizeof(suffix)-1]=0;
        size_t directory_length=(size_t)(dx_basename(t->owner_fixture)-t->owner_fixture);
        int length=snprintf(t->error_directory,sizeof(t->error_directory),"%.*snative-review-error-%s%s.dxls",(int)directory_length,t->owner_fixture,item ? "short-" : "wide",item ? suffix : "");
        if (!directory_length || length<0 || (size_t)length>=sizeof(t->error_directory) || FileExists(t->error_directory) || DirectoryExists(t->error_directory) || MakeDirectory(t->error_directory)!=0 || !DirectoryExists(t->error_directory) || !a->book.path) goto failed;
        length=snprintf(t->error_book_path,sizeof(t->error_book_path),"%s",a->book.path); if (length<0 || (size_t)length>=sizeof(t->error_book_path)) goto failed;
        review_smoke_stats_remember(a,t); t->error_active=a->book.active; t->error_object_next=active_sheet(a)->objects.next_id;
        dx_lock(&a->worker.mutex); t->error_jobs=a->worker.jobs_started; dx_unlock(&a->worker.mutex);
        workspace_job(a,JOB_SAVE,t->error_directory); t->error_request=a->request_id;
        if (!a->busy || !a->io_busy || a->pending_kind!=JOB_SAVE) goto failed; return 1;
    }
    if (phase==1) {
        if (a->busy || a->io_busy || !smoke_work_idle(a)) return 0;
        dx_lock(&a->worker.mutex); bool one_job=a->worker.jobs_started==t->error_jobs+1; dx_unlock(&a->worker.mutex);
        if (a->modal!=MODAL_ERROR || !one_job || a->request_id!=t->error_request || !a->message || !*a->message || strlen(a->message)>=sizeof(((DxReviewValue *)0)->exact_text) || !a->book.path || strcmp(a->book.path,t->error_book_path) || a->book.dirty!=t->dirty || active_sheet(a)->undo_actions_count!=t->history || !review_smoke_error_pages(a,t) || !DirectoryExists(t->error_directory)) goto failed;
        if (!review_can_capture(a)) return 0;
        review_smoke_stats_remember(a,t); t->error_text=dx_strdup(a->message); t->error_pointer=a->message; t->error_since=a->message_since; t->error_status_counter=a->message_revision;
        ui_note_key_press(KEY_PERIOD,DX_MOD_SUPER|DX_MOD_SHIFT); return 1;
    }
    if (phase==2) {
        if (r->phase!=DX_REPORT_PICK) return 0;
        if (!review_smoke_error_same(a,t) || !review_smoke_error_targets(a,t,item)) goto failed;
        r->toolbar_top=true; review_smoke_pick(a,t->author_target); return 1;
    }
    if (phase==3) {
        if (r->phase!=DX_REPORT_NOTE) return 0;
        if (strcmp(r->target.widget.id,t->author_target) || !review_smoke_error_same(a,t)) goto failed;
        ui_note_key_press(KEY_ESCAPE,0); return 1;
    }
    if (r->phase!=DX_REPORT_IDLE) return 0;
    if (phase==4) { if (!review_smoke_error_same(a,t)) goto failed; ui_note_key_press(item ? KEY_ESCAPE : KEY_ENTER,0); return 1; }
    t->choice_modal=MODAL_NONE; if (!review_smoke_error_same(a,t) || !DirectoryExists(t->error_directory)) goto failed;
    printf("PASS: native Error review after actual failed workspace Save retains owned message/timestamp, source/model/Pages/dirty/history/io/jobs and document-only scope; %s; normal %s preserves error context without clipboard or OS panels\n",item ? "CSV short150% exposes real Relink and clipped text" : "SOURCE_NONE wide has no hidden Relink despite selected Pivot",item ? "Escape" : "Close/Enter");
    review_smoke_author_release(t); if (item) { ui_set_scale(&a->ui,1); SetWindowSize(1280,820); } return 1;
failed:
    fprintf(stderr,"Review Error mismatch stage=%d phase=%d modal=%d report=%d source=%d sheets=%zu/%zu active=%zu message=%s request=%llu/%llu io=%d busy=%d target=%s actual=%s\n",t->stage,phase,(int)a->modal,(int)r->phase,(int)a->analysis.kind,a->book.count,t->error_sheets,a->book.active,a->message ? a->message : "",(unsigned long long)a->request_id,(unsigned long long)t->error_request,a->io_busy,a->busy,t->author_target,r->target.widget.id);
    fprintf(stderr,"Review Error state %s\n",r->frame.state_text); return -1;
}
static const char review_query_invalid_draft[]="SELECT duxcel_missing_review_column,\n       City\nFROM input";
static bool review_smoke_query_failure_same(App *a,ReviewSmoke *t,bool closed) {
    return review_smoke_query_same(a,t,closed) && review_smoke_cell_same(a,t) && !memcmp(&a->query_editor,&t->query_editor,sizeof(TextEditor)) &&
        a->pending_kind==JOB_VALIDATE_SQL && !a->fetch_failed && a->document_action==DOCUMENT_NONE && !a->document_saving;
}
static bool review_smoke_query_failure_targets(App *a,ReviewSmoke *t) {
    if (!review_smoke_query_targets(a,t) || !t->query_error_text || !strstr(t->query_error_text,"duxcel_missing_review_column") || !t->query_error_lines.offsets || t->query_error_lines.count<2 || t->query_error_lines.limited || t->query_error_scroll) return false;
    const char *state=a->review->frame.state_text,*lines=strstr(state," query_error_lines="),*bytes=strstr(state," query_error_bytes=");
    if (!lines || strtoull(lines+19,NULL,10)!=t->query_error_lines.count || !bytes || strtoull(bytes+19,NULL,10)!=strlen(t->query_error_text) || !strstr(state," query_error_scroll=0") || !strstr(state," query_error_limited=0") || !strstr(state," query_error_value_truncated=0")) return false;
    snprintf(t->author_target,sizeof(t->author_target),"author.source_query.validation_error"); return true;
}
static int review_smoke_query_failure_tick(App *a,ReviewSmoke *t) {
    int phase=t->stage-REVIEW_ERROR_END; DxReviewCapture *r=a->review;
    if (!phase) {
        if (r->phase!=DX_REPORT_IDLE || !smoke_work_idle(a)) return 0;
        if (!t->query_failure_started) {
            if (a->statistics_state==STATS_WAITING || a->statistics_state==STATS_CALCULATING) return 0;
            if (a->modal!=MODAL_NONE || a->analysis.kind!=SOURCE_CSV || a->page.total!=5 || a->page.column_count!=6 || a->page.count_pending || a->selected_object!=-1) goto failed;
            edit_source_query(a); if (a->modal!=MODAL_SOURCE_QUERY || a->query_object!=-1) goto failed;
            input_set(&a->input,review_query_invalid_draft); a->input.select_all=false; a->input.cursor=34; a->input.anchor=7; a->query_edit_revision=a->input.revision;
            ui_set_scale(&a->ui,1.5f); SetWindowSize(760,640); t->query_failure_started=true; return 0;
        }
        if (a->modal!=MODAL_SOURCE_QUERY) goto failed;
        if (GetScreenWidth()!=760 || GetScreenHeight()!=640 || a->ui.scale!=1.5f || !review_can_capture(a) || a->query_editor.revision!=a->input.revision || a->statistics_state==STATS_WAITING || a->statistics_state==STATS_CALCULATING) return 0;
        review_smoke_cell_remember(a,t); t->query_failure_jobs=t->query_jobs; t->query_failure_sql=t->query_sql; t->query_failure_counts=t->query_counts; t->query_failure_prepares=t->query_prepares;
        submit_source_query(a); t->query_failure_request=a->request_id;
        if (!a->busy || !a->query_loading || a->pending_kind!=JOB_VALIDATE_SQL || a->query_error || a->query_object!=-1) goto failed; return 1;
    }
    if (phase==1) {
        if (r->phase!=DX_REPORT_IDLE || !smoke_work_idle(a)) return 0;
        if (a->modal!=MODAL_SOURCE_QUERY || !a->query_error || !strstr(a->query_error,"duxcel_missing_review_column") || strlen(a->query_error)>=sizeof(((DxReviewValue *)0)->exact_text) || a->query_validated || a->request_id!=t->query_failure_request ||
            !review_smoke_cell_pages(a,t) || !review_smoke_author_models_same(a,t) || !review_smoke_cell_content(&active_sheet(a)->cells,&t->cell_model) || a->book.dirty!=t->dirty || active_sheet(a)->undo_actions_count!=t->history || active_sheet(a)->redo_actions_count!=t->cell_worksheet_redo || active_sheet(a)->formats.revision!=t->choice_formats ||
            strcmp(a->input.text,review_query_invalid_draft) || a->input.cursor!=34 || a->input.anchor!=7 || a->input.revision!=t->cell_input.revision) goto failed;
        dx_lock(&a->worker.mutex); bool one_validation=a->worker.jobs_started==t->query_failure_jobs+1 && a->worker.latest_id==t->query_failure_request && a->worker.db.relation_prepares==t->query_failure_prepares+1 && a->worker.db.query_count==t->query_failure_sql && a->worker.db.count_queries==t->query_failure_counts; dx_unlock(&a->worker.mutex);
        if (!one_validation) goto failed;
        if (!a->query_errors.offsets || !a->query_errors.count || a->query_editor.revision!=a->input.revision || !review_can_capture(a)) return 0;
        review_smoke_cell_remember(a,t); review_smoke_query_state_remember(a,t,false);
        if (t->query_owner || t->query_source_revision!=a->analysis_revision) goto failed;
        ui_note_key_press(KEY_PERIOD,DX_MOD_SUPER|DX_MOD_SHIFT); return 1;
    }
    if (phase==2) {
        if (r->phase!=DX_REPORT_PICK) return 0;
        if (!review_smoke_query_failure_same(a,t,false) || !review_smoke_query_failure_targets(a,t)) goto failed;
        r->toolbar_top=true; review_smoke_pick(a,t->author_target); return 1;
    }
    if (phase==3) {
        if (r->phase!=DX_REPORT_NOTE) return 0;
        if (strcmp(r->target.widget.id,t->author_target) || !review_smoke_query_failure_same(a,t,false)) goto failed;
        ui_note_key_press(KEY_ESCAPE,0); return 1;
    }
    if (r->phase!=DX_REPORT_IDLE) return 0;
    if (phase==4) { if (!review_smoke_query_failure_same(a,t,false)) goto failed; ui_note_key_press(KEY_ESCAPE,0); return 1; }
    t->choice_modal=MODAL_NONE; if (!review_smoke_query_failure_same(a,t,true)) goto failed;
    printf("PASS: native Source Query missing-column validation produces one real prepare and no executed SQL; actual status/error targets retain owned draft, exact message, line cache/scroll, input histories, source/model/Pages/jobs through capture/pick/cancel and normal Escape\n");
    review_smoke_author_release(t); ui_set_scale(&a->ui,1); SetWindowSize(1280,820); return 1;
failed:
    review_smoke_query_diagnostic(a,t,"missing-column validation feedback");
    fprintf(stderr,"Review validation feedback phase=%d error=%s lines=%zu scroll=%d limited=%d jobs_base=%llu prepares_base=%llu request=%llu state=%s\n",phase,a->query_error ? a->query_error : "",a->query_errors.count,a->query_error_scroll,a->query_errors.limited,(unsigned long long)t->query_failure_jobs,(unsigned long long)t->query_failure_prepares,(unsigned long long)t->query_failure_request,r->frame.state_text); return -1;
}
static void review_smoke_workspace_remember(App *a,ReviewSmoke *t) {
    review_smoke_stats_remember(a,t); t->cell_input=a->pivot_search; t->cell_text=dx_strdup(a->pivot_search.text);
    for (size_t stack=0;stack<2;stack++) {
        const TextInputState *states=stack ? a->pivot_search.redo : a->pivot_search.undo; size_t count=stack ? a->pivot_search.redo_count : a->pivot_search.undo_count;
        for (size_t i=0;i<count;i++) t->cell_history[32*stack+i]=dx_strdup(states[i].text);
    }
    const Worksheet *sheet=active_sheet(a); t->cell_borrowed=sheet->cells; cells_copy(&t->cell_model,&sheet->cells);
    t->cell_worksheet_redo=sheet->redo_actions_count; t->cell_object_next=sheet->objects.next_id;
    t->workspace_match_pointer=a->field_matches; t->workspace_match_count=a->field_match_count; t->workspace_match_capacity=a->field_match_capacity; t->workspace_schema_count=a->field_schema_count;
    if (a->field_match_count) { size_t bytes=a->field_match_count*sizeof(size_t); t->workspace_matches=dx_alloc(bytes); memcpy(t->workspace_matches,a->field_matches,bytes); }
    t->workspace_source_id=a->field_source_id; t->workspace_source_revision=a->field_source_revision; t->workspace_match_generation=a->field_match_generation; t->workspace_search_revision=a->field_search_revision;
    t->workspace_search_focus=a->pivot_search_focus; t->workspace_focus=a->pivot_focus; t->workspace_role=a->pivot_role; t->workspace_value=a->pivot_value; t->workspace_field_scroll=a->pivot_field_scroll;
    t->workspace_author_scroll=a->author_scroll; memcpy(t->workspace_dock_scroll,a->dock_scroll,sizeof(t->workspace_dock_scroll)); t->workspace_marks=a->author_marks_layout;
    t->workspace_title=a->context_title; t->workspace_fields=a->field_list; memcpy(t->workspace_axes,a->axis_shelves,sizeof(t->workspace_axes)); memcpy(t->workspace_tokens,a->pivot_tokens,sizeof(t->workspace_tokens)); memcpy(t->workspace_mark_labels,a->mark_labels,sizeof(t->workspace_mark_labels));
}
static bool review_smoke_workspace_same(App *a,ReviewSmoke *t) {
    const Worksheet *sheet=active_sheet(a); const CellStore *cells=&sheet->cells,*old=&t->cell_borrowed;
    return review_smoke_stats_same(a,t) && review_smoke_cell_input_same(&a->pivot_search,t) && review_smoke_cell_content(cells,&t->cell_model) && cells->cells==old->cells && cells->capacity==old->capacity && cells->revision==old->revision && cells->undo_count==old->undo_count && cells->redo_count==old->redo_count && sheet->redo_actions_count==t->cell_worksheet_redo && sheet->objects.next_id==t->cell_object_next &&
        a->field_matches_valid && a->field_matches==t->workspace_match_pointer && a->field_match_count==t->workspace_match_count && a->field_match_capacity==t->workspace_match_capacity && a->field_schema_count==t->workspace_schema_count && (!a->field_match_count || !memcmp(a->field_matches,t->workspace_matches,a->field_match_count*sizeof(size_t))) &&
        a->field_source_id==t->workspace_source_id && a->field_source_revision==t->workspace_source_revision && a->field_match_generation==t->workspace_match_generation && a->field_search_revision==t->workspace_search_revision &&
        a->pivot_search_focus==t->workspace_search_focus && a->pivot_focus==t->workspace_focus && a->pivot_role==t->workspace_role && a->pivot_value==t->workspace_value && a->pivot_field_scroll==t->workspace_field_scroll && a->author_scroll==t->workspace_author_scroll && !memcmp(a->dock_scroll,t->workspace_dock_scroll,sizeof(t->workspace_dock_scroll)) &&
        !memcmp(&a->author_marks_layout,&t->workspace_marks,sizeof(t->workspace_marks)) && !memcmp(&a->context_title,&t->workspace_title,sizeof(t->workspace_title)) && review_smoke_guide_rect(a->field_list,t->workspace_fields) && !memcmp(a->axis_shelves,t->workspace_axes,sizeof(t->workspace_axes)) && !memcmp(a->pivot_tokens,t->workspace_tokens,sizeof(t->workspace_tokens)) && !memcmp(a->mark_labels,t->workspace_mark_labels,sizeof(t->workspace_mark_labels));
}
static bool review_smoke_workspace_targets(App *a,ReviewSmoke *t,int item) {
    DxReviewRegistry *registry=a->review->registry; SheetObject *pivot=selected_pivot(a); if (!pivot || registry->duplicate_count || registry->invalid_count || registry->overflow_count || registry->truncated) return false;
    char document[128]; review_document_id(a,document); size_t fields=0,tokens=0;
    const SheetObject *source=objects_source(&active_sheet(a)->objects,pivot); uint64_t source_id=source ? source->id : 0,source_revision=source ? source->revision : a->analysis_revision;
    for (size_t i=0;i<registry->count;i++) {
        const DxReviewWidget *widget=&registry->widgets[i]; bool field=!strncmp(widget->id,"pane.fields",11),author=!strncmp(widget->id,"analysis.context",16) || !strncmp(widget->id,"analysis.shelf.",15) || !strncmp(widget->id,"analysis.marks",14); if (!field && !author) continue;
        const DxReviewDataIdentity *data=&widget->data; Rectangle shown=GetCollisionRec((Rectangle){widget->bounds.x,widget->bounds.y,widget->bounds.width,widget->bounds.height},(Rectangle){widget->clip.x,widget->clip.y,widget->clip.width,widget->clip.height});
        if (!data->present || strcmp(data->document_id,document) || strcmp(data->document_id,registry->frame.document_id) || data->sheet_id!=a->book.active+1 || data->has_cell || data->source_id!=source_id || data->object_id!=(field ? source_id : pivot->id) || data->schema_generation!=(field ? a->field_schema_generation : source_revision) || data->query_generation!=(field ? source_revision : pivot->revision) || shown.width<=0 || shown.height<=0 || !strstr(widget->source_file,"src/app/app.c") || !widget->source_line) return false;
        if (widget->container || !strcmp(widget->role,"text") || !strcmp(widget->role,"region")) { if (widget->input_eligible) return false; }
        if (!strncmp(widget->id,"pane.fields.field.",18) && !strstr(widget->id,".more")) fields++;
        if (strstr(widget->id,".token.") && !strstr(widget->id,".remove")) tokens++;
    }
    const char *required[]={"pane.fields","pane.fields.heading","pane.fields.close","pane.fields.search","pane.fields.list","analysis.context","analysis.context.title","analysis.context.browse","analysis.shelf.rows","analysis.shelf.rows.add","analysis.shelf.columns","analysis.shelf.columns.add","analysis.marks","analysis.marks.representation","analysis.shelf.values","analysis.shelf.values.add"};
    for (size_t i=0;i<sizeof(required)/sizeof(*required);i++) if (!dx_review_find(registry,required[i])) { fprintf(stderr,"Review workspace missing %s\n",required[i]); return false; }
    const DxReviewWidget *search=dx_review_find(registry,"pane.fields.search"),*representation=dx_review_find(registry,"analysis.marks.representation");
    if (!search->value.present || search->value.is_null || strcmp(search->value.type,"VARCHAR") || strcmp(search->value.exact_text,t->cell_text) || !representation->enabled || !representation->input_eligible || strcmp(representation->role,"combobox") || !representation->value.present || strcmp(representation->value.exact_text,"Matrix") || dx_review_find(registry,"analysis.marks.plot")) return false;
    Page *available=context_fields(a); size_t expected_fields=0;
    for (size_t i=0;i<available->column_count;i++) {
        char id[128],more[128]; snprintf(id,sizeof(id),"pane.fields.field.f%016llx",(unsigned long long)review_field_hash(available->columns[i].name)); snprintf(more,sizeof(more),"%s.more",id);
        const DxReviewWidget *row=dx_review_find(registry,id),*button=dx_review_find(registry,more); bool matched=false;
        for (size_t j=(size_t)a->pivot_field_scroll;j<a->field_match_count && j<(size_t)(a->pivot_field_scroll+a->field_visible);j++) if (a->field_matches[j]==i) matched=true;
        if (!matched) { if (row || button) return false; continue; }
        if (!row && !button) continue; expected_fields++;
        bool focused=a->pivot_focus==1 && a->pivot_field==(int)i;
        if (!row || !button || strcmp(row->data.kind,"available-field") || strcmp(row->data.field,available->columns[i].name) || row->data.row!=i || row->data.column || !row->enabled || !row->input_eligible || row->focused!=focused || row->checked!=focused || !row->value.present || row->value.is_null || strcmp(row->value.exact_text,available->columns[i].name) || strcmp(button->parent_id,id) || !button->enabled || !button->input_eligible || button->checked || !review_smoke_sheet_pick(a,button,"pane.fields.list")) return false;
        if (!t->author_target[0]) snprintf(t->author_target,sizeof(t->author_target),"%s",more);
    }
    if (!fields || fields!=expected_fields || fields>(size_t)a->field_visible || (!!dx_review_find(registry,"pane.fields.clear"))!=(item!=0)) return false;
    size_t expected_tokens=0;
    for (int role=0;role<4;role++) for (size_t i=0;i<pivot->pivot.counts[role];i++) {
        char id[128],remove_id[128]; const PivotField *field=&pivot->pivot.fields[role][i]; if (!review_workspace_token_id(id,role,field,false) || !review_workspace_token_id(remove_id,role,field,true)) return false;
        const DxReviewWidget *token=dx_review_find(registry,id),*remove=dx_review_find(registry,remove_id); if (!token) { if (remove) return false; continue; } expected_tokens++;
        if (strcmp(token->data.kind,"pivot-token") || strcmp(token->data.field,field->name) || token->data.row!=i || token->data.column!=(uint64_t)role || !token->enabled || !token->input_eligible || !token->value.present || token->value.is_null || strcmp(token->value.type,"VARCHAR")) return false;
        if (role==PIVOT_VALUES) {
            if (!remove || remove->enabled || remove->input_eligible || strcmp(remove->parent_id,id) || !review_smoke_sheet_pick(a,remove,"analysis.shelf.values")) return false;
            if (item) snprintf(t->author_target,sizeof(t->author_target),"%s",remove_id);
        } else if (remove) return false;
    }
    if (tokens!=expected_tokens || (!item && tokens) || (item && tokens<3)) return false;
    const DxReviewWidget *color=dx_review_find(registry,"analysis.marks.target.color");
    if (!color || !color->enabled || !color->input_eligible || color->checked!=(item!=0) || (item ? !color->value.present || color->value.is_null || strcmp(color->value.exact_text,"City") : color->value.present)) return false;
    return t->author_target[0]!=0;
}
static int review_smoke_workspace_tick(App *a,ReviewSmoke *t) {
    int item=(t->stage-REVIEW_QUERY_FEEDBACK_END)/4,phase=(t->stage-REVIEW_QUERY_FEEDBACK_END)%4; DxReviewCapture *r=a->review;
    if (!phase) {
        if (r->phase!=DX_REPORT_IDLE || !smoke_work_idle(a)) return 0;
        if (!t->workspace_started || t->workspace_setup_item!=item) {
            if (a->modal!=MODAL_NONE || a->analysis.kind!=SOURCE_CSV || a->page.total!=5 || a->page.column_count!=6) goto failed;
            run_action(a,ACTION_RESET_LAYOUT);
            if (!item) { grid_select_cell(&a->grid,0,8,false); keyboard_object(a); create_pivot(a); if (!selected_pivot(a)) goto failed; t->workspace_object=a->selected_object; }
            else {
                if (a->selected_object!=t->workspace_object || !selected_pivot(a)) goto failed;
                PivotDefinition definition={0}; char *error=NULL; bool ok=pivot_assign(&definition,PIVOT_ROWS,"City",AGG_SUM,&error) && pivot_assign(&definition,PIVOT_COLUMNS,"Approved",AGG_SUM,&error) && pivot_assign(&definition,PIVOT_VALUES,"Amount",AGG_SUM,&error) && pivot_assign(&definition,PIVOT_FILTERS,"Note",AGG_SUM,&error); free(error);
                if (!ok) { pivot_free(&definition); goto failed; } if (!set_pivot_definition(a,&definition)) goto failed;
                MarkField color={.name="City"}; if (!assign_mark_field(a,MARKS_COLOR,&color)) goto failed;
            }
            input_set(&a->pivot_search,""); if (item) { input_insert(&a->pivot_search,"i"); a->pivot_search.select_all=false; a->pivot_search.cursor=1; a->pivot_search.anchor=0; }
            a->pivot_focus=6; a->author_reveal=true;
            ui_set_scale(&a->ui,item ? 1.5f : 1); SetWindowSize(1280,item ? 1000 : 820);
            t->workspace_started=true; t->workspace_setup_item=item; return 0;
        }
        if (!selected_pivot(a) || a->modal!=MODAL_NONE) goto failed;
        if ((item && !object_ready(a,(size_t)t->workspace_object)) || GetScreenWidth()!=1280 || GetScreenHeight()!=(item ? 1000 : 820) || a->ui.scale!=(item ? 1.5f : 1) || a->author_context_progress!=1 || fabsf(a->left_extent-a->sidebar_width)>.01f || !a->author_marks_layout.valid || !a->fields_visible || a->field_visible<1 || a->field_search_revision!=a->pivot_search.revision || !review_can_capture(a) || a->statistics_state==STATS_WAITING || a->statistics_state==STATS_CALCULATING) return 0;
        review_smoke_workspace_remember(a,t); t->author_target[0]=0; ui_note_key_press(KEY_PERIOD,DX_MOD_SUPER|DX_MOD_SHIFT); return 1;
    }
    if (phase==1) {
        if (r->phase!=DX_REPORT_PICK) return 0;
        if (!review_smoke_workspace_same(a,t) || !review_smoke_workspace_targets(a,t,item)) goto failed;
        r->toolbar_top=true; review_smoke_pick(a,t->author_target); return 1;
    }
    if (phase==2) {
        if (r->phase!=DX_REPORT_NOTE) return 0;
        if (strcmp(r->target.widget.id,t->author_target) || !review_smoke_workspace_same(a,t) || (item && r->target.widget.enabled)) goto failed;
        ui_note_key_press(KEY_ESCAPE,0); return 1;
    }
    if (r->phase!=DX_REPORT_IDLE) return 0;
    if (!review_smoke_workspace_same(a,t)) goto failed;
    printf("PASS: native contextual workspace review %s checks actual visible Fields/actions, Rows/Columns, Measures/Filters/Marks ownership/clipping and frozen pick/cancel; retains search/history, source/Pivot/manual/Pages/Stats/jobs without underlying activation\n",item ? "populated150% with disabled sole-measure remove" : "empty100% with field actions");
    review_smoke_author_release(t); if (item) { ui_set_scale(&a->ui,1); SetWindowSize(1280,820); } return 1;
failed:
    fprintf(stderr,"Review workspace mismatch stage=%d phase=%d item=%d modal=%d report=%d object=%d focus=%d search=%s matches=%zu visible=%d scroll=%d/%.3f dock=%.3f target=%s actual=%s\n",t->stage,phase,item,(int)a->modal,(int)r->phase,a->selected_object,a->pivot_focus,a->pivot_search.text ? a->pivot_search.text : "",a->field_match_count,a->field_visible,a->pivot_field_scroll,a->author_scroll,a->dock_scroll[1],t->author_target,r->target.widget.id);
    for (size_t i=0;i<r->registry->count;i++) { const DxReviewWidget *w=&r->registry->widgets[i]; if (!strncmp(w->id,"pane.fields",11) || !strncmp(w->id,"analysis.shelf.",15) || !strncmp(w->id,"analysis.marks",14)) fprintf(stderr,"Review workspace widget %s parent=%s role=%s enabled=%d checked=%d focus=%d kind=%s owner=%llu source=%llu schema=%llu query=%llu index=%llu/%llu field=%s value=%s rect=%.2f,%.2f,%.2f,%.2f clip=%.2f,%.2f,%.2f,%.2f\n",w->id,w->parent_id,w->role,w->enabled,w->checked,w->focused,w->data.kind,(unsigned long long)w->data.object_id,(unsigned long long)w->data.source_id,(unsigned long long)w->data.schema_generation,(unsigned long long)w->data.query_generation,(unsigned long long)w->data.row,(unsigned long long)w->data.column,w->data.field,w->value.exact_text,w->bounds.x,w->bounds.y,w->bounds.width,w->bounds.height,w->clip.x,w->clip.y,w->clip.width,w->clip.height); } return -1;
}
static bool review_smoke_feedback_restore(ReviewSmoke *t) {
    if (!t->feedback_hidden) return true;
    if (rename(t->feedback_backup,t->feedback_path)) { fprintf(stderr,"Review feedback fixture restore failed: %s\n",t->feedback_path); return false; }
    t->feedback_hidden=false; return true;
}
static bool review_smoke_feedback_fixture(ReviewSmoke *t) {
    if (t->feedback_fixture) return true;
    if (!t->owner_fixture[0] || snprintf(t->feedback_path,sizeof(t->feedback_path),"%s.feedback.csv",t->owner_fixture)>=(int)sizeof(t->feedback_path) ||
        snprintf(t->feedback_backup,sizeof(t->feedback_backup),"%s.unavailable",t->feedback_path)>=(int)sizeof(t->feedback_backup) || FileExists(t->feedback_path) || FileExists(t->feedback_backup)) return false;
    FILE *file=fopen(t->feedback_path,"wb"); if (!file) return false;
    bool ok=fprintf(file,"ID,City,Amount\n")>0;
    for (int i=0;i<96 && ok;i++) ok=fprintf(file,"%d,City%03d,%d.25\n",i+1,i,i+1)>0;
    ok=fclose(file)==0 && ok; t->feedback_fixture=ok; return ok;
}
static void review_smoke_feedback_remember(App *a,ReviewSmoke *t) {
    review_smoke_cell_remember(a,t);
    SheetObject *table=&active_sheet(a)->objects.items[t->feedback_table]; t->feedback_source=analysis_copy(&table->source);
    t->feedback_scroll=a->step_scroll; t->feedback_chrome=a->chrome_focus; t->feedback_filter_focus=a->filter_focus; t->feedback_width=a->pipeline_width;
    t->sql_pane_visible=a->pipeline; t->sql_pane_collapsed=a->pipeline_collapsed; t->sql_pane_mode=a->pipeline_sql; t->sql_pane_focus=a->pipeline_focus_index; t->sql_pane_step=a->pipeline_selected_step;
    t->sql_pane_pointer=a->inspector_sql_text; t->sql_pane_text=a->inspector_sql_text ? dx_strdup(a->inspector_sql_text) : NULL; t->sql_pane_scroll=a->inspector_sql_scroll; t->sql_pane_lines=a->inspector_sql_lines;
    if (a->inspector_sql_lines.offsets) { size_t bytes=(a->inspector_sql_lines.count+1)*sizeof(size_t); t->sql_pane_offsets=dx_alloc(bytes); memcpy(t->sql_pane_offsets,a->inspector_sql_lines.offsets,bytes); }
    t->feedback_preview=a->step_preview; t->feedback_preview_source=analysis_copy(&a->step_preview.source);
    if (a->step_preview.active) t->feedback_preview_selection=grid_location(&a->step_preview.grid);
    for (size_t i=0;i<4;i++) { t->feedback_values[i]=a->value_cache[i]; t->feedback_keys[i]=a->value_cache_keys[i] ? dx_strdup(a->value_cache_keys[i]) : NULL; }
    t->feedback_context=a->values_context ? dx_strdup(a->values_context) : NULL; t->feedback_pending=a->value_pending_key ? dx_strdup(a->value_pending_key) : NULL;
    t->feedback_total=a->values_total; t->feedback_value_scroll=a->values_scroll; t->feedback_value_focus=a->values_focus; t->feedback_search_revision=a->values_search_revision;
    t->feedback_loading=a->values_loading; t->feedback_failed=a->values_failed; t->feedback_all=a->values_all;
    t->feedback_message_pointer=a->message; t->feedback_message=dx_strdup(a->message ? a->message : ""); t->feedback_message_since=a->message_since; t->feedback_message_revision=a->message_revision;
}
static bool review_smoke_feedback_worker(App *a,ReviewSmoke *t,bool pending,bool published) {
    if (!pending) return review_smoke_query_worker(a,t,false);
    QueryWorker *worker=&a->worker; bool ok;
    dx_lock(&worker->mutex);
    ok=worker->latest_id==t->request && worker->jobs_started>=t->query_jobs && worker->jobs_started<=t->query_jobs+1;
    if (published) ok&=t->query_complete && !worker->pending && !worker->running && !worker->reply && worker->jobs_started==t->query_jobs+1 && worker->db.query_count==t->query_complete_sql && worker->db.count_queries==t->query_complete_counts && worker->db.relation_prepares==t->query_complete_prepares;
    else {
        if (worker->pending) ok&=worker->pending->id==t->request && worker->pending->kind==JOB_STEP_PREVIEW && worker->pending->is_preview && analysis_same_result(&worker->pending->analysis,&t->feedback_preview_source);
        if (worker->running) ok&=worker->active_id==t->request && worker->jobs_started==t->query_jobs+1;
        if (worker->reply) {
            QueryReply *reply=worker->reply;
            ok&=!worker->pending && !worker->running && worker->jobs_started==t->query_jobs+1 && reply->kind==JOB_STEP_PREVIEW && reply->is_preview && reply->id==t->request && !reply->error && reply->page.column_count==3 && reply->page.total==96;
            if (ok && !t->query_complete) { t->query_complete=true; t->query_reply=reply; t->query_complete_sql=worker->db.query_count; t->query_complete_counts=worker->db.count_queries; t->query_complete_prepares=worker->db.relation_prepares; }
        }
        if (t->query_complete) ok&=worker->reply==t->query_reply && !worker->pending && !worker->running && worker->db.query_count==t->query_complete_sql && worker->db.count_queries==t->query_complete_counts && worker->db.relation_prepares==t->query_complete_prepares;
        else ok&=worker->pending || worker->running;
    }
    dx_unlock(&worker->mutex); return ok;
}
static bool review_smoke_feedback_same(App *a,ReviewSmoke *t,int item) {
    Worksheet *sheet=active_sheet(a); GridLocation location=grid_location(&a->grid); const StepPreview *preview=&a->step_preview,*old=&t->feedback_preview;
    const Analysis *source=&sheet->objects.items[t->feedback_table].source;
    if (!review_smoke_author_models_same(a,t) || !review_smoke_cell_input_same(&a->input,t) || !review_smoke_cell_content(&sheet->cells,&t->cell_model) ||
        sheet->cells.cells!=t->cell_borrowed.cells || sheet->cells.capacity!=t->cell_borrowed.capacity || sheet->cells.revision!=t->cell_borrowed.revision || sheet->cells.undo_count!=t->cell_borrowed.undo_count || sheet->cells.redo_count!=t->cell_borrowed.redo_count || sheet->redo_actions_count!=t->cell_worksheet_redo || sheet->objects.next_id!=t->cell_object_next ||
        !analysis_same_result(source,&t->feedback_source) || !analysis_sorts_equal(source,&t->feedback_source) || a->modal!=t->choice_modal || a->request_id!=t->request || a->page.cells!=t->page || a->book.dirty!=t->dirty || sheet->undo_actions_count!=t->history || sheet->formats.revision!=t->choice_formats || a->ui.scale!=t->choice_scale || a->ribbon_mode!=t->choice_ribbon_mode ||
        location.row!=t->selection.row || location.col!=t->selection.col || location.end_row!=t->selection.end_row || location.end_col!=t->selection.end_col || a->grid.row_height!=t->choice_row_height ||
        a->pipeline!=t->sql_pane_visible || a->pipeline_collapsed!=t->sql_pane_collapsed || a->pipeline_sql!=t->sql_pane_mode || a->pipeline_focus_index!=t->sql_pane_focus || a->pipeline_selected_step!=t->sql_pane_step || a->step_scroll!=t->feedback_scroll || a->chrome_focus!=t->feedback_chrome || a->filter_focus!=t->feedback_filter_focus || a->pipeline_width!=t->feedback_width ||
        a->inspector_sql_text!=t->sql_pane_pointer || !review_smoke_text_same(a->inspector_sql_text,t->sql_pane_text) || a->inspector_sql_scroll!=t->sql_pane_scroll || memcmp(&a->inspector_sql_lines,&t->sql_pane_lines,sizeof(TextLines)) || (t->sql_pane_offsets && memcmp(a->inspector_sql_lines.offsets,t->sql_pane_offsets,(t->sql_pane_lines.count+1)*sizeof(size_t))) ||
        memcmp(&a->statistics,&t->stats_cache,sizeof(SelectionStats)) || memcmp(&a->statistics_key,&t->stats_key,sizeof(StatsKey)) || a->statistics_state!=t->stats_state || a->statistics_request!=t->stats_request || a->statistics_object!=t->stats_object || a->statistics_frame!=t->stats_frame ||
        a->message!=t->feedback_message_pointer || strcmp(a->message ? a->message : "",t->feedback_message) || a->message_since!=t->feedback_message_since || a->message_revision!=t->feedback_message_revision ||
        !review_smoke_text_same(a->values_context,t->feedback_context) || !review_smoke_text_same(a->value_pending_key,t->feedback_pending) || a->values_total!=t->feedback_total || a->values_scroll!=t->feedback_value_scroll || a->values_focus!=t->feedback_value_focus || a->values_search_revision!=t->feedback_search_revision || a->values_loading!=t->feedback_loading || a->values_failed!=t->feedback_failed || a->values_all!=t->feedback_all || a->io_busy || a->busy!=(item==2)) return false;
    for (size_t i=0;i<4;i++) if (memcmp(&a->value_cache[i],&t->feedback_values[i],sizeof(DistinctPage)) || !review_smoke_text_same(a->value_cache_keys[i],t->feedback_keys[i])) return false;
    for (size_t i=0;i<sheet->objects.count;i++) {
        const SheetObject *object=&sheet->objects.items[i]; const Page *page=&a->objects[i].page;
        if (page->cells!=t->query_views[i].cells || object->id!=t->query_views[i].id || object->revision!=t->query_views[i].revision || object->source_id!=t->query_views[i].source_id || page->total!=t->query_views[i].total || page->row_count!=t->query_views[i].rows || page->column_count!=t->query_views[i].columns) return false;
    }
    if (preview->active!=old->active || preview->loading!=old->loading || preview->request!=old->request || preview->source_id!=old->source_id || preview->source_revision!=old->source_revision || preview->step!=old->step || preview->page.cells!=old->page.cells || preview->page.column_count!=old->page.column_count || preview->page.total!=old->page.total || preview->error!=old->error || preview->title!=old->title || !analysis_same_result(&preview->source,&t->feedback_preview_source) || !analysis_sorts_equal(&preview->source,&t->feedback_preview_source)) return false;
    if (preview->active) { GridLocation selected=grid_location(&preview->grid); if (selected.row!=t->feedback_preview_selection.row || selected.col!=t->feedback_preview_selection.col || selected.end_row!=t->feedback_preview_selection.end_row || selected.end_col!=t->feedback_preview_selection.end_col || preview->grid.top!=old->grid.top || preview->grid.first_col!=old->grid.first_col || preview->grid.zoom!=old->grid.zoom || memcmp(&preview->formats,&old->formats,sizeof(FormatStore))) return false; }
    return review_smoke_feedback_worker(a,t,item==2,false);
}
static bool review_smoke_feedback_targets(App *a,ReviewSmoke *t,int item) {
    DxReviewRegistry *registry=a->review->registry; const SheetObject *table=&active_sheet(a)->objects.items[t->feedback_table]; char document[128]; review_document_id(a,document);
    if (registry->duplicate_count || registry->invalid_count || registry->overflow_count || registry->truncated || strcmp(registry->frame.document_id,document)) return false;
    const char *target=item==0 ? "pane.transformations.preview" : item==1 ? "pane.transformations.copy" : item==2 ? "preview.return" : item==3 ? "filter.values.scrollbar.thumb" : "filter.values.status";
    const DxReviewWidget *chosen=dx_review_find(registry,target); if (!chosen) return false;
    snprintf(t->author_target,sizeof(t->author_target),"%s",target);
    for (size_t i=0;i<registry->count;i++) {
        const DxReviewWidget *widget=&registry->widgets[i]; const DxReviewDataIdentity *data=&widget->data;
        bool pipeline=!strncmp(widget->id,"pane.transformations.",21),preview=!strncmp(widget->id,"preview.",8),filter=!strncmp(widget->id,"filter.values",13);
        if (!pipeline && !preview && !filter && strcmp(widget->id,"status.metrics") && strcmp(widget->id,"status.source_count") && strcmp(widget->id,"toast.content") && strncmp(widget->id,"cell.readout.",13)) continue;
        if (!data->present || strcmp(data->document_id,document) || !widget->source_file[0] || !widget->source_line) return false;
        if (pipeline && strcmp(widget->id,"pane.transformations.background") && (data->sheet_id!=a->book.active+1 || data->object_id!=table->id || data->source_id!=table->id || data->query_generation!=table->revision || data->has_cell)) return false;
        if (preview && (strcmp(data->kind,"step-preview") || data->object_id || data->source_id!=table->id || data->query_generation!=t->request || data->schema_generation!=table->revision || data->has_cell || data->row!=0)) return false;
        if (filter && (strcmp(data->kind,"filter-domain") || data->object_id!=table->id || data->source_id!=table->id || data->query_generation!=table->revision || data->column!=1 || strcmp(data->field,"City") || data->has_cell)) return false;
        if (!strcmp(widget->id,"toast.content") && (strcmp(data->kind,"document-notification") || data->sheet_id || data->object_id || data->source_id || data->query_generation || data->schema_generation || data->has_cell)) return false;
        if (widget->container || !strcmp(widget->role,"text") || !strcmp(widget->role,"region")) { if (widget->input_eligible) return false; }
    }
    if (item<2) {
        const DxReviewWidget *source=dx_review_find(registry,"pane.transformations.source"),*footer=dx_review_find(registry,"status.source_count"),*metrics=dx_review_find(registry,"status.metrics");
        if (a->selected_object!=t->feedback_pivot || !source || !source->value.present || strcmp(source->value.exact_text,table->name) || !footer || footer->data.object_id || footer->data.source_id || footer->data.query_generation!=a->analysis_revision || strcmp(footer->data.kind,"source-status") || !metrics || strcmp(metrics->data.kind,"statistics-frame") || metrics->data.object_id!=active_sheet(a)->objects.items[t->feedback_pivot].id || metrics->data.source_id!=table->id) return false;
        if (!item) {
            size_t count=0; bool child=false;
            for (size_t i=0;i<registry->count;i++) { const DxReviewWidget *widget=&registry->widgets[i]; if (strncmp(widget->id,"pane.transformations.step.",26)) continue; count++; if (widget->data.row==0) child=review_smoke_sheet_pick(a,widget,"pane.transformations.steps"); }
            if (count!=3 || !child || !chosen->enabled || !chosen->input_eligible) return false;
        } else {
            const DxReviewWidget *sql=dx_review_find(registry,"pane.transformations.sql"),*body=dx_review_find(registry,"pane.transformations.body"),*back=dx_review_find(registry,"pane.transformations.back");
            float width=fmaxf(340,t->feedback_width)*a->ui.scale;
            if (!sql || !body || !back || a->ui.scale!=1.5f || registry->frame.ui_scale!=1.5f || fabsf(body->bounds.width-width)>.01f || fabsf(a->pipeline_extent-fmaxf(340,t->feedback_width))>.01f || fabsf(sql->bounds.width-(a->pipeline_layout.sql_code.width-8*a->ui.scale))>.01f || !sql->value.present || sql->value.is_null || strcmp(sql->value.type,"SQL") || strcmp(sql->value.exact_text,t->sql_pane_text) || sql->value.truncated || !chosen->enabled || !chosen->input_eligible || !back->enabled || !back->input_eligible || dx_review_find(registry,"pane.transformations.steps")) return false;
            const DxReviewWidget *parts[]={body,sql,chosen,back};
            for (size_t i=0;i<sizeof(parts)/sizeof(*parts);i++) {
                const DxReviewWidget *widget=parts[i]; Rectangle bounds={widget->bounds.x,widget->bounds.y,widget->bounds.width,widget->bounds.height},clip={widget->clip.x,widget->clip.y,widget->clip.width,widget->clip.height}; Rectangle shown=GetCollisionRec(bounds,clip);
                if (fabsf(shown.x-bounds.x)>.01f || fabsf(shown.y-bounds.y)>.01f || fabsf(shown.width-bounds.width)>.01f || fabsf(shown.height-bounds.height)>.01f || bounds.x<body->bounds.x-.01f || bounds.y<body->bounds.y-.01f || bounds.x+bounds.width>body->bounds.x+body->bounds.width+.01f || bounds.y+bounds.height>body->bounds.y+body->bounds.height+.01f || bounds.x+bounds.width>GetScreenWidth()+.01f || bounds.y+bounds.height>GetScreenHeight()+.01f) return false;
            }
        }
    } else if (item==2) {
        const DxReviewWidget *status=dx_review_find(registry,"preview.status"),*readout=dx_review_find(registry,"cell.readout.value"),*body=dx_review_find(registry,"pane.transformations.body");
        if (!status || !status->value.present || status->value.is_null || strcmp(status->value.exact_text,"Loading step preview…") || !readout || readout->value.present || !a->step_preview.loading || a->step_preview.page.column_count || !body || a->pipeline_sql || fabsf(body->bounds.width-t->feedback_width*a->ui.scale)>.01f) return false;
    } else if (item==3) {
        const DxReviewWidget *track=dx_review_find(registry,"filter.values.scrollbar"); bool child=false;
        for (size_t i=0;i<registry->count;i++) {
            const DxReviewWidget *widget=&registry->widgets[i]; if (strncmp(widget->id,"filter.value.",13)) continue;
            const DxReviewDataIdentity *data=&widget->data; DistinctPage *page=cached_values(a,data->row/DX_DISTINCT_ROWS*DX_DISTINCT_ROWS);
            if (strcmp(widget->parent_id,"filter.values") || strcmp(data->kind,"filter-domain") || strcmp(data->document_id,document) || data->sheet_id!=a->book.active+1 || data->object_id!=table->id || data->source_id!=table->id || data->schema_generation!=table->revision || data->query_generation!=table->revision || data->has_cell || data->column!=1 || strcmp(data->field,"City") || data->row<a->values_scroll || !page || data->row-page->offset>=page->count) return false;
            const DistinctValue *value=&page->values[data->row-page->offset];
            if (!widget->value.present || widget->value.truncated || strcmp(widget->value.type,filter_fields(a)->columns[1].type_name) || widget->value.is_null!=value->is_null || strcmp(widget->value.exact_text,value->value ? value->value : "")) return false;
            DxReviewTarget picked; Rectangle shown=GetCollisionRec((Rectangle){widget->bounds.x,widget->bounds.y,widget->bounds.width,widget->bounds.height},(Rectangle){widget->clip.x,widget->clip.y,widget->clip.width,widget->clip.height});
            if (!dx_review_pick(registry,shown.x+shown.width*.5f,shown.y+shown.height*.5f,DX_REVIEW_PICK_CONTROL,&picked) || strcmp(picked.widget.id,widget->id)) return false;
            child=true;
        }
        if (!child || !track || chosen->bounds.y<=track->bounds.y || !chosen->input_eligible || strcmp(chosen->parent_id,track->id) || a->values_total!=96 || !a->values_scroll || dx_review_find(registry,"filter.values.status")) return false;
    } else if (!chosen->value.present || chosen->value.is_null || strcmp(chosen->value.type,"VARCHAR") || chosen->input_eligible || (item==4 ? strcmp(chosen->value.exact_text,"No matching values") || a->values_total!=0 || a->values_failed : strcmp(chosen->value.exact_text,t->feedback_message) || !a->values_failed || (!strstr(chosen->value.exact_text,"feedback.csv") && strcmp(chosen->value.exact_text,"The source changed since its displayed result. Refresh before using its data."))) || dx_review_find(registry,"filter.values.scrollbar")) return false;
    DxReviewTarget picked; Rectangle shown=GetCollisionRec((Rectangle){chosen->bounds.x,chosen->bounds.y,chosen->bounds.width,chosen->bounds.height},(Rectangle){chosen->clip.x,chosen->clip.y,chosen->clip.width,chosen->clip.height});
    return dx_review_pick(registry,shown.x+shown.width*.5f,shown.y+shown.height*.5f,DX_REVIEW_PICK_CONTROL,&picked) && !strcmp(picked.widget.id,target);
}
static int review_smoke_feedback_tick(App *a,ReviewSmoke *t) {
    int item=(t->stage-REVIEW_WORKSPACE_END)/4,phase=(t->stage-REVIEW_WORKSPACE_END)%4; DxReviewCapture *r=a->review;
    if (!phase) {
        if (r->phase!=DX_REPORT_IDLE || !smoke_work_idle(a)) return 0;
        if (!review_smoke_feedback_fixture(t)) goto failed;
        if (!t->feedback_table_added) { grid_select_cell(&a->grid,20,8,false); keyboard_object(a); if (!import_table(a,t->feedback_path)) goto failed; t->feedback_table=a->selected_object; t->feedback_table_added=true; return 0; }
        if (!object_ready(a,(size_t)t->feedback_table)) return 0;
        if (!t->feedback_transformed) {
            Analysis source=analysis_copy(&active_sheet(a)->objects.items[t->feedback_table].source); char *error=NULL; source.unique_rows=true; bool ok=analysis_sort_set(&source,"Amount",true,false,&error); free(error);
            if (ok) ok=set_table_source(a,(size_t)t->feedback_table,&source); analysis_free(&source); if (!ok) goto failed; t->feedback_transformed=true; return 0;
        }
        if (!t->feedback_pivot_added) {
            a->selected_object=t->feedback_table; create_pivot(a); if (!selected_pivot(a)) goto failed; t->feedback_pivot=a->selected_object;
            PivotDefinition definition={0}; char *error=NULL; bool ok=pivot_assign(&definition,PIVOT_ROWS,"City",AGG_SUM,&error) && pivot_assign(&definition,PIVOT_VALUES,"Amount",AGG_SUM,&error); free(error);
            if (!ok) { pivot_free(&definition); goto failed; } if (!set_pivot_definition(a,&definition)) goto failed; t->feedback_pivot_added=true; return 0;
        }
        if (!object_ready(a,(size_t)t->feedback_pivot)) return 0;
        if (!t->feedback_started || t->feedback_item!=item) {
            if (a->modal!=MODAL_NONE || a->step_preview.active) goto failed;
            if ((item==1 || item==2) && a->pipeline_width!=t->feedback_width) goto failed;
            t->feedback_item=item; t->feedback_started=true; t->feedback_end_sent=false; t->feedback_resume=false;
            a->selected_object=t->feedback_pivot; SheetObject *pivot=selected_pivot(a); if (!pivot || pivot->source_id!=active_sheet(a)->objects.items[t->feedback_table].id) goto failed;
            grid_select_cell(&a->grid,pivot->row,pivot->col,false); a->matrix_cells_active=false; a->pivot_focus=0; a->pivot_search_focus=false; a->ribbon_focus=false;
            /* Browse is the actual controller for returning from the Pivot to
               its bound Table before opening a historical source preview. */
            if (item==2) browse_source(a);
            present_pipeline(a,true,false); a->pipeline_sql=false;
            ui_set_scale(&a->ui,item==0 ? 1 : 1.5f); SetWindowSize(item==0 ? 1280 : 1920,item==0 ? 900 : 1120);
            t->note_frame=a->frames;
            if (item==1) pipeline_show_sql(a);
            if (item>=3) {
                open_table_filter(a,(size_t)t->feedback_table,1); if (a->modal!=MODAL_FILTER) goto failed;
                if (item==4) { input_set(&a->input,"not-present-feedback"); restart_values(a); }
                if (item==5) { if (rename(t->feedback_path,t->feedback_backup)) goto failed; t->feedback_hidden=true; input_set(&a->input,"City0"); restart_values(a); }
            }
            return 0;
        }
        if (a->ui.scale!=(item==0 ? 1 : 1.5f) || GetScreenWidth()!=(item==0 ? 1280 : 1920) || GetScreenHeight()!=(item==0 ? 900 : 1120) || fabsf(a->pipeline_extent-pipeline_target_width(a))>.01f || !a->pipeline_layout.valid || !review_can_capture(a) || a->statistics_state==STATS_WAITING || a->statistics_state==STATS_CALCULATING) return 0;
        if (item==1 && (a->frames<=t->note_frame || !a->inspector_sql_valid || a->inspector_sql_id!=active_sheet(a)->objects.items[t->feedback_table].id || !a->inspector_sql_lines.offsets ||
            fabsf((a->pipeline_extent-pipeline_target_width(a))*a->ui.scale)>.005f || a->inspector_sql_lines.scale!=a->ui.scale || a->inspector_sql_lines.raster_size!=a->ui.raster_size || fabsf(a->inspector_sql_lines.width-(a->pipeline_layout.sql_code.width-8*a->ui.scale))>.01f)) return 0;
        if (item>=3) {
            if (a->modal!=MODAL_FILTER || a->popup_presence.offset || a->popup_presence.alpha!=1 || !a->filter_layout.valid) return 0;
            if ((item!=5 && a->values_total<0) || (item==5 && !a->values_failed)) return 0;
            if (item==3 && !t->feedback_end_sent) { a->filter_focus=5; ui_note_key_press(KEY_END,0); t->feedback_end_sent=true; return 0; }
            if (item==3 && !a->values_scroll) return 0;
        }
        if (item==2) {
            const SheetObject *table=NULL; const Analysis *source=pipeline_source(a,&table); if (!table || !step_preview_start(&a->step_preview,source,0,table->id,table->revision,&a->grid)) goto failed;
            step_preview_presentation(&a->step_preview,context_fields(a),&a->grid,&active_sheet(a)->formats,table->col); watch_statistics(a);
        }
        /* Copy cache/model ownership and database counters while the worker is
           idle. After submission only UI-owned pending fields are observed;
           the worker's database counters are read again at its owned reply. */
        review_smoke_feedback_remember(a,t); t->query_complete=false; t->query_reply=NULL;
        if (item==2) {
            schedule_step_preview(a); if (!a->step_preview.loading || !a->busy || a->pending_kind!=JOB_STEP_PREVIEW) goto failed;
            t->request=a->request_id; t->feedback_preview=a->step_preview;
        }
        ui_note_key_press(KEY_PERIOD,DX_MOD_SUPER|DX_MOD_SHIFT); return 1;
    }
    if (phase==1) {
        if (r->phase!=DX_REPORT_PICK) return 0;
        bool retained=review_smoke_feedback_same(a,t,item),targets=retained && review_smoke_feedback_targets(a,t,item);
        if (!retained || !targets) { fprintf(stderr,"Review feedback gate retained=%d targets=%d registry=%zu duplicate=%zu invalid=%zu overflow=%zu truncated=%d/%d state_bytes=%zu\n",retained,targets,r->registry->count,r->registry->duplicate_count,r->registry->invalid_count,r->registry->overflow_count,r->registry->truncated,r->frame.truncated,strlen(r->frame.state_text)); goto failed; }
        r->toolbar_top=true; review_smoke_pick(a,t->author_target); return 1;
    }
    if (phase==2) {
        if (r->phase!=DX_REPORT_NOTE) return 0;
        if (strcmp(r->target.widget.id,t->author_target) || !review_smoke_feedback_same(a,t,item)) goto failed;
        if (item==2 && !t->query_complete) return 0;
        ui_note_key_press(KEY_ESCAPE,0); return 1;
    }
    if (r->phase!=DX_REPORT_IDLE) return 0;
    if (item==2 && t->feedback_resume) {
        if (a->step_preview.loading || !smoke_work_idle(a)) return 0;
        if (!a->step_preview.active || a->step_preview.page.total!=96 || a->step_preview.page.column_count!=3 || a->step_preview.request || a->step_preview.error || !review_smoke_feedback_worker(a,t,true,true)) goto failed;
        end_step_preview(a);
    } else {
        if (!review_smoke_feedback_same(a,t,item)) goto failed;
        if (item==2) { t->feedback_resume=true; return 0; }
    }
    if (item>=3) { if (!review_smoke_feedback_restore(t)) goto failed; ui_note_key_press(KEY_ESCAPE,0); }
    printf("PASS: native feedback closure case=%d actual %s ownership/clipping/value and frozen pick/cancel retain models/manual/history/Pages/input/SQL caches/worker jobs%s\n",item,((const char *const[]){"Table Steps100 with Pivot selected","upstream Table SQL150 with Pivot selected","pending historical preview","scrolled distinct domain","no-match domain","genuine unavailable-file distinct error"})[item],item==2 ? "; exactly one owned preview reply publishes after cancellation" : "");
    review_smoke_author_release(t); if (item==5) { ui_set_scale(&a->ui,1); SetWindowSize(1280,820); } return 1;
failed:
    review_smoke_feedback_restore(t);
    if (phase && t->author_owned) {
        bool views=true; for (size_t i=0;i<active_sheet(a)->objects.count;i++) { const SheetObject *object=&active_sheet(a)->objects.items[i]; const Page *page=&a->objects[i].page; if (page->cells!=t->query_views[i].cells || object->id!=t->query_views[i].id || object->revision!=t->query_views[i].revision || object->source_id!=t->query_views[i].source_id || page->total!=t->query_views[i].total || page->row_count!=t->query_views[i].rows || page->column_count!=t->query_views[i].columns) views=false; }
        fprintf(stderr,"Review feedback retained groups models=%d input=%d manual=%d views=%d stats_cache=%d stats_key=%d sql_pointer=%d sql_text=%d sql_lines=%d pipeline=%d/%d/%d focus=%d/%d chrome=%d/%d scroll=%d/%d\n",review_smoke_author_models_same(a,t),review_smoke_cell_input_same(&a->input,t),review_smoke_cell_content(&active_sheet(a)->cells,&t->cell_model),views,!memcmp(&a->statistics,&t->stats_cache,sizeof(SelectionStats)),!memcmp(&a->statistics_key,&t->stats_key,sizeof(StatsKey)),a->inspector_sql_text==t->sql_pane_pointer,review_smoke_text_same(a->inspector_sql_text,t->sql_pane_text),!memcmp(&a->inspector_sql_lines,&t->sql_pane_lines,sizeof(TextLines)),a->pipeline,a->pipeline_collapsed,a->pipeline_sql,a->pipeline_focus_index,t->sql_pane_focus,a->chrome_focus,t->feedback_chrome,a->step_scroll,t->feedback_scroll);
        fprintf(stderr,"Review feedback SQL cache offsets=%p/%p count=%zu/%zu capacity=%zu/%zu width=%.3f/%.3f scale=%.3f/%.3f raster=%d/%d code_width=%.3f ui_raster=%d message=%d/%d since=%.3f/%.3f revision=%llu/%llu\n",(void *)a->inspector_sql_lines.offsets,(void *)t->sql_pane_lines.offsets,a->inspector_sql_lines.count,t->sql_pane_lines.count,a->inspector_sql_lines.capacity,t->sql_pane_lines.capacity,a->inspector_sql_lines.width,t->sql_pane_lines.width,a->inspector_sql_lines.scale,t->sql_pane_lines.scale,a->inspector_sql_lines.raster_size,t->sql_pane_lines.raster_size,a->pipeline_layout.sql_code.width,a->ui.raster_size,a->message==t->feedback_message_pointer,review_smoke_text_same(a->message,t->feedback_message),a->message_since,t->feedback_message_since,(unsigned long long)a->message_revision,(unsigned long long)t->feedback_message_revision);
        dx_lock(&a->worker.mutex); fprintf(stderr,"Review feedback worker jobs=%llu/%llu latest=%llu/%llu active=%llu running=%d pending=%d reply=%d request=%llu\n",(unsigned long long)a->worker.jobs_started,(unsigned long long)t->query_jobs,(unsigned long long)a->worker.latest_id,(unsigned long long)t->query_latest,(unsigned long long)a->worker.active_id,a->worker.running,a->worker.pending ? (int)a->worker.pending->kind : -1,a->worker.reply ? (int)a->worker.reply->kind : -1,(unsigned long long)t->request); dx_unlock(&a->worker.mutex);
    }
    fprintf(stderr,"Review feedback mismatch stage=%d case=%d phase=%d modal=%d report=%d selected=%d table=%d pivot=%d busy=%d/%d values=%lld/%llu loading=%d failed=%d target=%s actual=%s preview=%d/%d/%llu query_complete=%d resume=%d state=%s\n",t->stage,item,phase,(int)a->modal,(int)r->phase,a->selected_object,t->feedback_table,t->feedback_pivot,a->busy,a->io_busy,(long long)a->values_total,(unsigned long long)a->values_scroll,a->values_loading,a->values_failed,t->author_target,r->target.widget.id,a->step_preview.active,a->step_preview.loading,(unsigned long long)a->step_preview.request,t->query_complete,t->feedback_resume,r->frame.state_text);
    for (size_t i=0;i<r->registry->count;i++) { const DxReviewWidget *widget=&r->registry->widgets[i]; if (strstr(widget->id,"transformations") || strstr(widget->id,"preview.") || strstr(widget->id,"filter.values") || strstr(widget->id,"status.") || strstr(widget->id,"readout.")) fprintf(stderr,"Review feedback widget=%s parent=%s role=%s enabled=%d eligible=%d kind=%s sheet=%llu owner=%llu source=%llu schema=%llu query=%llu index=%llu/%llu field=%s value=%d/%d/%s bounds=%.2f,%.2f,%.2f,%.2f clip=%.2f,%.2f,%.2f,%.2f\n",widget->id,widget->parent_id,widget->role,widget->enabled,widget->input_eligible,widget->data.kind,(unsigned long long)widget->data.sheet_id,(unsigned long long)widget->data.object_id,(unsigned long long)widget->data.source_id,(unsigned long long)widget->data.schema_generation,(unsigned long long)widget->data.query_generation,(unsigned long long)widget->data.row,(unsigned long long)widget->data.column,widget->data.field,widget->value.present,widget->value.is_null,widget->value.exact_text,widget->bounds.x,widget->bounds.y,widget->bounds.width,widget->bounds.height,widget->clip.x,widget->clip.y,widget->clip.width,widget->clip.height); }
    return -1;
}
static void review_smoke_long_field_remember(App *a,ReviewSmoke *t) {
    review_smoke_feedback_remember(a,t); t->long_search=a->pivot_search; t->dialog_text=dx_strdup(a->pivot_search.text);
    t->workspace_match_pointer=a->field_matches; t->workspace_match_count=a->field_match_count; t->workspace_match_capacity=a->field_match_capacity; t->workspace_schema_count=a->field_schema_count;
    if (a->field_match_count) { size_t bytes=a->field_match_count*sizeof(size_t); t->workspace_matches=dx_alloc(bytes); memcpy(t->workspace_matches,a->field_matches,bytes); }
    t->workspace_source_id=a->field_source_id; t->workspace_source_revision=a->field_source_revision; t->workspace_match_generation=a->field_match_generation; t->workspace_search_revision=a->field_search_revision;
    t->workspace_search_focus=a->pivot_search_focus; t->workspace_field_scroll=a->pivot_field_scroll;
}
static bool review_smoke_long_field_same(App *a,ReviewSmoke *t) {
    return a->book.active==t->long_sheet && review_smoke_feedback_same(a,t,3) && !memcmp(&a->pivot_search,&t->long_search,sizeof(TextInput)) && !strcmp(a->pivot_search.text,t->dialog_text) &&
        a->field_matches_valid && a->field_matches==t->workspace_match_pointer && a->field_match_count==t->workspace_match_count && a->field_match_capacity==t->workspace_match_capacity && a->field_schema_count==t->workspace_schema_count && (!a->field_match_count || !memcmp(a->field_matches,t->workspace_matches,a->field_match_count*sizeof(size_t))) &&
        a->field_source_id==t->workspace_source_id && a->field_source_revision==t->workspace_source_revision && a->field_match_generation==t->workspace_match_generation && a->field_search_revision==t->workspace_search_revision && a->pivot_search_focus==t->workspace_search_focus && a->pivot_field_scroll==t->workspace_field_scroll;
}
static bool review_smoke_long_field_targets(App *a,ReviewSmoke *t) {
    DxReviewRegistry *registry=a->review->registry; const SheetObject *table=&active_sheet(a)->objects.items[t->feedback_table]; char document[128],id[128],more_id[128]; review_document_id(a,document);
    snprintf(id,sizeof(id),"pane.fields.field.f%016llx",(unsigned long long)review_field_hash(t->long_field)); snprintf(more_id,sizeof(more_id),"%s.more",id);
    const DxReviewWidget *row=dx_review_find(registry,id),*more=dx_review_find(registry,more_id),*domain=dx_review_find(registry,"filter.values");
    if (!row || !more || !domain || !registry->truncated || registry->frame.truncated || a->review->frame.truncated || registry->duplicate_count || registry->invalid_count || registry->overflow_count || review_field_hash(t->long_field)==review_field_hash(row->data.field)) return false;
    const DxReviewWidget *fields[]={row,more};
    for (size_t i=0;i<2;i++) {
        const DxReviewWidget *widget=fields[i]; const DxReviewDataIdentity *data=&widget->data;
        if (widget->identity_truncated || !widget->truncated || strcmp(data->kind,"available-field-truncated") || strcmp(data->document_id,document) || data->sheet_id!=a->book.active+1 || data->object_id!=table->id || data->source_id!=table->id || data->schema_generation!=a->field_schema_generation || data->query_generation!=table->revision || data->has_cell || data->row!=1 || data->column || strlen(data->field)!=127 || strncmp(data->field,t->long_field,124) || strcmp(data->field+124,"…") || widget->enabled || widget->input_eligible || !widget->value.present || widget->value.is_null || widget->value.truncated || strcmp(widget->value.type,"VARCHAR") || strcmp(widget->value.exact_text,t->long_field) || !review_smoke_sheet_pick(a,widget,"pane.fields.list")) return false;
    }
    if (strcmp(more->parent_id,row->id)) return false;
    const char *controls[]={"filter.panel","filter.search","filter.all","filter.apply","filter.values","filter.values.scrollbar","filter.values.scrollbar.thumb"};
    for (size_t i=0;i<sizeof(controls)/sizeof(*controls);i++) {
        const DxReviewWidget *widget=dx_review_find(registry,controls[i]); if (!widget) return false; const DxReviewDataIdentity *data=&widget->data;
        if (widget->identity_truncated || !widget->truncated || strcmp(data->kind,"filter-domain-truncated") || strcmp(data->document_id,document) || data->sheet_id!=a->book.active+1 || data->object_id!=table->id || data->source_id!=table->id || data->schema_generation!=table->revision || data->query_generation!=table->revision || data->has_cell || data->row || data->column!=1 || strlen(data->field)!=127 || strncmp(data->field,t->long_field,124) || strcmp(data->field+124,"…")) return false;
    }
    const DxReviewWidget *value=NULL;
    for (size_t i=0;i<registry->count;i++) {
        const DxReviewWidget *widget=&registry->widgets[i]; if (strncmp(widget->id,"filter.value.",13)) continue;
        const DxReviewDataIdentity *data=&widget->data; DistinctPage *page=cached_values(a,data->row/DX_DISTINCT_ROWS*DX_DISTINCT_ROWS);
        if (widget->identity_truncated || !widget->truncated || strcmp(widget->parent_id,"filter.values") || strcmp(data->kind,"filter-domain-truncated") || strcmp(data->document_id,document) || data->sheet_id!=a->book.active+1 || data->object_id!=table->id || data->source_id!=table->id || data->schema_generation!=table->revision || data->query_generation!=table->revision || data->has_cell || data->column!=1 || strlen(data->field)!=127 || strncmp(data->field,t->long_field,124) || strcmp(data->field+124,"…") || !page || data->row-page->offset>=page->count || !widget->enabled || !widget->input_eligible || !widget->value.present || widget->value.is_null || widget->value.truncated || strcmp(widget->value.type,"VARCHAR") || strcmp(widget->value.exact_text,page->values[data->row-page->offset].value) || !review_smoke_sheet_pick(a,widget,"filter.panel")) return false;
        if (!value) value=widget;
    }
    if (!value || a->values_total!=96 || a->values_failed || a->value_choice_count || !a->values_all || dx_review_find(registry,"filter.values.status")) return false;
    snprintf(t->author_target,sizeof(t->author_target),"%s",value->id); return true;
}
static int review_smoke_long_field_tick(App *a,ReviewSmoke *t) {
    int phase=t->stage-REVIEW_FEEDBACK_END; DxReviewCapture *r=a->review;
    if (!phase) {
        if (r->phase!=DX_REPORT_IDLE || !smoke_work_idle(a)) return 0;
        if (!t->long_sheet_added) {
            if (a->modal!=MODAL_NONE || !t->feedback_fixture || !FileExists(t->feedback_path)) goto failed;
            size_t count=a->book.count; new_sheet(a); if (a->book.count!=count+1 || a->analysis.kind!=SOURCE_NONE) goto failed;
            t->long_sheet=a->book.active; t->long_sheet_added=true;
            memcpy(t->long_field,"City",4); for (size_t i=0;i<70;i++) memcpy(t->long_field+4+2*i,"β",2); memcpy(t->long_field+144,"_exact",7);
            run_action(a,ACTION_RESET_LAYOUT); input_set(&a->pivot_search,""); ui_set_scale(&a->ui,1); SetWindowSize(1280,900); return 0;
        }
        if (a->book.active!=t->long_sheet || strlen(t->long_field)!=150) goto failed;
        if (!t->long_table_added) { grid_select_cell(&a->grid,0,0,false); keyboard_object(a); if (!import_table(a,t->feedback_path)) goto failed; t->feedback_table=a->selected_object; t->long_table_added=true; return 0; }
        if (!object_ready(a,(size_t)t->feedback_table)) return 0;
        if (!t->long_query_submitted) {
            begin_source_query(a); if (a->modal!=MODAL_SOURCE_QUERY || a->query_object!=t->feedback_table) goto failed;
            char *quoted=dx_quote(t->long_field,'"'),*query=dx_printf("SELECT ID, City AS %s, Amount FROM input",quoted); free(quoted); input_set(&a->input,query); free(query); submit_source_query(a);
            if (!a->query_loading || a->pending_kind!=JOB_VALIDATE_SQL) goto failed; t->long_query_submitted=true; return 0;
        }
        if (a->modal==MODAL_SOURCE_QUERY) { if (a->query_error) goto failed; return 0; }
        Page *page=&a->objects[t->feedback_table].page; SheetObject *table=&active_sheet(a)->objects.items[t->feedback_table];
        if (a->modal!=MODAL_NONE && a->modal!=MODAL_FILTER) goto failed;
        if (page->total!=96 || page->column_count!=3 || strcmp(page->columns[1].name,t->long_field) || !table->source.query) goto failed;
        if (!t->long_filter_opened) { open_table_filter(a,(size_t)t->feedback_table,1); if (a->modal!=MODAL_FILTER) goto failed; t->long_filter_opened=true; return 0; }
        if (a->modal!=MODAL_FILTER || a->ui.scale!=1 || GetScreenWidth()!=1280 || GetScreenHeight()!=900 || a->popup_presence.offset || a->popup_presence.alpha!=1 || !a->filter_layout.valid || a->values_total<0 || a->values_loading || !a->field_matches_valid || a->field_source_id!=table->id || a->field_source_revision!=table->revision || a->field_schema_count!=3 || a->field_visible<2 || a->pivot_field_scroll || a->pivot_search.undo_count || a->pivot_search.redo_count || !review_can_capture(a) || a->statistics_state==STATS_WAITING || a->statistics_state==STATS_CALCULATING) return 0;
        review_smoke_long_field_remember(a,t); ui_note_key_press(KEY_PERIOD,DX_MOD_SUPER|DX_MOD_SHIFT); return 1;
    }
    if (phase==1) { if (r->phase!=DX_REPORT_PICK) return 0; if (!review_smoke_long_field_same(a,t) || !review_smoke_long_field_targets(a,t)) goto failed; r->toolbar_top=true; review_smoke_pick(a,t->author_target); return 1; }
    if (phase==2) { if (r->phase!=DX_REPORT_NOTE) return 0; if (strcmp(r->target.widget.id,t->author_target) || r->target.widget.identity_truncated || !r->target.widget.truncated || r->target.widget.value.truncated || !review_smoke_long_field_same(a,t)) goto failed; ui_note_key_press(KEY_ESCAPE,0); return 1; }
    if (r->phase!=DX_REPORT_IDLE) return 0;
    if (!review_smoke_long_field_same(a,t)) goto failed;
    printf("PASS: native150-byte UTF8 source-query field remains selectable by complete stable hash/source/schema/column scope with honest124-byte UTF8 prefix plus ellipsis; actual Fields/More and filter controls/typed values retain definitions/input/history/Pages/jobs through capture/pick/cancel\n");
    ui_note_key_press(KEY_ESCAPE,0); review_smoke_author_release(t); printf("Review smoke completed [0,%d)\n",REVIEW_SMOKE_STAGES); return 1;
failed:
    fprintf(stderr,"Review long field mismatch stage=%d phase=%d modal=%d report=%d sheet=%zu/%zu table=%d setup=%d/%d/%d/%d alias_bytes=%zu values=%lld focus=%d fields=%zu/%d revision=%llu/%llu registry=%zu duplicate=%zu invalid=%zu overflow=%zu truncated=%d/%d target=%s actual=%s error=%s\n",t->stage,phase,(int)a->modal,(int)r->phase,a->book.active,t->long_sheet,t->feedback_table,t->long_sheet_added,t->long_table_added,t->long_query_submitted,t->long_filter_opened,strlen(t->long_field),(long long)a->values_total,a->filter_focus,a->field_match_count,a->field_visible,(unsigned long long)a->field_source_revision,(unsigned long long)(t->long_table_added ? active_sheet(a)->objects.items[t->feedback_table].revision : 0),r->registry->count,r->registry->duplicate_count,r->registry->invalid_count,r->registry->overflow_count,r->registry->truncated,r->frame.truncated,t->author_target,r->target.widget.id,a->query_error ? a->query_error : a->message ? a->message : "");
    for (size_t i=0;i<r->registry->count;i++) { const DxReviewWidget *widget=&r->registry->widgets[i]; if (strstr(widget->id,"pane.fields") || !strncmp(widget->id,"filter.",7)) fprintf(stderr,"Review long field widget=%s parent=%s kind=%s scope=%llu/%llu/%llu/%llu/%llu position=%llu/%llu field_bytes=%zu flags=%d/%d value=%d/%d/%d/%zu enabled=%d rect=%.1f,%.1f,%.1f,%.1f\n",widget->id,widget->parent_id,widget->data.kind,(unsigned long long)widget->data.sheet_id,(unsigned long long)widget->data.object_id,(unsigned long long)widget->data.source_id,(unsigned long long)widget->data.schema_generation,(unsigned long long)widget->data.query_generation,(unsigned long long)widget->data.row,(unsigned long long)widget->data.column,strlen(widget->data.field),widget->identity_truncated,widget->truncated,widget->value.present,widget->value.is_null,widget->value.truncated,strlen(widget->value.exact_text),widget->enabled,widget->bounds.x,widget->bounds.y,widget->bounds.width,widget->bounds.height); } return -1;
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
        static const struct { int exclusive_end; int (*tick)(App *,ReviewSmoke *); } handlers[]={
            {REVIEW_AUTHOR_END,review_smoke_author_tick},
            {REVIEW_DIALOG_END,review_smoke_dialog_tick},
            {REVIEW_PLOT_END,review_smoke_plot_identity_tick},
            {REVIEW_SHEET_END,review_smoke_sheet_tick},
            {REVIEW_QUERY_END,review_smoke_query_tick},
            {REVIEW_RELATION_END,review_smoke_relation_tick},
            {REVIEW_SORT_END,review_smoke_sort_tick},
            {REVIEW_STATS_END,review_smoke_stats_tick},
            {REVIEW_INSPECTOR_END,review_smoke_inspector_tick},
            {REVIEW_SETTINGS_END,review_smoke_settings_tick},
            {REVIEW_FORMAT_END,review_smoke_format_tick},
            {REVIEW_SQL_END,review_smoke_sql_tick},
            {REVIEW_OWNER_END,review_smoke_owner_tick},
            {REVIEW_GUIDE_END,review_smoke_guide_tick},
            {REVIEW_CELL_END,review_smoke_cell_tick},
            {REVIEW_ERROR_END,review_smoke_error_tick},
            {REVIEW_QUERY_FEEDBACK_END,review_smoke_query_failure_tick},
            {REVIEW_WORKSPACE_END,review_smoke_workspace_tick},
            {REVIEW_FEEDBACK_END,review_smoke_feedback_tick},
            {REVIEW_SMOKE_STAGES,review_smoke_long_field_tick},
        };
        int progress=-1;
        for (size_t i=0;i<sizeof(handlers)/sizeof(*handlers);i++)
            if (t->stage<handlers[i].exclusive_end) { progress=handlers[i].tick(a,t); break; }
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
        {
            /* These real controllers open before the first reporting frame.
               Pinned origins must not inherit its still-empty document ID. */
            char document[128]; snprintf(document,sizeof(document),"%s-document-%u",r->frame.session_id,r->document_serial);
            if (r->frame.document_id[0]) goto failed;
            a->selected_object=-1; grid_select_cell(&a->grid,0,1,false); inspect_value(a);
            if (a->modal!=MODAL_INSPECT || !a->inspector_null || !a->inspector_origin_valid || strcmp(a->inspector_origin.document_id,document)) goto failed;
            a->modal=MODAL_NONE; inspector_free(a); open_modal(a,MODAL_SQL);
            if (a->modal!=MODAL_SQL || !a->sql_origin_valid || strcmp(a->sql_origin.document_id,document) || a->sql_origin.object_id || a->sql_origin.source_id || a->sql_origin.query_generation!=a->analysis_revision || r->frame.document_id[0]) goto failed;
            a->modal=MODAL_NONE;
            puts("PASS: first-open inspector/Generated SQL pin current document before any captured frame");
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
