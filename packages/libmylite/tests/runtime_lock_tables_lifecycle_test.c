#include <mylite/mylite.h>

#include "runtime/mylite_connection.h"
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
    mysql_error_no_database_selected = 1046,
    mysql_error_unknown_database = 1049,
    mysql_error_not_unique_table_alias = 1066,
    mysql_error_incorrect_database_name = 1102,
    mysql_error_incorrect_table_name = 1103,
    mysql_error_table_does_not_exist = 1146,
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
    const char *message;
};

struct expected_table_lock {
    const char *schema_name;
    const char *table_name;
    const char *alias;
    const char *effective_name;
    enum mylite_session_table_lock_mode mode;
    bool is_temporary;
};

static int test_lock_tables_successful_lifecycle(void);
static int test_lock_tables_resolution_and_diagnostics(void);
static int test_lock_tables_transaction_persistence_and_preamble(void);
static int test_independent_lock_table_handles(void);
static int create_schema_with_tables(mylite_db *database);
static int expect_nonquery(mylite_db *database, const char *sql, int64_t affected_rows);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_lock_state(
    const mylite_db *database,
    const struct expected_table_lock *expected,
    size_t expected_count,
    const char *context
);
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
static int expect_bool(bool actual, bool expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_lock_tables_successful_lifecycle();
    failures += test_lock_tables_resolution_and_diagnostics();
    failures += test_lock_tables_transaction_persistence_and_preamble();
    failures += test_independent_lock_table_handles();

    return failures == 0 ? 0 : 1;
}

