#include "../src/workbook/version_chunks.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL version chunks:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static unsigned char block[DX_VERSION_CHUNK_BYTES];
static void round_trip(uint64_t bytes) {
    WorkbookChunks chunks={0}; CHECK(workbook_chunks_begin(bytes,bytes,&chunks)==VERSION_CHUNKS_OK);
    CHECK(workbook_chunks_finish(&chunks)==VERSION_CHUNKS_INCOMPLETE);
    uint64_t consumed=0;
    for (uint64_t i=0;i<chunks.count;i++) {
        uint64_t offset=UINT64_MAX; size_t length=SIZE_MAX;
        CHECK(workbook_chunk_span(bytes,i,&offset,&length) && offset==consumed && length && length<=sizeof(block));
        CHECK(workbook_chunks_offer(&chunks,i,block,length)==VERSION_CHUNKS_OK);
        consumed+=length; CHECK(chunks.received==consumed);
    }
    CHECK(consumed==bytes && workbook_chunks_finish(&chunks)==VERSION_CHUNKS_OK && workbook_chunks_finish(&chunks)==VERSION_CHUNKS_OK);
    CHECK(workbook_chunks_offer(&chunks,chunks.count,block,1)==VERSION_CHUNKS_SEQUENCE);
    CHECK(workbook_chunks_finish(&chunks)==VERSION_CHUNKS_SEQUENCE);
}
int main(void) {
    uint64_t sizes[]={1,2,65535,65536,65537,131071,131072,131073,900001};
    for (size_t i=0;i<sizeof(sizes)/sizeof(*sizes);i++) round_trip(sizes[i]);
    WorkbookChunks chunks,prior; memset(&chunks,0xa5,sizeof(chunks)); memcpy(&prior,&chunks,sizeof(prior));
    CHECK(workbook_chunks_begin(0,0,&chunks)==VERSION_CHUNKS_BOUNDS && !memcmp(&chunks,&prior,sizeof(chunks)));
    CHECK(workbook_chunks_begin(65536,65535,&chunks)==VERSION_CHUNKS_BOUNDS && !memcmp(&chunks,&prior,sizeof(chunks)));
    CHECK(workbook_chunks_begin(UINT64_MAX,UINT64_MAX,&chunks)==VERSION_CHUNKS_OK);
    CHECK(chunks.count==UINT64_C(281474976710656));
    uint64_t offset=1; size_t length=2;
    CHECK(workbook_chunk_span(UINT64_MAX,chunks.count-1,&offset,&length) && offset==UINT64_MAX-65535 && length==65535);
    offset=1; length=2;
    CHECK(!workbook_chunk_span(UINT64_MAX,chunks.count,&offset,&length) && offset==1 && length==2);
    CHECK(!workbook_chunk_span(0,0,&offset,&length) && offset==1 && length==2);
    CHECK(!workbook_chunk_span(1,0,NULL,&length) && length==2);
    CHECK(!workbook_chunk_span(1,0,&offset,NULL) && offset==1);
    CHECK(workbook_chunks_begin(65537,65537,&chunks)==VERSION_CHUNKS_OK);
    CHECK(workbook_chunks_offer(&chunks,1,block,1)==VERSION_CHUNKS_SEQUENCE);
    CHECK(workbook_chunks_offer(&chunks,0,block,sizeof(block))==VERSION_CHUNKS_SEQUENCE && !chunks.received);
    CHECK(workbook_chunks_begin(65537,65537,&chunks)==VERSION_CHUNKS_OK);
    CHECK(workbook_chunks_offer(&chunks,0,block,sizeof(block))==VERSION_CHUNKS_OK);
    CHECK(workbook_chunks_offer(&chunks,0,block,sizeof(block))==VERSION_CHUNKS_SEQUENCE && chunks.received==65536);
    CHECK(workbook_chunks_begin(65537,65537,&chunks)==VERSION_CHUNKS_OK);
    CHECK(workbook_chunks_offer(&chunks,0,NULL,sizeof(block))==VERSION_CHUNKS_LENGTH);
    CHECK(workbook_chunks_finish(&chunks)==VERSION_CHUNKS_LENGTH);
    CHECK(workbook_chunks_begin(1,1,&chunks)==VERSION_CHUNKS_OK);
    CHECK(workbook_chunks_offer(&chunks,0,block,0)==VERSION_CHUNKS_LENGTH);
    CHECK(workbook_chunks_begin(1,1,&chunks)==VERSION_CHUNKS_OK);
    CHECK(workbook_chunks_offer(&chunks,0,block,SIZE_MAX)==VERSION_CHUNKS_LENGTH);
    CHECK(workbook_chunks_begin(1,1,&chunks)==VERSION_CHUNKS_OK);
    CHECK(workbook_chunks_offer(&chunks,UINT64_MAX,block,1)==VERSION_CHUNKS_SEQUENCE);
    CHECK(workbook_chunks_begin(1,1,&chunks)==VERSION_CHUNKS_OK); chunks.next=UINT64_MAX;
    CHECK(workbook_chunks_offer(&chunks,0,block,1)==VERSION_CHUNKS_BOUNDS);
    CHECK(workbook_chunks_begin(1,1,&chunks)==VERSION_CHUNKS_OK); chunks.received=1;
    CHECK(workbook_chunks_finish(&chunks)==VERSION_CHUNKS_BOUNDS);
    CHECK(workbook_chunks_begin(1,1,NULL)==VERSION_CHUNKS_BOUNDS);
    CHECK(workbook_chunks_offer(NULL,0,block,1)==VERSION_CHUNKS_BOUNDS);
    CHECK(workbook_chunks_finish(NULL)==VERSION_CHUNKS_BOUNDS);
    printf("PASS: %u bounded version chunk checks\n",checks); return 0;
}
