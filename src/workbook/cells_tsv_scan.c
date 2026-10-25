#include "cells_tsv_scan.h"
#include <stdlib.h>
#include <string.h>
static bool fail(char **error,const char *text) { if (error) { free(*error); *error=dx_strdup(text); } return false; }
bool cells_tsv_scan(uint64_t row,size_t col,const char *text,CellTSVVisitor visitor,void *context,size_t *rows,size_t *columns,char **error) {
    if (!text || !visitor || !rows || !columns) return fail(error,"Choose complete clipboard text and a field visitor.");
    size_t r=0,c=0,width=0,count=0,length=strlen(text);
    if (length>8*1024*1024) return fail(error,"Paste up to 8 MiB of text.");
    char field[65537]; const char *p=text;
    for (;;) {
        size_t n=0; bool quoted=*p=='"',closed=!quoted; if (quoted) p++;
        while (*p) {
            if (quoted && *p=='"') { p++; if (*p!='"') { closed=true; break; } }
            else if (!quoted && (*p=='\t' || *p=='\r' || *p=='\n')) break;
            if (n==65536) return fail(error,"Cell exceeds the 64 KiB text limit.");
            field[n++]=*p++;
        }
        if (!closed || (*p && *p!='\t' && *p!='\r' && *p!='\n')) return fail(error,"Invalid quoted clipboard text.");
        field[n]=0;
        if (count==DX_MAX_EDIT_CELLS || (uint64_t)r>=DX_SHEET_ROWS || row>=DX_SHEET_ROWS-r || c>=16384 || col>=16384-c) return fail(error,"Paste up to 20,000 cells inside the worksheet.");
        if (!visitor(context,row+r,col+c,field,n,error)) return false;
        count++; if (c+1>width) width=c+1;
        if (!*p) break;
        if (*p++=='\t') { c++; continue; }
        if (p[-1]=='\r' && *p=='\n') p++;
        if (!*p) break;
        r++; c=0;
    }
    *rows=r+1; *columns=width; return true;
}
