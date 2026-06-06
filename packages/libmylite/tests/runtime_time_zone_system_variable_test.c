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
    path_suffix_capacity = 16,
    default_scalar_column_count = 5,
    stored_current_column_count = 5,
    mysql_error_parse = 1064,
    mysql_error_variable_cant_be_set = 1231,
    mysql_error_incorrect_argument_type = 1232,
    mysql_error_session_variable_only = 1238,
    mysql_error_unknown_or_incorrect_time_zone = 1298,
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

struct expected_assignment {
    const char *sql;
    const char *text;
    int offset_minutes;
    const char *context;
};

static int test_time_zone_defaults_and_assignments(void);
static int test_time_zone_current_time_and_file_persistence(void);
static int test_time_zone_diagnostics_and_independent_handles(void);
static int expect_assignment(mylite_db *database, struct expected_assignment assignment);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_int(int actual, int expected, const char *context);
static int expect_uint64(uint64_t actual, uint64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
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

    failures += test_time_zone_defaults_and_assignments();
    failures += test_time_zone_current_time_and_file_persistence();
    failures += test_time_zone_diagnostics_and_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_time_zone_defaults_and_assignments(void) {
    static const char *const default_values[] = {"SYSTEM", "SYSTEM", "SYSTEM", "UTC", "-1"};
    static const char *const show_time_zone_values[] = {"time_zone", "SYSTEM"};
    static const char *const show_system_time_zone_values[] = {"system_time_zone", "UTC"};
    static const struct expected_assignment assignments[] = {
        {"SET time_zone = '+00:00'", "+00:00", 0, "set unscoped zero offset"},
        {"SET @@time_zone = '+5:30'", "+05:30", 330, "set system variable one-digit hour"},
        {"SET SESSION time_zone = '-6:00'", "-06:00", -360, "set session scope"},
        {"SET LOCAL time_zone = '+14:00'", "+14:00", 840, "set local maximum offset"},
        {"SET @@SESSION.time_zone = '-13:59'", "-13:59", -839, "set session minimum offset"},
        {"SET @@LOCAL.time_zone = '-0:00'", "+00:00", 0, "set local negative zero"},
        {"SET @@session.Time_Zone = '+0:00'", "+00:00", 0, "set case-insensitive target"},
        {"SET time_zone = DEFAULT", "SYSTEM", 0, "set default"},
        {"SET time_zone = 'system'", "SYSTEM", 0, "set lowercase system string"},
        {"SET time_zone = 'utc'", "UTC", 0, "set lowercase utc string"},
        {"SET time_zone = UTC", "UTC", 0, "set unquoted utc"},
        {"SET time_zone = SYSTEM", "SYSTEM", 0, "set unquoted system"},
    };
    const struct mylite_session_state *session = NULL;
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open defaults memory");
    session = mylite_connection_session_state(database);
    if (session != NULL) {
        failures += expect_text(session->time_zone, "SYSTEM", "initial time_zone text");
        failures += expect_int(session->time_zone_offset_minutes, 0, "initial time_zone offset");
        failures += expect_int((int)session->time_zone_is_placeholder, 0, "initial placeholder");
    }

    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@GLOBAL.time_zone, @@SESSION.time_zone, @@time_zone, "
                   "@@GLOBAL.system_time_zone, ROW_COUNT() FROM DUAL",
            .values = default_values,
            .column_count = default_scalar_column_count,
            .row_count = 1U,
            .context = "time zone scalar defaults",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW VARIABLES LIKE 'time_zone'",
            .values = show_time_zone_values,
            .column_count = 2U,
            .row_count = 1U,
            .context = "show session time_zone",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW GLOBAL VARIABLES LIKE 'time_zone'",
            .values = show_time_zone_values,
            .column_count = 2U,
            .row_count = 1U,
            .context = "show global time_zone",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW VARIABLES LIKE 'system_time_zone'",
            .values = show_system_time_zone_values,
            .column_count = 2U,
            .row_count = 1U,
            .context = "show session system_time_zone",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW GLOBAL VARIABLES LIKE 'system_time_zone'",
            .values = show_system_time_zone_values,
            .column_count = 2U,
            .row_count = 1U,
            .context = "show global system_time_zone",
        }
    );

    for (size_t index = 0U; index < sizeof(assignments) / sizeof(assignments[0]); ++index) {
        failures += expect_assignment(database, assignments[index]);
    }

    mylite_close(database);
    return failures;
}

