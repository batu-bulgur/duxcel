#include "keytips.h"
#include <string.h>

/* English/US Excel access keys, not localized keyboard-layout shortcuts.
   Microsoft 365's official ribbon image verifies Home's formatting codes:
   https://support.microsoft.com/en-us/accessibility/media/excel-o365-win-keytips.png
   Root tab and sequential-key behavior:
   https://support.microsoft.com/en-us/accessibility/excel/keyboard-shortcuts-in-excel
   Data SA/SD and Office File N/O/S/A:
   https://support.microsoft.com/en-us/accessibility/excel/use-a-screen-reader-to-sort-or-filter-a-table-in-excel
   https://support.microsoft.com/en-us/accessibility/office-accessibility/keyboard-shortcuts-for-the-file-menu-in-microsoft-365-for-windows
   File C/X retain the historical Excel keys shown in Microsoft's Excel book:
   https://download.microsoft.com/download/a/8/e/a8e56ca8-d277-4250-bfc7-8aa4b8cbb70f/revuptoexcelebook.pdf
   Duxcel has no Office Backstage; O/N/A dispatch its existing document actions.
   Copy CC models the Home C dropdown followed by Copy. Extensions use Z+digit
   rather than assigning unrelated actions to Excel's existing command keys. */
typedef struct { int tab; UIAction action; const char *keys; bool custom; } Binding;
#define E(tab,action,keys) {RIBBON_##tab,ACTION_##action,keys,false}
#define D(tab,action,keys) {RIBBON_##tab,ACTION_##action,keys,true}
#define VISUALS(tab) \
    D(tab,CREATE_VIEW,"E"),D(tab,MATRIX,"M"),D(tab,BAR,"B"),D(tab,LINE,"L"), \
    D(tab,ADD_ROWS,"R"),D(tab,ADD_COLUMNS,"C"),D(tab,SWAP_SHELVES,"S"), \
    D(tab,ADD_MEASURE,"V"),D(tab,TRANSFORM,"T"),D(tab,SQL,"Q"),D(tab,BROWSE,"D")
static const Binding bindings[]={
    E(HOME,COPY,"CC"),E(HOME,PASTE,"V"),E(HOME,CUT,"X"),
    E(HOME,FONT,"FF"),E(HOME,FONT_SIZE,"FS"),E(HOME,BOLD,"1"),
    E(HOME,ITALIC,"2"),E(HOME,UNDERLINE,"3"),E(HOME,TEXT_COLOR,"FC"),
    E(HOME,ALIGN_LEFT,"AL"),E(HOME,ALIGN_CENTER,"AC"),E(HOME,ALIGN_RIGHT,"AR"),
    E(HOME,WRAP,"W"),E(HOME,NUMBER_FORMAT,"AN"),
    E(HOME,DECIMAL_LESS,"9"),E(HOME,DECIMAL_MORE,"0"),
    D(HOME,ACCOUNTING,"Z1"),D(HOME,SORT_ASC,"Z2"),D(HOME,SORT_DESC,"Z3"),
    D(HOME,FILTER,"Z4"),D(HOME,CLEAR_FILTERS,"Z5"),D(HOME,CREATE_VIEW,"Z6"),
    D(HOME,TRANSFORM,"Z7"),D(HOME,REFRESH,"Z8"),
    D(INSERT,CREATE_VIEW,"Z1"),D(INSERT,MATRIX,"Z2"),D(INSERT,BAR,"Z3"),
    D(INSERT,LINE,"Z4"),D(INSERT,NEW_SHEET,"Z5"),
    E(DATA,SORT_ASC,"SA"),E(DATA,SORT_DESC,"SD"),
    D(DATA,SOURCE_ORDER,"Z1"),D(DATA,FILTER,"Z2"),D(DATA,CLEAR_FILTERS,"Z3"),
    D(DATA,TRANSFORM,"Z4"),D(DATA,SQL,"Z5"),D(DATA,REFRESH,"Z6"),
    VISUALS(ANALYZE),VISUALS(VISUALIZATION),
    D(VIEW,WORKBOOK,"Z1"),D(VIEW,FIELDS,"Z2"),D(VIEW,TRANSFORM,"Z3"),
    D(VIEW,SQL,"Z4"),D(VIEW,FOCUS,"Z5"),D(VIEW,DENSITY,"Z6"),
    D(VIEW,SCALE,"Z7"),D(VIEW,GRIDLINES,"Z8"),D(VIEW,FREEZE,"Z9"),
    D(VIEW,RESET_ZOOM,"Z0"),D(VIEW,RESET_LAYOUT,"ZA"),D(VIEW,GUIDE,"ZB")
};
#undef VISUALS
#undef D
#undef E
static const char *const tab_keys[RIBBON_TABS]={"H","N","A","Y","W","J"};
/* Import is a Duxcel extension; Export retains the Excel File E sequence. */
static const UIKeytipFileSpec file_specs[]={
    {"N","New workbook","new","command.new_workbook",CARBON_DOCUMENT_BLANK},
    {"O","Open…","open","command.open",CARBON_FOLDER_OPEN},
    {"S","Save","save","command.save",CARBON_SAVE},
    {"A","Save as…","save_as","command.save_as",CARBON_SAVE},
    {"C","Close workbook","close","command.close",CARBON_CLOSE},
    {"I","Import table…","import","command.import_table",CARBON_TABLE},
    {"E","Export Parquet…","export","command.export",CARBON_ARROW_DOWN},
    {"R","Recover session…","recover","command.recover_session",CARBON_UNDO},
    {"V","Versions…","versions","command.versions",CARBON_SAVE},
    {"X","Quit Duxcel","quit","command.quit",CARBON_CLOSE}
};
typedef char FileRegistryMatchesEnum[sizeof(file_specs)/sizeof(*file_specs)==UI_KEYTIP_FILE_COUNT ? 1 : -1];
const UIKeytipFileSpec *ui_keytips_file_spec(UIKeytipFileAction action) { return (unsigned)action<UI_KEYTIP_FILE_COUNT ? file_specs+action : NULL; }

