#include "mylite_test_support.h"

#include <mylite/mylite.h>

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

enum {
    test_path_capacity = 1024,
    scalar_column_count = 8,
    show_variable_column_count = 2,
    show_variable_row_count = 3,
    diagnostics_column_count = 3,
    mysql_error_parse = 1064,
    mysql_error_global_variable_only = 1229,
    mysql_error_session_variable_only = 1238,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_query {
    const char *sql;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    const char *context;
};

static int test_read_only_variable_values_and_noop_sets(void);
static int test_read_only_variable_diagnostics(void);
static int test_read_only_variable_independent_handles(void);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_set_ok(mylite_db *database, const char *sql);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_read_only_variable_values_and_noop_sets();
    failures += test_read_only_variable_diagnostics();
    failures += test_read_only_variable_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_read_only_variable_values_and_noop_sets(void) {
    static const char *const scalar_values[] = {
        "0",
        "0",
        "0",
        "0",
        "0",
        "0",
        "0",
        "-1",
    };
    static const char *const scalar_after_set_values[] = {
        "0",
        "0",
        "0",
        "0",
        "0",
        "0",
        "0",
        "0",
    };
    static const char *const reopened_values[] = {"0", "0", "0", "0"};
    static const char *const show_rows[] = {
        "innodb_read_only",
        "OFF",
        "read_only",
        "OFF",
        "super_read_only",
        "OFF",
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    const struct mylite_session_state *session = NULL;
    uint64_t catalog_generation_before_set = 0U;
    uint64_t sqlite_generation_before_set = 0U;
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "values") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "open read-only variable file"
    );
    session = mylite_connection_session_state(database);
    if (session != NULL) {
        catalog_generation_before_set = session->catalog_generation;
        sqlite_generation_before_set = session->sqlite_schema_generation;
    }

    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@read_only, @@GLOBAL.read_only, "
                   "@@super_read_only, @@GLOBAL.super_read_only, "
                   "@@innodb_read_only, @@GLOBAL.innodb_read_only, "
                   "@@warning_count, ROW_COUNT()",
            .values = scalar_values,
            .column_count = scalar_column_count,
            .row_count = 1U,
            .context = "read-only scalar values",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW VARIABLES WHERE Variable_name IN "
                   "('read_only','super_read_only','innodb_read_only')",
            .values = show_rows,
            .column_count = show_variable_column_count,
            .row_count = show_variable_row_count,
            .context = "read-only show variables rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW GLOBAL VARIABLES WHERE Variable_name IN "
                   "('read_only','super_read_only','innodb_read_only')",
            .values = show_rows,
            .column_count = show_variable_column_count,
            .row_count = show_variable_row_count,
            .context = "read-only show global variables rows",
        }
    );

    failures += expect_set_ok(database, "SET GLOBAL read_only = OFF");
    failures += expect_set_ok(database, "SET @@GLOBAL.read_only = 0");
    failures += expect_set_ok(database, "SET GLOBAL read_only = FALSE");
    failures += expect_set_ok(database, "SET GLOBAL read_only = DEFAULT");
    failures += expect_set_ok(database, "SET GLOBAL super_read_only = OFF");
    failures += expect_set_ok(database, "SET @@GLOBAL.super_read_only = 0");
    failures += expect_set_ok(database, "SET GLOBAL super_read_only = FALSE");
    failures += expect_set_ok(database, "SET GLOBAL super_read_only = DEFAULT");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@read_only, @@GLOBAL.read_only, "
                   "@@super_read_only, @@GLOBAL.super_read_only, "
                   "@@innodb_read_only, @@GLOBAL.innodb_read_only, "
                   "@@warning_count, ROW_COUNT()",
            .values = scalar_after_set_values,
            .column_count = scalar_column_count,
            .row_count = 1U,
            .context = "read-only no-op SET keeps values",
        }
    );

    session = mylite_connection_session_state(database);
    if (session != NULL) {
        failures += mylite_test_expect_uint64(
            session->catalog_generation,
            catalog_generation_before_set,
            "read-only no-op SET leaves catalog generation"
        );
        failures += mylite_test_expect_uint64(
            session->sqlite_schema_generation,
            sqlite_generation_before_set,
            "read-only no-op SET leaves SQLite schema generation"
        );
    }

    mylite_close(database);
    database = NULL;

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "read-only no-op SET preserves preamble"
    );

    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "reopen read-only variable file"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@read_only, @@super_read_only, @@innodb_read_only, @@warning_count",
            .values = reopened_values,
            .column_count = 4U,
            .row_count = 1U,
            .context = "reopened read-only values",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_read_only_variable_diagnostics(void) {
    static const char *const diagnostic_values[] = {"1", "1", "-1"};
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open diagnostics memory");

    failures += execute_error(
        database,
        "SELECT @@SESSION.read_only",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "Variable 'read_only' is a GLOBAL variable",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@LOCAL.super_read_only",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "Variable 'super_read_only' is a GLOBAL variable",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@SESSION.innodb_read_only",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "Variable 'innodb_read_only' is a GLOBAL variable",
        }
    );
    failures += execute_error(
        database,
        "SET read_only = 0",
        (struct expected_sql_error){
            .code = mysql_error_global_variable_only,
            .sqlstate = "HY000",
            .message_part =
                "Variable 'read_only' is a GLOBAL variable and should be set with SET GLOBAL",
        }
    );
    failures += execute_error(
        database,
        "SET SESSION read_only = 0",
        (struct expected_sql_error){
            .code = mysql_error_global_variable_only,
            .sqlstate = "HY000",
            .message_part =
                "Variable 'read_only' is a GLOBAL variable and should be set with SET GLOBAL",
        }
    );
    failures += execute_error(
        database,
        "SET @@read_only = 0",
        (struct expected_sql_error){
            .code = mysql_error_global_variable_only,
            .sqlstate = "HY000",
            .message_part =
                "Variable 'read_only' is a GLOBAL variable and should be set with SET GLOBAL",
        }
    );
    failures += execute_error(
        database,
        "SET @@LOCAL.read_only = 0",
        (struct expected_sql_error){
            .code = mysql_error_global_variable_only,
            .sqlstate = "HY000",
            .message_part =
                "Variable 'read_only' is a GLOBAL variable and should be set with SET GLOBAL",
        }
    );
    failures += execute_error(
        database,
        "SET super_read_only = 0",
        (struct expected_sql_error){
            .code = mysql_error_global_variable_only,
            .sqlstate = "HY000",
            .message_part =
                "Variable 'super_read_only' is a GLOBAL variable and should be set with SET GLOBAL",
        }
    );
    failures += execute_error(
        database,
        "SET LOCAL super_read_only = 0",
        (struct expected_sql_error){
            .code = mysql_error_global_variable_only,
            .sqlstate = "HY000",
            .message_part =
                "Variable 'super_read_only' is a GLOBAL variable and should be set with SET GLOBAL",
        }
    );
    failures += execute_error(
        database,
        "SET @@SESSION.super_read_only = 0",
        (struct expected_sql_error){
            .code = mysql_error_global_variable_only,
            .sqlstate = "HY000",
            .message_part =
                "Variable 'super_read_only' is a GLOBAL variable and should be set with SET GLOBAL",
        }
    );
    failures += execute_error(
        database,
        "SET GLOBAL read_only = ON",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SET supports only fixed no-op system variable assignments",
        }
    );
    failures += execute_error(
        database,
        "SET GLOBAL read_only = TRUE",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SET supports only fixed no-op system variable assignments",
        }
    );
    failures += execute_error(
        database,
        "SET @@GLOBAL.super_read_only = 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SET supports only fixed no-op system variable assignments",
        }
    );
    failures += execute_error(
        database,
        "SET innodb_read_only = 0",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "Variable 'innodb_read_only' is a read only variable",
        }
    );
    failures += execute_error(
        database,
        "SET GLOBAL innodb_read_only = 0",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "Variable 'innodb_read_only' is a read only variable",
        }
    );
    failures += execute_error(
        database,
        "SET @@GLOBAL.innodb_read_only = 0",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "Variable 'innodb_read_only' is a read only variable",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count, @@error_count, ROW_COUNT()",
            .values = diagnostic_values,
            .column_count = diagnostics_column_count,
            .row_count = 1U,
            .context = "read-only diagnostics counts",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_read_only_variable_independent_handles(void) {
    static const char *const values[] = {"0", "0", "0", "0"};
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    failures += mylite_test_expect_int(mylite_open_memory(&first), MYLITE_OK, "open first handle");
    failures +=
        mylite_test_expect_int(mylite_open_memory(&second), MYLITE_OK, "open second handle");
    failures += expect_set_ok(first, "SET GLOBAL read_only = OFF");
    failures += expect_set_ok(second, "SET GLOBAL super_read_only = DEFAULT");
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT @@read_only, @@super_read_only, @@innodb_read_only, @@warning_count",
            .values = values,
            .column_count = 4U,
            .row_count = 1U,
            .context = "first read-only handle values",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT @@read_only, @@super_read_only, @@innodb_read_only, @@warning_count",
            .values = values,
            .column_count = 4U,
            .row_count = 1U,
            .context = "second read-only handle values",
        }
    );

    mylite_close(second);
    mylite_close(first);
    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    if (result == NULL) {
        return failures + 1;
    }
    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        query.column_count,
        query.context
    );
    failures +=
        mylite_test_expect_size(mylite_result_row_count(result), query.row_count, query.context);
    failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, query.context);
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, query.context);
    for (size_t row = 0U; row < query.row_count; ++row) {
        for (size_t column = 0U; column < query.column_count; ++column) {
            size_t value_index = (row * query.column_count) + column;

            failures += mylite_test_expect_text(
                mylite_result_value_text(result, row, column),
                query.values[value_index],
                query.context
            );
        }
    }
    mylite_result_free(result);

    return failures;
}

