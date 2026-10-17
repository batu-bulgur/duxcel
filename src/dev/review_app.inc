/* Review-only App adapter. Included after the normal controllers so targets
   use their actual geometry/cache, with no database access or retained cells. */
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
    w.layer=layer; w.enabled=enabled; w.input_eligible=enabled && a->modal==MODAL_NONE; w.focused=focused;
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
        UIMenuLayout m=ui_menu_layout((Vector2){r.condition.x,r.condition.y+r.condition.height+2*s},count,27,r.condition.width/s,s,(Vector2){GetScreenWidth(),GetScreenHeight()});
        review_control(a,"filter.conditions","filter.conditions","popup/column-filter/conditions","menu","Conditions",m.panel,10,true,false,true,__FILE__,__LINE__);
        for (int i=0;i<count;i++) { char id[64]; snprintf(id,sizeof(id),"filter.condition.option.%s",review_filter_op(choices[i])); DxReviewWidget *w=review_filter_item(a,id,"filter.choose_condition","menuitem",filter_condition_label(a,choices[i]),box(m.items.x,m.items.y+i*27*s,m.items.width,27*s),m.panel,111,-1,true); if (w) { w->focused=i==a->filter_condition_index; snprintf(w->parent_id,sizeof(w->parent_id),"filter.conditions"); } }
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
static bool review_can_capture(App *a) {
    return a->review && a->review->phase==DX_REPORT_IDLE && !a->grid.dragging && !a->grid.moving && a->grid.resizing<0 && !a->grid.scrollbar && !a->chart_resizing && a->pivot_drag_field<0 && a->pivot_drag_role<0 && !a->matrix_selecting && !(a->modal==MODAL_GROUP && a->summary_drag>=0) && !a->split_side && !a->split_pipeline && !a->steps_drag && !a->workbook_drag && !a->field_drag && !a->values_drag && !a->input.dragging && !a->filter_term.dragging && !a->filter_end.dragging;
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
#define REVIEW_SMOKE_STAGES 26
typedef struct {
    int stage; uint64_t request,jobs; GridLocation selection; Cell *page;
    size_t cursor,anchor,history; bool dirty; char root[4096];
} ReviewSmoke;
static void review_smoke_click(Rectangle b) { float x=b.x+b.width*.5f,y=b.y+b.height*.5f; ui_note_pointer_gesture((DxPointerGesture){x,y,x,y,0,false}); }
static void review_smoke_pick(App *a,const char *id) {
    const DxReviewWidget *w=dx_review_find(a->review->registry,id);
    if (w) review_smoke_click((Rectangle){w->bounds.x,w->bounds.y,w->bounds.width,w->bounds.height});
}
static bool review_smoke_same(App *a,ReviewSmoke *t) {
    GridLocation g=grid_location(&a->grid);
    return a->modal==MODAL_FILTER && a->filter_focus==3 && a->input.cursor==t->cursor && a->input.anchor==t->anchor && a->request_id==t->request && a->page.cells==t->page && a->book.dirty==t->dirty && active_sheet(a)->undo_actions_count==t->history && g.row==t->selection.row && g.col==t->selection.col && g.end_row==t->selection.end_row && g.end_col==t->selection.end_col;
}
static void review_smoke_tick(App *a,ReviewSmoke *t) {
    DxReviewCapture *r=a->review; if (!r || t->stage<0 || t->stage>=REVIEW_SMOKE_STAGES) return;
    switch (t->stage) {
    case 0:
        if (a->busy || a->page.count_pending || a->page.column_count!=3) return;
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
        close_command_group(a);
        printf("PASS: native review filter/selection isolation, typed frozen cell, disabled ribbon/overflow, pane controls, 100/150 percent, capture/write retry, durable reports and queued query reply\nReview smoke completed [0,%d)\n",REVIEW_SMOKE_STAGES); break;
    }
    }
    t->stage++; return;
failed:
    fprintf(stderr,"FAIL: native review stage=%d phase=%d target=%s error=%s\n",t->stage,(int)r->phase,r->target.widget.id,r->error); t->stage=-1;
}
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