static int test_lock_tables_successful_lifecycle(void) {
    static const char *const status_values[] = {"0", "0"};
    static const struct expected_table_lock read_lock[] = {
        {"app", "t", "", "t", MYLITE_SESSION_TABLE_LOCK_READ, false},
    };
    static const struct expected_table_lock multi_locks[] = {
        {"app", "t", "reader", "reader", MYLITE_SESSION_TABLE_LOCK_READ_LOCAL, false},
        {"app", "u", "writer", "writer", MYLITE_SESSION_TABLE_LOCK_WRITE, false},
    };
    static const struct expected_table_lock case_sensitive_locks[] = {
        {"app", "t", "", "t", MYLITE_SESSION_TABLE_LOCK_READ, false},
        {"app", "u", "T", "T", MYLITE_SESSION_TABLE_LOCK_WRITE, false},
    };
    static const struct expected_table_lock temporary_lock[] = {
        {"app", "temp_t", "", "temp_t", MYLITE_SESSION_TABLE_LOCK_WRITE, true},
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "success") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open success file");
    failures += create_schema_with_tables(database);

    failures += expect_nonquery(database, "LOCK TABLES t READ", 0);
    failures += expect_lock_state(database, read_lock, 1U, "single read lock");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .values = status_values,
            .column_count = 2U,
            .row_count = 1U,
            .context = "lock status variables",
        }
    );

    failures +=
        expect_nonquery(database, "LOCK TABLES app.t AS reader READ LOCAL, u writer WRITE", 0);
    failures += expect_lock_state(database, multi_locks, 2U, "multi lock replacement");
    failures += expect_nonquery(database, "UNLOCK TABLES", 0);
    failures += expect_lock_state(database, NULL, 0U, "unlock tables releases locks");

    failures += expect_nonquery(database, "LOCK TABLES t READ, u AS T WRITE", 0);
    failures +=
        expect_lock_state(database, case_sensitive_locks, 2U, "case-sensitive effective aliases");
    failures += expect_nonquery(database, "UNLOCK TABLES", 0);

    failures += expect_nonquery(database, "LOCK TABLE t WRITE", 0);
    failures += expect_nonquery(database, "UNLOCK TABLE", 0);
    failures += expect_lock_state(database, NULL, 0U, "unlock table releases locks");

    failures += expect_nonquery(database, "CREATE TEMPORARY TABLE temp_t (id INT)", 0);
    failures += expect_nonquery(database, "LOCK TABLES temp_t WRITE", 0);
    failures += expect_lock_state(database, temporary_lock, 1U, "temporary lock");
    failures += expect_nonquery(database, "UNLOCK TABLES", 0);

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_lock_tables_resolution_and_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics file");
    failures += expect_nonquery(database, "CREATE DATABASE app", 1);
    failures += expect_error(
        database,
        "LOCK TABLES t READ",
        (struct expected_sql_error){
            mysql_error_no_database_selected,
            "3D000",
            "No database selected",
        }
    );
    failures += expect_nonquery(database, "USE app", 0);
    failures += expect_nonquery(database, "CREATE TABLE t (id INT)", 0);
    failures += expect_nonquery(database, "CREATE TABLE u (id INT)", 0);

    failures += expect_error(
        database,
        "LOCK TABLES missing_schema.t READ",
        (struct expected_sql_error){
            mysql_error_unknown_database,
            "42000",
            "Unknown database 'missing_schema'",
        }
    );
    failures += expect_error(
        database,
        "LOCK TABLES missing READ",
        (struct expected_sql_error){
            mysql_error_table_does_not_exist,
            "42S02",
            "Table 'app.missing' doesn't exist",
        }
    );
    failures += expect_error(
        database,
        "LOCK TABLES t AS same READ, u same WRITE",
        (struct expected_sql_error){
            mysql_error_not_unique_table_alias,
            "42000",
            "Not unique table/alias: 'same'",
        }
    );
    failures += expect_error(
        database,
        "LOCK TABLES _mylite_hidden READ",
        (struct expected_sql_error){
            mysql_error_incorrect_table_name,
            "42000",
            "Incorrect table name '_mylite_hidden'",
        }
    );
    failures += expect_error(
        database,
        "LOCK TABLES _mylite_internal.t READ",
        (struct expected_sql_error){
            mysql_error_incorrect_database_name,
            "42000",
            "Incorrect database name '_mylite_internal'",
        }
    );
    failures += expect_lock_state(database, NULL, 0U, "failed locks do not mutate state");

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_lock_tables_transaction_persistence_and_preamble(void) {
    static const char *const committed_values[] = {"1", "10"};
    static const char *const failed_lock_committed_values[] = {"1", "10", "4", "40"};
    static const char *const replacement_released_values[] = {"1", "10", "4", "40", "5", "50"};
    static const char *const reopened_values[] = {"1", "10", "3", "30", "4", "40", "5", "50"};
    static const struct expected_table_lock write_lock[] = {
        {"app", "t", "", "t", MYLITE_SESSION_TABLE_LOCK_WRITE, false},
    };
    static const struct expected_table_lock read_lock[] = {
        {"app", "t", "", "t", MYLITE_SESSION_TABLE_LOCK_READ, false},
    };
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "transaction") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open transaction file");
    failures += create_schema_with_tables(database);

    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (1, 10)", 1);
    failures += expect_nonquery(database, "LOCK TABLES t WRITE", 0);
    failures += expect_lock_state(database, write_lock, 1U, "lock after implicit commit");
    failures += expect_nonquery(database, "ROLLBACK", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, value FROM t ORDER BY id",
            .values = committed_values,
            .column_count = 2U,
            .row_count = 1U,
            .context = "lock implicitly commits transaction",
        }
    );
    failures += expect_lock_state(database, write_lock, 1U, "rollback does not release locks");

    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_lock_state(database, NULL, 0U, "start transaction releases locks");
    failures += expect_nonquery(database, "INSERT INTO t VALUES (2, 20)", 1);
    failures += expect_nonquery(database, "ROLLBACK", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, value FROM t ORDER BY id",
            .values = committed_values,
            .column_count = 2U,
            .row_count = 1U,
            .context = "transaction after lock release rolls back",
        }
    );

    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (4, 40)", 1);
    failures += expect_error(
        database,
        "LOCK TABLES missing_after_tx READ",
        (struct expected_sql_error){
            mysql_error_table_does_not_exist,
            "42S02",
            "Table 'app.missing_after_tx' doesn't exist",
        }
    );
    failures += expect_nonquery(database, "ROLLBACK", 0);
    failures += expect_lock_state(database, NULL, 0U, "failed lock leaves no lock intent");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, value FROM t ORDER BY id",
            .values = failed_lock_committed_values,
            .column_count = 2U,
            .row_count = 2U,
            .context = "failed lock implicitly commits transaction",
        }
    );

    failures += expect_nonquery(database, "LOCK TABLES t READ", 0);
    failures += expect_lock_state(database, read_lock, 1U, "read lock before failed replacement");
    failures += expect_error(
        database,
        "LOCK TABLES missing_after_lock READ",
        (struct expected_sql_error){
            mysql_error_table_does_not_exist,
            "42S02",
            "Table 'app.missing_after_lock' doesn't exist",
        }
    );
    failures += expect_lock_state(database, NULL, 0U, "failed replacement releases locks");
    failures += expect_nonquery(database, "INSERT INTO t VALUES (5, 50)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, value FROM t ORDER BY id",
            .values = replacement_released_values,
            .column_count = 2U,
            .row_count = 3U,
            .context = "write after failed replacement",
        }
    );

    failures += expect_nonquery(database, "INSERT INTO t VALUES (3, 30)", 1);
    failures += expect_nonquery(database, "LOCK TABLES t READ", 0);
    mylite_close(database);
    database = NULL;

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "lock lifecycle preserves MyLite preamble"
    );

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen transaction file");
    failures += expect_lock_state(database, NULL, 0U, "reopen clears lock state");
    failures += expect_nonquery(database, "USE app", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, value FROM t ORDER BY id",
            .values = reopened_values,
            .column_count = 2U,
            .row_count = 4U,
            .context = "reopened persisted rows",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_independent_lock_table_handles(void) {
    static const struct expected_table_lock read_lock[] = {
        {"app", "t", "", "t", MYLITE_SESSION_TABLE_LOCK_READ, false},
    };
    static const struct expected_table_lock write_lock[] = {
        {"app", "t", "", "t", MYLITE_SESSION_TABLE_LOCK_WRITE, false},
    };
    char path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "handles") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &first), MYLITE_OK, "open first handle");
    failures += create_schema_with_tables(first);
    failures += expect_int(mylite_open(path, &second), MYLITE_OK, "open second handle");
    failures += expect_nonquery(second, "USE app", 0);

    failures += expect_nonquery(first, "LOCK TABLES t READ", 0);
    failures += expect_lock_state(first, read_lock, 1U, "first handle lock");
    failures += expect_lock_state(second, NULL, 0U, "second handle starts unlocked");

    failures += expect_nonquery(second, "LOCK TABLES t WRITE", 0);
    failures += expect_lock_state(first, read_lock, 1U, "first handle remains locked");
    failures += expect_lock_state(second, write_lock, 1U, "second handle independent lock");

    mylite_close(second);
    mylite_close(first);
    remove_related_files(path);
    return failures;
}

