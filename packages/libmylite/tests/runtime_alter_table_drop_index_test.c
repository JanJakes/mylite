#include <mylite/mylite.h>

#include "runtime/mylite_connection.h"
#include "runtime/mylite_diagnostics.h"
#include "sqlite3.h"
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
    show_columns_field_count = 6,
    show_index_field_count = 15,
    mysql_error_no_database_selected = 1046,
    mysql_error_unknown_database = 1049,
    mysql_error_parse = 1064,
    mysql_error_wrong_auto_key = 1075,
    mysql_error_cant_drop_field_or_key = 1091,
    mysql_error_incorrect_database_name = 1102,
    mysql_error_incorrect_table_name = 1103,
    mysql_error_table_does_not_exist = 1146,
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

static int test_drop_index_success_metadata_and_persistence(void);
static int test_drop_index_added_case_schema_and_rename(void);
static int test_drop_index_auto_increment_and_diagnostics(void);
static int test_drop_index_independent_handles(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_alter_drop_index_ok(mylite_db *database, const char *sql);
static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_physical_index_count(
    mylite_db *database,
    int expected_count,
    const char *context
);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
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

    failures += test_drop_index_success_metadata_and_persistence();
    failures += test_drop_index_added_case_schema_and_rename();
    failures += test_drop_index_auto_increment_and_diagnostics();
    failures += test_drop_index_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_drop_index_success_metadata_and_persistence(void) {
    static const char *const show_create_rows[] = {
        "drop_idx",
        "CREATE TABLE `drop_idx` (\n"
        "  `id` int NOT NULL,\n"
        "  `v` int DEFAULT NULL,\n"
        "  `u` int DEFAULT NULL,\n"
        "  PRIMARY KEY (`id`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const show_index_rows[] = {
        "drop_idx",
        "0",
        "PRIMARY",
        "1",
        "id",
        "A",
        "0",
        NULL,
        NULL,
        "",
        "BTREE",
        "",
        "",
        "YES",
        NULL,
    };
    static const char *const column_key_rows[] = {"id", "PRI", "v", "", "u", ""};
    static const char *const zero_count_rows[] = {"0"};
    static const char *const row_values[] = {"1", "10", "100", "2", "20", "200", "3", "10", "300"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "success") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open drop index file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE drop_idx ("
        "id INT PRIMARY KEY, v INT, u INT, UNIQUE KEY u_v (v), KEY k_u (u))"
    );
    failures += expect_dml_ok(database, "INSERT INTO drop_idx VALUES (1,10,100),(2,20,200)", 2);
    failures += expect_physical_index_count(database, 3, "physical indexes before DROP INDEX");
    failures += expect_alter_drop_index_ok(database, "ALTER TABLE drop_idx DROP INDEX k_u");
    failures += expect_alter_drop_index_ok(database, "ALTER TABLE drop_idx DROP KEY u_v");
    failures += expect_physical_index_count(database, 1, "physical indexes after DROP INDEX");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE drop_idx",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "SHOW CREATE after DROP INDEX",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM drop_idx",
            .values = show_index_rows,
            .column_count = show_index_field_count,
            .row_count = 1U,
            .context = "SHOW INDEX after DROP INDEX",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, COLUMN_KEY FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'drop_idx' "
                   "ORDER BY ORDINAL_POSITION",
            .values = column_key_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "I_S COLUMNS after DROP INDEX",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'drop_idx' "
                   "AND INDEX_NAME <> 'PRIMARY'",
            .values = zero_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "I_S STATISTICS after DROP INDEX",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'drop_idx' "
                   "AND CONSTRAINT_TYPE = 'UNIQUE'",
            .values = zero_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "unique constraints after DROP INDEX",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'drop_idx' "
                   "AND CONSTRAINT_NAME <> 'PRIMARY'",
            .values = zero_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "key-column rows after DROP INDEX",
        }
    );
    failures += expect_dml_ok(database, "INSERT INTO drop_idx VALUES (3,10,300)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, u FROM drop_idx ORDER BY id",
            .values = row_values,
            .column_count = 3U,
            .row_count = 3U,
            .context = "rows after unique DROP INDEX",
        }
    );
    failures += expect_bytes(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)) == 0 ? actual_preamble
                                                                              : NULL,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after DROP INDEX"
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen drop index file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'drop_idx' "
                   "AND INDEX_NAME <> 'PRIMARY'",
            .values = zero_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "reopened dropped index metadata",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, u FROM drop_idx ORDER BY id",
            .values = row_values,
            .column_count = 3U,
            .row_count = 3U,
            .context = "reopened rows after DROP INDEX",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_drop_index_added_case_schema_and_rename(void) {
    static const char *const zero_count_rows[] = {"0"};
    static const char *const one_count_rows[] = {"1"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "added_case") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open added/case file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(database, "CREATE TABLE added (id INT, v INT)");
    failures += expect_statement_ok(database, "ALTER TABLE added ADD INDEX k_v (v)");
    failures += expect_alter_drop_index_ok(database, "ALTER TABLE app.added DROP KEY K_V");
    failures += expect_physical_index_count(database, 0, "physical indexes after schema drop");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'added'",
            .values = zero_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "schema-qualified case-insensitive DROP KEY",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE renamed (id INT, v INT, KEY k_v (v))");
    failures += expect_statement_ok(database, "RENAME TABLE renamed TO renamed_after");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'renamed_after'",
            .values = one_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "renamed table keeps index before drop",
        }
    );
    failures += expect_alter_drop_index_ok(database, "ALTER TABLE renamed_after DROP INDEX k_v");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'renamed_after'",
            .values = zero_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "renamed table DROP INDEX metadata",
        }
    );
    failures += expect_statement_ok(database, "DROP TABLE renamed_after");

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_drop_index_auto_increment_and_diagnostics(void) {
    static const char *const auto_column_rows[] =
        {"id", "int", "NO", "PRI", NULL, "auto_increment", "v", "int", "YES", "", NULL, ""};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics file");
    failures += execute_error(
        database,
        "ALTER TABLE no_default DROP INDEX k_v",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += execute_error(
        database,
        "ALTER TABLE missing_schema.missing DROP INDEX k_v",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'missing_schema'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE missing_table DROP INDEX k_v",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing_table' doesn't exist",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE diag (id INT, v INT, KEY k_v (v))");
    failures += execute_error(
        database,
        "ALTER TABLE diag DROP INDEX missing_idx",
        (struct expected_sql_error){
            .code = mysql_error_cant_drop_field_or_key,
            .sqlstate = "42000",
            .message_part = "Can't DROP 'missing_idx'; check that column/key exists",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE _mylite_private.diag DROP INDEX k_v",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_database_name,
            .sqlstate = "42000",
            .message_part = "Incorrect database name '_mylite_private'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE _mylite_private DROP INDEX k_v",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name '_mylite_private'",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE ai_primary (id INT AUTO_INCREMENT PRIMARY KEY, v INT, KEY id_idx (id))"
    );
    failures += expect_alter_drop_index_ok(database, "ALTER TABLE ai_primary DROP INDEX id_idx");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM ai_primary",
            .values = auto_column_rows,
            .column_count = show_columns_field_count,
            .row_count = 2U,
            .context = "auto increment key remains after DROP INDEX",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE ai_last (id INT AUTO_INCREMENT PRIMARY KEY, v INT, KEY id_idx (id))"
    );
    failures += expect_dml_ok(database, "ALTER TABLE ai_last DROP PRIMARY KEY", 0);
    failures += execute_error(
        database,
        "ALTER TABLE ai_last DROP INDEX id_idx",
        (struct expected_sql_error){
            .code = mysql_error_wrong_auto_key,
            .sqlstate = "42000",
            .message_part =
                "Incorrect table definition; there can be only one auto column and it must be "
                "defined as a key",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE ai_last DROP INDEX `PRIMARY`",
        (struct expected_sql_error){
            .code = mysql_error_cant_drop_field_or_key,
            .sqlstate = "42000",
            .message_part = "Can't DROP 'PRIMARY'; check that column/key exists",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE primary_t (id INT PRIMARY KEY)");
    failures += execute_error(
        database,
        "ALTER TABLE primary_t DROP INDEX `PRIMARY`",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "DROP INDEX does not drop primary keys",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE primary_t DROP INDEX PRIMARY",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE diag DROP INDEX k_v, ALGORITHM=BOGUS",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE diag DROP INDEX k_v, RENAME INDEX k_u TO k_id",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_drop_index_independent_handles(void) {
    static const char *const first_index_count_rows[] = {"0"};
    static const char *const second_index_count_rows[] = {"1"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "first") != 0 ||
        make_test_path(second_path, sizeof(second_path), "second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first handle");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second handle");
    failures += expect_statement_ok(first, "CREATE DATABASE app");
    failures += expect_statement_ok(first, "USE app");
    failures += expect_statement_ok(first, "CREATE TABLE t (id INT, v INT, KEY k_v (v))");
    failures += expect_statement_ok(second, "CREATE DATABASE app");
    failures += expect_statement_ok(second, "USE app");
    failures += expect_statement_ok(second, "CREATE TABLE t (id INT, v INT, KEY k_v (v))");
    failures += expect_alter_drop_index_ok(first, "ALTER TABLE t DROP INDEX k_v");
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 't'",
            .values = first_index_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "first handle dropped index",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 't'",
            .values = second_index_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "second handle remains independent",
        }
    );
    failures += expect_physical_index_count(first, 0, "first physical index");
    failures += expect_physical_index_count(second, 1, "second physical index");

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        const struct mylite_diagnostics *diagnostics = mylite_connection_diagnostics(database);

        fprintf(
            stderr,
            "%s: expected success, got %d %s %s\n",
            sql,
            mylite_diagnostics_errcode(diagnostics),
            mylite_diagnostics_sqlstate(diagnostics),
            mylite_diagnostics_errmsg(diagnostics)
        );
        return 1;
    }
    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    const struct mylite_diagnostics *diagnostics = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc == MYLITE_OK) {
        fprintf(stderr, "%s: expected error, got success\n", sql);
        mylite_result_free(result);
        return 1;
    }
    diagnostics = mylite_connection_diagnostics(database);
    failures += expect_int(mylite_diagnostics_errcode(diagnostics), expected.code, sql);
    failures += expect_text(mylite_diagnostics_sqlstate(diagnostics), expected.sqlstate, sql);
    failures += expect_contains(mylite_diagnostics_errmsg(diagnostics), expected.message_part, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, sql);
        failures += expect_size(mylite_result_row_count(result), 0U, sql);
        failures += expect_size(mylite_result_warning_count(result), 0U, sql);
    }
    mylite_result_free(result);
    return failures;
}

static int expect_alter_drop_index_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, sql);
        failures += expect_size(mylite_result_row_count(result), 0U, sql);
        failures += expect_int64(mylite_result_affected_rows(result), 0, sql);
        failures += expect_size(mylite_result_warning_count(result), 0U, sql);
    }
    mylite_result_free(result);
    return failures;
}

