#include <mylite/mylite.h>

#include "runtime/mylite_connection.h"
#include "sqlite3.h"
#include "storage/mylite_file_format.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <process.h>
#else
#  include <unistd.h>
#endif

enum {
    test_path_capacity = 1024,
    mysql_error_invalid_default = 1067,
    mysql_error_field_no_default = 1364,
    default_projection_column_count = 14,
    alter_add_projection_column_count = 5,
    alter_removed_default_row_count = 5,
    show_columns_column_count = 6,
};

struct expected_statement {
    int64_t affected_rows;
    size_t warning_count;
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_query {
    const char *sql;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    const char *context;
};

struct expected_contains_query {
    const char *sql;
    const char *needle;
    const char *context;
};

static int test_create_insert_metadata_and_persistence(void);
static int test_alter_defaults(void);
static int test_default_diagnostics_and_if_not_exists(void);
static int test_catalog_v1_migration(void);
static int test_independent_default_handles(void);
static int seed_schema(mylite_db *database);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int expect_statement_result(
    mylite_db *database,
    const char *sql,
    struct expected_statement expected
);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int expect_single_value_contains(mylite_db *database, struct expected_contains_query query);
static int make_catalog_look_like_v1(sqlite3 *sqlite);
static int execute_sql(sqlite3 *connection, const char *sql);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_true(int condition, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_create_insert_metadata_and_persistence();
    failures += test_alter_defaults();
    failures += test_default_diagnostics_and_if_not_exists();
    failures += test_catalog_v1_migration();
    failures += test_independent_default_handles();

    return failures == 0 ? 0 : 1;
}

static int test_create_insert_metadata_and_persistence(void) {
    static const char *const default_row[] = {
        "5",
        "9",
        "-7",
        "255",
        "-32768",
        "65535",
        "-8388608",
        "16777215",
        "-9223372036854775808",
        "9223372036854775807",
        "1",
        "0",
        "11",
        NULL,
    };
    static const char *const explicit_null[] = {NULL};
    static const char *const show_i[] = {"i", "int", "YES", "", "5", ""};
    static const char *const show_nn[] = {"nn", "int", "NO", "", "11", ""};
    static const char *const show_nul[] = {"nul", "int", "YES", "", NULL, ""};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "create_insert") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open create defaults");
    failures += seed_schema(database);
    failures += execute_statement_ok(
        database,
        "CREATE TABLE defaults ("
        "id INT NOT NULL, "
        "i INT DEFAULT 5, "
        "ip INTEGER DEFAULT +9, "
        "n INT DEFAULT -7, "
        "tiu TINYINT UNSIGNED DEFAULT 255, "
        "si SMALLINT DEFAULT -32768, "
        "siu SMALLINT UNSIGNED DEFAULT 65535, "
        "mi MEDIUMINT DEFAULT -8388608, "
        "miu MEDIUMINT UNSIGNED DEFAULT 16777215, "
        "bi BIGINT DEFAULT -9223372036854775808, "
        "bu BIGINT UNSIGNED DEFAULT 9223372036854775807, "
        "b BOOL DEFAULT TRUE, "
        "f BOOLEAN DEFAULT FALSE, "
        "nn INT NOT NULL DEFAULT 11, "
        "nul INT DEFAULT NULL)"
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO defaults (id) VALUES (1)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO defaults SET id = 2",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO defaults (id, i) VALUES (3, NULL)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT i, ip, n, tiu, si, siu, mi, miu, bi, bu, b, f, nn, nul "
                   "FROM defaults WHERE id = 1",
            .values = default_row,
            .column_count = default_projection_column_count,
            .row_count = 1U,
            .context = "created defaults fill omitted values",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT i FROM defaults WHERE id = 3",
            .values = explicit_null,
            .column_count = 1U,
            .row_count = 1U,
            .context = "explicit NULL overrides nullable default",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM defaults LIKE 'i'",
            .values = show_i,
            .column_count = show_columns_column_count,
            .row_count = 1U,
            .context = "SHOW COLUMNS integer default",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM defaults LIKE 'nn'",
            .values = show_nn,
            .column_count = show_columns_column_count,
            .row_count = 1U,
            .context = "SHOW COLUMNS not-null integer default",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM defaults LIKE 'nul'",
            .values = show_nul,
            .column_count = show_columns_column_count,
            .row_count = 1U,
            .context = "SHOW COLUMNS default null",
        }
    );
    failures += expect_single_value_contains(
        database,
        (struct expected_contains_query){
            .sql = "SHOW CREATE TABLE defaults",
            .needle = "`i` int DEFAULT '5'",
            .context = "SHOW CREATE TABLE nullable integer default",
        }
    );
    failures += expect_single_value_contains(
        database,
        (struct expected_contains_query){
            .sql = "SHOW CREATE TABLE defaults",
            .needle = "`nn` int NOT NULL DEFAULT '11'",
            .context = "SHOW CREATE TABLE not-null integer default",
        }
    );

    mylite_close(database);
    database = NULL;
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "integer defaults preserve MyLite preamble"
    );

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen create defaults");
    failures += execute_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT i, ip, n, tiu, si, siu, mi, miu, bi, bu, b, f, nn, nul "
                   "FROM defaults WHERE id = 1",
            .values = default_row,
            .column_count = default_projection_column_count,
            .row_count = 1U,
            .context = "reopened defaults persist",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_alter_defaults(void) {
    static const char *const after_add[] = {
        "1",
        "1",
        "12",
        "13",
        NULL,
        "2",
        "1",
        "12",
        "13",
        NULL,
    };
    static const char *const after_modify[] = {"1", "1", "2", "1", "3", "8"};
    static const char *const after_change[] = {"1", "1", "2", "1", "3", "8", "4", "9"};
    static const char *const after_remove_default[] = {
        "1",
        "1",
        "2",
        "1",
        "3",
        "8",
        "4",
        "9",
        "5",
        NULL,
    };
    static const char *const show_renamed[] = {"renamed", "int", "YES", "", NULL, ""};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "alter") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open alter defaults");
    failures += seed_schema(database);
    failures += execute_statement_ok(
        database,
        "CREATE TABLE alter_defaults (id INT NOT NULL, v INT DEFAULT 1)"
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO alter_defaults (id) VALUES (1)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "ALTER TABLE alter_defaults ADD COLUMN added INT DEFAULT 12",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "ALTER TABLE alter_defaults ADD COLUMN added_nn INT NOT NULL DEFAULT 13",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "ALTER TABLE alter_defaults ADD COLUMN added_null INT DEFAULT NULL",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO alter_defaults (id) VALUES (2)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, added, added_nn, added_null FROM alter_defaults ORDER BY id",
            .values = after_add,
            .column_count = alter_add_projection_column_count,
            .row_count = 2U,
            .context = "ALTER ADD default backfill and future insert",
        }
    );
    failures += expect_statement_result(
        database,
        "ALTER TABLE alter_defaults MODIFY v INT DEFAULT 8",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO alter_defaults (id) VALUES (3)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM alter_defaults ORDER BY id",
            .values = after_modify,
            .column_count = 2U,
            .row_count = 3U,
            .context = "ALTER MODIFY default affects future rows only",
        }
    );
    failures += expect_statement_result(
        database,
        "ALTER TABLE alter_defaults CHANGE v renamed INT DEFAULT 9",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO alter_defaults (id) VALUES (4)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, renamed FROM alter_defaults ORDER BY id",
            .values = after_change,
            .column_count = 2U,
            .row_count = 4U,
            .context = "ALTER CHANGE default affects future rows only",
        }
    );
    failures += expect_statement_result(
        database,
        "ALTER TABLE alter_defaults MODIFY renamed INT",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO alter_defaults (id) VALUES (5)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, renamed FROM alter_defaults ORDER BY id",
            .values = after_remove_default,
            .column_count = 2U,
            .row_count = alter_removed_default_row_count,
            .context = "ALTER MODIFY without DEFAULT removes previous default",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM alter_defaults LIKE 'renamed'",
            .values = show_renamed,
            .column_count = show_columns_column_count,
            .row_count = 1U,
            .context = "SHOW COLUMNS removed default",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_default_diagnostics_and_if_not_exists(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open default diagnostics");
    failures += seed_schema(database);
    failures += execute_error(
        database,
        "CREATE TABLE bad_null (id INT NOT NULL DEFAULT NULL)",
        (struct expected_sql_error){mysql_error_invalid_default, "42000", "Invalid default value"}
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_int (id INT DEFAULT 2147483648)",
        (struct expected_sql_error){mysql_error_invalid_default, "42000", "Invalid default value"}
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_unsigned (id INT UNSIGNED DEFAULT -1)",
        (struct expected_sql_error){mysql_error_invalid_default, "42000", "Invalid default value"}
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_big_unsigned (id BIGINT UNSIGNED DEFAULT 9223372036854775808)",
        (struct expected_sql_error){mysql_error_invalid_default, "42000", "Invalid default value"}
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_tiny (id TINYINT DEFAULT 128)",
        (struct expected_sql_error){mysql_error_invalid_default, "42000", "Invalid default value"}
    );
    failures += execute_statement_ok(database, "CREATE TABLE no_default (id INT NOT NULL, v INT)");
    failures += execute_error(
        database,
        "INSERT INTO no_default (v) VALUES (1)",
        (
            struct expected_sql_error
        ){mysql_error_field_no_default, "HY000", "doesn't have a default value"}
    );
    failures += execute_statement_ok(database, "CREATE TABLE ifne (id INT)");
    failures += expect_statement_result(
        database,
        "CREATE TABLE IF NOT EXISTS ifne (id INT DEFAULT 2147483648)",
        (struct expected_statement){.affected_rows = 0, .warning_count = 1U}
    );
    failures += execute_error(
        database,
        "CREATE TABLE IF NOT EXISTS ifne (bad INT NOT NULL DEFAULT NULL)",
        (struct expected_sql_error){mysql_error_invalid_default, "42000", "Invalid default value"}
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_catalog_v1_migration(void) {
    static const char *const migrated_default[] = {"v", "int", "YES", "", NULL, ""};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    sqlite3 *sqlite = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "migration") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open migration source");
    failures += seed_schema(database);
    failures += execute_statement_ok(database, "CREATE TABLE migrated (id INT, v INT)");
    sqlite = mylite_connection_sqlite_for_test(database);
    failures += make_catalog_look_like_v1(sqlite);
    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open migrated catalog");
    failures += execute_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM migrated LIKE 'v'",
            .values = migrated_default,
            .column_count = show_columns_column_count,
            .row_count = 1U,
            .context = "migrated v1 column has no integer default",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_independent_default_handles(void) {
    static const char *const first_value[] = {"1"};
    static const char *const second_value[] = {"2"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "first_handle") != 0 ||
        make_test_path(second_path, sizeof(second_path), "second_handle") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first defaults");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second defaults");
    failures += seed_schema(first);
    failures += seed_schema(second);
    failures += execute_statement_ok(first, "CREATE TABLE t (id INT NOT NULL, v INT DEFAULT 1)");
    failures += execute_statement_ok(second, "CREATE TABLE t (id INT NOT NULL, v INT DEFAULT 2)");
    failures += expect_statement_result(
        first,
        "INSERT INTO t (id) VALUES (1)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_result(
        second,
        "INSERT INTO t (id) VALUES (1)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT v FROM t",
            .values = first_value,
            .column_count = 1U,
            .row_count = 1U,
            .context = "first handle default value",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT v FROM t",
            .values = second_value,
            .column_count = 1U,
            .row_count = 1U,
            .context = "second handle default value",
        }
    );

    mylite_close(second);
    mylite_close(first);
    remove_related_files(second_path);
    remove_related_files(first_path);

    return failures;
}

static int seed_schema(mylite_db *database) {
    int failures = expect_statement_result(
        database,
        "CREATE DATABASE app",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );

    failures += execute_statement_ok(database, "USE app");
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "execute '%s': expected OK, got %d / %d %s %s\n",
            sql,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        return 1;
    }

    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_ERROR) {
        fprintf(stderr, "execute '%s': expected MYLITE_ERROR, got %d\n", sql, rc);
        failures += 1;
    }
    failures += expect_true(result == NULL, "failed execute leaves result null");
    failures += expect_int(mylite_errcode(database), expected.code, "error code");
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, "SQLSTATE");
    failures += expect_contains(mylite_errmsg(database), expected.message_part, "error message");
    mylite_result_free(result);

    return failures;
}

