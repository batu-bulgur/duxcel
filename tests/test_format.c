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
    CHECK(s.count==1 && !s.scope_count && format_persistable(&s,&error) && format_valid(&s,&error));
    CHECK(format_history(&s,false) && !format_persistable(&s,&error)); clear_error(&error);
    CHECK(format_history(&s,true) && format_persistable(&s,&error));
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
    CHECK(format_plan_axis(&s,true,0,DX_SHEET_ROWS-1,false,&plan,&error) && format_commit(&s,&plan) && !s.scope_count && format_persistable(&s,&error));
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
static void record_budgets(void) {
    FormatStore s={0}; CellFormat v=format_default(); char *error=NULL;
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
    CHECK(!format_apply_records(&s,&scopes[3],ids+1,1,fields,1,FORMAT_BOLD,v,&error) && s.revision==revision && format_owned_bytes(&s,true)==bytes && s.count==4 && !s.undo_count); clear_error(&error);
    for (size_t i=0;i<31;i++) { v.flags=(unsigned char)(i%2 ? FORMAT_BOLD : 0); CHECK(format_apply_records(&s,&scopes[3],ids,1,fields,1,FORMAT_BOLD,v,&error)); }
    CHECK(s.undo_count==31 && format_owned_bytes(&s,true)==DX_FORMAT_HISTORY_BYTES && format_valid(&s,&error));
    revision=s.revision; bytes=format_owned_bytes(&s,true); v.flags=FORMAT_BOLD;
    CHECK(!format_apply_records(&s,&scopes[3],ids,1,fields,1,FORMAT_BOLD,v,&error) && s.revision==revision && s.undo_count==31 && format_owned_bytes(&s,true)==bytes && strstr(error,"history")); clear_error(&error);
    CHECK(format_history(&s,false) && s.redo_count==1 && format_owned_bytes(&s,true)==bytes);
    revision=s.revision; size_t undo=s.undo_count;
    CHECK(!format_apply_records(&s,&scopes[3],ids+1,1,fields,1,FORMAT_BOLD,v,&error) && s.revision==revision && s.undo_count==undo && s.redo_count==1 && format_owned_bytes(&s,true)==bytes); clear_error(&error);
    CHECK(format_history(&s,true) && format_valid(&s,&error)); format_free(&s);
    scope.bytes=1; v.flags=FORMAT_BOLD;
    for (size_t i=0;i<DX_FORMAT_PATCHES;i++) CHECK(format_apply_records(&s,&scope,ids+i,1,fields,1,FORMAT_BOLD,v,&error));
    revision=s.revision; bytes=format_owned_bytes(&s,true); undo=s.undo_count;
    CHECK(s.count==DX_FORMAT_PATCHES && !format_apply_records(&s,&scope,ids+DX_FORMAT_PATCHES,1,fields,1,FORMAT_BOLD,v,&error) && s.revision==revision && s.undo_count==undo && format_owned_bytes(&s,true)==bytes); clear_error(&error);
    v.flags=0; CHECK(format_apply_records(&s,&scope,ids,DX_FORMAT_PATCHES,fields,1,FORMAT_BOLD,v,&error) && s.count==1 && format_valid(&s,&error));
    CHECK(!record_at(&s,1,1,&scope,DX_FORMAT_PATCHES,0).flags && format_history(&s,false) && s.count==DX_FORMAT_PATCHES && record_at(&s,1,1,&scope,DX_FORMAT_PATCHES,0).flags==FORMAT_BOLD);
    CHECK(format_history(&s,true) && s.count==1); format_free(&s); free(ids); free(fields); free(definition); clear_error(&error);
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
    scoped_resolution(); scoped_legacy(); scoped_budgets(); scoped_precedence();
    record_resolution(); record_planners(); record_dual_scopes(); record_invalid(); record_budgets(); record_precedence();
    printf("PASS: %u formatting checks (exact numeric display, bounded ranges, scoped record membership, precedence, ownership, moves, structural edits, history and whole-sheet selection)\n",checks); return 0;
}
