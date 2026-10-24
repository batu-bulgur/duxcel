/* Private real-App input/plan/Undo/Save fixture; production has no hook/state. */
static unsigned format_plan_stage,format_plan_wait,format_plan_checks;
static int format_plan_result;
static uint64_t format_plan_request,format_plan_command_revision;
static const char *format_plan_path="build/tests/generated/format-plan.dxls";
#define FORMAT_GUI_CHECK(x) do { format_plan_checks++; if (!(x)) { fprintf(stderr,"FAIL format plan GUI stage %u:%d: %s (%s)\n",format_plan_stage,__LINE__,#x,a->message ? a->message : ""); format_plan_result=-1; return; } } while (0)
static void format_plan_test_frame(AppSession *session) {
    App *a=&session->app; Worksheet *s=active_sheet(a);
    if (format_plan_result || a->busy || a->io_busy || !s || ++format_plan_wait<12) return;
    format_plan_wait=0;
    switch (format_plan_stage) {
    case 0: {
        remove(format_plan_path); char *error=NULL;
        FORMAT_GUI_CHECK(cells_set(&s->cells,1,1,"1234.56",&error) && !error); worksheet_record(s,EDIT_CELLS);
        grid_select_cell(&a->grid,1,1,false); a->ribbon_tab=RIBBON_HOME; set_ribbon_mode(a,0);
        ui_set_scale(&a->ui,1); SetWindowSize(1280,820); format_plan_request=a->request_id; break;
    }
    case 1: {
        format_plan_command_revision=a->book.command_revision;
        int index=ui_ribbon_action_index(RIBBON_HOME,ACTION_BOLD);
        FORMAT_GUI_CHECK(index>=0 && index<ribbon_counts[RIBBON_HOME]); Rectangle r=ribbon_rect(a,a->ribbon_layout.commands[index]);
        FORMAT_GUI_CHECK(r.width>0 && r.height>0);
        ui_note_pointer_gesture((DxPointerGesture){r.x+r.width*.5f,r.y+r.height*.5f,r.x+r.width*.5f,r.y+r.height*.5f,0,false}); break;
    }
    case 2:
        FORMAT_GUI_CHECK(a->book.command_revision==format_plan_command_revision+1);
        FORMAT_GUI_CHECK(format_at(&s->formats,1,1).flags==FORMAT_BOLD && s->undo_actions_count==2 && !strcmp(cells_get(&s->cells,1,1),"1234.56"));
        ui_note_key_press(KEY_I,DX_MOD_SUPER); break;
    case 3:
        FORMAT_GUI_CHECK(a->book.command_revision==format_plan_command_revision+2);
        FORMAT_GUI_CHECK(format_at(&s->formats,1,1).flags==(FORMAT_BOLD|FORMAT_ITALIC) && s->undo_actions_count==3);
        ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 4: {
        FORMAT_GUI_CHECK(format_at(&s->formats,1,1).flags==FORMAT_BOLD && s->redo_actions_count==1);
        FormatSnapshot plan={0}; CellFormat value=format_default(); value.flags=FORMAT_BOLD; char *error=NULL;
        uint64_t revision=s->formats.revision;
        FORMAT_GUI_CHECK(a->book.command_revision==format_plan_command_revision+3 && apply_format(a,FORMAT_BOLD,value));
        FORMAT_GUI_CHECK(a->book.command_revision==format_plan_command_revision+3 && s->redo_actions_count==1);
        size_t undo=s->formats.undo_count,redo=s->formats.redo_count;
        FORMAT_GUI_CHECK(format_plan_spatial(&s->formats,(FormatRange){1,1,1,1},FORMAT_BOLD,value,&plan,&error) && !error && format_snapshot_matches(&s->formats,&plan));
        format_snapshot_free(&plan);
        const unsigned char definition[]={'a',0,'b'}; FormatScopeView scope={0,definition,sizeof(definition)}; value.color=2;
        FORMAT_GUI_CHECK(format_plan_scoped(&s->formats,(FormatRange){1,1,1,1},FORMAT_COLOR,value,&scope,1,true,&plan,&error) && !error && plan.source_aware && !format_snapshot_matches(&s->formats,&plan));
        format_snapshot_free(&plan);
        FORMAT_GUI_CHECK(s->formats.revision==revision && s->formats.undo_count==undo && s->formats.redo_count==redo && !s->formats.source_aware && s->redo_actions_count==1 && !format_at(&s->formats,1,1).color);
        ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    }
    case 5:
        FORMAT_GUI_CHECK(format_at(&s->formats,1,1).flags==(FORMAT_BOLD|FORMAT_ITALIC) && !s->redo_actions_count && a->request_id==format_plan_request);
        ui_set_scale(&a->ui,1.5f); SetWindowSize(640,640); ui_note_key_press(KEY_U,DX_MOD_SUPER); break;
    case 6:
        FORMAT_GUI_CHECK(a->book.command_revision==format_plan_command_revision+5);
        FORMAT_GUI_CHECK(format_at(&s->formats,1,1).flags==7 && !strcmp(cells_get(&s->cells,1,1),"1234.56") && a->request_id==format_plan_request);
        workspace_job(a,JOB_SAVE,format_plan_path); break;
    case 7:
        FORMAT_GUI_CHECK(!a->book.dirty); workspace_job(a,JOB_LOAD,format_plan_path); break;
    case 8:
        FORMAT_GUI_CHECK(a->book.command_revision==format_plan_command_revision+5);
        FORMAT_GUI_CHECK(!a->book.dirty && !s->undo_actions_count && !strcmp(cells_get(&s->cells,1,1),"1234.56") && format_at(&s->formats,1,1).flags==7 && !s->formats.source_aware);
        ui_set_scale(&a->ui,1); SetWindowSize(1280,820); ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 9:
        FORMAT_GUI_CHECK(!a->book.dirty && !s->undo_actions_count && format_at(&s->formats,1,1).flags==7 && !strcmp(cells_get(&s->cells,1,1),"1234.56"));
        format_plan_result=1; printf("PASS: native cancellable format workflow (10 stages, %u checks)\n",format_plan_checks); break;
    }
    format_plan_stage++;
}
int app_test_format_plan_result(void) { return format_plan_result; }
#undef FORMAT_GUI_CHECK
