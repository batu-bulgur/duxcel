#ifdef __APPLE__
#define _DARWIN_C_SOURCE 1
#else
#define _POSIX_C_SOURCE 200809L
#endif
#include "dev/review_store.h"
#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static unsigned checks;
#define CHECK(condition) do { checks++; if (!(condition)) { fprintf(stderr,"FAIL review_store:%d: %s\n",__LINE__,#condition); exit(1); } } while (0)
static const unsigned char png[8]={137,80,78,71,13,10,26,10};
static const char manifest[]="{\"schema\":\"duxcel.native-review\",\"version\":1,\"report_id\":\"DXR-one\"}";
static void joined(char output[4096],const char *root,const char *suffix) {
    int n=snprintf(output,4096,"%s/%s",root,suffix); CHECK(n>0 && n<4096);
}
static unsigned entries(const char *path) {
    DIR *directory=opendir(path); CHECK(directory!=NULL); unsigned count=0; struct dirent *entry;
    while ((entry=readdir(directory))) if (strcmp(entry->d_name,".") && strcmp(entry->d_name,"..")) count++;
    closedir(directory); return count;
}
static void remove_bundle(const char *root,const char *id) {
    char path[4096],file[4096]; joined(path,root,id); joined(file,path,"manifest.json"); CHECK(!unlink(file));
    joined(file,path,"frame.png"); CHECK(!unlink(file)); CHECK(!rmdir(path));
}
static void check_manifest(const char *root,const char *id) {
    char path[4096],file[4096],buffer[sizeof(manifest)+1]; joined(path,root,id); joined(file,path,"manifest.json");
    FILE *stream=fopen(file,"rb"); CHECK(stream!=NULL); size_t n=fread(buffer,1,sizeof(buffer),stream); fclose(stream);
    CHECK(n==strlen(manifest) && !memcmp(buffer,manifest,n));
    struct stat info; CHECK(!stat(file,&info) && (info.st_mode&0777)==0600);
    joined(file,path,"frame.png"); stream=fopen(file,"rb"); CHECK(stream!=NULL);
    unsigned char bytes[9]; n=fread(bytes,1,sizeof(bytes),stream); fclose(stream); CHECK(n==8 && !memcmp(bytes,png,8));
    CHECK(!stat(path,&info) && (info.st_mode&0777)==0700);
}
int main(void) {
    char base[]="/tmp/duxcel-review-store-XXXXXX"; CHECK(mkdtemp(base)!=NULL);
    char root[4096],path[4096],error[256],selected[4096]; joined(root,base,"nested/outbox");
    const char *old_override=getenv("DUXCEL_REVIEW_OUTBOX"),*old_home=getenv("HOME");
    char *saved_override=old_override ? strdup(old_override) : NULL,*saved_home=old_home ? strdup(old_home) : NULL;
    CHECK(!setenv("DUXCEL_REVIEW_OUTBOX",root,1));
    CHECK(dx_review_store_default_root("project_A-1",selected,sizeof(selected),error,sizeof(error)) && !strcmp(selected,root) && !*error);
    CHECK(!dx_review_store_default_root("../project",selected,sizeof(selected),error,sizeof(error)) && *error && !*selected);
    CHECK(!dx_review_store_default_root("project",selected,4,error,sizeof(error)) && *error);
    CHECK(!setenv("DUXCEL_REVIEW_OUTBOX","relative/outbox",1));
    CHECK(!dx_review_store_default_root("project",selected,sizeof(selected),error,sizeof(error)) && *error);
    CHECK(!unsetenv("DUXCEL_REVIEW_OUTBOX")); CHECK(!setenv("HOME",base,1));
    CHECK(dx_review_store_default_root("project",selected,sizeof(selected),error,sizeof(error)) && strstr(selected,"/Duxcel/Review/project"));
    CHECK(!setenv("HOME","relative",1));
#ifdef __APPLE__
    CHECK(!dx_review_store_default_root("project",selected,sizeof(selected),error,sizeof(error)));
#else
    if (!getenv("XDG_DATA_HOME")) CHECK(!dx_review_store_default_root("project",selected,sizeof(selected),error,sizeof(error)));
#endif
    if (saved_home) { CHECK(!setenv("HOME",saved_home,1)); free(saved_home); } else CHECK(!unsetenv("HOME"));
    if (saved_override) { CHECK(!setenv("DUXCEL_REVIEW_OUTBOX",saved_override,1)); free(saved_override); } else CHECK(!unsetenv("DUXCEL_REVIEW_OUTBOX"));

    CHECK(!dx_review_store_write("relative","DXR-one",manifest,png,8,error,sizeof(error)) && *error);
    const char *bad_ids[]={"",".","..","../outside","report/child","report\\child","report.json","report name","r\xc3\xa9port"};
    for (size_t i=0;i<sizeof(bad_ids)/sizeof(*bad_ids);i++) CHECK(!dx_review_store_write(root,bad_ids[i],manifest,png,8,error,sizeof(error)) && *error);
    char oversized[97]; memset(oversized,'a',96); oversized[96]=0;
    CHECK(!dx_review_store_write(root,oversized,manifest,png,8,error,sizeof(error)));
    CHECK(!dx_review_store_write(root,"DXR-one","",png,8,error,sizeof(error)));
    CHECK(!dx_review_store_write(root,"DXR-one",manifest,png,7,error,sizeof(error)));
    unsigned char bad_png[8]={0}; CHECK(!dx_review_store_write(root,"DXR-one",manifest,bad_png,8,error,sizeof(error)));
    CHECK(!dx_review_store_write(root,"DXR-one",manifest,png,(size_t)DX_REVIEW_STORE_PNG_MAX+1,error,sizeof(error)));
    char *large=malloc((size_t)DX_REVIEW_STORE_JSON_MAX+1); CHECK(large!=NULL);
    memset(large,'x',DX_REVIEW_STORE_JSON_MAX); large[DX_REVIEW_STORE_JSON_MAX]=0;
    CHECK(!dx_review_store_write(root,"DXR-one",large,png,8,error,sizeof(error))); free(large);
    CHECK(access(root,F_OK)!=0); /* invalid payloads perform no filesystem work */

    CHECK(dx_review_store_write(root,"DXR-one",manifest,png,8,error,sizeof(error)) && !*error);
    CHECK(entries(root)==1); check_manifest(root,"DXR-one");
    CHECK(!dx_review_store_write(root,"DXR-one","replacement",png,8,error,sizeof(error)) && *error);
    check_manifest(root,"DXR-one");
    joined(path,root,"DXR-empty"); CHECK(!mkdir(path,0700));
    CHECK(!dx_review_store_write(root,"DXR-empty",manifest,png,8,error,sizeof(error)));
    CHECK(entries(path)==0); CHECK(!rmdir(path));
    joined(path,root,"DXR-link"); CHECK(!symlink(base,path));
    CHECK(!dx_review_store_write(root,"DXR-link",manifest,png,8,error,sizeof(error))); CHECK(!unlink(path));
    char link_root[4096]; joined(link_root,base,"linked-outbox"); CHECK(!symlink(root,link_root));
    CHECK(!dx_review_store_write(link_root,"DXR-unsafe",manifest,png,8,error,sizeof(error))); CHECK(!unlink(link_root));
    joined(path,base,"plain-file"); FILE *file=fopen(path,"wb"); CHECK(file!=NULL); fclose(file);
    CHECK(!dx_review_store_write(path,"DXR-file",manifest,png,8,error,sizeof(error))); CHECK(!unlink(path));

    if (geteuid()!=0) {
        CHECK(!chmod(root,0500)); CHECK(!dx_review_store_write(root,"DXR-denied",manifest,png,8,error,sizeof(error)) && *error);
        CHECK(!chmod(root,0700)); CHECK(entries(root)==1);
    }
    /* A real short write after the manifest was flushed must remove only the
       current private stage, retaining both earlier reports and abandoned work. */
    joined(path,root,".tmp-abandoned"); CHECK(!mkdir(path,0700));
    struct rlimit before; CHECK(!getrlimit(RLIMIT_FSIZE,&before)); struct rlimit limit=before; limit.rlim_cur=512;
    void (*handler)(int)=signal(SIGXFSZ,SIG_IGN); CHECK(handler!=SIG_ERR); CHECK(!setrlimit(RLIMIT_FSIZE,&limit));
    unsigned char payload[2048]={137,80,78,71,13,10,26,10};
    CHECK(!dx_review_store_write(root,"DXR-write-failure",manifest,payload,sizeof(payload),error,sizeof(error)) && *error);
    CHECK(!setrlimit(RLIMIT_FSIZE,&before)); CHECK(signal(SIGXFSZ,handler)!=SIG_ERR);
    CHECK(entries(root)==2 && entries(path)==0); CHECK(!rmdir(path)); check_manifest(root,"DXR-one");

    pid_t a=fork(); CHECK(a>=0); if (!a) _exit(dx_review_store_write(root,"DXR-race",manifest,png,8,error,sizeof(error)) ? 0 : 10);
    pid_t b=fork(); CHECK(b>=0); if (!b) _exit(dx_review_store_write(root,"DXR-race",manifest,png,8,error,sizeof(error)) ? 0 : 10);
    int first,second; CHECK(waitpid(a,&first,0)==a && waitpid(b,&second,0)==b);
    CHECK(WIFEXITED(first) && WIFEXITED(second)); CHECK((WEXITSTATUS(first)==0 && WEXITSTATUS(second)==10) || (WEXITSTATUS(first)==10 && WEXITSTATUS(second)==0));
    CHECK(entries(root)==2); check_manifest(root,"DXR-race");
    remove_bundle(root,"DXR-race"); remove_bundle(root,"DXR-one"); CHECK(!rmdir(root));
    joined(path,base,"nested"); CHECK(!rmdir(path)); CHECK(!rmdir(base));
    printf("PASS: %u review-store checks (atomic durable publication, no overwrite, bounded payloads, failure cleanup and concurrent IDs)\n",checks);
    return 0;
}
