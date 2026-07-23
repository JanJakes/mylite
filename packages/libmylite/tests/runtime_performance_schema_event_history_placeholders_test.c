#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    information_schema_columns_projection_count = 6,
    information_schema_columns_row_count = 11,
    information_schema_statistics_projection_count = 6,
    information_schema_statistics_row_count = 11,
    information_schema_tables_projection_count = 5,
    information_schema_tables_row_count = 8,
    show_columns_column_count = 6,
    show_stages_current_row_count = 12,
    show_index_column_count = 15,
    show_index_program_row_count = 3,
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

static int test_performance_schema_event_history_placeholders(void);
static int execute_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query query);
static int expect_empty_count(mylite_db *database, const char *sql, const char *context);
static int expect_row_count_state(mylite_db *database, const char *context);

int main(void) {
    return test_performance_schema_event_history_placeholders() == 0 ? 0 : 1;
}

static int test_performance_schema_event_history_placeholders(void) {
    static const char *const show_columns_columns[] = {
        "Field",
        "Type",
        "Null",
        "Key",
        "Default",
        "Extra",
    };
    static const char *const stages_current_columns[] = {
        "THREAD_ID",
        "bigint unsigned",
        "NO",
        "PRI",
        NULL,
        "",
        "EVENT_ID",
        "bigint unsigned",
        "NO",
        "PRI",
        NULL,
        "",
        "END_EVENT_ID",
        "bigint unsigned",
        "YES",
        "",
        NULL,
        "",
        "EVENT_NAME",
        "varchar(128)",
        "NO",
        "",
        NULL,
        "",
        "SOURCE",
        "varchar(64)",
        "YES",
        "",
        NULL,
        "",
        "TIMER_START",
        "bigint unsigned",
        "YES",
        "",
        NULL,
        "",
        "TIMER_END",
        "bigint unsigned",
        "YES",
        "",
        NULL,
        "",
        "TIMER_WAIT",
        "bigint unsigned",
        "YES",
        "",
        NULL,
        "",
        "WORK_COMPLETED",
        "bigint unsigned",
        "YES",
        "",
        NULL,
        "",
        "WORK_ESTIMATED",
        "bigint unsigned",
        "YES",
        "",
        NULL,
        "",
        "NESTING_EVENT_ID",
        "bigint unsigned",
        "YES",
        "",
        NULL,
        "",
        "NESTING_EVENT_TYPE",
        "enum('TRANSACTION','STATEMENT','STAGE','WAIT')",
        "YES",
        "",
        NULL,
        "",
    };
    static const char *const information_schema_columns_columns[] = {
        "TABLE_NAME",
        "COLUMN_NAME",
        "ORDINAL_POSITION",
        "COLUMN_TYPE",
        "COLUMN_KEY",
        "IS_NULLABLE",
    };
    static const char *const information_schema_columns_rows[] = {
        "events_stages_current",
        "THREAD_ID",
        "1",
        "bigint unsigned",
        "PRI",
        "NO",
        "events_stages_current",
        "NESTING_EVENT_TYPE",
        "12",
        "enum('TRANSACTION','STATEMENT','STAGE','WAIT')",
        "",
        "YES",
        "events_stages_history_long",
        "THREAD_ID",
        "1",
        "bigint unsigned",
        "",
        "NO",
        "events_statements_history_long",
        "SQL_TEXT",
        "10",
        "longtext",
        "",
        "YES",
        "events_statements_history_long",
        "EXECUTION_ENGINE",
        "46",
        "enum('PRIMARY','SECONDARY')",
        "",
        "YES",
        "events_statements_summary_by_program",
        "OBJECT_TYPE",
        "1",
        "enum('EVENT','FUNCTION','PROCEDURE','TABLE','TRIGGER')",
        "PRI",
        "NO",
        "events_statements_summary_by_program",
        "COUNT_SECONDARY",
        "36",
        "bigint unsigned",
        "",
        "NO",
        "events_waits_current",
        "OBJECT_NAME",
        "11",
        "varchar(512)",
        "",
        "YES",
        "events_waits_current",
        "NUMBER_OF_BYTES",
        "18",
        "bigint",
        "",
        "YES",
        "events_waits_history",
        "EVENT_ID",
        "2",
        "bigint unsigned",
        "PRI",
        "NO",
        "events_waits_history_long",
        "THREAD_ID",
        "1",
        "bigint unsigned",
        "",
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
        "events_stages_current",
        "PRIMARY",
        "0",
        "1",
        "THREAD_ID",
        "HASH",
        "events_stages_current",
        "PRIMARY",
        "0",
        "2",
        "EVENT_ID",
        "HASH",
        "events_stages_history",
        "PRIMARY",
        "0",
        "1",
        "THREAD_ID",
        "HASH",
        "events_stages_history",
        "PRIMARY",
        "0",
        "2",
        "EVENT_ID",
        "HASH",
        "events_statements_summary_by_program",
        "PRIMARY",
        "0",
        "1",
        "OBJECT_TYPE",
        "HASH",
        "events_statements_summary_by_program",
        "PRIMARY",
        "0",
        "2",
        "OBJECT_SCHEMA",
        "HASH",
        "events_statements_summary_by_program",
        "PRIMARY",
        "0",
        "3",
        "OBJECT_NAME",
        "HASH",
        "events_waits_current",
        "PRIMARY",
        "0",
        "1",
        "THREAD_ID",
        "HASH",
        "events_waits_current",
        "PRIMARY",
        "0",
        "2",
        "EVENT_ID",
        "HASH",
        "events_waits_history",
        "PRIMARY",
        "0",
        "1",
        "THREAD_ID",
        "HASH",
        "events_waits_history",
        "PRIMARY",
        "0",
        "2",
        "EVENT_ID",
        "HASH",
    };
    static const char *const information_schema_tables_columns[] = {
        "TABLE_NAME",
        "ENGINE",
        "ROW_FORMAT",
        "TABLE_ROWS",
        "TABLE_COLLATION",
    };
    static const char *const information_schema_tables_rows[] = {
        "events_stages_current",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        "256",
        "utf8mb4_0900_ai_ci",
        "events_stages_history",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        "2560",
        "utf8mb4_0900_ai_ci",
        "events_stages_history_long",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        "10000",
        "utf8mb4_0900_ai_ci",
        "events_statements_history_long",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        "10000",
        "utf8mb4_0900_ai_ci",
        "events_statements_summary_by_program",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        "0",
        "utf8mb4_0900_ai_ci",
        "events_waits_current",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        "1536",
        "utf8mb4_0900_ai_ci",
        "events_waits_history",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        "2560",
        "utf8mb4_0900_ai_ci",
        "events_waits_history_long",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        "10000",
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
    static const char *const show_index_program_rows[] = {
        "events_statements_summary_by_program",
        "0",
        "PRIMARY",
        "1",
        "OBJECT_TYPE",
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
        "events_statements_summary_by_program",
        "0",
        "PRIMARY",
        "2",
        "OBJECT_SCHEMA",
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
        "events_statements_summary_by_program",
        "0",
        "PRIMARY",
        "3",
        "OBJECT_NAME",
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
    static const char *const one_count_column[] = {"COUNT(*)"};
    static const char *const zero_count[] = {"0"};
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        mylite_test_expect_int(mylite_test_open_temporary(&database), MYLITE_OK, "open database");
    if (database == NULL) {
        return failures + 1;
    }

    failures += expect_empty_count(
        database,
        "SELECT COUNT(*) FROM performance_schema.events_stages_current",
        "events_stages_current count"
    );
    failures += expect_empty_count(
        database,
        "SELECT COUNT(*) FROM performance_schema.events_stages_history",
        "events_stages_history count"
    );
    failures += expect_empty_count(
        database,
        "SELECT COUNT(*) FROM performance_schema.events_stages_history_long",
        "events_stages_history_long count"
    );
    failures += expect_empty_count(
        database,
        "SELECT COUNT(*) FROM performance_schema.events_statements_history_long",
        "events_statements_history_long count"
    );
    failures += expect_empty_count(
        database,
        "SELECT COUNT(*) FROM performance_schema.events_statements_summary_by_program",
        "events_statements_summary_by_program count"
    );
    failures += expect_empty_count(
        database,
        "SELECT COUNT(*) FROM performance_schema.events_waits_current",
        "events_waits_current count"
    );
    failures += expect_empty_count(
        database,
        "SELECT COUNT(*) FROM performance_schema.events_waits_history",
        "events_waits_history count"
    );
    failures += expect_empty_count(
        database,
        "SELECT COUNT(*) FROM performance_schema.events_waits_history_long",
        "events_waits_history_long count"
    );
    failures += execute_ok(database, "USE performance_schema");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM events_waits_history_long",
            .column_names = one_count_column,
            .values = zero_count,
            .column_count = 1U,
            .row_count = 1U,
            .context = "selected schema empty event history read",
        }
    );
    failures += execute_ok(database, "USE mysql");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM performance_schema.events_stages_current",
            .column_names = show_columns_columns,
            .values = stages_current_columns,
            .column_count = show_columns_column_count,
            .row_count = show_stages_current_row_count,
            .context = "show stage current columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM performance_schema.events_statements_summary_by_program",
            .column_names = show_index_columns,
            .values = show_index_program_rows,
            .column_count = show_index_column_count,
            .row_count = show_index_program_row_count,
            .context = "show program summary primary key",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM performance_schema.events_stages_history_long",
            .column_names = show_index_columns,
            .values = NULL,
            .column_count = show_index_column_count,
            .row_count = 0U,
            .context = "show history long has no index",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW TABLE STATUS FROM performance_schema "
                   "WHERE Name = 'events_waits_current' "
                   "AND Engine = 'PERFORMANCE_SCHEMA' "
                   "AND Row_format = 'Dynamic' "
                   "AND Auto_increment IS NULL "
                   "AND Collation = 'utf8mb4_0900_ai_ci'",
            .column_names = show_table_status_columns,
            .values = NULL,
            .column_count = show_table_status_column_count,
            .row_count = 1U,
            .context = "show table status for event placeholder",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, COLUMN_TYPE, "
                   "COLUMN_KEY, IS_NULLABLE "
                   "FROM information_schema.columns "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND ((TABLE_NAME = 'events_stages_current' "
                   "AND COLUMN_NAME IN ('THREAD_ID', 'NESTING_EVENT_TYPE')) "
                   "OR (TABLE_NAME = 'events_stages_history_long' "
                   "AND COLUMN_NAME = 'THREAD_ID') "
                   "OR (TABLE_NAME = 'events_statements_history_long' "
                   "AND COLUMN_NAME IN ('SQL_TEXT', 'EXECUTION_ENGINE')) "
                   "OR (TABLE_NAME = 'events_statements_summary_by_program' "
                   "AND COLUMN_NAME IN ('OBJECT_TYPE', 'COUNT_SECONDARY')) "
                   "OR (TABLE_NAME = 'events_waits_current' "
                   "AND COLUMN_NAME IN ('OBJECT_NAME', 'NUMBER_OF_BYTES')) "
                   "OR (TABLE_NAME = 'events_waits_history' "
                   "AND COLUMN_NAME = 'EVENT_ID') "
                   "OR (TABLE_NAME = 'events_waits_history_long' "
                   "AND COLUMN_NAME = 'THREAD_ID')) "
                   "ORDER BY TABLE_NAME, ORDINAL_POSITION",
            .column_names = information_schema_columns_columns,
            .values = information_schema_columns_rows,
            .column_count = information_schema_columns_projection_count,
            .row_count = information_schema_columns_row_count,
            .context = "information schema event columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, "
                   "COLUMN_NAME, INDEX_TYPE "
                   "FROM information_schema.statistics "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME IN ('events_stages_current', "
                   "'events_stages_history', "
                   "'events_statements_summary_by_program', "
                   "'events_waits_current', "
                   "'events_waits_history') "
                   "ORDER BY TABLE_NAME, INDEX_NAME, SEQ_IN_INDEX",
            .column_names = information_schema_statistics_columns,
            .values = information_schema_statistics_rows,
            .column_count = information_schema_statistics_projection_count,
            .row_count = information_schema_statistics_row_count,
            .context = "information schema event statistics",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, ENGINE, ROW_FORMAT, TABLE_ROWS, TABLE_COLLATION "
                   "FROM information_schema.tables "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME IN ('events_stages_current', "
                   "'events_stages_history', "
                   "'events_stages_history_long', "
                   "'events_statements_history_long', "
                   "'events_statements_summary_by_program', "
                   "'events_waits_current', "
                   "'events_waits_history', "
                   "'events_waits_history_long') "
                   "ORDER BY TABLE_NAME",
            .column_names = information_schema_tables_columns,
            .values = information_schema_tables_rows,
            .column_count = information_schema_tables_projection_count,
            .row_count = information_schema_tables_row_count,
            .context = "information schema event tables",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO performance_schema.events_stages_current "
        "(THREAD_ID, EVENT_ID, EVENT_NAME) VALUES (1, 1, 'stage/sql/test')",
        (struct expected_sql_error){
            .code = mysql_error_access_denied,
            .sqlstate = "42000",
            .message_part = "Access denied",
        }
    );
    failures += expect_row_count_state(database, "row count after performance schema write error");

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
        if (query.values == NULL) {
            mylite_result_free(result);
            return failures;
        }
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
            .column_count = 1U,
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
