#include <mylite/mylite.h>

#include "storage/mylite_file_format.h"

#include <inttypes.h>
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
    show_columns_column_count = 6,
    decimal_base = 10,
    mysql_error_parse = 1064,
    mysql_error_no_database_selected = 1046,
    mysql_error_unknown_database = 1049,
    mysql_error_incorrect_database_name = 1102,
    mysql_error_incorrect_table_name = 1103,
    mysql_error_unknown = 1105,
    mysql_error_table_does_not_exist = 1146,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct single_column_result_expectation {
    const char *sql;
    const char *expected_column_name;
    const char *const *expected_rows;
    size_t expected_row_count;
    const char *context;
};

static const char *const show_columns_names[show_columns_column_count] = {
    "Field",
    "Type",
    "Null",
    "Key",
    "Default",
    "Extra",
};

static const char *const columns_i_percent[][show_columns_column_count] = {
    {"id", "int", "NO", "", NULL, ""},
    {"id2", "int", "YES", "", NULL, ""},
    {"i_1", "int", "YES", "", NULL, ""},
};

static const char *const column_id[][show_columns_column_count] = {
    {"id", "int", "NO", "", NULL, ""},
};

static const char *const column_i_1[][show_columns_column_count] = {
    {"i_1", "int", "YES", "", NULL, ""},
};

static const char *const column_a_percent_b[][show_columns_column_count] = {
    {"a%b", "int", "YES", "", NULL, ""},
};

static const char *const column_a_underscore_b[][show_columns_column_count] = {
    {"a_b", "int", "YES", "", NULL, ""},
};

static const char *const column_a_backslash_b[][show_columns_column_count] = {
    {"a\\b", "int", "YES", "", NULL, ""},
};

static const char *const column_mixed_case[][show_columns_column_count] = {
    {"MixedCase", "int", "YES", "", NULL, ""},
};

static const char *const column_name_id[][show_columns_column_count] = {
    {"name_id", "bigint", "YES", "", NULL, ""},
};

static const char *const column_literal_percent[][show_columns_column_count] = {
    {"literal_percent", "int", "YES", "", NULL, ""},
};

static int test_show_like_values_persistence_rename_and_drop(void);
static int test_show_like_diagnostics_and_unsupported_forms(void);
static int test_independent_show_like_handles(void);
static int create_like_schema(mylite_db *database);
static int expect_single_column_result(
    mylite_db *database,
    struct single_column_result_expectation expectation
);
static int expect_show_columns_result(
    mylite_db *database,
    const char *sql,
    const char *const expected_rows[][show_columns_column_count],
    size_t expected_row_count,
    const char *context
);
static int expect_row_count(mylite_db *database, int64_t expected, const char *context);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text_or_null(const char *actual, const char *expected, const char *context);
static int expect_text_contains(const char *actual, const char *needle, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_show_like_values_persistence_rename_and_drop();
    failures += test_show_like_diagnostics_and_unsupported_forms();
    failures += test_independent_show_like_handles();

    return failures == 0 ? 0 : 1;
}

