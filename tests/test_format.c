#include "workbook/format.h"
#include "transforms/number.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL format:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void display(const char *raw,unsigned decimals,bool accounting,const char *expected) {
    char text[512]; CHECK(number_display(raw,decimals,accounting,text)); CHECK(!strcmp(text,expected));
}
static void clear_error(char **error) { free(*error); *error=NULL; }
static void scoped_resolution(void) {
    FormatStore s={0},copy={0}; FormatSnapshot plan={0}; CellFormat v=format_default(); char *error=NULL;
    char definition[]="source=1;version=7;sort=none;schema=DECIMAL(38,2)";
    FormatScopeView a={11,definition,sizeof(definition)-1},b={12,definition,sizeof(definition)-1};
    const char changed[]="source=1;version=7;sort=desc;schema=DECIMAL(38,2)";
    FormatScopeView sorted={11,changed,sizeof(changed)-1}; FormatRange r={2,4,1,3};
    v.flags=FORMAT_BOLD; CHECK(format_apply(&s,r,FORMAT_BOLD,v,&error));
    CHECK(format_persistable(&s,&error) && !s.scope_count);
    CHECK(format_at(&s,3,2).flags==FORMAT_BOLD && !format_resolve(&s,3,2,&a).flags);
    v.color=2; CHECK(format_apply(&s,(FormatRange){0,DX_SHEET_ROWS-1,0,3},FORMAT_COLOR,v,&error));
    CHECK(format_resolve(&s,3,2,&a).color==2 && !format_resolve(&s,3,2,&a).flags);
    CHECK(format_apply_scoped(&s,r,FORMAT_BOLD,v,&a,1,false,&error));
    CHECK(s.scope_count==1 && s.items[s.count-1].selector==FORMAT_RESULT && format_valid(&s,&error));
    CHECK(!format_persistable(&s,&error) && strstr(error,"v12")); clear_error(&error);
    CHECK(format_resolve(&s,3,2,&a).flags==FORMAT_BOLD && !format_resolve(&s,3,2,&b).flags);
    CHECK(!format_resolve(&s,3,2,&sorted).flags && format_at(&s,3,2).flags==FORMAT_BOLD);
    size_t token=format_scope_token(&s,&a); CHECK(token && token!=SIZE_MAX && format_scope_token(&s,&b)==SIZE_MAX && !format_scope_token(&s,NULL));
    CHECK(format_resolve_token(&s,3,2,token).flags==FORMAT_BOLD && !format_resolve_token(&s,3,2,SIZE_MAX).flags);
    uint64_t revision=s.revision; CHECK(format_apply_scoped(&s,r,FORMAT_BOLD,v,&a,1,false,&error) && s.revision==revision);
    definition[0]='X'; CHECK(!format_resolve(&s,3,2,&a).flags); definition[0]='s'; CHECK(format_resolve(&s,3,2,&a).flags==FORMAT_BOLD);
    v.flags=FORMAT_ITALIC; CHECK(format_apply_scoped(&s,r,FORMAT_BOLD|FORMAT_ITALIC,v,&sorted,1,false,&error));
    CHECK(s.scope_count==2 && format_resolve(&s,3,2,&a).flags==FORMAT_BOLD && format_resolve(&s,3,2,&sorted).flags==FORMAT_ITALIC);
    CHECK(format_history(&s,false) && format_resolve(&s,3,2,&a).flags==FORMAT_BOLD && !format_resolve(&s,3,2,&sorted).flags);
    CHECK(format_history(&s,true) && format_resolve(&s,3,2,&sorted).flags==FORMAT_ITALIC);
    v.flags=FORMAT_UNDERLINE; CHECK(format_apply(&s,r,FORMAT_BOLD|FORMAT_ITALIC|FORMAT_UNDERLINE,v,&error));
    CHECK(format_at(&s,3,2).flags==FORMAT_UNDERLINE && format_resolve(&s,3,2,&a).flags==FORMAT_BOLD && format_resolve(&s,3,2,&sorted).flags==FORMAT_ITALIC);
    FormatScopeView both[]={a,b}; v.size=16;
    CHECK(format_apply_scoped(&s,r,FORMAT_SIZE,v,both,2,true,&error));
    CHECK(s.scope_count==3 && format_at(&s,3,2).size==16 && format_resolve(&s,3,2,&a).size==16 && format_resolve(&s,3,2,&b).size==16 && format_resolve(&s,3,2,&sorted).size==13);
    FormatScopeView reversed[]={b,a}; revision=s.revision;
    CHECK(format_apply_scoped(&s,r,FORMAT_SIZE,v,reversed,2,true,&error) && s.revision==revision);
    FormatScopeView invalid[]={a,sorted}; CHECK(!format_apply_scoped(&s,r,FORMAT_SIZE,v,invalid,2,true,&error) && s.revision==revision); clear_error(&error);
    CHECK(!format_apply_scoped(&s,r,FORMAT_SIZE,v,NULL,0,true,&error) && s.revision==revision); clear_error(&error);
    CHECK(!format_apply_scoped(&s,r,FORMAT_SIZE,v,both,DX_FORMAT_GUARDS+1,true,&error) && s.revision==revision); clear_error(&error);
    format_copy(&copy,&s); CHECK(copy.count==s.count && copy.scope_count==s.scope_count && !copy.undo_count && copy.items!=s.items && copy.scopes!=s.scopes);
    for (size_t i=0;i<s.scope_count;i++) CHECK(copy.scopes[i].definition!=s.scopes[i].definition && !memcmp(copy.scopes[i].definition,s.scopes[i].definition,s.scopes[i].bytes));
    for (size_t i=0;i<s.count;i++) if (s.items[i].guard_count) CHECK(copy.items[i].guard_ids!=s.items[i].guard_ids);
    CHECK(format_owned_bytes(&copy,false)==format_owned_bytes(&s,false) && format_owned_bytes(&s,true)>format_owned_bytes(&s,false));
    format_copy(&copy,&copy); CHECK(format_valid(&copy,&error) && format_resolve(&copy,3,2,&b).size==16);
    CHECK(format_plan_move(&s,r,10,4,&plan,&error) && format_commit(&s,&plan) && !plan.scopes && !plan.items);
    CHECK(format_resolve(&s,13,6,&a).flags==FORMAT_BOLD && format_resolve(&s,13,6,&sorted).flags==FORMAT_ITALIC && format_resolve(&s,13,6,&b).size==16);
    CHECK(format_at(&s,13,6).flags==FORMAT_UNDERLINE && format_at(&s,13,6).size==16);
    CHECK(!format_resolve(&s,3,2,&a).flags && !format_resolve(&s,3,2,&sorted).flags && format_resolve(&s,3,2,&b).size==13 && format_at(&s,3,2).size==13);
    CHECK(format_history(&s,false) && format_resolve(&s,3,2,&a).flags==FORMAT_BOLD && format_resolve(&s,13,6,&a).size==13);
    CHECK(format_history(&s,true) && format_resolve(&s,13,6,&b).size==16);
    CHECK(format_plan_axis(&s,true,12,2,true,&plan,&error) && format_commit(&s,&plan) && format_resolve(&s,15,6,&a).flags==FORMAT_BOLD);
    CHECK(format_plan_axis(&s,false,5,1,true,&plan,&error) && format_commit(&s,&plan) && format_resolve(&s,15,7,&b).size==16);
    CHECK(format_plan_axis(&s,true,12,2,false,&plan,&error) && format_commit(&s,&plan) && format_resolve(&s,13,7,&sorted).flags==FORMAT_ITALIC);
    CHECK(format_plan_axis(&s,false,5,1,false,&plan,&error) && format_commit(&s,&plan) && format_resolve(&s,13,6,&b).size==16);
    revision=s.revision; CHECK(!format_plan_move(&s,(FormatRange){0,0,0,0},-1,0,&plan,&error) && !plan.items && !plan.scopes && s.revision==revision); clear_error(&error);
    CHECK(format_valid(&s,&error)); format_free(&s);
    CHECK(format_resolve(&copy,3,2,&a).flags==FORMAT_BOLD && format_resolve(&copy,3,2,&b).size==16);
    /* Exact bytes after an embedded NUL are part of identity, unlike a C string
       or a cache hash. Neither equal-looking text nor a different owner matches. */
    const unsigned char one[]={'a',0,'1'},two[]={'a',0,'2'};
    FormatScopeView binary={0,one,sizeof(one)},other={0,two,sizeof(two)}; v.flags=FORMAT_BOLD;
    CHECK(format_apply_scoped(&s,r,FORMAT_BOLD,v,&binary,1,false,&error));
    CHECK(format_resolve(&s,3,2,&binary).flags==FORMAT_BOLD && !format_resolve(&s,3,2,&other).flags && !format_at(&s,3,2).flags);
    v.flags=0; CHECK(format_apply(&s,(FormatRange){0,DX_SHEET_ROWS-1,0,16383},FORMAT_ALL,v,&error));
    CHECK(s.count==1 && !s.scope_count && s.source_aware && !format_persistable(&s,&error) && format_valid(&s,NULL)); clear_error(&error);
    CHECK(format_history(&s,false) && !format_persistable(&s,&error)); clear_error(&error);
    CHECK(format_history(&s,true) && s.source_aware && !format_persistable(&s,&error)); clear_error(&error);
    format_trim_history(&s,false,1); CHECK(s.undo_count==1 && format_valid(&s,&error));
    format_free(&copy); format_free(&s); clear_error(&error);
}
static void scoped_legacy(void) {
    FormatStore s={0}; FormatSnapshot plan={0}; CellFormat v=format_default(); char *error=NULL;
    const char atext[]="table=7;version=1",btext[]="table=8;version=1";
    FormatScopeView scopes[]={{7,atext,sizeof(atext)-1},{8,btext,sizeof(btext)-1}};
    v.flags=FORMAT_BOLD;
    for (size_t i=0;i<DX_FORMAT_PATCHES-1;i++) CHECK(format_apply(&s,(FormatRange){i,i,0,0},FORMAT_BOLD,v,&error));
    v.size=16; CHECK(format_apply(&s,(FormatRange){0,DX_SHEET_ROWS-1,0,0},FORMAT_SIZE,v,&error));
    CHECK(s.count==DX_FORMAT_PATCHES && !format_resolve(&s,3,0,&scopes[0]).flags && format_resolve(&s,3,0,&scopes[0]).size==16);
    uint64_t revision=s.revision; CHECK(format_plan_guard_legacy(&s,scopes,2,&plan,&error) && s.revision==revision && !s.scope_count);
    CHECK(plan.count==DX_FORMAT_PATCHES && plan.scope_count==2 && plan.items[0].selector==FORMAT_GUARDED && plan.items[DX_FORMAT_PATCHES-1].selector==FORMAT_SPATIAL);
    CHECK(format_commit(&s,&plan) && format_at(&s,3,0).flags==FORMAT_BOLD && format_resolve(&s,3,0,&scopes[0]).flags==FORMAT_BOLD && format_resolve(&s,3,0,&scopes[1]).flags==FORMAT_BOLD);
    FormatScopeView changed={7,btext,sizeof(btext)-1};
    CHECK(!format_resolve(&s,3,0,&changed).flags && format_resolve(&s,3,0,&changed).size==16);
    revision=s.revision; CHECK(format_plan_guard_legacy(&s,scopes,2,&plan,&error) && !format_commit(&s,&plan) && s.revision==revision);
    CHECK(!format_plan_move(&s,(FormatRange){1,1,0,0},600,1,&plan,&error) && !plan.items && !plan.scopes && s.revision==revision && s.count==DX_FORMAT_PATCHES); clear_error(&error);
    CHECK(format_history(&s,false) && format_persistable(&s,&error) && !format_resolve(&s,3,0,&scopes[0]).flags && format_at(&s,3,0).flags==FORMAT_BOLD);
    CHECK(format_history(&s,true) && format_valid(&s,&error));
    CHECK(format_plan_axis(&s,true,0,DX_SHEET_ROWS-1,false,&plan,&error) && format_commit(&s,&plan) && !s.scope_count && s.source_aware && !format_persistable(&s,&error)); clear_error(&error);
    CHECK(format_history(&s,false) && s.scope_count==2 && format_valid(&s,&error));
    /* Whole-column defaults remain explicit defaults when worksheet rows are
       inserted/deleted; their future-cell meaning must not become finite. */
    CHECK(format_plan_axis(&s,true,0,2,true,&plan,&error) && format_commit(&s,&plan));
    CHECK(format_resolve(&s,0,0,&changed).size==16 && format_resolve(&s,DX_SHEET_ROWS-1,0,&changed).size==16);
    CHECK(format_plan_axis(&s,true,0,2,false,&plan,&error) && format_commit(&s,&plan));
    CHECK(format_resolve(&s,0,0,&changed).size==16 && format_resolve(&s,DX_SHEET_ROWS-1,0,&changed).size==16);
    format_free(&s); clear_error(&error);
}
static void scoped_budgets(void) {
    FormatStore s={0}; FormatSnapshot plan={0}; CellFormat v=format_default(); char *error=NULL;
    char *definition=dx_alloc(DX_FORMAT_SCOPE_BYTES+1); memset(definition,'x',DX_FORMAT_SCOPE_BYTES);
    FormatScopeView maximum={1,definition,DX_FORMAT_SCOPE_BYTES-1}; FormatRange range={1,1,0,0}; v.flags=FORMAT_BOLD;
    CHECK(format_apply_scoped(&s,range,FORMAT_BOLD,v,&maximum,1,true,&error) && format_valid(&s,&error));
    uint64_t revision=s.revision; size_t bytes=format_owned_bytes(&s,true);
    maximum.bytes++; CHECK(!format_apply_scoped(&s,range,FORMAT_BOLD,v,&maximum,1,true,&error) && s.revision==revision && format_owned_bytes(&s,true)==bytes); clear_error(&error);
    CHECK(format_scope_token(&s,&maximum)==SIZE_MAX); maximum.bytes=0;
    CHECK(!format_apply_scoped(&s,range,FORMAT_BOLD,v,&maximum,1,true,&error) && s.revision==revision); clear_error(&error);
    maximum.bytes=1; maximum.definition=NULL;
    CHECK(!format_apply_scoped(&s,range,FORMAT_BOLD,v,&maximum,1,true,&error) && s.revision==revision); clear_error(&error); format_free(&s);
    /* Hit the concrete owned-byte boundary exactly, including all definitions,
       guard references and arrays, rather than only limiting visible text. */
    size_t text_bytes=DX_FORMAT_CURRENT_BYTES-sizeof(FormatPatch)-4*sizeof(FormatScope)-4*sizeof(size_t)-4;
    FormatScopeView scopes[4];
    for (size_t i=0;i<4;i++) scopes[i]=(FormatScopeView){i+1,definition,text_bytes/4+(i<text_bytes%4)};
    CHECK(format_apply_scoped(&s,range,FORMAT_BOLD,v,scopes,4,true,&error) && format_owned_bytes(&s,false)==DX_FORMAT_CURRENT_BYTES && format_valid(&s,&error));
    revision=s.revision; bytes=format_owned_bytes(&s,true);
    /* Adding an incompatible definition cannot evict or truncate an old scope. */
    FormatScopeView extra={5,definition,1}; CHECK(!format_apply_scoped(&s,range,FORMAT_BOLD,v,&extra,1,true,&error) && s.revision==revision && s.scope_count==4 && format_owned_bytes(&s,true)==bytes); clear_error(&error);
    CHECK(!format_plan_move(&s,range,1,1,&plan,&error) && !plan.items && !plan.scopes && s.revision==revision); clear_error(&error);
    /* Malformed external selectors are rejected, not converted to coordinates. */
    FormatPatch *p=&s.items[0]; size_t saved=p->guard_ids[0]; p->guard_ids[0]=s.scope_count;
    CHECK(!format_valid(&s,&error)); clear_error(&error); p->guard_ids[0]=saved;
    saved=p->guard_ids[1]; p->guard_ids[1]=p->guard_ids[0]; CHECK(!format_valid(&s,&error)); clear_error(&error); p->guard_ids[1]=saved;
    p->selector=(FormatSelector)-1; CHECK(!format_valid(&s,&error)); clear_error(&error); p->selector=FORMAT_GUARDED;
    p->guard_count=DX_FORMAT_GUARDS+1; CHECK(!format_valid(&s,&error)); clear_error(&error); p->guard_count=4;
    size_t saved_count=s.scope_count; s.scope_count=SIZE_MAX; CHECK(!format_valid(&s,&error) && format_owned_bytes(&s,false)==SIZE_MAX); clear_error(&error); s.scope_count=saved_count;
    CHECK(format_valid(&s,&error));
    for (size_t i=0;i<31;i++) { v.flags=(i%2) ? FORMAT_BOLD : 0; CHECK(format_apply_scoped(&s,range,FORMAT_BOLD,v,scopes,4,true,&error)); }
    CHECK(s.undo_count==DX_FORMAT_HISTORY && format_owned_bytes(&s,true)==DX_FORMAT_HISTORY_BYTES && format_valid(&s,&error));
    revision=s.revision; v.flags=FORMAT_BOLD;
    CHECK(!format_apply_scoped(&s,range,FORMAT_BOLD,v,scopes,4,true,&error) && s.revision==revision && s.undo_count==DX_FORMAT_HISTORY && strstr(error,"history")); clear_error(&error);
    CHECK(format_history(&s,false) && s.redo_count==1 && format_owned_bytes(&s,true)==DX_FORMAT_HISTORY_BYTES);
    CHECK(format_apply(&s,(FormatRange){0,DX_SHEET_ROWS-1,0,16383},FORMAT_ALL,format_default(),&error) && !s.redo_count && !s.scope_count && format_valid(&s,&error));
    format_trim_history(&s,false,1); CHECK(s.undo_count==1 && format_owned_bytes(&s,true)==DX_FORMAT_CURRENT_BYTES+sizeof(FormatPatch));
    format_free(&s); free(definition); clear_error(&error);
}
static void source_aware_lifetime(void) {
    FormatStore s={0},copy={0},empty={0}; FormatSnapshot plan={0}; CellFormat v=format_default(); char *error=NULL;
    const char definition[]="owner=7;verified-result=1"; FormatScopeView scope={7,definition,sizeof(definition)-1},invalid=scope;
    FormatRange finite={4,4,2,2},column={0,DX_SHEET_ROWS-1,2,2}; v.flags=FORMAT_BOLD;
    CHECK(!s.source_aware && format_valid(&s,&error) && format_persistable(&s,&error));
    CHECK(format_apply(&s,finite,FORMAT_BOLD,v,&error) && !s.source_aware);
    unsigned char live[sizeof(s)]; memcpy(live,&s,sizeof(s)); size_t bytes=format_owned_bytes(&s,true);
    /* Merely preparing/cancelling a migration does not change live mode. */
    CHECK(format_plan_guard_legacy(&s,&scope,1,&plan,&error) && plan.source_aware && !s.source_aware && !memcmp(live,&s,sizeof(s)));
    format_snapshot_free(&plan); CHECK(!plan.source_aware && !plan.items && !memcmp(live,&s,sizeof(s)) && format_owned_bytes(&s,true)==bytes);
    invalid.bytes=0; CHECK(!format_plan_guard_legacy(&s,&invalid,1,&plan,&error) && !plan.source_aware && !memcmp(live,&s,sizeof(s))); clear_error(&error);
    CHECK(!format_apply_scoped(&s,finite,0,v,&scope,1,true,&error) && !memcmp(live,&s,sizeof(s))); clear_error(&error);
    CHECK(format_plan_guard_legacy(&s,&scope,1,&plan,&error) && format_commit(&s,&plan) && s.source_aware && !plan.source_aware);
    CHECK(s.undo_count==2 && !s.undo[1].source_aware && s.items[0].selector==FORMAT_GUARDED);
    FormatStore unsupported=s; unsupported.source_aware=false;
    CHECK(!format_valid(&unsupported,&error) && s.source_aware && format_valid(&s,NULL)); clear_error(&error);
    v.color=3; CHECK(format_apply(&s,finite,FORMAT_COLOR,v,&error) && s.source_aware);
    CHECK(s.scope_count==1 && s.count==2 && s.items[0].selector==FORMAT_GUARDED && s.items[1].selector==FORMAT_SPATIAL);
    /* The broad Bold supersedes the only guarded mask while the unrelated
       finite manual Color survives. No scope remains to infer mode from. */
    CHECK(format_apply(&s,column,FORMAT_BOLD,v,&error) && s.source_aware && !s.scope_count && s.count==2);
    CHECK(s.items[0].selector==FORMAT_SPATIAL && s.items[0].mask==FORMAT_COLOR && s.items[1].mask==FORMAT_BOLD);
    CHECK(format_at(&s,4,2).color==3 && format_at(&s,4,2).flags==FORMAT_BOLD);
    CHECK(format_resolve_token(&s,4,2,SIZE_MAX).color==0 && format_resolve_token(&s,4,2,SIZE_MAX).flags==FORMAT_BOLD);
    CHECK(format_valid(&s,&error) && !format_persistable(&s,&error) && strstr(error,"v12")); clear_error(&error);
    format_copy(&copy,&s); CHECK(copy.source_aware && !copy.scope_count && copy.items!=s.items && !copy.undo_count && format_owned_bytes(&copy,false)==format_owned_bytes(&s,false));
    format_copy(&copy,&copy); CHECK(copy.source_aware && format_at(&copy,4,2).color==3);
    CHECK(format_history(&s,false) && s.source_aware && s.scope_count==1 && s.redo[0].source_aware);
    CHECK(format_history(&s,false) && s.source_aware && s.count==1);
    CHECK(format_history(&s,false) && !s.source_aware && !s.scope_count && s.items[0].selector==FORMAT_SPATIAL && format_persistable(&s,&error));
    CHECK(format_history(&s,true) && s.source_aware && s.scope_count==1);
    CHECK(format_history(&s,true) && s.source_aware && format_at(&s,4,2).color==3);
    CHECK(format_history(&s,true) && s.source_aware && !s.scope_count && format_at(&s,4,2).color==3);
    uint64_t revision=s.revision; memcpy(live,&s,sizeof(s));
    CHECK(format_plan_move(&s,finite,0,0,&plan,&error) && plan.source_aware && !format_commit(&s,&plan) && !plan.source_aware && !memcmp(live,&s,sizeof(s)));
    CHECK(!format_plan_move(&s,(FormatRange){0,0,0,0},-1,0,&plan,&error) && !plan.source_aware && !memcmp(live,&s,sizeof(s))); clear_error(&error);
    CHECK(format_plan_move(&s,finite,3,1,&plan,&error) && plan.source_aware && format_commit(&s,&plan) && s.source_aware && s.revision==revision+1 && format_at(&s,7,3).color==3);
    CHECK(format_history(&s,false) && s.source_aware && format_at(&s,4,2).color==3);
    CHECK(format_history(&s,true) && s.source_aware && format_at(&s,7,3).color==3);
    /* Empty migration is a real mode-only edit; equality/history must retain it
       despite zero owned arrays or definitions. Repeating it is a no-op. */
    CHECK(format_plan_guard_legacy(&empty,&scope,1,&plan,&error) && plan.source_aware && !plan.count && !plan.scope_count && !empty.source_aware);
    format_snapshot_free(&plan); CHECK(!plan.source_aware && !empty.source_aware && !empty.revision && !empty.undo_count);
    CHECK(format_plan_guard_legacy(&empty,&scope,1,&plan,&error) && format_commit(&empty,&plan) && empty.source_aware && empty.undo_count==1 && empty.revision==1);
    CHECK(!format_persistable(&empty,&error) && strstr(error,"v12") && !format_owned_bytes(&empty,true)); clear_error(&error);
    CHECK(format_history(&empty,false) && !empty.source_aware && empty.redo_count==1 && empty.redo[0].source_aware);
    CHECK(format_history(&empty,true) && empty.source_aware && empty.undo_count==1 && !empty.undo[0].source_aware);
    revision=empty.revision; size_t undo=empty.undo_count;
    CHECK(format_plan_guard_legacy(&empty,&scope,1,&plan,&error) && plan.source_aware && !format_commit(&empty,&plan) && !plan.source_aware && empty.revision==revision && empty.undo_count==undo);
    CHECK(format_apply(&empty,finite,FORMAT_COLOR,v,&error) && empty.source_aware);
    CHECK(format_plan_axis(&empty,true,4,1,false,&plan,&error) && plan.source_aware && !plan.count && format_commit(&empty,&plan) && empty.source_aware && !empty.count && !empty.scope_count);
    CHECK(format_history(&empty,false) && empty.source_aware && format_at(&empty,4,2).color==3);
    CHECK(format_history(&empty,true) && empty.source_aware && !empty.count && format_valid(&empty,&error));
    format_copy(&copy,&empty); CHECK(copy.source_aware && !copy.count && !copy.scope_count && !copy.undo_count);
    format_free(&copy); CHECK(!copy.source_aware && !copy.revision && !format_owned_bytes(&copy,true));
    format_free(&empty); CHECK(!empty.source_aware && !empty.undo_count && !empty.redo_count);
    format_free(&s); CHECK(!s.source_aware && !s.undo_count && !s.redo_count); clear_error(&error);
}
static uint32_t random_step(uint32_t *state) { *state=*state*UINT32_C(1664525)+UINT32_C(1013904223); return *state; }
static void scoped_precedence(void) {
    /* Independent small-sheet oracle: compare the final visible masks for each
       result owner/definition and manual cells after overlapping commands. */
    const char first[]="query=A;version=1",second[]="query=B;version=1";
    FormatScopeView scopes[]={{1,first,sizeof(first)-1},{1,second,sizeof(second)-1},{2,first,sizeof(first)-1},{3,first,sizeof(first)-1}};
    unsigned char expected[5][8][4]={0}; FormatStore s={0}; char *error=NULL; uint32_t random=UINT32_C(716271);
    for (unsigned command=0;command<160;command++) {
        unsigned mode=random_step(&random)%5,owner=random_step(&random)%3,mask=1u+(random_step(&random)%7);
        uint64_t row=random_step(&random)%8,rows=1+random_step(&random)%(8-row);
        size_t col=random_step(&random)%4,columns=1+random_step(&random)%(4-col);
        FormatRange range={row,row+rows-1,col,col+columns-1}; CellFormat value=format_default(); value.flags=(unsigned char)(random_step(&random)%8);
        bool spatial=mode<2,manual=mode==4;
        if (mode==1) { range.r0=0; range.r1=DX_SHEET_ROWS-1; }
        CHECK(spatial ? format_apply(&s,range,mask,value,&error) : format_apply_scoped(&s,range,mask,value,&scopes[owner],1,manual,&error));
        for (size_t context=0;context<5;context++) for (size_t r=0;r<8;r++) for (size_t c=0;c<4;c++) {
            bool eligible=context==0 ? spatial || manual : (spatial ? mode==1 : context==owner+1);
            if (eligible && r>=range.r0 && r<=range.r1 && c>=range.c0 && c<=range.c1) expected[context][r][c]=(unsigned char)((expected[context][r][c]&~mask)|(value.flags&mask));
            CHECK(format_resolve(&s,r,c,context ? &scopes[context-1] : NULL).flags==expected[context][r][c]);
        }
        CHECK(format_valid(&s,&error));
    }
    format_free(&s); clear_error(&error);
}
static CellFormat record_at(const FormatStore *s,uint64_t row,size_t col,const FormatScopeView *scope,uint64_t id,size_t field) {
    return format_resolve_record_token(s,row,col,format_scope_token(s,scope),format_scope_token(s,scope),scope ? id : 0,field);
}
static void record_resolution(void) {
    FormatStore s={0},copy={0}; CellFormat v=format_default(); char *error=NULL;
    unsigned char definition[]={'v','1',0,'D','E','C','I','M','A','L','(','3','8',',','2',')'};
    FormatScopeView scope={UINT64_C(9007199254740993),definition,sizeof(definition)},other=scope;
    uint64_t ids[]={1,UINT64_C(9007199254740993),UINT64_MAX}; size_t fields[]={0,3,16383};
    v.flags=FORMAT_BOLD; v.color=2;
    CHECK(format_apply_records(&s,&scope,ids,3,fields,3,FORMAT_BOLD|FORMAT_COLOR,v,&error));
    CHECK(s.count==1 && s.scope_count==1 && s.items[0].selector==FORMAT_RECORDS && format_valid(&s,&error));
    CHECK(s.items[0].record_ids!=ids && s.items[0].fields!=fields && s.scopes[0].definition!=definition);
    size_t expected=sizeof(FormatPatch)+sizeof(FormatScope)+sizeof(size_t)+sizeof(ids)+sizeof(fields)+sizeof(definition)+1;
    CHECK(format_owned_bytes(&s,false)==expected && format_owned_bytes(&s,true)==expected);
    CHECK(record_at(&s,4,2,&scope,ids[1],3).flags==FORMAT_BOLD && record_at(&s,900,15,&scope,ids[1],3).color==2);
    CHECK(record_at(&s,4,2,&scope,UINT64_MAX,16383).flags==FORMAT_BOLD);
    CellFormat last=record_at(&s,DX_SHEET_ROWS-1,DX_FORMAT_FIELDS-1,&scope,UINT64_MAX,16383);
    CHECK(last.flags==FORMAT_BOLD && last.color==2 && last.specified==(FORMAT_BOLD|FORMAT_COLOR));
    const uint64_t outside_rows[]={DX_SHEET_ROWS,UINT64_MAX}; const size_t outside_columns[]={DX_FORMAT_FIELDS,SIZE_MAX};
    for (size_t i=0;i<2;i++) {
        CellFormat row=record_at(&s,outside_rows[i],2,&scope,UINT64_MAX,16383),column=record_at(&s,4,outside_columns[i],&scope,UINT64_MAX,16383);
        CHECK(!row.flags && !row.color && row.size==13 && row.decimals==2 && !row.specified);
        CHECK(!column.flags && !column.color && column.size==13 && column.decimals==2 && !column.specified);
    }
    /* Same-looking physical rows remain independent when their record IDs differ. */
    CHECK(!record_at(&s,4,2,&scope,2,3).flags && !record_at(&s,5,2,&scope,2,3).flags);
    CHECK(!record_at(&s,4,2,&scope,ids[1],2).flags && !record_at(&s,4,2,&scope,0,3).flags);
    CHECK(!record_at(&s,4,2,&scope,ids[1],16384).flags && !record_at(&s,4,2,NULL,ids[1],3).flags);
    size_t token=format_scope_token(&s,&scope);
    CHECK(!format_at(&s,0,0).flags && !format_resolve(&s,0,0,&scope).flags && !format_resolve_token(&s,0,0,token).flags);
    CHECK(!format_resolve_record_token(&s,0,0,SIZE_MAX,SIZE_MAX,ids[1],3).flags && !format_resolve_record_token(&s,0,0,2,2,ids[1],3).flags);
    other.owner++; CHECK(!record_at(&s,4,2,&other,ids[1],3).flags);
    definition[sizeof(definition)-2]='3'; CHECK(format_scope_token(&s,&scope)==SIZE_MAX && !record_at(&s,4,2,&scope,ids[1],3).flags); definition[sizeof(definition)-2]='2';
    definition[1]='2'; CHECK(!record_at(&s,4,2,&scope,ids[1],3).flags); definition[1]='1';
    /* Input mutation cannot change the owned membership or scope. */
    ids[1]=2; fields[1]=2;
    CHECK(record_at(&s,4,2,&scope,UINT64_C(9007199254740993),3).flags==FORMAT_BOLD && !record_at(&s,4,2,&scope,2,2).flags);
    ids[1]=UINT64_C(9007199254740993); fields[1]=3;
    uint64_t revision=s.revision; size_t bytes=format_owned_bytes(&s,true);
    CHECK(format_apply_records(&s,&scope,ids,3,fields,3,FORMAT_BOLD|FORMAT_COLOR,v,&error) && s.revision==revision && format_owned_bytes(&s,true)==bytes);
    CHECK(!format_persistable(&s,&error) && strstr(error,"v12")); clear_error(&error);
    /* All selectors participate in one mask-wise order, using physical bounds
       for spatial/result patches and original ordinals only for membership. */
    v.color=1; CHECK(format_apply(&s,(FormatRange){0,DX_SHEET_ROWS-1,2,2},FORMAT_COLOR,v,&error));
    CHECK(!record_at(&s,UINT64_MAX,2,&scope,UINT64_MAX,16383).specified && !format_resolve(&s,UINT64_MAX,2,&scope).specified && !format_at(&s,DX_SHEET_ROWS,2).specified);
    CHECK(!format_resolve(&s,4,DX_FORMAT_FIELDS,&scope).specified && !format_at(&s,4,SIZE_MAX).specified);
    CHECK(record_at(&s,4,2,&scope,ids[1],3).flags==FORMAT_BOLD && record_at(&s,4,2,&scope,ids[1],3).color==1 && record_at(&s,4,9,&scope,ids[1],3).color==2);
    CHECK(s.items[0].selector==FORMAT_RECORDS && s.items[0].record_count==3);
    v.flags=FORMAT_ITALIC; CHECK(format_apply_scoped(&s,(FormatRange){4,4,2,2},FORMAT_BOLD|FORMAT_ITALIC,v,&scope,1,false,&error));
    CHECK(record_at(&s,4,2,&scope,ids[1],3).flags==FORMAT_ITALIC && record_at(&s,5,2,&scope,ids[1],3).flags==FORMAT_BOLD);
    uint64_t one[]={ids[1]}; size_t field[]={3}; v.flags=FORMAT_UNDERLINE; v.size=16;
    CHECK(format_apply_records(&s,&scope,one,1,field,1,FORMAT_ALL,v,&error));
    CellFormat got=record_at(&s,4,2,&scope,ids[1],3);
    CHECK(got.flags==FORMAT_UNDERLINE && got.size==16 && got.specified==FORMAT_ALL);
    CHECK(record_at(&s,4,2,&scope,ids[0],3).flags==FORMAT_ITALIC && record_at(&s,5,2,&scope,ids[0],3).flags==FORMAT_BOLD);
    CHECK(format_history(&s,false) && record_at(&s,4,2,&scope,ids[1],3).flags==FORMAT_ITALIC);
    CHECK(format_history(&s,true) && record_at(&s,4,2,&scope,ids[1],3).flags==FORMAT_UNDERLINE);
    format_copy(&copy,&s); CHECK(copy.items!=s.items && copy.scope_count==s.scope_count && !copy.undo_count);
    for (size_t i=0;i<s.count;i++) if (s.items[i].selector==FORMAT_RECORDS) CHECK(copy.items[i].record_ids!=s.items[i].record_ids && copy.items[i].fields!=s.items[i].fields && !memcmp(copy.items[i].record_ids,s.items[i].record_ids,s.items[i].record_count*sizeof(uint64_t)));
    format_copy(&copy,&copy); CHECK(format_valid(&copy,&error) && format_owned_bytes(&copy,false)==format_owned_bytes(&s,false));
    format_free(&s); CHECK(record_at(&copy,4,2,&scope,one[0],3).flags==FORMAT_UNDERLINE && format_valid(&copy,&error));
    format_free(&copy); CHECK(!copy.items && !copy.scopes && !copy.undo_count && !copy.redo_count && !format_owned_bytes(&copy,true)); clear_error(&error);
}
static void record_planners(void) {
    FormatStore s={0}; FormatSnapshot plan={0}; CellFormat v=format_default(); char *error=NULL;
    const char definition[]="raw-version=2;reader=csv;digest=abc;schema=(a:INTEGER,b:VARCHAR)";
    FormatScopeView scope={17,definition,sizeof(definition)-1},dormant={18,definition,sizeof(definition)-1};
    uint64_t ids[]={1,8,UINT64_MAX}; size_t fields[]={0,1}; v.flags=FORMAT_BOLD;
    CHECK(format_apply_records(&s,&scope,ids,3,fields,2,FORMAT_BOLD,v,&error));
    CHECK(format_apply_records(&s,&dormant,ids,3,fields,2,FORMAT_BOLD,v,&error));
    uint64_t revision=s.revision; size_t bytes=format_owned_bytes(&s,true);
    CHECK(format_plan_move(&s,(FormatRange){0,5,0,2},5,5,&plan,&error) && !format_commit(&s,&plan) && s.revision==revision && format_owned_bytes(&s,true)==bytes);
    CHECK(format_plan_axis(&s,true,0,DX_SHEET_ROWS-1,false,&plan,&error) && !format_commit(&s,&plan));
    CHECK(format_plan_axis(&s,false,0,16384,false,&plan,&error) && !format_commit(&s,&plan));
    CHECK(s.scope_count==2 && s.count==2 && record_at(&s,0,0,&dormant,8,1).flags==FORMAT_BOLD);
    v.size=16; CHECK(format_apply(&s,(FormatRange){0,2,0,1},FORMAT_SIZE,v,&error));
    CHECK(format_plan_move(&s,(FormatRange){0,2,0,1},10,4,&plan,&error) && format_commit(&s,&plan));
    CHECK(format_at(&s,10,4).size==16 && record_at(&s,10,4,&scope,8,1).flags==FORMAT_BOLD && record_at(&s,10,4,&scope,8,1).size==13);
    CHECK(format_history(&s,false) && format_at(&s,0,0).size==16);
    CHECK(format_plan_guard_legacy(&s,&scope,1,&plan,&error) && format_commit(&s,&plan));
    CHECK(s.items[0].selector==FORMAT_RECORDS && s.items[1].selector==FORMAT_RECORDS && s.items[2].selector==FORMAT_GUARDED);
    revision=s.revision; bytes=format_owned_bytes(&s,true); size_t undo=s.undo_count,redo=s.redo_count;
    /* Full positional scoped resets could mask an existing raw-record patch,
       whose scope deliberately differs from the exact query result definition.
       Refuse the mixed operation before modifying any owned state/history. */
    CHECK(!format_plan_move(&s,(FormatRange){0,2,0,1},10,4,&plan,&error) && strstr(error,"record-format precedence") && !plan.items && !plan.scopes);
    CHECK(s.revision==revision && s.undo_count==undo && s.redo_count==redo && format_owned_bytes(&s,true)==bytes && s.count==3 && s.scope_count==2);
    CHECK(record_at(&s,0,0,&scope,8,1).flags==FORMAT_BOLD && record_at(&s,0,0,&scope,8,1).size==16 && record_at(&s,10,4,&scope,8,1).flags==FORMAT_BOLD); clear_error(&error);
    CHECK(format_plan_move(&s,(FormatRange){0,2,0,1},0,0,&plan,&error) && !format_commit(&s,&plan) && s.revision==revision);
    CHECK(format_plan_move(&s,(FormatRange){100,102,8,9},10,4,&plan,&error) && !format_commit(&s,&plan) && s.revision==revision);
    CHECK(format_plan_axis(&s,true,0,3,true,&plan,&error) && format_commit(&s,&plan));
    CHECK(format_plan_axis(&s,false,0,2,true,&plan,&error) && format_commit(&s,&plan));
    CHECK(record_at(&s,3,2,&scope,8,1).flags==FORMAT_BOLD && record_at(&s,3,2,&scope,8,1).size==16);
    CHECK(format_history(&s,false) && s.redo_count==1);
    revision=s.revision; bytes=format_owned_bytes(&s,true); undo=s.undo_count;
    CHECK(!format_plan_move(&s,(FormatRange){3,5,0,1},10,4,&plan,&error) && !plan.items && !plan.scopes);
    CHECK(s.revision==revision && s.undo_count==undo && s.redo_count==1 && format_owned_bytes(&s,true)==bytes && record_at(&s,3,0,&scope,8,1).flags==FORMAT_BOLD); clear_error(&error);
    CHECK(format_history(&s,true) && record_at(&s,3,2,&scope,8,1).size==16);
    CHECK(format_plan_axis(&s,true,0,3,false,&plan,&error) && format_commit(&s,&plan));
    CHECK(format_plan_axis(&s,false,0,2,false,&plan,&error) && format_commit(&s,&plan));
    CHECK(record_at(&s,0,0,&scope,8,1).size==16 && record_at(&s,0,0,&dormant,UINT64_MAX,0).flags==FORMAT_BOLD);
    for (size_t i=0;i<s.count;i++) if (s.items[i].selector==FORMAT_RECORDS) CHECK(!memcmp(s.items[i].record_ids,ids,sizeof(ids)) && !memcmp(s.items[i].fields,fields,sizeof(fields)) && s.items[i].range.r0==0 && s.items[i].range.r1==0 && s.items[i].range.c0==0 && s.items[i].range.c1==0);
    CHECK(format_valid(&s,&error));
    v.flags=0; CHECK(format_apply(&s,(FormatRange){0,DX_SHEET_ROWS-1,0,16383},FORMAT_ALL,v,&error));
    CHECK(!record_at(&s,0,0,&scope,8,1).flags && s.scope_count==2 && s.items[0].selector==FORMAT_RECORDS && s.items[1].selector==FORMAT_RECORDS);
    CHECK(format_history(&s,false) && record_at(&s,200,200,&scope,8,1).flags==FORMAT_BOLD);
    CHECK(format_history(&s,true) && !record_at(&s,200,200,&scope,8,1).flags);
    format_trim_history(&s,false,1); CHECK(s.undo_count==1 && format_valid(&s,&error)); format_free(&s);
    /* An unrelated dormant owner does not make another owner's move unsafe. */
    v.flags=FORMAT_BOLD; CHECK(format_apply_records(&s,&dormant,ids,3,fields,2,FORMAT_BOLD,v,&error));
    CHECK(format_apply_scoped(&s,(FormatRange){0,2,0,1},FORMAT_SIZE,v,&scope,1,false,&error));
    CHECK(format_plan_move(&s,(FormatRange){0,2,0,1},10,4,&plan,&error) && format_commit(&s,&plan));
    CHECK(s.scope_count==2 && record_at(&s,10,4,&dormant,8,1).flags==FORMAT_BOLD && record_at(&s,10,4,&scope,8,1).size==16 && format_valid(&s,&error));
    format_free(&s); clear_error(&error);
}
static void record_dual_scopes(void) {
    FormatStore s={0}; CellFormat v=format_default(); char *error=NULL;
    const char raw_text[]="source=1;version=7;reader=csv;schema=(a:DECIMAL(38,2),b:VARCHAR)";
    const char result_text[]="source=1;version=7;sort=desc;filter=(a>0);schema=(a:DECIMAL(38,2),b:VARCHAR)";
    const char changed_text[]="source=1;version=7;sort=asc;filter=(a>0);schema=(a:DECIMAL(38,2),b:VARCHAR)";
    FormatScopeView raw={0,raw_text,sizeof(raw_text)-1},result={0,result_text,sizeof(result_text)-1},changed={0,changed_text,sizeof(changed_text)-1};
    uint64_t ids[]={1}; size_t fields[]={0}; v.flags=FORMAT_UNDERLINE; v.size=16;
    CHECK(format_apply(&s,(FormatRange){0,5,0,2},FORMAT_UNDERLINE|FORMAT_SIZE,v,&error));
    v.color=3; CHECK(format_apply(&s,(FormatRange){0,DX_SHEET_ROWS-1,0,16383},FORMAT_COLOR,v,&error));
    v.flags=FORMAT_BOLD; CHECK(format_apply_records(&s,&raw,ids,1,fields,1,FORMAT_BOLD,v,&error));
    v.flags=FORMAT_ITALIC; CHECK(format_apply_scoped(&s,(FormatRange){0,5,0,2},FORMAT_ITALIC,v,&result,1,false,&error));
    size_t rt=format_scope_token(&s,&raw),qt=format_scope_token(&s,&result),missing=format_scope_token(&s,&changed);
    CHECK(rt && rt!=SIZE_MAX && qt && qt!=SIZE_MAX && rt!=qt && missing==SIZE_MAX);
    CellFormat got=format_resolve_record_token(&s,2,1,qt,rt,1,0);
    CHECK(got.flags==(FORMAT_BOLD|FORMAT_ITALIC) && got.size==13 && got.color==3);
    got=format_resolve_record_token(&s,2,1,missing,rt,1,0);
    CHECK(got.flags==FORMAT_BOLD && got.size==13 && got.color==3);
    got=format_resolve_record_token(&s,2,1,qt,SIZE_MAX,1,0);
    CHECK(got.flags==FORMAT_ITALIC && got.size==13 && got.color==3);
    got=format_resolve_record_token(&s,2,1,0,rt,1,0);
    CHECK(got.flags==FORMAT_BOLD && got.size==13 && got.color==3);
    got=format_resolve_record_token(&s,2,1,0,SIZE_MAX,1,0);
    CHECK(!got.flags && got.size==13 && got.color==3);
    got=format_resolve_record_token(&s,2,1,qt,0,1,0);
    CHECK(got.flags==FORMAT_ITALIC && got.size==13 && got.color==3);
    got=format_resolve_record_token(&s,2,1,0,0,1,0);
    CHECK(!got.flags && got.size==13 && got.color==3);
    CHECK(format_resolve(&s,2,1,&result).flags==FORMAT_ITALIC && format_at(&s,2,1).flags==FORMAT_UNDERLINE && format_at(&s,2,1).size==16);
    /* New sort/filter definitions still match the immutable raw binding. */
    CHECK(format_apply_scoped(&s,(FormatRange){0,5,0,2},FORMAT_ITALIC,v,&changed,1,false,&error));
    CHECK(format_resolve_record_token(&s,2,1,format_scope_token(&s,&changed),format_scope_token(&s,&raw),1,0).flags==(FORMAT_BOLD|FORMAT_ITALIC));
    FormatScopeView other_raw=raw,other_result=result; other_raw.owner=other_result.owner=1;
    v.color=2; CHECK(format_apply_records(&s,&other_raw,ids,1,fields,1,FORMAT_COLOR,v,&error));
    CHECK(format_apply_scoped(&s,(FormatRange){0,5,0,2},FORMAT_ITALIC,v,&other_result,1,false,&error));
    got=format_resolve_record_token(&s,2,1,format_scope_token(&s,&result),format_scope_token(&s,&other_raw),1,0);
    CHECK(!got.flags && got.size==13 && got.color==3);
    got=format_resolve_record_token(&s,2,1,format_scope_token(&s,&other_result),format_scope_token(&s,&raw),1,0);
    CHECK(!got.flags && got.size==13 && got.color==3);
    got=format_resolve_record_token(&s,2,1,format_scope_token(&s,&other_result),format_scope_token(&s,&other_raw),1,0);
    CHECK(got.flags==FORMAT_ITALIC && got.size==13 && got.color==2);
    got=format_resolve_record_token(&s,2,1,format_scope_token(&s,&result),format_scope_token(&s,&raw),1,0);
    CHECK(got.flags==(FORMAT_BOLD|FORMAT_ITALIC) && got.size==13 && got.color==3);
    FormatSnapshot plan={0}; uint64_t revision=s.revision; size_t bytes=format_owned_bytes(&s,true),undo=s.undo_count;
    CHECK(!format_plan_move(&s,(FormatRange){0,5,0,2},10,4,&plan,&error) && !plan.items && !plan.scopes && s.revision==revision && s.undo_count==undo && format_owned_bytes(&s,true)==bytes); clear_error(&error);
    CHECK(format_valid(&s,&error)); format_free(&s); clear_error(&error);
}
static void record_invalid(void) {
    FormatStore s={0}; CellFormat v=format_default(); char *error=NULL;
    const char definition[]="typed-schema"; FormatScopeView scope={1,definition,sizeof(definition)-1};
    uint64_t ids[]={1,2,3}; size_t fields[]={0,1,16383}; v.flags=FORMAT_BOLD;
    CHECK(format_apply_records(&s,&scope,ids,3,fields,3,FORMAT_BOLD,v,&error));
    v.flags=0; CHECK(format_apply_records(&s,&scope,ids,3,fields,3,FORMAT_BOLD,v,&error) && format_history(&s,false));
    uint64_t revision=s.revision; size_t bytes=format_owned_bytes(&s,true),undo=s.undo_count,redo=s.redo_count;
#define BAD_RECORDS(scope_,ids_,count_,fields_,fields_count_,mask_,value_) do { \
    CHECK(!format_apply_records(&s,(scope_),(ids_),(count_),(fields_),(fields_count_),(mask_),(value_),&error)); \
    CHECK(s.revision==revision && s.undo_count==undo && s.redo_count==redo && format_owned_bytes(&s,true)==bytes && record_at(&s,2,4,&scope,2,1).flags==FORMAT_BOLD); clear_error(&error); \
} while (0)
    BAD_RECORDS(NULL,ids,3,fields,3,FORMAT_BOLD,v);
    BAD_RECORDS(&scope,NULL,3,fields,3,FORMAT_BOLD,v);
    BAD_RECORDS(&scope,ids,0,fields,3,FORMAT_BOLD,v);
    BAD_RECORDS(&scope,ids,DX_FORMAT_RECORDS+1,fields,3,FORMAT_BOLD,v);
    BAD_RECORDS(&scope,ids,3,NULL,3,FORMAT_BOLD,v);
    BAD_RECORDS(&scope,ids,3,fields,0,FORMAT_BOLD,v);
    BAD_RECORDS(&scope,ids,3,fields,DX_FORMAT_FIELDS+1,FORMAT_BOLD,v);
    ids[0]=0; BAD_RECORDS(&scope,ids,3,fields,3,FORMAT_BOLD,v); ids[0]=1;
    ids[1]=1; BAD_RECORDS(&scope,ids,3,fields,3,FORMAT_BOLD,v); ids[1]=2;
    ids[0]=3; BAD_RECORDS(&scope,ids,3,fields,3,FORMAT_BOLD,v); ids[0]=1;
    fields[1]=0; BAD_RECORDS(&scope,ids,3,fields,3,FORMAT_BOLD,v); fields[1]=1;
    fields[2]=16384; BAD_RECORDS(&scope,ids,3,fields,3,FORMAT_BOLD,v); fields[2]=16383;
    fields[0]=2; BAD_RECORDS(&scope,ids,3,fields,3,FORMAT_BOLD,v); fields[0]=0;
    BAD_RECORDS(&scope,ids,3,fields,3,0,v);
    BAD_RECORDS(&scope,ids,3,fields,3,FORMAT_ALL+1,v);
    v.size=17; BAD_RECORDS(&scope,ids,3,fields,3,FORMAT_BOLD,v); v.size=13;
    FormatScopeView bad=scope; bad.definition=NULL; BAD_RECORDS(&bad,ids,3,fields,3,FORMAT_BOLD,v);
    bad=scope; bad.bytes=0; BAD_RECORDS(&bad,ids,3,fields,3,FORMAT_BOLD,v);
    bad.bytes=DX_FORMAT_SCOPE_BYTES; BAD_RECORDS(&bad,ids,3,fields,3,FORMAT_BOLD,v);
