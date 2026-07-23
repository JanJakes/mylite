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
    value_column_count = 6,
    label_column_count = 5,
    post_set_column_count = 5,
    independent_column_count = 3,
    mysql_error_parse = 1064,
    mysql_error_session_variable_read_only = 1621,
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

struct expected_statement {
    const char *sql;
    const char *context;
};

static int test_max_allowed_packet_values_and_persistence(void);
static int test_max_allowed_packet_set_diagnostics(void);
static int test_independent_max_allowed_packet_handles(void);
static int expect_query_result(
    mylite_db *database,
    const char *sql,
    struct expected_result expected
);
static int expect_result(const mylite_result *result, struct expected_result expected);
static int expect_nonquery_ok(mylite_db *database, struct expected_statement expected);
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

    failures += test_max_allowed_packet_values_and_persistence();
    failures += test_max_allowed_packet_set_diagnostics();
    failures += test_independent_max_allowed_packet_handles();

    return failures == 0 ? 0 : 1;
}

static int test_max_allowed_packet_values_and_persistence(void) {
    static const char *const value_columns[] = {
        "@@max_allowed_packet",
        "@@global.max_allowed_packet",
        "@@session.max_allowed_packet",
        "@@local.max_allowed_packet",
        "@@warning_count",
        "ROW_COUNT()",
    };
    static const char *const value_values[] = {
        "67108864",
        "67108864",
        "67108864",
        "67108864",
        "0",
        "-1",
    };
    static const char *const label_columns[] = {
        "@@MAX_ALLOWED_PACKET",
        "@@Global.Max_Allowed_Packet",
        "@@session.`max_allowed_packet`",
        "@@`max_allowed_packet`",
        "(@@max_allowed_packet)",
    };
    static const char *const label_values[] = {
        "67108864",
        "67108864",
        "67108864",
        "67108864",
        "67108864",
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    const struct mylite_session_state *session = NULL;
    uint64_t catalog_generation = 0U;
    uint64_t sqlite_schema_generation = 0U;
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "lifecycle") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open max packet file");
    session = mylite_connection_session_state(database);
    catalog_generation = session->catalog_generation;
    sqlite_schema_generation = session->sqlite_schema_generation;

    failures += expect_query_result(
        database,
        "SELECT @@max_allowed_packet, @@global.max_allowed_packet, "
        "@@session.max_allowed_packet, @@local.max_allowed_packet, @@warning_count, "
        "ROW_COUNT() FROM DUAL",
        (struct expected_result){
            .columns = value_columns,
            .values = value_values,
            .count = value_column_count,
            .context = "max allowed packet values",
        }
    );
    failures += expect_query_result(
        database,
        "SELECT @@MAX_ALLOWED_PACKET, @@Global.Max_Allowed_Packet, "
        "@@session.`max_allowed_packet`, @@`max_allowed_packet`, "
        "(@@max_allowed_packet)",
        (struct expected_result){
            .columns = label_columns,
            .values = label_values,
            .count = label_column_count,
            .context = "max allowed packet labels",
        }
    );
    failures += expect_nonquery_ok(
        database,
        (struct expected_statement){
            .sql = "SET GLOBAL max_allowed_packet = 67108864",
            .context = "set global max packet same value",
        }
    );
    failures += expect_nonquery_ok(
        database,
        (struct expected_statement){
            .sql = "SET @@GLOBAL.max_allowed_packet = 67108864",
            .context = "set global system variable max packet same value",
        }
    );
    failures += expect_nonquery_ok(
        database,
        (struct expected_statement){
            .sql = "SET @@GLOBAL.max_allowed_packet = DEFAULT",
            .context = "set global max packet default",
        }
    );
    failures += expect_nonquery_ok(
        database,
        (struct expected_statement){
            .sql = "SET GLOBAL max_allowed_packet = +67108864",
            .context = "set global max packet plus same value",
        }
    );
    failures += expect_query_result(
        database,
        "SELECT @@global.max_allowed_packet, @@session.max_allowed_packet, "
        "@@warning_count, @@error_count, ROW_COUNT()",
        (struct expected_result){
            .columns =
                (const char *const[]){
                    "@@global.max_allowed_packet",
                    "@@session.max_allowed_packet",
                    "@@warning_count",
                    "@@error_count",
                    "ROW_COUNT()",
                },
            .values = (const char *const[]){"67108864", "67108864", "0", "0", "0"},
            .count = post_set_column_count,
            .context = "max packet after no-op set",
        }
    );

    session = mylite_connection_session_state(database);
    failures += mylite_test_expect_uint64(
        session->catalog_generation,
        catalog_generation,
        "max packet leaves catalog generation"
    );
    failures += mylite_test_expect_uint64(
        session->sqlite_schema_generation,
        sqlite_schema_generation,
        "max packet leaves SQLite schema generation"
    );

    mylite_close(database);
    database = NULL;

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "max packet preserves preamble"
    );

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "reopen max packet file");
    failures += expect_query_result(
        database,
        "SELECT @@max_allowed_packet, @@global.max_allowed_packet, "
        "@@session.max_allowed_packet, @@local.max_allowed_packet, @@warning_count, "
        "ROW_COUNT()",
        (struct expected_result){
            .columns = value_columns,
            .values = value_values,
            .count = value_column_count,
            .context = "reopened max allowed packet values",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_max_allowed_packet_set_diagnostics(void) {
    mylite_db *database = NULL;
    int failures = mylite_test_expect_int(
        mylite_test_open_temporary(&database),
        MYLITE_OK,
        "open diagnostics"
    );
    const struct expected_sql_error session_read_only = {
        .code = mysql_error_session_variable_read_only,
        .sqlstate = "HY000",
        .message_part = "SESSION variable 'max_allowed_packet' is read-only. Use SET GLOBAL to "
                        "assign the value",
    };
    const struct expected_sql_error unsupported_global = {
        .code = mysql_error_parse,
        .sqlstate = "42000",
        .message_part = "SET max_allowed_packet supports only fixed no-op global assignments",
    };

    failures += execute_error(database, "SET max_allowed_packet = 67108864", session_read_only);
    failures +=
        execute_error(database, "SET SESSION max_allowed_packet = 67108864", session_read_only);
    failures +=
        execute_error(database, "SET LOCAL max_allowed_packet = 67108864", session_read_only);
    failures += execute_error(database, "SET @@max_allowed_packet = 67108864", session_read_only);
    failures +=
        execute_error(database, "SET @@SESSION.max_allowed_packet = 67108864", session_read_only);
    failures +=
        execute_error(database, "SET @@LOCAL.max_allowed_packet = 67108864", session_read_only);
    failures += execute_error(database, "SET GLOBAL max_allowed_packet = 1024", unsupported_global);
    failures += execute_error(database, "SET @@GLOBAL.max_allowed_packet = -1", unsupported_global);
    failures += expect_query_result(
        database,
        "SELECT @@max_allowed_packet, @@warning_count, @@error_count, ROW_COUNT()",
        (struct expected_result){
            .columns =
                (const char *const[]){
                    "@@max_allowed_packet",
                    "@@warning_count",
                    "@@error_count",
                    "ROW_COUNT()",
                },
            .values = (const char *const[]){"67108864", "1", "1", "-1"},
            .count = 4U,
            .context = "max packet survives rejected set",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_independent_max_allowed_packet_handles(void) {
    static const char *const columns[] = {
        "@@max_allowed_packet",
        "@@warning_count",
        "@@error_count",
    };
    static const char *const values[] = {"67108864", "0", "0"};
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    failures +=
        mylite_test_expect_int(mylite_test_open_temporary(&first), MYLITE_OK, "open first handle");
    failures += mylite_test_expect_int(
        mylite_test_open_temporary(&second),
        MYLITE_OK,
        "open second handle"
    );

    failures += expect_nonquery_ok(
        first,
        (struct expected_statement){
            .sql = "SET GLOBAL max_allowed_packet = DEFAULT",
            .context = "first no-op global set",
        }
    );
    failures += expect_query_result(
        first,
        "SELECT @@max_allowed_packet, @@warning_count, @@error_count",
        (struct expected_result){
            .columns = columns,
            .values = values,
            .count = independent_column_count,
            .context = "first max packet handle",
        }
    );
    failures += expect_query_result(
        second,
        "SELECT @@max_allowed_packet, @@warning_count, @@error_count",
        (struct expected_result){
            .columns = columns,
            .values = values,
            .count = independent_column_count,
            .context = "second max packet handle",
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

    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        expected.count,
        expected.context
    );
    failures += mylite_test_expect_size(mylite_result_row_count(result), 1U, expected.context);
    failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, expected.context);
    for (size_t index = 0U; index < expected.count; ++index) {
        failures += mylite_test_expect_text_or_null(
            mylite_result_column_name(result, index),
            expected.columns[index],
            expected.context
        );
        failures += mylite_test_expect_text_or_null(
            mylite_result_value_text(result, 0U, index),
            expected.values[index],
            expected.context
        );
    }

    return failures;
}

static int expect_nonquery_ok(mylite_db *database, struct expected_statement expected) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, expected.sql, &result);

    failures += mylite_test_expect_size(mylite_result_column_count(result), 0U, expected.context);
    failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, expected.context);
    failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, expected.context);
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

    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, sql);
    failures += mylite_test_expect_text_or_null(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += mylite_test_expect_contains(mylite_errmsg(database), expected.message_part, sql);
    return failures;
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
