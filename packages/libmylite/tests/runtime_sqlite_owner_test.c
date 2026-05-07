#include <mylite/mylite.h>

#include "runtime/mylite_connection.h"
#include "sqlite3.h"

#include <stdio.h>

static int test_each_handle_owns_distinct_usable_sqlite_connection(void);
static int execute_sql(sqlite3 *connection, const char *sql);
static int table_exists(sqlite3 *connection, const char *table_name, int *out_exists);
static int expect_int(int actual, int expected, const char *context);
static int expect_true(int condition, const char *context);

int main(void) {
    return test_each_handle_owns_distinct_usable_sqlite_connection() == 0 ? 0 : 1;
}

static int test_each_handle_owns_distinct_usable_sqlite_connection(void) {
    enum {
        table_missing = 0,
        table_present = 1,
    };

    mylite_db *first = NULL;
    mylite_db *second = NULL;
    sqlite3 *first_sqlite = NULL;
    sqlite3 *second_sqlite = NULL;
    int first_has_table = table_missing;
    int second_has_table = table_missing;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&first), MYLITE_OK, "open first handle");
    failures += expect_int(mylite_open_memory(&second), MYLITE_OK, "open second handle");

    first_sqlite = mylite_connection_sqlite_for_test(first);
    second_sqlite = mylite_connection_sqlite_for_test(second);
    failures += expect_true(first_sqlite != NULL, "first SQLite connection exists");
    failures += expect_true(second_sqlite != NULL, "second SQLite connection exists");
    failures += expect_true(first_sqlite != second_sqlite, "SQLite connections are distinct");
    if (first_sqlite == NULL || second_sqlite == NULL) {
        mylite_close(second);
        mylite_close(first);
        return failures;
    }

    failures += execute_sql(first_sqlite, "CREATE TABLE ownership_marker(value INTEGER)");
    failures += table_exists(first_sqlite, "ownership_marker", &first_has_table);
    failures += table_exists(second_sqlite, "ownership_marker", &second_has_table);
    failures += expect_int(first_has_table, table_present, "first connection sees table");
    failures += expect_int(second_has_table, table_missing, "second connection does not see table");

    failures += execute_sql(second_sqlite, "CREATE TABLE ownership_marker(value INTEGER)");
    failures += table_exists(second_sqlite, "ownership_marker", &second_has_table);
    failures += expect_int(second_has_table, table_present, "second connection remains usable");

    mylite_close(second);
    mylite_close(first);

    return failures;
}

static int execute_sql(sqlite3 *connection, const char *sql) {
    char *message = NULL;
    int rc = sqlite3_exec(connection, sql, NULL, NULL, &message);

    if (rc != SQLITE_OK) {
        fprintf(
            stderr,
            "execute SQLite SQL \"%s\": error %d: %s\n",
            sql,
            rc,
            message == NULL ? "(no message)" : message
        );
        sqlite3_free(message);
        return 1;
    }

    return 0;
}

static int table_exists(sqlite3 *connection, const char *table_name, int *out_exists) {
    enum { sqlite_use_nul_terminated_string = -1 };

    static const char *sql =
        "SELECT count(*) FROM sqlite_master WHERE type = 'table' AND name = ?1";

    sqlite3_stmt *statement = NULL;
    int rc = SQLITE_OK;

    *out_exists = 0;

    rc = sqlite3_prepare_v2(connection, sql, sqlite_use_nul_terminated_string, &statement, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "prepare table existence query: SQLite error %d\n", rc);
        return 1;
    }

    rc = sqlite3_bind_text(
        statement,
        1,
        table_name,
        sqlite_use_nul_terminated_string,
        SQLITE_STATIC
    );
    if (rc != SQLITE_OK) {
        fprintf(stderr, "bind table name: SQLite error %d\n", rc);
        sqlite3_finalize(statement);
        return 1;
    }

    rc = sqlite3_step(statement);
    if (rc != SQLITE_ROW) {
        fprintf(stderr, "step table existence query: SQLite error %d\n", rc);
        sqlite3_finalize(statement);
        return 1;
    }

    *out_exists = sqlite3_column_int(statement, 0) > 0 ? 1 : 0;

    rc = sqlite3_finalize(statement);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "finalize table existence query: SQLite error %d\n", rc);
        return 1;
    }

    return 0;
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
