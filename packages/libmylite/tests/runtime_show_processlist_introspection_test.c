#include <mylite/mylite.h>

#include "runtime/mylite_connection.h"
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
    show_processlist_column_count = 8,
    show_processlist_id_column = 0,
    show_processlist_user_column = 1,
    show_processlist_host_column = 2,
    show_processlist_db_column = 3,
    show_processlist_command_column = 4,
    show_processlist_time_column = 5,
    show_processlist_state_column = 6,
    show_processlist_info_column = 7,
    show_processlist_info_truncation_length = 100,
    row_count_text_capacity = 32,
    connection_id_text_capacity = 32,
    long_comment_length = 160,
    long_sql_capacity = 256,
    suffix_capacity = 16,
    mysql_error_parse = 1064,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_show_processlist_result {
    const char *sql;
    const char *expected_db;
    const char *expected_info;
    size_t expected_info_length;
    const char *context;
    char *out_connection_id;
    size_t out_connection_id_size;
};

static const char *const show_processlist_names[show_processlist_column_count] = {
    "Id",
    "User",
    "Host",
    "db",
    "Command",
    "Time",
    "State",
    "Info",
};

static int test_show_processlist_result_shape_session_info_and_preamble(void);
static int test_show_processlist_diagnostics_and_unsupported_forms(void);
static int test_independent_show_processlist_handles(void);
static int expect_show_processlist_result(
    mylite_db *database,
    struct expected_show_processlist_result expectation
);
static int expect_row_count(mylite_db *database, int64_t expected, const char *context);
static int expect_connection_id(mylite_db *database, const char *expected, const char *context);
static int expect_decimal_text(const char *text, const char *context);
static int build_long_processlist_sql(
    char *buffer,
    size_t buffer_size,
    const char *prefix,
    const char *suffix
);
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

    failures += test_show_processlist_result_shape_session_info_and_preamble();
    failures += test_show_processlist_diagnostics_and_unsupported_forms();
    failures += test_independent_show_processlist_handles();

    return failures == 0 ? 0 : 1;
}

