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
    scalar_column_count = 9,
    quoted_column_count = 4,
    build_value_column_count = 4,
    independent_value_column_count = 3,
    show_variable_column_count = 2,
    show_variable_row_count = 4,
    show_version_compile_row_count = 3,
    diagnostics_column_count = 3,
    scoped_system_variable_sql_capacity = 128,
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

static const char protocol_version_value[] = "10";
static const char protocol_version_hex_value[] = "A";
static const char version_compile_machine_value[] = "aarch64";
static const char version_compile_os_value[] = "Linux";
static const char version_compile_zlib_value[] = "1.3.2";

static int test_server_build_values_and_show_rows(void);
static int test_server_build_diagnostics(void);
static int test_server_build_file_safety_and_independent_handles(void);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_statement_ok(mylite_db *database, const char *sql);
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

    failures += test_server_build_values_and_show_rows();
    failures += test_server_build_diagnostics();
    failures += test_server_build_file_safety_and_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_server_build_values_and_show_rows(void) {
    static const char *const scalar_values[] = {
        protocol_version_value,
        protocol_version_value,
        protocol_version_hex_value,
        version_compile_machine_value,
        version_compile_os_value,
        version_compile_zlib_value,
        "0",
        "0",
        "-1",
    };
    static const char *const quoted_values[] = {
        protocol_version_value,
        version_compile_machine_value,
        version_compile_os_value,
        version_compile_zlib_value,
    };
    static const char *const show_rows[] = {
        "protocol_version",
        protocol_version_value,
        "version_compile_machine",
        version_compile_machine_value,
        "version_compile_os",
        version_compile_os_value,
        "version_compile_zlib",
        version_compile_zlib_value,
    };
    static const char *const show_version_compile_rows[] = {
        "version_compile_machine",
        version_compile_machine_value,
        "version_compile_os",
        version_compile_os_value,
        "version_compile_zlib",
        version_compile_zlib_value,
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += mylite_test_expect_int(
        mylite_open_memory(&database),
        MYLITE_OK,
        "open server build memory"
    );

    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@protocol_version, @@GLOBAL.protocol_version, "
                   "HEX(@@protocol_version), @@version_compile_machine, "
                   "@@version_compile_os, @@version_compile_zlib, "
                   "@@warning_count, @@error_count, ROW_COUNT()",
            .values = scalar_values,
            .column_count = scalar_column_count,
            .row_count = 1U,
            .context = "server build scalar values",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@PROTOCOL_VERSION, @@global.`version_compile_machine`, "
                   "@@`VERSION_COMPILE_OS`, @@GLOBAL.VERSION_COMPILE_ZLIB",
            .values = quoted_values,
            .column_count = quoted_column_count,
            .row_count = 1U,
            .context = "server build case and quoted labels",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW VARIABLES WHERE Variable_name IN "
                   "('protocol_version','version_compile_machine',"
                   "'version_compile_os','version_compile_zlib')",
            .values = show_rows,
            .column_count = show_variable_column_count,
            .row_count = show_variable_row_count,
            .context = "server build show variables rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW GLOBAL VARIABLES WHERE Variable_name IN "
                   "('protocol_version','version_compile_machine',"
                   "'version_compile_os','version_compile_zlib')",
            .values = show_rows,
            .column_count = show_variable_column_count,
            .row_count = show_variable_row_count,
            .context = "server build show global variables rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW SESSION VARIABLES WHERE Variable_name IN "
                   "('protocol_version','version_compile_machine',"
                   "'version_compile_os','version_compile_zlib')",
            .values = show_rows,
            .column_count = show_variable_column_count,
            .row_count = show_variable_row_count,
            .context = "server build show session variables rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW VARIABLES LIKE 'version_compile_%'",
            .values = show_version_compile_rows,
            .column_count = show_variable_column_count,
            .row_count = show_version_compile_row_count,
            .context = "server build show version_compile-like rows",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_server_build_diagnostics(void) {
    static const char *const diagnostic_values[] = {"1", "1", "-1"};
    static const char *const variables[] = {
        "protocol_version",
        "version_compile_machine",
        "version_compile_os",
        "version_compile_zlib",
    };
    static const char *const readonly_sql[] = {
        "SET protocol_version = DEFAULT",
        "SET GLOBAL version_compile_machine = DEFAULT",
        "SET SESSION version_compile_os = DEFAULT",
        "SET @@SESSION.version_compile_zlib = '1.3.2'",
        "SET @@GLOBAL.protocol_version = 10",
        "SET version_compile_machine = 'aarch64'",
        "SET @@version_compile_os = 'Linux'",
        "SET @@GLOBAL.version_compile_zlib = '1.3.2'",
    };
    const struct expected_sql_error global_only_read = {
        .code = mysql_error_session_variable_only,
        .sqlstate = "HY000",
        .message_part = "is a GLOBAL variable",
    };
    const struct expected_sql_error read_only = {
        .code = mysql_error_session_variable_only,
        .sqlstate = "HY000",
        .message_part = "is a read only variable",
    };
    mylite_db *database = NULL;
    char sql[scoped_system_variable_sql_capacity];
    int failures = 0;

    failures +=
        mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open diagnostics memory");

    for (size_t index = 0U; index < sizeof(variables) / sizeof(variables[0]); ++index) {
        int written = snprintf(sql, sizeof(sql), "SELECT @@SESSION.%s", variables[index]);

        if (written < 0 || (size_t)written >= sizeof(sql)) {
            failures += 1;
        } else {
            failures += execute_error(database, sql, global_only_read);
        }
        written = snprintf(sql, sizeof(sql), "SELECT @@LOCAL.%s", variables[index]);
        if (written < 0 || (size_t)written >= sizeof(sql)) {
            failures += 1;
        } else {
            failures += execute_error(database, sql, global_only_read);
        }
    }

    for (size_t index = 0U; index < sizeof(readonly_sql) / sizeof(readonly_sql[0]); ++index) {
        failures += execute_error(database, readonly_sql[index], read_only);
    }
    failures += execute_statement_ok(database, "SET @protocol_value = 10");
    failures += execute_error(database, "SET @@protocol_version = @protocol_value", read_only);

    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count, @@error_count, ROW_COUNT()",
            .values = diagnostic_values,
            .column_count = diagnostics_column_count,
            .row_count = 1U,
            .context = "server build diagnostics counts",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_server_build_file_safety_and_independent_handles(void) {
    static const char *const reopened_values[] = {
        protocol_version_value,
        version_compile_machine_value,
        version_compile_os_value,
        version_compile_zlib_value,
    };
    static const char *const independent_values[] = {
        protocol_version_value,
        version_compile_os_value,
        version_compile_zlib_value,
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    const struct mylite_session_state *session = NULL;
    uint64_t catalog_generation_before = 0U;
    uint64_t sqlite_generation_before = 0U;
    mylite_db *database = NULL;
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "file_safety") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open server build file");
    session = mylite_connection_session_state(database);
    if (session != NULL) {
        catalog_generation_before = session->catalog_generation;
        sqlite_generation_before = session->sqlite_schema_generation;
    }
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@protocol_version, @@version_compile_machine, "
                   "@@version_compile_os, @@version_compile_zlib",
            .values = reopened_values,
            .column_count = build_value_column_count,
            .row_count = 1U,
            .context = "server build file values",
        }
    );
    session = mylite_connection_session_state(database);
    if (session != NULL) {
        failures += mylite_test_expect_uint64(
            session->catalog_generation,
            catalog_generation_before,
            "server build read leaves catalog generation"
        );
        failures += mylite_test_expect_uint64(
            session->sqlite_schema_generation,
            sqlite_generation_before,
            "server build read leaves SQLite schema generation"
        );
    }
    mylite_close(database);
    database = NULL;

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "server build reads preserve preamble"
    );

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "reopen server build file");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@protocol_version, @@version_compile_machine, "
                   "@@version_compile_os, @@version_compile_zlib",
            .values = reopened_values,
            .column_count = build_value_column_count,
            .row_count = 1U,
            .context = "reopened server build values",
        }
    );
    mylite_close(database);
    remove_related_files(path);

    failures += mylite_test_expect_int(
        mylite_open_memory(&first),
        MYLITE_OK,
        "open first server build handle"
    );
    failures += mylite_test_expect_int(
        mylite_open_memory(&second),
        MYLITE_OK,
        "open second server build handle"
    );
    failures += execute_error(
        first,
        "SET @@GLOBAL.protocol_version = 10",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "is a read only variable",
        }
    );
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT @@protocol_version, @@version_compile_os, @@version_compile_zlib",
            .values = independent_values,
            .column_count = independent_value_column_count,
            .row_count = 1U,
            .context = "first server build handle values",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT @@protocol_version, @@version_compile_os, @@version_compile_zlib",
            .values = independent_values,
            .column_count = independent_value_column_count,
            .row_count = 1U,
            .context = "second server build handle values",
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

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (result == NULL) {
        return failures + 1;
    }
    failures += mylite_test_expect_size(mylite_result_column_count(result), 0U, sql);
    failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, sql);
    mylite_result_free(result);

    return failures;
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
