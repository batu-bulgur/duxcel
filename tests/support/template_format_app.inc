/* Existing unbound templates retain manual finite styles and ordinary input. */
static unsigned template_format_stage,template_format_wait,template_format_checks;
static int template_format_result;
static uint64_t template_format_revision,template_format_order,template_format_request;
static ObjectStore template_format_objects;
static SheetObject template_format_items[2];
static SourceVersions template_format_sources;
#define TEMPLATE_FORMAT_CHECK(x) do { template_format_checks++; if (!(x)) { fprintf(stderr,"FAIL template format stage %u:%d: %s (%s)\n",template_format_stage,__LINE__,#x,a->message ? a->message : ""); template_format_result=-1; return; } } while (0)
static bool template_format_immutable(const App *a,const Worksheet *s) {
    if (a->request_id!=template_format_request || a->analysis.kind!=SOURCE_NONE || s->analysis.kind!=SOURCE_NONE || s->formats.source_aware ||
        memcmp(&template_format_objects,&s->objects,sizeof(s->objects)) || memcmp(template_format_items,s->objects.items,sizeof(template_format_items)) ||
        memcmp(&template_format_sources,&s->source_versions,sizeof(s->source_versions))) return false;
    for (size_t i=0;i<s->formats.undo_count;i++) if (s->formats.undo[i].source_aware) return false;
    for (size_t i=0;i<s->formats.redo_count;i++) if (s->formats.redo[i].source_aware) return false;
    return true;
}
static void template_format_test_frame(AppSession *session) {
    App *a=&session->app; Worksheet *s=active_sheet(a);
    if (template_format_result || !s || a->busy || a->io_busy || ++template_format_wait<20) return;
    template_format_wait=0; TEMPLATE_FORMAT_CHECK(a->modal!=MODAL_ERROR);
    switch (template_format_stage) {
    case 0: {
        TEMPLATE_FORMAT_CHECK(a->analysis.kind==SOURCE_NONE && !s->objects.count && !s->formats.source_aware);
        ManualFormatTarget target; FormatSnapshot plan={0}; CellFormat bold=format_default(); bold.flags=FORMAT_BOLD;
        TEMPLATE_FORMAT_CHECK(workbook_prepare_manual_format(&a->book,a->book.active,&target,NULL) && format_plan_spatial(&s->formats,(FormatRange){1,1,1,1},FORMAT_BOLD,bold,&plan,NULL));
        TEMPLATE_FORMAT_CHECK(workbook_apply_manual_format_recorded(&a->book,&target,&plan,NULL,NULL).status==MANUAL_FORMAT_APPLIED);
        /* Same current authored-template setup as template-cells; these are not
           sourced creation commands and issue no source identity. */
        TEMPLATE_FORMAT_CHECK(objects_add(&s->objects,5,2,"Pivot template",NULL)==0); worksheet_record(s,EDIT_OBJECTS); a->book.command_revision++; workbook_history_note_edit(&a->book);
        TEMPLATE_FORMAT_CHECK(objects_add_chart(&s->objects,15,2,"Chart template",&(PivotDefinition){0},NULL)==1); worksheet_record(s,EDIT_OBJECTS); a->book.command_revision++; workbook_history_note_edit(&a->book);
        template_format_objects=s->objects; memcpy(template_format_items,s->objects.items,sizeof(template_format_items)); template_format_sources=s->source_versions;
        template_format_revision=a->book.command_revision; template_format_order=worksheet_history_high_water(); template_format_request=a->request_id;
        grid_select_cell(&a->grid,1,1,false); a->selected_object=-1; a->pivot_focus=a->chrome_focus=0; a->ribbon_focus=false;
        a->ribbon_tab=RIBBON_HOME; set_ribbon_mode(a,0); ui_set_scale(&a->ui,1); SetWindowSize(1280,820); a->statistics_enabled=false; break;
    }
    case 1: {
        TEMPLATE_FORMAT_CHECK(template_format_immutable(a,s)); CellFormat style=format_default(); style.flags=FORMAT_ITALIC;
        a->source_format.bytes=SIZE_MAX;
        TEMPLATE_FORMAT_CHECK(!apply_format(a,FORMAT_ITALIC,style) && a->book.command_revision==template_format_revision && worksheet_history_high_water()==template_format_order && format_at(&s->formats,1,1).flags==FORMAT_BOLD && template_format_immutable(a,s));
        a->source_format.bytes=0;
        int index=ui_ribbon_action_index(RIBBON_HOME,ACTION_ITALIC); TEMPLATE_FORMAT_CHECK(index>=0 && index<ribbon_counts[RIBBON_HOME]);
        Rectangle rect=ribbon_rect(a,a->ribbon_layout.commands[index]); TEMPLATE_FORMAT_CHECK(rect.width>0 && rect.height>0);
        ui_note_pointer_gesture((DxPointerGesture){rect.x+rect.width*.5f,rect.y+rect.height*.5f,rect.x+rect.width*.5f,rect.y+rect.height*.5f,0,false}); break;
    }
    case 2:
        TEMPLATE_FORMAT_CHECK(template_format_immutable(a,s) && format_at(&s->formats,1,1).flags==(FORMAT_BOLD|FORMAT_ITALIC) && a->book.command_revision==template_format_revision+1 && worksheet_history_high_water()==template_format_order+1);
        ui_note_key_press(KEY_U,DX_MOD_SUPER); break;
    case 3:
        TEMPLATE_FORMAT_CHECK(template_format_immutable(a,s) && format_at(&s->formats,1,1).flags==(FORMAT_BOLD|FORMAT_ITALIC|FORMAT_UNDERLINE) && a->book.command_revision==template_format_revision+2);
        ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 4: {
        TEMPLATE_FORMAT_CHECK(template_format_immutable(a,s) && format_at(&s->formats,1,1).flags==(FORMAT_BOLD|FORMAT_ITALIC) && a->book.command_revision==template_format_revision+3 && s->formats.redo_count==1);
        size_t redo=s->redo_actions_count; bool dirty=a->book.dirty; CellFormat style=format_default(); style.flags=FORMAT_ITALIC;
        TEMPLATE_FORMAT_CHECK(apply_format(a,FORMAT_ITALIC,style) && a->book.command_revision==template_format_revision+3 && worksheet_history_high_water()==template_format_order+2 && s->redo_actions_count==redo && s->formats.redo_count==1 && a->book.dirty==dirty && template_format_immutable(a,s));
        ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    }
    case 5:
        TEMPLATE_FORMAT_CHECK(template_format_immutable(a,s) && format_at(&s->formats,1,1).flags==(FORMAT_BOLD|FORMAT_ITALIC|FORMAT_UNDERLINE) && a->book.command_revision==template_format_revision+4 && worksheet_history_high_water()==template_format_order+2 && !s->formats.redo_count);
        template_format_result=1; printf("PASS: native template spatial format workflow (6 stages, %u checks)\n",template_format_checks); return;
    }
    template_format_stage++;
}
int app_test_template_format_result(void) { return template_format_result; }
unsigned app_test_template_format_stage(void) { return template_format_stage; }