static int test_show_processlist_result_shape_session_info_and_preamble(void) {
    char path[test_path_capacity];
    char nonfull_sql[long_sql_capacity];
    char full_sql[long_sql_capacity];
    char expected_full_info[long_sql_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    const struct mylite_session_state *session = NULL;
    uint64_t catalog_generation = 0U;
    uint64_t sqlite_schema_generation = 0U;
    mylite_db *database = NULL;
    int failures = 0;

    mylite_result_free(NULL);
    if (make_test_path(path, sizeof(path), "result") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open processlist file");
    failures += expect_show_processlist_result(
        database,
        (struct expected_show_processlist_result){
            .sql = "SHOW PROCESSLIST",
            .expected_db = NULL,
            .expected_info = "SHOW PROCESSLIST",
            .expected_info_length = strlen("SHOW PROCESSLIST"),
            .context = "bare show processlist",
        }
    );
    failures += expect_show_processlist_result(
        database,
        (struct expected_show_processlist_result){
            .sql = "/* lead */ SHOW FULL PROCESSLIST /* trail */;",
            .expected_db = NULL,
            .expected_info = "/* lead */ SHOW FULL PROCESSLIST /* trail */",
            .expected_info_length = strlen("/* lead */ SHOW FULL PROCESSLIST /* trail */"),
            .context = "commented show full processlist",
        }
    );

    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");

    session = mylite_connection_session_state(database);
    catalog_generation = session->catalog_generation;
    sqlite_schema_generation = session->sqlite_schema_generation;
    failures += expect_show_processlist_result(
        database,
        (struct expected_show_processlist_result){
            .sql = "SHOW PROCESSLIST",
            .expected_db = "app",
            .expected_info = "SHOW PROCESSLIST",
            .expected_info_length = strlen("SHOW PROCESSLIST"),
            .context = "selected schema show processlist",
        }
    );
    session = mylite_connection_session_state(database);
    failures += expect_int64(
        (int64_t)session->catalog_generation,
        (int64_t)catalog_generation,
        "catalog generation after show processlist"
    );
    failures += expect_int64(
        (int64_t)session->sqlite_schema_generation,
        (int64_t)sqlite_schema_generation,
        "sqlite schema generation after show processlist"
    );

    failures +=
        build_long_processlist_sql(nonfull_sql, sizeof(nonfull_sql), "SHOW /* ", " */ PROCESSLIST");
    failures += build_long_processlist_sql(
        expected_full_info,
        sizeof(expected_full_info),
        "SHOW FULL /* ",
        " */ PROCESSLIST"
    );
    failures +=
        build_long_processlist_sql(full_sql, sizeof(full_sql), "SHOW FULL /* ", " */ PROCESSLIST");
    if (failures == 0) {
        char expected_nonfull_info[show_processlist_info_truncation_length + 1U];

        memcpy(expected_nonfull_info, nonfull_sql, show_processlist_info_truncation_length);
        expected_nonfull_info[show_processlist_info_truncation_length] = '\0';
        failures += expect_show_processlist_result(
            database,
            (struct expected_show_processlist_result){
                .sql = nonfull_sql,
                .expected_db = "app",
                .expected_info = expected_nonfull_info,
                .expected_info_length = show_processlist_info_truncation_length,
                .context = "truncated show processlist info",
            }
        );
        failures += expect_show_processlist_result(
            database,
            (struct expected_show_processlist_result){
                .sql = full_sql,
                .expected_db = "app",
                .expected_info = expected_full_info,
                .expected_info_length = strlen(expected_full_info),
                .context = "full show processlist info",
            }
        );
    }

    failures += execute_statement_ok(database, "DROP DATABASE app");
    failures += expect_show_processlist_result(
        database,
        (struct expected_show_processlist_result){
            .sql = "SHOW PROCESSLIST",
            .expected_db = NULL,
            .expected_info = "SHOW PROCESSLIST",
            .expected_info_length = strlen("SHOW PROCESSLIST"),
            .context = "dropped selected schema show processlist",
        }
    );

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(actual_preamble),
        "processlist preamble"
    );

    mylite_close(database);
    database = NULL;
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen processlist file");
    failures += execute_statement_ok(database, "CREATE DATABASE reopened");
    failures += execute_statement_ok(database, "USE reopened");
    failures += expect_show_processlist_result(
        database,
        (struct expected_show_processlist_result){
            .sql = "SHOW PROCESSLIST",
            .expected_db = "reopened",
            .expected_info = "SHOW PROCESSLIST",
            .expected_info_length = strlen("SHOW PROCESSLIST"),
            .context = "reopened selected schema show processlist",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_show_processlist_diagnostics_and_unsupported_forms(void) {
    static const char *const unsupported_forms[] = {
        "SHOW PROCESSLIST LIKE 'root%'",
        "SHOW PROCESSLIST WHERE Id > 0",
        "SHOW PROCESSLIST ORDER BY Id",
        "SHOW PROCESSLIST LIMIT 1",
        "SHOW FULL PROCESSLIST LIMIT 1",
        "SHOW PROCESSLIST FROM app",
        "SHOW PROCESSLIST IN app",
        "SHOW EXTENDED PROCESSLIST",
        "SHOW PROCESSLIST + 1",
        "SHOW PROCESSLIST()",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open processlist memory");
    for (size_t form_index = 0U;
         form_index < sizeof(unsupported_forms) / sizeof(unsupported_forms[0]);
         ++form_index) {
        failures += execute_error(
            database,
            unsupported_forms[form_index],
            (struct expected_sql_error){
                .code = mysql_error_parse,
                .sqlstate = "42000",
                .message_part = "SQL syntax",
            }
        );
    }

    mylite_close(database);
    return failures;
}

static int test_independent_show_processlist_handles(void) {
    char first_id[connection_id_text_capacity];
    char second_id[connection_id_text_capacity];
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "first") != 0 ||
        make_test_path(second_path, sizeof(second_path), "second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures +=
        expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first processlist handle");
    failures +=
        expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second processlist handle");
    failures += execute_statement_ok(first, "CREATE DATABASE first_app");
    failures += execute_statement_ok(first, "USE first_app");
    failures += execute_statement_ok(second, "CREATE DATABASE second_app");
    failures += execute_statement_ok(second, "USE second_app");
    failures += expect_show_processlist_result(
        first,
        (struct expected_show_processlist_result){
            .sql = "SHOW PROCESSLIST",
            .expected_db = "first_app",
            .expected_info = "SHOW PROCESSLIST",
            .expected_info_length = strlen("SHOW PROCESSLIST"),
            .context = "first handle show processlist",
            .out_connection_id = first_id,
            .out_connection_id_size = sizeof(first_id),
        }
    );
    failures += expect_show_processlist_result(
        second,
        (struct expected_show_processlist_result){
            .sql = "SHOW PROCESSLIST",
            .expected_db = "second_app",
            .expected_info = "SHOW PROCESSLIST",
            .expected_info_length = strlen("SHOW PROCESSLIST"),
            .context = "second handle show processlist",
            .out_connection_id = second_id,
            .out_connection_id_size = sizeof(second_id),
        }
    );
    if (strcmp(first_id, second_id) == 0) {
        fprintf(stderr, "expected independent processlist ids, both were %s\n", first_id);
        ++failures;
    }

    mylite_close(second);
    mylite_close(first);
    remove_related_files(second_path);
    remove_related_files(first_path);
    return failures;
}

static int expect_show_processlist_result(
    mylite_db *database,
    struct expected_show_processlist_result expectation
) {
    char connection_id_copy[connection_id_text_capacity];
    mylite_result *result = NULL;
    const char *connection_id = NULL;
    const char *info = NULL;
    int failures = 0;

    connection_id_copy[0] = '\0';
    failures += execute_ok(database, expectation.sql, &result);
    if (result == NULL) {
        return failures + 1;
    }

    failures += expect_size(
        mylite_result_column_count(result),
        show_processlist_column_count,
        expectation.context
    );
    for (size_t column_index = 0U; column_index < show_processlist_column_count; ++column_index) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, column_index),
            show_processlist_names[column_index],
            expectation.context
        );
    }
    failures += expect_size(mylite_result_row_count(result), 1U, expectation.context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, expectation.context);
    failures += expect_size(mylite_result_warning_count(result), 1U, expectation.context);
    connection_id = mylite_result_value_text(result, 0U, show_processlist_id_column);
    failures += expect_decimal_text(connection_id, expectation.context);
    if (connection_id != NULL) {
        int written = snprintf(connection_id_copy, sizeof(connection_id_copy), "%s", connection_id);
        if (written < 0 || (size_t)written >= sizeof(connection_id_copy)) {
            fprintf(stderr, "failed to copy processlist id for %s\n", expectation.context);
            ++failures;
        }
    }
    failures += expect_text_or_null(
        mylite_result_value_text(result, 0U, show_processlist_user_column),
        "root",
        expectation.context
    );
    failures += expect_text_or_null(
        mylite_result_value_text(result, 0U, show_processlist_host_column),
        "%",
        expectation.context
    );
    failures += expect_text_or_null(
        mylite_result_value_text(result, 0U, show_processlist_db_column),
        expectation.expected_db,
        expectation.context
    );
    failures += expect_text_or_null(
        mylite_result_value_text(result, 0U, show_processlist_command_column),
        "Query",
        expectation.context
    );
    failures += expect_text_or_null(
        mylite_result_value_text(result, 0U, show_processlist_time_column),
        "0",
        expectation.context
    );
    failures += expect_text_or_null(
        mylite_result_value_text(result, 0U, show_processlist_state_column),
        "init",
        expectation.context
    );
    info = mylite_result_value_text(result, 0U, show_processlist_info_column);
    failures += expect_text_or_null(info, expectation.expected_info, expectation.context);
    if (info == NULL) {
        fprintf(stderr, "%s: expected non-null processlist Info\n", expectation.context);
        ++failures;
    } else {
        failures +=
            expect_size(strlen(info), expectation.expected_info_length, expectation.context);
    }
    if (expectation.out_connection_id != NULL) {
        int written = snprintf(
            expectation.out_connection_id,
            expectation.out_connection_id_size,
            "%s",
            connection_id_copy
        );
        if (written < 0 || (size_t)written >= expectation.out_connection_id_size) {
            fprintf(stderr, "failed to copy processlist id for %s\n", expectation.context);
            ++failures;
        }
    }

    mylite_result_free(result);
    failures += expect_row_count(database, -1, expectation.context);
    failures += expect_connection_id(database, connection_id_copy, expectation.context);
    return failures;
}

static int expect_row_count(mylite_db *database, int64_t expected, const char *context) {
    char expected_text[row_count_text_capacity];
    mylite_result *result = NULL;
    int written = snprintf(expected_text, sizeof(expected_text), "%" PRId64, expected);
    int failures = 0;

    if (written < 0 || (size_t)written >= sizeof(expected_text)) {
        fprintf(stderr, "failed to format row count expectation for %s\n", context);
        return 1;
    }

    failures += execute_ok(database, "SELECT ROW_COUNT()", &result);
    failures += expect_size(mylite_result_column_count(result), 1U, context);
    failures += expect_text_or_null(mylite_result_column_name(result, 0U), "ROW_COUNT()", context);
    failures += expect_size(mylite_result_row_count(result), 1U, context);
    failures +=
        expect_text_or_null(mylite_result_value_text(result, 0U, 0U), expected_text, context);
    failures += expect_size(mylite_result_warning_count(result), 0U, context);
    mylite_result_free(result);
    return failures;
}

static int expect_connection_id(mylite_db *database, const char *expected, const char *context) {
    mylite_result *result = NULL;
    int failures = 0;

    failures += execute_ok(database, "SELECT CONNECTION_ID()", &result);
    failures += expect_size(mylite_result_column_count(result), 1U, context);
    failures +=
        expect_text_or_null(mylite_result_column_name(result, 0U), "CONNECTION_ID()", context);
    failures += expect_size(mylite_result_row_count(result), 1U, context);
    failures += expect_text_or_null(mylite_result_value_text(result, 0U, 0U), expected, context);
    failures += expect_size(mylite_result_warning_count(result), 0U, context);
    mylite_result_free(result);
    return failures;
}

static int expect_decimal_text(const char *text, const char *context) {
    if (text == NULL || text[0] == '\0') {
        fprintf(stderr, "%s: expected decimal text, got null or empty\n", context);
        return 1;
    }
    for (size_t index = 0U; text[index] != '\0'; ++index) {
        if (text[index] < '0' || text[index] > '9') {
            fprintf(stderr, "%s: expected decimal text, got %s\n", context, text);
            return 1;
        }
    }
    return 0;
}

static int build_long_processlist_sql(
    char *buffer,
    size_t buffer_size,
    const char *prefix,
    const char *suffix
) {
    size_t prefix_length = strlen(prefix);
    size_t suffix_length = strlen(suffix);
    size_t required = prefix_length + long_comment_length + suffix_length;

    if (required >= buffer_size) {
        fprintf(stderr, "long processlist SQL buffer is too small\n");
        return 1;
    }
    memcpy(buffer, prefix, prefix_length);
    memset(buffer + prefix_length, 'x', long_comment_length);
    memcpy(buffer + prefix_length + long_comment_length, suffix, suffix_length);
    buffer[required] = '\0';
    return 0;
}

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (result != NULL) {
        failures += expect_size(mylite_result_column_count(result), 0U, sql);
        failures += expect_size(mylite_result_row_count(result), 0U, sql);
        failures += expect_size(mylite_result_warning_count(result), 0U, sql);
    }
    mylite_result_free(result);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "expected success for %s, got rc=%d code=%d state=%s message=%s\n",
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
        fprintf(stderr, "expected error for %s, got rc=%d\n", sql, rc);
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
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_runtime_show_processlist_%s_%d.mylite",
        name,
        current_process_id()
    );

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "failed to create test path\n");
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
    remove(path);
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related_path[test_path_capacity + suffix_capacity];
    int written = snprintf(related_path, sizeof(related_path), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related_path)) {
        return;
    }
    remove(related_path);
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t bytes_read = 0U;

    if (file == NULL) {
        fprintf(stderr, "failed to open %s for reading\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "failed to seek in %s\n", path);
        fclose(file);
        return 1;
    }
    bytes_read = fread(buffer, 1U, size, file);
    fclose(file);
    if (bytes_read != size) {
        fprintf(stderr, "failed to read %zu bytes from %s\n", size, path);
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
        fprintf(stderr, "%s: expected %" PRId64 ", got %" PRId64 "\n", context, expected, actual);
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
            "%s: expected %s, got %s\n",
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
            "%s: expected text containing %s, got %s\n",
            context,
            needle == NULL ? "NULL" : needle,
            actual == NULL ? "NULL" : actual
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