static int execute_statement_ok(mylite_db *database, const char *sql) {
    return expect_statement_result(
        database,
        sql,
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
}

static int expect_statement_result(
    mylite_db *database,
    const char *sql,
    struct expected_statement expected
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, sql);
    failures += expect_size(mylite_result_row_count(result), 0U, sql);
    failures += expect_int64(mylite_result_affected_rows(result), expected.affected_rows, sql);
    failures += expect_size(mylite_result_warning_count(result), expected.warning_count, sql);
    mylite_result_free(result);

    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    failures += expect_size(mylite_result_column_count(result), query.column_count, query.context);
    failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
    if (query.values != NULL) {
        for (size_t row = 0U; row < query.row_count; ++row) {
            for (size_t column = 0U; column < query.column_count; ++column) {
                size_t value_index = (row * query.column_count) + column;

                failures += expect_result_value(
                    result,
                    row,
                    column,
                    query.values[value_index],
                    query.context
                );
            }
        }
    }
    failures += expect_int64(mylite_result_affected_rows(result), 0, query.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, query.context);
    mylite_result_free(result);

    return failures;
}

static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
) {
    const char *actual = mylite_result_value_text(result, row, column);

    if (expected == NULL) {
        return expect_true(actual == NULL, context);
    }

    return expect_text(actual, expected, context);
}

