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
#  define P_tmpdir "."
#endif

enum {
    test_path_capacity = 1024,
    test_path_suffix_capacity = 16,
    scalar_column_count = 7,
    single_column_count = 1,
    variable_row_column_count = 2,
    mysql_error_parse = 1064,
    mysql_error_variable_cant_be_set = 1231,
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

static int test_big_tables_values_and_file_safety(void);
static int test_big_tables_assignment_diagnostics(void);
static int test_big_tables_independent_handles(void);
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

    failures += test_big_tables_values_and_file_safety();
    failures += test_big_tables_assignment_diagnostics();
    failures += test_big_tables_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_big_tables_values_and_file_safety(void) {
    static const char *const scalar_columns[] = {
        "@@big_tables",
        "@@global.big_tables",
        "@@session.big_tables",
        "@@local.big_tables",
        "HEX(@@big_tables)",
        "@@warning_count",
        "ROW_COUNT()",
    };
    static const char *const off_scalar_values[] = {"0", "0", "0", "0", "0", "0", "-1"};
    static const char *const on_scalar_values[] = {"1", "0", "1", "1", "1", "0", "0"};
    static const char *const show_columns[] = {"Variable_name", "Value"};
    static const char *const show_off_values[] = {"big_tables", "OFF"};
    static const char *const show_on_values[] = {"big_tables", "ON"};
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

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open big_tables file");
    session = mylite_connection_session_state(database);
    catalog_generation = session == NULL ? 0U : session->catalog_generation;
    sqlite_schema_generation = session == NULL ? 0U : session->sqlite_schema_generation;

    failures += expect_query_result(
        database,
        "SELECT @@big_tables, @@global.big_tables, @@session.big_tables, "
        "@@local.big_tables, HEX(@@big_tables), @@warning_count, ROW_COUNT()",
        (struct expected_result){
            .columns = scalar_columns,
            .values = off_scalar_values,
            .column_count = scalar_column_count,
            .row_count = 1U,
            .context = "default scalar values",
        }
    );
    failures += expect_query_result(
        database,
        "SHOW VARIABLES LIKE 'big_tables'",
        (struct expected_result){
            .columns = show_columns,
            .values = show_off_values,
            .column_count = variable_row_column_count,
            .row_count = 1U,
            .context = "default show row",
        }
    );

    failures += expect_nonquery_result(
        database,
        "SET SESSION big_tables = 1",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_query_result(
        database,
        "SELECT @@big_tables, @@global.big_tables, @@session.big_tables, "
        "@@local.big_tables, HEX(@@big_tables), @@warning_count, ROW_COUNT()",
        (struct expected_result){
            .columns = scalar_columns,
            .values = on_scalar_values,
            .column_count = scalar_column_count,
            .row_count = 1U,
            .context = "enabled scalar values",
        }
    );
    failures += expect_query_result(
        database,
        "SHOW SESSION VARIABLES LIKE 'big_tables'",
        (struct expected_result){
            .columns = show_columns,
            .values = show_on_values,
            .column_count = variable_row_column_count,
            .row_count = 1U,
            .context = "enabled show row",
        }
    );

    failures += expect_nonquery_result(
        database,
        "SET LOCAL big_tables = OFF",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_nonquery_result(
        database,
        "SET @@big_tables = ON",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_nonquery_result(
        database,
        "SET @@SESSION.big_tables = DEFAULT",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_nonquery_result(
        database,
        "SET @@LOCAL.big_tables = TRUE",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_nonquery_result(
        database,
        "SET big_tables = FALSE",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_nonquery_result(
        database,
        "SET big_tables = ('ON')",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_nonquery_result(
        database,
        "SET big_tables = 'OFF'",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_nonquery_result(
        database,
        "SET big_tables = (+1)",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_nonquery_result(
        database,
        "SET big_tables = -0",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_nonquery_result(
        database,
        "SET @bt = 1",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_nonquery_result(
        database,
        "SET big_tables = @bt",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_nonquery_result(
        database,
        "SET @bt = 'OFF'",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_nonquery_result(
        database,
        "SET big_tables = @bt",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_query_result(
        database,
        "SHOW VARIABLES LIKE 'big_tables'",
        (struct expected_result){
            .columns = show_columns,
            .values = show_off_values,
            .column_count = variable_row_column_count,
            .row_count = 1U,
            .context = "disabled show row",
        }
    );

    failures += expect_nonquery_result(
        database,
        "SET big_tables = ON",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_nonquery_result(
        database,
        "SET GLOBAL big_tables = DEFAULT",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_nonquery_result(
        database,
        "SET @@GLOBAL.big_tables = 0",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_nonquery_result(
        database,
        "SET GLOBAL big_tables = OFF",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_nonquery_result(
        database,
        "SET @@GLOBAL.big_tables = 'OFF'",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_query_result(
        database,
        "SELECT @@big_tables, @@global.big_tables, @@session.big_tables, "
        "@@local.big_tables, HEX(@@big_tables), @@warning_count, ROW_COUNT()",
        (struct expected_result){
            .columns = scalar_columns,
            .values = on_scalar_values,
            .column_count = scalar_column_count,
            .row_count = 1U,
            .context = "global no-op leaves session unchanged",
        }
    );

    session = mylite_connection_session_state(database);
    if (session == NULL) {
        failures += expect_int(0, 1, "session state");
    } else {
        int session_big_tables = 0;

        if (session->big_tables) {
            session_big_tables = 1;
        }
        failures += expect_int(session_big_tables, 1, "session state");
    }
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

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen big_tables file");
    failures += expect_query_result(
        database,
        "SHOW VARIABLES LIKE 'big_tables'",
        (struct expected_result){
            .columns = show_columns,
            .values = show_off_values,
            .column_count = variable_row_column_count,
            .row_count = 1U,
            .context = "reopen resets session value",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_big_tables_assignment_diagnostics(void) {
    static const char *const value_column[] = {"@@big_tables"};
    static const char *const on_value[] = {"1"};
    struct expected_sql_error cant_set = {
        .code = mysql_error_variable_cant_be_set,
        .sqlstate = "42000",
        .message_part = "Variable 'big_tables' can't be set to the value",
    };
    struct expected_sql_error incorrect_type = {
        .code = mysql_error_incorrect_argument_type,
        .sqlstate = "42000",
        .message_part = "Incorrect argument type to variable 'big_tables'",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open diagnostics memory");
    failures += expect_nonquery_result(
        database,
        "SET big_tables = ON",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );

    failures += execute_error(database, "SET SESSION big_tables = 2", cant_set);
    failures += execute_error(database, "SET SESSION big_tables = -1", cant_set);
    failures += execute_error(database, "SET SESSION big_tables = '1'", cant_set);
    failures += execute_error(database, "SET SESSION big_tables = 'TRUE'", cant_set);
    failures += execute_error(database, "SET SESSION big_tables = NULL", cant_set);
    failures += execute_error(database, "SET SESSION big_tables = 1.5", incorrect_type);
    failures += execute_error(
        database,
        "SET SESSION big_tables = (ON)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error",
        }
    );

    failures += expect_nonquery_result(
        database,
        "SET @bt = 2",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += execute_error(database, "SET SESSION big_tables = @bt", cant_set);
    failures += expect_nonquery_result(
        database,
        "SET @bt = '1'",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += execute_error(database, "SET SESSION big_tables = @bt", cant_set);
    failures += expect_nonquery_result(
        database,
        "SET @bt = 1.0",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += execute_error(database, "SET SESSION big_tables = @bt", incorrect_type);
    failures += expect_nonquery_result(
        database,
        "SET @bt = '1.0'",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += execute_error(database, "SET SESSION big_tables = @bt", cant_set);
    failures += expect_nonquery_result(
        database,
        "SET @bt = NULL",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += execute_error(database, "SET SESSION big_tables = @bt", cant_set);
    failures += execute_error(
        database,
        "SET GLOBAL big_tables = ON",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SET big_tables supports only fixed no-op global assignments",
        }
    );
    failures += execute_error(database, "SET big_tables = OFF, big_tables = 2", cant_set);
    failures += expect_query_result(
        database,
        "SELECT @@big_tables",
        (struct expected_result){
            .columns = value_column,
            .values = on_value,
            .column_count = single_column_count,
            .row_count = 1U,
            .context = "multi-assignment rollback",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_big_tables_independent_handles(void) {
    static const char *const columns[] = {"@@big_tables"};
    static const char *const first_values[] = {"1"};
    static const char *const second_values[] = {"0"};
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&first), MYLITE_OK, "open first handle");
    failures += expect_int(mylite_open_memory(&second), MYLITE_OK, "open second handle");
    failures += expect_nonquery_result(
        first,
        "SET big_tables = ON",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_query_result(
        first,
        "SELECT @@big_tables",
        (struct expected_result){
            .columns = columns,
            .values = first_values,
            .column_count = single_column_count,
            .row_count = 1U,
            .context = "first handle value",
        }
    );
    failures += expect_query_result(
        second,
        "SELECT @@big_tables",
        (struct expected_result){
            .columns = columns,
            .values = second_values,
            .column_count = single_column_count,
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
        "%s/mylite_big_tables_%d_%s.mylite",
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
