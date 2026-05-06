#include <mylite_fork/mylite_sqlite_fork.h>

#include "sqlite3.h"

#include <stdio.h>
#include <string.h>

struct expected_sqlite_error {
    const char *sql;
    const char *fragment;
};

struct expected_text {
    const char *sql;
    const char *expected;
    const char *context;
};

static const sqlite3_int64 mylite_test_int_minimum = -2147483647LL - 1LL;
static const sqlite3_int64 mylite_test_int_maximum = 2147483647LL;
static const sqlite3_int64 mylite_test_tinyint_minimum = -128LL;
static const sqlite3_int64 mylite_test_tinyint_maximum = 127LL;
static const sqlite3_int64 mylite_test_unsigned_int_maximum = 4294967295LL;

static int expect_sqlite_ok(int rc, sqlite3 *database, const char *context);
static int expect_sqlite_error(sqlite3 *database, struct expected_sqlite_error expectation);
static int expect_text(sqlite3 *database, struct expected_text expectation);
static int prepare_single_text(sqlite3 *database, const char *sql, sqlite3_stmt **out_statement);
static int configure_column_types(sqlite3 *database);

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
    failures += expect_sqlite_ok(
        sqlite3_exec(
            database,
            "CREATE TABLE typed_values("
            "id INTEGER,"
            "tiny INTEGER,"
            "unsigned_id INTEGER,"
            "label TEXT,"
            "score REAL"
            ")",
            NULL,
            NULL,
            NULL
        ),
        database,
        "create native type extension fixture"
    );
    failures += configure_column_types(database);
    failures += expect_sqlite_ok(
        sqlite3_exec(
            database,
            "INSERT INTO typed_values VALUES('1', '42', '7', 123, '3.25')",
            NULL,
            NULL,
            NULL
        ),
        database,
        "insert through native MyLite column descriptors"
    );
    failures += expect_text(
        database,
        (struct expected_text){
            .sql = "SELECT id || '|' || tiny || '|' || unsigned_id || '|' || label || '|' || score "
                   "FROM typed_values",
            .expected = "1|42|7|123|3.25",
            .context = "native INSERT coercion result",
        }
    );
    failures += expect_sqlite_ok(
        sqlite3_exec(
            database,
            "UPDATE typed_values SET tiny = '-3.5', unsigned_id = 8, label = 99, score = '4.5' "
            "WHERE id = '1'",
            NULL,
            NULL,
            NULL
        ),
        database,
        "update through native MyLite column descriptors"
    );
    failures += expect_text(
        database,
        (struct expected_text){
            .sql = "SELECT id || '|' || tiny || '|' || unsigned_id || '|' || label || '|' || score "
                   "FROM typed_values",
            .expected = "1|-4|8|99|4.5",
            .context = "native UPDATE coercion result",
        }
    );
    failures += expect_sqlite_error(
        database,
        (struct expected_sqlite_error){
            .sql = "INSERT INTO typed_values VALUES(2, 128, 1, 'ok', 1.0)",
            .fragment = "integer value is out of range",
        }
    );
    failures += expect_sqlite_error(
        database,
        (struct expected_sqlite_error){
            .sql = "INSERT INTO typed_values VALUES(2, 1, -1, 'ok', 1.0)",
            .fragment = "unsigned integer value is out of range",
        }
    );
    failures += expect_sqlite_error(
        database,
        (struct expected_sqlite_error){
            .sql = "INSERT INTO typed_values VALUES(2, 1, 1, 'abcde', 1.0)",
            .fragment = "varchar value is too long",
        }
    );
    failures += expect_sqlite_error(
        database,
        (struct expected_sqlite_error){
            .sql = "INSERT INTO typed_values VALUES(2, 1, 1, 'ok', 'bad')",
            .fragment = "invalid double value",
        }
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

static int expect_sqlite_error(sqlite3 *database, struct expected_sqlite_error expectation) {
    int rc = sqlite3_exec(database, expectation.sql, NULL, NULL, NULL);
    const char *message = sqlite3_errmsg(database);

    if (rc != SQLITE_OK && strstr(message, expectation.fragment) != NULL) {
        return 0;
    }
    fprintf(
        stderr,
        "expected error containing '%s', got rc=%d: %s\n",
        expectation.fragment,
        rc,
        message
    );
    return 1;
}

static int expect_text(sqlite3 *database, struct expected_text expectation) {
    sqlite3_stmt *statement = NULL;
    const unsigned char *actual = NULL;
    int rc = SQLITE_OK;
    int failures = 0;

    failures += prepare_single_text(database, expectation.sql, &statement);
    if (failures != 0) {
        return failures;
    }
    rc = sqlite3_step(statement);
    if (rc != SQLITE_ROW) {
        fprintf(
            stderr,
            "%s: expected row, got rc=%d: %s\n",
            expectation.context,
            rc,
            sqlite3_errmsg(database)
        );
        sqlite3_finalize(statement);
        return 1;
    }
    actual = sqlite3_column_text(statement, 0);
    if (actual == NULL || strcmp((const char *)actual, expectation.expected) != 0) {
        fprintf(
            stderr,
            "%s: expected '%s', got '%s'\n",
            expectation.context,
            expectation.expected,
            actual == NULL ? "(null)" : (const char *)actual
        );
        ++failures;
    }
    rc = sqlite3_step(statement);
    if (rc != SQLITE_DONE) {
        fprintf(
            stderr,
            "%s: expected done, got rc=%d: %s\n",
            expectation.context,
            rc,
            sqlite3_errmsg(database)
        );
        ++failures;
    }
    sqlite3_finalize(statement);
    return failures;
}

static int prepare_single_text(sqlite3 *database, const char *sql, sqlite3_stmt **out_statement) {
    int rc = sqlite3_prepare_v3(database, sql, -1, SQLITE_PREPARE_PERSISTENT, out_statement, NULL);

    if (rc == SQLITE_OK) {
        return 0;
    }
    fprintf(stderr, "prepare text query: sqlite rc=%d: %s\n", rc, sqlite3_errmsg(database));
    return 1;
}

static int configure_column_types(sqlite3 *database) {
    int failures = 0;

    failures += expect_sqlite_ok(
        mylite_sqlite_fork_set_column_type(
            database,
            NULL,
            "typed_values",
            "id",
            &(const struct mylite_sqlite_fork_column_type){
                .kind = MYLITE_SQLITE_FORK_COLUMN_TYPE_SIGNED_INTEGER,
                .integer_minimum = mylite_test_int_minimum,
                .integer_maximum = mylite_test_int_maximum,
            }
        ),
        database,
        "set id type"
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_set_column_type(
            database,
            NULL,
            "typed_values",
            "tiny",
            &(const struct mylite_sqlite_fork_column_type){
                .kind = MYLITE_SQLITE_FORK_COLUMN_TYPE_SIGNED_INTEGER,
                .integer_minimum = mylite_test_tinyint_minimum,
                .integer_maximum = mylite_test_tinyint_maximum,
            }
        ),
        database,
        "set tiny type"
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_set_column_type(
            database,
            NULL,
            "typed_values",
            "unsigned_id",
            &(const struct mylite_sqlite_fork_column_type){
                .kind = MYLITE_SQLITE_FORK_COLUMN_TYPE_UNSIGNED_INTEGER,
                .integer_maximum = mylite_test_unsigned_int_maximum,
            }
        ),
        database,
        "set unsigned_id type"
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_set_column_type(
            database,
            NULL,
            "typed_values",
            "label",
            &(const struct mylite_sqlite_fork_column_type){
                .kind = MYLITE_SQLITE_FORK_COLUMN_TYPE_VARCHAR,
                .character_maximum_length = 4,
            }
        ),
        database,
        "set label type"
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_set_column_type(
            database,
            NULL,
            "typed_values",
            "score",
            &(const struct mylite_sqlite_fork_column_type){
                .kind = MYLITE_SQLITE_FORK_COLUMN_TYPE_DOUBLE,
            }
        ),
        database,
        "set score type"
    );
    return failures;
}