static int create_schema_with_tables(mylite_db *database) {
    int failures = 0;

    failures += expect_nonquery(database, "CREATE DATABASE app", 1);
    failures += expect_nonquery(database, "USE app", 0);
    failures += expect_nonquery(database, "CREATE TABLE t (id INT, value INT)", 0);
    failures += expect_nonquery(database, "CREATE TABLE u (id INT)", 0);
    return failures;
}

static int expect_nonquery(mylite_db *database, const char *sql, int64_t affected_rows) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    failures += expect_int(rc, MYLITE_OK, sql);
    if (rc == MYLITE_OK) {
        failures += expect_size(mylite_result_column_count(result), 0U, "nonquery column count");
        failures += expect_size(mylite_result_row_count(result), 0U, "nonquery row count");
        failures += expect_int64(mylite_result_affected_rows(result), affected_rows, sql);
        failures += expect_size(mylite_result_warning_count(result), 0U, "nonquery warning count");
    } else {
        fprintf(stderr, "%s failed: %s\n", sql, mylite_errmsg(database));
    }
    mylite_result_free(result);
    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, query.sql, strlen(query.sql), &result);

    failures += expect_int(rc, MYLITE_OK, query.context);
    if (rc == MYLITE_OK) {
        failures +=
            expect_size(mylite_result_column_count(result), query.column_count, query.context);
        failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
        failures += expect_size(mylite_result_warning_count(result), 0U, query.context);
        for (size_t row = 0U; row < query.row_count; ++row) {
            for (size_t column = 0U; column < query.column_count; ++column) {
                const size_t value_index = (row * query.column_count) + column;

                failures += expect_result_value(
                    result,
                    row,
                    column,
                    query.values == NULL ? NULL : query.values[value_index],
                    query.context
                );
            }
        }
    } else {
        fprintf(stderr, "%s failed: %s\n", query.sql, mylite_errmsg(database));
    }
    mylite_result_free(result);
    return failures;
}

