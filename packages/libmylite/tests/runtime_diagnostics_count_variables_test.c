#include <mylite/mylite.h>

#include "runtime/mylite_connection.h"
#include "storage/mylite_file_format.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <process.h>
#else
#  include <unistd.h>
#endif

#ifndef P_tmpdir
#  define P_tmpdir "."
#endif

enum {
    test_path_capacity = 1024,
    path_suffix_capacity = 16,
    scalar_count_column_count = 3,
    mixed_count_column_count = 6,
    mysql_error_parse = 1064,
    mysql_error_unknown_system_variable = 1193,
    mysql_error_session_variable_only = 1238,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_scalar_result {
    const char *const *columns;
    const char *const *values;
    size_t count;
    const char *context;
};

static int test_diagnostics_count_variable_lifecycle_and_preamble(void);
static int test_diagnostics_count_variable_qualifiers_and_errors(void);
static int test_independent_diagnostics_count_handles(void);
static int expect_scalar_result(
    const mylite_result *result,
    struct expected_scalar_result expected
);
static int expect_show_count_warnings(
    mylite_db *database,
    const char *expected,
    const char *context
);
static int expect_show_count_errors(mylite_db *database, const char *expected, const char *context);
static int expect_public_ok_diagnostics(mylite_db *database, const char *context);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_statement_ok(mylite_db *database, const char *sql);
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

    failures += test_diagnostics_count_variable_lifecycle_and_preamble();
    failures += test_diagnostics_count_variable_qualifiers_and_errors();
    failures += test_independent_diagnostics_count_handles();

    return failures == 0 ? 0 : 1;
}

static int test_diagnostics_count_variable_lifecycle_and_preamble(void) {
    static const char *const empty_columns[] = {
        "@@warning_count",
        "@@error_count",
        "ROW_COUNT()",
    };
    static const char *const empty_values[] = {"0", "0", "-1"};
    static const char *const warning_values[] = {"1", "0", "-1"};
    static const char *const error_columns[] = {
        "@@error_count",
        "@@warning_count",
        "ROW_COUNT()",
    };
    static const char *const error_values[] = {"1", "1", "-1"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    const struct mylite_session_state *session = NULL;
    uint64_t catalog_generation = 0U;
    uint64_t sqlite_schema_generation = 0U;
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "lifecycle") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open count variables file");
    session = mylite_connection_session_state(database);
    catalog_generation = session->catalog_generation;
    sqlite_schema_generation = session->sqlite_schema_generation;

    failures += execute_ok(database, "SELECT @@warning_count, @@error_count, ROW_COUNT()", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = empty_columns,
            .values = empty_values,
            .count = scalar_count_column_count,
            .context = "initial diagnostics counts",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_statement_ok(database, "SHOW PROCESSLIST");
    failures += execute_ok(database, "SHOW COUNT(*) WARNINGS", &result);
    failures += expect_size(mylite_result_row_count(result), 1U, "preserved warning count row");
    failures += expect_text_or_null(
        mylite_result_value_text(result, 0U, 0U),
        "1",
        "preserved warning count value"
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "SELECT @@warning_count, @@error_count, ROW_COUNT()", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = empty_columns,
            .values = warning_values,
            .count = scalar_count_column_count,
            .context = "warning diagnostics counts",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_show_count_warnings(database, "0", "scalar count clears warnings");

    failures += execute_error(
        database,
        "BAD SQL",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "BAD",
        }
    );
    failures += execute_ok(database, "SELECT @@error_count, @@warning_count, ROW_COUNT()", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = error_columns,
            .values = error_values,
            .count = scalar_count_column_count,
            .context = "parse error diagnostics counts",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_show_count_errors(database, "0", "scalar count clears errors");
    failures += expect_show_count_warnings(database, "0", "scalar count clears error warnings");
    failures += expect_public_ok_diagnostics(database, "scalar count public diagnostics");

    session = mylite_connection_session_state(database);
    failures += expect_int64(
        (int64_t)session->catalog_generation,
        (int64_t)catalog_generation,
        "catalog generation unchanged"
    );
    failures += expect_int64(
        (int64_t)session->sqlite_schema_generation,
        (int64_t)sqlite_schema_generation,
        "sqlite schema generation unchanged"
    );
    failures += expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read diagnostics count preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "diagnostics count preamble unchanged"
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_diagnostics_count_variable_qualifiers_and_errors(void) {
    static const char *const mixed_columns[] = {
        "@@WARNING_COUNT",
        "@@SESSION.ERROR_COUNT",
        "@@Local.Warning_Count",
        "(@@warning_count)",
        "@@session.`warning_count`",
        "@@`error_count`",
    };
    static const char *const mixed_values[] = {"0", "0", "0", "0", "0", "0"};
    static const char *const expression_columns[] = {
        "@@warning_count + 1",
        "@@error_count + 2",
    };
    static const char *const expression_values[] = {"1", "2"};
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open count variables memory");
    failures += execute_ok(
        database,
        "SELECT @@WARNING_COUNT, @@SESSION.ERROR_COUNT, @@Local.Warning_Count, "
        "(@@warning_count), @@session.`warning_count`, @@`error_count`",
        &result
    );
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = mixed_columns,
            .values = mixed_values,
            .count = mixed_count_column_count,
            .context = "diagnostics count qualifiers",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_error(
        database,
        "SELECT @@global.warning_count",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "SESSION variable",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@global.error_count",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "SESSION variable",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@no_such_mylite_variable",
        (struct expected_sql_error){
            .code = mysql_error_unknown_system_variable,
            .sqlstate = "HY000",
            .message_part = "Unknown system variable 'no_such_mylite_variable'",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@Local.no_such_mylite_variable",
        (struct expected_sql_error){
            .code = mysql_error_unknown_system_variable,
            .sqlstate = "HY000",
            .message_part = "Unknown system variable 'no_such_mylite_variable'",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@SESSION.no_such_mylite_variable",
        (struct expected_sql_error){
            .code = mysql_error_unknown_system_variable,
            .sqlstate = "HY000",
            .message_part = "Unknown system variable 'no_such_mylite_variable'",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@Global.no_such_mylite_variable",
        (struct expected_sql_error){
            .code = mysql_error_unknown_system_variable,
            .sqlstate = "HY000",
            .message_part = "Unknown system variable 'no_such_mylite_variable'",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@session.`no_such_mylite_variable`",
        (struct expected_sql_error){
            .code = mysql_error_unknown_system_variable,
            .sqlstate = "HY000",
            .message_part = "Unknown system variable 'no_such_mylite_variable'",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@`no_such_mylite_variable`",
        (struct expected_sql_error){
            .code = mysql_error_unknown_system_variable,
            .sqlstate = "HY000",
            .message_part = "Unknown system variable 'no_such_mylite_variable'",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@`session`.warning_count",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "quoted system variable scope",
        }
    );
    failures += execute_ok(database, "SELECT @@warning_count + 1, @@error_count + 2", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = expression_columns,
            .values = expression_values,
            .count = sizeof(expression_columns) / sizeof(expression_columns[0]),
            .context = "diagnostics count expression reads",
        }
    );
    mylite_result_free(result);

    mylite_close(database);
    return failures;
}

static int test_independent_diagnostics_count_handles(void) {
    static const char *const count_columns[] = {"@@warning_count", "@@error_count"};
    static const char *const empty_values[] = {"0", "0"};
    static const char *const warning_values[] = {"1", "0"};
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&first), MYLITE_OK, "open first count handle");
    failures += expect_int(mylite_open_memory(&second), MYLITE_OK, "open second count handle");
    failures += execute_statement_ok(first, "SHOW PROCESSLIST");

    failures += execute_ok(first, "SELECT @@warning_count, @@error_count", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = count_columns,
            .values = warning_values,
            .count = 2U,
            .context = "first handle warning count",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(second, "SELECT @@warning_count, @@error_count", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = count_columns,
            .values = empty_values,
            .count = 2U,
            .context = "second handle empty count",
        }
    );
    mylite_result_free(result);

    mylite_close(second);
    mylite_close(first);
    return failures;
}

static int expect_scalar_result(
    const mylite_result *result,
    struct expected_scalar_result expected
) {
    int failures = 0;

    failures += expect_size(mylite_result_column_count(result), expected.count, expected.context);
    failures += expect_size(mylite_result_row_count(result), 1U, expected.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, expected.context);
    for (size_t index = 0U; index < expected.count; ++index) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, index),
            expected.columns[index],
            expected.context
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, 0U, index),
            expected.values[index],
            expected.context
        );
    }

    return failures;
}

static int expect_show_count_warnings(
    mylite_db *database,
    const char *expected,
    const char *context
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, "SHOW COUNT(*) WARNINGS", &result);

    failures += expect_size(mylite_result_row_count(result), 1U, context);
    failures += expect_text_or_null(mylite_result_value_text(result, 0U, 0U), expected, context);
    failures += expect_size(mylite_result_warning_count(result), 0U, context);

    mylite_result_free(result);
    return failures;
}

