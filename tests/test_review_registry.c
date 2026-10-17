#include "dev/review_registry.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL review_registry:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static DxReviewFrame frame(void) {
    DxReviewFrame f={.frame_id=UINT64_C(9007199254740993),.timestamp_ns=UINT64_MAX,.window_width=640,.window_height=640,.framebuffer_width=1280,.framebuffer_height=1280,.dpi_x=2,.dpi_y=2,.ui_scale=1.5f,.grid_zoom=.8f};
    strcpy(f.session_id,"session_1"); strcpy(f.project_id,"duxcel"); strcpy(f.document_id,"document_1");
    strcpy(f.source_fingerprint,"dirty-source-sha256"); strcpy(f.binary_sha256,"binary-sha256"); strcpy(f.build_flags,"C99 review ASan"); strcpy(f.assets_fingerprint,"font-icon-sha256");
    strcpy(f.fonts,"IBM Plex Sans, JetBrains Mono"); strcpy(f.state_text,"menu=filter; source=7; selected=F7:F10"); strcpy(f.trace_text,"command filter.open\nasync distinct.complete\n"); return f;
}
static DxReviewWidget widget(const char *id,int layer,DxReviewRect rect) {
    DxReviewWidget w; dx_review_widget_init(&w,id,"filter.open","popup/filter/status","button","Status filter"); w.layer=layer; w.bounds=rect; w.clip=(DxReviewRect){0,0,640,640};
    strcpy(w.source_file,"src/app/app.c"); w.source_line=123; return w;
}
static void spaces(const char **p) { while (**p==' ' || **p=='\t' || **p=='\n' || **p=='\r') (*p)++; }
static bool json_string(const char **p) {
    if (*(*p)++!='"') return false;
    while (**p && **p!='"') {
        unsigned char c=(unsigned char)*(*p)++; if (c<32) return false;
        if (c=='\\') {
            char e=*(*p)++; if (!e) return false;
            if (e=='u') { for (int i=0;i<4;i++) { char h=*(*p)++; if (!((h>='0' && h<='9') || (h>='a' && h<='f') || (h>='A' && h<='F'))) return false; } }
            else if (!strchr("\"\\/bfnrt",e)) return false;
        }
    }
    return *(*p)++=='"';
}
static bool json_value(const char **p,unsigned depth) {
    if (depth>32) return false; spaces(p);
    if (**p=='"') return json_string(p);
    if (**p=='{' || **p=='[') {
        char close=**p=='{' ? '}' : ']'; bool object=**p=='{'; (*p)++; spaces(p);
        if (**p==close) { (*p)++; return true; }
        for (;;) {
            if (object) { if (**p!='"' || !json_string(p)) return false; spaces(p); if (*(*p)++!=':') return false; }
            if (!json_value(p,depth+1)) return false; spaces(p);
            if (**p==close) { (*p)++; return true; }
            if (*(*p)++!=',') return false; spaces(p);
        }
    }
    const char *keywords[]={"true","false","null"};
    for (size_t i=0;i<3;i++) { size_t n=strlen(keywords[i]); if (!strncmp(*p,keywords[i],n)) { *p+=n; return true; } }
    if (**p=='-') (*p)++;
    if (**p=='0') (*p)++; else { if (**p<'1' || **p>'9') return false; do { (*p)++; } while (**p>='0' && **p<='9'); }
    if (**p=='.') { (*p)++; if (**p<'0' || **p>'9') return false; do { (*p)++; } while (**p>='0' && **p<='9'); }
    if (**p=='e' || **p=='E') { (*p)++; if (**p=='+' || **p=='-') (*p)++; if (**p<'0' || **p>'9') return false; do { (*p)++; } while (**p>='0' && **p<='9'); }
    return true;
}
static bool valid_json(const char *json) { const char *p=json; bool valid=json_value(&p,0); spaces(&p); return valid && !*p; }
static bool utf8(const char *text) {
    const unsigned char *p=(const unsigned char *)text;
    while (*p) {
        size_t n=*p<128 ? 0 : *p>=0xc2 && *p<=0xdf ? 1 : *p>=0xe0 && *p<=0xef ? 2 : *p>=0xf0 && *p<=0xf4 ? 3 : 9;
        if (n>3 || (*p==0xe0 && p[1]<0xa0) || (*p==0xed && p[1]>=0xa0) || (*p==0xf0 && p[1]<0x90) || (*p==0xf4 && p[1]>=0x90)) return false;
        p++; for (size_t i=0;i<n;i++,p++) if (!*p || (*p&0xc0)!=0x80) return false;
    }
    return true;
}
static void test_scope_and_lifetime(DxReviewRegistry *r) {
    CHECK(dx_review_begin(r,NULL,false)==DX_REVIEW_NOT_ARMED); CHECK(dx_review_add(r,NULL)==DX_REVIEW_NOT_ARMED); CHECK(dx_review_seal(r)==DX_REVIEW_NOT_ARMED);
    DxReviewFrame f=frame(); CHECK(dx_review_begin(r,&f,true)==DX_REVIEW_OK);
    DxReviewWidget a=widget("filter.condition",3,(DxReviewRect){10,20,100,28}); a.focused=true; a.checked=true; a.indeterminate=true; a.expanded=true;
    CHECK(dx_review_add(r,&a)==DX_REVIEW_OK);
    strcpy(a.path,"ribbon/home/data/filter"); strcpy(a.label,"changed"); CHECK(dx_review_add(r,&a)==DX_REVIEW_DUPLICATE);
    CHECK(r->count==1 && r->duplicate_count==1 && !strcmp(r->widgets[0].path,"popup/filter/status") && !strcmp(r->widgets[0].label,"Status filter"));
    strcpy(a.id,"ribbon.filter"); CHECK(dx_review_add(r,&a)==DX_REVIEW_OK); CHECK(!strcmp(r->widgets[0].action_id,r->widgets[1].action_id));
    memset(f.state_text,'x',strlen(f.state_text)); CHECK(strstr(r->frame.state_text,"menu=filter"));
    CHECK(r->widgets[0].focused && r->widgets[0].checked && r->widgets[0].indeterminate && r->widgets[0].expanded);
    CHECK(dx_review_find(r,"filter.condition")==&r->widgets[0]); CHECK(!dx_review_find(r,"missing")); CHECK(dx_review_seal(r)==DX_REVIEW_OK); CHECK(dx_review_seal(r)==DX_REVIEW_SEALED); CHECK(dx_review_add(r,&a)==DX_REVIEW_SEALED);
    DxReviewTarget t; CHECK(dx_review_pick(r,15,25,DX_REVIEW_PICK_ANY,&t)); CHECK(!strcmp(t.widget.id,"ribbon.filter"));
    CHECK(dx_review_begin(r,&f,true)==DX_REVIEW_OK); CHECK(!r->count && !r->duplicate_count && !r->sealed); CHECK(!strcmp(t.widget.id,"ribbon.filter") && !strcmp(t.widget.label,"changed"));
    CHECK(!dx_review_pick(r,15,25,DX_REVIEW_PICK_ANY,&t));
    CHECK(dx_review_begin(NULL,&f,true)==DX_REVIEW_INVALID);
}
static void test_geometry_and_layers(DxReviewRegistry *r) {
    DxReviewFrame f=frame(); CHECK(dx_review_begin(r,&f,true)==DX_REVIEW_OK);
    DxReviewWidget canvas=widget("canvas",0,(DxReviewRect){0,0,640,640}); canvas.container=true; CHECK(dx_review_add(r,&canvas)==DX_REVIEW_OK);
    DxReviewWidget behind=widget("ribbon.filter",1,(DxReviewRect){0,0,200,200}); CHECK(dx_review_add(r,&behind)==DX_REVIEW_OK);
    DxReviewWidget popup=widget("filter.popup",10,(DxReviewRect){50,50,200,200}); popup.container=true; strcpy(popup.parent_id,"canvas"); CHECK(dx_review_add(r,&popup)==DX_REVIEW_OK);
    DxReviewWidget disabled=widget("filter.disabled",10,(DxReviewRect){60,60,100,30}); disabled.enabled=false; disabled.input_eligible=false; strcpy(disabled.parent_id,"filter.popup"); CHECK(dx_review_add(r,&disabled)==DX_REVIEW_OK);
    DxReviewWidget clipped=widget("filter.values",11,(DxReviewRect){60,100,100,100}); clipped.clip=(DxReviewRect){70,110,50,40}; CHECK(dx_review_add(r,&clipped)==DX_REVIEW_OK);
    DxReviewWidget invisible=widget("filter.closed",100,(DxReviewRect){60,60,100,30}); invisible.clip=(DxReviewRect){0}; CHECK(dx_review_add(r,&invisible)==DX_REVIEW_OK);
    DxReviewWidget decoration=widget("filter.decoration",100,(DxReviewRect){0,0,640,640}); decoration.pickable=false; CHECK(dx_review_add(r,&decoration)==DX_REVIEW_OK);
    CHECK(dx_review_seal(r)==DX_REVIEW_OK); DxReviewTarget t,parent;
    CHECK(dx_review_pick(r,65,65,DX_REVIEW_PICK_CONTROL,&t) && !strcmp(t.widget.id,"filter.disabled") && !t.widget.enabled);
    CHECK(dx_review_parent(r,&t,&parent) && !strcmp(parent.widget.id,"filter.popup")); CHECK(dx_review_parent(r,&parent,&parent) && !strcmp(parent.widget.id,"canvas")); CHECK(!dx_review_parent(r,&parent,&parent));
    CHECK(dx_review_pick(r,65,65,DX_REVIEW_PICK_CONTAINER,&t) && !strcmp(t.widget.id,"filter.popup"));
    CHECK(dx_review_pick(r,190,190,DX_REVIEW_PICK_ANY,&t) && !strcmp(t.widget.id,"filter.popup")); CHECK(!dx_review_pick(r,190,190,DX_REVIEW_PICK_CONTROL,&t));
    CHECK(dx_review_pick(r,75,115,DX_REVIEW_PICK_ANY,&t) && !strcmp(t.widget.id,"filter.values")); CHECK(t.bounds.x==70 && t.bounds.y==110 && t.bounds.width==50 && t.bounds.height==40);
    CHECK(dx_review_pick(r,120,115,DX_REVIEW_PICK_ANY,&t) && strcmp(t.widget.id,"filter.values")); CHECK(dx_review_pick(r,75,150,DX_REVIEW_PICK_ANY,&t) && strcmp(t.widget.id,"filter.values"));
    CHECK(dx_review_pick(r,5,5,DX_REVIEW_PICK_CONTROL,&t) && !strcmp(t.widget.id,"ribbon.filter"));
    CHECK(!dx_review_pick(r,-1,5,DX_REVIEW_PICK_ANY,&t) && !t.found); CHECK(!dx_review_pick(r,NAN,5,DX_REVIEW_PICK_ANY,&t)); CHECK(!dx_review_pick(r,5,5,(DxReviewPick)99,&t));
    CHECK(dx_review_region(r,(DxReviewRect){-20,-30,50,50},&t) && t.region && t.bounds.x==0 && t.bounds.y==0 && t.bounds.width==30 && t.bounds.height==20);
    CHECK(!dx_review_parent(r,&t,&parent)); CHECK(!dx_review_region(r,(DxReviewRect){700,700,20,20},&t)); CHECK(!dx_review_region(r,(DxReviewRect){0,0,-1,20},&t));
}
static void test_data_identity(DxReviewRegistry *r) {
    DxReviewFrame f=frame(); CHECK(dx_review_begin(r,&f,true)==DX_REVIEW_OK);
    DxReviewWidget w=widget("canvas.active-cell",1,(DxReviewRect){10,10,80,24});
    w.data=(DxReviewDataIdentity){.present=true,.has_cell=true,.sheet_id=9,.source_id=7,.object_id=11,.schema_generation=13,.query_generation=15,.row=UINT64_MAX,.column=UINT64_C(9007199254740993)};
    strcpy(w.data.kind,"source-cell"); strcpy(w.data.document_id,"doc-A"); strcpy(w.data.field,"Amount"); dx_review_value_set(&w.value,"DECIMAL(38,2)",false,"-123456789012345678901234567890.12");
    CHECK(dx_review_add(r,&w)==DX_REVIEW_OK); CHECK(dx_review_seal(r)==DX_REVIEW_OK); DxReviewTarget t; CHECK(dx_review_pick(r,15,15,DX_REVIEW_PICK_CONTROL,&t));
    DxReviewDataIdentity now=t.widget.data; CHECK(!dx_review_target_stale(&t,&now)); now.row=0; now.column=0; CHECK(!dx_review_target_stale(&t,&now));
    now.query_generation++; CHECK(dx_review_target_stale(&t,&now)); now=t.widget.data; now.schema_generation++; CHECK(dx_review_target_stale(&t,&now));
    now=t.widget.data; now.source_id++; CHECK(dx_review_target_stale(&t,&now)); now=t.widget.data; now.object_id++; CHECK(dx_review_target_stale(&t,&now));
    now=t.widget.data; now.sheet_id++; CHECK(dx_review_target_stale(&t,&now)); now=t.widget.data; strcpy(now.document_id,"doc-B"); CHECK(dx_review_target_stale(&t,&now));
    CHECK(dx_review_target_stale(&t,NULL)); t.widget.data.present=false; CHECK(!dx_review_target_stale(&t,NULL)); t.found=false; CHECK(dx_review_target_stale(&t,&now));
}
static char *test_json_and_values(DxReviewRegistry *r) {
    DxReviewFrame f=frame(); CHECK(dx_review_begin(r,&f,true)==DX_REVIEW_OK); DxReviewStatus status; size_t length;
    CHECK(!dx_review_json_alloc(r,NULL,"report_1","note",&length,&status) && status==DX_REVIEW_NOT_SEALED);
    const char *raw[]={NULL,"","NULL","\"\"","-123456789012345678901234567890.12","İzmir 😀 \"quoted\" \\ slash\n\001"};
    for (size_t i=0;i<sizeof(raw)/sizeof(*raw);i++) {
        char id[32]; snprintf(id,sizeof(id),"cell.%zu",i); DxReviewWidget w=widget(id,1,(DxReviewRect){(float)i*20,0,20,20});
        w.data=(DxReviewDataIdentity){.present=true,.has_cell=true,.query_generation=UINT64_MAX,.row=UINT64_MAX}; strcpy(w.data.kind,"source-cell");
        dx_review_value_set(&w.value,i==4 ? "DECIMAL(38,2)" : "VARCHAR",!raw[i],raw[i]); CHECK(dx_review_add(r,&w)==DX_REVIEW_OK);
    }
    CHECK(dx_review_seal(r)==DX_REVIEW_OK); DxReviewTarget t; CHECK(dx_review_pick(r,82,5,DX_REVIEW_PICK_CONTROL,&t));
    char *json=dx_review_json_alloc(r,&t,"report_1","A \"quote\" and newline\n😀",&length,&status);
    CHECK(json && status==DX_REVIEW_OK && strlen(json)==length && length<DX_REVIEW_JSON_LIMIT && valid_json(json) && utf8(json));
    CHECK(strstr(json,"\"schema\":\"duxcel.native-review\"") && strstr(json,"\"version\":1") && strstr(json,"\"project_id\":\"duxcel\""));
    CHECK(strstr(json,"\"frame_id\":\"9007199254740993\"") && strstr(json,"\"timestamp_ns\":\"18446744073709551615\"") && strstr(json,"\"row\":\"18446744073709551615\""));
    CHECK(strstr(json,"\"is_null\":true,\"exact_text\":null") && strstr(json,"\"is_null\":false,\"exact_text\":\"\"") && strstr(json,"\"is_null\":false,\"exact_text\":\"NULL\""));
    CHECK(strstr(json,"-123456789012345678901234567890.12") && strstr(json,"\\\"quoted\\\"") && strstr(json,"\\\\ slash\\u000a\\u0001") && strstr(json,"İzmir 😀"));
    CHECK(strstr(json,"\"kind\":\"trace\",\"text\":\"command filter.open\"") && strstr(json,"\"state_text\":\"menu=filter"));
    size_t required=0; char small[8]="before"; CHECK(dx_review_json(r,&t,"report_1","A \"quote\" and newline\n😀",small,sizeof(small),&required)==DX_REVIEW_BUFFER_TOO_SMALL && !*small && required==length+1);
    char *buffer=malloc(required); CHECK(buffer); CHECK(dx_review_json(r,&t,"report_1","A \"quote\" and newline\n😀",buffer,required,&required)==DX_REVIEW_OK && !strcmp(buffer,json)); free(buffer);
    CHECK(!dx_review_json_alloc(r,&t,"../escape","",&length,&status) && status==DX_REVIEW_INVALID);
    t.frame_id++; CHECK(!dx_review_json_alloc(r,&t,"report_1","",&length,&status) && status==DX_REVIEW_INVALID);
    f.frame_id++; CHECK(dx_review_begin(r,&f,true)==DX_REVIEW_OK); CHECK(strstr(json,"\"frame_id\":\"9007199254740993\""));
    return json;
}
static void test_bounds_and_failures(DxReviewRegistry *r) {
    DxReviewFrame f=frame(); f.ui_scale=NAN; CHECK(dx_review_begin(r,&f,true)==DX_REVIEW_INVALID && !r->armed); f=frame(); strcpy(f.session_id,"bad/session"); CHECK(dx_review_begin(r,&f,true)==DX_REVIEW_INVALID);
    f=frame(); memset(f.session_id,'x',sizeof(f.session_id)); CHECK(dx_review_begin(r,&f,true)==DX_REVIEW_INVALID);
    f=frame(); CHECK(dx_review_begin(r,&f,true)==DX_REVIEW_OK);
    DxReviewWidget w=widget("valid",1,(DxReviewRect){0,0,10,10}); w.bounds.width=NAN; CHECK(dx_review_add(r,&w)==DX_REVIEW_INVALID); w=widget("valid",1,(DxReviewRect){0,0,10,10}); w.transform.a=INFINITY; CHECK(dx_review_add(r,&w)==DX_REVIEW_INVALID);
    w=widget("valid",1,(DxReviewRect){0,0,10,10}); memset(w.id,'x',sizeof(w.id)); CHECK(dx_review_add(r,&w)==DX_REVIEW_INVALID);
    char name[512]; memset(name,'x',sizeof(name)-1); name[sizeof(name)-1]=0; dx_review_widget_init(&w,name,"a","path","button","label"); CHECK(w.identity_truncated && dx_review_add(r,&w)==DX_REVIEW_INVALID);
    dx_review_widget_init(&w,"invalid\nidentity","a","path","button","label"); CHECK(dx_review_add(r,&w)==DX_REVIEW_INVALID);
    dx_review_widget_init(&w,"invalid\xff","a","path","button","label"); CHECK(dx_review_add(r,&w)==DX_REVIEW_INVALID);
    char raw[4096];
    for (size_t padding=0;padding<12;padding++) {
        memset(raw,'x',padding); size_t n=padding; while (n+9<sizeof(raw)) { memcpy(raw+n,"ğ€😀",9); n+=9; } raw[n]=0;
        dx_review_widget_init(&w,"longlabel","action","path","button",raw); CHECK(w.truncated && utf8(w.label));
        dx_review_value_set(&w.value,"VARCHAR",false,raw); CHECK(w.value.present && w.value.truncated && utf8(w.value.exact_text) && strlen(w.value.exact_text)<sizeof(w.value.exact_text));
        dx_review_value_set(&w.value,"VARCHAR",false,w.value.exact_text); CHECK(utf8(w.value.exact_text));
    }
    dx_review_value_set(&w.value,"VARCHAR",true,"not NULL"); CHECK(w.value.is_null && !*w.value.exact_text);
    dx_review_value_set(&w.value,"VARCHAR",false,NULL); CHECK(!w.value.present);
    char original[]="exact owned value"; dx_review_value_set(&w.value,"VARCHAR",false,original); original[0]='X'; CHECK(!strcmp(w.value.exact_text,"exact owned value"));
    f=frame(); strcpy(f.state_text,"Malformed \xff text"); CHECK(dx_review_begin(r,&f,true)==DX_REVIEW_OK && strstr(r->frame.state_text,"�") && utf8(r->frame.state_text));
    f=frame(); for (size_t i=0;i<sizeof(f.trace_text)-1;i++) f.trace_text[i]=i%2 ? '\n' : 'x'; f.trace_text[sizeof(f.trace_text)-1]=0;
    CHECK(dx_review_begin(r,&f,true)==DX_REVIEW_OK); CHECK(dx_review_seal(r)==DX_REVIEW_OK);
    char note[8192]; memset(note,'n',sizeof(note)-1); note[sizeof(note)-1]=0; DxReviewStatus status; size_t length;
    char *json=dx_review_json_alloc(r,NULL,"event_cap",note,&length,&status); CHECK(json && valid_json(json) && strstr(json,"\"events_truncated\":true") && strstr(json,"\"note_truncated\":true")); free(json);
    f.window_width=f.window_height=f.framebuffer_width=f.framebuffer_height=UINT32_MAX;
    CHECK(dx_review_begin(r,&f,true)==DX_REVIEW_OK); w=widget("integer-metadata",INT32_MAX,(DxReviewRect){0,0,10,10}); w.source_line=UINT32_MAX;
    CHECK(dx_review_add(r,&w)==DX_REVIEW_OK && dx_review_seal(r)==DX_REVIEW_OK);
    json=dx_review_json_alloc(r,NULL,"integer_metadata","",&length,&status);
    CHECK(json && valid_json(json) && strstr(json,"\"width\":4294967295") && strstr(json,"\"layer\":2147483647") && strstr(json,"\"line\":4294967295")); free(json);
    f=frame(); CHECK(dx_review_begin(r,&f,true)==DX_REVIEW_OK);
    w=widget("capacity",1,(DxReviewRect){0,0,10,10});
    for (size_t i=0;i<DX_REVIEW_WIDGETS;i++) { snprintf(w.id,sizeof(w.id),"control.%zu",i); CHECK(dx_review_add(r,&w)==DX_REVIEW_OK); }
    CHECK(r->count==DX_REVIEW_WIDGETS); strcpy(w.id,"extra"); CHECK(dx_review_add(r,&w)==DX_REVIEW_CAPACITY && r->overflow_count==1); strcpy(w.id,"control.0"); CHECK(dx_review_add(r,&w)==DX_REVIEW_DUPLICATE && r->duplicate_count==1);
    CHECK(dx_review_seal(r)==DX_REVIEW_OK); json=dx_review_json_alloc(r,NULL,"capacity","",&length,&status); CHECK(json && valid_json(json) && length<DX_REVIEW_JSON_LIMIT && strstr(json,"\"overflow\":\"1\"")); free(json);
    CHECK(dx_review_begin(r,&f,true)==DX_REVIEW_OK); w=widget("large",1,(DxReviewRect){0,0,10,10}); memset(w.value.exact_text,1,sizeof(w.value.exact_text)-1); w.value.present=true; strcpy(w.value.type,"VARCHAR");
    for (size_t i=0;i<700;i++) { snprintf(w.id,sizeof(w.id),"large.%zu",i); CHECK(dx_review_add(r,&w)==DX_REVIEW_OK); }
    CHECK(dx_review_seal(r)==DX_REVIEW_OK); CHECK(!dx_review_json_alloc(r,NULL,"too_large","",&length,&status) && status==DX_REVIEW_JSON_TOO_LARGE && !length);
    char buffer[8]="before"; CHECK(dx_review_json(r,NULL,"too_large","",buffer,sizeof(buffer),&length)==DX_REVIEW_JSON_TOO_LARGE && !*buffer && !length);
    CHECK(!strcmp(dx_review_status_text(DX_REVIEW_DUPLICATE),"duplicate widget ID") && !strcmp(dx_review_status_text((DxReviewStatus)-1),"unknown review status"));
}
int main(int argc,char **argv) {
    DxReviewRegistry *r=calloc(1,sizeof(*r)); CHECK(r);
    test_scope_and_lifetime(r); test_geometry_and_layers(r); test_data_identity(r); char *sample=test_json_and_values(r); test_bounds_and_failures(r);
    if (argc==2) { FILE *file=fopen(argv[1],"wb"); CHECK(file); CHECK(fwrite(sample,1,strlen(sample),file)==strlen(sample)); CHECK(!fclose(file)); }
    free(sample); free(r);
    printf("PASS: %u review registry checks (armed capture, stable identities, clipping/layers/disabled controls, owned snapshots, stale query scope, typed exact values, UTF-8 JSON and bounded overflow)\n",checks); return 0;
}
