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
    scalar_column_count = 18,
    label_column_count = 7,
    persisted_scalar_column_count = 8,
    independent_handle_column_count = 6,
    show_warnings_column_count = 3,
    show_variable_column_count = 2,
    show_variable_row_count = 7,
    show_log_bin_like_row_count = 4,
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
    size_t warning_count;
    const char *context;
};

static const char server_uuid_value[] = "4d796c69-7465-4000-8000-000000000001";
static const char log_bin_trust_deprecation_warning[] =
    "'@@log_bin_trust_function_creators' is deprecated and will be removed in a future release.";

static int test_server_identity_binary_log_values_and_noop_sets(void);
static int test_server_identity_binary_log_diagnostics(void);
static int test_server_identity_binary_log_independent_handles(void);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_log_bin_trust_deprecation_warnings(
    mylite_db *database,
    size_t row_count,
    const char *context
);
static int expect_set_ok(mylite_db *database, const char *sql);
static int expect_set_warning(mylite_db *database, const char *sql);
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
static int expect_true(int condition, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_server_identity_binary_log_values_and_noop_sets();
    failures += test_server_identity_binary_log_diagnostics();
    failures += test_server_identity_binary_log_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_server_identity_binary_log_values_and_noop_sets(void) {
    static const char *const scalar_values[] = {
        "1",
        "1",
        "32",
        "32",
        server_uuid_value,
        server_uuid_value,
        "1",
        "1",
        "binlog",
        "binlog",
        "binlog.index",
        "binlog.index",
        "0",
        "0",
        "2",
        "0",
        "-1",
        "20",
    };
    static const char *const label_values[] = {
        "1",
        "32",
        server_uuid_value,
        "1",
        "binlog",
        "binlog.index",
        "0",
    };
    static const char *const show_rows[] = {
        "log_bin",
        "ON",
        "log_bin_basename",
        "binlog",
        "log_bin_index",
        "binlog.index",
        "log_bin_trust_function_creators",
        "OFF",
        "server_id",
        "1",
        "server_id_bits",
        "32",
        "server_uuid",
        server_uuid_value,
    };
    static const char *const show_log_bin_like_rows[] = {
        "log_bin",
        "ON",
        "log_bin_basename",
        "binlog",
        "log_bin_index",
        "binlog.index",
        "log_bin_trust_function_creators",
        "OFF",
    };
    static const char *const reopened_values[] = {
        "1",
        "32",
        server_uuid_value,
        "1",
        "binlog",
        "binlog.index",
        "0",
        "1",
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    const struct mylite_session_state *session = NULL;
    uint64_t catalog_generation_before_set = 0U;
    uint64_t sqlite_generation_before_set = 0U;
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "values") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open server identity file");
    session = mylite_connection_session_state(database);
    if (session != NULL) {
        catalog_generation_before_set = session->catalog_generation;
        sqlite_generation_before_set = session->sqlite_schema_generation;
    }

    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@server_id, @@GLOBAL.server_id, "
                   "@@server_id_bits, @@GLOBAL.server_id_bits, "
                   "@@server_uuid, @@GLOBAL.server_uuid, "
                   "@@log_bin, @@GLOBAL.log_bin, "
                   "@@log_bin_basename, @@GLOBAL.log_bin_basename, "
                   "@@log_bin_index, @@GLOBAL.log_bin_index, "
                   "@@log_bin_trust_function_creators, "
                   "@@GLOBAL.log_bin_trust_function_creators, "
                   "@@warning_count, @@error_count, ROW_COUNT(), "
                   "HEX(@@server_id_bits)",
            .values = scalar_values,
            .column_count = scalar_column_count,
            .row_count = 1U,
            .warning_count = 2U,
            .context = "server identity scalar values",
        }
    );
    failures += expect_log_bin_trust_deprecation_warnings(
        database,
        2U,
        "server identity scalar deprecation warnings"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@SERVER_ID, @@global.`server_id_bits`, @@`server_uuid`, "
                   "@@GLOBAL.LOG_BIN, @@`log_bin_basename`, @@Global.Log_Bin_Index, "
                   "@@LOG_BIN_TRUST_FUNCTION_CREATORS",
            .values = label_values,
            .column_count = label_column_count,
            .row_count = 1U,
            .warning_count = 1U,
            .context = "server identity label values",
        }
    );
    failures += expect_log_bin_trust_deprecation_warnings(
        database,
        1U,
        "server identity label deprecation warning"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW VARIABLES WHERE Variable_name IN "
                   "('server_id','server_id_bits','server_uuid','log_bin',"
                   "'log_bin_basename','log_bin_index',"
                   "'log_bin_trust_function_creators')",
            .values = show_rows,
            .column_count = show_variable_column_count,
            .row_count = show_variable_row_count,
            .context = "server identity show variables rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW GLOBAL VARIABLES WHERE Variable_name IN "
                   "('server_id','server_id_bits','server_uuid','log_bin',"
                   "'log_bin_basename','log_bin_index',"
                   "'log_bin_trust_function_creators')",
            .values = show_rows,
            .column_count = show_variable_column_count,
            .row_count = show_variable_row_count,
            .context = "server identity show global variables rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW SESSION VARIABLES WHERE Variable_name IN "
                   "('server_id','server_id_bits','server_uuid','log_bin',"
                   "'log_bin_basename','log_bin_index',"
                   "'log_bin_trust_function_creators')",
            .values = show_rows,
            .column_count = show_variable_column_count,
            .row_count = show_variable_row_count,
            .context = "server identity show session variables rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW VARIABLES LIKE 'log\\_bin%'",
            .values = show_log_bin_like_rows,
            .column_count = show_variable_column_count,
            .row_count = show_log_bin_like_row_count,
            .context = "server identity show log_bin like rows",
        }
    );

    failures += expect_set_ok(database, "SET GLOBAL server_id = 1");
    failures += expect_set_ok(database, "SET @@GLOBAL.server_id = DEFAULT");
    failures += expect_set_ok(database, "SET GLOBAL server_id_bits = 32");
    failures += expect_set_ok(database, "SET @@GLOBAL.server_id_bits = DEFAULT");
    failures += expect_set_warning(database, "SET GLOBAL log_bin_trust_function_creators = OFF");
    failures += expect_set_warning(database, "SET GLOBAL log_bin_trust_function_creators = 0");
    failures += expect_set_warning(database, "SET GLOBAL log_bin_trust_function_creators = FALSE");
    failures +=
        expect_set_warning(database, "SET @@GLOBAL.log_bin_trust_function_creators = DEFAULT");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@server_id, @@server_id_bits, @@server_uuid, @@log_bin, "
                   "@@log_bin_basename, @@log_bin_index, "
                   "@@log_bin_trust_function_creators, @@warning_count",
            .values = reopened_values,
            .column_count = persisted_scalar_column_count,
            .row_count = 1U,
            .warning_count = 1U,
            .context = "server identity no-op SET keeps values",
        }
    );

    session = mylite_connection_session_state(database);
    if (session != NULL) {
        failures += expect_uint64(
            session->catalog_generation,
            catalog_generation_before_set,
            "server identity no-op SET leaves catalog generation"
        );
        failures += expect_uint64(
            session->sqlite_schema_generation,
            sqlite_generation_before_set,
            "server identity no-op SET leaves SQLite schema generation"
        );
    }

    mylite_close(database);
    database = NULL;

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "server identity no-op SET preserves preamble"
    );

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen server identity file");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@server_id, @@server_id_bits, @@server_uuid, @@log_bin, "
                   "@@log_bin_basename, @@log_bin_index, "
                   "@@log_bin_trust_function_creators, @@warning_count",
            .values = reopened_values,
            .column_count = persisted_scalar_column_count,
            .row_count = 1U,
            .warning_count = 1U,
            .context = "reopened server identity values",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_server_identity_binary_log_diagnostics(void) {
    static const char *const diagnostic_values[] = {"1", "1", "-1"};
    const struct expected_sql_error global_only_read = {
        .code = mysql_error_session_variable_only,
        .sqlstate = "HY000",
        .message_part = "is a GLOBAL variable",
    };
    const struct expected_sql_error set_global_required = {
        .code = mysql_error_global_variable_only,
        .sqlstate = "HY000",
        .message_part = "is a GLOBAL variable and should be set with SET GLOBAL",
    };
    const struct expected_sql_error read_only = {
        .code = mysql_error_session_variable_only,
        .sqlstate = "HY000",
        .message_part = "is a read only variable",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open diagnostics memory");

    failures += execute_error(database, "SELECT @@SESSION.server_id", global_only_read);
    failures += execute_error(database, "SELECT @@LOCAL.server_id_bits", global_only_read);
    failures += execute_error(database, "SELECT @@SESSION.server_uuid", global_only_read);
    failures += execute_error(database, "SELECT @@LOCAL.log_bin", global_only_read);
    failures += execute_error(database, "SELECT @@SESSION.log_bin_basename", global_only_read);
    failures += execute_error(database, "SELECT @@LOCAL.log_bin_index", global_only_read);
    failures += execute_error(
        database,
        "SELECT @@SESSION.log_bin_trust_function_creators",
        global_only_read
    );

    failures += execute_error(database, "SET server_id = 1", set_global_required);
    failures += execute_error(database, "SET SESSION server_id = 1", set_global_required);
    failures += execute_error(database, "SET @@server_id = 1", set_global_required);
    failures += execute_error(database, "SET LOCAL server_id_bits = 32", set_global_required);
    failures += execute_error(database, "SET @@SESSION.server_id_bits = 32", set_global_required);
    failures +=
        execute_error(database, "SET log_bin_trust_function_creators = OFF", set_global_required);
    failures += execute_error(
        database,
        "SET @@LOCAL.log_bin_trust_function_creators = OFF",
        set_global_required
    );

    failures += execute_error(
        database,
        "SET GLOBAL server_id = 2",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SET server_id supports only fixed no-op global assignments",
        }
    );
    failures += execute_error(
        database,
        "SET GLOBAL server_id_bits = 31",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SET server_id_bits supports only fixed no-op global assignments",
        }
    );
    failures += execute_error(
        database,
        "SET GLOBAL log_bin_trust_function_creators = ON",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SET supports only fixed no-op system variable assignments",
        }
    );
    failures += execute_error(database, "SET server_uuid = 'x'", read_only);
    failures += execute_error(database, "SET GLOBAL server_uuid = 'x'", read_only);
    failures += execute_error(database, "SET log_bin = ON", read_only);
    failures += execute_error(database, "SET GLOBAL log_bin_basename = 'binlog'", read_only);
    failures += execute_error(database, "SET @@GLOBAL.log_bin_index = 'binlog.index'", read_only);

    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count, @@error_count, ROW_COUNT()",
            .values = diagnostic_values,
            .column_count = diagnostics_column_count,
            .row_count = 1U,
            .context = "server identity diagnostics counts",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_server_identity_binary_log_independent_handles(void) {
    static const char *const values[] = {
        "1",
        "32",
        server_uuid_value,
        "1",
        "0",
        "1",
    };
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&first), MYLITE_OK, "open first handle");
    failures += expect_int(mylite_open_memory(&second), MYLITE_OK, "open second handle");
    failures += expect_set_ok(first, "SET GLOBAL server_id = DEFAULT");
    failures += expect_set_warning(second, "SET GLOBAL log_bin_trust_function_creators = OFF");
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT @@server_id, @@server_id_bits, @@server_uuid, @@log_bin, "
                   "@@log_bin_trust_function_creators, @@warning_count",
            .values = values,
            .column_count = independent_handle_column_count,
            .row_count = 1U,
            .warning_count = 1U,
            .context = "first server identity handle values",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT @@server_id, @@server_id_bits, @@server_uuid, @@log_bin, "
                   "@@log_bin_trust_function_creators, @@warning_count",
            .values = values,
            .column_count = independent_handle_column_count,
            .row_count = 1U,
            .warning_count = 1U,
            .context = "second server identity handle values",
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
    failures += expect_size(mylite_result_column_count(result), query.column_count, query.context);
    failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, query.context);
    failures +=
        expect_size(mylite_result_warning_count(result), query.warning_count, query.context);
    for (size_t row = 0U; row < query.row_count; ++row) {
        for (size_t column = 0U; column < query.column_count; ++column) {
            size_t value_index = (row * query.column_count) + column;

            failures += expect_text(
                mylite_result_value_text(result, row, column),
                query.values[value_index],
                query.context
            );
        }
    }
    mylite_result_free(result);

    return failures;
}