static int expect_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    failures += expect_int(rc, MYLITE_ERROR, sql);
    failures += expect_int(mylite_errcode(database), expected.code, "diagnostic code");
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, "diagnostic SQLSTATE");
    failures += expect_text(mylite_errmsg(database), expected.message, "diagnostic message");
    failures += expect_size((size_t)(result != NULL), 0U, "error result");
    mylite_result_free(result);
    return failures;
}

static int expect_lock_state(
    const mylite_db *database,
    const struct expected_table_lock *expected,
    size_t expected_count,
    const char *context
) {
    const struct mylite_session_state *session = mylite_connection_session_state(database);
    int failures = 0;

    failures += expect_size(session->table_lock_count, expected_count, context);
    for (size_t index = 0U; index < expected_count && index < session->table_lock_count; ++index) {
        const struct mylite_session_table_lock *actual = &session->table_locks[index];
        const struct expected_table_lock *entry = &expected[index];

        failures += expect_text(actual->schema_name, entry->schema_name, context);
        failures += expect_text(actual->table_name, entry->table_name, context);
        failures += expect_text(actual->alias, entry->alias, context);
        failures += expect_text(actual->effective_name, entry->effective_name, context);
        failures += expect_int((int)actual->mode, (int)entry->mode, context);
        failures += expect_bool(actual->is_temporary, entry->is_temporary, context);
    }
    return failures;
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
        return expect_size((size_t)(actual != NULL), 0U, context);
    }
    return expect_text(actual, expected, context);
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_lock_tables_lifecycle_%d_%s.mylite",
        current_process_id(),
        name
    );

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "test path truncated\n");
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
    remove_with_suffix(path, "-shm");
    remove_with_suffix(path, "-wal");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related[test_path_capacity];
    int written = snprintf(related, sizeof(related), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related)) {
        return;
    }
    (void)remove(related);
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t bytes_read = 0U;

    if (file == NULL) {
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fclose(file);
        return 1;
    }
    bytes_read = fread(buffer, 1U, size, file);
    fclose(file);
    return bytes_read == size ? 0 : 1;
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual == expected) {
        return 0;
    }

    fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
    return 1;
}

static int expect_int64(int64_t actual, int64_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }

    fprintf(
        stderr,
        "%s: expected %lld, got %lld\n",
        context,
        (long long)expected,
        (long long)actual
    );
    return 1;
}

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }

    fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
    return 1;
}

static int expect_bool(bool actual, bool expected, const char *context) {
    const char *actual_text = "false";
    const char *expected_text = "false";

    if (actual == expected) {
        return 0;
    }

    if (expected) {
        expected_text = "true";
    }
    if (actual) {
        actual_text = "true";
    }
    fprintf(stderr, "%s: expected %s, got %s\n", context, expected_text, actual_text);
    return 1;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual != NULL && expected != NULL && strcmp(actual, expected) == 0) {
        return 0;
    }

    fprintf(stderr, "%s: expected \"%s\", got \"%s\"\n", context, expected, actual);
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

    fprintf(stderr, "%s: bytes differ\n", context);
    return 1;
}
