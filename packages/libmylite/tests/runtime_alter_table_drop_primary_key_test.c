#include <mylite/mylite.h>

#include "runtime/mylite_connection.h"
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
    information_schema_statistics_field_count = 5,
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

static int test_drop_primary_key_success_metadata_and_persistence(void);
static int test_drop_added_and_composite_primary_keys(void);
static int test_drop_primary_key_auto_increment_and_diagnostics(void);
static int test_drop_primary_key_independent_handles(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows);
static int expect_drop_primary_key_ok(mylite_db *database, const char *sql, int64_t affected_rows);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int expect_physical_index_count(
    mylite_db *database,
    int64_t expected_count,
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

    failures += test_drop_primary_key_success_metadata_and_persistence();
    failures += test_drop_added_and_composite_primary_keys();
    failures += test_drop_primary_key_auto_increment_and_diagnostics();
    failures += test_drop_primary_key_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_drop_primary_key_success_metadata_and_persistence(void) {
    static const char *const show_columns_rows[] = {
        "id",
        "int",
        "NO",
        "",
        NULL,
        "",
        "v",
        "int",
        "YES",
        "MUL",
        NULL,
        "",
    };
    static const char *const show_index_rows[] = {
        "drop_pk",
        "1",
        "k_v",
        "1",
        "v",
        "A",
        "0",
        NULL,
        NULL,
        "YES",
        "BTREE",
        "",
        "",
        "YES",
        NULL,
    };
    static const char *const show_create_rows[] = {
        "drop_pk",
        "CREATE TABLE `drop_pk` (\n"
        "  `id` int NOT NULL,\n"
        "  `v` int DEFAULT NULL,\n"
        "  KEY `k_v` (`v`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const information_schema_columns_rows[] = {
        "id",
        "NO",
        "",
        NULL,
        "v",
        "YES",
        "MUL",
        NULL,
    };
    static const char *const information_schema_statistics_rows[] = {"k_v", "1", "1", "v", "YES"};
    static const char *const metadata_count_rows[] = {"0"};
    static const char *const rows_after_duplicate[] = {"1", "10", "2", "20", "1", "30"};
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

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open drop primary key file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE drop_pk (id INT PRIMARY KEY, v INT, KEY k_v (v))"
    );
    failures += expect_dml_ok(database, "INSERT INTO drop_pk VALUES (1,10),(2,20)", 2);
    failures += expect_drop_primary_key_ok(database, "ALTER TABLE drop_pk DROP PRIMARY KEY", 2);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM drop_pk",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = 2U,
            .context = "SHOW COLUMNS after DROP PRIMARY KEY",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM drop_pk",
            .values = show_index_rows,
            .column_count = show_index_field_count,
            .row_count = 1U,
            .context = "SHOW INDEX after DROP PRIMARY KEY",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE drop_pk",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "SHOW CREATE after DROP PRIMARY KEY",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, IS_NULLABLE, COLUMN_KEY, COLUMN_DEFAULT "
                   "FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = 'app' "
                   "AND TABLE_NAME = 'drop_pk' ORDER BY ORDINAL_POSITION",
            .values = information_schema_columns_rows,
            .column_count = 4U,
            .row_count = 2U,
            .context = "I_S COLUMNS after DROP PRIMARY KEY",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME, NULLABLE "
                   "FROM INFORMATION_SCHEMA.STATISTICS WHERE TABLE_SCHEMA = 'app' "
                   "AND TABLE_NAME = 'drop_pk' ORDER BY INDEX_NAME",
            .values = information_schema_statistics_rows,
            .column_count = information_schema_statistics_field_count,
            .row_count = 1U,
            .context = "I_S STATISTICS after DROP PRIMARY KEY",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'drop_pk'",
            .values = metadata_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "table constraint metadata removed after DROP PRIMARY KEY",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'drop_pk'",
            .values = metadata_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "key-column metadata removed after DROP PRIMARY KEY",
        }
    );
    failures += expect_dml_ok(database, "INSERT INTO drop_pk VALUES (1,30)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM drop_pk ORDER BY v",
            .values = rows_after_duplicate,
            .column_count = 2U,
            .row_count = 3U,
            .context = "duplicates allowed after DROP PRIMARY KEY",
        }
    );
    failures += expect_physical_index_count(database, 1, "secondary physical index remains");
    failures += expect_bytes(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)) == 0 ? actual_preamble
                                                                              : NULL,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after DROP PRIMARY KEY"
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen drop primary key file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE drop_pk",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "reopened table keeps dropped primary key",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_drop_added_and_composite_primary_keys(void) {
    static const char *const added_show_create_rows[] = {
        "added_pk",
        "CREATE TABLE `added_pk` (\n"
        "  `id` int NOT NULL,\n"
        "  `v` int DEFAULT NULL\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const composite_show_columns_rows[] = {
        "a",
        "int",
        "NO",
        "",
        NULL,
        "",
        "b",
        "int",
        "NO",
        "",
        NULL,
        "",
        "v",
        "int",
        "YES",
        "",
        NULL,
        "",
    };
    static const char *const composite_show_create_rows[] = {
        "comp_pk",
        "CREATE TABLE `comp_pk` (\n"
        "  `a` int NOT NULL,\n"
        "  `b` int NOT NULL,\n"
        "  `v` int DEFAULT NULL\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const empty_index_rows[] = {0};
    static const char *const added_rows_after_duplicate[] = {"1", "10", "2", "20", "1", "30"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "added_composite") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open added/composite file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(database, "CREATE TABLE added_pk (id INT, v INT)");
    failures += expect_dml_ok(database, "INSERT INTO added_pk VALUES (1,10),(2,20)", 2);
    failures += expect_statement_ok(database, "ALTER TABLE added_pk ADD PRIMARY KEY (id)");
    failures += expect_drop_primary_key_ok(database, "ALTER TABLE added_pk DROP PRIMARY KEY", 2);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE added_pk",
            .values = added_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "added primary key dropped",
        }
    );
    failures += expect_dml_ok(database, "INSERT INTO added_pk VALUES (1,30)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM added_pk ORDER BY v",
            .values = added_rows_after_duplicate,
            .column_count = 2U,
            .row_count = 3U,
            .context = "duplicates allowed after dropping added primary key",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE comp_pk (a INT, b INT, v INT, PRIMARY KEY (a,b))"
    );
    failures += expect_dml_ok(database, "INSERT INTO comp_pk VALUES (1,1,10),(2,2,20)", 2);
    failures += expect_drop_primary_key_ok(database, "ALTER TABLE comp_pk DROP PRIMARY KEY", 2);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM comp_pk",
            .values = composite_show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = 3U,
            .context = "composite primary key columns after drop",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE comp_pk",
            .values = composite_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "composite primary key dropped",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM comp_pk",
            .values = empty_index_rows,
            .column_count = show_index_field_count,
            .row_count = 0U,
            .context = "composite primary key index rows removed",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE rename_pk (id INT PRIMARY KEY)");
    failures += expect_drop_primary_key_ok(database, "ALTER TABLE rename_pk DROP PRIMARY KEY", 0);
    failures += expect_statement_ok(database, "RENAME TABLE rename_pk TO renamed_pk");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM renamed_pk",
            .values = empty_index_rows,
            .column_count = show_index_field_count,
            .row_count = 0U,
            .context = "renamed table keeps dropped primary key",
        }
    );
    failures += expect_statement_ok(database, "DROP TABLE renamed_pk");
    failures += execute_error(
        database,
        "ALTER TABLE renamed_pk DROP PRIMARY KEY",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.renamed_pk' doesn't exist",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_drop_primary_key_auto_increment_and_diagnostics(void) {
    static const char *const auto_columns_rows[] = {
        "id",
        "int",
        "NO",
        "MUL",
        NULL,
        "auto_increment",
        "v",
        "int",
        "YES",
        "",
        NULL,
        "",
    };
    static const char *const auto_index_rows[] = {
        "ai_key",
        "1",
        "id_idx",
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
    static const char *const auto_rows[] = {"1", "10", "2", "20"};
    static const char *const auto_clone_rows[] = {"1", "30"};
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
        "ALTER TABLE no_default DROP PRIMARY KEY",
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
        "ALTER TABLE missing_table DROP PRIMARY KEY",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing_table' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE missing_schema.missing DROP PRIMARY KEY",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'missing_schema'",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE no_pk (id INT)");
    failures += execute_error(
        database,
        "ALTER TABLE no_pk DROP PRIMARY KEY",
        (struct expected_sql_error){
            .code = mysql_error_cant_drop_field_or_key,
            .sqlstate = "42000",
            .message_part = "Can't DROP 'PRIMARY'; check that column/key exists",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE ai_only (id INT AUTO_INCREMENT PRIMARY KEY, v INT)"
    );
    failures += execute_error(
        database,
        "ALTER TABLE ai_only DROP PRIMARY KEY",
        (struct expected_sql_error){
            .code = mysql_error_wrong_auto_key,
            .sqlstate = "42000",
            .message_part = "Incorrect table definition; there can be only one auto column and it "
                            "must be defined as a key",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE ai_key (id INT AUTO_INCREMENT PRIMARY KEY, v INT, KEY id_idx (id))"
    );
    failures += expect_drop_primary_key_ok(database, "ALTER TABLE ai_key DROP PRIMARY KEY", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM ai_key",
            .values = auto_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = 2U,
            .context = "auto increment column remains keyed after drop",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM ai_key",
            .values = auto_index_rows,
            .column_count = show_index_field_count,
            .row_count = 1U,
            .context = "auto increment secondary key remains after drop",
        }
    );
    failures += expect_dml_ok(database, "INSERT INTO ai_key (v) VALUES (10),(20)", 2);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM ai_key ORDER BY id",
            .values = auto_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "auto increment still generates after primary drop",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE ai_clone LIKE ai_key");
    failures += expect_dml_ok(database, "INSERT INTO ai_clone (v) VALUES (30)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM ai_clone ORDER BY id",
            .values = auto_clone_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "CREATE TABLE LIKE clones post-drop auto increment key",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE _mylite_private DROP PRIMARY KEY",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name '_mylite_private'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE _mylite_private.t DROP PRIMARY KEY",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_database_name,
            .sqlstate = "42000",
            .message_part = "Incorrect database name '_mylite_private'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE ai_key DROP INDEX id_idx",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE ai_key DROP CONSTRAINT `PRIMARY`",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE ai_key DROP PRIMARY KEY, ADD KEY k_v (v)",
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

static int test_drop_primary_key_independent_handles(void) {
    static const char *const empty_index_rows[] = {0};
    static const char *const second_index_rows[] = {
        "t",
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
    failures += expect_statement_ok(second, "CREATE DATABASE app");
    failures += expect_statement_ok(first, "USE app");
    failures += expect_statement_ok(second, "USE app");
    failures += expect_statement_ok(first, "CREATE TABLE t (id INT PRIMARY KEY)");
    failures += expect_statement_ok(second, "CREATE TABLE t (id INT PRIMARY KEY)");
    failures += expect_drop_primary_key_ok(first, "ALTER TABLE t DROP PRIMARY KEY", 0);
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SHOW INDEX FROM t",
            .values = empty_index_rows,
            .column_count = show_index_field_count,
            .row_count = 0U,
            .context = "first handle dropped primary key",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SHOW INDEX FROM t",
            .values = second_index_rows,
            .column_count = show_index_field_count,
            .row_count = 1U,
            .context = "second handle keeps primary key",
        }
    );

    mylite_close(second);
    mylite_close(first);
    remove_related_files(second_path);
    remove_related_files(first_path);
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

static int expect_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, "statement column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "statement row count");
    failures += expect_size(mylite_result_warning_count(result), 0U, "statement warning count");
    mylite_result_free(result);

    return failures;
}

static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, "DML column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "DML row count");
    failures += expect_int64(mylite_result_affected_rows(result), affected_rows, "DML affected");
    failures += expect_size(mylite_result_warning_count(result), 0U, "DML warning count");
    mylite_result_free(result);

    return failures;
}

static int expect_drop_primary_key_ok(mylite_db *database, const char *sql, int64_t affected_rows) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, "DROP PRIMARY column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "DROP PRIMARY row count");
    failures +=
        expect_int64(mylite_result_affected_rows(result), affected_rows, "DROP PRIMARY affected");
    failures += expect_size(mylite_result_warning_count(result), 0U, "DROP PRIMARY warnings");
    mylite_result_free(result);

    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    failures += expect_size(mylite_result_column_count(result), query.column_count, query.context);
    failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
    for (size_t row = 0U; row < query.row_count; ++row) {
        for (size_t column = 0U; column < query.column_count; ++column) {
            size_t value_index = (row * query.column_count) + column;

            failures +=
                expect_result_value(result, row, column, query.values[value_index], query.context);
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

static int expect_physical_index_count(
    mylite_db *database,
    int64_t expected_count,
    const char *context
) {
    static const char *const sql = "SELECT COUNT(*) FROM sqlite_master WHERE type = 'index' AND "
                                   "name LIKE '_mylite_user_index_%'";
    sqlite3_stmt *statement = NULL;
    sqlite3 *sqlite = mylite_connection_sqlite_for_test(database);
    int failures = 0;
    int sqlite_rc = sqlite3_prepare_v2(sqlite, sql, -1, &statement, NULL);

    if (sqlite_rc != SQLITE_OK) {
        fprintf(stderr, "%s: failed to prepare physical index count\n", context);
        return 1;
    }
    sqlite_rc = sqlite3_step(statement);
    if (sqlite_rc == SQLITE_ROW) {
        failures +=
            expect_int64((int64_t)sqlite3_column_int64(statement, 0), expected_count, context);
    } else {
        fprintf(stderr, "%s: physical index count did not return a row\n", context);
        failures += 1;
    }
    if (sqlite3_finalize(statement) != SQLITE_OK) {
        fprintf(stderr, "%s: failed to finalize physical index count\n", context);
        failures += 1;
    }

    return failures;
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
        "%s/mylite_alter_drop_primary_key_%d_%s.mylite",
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
        fprintf(stderr, "%s: expected true\n", context);
        return 1;
    }

    return 0;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected text '%s', got '%s'\n",
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
    if (actual == NULL || memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: byte comparison failed\n", context);
        return 1;
    }

    return 0;
}