#undef BAD_RECORDS
    FormatPatch *p=&s.items[0]; p->record_ids[0]=0; CHECK(!format_valid(&s,&error)); clear_error(&error); p->record_ids[0]=1;
    p->record_ids[1]=1; CHECK(!format_valid(&s,&error)); clear_error(&error); p->record_ids[1]=2;
    p->fields[1]=0; CHECK(!format_valid(&s,&error)); clear_error(&error); p->fields[1]=1;
    p->fields[2]=16384; CHECK(!format_valid(&s,&error)); clear_error(&error); p->fields[2]=16383;
    p->range.c0=p->range.c1=1; CHECK(!format_valid(&s,&error)); clear_error(&error); p->range.c0=p->range.c1=0;
    p->selector=FORMAT_RESULT; CHECK(!format_valid(&s,&error)); clear_error(&error); p->selector=FORMAT_RECORDS;
    p->guard_count=2; CHECK(!format_valid(&s,&error)); clear_error(&error); p->guard_count=1;
    p->record_count=SIZE_MAX; CHECK(!format_valid(&s,&error) && format_owned_bytes(&s,false)==SIZE_MAX); clear_error(&error); p->record_count=3;
    p->field_count=SIZE_MAX; CHECK(!format_valid(&s,&error) && format_owned_bytes(&s,false)==SIZE_MAX); clear_error(&error); p->field_count=3;
    CHECK(format_valid(&s,&error) && format_history(&s,true) && !record_at(&s,2,4,&scope,2,1).flags);
    format_free(&s); clear_error(&error);
}
static void record_edit_plan(void) {
    FormatStore s={0}; FormatSnapshot plan={0}; CellFormat v=format_default(); char *error=NULL;
    const char original[]="raw-version=1;schema=(a:BIGINT,b:VARCHAR)";
    char definition[]="raw-version=2;schema=(a:BIGINT,b:VARCHAR)";
    FormatScopeView old={17,original,sizeof(original)-1},scope={17,definition,sizeof(definition)-1};
    uint64_t ids[]={1,UINT64_C(9007199254740993),UINT64_MAX}; size_t fields[]={0,1};
    v.flags=FORMAT_BOLD; v.color=2; CHECK(format_apply_records(&s,&old,ids,3,fields,2,FORMAT_BOLD|FORMAT_COLOR,v,&error));
    v.color=1; CHECK(format_apply_records(&s,&old,ids,3,fields,2,FORMAT_COLOR,v,&error) && format_history(&s,false) && s.redo_count==1);
    unsigned char live[sizeof(s)]; memcpy(live,&s,sizeof(s)); size_t bytes=format_owned_bytes(&s,true);
    v.color=3; v.size=16;
    CHECK(format_plan_records(&s,&scope,ids,3,fields,2,FORMAT_COLOR|FORMAT_SIZE,v,&plan,&error));
    CHECK(!memcmp(live,&s,sizeof(s)) && format_owned_bytes(&s,true)==bytes && record_at(&s,4,2,&old,ids[1],1).color==2 && s.redo_count==1);
    CHECK(plan.source_aware && s.source_aware && plan.count==2 && plan.scope_count==2 && plan.items!=s.items && plan.scopes!=s.scopes);
    CHECK(plan.items[1].record_ids!=ids && plan.items[1].fields!=fields && plan.scopes[1].definition!=definition);
    ids[1]=2; fields[1]=3; definition[0]='X';
    CHECK(plan.items[1].record_ids[1]==UINT64_C(9007199254740993) && plan.items[1].fields[1]==1 && !memcmp(plan.scopes[1].definition,"raw-version=2;",14));
    CHECK(!memcmp(live,&s,sizeof(s)) && format_owned_bytes(&s,true)==bytes);
    ids[1]=UINT64_C(9007199254740993); fields[1]=1; definition[0]='r';
    unsigned char owned[sizeof(plan)]; memcpy(owned,&plan,sizeof(plan));
#define BAD_PLAN(scope_,ids_,count_,fields_,field_count_,mask_,value_) do { \
    CHECK(!format_plan_records(&s,(scope_),(ids_),(count_),(fields_),(field_count_),(mask_),(value_),&plan,&error)); \
    CHECK(!memcmp(live,&s,sizeof(s)) && format_owned_bytes(&s,true)==bytes && !memcmp(owned,&plan,sizeof(plan)) && plan.items[1].record_ids[1]==UINT64_C(9007199254740993)); clear_error(&error); \
} while (0)
    BAD_PLAN(NULL,ids,3,fields,2,FORMAT_COLOR,v);
    BAD_PLAN(&scope,ids,DX_FORMAT_RECORDS+1,fields,2,FORMAT_COLOR,v);
    ids[1]=1; BAD_PLAN(&scope,ids,3,fields,2,FORMAT_COLOR,v); ids[1]=UINT64_C(9007199254740993);
    fields[1]=16384; BAD_PLAN(&scope,ids,3,fields,2,FORMAT_COLOR,v); fields[1]=1;
    BAD_PLAN(&scope,ids,3,fields,2,0,v);
    v.size=17; BAD_PLAN(&scope,ids,3,fields,2,FORMAT_SIZE,v); v.size=16;
    FormatScopeView invalid=scope; invalid.bytes=0; BAD_PLAN(&invalid,ids,3,fields,2,FORMAT_COLOR,v);
