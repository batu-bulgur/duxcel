/* Real renderer/worker ownership test; the runner owns the desktop lock. */
#include "app/app.h"
#include "common.h"
#include "platform/platform.h"
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __APPLE__
bool dx_test_session_native_input(void);
#endif
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL: lifecycle line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
int main(void) {
    CHECK(app_start(NULL)==NULL);
    CHECK(!app_frame(NULL));
    CHECK(app_finish(NULL)==1);
    const char *image_path="build/tests/results/lifecycle-owned.png";
    remove(image_path);
    char *path=dx_strdup("tests/fixtures/null-text.csv"),*image=dx_strdup(image_path);
    AppOptions options={.path=path,.screenshot=image,.frames=3};
    AppSession *session=app_start(&options); CHECK(session!=NULL);
    CHECK(IsWindowState(FLAG_WINDOW_HIDDEN)); CHECK(!IsWindowFocused());
#ifdef __APPLE__
    CHECK(dx_test_session_native_input());
#endif
    CHECK(app_start(&options)==NULL);
    /* The host's command line/stack is gone before the first scheduled frame. */
    memset(path,'x',strlen(path)); memset(image,'x',strlen(image));
    free(path); free(image); memset(&options,0,sizeof(options));
    unsigned callbacks=0;
    while (app_frame(session)) {
        callbacks++;
        CHECK(callbacks<=3);
        dx_sleep_ms(2); /* Stand in for host event-loop work between ticks. */
    }
    CHECK(callbacks==3); CHECK(!app_frame(session)); CHECK(!app_frame(session));
    CHECK(app_finish(session)==0); CHECK(!IsWindowReady());
    CHECK(FileExists(image_path));
    Image captured=LoadImage(image_path);
    CHECK(captured.data!=NULL); CHECK(captured.width>0 && captured.height>0);
    UnloadImage(captured); remove(image_path);
    /* Early host shutdown still joins a running/pending query and frees pages. */
    options=(AppOptions){.path="tests/fixtures/null-text.csv",.frames=120};
    session=app_start(&options); CHECK(session!=NULL);
    CHECK(IsWindowState(FLAG_WINDOW_HIDDEN)); CHECK(!IsWindowFocused());
#ifdef __APPLE__
    CHECK(dx_test_session_native_input());
#endif
    CHECK(app_frame(session)); CHECK(app_finish(session)==0); CHECK(!IsWindowReady());
    printf("PASS: %u App lifecycle checks (owned options, yielded frames, stable completion, early worker shutdown, hidden windows)\n",checks);
    return 0;
}
