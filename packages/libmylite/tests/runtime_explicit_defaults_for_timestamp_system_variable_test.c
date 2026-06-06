#include <mylite/mylite.h>

#include "runtime/mylite_mysql_server_identity.h"

#include "runtime/mylite_connection.h"
#include "storage/mylite_file_format.h"

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
#  define P_tmpdir "."
#endif

enum {
    test_path_capacity = 1024,
    path_suffix_capacity = 16,
    explicit_defaults_for_timestamp_value_column_count = 6,
    explicit_defaults_for_timestamp_label_column_count = 5,
    explicit_defaults_for_timestamp_diagnostics_column_count = 4,
    explicit_defaults_for_timestamp_selected_column_count = 2,
    explicit_defaults_for_timestamp_independent_column_count = 3,
    mysql_error_parse = 1064,
    mysql_error_unknown_system_variable = 1193,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_result {
    const char *const *columns;
    const char *const *values;
    size_t count;
    const char *context;
};

static int test_explicit_defaults_for_timestamp_values_and_persistence(void);
static int test_explicit_defaults_for_timestamp_qualifiers_and_errors(void);
static int test_independent_explicit_defaults_for_timestamp_handles(void);
static int expect_result(const mylite_result *result, struct expected_result expected);
static int expect_query_result(
    mylite_db *database,
    const char *sql,
    struct expected_result expected
);
static int expect_show_count_warnings(
    mylite_db *database,
    const char *expected,
    const char *context
);
static int expect_show_count_errors(mylite_db *database, const char *expected, const char *context);
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

    failures += test_explicit_defaults_for_timestamp_values_and_persistence();
    failures += test_explicit_defaults_for_timestamp_qualifiers_and_errors();
    failures += test_independent_explicit_defaults_for_timestamp_handles();

    return failures == 0 ? 0 : 1;
}