static int expect_show_count_errors(
    mylite_db *database,
    const char *expected,
    const char *context
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, "SHOW COUNT(*) ERRORS", &result);

    failures += expect_size(mylite_result_row_count(result), 1U, context);
    failures += expect_text_or_null(mylite_result_value_text(result, 0U, 0U), expected, context);
    failures += expect_size(mylite_result_warning_count(result), 0U, context);

    mylite_result_free(result);
    return failures;
}

static int expect_public_ok_diagnostics(mylite_db *database, const char *context) {
    int failures = 0;

    failures += expect_int(mylite_errcode(database), MYLITE_OK, context);
    failures += expect_text_or_null(mylite_sqlstate(database), "00000", context);
    failures += expect_text_or_null(mylite_errmsg(database), "not an error", context);

    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected MYLITE_OK, got %d (%d %s %s)\n",
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
        failures += 1;
    }
    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text_or_null(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_text_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);

    return failures;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "%s/libmylite_diagnostics_count_variables_%d_%s.mylite",
        P_tmpdir,
        current_process_id(),
        name
    );

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "test path overflow\n");
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
    char buffer[test_path_capacity + path_suffix_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(buffer)) {
        return;
    }

    (void)remove(buffer);
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t bytes_read = 0U;
    int failures = 0;

    if (file == NULL) {
        fprintf(stderr, "%s: failed to open file\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "%s: failed to seek file\n", path);
        failures += 1;
    } else {
        bytes_read = fread(buffer, 1U, size, file);
        if (bytes_read != size) {
            fprintf(stderr, "%s: expected %zu bytes, read %zu\n", path, size, bytes_read);
            failures += 1;
        }
    }
    if (fclose(file) != 0) {
        fprintf(stderr, "%s: failed to close file\n", path);
        failures += 1;
    }

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

static int expect_text_or_null(const char *actual, const char *expected, const char *context) {
    if (actual == NULL && expected == NULL) {
        return 0;
    }
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

static int expect_text_contains(const char *actual, const char *needle, const char *context) {
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

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: bytes differ\n", context);
        return 1;
    }

    return 0;
}
