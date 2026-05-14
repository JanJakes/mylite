#include <mylite/mylite.h>

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
    test_path_capacity = 1024,
    test_path_suffix_capacity = 16,
    mysql_error_no_database_selected = 1046,
    mysql_error_duplicate_alias = 1066,
    mysql_error_savepoint_does_not_exist = 1305,
    maintenance_column_count = 4,
};

struct expected_query {
    const char *sql;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    const char *context;
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

static int test_table_maintenance_result_rows(void);
static int test_table_maintenance_resolution_and_diagnostics(void);
static int test_table_maintenance_transactions_persistence_and_preamble(void);
static int test_independent_table_maintenance_handles(void);
static int create_schema_with_table(mylite_db *database);
static int expect_maintenance_rows(
    mylite_db *database,
    const char *sql,
    const char *const *values,
    size_t row_count,
    const char *context
);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
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

    failures += test_table_maintenance_result_rows();
    failures += test_table_maintenance_resolution_and_diagnostics();
    failures += test_table_maintenance_transactions_persistence_and_preamble();
    failures += test_independent_table_maintenance_handles();

    return failures == 0 ? 0 : 1;
}

static int test_table_maintenance_result_rows(void) {
    static const char *const analyze_rows[] = {"app.a", "analyze", "status", "OK"};
    static const char *const check_rows[] = {"app.a", "check", "status", "OK"};
    static const char *const optimize_rows[] = {
        "app.a",
        "optimize",
        "note",
        "Table does not support optimize, doing recreate + analyze instead",
        "app.a",
        "optimize",
        "status",
        "OK",
    };
    static const char *const repair_rows[] = {
        "app.a",
        "repair",
        "note",
        "The storage engine for the table doesn't support repair",
    };
    static const char *const multi_rows[] = {
        "app.b",
        "analyze",
        "status",
        "OK",
        "app.a",
        "analyze",
        "status",
        "OK",
    };
    static const char *const temp_rows[] = {"app.temp_only", "check", "status", "OK"};
    static const char *const status_rows[] = {"-1", "0"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "rows") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open rows file");
    failures += create_schema_with_table(database);
    failures += expect_statement_ok(database, "CREATE TABLE b (id INT, v VARCHAR(20))");
    failures += expect_statement_ok(database, "INSERT INTO b VALUES (3, 'three')");

    failures += expect_maintenance_rows(database, "ANALYZE TABLE a", analyze_rows, 1U, "analyze");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .values = status_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "analyze status variables",
        }
    );
    failures += expect_maintenance_rows(database, "CHECK TABLE a", check_rows, 1U, "check");
    failures +=
        expect_maintenance_rows(database, "OPTIMIZE TABLE a", optimize_rows, 2U, "optimize");
    failures += expect_maintenance_rows(database, "REPAIR TABLE a", repair_rows, 1U, "repair");
    failures += expect_maintenance_rows(
        database,
        "ANALYZE NO_WRITE_TO_BINLOG TABLE a",
        analyze_rows,
        1U,
        "analyze no_write_to_binlog"
    );
    failures += expect_maintenance_rows(
        database,
        "ANALYZE LOCAL TABLE a",
        analyze_rows,
        1U,
        "analyze local"
    );
    failures += expect_maintenance_rows(
        database,
        "CHECK TABLE a QUICK FAST MEDIUM EXTENDED CHANGED FOR UPGRADE",
        check_rows,
        1U,
        "check options"
    );
    failures += expect_maintenance_rows(
        database,
        "REPAIR LOCAL TABLE a QUICK EXTENDED USE_FRM",
        repair_rows,
        1U,
        "repair options"
    );
    failures +=
        expect_maintenance_rows(database, "ANALYZE TABLE b, a", multi_rows, 2U, "multi target");

    failures += expect_statement_ok(database, "CREATE TEMPORARY TABLE temp_only (id INT)");
    failures += expect_maintenance_rows(
        database,
        "CHECK TABLE temp_only",
        temp_rows,
        1U,
        "temporary table target"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_maintenance_resolution_and_diagnostics(void) {
    static const char *const qualified_rows[] = {"app.a", "check", "status", "OK"};
    static const char *const unknown_table_rows[] = {
        "app.missing",
        "analyze",
        "Error",
        "Table 'app.missing' doesn't exist",
        "app.missing",
        "analyze",
        "status",
        "Operation failed",
    };
    static const char *const unknown_schema_rows[] = {
        "missing_schema.t",
        "check",
        "Error",
        "Unknown database 'missing_schema'",
        "missing_schema.t",
        "check",
        "error",
        "Corrupt",
    };
    static const char *const reserved_table_rows[] = {
        "app._mylite_shadow",
        "check",
        "Error",
        "Table 'app._mylite_shadow' doesn't exist",
        "app._mylite_shadow",
        "check",
        "status",
        "Operation failed",
    };
    static const char *const renamed_rows[] = {"app.renamed", "analyze", "status", "OK"};
    static const char *const status_rows[] = {"-1", "0"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "resolution") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open resolution file");
    failures += create_schema_with_table(database);
    failures += expect_statement_ok(database, "CREATE TABLE b (id INT)");
    failures += expect_statement_ok(database, "RENAME TABLE b TO renamed");
    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen resolution file");
    failures += expect_maintenance_rows(
        database,
        "CHECK TABLE app.a",
        qualified_rows,
        1U,
        "qualified target without default schema"
    );
    failures += expect_error(
        database,
        "ANALYZE TABLE a",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += expect_maintenance_rows(
        database,
        "ANALYZE TABLE app.missing",
        unknown_table_rows,
        2U,
        "unknown table rows"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .values = status_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "unknown table status variables",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .values = NULL,
            .column_count = 3U,
            .row_count = 0U,
            .context = "unknown table warnings",
        }
    );
    failures += expect_maintenance_rows(
        database,
        "CHECK TABLE missing_schema.t",
        unknown_schema_rows,
        2U,
        "unknown schema rows"
    );
    failures += expect_maintenance_rows(
        database,
        "CHECK TABLE app._mylite_shadow",
        reserved_table_rows,
        2U,
        "reserved table rows"
    );
    failures += expect_error(
        database,
        "ANALYZE TABLE app.a, app.a",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_alias,
            .sqlstate = "42000",
            .message_part = "Not unique table/alias",
        }
    );
    failures += expect_maintenance_rows(
        database,
        "ANALYZE TABLE app.renamed",
        renamed_rows,
        1U,
        "renamed target"
    );
    failures += expect_statement_ok(database, "DROP TABLE app.renamed");
    failures += expect_maintenance_rows(
        database,
        "CHECK TABLE app.renamed",
        (const char *const[]){
            "app.renamed",
            "check",
            "Error",
            "Table 'app.renamed' doesn't exist",
            "app.renamed",
            "check",
            "status",
            "Operation failed",
        },
        2U,
        "dropped target"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_maintenance_transactions_persistence_and_preamble(void) {
    static const char *const analyze_rows[] = {"app.a", "analyze", "status", "OK"};
    static const char *const persisted_rows[] = {"1", "one", "2", "two", "3", "three"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "transaction") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open transaction file");
    failures += create_schema_with_table(database);
    failures += expect_statement_ok(database, "START TRANSACTION");
    failures += expect_statement_ok(database, "SAVEPOINT s");
    failures += expect_statement_ok(database, "INSERT INTO a VALUES (3, 'three')");
    failures += expect_maintenance_rows(
        database,
        "ANALYZE TABLE a",
        analyze_rows,
        1U,
        "analyze implicitly commits"
    );
    failures += expect_statement_ok(database, "ROLLBACK");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM a ORDER BY id",
            .values = persisted_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "maintenance committed transaction rows",
        }
    );
    failures += expect_error(
        database,
        "ROLLBACK TO s",
        (struct expected_sql_error){
            .code = mysql_error_savepoint_does_not_exist,
            .sqlstate = "42000",
            .message_part = "SAVEPOINT s does not exist",
        }
    );

    mylite_close(database);
    database = NULL;

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "maintenance preserves preamble"
    );
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen transaction file");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM app.a ORDER BY id",
            .values = persisted_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "maintenance rows persist after reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_independent_table_maintenance_handles(void) {
    static const char *const first_rows[] = {"1", "one", "2", "two"};
    static const char *const second_rows[] = {"10", "ten"};
    static const char *const maintenance_rows[] = {"app.a", "check", "status", "OK"};
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

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first file");
    failures += create_schema_with_table(first);
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second file");
    failures += expect_statement_ok(second, "CREATE DATABASE app");
    failures += expect_statement_ok(second, "USE app");
    failures += expect_statement_ok(second, "CREATE TABLE a (id INT, v VARCHAR(20))");
    failures += expect_statement_ok(second, "INSERT INTO a VALUES (10, 'ten')");

    failures +=
        expect_maintenance_rows(first, "CHECK TABLE a", maintenance_rows, 1U, "first check");
    failures +=
        expect_maintenance_rows(second, "CHECK TABLE a", maintenance_rows, 1U, "second check");
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT id, v FROM a ORDER BY id",
            .values = first_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "first file rows remain independent",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT id, v FROM a ORDER BY id",
            .values = second_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "second file rows remain independent",
        }
    );

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);
    return failures;
}

