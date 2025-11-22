#define SAM2_SERVER
#include "sam2.h"
#include <stdio.h>

// Top level test runner
int sam2_test_all(void) {
    int num_failed = 0;

    return num_failed;
}

#ifdef SAM2_TEST_MAIN
int main(void) {
    int num_failed = sam2_test_all();
    if (num_failed != 0) {
        fprintf(stderr, __FILE__ ":%d: Total tests failed: %d\n", __LINE__, num_failed);
    }

    return num_failed > 0;
}
#endif

#ifdef SAM2_EXECUTABLE
// Secret knowledge hidden within libuv's test folder
#define ASSERT(expr) if (!(expr)) exit(69);
static void close_walk_cb(uv_handle_t* handle, void* arg) {
    if (!uv_is_closing(handle)) {
        uv_close(handle, NULL);
    }
}

static void close_loop(uv_loop_t* loop) {
    uv_walk(loop, close_walk_cb, NULL);
    uv_run(loop, UV_RUN_DEFAULT);
}

/* This macro cleans up the event loop. This is used to avoid valgrind
 * warnings about memory being "leaked" by the event loop.
 */
#define MAKE_VALGRIND_HAPPY(loop)                   \
  do {                                              \
    close_loop(loop);                               \
    ASSERT(0 == uv_loop_close(loop));               \
    uv_library_shutdown();                          \
  } while (0)

static void on_signal(uv_signal_t *handle, int signum) {
    sam2_server_begin_destroy((sam2_server_t *) handle->data);
    uv_close((uv_handle_t*) handle->data, NULL);
}

int main() {
    sam2_server_t server;

    int ret = sam2_server_init(&server, SAM2_SERVER_DEFAULT_PORT);

    if (ret < 0) {
        fprintf(stderr, __FILE__ ":%d: Error while initializing server", __LINE__);
        return ret;
    }

    // Setup signal handler
    uv_signal_t sig;
    uv_signal_init(&server.loop, &sig);
    sig.data = &server;
    uv_signal_start(&sig, on_signal, SIGINT);

    uv_run(&server.loop, UV_RUN_DEFAULT);

    MAKE_VALGRIND_HAPPY(&server.loop);

    return 0;
}
#endif
