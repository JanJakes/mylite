#include "storage/mylite_file_open.h"

#include "sqlite3.h"

#include <stdio.h>

static int test_environment_callbacks_are_forwarded(void);
static int expect_int(int actual, int expected, const char *context);
static int expect_true(int condition, const char *context);

int main(void) {
    return test_environment_callbacks_are_forwarded() == 0 ? 0 : 1;
}

static int test_environment_callbacks_are_forwarded(void) {
    enum { random_byte_count = 16 };

    sqlite3_vfs *vfs = NULL;
    char random_bytes[random_byte_count] = {0};
    double current_time = 0.0;
    int failures = 0;

    failures +=
        expect_int(mylite_storage_vfs_ensure_registered(), MYLITE_OK, "register offset VFS");
    vfs = sqlite3_vfs_find(mylite_storage_vfs_name());
    failures += expect_true(vfs != NULL, "find offset VFS");
    if (vfs == NULL) {
        return failures;
    }

    failures += expect_int(
        vfs->xRandomness(vfs, random_byte_count, random_bytes),
        random_byte_count,
        "forward randomness"
    );
    failures += expect_true(vfs->xSleep(vfs, 0) >= 0, "forward sleep");
    failures +=
        expect_int(vfs->xCurrentTime(vfs, &current_time), SQLITE_OK, "forward current time");
    failures += expect_true(current_time > 0.0, "current time is populated");

    return failures;
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
        return 1;
    }

    return 0;
}

static int expect_true(int condition, const char *context) {
    if (!condition) {
        fprintf(stderr, "%s: expected true\n", context);
        return 1;
    }

    return 0;
}
