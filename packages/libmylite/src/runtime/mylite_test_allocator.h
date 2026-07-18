#ifndef MYLITE_RUNTIME_MYLITE_TEST_ALLOCATOR_H
#define MYLITE_RUNTIME_MYLITE_TEST_ALLOCATOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

void mylite_test_allocator_fail_after(size_t successful_allocations);
void mylite_test_allocator_clear(void);
bool mylite_test_allocator_was_triggered(void);

void *mylite_test_malloc(size_t size);
void *mylite_test_calloc(size_t count, size_t size);
void *mylite_test_realloc(void *allocation, size_t size);
void mylite_test_free(void *allocation);

#ifndef MYLITE_TEST_ALLOCATOR_IMPLEMENTATION
#  define malloc mylite_test_malloc
#  define calloc mylite_test_calloc
#  define realloc mylite_test_realloc
#  define free mylite_test_free
#endif

#endif
