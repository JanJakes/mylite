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
    uninitialized_read_column_count = 5,
    assigned_read_column_count = 18,
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
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);

int main(void) {
    int failures = 0;

    failures += test_user_variable_values_and_scalar_reads();
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
        "@@warning_count",
        "@@error_count",
        "ROW_COUNT()",
    };
    static const char *const missing_values[] = {NULL, NULL, "0", "0", "-1"};
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
    int failures = expect_int(mylite_open_memory(&database), MYLITE_OK, "open user variables");

    if (failures != 0) {
        return failures;
    }

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @missing_a, @Missing_A, @@warning_count, @@error_count, ROW_COUNT()",
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
    int failures = expect_int(mylite_open_memory(&database), MYLITE_OK, "open restore variables");

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
    int failures = expect_int(mylite_open_memory(&database), MYLITE_OK, "open diagnostics");
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
    int failures = make_test_path(path, sizeof(path), "reopen");

    if (failures != 0) {
        return failures;
    }

    (void)remove(path);
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open user-variable file");
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

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen user-variable file");
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
    int failures = expect_int(mylite_open_memory(&first), MYLITE_OK, "open first handle");

    failures += expect_int(mylite_open_memory(&second), MYLITE_OK, "open second handle");
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
        failures += expect_size(mylite_result_column_count(result), 0U, sql);
        failures += expect_size(mylite_result_row_count(result), 0U, sql);
        failures += expect_int64(mylite_result_affected_rows(result), 0, sql);
        failures += expect_size(mylite_result_warning_count(result), 0U, sql);
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
    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, expected.sql, &result);

    if (failures != 0) {
        return failures;
    }
    failures +=
        expect_size(mylite_result_column_count(result), expected.column_count, expected.context);
    failures += expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
    failures +=
        expect_size(mylite_result_warning_count(result), expected.warning_count, expected.context);
    failures +=
        expect_int64(mylite_result_affected_rows(result), expected.affected_rows, expected.context);
    for (size_t column = 0U; column < expected.column_count; ++column) {
        failures += expect_text(
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
    return expect_text(actual, expected, context);
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
        failures += expect_size(mylite_result_column_count(result), 0U, context);
        failures += expect_size(mylite_result_row_count(result), 0U, context);
        failures += expect_int64(mylite_result_affected_rows(result), affected_rows, context);
        failures += expect_size(mylite_result_warning_count(result), 0U, context);
    }
    mylite_result_free(result);
    return failures;
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
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected [%s], got [%s]\n",
            context,
            expected == NULL ? "NULL" : expected,
            actual == NULL ? "NULL" : actual
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
            actual == NULL ? "NULL" : actual,
            needle == NULL ? "NULL" : needle
        );
        return 1;
    }
    return 0;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_user_variables_%d_%s.mylite",
        current_process_id(),
        name
    );

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "test path truncated\n");
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