static int expect_log_bin_trust_deprecation_warnings(
    mylite_db *database,
    size_t row_count,
    const char *context
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, "SHOW WARNINGS", &result);

    if (result == NULL) {
        return failures + 1;
    }
    failures +=
        expect_size(mylite_result_column_count(result), show_warnings_column_count, context);
    failures += expect_size(mylite_result_row_count(result), row_count, context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, context);
    failures += expect_size(mylite_result_warning_count(result), 0U, context);
    for (size_t row = 0U; row < row_count; ++row) {
        failures += expect_text(mylite_result_value_text(result, row, 0U), "Warning", context);
        failures += expect_text(mylite_result_value_text(result, row, 1U), "1287", context);
        failures += expect_contains(
            mylite_result_value_text(result, row, 2U),
            log_bin_trust_deprecation_warning,
            context
        );
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
    failures += expect_size(mylite_result_column_count(result), 0U, "server identity SET columns");
    failures += expect_size(mylite_result_row_count(result), 0U, "server identity SET rows");
    failures +=
        expect_int64(mylite_result_affected_rows(result), 0, "server identity SET affected");
    failures +=
        expect_size(mylite_result_warning_count(result), 0U, "server identity SET warnings");
    mylite_result_free(result);

    return failures;
}

static int expect_set_warning(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (result == NULL) {
        return failures + 1;
    }
    failures += expect_size(mylite_result_column_count(result), 0U, "server identity SET columns");
    failures += expect_size(mylite_result_row_count(result), 0U, "server identity SET rows");
    failures +=
        expect_int64(mylite_result_affected_rows(result), 0, "server identity SET affected");
    failures +=
        expect_size(mylite_result_warning_count(result), 1U, "server identity SET warnings");
    mylite_result_free(result);
    failures += expect_log_bin_trust_deprecation_warnings(database, 1U, sql);

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
    failures += expect_true(result == NULL, "failed execute leaves result null");
    failures += expect_int(mylite_errcode(database), expected.code, "error code");
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, "SQLSTATE");
    failures += expect_contains(mylite_errmsg(database), expected.message_part, "error message");
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
        "%s/mylite_server_identity_binary_log_system_variables_%d_%s.mylite",
        directory,
        current_process_id(),
        name
    );
    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "test path is too long for %s\n", name);
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
        fprintf(
            stderr,
            "%s: expected %llu, got %llu\n",
            context,
            (unsigned long long)expected,
            (unsigned long long)actual
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

static int expect_true(int condition, const char *context) {
    if (!condition) {
        fprintf(stderr, "%s: expected true\n", context);
        return 1;
    }

    return 0;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected text '%s', got '%s'\n",
            context,
            expected == NULL ? "(null)" : expected,
            actual == NULL ? "(null)" : actual
        );
        return 1;
    }

    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || needle == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected '%s' to contain '%s'\n",
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
        fprintf(stderr, "%s: byte comparison failed\n", context);
        return 1;
    }

    return 0;
}
