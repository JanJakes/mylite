#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define MYLITE_TEST_SETUP_METRICS_ROW_HASH UINT64_C(0x8358ac5bda78c673)
#define MYLITE_TEST_FNV1A_OFFSET_BASIS UINT64_C(1469598103934665603)
#define MYLITE_TEST_FNV1A_PRIME UINT64_C(1099511628211)

enum {
    mysql_error_access_denied = 1044,
    setup_metrics_column_count = 6,
    setup_metrics_row_count = 422,
    setup_metrics_sample_row_count = 13,
    show_full_columns_column_count = 9,
    show_index_column_count = 15,
    show_table_status_column_count = 18,
    information_schema_columns_projection_count = 7,
    information_schema_statistics_projection_count = 7,
    information_schema_tables_projection_count = 7,
    information_schema_constraints_projection_count = 3,
    information_schema_key_usage_projection_count = 3,
    information_schema_constraint_extensions_projection_count = 5,
};

struct expected_query {
    const char *sql;
    const char *const *column_names;
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

static int test_performance_schema_setup_metrics(void);
static int execute_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query query);
static int expect_rowset_hash(
    mylite_db *database,
    const char *sql,
    uint64_t expected_hash,
    const char *context
);
static int expect_row_count_state(mylite_db *database, const char *context);
static uint64_t fnv1a_update_text(uint64_t hash, const char *text);
static uint64_t fnv1a_update_byte(uint64_t hash, unsigned char byte);
static int expect_int(int actual, int expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_uint64(uint64_t actual, uint64_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

int main(void) {
    return test_performance_schema_setup_metrics() == 0 ? 0 : 1;
}

static int test_performance_schema_setup_metrics(void) {
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const setup_metric_columns[] = {
        "NAME",
        "METER",
        "METRIC_TYPE",
        "NUM_TYPE",
        "UNIT",
        "DESCRIPTION",
    };
    static const char *const show_full_columns_columns[] = {
        "Field",
        "Type",
        "Collation",
        "Null",
        "Key",
        "Default",
        "Extra",
        "Privileges",
        "Comment",
    };
    static const char *const show_index_columns[] = {
        "Table",
        "Non_unique",
        "Key_name",
        "Seq_in_index",
        "Column_name",
        "Collation",
        "Cardinality",
        "Sub_part",
        "Packed",
        "Null",
        "Index_type",
        "Comment",
        "Index_comment",
        "Visible",
        "Expression",
    };
    static const char *const show_table_status_columns[] = {
        "Name",
        "Engine",
        "Version",
        "Row_format",
        "Rows",
        "Avg_row_length",
        "Data_length",
        "Max_data_length",
        "Index_length",
        "Data_free",
        "Auto_increment",
        "Create_time",
        "Update_time",
        "Check_time",
        "Collation",
        "Checksum",
        "Create_options",
        "Comment",
    };
    static const char *const information_schema_columns_columns[] = {
        "COLUMN_NAME",
        "ORDINAL_POSITION",
        "IS_NULLABLE",
        "COLUMN_TYPE",
        "COLUMN_KEY",
        "COLUMN_DEFAULT",
        "COLLATION_NAME",
    };
    static const char *const information_schema_statistics_columns[] = {
        "INDEX_NAME",
        "NON_UNIQUE",
        "SEQ_IN_INDEX",
        "COLUMN_NAME",
        "COLLATION",
        "CARDINALITY",
        "INDEX_TYPE",
    };
    static const char *const information_schema_tables_columns[] = {
        "TABLE_NAME",
        "TABLE_TYPE",
        "ENGINE",
        "ROW_FORMAT",
        "TABLE_ROWS",
        "AUTO_INCREMENT",
        "TABLE_COLLATION",
    };
    static const char *const information_schema_constraints_columns[] = {
        "CONSTRAINT_NAME",
        "CONSTRAINT_TYPE",
        "ENFORCED",
    };
    static const char *const information_schema_key_usage_columns[] = {
        "CONSTRAINT_NAME",
        "COLUMN_NAME",
        "ORDINAL_POSITION",
    };
    static const char *const information_schema_constraint_extensions_columns[] = {
        "CONSTRAINT_SCHEMA",
        "TABLE_NAME",
        "CONSTRAINT_NAME",
        "ENGINE_ATTRIBUTE",
        "SECONDARY_ENGINE_ATTRIBUTE",
    };
    static const char *const count_two[] = {"2"};
    static const char *const count_422[] = {"422"};
    static const char *const selected_schema_num_type[] = {"INTEGER"};
    static const char *const setup_metric_samples[] = {
        "dblwr_pages_written",
        "mysql.inno",
        "ASYNC COUNTER",
        "INTEGER",
        "",
        // NOLINTNEXTLINE(bugprone-suspicious-missing-comma)
        "Number of pages that have been written for doublewrite operations "
        "(innodb_dblwr_pages_written)",
        "os_log_written",
        "mysql.inno",
        "ASYNC COUNTER",
        "INTEGER",
        "By",
        "Bytes of log written (innodb_os_log_written)",
        "wait_free",
        "mysql.inno.buffer_pool",
        "ASYNC COUNTER",
        "INTEGER",
        "",
        "Number of times waited for free buffer (innodb_buffer_pool_wait_free)",
        "fsyncs",
        "mysql.inno.data",
        "ASYNC COUNTER",
        "INTEGER",
        "",
        "Number of fsync() calls (innodb_data_fsyncs)",
        "key_writes",
        "mysql.myisam",
        "ASYNC COUNTER",
        "INTEGER",
        "",
        // NOLINTNEXTLINE(bugprone-suspicious-missing-comma)
        "The number of physical writes of a key block from the MyISAM key cache to disk "
        "(Key_writes)",
        "users_lost",
        "mysql.perf_schema",
        "ASYNC COUNTER",
        "INTEGER",
        "",
        // NOLINTNEXTLINE(bugprone-suspicious-missing-comma)
        "The number of times a row could not be added to the users table because it was full "
        "(Performance_schema_users_lost)",
        "slow_queries",
        "mysql.stats",
        "ASYNC COUNTER",
        "INTEGER",
        "",
        "The number of queries that have taken more than long_query_time seconds (Slow_queries)",
        "stmt_reprepare",
        "mysql.stats.com",
        "ASYNC COUNTER",
        "INTEGER",
        "",
        "Number of times corresponding command statement has been executed.",
        "errors_tcpwrap",
        "mysql.stats.connection",
        "ASYNC COUNTER",
        "INTEGER",
        "",
        "The number of connections refused by the libwrap library (Connection_errors_tcpwrap)",
        "update",
        "mysql.stats.handler",
        "ASYNC COUNTER",
        "INTEGER",
        "",
        "The number of requests to update a row in a table (Handler_update)",
        "callback_cache_hits",
        "mysql.stats.ssl",
        "ASYNC COUNTER",
        "INTEGER",
        "",
        "The number of accepted SSL connections (Ssl_callback_cache_hits)",
        "ssl_finished_accepts",
        "mysql.x",
        "ASYNC COUNTER",
        "INTEGER",
        "",
        "The number of successful SSL connections to the server (Mysqlx_ssl_finished_accepts)",
        "list_clients",
        "mysql.x.stmt",
        "ASYNC COUNTER",
        "INTEGER",
        "",
        "The number of list client statements received (Mysqlx_stmt_list_clients)",
    };
    static const char *const full_columns[] = {
        "NAME",
        "varchar(63)",
        "utf8mb4_0900_ai_ci",
        "NO",
        "PRI",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "METER",
        "varchar(63)",
        "utf8mb4_0900_ai_ci",
        "NO",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "METRIC_TYPE",
        "enum('ASYNC COUNTER','ASYNC UPDOWN COUNTER','ASYNC GAUGE COUNTER')",
        "utf8mb4_0900_ai_ci",
        "NO",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "NUM_TYPE",
        "enum('INTEGER','DOUBLE')",
        "utf8mb4_0900_ai_ci",
        "NO",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "UNIT",
        "varchar(63)",
        "utf8mb4_0900_ai_ci",
        "YES",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "DESCRIPTION",
        "varchar(1023)",
        "utf8mb4_0900_ai_ci",
        "YES",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
    };
    static const char *const show_index[] = {
        "setup_metrics",
        "0",
        "PRIMARY",
        "1",
        "NAME",
        NULL,
        NULL,
        NULL,
        NULL,
        "",
        "HASH",
        "",
        "",
        "YES",
        NULL,
    };
    static const char *const information_schema_columns[] = {
        "NAME",
        "1",
        "NO",
        "varchar(63)",
        "PRI",
        NULL,
        "utf8mb4_0900_ai_ci",
        "METER",
        "2",
        "NO",
        "varchar(63)",
        "",
        NULL,
        "utf8mb4_0900_ai_ci",
        "METRIC_TYPE",
        "3",
        "NO",
        "enum('ASYNC COUNTER','ASYNC UPDOWN COUNTER','ASYNC GAUGE COUNTER')",
        "",
        NULL,
        "utf8mb4_0900_ai_ci",
        "NUM_TYPE",
        "4",
        "NO",
        "enum('INTEGER','DOUBLE')",
        "",
        NULL,
        "utf8mb4_0900_ai_ci",
        "UNIT",
        "5",
        "YES",
        "varchar(63)",
        "",
        NULL,
        "utf8mb4_0900_ai_ci",
        "DESCRIPTION",
        "6",
        "YES",
        "varchar(1023)",
        "",
        NULL,
        "utf8mb4_0900_ai_ci",
    };
    static const char *const information_schema_statistics[] = {
        "PRIMARY",
        "0",
        "1",
        "NAME",
        NULL,
        NULL,
        "HASH",
    };
    static const char *const information_schema_tables[] = {
        "setup_metrics",
        "BASE TABLE",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        "422",
        NULL,
        "utf8mb4_0900_ai_ci",
    };
    static const char *const information_schema_constraints[] = {
        "PRIMARY",
        "PRIMARY KEY",
        "YES",
    };
    static const char *const information_schema_key_usage[] = {
        "PRIMARY",
        "NAME",
        "1",
    };
    static const char *const information_schema_constraint_extensions[] = {
        "performance_schema",
        "setup_metrics",
        "PRIMARY",
        NULL,
        NULL,
    };
    static const struct expected_sql_error access_denied = {
        .code = mysql_error_access_denied,
        .sqlstate = "42000",
        .message_part = "Access denied",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_test_open_temporary(&database), MYLITE_OK, "open database");
    if (failures != 0) {
        return failures;
    }

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM performance_schema.setup_metrics",
            .column_names = count_column,
            .values = count_422,
            .column_count = 1U,
            .row_count = 1U,
            .context = "setup_metrics row count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM performance_schema.setup_metrics "
                   "WHERE NAME = 'update'",
            .column_names = count_column,
            .values = count_two,
            .column_count = 1U,
            .row_count = 1U,
            .context = "setup_metrics duplicate names",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT NAME, METER, METRIC_TYPE, NUM_TYPE, UNIT, DESCRIPTION "
                   "FROM performance_schema.setup_metrics "
                   "WHERE (METER = 'mysql.inno' "
                   "AND NAME IN ('dblwr_pages_written','os_log_written')) "
                   "OR (METER = 'mysql.inno.buffer_pool' AND NAME = 'wait_free') "
                   "OR (METER = 'mysql.inno.data' AND NAME = 'fsyncs') "
                   "OR (METER = 'mysql.myisam' AND NAME = 'key_writes') "
                   "OR (METER = 'mysql.perf_schema' AND NAME = 'users_lost') "
                   "OR (METER = 'mysql.stats' AND NAME = 'slow_queries') "
                   "OR (METER = 'mysql.stats.com' AND NAME = 'stmt_reprepare') "
                   "OR (METER = 'mysql.stats.connection' AND NAME = 'errors_tcpwrap') "
                   "OR (METER = 'mysql.stats.handler' AND NAME = 'update') "
                   "OR (METER = 'mysql.stats.ssl' AND NAME = 'callback_cache_hits') "
                   "OR (METER = 'mysql.x' AND NAME = 'ssl_finished_accepts') "
                   "OR (METER = 'mysql.x.stmt' AND NAME = 'list_clients') "
                   "ORDER BY METER, NAME",
            .column_names = setup_metric_columns,
            .values = setup_metric_samples,
            .column_count = setup_metrics_column_count,
            .row_count = setup_metrics_sample_row_count,
            .context = "setup_metrics representative rows",
        }
    );
    failures += execute_ok(database, "USE performance_schema");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT NUM_TYPE FROM setup_metrics "
                   "WHERE METER = 'mysql.x' AND NAME = 'ssl_finished_accepts'",
            .column_names = (const char *const[]){"NUM_TYPE"},
            .values = selected_schema_num_type,
            .column_count = 1U,
            .row_count = 1U,
            .context = "selected schema setup_metrics",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW FULL COLUMNS FROM performance_schema.setup_metrics",
            .column_names = show_full_columns_columns,
            .values = full_columns,
            .column_count = show_full_columns_column_count,
            .row_count = setup_metrics_column_count,
            .context = "setup_metrics show full columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM performance_schema.setup_metrics",
            .column_names = show_index_columns,
            .values = show_index,
            .column_count = show_index_column_count,
            .row_count = 1U,
            .context = "setup_metrics show index",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW TABLE STATUS FROM performance_schema "
                   "WHERE Name = 'setup_metrics' "
                   "AND Engine = 'PERFORMANCE_SCHEMA' "
                   "AND Row_format = 'Dynamic' "
                   "AND Auto_increment IS NULL "
                   "AND Collation = 'utf8mb4_0900_ai_ci'",
            .column_names = show_table_status_columns,
            .values = NULL,
            .column_count = show_table_status_column_count,
            .row_count = 1U,
            .context = "setup_metrics show table status",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, COLUMN_TYPE, "
                   "COLUMN_KEY, COLUMN_DEFAULT, COLLATION_NAME "
                   "FROM information_schema.columns "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME = 'setup_metrics' "
                   "ORDER BY ORDINAL_POSITION",
            .column_names = information_schema_columns_columns,
            .values = information_schema_columns,
            .column_count = information_schema_columns_projection_count,
            .row_count = setup_metrics_column_count,
            .context = "setup_metrics information schema columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, ROW_FORMAT, TABLE_ROWS, "
                   "AUTO_INCREMENT, TABLE_COLLATION "
                   "FROM information_schema.tables "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME = 'setup_metrics'",
            .column_names = information_schema_tables_columns,
            .values = information_schema_tables,
            .column_count = information_schema_tables_projection_count,
            .row_count = 1U,
            .context = "setup_metrics information schema tables",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME, "
                   "COLLATION, CARDINALITY, INDEX_TYPE "
                   "FROM information_schema.statistics "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME = 'setup_metrics'",
            .column_names = information_schema_statistics_columns,
            .values = information_schema_statistics,
            .column_count = information_schema_statistics_projection_count,
            .row_count = 1U,
            .context = "setup_metrics information schema statistics",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED "
                   "FROM information_schema.table_constraints "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME = 'setup_metrics'",
            .column_names = information_schema_constraints_columns,
            .values = information_schema_constraints,
            .column_count = information_schema_constraints_projection_count,
            .row_count = 1U,
            .context = "setup_metrics information schema constraints",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION "
                   "FROM information_schema.key_column_usage "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME = 'setup_metrics'",
            .column_names = information_schema_key_usage_columns,
            .values = information_schema_key_usage,
            .column_count = information_schema_key_usage_projection_count,
            .row_count = 1U,
            .context = "setup_metrics information schema key usage",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONSTRAINT_SCHEMA, TABLE_NAME, CONSTRAINT_NAME, "
                   "ENGINE_ATTRIBUTE, SECONDARY_ENGINE_ATTRIBUTE "
                   "FROM information_schema.table_constraints_extensions "
                   "WHERE CONSTRAINT_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME = 'setup_metrics'",
            .column_names = information_schema_constraint_extensions_columns,
            .values = information_schema_constraint_extensions,
            .column_count = information_schema_constraint_extensions_projection_count,
            .row_count = 1U,
            .context = "setup_metrics information schema constraint extensions",
        }
    );
    failures += expect_rowset_hash(
        database,
        "SELECT NAME, METER, METRIC_TYPE, NUM_TYPE, UNIT, DESCRIPTION "
        "FROM performance_schema.setup_metrics "
        "ORDER BY METER, NAME, DESCRIPTION",
        MYLITE_TEST_SETUP_METRICS_ROW_HASH,
        "setup_metrics full row hash"
    );
    failures += execute_error(
        database,
        "UPDATE performance_schema.setup_metrics SET UNIT = 'x' WHERE NAME = 'slow_queries'",
        access_denied
    );
    failures += expect_row_count_state(database, "row count after denied setup_metrics update");

    mylite_close(database);
    return failures == 0 ? 0 : 1;
}