static int create_schema_with_table(mylite_db *database) {
    int failures = 0;

    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(database, "CREATE TABLE a (id INT, v VARCHAR(20))");
    failures += expect_statement_ok(database, "INSERT INTO a VALUES (1, 'one'), (2, 'two')");
    return failures;
}

static int expect_maintenance_rows(
    mylite_db *database,
    const char *sql,
    const char *const *values,
    size_t row_count,
    const char *context
) {
    static const char *const column_names[maintenance_column_count] = {
        "Table",
        "Op",
        "Msg_type",
        "Msg_text",
    };
    mylite_result *result = NULL;
    int failures = 0;

    failures += execute_ok(database, sql, &result);
    if (result == NULL) {
        return failures + 1;
    }
    failures += expect_size(mylite_result_column_count(result), maintenance_column_count, context);
    failures += expect_size(mylite_result_row_count(result), row_count, context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, context);
    failures += expect_size(mylite_result_warning_count(result), 0U, context);
    for (size_t column = 0U; column < maintenance_column_count; ++column) {
        failures +=
            expect_text(mylite_result_column_name(result, column), column_names[column], context);
    }
    for (size_t row = 0U; row < row_count; ++row) {
        for (size_t column = 0U; column < maintenance_column_count; ++column) {
            const size_t value_index = (row * maintenance_column_count) + column;

            failures += expect_result_value(result, row, column, values[value_index], context);
        }
    }
    mylite_result_free(result);
    return failures;
}