static int expect_single_value_contains(mylite_db *database, struct expected_contains_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    failures += expect_size(mylite_result_column_count(result), 2U, query.context);
    failures += expect_size(mylite_result_row_count(result), 1U, query.context);
    failures +=
        expect_contains(mylite_result_value_text(result, 0U, 1U), query.needle, query.context);
    mylite_result_free(result);

    return failures;
}

static int make_catalog_look_like_v1(sqlite3 *sqlite) {
    int failures = 0;

    failures += execute_sql(sqlite, "ALTER TABLE _mylite_catalog_columns DROP COLUMN default_kind");
    failures +=
        execute_sql(sqlite, "ALTER TABLE _mylite_catalog_columns DROP COLUMN default_integer");
    failures += execute_sql(
        sqlite,
        "UPDATE _mylite_catalog_state "
        "SET schema_version = 1, minimum_reader_schema_version = 1"
    );

    return failures;
}

static int execute_sql(sqlite3 *connection, const char *sql) {
    int rc = sqlite3_exec(connection, sql, NULL, NULL, NULL);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "sqlite exec failed for '%s': %d\n", sql, rc);
        return 1;
    }

    return 0;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    const char *directory = getenv("TMPDIR");
    int written = 0;

    if (directory == NULL || directory[0] == '\0') {
        directory = getenv("TEMP");
    }
    if (directory == NULL || directory[0] == '\0') {
        directory = ".";
    }

    written = snprintf(
        path,
        path_size,
        "%s/mylite_integer_default_literals_%d_%s.mylite",
        directory,
        current_process_id(),
        name
    );
    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "test path is too long for %s\n", name);
        return 1;
    }

    return 0;
}