static int execute_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    failures += expect_int(rc, MYLITE_OK, sql);
    if (rc == MYLITE_OK) {
        failures += expect_int(mylite_errcode(database), 0, sql);
        failures += expect_size(mylite_result_column_count(result), 0U, sql);
    }
    mylite_result_free(result);
    return failures;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    failures += expect_int(rc, MYLITE_ERROR, sql);
    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, query.sql, strlen(query.sql), &result);
    int failures = 0;

    failures += expect_int(rc, MYLITE_OK, query.context);
    if (rc == MYLITE_OK) {
        failures += expect_int(mylite_errcode(database), 0, query.context);
        failures +=
            expect_size(mylite_result_column_count(result), query.column_count, query.context);
        failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
        for (size_t column_index = 0U;
             column_index < query.column_count && column_index < mylite_result_column_count(result);
             ++column_index) {
            failures += expect_text(
                mylite_result_column_name(result, column_index),
                query.column_names[column_index],
                query.context
            );
        }
        if (query.values != NULL) {
            for (size_t row_index = 0U; row_index < query.row_count; ++row_index) {
                for (size_t column_index = 0U; column_index < query.column_count; ++column_index) {
                    size_t value_index = (row_index * query.column_count) + column_index;
                    failures += expect_text(
                        mylite_result_value_text(result, row_index, column_index),
                        query.values[value_index],
                        query.context
                    );
                }
            }
        }
    }
    mylite_result_free(result);
    return failures;
}

