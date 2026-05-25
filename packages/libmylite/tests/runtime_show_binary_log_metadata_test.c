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
    mysql_error_parse = 1064,
    show_binary_log_status_column_count = 5,
    show_binary_logs_column_count = 3,
    diagnostics_column_count = 2,
    test_path_capacity = 1024,
};

struct expected_show_result {
    const char *sql;
    const char *const *columns;
    const char *const *values;
    size_t column_count;
};

struct expected_sql_error {
    const char *sql;
    int code;
    const char *sqlstate;
    const char *message_part;
};

static const char *const show_binary_log_status_columns[show_binary_log_status_column_count] = {
    "File",
    "Position",
    "Binlog_Do_DB",
    "Binlog_Ignore_DB",
    "Executed_Gtid_Set",
};

static const char *const show_binary_log_status_values[show_binary_log_status_column_count] = {
    "binlog.000001",
    "4",
    "",
    "",
    "",
};

static const char *const show_binary_logs_columns[show_binary_logs_column_count] = {
    "Log_name",
    "File_size",
    "Encrypted",
};

static const char *const show_binary_logs_values[show_binary_logs_column_count] = {
    "binlog.000001",
    "4",
    "No",
};

static const struct expected_show_result show_binary_log_status = {
    .sql = "SHOW BINARY LOG STATUS",
    .columns = show_binary_log_status_columns,
    .values = show_binary_log_status_values,
    .column_count = show_binary_log_status_column_count,
};

static const struct expected_show_result show_binary_logs = {
    .sql = "SHOW BINARY LOGS",
    .columns = show_binary_logs_columns,
    .values = show_binary_logs_values,
    .column_count = show_binary_logs_column_count,
};

static int test_show_binary_log_metadata_results(void);
static int test_show_binary_log_metadata_file_reopen_and_preamble(void);
static int test_independent_show_binary_log_metadata_handles(void);
static int test_show_binary_log_metadata_unsupported_diagnostics(void);
static int expect_show_binary_log_metadata(
    mylite_db *database,
    struct expected_show_result expected
);
static int expect_show_result_columns(mylite_result *result, struct expected_show_result expected);
static int expect_show_result_row(mylite_result *result, struct expected_show_result expected);
static int expect_row_count_status(mylite_db *database, const char *context);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, struct expected_sql_error expected);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text_or_null(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    mylite_result_free(NULL);

    failures += test_show_binary_log_metadata_results();
    failures += test_show_binary_log_metadata_file_reopen_and_preamble();
    failures += test_independent_show_binary_log_metadata_handles();
    failures += test_show_binary_log_metadata_unsupported_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_show_binary_log_metadata_results(void) {
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        expect_int(mylite_open_memory(&database), MYLITE_OK, "open memory binary log metadata");
    failures += expect_show_binary_log_metadata(database, show_binary_log_status);
    failures += expect_show_binary_log_metadata(database, show_binary_logs);

    mylite_close(database);
    return failures;
}

static int test_show_binary_log_metadata_file_reopen_and_preamble(void) {
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    const struct mylite_session_state *session = NULL;
    uint64_t catalog_generation = 0U;
    uint64_t sqlite_schema_generation = 0U;
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "reopen") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open binary log file");
    if (database == NULL) {
        remove_related_files(path);
        return failures;
    }
    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");
    session = mylite_connection_session_state(database);
    if (session == NULL) {
        fprintf(stderr, "binary log file: expected session state\n");
        mylite_close(database);
        remove_related_files(path);
        return failures + 1;
    }
    catalog_generation = session->catalog_generation;
    sqlite_schema_generation = session->sqlite_schema_generation;
    failures += expect_show_binary_log_metadata(database, show_binary_log_status);
    failures += expect_show_binary_log_metadata(database, show_binary_logs);
    session = mylite_connection_session_state(database);
    failures += expect_int64(
        (int64_t)session->catalog_generation,
        (int64_t)catalog_generation,
        "catalog generation after binary log metadata"
    );
    failures += expect_int64(
        (int64_t)session->sqlite_schema_generation,
        (int64_t)sqlite_schema_generation,
        "sqlite schema generation after binary log metadata"
    );
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after binary log metadata"
    );
    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen binary log file");
    if (database == NULL) {
        remove_related_files(path);
        return failures;
    }
    failures += expect_show_binary_log_metadata(database, show_binary_log_status);
    failures += expect_show_binary_log_metadata(database, show_binary_logs);

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_independent_show_binary_log_metadata_handles(void) {
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&first), MYLITE_OK, "open first binary log handle");
    failures += expect_int(mylite_open_memory(&second), MYLITE_OK, "open second binary log handle");
    failures += expect_show_binary_log_metadata(first, show_binary_log_status);
    failures += expect_show_binary_log_metadata(first, show_binary_logs);
    failures += expect_show_binary_log_metadata(second, show_binary_log_status);
    failures += expect_show_binary_log_metadata(second, show_binary_logs);

    mylite_close(first);
    mylite_close(second);
    return failures;
}

