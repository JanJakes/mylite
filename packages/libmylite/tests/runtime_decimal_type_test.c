#include <mylite/mylite.h>

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
    decimal_table_column_count = 7,
    decimal_information_schema_column_count = 10,
    decimal_information_schema_row_count = 6,
    mysql_error_parse = 1064,
    mysql_error_invalid_default = 1067,
    mysql_error_bad_null = 1048,
    mysql_error_data_out_of_range = 1264,
    mysql_error_decimal_scale_too_big = 1425,
    mysql_error_decimal_precision_too_big = 1426,
    mysql_error_decimal_must_be_greater_or_equal_to_d = 1427,
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

struct expected_dml_result {
    int64_t affected_rows;
    size_t warning_count;
};

static int test_decimal_success_persistence_and_introspection(void);
static int test_decimal_diagnostics(void);
static int test_decimal_independent_handles(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_statement_result(
    mylite_db *database,
    const char *sql,
    struct expected_dml_result expected
);
static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows);
static int expect_dml_result(
    mylite_db *database,
    const char *sql,
    struct expected_dml_result expected
);
static int expect_query_values(mylite_db *database, struct expected_query query);
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

    failures += test_decimal_success_persistence_and_introspection();
    failures += test_decimal_diagnostics();
    failures += test_decimal_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_decimal_success_persistence_and_introspection(void) {
    static const char *const show_columns_rows[] = {
        "id",    "int",
        "NO",    "",
        NULL,    "",
        "d",     "decimal(10,0)",
        "YES",   "",
        NULL,    "",
        "m",     "decimal(5,0)",
        "YES",   "",
        NULL,    "",
        "s",     "decimal(5,2)",
        "YES",   "",
        NULL,    "",
        "n",     "decimal(4,1)",
        "YES",   "",
        NULL,    "",
        "u",     "decimal(5,2) unsigned",
        "YES",   "",
        NULL,    "",
        "nn",    "decimal(4,2)",
        "NO",    "",
        "-1.20", "",
    };
    static const char *const show_create_rows[] = {
        "decs",
        "CREATE TABLE `decs` (\n"
        "  `id` int NOT NULL,\n"
        "  `d` decimal(10,0) DEFAULT NULL,\n"
        "  `m` decimal(5,0) DEFAULT NULL,\n"
        "  `s` decimal(5,2) DEFAULT NULL,\n"
        "  `n` decimal(4,1) DEFAULT NULL,\n"
        "  `u` decimal(5,2) unsigned DEFAULT NULL,\n"
        "  `nn` decimal(4,2) NOT NULL DEFAULT '-1.20'\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const information_schema_rows[] = {
        "d",  "decimal", "decimal(10,0)",         "10", "0", "YES", NULL,    NULL, NULL, NULL,
        "m",  "decimal", "decimal(5,0)",          "5",  "0", "YES", NULL,    NULL, NULL, NULL,
        "s",  "decimal", "decimal(5,2)",          "5",  "2", "YES", NULL,    NULL, NULL, NULL,
        "n",  "decimal", "decimal(4,1)",          "4",  "1", "YES", NULL,    NULL, NULL, NULL,
        "u",  "decimal", "decimal(5,2) unsigned", "5",  "2", "YES", NULL,    NULL, NULL, NULL,
        "nn", "decimal", "decimal(4,2)",          "4",  "2", "NO",  "-1.20", NULL, NULL, NULL,
    };
    static const char *const initial_rows[] = {
        "1",  "3",     "12",   "3.10", "1.2", "1.20", "-1.20", "2",    "13",   "99999", "1.23",
        NULL, "99.99", "1.24", "3",    "0",   "-1",   "0.25",  "-1.5", "0.00", "0.00",
    };
    static const char *const after_update_rows[] = {
        "1",
        "3",
        NULL,
        "-1.20",
        "2",
        "13",
        "1.24",
        "1.24",
        "3",
        "0",
        NULL,
        "0.00",
    };
    static const char *const altered_rows[] = {
        "1",
        "0.00",
        "2.30",
        "2",
        "0.00",
        "2.30",
        "3",
        "0.00",
        "2.30",
    };
    static const char *const reopened_rows[] = {
        "1",
        NULL,
        "2.30",
        "2",
        "1.24",
        "2.30",
        "3",
        NULL,
        "2.30",
    };
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

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open decimal success file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_result(
        database,
        "CREATE TABLE decs (id INT NOT NULL, d DECIMAL, m DECIMAL(5), "
        "s DECIMAL(5,2), n NUMERIC(4,1), u DECIMAL(5,2) UNSIGNED, "
        "nn DECIMAL(4,2) NOT NULL DEFAULT -1.20)",
        (struct expected_dml_result){
            .affected_rows = 0,
            .warning_count = 1U,
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM decs",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = decimal_table_column_count,
            .context = "decimal SHOW COLUMNS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE decs",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "decimal SHOW CREATE TABLE",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, DATA_TYPE, COLUMN_TYPE, NUMERIC_PRECISION, "
                   "NUMERIC_SCALE, IS_NULLABLE, COLUMN_DEFAULT, CHARACTER_MAXIMUM_LENGTH, "
                   "CHARACTER_SET_NAME, COLLATION_NAME FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'decs' "
                   "AND COLUMN_NAME <> 'id' ORDER BY ORDINAL_POSITION",
            .values = information_schema_rows,
            .column_count = decimal_information_schema_column_count,
            .row_count = decimal_information_schema_row_count,
            .context = "decimal INFORMATION_SCHEMA.COLUMNS",
        }
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO decs VALUES (1, +0003, 12, 3.1, 1.2, 1.20, DEFAULT)",
        1
    );
    failures += expect_dml_result(
        database,
        "INSERT INTO decs VALUES (2, 12.5, 99999, 1.234, NULL, 99.99, 1.239)",
        (struct expected_dml_result){
            .affected_rows = 1,
            .warning_count = 3U,
        }
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO decs VALUES (3, -0.00, -1., .25, -1.5, -0.00, -0.00)",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, d, m, s, n, u, nn FROM decs ORDER BY id",
            .values = initial_rows,
            .column_count = decimal_table_column_count,
            .row_count = 3U,
            .context = "decimal canonical readback",
        }
    );
    failures += expect_dml_result(
        database,
        "UPDATE decs SET s = 1.235 WHERE id = 2",
        (struct expected_dml_result){
            .affected_rows = 1,
            .warning_count = 1U,
        }
    );
    failures += expect_dml_ok(database, "UPDATE decs SET s = 4.44 WHERE id = 1", 1);
    failures += expect_dml_ok(database, "UPDATE decs SET s = DEFAULT WHERE id = 1", 1);
    failures += expect_dml_ok(database, "UPDATE decs SET s = NULL WHERE id = 3", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, d, s, nn FROM decs ORDER BY id",
            .values = after_update_rows,
            .column_count = 4U,
            .row_count = 3U,
            .context = "decimal update readback",
        }
    );
    failures +=
        expect_statement_ok(database, "ALTER TABLE decs ADD COLUMN added DECIMAL(4,2) NOT NULL");
    failures += expect_statement_ok(
        database,
        "ALTER TABLE decs ADD COLUMN with_default DECIMAL(4,2) NOT NULL DEFAULT 2.30"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, added, with_default FROM decs ORDER BY id",
            .values = altered_rows,
            .column_count = 3U,
            .row_count = 3U,
            .context = "decimal alter add values",
        }
    );
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures +=
        expect_bytes(actual_preamble, expected_preamble, sizeof(actual_preamble), "preamble");

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen decimal success file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, s, with_default FROM decs ORDER BY id",
            .values = reopened_rows,
            .column_count = 3U,
            .row_count = 3U,
            .context = "decimal reopened rows",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_decimal_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures +=
        expect_int(mylite_open(path, &database), MYLITE_OK, "open decimal diagnostics file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_result(
        database,
        "CREATE TABLE values_t (id INT, d DECIMAL(5,2), z DECIMAL(5,0), "
        "u DECIMAL(3,1) UNSIGNED, nn DECIMAL(4,2) NOT NULL DEFAULT 1.00)",
        (struct expected_dml_result){
            .affected_rows = 0,
            .warning_count = 1U,
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO values_t VALUES (1, 999.995, 1, 1.1, 1.1)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'd' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO values_t VALUES (2, 1.00, 1, -1.0, 1.1)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'u' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO values_t VALUES (3, NULL, NULL, NULL, NULL)",
        (struct expected_sql_error){
            .code = mysql_error_bad_null,
            .sqlstate = "23000",
            .message_part = "Column 'nn' cannot be null",
        }
    );
    failures += expect_dml_result(
        database,
        "INSERT IGNORE INTO values_t VALUES (4, 1000.00, 100000, -1.0, NULL)",
        (struct expected_dml_result){
            .affected_rows = 1,
            .warning_count = 4U,
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO values_t VALUES (5, '1.20abc', 1, 1.1, 1.1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "DECIMAL values support only fixed decimal and integer literals",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE quoted_decimal_default (d DECIMAL(5,2) DEFAULT '1.20')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'd'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_m66 (d DECIMAL(66,0))",
        (struct expected_sql_error){
            .code = mysql_error_decimal_precision_too_big,
            .sqlstate = "42000",
            .message_part = "Too-big precision 66 specified for 'd'. Maximum is 65.",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_d31 (d DECIMAL(31,31))",
        (struct expected_sql_error){
            .code = mysql_error_decimal_scale_too_big,
            .sqlstate = "42000",
            .message_part = "Too big scale 31 specified for column 'd'. Maximum is 30.",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_d_gt_m (d DECIMAL(3,4))",
        (struct expected_sql_error){
            .code = mysql_error_decimal_must_be_greater_or_equal_to_d,
            .sqlstate = "42000",
            .message_part = "For float(M,D), double(M,D) or decimal(M,D), M must be >= D",
        }
    );
    failures += execute_error(
        database,
        "UPDATE values_t SET nn = NULL WHERE id = 4",
        (struct expected_sql_error){
            .code = mysql_error_bad_null,
            .sqlstate = "23000",
            .message_part = "Column 'nn' cannot be null",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_decimal_independent_handles(void) {
    static const char *const first_expected[] = {"1.10"};
    static const char *const second_expected[] = {"2.20"};
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
    failures += expect_statement_ok(first, "CREATE TABLE t (id INT, d DECIMAL(5,2))");
    failures += expect_statement_ok(second, "CREATE TABLE t (id INT, d DECIMAL(5,2))");
    failures += expect_dml_ok(first, "INSERT INTO t VALUES (1, 1.10)", 1);
    failures += expect_dml_ok(second, "INSERT INTO t VALUES (1, 2.20)", 1);
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT d FROM t WHERE id = 1",
            .values = first_expected,
            .column_count = 1U,
            .row_count = 1U,
            .context = "first independent decimal state",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT d FROM t WHERE id = 1",
            .values = second_expected,
            .column_count = 1U,
            .row_count = 1U,
            .context = "second independent decimal state",
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
    return expect_statement_result(
        database,
        sql,
        (struct expected_dml_result){
            .affected_rows = 0,
            .warning_count = 0U,
        }
    );
}

static int expect_statement_result(
    mylite_db *database,
    const char *sql,
    struct expected_dml_result expected
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, "statement column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "statement row count");
    failures += expect_size(
        mylite_result_warning_count(result),
        expected.warning_count,
        "statement warning count"
    );
    mylite_result_free(result);

    return failures;
}

static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows) {
    return expect_dml_result(
        database,
        sql,
        (struct expected_dml_result){
            .affected_rows = affected_rows,
            .warning_count = 0U,
        }
    );
}

static int expect_dml_result(
    mylite_db *database,
    const char *sql,
    struct expected_dml_result expected
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, "DML column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "DML row count");
    failures +=
        expect_int64(mylite_result_affected_rows(result), expected.affected_rows, "DML affected");
    if (mylite_result_warning_count(result) != expected.warning_count) {
        fprintf(stderr, "DML warning SQL: %s\n", sql);
    }
    failures +=
        expect_size(mylite_result_warning_count(result), expected.warning_count, "DML warnings");
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
        "%s/mylite_decimal_type_%d_%s.mylite",
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
