#include <mylite/mylite.h>

#include "runtime/mylite_catalog.h"
#include "runtime/mylite_connection.h"
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
    small_integer_sql_capacity = 512,
    small_integer_column_count = 8,
    show_columns_field_count = 6,
    mysql_error_parse = 1064,
    mysql_error_bad_null = 1048,
    mysql_error_data_out_of_range = 1264,
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

struct expected_column_descriptor {
    const char *name;
    const char *logical_type;
    bool is_nullable;
};

static int test_small_integer_success_persistence_and_dml(void);
static int test_small_integer_diagnostics_and_rollback(void);
static int test_small_integer_independent_handles(void);
static int create_small_integer_table(mylite_db *database, const char *table_name);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int expect_column_descriptor(
    mylite_db *database,
    int64_t table_id,
    struct expected_column_descriptor expected,
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

    failures += test_small_integer_success_persistence_and_dml();
    failures += test_small_integer_diagnostics_and_rollback();
    failures += test_small_integer_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_small_integer_success_persistence_and_dml(void) {
    static const char *const boundary_rows[] = {
        "1", "0",   "0",   "-32768", "0",     "-8388608", "0",        "1",
        "2", "127", "42",  NULL,     "65535", "8388607",  "16777215", "2",
        "4", "-1",  "255", NULL,     "65535", "-5",       "16777215", "4",
    };
    static const char *const show_columns_rows[] = {
        "id",  "int",
        "NO",  "",
        NULL,  "",
        "ti",  "tinyint",
        "YES", "",
        NULL,  "",
        "tiu", "tinyint unsigned",
        "YES", "",
        NULL,  "",
        "si",  "smallint",
        "YES", "",
        NULL,  "",
        "siu", "smallint unsigned",
        "YES", "",
        NULL,  "",
        "mi",  "mediumint",
        "YES", "",
        NULL,  "",
        "miu", "mediumint unsigned",
        "YES", "",
        NULL,  "",
        "nn",  "tinyint",
        "NO",  "",
        NULL,  "",
    };
    static const char *const show_create_rows[] = {
        "ints",
        "CREATE TABLE `ints` (\n"
        "  `id` int NOT NULL,\n"
        "  `ti` tinyint DEFAULT NULL,\n"
        "  `tiu` tinyint unsigned DEFAULT NULL,\n"
        "  `si` smallint DEFAULT NULL,\n"
        "  `siu` smallint unsigned DEFAULT NULL,\n"
        "  `mi` mediumint DEFAULT NULL,\n"
        "  `miu` mediumint unsigned DEFAULT NULL,\n"
        "  `nn` tinyint NOT NULL\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const changed_column_row[] = {
        "changed",
        "mediumint unsigned",
        "YES",
        "",
        NULL,
        "",
    };
    static const char *const added_column_row[] = {
        "added",
        "tinyint unsigned",
        "YES",
        "",
        NULL,
        "",
    };
    static const char *const persisted_row[] = {"-1", "255", "255"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    struct mylite_catalog_schema_descriptor schema = {0};
    struct mylite_catalog_table_descriptor table = {0};
    int failures = 0;

    if (make_test_path(path, sizeof(path), "success") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open success file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += create_small_integer_table(database, "ints");

    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM ints",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = small_integer_column_count,
            .context = "small integer SHOW COLUMNS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "DESCRIBE ints",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = small_integer_column_count,
            .context = "small integer DESCRIBE",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "EXPLAIN ints",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = small_integer_column_count,
            .context = "small integer EXPLAIN table",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE ints",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "small integer SHOW CREATE TABLE",
        }
    );

    failures += expect_int(
        mylite_catalog_read_schema_by_name(database, "app", &schema),
        MYLITE_OK,
        "read app schema"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "ints", &table),
        MYLITE_OK,
        "read ints table"
    );
    failures += expect_column_descriptor(
        database,
        table.table_id,
        (struct expected_column_descriptor){
            .name = "ti",
            .logical_type = "TINYINT",
            .is_nullable = true,
        },
        "ti descriptor"
    );
    failures += expect_column_descriptor(
        database,
        table.table_id,
        (struct expected_column_descriptor){
            .name = "tiu",
            .logical_type = "TINYINT UNSIGNED",
            .is_nullable = true,
        },
        "tiu descriptor"
    );
    failures += expect_column_descriptor(
        database,
        table.table_id,
        (struct expected_column_descriptor){
            .name = "si",
            .logical_type = "SMALLINT",
            .is_nullable = true,
        },
        "si descriptor"
    );
    failures += expect_column_descriptor(
        database,
        table.table_id,
        (struct expected_column_descriptor){
            .name = "siu",
            .logical_type = "SMALLINT UNSIGNED",
            .is_nullable = true,
        },
        "siu descriptor"
    );
    failures += expect_column_descriptor(
        database,
        table.table_id,
        (struct expected_column_descriptor){
            .name = "mi",
            .logical_type = "MEDIUMINT",
            .is_nullable = true,
        },
        "mi descriptor"
    );
    failures += expect_column_descriptor(
        database,
        table.table_id,
        (struct expected_column_descriptor){
            .name = "miu",
            .logical_type = "MEDIUMINT UNSIGNED",
            .is_nullable = true,
        },
        "miu descriptor"
    );

    failures += expect_dml_ok(database, "UPDATE ints SET ti = 0 WHERE ti = -128", 1);
    failures += expect_dml_ok(database, "UPDATE ints SET tiu = 42 WHERE ti <=> 127", 1);
    failures += expect_dml_ok(
        database,
        "UPDATE ints SET si = NULL WHERE si IS NOT NULL ORDER BY id DESC LIMIT 1",
        1
    );
    failures += expect_dml_ok(database, "DELETE FROM ints WHERE mi IS NULL", 1);
    failures += expect_dml_ok(
        database,
        "INSERT INTO ints SET id = 4, ti = -1, tiu = 255, siu = 65535, "
        "mi = -5, miu = 16777215, nn = 4",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, ti, tiu, si, siu, mi, miu, nn FROM ints ORDER BY id",
            .values = boundary_rows,
            .column_count = small_integer_column_count,
            .row_count = 3U,
            .context = "small integer DML values",
        }
    );

    failures += expect_statement_ok(database, "ALTER TABLE ints MODIFY ti SMALLINT");
    failures +=
        expect_statement_ok(database, "ALTER TABLE ints CHANGE tiu changed MEDIUMINT UNSIGNED");
    failures += expect_statement_ok(database, "ALTER TABLE ints ADD added TINYINT UNSIGNED");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM ints LIKE 'changed'",
            .values = changed_column_row,
            .column_count = show_columns_field_count,
            .row_count = 1U,
            .context = "changed small integer column",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM ints LIKE 'added'",
            .values = added_column_row,
            .column_count = show_columns_field_count,
            .row_count = 1U,
            .context = "added small integer column",
        }
    );
    failures += expect_dml_ok(database, "UPDATE ints SET added = 255 WHERE id = 4", 1);

    mylite_close(database);
    database = NULL;

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "small integer lifecycle preserves preamble"
    );

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen success file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ti, changed, added FROM ints WHERE id = 4",
            .values = persisted_row,
            .column_count = 3U,
            .row_count = 1U,
            .context = "small integer updates persist after reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_small_integer_diagnostics_and_rollback(void) {
    static const char *const alter_bad_column[] = {"c", "int", "NO", "", NULL, ""};
    static const char *const alter_bad_value[] = {"128"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE ranges (id INT NOT NULL, ti TINYINT NOT NULL, "
        "tiu TINYINT UNSIGNED, si SMALLINT, siu SMALLINT UNSIGNED, "
        "mi MEDIUMINT, miu MEDIUMINT UNSIGNED)"
    );
    failures += expect_dml_ok(database, "INSERT INTO ranges VALUES (1, 0, 0, 0, 0, 0, 0)", 1);

    failures += execute_error(
        database,
        "INSERT INTO ranges VALUES (2, 128, 0, 0, 0, 0, 0)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'ti' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO ranges VALUES (2, -129, 0, 0, 0, 0, 0)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'ti' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO ranges VALUES (2, 0, 256, 0, 0, 0, 0)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'tiu' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO ranges VALUES (2, 0, -1, 0, 0, 0, 0)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'tiu' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO ranges VALUES (2, 0, 0, 32768, 0, 0, 0)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'si' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO ranges VALUES (2, 0, 0, -32769, 0, 0, 0)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'si' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO ranges VALUES (2, 0, 0, 0, 65536, 0, 0)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'siu' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO ranges VALUES (2, 0, 0, 0, -1, 0, 0)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'siu' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO ranges VALUES (2, 0, 0, 0, 0, 8388608, 0)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'mi' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO ranges VALUES (2, 0, 0, 0, 0, -8388609, 0)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'mi' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO ranges VALUES (2, 0, 0, 0, 0, 0, 16777216)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'miu' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO ranges VALUES (2, 0, 0, 0, 0, 0, -1)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'miu' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO ranges VALUES (2, NULL, 0, 0, 0, 0, 0)",
        (struct expected_sql_error){
            .code = mysql_error_bad_null,
            .sqlstate = "23000",
            .message_part = "Column 'ti' cannot be null",
        }
    );
    failures += execute_error(
        database,
        "UPDATE ranges SET ti = 128",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'ti' at row 1",
        }
    );
    failures += execute_error(
        database,
        "UPDATE ranges SET tiu = -1",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'tiu' at row 1",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM ranges WHERE ti = 128",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'ti' in WHERE",
        }
    );
    failures += execute_error(
        database,
        "DELETE FROM ranges WHERE tiu = -1",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'tiu' in WHERE",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE unsupported_width (c TINYINT(1))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );

    failures +=
        expect_statement_ok(database, "CREATE TABLE alter_bad (id INT NOT NULL, c INT NOT NULL)");
    failures += expect_dml_ok(database, "INSERT INTO alter_bad VALUES (1, 128)", 1);
    failures += execute_error(
        database,
        "ALTER TABLE alter_bad MODIFY c TINYINT NOT NULL",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'c' at row 1",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE alter_bad CHANGE c changed TINYINT NOT NULL",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'changed' at row 1",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM alter_bad LIKE 'c'",
            .values = alter_bad_column,
            .column_count = show_columns_field_count,
            .row_count = 1U,
            .context = "failed small integer ALTER preserves descriptor",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT c FROM alter_bad WHERE id = 1",
            .values = alter_bad_value,
            .column_count = 1U,
            .row_count = 1U,
            .context = "failed small integer ALTER preserves rows",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_small_integer_independent_handles(void) {
    static const char *const first_expected[] = {"7"};
    static const char *const second_expected[] = {"1"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "independent_first") != 0 ||
        make_test_path(second_path, sizeof(second_path), "independent_second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first file");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second file");
    failures += expect_statement_ok(first, "CREATE DATABASE app");
    failures += expect_statement_ok(second, "CREATE DATABASE app");
    failures += expect_statement_ok(first, "USE app");
    failures += expect_statement_ok(second, "USE app");
    failures += expect_statement_ok(first, "CREATE TABLE ints (id INT NOT NULL, ti TINYINT)");
    failures += expect_statement_ok(second, "CREATE TABLE ints (id INT NOT NULL, ti TINYINT)");
    failures += expect_dml_ok(first, "INSERT INTO ints VALUES (1, 1)", 1);
    failures += expect_dml_ok(second, "INSERT INTO ints VALUES (1, 1)", 1);
    failures += expect_dml_ok(first, "UPDATE ints SET ti = 7 WHERE id = 1", 1);
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT ti FROM ints WHERE id = 1",
            .values = first_expected,
            .column_count = 1U,
            .row_count = 1U,
            .context = "first small integer handle",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT ti FROM ints WHERE id = 1",
            .values = second_expected,
            .column_count = 1U,
            .row_count = 1U,
            .context = "second small integer handle",
        }
    );

    mylite_close(second);
    mylite_close(first);
    remove_related_files(second_path);
    remove_related_files(first_path);

    return failures;
}

