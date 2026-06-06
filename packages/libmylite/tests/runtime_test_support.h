#ifndef MYLITE_RUNTIME_TEST_SUPPORT_H
#define MYLITE_RUNTIME_TEST_SUPPORT_H

#include <mylite/mylite.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <process.h>
#else
#  include <unistd.h>
#endif

enum {
    mylite_test_temp_path_capacity = 1024,
    mylite_test_temp_path_slots = 128,
    mylite_test_temp_path_suffix_capacity = 16,
};

static char mylite_test_temp_paths[mylite_test_temp_path_slots][mylite_test_temp_path_capacity];
static size_t mylite_test_temp_path_count;
static unsigned int mylite_test_temp_path_counter;
static int mylite_test_temp_cleanup_registered;

static int mylite_test_open_temporary(mylite_db **out_database);
static int mylite_test_make_temporary_path(char *path, size_t path_size);
static int mylite_test_register_temporary_path(const char *path);
static void mylite_test_cleanup_temporary_paths(void);
static void mylite_test_remove_related_files(const char *path);
static void mylite_test_remove_with_suffix(const char *path, const char *suffix);
static int mylite_test_process_id(void);

static int mylite_test_open_temporary(mylite_db **out_database) {
    char path[mylite_test_temp_path_capacity];
    int rc = MYLITE_OK;

    if (out_database == NULL) {
        return MYLITE_MISUSE;
    }

    *out_database = NULL;
    rc = mylite_test_make_temporary_path(path, sizeof(path));
    if (rc != MYLITE_OK) {
        return rc;
    }

    mylite_test_remove_related_files(path);
    rc = mylite_open(path, out_database);
    if (rc != MYLITE_OK) {
        mylite_test_remove_related_files(path);
        return rc;
    }

    rc = mylite_test_register_temporary_path(path);
    if (rc != MYLITE_OK) {
        mylite_close(*out_database);
        *out_database = NULL;
        mylite_test_remove_related_files(path);
        return rc;
    }

    return MYLITE_OK;
}

static int mylite_test_make_temporary_path(char *path, size_t path_size) {
    int written = snprintf(
        path,
        path_size,
        "./mylite_runtime_test_%d_%u.mylite",
        mylite_test_process_id(),
        mylite_test_temp_path_counter
    );

    ++mylite_test_temp_path_counter;
    if (written < 0 || (size_t)written >= path_size) {
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int mylite_test_register_temporary_path(const char *path) {
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

static void mylite_test_remove_related_files(const char *path) {
    mylite_test_remove_with_suffix(path, "");
    mylite_test_remove_with_suffix(path, "-journal");
    mylite_test_remove_with_suffix(path, "-wal");
    mylite_test_remove_with_suffix(path, "-shm");
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

#endif
