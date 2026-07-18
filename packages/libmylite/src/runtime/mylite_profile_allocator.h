#ifndef MYLITE_RUNTIME_MYLITE_PROFILE_ALLOCATOR_H
#define MYLITE_RUNTIME_MYLITE_PROFILE_ALLOCATOR_H

#include <stddef.h>
#include <stdlib.h>

void *mylite_profile_malloc(size_t size);
void *mylite_profile_calloc(size_t count, size_t size);
void *mylite_profile_realloc(void *allocation, size_t size);
void mylite_profile_free(void *allocation);

#ifndef MYLITE_PROFILE_ALLOCATOR_IMPLEMENTATION
#  define malloc mylite_profile_malloc
#  define calloc mylite_profile_calloc
#  define realloc mylite_profile_realloc
#  define free mylite_profile_free
#endif

#endif