static size_t prefix_length(const UIKeytips *state) {
    if (!state) return UI_KEYTIPS_PREFIX_CAP;
    for (size_t n=0;n<UI_KEYTIPS_PREFIX_CAP;n++) if (!state->prefix[n]) return n;
    return UI_KEYTIPS_PREFIX_CAP;
}
static const char *suffix(const UIKeytips *state,const char *keys) {
    size_t n=prefix_length(state);
    if (n>=UI_KEYTIPS_PREFIX_CAP || n>strlen(keys) || strncmp(state->prefix,keys,n)) return NULL;
    return keys+n;
}
const char *ui_keytips_tab_label(int tab) {
    return tab==UI_KEYTIPS_FILE ? "F" : tab>=0 && tab<RIBBON_TABS ? tab_keys[tab] : NULL;
}
const char *ui_keytips_command_label(const UIKeytips *state,int tab,UIAction action) {
    if (!state || state->tab!=tab) return NULL;
    for (size_t i=0;i<sizeof(bindings)/sizeof(*bindings);i++)
        if (bindings[i].tab==tab && bindings[i].action==action) return suffix(state,bindings[i].keys);
    return NULL;
}
bool ui_keytips_command_custom(int tab,UIAction action) {
    for (size_t i=0;i<sizeof(bindings)/sizeof(*bindings);i++)
        if (bindings[i].tab==tab && bindings[i].action==action) return bindings[i].custom;
    return false;
}
const char *ui_keytips_file_label(const UIKeytips *state,UIKeytipFileAction action) {
    return state && state->tab==UI_KEYTIPS_FILE && action>=0 && action<UI_KEYTIP_FILE_COUNT ? suffix(state,file_specs[action].keys) : NULL;
}
const char *ui_keytips_file_name(UIKeytipFileAction action) {
    return action>=0 && action<UI_KEYTIP_FILE_COUNT ? file_specs[action].name : NULL;
}
void ui_keytips_begin(UIKeytips *state) {
    if (state) *state=(UIKeytips){.active=true,.tab=UI_KEYTIPS_ROOT};
}
void ui_keytips_cancel(UIKeytips *state) { if (state) state->active=false; }
bool ui_keytips_escape(UIKeytips *state) {
    if (!state || !state->active) return false;
    size_t n=prefix_length(state);
    if (n && n<UI_KEYTIPS_PREFIX_CAP) state->prefix[n-1]=0;
    else if (state->tab!=UI_KEYTIPS_ROOT) { state->tab=UI_KEYTIPS_ROOT; state->prefix[0]=0; }
    else state->active=false;
    return true;
}
UIKeytipResult ui_keytips_feed(UIKeytips *state,int ascii,bool repeat) {
    UIKeytipResult none={UI_KEYTIP_NONE,-1};
    if (!state || !state->active || repeat) return none;
    if (ascii>='a' && ascii<='z') ascii+='A'-'a';
    if (!((ascii>='A' && ascii<='Z') || (ascii>='0' && ascii<='9'))) return none;
    if (state->tab==UI_KEYTIPS_ROOT) {
        int tab=UI_KEYTIPS_ROOT;
        if (ascii=='F') tab=UI_KEYTIPS_FILE;
        else for (int i=0;i<RIBBON_TABS;i++) if (ascii==tab_keys[i][0]) { tab=i; break; }
        if (tab==UI_KEYTIPS_ROOT) return none;
        state->tab=tab; state->prefix[0]=0;
        return (UIKeytipResult){UI_KEYTIP_TAB,tab};
    }
    size_t n=prefix_length(state);
    if (n>=UI_KEYTIPS_PREFIX_CAP-1) return none;
    UIKeytips next=*state; next.prefix[n]=(char)ascii; next.prefix[n+1]=0;
    if (state->tab==UI_KEYTIPS_FILE) {
        for (int i=0;i<UI_KEYTIP_FILE_COUNT;i++) {
            const char *remaining=suffix(&next,file_specs[i].keys);
            if (remaining && !*remaining) {
                state->active=false;
                return (UIKeytipResult){UI_KEYTIP_FILE,i};
            }
        }
    } else {
        bool matched=false;
        for (size_t i=0;i<sizeof(bindings)/sizeof(*bindings);i++) if (bindings[i].tab==state->tab) {
            const char *remaining=suffix(&next,bindings[i].keys);
            if (!remaining) continue;
            if (!*remaining) {
                state->active=false;
                return (UIKeytipResult){UI_KEYTIP_COMMAND,bindings[i].action};
            }
            matched=true;
        }
        if (matched) { *state=next; return (UIKeytipResult){UI_KEYTIP_PREFIX,-1}; }
    }
    return none;
}
