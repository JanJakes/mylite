#include <mylite/mylite.h>

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
};

struct expected_query {
    const char *sql;
    const char *const *column_names;
    size_t column_count;
    const char *const *values;
    size_t row_count;
    const char *context;
};

struct expected_statement {
    const char *sql;
    const char *context;
};

static int test_information_schema_innodb_trx_queries(void);
static int test_information_schema_innodb_trx_open_transaction(void);
static int setup_transaction_table(mylite_db *database);
static int expect_statement_ok(mylite_db *database, struct expected_statement expected);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_row_count_status(mylite_db *database, const char *context);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text_or_null(const char *actual, const char *expected, const char *context);

int main(void) {
    int failures = 0;

    failures += test_information_schema_innodb_trx_queries();
    failures += test_information_schema_innodb_trx_open_transaction();

    return failures == 0 ? 0 : 1;
}

static int test_information_schema_innodb_trx_queries(void) {
    static const char *const trx_columns[] = {
        "trx_id",
        "trx_state",
        "trx_started",
        "trx_requested_lock_id",
        "trx_wait_started",
        "trx_weight",
        "trx_mysql_thread_id",
        "trx_query",
        "trx_operation_state",
        "trx_tables_in_use",
        "trx_tables_locked",
        "trx_lock_structs",
        "trx_lock_memory_bytes",
        "trx_rows_locked",
        "trx_rows_modified",
        "trx_concurrency_tickets",
        "trx_isolation_level",
        "trx_unique_checks",
        "trx_foreign_key_checks",
        "trx_last_foreign_key_error",
        "trx_adaptive_hash_latched",
        "trx_adaptive_hash_timeout",
        "trx_is_read_only",
        "trx_autocommit_non_locking",
        "trx_schedule_weight",
    };
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_zero[] = {"0"};
    static const char *const trx_id_column[] = {"trx_id"};
    static const char *const system_table_columns[] = {
        "TABLE_SCHEMA",
        "TABLE_NAME",
        "TABLE_TYPE",
        "ENGINE",
        "VERSION",
        "ROW_FORMAT",
        "TABLE_ROWS",
        "DATA_LENGTH",
        "AUTO_INCREMENT",
    };
    static const char *const system_table_values[] = {
        "information_schema",
        "INNODB_TRX",
        "SYSTEM VIEW",
        NULL,
        "10",
        NULL,
        "0",
        "0",
        NULL,
    };
    static const char *const columns_metadata_columns[] = {
        "TABLE_NAME",
        "COLUMN_NAME",
        "ORDINAL_POSITION",
        "COLUMN_DEFAULT",
        "IS_NULLABLE",
        "DATA_TYPE",
        "CHARACTER_MAXIMUM_LENGTH",
        "CHARACTER_OCTET_LENGTH",
        "NUMERIC_PRECISION",
        "NUMERIC_SCALE",
        "DATETIME_PRECISION",
        "CHARACTER_SET_NAME",
        "COLLATION_NAME",
        "COLUMN_TYPE",
        "PRIVILEGES",
    };
    static const char *const columns_metadata_values[] = {
        "INNODB_TRX",
        "trx_id",
        "1",
        "",
        "NO",
        "bigint",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "bigint unsigned",
        "select",
        "INNODB_TRX",
        "trx_state",
        "2",
        "",
        "NO",
        "varchar",
        "4",
        "13",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(13)",
        "select",
        "INNODB_TRX",
        "trx_started",
        "3",
        "",
        "NO",
        "datetime",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "datetime",
        "select",
        "INNODB_TRX",
        "trx_requested_lock_id",
        "4",
        "",
        "YES",
        "varchar",
        "42",
        "126",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(126)",
        "select",
        "INNODB_TRX",
        "trx_wait_started",
        "5",
        "",
        "YES",
        "datetime",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "datetime",
        "select",
        "INNODB_TRX",
        "trx_weight",
        "6",
        "",
        "NO",
        "bigint",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "bigint unsigned",
        "select",
        "INNODB_TRX",
        "trx_mysql_thread_id",
        "7",
        "",
        "NO",
        "bigint",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "bigint unsigned",
        "select",
        "INNODB_TRX",
        "trx_query",
        "8",
        "",
        "YES",
        "varchar",
        "341",
        "1024",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(1024)",
        "select",
        "INNODB_TRX",
        "trx_operation_state",
        "9",
        "",
        "YES",
        "varchar",
        "21",
        "64",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(64)",
        "select",
        "INNODB_TRX",
        "trx_tables_in_use",
        "10",
        "",
        "NO",
        "bigint",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "bigint unsigned",
        "select",
        "INNODB_TRX",
        "trx_tables_locked",
        "11",
        "",
        "NO",
        "bigint",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "bigint unsigned",
        "select",
        "INNODB_TRX",
        "trx_lock_structs",
        "12",
        "",
        "NO",
        "bigint",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "bigint unsigned",
        "select",
        "INNODB_TRX",
        "trx_lock_memory_bytes",
        "13",
        "",
        "NO",
        "bigint",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "bigint unsigned",
        "select",
        "INNODB_TRX",
        "trx_rows_locked",
        "14",
        "",
        "NO",
        "bigint",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "bigint unsigned",
        "select",
        "INNODB_TRX",
        "trx_rows_modified",
        "15",
        "",
        "NO",
        "bigint",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "bigint unsigned",
        "select",
        "INNODB_TRX",
        "trx_concurrency_tickets",
        "16",
        "",
        "NO",
        "bigint",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "bigint unsigned",
        "select",
        "INNODB_TRX",
        "trx_isolation_level",
        "17",
        "",
        "NO",
        "varchar",
        "5",
        "16",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(16)",
        "select",
        "INNODB_TRX",
        "trx_unique_checks",
        "18",
        "",
        "NO",
        "int",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "int",
        "select",
        "INNODB_TRX",
        "trx_foreign_key_checks",
        "19",
        "",
        "NO",
        "int",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "int",
        "select",
        "INNODB_TRX",
        "trx_last_foreign_key_error",
        "20",
        "",
        "YES",
        "varchar",
        "85",
        "256",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(256)",
        "select",
        "INNODB_TRX",
        "trx_adaptive_hash_latched",
        "21",
        "",
        "NO",
        "int",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "int",
        "select",
        "INNODB_TRX",
        "trx_adaptive_hash_timeout",
        "22",
        "",
        "NO",
        "bigint",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "bigint unsigned",
        "select",
        "INNODB_TRX",
        "trx_is_read_only",
        "23",
        "",
        "NO",
        "int",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "int",
        "select",
        "INNODB_TRX",
        "trx_autocommit_non_locking",
        "24",
        "",
        "NO",
        "int",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "int",
        "select",
        "INNODB_TRX",
        "trx_schedule_weight",
        "25",
        "",
        "YES",
        "bigint",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "bigint unsigned",
        "select",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "queries") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open innodb trx db");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM INFORMATION_SCHEMA.INNODB_TRX",
            .column_names = trx_columns,
            .column_count = sizeof(trx_columns) / sizeof(trx_columns[0]),
            .values = NULL,
            .row_count = 0U,
            .context = "empty innodb trx wildcard",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_TRX",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "empty innodb trx count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.innodb_trx",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "case-insensitive innodb trx table",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT t.trx_id FROM INFORMATION_SCHEMA.INNODB_TRX AS t "
                   "WHERE t.trx_state = 'RUNNING' ORDER BY t.trx_id LIMIT 1",
            .column_names = trx_id_column,
            .column_count = 1U,
            .values = NULL,
            .row_count = 0U,
            .context = "empty innodb trx alias predicate",
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "USE information_schema",
            .context = "use information_schema for innodb trx",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INNODB_TRX",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "unqualified innodb trx count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, "
                   "TABLE_ROWS, DATA_LENGTH, AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'INNODB_TRX'",
            .column_names = system_table_columns,
            .column_count = sizeof(system_table_columns) / sizeof(system_table_columns[0]),
            .values = system_table_values,
            .row_count = 1U,
            .context = "innodb trx system table row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, "
                   "IS_NULLABLE, DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, "
                   "CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION, NUMERIC_SCALE, "
                   "DATETIME_PRECISION, CHARACTER_SET_NAME, COLLATION_NAME, "
                   "COLUMN_TYPE, PRIVILEGES FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'information_schema' "
                   "AND TABLE_NAME = 'INNODB_TRX' ORDER BY ORDINAL_POSITION",
            .column_names = columns_metadata_columns,
            .column_count = sizeof(columns_metadata_columns) / sizeof(columns_metadata_columns[0]),
            .values = columns_metadata_values,
            .row_count = sizeof(columns_metadata_values) / sizeof(columns_metadata_values[0]) /
                         (sizeof(columns_metadata_columns) / sizeof(columns_metadata_columns[0])),
            .context = "innodb trx columns metadata",
        }
    );
    failures += expect_row_count_status(database, "innodb trx row count status");

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_information_schema_innodb_trx_open_transaction(void) {
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_zero[] = {"0"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "transaction") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open innodb trx tx db");
    failures += setup_transaction_table(database);
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "START TRANSACTION",
            .context = "start transaction for innodb trx",
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "UPDATE tx_probe SET v = v + 1 WHERE id = 1",
            .context = "update transaction for innodb trx",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_TRX",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "innodb trx remains empty during transaction",
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "ROLLBACK",
            .context = "rollback transaction for innodb trx",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int setup_transaction_table(mylite_db *database) {
    int failures = 0;

    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "CREATE SCHEMA app",
            .context = "create schema for innodb trx",
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "USE app",
            .context = "use schema for innodb trx",
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "CREATE TABLE tx_probe (id INT PRIMARY KEY, v INT)",
            .context = "create tx probe for innodb trx",
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "INSERT INTO tx_probe VALUES (1, 1)",
            .context = "insert tx probe for innodb trx",
        }
    );

    return failures;
}

