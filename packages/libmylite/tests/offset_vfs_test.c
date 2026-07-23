#include "mylite_test_support.h"

#include "storage/mylite_file_open.h"

#include "sqlite3.h"

#include <stdio.h>

static int test_environment_callbacks_are_forwarded(void);

int main(void) {
    return test_environment_callbacks_are_forwarded() == 0 ? 0 : 1;
}

static int test_environment_callbacks_are_forwarded(void) {
    enum { random_byte_count = 16 };

    sqlite3_vfs *vfs = NULL;
    char random_bytes[random_byte_count] = {0};
    double current_time = 0.0;
    int failures = 0;

    failures += mylite_test_expect_int(
        mylite_storage_vfs_ensure_registered(),
        MYLITE_OK,
        "register offset VFS"
    );
    vfs = sqlite3_vfs_find(mylite_storage_vfs_name());
    failures += mylite_test_expect_true(vfs != NULL, "find offset VFS");
    if (vfs == NULL) {
        return failures;
    }

    failures += mylite_test_expect_int(
        vfs->xRandomness(vfs, random_byte_count, random_bytes),
        random_byte_count,
        "forward randomness"
    );
    failures += mylite_test_expect_true(vfs->xSleep(vfs, 0) >= 0, "forward sleep");
    failures += mylite_test_expect_int(
        vfs->xCurrentTime(vfs, &current_time),
        SQLITE_OK,
        "forward current time"
    );
    failures += mylite_test_expect_true(current_time > 0.0, "current time is populated");

    return failures;
}