static int current_process_id(void) {
#ifdef _WIN32
    return _getpid();
#else
    return getpid();
#endif
}

static void remove_related_files(const char *path) {
    remove_with_suffix(path, "");
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related_path[test_path_capacity];
    int written = snprintf(related_path, sizeof(related_path), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related_path)) {
        return;
    }

    (void)remove(related_path);
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        fprintf(stderr, "failed to open %s for reading\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "failed to seek %s\n", path);
        (void)fclose(file);
        return 1;
    }
    if (fread(buffer, 1U, size, file) != size) {
        fprintf(stderr, "failed to read %zu bytes from %s\n", size, path);
        (void)fclose(file);
        return 1;
    }
    if (fclose(file) != 0) {
        fprintf(stderr, "failed to close %s\n", path);
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

static int expect_int64(int64_t actual, int64_t expected, const char *context) {
    if (actual != expected) {
        fprintf(
            stderr,
            "%s: expected %lld, got %lld\n",
            context,
            (long long)expected,
            (long long)actual
        );
        return 1;
    }

    return 0;
}

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
        return 1;
    }

    return 0;
}

static int expect_true(int condition, const char *context) {
    if (!condition) {
        fprintf(stderr, "%s: condition is false\n", context);
        return 1;
    }

    return 0;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected '%s', got '%s'\n",
            context,
            expected == NULL ? "(null)" : expected,
            actual == NULL ? "(null)" : actual
        );
        return 1;
    }

    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || needle == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected '%s' to contain '%s'\n",
            context,
            actual == NULL ? "(null)" : actual,
            needle == NULL ? "(null)" : needle
        );
        return 1;
    }

    return 0;
}

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: bytes differ\n", context);
        return 1;
    }

    return 0;
}