static int expect_statement_ok(mylite_db *database, struct expected_statement expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);

    failures += expect_int(rc, MYLITE_OK, expected.context);
    if (rc == MYLITE_OK) {
        failures += expect_size(mylite_result_warning_count(result), 0U, expected.context);
    } else {
        fprintf(stderr, "%s: %s\n", expected.context, mylite_errmsg(database));
    }
    mylite_result_free(result);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);

    failures += expect_int(rc, MYLITE_OK, expected.context);
    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: %s\n", expected.context, mylite_errmsg(database));
        mylite_result_free(result);
        return failures + 1;
    }
    if (result == NULL) {
        fprintf(stderr, "%s: expected result object\n", expected.context);
        return failures + 1;
    }

    failures +=
        expect_size(mylite_result_column_count(result), expected.column_count, expected.context);
    failures += expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, expected.context);
    for (size_t column = 0U; column < expected.column_count; ++column) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, column),
            expected.column_names[column],
            expected.context
        );
    }
    for (size_t row = 0U; row < expected.row_count; ++row) {
        for (size_t column = 0U; column < expected.column_count; ++column) {
            size_t value_index = (row * expected.column_count) + column;

            failures += expect_text_or_null(
                mylite_result_value_text(result, row, column),
                expected.values[value_index],
                expected.context
            );
        }
    }
    mylite_result_free(result);
    return failures;
}

static int expect_row_count_status(mylite_db *database, const char *context) {
    static const char *const status_columns[] = {"@@warning_count", "ROW_COUNT()"};
    static const char *const status_values[] = {"0", "-1"};

    return expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count, ROW_COUNT()",
            .column_names = status_columns,
            .column_count = 2U,
            .values = status_values,
            .row_count = 1U,
            .context = context,
        }
    );
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "%s/mylite_information_schema_innodb_trx_%d_%s.mylite",
        P_tmpdir,
        current_process_id(),
        name
    );

    return written < 0 || (size_t)written >= path_size ? 1 : 0;
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
    char buffer[test_path_capacity + test_path_suffix_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written > 0 && (size_t)written < sizeof(buffer)) {
        remove(buffer);
    }
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

static int expect_text_or_null(const char *actual, const char *expected, const char *context) {
    if (actual == NULL && expected == NULL) {
        return 0;
    }
    if (actual != NULL && expected != NULL && strcmp(actual, expected) == 0) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected [%s], got [%s]\n",
        context,
        expected == NULL ? "NULL" : expected,
        actual == NULL ? "NULL" : actual
    );
    return 1;
}
