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

enum {
    test_path_capacity = 1024,
    show_warnings_column_count = 3,
    show_count_warnings_column_count = 1,
    row_count_column = 0,
    row_count_text_capacity = 32,
    mysql_error_parse = 1064,
    mysql_warning_processlist_deprecated = 1287,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_show_warnings {
    const char *sql;
    size_t row_count;
    const char *level;
    const char *code;
    const char *message_part;
    const char *context;
};

static const char *const show_warnings_names[show_warnings_column_count] = {
    "Level",
    "Code",
    "Message",
};
static const char *const processlist_warning_message =
    "'INFORMATION_SCHEMA.PROCESSLIST' is deprecated and will be removed in a future release. "
    "Please use performance_schema.processlist instead";

static int test_show_warnings_snapshot_lifecycle_and_preamble(void);
static int test_show_warnings_limits_errors_and_unsupported_forms(void);
static int test_independent_show_warnings_handles(void);
static int expect_show_warnings_result(
    mylite_db *database,
    struct expected_show_warnings expectation
);
static int expect_show_count_warnings(
    mylite_db *database,
    const char *expected,
    const char *context
);
static int expect_row_count(mylite_db *database, int64_t expected, const char *context);
static int expect_public_ok_diagnostics(mylite_db *database, const char *context);
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

    failures += test_show_warnings_snapshot_lifecycle_and_preamble();
    failures += test_show_warnings_limits_errors_and_unsupported_forms();
    failures += test_independent_show_warnings_handles();

    return failures == 0 ? 0 : 1;
}

static int test_show_warnings_snapshot_lifecycle_and_preamble(void) {
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    const struct mylite_session_state *session = NULL;
    uint64_t catalog_generation = 0U;
    uint64_t sqlite_schema_generation = 0U;
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    mylite_result_free(NULL);
    if (make_test_path(path, sizeof(path), "lifecycle") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open warnings file");
    session = mylite_connection_session_state(database);
    catalog_generation = session->catalog_generation;
    sqlite_schema_generation = session->sqlite_schema_generation;

    failures += execute_statement_ok(database, "SELECT ROW_COUNT()");
    failures += expect_show_warnings_result(
        database,
        (struct expected_show_warnings){
            .sql = "SHOW WARNINGS",
            .row_count = 0U,
            .context = "empty warnings",
        }
    );
    failures += expect_show_count_warnings(database, "0", "empty warning count");
    failures += expect_row_count(database, -1, "empty diagnostics row count");

    failures += execute_ok(database, "SHOW PROCESSLIST", &result);
    failures += expect_size(
        mylite_result_warning_count(result),
        1U,
        "show processlist result warning count"
    );
    mylite_result_free(result);
    failures += expect_show_warnings_result(
        database,
        (struct expected_show_warnings){
            .sql = "SHOW WARNINGS",
            .row_count = 1U,
            .level = "Warning",
            .code = "1287",
            .message_part = processlist_warning_message,
            .context = "processlist warning row",
        }
    );
    failures += expect_public_ok_diagnostics(database, "show warnings public diagnostics");
    failures += expect_show_count_warnings(database, "1", "processlist warning count");
    failures += expect_show_warnings_result(
        database,
        (struct expected_show_warnings){
            .sql = "SHOW WARNINGS",
            .row_count = 1U,
            .level = "Warning",
            .code = "1287",
            .message_part = processlist_warning_message,
            .context = "diagnostic chaining warning row",
        }
    );

    failures += execute_statement_ok(database, "SELECT ROW_COUNT()");
    failures += expect_show_count_warnings(database, "0", "ordinary statement clears warnings");

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
        "read warnings preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "warnings preamble unchanged"
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_show_warnings_limits_errors_and_unsupported_forms(void) {
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open warnings memory");
    failures += execute_statement_ok(database, "SELECT ROW_COUNT()");
    failures += execute_statement_ok(database, "SHOW PROCESSLIST");

    failures += expect_show_warnings_result(
        database,
        (struct expected_show_warnings){
            .sql = "SHOW WARNINGS LIMIT 0",
            .row_count = 0U,
            .context = "limit zero",
        }
    );
    failures += expect_show_count_warnings(database, "1", "limit zero preserves count");
    failures += expect_show_warnings_result(
        database,
        (struct expected_show_warnings){
            .sql = "SHOW WARNINGS LIMIT 1",
            .row_count = 1U,
            .level = "Warning",
            .code = "1287",
            .message_part = processlist_warning_message,
            .context = "limit one",
        }
    );
    failures += expect_show_warnings_result(
        database,
        (struct expected_show_warnings){
            .sql = "SHOW WARNINGS LIMIT 0, 1",
            .row_count = 1U,
            .level = "Warning",
            .code = "1287",
            .message_part = processlist_warning_message,
            .context = "comma offset includes warning",
        }
    );
    failures += expect_show_warnings_result(
        database,
        (struct expected_show_warnings){
            .sql = "SHOW WARNINGS LIMIT 1, 1",
            .row_count = 0U,
            .context = "comma offset skips warning",
        }
    );
    failures += expect_show_warnings_result(
        database,
        (struct expected_show_warnings){
            .sql = "SHOW WARNINGS LIMIT 1 OFFSET 0",
            .row_count = 1U,
            .level = "Warning",
            .code = "1287",
            .message_part = processlist_warning_message,
            .context = "offset includes warning",
        }
    );
    failures += expect_show_warnings_result(
        database,
        (struct expected_show_warnings){
            .sql = "SHOW WARNINGS LIMIT 1 OFFSET 1",
            .row_count = 0U,
            .context = "offset skips warning",
        }
    );
    failures += expect_show_warnings_result(
        database,
        (struct expected_show_warnings){
            .sql = "SHOW WARNINGS LIMIT 18446744073709551615",
            .row_count = 1U,
            .level = "Warning",
            .code = "1287",
            .message_part = processlist_warning_message,
            .context = "uint64 max limit",
        }
    );

    failures += execute_error(
        database,
        "SHOW WARNINGS LIMIT 18446744073709551616",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "LIMIT literal is outside",
        }
    );
    failures += execute_error(
        database,
        "SHOW WARNINGS LIMIT +1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW COUNT (*) WARNINGS",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW WARNINGS LIKE 'x'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "BAD SQL",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += expect_show_warnings_result(
        database,
        (struct expected_show_warnings){
            .sql = "SHOW WARNINGS",
            .row_count = 1U,
            .level = "Error",
            .code = "1064",
            .message_part = "SQL syntax",
            .context = "parse error diagnostic row",
        }
    );
    failures += expect_show_count_warnings(database, "1", "parse error warning count");
    failures +=
        expect_public_ok_diagnostics(database, "parse error show warnings clears live error");

    mylite_close(database);

    return failures;
}

static int test_independent_show_warnings_handles(void) {
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&first), MYLITE_OK, "open first warnings handle");
    failures += expect_int(mylite_open_memory(&second), MYLITE_OK, "open second warnings handle");

    failures += execute_statement_ok(first, "SHOW PROCESSLIST");
    failures += expect_show_count_warnings(first, "1", "first handle warning count");
    failures += expect_show_count_warnings(second, "0", "second handle warning count");

    failures += execute_statement_ok(first, "SELECT ROW_COUNT()");
    failures += expect_show_count_warnings(first, "0", "first handle cleared warning count");
    failures += expect_show_count_warnings(second, "0", "second handle still empty");

    mylite_close(second);
    mylite_close(first);

    return failures;
}