static int test_show_like_values_persistence_rename_and_drop(void) {
    static const char *const database_rows[] = {"app"};
    static const char *const other_database_rows[] = {"other"};
    static const char *const tables_a_percent[] = {"a%b", "a\\b", "a_b", "alpha"};
    static const char *const tables_a_wildcard_b[] = {"a%b", "a\\b", "a_b"};
    static const char *const table_a_underscore_b[] = {"a_b"};
    static const char *const table_a_percent_b[] = {"a%b"};
    static const char *const table_a_backslash_b[] = {"a\\b"};
    static const char *const table_beta[] = {"beta"};
    static const char *const table_other_alpha[] = {"other_alpha"};
    static const char *const table_renamed_alpha[] = {"renamed_alpha"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "values") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open values file");
    failures += create_like_schema(database);

    failures += expect_single_column_result(
        database,
        (struct single_column_result_expectation){
            .sql = "SHOW DATABASES LIKE 'a%'",
            .expected_column_name = "Database (a%)",
            .expected_rows = database_rows,
            .expected_row_count = sizeof(database_rows) / sizeof(database_rows[0]),
            .context = "show databases like",
        }
    );
    failures += expect_single_column_result(
        database,
        (struct single_column_result_expectation){
            .sql = "SHOW SCHEMAS LIKE 'o%'",
            .expected_column_name = "Database (o%)",
            .expected_rows = other_database_rows,
            .expected_row_count = sizeof(other_database_rows) / sizeof(other_database_rows[0]),
            .context = "show schemas like",
        }
    );
    failures += expect_single_column_result(
        database,
        (struct single_column_result_expectation){
            .sql = "SHOW DATABASES LIKE 'APP'",
            .expected_column_name = "Database (APP)",
            .expected_rows = NULL,
            .expected_row_count = 0U,
            .context = "show databases case-sensitive no match",
        }
    );

    failures += execute_statement_ok(database, "USE app");
    failures += expect_single_column_result(
        database,
        (struct single_column_result_expectation){
            .sql = "SHOW TABLES LIKE 'a%'",
            .expected_column_name = "Tables_in_app (a%)",
            .expected_rows = tables_a_percent,
            .expected_row_count = sizeof(tables_a_percent) / sizeof(tables_a_percent[0]),
            .context = "show tables percent",
        }
    );
    failures += expect_single_column_result(
        database,
        (struct single_column_result_expectation){
            .sql = "SHOW TABLES LIKE 'a_b'",
            .expected_column_name = "Tables_in_app (a_b)",
            .expected_rows = tables_a_wildcard_b,
            .expected_row_count = sizeof(tables_a_wildcard_b) / sizeof(tables_a_wildcard_b[0]),
            .context = "show tables underscore wildcard",
        }
    );
    failures += expect_single_column_result(
        database,
        (struct single_column_result_expectation){
            .sql = "SHOW TABLES LIKE 'a\\_b'",
            .expected_column_name = "Tables_in_app (a\\_b)",
            .expected_rows = table_a_underscore_b,
            .expected_row_count = sizeof(table_a_underscore_b) / sizeof(table_a_underscore_b[0]),
            .context = "show tables escaped underscore",
        }
    );
    failures += expect_single_column_result(
        database,
        (struct single_column_result_expectation){
            .sql = "SHOW TABLES LIKE 'a\\%b'",
            .expected_column_name = "Tables_in_app (a\\%b)",
            .expected_rows = table_a_percent_b,
            .expected_row_count = sizeof(table_a_percent_b) / sizeof(table_a_percent_b[0]),
            .context = "show tables escaped percent",
        }
    );
    failures += expect_single_column_result(
        database,
        (struct single_column_result_expectation){
            .sql = "SHOW TABLES LIKE 'a\\\\\\\\b'",
            .expected_column_name = "Tables_in_app (a\\\\b)",
            .expected_rows = table_a_backslash_b,
            .expected_row_count = sizeof(table_a_backslash_b) / sizeof(table_a_backslash_b[0]),
            .context = "show tables escaped backslash",
        }
    );
    failures += expect_single_column_result(
        database,
        (struct single_column_result_expectation){
            .sql = "SHOW TABLES LIKE 'ALPHA'",
            .expected_column_name = "Tables_in_app (ALPHA)",
            .expected_rows = NULL,
            .expected_row_count = 0U,
            .context = "show tables case-sensitive no match",
        }
    );
    failures += expect_single_column_result(
        database,
        (struct single_column_result_expectation){
            .sql = "SHOW TABLES FROM app LIKE 'b%'",
            .expected_column_name = "Tables_in_app (b%)",
            .expected_rows = table_beta,
            .expected_row_count = sizeof(table_beta) / sizeof(table_beta[0]),
            .context = "show tables from schema",
        }
    );
    failures += expect_single_column_result(
        database,
        (struct single_column_result_expectation){
            .sql = "SHOW TABLES IN other LIKE 'other\\_%'",
            .expected_column_name = "Tables_in_other (other\\_%)",
            .expected_rows = table_other_alpha,
            .expected_row_count = sizeof(table_other_alpha) / sizeof(table_other_alpha[0]),
            .context = "show tables in schema",
        }
    );

    failures += expect_show_columns_result(
        database,
        "SHOW COLUMNS FROM alpha LIKE 'i%'",
        columns_i_percent,
        sizeof(columns_i_percent) / sizeof(columns_i_percent[0]),
        "show columns percent"
    );
    failures += expect_show_columns_result(
        database,
        "SHOW COLUMNS FROM alpha LIKE 'i_'",
        column_id,
        sizeof(column_id) / sizeof(column_id[0]),
        "show columns underscore wildcard"
    );
    failures += expect_show_columns_result(
        database,
        "SHOW FIELDS FROM alpha LIKE 'i\\_1'",
        column_i_1,
        sizeof(column_i_1) / sizeof(column_i_1[0]),
        "show fields escaped underscore"
    );
    failures += expect_show_columns_result(
        database,
        "SHOW COLUMNS FROM alpha LIKE 'a\\%b'",
        column_a_percent_b,
        sizeof(column_a_percent_b) / sizeof(column_a_percent_b[0]),
        "show columns escaped percent"
    );
    failures += expect_show_columns_result(
        database,
        "SHOW COLUMNS FROM alpha LIKE 'a\\_b'",
        column_a_underscore_b,
        sizeof(column_a_underscore_b) / sizeof(column_a_underscore_b[0]),
        "show columns escaped underscore"
    );
    failures += expect_show_columns_result(
        database,
        "SHOW COLUMNS FROM alpha LIKE 'a\\\\\\\\b'",
        column_a_backslash_b,
        sizeof(column_a_backslash_b) / sizeof(column_a_backslash_b[0]),
        "show columns escaped backslash"
    );
    failures += expect_show_columns_result(
        database,
        "SHOW COLUMNS FROM alpha LIKE 'mixedcase'",
        column_mixed_case,
        sizeof(column_mixed_case) / sizeof(column_mixed_case[0]),
        "show columns case-insensitive"
    );
    failures += expect_show_columns_result(
        database,
        "SHOW COLUMNS FROM alpha FROM app LIKE 'name%'",
        column_name_id,
        sizeof(column_name_id) / sizeof(column_name_id[0]),
        "show columns explicit schema like"
    );
    failures += expect_show_columns_result(
        database,
        "SHOW COLUMNS FROM app.alpha LIKE 'literal\\_%'",
        column_literal_percent,
        sizeof(column_literal_percent) / sizeof(column_literal_percent[0]),
        "show columns qualified like"
    );
    failures += expect_show_columns_result(
        database,
        "SHOW COLUMNS FROM alpha LIKE ''",
        NULL,
        0U,
        "show columns empty pattern"
    );
    failures += expect_row_count(database, -1, "row count after show like");
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after show like"
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen values file");
    failures += execute_statement_ok(database, "USE app");
    failures += expect_show_columns_result(
        database,
        "SHOW COLUMNS FROM alpha LIKE 'mixedcase'",
        column_mixed_case,
        sizeof(column_mixed_case) / sizeof(column_mixed_case[0]),
        "reopened show columns like"
    );

    failures += execute_statement_ok(database, "RENAME TABLE alpha TO renamed_alpha");
    failures += expect_single_column_result(
        database,
        (struct single_column_result_expectation){
            .sql = "SHOW TABLES LIKE 'renamed%'",
            .expected_column_name = "Tables_in_app (renamed%)",
            .expected_rows = table_renamed_alpha,
            .expected_row_count = sizeof(table_renamed_alpha) / sizeof(table_renamed_alpha[0]),
            .context = "renamed table show like",
        }
    );
    failures += execute_statement_ok(database, "DROP TABLE renamed_alpha");
    failures += expect_single_column_result(
        database,
        (struct single_column_result_expectation){
            .sql = "SHOW TABLES LIKE 'renamed%'",
            .expected_column_name = "Tables_in_app (renamed%)",
            .expected_rows = NULL,
            .expected_row_count = 0U,
            .context = "dropped table show like",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_show_like_diagnostics_and_unsupported_forms(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics file");
    failures += create_like_schema(database);

    failures += execute_error(
        database,
        "SHOW TABLES LIKE 'a%'",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_error(
        database,
        "SHOW TABLES FROM missing_schema LIKE 'a%'",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'missing_schema'",
        }
    );
    failures += execute_error(
        database,
        "SHOW TABLES FROM _mylite_reserved LIKE 'a%'",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_database_name,
            .sqlstate = "42000",
            .message_part = "Incorrect database name '_mylite_reserved'",
        }
    );
    failures += execute_error(
        database,
        "SHOW COLUMNS FROM missing_schema.alpha LIKE 'i%'",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'missing_schema'",
        }
    );

    failures += execute_statement_ok(database, "USE app");
    failures += execute_error(
        database,
        "SHOW COLUMNS FROM missing_table LIKE 'i%'",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing_table' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "SHOW COLUMNS FROM _mylite_alpha LIKE 'i%'",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name '_mylite_alpha'",
        }
    );
    failures += execute_error(
        database,
        "SHOW TABLES LIKE 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW TABLES LIKE NULL",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW TABLES LIKE N'a%'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW TABLES LIKE 'a\\0%'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SHOW LIKE does not support NUL bytes in patterns",
        }
    );
    failures += execute_error(
        database,
        "SHOW TABLES WHERE Tables_in_app LIKE 'a%'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW COLUMNS FROM alpha WHERE Field LIKE 'i%'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "DESCRIBE alpha 'i%'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "EXPLAIN alpha 'i%'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT 'alpha' LIKE 'a%'",
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

static int test_independent_show_like_handles(void) {
    static const char *const first_rows[] = {"alpha"};
    static const char *const second_rows[] = {"beta"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "independent-a") != 0 ||
        make_test_path(second_path, sizeof(second_path), "independent-b") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first handle");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second handle");

    failures += execute_statement_ok(first, "CREATE DATABASE app");
    failures += execute_statement_ok(first, "USE app");
    failures += execute_statement_ok(first, "CREATE TABLE alpha (id INT NOT NULL)");

    failures += execute_statement_ok(second, "CREATE DATABASE app");
    failures += execute_statement_ok(second, "USE app");
    failures += execute_statement_ok(second, "CREATE TABLE beta (id INT NOT NULL)");

    failures += expect_single_column_result(
        first,
        (struct single_column_result_expectation){
            .sql = "SHOW TABLES LIKE '%'",
            .expected_column_name = "Tables_in_app (%)",
            .expected_rows = first_rows,
            .expected_row_count = sizeof(first_rows) / sizeof(first_rows[0]),
            .context = "first handle show like",
        }
    );
    failures += expect_single_column_result(
        second,
        (struct single_column_result_expectation){
            .sql = "SHOW TABLES LIKE '%'",
            .expected_column_name = "Tables_in_app (%)",
            .expected_rows = second_rows,
            .expected_row_count = sizeof(second_rows) / sizeof(second_rows[0]),
            .context = "second handle show like",
        }
    );

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);
    return failures;
}

