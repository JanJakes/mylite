#include <mylite/mylite.h>

#include "runtime_test_support.h"

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
#  define P_tmpdir "."
#endif

enum {
    test_path_capacity = 1024,
    path_suffix_capacity = 16,
    timeout_scalar_column_count = 10,
    timeout_label_column_count = 5,
    timeout_mutated_column_count = 9,
    mysql_error_parse = 1064,
    mysql_error_incorrect_argument_type = 1232,
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

static int test_timeout_variables_values_and_persistence(void);
static int test_timeout_assignment_diagnostics(void);
static int test_timeout_independent_handles(void);
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
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_uint64(uint64_t actual, uint64_t expected, const char *context);
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

    failures += test_timeout_variables_values_and_persistence();
    failures += test_timeout_assignment_diagnostics();
    failures += test_timeout_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_timeout_variables_values_and_persistence(void) {
    static const char *const scalar_columns[] = {
        "@@wait_timeout",
        "@@global.wait_timeout",
        "@@session.wait_timeout",
        "@@local.wait_timeout",
        "@@interactive_timeout",
        "@@global.interactive_timeout",
        "@@session.interactive_timeout",
        "@@local.interactive_timeout",
        "@@warning_count",
        "ROW_COUNT()",
    };
    static const char *const default_scalar_values[] = {
        "28800",
        "28800",
        "28800",
        "28800",
        "28800",
        "28800",
        "28800",
        "28800",
        "0",
        "-1",
    };
    static const char *const label_columns[] = {
        "@@WAIT_TIMEOUT",
        "@@Global.Interactive_Timeout",
        "@@session.`wait_timeout`",
        "@@`interactive_timeout`",
        "HEX(@@wait_timeout)",
    };
    static const char *const label_values[] = {"28800", "28800", "28800", "28800", "7080"};
    static const char *const show_columns[] = {"Variable_name", "Value"};
    static const char *const show_default_values[] = {
        "interactive_timeout",
        "28800",
        "wait_timeout",
        "28800",
    };
    static const char *const show_mutated_values[] = {
        "interactive_timeout",
        "2",
        "wait_timeout",
        "1",
    };
    static const char *const show_global_values[] = {
        "interactive_timeout",
        "28800",
        "wait_timeout",
        "28800",
    };
    static const char *const mutated_values[] = {
        "1",
        "28800",
        "1",
        "2",
        "28800",
        "2",
        "0",
        "0",
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

    if (make_test_path(path, sizeof(path), "lifecycle") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open timeout file");
    session = mylite_connection_session_state(database);
    catalog_generation = session == NULL ? 0U : session->catalog_generation;
    sqlite_schema_generation = session == NULL ? 0U : session->sqlite_schema_generation;

    failures += expect_query_result(
        database,
        "SELECT @@wait_timeout, @@global.wait_timeout, @@session.wait_timeout, "
        "@@local.wait_timeout, @@interactive_timeout, @@global.interactive_timeout, "
        "@@session.interactive_timeout, @@local.interactive_timeout, @@warning_count, "
        "ROW_COUNT()",
        (struct expected_result){
            .columns = scalar_columns,
            .values = default_scalar_values,
            .column_count = timeout_scalar_column_count,
            .row_count = 1U,
            .context = "timeout default scalar values",
        }
    );
    failures += expect_query_result(
        database,
        "SELECT @@WAIT_TIMEOUT, @@Global.Interactive_Timeout, "
        "@@session.`wait_timeout`, @@`interactive_timeout`, HEX(@@wait_timeout)",
        (struct expected_result){
            .columns = label_columns,
            .values = label_values,
            .column_count = timeout_label_column_count,
            .row_count = 1U,
            .context = "timeout label values",
        }
    );
    failures += expect_query_result(
        database,
        "SHOW VARIABLES WHERE Variable_name IN ('interactive_timeout','wait_timeout')",
        (struct expected_result){
            .columns = show_columns,
            .values = show_default_values,
            .column_count = 2U,
            .row_count = 2U,
            .context = "timeout default show rows",
        }
    );

    failures += expect_nonquery_result(
        database,
        "SET SESSION wait_timeout = 1",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_nonquery_result(
        database,
        "SET LOCAL interactive_timeout = 2",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_query_result(
        database,
        "SELECT @@wait_timeout, @@global.wait_timeout, @@session.wait_timeout, "
        "@@interactive_timeout, @@global.interactive_timeout, @@session.interactive_timeout, "
        "@@warning_count, @@error_count, ROW_COUNT()",
        (struct expected_result){
            .columns =
                (const char *const[]){
                    "@@wait_timeout",
                    "@@global.wait_timeout",
                    "@@session.wait_timeout",
                    "@@interactive_timeout",
                    "@@global.interactive_timeout",
                    "@@session.interactive_timeout",
                    "@@warning_count",
                    "@@error_count",
                    "ROW_COUNT()",
                },
            .values = mutated_values,
            .column_count = timeout_mutated_column_count,
            .row_count = 1U,
            .context = "timeout mutated scalar values",
        }
    );
    failures += expect_query_result(
        database,
        "SHOW SESSION VARIABLES WHERE Variable_name IN ('interactive_timeout','wait_timeout')",
        (struct expected_result){
            .columns = show_columns,
            .values = show_mutated_values,
            .column_count = 2U,
            .row_count = 2U,
            .context = "timeout session show rows",
        }
    );
    failures += expect_query_result(
        database,
        "SHOW GLOBAL VARIABLES WHERE Variable_name IN ('interactive_timeout','wait_timeout')",
        (struct expected_result){
            .columns = show_columns,
            .values = show_global_values,
            .column_count = 2U,
            .row_count = 2U,
            .context = "timeout global show rows",
        }
    );

    failures += expect_nonquery_result(
        database,
        "SET GLOBAL wait_timeout = 28800",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_nonquery_result(
        database,
        "SET @@GLOBAL.interactive_timeout = DEFAULT",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_nonquery_result(
        database,
        "SET GLOBAL wait_timeout = +28800",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_query_result(
        database,
        "SELECT @@wait_timeout, @@interactive_timeout, @@global.wait_timeout, "
        "@@global.interactive_timeout",
        (struct expected_result){
            .columns =
                (const char *const[]){
                    "@@wait_timeout",
                    "@@interactive_timeout",
                    "@@global.wait_timeout",
                    "@@global.interactive_timeout",
                },
            .values = (const char *const[]){"1", "2", "28800", "28800"},
            .column_count = 4U,
            .row_count = 1U,
            .context = "timeout global no-op preserves session",
        }
    );

    failures += expect_nonquery_result(
        database,
        "SET @@wait_timeout = DEFAULT",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_nonquery_result(
        database,
        "SET @@LOCAL.interactive_timeout = DEFAULT",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_query_result(
        database,
        "SELECT @@wait_timeout, @@interactive_timeout, @@warning_count",
        (struct expected_result){
            .columns =
                (const char *const[]){"@@wait_timeout", "@@interactive_timeout", "@@warning_count"},
            .values = (const char *const[]){"28800", "28800", "0"},
            .column_count = 3U,
            .row_count = 1U,
            .context = "timeout defaults after reset",
        }
    );

    session = mylite_connection_session_state(database);
    if (session != NULL) {
        failures += expect_uint64(
            session->catalog_generation,
            catalog_generation,
            "timeout variables leave catalog generation"
        );
        failures += expect_uint64(
            session->sqlite_schema_generation,
            sqlite_schema_generation,
            "timeout variables leave SQLite schema generation"
        );
    }

    mylite_close(database);
    database = NULL;

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "timeout variables preserve preamble"
    );

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen timeout file");
    failures += expect_query_result(
        database,
        "SELECT @@wait_timeout, @@interactive_timeout, @@warning_count, ROW_COUNT()",
        (struct expected_result){
            .columns =
                (const char *const[]){
                    "@@wait_timeout",
                    "@@interactive_timeout",
                    "@@warning_count",
                    "ROW_COUNT()",
                },
            .values = (const char *const[]){"28800", "28800", "0", "-1"},
            .column_count = 4U,
            .row_count = 1U,
            .context = "timeout values reset after reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_timeout_assignment_diagnostics(void) {
    static const char *const warning_columns[] = {"Level", "Code", "Message"};
    static const struct expected_sql_error incorrect_argument_type = {
        .code = mysql_error_incorrect_argument_type,
        .sqlstate = "42000",
        .message_part = "Incorrect argument type to variable 'wait_timeout'",
    };
    static const struct expected_sql_error incorrect_interactive_argument_type = {
        .code = mysql_error_incorrect_argument_type,
        .sqlstate = "42000",
        .message_part = "Incorrect argument type to variable 'interactive_timeout'",
    };
    static const struct expected_sql_error unsupported_global = {
        .code = mysql_error_parse,
        .sqlstate = "42000",
        .message_part = "SET timeout system variable supports only fixed no-op global assignments",
    };
    mylite_db *database = NULL;
    int failures = expect_int(mylite_test_open_temporary(&database), MYLITE_OK, "open diagnostics");

    failures += expect_nonquery_result(
        database,
        "SET @@wait_timeout = 0",
        (struct expected_statement){.affected_rows = 0, .warning_count = 1U}
    );
    failures += expect_query_result(
        database,
        "SHOW WARNINGS",
        (struct expected_result){
            .columns = warning_columns,
            .values =
                (const char *const[]){
                    "Warning",
                    "1292",
                    "Truncated incorrect wait_timeout value: '0'",
                },
            .column_count = 3U,
            .row_count = 1U,
            .context = "timeout zero warning",
        }
    );
    failures += expect_query_result(
        database,
        "SELECT @@wait_timeout, @@warning_count",
        (struct expected_result){
            .columns = (const char *const[]){"@@wait_timeout", "@@warning_count"},
            .values = (const char *const[]){"1", "1"},
            .column_count = 2U,
            .row_count = 1U,
            .context = "timeout zero clamped",
        }
    );

    failures += expect_nonquery_result(
        database,
        "SET @@interactive_timeout = -1",
        (struct expected_statement){.affected_rows = 0, .warning_count = 1U}
    );
    failures += expect_query_result(
        database,
        "SHOW WARNINGS",
        (struct expected_result){
            .columns = warning_columns,
            .values =
                (const char *const[]){
                    "Warning",
                    "1292",
                    "Truncated incorrect interactive_timeout value: '-1'",
                },
            .column_count = 3U,
            .row_count = 1U,
            .context = "interactive timeout negative warning",
        }
    );
    failures += expect_nonquery_result(
        database,
        "SET @@wait_timeout = 999999999999",
        (struct expected_statement){.affected_rows = 0, .warning_count = 1U}
    );
    failures += expect_query_result(
        database,
        "SELECT @@wait_timeout, @@warning_count",
        (struct expected_result){
            .columns = (const char *const[]){"@@wait_timeout", "@@warning_count"},
            .values = (const char *const[]){"31536000", "1"},
            .column_count = 2U,
            .row_count = 1U,
            .context = "timeout high clamped",
        }
    );
    failures += expect_nonquery_result(
        database,
        "SET LOCAL wait_timeout = +5",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_nonquery_result(
        database,
        "SET @@SESSION.interactive_timeout = TRUE",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_nonquery_result(
        database,
        "SET @@interactive_timeout = FALSE",
        (struct expected_statement){.affected_rows = 0, .warning_count = 1U}
    );
    failures += expect_query_result(
        database,
        "SELECT @@wait_timeout, @@interactive_timeout, @@warning_count",
        (struct expected_result){
            .columns =
                (const char *const[]){"@@wait_timeout", "@@interactive_timeout", "@@warning_count"},
            .values = (const char *const[]){"5", "1", "1"},
            .column_count = 3U,
            .row_count = 1U,
            .context = "timeout plus true false values",
        }
    );

    failures += execute_error(database, "SET wait_timeout = '5'", incorrect_argument_type);
    failures += execute_error(database, "SET wait_timeout = 1.5", incorrect_argument_type);
    failures += execute_error(database, "SET wait_timeout = NULL", incorrect_argument_type);
    failures += execute_error(database, "SET wait_timeout = ON", incorrect_argument_type);
    failures += execute_error(database, "SET wait_timeout = OFF", incorrect_argument_type);
    failures += execute_error(database, "SET GLOBAL wait_timeout = 1", unsupported_global);

    failures += expect_nonquery_result(
        database,
        "SET @wt = 7",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_nonquery_result(
        database,
        "SET wait_timeout = @wt",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_nonquery_result(
        database,
        "SET @it = -2",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_nonquery_result(
        database,
        "SET interactive_timeout = @it",
        (struct expected_statement){.affected_rows = 0, .warning_count = 1U}
    );
    failures += expect_query_result(
        database,
        "SHOW WARNINGS",
        (struct expected_result){
            .columns = warning_columns,
            .values =
                (const char *const[]){
                    "Warning",
                    "1292",
                    "Truncated incorrect interactive_timeout value: '-2'",
                },
            .column_count = 3U,
            .row_count = 1U,
            .context = "interactive timeout user variable warning",
        }
    );
    failures += expect_query_result(
        database,
        "SELECT @@wait_timeout, @@interactive_timeout",
        (struct expected_result){
            .columns = (const char *const[]){"@@wait_timeout", "@@interactive_timeout"},
            .values = (const char *const[]){"7", "1"},
            .column_count = 2U,
            .row_count = 1U,
            .context = "timeout user variable values",
        }
    );

    failures += expect_nonquery_result(
        database,
        "SET @wt = '7'",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += execute_error(database, "SET wait_timeout = @wt", incorrect_argument_type);
    failures += expect_nonquery_result(
        database,
        "SET @wt = NULL",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += execute_error(database, "SET wait_timeout = @wt", incorrect_argument_type);
    failures += execute_error(
        database,
        "SET wait_timeout = 9, interactive_timeout = 'x'",
        incorrect_interactive_argument_type
    );
    failures += expect_query_result(
        database,
        "SELECT @@wait_timeout",
        (struct expected_result){
            .columns = (const char *const[]){"@@wait_timeout"},
            .values = (const char *const[]){"7"},
            .column_count = 1U,
            .row_count = 1U,
            .context = "timeout rollback after failed multi-assignment",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_timeout_independent_handles(void) {
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    failures += expect_int(mylite_test_open_temporary(&first), MYLITE_OK, "open first handle");
    failures += expect_int(mylite_test_open_temporary(&second), MYLITE_OK, "open second handle");
    failures += expect_nonquery_result(
        first,
        "SET wait_timeout = 5",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_nonquery_result(
        first,
        "SET interactive_timeout = 6",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_query_result(
        first,
        "SELECT @@wait_timeout, @@interactive_timeout",
        (struct expected_result){
            .columns = (const char *const[]){"@@wait_timeout", "@@interactive_timeout"},
            .values = (const char *const[]){"5", "6"},
            .column_count = 2U,
            .row_count = 1U,
            .context = "first timeout handle",
        }
    );
    failures += expect_query_result(
        second,
        "SELECT @@wait_timeout, @@interactive_timeout",
        (struct expected_result){
            .columns = (const char *const[]){"@@wait_timeout", "@@interactive_timeout"},
            .values = (const char *const[]){"28800", "28800"},
            .column_count = 2U,
            .row_count = 1U,
            .context = "second timeout handle",
        }
    );

    mylite_close(first);
    mylite_close(second);
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
        "%s/mylite_timeout_system_variables_%d_%s.mylite",
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

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
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
