#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#  include <process.h>
#else
#  include <unistd.h>
#endif

enum {
    mysql_error_parse = 1064,
    mysql_error_unknown_system_variable = 1193,
    mysql_error_illegal_user_variable_name = 3061,
    utf8_e_acute_first_byte = 0xC3,
    utf8_e_acute_second_byte = 0xA9,
    utf8_e_acute_byte_count = 2,
    user_variable_name_boundary_character_count = 64,
    user_variable_name_too_long_character_count = 65,
    user_variable_sql_capacity = 320,
    uninitialized_read_column_count = 8,
    assigned_read_column_count = 18,
    assignment_expression_column_count = 7,
    scalar_temporal_column_count = 7,
    scalar_temporal_utc_column_index = 5,
    scalar_temporal_sysdate_column_index = 6,
    uuid_text_length = 36,
    uuid_first_dash_offset = 8,
    uuid_second_dash_offset = 13,
    uuid_third_dash_offset = 18,
    uuid_fourth_dash_offset = 23,
    atomic_rollback_column_count = 5,
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

static const char default_sql_mode[] =
    "ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,"
    "ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION";

static int test_user_variable_values_and_scalar_reads(void);
static int test_user_variable_assignment_expressions(void);
static int test_user_variable_scalar_function_assignments(void);
static int test_user_variable_system_restore_and_atomic_failure(void);
static int test_user_variable_diagnostics(void);
static int test_user_variable_file_reopen_is_nonpersistent(void);
static int test_user_variable_independent_handles(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int make_e_acute_user_variable_sql(
    char *buffer,
    size_t buffer_size,
    const char *prefix,
    size_t character_count,
    const char *suffix
);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int expect_statement_result(
    mylite_db *database,
    const char *sql,
    int64_t affected_rows,
    const char *context
);
static int expect_uuid_shape(const char *actual, const char *context);

int main(void) {
    int failures = 0;

    failures += test_user_variable_values_and_scalar_reads();
    failures += test_user_variable_assignment_expressions();
    failures += test_user_variable_scalar_function_assignments();
    failures += test_user_variable_system_restore_and_atomic_failure();
    failures += test_user_variable_diagnostics();
    failures += test_user_variable_file_reopen_is_nonpersistent();
    failures += test_user_variable_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_user_variable_values_and_scalar_reads(void) {
    static const char *const missing_columns[] = {
        "@missing_a",
        "@Missing_A",
        "@",
        "@''",
        "@``",
        "@@warning_count",
        "@@error_count",
        "ROW_COUNT()",
    };
    static const char *const missing_values[] = {NULL, NULL, NULL, NULL, NULL, "0", "0", "-1"};
    static const char *const assigned_columns[] = {
        "@a",
        "bee",
        "@c",
        "@p",
        "@a + 2",
        "@`dash-name`",
        "@'sp ace'",
        "@\"dq-name\"",
        "@d",
        "@nd",
        "@f",
        "@nf",
        "@h",
        "@bits",
        "@introduced",
        "ROW_COUNT()",
        "@@warning_count",
        "@@error_count",
    };
    static const char *const assigned_values[] = {
        "1",
        "x",
        NULL,
        "2",
        "3",
        "ok",
        "space",
        "dq",
        "1.0",
        "-1.50",
        "1e18",
        "-1.5e-2",
        "A",
        "a",
        "A",
        "0",
        "0",
        "0",
    };
    static const char *const case_columns[] = {"@foo", "@FOO", "@Foo"};
    static const char *const case_values[] = {"7", "7", "7"};
    static const char *const row_count_columns[] = {"ROW_COUNT()"};
    static const char *const row_count_values[] = {"0"};
    mylite_db *database = NULL;
    int failures =
        mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open user variables");

    if (failures != 0) {
        return failures;
    }

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @missing_a, @Missing_A, @, @'', @``, @@warning_count, @@error_count, "
                   "ROW_COUNT()",
            .columns = missing_columns,
            .values = missing_values,
            .column_count = uninitialized_read_column_count,
            .row_count = 1U,
            .affected_rows = 0,
            .warning_count = 0U,
            .context = "uninitialized user variables read as NULL",
        }
    );
    failures += expect_statement_result(
        database,
        "SET @a = 1, @b := 'x', @c = NULL, @p = (+2), @`dash-name` = 'ok', "
        "@'sp ace' = 'space', @\"dq-name\" = 'dq', @d = 1.0, @nd = -1.50, "
        "@f = 1e18, @nf = -1.5e-2, @h = X'41', @bits = b'01100001', "
        "@introduced = _latin1 X'41' COLLATE latin1_swedish_ci",
        0,
        "assign user variables"
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @a, @b AS bee, @c, @p, @a + 2, @`dash-name`, @'sp ace', "
                   "@\"dq-name\", @d, @nd, @f, @nf, @h, @bits, @introduced, ROW_COUNT(), "
                   "@@warning_count, @@error_count",
            .columns = assigned_columns,
            .values = assigned_values,
            .column_count = assigned_read_column_count,
            .row_count = 1U,
            .affected_rows = 0,
            .warning_count = 0U,
            .context = "assigned user variable readback",
        }
    );
    failures += execute_statement_ok(database, "DO @a, @a + 1");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT()",
            .columns = row_count_columns,
            .values = row_count_values,
            .column_count = 1U,
            .row_count = 1U,
            .affected_rows = 0,
            .warning_count = 0U,
            .context = "DO user variable row count",
        }
    );
    failures += execute_statement_ok(database, "SET @Foo = 7");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @foo, @FOO, @Foo",
            .columns = case_columns,
            .values = case_values,
            .column_count = 3U,
            .row_count = 1U,
            .affected_rows = 0,
            .warning_count = 0U,
            .context = "user variable names are case-insensitive",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_user_variable_assignment_expressions(void) {
    static const char *const assignment_columns[] = {
        "@a := 1",
        "@a",
        "@a := @a + 2",
        "@a",
        "@b = 1",
        "@b := 1",
        "@b",
    };
    static const char *const assignment_values[] = {"1", "1", "3", "3", NULL, "1", "1"};
    static const char *const nested_columns[] = {"@outer := (@inner := 10)", "@inner", "@outer"};
    static const char *const nested_values[] = {"10", "10", "10"};
    static const char *const subquery_columns[] = {"@sub := (SELECT 7)", "@sub"};
    static const char *const subquery_values[] = {"7", "7"};
    static const char *const do_columns[] = {"@done", "ROW_COUNT()", "@@warning_count"};
    static const char *const do_values[] = {"5", "0", "1"};
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = mylite_test_expect_int(
        mylite_open_memory(&database),
        MYLITE_OK,
        "open assignment expressions"
    );

    if (failures != 0) {
        return failures;
    }

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @a := 1, @a, @a := @a + 2, @a, @b = 1, @b := 1, @b",
            .columns = assignment_columns,
            .values = assignment_values,
            .column_count = assignment_expression_column_count,
            .row_count = 1U,
            .affected_rows = 0,
            .warning_count = 3U,
            .context = "assignment expressions update variables and return assigned values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @outer := (@inner := 10), @inner, @outer",
            .columns = nested_columns,
            .values = nested_values,
            .column_count = 3U,
            .row_count = 1U,
            .affected_rows = 0,
            .warning_count = 2U,
            .context = "nested assignment expressions",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @sub := (SELECT 7), @sub",
            .columns = subquery_columns,
            .values = subquery_values,
            .column_count = 2U,
            .row_count = 1U,
            .affected_rows = 0,
            .warning_count = 1U,
            .context = "assignment expression scalar subquery",
        }
    );
    failures += execute_ok(database, "DO @done := 5", &result);
    if (failures == 0) {
        failures += mylite_test_expect_size(
            mylite_result_column_count(result),
            0U,
            "DO assignment columns"
        );
        failures +=
            mylite_test_expect_size(mylite_result_row_count(result), 0U, "DO assignment rows");
        failures += mylite_test_expect_int64(
            mylite_result_affected_rows(result),
            0,
            "DO assignment affected"
        );
        failures += mylite_test_expect_size(
            mylite_result_warning_count(result),
            1U,
            "DO assignment warnings"
        );
    }
    mylite_result_free(result);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @done, ROW_COUNT(), @@warning_count",
            .columns = do_columns,
            .values = do_values,
            .column_count = 3U,
            .row_count = 1U,
            .affected_rows = 0,
            .warning_count = 0U,
            .context = "DO assignment side effects and warning count",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_user_variable_scalar_function_assignments(void) {
    static const char *const scalar_columns[] = {
        "@concat",
        "@flow",
        "@nil",
        "@co",
        "@rep",
        "@regexp",
        "@ws",
        "@neq",
        "@greatest",
        "@least",
        "@lid",
        "LAST_INSERT_ID()",
    };
    static const char *const scalar_values[] = {
        "abb",
        "3",
        "4",
        "5",
        "aBc",
        "aBc",
        "a-b",
        "8",
        "3",
        "4",
        "7",
        "7",
    };
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = mylite_test_expect_int(
        mylite_open_memory(&database),
        MYLITE_OK,
        "open scalar assignment functions"
    );

    if (failures != 0) {
        return failures;
    }

    failures += execute_statement_ok(
        database,
        "SET @concat = CONCAT('a', REPEAT('b', 2)), @flow = IF(0, 2, 3), "
        "@nil = IFNULL(NULL, 4), @co = COALESCE(NULL, 5), "
        "@rep = REPLACE('abc', 'b', 'B'), @regexp = REGEXP_REPLACE('abc', 'b', 'B'), "
        "@ws = CONCAT_WS('-', 'a', 'b'), @neq = NULLIF(8, 9), "
        "@greatest = GREATEST(1, 3, 2), @least = LEAST(9, 4, 7), "
        "@lid = LAST_INSERT_ID(7)"
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @concat, @flow, @nil, @co, @rep, @regexp, @ws, @neq, "
                   "@greatest, @least, @lid, LAST_INSERT_ID()",
            .columns = scalar_columns,
            .values = scalar_values,
            .column_count = sizeof(scalar_columns) / sizeof(scalar_columns[0]),
            .row_count = 1U,
            .affected_rows = 0,
            .warning_count = 0U,
            .context = "scalar function user variable assignments",
        }
    );
    failures += execute_statement_ok(database, "SET @keys = RANDOM_BYTES(4)");
    failures += execute_ok(database, "SELECT @keys", &result);
    if (failures == 0) {
        failures += mylite_test_expect_size(
            mylite_result_value_size(result, 0U, 0U),
            4U,
            "RANDOM_BYTES user variable assignment size"
        );
    }
    mylite_result_free(result);
    result = NULL;
    failures += execute_statement_ok(
        database,
        "SET @now = NOW(), @ts = CURRENT_TIMESTAMP(), @d = DATE '2001-01-02', "
        "@cd = CURRENT_DATE(), @ct = CURRENT_TIME(), @utc = UTC_TIMESTAMP(), @sys = SYSDATE()"
    );
    failures += execute_ok(database, "SELECT @now, @ts, @d, @cd, @ct, @utc, @sys", &result);
    if (failures == 0) {
        failures += mylite_test_expect_size(
            mylite_result_column_count(result),
            scalar_temporal_column_count,
            "temporal columns"
        );
        failures += mylite_test_expect_size(mylite_result_row_count(result), 1U, "temporal rows");
        failures += mylite_test_expect_contains(
            mylite_result_value_text(result, 0U, 0U),
            "-",
            "NOW assignment"
        );
        failures += mylite_test_expect_contains(
            mylite_result_value_text(result, 0U, 1U),
            "-",
            "CURRENT_TIMESTAMP assignment"
        );
        failures += mylite_test_expect_text(
            mylite_result_value_text(result, 0U, 2U),
            "2001-01-02",
            "DATE literal"
        );
        failures += mylite_test_expect_contains(
            mylite_result_value_text(result, 0U, 3U),
            "-",
            "CURRENT_DATE assignment"
        );
        failures += mylite_test_expect_contains(
            mylite_result_value_text(result, 0U, 4U),
            ":",
            "CURRENT_TIME assignment"
        );
        failures += mylite_test_expect_contains(
            mylite_result_value_text(result, 0U, scalar_temporal_utc_column_index),
            "-",
            "UTC_TIMESTAMP assignment"
        );
        failures += mylite_test_expect_contains(
            mylite_result_value_text(result, 0U, scalar_temporal_sysdate_column_index),
            "-",
            "SYSDATE assignment"
        );
    }
    mylite_result_free(result);
    result = NULL;
    failures += execute_statement_ok(database, "SET @cid = CONNECTION_ID(), @uuid = UUID()");
    failures += execute_ok(database, "SELECT @cid > 0, @uuid", &result);
    if (failures == 0) {
        failures += mylite_test_expect_size(
            mylite_result_column_count(result),
            2U,
            "uuid assignment columns"
        );
        failures +=
            mylite_test_expect_size(mylite_result_row_count(result), 1U, "uuid assignment rows");
        failures += mylite_test_expect_text(
            mylite_result_value_text(result, 0U, 0U),
            "1",
            "connection id assigned"
        );
        failures +=
            expect_uuid_shape(mylite_result_value_text(result, 0U, 1U), "uuid assigned value");
    }
    mylite_result_free(result);

    mylite_close(database);
    return failures;
}