static int test_time_zone_current_time_and_file_persistence(void) {
    static const char *const plus_values[] = {
        "2023-11-15 00:43:20",
        "2023-11-15",
        "00:43:20",
        "2023-11-15 00:43:20",
    };
    static const char *const minus_values[] = {
        "2023-11-14 16:13:20",
        "2023-11-14",
        "16:13:20",
    };
    static const char *const utc_values[] = {
        "2023-11-14 22:13:20",
        "2023-11-14",
        "22:13:20",
    };
    static const char *const stored_values[] = {
        "1",
        "2023-11-15",
        "00:43:20",
        "2023-11-15 00:43:20",
        "2023-11-15 00:43:20",
    };
    static const char *const reopened_time_zone[] = {"SYSTEM"};
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

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open time zone file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(database, "SET timestamp = 1700000000");
    failures += expect_statement_ok(database, "SET time_zone = '+02:30'");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT NOW(), CURDATE(), CURTIME(), CURRENT_TIMESTAMP",
            .values = plus_values,
            .column_count = 4U,
            .row_count = 1U,
            .context = "current functions plus offset",
        }
    );
    failures += expect_statement_ok(database, "SET time_zone = '-06:00'");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT NOW(), CURDATE(), CURTIME()",
            .values = minus_values,
            .column_count = 3U,
            .row_count = 1U,
            .context = "current functions negative offset",
        }
    );
    failures += expect_statement_ok(database, "SET time_zone = UTC");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT NOW(), CURDATE(), CURTIME()",
            .values = utc_values,
            .column_count = 3U,
            .row_count = 1U,
            .context = "current functions UTC",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE current_values ("
        "id INT, "
        "d DATE DEFAULT (CURRENT_DATE), "
        "tm TIME DEFAULT (CURRENT_TIME), "
        "dt DATETIME DEFAULT CURRENT_TIMESTAMP, "
        "ts TIMESTAMP DEFAULT CURRENT_TIMESTAMP)"
    );
    failures += expect_statement_ok(database, "SET time_zone = '+02:30'");
    failures += expect_statement_ok(database, "INSERT INTO current_values (id) VALUES (1)");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, d, tm, dt, ts FROM current_values",
            .values = stored_values,
            .column_count = stored_current_column_count,
            .row_count = 1U,
            .context = "stored current values use session offset",
        }
    );

    session = mylite_connection_session_state(database);
    if (session != NULL) {
        catalog_generation = session->catalog_generation;
        sqlite_schema_generation = session->sqlite_schema_generation;
    }
    failures += expect_statement_ok(database, "SET time_zone = '-06:00'");
    session = mylite_connection_session_state(database);
    if (session != NULL) {
        failures += expect_uint64(
            session->catalog_generation,
            catalog_generation,
            "time_zone does not mutate catalog generation"
        );
        failures += expect_uint64(
            session->sqlite_schema_generation,
            sqlite_schema_generation,
            "time_zone does not mutate SQLite schema generation"
        );
    }
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(actual_preamble),
        "time_zone file preamble"
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen time zone file");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@time_zone",
            .values = reopened_time_zone,
            .column_count = 1U,
            .row_count = 1U,
            .context = "time zone resets on reopen",
        }
    );
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, d, tm, dt, ts FROM current_values",
            .values = stored_values,
            .column_count = stored_current_column_count,
            .row_count = 1U,
            .context = "stored current values persist after reopen",
        }
    );
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(actual_preamble),
        "reopened time_zone file preamble"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_time_zone_diagnostics_and_independent_handles(void) {
    static const char *const first_values[] = {"+02:30"};
    static const char *const second_values[] = {"SYSTEM"};
    static const char *const invalid_offsets[] = {
        "SET time_zone = '+14:01'",
        "SET time_zone = '-14:00'",
        "SET time_zone = '+00:60'",
        "SET time_zone = '+0'",
        "SET time_zone = '00:00'",
        "SET time_zone = '+00:00:00'",
        "SET time_zone = ''",
        "SET time_zone = ' +00:00'",
    };
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&first), MYLITE_OK, "open first time zone handle");
    failures += expect_int(mylite_open_memory(&second), MYLITE_OK, "open second time zone handle");
    failures += expect_statement_ok(first, "SET time_zone = '+02:30'");
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT @@time_zone",
            .values = first_values,
            .column_count = 1U,
            .row_count = 1U,
            .context = "first handle time zone",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT @@time_zone",
            .values = second_values,
            .column_count = 1U,
            .row_count = 1U,
            .context = "second handle default time zone",
        }
    );

    failures += execute_error(
        first,
        "SELECT @@SESSION.system_time_zone",
        (struct expected_sql_error){
            mysql_error_session_variable_only,
            "HY000",
            "Variable 'system_time_zone' is a GLOBAL variable",
        }
    );
    failures += execute_error(
        first,
        "SET system_time_zone = 'UTC'",
        (struct expected_sql_error){
            mysql_error_session_variable_only,
            "HY000",
            "Variable 'system_time_zone' is a read only variable",
        }
    );
    failures += execute_error(
        first,
        "SET GLOBAL time_zone = '+00:00'",
        (struct expected_sql_error){
            mysql_error_parse,
            "42000",
            "SET GLOBAL system variable assignment is not supported",
        }
    );
    failures += execute_error(
        first,
        "SET time_zone = 0",
        (struct expected_sql_error){
            mysql_error_incorrect_argument_type,
            "42000",
            "Incorrect argument type to variable 'time_zone'",
        }
    );
    failures += execute_error(
        first,
        "SET time_zone = NULL",
        (struct expected_sql_error){
            mysql_error_variable_cant_be_set,
            "42000",
            "Variable 'time_zone' can't be set to the value of 'NULL'",
        }
    );

    for (size_t index = 0U; index < sizeof(invalid_offsets) / sizeof(invalid_offsets[0]); ++index) {
        failures += execute_error(
            first,
            invalid_offsets[index],
            (struct expected_sql_error){
                mysql_error_unknown_or_incorrect_time_zone,
                "HY000",
                "Unknown or incorrect time zone",
            }
        );
    }

    mylite_close(second);
    mylite_close(first);
    return failures;
}

