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
    show_events_column_count = 15,
    decimal_base = 10,
    row_count_text_capacity = 32,
    suffix_capacity = 16,
    mysql_error_parse = 1064,
    mysql_error_no_database_selected = 1046,
    mysql_error_incorrect_database_name = 1102,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_show_events_empty_result {
    const char *sql;
    const char *context;
};

static const char *const show_events_names[show_events_column_count] = {
    "Db",
    "Name",
    "Definer",
    "Time zone",
    "Type",
    "Execute at",
    "Interval value",
    "Interval field",
    "Starts",
    "Ends",
    "Status",
    "Originator",
    "character_set_client",
    "collation_connection",
    "Database Collation",
};

static int test_show_events_result_shape_schema_like_persistence_and_drop(void);
static int test_show_events_diagnostics_and_unsupported_forms(void);
static int test_independent_show_events_handles(void);
static int create_show_events_schemas(mylite_db *database);
static int expect_show_events_empty_result(
    mylite_db *database,
    struct expected_show_events_empty_result expectation
);
static int expect_row_count(mylite_db *database, int64_t expected, const char *context);
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

    failures += test_show_events_result_shape_schema_like_persistence_and_drop();
    failures += test_show_events_diagnostics_and_unsupported_forms();
    failures += test_independent_show_events_handles();

    return failures == 0 ? 0 : 1;
}