static int expect_rowset_hash(
    mylite_db *database,
    const char *sql,
    uint64_t expected_hash,
    const char *context
) {
    enum { hash_column_count = setup_metrics_column_count };

    mylite_result *result = NULL;
    uint64_t hash = MYLITE_TEST_FNV1A_OFFSET_BASIS;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    failures += expect_int(rc, MYLITE_OK, context);
    if (rc == MYLITE_OK) {
        failures += expect_size(mylite_result_column_count(result), hash_column_count, context);
        failures += expect_size(mylite_result_row_count(result), setup_metrics_row_count, context);
        for (size_t row = 0U; row < mylite_result_row_count(result); ++row) {
            for (size_t column = 0U; column < mylite_result_column_count(result); ++column) {
                const char *value = mylite_result_value_text(result, row, column);

                hash = fnv1a_update_text(hash, value == NULL ? "<NULL>" : value);
                if (column + 1U < mylite_result_column_count(result)) {
                    hash = fnv1a_update_byte(hash, '\t');
                }
            }
            hash = fnv1a_update_byte(hash, '\n');
        }
        failures += expect_uint64(hash, expected_hash, context);
    }
    mylite_result_free(result);
    return failures;
}

static int expect_row_count_state(mylite_db *database, const char *context) {
    static const char *const row_count_columns[] = {"ROW_COUNT()"};
    static const char *const row_count_minus_one[] = {"-1"};

    return expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT()",
            .column_names = row_count_columns,
            .values = row_count_minus_one,
            .column_count = 1U,
            .row_count = 1U,
            .context = context,
        }
    );
}

static uint64_t fnv1a_update_text(uint64_t hash, const char *text) {
    const unsigned char *cursor = (const unsigned char *)text;

    while (*cursor != '\0') {
        hash = fnv1a_update_byte(hash, *cursor);
        ++cursor;
    }
    return hash;
}

static uint64_t fnv1a_update_byte(uint64_t hash, unsigned char byte) {
    hash ^= byte;
    return hash * MYLITE_TEST_FNV1A_PRIME;
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
    return 1;
}

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
    return 1;
}

static int expect_uint64(uint64_t actual, uint64_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected 0x%016" PRIx64 ", got 0x%016" PRIx64 "\n",
        context,
        expected,
        actual
    );
    return 1;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
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

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual != NULL && strstr(actual, needle) != NULL) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected [%s] to contain [%s]\n",
        context,
        actual == NULL ? "NULL" : actual,
        needle
    );
    return 1;
}