static int test_explicit_defaults_for_timestamp_values_and_persistence(void) {
    static const char *const value_columns[] = {
        "@@explicit_defaults_for_timestamp",
        "@@global.explicit_defaults_for_timestamp",
        "@@session.explicit_defaults_for_timestamp",
        "@@local.explicit_defaults_for_timestamp",
        "@@warning_count",
        "ROW_COUNT()",
    };
    static const char *const value_values[] = {"1", "1", "1", "1", "0", "-1"};
    static const char *const label_columns[] = {
        "@@EXPLICIT_DEFAULTS_FOR_TIMESTAMP",
        "@@Global.Explicit_Defaults_For_Timestamp",
        "@@session.`explicit_defaults_for_timestamp`",
        "@@`explicit_defaults_for_timestamp`",
        "(@@explicit_defaults_for_timestamp)",
    };
    static const char *const label_values[] = {"1", "1", "1", "1", "1"};
    static const char *const mixed_columns[] = {
        "@@explicit_defaults_for_timestamp",
        "@@foreign_key_checks",
        "@@unique_checks",
        "@@updatable_views_with_limit",
        "@@sql_safe_updates",
        "@@sql_select_limit",
        "@@sql_notes",
        "@@sql_warnings",
        "@@sql_quote_show_create",
        "@@autocommit",
        "@@default_storage_engine",
        "@@character_set_server",
        "@@version_comment",
    };
    static const char *const mixed_values[] = {
        "1",
        "1",
        "1",
        "YES",
        "0",
        "18446744073709551615",
        "1",
        "0",
        "1",
        "1",
        "InnoDB",
        "utf8mb4",
        MYLITE_MYSQL_SERVER_VERSION_COMMENT_STRING,
    };
    static const char *const diagnostics_columns[] = {
        "@@explicit_defaults_for_timestamp",
        "@@warning_count",
        "@@error_count",
        "ROW_COUNT()",
    };
    static const char *const warning_values[] = {"1", "1", "0", "-1"};
    static const char *const error_values[] = {"1", "1", "1", "-1"};
    static const char *const selected_columns[] = {
        "@@explicit_defaults_for_timestamp",
        "DATABASE()"
    };
    static const char *const selected_values[] = {"1", "app"};
    static const char *const table_columns[] = {"id", "score"};
    static const char *const table_values[] = {"2", "20"};
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

    failures += expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "open explicit defaults for timestamp file"
    );
    session = mylite_connection_session_state(database);
    catalog_generation = session->catalog_generation;
    sqlite_schema_generation = session->sqlite_schema_generation;

    failures += execute_ok(
        database,
        "SELECT @@explicit_defaults_for_timestamp, @@global.explicit_defaults_for_timestamp, "
        "@@session.explicit_defaults_for_timestamp, @@local.explicit_defaults_for_timestamp, "
        "@@warning_count, "
        "ROW_COUNT() FROM DUAL",
        &result
    );
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = value_columns,
            .values = value_values,
            .count = explicit_defaults_for_timestamp_value_column_count,
            .context = "explicit defaults for timestamp values",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(
        database,
        "SELECT @@EXPLICIT_DEFAULTS_FOR_TIMESTAMP, @@Global.Explicit_Defaults_For_Timestamp, "
        "@@session.`explicit_defaults_for_timestamp`, @@`explicit_defaults_for_timestamp`, "
        "(@@explicit_defaults_for_timestamp)",
        &result
    );
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = label_columns,
            .values = label_values,
            .count = explicit_defaults_for_timestamp_label_column_count,
            .context = "explicit defaults for timestamp labels",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(
        database,
        "SELECT @@explicit_defaults_for_timestamp, @@foreign_key_checks, @@unique_checks, "
        "@@updatable_views_with_limit, @@sql_safe_updates, @@sql_select_limit, "
        "@@sql_notes, @@sql_warnings, @@sql_quote_show_create, @@autocommit, "
        "@@default_storage_engine, @@character_set_server, @@version_comment",
        &result
    );
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = mixed_columns,
            .values = mixed_values,
            .count = sizeof(mixed_columns) / sizeof(mixed_columns[0]),
            .context = "mixed explicit defaults for timestamp scalar values",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_statement_ok(database, "SHOW PROCESSLIST");
    failures += execute_ok(
        database,
        "SELECT @@explicit_defaults_for_timestamp, @@warning_count, @@error_count, ROW_COUNT()",
        &result
    );
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = diagnostics_columns,
            .values = warning_values,
            .count = explicit_defaults_for_timestamp_diagnostics_column_count,
            .context = "explicit defaults for timestamp warning diagnostics",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_show_count_warnings(
        database,
        "0",
        "explicit defaults for timestamp clears warnings"
    );

    failures += execute_error(
        database,
        "BAD SQL",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "BAD",
        }
    );
    failures += execute_ok(
        database,
        "SELECT @@explicit_defaults_for_timestamp, @@warning_count, @@error_count, ROW_COUNT()",
        &result
    );
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = diagnostics_columns,
            .values = error_values,
            .count = explicit_defaults_for_timestamp_diagnostics_column_count,
            .context = "explicit defaults for timestamp error diagnostics",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures +=
        expect_show_count_errors(database, "0", "explicit defaults for timestamp clears errors");
    failures += expect_show_count_warnings(
        database,
        "0",
        "explicit defaults for timestamp clears error warnings"
    );

    session = mylite_connection_session_state(database);
    failures += expect_int64(
        (int64_t)session->catalog_generation,
        (int64_t)catalog_generation,
        "catalog generation unchanged by explicit defaults for timestamp reads"
    );
    failures += expect_int64(
        (int64_t)session->sqlite_schema_generation,
        (int64_t)sqlite_schema_generation,
        "sqlite schema generation unchanged by explicit defaults for timestamp reads"
    );
    failures += expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read explicit defaults for timestamp preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after explicit defaults for timestamp reads"
    );

    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");
    failures += execute_statement_ok(database, "CREATE TABLE child (id INT, score INT)");
    failures += execute_statement_ok(
        database,
        "INSERT INTO child (id, score) VALUES (1, 10),(2, 20),(3, 30)"
    );
    failures += expect_query_result(
        database,
        "SELECT @@explicit_defaults_for_timestamp, DATABASE()",
        (struct expected_result){
            .columns = selected_columns,
            .values = selected_values,
            .count = explicit_defaults_for_timestamp_selected_column_count,
            .context = "explicit defaults for timestamp with selected database",
        }
    );
    failures += expect_query_result(
        database,
        "SELECT id, score FROM child WHERE id = 2 ORDER BY id",
        (struct expected_result){
            .columns = table_columns,
            .values = table_values,
            .count = sizeof(table_columns) / sizeof(table_columns[0]),
            .context = "explicit defaults for timestamp does not alter descriptor select",
        }
    );
    failures += expect_query_result(
        database,
        "SELECT id, score FROM child WHERE id = 2 ORDER BY id LIMIT 1",
        (struct expected_result){
            .columns = table_columns,
            .values = table_values,
            .count = sizeof(table_columns) / sizeof(table_columns[0]),
            .context = "explicit descriptor select limit still applies with explicit defaults for "
                       "timestamp",
        }
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "reopen explicit defaults for timestamp file"
    );
    failures += expect_query_result(
        database,
        "SELECT @@explicit_defaults_for_timestamp, @@global.explicit_defaults_for_timestamp",
        (struct expected_result){
            .columns = value_columns,
            .values = value_values,
            .count = 2U,
            .context = "reopened explicit defaults for timestamp values",
        }
    );
    failures += expect_query_result(
        database,
        "SELECT id, score FROM app.child WHERE id = 2 ORDER BY id",
        (struct expected_result){
            .columns = table_columns,
            .values = table_values,
            .count = sizeof(table_columns) / sizeof(table_columns[0]),
            .context = "reopened explicit defaults for timestamp table rows",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_explicit_defaults_for_timestamp_qualifiers_and_errors(void) {
    static const char *const scoped_columns[] = {
        "@@EXPLICIT_DEFAULTS_FOR_TIMESTAMP",
        "@@SESSION.EXPLICIT_DEFAULTS_FOR_TIMESTAMP",
        "@@Local.Explicit_Defaults_For_Timestamp",
        "@@global.`explicit_defaults_for_timestamp`",
        "(@@explicit_defaults_for_timestamp)",
    };
    static const char *const scoped_values[] = {"1", "1", "1", "1", "1"};
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += expect_int(
        mylite_open_memory(&database),
        MYLITE_OK,
        "open explicit defaults for timestamp memory"
    );
    failures += execute_ok(
        database,
        "SELECT @@EXPLICIT_DEFAULTS_FOR_TIMESTAMP, @@SESSION.EXPLICIT_DEFAULTS_FOR_TIMESTAMP, "
        "@@Local.Explicit_Defaults_For_Timestamp, @@global.`explicit_defaults_for_timestamp`, "
        "(@@explicit_defaults_for_timestamp)",
        &result
    );
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = scoped_columns,
            .values = scoped_values,
            .count = explicit_defaults_for_timestamp_label_column_count,
            .context = "explicit defaults for timestamp qualifiers",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_error(
        database,
        "SELECT @@no_such_explicit_defaults_for_timestamp_variable",
        (struct expected_sql_error){
            .code = mysql_error_unknown_system_variable,
            .sqlstate = "HY000",
            .message_part =
                "Unknown system variable 'no_such_explicit_defaults_for_timestamp_variable'",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@global.no_such_explicit_defaults_for_timestamp_variable",
        (struct expected_sql_error){
            .code = mysql_error_unknown_system_variable,
            .sqlstate = "HY000",
            .message_part =
                "Unknown system variable 'no_such_explicit_defaults_for_timestamp_variable'",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@session.no_such_explicit_defaults_for_timestamp_variable",
        (struct expected_sql_error){
            .code = mysql_error_unknown_system_variable,
            .sqlstate = "HY000",
            .message_part =
                "Unknown system variable 'no_such_explicit_defaults_for_timestamp_variable'",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@`session`.explicit_defaults_for_timestamp",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "quoted system variable scope",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@explicit_defaults_for_timestamp + 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "signed 64-bit +, binary -, and * arithmetic",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_independent_explicit_defaults_for_timestamp_handles(void) {
    static const char *const columns[] = {
        "@@explicit_defaults_for_timestamp",
        "@@warning_count",
        "@@error_count",
    };
    static const char *const first_values[] = {"1", "1", "0"};
    static const char *const second_values[] = {"1", "0", "0"};
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += expect_int(
        mylite_open_memory(&first),
        MYLITE_OK,
        "open first explicit defaults for timestamp handle"
    );
    failures += expect_int(
        mylite_open_memory(&second),
        MYLITE_OK,
        "open second explicit defaults for timestamp handle"
    );
    failures += execute_statement_ok(first, "SHOW PROCESSLIST");

    failures += execute_ok(
        first,
        "SELECT @@explicit_defaults_for_timestamp, @@warning_count, @@error_count",
        &result
    );
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = columns,
            .values = first_values,
            .count = explicit_defaults_for_timestamp_independent_column_count,
            .context = "first handle explicit defaults for timestamp variables",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(
        second,
        "SELECT @@explicit_defaults_for_timestamp, @@warning_count, @@error_count",
        &result
    );
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = columns,
            .values = second_values,
            .count = explicit_defaults_for_timestamp_independent_column_count,
            .context = "second handle explicit defaults for timestamp variables",
        }
    );
    mylite_result_free(result);

    mylite_close(second);
    mylite_close(first);
    return failures;
}

static int expect_result(const mylite_result *result, struct expected_result expected) {
    int failures = 0;

    if (result == NULL) {
        fprintf(stderr, "%s: expected result, got NULL\n", expected.context);
        return 1;
    }

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
        "%s/mylite_explicit_defaults_for_timestamp_system_variable_%d_%s.mylite",
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
    char related_path[test_path_capacity + path_suffix_capacity];
    int written = snprintf(related_path, sizeof(related_path), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related_path)) {
        return;
    }

    (void)remove(related_path);
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        return -1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fclose(file);
        return -1;
    }
    if (fread(buffer, 1U, size, file) != size) {
        fclose(file);
        return -1;
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