static int test_show_binary_log_metadata_unsupported_diagnostics(void) {
    static const struct expected_sql_error errors[] = {
        {
            .sql = "SHOW BINARY LOG STATUS LIKE '%'",
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        },
        {
            .sql = "SHOW BINARY LOG STATUS WHERE File IS NOT NULL",
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        },
        {
            .sql = "SHOW BINARY LOG STATUS LIMIT 1",
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        },
        {
            .sql = "SHOW BINARY LOGS LIKE '%'",
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        },
        {
            .sql = "SHOW BINARY LOGS WHERE Log_name IS NOT NULL",
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        },
        {
            .sql = "SHOW BINARY LOGS LIMIT 1",
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        },
        {
            .sql = "SHOW FULL BINARY LOG STATUS",
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        },
        {
            .sql = "SHOW FULL BINARY LOGS",
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        },
        {
            .sql = "SHOW MASTER STATUS",
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        },
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open binary log diagnostics");
    for (size_t index = 0U; index < sizeof(errors) / sizeof(errors[0]); ++index) {
        failures += execute_error(database, errors[index]);
    }

    mylite_close(database);
    return failures;
}

static int expect_show_binary_log_metadata(
    mylite_db *database,
    struct expected_show_result expected
) {
    mylite_result *result = NULL;
    int failures = 0;

    failures += execute_ok(database, expected.sql, &result);
    failures += expect_size(
        mylite_result_column_count(result),
        expected.column_count,
        "binary log metadata column count"
    );
    failures += expect_size(mylite_result_row_count(result), 1U, "binary log metadata row count");
    failures += expect_int64(mylite_result_affected_rows(result), 0, "binary log affected rows");
    failures += expect_size(mylite_result_warning_count(result), 0U, "binary log warning count");
    failures += expect_show_result_columns(result, expected);
    failures += expect_show_result_row(result, expected);

    mylite_result_free(result);
    failures += expect_row_count_status(database, expected.sql);
    return failures;
}

static int expect_show_result_columns(mylite_result *result, struct expected_show_result expected) {
    int failures = 0;

    for (size_t column_index = 0U; column_index < expected.column_count; ++column_index) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, column_index),
            expected.columns[column_index],
            "binary log metadata column label"
        );
    }
    return failures;
}

static int expect_show_result_row(mylite_result *result, struct expected_show_result expected) {
    int failures = 0;

    for (size_t column_index = 0U; column_index < expected.column_count; ++column_index) {
        failures += expect_text_or_null(
            mylite_result_value_text(result, 0U, column_index),
            expected.values[column_index],
            expected.sql
        );
    }
    return failures;
}

static int expect_row_count_status(mylite_db *database, const char *context) {
    mylite_result *result = NULL;
    int failures = 0;

    failures += execute_ok(database, "SELECT @@warning_count, ROW_COUNT()", &result);
    failures += expect_size(
        mylite_result_column_count(result),
        diagnostics_column_count,
        "binary log status column count"
    );
    failures += expect_size(mylite_result_row_count(result), 1U, "binary log status row count");
    failures += expect_text_or_null(
        mylite_result_value_text(result, 0U, 0U),
        "0",
        "binary log warning count"
    );
    failures += expect_text_or_null(mylite_result_value_text(result, 0U, 1U), "-1", context);

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
            "%s: expected OK, got %d / %d %s %s\n",
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

static int execute_error(mylite_db *database, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);

    if (rc == MYLITE_OK) {
        fprintf(stderr, "%s: expected error, got OK\n", expected.sql);
        mylite_result_free(result);
        return 1;
    }

    failures += expect_int(mylite_errcode(database), expected.code, expected.sql);
    failures += expect_text_or_null(mylite_sqlstate(database), expected.sqlstate, expected.sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, expected.sql);
    mylite_result_free(result);
    return failures;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    const char *directory = getenv("TMPDIR");
    int written = 0;

    if (directory == NULL || directory[0] == '\0') {
        directory = getenv("TEMP");
    }
    if (directory == NULL || directory[0] == '\0') {
        directory = ".";
    }

    written = snprintf(
        path,
        path_size,
        "%s/mylite_runtime_show_binary_log_metadata_%d_%s.mylite",
        directory,
        current_process_id(),
        name
    );

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
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char buffer[test_path_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(buffer)) {
        (void)remove(buffer);
    }
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        fprintf(stderr, "%s: failed to open file for read\n", path);
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
    fprintf(
        stderr,
        "%s: expected %lld, got %lld\n",
        context,
        (long long)expected,
        (long long)actual
    );
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

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual != NULL && strstr(actual, needle) != NULL) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected [%s] to contain [%s]\n",
        context,
        actual == NULL ? "NULL" : actual,
        needle
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
    fprintf(stderr, "%s: bytes differ\n", context);
    return 1;
}
