#include "ui/keytips.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int checks;
#define CHECK(value) do { checks++; if (!(value)) { fprintf(stderr,"keytips:%d: %s\n",__LINE__,#value); exit(1); } } while (0)
static UIKeytipResult sequence(UIKeytips *state,const char *keys) {
    UIKeytipResult result={UI_KEYTIP_NONE,-1};
    for (size_t i=0;keys[i];i++) result=ui_keytips_feed(state,keys[i],false);
    return result;
}
static void expect_command(const char *keys,UIAction action) {
    UIKeytips state={0}; ui_keytips_begin(&state);
    UIKeytipResult result=sequence(&state,keys);
    CHECK(result.kind==UI_KEYTIP_COMMAND && result.value==(int)action);
    CHECK(!state.active);
}
int main(void) {
    UIKeytips state={0};
    CHECK(ui_keytips_feed(&state,'H',false).kind==UI_KEYTIP_NONE);
    CHECK(!ui_keytips_escape(&state));
    ui_keytips_begin(&state);
    CHECK(state.active && state.tab==UI_KEYTIPS_ROOT && !state.prefix[0]);
    const int tabs[]={RIBBON_HOME,RIBBON_INSERT,RIBBON_DATA,RIBBON_VIEW,RIBBON_ANALYZE,RIBBON_VISUALIZATION,UI_KEYTIPS_FILE};
    const char *labels[]={"H","N","A","W","Y","J","F"};
    for (size_t i=0;i<sizeof(tabs)/sizeof(*tabs);i++) {
        CHECK(!strcmp(ui_keytips_tab_label(tabs[i]),labels[i]));
        ui_keytips_begin(&state);
        UIKeytipResult result=ui_keytips_feed(&state,labels[i][0]+'a'-'A',false);
        CHECK(result.kind==UI_KEYTIP_TAB && result.value==tabs[i]);
        CHECK(state.active && state.tab==tabs[i]);
    }
    CHECK(!ui_keytips_tab_label(UI_KEYTIPS_ROOT));
    CHECK(!ui_keytips_tab_label(RIBBON_TABS));
    /* Data and View retain Excel keys; Duxcel Analyze cannot steal Alt+A. */
    ui_keytips_begin(&state);
    CHECK(sequence(&state,"a").value==RIBBON_DATA);
    CHECK(sequence(&state,"s").kind==UI_KEYTIP_PREFIX);
    CHECK(!strcmp(ui_keytips_command_label(&state,RIBBON_DATA,ACTION_SORT_ASC),"A"));
    CHECK(!strcmp(ui_keytips_command_label(&state,RIBBON_DATA,ACTION_SORT_DESC),"D"));
    UIKeytips saved=state;
    CHECK(ui_keytips_feed(&state,'Q',false).kind==UI_KEYTIP_NONE);
    CHECK(!memcmp(&state,&saved,sizeof(state))); /* unknown prefix has no side effects */
    CHECK(ui_keytips_feed(&state,'A',true).kind==UI_KEYTIP_NONE);
    CHECK(!memcmp(&state,&saved,sizeof(state))); /* holding a key cannot complete a prefix */
    CHECK(ui_keytips_feed(&state,'A',false).value==ACTION_SORT_ASC);
    CHECK(!state.active);
    CHECK(ui_keytips_feed(&state,'A',false).kind==UI_KEYTIP_NONE);
    expect_command("asd",ACTION_SORT_DESC);
    expect_command("hcc",ACTION_COPY);
    expect_command("hv",ACTION_PASTE);
    expect_command("hx",ACTION_CUT);
    expect_command("hff",ACTION_FONT);
    expect_command("hfs",ACTION_FONT_SIZE);
    expect_command("hfc",ACTION_TEXT_COLOR);
    expect_command("h1",ACTION_BOLD);
    expect_command("h2",ACTION_ITALIC);
    expect_command("h3",ACTION_UNDERLINE);
    expect_command("hal",ACTION_ALIGN_LEFT);
    expect_command("hac",ACTION_ALIGN_CENTER);
    expect_command("har",ACTION_ALIGN_RIGHT);
    expect_command("hw",ACTION_WRAP);
    expect_command("han",ACTION_NUMBER_FORMAT);
    expect_command("h9",ACTION_DECIMAL_LESS);
    expect_command("h0",ACTION_DECIMAL_MORE);
    /* A multi-character prefix reveals only matching suffix badges; Escape
       moves back through prefix -> tab -> root -> dismissed. */
    ui_keytips_begin(&state); sequence(&state,"hf");
    CHECK(state.active && state.tab==RIBBON_HOME && !strcmp(state.prefix,"F"));
    CHECK(!strcmp(ui_keytips_command_label(&state,RIBBON_HOME,ACTION_FONT),"F"));
    CHECK(!strcmp(ui_keytips_command_label(&state,RIBBON_HOME,ACTION_FONT_SIZE),"S"));
    CHECK(!ui_keytips_command_label(&state,RIBBON_HOME,ACTION_COPY));
    CHECK(ui_keytips_escape(&state) && state.active && !state.prefix[0]);
    CHECK(!strcmp(ui_keytips_command_label(&state,RIBBON_HOME,ACTION_FONT),"FF"));
    CHECK(ui_keytips_escape(&state) && state.active && state.tab==UI_KEYTIPS_ROOT);
    CHECK(ui_keytips_escape(&state) && !state.active);
    CHECK(!ui_keytips_escape(&state));
    /* Pointer/modal cancellation preserves paint context for fade-out and
       cannot execute the rest of a half-entered command. */
    ui_keytips_begin(&state); sequence(&state,"hc");
    ui_keytips_cancel(&state);
    CHECK(!state.active && !strcmp(state.prefix,"C"));
    CHECK(!strcmp(ui_keytips_command_label(&state,RIBBON_HOME,ACTION_COPY),"C"));
    CHECK(ui_keytips_feed(&state,'C',false).kind==UI_KEYTIP_NONE);
    ui_keytips_begin(&state);
    CHECK(state.tab==UI_KEYTIPS_ROOT && !state.prefix[0]);
    /* Every current ribbon command has exactly one bounded, nonambiguous
       badge. It remains addressable when its visual group is overflowed. */
    for (int tab=0;tab<RIBBON_TABS;tab++) {
        UIKeytips context={.active=true,.tab=tab};
        for (int i=0;i<ribbon_counts[tab];i++) {
            UIAction action=ribbon_commands[tab][i].action;
            const char *keys=ui_keytips_command_label(&context,tab,action);
            CHECK(keys && *keys && strlen(keys)<UI_KEYTIPS_PREFIX_CAP);
            for (int j=i+1;j<ribbon_counts[tab];j++) {
                const char *other=ui_keytips_command_label(&context,tab,ribbon_commands[tab][j].action);
                CHECK(other);
                size_t shorter=strlen(keys)<strlen(other) ? strlen(keys) : strlen(other);
                CHECK(strncmp(keys,other,shorter)!=0);
            }
            UIKeytips input=context;
            for (size_t at=0;keys[at];at++) {
                UIKeytipResult result=ui_keytips_feed(&input,keys[at],false);
                if (keys[at+1]) CHECK(result.kind==UI_KEYTIP_PREFIX && input.active);
                else CHECK(result.kind==UI_KEYTIP_COMMAND && result.value==(int)action && !input.active);
            }
        }
    }
    CHECK(!ui_keytips_command_custom(RIBBON_HOME,ACTION_COPY));
    CHECK(!ui_keytips_command_custom(RIBBON_DATA,ACTION_SORT_ASC));
    CHECK(ui_keytips_command_custom(RIBBON_ANALYZE,ACTION_MATRIX));
    CHECK(ui_keytips_command_custom(RIBBON_VIEW,ACTION_FIELDS));
    const char *const file_keys[]={"N","O","S","A","C","I","E","R","V","X"};
    CHECK(UI_KEYTIP_FILE_COUNT==sizeof(file_keys)/sizeof(*file_keys));
    for (int action=0;action<UI_KEYTIP_FILE_COUNT;action++) {
        ui_keytips_begin(&state); sequence(&state,"f");
        const UIKeytipFileSpec *spec=ui_keytips_file_spec((UIKeytipFileAction)action);
        CHECK(spec && spec->keys && spec->name && spec->code && spec->command && (unsigned)spec->icon<CARBON_COUNT);
        for (int prior=0;prior<action;prior++) CHECK(strcmp(spec->code,ui_keytips_file_spec((UIKeytipFileAction)prior)->code));
        const char *keys=ui_keytips_file_label(&state,(UIKeytipFileAction)action);
        CHECK(keys && *keys && ui_keytips_file_name((UIKeytipFileAction)action));
        CHECK(!strcmp(keys,file_keys[action]));
        UIKeytipResult result=sequence(&state,keys);
        CHECK(result.kind==UI_KEYTIP_FILE && result.value==action && !state.active);
    }
    /* Export has its own Excel sequence, rather than consuming Import or Quit.
       A repeat cannot export, and cancellation retains only its fade context. */
    CHECK(!strcmp(ui_keytips_file_name(UI_KEYTIP_FILE_EXPORT),"Export Parquet…"));
    ui_keytips_begin(&state); sequence(&state,"f"); saved=state;
    CHECK(ui_keytips_feed(&state,'e',true).kind==UI_KEYTIP_NONE);
    CHECK(!memcmp(&state,&saved,sizeof(state)));
    CHECK(!strcmp(ui_keytips_file_label(&state,UI_KEYTIP_FILE_EXPORT),"E"));
    UIKeytipResult exported=sequence(&state,"e");
    CHECK(exported.kind==UI_KEYTIP_FILE && exported.value==UI_KEYTIP_FILE_EXPORT && !state.active);
    CHECK(ui_keytips_feed(&state,'E',false).kind==UI_KEYTIP_NONE);
    ui_keytips_begin(&state); sequence(&state,"f"); ui_keytips_cancel(&state);
    CHECK(!strcmp(ui_keytips_file_label(&state,UI_KEYTIP_FILE_EXPORT),"E"));
    CHECK(ui_keytips_feed(&state,'E',false).kind==UI_KEYTIP_NONE);
    CHECK(!ui_keytips_file_name(UI_KEYTIP_FILE_COUNT)); CHECK(!ui_keytips_file_spec(UI_KEYTIP_FILE_COUNT) && !ui_keytips_file_spec((UIKeytipFileAction)-1));
    CHECK(!ui_keytips_file_label(NULL,UI_KEYTIP_FILE_NEW));
    CHECK(!ui_keytips_command_label(NULL,RIBBON_HOME,ACTION_COPY));
    ui_keytips_begin(NULL); ui_keytips_cancel(NULL);
    CHECK(!ui_keytips_escape(NULL));
    CHECK(ui_keytips_feed(NULL,'H',false).kind==UI_KEYTIP_NONE);
    /* Reject non-ASCII text and malformed external state without overreading
       the fixed buffer or returning an action. Sanitizers cover this path. */
    for (int code=-256;code<512;code++) if (!((code>='A' && code<='Z') || (code>='a' && code<='z') || (code>='0' && code<='9'))) {
        ui_keytips_begin(&state); saved=state;
        CHECK(ui_keytips_feed(&state,code,false).kind==UI_KEYTIP_NONE);
        CHECK(!memcmp(&state,&saved,sizeof(state)));
    }
    state=(UIKeytips){.active=true,.tab=RIBBON_HOME,.prefix={'F','F','F','F'}};
    CHECK(ui_keytips_feed(&state,'F',false).kind==UI_KEYTIP_NONE);
    CHECK(!ui_keytips_command_label(&state,RIBBON_HOME,ACTION_FONT));
    CHECK(ui_keytips_escape(&state) && state.tab==UI_KEYTIPS_ROOT);
    printf("PASS: sequential Excel keytips and custom extension metadata, %d checks\n",checks);
    return 0;
}
