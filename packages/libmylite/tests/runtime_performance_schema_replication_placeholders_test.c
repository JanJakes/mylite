#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdio.h>
#include <string.h>

enum {
    count_projection_count = 1,
    information_schema_columns_projection_count = 7,
    information_schema_columns_row_count = 8,
    information_schema_constraint_extensions_projection_count = 4,
    information_schema_constraint_extensions_row_count = 9,
    information_schema_constraints_projection_count = 4,
    information_schema_constraints_row_count = 6,
    information_schema_key_usage_projection_count = 4,
    information_schema_key_usage_row_count = 7,
    information_schema_statistics_projection_count = 6,
    information_schema_statistics_row_count = 10,
    information_schema_tables_projection_count = 5,
    information_schema_tables_row_count = 7,
    show_columns_column_count = 6,
    show_connection_configuration_row_count = 27,
    show_failover_managed_row_count = 4,
    show_index_column_count = 15,
    show_index_worker_row_count = 3,
    show_table_status_column_count = 18,
    mysql_error_access_denied = 1044,
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

static int test_performance_schema_replication_placeholders(void);
static int execute_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query query);
static int expect_empty_count(mylite_db *database, const char *sql, const char *context);
static int expect_row_count_state(mylite_db *database, const char *context);

int main(void) {
    return test_performance_schema_replication_placeholders() == 0 ? 0 : 1;
}

