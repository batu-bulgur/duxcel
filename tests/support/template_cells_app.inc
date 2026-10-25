/* Private actual clipboard/context workflow on a source-free template sheet. */
static unsigned template_cells_stage,template_cells_wait,template_cells_checks;
static int template_cells_result;
static uint64_t template_cells_base,template_cells_high,template_cells_request;
#define TC_CHECK(x) do { template_cells_checks++; if (!(x)) { fprintf(stderr,"FAIL template cells GUI stage %u:%d: %s (%s)\n",template_cells_stage,__LINE__,#x,a->message ? a->message : ""); template_cells_result=-1; return; } } while (0)
static void template_cells_clear_menu(App *a) {
    grid_select_cell(&a->grid,0,0,false); a->grid.end_row=10; a->grid.end_col=10;
    a->modal=MODAL_CONTEXT; a->context_origin=(Vector2){300,200}; a->context_focus=6; a->context_scroll=0;
    a->chrome_focus=a->pivot_focus=0; ui_note_key_press(KEY_ENTER,0);
}
static void template_cells_paste(App *a,uint64_t row,size_t column,const char *text) {
    grid_select_cell(&a->grid,row,column,false); a->chrome_focus=a->pivot_focus=0;
    SetClipboardText(text); ui_note_key_press(KEY_V,DX_MOD_SUPER);
}
static void template_cells_test_frame(AppSession *session) {
    App *a=&session->app; Worksheet *s=active_sheet(a); char *error=NULL;
    if (template_cells_result || !s || a->busy || a->io_busy || ++template_cells_wait<24) return;
    template_cells_wait=0; TC_CHECK(a->modal!=MODAL_ERROR);
    switch (template_cells_stage) {
    case 0: {
        TC_CHECK(a->analysis.kind==SOURCE_NONE && !s->objects.count);
        TC_CHECK(objects_add(&s->objects,5,2,"Preserved template",&error)==0); s->objects.items[0].rows=3; s->objects.items[0].columns=2; worksheet_record(s,EDIT_OBJECTS);
        TC_CHECK(cells_set(&s->cells,0,0,"Before",&error)); worksheet_record(s,EDIT_CELLS);
        CellFormat bold=format_default(); bold.flags=FORMAT_BOLD; TC_CHECK(format_apply(&s->formats,(FormatRange){0,10,0,10},FORMAT_BOLD,bold,&error)); worksheet_record(s,EDIT_FORMAT);
        grid_set_width(&a->grid,0,121); TC_CHECK(commit_grid_widths(a));
        template_cells_base=a->book.command_revision; template_cells_high=s->objects.next_id; template_cells_request=a->request_id;
        template_cells_paste(a,0,0,"NULL\t\"\"\n\"two\tparts\"\t9007199254740993.0123456789\n"); break;
    }
    case 1:
        TC_CHECK(a->book.command_revision==template_cells_base+1 && s->undo_actions[s->undo_actions_count-1]==EDIT_CELLS && !strcmp(cells_get(&s->cells,0,0),"NULL") && !*cells_get(&s->cells,0,1) && !strcmp(cells_get(&s->cells,1,0),"two\tparts") && !strcmp(cells_get(&s->cells,1,1),"9007199254740993.0123456789"));
        TC_CHECK(a->grid.anchor_row==0 && a->grid.end_row==1 && a->grid.anchor_col==0 && a->grid.end_col==1); ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 2:
        TC_CHECK(a->book.command_revision==template_cells_base+2 && !strcmp(cells_get(&s->cells,0,0),"Before") && !*cells_get(&s->cells,1,0)); ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 3:
        TC_CHECK(a->book.command_revision==template_cells_base+3 && !strcmp(cells_get(&s->cells,1,0),"two\tparts")); template_cells_paste(a,5,2,""); break;
    case 4:
        TC_CHECK(a->book.command_revision==template_cells_base+3 && a->grid.row==5 && a->grid.col==2 && s->objects.items[0].row==5); template_cells_paste(a,3,1,"A\tB\tC\nD\nE"); break;
    case 5:
        TC_CHECK(a->book.command_revision==template_cells_base+3 && !*cells_get(&s->cells,3,1)); template_cells_clear_menu(a); break;
    case 6:
        TC_CHECK(a->book.command_revision==template_cells_base+4 && !s->cells.count && s->objects.count==1 && s->objects.items[0].row==5 && s->objects.items[0].col==2 && (format_at(&s->formats,0,0).flags&FORMAT_BOLD)); ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 7:
        TC_CHECK(a->book.command_revision==template_cells_base+5 && !strcmp(cells_get(&s->cells,0,0),"NULL") && !strcmp(cells_get(&s->cells,1,1),"9007199254740993.0123456789")); ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 8:
        TC_CHECK(a->book.command_revision==template_cells_base+6 && !s->cells.count); template_cells_clear_menu(a); break;
    case 9:
        TC_CHECK(a->book.command_revision==template_cells_base+6 && !s->cells.count && s->objects.count==1 && s->objects.next_id==template_cells_high && s->objects.items[0].row==5 && s->objects.items[0].col==2 && (format_at(&s->formats,0,0).flags&FORMAT_BOLD) && s->view.widths[0]==121 && a->grid.widths[0]==121 && a->request_id==template_cells_request && !a->busy && !a->io_busy);
        template_cells_result=1; printf("PASS: native template manual cell workflow (10 stages, %u checks)\n",template_cells_checks); break;
    }
    free(error); template_cells_stage++;
}
int app_test_template_cells_result(void) { return template_cells_result; }
unsigned app_test_template_cells_stage(void) { return template_cells_stage; }
#undef TC_CHECK