static int test_show_events_result_shape_schema_like_persistence_and_drop(void) {
    static const char *const forms[] = {
        "SHOW EVENTS",
        "SHOW EVENTS LIKE '%'",
        "SHOW EVENTS LIKE 'daily\\_%'",
        "SHOW EVENTS WHERE Name = 'daily'",
        "SHOW EVENTS FROM app",
        "SHOW EVENTS IN app",
        "SHOW EVENTS FROM app LIKE 'daily'",
        "SHOW EVENTS FROM app WHERE Name = 'daily'",
        "SHOW EVENTS FROM other LIKE 'missing%'",
        "SHOW EVENTS FROM missing_schema",
        "SHOW EVENTS IN missing_schema",
        "SHOW EVENTS FROM missing_schema LIKE 'missing%'",
        "SHOW EVENTS FROM missing_schema WHERE Name = 'missing'",
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

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open values file");
    failures += create_show_events_schemas(database);
    failures += expect_show_events_empty_result(
        database,
        (struct expected_show_events_empty_result){
            .sql = "SHOW EVENTS FROM app",
            .context = "qualified show events without default schema",
        }
    );
    failures += expect_show_events_empty_result(
        database,
        (struct expected_show_events_empty_result){
            .sql = "SHOW EVENTS FROM missing_schema",
            .context = "unknown explicit show events without default schema",
        }
    );
    failures += execute_statement_ok(database, "USE app");

    session = mylite_connection_session_state(database);
    catalog_generation = session->catalog_generation;
    sqlite_schema_generation = session->sqlite_schema_generation;
    for (size_t form_index = 0U; form_index < sizeof(forms) / sizeof(forms[0]); ++form_index) {
        failures += expect_show_events_empty_result(
            database,
            (struct expected_show_events_empty_result){
                .sql = forms[form_index],
                .context = forms[form_index],
            }
        );
    }
    session = mylite_connection_session_state(database);
    failures += expect_int64(
        (int64_t)session->catalog_generation,
        (int64_t)catalog_generation,
        "catalog generation after show events"
    );
    failures += expect_int64(
        (int64_t)session->sqlite_schema_generation,
        (int64_t)sqlite_schema_generation,
        "sqlite schema generation after show events"
    );
    failures += expect_row_count(database, -1, "row count after show events");
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after show events"
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen values file");
    failures += execute_statement_ok(database, "USE app");
    failures += expect_show_events_empty_result(
        database,
        (struct expected_show_events_empty_result){
            .sql = "SHOW EVENTS",
            .context = "reopened show events",
        }
    );

    failures += execute_statement_ok(database, "DROP DATABASE other");
    failures += expect_show_events_empty_result(
        database,
        (struct expected_show_events_empty_result){
            .sql = "SHOW EVENTS FROM other",
            .context = "dropped explicit schema remains empty success",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_show_events_diagnostics_and_unsupported_forms(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics file");
    failures += create_show_events_schemas(database);

    failures += execute_error(
        database,
        "SHOW EVENTS",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_error(
        database,
        "SHOW EVENTS LIKE '%'",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_error(
        database,
        "SHOW EVENTS WHERE Name = 'daily'",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_error(
        database,
        "SHOW EVENTS FROM _mylite_catalog",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_database_name,
            .sqlstate = "42000",
            .message_part = "Incorrect database name '_mylite_catalog'",
        }
    );

    failures += execute_statement_ok(database, "USE app");
    failures += execute_error(
        database,
        "SHOW FULL EVENTS FROM app",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW EXTENDED EVENTS FROM app",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW EVENT FROM app",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW EVENTS FROM app LIKE 'daily%' WHERE Name = 'daily_event'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW EVENTS FROM app ORDER BY Name",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW EVENTS FROM app LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW EVENTS FROM app.extra",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW EVENTS FROM app LIKE 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW EVENTS FROM app LIKE NULL",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW EVENTS FROM app LIKE N'daily'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW EVENTS FROM app LIKE _utf8mb4'daily'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW EVENTS FROM app LIKE ?",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW EVENTS FROM app LIKE CONCAT('daily', '%')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW EVENTS FROM app LIKE 'daily\\0%'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SHOW LIKE does not support NUL bytes in patterns",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_independent_show_events_handles(void) {
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

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first database");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second database");
    failures += execute_statement_ok(first, "CREATE DATABASE app");
    failures += execute_statement_ok(first, "USE app");
    failures += execute_statement_ok(second, "CREATE DATABASE other");
    failures += execute_statement_ok(second, "USE other");

    failures += expect_show_events_empty_result(
        first,
        (struct expected_show_events_empty_result){
            .sql = "SHOW EVENTS",
            .context = "first selected schema",
        }
    );
    failures += expect_show_events_empty_result(
        second,
        (struct expected_show_events_empty_result){
            .sql = "SHOW EVENTS",
            .context = "second selected schema",
        }
    );
    failures += expect_show_events_empty_result(
        first,
        (struct expected_show_events_empty_result){
            .sql = "SHOW EVENTS FROM other",
            .context = "first unknown other empty",
        }
    );
    failures += expect_show_events_empty_result(
        second,
        (struct expected_show_events_empty_result){
            .sql = "SHOW EVENTS FROM app",
            .context = "second unknown app empty",
        }
    );

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);
    return failures;
}

static int create_show_events_schemas(mylite_db *database) {
    int failures = 0;

    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "CREATE DATABASE other");
    return failures;
}

static int expect_show_events_empty_result(
    mylite_db *database,
    struct expected_show_events_empty_result expectation
) {
    mylite_result *result = NULL;
    int failures = 0;

    failures += execute_ok(database, expectation.sql, &result);
    if (result == NULL) {
        return failures + 1;
    }

    failures += expect_size(
        mylite_result_column_count(result),
        show_events_column_count,
        "show events column count"
    );
    for (size_t column_index = 0U; column_index < show_events_column_count; ++column_index) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, column_index),
            show_events_names[column_index],
            expectation.context
        );
    }
    failures += expect_size(mylite_result_row_count(result), 0U, expectation.context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, expectation.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, expectation.context);

    mylite_result_free(result);
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
        "/tmp/mylite_runtime_show_events_empty_%s_%d.mylite",
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
    char buffer[test_path_capacity + suffix_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(buffer)) {
        return;
    }
    remove(buffer);
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t read_count = 0U;

    if (file == NULL) {
        fprintf(stderr, "failed to open %s for reading\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "failed to seek %s\n", path);
        fclose(file);
        return 1;
    }
    read_count = fread(buffer, 1U, size, file);
    if (read_count != size) {
        fprintf(stderr, "failed to read %zu bytes from %s\n", size, path);
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
    if (actual != NULL && expected != NULL && strcmp(actual, expected) == 0) {
        return 0;
    }

    fprintf(
        stderr,
        "%s: expected text %s, got %s\n",
        context,
        expected == NULL ? "NULL" : expected,
        actual == NULL ? "NULL" : actual
    );
    return 1;
}

static int expect_text_contains(const char *actual, const char *needle, const char *context) {
    if (actual != NULL && needle != NULL && strstr(actual, needle) != NULL) {
        return 0;
    }

    fprintf(
        stderr,
        "%s: expected text containing %s, got %s\n",
        context,
        needle == NULL ? "NULL" : needle,
        actual == NULL ? "NULL" : actual
    );
    return 1;
}

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: byte buffers differ\n", context);
        return 1;
    }
    return 0;
}