static int create_small_integer_table(mylite_db *database, const char *table_name) {
    char sql[small_integer_sql_capacity];
    int written = snprintf(
        sql,
        sizeof(sql),
        "CREATE TABLE %s (id INT NOT NULL, ti TINYINT, tiu TINYINT UNSIGNED, "
        "si SMALLINT, siu SMALLINT UNSIGNED, mi MEDIUMINT, "
        "miu MEDIUMINT UNSIGNED, nn TINYINT NOT NULL)",
        table_name
    );
    int failures = 0;

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "small integer CREATE TABLE SQL is too long\n");
        return 1;
    }
    failures += expect_statement_ok(database, sql);

    written = snprintf(
        sql,
        sizeof(sql),
        "INSERT INTO %s VALUES "
        "(1, -128, 0, -32768, 0, -8388608, 0, 1), "
        "(2, 127, 255, 32767, 65535, 8388607, 16777215, 2), "
        "(3, NULL, NULL, NULL, NULL, NULL, NULL, 3)",
        table_name
    );
    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "small integer INSERT SQL is too long\n");
        return failures + 1;
    }
    failures += expect_dml_ok(database, sql, 3);

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

    failures += expect_size(mylite_result_column_count(result), 0U, sql);
    failures += expect_size(mylite_result_row_count(result), 0U, sql);
    failures += expect_int64(mylite_result_affected_rows(result), affected_rows, sql);
    failures += expect_size(mylite_result_warning_count(result), 0U, sql);
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

static int expect_column_descriptor(
    mylite_db *database,
    int64_t table_id,
    struct expected_column_descriptor expected,
    const char *context
) {
    struct mylite_catalog_column_descriptor column = {0};
    int failures = expect_int(
        mylite_catalog_read_column_by_name(database, table_id, expected.name, &column),
        MYLITE_OK,
        context
    );

    failures += expect_text(column.logical_type, expected.logical_type, context);
    failures += expect_text(column.physical_type, "INTEGER", context);
    failures += expect_true(column.is_nullable == expected.is_nullable, context);

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
        "%s/mylite_small_integer_types_%d_%s.mylite",
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
    if (memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: byte comparison failed\n", context);
        return 1;
    }

    return 0;
}