static int test_performance_schema_replication_placeholders(void) {
    static const char *const count_columns[] = {"COUNT(*)"};
    static const char *const zero_count_rows[] = {"0"};
    static const char *const count_4_rows[] = {"4"};
    static const char *const count_7_rows[] = {"7"};
    static const char *const count_24_rows[] = {"24"};
    static const char *const count_27_rows[] = {"27"};
    static const char *const show_columns_columns[] = {
        "Field",
        "Type",
        "Null",
        "Key",
        "Default",
        "Extra",
    };
    static const char *const failover_managed_values[] = {
        "CHANNEL_NAME",  "char(64)", "NO",  "", NULL, "",
        "MANAGED_NAME",  "char(64)", "NO",  "", "",   "",
        "MANAGED_TYPE",  "char(64)", "NO",  "", "",   "",
        "CONFIGURATION", "json",     "YES", "", NULL, "",
    };
    static const char *const information_schema_columns_columns[] = {
        "TABLE_NAME",
        "COLUMN_NAME",
        "COLUMN_TYPE",
        "COLUMN_KEY",
        "IS_NULLABLE",
        "COLLATION_NAME",
        "COLUMN_DEFAULT",
    };
    static const char *const information_schema_columns_rows[] = {
        "replication_applier_configuration",
        "PRIVILEGE_CHECKS_USER",
        "text",
        "",
        "YES",
        "utf8mb3_bin",
        NULL,
        "replication_applier_configuration",
        "REQUIRE_TABLE_PRIMARY_KEY_CHECK",
        "enum('STREAM','ON','OFF','GENERATE')",
        "",
        "NO",
        "utf8mb4_0900_ai_ci",
        NULL,
        "replication_applier_filters",
        "COUNTER",
        "bigint unsigned",
        "",
        "NO",
        NULL,
        "0",
        "replication_applier_status_by_worker",
        "WORKER_ID",
        "bigint unsigned",
        "PRI",
        "NO",
        NULL,
        NULL,
        "replication_asynchronous_connection_failover_managed",
        "CONFIGURATION",
        "json",
        "",
        "YES",
        NULL,
        NULL,
        "replication_connection_configuration",
        "HEARTBEAT_INTERVAL",
        "double(10,3)",
        "",
        "NO",
        NULL,
        NULL,
        "replication_connection_status",
        "COUNT_RECEIVED_HEARTBEATS",
        "bigint unsigned",
        "",
        "NO",
        NULL,
        "0",
        "replication_group_members",
        "MEMBER_HOST",
        "char(255)",
        "",
        "NO",
        "ascii_general_ci",
        NULL,
    };
    static const char *const information_schema_statistics_columns[] = {
        "TABLE_NAME",
        "INDEX_NAME",
        "NON_UNIQUE",
        "SEQ_IN_INDEX",
        "COLUMN_NAME",
        "INDEX_TYPE",
    };
    static const char *const information_schema_statistics_rows[] = {
        "replication_applier_configuration",
        "PRIMARY",
        "0",
        "1",
        "CHANNEL_NAME",
        "HASH",
        "replication_applier_status",
        "PRIMARY",
        "0",
        "1",
        "CHANNEL_NAME",
        "HASH",
        "replication_applier_status_by_coordinator",
        "PRIMARY",
        "0",
        "1",
        "CHANNEL_NAME",
        "HASH",
        "replication_applier_status_by_coordinator",
        "THREAD_ID",
        "1",
        "1",
        "THREAD_ID",
        "HASH",
        "replication_applier_status_by_worker",
        "PRIMARY",
        "0",
        "1",
        "CHANNEL_NAME",
        "HASH",
        "replication_applier_status_by_worker",
        "PRIMARY",
        "0",
        "2",
        "WORKER_ID",
        "HASH",
        "replication_applier_status_by_worker",
        "THREAD_ID",
        "1",
        "1",
        "THREAD_ID",
        "HASH",
        "replication_connection_configuration",
        "PRIMARY",
        "0",
        "1",
        "CHANNEL_NAME",
        "HASH",
        "replication_connection_status",
        "PRIMARY",
        "0",
        "1",
        "CHANNEL_NAME",
        "HASH",
        "replication_connection_status",
        "THREAD_ID",
        "1",
        "1",
        "THREAD_ID",
        "HASH",
    };
    static const char *const information_schema_constraints_columns[] = {
        "TABLE_NAME",
        "CONSTRAINT_NAME",
        "CONSTRAINT_TYPE",
        "ENFORCED",
    };
    static const char *const information_schema_constraints_rows[] = {
        "replication_applier_configuration",
        "PRIMARY",
        "PRIMARY KEY",
        "YES",
        "replication_applier_status",
        "PRIMARY",
        "PRIMARY KEY",
        "YES",
        "replication_applier_status_by_coordinator",
        "PRIMARY",
        "PRIMARY KEY",
        "YES",
        "replication_applier_status_by_worker",
        "PRIMARY",
        "PRIMARY KEY",
        "YES",
        "replication_connection_configuration",
        "PRIMARY",
        "PRIMARY KEY",
        "YES",
        "replication_connection_status",
        "PRIMARY",
        "PRIMARY KEY",
        "YES",
    };
    static const char *const information_schema_key_usage_columns[] = {
        "TABLE_NAME",
        "CONSTRAINT_NAME",
        "COLUMN_NAME",
        "ORDINAL_POSITION",
    };
    static const char *const information_schema_key_usage_rows[] = {
        "replication_applier_configuration",
        "PRIMARY",
        "CHANNEL_NAME",
        "1",
        "replication_applier_status",
        "PRIMARY",
        "CHANNEL_NAME",
        "1",
        "replication_applier_status_by_coordinator",
        "PRIMARY",
        "CHANNEL_NAME",
        "1",
        "replication_applier_status_by_worker",
        "PRIMARY",
        "CHANNEL_NAME",
        "1",
        "replication_applier_status_by_worker",
        "PRIMARY",
        "WORKER_ID",
        "2",
        "replication_connection_configuration",
        "PRIMARY",
        "CHANNEL_NAME",
        "1",
        "replication_connection_status",
        "PRIMARY",
        "CHANNEL_NAME",
        "1",
    };
    static const char *const information_schema_constraint_extensions_columns[] = {
        "TABLE_NAME",
        "CONSTRAINT_NAME",
        "ENGINE_ATTRIBUTE",
        "SECONDARY_ENGINE_ATTRIBUTE",
    };
    static const char *const information_schema_constraint_extensions_rows[] = {
        "replication_applier_configuration",
        "PRIMARY",
        NULL,
        NULL,
        "replication_applier_status",
        "PRIMARY",
        NULL,
        NULL,
        "replication_applier_status_by_coordinator",
        "PRIMARY",
        NULL,
        NULL,
        "replication_applier_status_by_coordinator",
        "THREAD_ID",
        NULL,
        NULL,
        "replication_applier_status_by_worker",
        "PRIMARY",
        NULL,
        NULL,
        "replication_applier_status_by_worker",
        "THREAD_ID",
        NULL,
        NULL,
        "replication_connection_configuration",
        "PRIMARY",
        NULL,
        NULL,
        "replication_connection_status",
        "PRIMARY",
        NULL,
        NULL,
        "replication_connection_status",
        "THREAD_ID",
        NULL,
        NULL,
    };
    static const char *const information_schema_tables_columns[] = {
        "TABLE_NAME",
        "ENGINE",
        "ROW_FORMAT",
        "AUTO_INCREMENT",
        "TABLE_COLLATION",
    };
    static const char *const information_schema_tables_rows[] = {
        "replication_applier_configuration",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        NULL,
        "utf8mb4_0900_ai_ci",
        "replication_applier_status",
        "PERFORMANCE_SCHEMA",
        "Fixed",
        NULL,
        "utf8mb4_0900_ai_ci",
        "replication_applier_status_by_worker",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        NULL,
        "utf8mb4_0900_ai_ci",
        "replication_asynchronous_connection_failover",
        "PERFORMANCE_SCHEMA",
        "Fixed",
        NULL,
        "utf8mb4_0900_ai_ci",
        "replication_connection_configuration",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        NULL,
        "utf8mb4_0900_ai_ci",
        "replication_connection_status",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        NULL,
        "utf8mb4_0900_ai_ci",
        "replication_group_members",
        "PERFORMANCE_SCHEMA",
        "Fixed",
        NULL,
        "utf8mb4_0900_ai_ci",
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
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        mylite_test_expect_int(mylite_test_open_temporary(&database), MYLITE_OK, "open database");
    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    failures += expect_empty_count(
        database,
        "SELECT COUNT(*) FROM performance_schema.replication_applier_configuration",
        "replication_applier_configuration row count"
    );
    failures += expect_empty_count(
        database,
        "SELECT COUNT(*) FROM performance_schema.replication_applier_filters",
        "replication_applier_filters row count"
    );
    failures += expect_empty_count(
        database,
        "SELECT COUNT(*) FROM performance_schema.replication_applier_global_filters",
        "replication_applier_global_filters row count"
    );
    failures += expect_empty_count(
        database,
        "SELECT COUNT(*) FROM performance_schema.replication_applier_status",
        "replication_applier_status row count"
    );
    failures += expect_empty_count(
        database,
        "SELECT COUNT(*) FROM performance_schema.replication_applier_status_by_coordinator",
        "replication_applier_status_by_coordinator row count"
    );
    failures += expect_empty_count(
        database,
        "SELECT COUNT(*) FROM performance_schema.replication_applier_status_by_worker",
        "replication_applier_status_by_worker row count"
    );
    failures += expect_empty_count(
        database,
        "SELECT COUNT(*) FROM performance_schema.replication_asynchronous_connection_failover",
        "replication_asynchronous_connection_failover row count"
    );
    failures += expect_empty_count(
        database,
        "SELECT COUNT(*) FROM "
        "performance_schema.replication_asynchronous_connection_failover_managed",
        "replication_asynchronous_connection_failover_managed row count"
    );
    failures += expect_empty_count(
        database,
        "SELECT COUNT(*) FROM performance_schema.replication_connection_configuration",
        "replication_connection_configuration row count"
    );
    failures += expect_empty_count(
        database,
        "SELECT COUNT(*) FROM performance_schema.replication_connection_status",
        "replication_connection_status row count"
    );
    failures += expect_empty_count(
        database,
        "SELECT COUNT(*) FROM performance_schema.replication_group_member_stats",
        "replication_group_member_stats row count"
    );
    failures += expect_empty_count(
        database,
        "SELECT COUNT(*) FROM performance_schema.replication_group_members",
        "replication_group_members row count"
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM performance_schema.replication_connection_configuration",
            .column_names = show_columns_columns,
            .values = NULL,
            .column_count = show_columns_column_count,
            .row_count = show_connection_configuration_row_count,
            .context = "replication_connection_configuration show columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "DESC performance_schema.replication_asynchronous_connection_failover_managed",
            .column_names = show_columns_columns,
            .values = failover_managed_values,
            .column_count = show_columns_column_count,
            .row_count = show_failover_managed_row_count,
            .context = "replication_asynchronous_connection_failover_managed describe",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM information_schema.columns "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME = 'replication_applier_configuration'",
            .column_names = count_columns,
            .values = count_7_rows,
            .column_count = count_projection_count,
            .row_count = 1U,
            .context = "replication_applier_configuration column count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM information_schema.columns "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME = 'replication_applier_status_by_worker'",
            .column_names = count_columns,
            .values = count_24_rows,
            .column_count = count_projection_count,
            .row_count = 1U,
            .context = "replication_applier_status_by_worker column count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM information_schema.columns "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME = 'replication_asynchronous_connection_failover_managed'",
            .column_names = count_columns,
            .values = count_4_rows,
            .column_count = count_projection_count,
            .row_count = 1U,
            .context = "replication_asynchronous_connection_failover_managed column count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM information_schema.columns "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME = 'replication_connection_configuration'",
            .column_names = count_columns,
            .values = count_27_rows,
            .column_count = count_projection_count,
            .row_count = 1U,
            .context = "replication_connection_configuration column count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, COLUMN_TYPE, COLUMN_KEY, "
                   "IS_NULLABLE, COLLATION_NAME, COLUMN_DEFAULT "
                   "FROM information_schema.columns "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME IN ('replication_applier_configuration', "
                   "'replication_applier_filters', "
                   "'replication_applier_status_by_worker', "
                   "'replication_asynchronous_connection_failover_managed', "
                   "'replication_connection_configuration', "
                   "'replication_connection_status', 'replication_group_members') "
                   "AND COLUMN_NAME IN ('PRIVILEGE_CHECKS_USER', "
                   "'REQUIRE_TABLE_PRIMARY_KEY_CHECK', 'COUNTER', 'WORKER_ID', "
                   "'CONFIGURATION', 'HEARTBEAT_INTERVAL', "
                   "'COUNT_RECEIVED_HEARTBEATS', 'MEMBER_HOST') "
                   "ORDER BY TABLE_NAME, ORDINAL_POSITION",
            .column_names = information_schema_columns_columns,
            .values = information_schema_columns_rows,
            .column_count = information_schema_columns_projection_count,
            .row_count = information_schema_columns_row_count,
            .context = "information_schema selected replication columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, "
                   "COLUMN_NAME, INDEX_TYPE "
                   "FROM information_schema.statistics "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME IN ('replication_applier_configuration', "
                   "'replication_applier_status', "
                   "'replication_applier_status_by_coordinator', "
                   "'replication_applier_status_by_worker', "
                   "'replication_connection_configuration', "
                   "'replication_connection_status') "
                   "ORDER BY TABLE_NAME, INDEX_NAME, SEQ_IN_INDEX",
            .column_names = information_schema_statistics_columns,
            .values = information_schema_statistics_rows,
            .column_count = information_schema_statistics_projection_count,
            .row_count = information_schema_statistics_row_count,
            .context = "information_schema replication statistics",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED "
                   "FROM information_schema.table_constraints "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME IN ('replication_applier_configuration', "
                   "'replication_applier_status', "
                   "'replication_applier_status_by_coordinator', "
                   "'replication_applier_status_by_worker', "
                   "'replication_connection_configuration', "
                   "'replication_connection_status') "
                   "ORDER BY TABLE_NAME, CONSTRAINT_NAME",
            .column_names = information_schema_constraints_columns,
            .values = information_schema_constraints_rows,
            .column_count = information_schema_constraints_projection_count,
            .row_count = information_schema_constraints_row_count,
            .context = "information_schema replication constraints",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, CONSTRAINT_NAME, COLUMN_NAME, "
                   "ORDINAL_POSITION "
                   "FROM information_schema.key_column_usage "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME IN ('replication_applier_configuration', "
                   "'replication_applier_status', "
                   "'replication_applier_status_by_coordinator', "
                   "'replication_applier_status_by_worker', "
                   "'replication_connection_configuration', "
                   "'replication_connection_status') "
                   "ORDER BY TABLE_NAME, CONSTRAINT_NAME, ORDINAL_POSITION",
            .column_names = information_schema_key_usage_columns,
            .values = information_schema_key_usage_rows,
            .column_count = information_schema_key_usage_projection_count,
            .row_count = information_schema_key_usage_row_count,
            .context = "information_schema replication key usage",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, CONSTRAINT_NAME, ENGINE_ATTRIBUTE, "
                   "SECONDARY_ENGINE_ATTRIBUTE "
                   "FROM information_schema.table_constraints_extensions "
                   "WHERE CONSTRAINT_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME IN ('replication_applier_configuration', "
                   "'replication_applier_status', "
                   "'replication_applier_status_by_coordinator', "
                   "'replication_applier_status_by_worker', "
                   "'replication_connection_configuration', "
                   "'replication_connection_status') "
                   "ORDER BY TABLE_NAME, CONSTRAINT_NAME",
            .column_names = information_schema_constraint_extensions_columns,
            .values = information_schema_constraint_extensions_rows,
            .column_count = information_schema_constraint_extensions_projection_count,
            .row_count = information_schema_constraint_extensions_row_count,
            .context = "information_schema replication constraint extensions",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, ENGINE, ROW_FORMAT, AUTO_INCREMENT, TABLE_COLLATION "
                   "FROM information_schema.tables "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME IN ('replication_applier_configuration', "
                   "'replication_applier_status', "
                   "'replication_applier_status_by_worker', "
                   "'replication_asynchronous_connection_failover', "
                   "'replication_connection_configuration', "
                   "'replication_connection_status', 'replication_group_members') "
                   "ORDER BY TABLE_NAME",
            .column_names = information_schema_tables_columns,
            .values = information_schema_tables_rows,
            .column_count = information_schema_tables_projection_count,
            .row_count = information_schema_tables_row_count,
            .context = "information_schema replication tables",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM performance_schema.replication_applier_status_by_worker",
            .column_names = show_index_columns,
            .values = NULL,
            .column_count = show_index_column_count,
            .row_count = show_index_worker_row_count,
            .context = "replication_applier_status_by_worker show index",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW TABLE STATUS FROM performance_schema "
                   "WHERE Name = 'replication_group_members' "
                   "AND Engine = 'PERFORMANCE_SCHEMA' AND Row_format = 'Fixed' "
                   "AND Auto_increment IS NULL AND Collation = 'utf8mb4_0900_ai_ci'",
            .column_names = show_table_status_columns,
            .values = NULL,
            .column_count = show_table_status_column_count,
            .row_count = 1U,
            .context = "show table status replication_group_members",
        }
    );
    failures += execute_ok(database, "USE performance_schema");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM replication_connection_status",
            .column_names = count_columns,
            .values = zero_count_rows,
            .column_count = count_projection_count,
            .row_count = 1U,
            .context = "selected performance_schema replication_connection_status row count",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO performance_schema.replication_group_members "
        "VALUES ('channel', 'member', 'host', 3306, 'ONLINE', 'PRIMARY', '8.4.9', 'XCom')",
        (struct expected_sql_error){
            .code = mysql_error_access_denied,
            .sqlstate = "42000",
            .message_part = "Access denied",
        }
    );
    failures += expect_row_count_state(database, "row count after replication placeholder error");

    mylite_close(database);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    failures += mylite_test_expect_int(rc, MYLITE_OK, sql);
    if (rc == MYLITE_OK) {
        failures += mylite_test_expect_size(mylite_result_column_count(result), 0U, sql);
        failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, sql);
    }
    mylite_result_free(result);
    return failures;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    failures += mylite_test_expect_int(rc, MYLITE_ERROR, sql);
    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, sql);
    failures += mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += mylite_test_expect_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, query.sql, strlen(query.sql), &result);
    int failures = 0;

    failures += mylite_test_expect_int(rc, MYLITE_OK, query.context);
    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s failed: %s\n", query.context, mylite_errmsg(database));
        mylite_result_free(result);
        return failures + 1;
    }

    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        query.column_count,
        query.context
    );
    failures +=
        mylite_test_expect_size(mylite_result_row_count(result), query.row_count, query.context);
    if (mylite_result_column_count(result) == query.column_count) {
        for (size_t column = 0U; column < query.column_count; ++column) {
            failures += mylite_test_expect_text(
                mylite_result_column_name(result, column),
                query.column_names[column],
                query.context
            );
        }
    }
    if (query.values != NULL && mylite_result_column_count(result) == query.column_count &&
        mylite_result_row_count(result) == query.row_count) {
        for (size_t row = 0U; row < query.row_count; ++row) {
            for (size_t column = 0U; column < query.column_count; ++column) {
                size_t index = (row * query.column_count) + column;
                failures += mylite_test_expect_text(
                    mylite_result_value_text(result, row, column),
                    query.values[index],
                    query.context
                );
            }
        }
    }

    mylite_result_free(result);
    return failures;
}

static int expect_empty_count(mylite_db *database, const char *sql, const char *context) {
    static const char *const columns[] = {"COUNT(*)"};
    static const char *const values[] = {"0"};

    return expect_query(
        database,
        (struct expected_query){
            .sql = sql,
            .column_names = columns,
            .values = values,
            .column_count = count_projection_count,
            .row_count = 1U,
            .context = context,
        }
    );
}

static int expect_row_count_state(mylite_db *database, const char *context) {
    static const char *const columns[] = {"ROW_COUNT()"};
    static const char *const values[] = {"-1"};

    return expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT()",
            .column_names = columns,
            .values = values,
            .column_count = 1U,
            .row_count = 1U,
            .context = context,
        }
    );
}
