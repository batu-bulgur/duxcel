/* Private native fixture; excluded from shipping builds. */
static unsigned mixed_stage,mixed_wait,mixed_checks;
static int mixed_result;
static uint64_t mixed_high;
#define MIXED_CHECK(x) do { mixed_checks++; if (!(x)) { fprintf(stderr,"FAIL mixed GUI stage %u:%d: %s (%s)\n",mixed_stage,__LINE__,#x,a->message ? a->message : ""); mixed_result=-1; return; } } while (0)
static void mixed_select(App *a,uint64_t row,size_t col) { grid_select_cell(&a->grid,row,col,false); a->grid.end_row=row+9; a->grid.end_col=col+13; }
static void mixed_move(App *a,const char *text) { begin_move_range(a,false); if (a->modal!=MODAL_MOVE_RANGE) return; input_set(&a->input,text); a->range_move_focus=1; ui_note_key_press(KEY_ENTER,0); }
static void mixed_range_test_frame(AppSession *session) {
    App *a=&session->app; Worksheet *s=active_sheet(a); char *error=NULL;
    if (mixed_result || !s || a->busy || a->io_busy || ++mixed_wait<24) return;
    mixed_wait=0; MIXED_CHECK(a->modal!=MODAL_ERROR);
    switch (mixed_stage) {
    case 0: {
        MIXED_CHECK(a->analysis.kind==SOURCE_NONE && !s->objects.count);
        MIXED_CHECK(objects_add(&s->objects,2,2,"Mixed Pivot",&error)==0); s->objects.items[0].rows=3; s->objects.items[0].columns=2; worksheet_record(s,EDIT_OBJECTS);
        MIXED_CHECK(objects_add_chart(&s->objects,2,8,"Mixed Chart",&(PivotDefinition){0},&error)==1); s->objects.items[1].rows=8; s->objects.items[1].columns=5; worksheet_record(s,EDIT_OBJECTS);
        MIXED_CHECK(cells_set(&s->cells,1,1,"NULL",&error)); worksheet_record(s,EDIT_CELLS);
        MIXED_CHECK(cells_set(&s->cells,8,1,"9007199254740993.0123456789",&error)); worksheet_record(s,EDIT_CELLS);
        CellFormat bold=format_default(); bold.flags=FORMAT_BOLD; MIXED_CHECK(format_apply(&s->formats,(FormatRange){1,9,1,13},FORMAT_BOLD,bold,&error)); worksheet_record(s,EDIT_FORMAT);
        mixed_high=s->objects.next_id; a->book.dirty=false; mixed_select(a,1,1); break;
    }
    case 1: mixed_move(a,"P20"); break;
    case 2:
        MIXED_CHECK(a->modal==MODAL_NONE && a->book.command_revision==1 && s->undo_actions[s->undo_actions_count-1]==EDIT_RANGE_FORMAT && s->objects.items[0].row==20 && s->objects.items[0].col==16 && s->objects.items[1].row==20 && s->objects.items[1].col==22);
        MIXED_CHECK(!strcmp(cells_get(&s->cells,19,15),"NULL") && !strcmp(cells_get(&s->cells,26,15),"9007199254740993.0123456789") && (format_at(&s->formats,19,15).flags&FORMAT_BOLD) && !*cells_get(&s->cells,1,1));
        MIXED_CHECK(a->grid.anchor_row==19 && a->grid.anchor_col==15 && a->grid.end_row==28 && a->grid.end_col==28 && s->objects.next_id==mixed_high);
        ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 3:
        MIXED_CHECK(a->book.command_revision==2 && s->objects.items[0].row==2 && s->objects.items[1].col==8 && !strcmp(cells_get(&s->cells,1,1),"NULL") && (format_at(&s->formats,1,1).flags&FORMAT_BOLD)); ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 4:
        MIXED_CHECK(a->book.command_revision==3 && s->objects.items[0].row==20 && !strcmp(cells_get(&s->cells,26,15),"9007199254740993.0123456789")); mixed_select(a,19,15); mixed_move(a,"P20"); break;
    case 5:
        MIXED_CHECK(a->modal==MODAL_NONE && a->book.command_revision==3); grid_select_cell(&a->grid,20,16,false); mixed_move(a,"A1"); break;
    case 6:
        MIXED_CHECK(a->modal==MODAL_NONE && a->range_move_error && a->book.command_revision==3 && s->objects.items[0].row==20); ui_note_key_press(KEY_ESCAPE,0); break;
    case 7:
        MIXED_CHECK(a->modal==MODAL_NONE); mixed_select(a,19,15); mixed_move(a,"AD40"); break;
    case 8:
        MIXED_CHECK(a->modal==MODAL_NONE && a->book.command_revision==4 && s->objects.items[0].row==40 && s->objects.items[0].col==30 && !strcmp(cells_get(&s->cells,39,29),"NULL")); ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 9:
        MIXED_CHECK(a->book.command_revision==5 && s->objects.items[0].row==20 && !strcmp(cells_get(&s->cells,19,15),"NULL")); ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 10:
        MIXED_CHECK(a->book.command_revision==6 && s->objects.items[0].row==2 && s->objects.items[1].row==2 && !strcmp(cells_get(&s->cells,8,1),"9007199254740993.0123456789")); ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 11:
        MIXED_CHECK(a->book.command_revision==7 && s->objects.items[0].row==20 && s->objects.items[1].col==22 && !strcmp(cells_get(&s->cells,19,15),"NULL")); break;
    case 12:
        MIXED_CHECK(s->objects.next_id==mixed_high && !strcmp(s->objects.items[0].name,"Mixed Pivot") && !strcmp(s->objects.items[1].name,"Mixed Chart") && a->analysis.kind==SOURCE_NONE && !a->busy && !a->io_busy);
        MIXED_CHECK(!s->source_versions.count && !a->replace_manual_target);
        mixed_result=1; printf("PASS: native source-free mixed range workflow (13 stages, %u checks)\n",mixed_checks); break;
    }
    free(error); mixed_stage++;
}
int app_test_mixed_range_result(void) { return mixed_result; }
unsigned app_test_mixed_range_stage(void) { return mixed_stage; }
#undef MIXED_CHECK
