#include "mylite_test_allocator.h"

#ifdef _MSC_VER
#  define MYLITE_TEST_THREAD_LOCAL __declspec(thread)
#else
#  define MYLITE_TEST_THREAD_LOCAL _Thread_local
#endif

struct mylite_test_allocator_state {
    size_t successful_allocations_before_failure;
    bool enabled;
    bool triggered;
};

static bool mylite_test_allocator_should_fail(void);

static MYLITE_TEST_THREAD_LOCAL struct mylite_test_allocator_state allocator_state = {0};

void mylite_test_allocator_fail_after(size_t successful_allocations) {
    allocator_state.successful_allocations_before_failure = successful_allocations;
    allocator_state.enabled = true;
    allocator_state.triggered = false;
}

void mylite_test_allocator_clear(void) {
    allocator_state = (struct mylite_test_allocator_state){0};
}

bool mylite_test_allocator_was_triggered(void) {
    return allocator_state.triggered;
}

void *mylite_test_malloc(size_t size) {
    return mylite_test_allocator_should_fail() ? NULL : malloc(size);
}

void *mylite_test_calloc(size_t count, size_t size) {
    return mylite_test_allocator_should_fail() ? NULL : calloc(count, size);
}

void *mylite_test_realloc(void *allocation, size_t size) {
    return mylite_test_allocator_should_fail() ? NULL : realloc(allocation, size);
}

void mylite_test_free(void *allocation) {
    free(allocation);
}

static bool mylite_test_allocator_should_fail(void) {
    if (!allocator_state.enabled) {
        return false;
    }
    if (allocator_state.successful_allocations_before_failure != 0U) {
        --allocator_state.successful_allocations_before_failure;
        return false;
    }

    allocator_state.enabled = false;
    allocator_state.triggered = true;
    return true;
}