#undef BAD_PLAN
    CHECK(!format_plan_records(&s,&scope,ids,3,fields,2,FORMAT_COLOR,v,NULL,&error) && !memcmp(live,&s,sizeof(s))); clear_error(&error);
    /* Replacing an owned plan may borrow that plan's current inputs: the new
       state must be fully copied before the old output is released. */
    FormatScopeView borrowed={plan.scopes[1].owner,plan.scopes[1].definition,plan.scopes[1].bytes}; FormatPatch *prior_items=plan.items;
    CHECK(format_plan_records(&s,&borrowed,plan.items[1].record_ids,3,plan.items[1].fields,2,FORMAT_COLOR|FORMAT_SIZE,v,&plan,&error));
    CHECK(plan.items!=prior_items && !memcmp(live,&s,sizeof(s)) && plan.items[1].record_ids[2]==UINT64_MAX && plan.items[1].fields[1]==1);
    format_snapshot_free(&plan); CHECK(!plan.items && !plan.scopes && !memcmp(live,&s,sizeof(s)) && format_owned_bytes(&s,true)==bytes);
    CHECK(!format_plan_records(&s,&invalid,ids,3,fields,2,FORMAT_COLOR,v,&plan,&error) && !plan.items && !plan.scopes && !plan.count && !plan.scope_count && !memcmp(live,&s,sizeof(s))); clear_error(&error);
    CHECK(format_plan_records(&s,&scope,ids,3,fields,2,FORMAT_COLOR|FORMAT_SIZE,v,&plan,&error));
    uint64_t revision=s.revision;
    CHECK(format_commit(&s,&plan) && !plan.items && !plan.scopes && s.revision==revision+1 && !s.redo_count);
    CHECK(record_at(&s,4,2,&scope,ids[1],1).color==3 && record_at(&s,4,2,&scope,ids[1],1).size==16 && record_at(&s,4,2,&old,ids[1],1).color==2);
    CHECK(format_history(&s,false) && s.redo_count==1 && format_scope_token(&s,&scope)==SIZE_MAX);
    memcpy(live,&s,sizeof(s)); bytes=format_owned_bytes(&s,true); v.color=2;
    CHECK(format_plan_records(&s,&old,ids,3,fields,2,FORMAT_BOLD|FORMAT_COLOR,v,&plan,&error));
    CHECK(!memcmp(live,&s,sizeof(s)) && !format_commit(&s,&plan) && !plan.items && !plan.scopes && !memcmp(live,&s,sizeof(s)) && format_owned_bytes(&s,true)==bytes);
    CHECK(format_history(&s,true) && record_at(&s,4,2,&scope,ids[1],1).color==3 && record_at(&s,4,2,&scope,ids[1],1).size==16 && format_valid(&s,&error));
    format_free(&s); clear_error(&error);
}
static void record_budgets(void) {
    FormatStore s={0},empty={0}; FormatSnapshot pending={0}; CellFormat v=format_default(); char *error=NULL;
    const char pending_definition[]="uncommitted"; FormatScopeView pending_scope={99,pending_definition,sizeof(pending_definition)-1};
    uint64_t pending_id=UINT64_MAX; size_t pending_field=16383;
    CHECK(format_plan_records(&empty,&pending_scope,&pending_id,1,&pending_field,1,FORMAT_BOLD,v,&pending,&error));
    CHECK(pending.source_aware && !empty.source_aware);
    unsigned char owned[sizeof(pending)],live[sizeof(s)]; memcpy(owned,&pending,sizeof(pending));
    char *definition=dx_alloc(DX_FORMAT_SCOPE_BYTES); memset(definition,'t',DX_FORMAT_SCOPE_BYTES);
    uint64_t *ids=dx_alloc(DX_FORMAT_RECORDS*sizeof(*ids)); size_t *fields=dx_alloc(DX_FORMAT_FIELDS*sizeof(*fields));
    for (size_t i=0;i<DX_FORMAT_RECORDS;i++) ids[i]=(uint64_t)i+1;
    for (size_t i=0;i<DX_FORMAT_FIELDS;i++) fields[i]=i;
    FormatScopeView scope={1,definition,DX_FORMAT_SCOPE_BYTES-1}; v.flags=FORMAT_BOLD;
    CHECK(format_apply_records(&s,&scope,ids,DX_FORMAT_RECORDS,fields,DX_FORMAT_FIELDS,FORMAT_BOLD,v,&error));
    CHECK(format_valid(&s,&error) && record_at(&s,1,1,&scope,DX_FORMAT_RECORDS,16383).flags==FORMAT_BOLD);
    CHECK(format_owned_bytes(&s,false)==sizeof(FormatPatch)+sizeof(FormatScope)+sizeof(size_t)+DX_FORMAT_RECORDS*sizeof(uint64_t)+DX_FORMAT_FIELDS*sizeof(size_t)+DX_FORMAT_SCOPE_BYTES);
    format_free(&s);
    /* Fill the exact current byte budget with owned memberships included. */
    size_t overhead=4*(sizeof(FormatPatch)+sizeof(FormatScope)+sizeof(size_t)+sizeof(uint64_t)+sizeof(size_t)+1);
    size_t text_bytes=DX_FORMAT_CURRENT_BYTES-overhead; FormatScopeView scopes[4];
    for (size_t i=0;i<4;i++) { scopes[i]=(FormatScopeView){i+1,definition,text_bytes/4+(i<text_bytes%4)}; CHECK(format_apply_records(&s,&scopes[i],ids,1,fields,1,FORMAT_BOLD,v,&error)); }
    CHECK(format_owned_bytes(&s,false)==DX_FORMAT_CURRENT_BYTES && format_valid(&s,&error));
    format_trim_history(&s,false,0);
    uint64_t revision=s.revision; size_t bytes=format_owned_bytes(&s,true);
    memcpy(live,&s,sizeof(s));
    CHECK(!format_plan_records(&s,&scopes[3],ids+1,1,fields,1,FORMAT_BOLD,v,&pending,&error) && !memcmp(live,&s,sizeof(s)) && !memcmp(owned,&pending,sizeof(pending)) && format_owned_bytes(&s,true)==bytes); clear_error(&error);
    CHECK(!format_apply_records(&s,&scopes[3],ids+1,1,fields,1,FORMAT_BOLD,v,&error) && s.revision==revision && format_owned_bytes(&s,true)==bytes && s.count==4 && !s.undo_count); clear_error(&error);
    for (size_t i=0;i<31;i++) { v.flags=(unsigned char)(i%2 ? FORMAT_BOLD : 0); CHECK(format_apply_records(&s,&scopes[3],ids,1,fields,1,FORMAT_BOLD,v,&error)); }
    CHECK(s.undo_count==31 && format_owned_bytes(&s,true)==DX_FORMAT_HISTORY_BYTES && format_valid(&s,&error));
    revision=s.revision; bytes=format_owned_bytes(&s,true); v.flags=FORMAT_BOLD;
    memcpy(live,&s,sizeof(s));
    CHECK(!format_plan_records(&s,&scopes[3],ids,1,fields,1,FORMAT_BOLD,v,&pending,&error) && !memcmp(live,&s,sizeof(s)) && !memcmp(owned,&pending,sizeof(pending)) && format_owned_bytes(&s,true)==bytes && strstr(error,"history")); clear_error(&error);
    CHECK(!format_apply_records(&s,&scopes[3],ids,1,fields,1,FORMAT_BOLD,v,&error) && s.revision==revision && s.undo_count==31 && format_owned_bytes(&s,true)==bytes && strstr(error,"history")); clear_error(&error);
    CHECK(format_history(&s,false) && s.redo_count==1 && format_owned_bytes(&s,true)==bytes);
    revision=s.revision; size_t undo=s.undo_count;
    CHECK(!format_apply_records(&s,&scopes[3],ids+1,1,fields,1,FORMAT_BOLD,v,&error) && s.revision==revision && s.undo_count==undo && s.redo_count==1 && format_owned_bytes(&s,true)==bytes); clear_error(&error);
    CHECK(format_history(&s,true) && format_valid(&s,&error)); format_free(&s);
    scope.bytes=1; v.flags=FORMAT_BOLD;
    for (size_t i=0;i<DX_FORMAT_PATCHES;i++) CHECK(format_apply_records(&s,&scope,ids+i,1,fields,1,FORMAT_BOLD,v,&error));
    revision=s.revision; bytes=format_owned_bytes(&s,true); undo=s.undo_count;
    memcpy(live,&s,sizeof(s));
    CHECK(!format_plan_records(&s,&scope,ids+DX_FORMAT_PATCHES,1,fields,1,FORMAT_BOLD,v,&pending,&error) && !memcmp(live,&s,sizeof(s)) && !memcmp(owned,&pending,sizeof(pending)) && format_owned_bytes(&s,true)==bytes); clear_error(&error);
    CHECK(s.count==DX_FORMAT_PATCHES && !format_apply_records(&s,&scope,ids+DX_FORMAT_PATCHES,1,fields,1,FORMAT_BOLD,v,&error) && s.revision==revision && s.undo_count==undo && format_owned_bytes(&s,true)==bytes); clear_error(&error);
    v.flags=0; CHECK(format_apply_records(&s,&scope,ids,DX_FORMAT_PATCHES,fields,1,FORMAT_BOLD,v,&error) && s.count==1 && format_valid(&s,&error));
    CHECK(!record_at(&s,1,1,&scope,DX_FORMAT_PATCHES,0).flags && format_history(&s,false) && s.count==DX_FORMAT_PATCHES && record_at(&s,1,1,&scope,DX_FORMAT_PATCHES,0).flags==FORMAT_BOLD);
    CHECK(format_history(&s,true) && s.count==1); format_free(&s); format_snapshot_free(&pending); free(ids); free(fields); free(definition); clear_error(&error);
}
static void record_precedence(void) {
    /* The oracle addresses permuted display positions and original fields
       independently. No patch order or compaction detail is reused here. */
    const unsigned char first[]={'q',0,'1'},second[]={'q',0,'2'};
    FormatScopeView scopes[]={{1,first,sizeof(first)},{1,second,sizeof(second)},{2,first,sizeof(first)}};
    const uint64_t displayed_ids[]={8,2,5,1,6,4,3,7}; const size_t displayed_fields[]={3,0,2,1};
    uint64_t ids[]={1,2,3,4,5,6,7,8}; size_t fields[]={0,1,2,3};
    unsigned char expected[4][8][4]={0}; FormatStore s={0}; char *error=NULL; uint32_t random=UINT32_C(391726);
    for (size_t command=0;command<120;command++) {
        unsigned mode=random_step(&random)%5,owner=random_step(&random)%3,mask=1+random_step(&random)%7;
        uint64_t row=random_step(&random)%8,rows=1+random_step(&random)%(8-row);
        size_t col=random_step(&random)%4,columns=1+random_step(&random)%(4-col);
        FormatRange range={row,row+rows-1,col,col+columns-1}; CellFormat value=format_default(); value.flags=(unsigned char)(random_step(&random)%8);
        if (mode==1) { range.r0=0; range.r1=DX_SHEET_ROWS-1; }
        CHECK(mode==4 ? format_apply_records(&s,&scopes[owner],ids+row,(size_t)rows,fields+col,columns,mask,value,&error) :
            mode<2 ? format_apply(&s,range,mask,value,&error) : format_apply_scoped(&s,range,mask,value,&scopes[owner],1,mode==3,&error));
        for (size_t context=0;context<4;context++) for (size_t r=0;r<8;r++) for (size_t c=0;c<4;c++) {
            bool eligible=context==0 ? mode<2 || mode==3 : mode<2 ? mode==1 : context==owner+1;
            bool member=mode==4 ? displayed_ids[r]>=ids[row] && displayed_ids[r]<=ids[row+rows-1] && displayed_fields[c]>=col && displayed_fields[c]<col+columns :
                r>=range.r0 && r<=range.r1 && c>=range.c0 && c<=range.c1;
            if (eligible && member) expected[context][r][c]=(unsigned char)((expected[context][r][c]&~mask)|(value.flags&mask));
            CHECK(record_at(&s,r,c,context ? &scopes[context-1] : NULL,displayed_ids[r],displayed_fields[c]).flags==expected[context][r][c]);
        }
        CHECK(format_valid(&s,&error));
    }
    format_free(&s); clear_error(&error);
}
static FormatSnapshot baseline_current(const FormatStore *s) {
    return (FormatSnapshot){.items=s->items,.count=s->count,.scopes=s->scopes,.scope_count=s->scope_count,.source_aware=s->source_aware};
}
static FormatStore baseline_view(FormatSnapshot s) {
    return (FormatStore){.items=s.items,.count=s.count,.scopes=s.scopes,.scope_count=s.scope_count,.source_aware=s.source_aware};
}
static void baseline_compare(FormatSnapshot before,FormatSnapshot after,const FormatScopeView *scopes,size_t count) {
    CHECK(after.source_aware && before.count==after.count);
    CHECK(!before.count || before.items!=after.items);
    CHECK(after.scope_count>=before.scope_count && (!before.scope_count || before.scopes!=after.scopes));
    for (size_t i=0;i<before.scope_count;i++) {
        CHECK(after.scopes[i].owner==before.scopes[i].owner && after.scopes[i].bytes==before.scopes[i].bytes);
        CHECK(after.scopes[i].definition!=before.scopes[i].definition && !memcmp(after.scopes[i].definition,before.scopes[i].definition,before.scopes[i].bytes));
    }
    for (size_t i=0;i<before.count;i++) {
        FormatPatch a=before.items[i],b=after.items[i]; bool migrated=!before.source_aware && (a.range.r0 || a.range.r1!=DX_SHEET_ROWS-1);
        CHECK(!memcmp(&a.range,&b.range,sizeof(a.range)) && a.mask==b.mask && !memcmp(&a.value,&b.value,sizeof(a.value)));
        CHECK(b.selector==(migrated ? FORMAT_GUARDED : a.selector) && b.guard_count==(migrated ? count : a.guard_count));
        CHECK(!b.guard_count || b.guard_ids!=a.guard_ids);
        CHECK(b.record_count==a.record_count && b.field_count==a.field_count);
        if (a.record_count) CHECK(a.record_ids!=b.record_ids && !memcmp(a.record_ids,b.record_ids,a.record_count*sizeof(uint64_t)));
        if (a.field_count) CHECK(a.fields!=b.fields && !memcmp(a.fields,b.fields,a.field_count*sizeof(size_t)));
        if (!migrated && a.guard_count) CHECK(!memcmp(a.guard_ids,b.guard_ids,a.guard_count*sizeof(size_t)));
    }
    FormatStore old=baseline_view(before),next=baseline_view(after); CHECK(format_valid(&next,NULL));
    for (uint64_t row=0;row<8;row++) for (size_t col=0;col<4;col++) {
        CellFormat manual=format_at(&old,row,col),now=format_at(&next,row,col); CHECK(!memcmp(&manual,&now,sizeof(manual)));
        for (size_t i=0;i<count;i++) {
            CellFormat expected=before.source_aware ? format_resolve(&old,row,col,&scopes[i]) : manual;
            now=format_resolve(&next,row,col,&scopes[i]); CHECK(!memcmp(&expected,&now,sizeof(now)));
        }
    }
}
static void guarded_store_baseline(void) {
    /* Sentinel result bytes exercise ownership only. The future App caller must
       supply complete verified canonical RESULT packets, not these fixtures. */
    unsigned char definition[]={'r',0,'1'},other[]={'r',0,'2'};
    FormatScopeView scopes[]={{UINT64_MAX,definition,sizeof(definition)},{0,other,sizeof(other)}};
    FormatStore s={0},plan={0}; CellFormat v=format_default(); char *error=NULL; bool changed=false;
    v.color=2; CHECK(format_apply(&s,(FormatRange){0,DX_SHEET_ROWS-1,0,3},FORMAT_COLOR,v,&error));
    v.flags=FORMAT_BOLD; CHECK(format_apply(&s,(FormatRange){1,6,0,2},FORMAT_BOLD,v,&error));
    v.flags=FORMAT_ITALIC; CHECK(format_apply(&s,(FormatRange){2,5,1,3},FORMAT_ITALIC,v,&error));
    v.size=16; CHECK(format_apply_scoped(&s,(FormatRange){1,3,1,2},FORMAT_SIZE,v,&scopes[0],1,false,&error));
    v.decimals=4; CHECK(format_apply(&s,(FormatRange){3,4,1,1},FORMAT_DECIMALS,v,&error));
    uint64_t records[]={1,UINT64_MAX}; size_t fields[]={0,16383}; v.flags=FORMAT_UNDERLINE;
    CHECK(format_apply_records(&s,&scopes[1],records,2,fields,2,FORMAT_UNDERLINE,v,&error));
    CHECK(format_history(&s,false) && format_history(&s,false) && format_history(&s,false));
    CHECK(!s.source_aware && s.undo_count==3 && s.redo_count==3 && s.redo[0].source_aware && s.redo[1].source_aware);
    unsigned char live[sizeof(s)]; memcpy(live,&s,sizeof(s)); size_t bytes=format_owned_bytes(&s,true); uint64_t revision=s.revision;
    CHECK(format_plan_guard_store(&s,scopes,2,&plan,&changed,&error) && changed);
    CHECK(!memcmp(live,&s,sizeof(s)) && format_owned_bytes(&s,true)==bytes && plan.revision==revision);
    CHECK(plan.undo_count==s.undo_count && plan.redo_count==s.redo_count);
    baseline_compare(baseline_current(&s),baseline_current(&plan),scopes,2);
    for (size_t i=0;i<s.undo_count;i++) baseline_compare(s.undo[i],plan.undo[i],scopes,2);
    for (size_t i=0;i<s.redo_count;i++) baseline_compare(s.redo[i],plan.redo[i],scopes,2);
    CHECK(plan.scopes[0].definition!=plan.undo[2].scopes[0].definition && plan.items[1].guard_ids!=plan.undo[2].items[1].guard_ids);
    ((unsigned char *)plan.undo[2].scopes[0].definition)[0]='X';
    CHECK(((unsigned char *)plan.scopes[0].definition)[0]=='r' && !memcmp(live,&s,sizeof(s)));
    ((unsigned char *)plan.undo[2].scopes[0].definition)[0]='r';
    /* Existing aware finite-manual state stays manual, and existing record
       selectors retain their independent memberships and definition bytes. */
    CHECK(plan.redo[1].items[plan.redo[1].count-1].selector==FORMAT_SPATIAL);
    CHECK(plan.redo[0].items[plan.redo[0].count-1].selector==FORMAT_RECORDS);
    definition[2]='X'; CHECK(!memcmp(plan.scopes[0].definition,"r\0" "1",3)); definition[2]='1';
    format_free(&plan); CHECK(!memcmp(live,&s,sizeof(s)) && format_owned_bytes(&s,true)==bytes);
    changed=false; CHECK(format_plan_guard_store(&s,scopes,2,&plan,&changed,&error) && changed);
    FormatPatch *owned=plan.items,*undo=plan.undo[2].items; FormatScope *owned_scopes=plan.scopes;
    format_adopt_guard_store(&s,&plan);
    CHECK(s.items==owned && s.scopes==owned_scopes && s.undo[2].items==undo && s.revision==revision+1);
    CHECK(s.undo_count==3 && s.redo_count==3 && !plan.items && !plan.scopes && !plan.undo_count && !plan.redo_count && !plan.revision);
    CHECK(format_resolve(&s,2,1,&scopes[0]).flags==(FORMAT_BOLD|FORMAT_ITALIC));
    FormatScopeView wrong=scopes[0]; wrong.definition=other;
    CHECK(!format_resolve(&s,2,1,&wrong).flags && format_resolve(&s,2,1,&wrong).color==2);
    while (s.undo_count) { CHECK(format_history(&s,false) && s.source_aware); }
    while (s.redo_count) { CHECK(format_history(&s,true) && s.source_aware); }
    CHECK(s.items[s.count-1].selector==FORMAT_RECORDS && s.items[s.count-1].record_ids[1]==UINT64_MAX);
    /* No-op never replaces an existing owned plan or clears a live redo, and
       resets a prior true output so a caller cannot adopt a stale plan. */
    CHECK(format_history(&s,false)); v.flags=FORMAT_BOLD;
    CHECK(format_apply(&plan,(FormatRange){0,0,0,0},FORMAT_BOLD,v,&error));
    unsigned char prior[sizeof(plan)]; memcpy(prior,&plan,sizeof(plan)); memcpy(live,&s,sizeof(s));
    changed=false; CHECK(format_plan_guard_store(&s,scopes,2,&plan,&changed,&error) && !changed && !memcmp(prior,&plan,sizeof(plan)) && !memcmp(live,&s,sizeof(s)));
    changed=true; CHECK(format_plan_guard_store(&s,scopes,2,&plan,&changed,&error) && !changed && !memcmp(prior,&plan,sizeof(plan)));
    s.revision=UINT64_MAX; CHECK(format_plan_guard_store(&s,scopes,2,&plan,&changed,&error) && !changed && !memcmp(prior,&plan,sizeof(plan)));
    format_free(&s); format_free(&plan);
    changed=false; CHECK(format_plan_guard_store(&s,scopes,2,&plan,&changed,&error) && changed && plan.source_aware && !plan.count && !plan.scope_count && !plan.undo_count);
    format_adopt_guard_store(&s,&plan); CHECK(s.source_aware && s.revision==1 && !s.undo_count && !s.redo_count);
    format_free(&s); clear_error(&error);
}
static void guarded_store_invalid(void) {
    FormatStore s={0},plan={0}; CellFormat v=format_default(); char *error=NULL; bool changed=false;
    unsigned char bytes[]={'r',0,'1'}; FormatScopeView scope={0,bytes,sizeof(bytes)},bad=scope;
    v.flags=FORMAT_BOLD; CHECK(format_apply(&s,(FormatRange){1,1,0,0},FORMAT_BOLD,v,&error));
    CHECK(format_apply(&s,(FormatRange){2,2,0,0},FORMAT_ITALIC,v,&error));
    CHECK(format_apply(&plan,(FormatRange){4,4,0,0},FORMAT_BOLD,v,&error));
    unsigned char live[sizeof(s)],owned[sizeof(plan)]; memcpy(live,&s,sizeof(s)); memcpy(owned,&plan,sizeof(plan));
#define BAD_BASELINE(views_,count_) do { \
    CHECK(!format_plan_guard_store(&s,(views_),(count_),&plan,&changed,&error)); \
    CHECK(!changed && !memcmp(owned,&plan,sizeof(plan))); clear_error(&error); \
} while (0)
    BAD_BASELINE(NULL,1); BAD_BASELINE(&scope,0); BAD_BASELINE(&scope,DX_FORMAT_GUARDS+1);
    bad.bytes=0; BAD_BASELINE(&bad,1); bad.bytes=DX_FORMAT_SCOPE_BYTES; BAD_BASELINE(&bad,1);
    bad=scope; bad.definition=NULL; BAD_BASELINE(&bad,1);
    FormatScopeView duplicate[]={scope,scope}; BAD_BASELINE(duplicate,2);
    CHECK(!format_plan_guard_store(NULL,&scope,1,&plan,&changed,&error) && !changed); clear_error(&error);
    CHECK(!format_plan_guard_store(&s,&scope,1,NULL,&changed,&error) && !changed); clear_error(&error);
    CHECK(!format_plan_guard_store(&s,&scope,1,&plan,NULL,&error)); clear_error(&error);
    CHECK(!format_plan_guard_store(&s,&scope,1,&s,&changed,&error) && !changed && !memcmp(live,&s,sizeof(s))); clear_error(&error);
    s.undo_count=SIZE_MAX; BAD_BASELINE(&scope,1); s.undo_count=2;
    s.redo_count=DX_FORMAT_HISTORY+1; BAD_BASELINE(&scope,1); s.redo_count=0;
    s.count=DX_FORMAT_PATCHES+1; BAD_BASELINE(&scope,1); s.count=2;
    s.undo[1].items[0].mask=0; BAD_BASELINE(&scope,1); s.undo[1].items[0].mask=FORMAT_BOLD;
    s.undo[1].items[0].selector=FORMAT_RESULT; BAD_BASELINE(&scope,1); s.undo[1].items[0].selector=FORMAT_SPATIAL;
    s.undo[1].items[0].range.c1=16384; BAD_BASELINE(&scope,1); s.undo[1].items[0].range.c1=0;
    s.revision=UINT64_MAX; BAD_BASELINE(&scope,1); s.revision=2;
    CHECK(!memcmp(live,&s,sizeof(s)) && format_valid(&s,&error));
    /* A bad Redo snapshot must not evade whole-store semantic validation. */
    CHECK(format_history(&s,false) && s.redo_count==1); s.redo[0].items[0].value.size=0;
    BAD_BASELINE(&scope,1); s.redo[0].items[0].value.size=13;
    changed=true; bad=scope; bad.bytes=0;
    CHECK(!format_plan_guard_store(&s,&bad,1,&plan,&changed,&error) && changed && !memcmp(owned,&plan,sizeof(plan))); clear_error(&error);