static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, sql);
        failures += expect_size(mylite_result_row_count(result), 0U, sql);
        failures += expect_int64(mylite_result_affected_rows(result), affected_rows, sql);
        failures += expect_size(mylite_result_warning_count(result), 0U, sql);
    }
    mylite_result_free(result);
    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    if (failures == 0) {
        failures +=
            expect_size(mylite_result_column_count(result), query.column_count, query.context);
        failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
    }
    for (size_t row = 0U; failures == 0 && row < query.row_count; ++row) {
        for (size_t column = 0U; column < query.column_count; ++column) {
            size_t value_index = (row * query.column_count) + column;

            failures +=
                expect_result_value(result, row, column, query.values[value_index], query.context);
        }
    }

    mylite_result_free(result);
    return failures;
}

static int expect_physical_index_count(
    mylite_db *database,
    int expected_count,
    const char *context
) {
    enum { sqlite_use_nul_terminated_string = -1 };

    sqlite3 *connection = mylite_connection_sqlite_for_test(database);
    sqlite3_stmt *statement = NULL;
    int actual_count = 0;
    int rc = SQLITE_OK;

    if (connection == NULL) {
        fprintf(stderr, "%s: missing SQLite test connection\n", context);
        return 1;
    }

    rc = sqlite3_prepare_v2(
        connection,
        "SELECT count(*) FROM sqlite_schema "
        "WHERE type = 'index' AND name GLOB '_mylite_user_index_*'",
        sqlite_use_nul_terminated_string,
        &statement,
        NULL
    );
    if (rc != SQLITE_OK) {
        fprintf(stderr, "%s: prepare physical index query failed: %d\n", context, rc);
        return 1;
    }

    rc = sqlite3_step(statement);
    if (rc == SQLITE_ROW) {
        actual_count = sqlite3_column_int(statement, 0);
        rc = SQLITE_OK;
    }
    if (statement != NULL && sqlite3_finalize(statement) != SQLITE_OK && rc == SQLITE_OK) {
        rc = SQLITE_ERROR;
    }
    if (rc != SQLITE_OK) {
        fprintf(stderr, "%s: physical index query failed: %d\n", context, rc);
        return 1;
    }

    return expect_int(actual_count, expected_count, context);
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
        if (actual != NULL) {
            fprintf(
                stderr,
                "%s: expected NULL at row %zu column %zu, got [%s]\n",
                context,
                row,
                column,
                actual
            );
            return 1;
        }
        return 0;
    }

    return expect_text(actual, expected, context);
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_alter_drop_index_%d_%s.mylite",
        current_process_id(),
        name
    );

    if (written < 0 || (size_t)written >= path_size) {
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
    char buffer[test_path_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(buffer)) {
        (void)remove(buffer);
    }
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    int status = -1;

    if (file == NULL) {
        return -1;
    }
    if (fseek(file, offset, SEEK_SET) == 0 && fread(buffer, 1U, size, file) == size) {
        status = 0;
    }
    if (fclose(file) != 0) {
        status = -1;
    }
    return status;
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

static int expect_text(const char *actual, const char *expected, const char *context) {
    if ((actual == NULL && expected != NULL) || (actual != NULL && expected == NULL) ||
        (actual != NULL && expected != NULL && strcmp(actual, expected) != 0)) {
        fprintf(
            stderr,
            "%s: expected [%s], got [%s]\n",
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
            "%s: expected [%s] to contain [%s]\n",
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
    if (actual == NULL || memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: bytes differ\n", context);
        return 1;
    }
    return 0;
}
