#include "mylite_test_support.h"

#include <mylite/mylite.h>

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
    mysql_error_select_reduced = 1222,
    mysql_error_result_consisted_more_than_one_row = 1172,
    test_path_capacity = 256,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_query {
    const char *sql;
    const char *const *columns;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    int64_t affected_rows;
    size_t warning_count;
    const char *context;
};

struct expected_empty_statement_result {
    int64_t affected_rows;
    size_t warning_count;
};

static int test_select_into_scalar_user_variables(void);
static int test_select_into_table_user_variables(void);
static int test_select_into_zero_rows_warning(void);
static int test_select_into_errors(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int open_file_database(
    const char *name,
    char *path,
    size_t path_size,
    mylite_db **out_database
);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_empty_statement_result(
    mylite_db *database,
    const char *sql,
    struct expected_empty_statement_result expected,
    const char *context
);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_select_into_scalar_user_variables();
    failures += test_select_into_table_user_variables();
    failures += test_select_into_zero_rows_warning();
    failures += test_select_into_errors();

    return failures == 0 ? 0 : 1;
}

static int test_select_into_scalar_user_variables(void) {
    static const char *const columns[] = {
        "@a",
        "@b",
        "@c",
        "ROW_COUNT()",
        "FOUND_ROWS()",
        "@@warning_count",
        "@@error_count",
    };
    static const char *const values[] = {"1", "a", NULL, "1", "1", "1", "0"};
    mylite_db *database = NULL;
    int failures =
        mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open scalar select into");

    if (failures != 0) {
        return failures;
    }

    failures += expect_empty_statement_result(
        database,
        "SELECT 1, 'a', NULL INTO @a, @b, @c",
        (struct expected_empty_statement_result){.affected_rows = 1, .warning_count = 0U},
        "scalar select into result shape"
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @a, @b, @c, ROW_COUNT(), FOUND_ROWS(), @@warning_count, "
                   "@@error_count",
            .columns = columns,
            .values = values,
            .column_count = sizeof(columns) / sizeof(columns[0]),
            .row_count = 1U,
            .affected_rows = 0,
            .warning_count = 1U,
            .context = "scalar select into readback",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_select_into_table_user_variables(void) {
    static const char *const assigned_columns[] = {
        "@id",
        "@name",
        "@first",
        "@locked",
        "@total",
        "@max_id",
        "@group_name",
        "@group_count",
        "@info_table",
        "ROW_COUNT()",
        "FOUND_ROWS()",
    };
    static const char *const assigned_values[] = {
        "2",
        "b",
        "1",
        "1",
        "2",
        "2",
        "a",
        "1",
        "t",
        "1",
        "1",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = open_file_database("table", path, sizeof(path), &database);

    if (failures != 0) {
        return failures;
    }

    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");
    failures += execute_statement_ok(database, "CREATE TABLE t (id INT, name VARCHAR(16))");
    failures += execute_statement_ok(database, "INSERT INTO t VALUES (1, 'a'), (2, 'b')");
    failures += expect_empty_statement_result(
        database,
        "SELECT id, name INTO @id, @name FROM t WHERE id = 2",
        (struct expected_empty_statement_result){.affected_rows = 1, .warning_count = 0U},
        "pre-FROM table select into result shape"
    );
    failures += execute_statement_ok(database, "SELECT id FROM t ORDER BY id LIMIT 1 INTO @first");
    failures += expect_empty_statement_result(
        database,
        "SELECT id FROM t ORDER BY id LIMIT 1 FOR UPDATE INTO @locked",
        (struct expected_empty_statement_result){.affected_rows = 1, .warning_count = 0U},
        "post-locking table select into result shape"
    );
    failures += expect_empty_statement_result(
        database,
        "SELECT COUNT(*) INTO @total FROM t",
        (struct expected_empty_statement_result){.affected_rows = 1, .warning_count = 0U},
        "aggregate select into result shape"
    );
    failures += expect_empty_statement_result(
        database,
        "SELECT MAX(id) INTO @max_id FROM t",
        (struct expected_empty_statement_result){.affected_rows = 1, .warning_count = 0U},
        "column aggregate select into result shape"
    );
    failures += expect_empty_statement_result(
        database,
        "SELECT name, COUNT(*) INTO @group_name, @group_count FROM t GROUP BY name ORDER BY name "
        "LIMIT 1",
        (struct expected_empty_statement_result){.affected_rows = 1, .warning_count = 0U},
        "grouped aggregate select into result shape"
    );
    failures += expect_empty_statement_result(
        database,
        "SELECT TABLE_NAME INTO @info_table FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = "
        "'app' AND TABLE_NAME = 't' LIMIT 1",
        (struct expected_empty_statement_result){.affected_rows = 1, .warning_count = 0U},
        "information schema select into result shape"
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @id, @name, @first, @locked, @total, @max_id, @group_name, "
                   "@group_count, @info_table, ROW_COUNT(), FOUND_ROWS()",
            .columns = assigned_columns,
            .values = assigned_values,
            .column_count = sizeof(assigned_columns) / sizeof(assigned_columns[0]),
            .row_count = 1U,
            .affected_rows = 0,
            .warning_count = 1U,
            .context = "table select into readback",
        }
    );

    mylite_close(database);
    remove(path);
    return failures;
}

static int test_select_into_zero_rows_warning(void) {
    static const char *const columns[] = {
        "@missing",
        "ROW_COUNT()",
        "FOUND_ROWS()",
        "@@warning_count",
        "@@error_count",
    };
    static const char *const values[] = {"keep", "0", "0", "1", "0"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = open_file_database("zero", path, sizeof(path), &database);

    if (failures != 0) {
        return failures;
    }

    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");
    failures += execute_statement_ok(database, "CREATE TABLE t (id INT, name VARCHAR(16))");
    failures += execute_statement_ok(database, "INSERT INTO t VALUES (1, 'a'), (2, 'b')");
    failures += execute_statement_ok(database, "SET @missing = 'keep'");
    failures += expect_empty_statement_result(
        database,
        "SELECT id INTO @missing FROM t WHERE id = 99",
        (struct expected_empty_statement_result){.affected_rows = 0, .warning_count = 1U},
        "zero-row select into result shape"
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @missing, ROW_COUNT(), FOUND_ROWS(), @@warning_count, @@error_count",
            .columns = columns,
            .values = values,
            .column_count = sizeof(columns) / sizeof(columns[0]),
            .row_count = 1U,
            .affected_rows = 0,
            .warning_count = 1U,
            .context = "zero-row select into leaves variable unchanged",
        }
    );

    mylite_close(database);
    remove(path);
    return failures;
}

static int test_select_into_errors(void) {
    static const char *const columns[] = {"@too_many", "@mismatch", "ROW_COUNT()", "@@error_count"};
    static const char *const values[] = {"keep", "keep", "-1", "1"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = open_file_database("errors", path, sizeof(path), &database);

    if (failures != 0) {
        return failures;
    }

    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");
    failures += execute_statement_ok(database, "CREATE TABLE t (id INT, name VARCHAR(16))");
    failures += execute_statement_ok(database, "INSERT INTO t VALUES (1, 'a'), (2, 'b')");
    failures += execute_statement_ok(database, "SET @too_many = 'keep', @mismatch = 'keep'");
    failures += execute_error(
        database,
        "SELECT id INTO @too_many FROM t",
        (struct expected_sql_error){
            .code = mysql_error_result_consisted_more_than_one_row,
            .sqlstate = "42000",
            .message_part = "Result consisted of more than one row",
        }
    );
    failures += execute_error(
        database,
        "SELECT id, name INTO @mismatch FROM t LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_select_reduced,
            .sqlstate = "21000",
            .message_part = "different number of columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @too_many, @mismatch, ROW_COUNT(), @@error_count",
            .columns = columns,
            .values = values,
            .column_count = sizeof(columns) / sizeof(columns[0]),
            .row_count = 1U,
            .affected_rows = 0,
            .warning_count = 0U,
            .context = "select into errors leave variables unchanged",
        }
    );

    mylite_close(database);
    remove(path);
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

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    mylite_result_free(result);
    return failures;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc == MYLITE_OK) {
        fprintf(stderr, "%s: expected error, got success\n", sql);
        mylite_result_free(result);
        return 1;
    }

    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, "error code");
    failures +=
        mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, "error sqlstate");
    failures += mylite_test_expect_contains(
        mylite_errmsg(database),
        expected.message_part,
        "error message"
    );
    mylite_result_free(result);
    return failures;
}

