#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    mysql_collation_binary_id = 63,
    mysql_error_native_function_arity = 1582,
    mysql_validate_password_strength_display_length = 10,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_query {
    const char *sql;
    const char *const *columns;
    size_t column_count;
    const char *const *values;
    size_t row_count;
    size_t warning_count;
    int64_t affected_rows;
    const char *context;
};

struct expected_column_metadata {
    enum mylite_result_column_type type;
    uint32_t flags;
    uint32_t charset_id;
    uint32_t collation_id;
    uint64_t display_length;
    uint16_t decimals;
    int nullable;
    const char *context;
};

static int test_scalar_validate_password_strength(void);
static int test_dual_do_and_dml_contexts(void);
static int test_table_backed_validate_password_strength(void);
static int test_validate_password_strength_diagnostics(void);
static int test_validate_password_strength_metadata(void);
static int setup_database(mylite_db **out_database);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int expect_column_metadata(
    const mylite_result *result,
    size_t column,
    struct expected_column_metadata expected
);

int main(void) {
    int failures = 0;

    failures += test_scalar_validate_password_strength();
    failures += test_dual_do_and_dml_contexts();
    failures += test_table_backed_validate_password_strength();
    failures += test_validate_password_strength_diagnostics();
    failures += test_validate_password_strength_metadata();

    return failures == 0 ? 0 : 1;
}

