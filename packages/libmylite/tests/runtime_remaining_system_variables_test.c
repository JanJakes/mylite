#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    mysql_error_parse = 1064,
    mysql_error_unknown_system_variable = 1193,
    mysql_error_session_variable_set_global = 1228,
    mysql_error_global_variable_set_global_required = 1229,
    mysql_error_no_default = 1230,
    mysql_error_incorrect_argument_type = 1232,
    mysql_error_wrong_scope = 1238,
    decimal_radix = 10,
    temptable_minimum_ram_minus_one = 1073741823,
};

static const double rand_seed_first_lower_bound = 0.000000004656612876;
static const double rand_seed_first_upper_bound = 0.000000004656612878;
static const double rand_seed_second_lower_bound = 0.000000051222741651;
static const double rand_seed_second_upper_bound = 0.000000051222741653;

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_result {
    const char *const *columns;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    const char *context;
};

struct expected_statement {
    int64_t affected_rows;
    size_t warning_count;
};

static int test_remaining_system_variable_readback_and_show(void);
static int test_insert_id_auto_increment_allocation(void);
static int test_pseudo_thread_id_and_rand_seed_state(void);
static int test_statement_id_and_scope_diagnostics(void);
static int test_temptable_global_placeholder(void);
static int test_remaining_system_variable_independent_handles(void);
static int expect_query_result(
    mylite_db *database,
    const char *sql,
    struct expected_result expected
);
static int expect_result(const mylite_result *result, struct expected_result expected);
static int expect_nonquery_result(
    mylite_db *database,
    const char *sql,
    struct expected_statement expected
);
static int expect_statement_id_increases(mylite_db *database);
static int expect_pseudo_thread_id_matches_connection_id(mylite_db *database);
static int expect_rand_seed_sequence(mylite_db *database);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_uint64_greater_than(uint64_t actual, uint64_t minimum, const char *context);

