#include <mylite_fork/mylite_sqlite_fork.h>

#include "sqlite3.h"

#include <stdio.h>

static int expect_sqlite_ok(int rc, sqlite3 *database, const char *context);

int main(void) {
    enum { expected_sqlite_version_number = 3053000 };

    sqlite3 *database = NULL;
    int failures = 0;

    if (sqlite3_libversion_number() != expected_sqlite_version_number) {
        fprintf(
            stderr,
            "expected SQLite version number 3053000, got %d\n",
            sqlite3_libversion_number()
        );
        ++failures;
    }

    failures += expect_sqlite_ok(
        sqlite3_open_v2(
            ":memory:",
            &database,
            SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_MEMORY,
            NULL
        ),
        database,
        "open source-tree SQLite fork"
    );
    if (failures != 0) {
        sqlite3_close(database);
        return failures;
    }

    failures += expect_sqlite_ok(
        mylite_sqlite_fork_configure(database),
        database,
        "configure MyLite fork primitives"
    );
    failures += expect_sqlite_ok(
        sqlite3_exec(
            database,
            "CREATE TABLE t(value TEXT COLLATE utf8mb4_unicode_ci);"
            "INSERT INTO t VALUES (_mylite_coerce_varchar(123, 3));",
            NULL,
            NULL,
            NULL
        ),
        database,
        "execute MyLite fork primitive on source-tree SQLite"
    );

    sqlite3_close(database);
    return failures;
}

static int expect_sqlite_ok(int rc, sqlite3 *database, const char *context) {
    if (rc == SQLITE_OK) {
        return 0;
    }
    fprintf(stderr, "%s: sqlite rc=%d: %s\n", context, rc, sqlite3_errmsg(database));
    return 1;
}
