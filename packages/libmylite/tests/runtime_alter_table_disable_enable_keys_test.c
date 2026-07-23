#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include "runtime/mylite_connection.h"
#include "runtime/mylite_diagnostics.h"
#include "storage/mylite_file_format.h"

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
    mysql_error_parse = 1064,
    mysql_error_no_database_selected = 1046,
    mysql_error_unknown_database = 1049,
    mysql_error_incorrect_database_name = 1102,
    mysql_error_incorrect_table_name = 1103,
    mysql_error_table_does_not_exist = 1146,
    mysql_error_wrong_usage = 1221,
    mysql_error_access_denied = 1044,
    mysql_error_algorithm_not_supported = 1845,
    mysql_error_algorithm_not_supported_reason = 1846,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_statement {
    int64_t affected_rows;
    size_t warning_count;
};

struct expected_query {
    const char *sql;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    const char *context;
};

static int test_disable_enable_success_metadata_persistence_and_preamble(void);
static int test_temporary_table_shadowing(void);
static int test_disable_enable_diagnostics(void);
static int test_independent_disable_enable_handles(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_statement_result(
    mylite_db *database,
    const char *sql,
    struct expected_statement expected
);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_key_maintenance_ok(mylite_db *database, const char *sql);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_disable_enable_success_metadata_persistence_and_preamble();
    failures += test_temporary_table_shadowing();
    failures += test_disable_enable_diagnostics();
    failures += test_independent_disable_enable_handles();

    return failures == 0 ? 0 : 1;
}

