/* Private primary sort/filter input fixture; absent from production builds. */
static unsigned primary_transforms_stage,primary_transforms_wait,primary_transforms_checks;
static int primary_transforms_result;
static uint64_t primary_transforms_revision;
static const char *primary_transforms_path="build/tests/generated/primary-transforms.dxls";
#define TRANSFORM_GUI_CHECK(x) do { primary_transforms_checks++; if (!(x)) { fprintf(stderr,"FAIL primary transforms GUI stage %u:%d: %s (%s)\n",primary_transforms_stage,__LINE__,#x,a->message ? a->message : ""); primary_transforms_result=-1; return; } } while (0)
static void primary_transforms_test_frame(AppSession *session) {
    App *a=&session->app; Worksheet *s=active_sheet(a);
    if (primary_transforms_result || !s || a->busy || a->io_busy || a->page_stale || ++primary_transforms_wait<24) return;
    primary_transforms_wait=0; TRANSFORM_GUI_CHECK(!a->fetch_failed && a->modal!=MODAL_ERROR);
    switch (primary_transforms_stage) {
    case 0:
        remove(primary_transforms_path); TRANSFORM_GUI_CHECK(a->page.total==8 && !a->analysis.sort_column && !a->analysis.filter_column);
        primary_transforms_revision=a->book.command_revision; a->ribbon_tab=RIBBON_DATA; set_ribbon_mode(a,0); grid_select_cell(&a->grid,0,1,false); break;
    case 1: {
        int index=ui_ribbon_action_index(RIBBON_DATA,ACTION_SORT_DESC); TRANSFORM_GUI_CHECK(index>=0);
        Rectangle r=ribbon_rect(a,a->ribbon_layout.commands[index]); TRANSFORM_GUI_CHECK(r.width>0 && r.height>0);
        ui_note_pointer_gesture((DxPointerGesture){r.x+r.width*.5f,r.y+r.height*.5f,r.x+r.width*.5f,r.y+r.height*.5f,0,false}); break;
    }
    case 2:
        TRANSFORM_GUI_CHECK(a->page.total==8 && a->analysis.sort_column && !strcmp(a->analysis.sort_column,"Amount") && a->analysis.descending && a->page.cells[1].number==30 &&
            a->grid.col==1 && a->book.command_revision==primary_transforms_revision+1);
        set_source_filter(a,"City",FILTER_EQUAL,"Ankara",false); break;
    case 3: {
        TRANSFORM_GUI_CHECK(a->page.total==2 && !strcmp(a->page.cells[0].text,"Ankara") && a->analysis.filter_column && a->analysis.sort_column && a->book.command_revision==primary_transforms_revision+2);
        uint64_t request=a->request_id; size_t history=s->undo_actions_count; Analysis order=analysis_copy(&a->analysis);
        set_source_filter(a,"City",FILTER_EQUAL,"Ankara",false); apply_sort_order(a,&order); analysis_free(&order);
        TRANSFORM_GUI_CHECK(request==a->request_id && history==s->undo_actions_count && a->book.command_revision==primary_transforms_revision+2);
        ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    }
    case 4: {
        TRANSFORM_GUI_CHECK(a->page.total==8 && !a->analysis.filter_column && a->analysis.sort_column && s->redo_actions_count==1);
        uint64_t request=a->request_id; set_source_filter(a,"City",FILTER_EQUAL,NULL,true);
        TRANSFORM_GUI_CHECK(request==a->request_id && s->redo_actions_count==1);
        ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    }
    case 5:
        TRANSFORM_GUI_CHECK(a->page.total==2 && a->analysis.filter_column && a->analysis.sort_column);
        set_source_filter(a,"City",FILTER_EQUAL,NULL,true); break;
    case 6:
        TRANSFORM_GUI_CHECK(a->page.total==8 && !a->analysis.filter_column && a->analysis.sort_column && a->book.command_revision==primary_transforms_revision+5);
        apply_sort_order(a,&(Analysis){0}); break;
    case 7:
        TRANSFORM_GUI_CHECK(a->page.total==8 && !a->analysis.sort_column && !a->analysis.filter_column && a->book.command_revision==primary_transforms_revision+6);
        ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 8:
        TRANSFORM_GUI_CHECK(a->page.total==8 && a->analysis.sort_column && a->analysis.descending && a->page.cells[1].number==30 && a->book.command_revision==primary_transforms_revision+7);
        workspace_job(a,JOB_SAVE,primary_transforms_path); break;
    case 9:
        TRANSFORM_GUI_CHECK(!a->book.dirty); workspace_job(a,JOB_LOAD,primary_transforms_path); break;
    case 10:
        TRANSFORM_GUI_CHECK(a->page.total==8 && a->analysis.sort_column && a->analysis.descending && a->page.cells[1].number==30 && !a->analysis.filter_column &&
            a->book.command_revision==primary_transforms_revision+7 && !a->book.dirty && !s->undo_actions_count);
        primary_transforms_result=1; printf("PASS: native primary sort/filter workflow (11 stages, %u checks)\n",primary_transforms_checks); break;
    }
    primary_transforms_stage++;
}
int app_test_primary_transforms_result(void) { return primary_transforms_result; }
#undef TRANSFORM_GUI_CHECK
