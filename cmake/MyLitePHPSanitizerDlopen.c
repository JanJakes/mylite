#include <dlfcn.h>
#include <string.h>

typedef void *(*mylite_dlopen_function)(const char *filename, int flags);

static mylite_dlopen_function resolve_next_dlopen(void);

// NOLINTNEXTLINE(readability-inconsistent-declaration-parameter-name): external libc ABI.
void *dlopen(const char *filename, int flags) {
    mylite_dlopen_function next_dlopen = resolve_next_dlopen();
    if (next_dlopen == NULL) {
        return NULL;
    }

    return next_dlopen(filename, flags & ~RTLD_DEEPBIND);
}

static mylite_dlopen_function resolve_next_dlopen(void) {
    static mylite_dlopen_function next_dlopen = NULL;

    if (next_dlopen == NULL) {
        void *symbol = dlsym(RTLD_NEXT, "dlopen");
        _Static_assert(sizeof(next_dlopen) == sizeof(symbol), "dlopen pointer size mismatch");
        memcpy((void *)&next_dlopen, (const void *)&symbol, sizeof(next_dlopen));
    }

    return next_dlopen;
}