static int test_disable_enable_success_metadata_persistence_and_preamble(void) {
    static const char *const row_count_values[] = {"0", "1", "0"};
    static const char *const copy_row_count_values[] = {"2", "1", "0"};
    static const char *const warning_rows[] = {
        "Note",
        "1031",
        "Table storage engine for 't' doesn't have this option",
    };
    static const char *const copy_warning_rows[] = {
        "Note",
        "1031",
        "Table storage engine for '#sql-mylite-copy' doesn't have this option",
    };
    static const char *const table_rows[] = {"1", "10", "2", "20"};
    static const char *const show_create_rows[] = {
        "t",
        "CREATE TABLE `t` (\n"
        "  `id` int DEFAULT NULL,\n"
        "  `v` int DEFAULT NULL,\n"
        "  KEY `k_v` (`v`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const statistics_rows[] = {"k_v", "v"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    const struct mylite_session_state *session = NULL;
    uint64_t catalog_generation = 0U;
    uint64_t sqlite_schema_generation = 0U;
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "success") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open success file");
    failures += expect_statement_result(
        database,
        "CREATE DATABASE app",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(database, "CREATE TABLE t (id INT, v INT, KEY k_v (v))");
    failures += expect_statement_result(
        database,
        "INSERT INTO t VALUES (1, 10), (2, 20)",
        (struct expected_statement){.affected_rows = 2, .warning_count = 0U}
    );

    session = mylite_connection_session_state(database);
    catalog_generation = session == NULL ? 0U : session->catalog_generation;
    sqlite_schema_generation = session == NULL ? 0U : session->sqlite_schema_generation;

    failures += expect_key_maintenance_ok(database, "ALTER TABLE app.t DISABLE KEYS");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count, @@error_count",
            .values = row_count_values,
            .column_count = 3U,
            .row_count = 1U,
            .context = "disable keys row and warning count",
        }
    );

    failures += expect_statement_result(
        database,
        "ALTER TABLE t ENABLE KEYS, ALGORITHM=COPY",
        (struct expected_statement){.affected_rows = 2, .warning_count = 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count, @@error_count",
            .values = copy_row_count_values,
            .column_count = 3U,
            .row_count = 1U,
            .context = "enable keys copy row and warning count",
        }
    );
    failures += expect_statement_result(
        database,
        "ALTER TABLE t ENABLE KEYS, ALGORITHM=COPY",
        (struct expected_statement){.affected_rows = 2, .warning_count = 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .values = copy_warning_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "enable keys copy storage engine note",
        }
    );
    failures += expect_key_maintenance_ok(database, "ALTER TABLE t ENABLE KEYS");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .values = warning_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "enable keys storage engine note",
        }
    );
    session = mylite_connection_session_state(database);
    failures += mylite_test_expect_uint64(
        session == NULL ? 0U : session->catalog_generation,
        catalog_generation,
        "key maintenance preserves catalog generation"
    );
    failures += mylite_test_expect_uint64(
        session == NULL ? 0U : session->sqlite_schema_generation,
        sqlite_schema_generation,
        "key maintenance preserves SQLite schema generation"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM t ORDER BY id",
            .values = table_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "rows after key maintenance",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE t",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "SHOW CREATE after key maintenance",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT INDEX_NAME, COLUMN_NAME FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 't' ORDER BY INDEX_NAME",
            .values = statistics_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "index metadata after key maintenance",
        }
    );

    mylite_close(database);
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures +=
        expect_bytes(actual_preamble, expected_preamble, sizeof(actual_preamble), "preamble");

    database = NULL;
    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "reopen success file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM t ORDER BY id",
            .values = table_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "reopened rows after key maintenance",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE t",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "reopened SHOW CREATE after key maintenance",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_temporary_table_shadowing(void) {
    static const char *const warning_rows[] = {
        "Note",
        "1031",
        "Table storage engine for 'shadow_t' doesn't have this option",
    };
    static const char *const temp_rows[] = {"99"};
    static const char *const persistent_rows[] = {"1"};
    mylite_db *database = NULL;
    int failures = 0;

    failures += mylite_test_expect_int(
        mylite_test_open_temporary(&database),
        MYLITE_OK,
        "open shadow memory db"
    );
    failures += expect_statement_result(
        database,
        "CREATE DATABASE app",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(database, "CREATE TABLE shadow_t (id INT, KEY k_id (id))");
    failures += expect_statement_result(
        database,
        "INSERT INTO shadow_t VALUES (1)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures +=
        expect_statement_ok(database, "CREATE TEMPORARY TABLE shadow_t (id INT, KEY k_id (id))");
    failures += expect_statement_result(
        database,
        "INSERT INTO shadow_t VALUES (99)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_key_maintenance_ok(database, "ALTER TABLE shadow_t DISABLE KEYS");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .values = warning_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "temporary shadow storage engine note",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM shadow_t",
            .values = temp_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "temporary shadow rows",
        }
    );
    failures += expect_statement_ok(database, "DROP TEMPORARY TABLE shadow_t");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM shadow_t",
            .values = persistent_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "persistent rows after temporary shadow dropped",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_disable_enable_diagnostics(void) {
    mylite_db *database = NULL;
    int failures = 0;

    failures += mylite_test_expect_int(
        mylite_test_open_temporary(&database),
        MYLITE_OK,
        "open diagnostics db"
    );
    failures += expect_statement_result(
        database,
        "CREATE DATABASE app",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += execute_error(
        database,
        "ALTER TABLE t DISABLE KEYS",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE t DISABLE KEYS, LOCK=NONE",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE t DISABLE KEYS, ALGORITHM=INSTANT, LOCK=EXCLUSIVE",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE missing_schema.t DISABLE KEYS",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'missing_schema'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE missing_schema.t DISABLE KEYS, LOCK=SHARED",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'missing_schema'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE information_schema.tables DISABLE KEYS",
        (struct expected_sql_error){
            .code = mysql_error_access_denied,
            .sqlstate = "42000",
            .message_part = "Access denied",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE information_schema.tables DISABLE KEYS, LOCK=NONE",
        (struct expected_sql_error){
            .code = mysql_error_access_denied,
            .sqlstate = "42000",
            .message_part = "Access denied",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE information_schema.tables DISABLE KEYS, ALGORITHM=INSTANT, LOCK=EXCLUSIVE",
        (struct expected_sql_error){
            .code = mysql_error_access_denied,
            .sqlstate = "42000",
            .message_part = "Access denied",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE _mylite_private.t DISABLE KEYS",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_database_name,
            .sqlstate = "42000",
            .message_part = "Incorrect database name '_mylite_private'",
        }
    );
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(database, "CREATE TABLE t (id INT, KEY k_id (id))");
    failures += execute_error(
        database,
        "ALTER TABLE missing_t ENABLE KEYS",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing_t' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE missing_t ENABLE KEYS, LOCK=NONE",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing_t' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE missing_t ENABLE KEYS, ALGORITHM=INSTANT, LOCK=EXCLUSIVE",
        (struct expected_sql_error){
            .code = mysql_error_wrong_usage,
            .sqlstate = "HY000",
            .message_part = "Incorrect usage of ALGORITHM=INSTANT",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE _mylite_private DISABLE KEYS",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name '_mylite_private'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE t DISABLE KEYS, LOCK=NONE",
        (struct expected_sql_error){
            .code = mysql_error_algorithm_not_supported,
            .sqlstate = "0A000",
            .message_part = "LOCK=NONE/SHARED is not supported for this operation",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE t ENABLE KEYS, LOCK=SHARED",
        (struct expected_sql_error){
            .code = mysql_error_algorithm_not_supported,
            .sqlstate = "0A000",
            .message_part = "LOCK=NONE/SHARED is not supported for this operation",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE t DISABLE KEYS, ALGORITHM=COPY, LOCK=NONE",
        (struct expected_sql_error){
            .code = mysql_error_algorithm_not_supported_reason,
            .sqlstate = "0A000",
            .message_part = "COPY algorithm requires a lock",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE t ENABLE KEYS, ALGORITHM=INSTANT, LOCK=EXCLUSIVE",
        (struct expected_sql_error){
            .code = mysql_error_wrong_usage,
            .sqlstate = "HY000",
            .message_part = "Incorrect usage of ALGORITHM=INSTANT",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE t DISABLE KEY",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE t DISABLE KEYS, ENABLE KEYS",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "multi-action ALTER TABLE does not support this action",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_independent_disable_enable_handles(void) {
    static const char *const first_rows[] = {"1"};
    static const char *const second_rows[] = {"2"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (mylite_test_make_path(first_path, sizeof(first_path), "first") != 0 ||
        mylite_test_make_path(second_path, sizeof(second_path), "second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures +=
        mylite_test_expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first file");
    failures +=
        mylite_test_expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second file");
    failures += expect_statement_result(
        first,
        "CREATE DATABASE app",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_ok(first, "USE app");
    failures += expect_statement_ok(first, "CREATE TABLE t (id INT, KEY k_id (id))");
    failures += expect_statement_result(
        first,
        "INSERT INTO t VALUES (1)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_result(
        second,
        "CREATE DATABASE app",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_ok(second, "USE app");
    failures += expect_statement_ok(second, "CREATE TABLE t (id INT, KEY k_id (id))");
    failures += expect_statement_result(
        second,
        "INSERT INTO t VALUES (2)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );

    failures += expect_key_maintenance_ok(first, "ALTER TABLE t DISABLE KEYS");
    failures += expect_key_maintenance_ok(second, "ALTER TABLE t ENABLE KEYS");
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT id FROM t",
            .values = first_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "first handle rows",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT id FROM t",
            .values = second_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "second handle rows",
        }
    );

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected success, got %d %s %s\n",
            sql,
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
        fprintf(stderr, "%s: expected MYLITE_ERROR, got %d\n", sql, rc);
        failures += 1;
    }
    failures += mylite_test_expect_true(result == NULL, "failed execute leaves result null");
    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, sql);
    failures += mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += mylite_test_expect_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);

    return failures;
}

static int expect_statement_result(
    mylite_db *database,
    const char *sql,
    struct expected_statement expected
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += mylite_test_expect_size(mylite_result_column_count(result), 0U, sql);
    failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, sql);
    failures +=
        mylite_test_expect_int64(mylite_result_affected_rows(result), expected.affected_rows, sql);
    failures +=
        mylite_test_expect_size(mylite_result_warning_count(result), expected.warning_count, sql);
    mylite_result_free(result);

    return failures;
}

static int expect_statement_ok(mylite_db *database, const char *sql) {
    return expect_statement_result(
        database,
        sql,
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
}

static int expect_key_maintenance_ok(mylite_db *database, const char *sql) {
    return expect_statement_result(
        database,
        sql,
        (struct expected_statement){.affected_rows = 0, .warning_count = 1U}
    );
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        query.column_count,
        query.context
    );
    failures +=
        mylite_test_expect_size(mylite_result_row_count(result), query.row_count, query.context);
    for (size_t row = 0U; row < query.row_count; ++row) {
        for (size_t column = 0U; column < query.column_count; ++column) {
            size_t value_index = (row * query.column_count) + column;

            failures +=
                expect_result_value(result, row, column, query.values[value_index], query.context);
        }
    }
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
        return mylite_test_expect_true(actual == NULL, context);
    }

    return mylite_test_expect_text(actual, expected, context);
}

static void remove_related_files(const char *path) {
    remove_with_suffix(path, "");
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char buffer[test_path_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(buffer)) {
        (void)remove(buffer);
    }
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        fprintf(stderr, "failed to open %s\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "failed to seek %s\n", path);
        fclose(file);
        return 1;
    }
    if (fread(buffer, 1U, size, file) != size) {
        fprintf(stderr, "failed to read %s\n", path);
        fclose(file);
        return 1;
    }
    fclose(file);
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