static int expect_set_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (result == NULL) {
        return failures + 1;
    }
    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        0U,
        "read-only SET column count"
    );
    failures +=
        mylite_test_expect_size(mylite_result_row_count(result), 0U, "read-only SET row count");
    failures += mylite_test_expect_int64(
        mylite_result_affected_rows(result),
        0,
        "read-only SET affected rows"
    );
    failures += mylite_test_expect_size(
        mylite_result_warning_count(result),
        0U,
        "read-only SET warning count"
    );
    mylite_result_free(result);

    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "execute '%s': expected OK, got %d / %d %s %s\n",
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
        fprintf(stderr, "execute '%s': expected MYLITE_ERROR, got %d\n", sql, rc);
        failures += 1;
    }
    failures += mylite_test_expect_true(result == NULL, "failed execute leaves result null");
    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, "error code");
    failures += mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, "SQLSTATE");
    failures += mylite_test_expect_contains(
        mylite_errmsg(database),
        expected.message_part,
        "error message"
    );
    mylite_result_free(result);

    return failures;
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

    if (written < 0 || (size_t)written >= sizeof(related_path)) {
        return;
    }

    (void)remove(related_path);
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        fprintf(stderr, "failed to open %s for reading\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "failed to seek %s\n", path);
        (void)fclose(file);
        return 1;
    }
    if (fread(buffer, 1U, size, file) != size) {
        fprintf(stderr, "failed to read %zu bytes from %s\n", size, path);
        (void)fclose(file);
        return 1;
    }
    if (fclose(file) != 0) {
        fprintf(stderr, "failed to close %s\n", path);
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
        fprintf(stderr, "%s: byte comparison failed\n", context);
        return 1;
    }

    return 0;
}
