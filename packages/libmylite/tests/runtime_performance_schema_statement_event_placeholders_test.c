#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    mysql_error_access_denied = 1044,
    statement_table_count = 10,
    count_column_count = 1,
    information_schema_columns_projection_count = 6,
    information_schema_columns_row_count = 14,
    information_schema_statistics_projection_count = 6,
    information_schema_statistics_row_count = 20,
    information_schema_constraints_projection_count = 4,
    information_schema_constraints_row_count = 10,
    information_schema_key_usage_projection_count = 4,
    information_schema_key_usage_row_count = 20,
    information_schema_tables_projection_count = 5,
    show_columns_column_count = 6,
    show_columns_current_row_count = 46,
    show_index_column_count = 15,
    show_index_digest_row_count = 2,
    show_table_status_column_count = 18,
    statement_query_buffer_size = 256,
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

static int test_performance_schema_statement_event_placeholders(void);
static int execute_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query query);

int main(void) {
    return test_performance_schema_statement_event_placeholders() == 0 ? 0 : 1;
}

static int test_performance_schema_statement_event_placeholders(void) {
    static const char *const statement_tables[] = {
        "events_statements_current",
        "events_statements_histogram_by_digest",
        "events_statements_histogram_global",
        "events_statements_history",
        "events_statements_summary_by_account_by_event_name",
        "events_statements_summary_by_digest",
        "events_statements_summary_by_host_by_event_name",
        "events_statements_summary_by_thread_by_event_name",
        "events_statements_summary_by_user_by_event_name",
        "events_statements_summary_global_by_event_name",
    };
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const zero_count[] = {"0"};
    static const char *const information_schema_columns_columns[] = {
        "TABLE_NAME",
        "COLUMN_NAME",
        "ORDINAL_POSITION",
        "COLUMN_TYPE",
        "COLUMN_KEY",
        "IS_NULLABLE",
    };
    static const char *const information_schema_columns_rows[] = {
        "events_statements_current",
        "THREAD_ID",
        "1",
        "bigint unsigned",
        "PRI",
        "NO",
        "events_statements_current",
        "EXECUTION_ENGINE",
        "46",
        "enum('PRIMARY','SECONDARY')",
        "",
        "YES",
        "events_statements_histogram_by_digest",
        "SCHEMA_NAME",
        "1",
        "varchar(64)",
        "MUL",
        "YES",
        "events_statements_histogram_by_digest",
        "BUCKET_QUANTILE",
        "8",
        "double(7,6)",
        "",
        "NO",
        "events_statements_histogram_global",
        "BUCKET_NUMBER",
        "1",
        "int unsigned",
        "PRI",
        "NO",
        "events_statements_history",
        "SQL_TEXT",
        "10",
        "longtext",
        "",
        "YES",
        "events_statements_summary_by_account_by_event_name",
        "USER",
        "1",
        "char(32)",
        "MUL",
        "YES",
        "events_statements_summary_by_account_by_event_name",
        "COUNT_SECONDARY",
        "31",
        "bigint unsigned",
        "",
        "NO",
        "events_statements_summary_by_digest",
        "FIRST_SEEN",
        "32",
        "timestamp(6)",
        "",
        "NO",
        "events_statements_summary_by_digest",
        "QUERY_SAMPLE_TEXT",
        "37",
        "longtext",
        "",
        "YES",
        "events_statements_summary_by_host_by_event_name",
        "HOST",
        "1",
        "char(255)",
        "MUL",
        "YES",
        "events_statements_summary_by_thread_by_event_name",
        "THREAD_ID",
        "1",
        "bigint unsigned",
        "PRI",
        "NO",
        "events_statements_summary_by_user_by_event_name",
        "USER",
        "1",
        "char(32)",
        "MUL",
        "YES",
        "events_statements_summary_global_by_event_name",
        "EVENT_NAME",
        "1",
        "varchar(128)",
        "PRI",
        "NO",
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
        "events_statements_current",
        "PRIMARY",
        "0",
        "1",
        "THREAD_ID",
        "HASH",
        "events_statements_current",
        "PRIMARY",
        "0",
        "2",
        "EVENT_ID",
        "HASH",
        "events_statements_histogram_by_digest",
        "SCHEMA_NAME",
        "0",
        "1",
        "SCHEMA_NAME",
        "HASH",
        "events_statements_histogram_by_digest",
        "SCHEMA_NAME",
        "0",
        "2",
        "DIGEST",
        "HASH",
        "events_statements_histogram_by_digest",
        "SCHEMA_NAME",
        "0",
        "3",
        "BUCKET_NUMBER",
        "HASH",
        "events_statements_histogram_global",
        "PRIMARY",
        "0",
        "1",
        "BUCKET_NUMBER",
        "HASH",
        "events_statements_history",
        "PRIMARY",
        "0",
        "1",
        "THREAD_ID",
        "HASH",
        "events_statements_history",
        "PRIMARY",
        "0",
        "2",
        "EVENT_ID",
        "HASH",
        "events_statements_summary_by_account_by_event_name",
        "ACCOUNT",
        "0",
        "1",
        "USER",
        "HASH",
        "events_statements_summary_by_account_by_event_name",
        "ACCOUNT",
        "0",
        "2",
        "HOST",
        "HASH",
        "events_statements_summary_by_account_by_event_name",
        "ACCOUNT",
        "0",
        "3",
        "EVENT_NAME",
        "HASH",
        "events_statements_summary_by_digest",
        "SCHEMA_NAME",
        "0",
        "1",
        "SCHEMA_NAME",
        "HASH",
        "events_statements_summary_by_digest",
        "SCHEMA_NAME",
        "0",
        "2",
        "DIGEST",
        "HASH",
        "events_statements_summary_by_host_by_event_name",
        "HOST",
        "0",
        "1",
        "HOST",
        "HASH",
        "events_statements_summary_by_host_by_event_name",
        "HOST",
        "0",
        "2",
        "EVENT_NAME",
        "HASH",
        "events_statements_summary_by_thread_by_event_name",
        "PRIMARY",
        "0",
        "1",
        "THREAD_ID",
        "HASH",
        "events_statements_summary_by_thread_by_event_name",
        "PRIMARY",
        "0",
        "2",
        "EVENT_NAME",
        "HASH",
        "events_statements_summary_by_user_by_event_name",
        "USER",
        "0",
        "1",
        "USER",
        "HASH",
        "events_statements_summary_by_user_by_event_name",
        "USER",
        "0",
        "2",
        "EVENT_NAME",
        "HASH",
        "events_statements_summary_global_by_event_name",
        "PRIMARY",
        "0",
        "1",
        "EVENT_NAME",
        "HASH",
    };
    static const char *const information_schema_constraints_columns[] = {
        "TABLE_NAME",
        "CONSTRAINT_NAME",
        "CONSTRAINT_TYPE",
        "ENFORCED",
    };
    static const char *const information_schema_constraints_rows[] = {
        "events_statements_current",
        "PRIMARY",
        "PRIMARY KEY",
        "YES",
        "events_statements_histogram_by_digest",
        "SCHEMA_NAME",
        "UNIQUE",
        "YES",
        "events_statements_histogram_global",
        "PRIMARY",
        "PRIMARY KEY",
        "YES",
        "events_statements_history",
        "PRIMARY",
        "PRIMARY KEY",
        "YES",
        "events_statements_summary_by_account_by_event_name",
        "ACCOUNT",
        "UNIQUE",
        "YES",
        "events_statements_summary_by_digest",
        "SCHEMA_NAME",
        "UNIQUE",
        "YES",
        "events_statements_summary_by_host_by_event_name",
        "HOST",
        "UNIQUE",
        "YES",
        "events_statements_summary_by_thread_by_event_name",
        "PRIMARY",
        "PRIMARY KEY",
        "YES",
        "events_statements_summary_by_user_by_event_name",
        "USER",
        "UNIQUE",
        "YES",
        "events_statements_summary_global_by_event_name",
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
        "events_statements_current",
        "PRIMARY",
        "THREAD_ID",
        "1",
        "events_statements_current",
        "PRIMARY",
        "EVENT_ID",
        "2",
        "events_statements_histogram_by_digest",
        "SCHEMA_NAME",
        "SCHEMA_NAME",
        "1",
        "events_statements_histogram_by_digest",
        "SCHEMA_NAME",
        "DIGEST",
        "2",
        "events_statements_histogram_by_digest",
        "SCHEMA_NAME",
        "BUCKET_NUMBER",
        "3",
        "events_statements_histogram_global",
        "PRIMARY",
        "BUCKET_NUMBER",
        "1",
        "events_statements_history",
        "PRIMARY",
        "THREAD_ID",
        "1",
        "events_statements_history",
        "PRIMARY",
        "EVENT_ID",
        "2",
        "events_statements_summary_by_account_by_event_name",
        "ACCOUNT",
        "USER",
        "1",
        "events_statements_summary_by_account_by_event_name",
        "ACCOUNT",
        "HOST",
        "2",
        "events_statements_summary_by_account_by_event_name",
        "ACCOUNT",
        "EVENT_NAME",
        "3",
        "events_statements_summary_by_digest",
        "SCHEMA_NAME",
        "SCHEMA_NAME",
        "1",
        "events_statements_summary_by_digest",
        "SCHEMA_NAME",
        "DIGEST",
        "2",
        "events_statements_summary_by_host_by_event_name",
        "HOST",
        "HOST",
        "1",
        "events_statements_summary_by_host_by_event_name",
        "HOST",
        "EVENT_NAME",
        "2",
        "events_statements_summary_by_thread_by_event_name",
        "PRIMARY",
        "THREAD_ID",
        "1",
        "events_statements_summary_by_thread_by_event_name",
        "PRIMARY",
        "EVENT_NAME",
        "2",
        "events_statements_summary_by_user_by_event_name",
        "USER",
        "USER",
        "1",
        "events_statements_summary_by_user_by_event_name",
        "USER",
        "EVENT_NAME",
        "2",
        "events_statements_summary_global_by_event_name",
        "PRIMARY",
        "EVENT_NAME",
        "1",
    };
    static const char *const information_schema_tables_columns[] = {
        "TABLE_NAME",
        "ENGINE",
        "ROW_FORMAT",
        "TABLE_ROWS",
        "TABLE_COLLATION",
    };
    static const char *const information_schema_tables_rows[] = {
        "events_statements_current",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        "2560",
        "utf8mb4_0900_ai_ci",
        "events_statements_histogram_by_digest",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        "10000",
        "utf8mb4_0900_ai_ci",
        "events_statements_histogram_global",
        "PERFORMANCE_SCHEMA",
        "Fixed",
        "450",
        "utf8mb4_0900_ai_ci",
        "events_statements_history",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        "2560",
        "utf8mb4_0900_ai_ci",
        "events_statements_summary_by_account_by_event_name",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        "28160",
        "utf8mb4_0900_ai_ci",
        "events_statements_summary_by_digest",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        "10000",
        "utf8mb4_0900_ai_ci",
        "events_statements_summary_by_host_by_event_name",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        "28160",
        "utf8mb4_0900_ai_ci",
        "events_statements_summary_by_thread_by_event_name",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        "56320",
        "utf8mb4_0900_ai_ci",
        "events_statements_summary_by_user_by_event_name",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        "28160",
        "utf8mb4_0900_ai_ci",
        "events_statements_summary_global_by_event_name",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        "220",
        "utf8mb4_0900_ai_ci",
    };
    static const char *const show_columns_columns[] = {
        "Field",
        "Type",
        "Null",
        "Key",
        "Default",
        "Extra",
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
    static const char *const show_digest_index_rows[] = {
        "events_statements_summary_by_digest",
        "0",
        "SCHEMA_NAME",
        "1",
        "SCHEMA_NAME",
        NULL,
        NULL,
        NULL,
        NULL,
        "YES",
        "HASH",
        "",
        "",
        "YES",
        NULL,
        "events_statements_summary_by_digest",
        "0",
        "SCHEMA_NAME",
        "2",
        "DIGEST",
        NULL,
        NULL,
        NULL,
        NULL,
        "YES",
        "HASH",
        "",
        "",
        "YES",
        NULL,
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

    if (mylite_test_open_temporary(&database) != MYLITE_OK) {
        fprintf(stderr, "failed to open temporary database\n");
        return 1;
    }

    for (size_t i = 0U; i < statement_table_count; ++i) {
        char sql[statement_query_buffer_size];
        int written = snprintf(
            sql,
            sizeof(sql),
            "SELECT COUNT(*) FROM performance_schema.%s",
            statement_tables[i]
        );

        if (written < 0 || (size_t)written >= sizeof(sql)) {
            fprintf(stderr, "statement table SQL buffer too small\n");
            ++failures;
            continue;
        }
        failures += expect_query(
            database,
            (struct expected_query){
                .sql = sql,
                .column_names = count_column,
                .values = zero_count,
                .column_count = count_column_count,
                .row_count = 1U,
                .context = statement_tables[i],
            }
        );
    }
    failures += execute_ok(database, "USE performance_schema");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM events_statements_current",
            .column_names = count_column,
            .values = zero_count,
            .column_count = count_column_count,
            .row_count = 1U,
            .context = "selected events_statements_current count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM performance_schema.events_statements_current",
            .column_names = show_columns_columns,
            .values = NULL,
            .column_count = show_columns_column_count,
            .row_count = show_columns_current_row_count,
            .context = "show events_statements_current columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM performance_schema.events_statements_summary_by_digest",
            .column_names = show_index_columns,
            .values = show_digest_index_rows,
            .column_count = show_index_column_count,
            .row_count = show_index_digest_row_count,
            .context = "show events_statements_summary_by_digest index",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW TABLE STATUS FROM performance_schema "
                   "LIKE 'events_statements_histogram_global'",
            .column_names = show_table_status_columns,
            .values = NULL,
            .column_count = show_table_status_column_count,
            .row_count = 1U,
            .context = "show events_statements_histogram_global table status",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, COLUMN_TYPE, "
                   "COLUMN_KEY, IS_NULLABLE "
                   "FROM information_schema.columns "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND ((TABLE_NAME = 'events_statements_current' "
                   "AND COLUMN_NAME IN ('THREAD_ID','EXECUTION_ENGINE')) "
                   "OR (TABLE_NAME = 'events_statements_histogram_by_digest' "
                   "AND COLUMN_NAME IN ('SCHEMA_NAME','BUCKET_QUANTILE')) "
                   "OR (TABLE_NAME = 'events_statements_histogram_global' "
                   "AND COLUMN_NAME = 'BUCKET_NUMBER') "
                   "OR (TABLE_NAME = 'events_statements_history' "
                   "AND COLUMN_NAME = 'SQL_TEXT') "
                   "OR (TABLE_NAME = 'events_statements_summary_by_account_by_event_name' "
                   "AND COLUMN_NAME IN ('USER','COUNT_SECONDARY')) "
                   "OR (TABLE_NAME = 'events_statements_summary_by_digest' "
                   "AND COLUMN_NAME IN ('FIRST_SEEN','QUERY_SAMPLE_TEXT')) "
                   "OR (TABLE_NAME = 'events_statements_summary_by_host_by_event_name' "
                   "AND COLUMN_NAME = 'HOST') "
                   "OR (TABLE_NAME = 'events_statements_summary_by_thread_by_event_name' "
                   "AND COLUMN_NAME = 'THREAD_ID') "
                   "OR (TABLE_NAME = 'events_statements_summary_by_user_by_event_name' "
                   "AND COLUMN_NAME = 'USER') "
                   "OR (TABLE_NAME = 'events_statements_summary_global_by_event_name' "
                   "AND COLUMN_NAME = 'EVENT_NAME')) "
                   "ORDER BY TABLE_NAME, ORDINAL_POSITION",
            .column_names = information_schema_columns_columns,
            .values = information_schema_columns_rows,
            .column_count = information_schema_columns_projection_count,
            .row_count = information_schema_columns_row_count,
            .context = "information schema statement columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME, "
                   "INDEX_TYPE "
                   "FROM information_schema.statistics "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME IN ("
                   "'events_statements_current',"
                   "'events_statements_histogram_by_digest',"
                   "'events_statements_histogram_global',"
                   "'events_statements_history',"
                   "'events_statements_summary_by_account_by_event_name',"
                   "'events_statements_summary_by_digest',"
                   "'events_statements_summary_by_host_by_event_name',"
                   "'events_statements_summary_by_thread_by_event_name',"
                   "'events_statements_summary_by_user_by_event_name',"
                   "'events_statements_summary_global_by_event_name') "
                   "ORDER BY TABLE_NAME, INDEX_NAME, SEQ_IN_INDEX",
            .column_names = information_schema_statistics_columns,
            .values = information_schema_statistics_rows,
            .column_count = information_schema_statistics_projection_count,
            .row_count = information_schema_statistics_row_count,
            .context = "information schema statement statistics",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED "
                   "FROM information_schema.table_constraints "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME IN ("
                   "'events_statements_current',"
                   "'events_statements_histogram_by_digest',"
                   "'events_statements_histogram_global',"
                   "'events_statements_history',"
                   "'events_statements_summary_by_account_by_event_name',"
                   "'events_statements_summary_by_digest',"
                   "'events_statements_summary_by_host_by_event_name',"
                   "'events_statements_summary_by_thread_by_event_name',"
                   "'events_statements_summary_by_user_by_event_name',"
                   "'events_statements_summary_global_by_event_name') "
                   "ORDER BY TABLE_NAME, CONSTRAINT_NAME",
            .column_names = information_schema_constraints_columns,
            .values = information_schema_constraints_rows,
            .column_count = information_schema_constraints_projection_count,
            .row_count = information_schema_constraints_row_count,
            .context = "information schema statement constraints",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION "
                   "FROM information_schema.key_column_usage "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME IN ("
                   "'events_statements_current',"
                   "'events_statements_histogram_by_digest',"
                   "'events_statements_histogram_global',"
                   "'events_statements_history',"
                   "'events_statements_summary_by_account_by_event_name',"
                   "'events_statements_summary_by_digest',"
                   "'events_statements_summary_by_host_by_event_name',"
                   "'events_statements_summary_by_thread_by_event_name',"
                   "'events_statements_summary_by_user_by_event_name',"
                   "'events_statements_summary_global_by_event_name') "
                   "ORDER BY TABLE_NAME, CONSTRAINT_NAME, ORDINAL_POSITION",
            .column_names = information_schema_key_usage_columns,
            .values = information_schema_key_usage_rows,
            .column_count = information_schema_key_usage_projection_count,
            .row_count = information_schema_key_usage_row_count,
            .context = "information schema statement key usage",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, ENGINE, ROW_FORMAT, TABLE_ROWS, TABLE_COLLATION "
                   "FROM information_schema.tables "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME IN ("
                   "'events_statements_current',"
                   "'events_statements_histogram_by_digest',"
                   "'events_statements_histogram_global',"
                   "'events_statements_history',"
                   "'events_statements_summary_by_account_by_event_name',"
                   "'events_statements_summary_by_digest',"
                   "'events_statements_summary_by_host_by_event_name',"
                   "'events_statements_summary_by_thread_by_event_name',"
                   "'events_statements_summary_by_user_by_event_name',"
                   "'events_statements_summary_global_by_event_name') "
                   "ORDER BY TABLE_NAME",
            .column_names = information_schema_tables_columns,
            .values = information_schema_tables_rows,
            .column_count = information_schema_tables_projection_count,
            .row_count = statement_table_count,
            .context = "information schema statement tables",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO performance_schema.events_statements_current (THREAD_ID) VALUES (1)",
        (struct expected_sql_error){
            .code = mysql_error_access_denied,
            .sqlstate = "42000",
            .message_part = "Access denied",
        }
    );

    mylite_close(database);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = mylite_test_expect_int(rc, MYLITE_OK, sql);

    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: %s / %s\n", sql, mylite_sqlstate(database), mylite_errmsg(database));
    }
    if (result != NULL) {
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
    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, "error code");
    failures +=
        mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, "error sqlstate");
    failures += mylite_test_expect_contains(
        mylite_errmsg(database),
        expected.message_part,
        "error message"
    );
    mylite_result_free(result);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, query.sql, strlen(query.sql), &result);
    int failures = 0;

    failures += mylite_test_expect_int(rc, MYLITE_OK, query.context);
    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: %s / %s\n",
            query.context,
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
    }
    if (rc == MYLITE_OK) {
        failures += mylite_test_expect_size(
            mylite_result_column_count(result),
            query.column_count,
            query.context
        );
        failures += mylite_test_expect_size(
            mylite_result_row_count(result),
            query.row_count,
            query.context
        );
        failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, query.context);
        for (size_t column = 0U; column < query.column_count; ++column) {
            failures += mylite_test_expect_text(
                mylite_result_column_name(result, column),
                query.column_names[column],
                query.context
            );
        }
        if (query.values != NULL) {
            for (size_t row = 0U; row < query.row_count; ++row) {
                for (size_t column = 0U; column < query.column_count; ++column) {
                    size_t value_index = (row * query.column_count) + column;
                    failures += mylite_test_expect_text(
                        mylite_result_value_text(result, row, column),
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