static int expect_statement_ok(mylite_db *database, const char *sql) {
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

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = 0;

    failures += execute_ok(database, query.sql, &result);
    if (result == NULL) {
        return failures + 1;
    }
    failures += expect_size(mylite_result_column_count(result), query.column_count, query.context);
    failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, query.context);
    for (size_t row = 0U; row < query.row_count; ++row) {
        for (size_t column = 0U; column < query.column_count; ++column) {
            const size_t value_index = (row * query.column_count) + column;
            const char *expected = query.values == NULL ? NULL : query.values[value_index];

            failures += expect_result_value(result, row, column, expected, query.context);
        }
    }
    mylite_result_free(result);
    return failures;
}

static int expect_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    const int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    if (rc != MYLITE_ERROR) {
        fprintf(stderr, "%s: expected error, got rc=%d\n", sql, rc);
        failures += 1;
    }
    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    const int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected success, got rc=%d error=%d state=%s message=%s\n",
            sql,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        if (out_result != NULL) {
            *out_result = NULL;
        }
        return 1;
    }
    if (out_result != NULL) {
        *out_result = result;
    } else {
        mylite_result_free(result);
    }
    return 0;
}

static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
) {
    const char *actual = mylite_result_value_text(result, row, column);

    if (expected == NULL) {
        if (actual != NULL) {
            fprintf(
                stderr,
                "%s: expected NULL at (%zu,%zu), got %s\n",
                context,
                row,
                column,
                actual
            );
            return 1;
        }
        return 0;
    }
    return expect_text(actual, expected, context);
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    const int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_table_maintenance_%s_%d.mylite",
        name,
        current_process_id()
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
    remove_with_suffix(path, "");
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char buffer[test_path_capacity + test_path_suffix_capacity];
    const int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(buffer)) {
        (void)remove(buffer);
    }
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        fprintf(stderr, "%s: failed to open file for reading\n", path);
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

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual == NULL && expected == NULL) {
        return 0;
    }
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected %s, got %s\n",
            context,
            expected == NULL ? "NULL" : expected,
            actual == NULL ? "NULL" : actual
        );
        return 1;
    }
    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected [%s] to contain [%s]\n",
            context,
            actual == NULL ? "NULL" : actual,
            needle
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
        fprintf(stderr, "%s: byte mismatch\n", context);
        return 1;
    }
    return 0;
}