static int expect_assignment(mylite_db *database, struct expected_assignment assignment) {
    static const char *const count_columns[] = {"@@time_zone", "@@warning_count", "ROW_COUNT()"};
    const struct mylite_session_state *session = NULL;
    const char *values[3] = {assignment.text, "0", "0"};
    int failures = 0;

    failures += expect_statement_ok(database, assignment.sql);
    session = mylite_connection_session_state(database);
    if (session != NULL) {
        failures += expect_text(session->time_zone, assignment.text, assignment.context);
        failures += expect_int(
            session->time_zone_offset_minutes,
            assignment.offset_minutes,
            assignment.context
        );
    }
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@time_zone, @@warning_count, ROW_COUNT()",
            .values = values,
            .column_count = sizeof(count_columns) / sizeof(count_columns[0]),
            .row_count = 1U,
            .context = assignment.context,
        }
    );
    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    size_t value_count = query.column_count * query.row_count;
    int failures = 0;

    failures += execute_ok(database, query.sql, &result);
    if (result == NULL) {
        return failures + 1;
    }
    failures += expect_size(mylite_result_column_count(result), query.column_count, query.context);
    failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
    for (size_t index = 0U; index < value_count; ++index) {
        failures += expect_text(
            mylite_result_value_text(
                result,
                index / query.column_count,
                index % query.column_count
            ),
            query.values[index],
            query.context
        );
    }
    mylite_result_free(result);
    return failures;
}

static int expect_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (result == NULL) {
        return failures + 1;
    }
    failures += expect_size(mylite_result_column_count(result), 0U, sql);
    failures += expect_size(mylite_result_row_count(result), 0U, sql);
    mylite_result_free(result);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected success, got %d/%s/%s\n",
            sql,
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

    if (rc == MYLITE_OK) {
        fprintf(stderr, "%s: expected error, got success\n", sql);
        mylite_result_free(result);
        return 1;
    }
    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, sql);
    return failures;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "%s/mylite_time_zone_system_variable_%d_%s.mylite",
        P_tmpdir,
        current_process_id(),
        name
    );

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "failed to build test path\n");
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
    char related[test_path_capacity + path_suffix_capacity];
    int written = snprintf(related, sizeof(related), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(related)) {
        remove(related);
    }
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        fprintf(stderr, "failed to open %s\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "failed to seek %s\n", path);
        fclose(file);
        return 1;
    }
    if (fread(buffer, 1U, size, file) != size) {
        fprintf(stderr, "failed to read %s\n", path);
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

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected \"%s\", got \"%s\"\n",
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
            "%s: expected \"%s\" to contain \"%s\"\n",
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
