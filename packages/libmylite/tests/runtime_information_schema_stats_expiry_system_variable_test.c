#include <mylite/mylite.h>

#include "runtime/mylite_connection.h"
#include "storage/mylite_file_format.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <process.h>
#else
#  include <unistd.h>
#endif

#ifndef P_tmpdir
#  define P_tmpdir "/tmp"
#endif

enum {
    test_path_capacity = 1024,
    test_path_suffix_capacity = 16,
    scalar_column_count = 7,
    variable_row_column_count = 2,
    warning_column_count = 3,
    mysql_error_parse = 1064,
    mysql_error_incorrect_argument_type = 1232,
    information_schema_stats_expiry_max = 31536000,
};

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

static int test_information_schema_stats_expiry_values_and_file_safety(void);
static int test_information_schema_stats_expiry_assignment_diagnostics(void);
static int test_information_schema_stats_expiry_independent_handles(void);
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
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_uint64(uint64_t actual, uint64_t expected, const char *context);
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

    failures += test_information_schema_stats_expiry_values_and_file_safety();
    failures += test_information_schema_stats_expiry_assignment_diagnostics();
    failures += test_information_schema_stats_expiry_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_information_schema_stats_expiry_values_and_file_safety(void) {
    static const char *const scalar_columns[] = {
        "@@information_schema_stats_expiry",
        "@@global.information_schema_stats_expiry",
        "@@session.information_schema_stats_expiry",
        "@@local.information_schema_stats_expiry",
        "HEX(@@information_schema_stats_expiry)",
        "@@warning_count",
        "ROW_COUNT()",
    };
    static const char *const default_scalar_values[] = {
        "86400",
        "86400",
        "86400",
        "86400",
        "15180",
        "0",
        "-1",
    };
    static const char *const zero_scalar_values[] = {
        "0",
        "86400",
        "0",
        "0",
        "0",
        "0",
        "0",
    };
    static const char *const max_scalar_values[] = {
        "31536000",
        "86400",
        "31536000",
        "31536000",
        "1E13380",
        "0",
        "0",
    };
    static const char *const show_columns[] = {"Variable_name", "Value"};
    static const char *const show_default_values[] = {
        "information_schema_stats_expiry",
        "86400",
    };
    static const char *const show_zero_values[] = {
        "information_schema_stats_expiry",
        "0",
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    const struct mylite_session_state *session = NULL;
    uint64_t catalog_generation = 0U;
    uint64_t sqlite_schema_generation = 0U;
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "values") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open stats expiry file");
    session = mylite_connection_session_state(database);
    catalog_generation = session == NULL ? 0U : session->catalog_generation;
    sqlite_schema_generation = session == NULL ? 0U : session->sqlite_schema_generation;

    failures += expect_query_result(
        database,
        "SELECT @@information_schema_stats_expiry, "
        "@@global.information_schema_stats_expiry, "
        "@@session.information_schema_stats_expiry, "
        "@@local.information_schema_stats_expiry, "
        "HEX(@@information_schema_stats_expiry), @@warning_count, ROW_COUNT()",
        (struct expected_result){
            .columns = scalar_columns,
            .values = default_scalar_values,
            .column_count = scalar_column_count,
            .row_count = 1U,
            .context = "default scalar values",
        }
    );
    failures += expect_query_result(
        database,
        "SHOW VARIABLES LIKE 'information_schema_stats_expiry'",
        (struct expected_result){
            .columns = show_columns,
            .values = show_default_values,
            .column_count = variable_row_column_count,
            .row_count = 1U,
            .context = "default show row",
        }
    );

    failures += expect_nonquery_result(
        database,
        "SET SESSION information_schema_stats_expiry = 0",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_query_result(
        database,
        "SELECT @@information_schema_stats_expiry, "
        "@@global.information_schema_stats_expiry, "
        "@@session.information_schema_stats_expiry, "
        "@@local.information_schema_stats_expiry, "
        "HEX(@@information_schema_stats_expiry), @@warning_count, ROW_COUNT()",
        (struct expected_result){
            .columns = scalar_columns,
            .values = zero_scalar_values,
            .column_count = scalar_column_count,
            .row_count = 1U,
            .context = "zero scalar values",
        }
    );
    failures += expect_query_result(
        database,
        "SHOW SESSION VARIABLES LIKE 'information_schema_stats_expiry'",
        (struct expected_result){
            .columns = show_columns,
            .values = show_zero_values,
            .column_count = variable_row_column_count,
            .row_count = 1U,
            .context = "zero show row",
        }
    );

    failures += expect_nonquery_result(
        database,
        "SET LOCAL information_schema_stats_expiry = +7",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_nonquery_result(
        database,
        "SET @@information_schema_stats_expiry = 8",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_nonquery_result(
        database,
        "SET @@SESSION.information_schema_stats_expiry = 9",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_nonquery_result(
        database,
        "SET @@LOCAL.information_schema_stats_expiry = 10",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_nonquery_result(
        database,
        "SET SESSION information_schema_stats_expiry = (11)",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_nonquery_result(
        database,
        "SET @stats_expiry = 31536000",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_nonquery_result(
        database,
        "SET information_schema_stats_expiry = @stats_expiry",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_query_result(
        database,
        "SELECT @@information_schema_stats_expiry, "
        "@@global.information_schema_stats_expiry, "
        "@@session.information_schema_stats_expiry, "
        "@@local.information_schema_stats_expiry, "
        "HEX(@@information_schema_stats_expiry), @@warning_count, ROW_COUNT()",
        (struct expected_result){
            .columns = scalar_columns,
            .values = max_scalar_values,
            .column_count = scalar_column_count,
            .row_count = 1U,
            .context = "max scalar values",
        }
    );

    failures += expect_nonquery_result(
        database,
        "SET GLOBAL information_schema_stats_expiry = DEFAULT",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_nonquery_result(
        database,
        "SET @@GLOBAL.information_schema_stats_expiry = 86400",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += execute_error(
        database,
        "SET GLOBAL information_schema_stats_expiry = 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "SET information_schema_stats_expiry supports only fixed no-op global assignments",
        }
    );
    failures += expect_nonquery_result(
        database,
        "SET @@GLOBAL.information_schema_stats_expiry = 86400",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_query_result(
        database,
        "SELECT @@information_schema_stats_expiry, "
        "@@global.information_schema_stats_expiry, "
        "@@session.information_schema_stats_expiry, "
        "@@local.information_schema_stats_expiry, "
        "HEX(@@information_schema_stats_expiry), @@warning_count, ROW_COUNT()",
        (struct expected_result){
            .columns = scalar_columns,
            .values = max_scalar_values,
            .column_count = scalar_column_count,
            .row_count = 1U,
            .context = "global no-op leaves session unchanged",
        }
    );

    session = mylite_connection_session_state(database);
    failures += expect_uint64(
        session == NULL ? 0U : session->information_schema_stats_expiry,
        information_schema_stats_expiry_max,
        "session stats expiry state"
    );
    failures += expect_uint64(
        session == NULL ? 1U : session->catalog_generation,
        catalog_generation,
        "catalog generation unchanged"
    );
    failures += expect_uint64(
        session == NULL ? 1U : session->sqlite_schema_generation,
        sqlite_schema_generation,
        "sqlite schema generation unchanged"
    );
    failures +=
        expect_int(read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)), 0, "preamble");
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(actual_preamble),
        "preamble unchanged"
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen stats expiry file");
    failures += expect_query_result(
        database,
        "SHOW VARIABLES LIKE 'information_schema_stats_expiry'",
        (struct expected_result){
            .columns = show_columns,
            .values = show_default_values,
            .column_count = variable_row_column_count,
            .row_count = 1U,
            .context = "reopen resets session value",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_information_schema_stats_expiry_assignment_diagnostics(void) {
    static const char *const scalar_columns[] = {
        "@@information_schema_stats_expiry",
        "@@warning_count",
        "ROW_COUNT()",
    };
    static const char *const negative_values[] = {"0", "1", "-1"};
    static const char *const over_values[] = {"31536000", "1", "-1"};
    static const char *const true_values[] = {"1", "0", "0"};
    static const char *const false_values[] = {"0", "0", "0"};
    static const char *const rollback_values[] = {"12", "1", "-1"};
    static const char *const negative_warning[] = {
        "Warning",
        "1292",
        "Truncated incorrect information_schema_stats_expiry value: '-1'",
    };
    static const char *const over_warning[] = {
        "Warning",
        "1292",
        "Truncated incorrect information_schema_stats_expiry value: '31536001'",
    };
    struct expected_sql_error incorrect_argument = {
        .code = mysql_error_incorrect_argument_type,
        .sqlstate = "42000",
        .message_part = "Incorrect argument type to variable 'information_schema_stats_expiry'",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open diagnostics memory");

    failures += expect_nonquery_result(
        database,
        "SET information_schema_stats_expiry = -1",
        (struct expected_statement){.affected_rows = 0, .warning_count = 1U}
    );
    failures += expect_query_result(
        database,
        "SHOW WARNINGS",
        (struct expected_result){
            .columns = (const char *const[]){"Level", "Code", "Message"},
            .values = negative_warning,
            .column_count = warning_column_count,
            .row_count = 1U,
            .context = "negative warning",
        }
    );
    failures += expect_query_result(
        database,
        "SELECT @@information_schema_stats_expiry, @@warning_count, ROW_COUNT()",
        (struct expected_result){
            .columns = scalar_columns,
            .values = negative_values,
            .column_count = 3U,
            .row_count = 1U,
            .context = "negative clamp value",
        }
    );

    failures += expect_nonquery_result(
        database,
        "SET information_schema_stats_expiry = 31536001",
        (struct expected_statement){.affected_rows = 0, .warning_count = 1U}
    );
    failures += expect_query_result(
        database,
        "SHOW WARNINGS",
        (struct expected_result){
            .columns = (const char *const[]){"Level", "Code", "Message"},
            .values = over_warning,
            .column_count = warning_column_count,
            .row_count = 1U,
            .context = "over max warning",
        }
    );
    failures += expect_query_result(
        database,
        "SELECT @@information_schema_stats_expiry, @@warning_count, ROW_COUNT()",
        (struct expected_result){
            .columns = scalar_columns,
            .values = over_values,
            .column_count = 3U,
            .row_count = 1U,
            .context = "over max clamp value",
        }
    );

    failures += expect_nonquery_result(
        database,
        "SET information_schema_stats_expiry = TRUE",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_query_result(
        database,
        "SELECT @@information_schema_stats_expiry, @@warning_count, ROW_COUNT()",
        (struct expected_result){
            .columns = scalar_columns,
            .values = true_values,
            .column_count = 3U,
            .row_count = 1U,
            .context = "true value",
        }
    );
    failures += expect_nonquery_result(
        database,
        "SET information_schema_stats_expiry = FALSE",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_query_result(
        database,
        "SELECT @@information_schema_stats_expiry, @@warning_count, ROW_COUNT()",
        (struct expected_result){
            .columns = scalar_columns,
            .values = false_values,
            .column_count = 3U,
            .row_count = 1U,
            .context = "false value",
        }
    );

    failures += expect_nonquery_result(
        database,
        "SET @stats_expiry = -2",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_nonquery_result(
        database,
        "SET information_schema_stats_expiry = @stats_expiry",
        (struct expected_statement){.affected_rows = 0, .warning_count = 1U}
    );
    failures += expect_nonquery_result(
        database,
        "SET information_schema_stats_expiry = 12",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );

    failures +=
        execute_error(database, "SET information_schema_stats_expiry = '5'", incorrect_argument);
    failures +=
        execute_error(database, "SET information_schema_stats_expiry = 1.5", incorrect_argument);
    failures +=
        execute_error(database, "SET information_schema_stats_expiry = NULL", incorrect_argument);
    failures +=
        execute_error(database, "SET information_schema_stats_expiry = ON", incorrect_argument);
    failures +=
        execute_error(database, "SET information_schema_stats_expiry = OFF", incorrect_argument);
    failures += execute_error(
        database,
        "SET information_schema_stats_expiry = 18446744073709551616",
        incorrect_argument
    );
    failures += expect_nonquery_result(
        database,
        "SET @stats_expiry_text = '5'",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += execute_error(
        database,
        "SET information_schema_stats_expiry = @stats_expiry_text",
        incorrect_argument
    );
    failures += execute_error(
        database,
        "SET information_schema_stats_expiry = 5, information_schema_stats_expiry = 'bad'",
        incorrect_argument
    );
    failures += expect_query_result(
        database,
        "SELECT @@information_schema_stats_expiry, @@warning_count, ROW_COUNT()",
        (struct expected_result){
            .columns = scalar_columns,
            .values = rollback_values,
            .column_count = 3U,
            .row_count = 1U,
            .context = "multi-assignment rollback",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_information_schema_stats_expiry_independent_handles(void) {
    static const char *const columns[] = {
        "@@information_schema_stats_expiry",
        "@@warning_count",
        "@@error_count",
    };
    static const char *const first_values[] = {"5", "0", "0"};
    static const char *const second_values[] = {"86400", "0", "0"};
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&first), MYLITE_OK, "open first handle");
    failures += expect_int(mylite_open_memory(&second), MYLITE_OK, "open second handle");
    failures += expect_nonquery_result(
        first,
        "SET information_schema_stats_expiry = 5",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_query_result(
        first,
        "SELECT @@information_schema_stats_expiry, @@warning_count, @@error_count",
        (struct expected_result){
            .columns = columns,
            .values = first_values,
            .column_count = 3U,
            .row_count = 1U,
            .context = "first handle value",
        }
    );
    failures += expect_query_result(
        second,
        "SELECT @@information_schema_stats_expiry, @@warning_count, @@error_count",
        (struct expected_result){
            .columns = columns,
            .values = second_values,
            .column_count = 3U,
            .row_count = 1U,
            .context = "second handle value",
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

    failures +=
        expect_size(mylite_result_column_count(result), expected.column_count, expected.context);
    failures += expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, expected.context);
    for (size_t row = 0U; row < expected.row_count; ++row) {
        for (size_t column = 0U; column < expected.column_count; ++column) {
            const size_t index = (row * expected.column_count) + column;

            failures += expect_text_or_null(
                mylite_result_column_name(result, column),
                expected.columns[column],
                expected.context
            );
            failures += expect_text_or_null(
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

    failures += expect_size(mylite_result_column_count(result), 0U, sql);
    failures += expect_size(mylite_result_row_count(result), 0U, sql);
    failures += expect_int64(mylite_result_affected_rows(result), expected.affected_rows, sql);
    failures += expect_size(mylite_result_warning_count(result), expected.warning_count, sql);
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

    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text_or_null(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_text_contains(mylite_errmsg(database), expected.message_part, sql);
    return failures;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "%s/mylite_information_schema_stats_expiry_%d_%s.mylite",
        P_tmpdir,
        current_process_id(),
        name
    );

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "test path is too long\n");
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
    char related_path[test_path_capacity + test_path_suffix_capacity];
    int written = snprintf(related_path, sizeof(related_path), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related_path)) {
        return;
    }

    (void)remove(related_path);
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fclose(file);
        return 1;
    }
    if (fread(buffer, 1U, size, file) != size) {
        fclose(file);
        return 1;
    }

    fclose(file);
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

static int expect_uint64(uint64_t actual, uint64_t expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %" PRIu64 ", got %" PRIu64 "\n", context, expected, actual);
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
    if (actual != NULL && expected != NULL && strcmp(actual, expected) == 0) {
        return 0;
    }

    fprintf(
        stderr,
        "%s: expected text [%s], got [%s]\n",
        context,
        expected == NULL ? "(null)" : expected,
        actual == NULL ? "(null)" : actual
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
        actual == NULL ? "(null)" : actual,
        needle == NULL ? "(null)" : needle
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

    fprintf(stderr, "%s: byte buffer mismatch\n", context);
    return 1;
}
