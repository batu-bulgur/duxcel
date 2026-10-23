#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "platform/directory.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL native directory:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
int main(void) {
    char root[]="build/tests/directory-XXXXXX"; CHECK(mkdtemp(root));
    char *missing=dx_printf("%s/missing",root),*folder=dx_printf("%s/İstanbul",root),*file=dx_printf("%s/file",root),*alias=dx_printf("%s/alias",root),*hard=dx_printf("%s/hard",root);
    DxDirectory *cursor=NULL; char *error=NULL;
    CHECK(dx_directory_open(missing,&cursor,&error)==DX_DIRECTORY_MISSING && !cursor && !error && access(missing,F_OK));
    CHECK(dx_path_kind(missing)==DX_PATH_MISSING && !dx_path_independent_file(missing));
    CHECK(!mkdir(folder,0700)); FILE *f=fopen(file,"wb"); CHECK(f && fputs("preserve",f)>=0 && !fclose(f)); CHECK(!symlink("İstanbul",alias));
    CHECK(dx_path_kind(root)==DX_PATH_DIRECTORY && dx_path_kind(alias)==DX_PATH_SYMLINK && dx_path_kind(file)==DX_PATH_FILE && dx_path_independent_file(file));
    CHECK(!link(file,hard)); CHECK(!dx_path_independent_file(file) && !dx_path_independent_file(hard)); CHECK(!unlink(hard)); CHECK(dx_path_independent_file(file));
    CHECK(dx_directory_open(file,&cursor,&error)==DX_DIRECTORY_ERROR && error && !cursor); free(error); error=NULL;
    CHECK(dx_directory_open(root,&cursor,&error)==DX_DIRECTORY_OPEN && cursor && !error);
    DxDirectory *same=cursor; CHECK(dx_directory_open(root,&cursor,&error)==DX_DIRECTORY_ERROR && error && cursor==same); free(error); error=NULL;
    bool seen_file=false,seen_folder=false,seen_alias=false; unsigned entries=0; DxDirectoryEntry entry={0}; DxDirectoryNext next;
    while ((next=dx_directory_next(cursor,&entry,&error))==DX_DIRECTORY_ENTRY) {
        entries++;
        if (!strcmp(entry.name,"file")) { CHECK(entry.kind==DX_PATH_FILE); seen_file=true; }
        else if (!strcmp(entry.name,"İstanbul")) { CHECK(entry.kind==DX_PATH_DIRECTORY); seen_folder=true; }
        else if (!strcmp(entry.name,"alias")) { CHECK(entry.kind==DX_PATH_SYMLINK); seen_alias=true; }
        else CHECK(false);
    }
    CHECK(next==DX_DIRECTORY_END && !error && entries==3 && seen_file && seen_folder && seen_alias);
    CHECK(dx_directory_next(cursor,&entry,&error)==DX_DIRECTORY_END); dx_directory_close(cursor); dx_directory_close(NULL);
    CHECK(!unlink(alias) && !unlink(file) && !rmdir(folder) && !rmdir(root)); free(missing); free(folder); free(file); free(alias); free(hard);
    printf("PASS: %u streaming native directory checks\n",checks); return 0;
}
