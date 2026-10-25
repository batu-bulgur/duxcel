#include "workbook/cells_tsv_scan.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL cell TSV:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
typedef struct { unsigned count; char *text[16]; uint64_t row[16]; size_t col[16]; } Fields;
static bool field(void *context,uint64_t row,size_t col,const char *text,size_t bytes,char **error) {
    (void)error; Fields *f=context; if (f->count==16) return false;
    unsigned i=f->count++; f->text[i]=dx_strdup(text); f->row[i]=row; f->col[i]=col; CHECK(strlen(text)==bytes); return true;
}
int main(void) {
    const char *input="NULL\t\"\"\t\"a\t\"\"b\"\r\n\"İstanbul\nline\"\r\n";
    Fields f={0}; size_t rows=99,cols=99; char *error=NULL;
    CHECK(cells_tsv_scan(3,2,input,field,&f,&rows,&cols,&error) && !error && rows==2 && cols==3 && f.count==4);
    CHECK(!strcmp(f.text[0],"NULL") && !*f.text[1] && !strcmp(f.text[2],"a\t\"b") && !strcmp(f.text[3],"İstanbul\nline") && f.row[3]==4 && f.col[3]==2);
    for (unsigned i=0;i<f.count;i++) free(f.text[i]);
    CellStore s={0}; CHECK(cells_set(&s,4,3,"Ragged hole",&error));
    CHECK(cells_paste(&s,3,2,input,&rows,&cols,&error) && rows==2 && cols==3 && !strcmp(cells_get(&s,4,3),"Ragged hole") && !strcmp(cells_get(&s,3,2),"NULL") && !*cells_get(&s,3,3));
    CellStore before=s; rows=cols=99;
    CHECK(!cells_paste(&s,0,0,"ok\t\"unterminated",&rows,&cols,&error) && error && rows==99 && cols==99 && !memcmp(&before,&s,sizeof(s))); free(error); error=NULL;
    CHECK(!cells_paste(&s,0,0,"\"closed\"bad",&rows,&cols,&error) && error && !memcmp(&before,&s,sizeof(s))); free(error); error=NULL;
    CHECK(cells_paste(&s,3,2,"",&rows,&cols,&error) && rows==1 && cols==1 && !*cells_get(&s,3,2));
    CHECK(cells_paste(&s,0,0,"A\t\n\n",&rows,&cols,&error) && rows==2 && cols==2);
    CHECK(!cells_paste(&s,DX_SHEET_ROWS-1,16383,"A\tB",&rows,&cols,&error) && error); free(error); error=NULL;
    char *long_text=dx_alloc(65538); memset(long_text,'X',65537); long_text[65537]=0;
    CHECK(!cells_paste(&s,0,0,long_text,&rows,&cols,&error) && error); free(error); free(long_text);
    cells_free(&s); printf("PASS: %u shared TSV scanner checks\n",checks); return 0;
}
