/* Selected source presentation only. No stored connection mode or row owner. */
static bool source_details_view(App *a,SourceProvenanceView *out) {
#ifdef DUXCEL_BROWSER_HOST
    /* Browser uploaded virtual locators need a separate provenance surface. */
    (void)a; (void)out; return false;
#else
    Worksheet *sheet=active_sheet(a); if (!sheet) return false;
    const SheetObject *table=objects_source(&sheet->objects,selected_object(a));
    const Analysis *source=table ? &table->source : &a->analysis;
    SourceViewState state=SOURCE_VIEW_LOADING;
    if (table) {
        size_t index=(size_t)(table-sheet->objects.items); const ObjectView *view=a->objects+index;
        bool associated=view->id==table->id && view->revision==table->revision && view->source_revision==object_source_revision(a,table);
        bool pending=a->busy && a->pending_object_id==table->id && (a->pending_kind==JOB_PIVOT || a->pending_kind==JOB_COUNT);
        if (associated && view->failed) state=SOURCE_VIEW_UNAVAILABLE;
        else if (associated && !pending && object_ready(a,index)) state=SOURCE_VIEW_READY;
    } else {
        bool pending=!a->object_import.active && !app_primary_source_pending(a) && a->busy && (a->pending_kind==JOB_PAGE || (a->pending_kind==JOB_COUNT && !a->pending_object_id));
        if (a->fetch_failed) state=SOURCE_VIEW_UNAVAILABLE;
        else if (!pending && !a->page_stale && a->page.column_count && !a->page.count_pending) state=SOURCE_VIEW_READY;
    }
    return ui_source_provenance(source,state,out);
#endif
}
static void source_details_free(App *a) {
    free(a->source_details_text); a->source_details_text=NULL;
    text_lines_free(&a->source_details_lines); a->source_details_scroll=0; a->source_details_key=0; a->source_details_bar=(UIScrollbar){0};
}
static void source_details_refresh(App *a,const SourceProvenanceView *view) {
    uint64_t key=UINT64_C(1469598103934665603);
    const char *values[]={view->path,view->schema,view->table,view->query};
    for (size_t i=0;i<4;i++) {
        key^=values[i]!=NULL; key*=UINT64_C(1099511628211);
        if (values[i]) for (const unsigned char *p=(const unsigned char *)values[i];*p;p++) { key^=*p; key*=UINT64_C(1099511628211); }
        key^=UINT64_C(255); key*=UINT64_C(1099511628211);
    }
    key^=(uint64_t)view->kind<<8 | view->state;
    if (a->source_details_text && a->source_details_key==key) return;
    char *text=dx_printf("Connection: %s · External · Read-only\nAvailability: %s\n\nSource path\n%s\n\nSchema\n%s\n\nTable or view\n%s\n\nSource query\n%s\n\nSave keeps definitions and formatting. Source rows remain external. Refresh requires the original source.",
        ui_source_kind_label(view->kind),ui_source_state_label(view->state),view->path,
        view->schema ? view->schema : "(not specified)",view->table ? view->table : "(not specified)",view->query ? view->query : "(original source reader)");
    source_details_free(a); a->source_details_text=text; a->source_details_key=key;
}
static void begin_source_details(App *a) {
    SourceProvenanceView view;
    if (!source_details_view(a,&view)) return;
    source_details_free(a); source_details_refresh(a,&view);
    a->modal=MODAL_SOURCE_DETAILS; a->consume_grid_input=true;
}
static void draw_source_details(App *a) {
    SourceProvenanceView view;
    if (!source_details_view(a,&view) || ui_pressed(KEY_ESCAPE)) { source_details_free(a); a->modal=MODAL_NONE; a->consume_grid_input=true; return; }
    source_details_refresh(a,&view);
    UIContext *u=&a->ui; float s=u->scale,w=fminf(900*s,GetScreenWidth()-32*s),h=fminf(650*s,GetScreenHeight()-60*s);
    float x=(GetScreenWidth()-w)/2,y=(GetScreenHeight()-h)/2;
    Rectangle panel=box(x,y,w,h),content=box(x+24*s,y+66*s,w-60*s,h-152*s);
    DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),(Color){20,32,45,95});
    DrawRectangleRec(box(x+4*s,y+5*s,w,h),(Color){20,32,45,36}); DrawRectangleRec(panel,UI_SURFACE); DrawRectangleLinesEx(panel,1,UI_BORDER);
    ui_text(u,"Source details",x+24*s,y+20*s,19,UI_TEXT);
    TextLines *lines=&a->source_details_lines;
    if (!lines->offsets || fabsf(lines->width-content.width)>.5f || lines->scale!=s || lines->raster_size!=u->raster_size) text_lines_layout(lines,u,a->source_details_text,content.width);
    size_t visible=(size_t)fmaxf(1,floorf(content.height/(21*s))),max=lines->count>visible ? lines->count-visible : 0;
    int delta=0;
    if (CheckCollisionPointRec(ui_mouse_position(),content)) delta=ui_scroll_steps(&a->source_details_wheel,-GetMouseWheelMoveV().y*3);
    if (ui_key(KEY_UP)) delta--; if (ui_key(KEY_DOWN)) delta++;
    if (ui_key(KEY_PAGE_UP)) delta-=(int)visible; if (ui_key(KEY_PAGE_DOWN)) delta+=(int)visible;
    if (ui_pressed(KEY_HOME)) a->source_details_scroll=0; if (ui_pressed(KEY_END)) a->source_details_scroll=max;
    if (delta<0) a->source_details_scroll=a->source_details_scroll<(size_t)-delta ? 0 : a->source_details_scroll-(size_t)-delta;
    else a->source_details_scroll+=(size_t)delta;
    if (a->source_details_scroll>max) a->source_details_scroll=max;
    Rectangle track=box(content.x+content.width+10*s,content.y,8*s,content.height);
    float position=(float)a->source_details_scroll;
    if (ui_scrollbar(u,track,(float)lines->count,(float)visible,&position,false,&a->source_details_bar,true)) a->consume_grid_input=true;
    a->source_details_scroll=(size_t)position;
    text_lines_draw(lines,u,a->source_details_text,a->source_details_scroll,content);
    if (max) REVIEW_AUTHOR_SCROLL(a,"source_details",track,ui_scrollbar_thumb(track,(float)lines->count,(float)visible,position,false,s),panel,a->source_details_bar.dragging);
    char progress[160]; snprintf(progress,sizeof(progress),"Line %zu of %zu%s",a->source_details_scroll+1,lines->count,lines->limited ? " · display limit reached; Copy includes complete details" : "");
    ui_text_clip(u,progress,box(x+17*s,y+h-75*s,w-34*s,25*s),10,UI_MUTED,false);
    Rectangle copy=box(x+24*s,y+h-40*s,140*s,28*s),close=box(x+w-112*s,y+h-40*s,88*s,28*s);
    REVIEW_AUTHOR_PANEL(a,"source_details","Source details",panel,box(x+24*s,y+20*s,w-48*s,30*s));
    REVIEW_AUTHOR_ITEM(a,"source_details","copy","source.copy_details","button","Copy details",copy,panel,true,false,false,NULL);
    REVIEW_AUTHOR_ITEM(a,"source_details","close","source.close_details","button","Close",close,panel,true,false,false,NULL);
    if (ui_button(u,copy,"Copy details",false,true) || (ui_key_shortcut(KEY_C) && ui_pressed(KEY_C))) copy_text(a,a->source_details_text,"Source details copied.");
    if (ui_button(u,close,"Close",true,true) || ui_pressed(KEY_ENTER) || ui_pressed(KEY_KP_ENTER)) { source_details_free(a); a->modal=MODAL_NONE; a->consume_grid_input=true; }
}