static int open_file_database(
    const char *name,
    char *path,
    size_t path_size,
    mylite_db **out_database
) {
    int failures = 0;

    if (mylite_test_make_path(path, path_size, name) != 0) {
        return 1;
    }
    remove(path);
    failures +=
        mylite_test_expect_int(mylite_open(path, out_database), MYLITE_OK, "open file database");
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, expected.sql, &result);

    if (failures != 0) {
        mylite_result_free(result);
        return failures;
    }
    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        expected.column_count,
        expected.context
    );
    failures += mylite_test_expect_size(
        mylite_result_row_count(result),
        expected.row_count,
        expected.context
    );
    failures += mylite_test_expect_int64(
        mylite_result_affected_rows(result),
        expected.affected_rows,
        expected.context
    );
    failures += mylite_test_expect_size(
        mylite_result_warning_count(result),
        expected.warning_count,
        expected.context
    );
    for (size_t column = 0U; column < expected.column_count; ++column) {
        failures += mylite_test_expect_text(
            mylite_result_column_name(result, column),
            expected.columns[column],
            expected.context
        );
        failures +=
            expect_result_value(result, 0U, column, expected.values[column], expected.context);
    }

    mylite_result_free(result);
    return failures;
}

static int expect_empty_statement_result(
    mylite_db *database,
    const char *sql,
    struct expected_empty_statement_result expected,
    const char *context
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures != 0) {
        mylite_result_free(result);
        return failures;
    }
    failures += mylite_test_expect_size(mylite_result_column_count(result), 0U, context);
    failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, context);
    failures += mylite_test_expect_int64(
        mylite_result_affected_rows(result),
        expected.affected_rows,
        context
    );
    failures += mylite_test_expect_size(
        mylite_result_warning_count(result),
        expected.warning_count,
        context
    );

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
    return mylite_test_expect_text(
        mylite_result_value_text(result, row, column),
        expected,
        context
    );
}