static int test_user_variable_system_restore_and_atomic_failure(void) {
    static const char *const sql_mode_columns[] = {"@old_sql_mode", "@@sql_mode", "ROW_COUNT()"};
    static const char *const sql_mode_values[] = {default_sql_mode, "NO_ENGINE_SUBSTITUTION", "0"};
    static const char *const restored_columns[] = {"@@sql_mode", "ROW_COUNT()"};
    static const char *const restored_values[] = {default_sql_mode, "0"};
    static const char *const boolean_columns[] = {"@old_notes", "@old_unique", "@old_fk"};
    static const char *const boolean_values[] = {"1", "1", "1"};
    static const char *const boolean_changed_columns[] = {
        "@@sql_notes",
        "@@foreign_key_checks",
        "@@unique_checks",
    };
    static const char *const boolean_changed_values[] = {"1", "0", "1"};
    static const char *const boolean_restored_columns[] = {
        "@@foreign_key_checks",
        "ROW_COUNT()",
    };
    static const char *const boolean_restored_values[] = {"1", "0"};
    static const char *const time_zone_columns[] = {"@old_time_zone", "@@time_zone"};
    static const char *const time_zone_values[] = {"SYSTEM", "+02:30"};
    static const char *const time_zone_restored_columns[] = {"@@time_zone", "ROW_COUNT()"};
    static const char *const time_zone_restored_values[] = {"SYSTEM", "0"};
    static const char *const atomic_columns[] = {
        "@atomic",
        "@@sql_mode",
        "@@warning_count",
        "@@error_count",
        "ROW_COUNT()",
    };
    static const char *const atomic_values[] = {"before", default_sql_mode, "1", "1", "-1"};
    mylite_db *database = NULL;
    int failures =
        mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open restore variables");

    if (failures != 0) {
        return failures;
    }

    failures += execute_statement_ok(
        database,
        "SET @old_sql_mode = @@sql_mode, sql_mode = 'NO_ENGINE_SUBSTITUTION'"
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @old_sql_mode, @@sql_mode, ROW_COUNT()",
            .columns = sql_mode_columns,
            .values = sql_mode_values,
            .column_count = 3U,
            .row_count = 1U,
            .affected_rows = 0,
            .warning_count = 0U,
            .context = "save sql_mode in user variable",
        }
    );
    failures += execute_statement_ok(database, "SET sql_mode = @old_sql_mode");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@sql_mode, ROW_COUNT()",
            .columns = restored_columns,
            .values = restored_values,
            .column_count = 2U,
            .row_count = 1U,
            .affected_rows = 0,
            .warning_count = 0U,
            .context = "restore sql_mode from user variable",
        }
    );

    failures += execute_statement_ok(
        database,
        "SET @old_notes = @@sql_notes, @old_unique = @@unique_checks, "
        "@old_fk = @@foreign_key_checks"
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @old_notes, @old_unique, @old_fk",
            .columns = boolean_columns,
            .values = boolean_values,
            .column_count = 3U,
            .row_count = 1U,
            .affected_rows = 0,
            .warning_count = 0U,
            .context = "save fixed booleans in user variables",
        }
    );
    failures += execute_statement_ok(
        database,
        "SET sql_notes = @old_notes, unique_checks = @old_unique, foreign_key_checks = 0"
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@sql_notes, @@foreign_key_checks, @@unique_checks",
            .columns = boolean_changed_columns,
            .values = boolean_changed_values,
            .column_count = 3U,
            .row_count = 1U,
            .affected_rows = 0,
            .warning_count = 0U,
            .context = "restore fixed boolean no-op values and change foreign_key_checks",
        }
    );
    failures += execute_statement_ok(database, "SET foreign_key_checks = @old_fk");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@foreign_key_checks, ROW_COUNT()",
            .columns = boolean_restored_columns,
            .values = boolean_restored_values,
            .column_count = 2U,
            .row_count = 1U,
            .affected_rows = 0,
            .warning_count = 0U,
            .context = "restore foreign_key_checks from user variable",
        }
    );

    failures +=
        execute_statement_ok(database, "SET @old_time_zone = @@time_zone, time_zone = '+02:30'");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @old_time_zone, @@time_zone",
            .columns = time_zone_columns,
            .values = time_zone_values,
            .column_count = 2U,
            .row_count = 1U,
            .affected_rows = 0,
            .warning_count = 0U,
            .context = "save time_zone in user variable",
        }
    );
    failures += execute_statement_ok(database, "SET time_zone = @old_time_zone");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@time_zone, ROW_COUNT()",
            .columns = time_zone_restored_columns,
            .values = time_zone_restored_values,
            .column_count = 2U,
            .row_count = 1U,
            .affected_rows = 0,
            .warning_count = 0U,
            .context = "restore time_zone from user variable",
        }
    );

    failures += execute_statement_ok(database, "SET @atomic = 'before', sql_mode = DEFAULT");
    failures += execute_error(
        database,
        "SET @atomic = 'after', no_such_system_var = 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_system_variable,
            .sqlstate = "HY000",
            .message_part = "Unknown system variable",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @atomic, @@sql_mode, @@warning_count, @@error_count, ROW_COUNT()",
            .columns = atomic_columns,
            .values = atomic_values,
            .column_count = atomic_rollback_column_count,
            .row_count = 1U,
            .affected_rows = 0,
            .warning_count = 0U,
            .context = "failing SET list rolls back user variable changes",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_user_variable_diagnostics(void) {
    static const char *const utf8_values[] = {"1"};
    char utf8_set[user_variable_sql_capacity];
    char utf8_select[user_variable_sql_capacity];
    char utf8_column[user_variable_sql_capacity];
    char utf8_too_long[user_variable_sql_capacity];
    const char *utf8_columns[] = {utf8_column};
    mylite_db *database = NULL;
    int failures =
        mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open diagnostics");
    int utf8_setup_failures = 0;

    if (failures != 0) {
        return failures;
    }

    failures += execute_error(
        database,
        "SET @aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa = 1",
        (struct expected_sql_error){
            .code = mysql_error_illegal_user_variable_name,
            .sqlstate = "42000",
            .message_part = "User variable name",
        }
    );
    utf8_setup_failures += make_e_acute_user_variable_sql(
        utf8_set,
        sizeof(utf8_set),
        "SET @'",
        user_variable_name_boundary_character_count,
        "' = 1"
    );
    utf8_setup_failures += make_e_acute_user_variable_sql(
        utf8_select,
        sizeof(utf8_select),
        "SELECT @'",
        user_variable_name_boundary_character_count,
        "'"
    );
    utf8_setup_failures += make_e_acute_user_variable_sql(
        utf8_column,
        sizeof(utf8_column),
        "@'",
        user_variable_name_boundary_character_count,
        "'"
    );
    utf8_setup_failures += make_e_acute_user_variable_sql(
        utf8_too_long,
        sizeof(utf8_too_long),
        "SET @'",
        user_variable_name_too_long_character_count,
        "' = 1"
    );
    failures += utf8_setup_failures;
    if (utf8_setup_failures == 0) {
        failures += execute_statement_ok(database, utf8_set);
        failures += expect_query(
            database,
            (struct expected_query){
                .sql = utf8_select,
                .columns = utf8_columns,
                .values = utf8_values,
                .column_count = 1U,
                .row_count = 1U,
                .affected_rows = 0,
                .warning_count = 0U,
                .context = "64 UTF-8 character user variable name",
            }
        );
        failures += execute_error(
            database,
            utf8_too_long,
            (struct expected_sql_error){
                .code = mysql_error_illegal_user_variable_name,
                .sqlstate = "42000",
                .message_part = "User variable name",
            }
        );
    }
    failures += execute_error(
        database,
        "SET @d = DEFAULT",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SET @f = ABS(1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SET @ = 1",
        (struct expected_sql_error){
            .code = mysql_error_illegal_user_variable_name,
            .sqlstate = "42000",
            .message_part = "User variable name",
        }
    );
    failures += execute_error(
        database,
        "SET @'' = 1",
        (struct expected_sql_error){
            .code = mysql_error_illegal_user_variable_name,
            .sqlstate = "42000",
            .message_part = "User variable name",
        }
    );
    failures += execute_error(
        database,
        "SELECT @ := 1",
        (struct expected_sql_error){
            .code = mysql_error_illegal_user_variable_name,
            .sqlstate = "42000",
            .message_part = "User variable name",
        }
    );

    mylite_close(database);
    return failures;
}

static int make_e_acute_user_variable_sql(
    char *buffer,
    size_t buffer_size,
    const char *prefix,
    size_t character_count,
    const char *suffix
) {
    static const char e_acute[utf8_e_acute_byte_count] = {
        (char)utf8_e_acute_first_byte,
        (char)utf8_e_acute_second_byte,
    };
    size_t length = 0U;
    size_t suffix_length = 0U;
    size_t repeated_length = 0U;

    if (buffer == NULL || prefix == NULL || suffix == NULL ||
        character_count > SIZE_MAX / sizeof(e_acute)) {
        fprintf(stderr, "failed to build UTF-8 user variable SQL\n");
        return 1;
    }

    repeated_length = character_count * sizeof(e_acute);
    length = strlen(prefix);
    suffix_length = strlen(suffix);
    if (length > SIZE_MAX - repeated_length ||
        length + repeated_length > SIZE_MAX - suffix_length ||
        length + repeated_length + suffix_length + 1U > buffer_size) {
        fprintf(stderr, "failed to build UTF-8 user variable SQL\n");
        return 1;
    }

    memcpy(buffer, prefix, length);
    for (size_t index = 0U; index < character_count; ++index) {
        memcpy(buffer + length, e_acute, sizeof(e_acute));
        length += sizeof(e_acute);
    }
    memcpy(buffer + length, suffix, suffix_length + 1U);
    return 0;
}

static int test_user_variable_file_reopen_is_nonpersistent(void) {
    static const char *const columns[] = {"@persisted"};
    static const char *const assigned_values[] = {"9"};
    static const char *const reopened_values[] = {NULL};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = mylite_test_make_path(path, sizeof(path), "reopen");

    if (failures != 0) {
        return failures;
    }

    (void)remove(path);
    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open user-variable file");
    if (failures == 0) {
        failures += execute_statement_ok(database, "SET @persisted = 9");
        failures += expect_query(
            database,
            (struct expected_query){
                .sql = "SELECT @persisted",
                .columns = columns,
                .values = assigned_values,
                .column_count = 1U,
                .row_count = 1U,
                .affected_rows = 0,
                .warning_count = 0U,
                .context = "file handle user variable before close",
            }
        );
    }
    mylite_close(database);
    database = NULL;

    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "reopen user-variable file"
    );
    if (failures == 0) {
        failures += expect_query(
            database,
            (struct expected_query){
                .sql = "SELECT @persisted",
                .columns = columns,
                .values = reopened_values,
                .column_count = 1U,
                .row_count = 1U,
                .affected_rows = 0,
                .warning_count = 0U,
                .context = "user variable does not persist across reopen",
            }
        );
    }
    mylite_close(database);
    (void)remove(path);
    return failures;
}

static int test_user_variable_independent_handles(void) {
    static const char *const first_columns[] = {"@shared"};
    static const char *const first_values[] = {"1"};
    static const char *const second_values[] = {NULL};
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures =
        mylite_test_expect_int(mylite_open_memory(&first), MYLITE_OK, "open first handle");

    failures +=
        mylite_test_expect_int(mylite_open_memory(&second), MYLITE_OK, "open second handle");
    if (failures != 0) {
        mylite_close(first);
        mylite_close(second);
        return failures;
    }

    failures += execute_statement_ok(first, "SET @shared = 1");
    failures += expect_query(
        first,
        (struct expected_query){
            .sql = "SELECT @shared",
            .columns = first_columns,
            .values = first_values,
            .column_count = 1U,
            .row_count = 1U,
            .affected_rows = 0,
            .warning_count = 0U,
            .context = "first handle user variable",
        }
    );
    failures += expect_query(
        second,
        (struct expected_query){
            .sql = "SELECT @shared",
            .columns = first_columns,
            .values = second_values,
            .column_count = 1U,
            .row_count = 1U,
            .affected_rows = 0,
            .warning_count = 0U,
            .context = "second handle user variable is independent",
        }
    );

    mylite_close(first);
    mylite_close(second);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected success, got %d/%s/%s\n",
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

    if (failures == 0) {
        failures += mylite_test_expect_size(mylite_result_column_count(result), 0U, sql);
        failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, sql);
        failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, sql);
        failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, sql);
    }
    mylite_result_free(result);
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

static int expect_statement_result(
    mylite_db *database,
    const char *sql,
    int64_t affected_rows,
    const char *context
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures == 0) {
        failures += mylite_test_expect_size(mylite_result_column_count(result), 0U, context);
        failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, context);
        failures +=
            mylite_test_expect_int64(mylite_result_affected_rows(result), affected_rows, context);
        failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, context);
    }
    mylite_result_free(result);
    return failures;
}

static int expect_uuid_shape(const char *actual, const char *context) {
    if (actual == NULL || strlen(actual) != uuid_text_length ||
        actual[uuid_first_dash_offset] != '-' || actual[uuid_second_dash_offset] != '-' ||
        actual[uuid_third_dash_offset] != '-' || actual[uuid_fourth_dash_offset] != '-') {
        fprintf(
            stderr,
            "%s: expected UUID-shaped text, got [%s]\n",
            context,
            actual == NULL ? "(null)" : actual
        );
        return 1;
    }
    return 0;
}