static int expect_show_warnings_result(
    mylite_db *database,
    struct expected_show_warnings expectation
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, expectation.sql, &result);

    failures += expect_size(
        mylite_result_column_count(result),
        show_warnings_column_count,
        expectation.context
    );
    for (size_t column_index = 0U; column_index < show_warnings_column_count; ++column_index) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, column_index),
            show_warnings_names[column_index],
            expectation.context
        );
    }
    failures +=
        expect_size(mylite_result_row_count(result), expectation.row_count, expectation.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, expectation.context);

    if (expectation.row_count > 0U) {
        failures += expect_text_or_null(
            mylite_result_value_text(result, 0U, 0U),
            expectation.level,
            expectation.context
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, 0U, 1U),
            expectation.code,
            expectation.context
        );
        failures += expect_text_contains(
            mylite_result_value_text(result, 0U, 2U),
            expectation.message_part,
            expectation.context
        );
    }

    mylite_result_free(result);
    return failures;
}

static int expect_show_count_warnings(
    mylite_db *database,
    const char *expected,
    const char *context
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, "SHOW COUNT(*) WARNINGS", &result);

    failures +=
        expect_size(mylite_result_column_count(result), show_count_warnings_column_count, context);
    failures += expect_text_or_null(
        mylite_result_column_name(result, 0U),
        "@@session.warning_count",
        context
    );
    failures += expect_size(mylite_result_row_count(result), 1U, context);
    failures += expect_text_or_null(mylite_result_value_text(result, 0U, 0U), expected, context);
    failures += expect_size(mylite_result_warning_count(result), 0U, context);

    mylite_result_free(result);
    return failures;
}

static int expect_row_count(mylite_db *database, int64_t expected, const char *context) {
    mylite_result *result = NULL;
    const char *value = NULL;
    char expected_text[row_count_text_capacity];
    int failures = execute_ok(database, "SELECT ROW_COUNT()", &result);
    int written = snprintf(expected_text, sizeof(expected_text), "%lld", (long long)expected);

    if (written < 0 || (size_t)written >= sizeof(expected_text)) {
        fprintf(stderr, "%s: failed to format expected row count\n", context);
        failures += 1;
    }

    failures += expect_size(mylite_result_row_count(result), 1U, context);
    failures += expect_size(mylite_result_column_count(result), 1U, context);
    value = mylite_result_value_text(result, 0U, row_count_column);
    failures += expect_text_or_null(value, expected_text, context);
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

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    mylite_result_free(result);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = MYLITE_OK;

    if (out_result == NULL) {
        return 1;
    }
    *out_result = NULL;

    rc = mylite_execute(database, sql, strlen(sql), out_result);
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

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc == MYLITE_OK) {
        fprintf(stderr, "%s: expected error, got success\n", sql);
        mylite_result_free(result);
        return 1;
    }

    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text_or_null(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_text_contains(mylite_errmsg(database), expected.message_part, sql);
    failures += expect_size(mylite_result_column_count(result), 0U, sql);
    failures += expect_size(mylite_result_row_count(result), 0U, sql);
    mylite_result_free(result);

    return failures;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_runtime_show_warnings_%s_%d.mylite",
        name,
        current_process_id()
    );

    return written < 0 || (size_t)written >= path_size ? 1 : 0;
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

    if (written >= 0 && (size_t)written < sizeof(related_path)) {
        (void)remove(related_path);
    }
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    int status = 0;

    if (file == NULL) {
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0 || fread(buffer, 1U, size, file) != size) {
        status = 1;
    }
    if (fclose(file) != 0) {
        status = 1;
    }

    return status;
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
            "%s: expected \"%s\", got \"%s\"\n",
            context,
            expected == NULL ? "(null)" : expected,
            actual == NULL ? "(null)" : actual
        );
        return 1;
    }

    return 0;
}

static int expect_text_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || needle == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected \"%s\" to contain \"%s\"\n",
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
        fprintf(stderr, "%s: bytes differ\n", context);
        return 1;
    }

    return 0;
}