static int create_like_schema(mylite_db *database) {
    int failures = 0;

    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "CREATE DATABASE other");
    failures += execute_statement_ok(
        database,
        "CREATE TABLE app.alpha ("
        "id INT NOT NULL, "
        "id2 INT NULL, "
        "i_1 INT NULL, "
        "name_id BIGINT NULL, "
        "literal_percent INT NULL, "
        "MixedCase INT NULL, "
        "`a%b` INT NULL, "
        "`a_b` INT NULL, "
        "`a\\b` INT NULL)"
    );
    failures += execute_statement_ok(database, "CREATE TABLE app.beta (id INT NOT NULL)");
    failures += execute_statement_ok(database, "CREATE TABLE app.`a%b` (id INT NOT NULL)");
    failures += execute_statement_ok(database, "CREATE TABLE app.`a_b` (id INT NOT NULL)");
    failures += execute_statement_ok(database, "CREATE TABLE app.`a\\b` (id INT NOT NULL)");
    failures += execute_statement_ok(database, "CREATE TABLE other.other_alpha (id INT NOT NULL)");

    return failures;
}

static int expect_single_column_result(
    mylite_db *database,
    struct single_column_result_expectation expectation
) {
    mylite_result *result = NULL;
    int failures = 0;

    failures += execute_ok(database, expectation.sql, &result);
    if (result == NULL) {
        return failures + 1;
    }

    failures += expect_size(mylite_result_column_count(result), 1U, expectation.context);
    failures += expect_size(
        mylite_result_row_count(result),
        expectation.expected_row_count,
        expectation.context
    );
    failures += expect_int64(mylite_result_affected_rows(result), 0, expectation.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, expectation.context);
    failures += expect_text_or_null(
        mylite_result_column_name(result, 0U),
        expectation.expected_column_name,
        expectation.context
    );
    for (size_t row_index = 0U; row_index < expectation.expected_row_count; ++row_index) {
        failures += expect_text_or_null(
            mylite_result_value_text(result, row_index, 0U),
            expectation.expected_rows[row_index],
            expectation.context
        );
    }

    mylite_result_free(result);
    return failures;
}