static int test_scalar_validate_password_strength(void) {
    static const char *const columns[] = {
        "plain",
        "empty_password",
        "null_password",
        "integer_password",
        "true_password",
        "false_password",
        "binary_password",
        "comparison",
        "plus_one",
    };
    static const char *const values[] = {
        "0",
        "0",
        NULL,
        "0",
        "0",
        "0",
        "0",
        "1",
        "1",
    };
    static const char *const warning_columns[] = {"warnings"};
    static const char *const warning_values[] = {"0"};
    mylite_db *database = NULL;
    int failures = setup_database(&database);

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT VALIDATE_PASSWORD_STRENGTH('abc') AS plain, "
                   "VALIDATE_PASSWORD_STRENGTH('') AS empty_password, "
                   "VALIDATE_PASSWORD_STRENGTH(NULL) AS null_password, "
                   "VALIDATE_PASSWORD_STRENGTH(123) AS integer_password, "
                   "VALIDATE_PASSWORD_STRENGTH(TRUE) AS true_password, "
                   "VALIDATE_PASSWORD_STRENGTH(FALSE) AS false_password, "
                   "VALIDATE_PASSWORD_STRENGTH(_binary'abc') AS binary_password, "
                   "VALIDATE_PASSWORD_STRENGTH('abc') = 0 AS comparison, "
                   "VALIDATE_PASSWORD_STRENGTH('abc') + 1 AS plus_one",
            .columns = columns,
            .column_count = sizeof(columns) / sizeof(columns[0]),
            .values = values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "validate password strength scalar values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count AS warnings",
            .columns = warning_columns,
            .column_count = sizeof(warning_columns) / sizeof(warning_columns[0]),
            .values = warning_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "validate password strength scalar warnings",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_dual_do_and_dml_contexts(void) {
    static const char *const dual_columns[] = {"strength", "null_strength"};
    static const char *const dual_values[] = {"0", NULL};
    static const char *const assignment_columns[] = {"assigned_strength", "row_count"};
    static const char *const assignment_values[] = {"0", "0"};
    static const char *const dml_columns[] = {"id", "strength"};
    static const char *const insert_values[] = {"1", "0"};
    static const char *const update_values[] = {"1", NULL};
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = setup_database(&database);

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT VALIDATE_PASSWORD_STRENGTH('abc') AS strength, "
                   "VALIDATE_PASSWORD_STRENGTH(NULL) AS null_strength FROM DUAL",
            .columns = dual_columns,
            .column_count = sizeof(dual_columns) / sizeof(dual_columns[0]),
            .values = dual_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "validate password strength DUAL",
        }
    );

    failures += execute_ok(
        database,
        "DO VALIDATE_PASSWORD_STRENGTH('abc'), VALIDATE_PASSWORD_STRENGTH(NULL)",
        &result
    );
    if (result != NULL) {
        failures +=
            mylite_test_expect_size(mylite_result_column_count(result), 0U, "validate DO columns");
        failures +=
            mylite_test_expect_size(mylite_result_row_count(result), 0U, "validate DO rows");
        failures += mylite_test_expect_size(
            mylite_result_warning_count(result),
            0U,
            "validate DO warnings"
        );
        failures += mylite_test_expect_int64(
            mylite_result_affected_rows(result),
            0,
            "validate DO affected"
        );
    }
    mylite_result_free(result);

    failures += execute_ok(database, "SET @strength = VALIDATE_PASSWORD_STRENGTH('abc')", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @strength AS assigned_strength, ROW_COUNT() AS row_count",
            .columns = assignment_columns,
            .column_count = sizeof(assignment_columns) / sizeof(assignment_columns[0]),
            .values = assignment_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "validate password strength user variable",
        }
    );

    failures += execute_ok(database, "CREATE TABLE values_t(id INT, strength INT)", NULL);
    failures += execute_ok(
        database,
        "INSERT INTO values_t VALUES (1, VALIDATE_PASSWORD_STRENGTH('abc'))",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, strength FROM values_t",
            .columns = dml_columns,
            .column_count = sizeof(dml_columns) / sizeof(dml_columns[0]),
            .values = insert_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "validate password strength insert value",
        }
    );
    failures += execute_ok(
        database,
        "UPDATE values_t SET strength = VALIDATE_PASSWORD_STRENGTH(NULL) WHERE id = 1",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, strength FROM values_t",
            .columns = dml_columns,
            .column_count = sizeof(dml_columns) / sizeof(dml_columns[0]),
            .values = update_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "validate password strength update value",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_table_backed_validate_password_strength(void) {
    static const char *const projection_columns[] = {"id", "strength", "null_strength"};
    static const char *const projection_values[] = {
        "1",
        "0",
        NULL,
        "2",
        NULL,
        NULL,
        "3",
        "0",
        NULL,
    };
    static const char *const predicate_columns[] = {"id"};
    static const char *const predicate_values[] = {"1", "3"};
    mylite_db *database = NULL;
    int failures = setup_database(&database);

    failures += execute_ok(database, "CREATE TABLE vp(id INT, p VARCHAR(20))", NULL);
    failures +=
        execute_ok(database, "INSERT INTO vp VALUES (1,'abc'),(2,NULL),(3,'N0Tweak')", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, VALIDATE_PASSWORD_STRENGTH(p) AS strength, "
                   "VALIDATE_PASSWORD_STRENGTH(NULL) AS null_strength FROM vp ORDER BY id",
            .columns = projection_columns,
            .column_count = sizeof(projection_columns) / sizeof(projection_columns[0]),
            .values = projection_values,
            .row_count = 3U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "validate password strength row projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM vp WHERE VALIDATE_PASSWORD_STRENGTH(p) = 0 ORDER BY id",
            .columns = predicate_columns,
            .column_count = sizeof(predicate_columns) / sizeof(predicate_columns[0]),
            .values = predicate_values,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "validate password strength row predicate",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_validate_password_strength_diagnostics(void) {
    mylite_db *database = NULL;
    int failures = setup_database(&database);

    failures += execute_error(
        database,
        "SELECT VALIDATE_PASSWORD_STRENGTH()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_arity,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'VALIDATE_PASSWORD_STRENGTH'",
        }
    );
    failures += execute_error(
        database,
        "SELECT VALIDATE_PASSWORD_STRENGTH('a','b')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_arity,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'VALIDATE_PASSWORD_STRENGTH'",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_validate_password_strength_metadata(void) {
    static const char *const values[] = {"0", NULL};
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = setup_database(&database);

    failures += execute_ok(
        database,
        "SELECT VALIDATE_PASSWORD_STRENGTH('abc') AS strength, "
        "VALIDATE_PASSWORD_STRENGTH(NULL) AS n",
        &result
    );
    if (result != NULL) {
        failures += mylite_test_expect_size(
            mylite_result_column_count(result),
            2U,
            "validate metadata cols"
        );
        failures +=
            mylite_test_expect_size(mylite_result_row_count(result), 1U, "validate metadata rows");
        failures += expect_result_value(result, 0U, 0U, values[0], "validate metadata value");
        failures += expect_result_value(result, 0U, 1U, values[1], "validate metadata NULL");
        for (size_t column = 0U; column < 2U; ++column) {
            failures += expect_column_metadata(
                result,
                column,
                (struct expected_column_metadata){
                    .type = MYLITE_RESULT_COLUMN_TYPE_LONGLONG,
                    .flags = MYLITE_RESULT_COLUMN_FLAG_BINARY | MYLITE_RESULT_COLUMN_FLAG_NUM,
                    .charset_id = mysql_collation_binary_id,
                    .collation_id = mysql_collation_binary_id,
                    .display_length = mysql_validate_password_strength_display_length,
                    .decimals = 0U,
                    .nullable = 1,
                    .context = "validate password strength metadata",
                }
            );
        }
    }
    mylite_result_free(result);

    mylite_close(database);
    return failures;
}

static int setup_database(mylite_db **out_database) {
    int failures = 0;

    failures +=
        mylite_test_expect_int(mylite_test_open_temporary(out_database), MYLITE_OK, "open db");
    failures += execute_ok(*out_database, "CREATE DATABASE app", NULL);
    failures += execute_ok(*out_database, "USE app", NULL);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    failures += mylite_test_expect_int(rc, MYLITE_OK, sql);
    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: %s\n", sql, mylite_errmsg(database));
        return failures;
    }
    if (out_result != NULL) {
        *out_result = result;
    } else {
        mylite_result_free(result);
    }
    return failures;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    if (rc == MYLITE_OK) {
        fprintf(stderr, "%s: expected error, got success\n", sql);
        mylite_result_free(result);
        return 1;
    }
    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, sql);
    failures += mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += mylite_test_expect_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, expected.sql, &result);

    if (failures != 0) {
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
    failures += mylite_test_expect_size(
        mylite_result_warning_count(result),
        expected.warning_count,
        expected.context
    );
    failures += mylite_test_expect_int64(
        mylite_result_affected_rows(result),
        expected.affected_rows,
        expected.context
    );
    for (size_t column = 0U; column < expected.column_count; ++column) {
        failures += mylite_test_expect_text(
            mylite_result_column_name(result, column),
            expected.columns[column],
            expected.context
        );
    }
    for (size_t row = 0U; row < expected.row_count; ++row) {
        for (size_t column = 0U; column < expected.column_count; ++column) {
            failures += expect_result_value(
                result,
                row,
                column,
                expected.values[(row * expected.column_count) + column],
                expected.context
            );
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
        if (actual != NULL) {
            fprintf(stderr, "%s: expected NULL at %zu,%zu, got %s\n", context, row, column, actual);
            return 1;
        }
        return 0;
    }
    return mylite_test_expect_text(actual, expected, context);
}

static int expect_column_metadata(
    const mylite_result *result,
    size_t column,
    struct expected_column_metadata expected
) {
    int failures = 0;

    failures += mylite_test_expect_int(
        mylite_result_column_type(result, column),
        expected.type,
        expected.context
    );
    failures += mylite_test_expect_int64(
        mylite_result_column_flags(result, column),
        expected.flags,
        expected.context
    );
    failures += mylite_test_expect_int64(
        mylite_result_column_charset_id(result, column),
        expected.charset_id,
        expected.context
    );
    failures += mylite_test_expect_int64(
        mylite_result_column_collation_id(result, column),
        expected.collation_id,
        expected.context
    );
    failures += mylite_test_expect_int64(
        (int64_t)mylite_result_column_display_length(result, column),
        (int64_t)expected.display_length,
        expected.context
    );
    failures += mylite_test_expect_int(
        mylite_result_column_decimals(result, column),
        expected.decimals,
        expected.context
    );
    failures += mylite_test_expect_int(
        mylite_result_column_nullable(result, column),
        expected.nullable,
        expected.context
    );
    return failures;
}
