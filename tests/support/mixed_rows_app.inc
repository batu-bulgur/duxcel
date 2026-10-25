/* Private actual multi-row context actions; excluded from production. */
static unsigned mixed_rows_stage,mixed_rows_wait,mixed_rows_checks;
static int mixed_rows_result;
static uint64_t mixed_rows_high;
#define ROWS_CHECK(x) do { mixed_rows_checks++; if (!(x)) { fprintf(stderr,"FAIL mixed rows GUI stage %u:%d: %s (%s)\n",mixed_rows_stage,__LINE__,#x,a->message ? a->message : ""); mixed_rows_result=-1; return; } } while (0)
static void mixed_rows_select(App *a,uint64_t row,uint64_t count) { grid_select_cell(&a->grid,row,0,false); a->grid.selection=SELECT_ROWS; a->grid.end_row=row+count-1; }
static void mixed_rows_menu(App *a,int action) { a->modal=MODAL_CONTEXT; a->context_origin=(Vector2){300,200}; a->context_focus=action; a->context_scroll=0; a->chrome_focus=a->pivot_focus=0; ui_note_key_press(KEY_ENTER,0); }
static void mixed_rows_test_frame(AppSession *session) {
    App *a=&session->app; Worksheet *s=active_sheet(a); char *error=NULL;
    if (mixed_rows_result || !s || a->busy || a->io_busy || ++mixed_rows_wait<24) return;
    mixed_rows_wait=0; ROWS_CHECK(a->modal!=MODAL_ERROR);
    switch (mixed_rows_stage) {
    case 0: {
        ROWS_CHECK(a->analysis.kind==SOURCE_NONE && !s->objects.count);
        ROWS_CHECK(objects_add(&s->objects,5,2,"Rows Pivot",&error)==0); s->objects.items[0].rows=3; s->objects.items[0].columns=2; worksheet_record(s,EDIT_OBJECTS);
        ROWS_CHECK(objects_add_chart(&s->objects,20,8,"Rows Chart",&(PivotDefinition){0},&error)==1); worksheet_record(s,EDIT_OBJECTS);
        ROWS_CHECK(cells_set(&s->cells,1,1,"NULL",&error)); worksheet_record(s,EDIT_CELLS);
        ROWS_CHECK(cells_set(&s->cells,10,1,"9007199254740993.0123456789",&error)); worksheet_record(s,EDIT_CELLS);
        CellFormat bold=format_default(); bold.flags=FORMAT_BOLD; ROWS_CHECK(format_apply(&s->formats,(FormatRange){1,10,1,3},FORMAT_BOLD,bold,&error)); worksheet_record(s,EDIT_FORMAT);
        mixed_rows_high=s->objects.next_id; a->book.dirty=false; mixed_rows_select(a,0,2); break;
    }
    case 1: mixed_rows_menu(a,0); break;
    case 2:
        ROWS_CHECK(a->modal==MODAL_NONE && a->book.command_revision==1 && s->undo_actions[s->undo_actions_count-1]==EDIT_RANGE_FORMAT && s->objects.items[0].row==7 && s->objects.items[1].row==22);
        ROWS_CHECK(!strcmp(cells_get(&s->cells,3,1),"NULL") && !strcmp(cells_get(&s->cells,12,1),"9007199254740993.0123456789") && (format_at(&s->formats,3,1).flags&FORMAT_BOLD));
        ROWS_CHECK(a->grid.selection==SELECT_ROWS && a->grid.anchor_row==0 && a->grid.end_row==1 && !s->width_revision);
        ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 3:
        ROWS_CHECK(a->book.command_revision==2 && s->objects.items[0].row==5 && s->objects.items[1].row==20 && !strcmp(cells_get(&s->cells,1,1),"NULL")); ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 4:
        ROWS_CHECK(a->book.command_revision==3 && s->objects.items[0].row==7); mixed_rows_select(a,0,2); mixed_rows_menu(a,2); break;
    case 5:
        ROWS_CHECK(a->book.command_revision==4 && s->objects.items[0].row==5 && s->objects.items[1].row==20 && !strcmp(cells_get(&s->cells,10,1),"9007199254740993.0123456789")); ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 6:
        ROWS_CHECK(a->book.command_revision==5 && s->objects.items[0].row==7 && !strcmp(cells_get(&s->cells,3,1),"NULL")); ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 7:
        ROWS_CHECK(a->book.command_revision==6 && s->objects.items[0].row==5); mixed_rows_select(a,6,1); mixed_rows_menu(a,0); break;
    case 8:
        ROWS_CHECK(a->modal==MODAL_NONE && a->book.command_revision==6 && s->objects.items[0].row==5 && !strcmp(cells_get(&s->cells,1,1),"NULL")); mixed_rows_select(a,1000,2); mixed_rows_menu(a,0); break;
    case 9:
        ROWS_CHECK(a->modal==MODAL_NONE && a->book.command_revision==6 && s->objects.items[0].row==5 && s->objects.items[1].row==20 && s->objects.next_id==mixed_rows_high && !s->width_revision && (format_at(&s->formats,1,1).flags&FORMAT_BOLD) && !a->busy && !a->io_busy);
        mixed_rows_result=1; printf("PASS: native source-free mixed row workflow (10 stages, %u checks)\n",mixed_rows_checks); break;
    }
    free(error); mixed_rows_stage++;
}
int app_test_mixed_rows_result(void) { return mixed_rows_result; }
unsigned app_test_mixed_rows_stage(void) { return mixed_rows_stage; }
#undef ROWS_CHECK