#undef BAD_BASELINE
    /* Borrowed plan input survives replacement of that same owned output. */
    changed=false; CHECK(format_plan_guard_store(&s,&scope,1,&plan,&changed,&error) && changed);
    FormatScopeView borrowed={plan.scopes[0].owner,plan.scopes[0].definition,plan.scopes[0].bytes}; void *prior=plan.scopes[0].definition;
    changed=false; CHECK(format_plan_guard_store(&s,&borrowed,1,&plan,&changed,&error) && changed && plan.scopes[0].definition!=prior && !memcmp(plan.scopes[0].definition,bytes,sizeof(bytes)));
    format_free(&plan); format_free(&s); clear_error(&error);
}
static FormatSnapshot baseline_sized(size_t target,const FormatScopeView *scopes,size_t count) {
    FormatSnapshot state={.count=1,.scope_count=count}; state.items=dx_alloc(sizeof(*state.items));
    state.items[0]=(FormatPatch){.range={2,4,1,2},.mask=FORMAT_BOLD,.value=format_default()}; state.items[0].value.flags=FORMAT_BOLD;
    state.scopes=dx_alloc(count*sizeof(*state.scopes));
    for (size_t i=0;i<count;i++) {
        state.scopes[i]=(FormatScope){scopes[i].owner,dx_alloc(scopes[i].bytes+2),scopes[i].bytes};
        memcpy(state.scopes[i].definition,scopes[i].definition,scopes[i].bytes);
        memset((unsigned char *)state.scopes[i].definition+scopes[i].bytes,'r',2);
    }
    FormatStore view=baseline_view(state); CHECK(format_owned_bytes(&view,false)==target && format_valid(&view,NULL)); return state;
}
static void guarded_store_limits(void) {
    char *definition=dx_alloc(DX_FORMAT_SCOPE_BYTES); memset(definition,'r',DX_FORMAT_SCOPE_BYTES);
    FormatScopeView scopes[DX_FORMAT_GUARDS]; FormatStore s={0},plan={0}; char *error=NULL; bool changed=false;
    /* Exact current-state boundary, including reused scopes and new vectors. */
    size_t base=DX_FORMAT_CURRENT_BYTES-4*sizeof(size_t),text=base-sizeof(FormatPatch)-4*sizeof(FormatScope)-4;
    for (size_t i=0;i<4;i++) scopes[i]=(FormatScopeView){i,definition,text/4+(i<text%4)};
    FormatSnapshot state=baseline_sized(base,scopes,4); s=baseline_view(state);
    FormatScopeView reversed[]={scopes[3],scopes[2],scopes[1],scopes[0]};
    CHECK(format_plan_guard_store(&s,reversed,4,&plan,&changed,&error) && changed && format_owned_bytes(&plan,false)==DX_FORMAT_CURRENT_BYTES);
    CHECK(plan.scope_count==4 && plan.items[0].guard_ids[0]==0 && plan.items[0].guard_ids[3]==3 && plan.scopes[0].owner==0);
    format_free(&plan); s.scopes[0].bytes++; scopes[0].bytes++;
    unsigned char live[sizeof(s)]; memcpy(live,&s,sizeof(s)); changed=false;
    CHECK(!format_plan_guard_store(&s,scopes,4,&plan,&changed,&error) && !changed && !plan.items && !memcmp(live,&s,sizeof(s)) && strstr(error,"1 MiB")); clear_error(&error);
    format_free(&s);
    /* Both history stacks remain intact at the exact retained cap. Additional
       guard storage cannot silently evict any oldest/redo state to make room. */
    size_t states=2*DX_FORMAT_HISTORY,per=DX_FORMAT_HISTORY_BYTES/states; base=per-2*sizeof(size_t); text=base-sizeof(FormatPatch)-2*sizeof(FormatScope)-2;
    for (size_t i=0;i<2;i++) scopes[i]=(FormatScopeView){i,definition,text/2+(i<text%2)};
    state=baseline_sized(base,scopes,2); s=baseline_view(state); s.undo_count=DX_FORMAT_HISTORY; s.redo_count=DX_FORMAT_HISTORY-1;
    for (size_t i=0;i<s.undo_count;i++) s.undo[i]=baseline_sized(base,scopes,2);
    for (size_t i=0;i<s.redo_count;i++) s.redo[i]=baseline_sized(base,scopes,2);
    CHECK(format_valid(&s,&error)); changed=false;
    CHECK(format_plan_guard_store(&s,scopes,2,&plan,&changed,&error) && changed && format_owned_bytes(&plan,true)==DX_FORMAT_HISTORY_BYTES);
    CHECK(plan.undo_count==DX_FORMAT_HISTORY && plan.redo_count==DX_FORMAT_HISTORY-1 && plan.undo[0].items!=s.undo[0].items && plan.redo[DX_FORMAT_HISTORY-2].source_aware);
    format_free(&plan); s.undo[0].scopes[0].bytes++; memcpy(live,&s,sizeof(s)); changed=false;
    CHECK(format_valid(&s,&error) && !format_plan_guard_store(&s,scopes,2,&plan,&changed,&error) && !changed && !plan.items && !memcmp(live,&s,sizeof(s)) && strstr(error,"history")); clear_error(&error);
    format_free(&s);
    /* Maximum patch/owner/history counts; no compaction, lost mask or synthetic
       history entry is permitted even when many snapshots are empty. */
    for (size_t i=0;i<DX_FORMAT_GUARDS;i++) scopes[i]=(FormatScopeView){i,definition,1};
    s.count=DX_FORMAT_PATCHES; s.items=dx_alloc(s.count*sizeof(*s.items)); s.undo_count=s.redo_count=DX_FORMAT_HISTORY;
    for (size_t i=0;i<s.count;i++) s.items[i]=(FormatPatch){.range={i,i,0,0},.mask=FORMAT_BOLD,.value=format_default()};
    changed=false; CHECK(format_plan_guard_store(&s,scopes,DX_FORMAT_GUARDS,&plan,&changed,&error) && changed && plan.count==DX_FORMAT_PATCHES && plan.scope_count==DX_FORMAT_GUARDS && plan.undo_count==DX_FORMAT_HISTORY && plan.redo_count==DX_FORMAT_HISTORY);
    for (size_t i=0;i<plan.count;i++) CHECK(plan.items[i].range.r0==i && plan.items[i].guard_count==DX_FORMAT_GUARDS && plan.items[i].guard_ids[DX_FORMAT_GUARDS-1]==DX_FORMAT_GUARDS-1);
    for (size_t i=0;i<DX_FORMAT_HISTORY;i++) CHECK(plan.undo[i].source_aware && plan.redo[i].source_aware && !plan.undo[i].count && !plan.redo[i].count);
    format_free(&plan); format_free(&s);
    /* Unused legacy scopes are still owned model state. Guarding cannot append
       beyond the scope count cap even when its byte reservation would fit. */
    s.count=1; s.items=dx_alloc(sizeof(*s.items)); s.items[0]=(FormatPatch){.range={0,1,0,0},.mask=FORMAT_BOLD,.value=format_default()};
    s.scope_count=DX_FORMAT_PATCHES*DX_FORMAT_GUARDS; s.scopes=dx_alloc(s.scope_count*sizeof(*s.scopes));
    for (size_t i=0;i<s.scope_count;i++) s.scopes[i]=(FormatScope){i,dx_strdup("r"),1};
    scopes[0]=(FormatScopeView){UINT64_MAX,definition,1}; changed=false; memcpy(live,&s,sizeof(s));
    CHECK(!format_plan_guard_store(&s,scopes,1,&plan,&changed,&error) && !changed && !plan.items && !memcmp(live,&s,sizeof(s)) && strstr(error,"scope limit"));
    format_free(&s); free(definition); clear_error(&error);
}
static void scope_duplicate_validation(void) {
    const size_t counts[]={DX_FORMAT_GUARDS,DX_FORMAT_GUARDS+1,1000,4000,DX_FORMAT_PATCHES*DX_FORMAT_GUARDS};
    const uint64_t owners[]={0,UINT64_MAX,UINT64_C(9007199254740993),17};
    for (size_t which=0;which<sizeof(counts)/sizeof(*counts);which++) {
        size_t count=counts[which]; FormatStore s={.scope_count=count,.source_aware=true}; char *error=NULL;
        s.scopes=dx_alloc(count*sizeof(*s.scopes)); FormatScope *prior=dx_alloc(count*sizeof(*prior));
        for (size_t i=0;i<count;i++) {
            size_t bytes=8+i%8; unsigned char *data=dx_alloc(bytes+1); memset(data,'x',bytes+1);
            data[1]=0; data[bytes-2]=(unsigned char)i; data[bytes-1]=(unsigned char)(i>>8);
            s.scopes[i]=(FormatScope){owners[(i/8)%4],data,bytes};
        }
        CHECK(format_owned_bytes(&s,false)<=DX_FORMAT_CURRENT_BYTES);
        for (size_t reversed=0;reversed<2;reversed++) {
            if (reversed) for (size_t i=0;i<count/2;i++) { FormatScope swap=s.scopes[i]; s.scopes[i]=s.scopes[count-1-i]; s.scopes[count-1-i]=swap; }
            memcpy(prior,s.scopes,count*sizeof(*prior)); CHECK(format_valid(&s,&error) && !memcmp(prior,s.scopes,count*sizeof(*prior)));
            const size_t pairs[][2]={{0,1},{0,count-1},{count/2,count/2+1},{count-2,count-1},{5,count/2},{1,count-2}};
            for (size_t pair=0;pair<sizeof(pairs)/sizeof(*pairs);pair++) {
                size_t a=pairs[pair][0],b=pairs[pair][1]; FormatScope saved=s.scopes[b]; s.scopes[b]=s.scopes[a];
                CHECK(!format_valid(&s,&error) && error && !strcmp(error,"Duplicate cell format result definition.")); clear_error(&error);
                CHECK(s.scopes[b].definition==s.scopes[a].definition && s.scopes[b].owner==s.scopes[a].owner);
                s.scopes[b]=saved; CHECK(!memcmp(prior,s.scopes,count*sizeof(*prior)));
            }
            /* Equal text after an embedded zero remains exact binary data;
               owner or byte-length differences must not become duplicates. */
            FormatScope saved=s.scopes[1]; s.scopes[1]=s.scopes[0]; s.scopes[1].owner^=UINT64_C(1);
            CHECK(format_valid(&s,&error)); s.scopes[1]=s.scopes[0]; s.scopes[1].bytes=7;
            CHECK(format_valid(&s,&error)); s.scopes[1]=saved;
            CHECK(!memcmp(prior,s.scopes,count*sizeof(*prior)));
        }
        /* Validate every shape before a sort comparator can read it, even when
           an earlier pair would already be a duplicate. */
        FormatScope first=s.scopes[1],last=s.scopes[count-1]; s.scopes[1]=s.scopes[0]; s.scopes[count-1].definition=NULL;
        CHECK(!format_valid(&s,&error) && strstr(error,"256 KiB")); clear_error(&error);
        s.scopes[1]=first; s.scopes[count-1]=last;
        size_t bytes=s.scopes[count/2].bytes; s.scopes[count/2].bytes=0;
        CHECK(!format_valid(&s,&error) && strstr(error,"256 KiB")); clear_error(&error); s.scopes[count/2].bytes=DX_FORMAT_SCOPE_BYTES;
        CHECK(!format_valid(&s,&error) && strstr(error,"256 KiB")); clear_error(&error); s.scopes[count/2].bytes=bytes;
        FormatScope *array=s.scopes; s.scopes=NULL; CHECK(!format_valid(&s,&error)); clear_error(&error); s.scopes=array;
        s.scope_count=SIZE_MAX; CHECK(!format_valid(&s,&error)); clear_error(&error); s.scope_count=count;
        CHECK(format_valid(&s,&error) && !memcmp(prior,s.scopes,count*sizeof(*prior)));
        for (size_t i=0;i<count;i++) {
            /* Both original and reversed orders preserve their exact payload. */
            size_t original=count-1-i,bytes_expected=8+original%8; const unsigned char *data=s.scopes[i].definition;
            CHECK(s.scopes[i].owner==owners[(original/8)%4] && s.scopes[i].bytes==bytes_expected && data[1]==0);
            CHECK(data[bytes_expected-2]==(unsigned char)original && data[bytes_expected-1]==(unsigned char)(original>>8));
        }
        free(prior); format_free(&s); clear_error(&error);
    }
}
static void disjoint_property_noop(void) {
    FormatStore s={0}; FormatSnapshot plan={0}; char *error=NULL; CellFormat value=format_default();
    const char raw_bytes[]="raw-v1",result_bytes[]="result-v1",filtered_bytes[]="result-filtered";
    FormatScopeView raw={0,raw_bytes,sizeof(raw_bytes)-1},result={0,result_bytes,sizeof(result_bytes)-1},filtered={0,filtered_bytes,sizeof(filtered_bytes)-1};
    uint64_t id=11; size_t field=4; value.flags=FORMAT_BOLD;
    CHECK(format_apply_records(&s,&raw,&id,1,&field,1,FORMAT_BOLD,value,&error));
    value.flags=FORMAT_ITALIC; CHECK(format_apply_scoped(&s,(FormatRange){0,11,4,4},FORMAT_ITALIC,value,&result,1,false,&error));
    value.color=2; CHECK(format_apply_scoped(&s,(FormatRange){20,700,4,8},FORMAT_COLOR,value,&result,1,true,&error));
    value.flags=FORMAT_UNDERLINE; CHECK(format_apply_scoped(&s,(FormatRange){0,2,4,4},FORMAT_UNDERLINE,value,&filtered,1,false,&error));
    value.size=16; CHECK(format_apply(&s,(FormatRange){50,50,9,9},FORMAT_SIZE,value,&error) && format_history(&s,false));
    CHECK(s.count==4 && s.undo_count==4 && s.redo_count==1 && s.items[0].selector==FORMAT_RECORDS);
    unsigned char live[sizeof(s)]; memcpy(live,&s,sizeof(s)); size_t bytes=format_owned_bytes(&s,true);
    value.flags=FORMAT_BOLD;
    CHECK(format_plan_records(&s,&raw,&id,1,&field,1,FORMAT_BOLD,value,&plan,&error) && !format_commit(&s,&plan));
    CHECK(!memcmp(live,&s,sizeof(s)) && !plan.items && !plan.scopes && format_owned_bytes(&s,true)==bytes);
    CHECK(format_apply_records(&s,&raw,&id,1,&field,1,FORMAT_BOLD,value,&error) && !memcmp(live,&s,sizeof(s)));
    value.flags=FORMAT_ITALIC;
    CHECK(format_apply_scoped(&s,(FormatRange){0,11,4,4},FORMAT_ITALIC,value,&result,1,false,&error) && !memcmp(live,&s,sizeof(s)));
    CHECK(format_apply_scoped(&s,(FormatRange){20,700,4,8},FORMAT_COLOR,value,&result,1,true,&error) && !memcmp(live,&s,sizeof(s)));
    value.flags=FORMAT_UNDERLINE;
    CHECK(format_apply_scoped(&s,(FormatRange){0,2,4,4},FORMAT_UNDERLINE,value,&filtered,1,false,&error) && !memcmp(live,&s,sizeof(s)));
    CHECK(format_resolve_record_token(&s,10,4,format_scope_token(&s,&result),format_scope_token(&s,&raw),id,field).flags==(FORMAT_BOLD|FORMAT_ITALIC));
    CHECK(format_valid(&s,&error)); format_free(&s);
    /* A newer same-property result/default reset must block the shortcut, even
       when intervening patches change only unrelated properties. */
    for (size_t broad=0;broad<2;broad++) {
        value=format_default(); value.flags=FORMAT_BOLD; CHECK(format_apply_records(&s,&raw,&id,1,&field,1,FORMAT_BOLD,value,&error));
        value.flags=0;
        CHECK(broad ? format_apply(&s,(FormatRange){0,DX_SHEET_ROWS-1,4,4},FORMAT_BOLD,value,&error) :
            format_apply_scoped(&s,(FormatRange){0,20,4,4},FORMAT_BOLD,value,&result,1,false,&error));
        value.color=3; CHECK(format_apply_scoped(&s,(FormatRange){1,3,4,4},FORMAT_COLOR,value,&result,1,false,&error));
        size_t result_token=format_scope_token(&s,&result),raw_token=format_scope_token(&s,&raw);
        CHECK(!format_resolve_record_token(&s,10,4,result_token,raw_token,id,field).flags);
        uint64_t revision=s.revision; size_t undo=s.undo_count; value.flags=FORMAT_BOLD;
        CHECK(format_apply_records(&s,&raw,&id,1,&field,1,FORMAT_BOLD,value,&error) && s.revision==revision+1 && s.undo_count==undo+1);
        CHECK(format_resolve_record_token(&s,10,4,format_scope_token(&s,&result),format_scope_token(&s,&raw),id,field).flags==FORMAT_BOLD);
        CHECK(format_history(&s,false) && !format_resolve_record_token(&s,10,4,format_scope_token(&s,&result),format_scope_token(&s,&raw),id,field).flags);
        CHECK(format_history(&s,true) && format_resolve_record_token(&s,10,4,format_scope_token(&s,&result),format_scope_token(&s,&raw),id,field).flags==FORMAT_BOLD);
        format_free(&s);
    }
    /* Any differing membership, exact owner, value or newly requested mask
       remains a real action. The new shortcut does not reason across owners. */
    for (size_t variation=0;variation<4;variation++) {
        value=format_default(); value.flags=FORMAT_BOLD; CHECK(format_apply_records(&s,&raw,&id,1,&field,1,FORMAT_BOLD,value,&error));
        value.color=2; CHECK(format_apply_scoped(&s,(FormatRange){0,20,4,4},FORMAT_COLOR,value,&result,1,false,&error));
        uint64_t next_id=variation==0 ? id+1 : id; FormatScopeView next_raw=raw; unsigned mask=FORMAT_BOLD;
        if (variation==1) next_raw.owner=UINT64_MAX;
        if (variation==2) value.flags=0;
        if (variation==3) { mask|=FORMAT_UNDERLINE; value.flags|=FORMAT_UNDERLINE; }
        uint64_t revision=s.revision; size_t undo=s.undo_count;
        CHECK(format_apply_records(&s,&next_raw,&next_id,1,&field,1,mask,value,&error) && s.revision==revision+1 && s.undo_count==undo+1);
        CellFormat actual=record_at(&s,0,4,&next_raw,next_id,field);
        CHECK(actual.flags==(variation==2 ? 0 : variation==3 ? FORMAT_BOLD|FORMAT_UNDERLINE : FORMAT_BOLD));
        CHECK(format_valid(&s,&error)); format_free(&s);
    }
    clear_error(&error);
}
int main(void) {
    display("128450.00",2,true,"128,450.00"); display("-24860",2,true,"(24,860.00)"); display("-24860",0,false,"−24,860");
    display("9.995",2,true,"10.00"); display("-0.00005",4,true,"(0.0001)"); display("0.00000000001",4,false,"0.0000");
    display("99999999999999999999999999999999999999.99",0,true,"100,000,000,000,000,000,000,000,000,000,000,000,000");
    display("340282366920938463463374607431768211455",2,true,"340,282,366,920,938,463,463,374,607,431,768,211,455.00");
    display("-170141183460469231731687303715884105728",0,true,"(170,141,183,460,469,231,731,687,303,715,884,105,728)");
    display("1.25e-2",4,false,"0.0125"); display("2.675",2,true,"2.68"); display("1e3",1,true,"1,000.0");
    char text[512]; CHECK(!number_display("nan",2,true,text)); CHECK(!number_display("1e400",2,true,text)); CHECK(!number_display("1",5,true,text));
    FormatStore s={0},copy={0}; CellFormat value=format_default(); char *error=NULL;
    CHECK(format_at(&s,0,0).size==13 && !s.count); value.flags=FORMAT_BOLD;
    CHECK(format_apply(&s,(FormatRange){1,9,2,5},FORMAT_BOLD,value,&error)); CHECK(s.count==1 && s.undo_count==1 && format_at(&s,4,3).flags==FORMAT_BOLD && !format_at(&s,0,3).flags);
    uint64_t revision=s.revision; CHECK(format_apply(&s,(FormatRange){1,9,2,5},FORMAT_BOLD,value,&error) && s.revision==revision && s.undo_count==1);
    value.color=3; value.size=16; CHECK(format_apply(&s,(FormatRange){3,5,3,4},FORMAT_COLOR|FORMAT_SIZE,value,&error));
    CHECK(format_at(&s,4,3).flags==FORMAT_BOLD && format_at(&s,4,3).color==3 && format_at(&s,4,3).size==16 && !format_at(&s,1,3).color);
    value.flags=0; CHECK(format_apply(&s,(FormatRange){4,4,3,3},FORMAT_BOLD,value,&error)); CHECK(!format_at(&s,4,3).flags && format_at(&s,4,3).color==3);
    CHECK(format_history(&s,false) && format_at(&s,4,3).flags==FORMAT_BOLD); CHECK(format_history(&s,true) && !format_at(&s,4,3).flags);
    format_copy(&copy,&s); CHECK(copy.count==s.count && copy.items!=s.items && !copy.undo_count); format_copy(&copy,&copy); CHECK(copy.count==s.count);
    FormatSnapshot plan={0}; CHECK(format_plan_move(&s,(FormatRange){3,5,3,4},10,5,&plan,&error)); CHECK(format_commit(&s,&plan) && !plan.items);
    CHECK(format_at(&s,14,8).size==16 && format_at(&s,14,8).color==3 && !format_at(&s,14,8).flags);
    CHECK(format_at(&s,3,3).size==13 && !format_at(&s,3,3).flags && !format_at(&s,3,3).color && format_at(&s,3,2).flags==FORMAT_BOLD);
    CHECK(format_history(&s,false) && format_at(&s,4,3).color==3 && !format_at(&s,14,8).color);
    CHECK(format_plan_move(&s,(FormatRange){3,5,3,4},1,1,&plan,&error) && format_commit(&s,&plan));
    CHECK(format_at(&s,5,4).size==16 && !format_at(&s,5,4).flags && !format_at(&s,3,3).color);
    CHECK(format_plan_move(&s,(FormatRange){3,5,3,4},0,0,&plan,&error) && !format_commit(&s,&plan));
    CHECK(!format_plan_move(&s,(FormatRange){0,0,0,0},-1,0,&plan,&error)); free(error); error=NULL;
    CHECK(format_plan_axis(&s,true,4,2,true,&plan,&error) && format_commit(&s,&plan)); CHECK(format_at(&s,7,4).size==16 && format_at(&s,7,4).color==3);
    CHECK(format_plan_axis(&s,true,4,2,false,&plan,&error) && format_commit(&s,&plan)); CHECK(format_at(&s,5,4).size==16 && !format_at(&s,5,4).flags);
    CHECK(format_plan_axis(&s,false,4,1,true,&plan,&error) && format_commit(&s,&plan)); CHECK(format_at(&s,5,5).size==16);
    CHECK(format_plan_axis(&s,false,4,1,false,&plan,&error) && format_commit(&s,&plan)); CHECK(format_at(&s,5,4).size==16);
    CHECK(format_valid(&s,&error)); format_free(&s); format_free(&copy);
    for (size_t i=0;i<DX_FORMAT_PATCHES;i++) { value.flags=(unsigned char)(i%2 ? FORMAT_ITALIC : FORMAT_BOLD); CHECK(format_apply(&s,(FormatRange){i,i,0,0},FORMAT_BOLD|FORMAT_ITALIC,value,&error)); }
    CHECK(s.count==DX_FORMAT_PATCHES && s.undo_count==DX_FORMAT_HISTORY);
    revision=s.revision; CHECK(!format_apply(&s,(FormatRange){DX_FORMAT_PATCHES,DX_FORMAT_PATCHES,0,0},FORMAT_BOLD,value,&error) && s.revision==revision && s.count==DX_FORMAT_PATCHES); free(error); error=NULL;
    value.flags=0; value.number=FORMAT_ACCOUNTING; CHECK(format_apply(&s,(FormatRange){0,DX_SHEET_ROWS-1,0,16383},FORMAT_ALL,value,&error)); CHECK(s.count==1 && format_at(&s,DX_SHEET_ROWS-1,16383).number==FORMAT_ACCOUNTING);
    CHECK(format_plan_axis(&s,true,DX_SHEET_ROWS-1,1,true,&plan,&error) && !format_commit(&s,&plan));
    format_trim_history(&s,false,3); CHECK(s.undo_count==3); format_free(&s);
    scoped_resolution(); scoped_legacy(); scoped_budgets(); source_aware_lifetime(); scoped_precedence();
    record_resolution(); record_planners(); record_dual_scopes(); record_invalid(); record_edit_plan(); record_budgets(); record_precedence();
    guarded_store_baseline(); guarded_store_invalid(); guarded_store_limits();
    scope_duplicate_validation();
    disjoint_property_noop();
    printf("PASS: %u formatting checks (exact numeric display, bounded ranges, scoped record membership, precedence, ownership, moves, structural edits, history and whole-sheet selection)\n",checks); return 0;
}
