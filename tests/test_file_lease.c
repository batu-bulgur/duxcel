#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "platform/file_lease.h"
#include "common.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL recovery lease:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
int main(void) {
    char directory[]="build/tests/file-lease-XXXXXX"; CHECK(mkdtemp(directory));
    char *path=dx_printf("%s/lease.lock",directory),*alias=dx_printf("%s/alias.lock",directory);
    DxFileLease *lease=NULL,*other=NULL; char *error=NULL;
    CHECK(dx_file_lease_acquire(path,false,&lease,&error)==DX_FILE_LEASE_ERROR && error && !lease && access(path,F_OK)); free(error); error=NULL;
    CHECK(dx_file_lease_acquire(path,true,&lease,&error)==DX_FILE_LEASE_ACQUIRED && lease && !error);
    CHECK(dx_file_lease_acquire(path,false,&other,&error)==DX_FILE_LEASE_BUSY && !other && !error);
    DxFileLease *same=lease; CHECK(dx_file_lease_acquire(path,true,&lease,&error)==DX_FILE_LEASE_ERROR && error && lease==same); free(error); error=NULL;
    CHECK(!link(path,alias)); CHECK(dx_file_lease_acquire(alias,false,&other,&error)==DX_FILE_LEASE_ERROR && error && !other); free(error); error=NULL; CHECK(!unlink(alias));
    CHECK(!symlink("lease.lock",alias)); CHECK(dx_file_lease_acquire(alias,false,&other,&error)==DX_FILE_LEASE_ERROR && error && !other); free(error); error=NULL; CHECK(!unlink(alias));
    dx_file_lease_release(lease); lease=NULL; CHECK(!access(path,F_OK));
    CHECK(dx_file_lease_acquire(path,false,&lease,&error)==DX_FILE_LEASE_ACQUIRED && !error); dx_file_lease_release(lease); lease=NULL;
    int ready[2]; CHECK(!pipe(ready)); pid_t child=fork(); CHECK(child>=0);
    if (!child) {
        close(ready[0]); DxFileLease *owned=NULL; char *message=NULL;
        if (dx_file_lease_acquire(path,false,&owned,&message)!=DX_FILE_LEASE_ACQUIRED) _exit(2);
        if (write(ready[1],"r",1)!=1) _exit(3); close(ready[1]);
        for (;;) pause();
    }
    close(ready[1]); char byte=0; CHECK(read(ready[0],&byte,1)==1 && byte=='r'); close(ready[0]);
    CHECK(dx_file_lease_acquire(path,false,&other,&error)==DX_FILE_LEASE_BUSY && !other && !error);
    CHECK(!kill(child,SIGKILL)); int status; CHECK(waitpid(child,&status,0)==child && WIFSIGNALED(status) && WTERMSIG(status)==SIGKILL);
    CHECK(dx_file_lease_acquire(path,false,&other,&error)==DX_FILE_LEASE_ACQUIRED && other && !error); dx_file_lease_release(other); other=NULL;
    FILE *file=fopen(path,"wb"); CHECK(file); CHECK(fputs("preserve",file)>=0 && !fclose(file));
    CHECK(dx_file_lease_acquire(path,true,&other,&error)==DX_FILE_LEASE_ERROR && !other && error); free(error); error=NULL;
    file=fopen(path,"rb"); CHECK(file); char content[9]={0}; CHECK(fread(content,1,8,file)==8 && !strcmp(content,"preserve") && !fclose(file));
    CHECK(!unlink(path) && !rmdir(directory)); free(path); free(alias); dx_file_lease_release(NULL);
    printf("PASS: %u native recovery lease checks\n",checks); return 0;
}