int main(void) {
    int failures = 0;

    failures += test_remaining_system_variable_readback_and_show();
    failures += test_insert_id_auto_increment_allocation();
    failures += test_pseudo_thread_id_and_rand_seed_state();
    failures += test_statement_id_and_scope_diagnostics();
    failures += test_temptable_global_placeholder();
    failures += test_remaining_system_variable_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_remaining_system_variable_readback_and_show(void) {
    static const char *const scalar_columns[] = {
        "@@insert_id",
        "@@rand_seed1",
        "@@rand_seed2",
        "@@GLOBAL.open_files_limit",
    };
    static const char *const scalar_values[] = {"0", "0", "0", "8161"};
    static const char *const show_columns[] = {"Variable_name", "Value"};
    static const char *const show_values[] = {
        "insert_id",
        "0",
        "open_files_limit",
        "8161",
        "rand_seed1",
        "0",
        "rand_seed2",
        "0",
    };
    static const char *const global_show_values[] = {
        "open_files_limit",
        "8161",
    };
    mylite_db *database = NULL;
    int failures =
        mylite_test_expect_int(mylite_test_open_temporary(&database), MYLITE_OK, "open readback");

    failures += expect_query_result(
        database,
        "SELECT @@insert_id, @@rand_seed1, @@rand_seed2, @@GLOBAL.open_files_limit",
        (struct expected_result){
            .columns = scalar_columns,
            .values = scalar_values,
            .column_count = sizeof(scalar_columns) / sizeof(scalar_columns[0]),
            .row_count = 1U,
            .context = "remaining scalar defaults",
        }
    );
    failures += expect_query_result(
        database,
        "SHOW VARIABLES WHERE Variable_name IN "
        "('insert_id','open_files_limit','rand_seed1','rand_seed2')",
        (struct expected_result){
            .columns = show_columns,
            .values = show_values,
            .column_count = 2U,
            .row_count = 4U,
            .context = "remaining show rows",
        }
    );
    failures += expect_query_result(
        database,
        "SHOW GLOBAL VARIABLES WHERE Variable_name IN "
        "('insert_id','open_files_limit','rand_seed1','rand_seed2')",
        (struct expected_result){
            .columns = show_columns,
            .values = global_show_values,
            .column_count = 2U,
            .row_count = 1U,
            .context = "remaining global show rows",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_insert_id_auto_increment_allocation(void) {
    static const char *const set_columns[] = {"@@insert_id", "LAST_INSERT_ID()"};
    static const char *const set_values[] = {"100", "0"};
    static const char *const first_insert_columns[] = {"id", "@@insert_id", "LAST_INSERT_ID()"};
    static const char *const first_insert_values[] = {"100", "0", "100"};
    static const char *const multi_insert_columns[] = {"id", "name"};
    static const char *const multi_insert_values[] = {
        "100",
        "a",
        "200",
        "b",
        "201",
        "c",
    };
    static const char *const final_columns[] = {"@@insert_id", "LAST_INSERT_ID()"};
    static const char *const final_values[] = {"0", "200"};
    static const char *const warning_columns[] = {"Level", "Code", "Message"};
    static const char *const negative_warning[] = {
        "Warning",
        "1292",
        "Truncated incorrect insert_id value: '-1'",
    };
    mylite_db *database = NULL;
    int failures =
        mylite_test_expect_int(mylite_test_open_temporary(&database), MYLITE_OK, "open insert_id");

    failures += expect_nonquery_result(
        database,
        "CREATE DATABASE app",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_nonquery_result(
        database,
        "USE app",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_nonquery_result(
        database,
        "CREATE TABLE t (id INT AUTO_INCREMENT PRIMARY KEY, name VARCHAR(10))",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_nonquery_result(
        database,
        "SET insert_id = 100",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_query_result(
        database,
        "SELECT @@insert_id, LAST_INSERT_ID()",
        (struct expected_result){
            .columns = set_columns,
            .values = set_values,
            .column_count = 2U,
            .row_count = 1U,
            .context = "insert_id set readback",
        }
    );
    failures += expect_nonquery_result(
        database,
        "INSERT INTO t (name) VALUES ('a')",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_result(
        database,
        "SELECT id, @@insert_id, LAST_INSERT_ID() FROM t",
        (struct expected_result){
            .columns = first_insert_columns,
            .values = first_insert_values,
            .column_count = 3U,
            .row_count = 1U,
            .context = "insert_id single allocation",
        }
    );
    failures += expect_nonquery_result(
        database,
        "SET insert_id = 200",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_nonquery_result(
        database,
        "INSERT INTO t (name) VALUES ('b'),('c')",
        (struct expected_statement){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_query_result(
        database,
        "SELECT id, name FROM t ORDER BY id",
        (struct expected_result){
            .columns = multi_insert_columns,
            .values = multi_insert_values,
            .column_count = 2U,
            .row_count = 3U,
            .context = "insert_id multi allocation",
        }
    );
    failures += expect_query_result(
        database,
        "SELECT @@insert_id, LAST_INSERT_ID()",
        (struct expected_result){
            .columns = final_columns,
            .values = final_values,
            .column_count = 2U,
            .row_count = 1U,
            .context = "insert_id reset after generated insert",
        }
    );
    failures += expect_nonquery_result(
        database,
        "SET insert_id = -1",
        (struct expected_statement){.affected_rows = 0, .warning_count = 1U}
    );
    failures += expect_query_result(
        database,
        "SHOW WARNINGS LIMIT 1",
        (struct expected_result){
            .columns = warning_columns,
            .values = negative_warning,
            .column_count = 3U,
            .row_count = 1U,
            .context = "negative insert_id warning",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_pseudo_thread_id_and_rand_seed_state(void) {
    static const char *const pseudo_set_columns[] = {"@@pseudo_thread_id", "CONNECTION_ID()"};
    static const char *const pseudo_set_values[] = {"456", "456"};
    static const char *const pseudo_default_reset_values[] = {"0", "0"};
    static const char *const warning_columns[] = {"Level", "Code", "Message"};
    static const char *const pseudo_negative_warning[] = {
        "Warning",
        "1292",
        "Truncated incorrect pseudo_thread_id value: '-1'",
    };
    static const char *const pseudo_high_warning[] = {
        "Warning",
        "1292",
        "Truncated incorrect pseudo_thread_id value: '18446744073709551615'",
    };
    static const char *const pseudo_high_columns[] = {"@@pseudo_thread_id"};
    static const char *const pseudo_high_values[] = {"4294967295"};
    static const char *const rand_readback_columns[] = {"@@rand_seed1", "@@rand_seed2"};
    static const char *const rand_readback_values[] = {"0", "0"};
    static const char *const rand_negative_warning[] = {
        "Warning",
        "1292",
        "Truncated incorrect rand_seed1 value: '-1'",
    };
    mylite_db *database = NULL;
    int failures = mylite_test_expect_int(
        mylite_test_open_temporary(&database),
        MYLITE_OK,
        "open pseudo/rand"
    );

    failures += expect_pseudo_thread_id_matches_connection_id(database);
    failures += expect_nonquery_result(
        database,
        "SET pseudo_thread_id = 456",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_query_result(
        database,
        "SELECT @@pseudo_thread_id, CONNECTION_ID()",
        (struct expected_result){
            .columns = pseudo_set_columns,
            .values = pseudo_set_values,
            .column_count = 2U,
            .row_count = 1U,
            .context = "pseudo set connection id",
        }
    );
    failures += expect_nonquery_result(
        database,
        "SET pseudo_thread_id = DEFAULT",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_query_result(
        database,
        "SELECT @@pseudo_thread_id, CONNECTION_ID()",
        (struct expected_result){
            .columns = pseudo_set_columns,
            .values = pseudo_default_reset_values,
            .column_count = 2U,
            .row_count = 1U,
            .context = "pseudo default reset",
        }
    );
    failures += expect_nonquery_result(
        database,
        "SET pseudo_thread_id = -1",
        (struct expected_statement){.affected_rows = 0, .warning_count = 1U}
    );
    failures += expect_query_result(
        database,
        "SHOW WARNINGS LIMIT 1",
        (struct expected_result){
            .columns = warning_columns,
            .values = pseudo_negative_warning,
            .column_count = 3U,
            .row_count = 1U,
            .context = "pseudo negative warning",
        }
    );
    failures += expect_nonquery_result(
        database,
        "SET pseudo_thread_id = 18446744073709551615",
        (struct expected_statement){.affected_rows = 0, .warning_count = 1U}
    );
    failures += expect_query_result(
        database,
        "SHOW WARNINGS LIMIT 1",
        (struct expected_result){
            .columns = warning_columns,
            .values = pseudo_high_warning,
            .column_count = 3U,
            .row_count = 1U,
            .context = "pseudo high warning",
        }
    );
    failures += expect_query_result(
        database,
        "SELECT @@pseudo_thread_id",
        (struct expected_result){
            .columns = pseudo_high_columns,
            .values = pseudo_high_values,
            .column_count = 1U,
            .row_count = 1U,
            .context = "pseudo high clamp",
        }
    );

    failures += expect_nonquery_result(
        database,
        "SET rand_seed1 = 1",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_nonquery_result(
        database,
        "SET rand_seed2 = 2",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_query_result(
        database,
        "SELECT @@rand_seed1, @@rand_seed2",
        (struct expected_result){
            .columns = rand_readback_columns,
            .values = rand_readback_values,
            .column_count = 2U,
            .row_count = 1U,
            .context = "rand_seed readback",
        }
    );
    failures += expect_rand_seed_sequence(database);
    failures += expect_nonquery_result(
        database,
        "SET rand_seed1 = -1",
        (struct expected_statement){.affected_rows = 0, .warning_count = 1U}
    );
    failures += expect_query_result(
        database,
        "SHOW WARNINGS LIMIT 1",
        (struct expected_result){
            .columns = warning_columns,
            .values = rand_negative_warning,
            .column_count = 3U,
            .row_count = 1U,
            .context = "rand negative warning",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_statement_id_and_scope_diagnostics(void) {
    mylite_db *database = NULL;
    int failures =
        mylite_test_expect_int(mylite_test_open_temporary(&database), MYLITE_OK, "open statement");

    failures += expect_statement_id_increases(database);
    failures += execute_error(
        database,
        "SET statement_id = 1",
        (struct expected_sql_error){
            .code = mysql_error_wrong_scope,
            .sqlstate = "HY000",
            .message_part = "read only variable",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@SESSION.open_files_limit",
        (struct expected_sql_error){
            .code = mysql_error_wrong_scope,
            .sqlstate = "HY000",
            .message_part = "is a GLOBAL variable",
        }
    );
    failures += execute_error(
        database,
        "SET GLOBAL open_files_limit = DEFAULT",
        (struct expected_sql_error){
            .code = mysql_error_wrong_scope,
            .sqlstate = "HY000",
            .message_part = "read only variable",
        }
    );
    failures += execute_error(
        database,
        "SET insert_id = DEFAULT",
        (struct expected_sql_error){
            .code = mysql_error_no_default,
            .sqlstate = "42000",
            .message_part = "doesn't have a default value",
        }
    );
    failures += execute_error(
        database,
        "SET insert_id = NULL",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_argument_type,
            .sqlstate = "42000",
            .message_part = "Incorrect argument type",
        }
    );
    failures += execute_error(
        database,
        "SET pseudo_thread_id = 'abc'",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_argument_type,
            .sqlstate = "42000",
            .message_part = "Incorrect argument type",
        }
    );
    failures += execute_error(
        database,
        "SET GLOBAL rand_seed1 = 1",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_set_global,
            .sqlstate = "HY000",
            .message_part = "can't be used with SET GLOBAL",
        }
    );
    failures += execute_error(
        database,
        "SET rand_seed1 = DEFAULT",
        (struct expected_sql_error){
            .code = mysql_error_no_default,
            .sqlstate = "42000",
            .message_part = "doesn't have a default value",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@unknown_remaining_variable",
        (struct expected_sql_error){
            .code = mysql_error_unknown_system_variable,
            .sqlstate = "HY000",
            .message_part = "Unknown system variable",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_temptable_global_placeholder(void) {
    static const char *const set_columns[] = {"@@GLOBAL.temptable_max_ram"};
    static const char *const override_values[] = {"1073741824"};
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    uint64_t default_value = 0U;
    int failures =
        mylite_test_expect_int(mylite_test_open_temporary(&database), MYLITE_OK, "open temptable");

    failures += execute_ok(database, "SELECT @@GLOBAL.temptable_max_ram", &result);
    if (result != NULL) {
        default_value = strtoull(mylite_result_value_text(result, 0U, 0U), NULL, decimal_radix);
        failures += expect_uint64_greater_than(
            default_value,
            temptable_minimum_ram_minus_one,
            "temptable computed default"
        );
    } else {
        failures += 1;
    }
    mylite_result_free(result);
    result = NULL;

    failures += execute_error(
        database,
        "SELECT @@SESSION.temptable_max_ram",
        (struct expected_sql_error){
            .code = mysql_error_wrong_scope,
            .sqlstate = "HY000",
            .message_part = "is a GLOBAL variable",
        }
    );
    failures += execute_error(
        database,
        "SET SESSION temptable_max_ram = DEFAULT",
        (struct expected_sql_error){
            .code = mysql_error_global_variable_set_global_required,
            .sqlstate = "HY000",
            .message_part = "should be set with SET GLOBAL",
        }
    );
    failures += expect_nonquery_result(
        database,
        "SET GLOBAL temptable_max_ram = 1073741824",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_query_result(
        database,
        "SELECT @@GLOBAL.temptable_max_ram",
        (struct expected_result){
            .columns = set_columns,
            .values = override_values,
            .column_count = 1U,
            .row_count = 1U,
            .context = "temptable override",
        }
    );
    failures += expect_nonquery_result(
        database,
        "SET GLOBAL temptable_max_ram = DEFAULT",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += execute_ok(database, "SELECT @@GLOBAL.temptable_max_ram", &result);
    if (result != NULL) {
        const uint64_t reset_value =
            strtoull(mylite_result_value_text(result, 0U, 0U), NULL, decimal_radix);

        failures += mylite_test_expect_int64(
            reset_value == default_value ? 1 : 0,
            1,
            "temptable default reset"
        );
    } else {
        failures += 1;
    }
    mylite_result_free(result);

    mylite_close(database);
    return failures;
}

static int test_remaining_system_variable_independent_handles(void) {
    static const char *const first_columns[] = {
        "@@insert_id",
        "@@pseudo_thread_id",
        "@@GLOBAL.temptable_max_ram",
    };
    static const char *const first_values[] = {"17", "222", "1073741824"};
    static const char *const second_columns[] = {"@@insert_id"};
    static const char *const second_values[] = {"0"};
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures =
        mylite_test_expect_int(mylite_test_open_temporary(&first), MYLITE_OK, "open first");

    failures +=
        mylite_test_expect_int(mylite_test_open_temporary(&second), MYLITE_OK, "open second");
    failures += expect_nonquery_result(
        first,
        "SET insert_id = 17",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_nonquery_result(
        first,
        "SET pseudo_thread_id = 222",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_nonquery_result(
        first,
        "SET GLOBAL temptable_max_ram = 1073741824",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_query_result(
        first,
        "SELECT @@insert_id, @@pseudo_thread_id, @@GLOBAL.temptable_max_ram",
        (struct expected_result){
            .columns = first_columns,
            .values = first_values,
            .column_count = 3U,
            .row_count = 1U,
            .context = "first handle remaining values",
        }
    );
    failures += expect_query_result(
        second,
        "SELECT @@insert_id",
        (struct expected_result){
            .columns = second_columns,
            .values = second_values,
            .column_count = 1U,
            .row_count = 1U,
            .context = "second handle insert_id default",
        }
    );

    mylite_close(second);
    mylite_close(first);
    return failures;
}

static int expect_query_result(
    mylite_db *database,
    const char *sql,
    struct expected_result expected
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_result(result, expected);
    mylite_result_free(result);
    return failures;
}

static int expect_result(const mylite_result *result, struct expected_result expected) {
    int failures = 0;

    if (result == NULL) {
        fprintf(stderr, "%s: expected result, got NULL\n", expected.context);
        return 1;
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
    failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, expected.context);
    for (size_t row = 0U; row < expected.row_count; ++row) {
        for (size_t column = 0U; column < expected.column_count; ++column) {
            const size_t index = (row * expected.column_count) + column;

            failures += mylite_test_expect_text_or_null(
                mylite_result_column_name(result, column),
                expected.columns[column],
                expected.context
            );
            failures += mylite_test_expect_text_or_null(
                mylite_result_value_text(result, row, column),
                expected.values[index],
                expected.context
            );
        }
    }

    return failures;
}

static int expect_nonquery_result(
    mylite_db *database,
    const char *sql,
    struct expected_statement expected
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (result == NULL) {
        fprintf(stderr, "%s: expected result, got NULL\n", sql);
        return failures + 1;
    }
    failures += mylite_test_expect_size(mylite_result_column_count(result), 0U, sql);
    failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, sql);
    failures +=
        mylite_test_expect_int64(mylite_result_affected_rows(result), expected.affected_rows, sql);
    failures +=
        mylite_test_expect_size(mylite_result_warning_count(result), expected.warning_count, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_statement_id_increases(mylite_db *database) {
    mylite_result *result = NULL;
    uint64_t first = 0U;
    uint64_t second = 0U;
    int failures = execute_ok(database, "SELECT @@statement_id", &result);

    if (result == NULL) {
        return failures + 1;
    }
    failures +=
        mylite_test_expect_size(mylite_result_column_count(result), 1U, "statement_id columns");
    failures +=
        mylite_test_expect_size(mylite_result_row_count(result), 1U, "statement_id first row");
    first = strtoull(mylite_result_value_text(result, 0U, 0U), NULL, decimal_radix);
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "SELECT @@statement_id", &result);
    if (result == NULL) {
        return failures + 1;
    }
    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        1U,
        "statement_id second columns"
    );
    failures +=
        mylite_test_expect_size(mylite_result_row_count(result), 1U, "statement_id second row");
    second = strtoull(mylite_result_value_text(result, 0U, 0U), NULL, decimal_radix);
    failures += expect_uint64_greater_than(second, first, "statement_id monotonic");
    mylite_result_free(result);
    return failures;
}

static int expect_pseudo_thread_id_matches_connection_id(mylite_db *database) {
    mylite_result *result = NULL;
    uint64_t pseudo_thread_id = 0U;
    uint64_t connection_id = 0U;
    int failures = execute_ok(database, "SELECT @@pseudo_thread_id, CONNECTION_ID()", &result);

    if (result == NULL) {
        return failures + 1;
    }
    failures +=
        mylite_test_expect_size(mylite_result_column_count(result), 2U, "pseudo default columns");
    failures += mylite_test_expect_size(mylite_result_row_count(result), 1U, "pseudo default row");
    pseudo_thread_id = strtoull(mylite_result_value_text(result, 0U, 0U), NULL, decimal_radix);
    connection_id = strtoull(mylite_result_value_text(result, 0U, 1U), NULL, decimal_radix);
    failures += mylite_test_expect_int64(
        pseudo_thread_id == connection_id ? 1 : 0,
        1,
        "pseudo default connection id"
    );
    failures += expect_uint64_greater_than(pseudo_thread_id, 0U, "pseudo default nonzero");
    mylite_result_free(result);
    return failures;
}

static int expect_rand_seed_sequence(mylite_db *database) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, "SELECT RAND(), RAND()", &result);

    if (result == NULL) {
        return failures + 1;
    }
    failures +=
        mylite_test_expect_size(mylite_result_column_count(result), 2U, "rand sequence columns");
    failures += mylite_test_expect_size(mylite_result_row_count(result), 1U, "rand sequence row");
    if (result != NULL && mylite_result_column_count(result) == 2U &&
        mylite_result_row_count(result) == 1U) {
        const double first = strtod(mylite_result_value_text(result, 0U, 0U), NULL);
        const double second = strtod(mylite_result_value_text(result, 0U, 1U), NULL);

        failures += mylite_test_expect_int64(
            first > rand_seed_first_lower_bound && first < rand_seed_first_upper_bound ? 1 : 0,
            1,
            "rand first seeded value"
        );
        failures += mylite_test_expect_int64(
            second > rand_seed_second_lower_bound && second < rand_seed_second_upper_bound ? 1 : 0,
            1,
            "rand second seeded value"
        );
    }
    mylite_result_free(result);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "expected success for [%s], got rc=%d err=%d state=%s message=%s\n",
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
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    mylite_result_free(result);
    if (rc == MYLITE_OK) {
        fprintf(stderr, "expected error for [%s], got success\n", sql);
        return 1;
    }

    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, sql);
    failures += mylite_test_expect_text_or_null(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += mylite_test_expect_contains(mylite_errmsg(database), expected.message_part, sql);
    return failures;
}

static int expect_uint64_greater_than(uint64_t actual, uint64_t minimum, const char *context) {
    if (actual > minimum) {
        return 0;
    }

    fprintf(
        stderr,
        "%s: expected value greater than %llu, got %llu\n",
        context,
        (unsigned long long)minimum,
        (unsigned long long)actual
    );
    return 1;
}
