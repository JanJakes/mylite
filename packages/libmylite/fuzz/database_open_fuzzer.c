#include <mylite/mylite.h>

#include "mylite_fuzzer.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

enum { path_capacity = 256 };

static int write_input(const char *path, const uint8_t *data, size_t size);
static void remove_database_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    char path[path_capacity];
    mylite_db *database = NULL;
    int length = snprintf(path, sizeof(path), "/tmp/mylite-open-fuzzer-%ld.mylite", (long)getpid());

    if (length < 0 || (size_t)length >= sizeof(path)) {
        return 0;
    }

    remove_database_files(path);
    if (write_input(path, data, size) == 0 && mylite_open(path, &database) == MYLITE_OK) {
        mylite_close(database);
    }
    remove_database_files(path);
    return 0;
}

static int write_input(const char *path, const uint8_t *data, size_t size) {
    FILE *file = fopen(path, "wb");
    int rc = 0;

    if (file == NULL) {
        return -1;
    }
    if (size > 0U && fwrite(data, 1U, size, file) != size) {
        rc = -1;
    }
    if (fclose(file) != 0) {
        rc = -1;
    }
    return rc;
}

static void remove_database_files(const char *path) {
    (void)remove(path);
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char suffixed_path[path_capacity];
    int length = snprintf(suffixed_path, sizeof(suffixed_path), "%s%s", path, suffix);

    if (length >= 0 && (size_t)length < sizeof(suffixed_path)) {
        (void)remove(suffixed_path);
    }
}
