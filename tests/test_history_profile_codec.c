#include "workbook/history_profile_codec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL history profile codec:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
#include "support/history_profile_fixture.h"
static WorkbookHistoryProfileCaps caps(void) { return (WorkbookHistoryProfileCaps){4096,4u*1024u*1024u,DX_HISTORY_PROFILE_OWNED_BYTES,DX_HISTORY_PROFILE_STAGING_BYTES}; }
static void put(unsigned char *p,uint64_t v,unsigned n) { for (unsigned i=0;i<n;i++) p[i]=(unsigned char)(v>>(8*i)); }
static void rejected(unsigned char *wire,size_t bytes,WorkbookHistoryProfileBinding binding,WorkbookHistoryProfileCaps budget,
    void *scratch,size_t scratch_bytes,WorkbookHistoryProfile *prior,WorkbookHistoryProfileCodecStatus expected) {
    WorkbookHistoryProfile *out=prior; CHECK(workbook_history_profile_decode(wire,bytes,binding,budget,scratch,scratch_bytes,&out)==expected && out==prior);
}
static void malformed(unsigned char *wire,size_t bytes,size_t offset,unsigned char mask,WorkbookHistoryProfileBinding binding,void *scratch,size_t scratch_bytes,WorkbookHistoryProfile *prior,WorkbookHistoryProfileCodecStatus expected) {
    wire[offset]^=mask; rejected(wire,bytes,binding,caps(),scratch,scratch_bytes,prior,expected); wire[offset]^=mask;
}
static unsigned char *encode(const WorkbookHistoryProfile *profile,size_t *bytes) {
    CHECK(workbook_history_profile_encoded_size(profile,bytes)==HISTORY_PROFILE_CODEC_OK); unsigned char *wire=dx_alloc(*bytes+1); size_t written=77; wire[*bytes]=0xa5;
    CHECK(workbook_history_profile_encode(profile,wire,*bytes,&written)==HISTORY_PROFILE_CODEC_OK && written==*bytes && wire[*bytes]==0xa5); return wire;
}
static void mixed(void) {
    Fixture f; fixture(&f); WorkbookHistoryProfile *owner=NULL,*out=NULL;
    CHECK(workbook_history_profile_prepare(f.view,caps(),&owner)==HISTORY_PROFILE_OK);
    size_t bytes=0; unsigned char *wire=encode(owner,&bytes); WorkbookHistoryProfileWireInfo info={0};
    CHECK(workbook_history_profile_preflight(wire,bytes,&info)==HISTORY_PROFILE_CODEC_OK && info.bytes==bytes && info.records==6 && info.parse_bytes && info.parse_bytes<=DX_HISTORY_PROFILE_PARSE_BYTES && info.alignment);
    void *scratch=dx_alloc(info.parse_bytes+info.alignment); size_t scratch_bytes=info.parse_bytes+info.alignment;
    CHECK((uintptr_t)scratch%info.alignment==0); WorkbookHistoryProfileBinding binding={{f.doc.high,f.doc.low,999,VERSION_WORKING_HEAD},55};
    CHECK(workbook_history_profile_decode(wire,bytes,binding,caps(),scratch,scratch_bytes,&out)==HISTORY_PROFILE_CODEC_OK);
    const WorkbookHistoryProfileView *held=workbook_history_profile_view(out);
    CHECK(held->count==6 && held->current.records[0]==held->saved.records[0] && held->records[5].event->owner.session==999 && held->records[5].event->edit_session==55);
    CHECK(held->records[5].event->action==6 && held->records[0].sheet==1 && !held->records[1].live && workbook_history_profile_report(out,2)->compatibility==SOURCE_DEPENDENCY_UNAVAILABLE);
    size_t same_bytes=0; unsigned char *same=encode(out,&same_bytes); CHECK(same_bytes==bytes && !memcmp(wire,same,bytes)); free(same);
    WorkbookHistoryProfileMeasure peak; CHECK(workbook_history_profile_measure(*held,caps(),out,&peak)==HISTORY_PROFILE_OK);
    WorkbookHistoryProfileCaps exact=caps(); exact.staging_bytes=peak.staging+scratch_bytes;
    WorkbookHistoryProfileCaps short_budget=exact; short_budget.staging_bytes--; rejected(wire,bytes,binding,short_budget,scratch,scratch_bytes,out,HISTORY_PROFILE_CODEC_LIMIT);
    CHECK(workbook_history_profile_decode(wire,bytes,binding,exact,scratch,scratch_bytes,&out)==HISTORY_PROFILE_CODEC_OK);
    short_budget=caps(); short_budget.records=5; rejected(wire,bytes,binding,short_budget,scratch,scratch_bytes,out,HISTORY_PROFILE_CODEC_LIMIT);
    rejected(wire,bytes,binding,caps(),scratch,info.parse_bytes-1,out,HISTORY_PROFILE_CODEC_LIMIT);
    rejected(wire,bytes,binding,caps(),(unsigned char *)scratch+1,scratch_bytes,out,HISTORY_PROFILE_CODEC_INVALID);
    rejected(wire,bytes,binding,caps(),wire,scratch_bytes,out,HISTORY_PROFILE_CODEC_INVALID);
    rejected(wire,bytes,binding,caps(),scratch,DX_HISTORY_PROFILE_PARSE_BYTES+1u,out,HISTORY_PROFILE_CODEC_LIMIT);
    WorkbookHistoryProfileBinding wrong=binding; wrong.owner.document_low++; rejected(wire,bytes,wrong,caps(),scratch,scratch_bytes,out,HISTORY_PROFILE_CODEC_ADMISSION);
    wrong=binding; wrong.owner.session=0; rejected(wire,bytes,wrong,caps(),scratch,scratch_bytes,out,HISTORY_PROFILE_CODEC_INVALID);
    wrong=binding; wrong.edit_session=0; rejected(wire,bytes,wrong,caps(),scratch,scratch_bytes,out,HISTORY_PROFILE_CODEC_INVALID);
    wrong=binding; wrong.owner.mode=VERSION_HISTORICAL_VIEW; rejected(wire,bytes,wrong,caps(),scratch,scratch_bytes,out,HISTORY_PROFILE_CODEC_INVALID);
    malformed(wire,bytes,0,1,binding,scratch,scratch_bytes,out,HISTORY_PROFILE_CODEC_UNSUPPORTED);
    malformed(wire,bytes,4,2,binding,scratch,scratch_bytes,out,HISTORY_PROFILE_CODEC_UNSUPPORTED);
    malformed(wire,bytes,32,2,binding,scratch,scratch_bytes,out,HISTORY_PROFILE_CODEC_UNSUPPORTED);
    malformed(wire,bytes,36,1,binding,scratch,scratch_bytes,out,HISTORY_PROFILE_CODEC_UNSUPPORTED);
    malformed(wire,bytes,40,1,binding,scratch,scratch_bytes,out,HISTORY_PROFILE_CODEC_UNSUPPORTED);
    malformed(wire,bytes,44,2,binding,scratch,scratch_bytes,out,HISTORY_PROFILE_CODEC_INVALID);
    malformed(wire,bytes,116,1,binding,scratch,scratch_bytes,out,HISTORY_PROFILE_CODEC_INVALID);
    malformed(wire,bytes,159,1,binding,scratch,scratch_bytes,out,HISTORY_PROFILE_CODEC_INVALID);
    malformed(wire,bytes,DX_HISTORY_PROFILE_HEADER_BYTES+44,1,binding,scratch,scratch_bytes,out,HISTORY_PROFILE_CODEC_INVALID);
    put(wire+112,DX_HISTORY_PROFILE_RECORDS+1u,4); rejected(wire,bytes,binding,caps(),scratch,scratch_bytes,out,HISTORY_PROFILE_CODEC_LIMIT); put(wire+112,6,4);
    WorkbookHistoryProfileWireInfo old={.bytes=77},before=old;
    for (size_t n=0;n<DX_HISTORY_PROFILE_HEADER_BYTES;n++) CHECK(workbook_history_profile_preflight(wire,n,&old)!=HISTORY_PROFILE_CODEC_OK && !memcmp(&old,&before,sizeof(old)));
    rejected(wire,bytes-1,binding,caps(),scratch,scratch_bytes,out,HISTORY_PROFILE_CODEC_INVALID);
    put(wire+8,bytes-1,8); rejected(wire,bytes-1,binding,caps(),scratch,scratch_bytes,out,HISTORY_PROFILE_CODEC_INVALID); put(wire+8,bytes,8);
    wire[bytes]=0; put(wire+8,bytes+1,8); rejected(wire,bytes+1,binding,caps(),scratch,scratch_bytes,out,HISTORY_PROFILE_CODEC_INVALID); put(wire+8,bytes,8);
    const WorkbookHistoryProfileView *v=workbook_history_profile_view(owner); size_t last=bytes-v->records[5].bytes-112u;
    malformed(wire,bytes,last+20,1,binding,scratch,scratch_bytes,out,HISTORY_PROFILE_CODEC_INVALID);
    malformed(wire,bytes,last+80,2,binding,scratch,scratch_bytes,out,HISTORY_PROFILE_CODEC_INVALID);
    put(wire+last+40,5,8); rejected(wire,bytes,binding,caps(),scratch,scratch_bytes,out,HISTORY_PROFILE_CODEC_INVALID); put(wire+last+40,6,8);
    put(wire+last,99,4); rejected(wire,bytes,binding,caps(),scratch,scratch_bytes,out,HISTORY_PROFILE_CODEC_ADMISSION); put(wire+last,5,4);
    SheetTopologyInfo topology; CHECK(sheet_topology_preflight(v->records[5].data,v->records[5].bytes,&topology)==SHEET_TOPOLOGY_OK);
    size_t late=bytes-topology.presence_bytes+DX_SHEET_PRESENCE_HEADER_BYTES;
    malformed(wire,bytes,late,1,binding,scratch,scratch_bytes,out,HISTORY_PROFILE_CODEC_ADMISSION);
    /* Failed encoder sizing leaves both output bytes and scalar untouched. */
    unsigned char *tiny=dx_alloc(bytes); memset(tiny,0xa5,bytes); size_t written=77;
    CHECK(workbook_history_profile_encode(owner,tiny,bytes-1,&written)==HISTORY_PROFILE_CODEC_LIMIT && written==77);
    for (size_t i=0;i<bytes;i++) CHECK(tiny[i]==0xa5); free(tiny);
    fixture_free(&f); workbook_history_profile_free(&owner); free(wire); free(scratch);
    held=workbook_history_profile_view(out); CHECK(!memcmp(held->records[5].data,"DXTP",4) && !memcmp(held->versions->tags[0].label.data,"Current",7));
    workbook_history_profile_free(&out);
}
static void canonical_metadata(void) {
    Fixture f; fixture(&f); WorkbookVersionRoot roots[]={f.root,f.root}; roots[1].key.bytes[0]++; roots[1].bytes=44;
    WorkbookVersionTag tags[]={{{"Base",4},roots[1].key,1},{{"Current",7},roots[0].key,2}};
    WorkbookHistoryAnchor anchors[]={{{0,2,roots[1].key},0},{{1,4,roots[0].key},0}}; WorkbookHistoryTagAnchor bindings[]={{1,0},{2,1}};
    f.index.roots=roots; f.index.root_count=2; f.index.tags=tags; f.index.tag_count=2; f.index.generation=f.index.saved_generation=2;
    f.anchors.items=anchors; f.anchors.count=2; f.anchors.tags=bindings; f.anchors.tag_count=2;
    WorkbookHistoryProfile *a=NULL,*b=NULL; CHECK(workbook_history_profile_prepare(f.view,caps(),&a)==HISTORY_PROFILE_OK);
    WorkbookVersionRoot root=roots[0]; roots[0]=roots[1]; roots[1]=root; WorkbookVersionTag tag=tags[0]; tags[0]=tags[1]; tags[1]=tag;
    WorkbookHistoryAnchor anchor=anchors[0]; anchors[0]=anchors[1]; anchors[1]=anchor; WorkbookHistoryTagAnchor binding=bindings[0]; bindings[0]=bindings[1]; bindings[1]=binding;
    CHECK(workbook_history_profile_prepare(f.view,caps(),&b)==HISTORY_PROFILE_OK);
    size_t an=0,bn=0; unsigned char *aw=encode(a,&an),*bw=encode(b,&bn); CHECK(an==bn && !memcmp(aw,bw,an));
    WorkbookHistoryProfileWireInfo info; CHECK(workbook_history_profile_preflight(aw,an,&info)==HISTORY_PROFILE_CODEC_OK);
    void *scratch=dx_alloc(info.parse_bytes); WorkbookHistoryProfile *decoded=NULL;
    WorkbookHistoryProfileBinding runtime={{f.doc.high,f.doc.low,90,VERSION_WORKING_HEAD},91};
    CHECK(workbook_history_profile_decode(aw,an,runtime,caps(),scratch,info.parse_bytes,&decoded)==HISTORY_PROFILE_CODEC_OK);
    const WorkbookHistoryProfileView *held=workbook_history_profile_view(decoded); CHECK(held->versions->roots[0].key.bytes[0]==71 && held->versions->tags[0].created==1 && held->anchors->items[0].boundary.anchor==0);
    unsigned char swapped[48]; memcpy(swapped,aw+160,48); memcpy(aw+160,aw+208,48); memcpy(aw+208,swapped,48);
    rejected(aw,an,runtime,caps(),scratch,info.parse_bytes,decoded,HISTORY_PROFILE_CODEC_INVALID);
    free(scratch); free(aw); free(bw); workbook_history_profile_free(&a); workbook_history_profile_free(&b); workbook_history_profile_free(&decoded); fixture_free(&f);
}
static void checkpoint(void) {
    Fixture f; fixture(&f); WorkbookHistoryProfileView view=f.view; view.kind=HISTORY_PROFILE_CHECKPOINT_ONLY; view.anchors=NULL; view.current=(WorkbookHistoryProfilePath){0}; view.saved=(WorkbookHistoryProfilePath){0}; view.records=NULL; view.count=0;
    WorkbookHistoryProfile *owner=NULL,*out=NULL; CHECK(workbook_history_profile_prepare(view,caps(),&owner)==HISTORY_PROFILE_OK);
    size_t bytes=0; unsigned char *wire=encode(owner,&bytes); WorkbookHistoryProfileWireInfo info;
    CHECK(workbook_history_profile_preflight(wire,bytes,&info)==HISTORY_PROFILE_CODEC_OK && !info.records);
    void *scratch=dx_alloc(info.parse_bytes); WorkbookHistoryProfileBinding runtime={{f.doc.high,f.doc.low,6,VERSION_WORKING_HEAD},7};
    CHECK(workbook_history_profile_decode(wire,bytes,runtime,caps(),scratch,info.parse_bytes,&out)==HISTORY_PROFILE_CODEC_OK && !workbook_history_profile_protection(out));
    put(wire+104,1,4); rejected(wire,bytes,runtime,caps(),scratch,info.parse_bytes,out,HISTORY_PROFILE_CODEC_INVALID); put(wire+104,0,4);
    free(wire); free(scratch); workbook_history_profile_free(&owner); workbook_history_profile_free(&out); fixture_free(&f);
}
static void fork_union(void) {
    Fixture f; fixture(&f);
    WorkbookHistoryEventPosition saved_events[]={{1,0,1},{2,0,2}},current_events[]={{1,0,1},{3,1,2}};
    WorkbookHistorySegment saved_segment={0,0,2},current_segments[]={{0,0,1},{1,1,2}};
    WorkbookHistoryPath saved={f.doc.high,f.doc.low,&saved_segment,1,saved_events,2,0,2,0,0,2,2};
    WorkbookHistoryPath current={f.doc.high,f.doc.low,current_segments,2,current_events,2,1,3,0,0,1,2};
    size_t saved_map[]={0,1},current_map[]={0,2}; f.anchor.boundary.position=2;
    f.frames[2].lineage=1; f.frames[2].parent_lineage=0; f.frames[2].forked=true; f.frames[2].position=2; f.frames[2].previous_position=1; f.frames[2].action=1;
    f.view.current=(WorkbookHistoryProfilePath){&current,current_map,2}; f.view.saved=(WorkbookHistoryProfilePath){&saved,saved_map,2}; f.view.count=3;
    WorkbookHistoryProfile *owner=NULL,*out=NULL; CHECK(workbook_history_profile_prepare(f.view,caps(),&owner)==HISTORY_PROFILE_OK);
    size_t bytes=0; unsigned char *wire=encode(owner,&bytes); WorkbookHistoryProfileWireInfo info;
    CHECK(workbook_history_profile_preflight(wire,bytes,&info)==HISTORY_PROFILE_CODEC_OK && info.records==3);
    void *scratch=dx_alloc(info.parse_bytes); WorkbookHistoryProfileBinding binding={{f.doc.high,f.doc.low,70,VERSION_WORKING_HEAD},71};
    CHECK(workbook_history_profile_decode(wire,bytes,binding,caps(),scratch,info.parse_bytes,&out)==HISTORY_PROFILE_CODEC_OK);
    const WorkbookHistoryProfileView *v=workbook_history_profile_view(out); const WorkbookHistoryProtection *p=workbook_history_profile_protection(out);
    CHECK(p && p->shared_through==1 && p->current_cursor==1 && p->current_through==2 && p->saved.through==2);
    CHECK(v->current.records[0]==v->saved.records[0] && v->current.records[1]==2 && v->saved.records[1]==1 && v->records[2].event->action==1 && v->records[2].event->transition==3 && v->records[2].event->forked);
    size_t encoded_bytes=0; unsigned char *encoded=encode(out,&encoded_bytes); CHECK(bytes==encoded_bytes && !memcmp(wire,encoded,bytes));
    /* Wire slot mapping remains bound to durable transition/path identity even
       though its runtime fences are freshly assigned by decode. */
    size_t path=160u+48u+48u+7u+64u+16u;
    CHECK(!memcmp(wire+path,"\1\0\0\0",4));
    size_t first_map=path+64u+48u+24u;
    put(wire+first_map,1,4); rejected(wire,bytes,binding,caps(),scratch,info.parse_bytes,out,HISTORY_PROFILE_CODEC_ADMISSION); put(wire+first_map,0,4);
    malformed(wire,bytes,first_map+4,1,binding,scratch,info.parse_bytes,out,HISTORY_PROFILE_CODEC_INVALID);
    free(wire); free(encoded); free(scratch); workbook_history_profile_free(&owner); workbook_history_profile_free(&out); fixture_free(&f);
}
static void empty_checkpoint(void) {
    WorkbookVersionIndex index={.document_high=8,.document_low=9};
    WorkbookHistoryProfileView view={.document={8,9},.container_version=16,.core_version=15,.kind=HISTORY_PROFILE_CHECKPOINT_ONLY,.versions=&index};
    WorkbookHistoryProfile *owner=NULL,*out=NULL; CHECK(workbook_history_profile_prepare(view,caps(),&owner)==HISTORY_PROFILE_OK);
    size_t bytes=0; unsigned char *wire=encode(owner,&bytes); WorkbookHistoryProfileWireInfo info;
    CHECK(bytes==160u+128u && workbook_history_profile_preflight(wire,bytes,&info)==HISTORY_PROFILE_CODEC_OK && !info.parse_bytes && !info.records);
    WorkbookHistoryProfileBinding binding={{8,9,5,VERSION_WORKING_HEAD},6};
    CHECK(workbook_history_profile_decode(wire,bytes,binding,caps(),NULL,0,&out)==HISTORY_PROFILE_CODEC_OK && !workbook_history_profile_view(out)->versions->saved.present);
    put(wire+32,HISTORY_PROFILE_RETAINED,4); rejected(wire,bytes,binding,caps(),NULL,0,out,HISTORY_PROFILE_CODEC_ADMISSION); put(wire+32,HISTORY_PROFILE_CHECKPOINT_ONLY,4);
    /* Absence is encoded, not filled from a missing field or older format. */
    put(wire+44,1,4); rejected(wire,bytes,binding,caps(),NULL,0,out,HISTORY_PROFILE_CODEC_ADMISSION); put(wire+44,0,4);
    free(wire); workbook_history_profile_free(&owner); workbook_history_profile_free(&out);
}
int main(void) { mixed(); canonical_metadata(); checkpoint(); fork_union(); empty_checkpoint(); printf("PASS: %u canonical current history profile wire checks\n",checks); return 0; }
