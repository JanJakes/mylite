#include "sqlite3.h"

#include <stdio.h>
#include <string.h>

static int test_sqlite_version(void);
static int test_open_memory_database(void);

int main(void) {
    int failures = 0;

    failures += test_sqlite_version();
    failures += test_open_memory_database();

    return failures == 0 ? 0 : 1;
}

static int test_sqlite_version(void) {
    if (sqlite3_libversion_number() != SQLITE_VERSION_NUMBER) {
        fprintf(
            stderr,
            "expected SQLite version number %d, got %d\n",
            SQLITE_VERSION_NUMBER,
            sqlite3_libversion_number()
        );
        return 1;
    }

    if (strcmp(sqlite3_libversion(), SQLITE_VERSION) != 0) {
        fprintf(
            stderr,
            "expected SQLite version %s, got %s\n",
            SQLITE_VERSION,
            sqlite3_libversion()
        );
        return 1;
    }

    return 0;
}

static int test_open_memory_database(void) {
    sqlite3 *database = NULL;
    int rc = sqlite3_open(":memory:", &database);

    if (rc != SQLITE_OK) {
        fprintf(
            stderr,
            "failed to open SQLite memory database: %s\n",
            database == NULL ? "out of memory" : sqlite3_errmsg(database)
        );
        sqlite3_close(database);
        return 1;
    }

    rc = sqlite3_close(database);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "failed to close SQLite memory database: %d\n", rc);
        return 1;
    }

    return 0;
}
