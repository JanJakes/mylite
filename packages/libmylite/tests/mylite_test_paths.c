#include "mylite_test_support.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <process.h>
#else
#  include <unistd.h>
#endif

enum {
    mylite_test_temp_path_slots = 128,
    mylite_test_temp_path_suffix_capacity = 16,
};

static char mylite_test_temp_paths[mylite_test_temp_path_slots][mylite_test_temp_path_capacity];
static size_t mylite_test_temp_path_count;
static unsigned int mylite_test_temp_path_counter;
static int mylite_test_temp_cleanup_registered;

static void mylite_test_cleanup_temporary_paths(void);
static void mylite_test_remove_with_suffix(const char *path, const char *suffix);
static int mylite_test_process_id(void);
static const char *mylite_test_temp_directory(void);

int mylite_test_make_path(char *path, size_t path_size, const char *name) {
    const char *effective_name = name == NULL || name[0] == '\0' ? "database" : name;
    int written = 0;

    if (path == NULL || path_size == 0U) {
        return 1;
    }

    written = snprintf(
        path,
        path_size,
        "%s/mylite_test_%d_%u_%s.mylite",
        mylite_test_temp_directory(),
        mylite_test_process_id(),
        mylite_test_temp_path_counter,
        effective_name
    );
    ++mylite_test_temp_path_counter;
    if (written < 0 || (size_t)written >= path_size) {
        return 1;
    }

    return 0;
}

int mylite_test_make_default_path(char *path, size_t path_size) {
    return mylite_test_make_path(path, path_size, "database");
}

int mylite_test_make_path_with_suffix(
    char *path,
    size_t path_size,
    const char *name,
    const char *suffix
) {
    char combined_name[256];
    int written = snprintf(
        combined_name,
        sizeof(combined_name),
        "%s%s",
        name == NULL ? "database" : name,
        suffix == NULL ? "" : suffix
    );

    if (written < 0 || (size_t)written >= sizeof(combined_name)) {
        return 1;
    }

    return mylite_test_make_path(path, path_size, combined_name);
}

void mylite_test_remove_related_files(const char *path) {
    if (path == NULL) {
        return;
    }

    mylite_test_remove_with_suffix(path, "");
    mylite_test_remove_with_suffix(path, "-journal");
    mylite_test_remove_with_suffix(path, "-wal");
    mylite_test_remove_with_suffix(path, "-shm");
}

int mylite_test_register_temporary_path(const char *path) {
    int written = 0;

    if (!mylite_test_temp_cleanup_registered) {
        if (atexit(mylite_test_cleanup_temporary_paths) != 0) {
            return MYLITE_ERROR;
        }
        mylite_test_temp_cleanup_registered = 1;
    }

    if (mylite_test_temp_path_count >= mylite_test_temp_path_slots) {
        return MYLITE_ERROR;
    }

    written = snprintf(
        mylite_test_temp_paths[mylite_test_temp_path_count],
        sizeof(mylite_test_temp_paths[mylite_test_temp_path_count]),
        "%s",
        path
    );
    if (written < 0 ||
        (size_t)written >= sizeof(mylite_test_temp_paths[mylite_test_temp_path_count])) {
        return MYLITE_ERROR;
    }

    ++mylite_test_temp_path_count;
    return MYLITE_OK;
}

static void mylite_test_cleanup_temporary_paths(void) {
    size_t i = 0U;

    for (i = 0U; i < mylite_test_temp_path_count; ++i) {
        mylite_test_remove_related_files(mylite_test_temp_paths[i]);
    }
}

static void mylite_test_remove_with_suffix(const char *path, const char *suffix) {
    char related_path[mylite_test_temp_path_capacity + mylite_test_temp_path_suffix_capacity];
    int written = snprintf(related_path, sizeof(related_path), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related_path)) {
        return;
    }

    (void)remove(related_path);
}

static int mylite_test_process_id(void) {
#ifdef _WIN32
    return _getpid();
#else
    return getpid();
#endif
}

static const char *mylite_test_temp_directory(void) {
    const char *directory = getenv("TMPDIR");

    if (directory == NULL || directory[0] == '\0') {
        directory = getenv("TEMP");
    }
    if (directory == NULL || directory[0] == '\0') {
#ifdef _WIN32
        directory = ".";
#else
        directory = "/tmp";
#endif
    }

    return directory;
}
