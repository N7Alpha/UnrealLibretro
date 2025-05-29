#define SAM2_SERVER
#include "sam2.h"

#define SAM2__TEST_LIST_CAPACITY 3
#define SAM2__TEST_LIST_SIZE (SAM2__TEST_LIST_CAPACITY+1)

typedef struct sam2__test_container {
    sam2__pool_t pool;
    sam2__pool_node_t next[SAM2__TEST_LIST_SIZE];
} sam2__test_container_t;

// Helper macro to check a condition and print an error if it fails.
#define TEST_ASSERT(cond, message)       \
    do {                                 \
        if (!(cond)) {                   \
            fprintf(stderr, "FAIL: %s\n", message); \
            test_failed_local = 1;       \
        }                                \
    } while (0)

// 1) One allocation and free.
int sam2__test_one_allocation_and_free(void) {
    int test_failed_local = 0;

    sam2__test_container_t container;
    memset(&container, 0, sizeof(container));
    sam2__pool_init(&container.pool, SAM2__TEST_LIST_SIZE);

    // Allocate 1 element
    uint16_t idx = sam2__pool_alloc(&container.pool);
    TEST_ASSERT(idx != SAM2__INDEX_NULL, "Expected allocation to succeed, got LIST_NULL instead.");
    TEST_ASSERT(container.pool.used == 1, "Used count should be 1 after first allocation.");

    // Free it
    const char *err = sam2__pool_free(&container.pool, idx);
    TEST_ASSERT(err == NULL, "Expected free to succeed, got an error instead.");
    TEST_ASSERT(container.pool.used == 0, "Used count should be 0 after freeing.");

    return test_failed_local;
}

// 2) Out-of-memory check. We only have 3 spots. Allocate them all, then one extra.
int sam2__test_out_of_memory(void) {
    int test_failed_local = 0;

    sam2__test_container_t container;
    memset(&container, 0, sizeof(container));
    sam2__pool_init(&container.pool, SAM2__TEST_LIST_SIZE);

    // We have 3 total. Allocate 3 times.
    uint16_t idx1 = sam2__pool_alloc(&container.pool);
    uint16_t idx2 = sam2__pool_alloc(&container.pool);
    uint16_t idx3 = sam2__pool_alloc(&container.pool);

    TEST_ASSERT(idx1 != SAM2__INDEX_NULL && idx2 != SAM2__INDEX_NULL && idx3 != SAM2__INDEX_NULL,
        "Allocations should succeed (3 total).");
    TEST_ASSERT(container.pool.used == 3, "Used count should be 3 after allocating 3 elements.");

    // Now the next allocation should return SAM2__INDEX_NULL.
    uint16_t idx4 = sam2__pool_alloc(&container.pool);
    TEST_ASSERT(idx4 == SAM2__INDEX_NULL, "Expected out-of-memory (LIST_NULL) for the 4th allocation.");

    // Cleanup: free them all so we don't leave them allocated.
    sam2__pool_free(&container.pool, idx1);
    sam2__pool_free(&container.pool, idx2);
    sam2__pool_free(&container.pool, idx3);
    TEST_ASSERT(container.pool.used == 0, "Used count should be 0 after freeing everything.");

    return test_failed_local;
}

// 3) Full allocation and full deallocation.
//    Allocate all resources and ensure that after deallocation the allocator
//    is restored to its initial state (thus we can re-allocate again).
int sam2__test_full_allocation_and_deallocation(void) {
    int test_failed_local = 0;

    sam2__test_container_t container;
    memset(&container, 0, sizeof(container));
    sam2__pool_init(&container.pool, SAM2__TEST_LIST_SIZE);

    // Allocate all (3)
    uint16_t idx[3];
    for (int i = 0; i < SAM2__TEST_LIST_CAPACITY; i++) {
        idx[i] = sam2__pool_alloc(&container.pool);
        TEST_ASSERT(idx[i] != SAM2__INDEX_NULL, "Allocation failed unexpectedly.");
    }
    TEST_ASSERT(container.pool.used == 3,
        "Used count should be 3 after allocating all resources.");

    // Free all
    for (int i = 0; i < SAM2__TEST_LIST_CAPACITY; i++) {
        const char* err = sam2__pool_free(&container.pool, idx[i]);
        TEST_ASSERT(err == NULL, "Free returned an error unexpectedly.");
    }
    TEST_ASSERT(container.pool.used == 0,
        "Used count should be 0 after freeing everything.");

    // Check that we can allocate again (meaning the state was restored).
    uint16_t idx_new = sam2__pool_alloc(&container.pool);
    TEST_ASSERT(idx_new != SAM2__INDEX_NULL, "Allocation after full free should succeed.");
    TEST_ASSERT(container.pool.used == 1, "Used count should be 1 after new allocation.");

    // Cleanup
    sam2__pool_free(&container.pool, idx_new);

    return test_failed_local;
}