static int expect_show_columns_result(
    mylite_db *database,
    const char *sql,
    const char *const expected_rows[][show_columns_column_count],
    size_t expected_row_count,
    const char *context
) {
    mylite_result *result = NULL;
    int failures = 0;

    failures += execute_ok(database, sql, &result);
    if (result == NULL) {
        return failures + 1;
    }

    failures += expect_size(mylite_result_column_count(result), show_columns_column_count, context);
    failures += expect_size(mylite_result_row_count(result), expected_row_count, context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, context);
    failures += expect_size(mylite_result_warning_count(result), 0U, context);

    for (size_t column_index = 0U; column_index < show_columns_column_count; ++column_index) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, column_index),
            show_columns_names[column_index],
            context
        );
    }
    for (size_t row_index = 0U; row_index < expected_row_count; ++row_index) {
        for (size_t column_index = 0U; column_index < show_columns_column_count; ++column_index) {
            failures += expect_text_or_null(
                mylite_result_value_text(result, row_index, column_index),
                expected_rows[row_index][column_index],
                context
            );
        }
    }

    mylite_result_free(result);
    return failures;
}

static int expect_row_count(mylite_db *database, int64_t expected, const char *context) {
    mylite_result *result = NULL;
    int failures = 0;

    failures += execute_ok(database, "SELECT ROW_COUNT()", &result);
    if (result == NULL) {
        return failures + 1;
    }

    failures += expect_size(mylite_result_column_count(result), 1U, context);
    failures += expect_size(mylite_result_row_count(result), 1U, context);
    if (mylite_result_value_text(result, 0U, 0U) == NULL) {
        fprintf(stderr, "%s: expected row count value\n", context);
        failures += 1;
    } else {
        failures += expect_int64(
            strtoll(mylite_result_value_text(result, 0U, 0U), NULL, decimal_base),
            expected,
            context
        );
    }

    mylite_result_free(result);
    return failures;
}

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    mylite_result_free(result);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected success, got %d/%s: %s\n",
            sql,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        return 1;
    }
    if (out_result == NULL || *out_result == NULL) {
        fprintf(stderr, "%s: expected result object\n", sql);
        return 1;
    }

    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    if (rc == MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected error %d/%s, got success\n",
            sql,
            expected.code,
            expected.sqlstate
        );
        mylite_result_free(result);
        return 1;
    }

    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text_or_null(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_text_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);

    return failures;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written =
        snprintf(path, path_size, "/tmp/mylite_show_like_%s_%d.mylite", name, current_process_id());

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "failed to build test path for %s\n", name);
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
        fprintf(stderr, "%s: failed to open file\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "%s: failed to seek file\n", path);
        fclose(file);
        return 1;
    }
    if (fread(buffer, 1U, size, file) != size) {
        fprintf(stderr, "%s: failed to read file\n", path);
        fclose(file);
        return 1;
    }

    fclose(file);
    return 0;
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual == expected) {
        return 0;
    }

    fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
    return 1;
}

static int expect_int64(int64_t actual, int64_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }

    fprintf(stderr, "%s: expected %" PRId64 ", got %" PRId64 "\n", context, expected, actual);
    return 1;
}

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }

    fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
    return 1;
}

static int expect_text_or_null(const char *actual, const char *expected, const char *context) {
    if (actual == NULL && expected == NULL) {
        return 0;
    }
    if (actual != NULL && expected != NULL && strcmp(actual, expected) == 0) {
        return 0;
    }

    fprintf(
        stderr,
        "%s: expected [%s], got [%s]\n",
        context,
        expected == NULL ? "NULL" : expected,
        actual == NULL ? "NULL" : actual
    );
    return 1;
}

static int expect_text_contains(const char *actual, const char *needle, const char *context) {
    if (actual != NULL && needle != NULL && strstr(actual, needle) != NULL) {
        return 0;
    }

    fprintf(
        stderr,
        "%s: expected [%s] to contain [%s]\n",
        context,
        actual == NULL ? "NULL" : actual,
        needle == NULL ? "NULL" : needle
    );
    return 1;
}

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (memcmp(actual, expected, size) == 0) {
        return 0;
    }

    fprintf(stderr, "%s: byte comparison failed\n", context);
    return 1;
}