// 4) Test alloc_at_index. We have the precondition that index must be free.
//    We'll try to allocate normally, free it, then explicitly call alloc_at_index
//    for that index. Also try to allocate_at_index out of range.
int sam2__test_alloc_at_index(void) {
    int test_failed_local = 0;

    sam2__test_container_t container;
    memset(&container, 0, sizeof(container));
    sam2__pool_init(&container.pool, SAM2__TEST_LIST_SIZE);

    // Grab one normally.
    uint16_t idx_normal = sam2__pool_alloc(&container.pool);
    TEST_ASSERT(idx_normal != SAM2__INDEX_NULL, "Normal allocation should succeed.");

    // Now free it.
    const char *err = sam2__pool_free(&container.pool, idx_normal);
    TEST_ASSERT(err == NULL, "Free should succeed for a valid index.");

    // Now we know idx_normal is free again. Let's allocate at that exact index.
    uint16_t idx_allocated_at = sam2__pool_alloc_at_index(&container.pool, idx_normal);
    TEST_ASSERT(idx_allocated_at == idx_normal,
        "Expected to re-allocate at the same index we freed, but got a different index or LIST_NULL.");
    TEST_ASSERT(container.pool.used == 1,
        "Used count should be 1 after alloc_at_index.");

    // Cleanup
    err = sam2__pool_free(&container.pool, idx_allocated_at);
    TEST_ASSERT(err == NULL, "Free should succeed at the end.");

    return test_failed_local;
}

// 5) Test free and re-allocate. Possibly interleaved: allocate some, free some,
//    allocate again. The main check is that the used/freelist is consistent.
int sam2__test_free_and_realloc(void) {
    int test_failed_local = 0;

    sam2__test_container_t container;
    memset(&container, 0, sizeof(container));
    sam2__pool_init(&container.pool, SAM2__TEST_LIST_SIZE);

    // Allocate 2 of the 3 available.
    uint16_t idx1 = sam2__pool_alloc(&container.pool);
    uint16_t idx2 = sam2__pool_alloc(&container.pool);

    TEST_ASSERT(idx1 != SAM2__INDEX_NULL && idx2 != SAM2__INDEX_NULL, "Expected first 2 allocations to succeed.");
    TEST_ASSERT(container.pool.used == 2, "Used count should be 2 after 2 allocs.");

    // Free the first one.
    const char *err = sam2__pool_free(&container.pool, idx1);
    TEST_ASSERT(err == NULL, "Freeing idx1 should succeed.");
    TEST_ASSERT(container.pool.used == 1, "Used count should drop to 1 after free.");

    // Now allocate a third one. It's possible that the newly freed index1 is reused or
    // we get the last slot. We do not rely on the order. Only that it succeeds.
    uint16_t idx3 = sam2__pool_alloc(&container.pool);
    TEST_ASSERT(idx3 != SAM2__INDEX_NULL, "Expected the 3rd allocation to succeed since we freed idx1.");
    TEST_ASSERT(container.pool.used == 2, "Used count should be 2 again after the 3rd allocation.");

    // Cleanup: free the remaining.
    sam2__pool_free(&container.pool, idx2);
    sam2__pool_free(&container.pool, idx3);
    TEST_ASSERT(container.pool.used == 0,
        "Used count should be 0 after final free of idx2 and idx3.");

    return test_failed_local;
}

// Top level test runner
int sam2_test_all(void) {
    int num_failed = 0;

    num_failed += sam2__test_one_allocation_and_free();
    num_failed += sam2__test_out_of_memory();
    num_failed += sam2__test_full_allocation_and_deallocation();
    num_failed += sam2__test_alloc_at_index();
    num_failed += sam2__test_free_and_realloc();

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
