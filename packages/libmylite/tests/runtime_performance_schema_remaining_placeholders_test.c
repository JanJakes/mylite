#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    mysql_error_access_denied = 1044,
    count_column_count = 1,
    remaining_table_count = 3,
    single_row_count = 1,
    error_log_column_count = 6,
    log_status_column_count = 4,
    setup_instrument_column_count = 7,
    setup_instrument_row_count = 8,
    show_columns_column_count = 6,
    show_full_columns_column_count = 9,
    show_index_column_count = 15,
    error_log_show_index_row_count = 5,
    show_table_status_column_count = 18,
    information_schema_columns_projection_count = 6,
    information_schema_columns_row_count = 11,
    information_schema_statistics_projection_count = 6,
    information_schema_statistics_row_count = 6,
    information_schema_constraints_projection_count = 4,
    information_schema_constraints_row_count = 2,
    information_schema_key_usage_projection_count = 4,
    information_schema_key_usage_row_count = 2,
    information_schema_tables_projection_count = 5,
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

static int test_performance_schema_remaining_placeholders(void);
static int execute_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query query);

int main(void) {
    return test_performance_schema_remaining_placeholders() == 0 ? 0 : 1;
}

static int test_performance_schema_remaining_placeholders(void) {
    static const char setup_instrument_lock_doc[] =
        "Components can provide their own performance_schema tables. This lock protects the list "
        "of such tables definitions.";
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const zero_count[] = {"0"};
    static const char *const one_count[] = {"1"};
    static const char *const setup_count[] = {"8"};
    static const char *const show_columns_columns[] = {
        "Field",
        "Type",
        "Null",
        "Key",
        "Default",
        "Extra",
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
    static const char *const log_status_columns[] = {
        "SERVER_UUID",
        "LOCAL",
        "REPLICATION",
        "STORAGE_ENGINES",
    };
    static const char *const log_status_row[] = {
        "4d796c69-7465-4000-8000-000000000001",
        "{}",
        "{}",
        "{}",
    };
    static const char *const setup_instrument_columns[] = {
        "NAME",
        "ENABLED",
        "TIMED",
        "PROPERTIES",
        "FLAGS",
        "VOLATILITY",
        "DOCUMENTATION",
    };
    static const char *const setup_instrument_rows[] = {
        "idle",
        "YES",
        "YES",
        "user",
        NULL,
        "0",
        NULL,
        "memory/sql/TABLE",
        "YES",
        NULL,
        "global_statistics",
        "",
        "0",
        "Memory used by TABLE objects and their mem root.",
        "stage/sql/starting",
        "NO",
        "NO",
        "",
        NULL,
        "0",
        NULL,
        "statement/sql/select",
        "YES",
        "YES",
        "",
        NULL,
        "0",
        NULL,
        "wait/io/socket/sql/client_connection",
        "NO",
        "NO",
        "user",
        NULL,
        "0",
        NULL,
        "wait/synch/mutex/pfs/LOCK_pfs_share_list",
        "NO",
        "NO",
        "singleton",
        NULL,
        "1",
        setup_instrument_lock_doc,
        "wait/synch/mutex/sql/MYSQL_BIN_LOG::LOCK_commit",
        "NO",
        "NO",
        "",
        NULL,
        "0",
        NULL,
        "wait/synch/mutex/sql/TC_LOG_MMAP::LOCK_tc",
        "NO",
        "NO",
        "",
        NULL,
        "0",
        NULL,
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
        "error_log",
        "LOGGED",
        "1",
        "timestamp(6)",
        "PRI",
        "NO",
        "error_log",
        "THREAD_ID",
        "2",
        "bigint unsigned",
        "MUL",
        "YES",
        "error_log",
        "PRIO",
        "3",
        "enum('System','Error','Warning','Note')",
        "MUL",
        "NO",
        "error_log",
        "DATA",
        "6",
        "text",
        "",
        "NO",
        "log_status",
        "SERVER_UUID",
        "1",
        "char(36)",
        "",
        "NO",
        "log_status",
        "LOCAL",
        "2",
        "json",
        "",
        "NO",
        "setup_instruments",
        "NAME",
        "1",
        "varchar(128)",
        "PRI",
        "NO",
        "setup_instruments",
        "TIMED",
        "3",
        "enum('YES','NO')",
        "",
        "YES",
        "setup_instruments",
        "PROPERTIES",
        "4",
        "set('singleton','progress','user','global_statistics','mutable','controlled_by_default')",
        "",
        "NO",
        "setup_instruments",
        "FLAGS",
        "5",
        "set('controlled')",
        "",
        "YES",
        "setup_instruments",
        "DOCUMENTATION",
        "7",
        "longtext",
        "",
        "YES",
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
        "error_log",         "ERROR_CODE", "1", "1", "ERROR_CODE", "HASH",
        "error_log",         "PRIMARY",    "0", "1", "LOGGED",     "HASH",
        "error_log",         "PRIO",       "1", "1", "PRIO",       "HASH",
        "error_log",         "SUBSYSTEM",  "1", "1", "SUBSYSTEM",  "HASH",
        "error_log",         "THREAD_ID",  "1", "1", "THREAD_ID",  "HASH",
        "setup_instruments", "PRIMARY",    "0", "1", "NAME",       "HASH",
    };
    static const char *const information_schema_constraints_columns[] = {
        "TABLE_NAME",
        "CONSTRAINT_NAME",
        "CONSTRAINT_TYPE",
        "ENFORCED",
    };
    static const char *const information_schema_constraints_rows[] = {
        "error_log",
        "PRIMARY",
        "PRIMARY KEY",
        "YES",
        "setup_instruments",
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
        "error_log",
        "PRIMARY",
        "LOGGED",
        "1",
        "setup_instruments",
        "PRIMARY",
        "NAME",
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
        "error_log",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        "0",
        "utf8mb4_0900_ai_ci",
        "log_status",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        "1",
        "utf8mb4_0900_ai_ci",
        "setup_instruments",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        "1561",
        "utf8mb4_0900_ai_ci",
    };
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_open_temporary(&database) != MYLITE_OK) {
        fprintf(stderr, "failed to open temporary database\n");
        return 1;
    }

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM performance_schema.error_log",
            .column_names = count_column,
            .values = zero_count,
            .column_count = count_column_count,
            .row_count = single_row_count,
            .context = "error_log count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT SERVER_UUID, LOCAL, REPLICATION, STORAGE_ENGINES "
                   "FROM performance_schema.log_status",
            .column_names = log_status_columns,
            .values = log_status_row,
            .column_count = log_status_column_count,
            .row_count = single_row_count,
            .context = "log_status row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM performance_schema.log_status",
            .column_names = count_column,
            .values = one_count,
            .column_count = count_column_count,
            .row_count = single_row_count,
            .context = "log_status count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT NAME, ENABLED, TIMED, PROPERTIES, FLAGS, VOLATILITY, DOCUMENTATION "
                   "FROM performance_schema.setup_instruments ORDER BY NAME",
            .column_names = setup_instrument_columns,
            .values = setup_instrument_rows,
            .column_count = setup_instrument_column_count,
            .row_count = setup_instrument_row_count,
            .context = "setup_instruments rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM performance_schema.setup_instruments "
                   "WHERE NAME LIKE 'memory/%' AND ENABLED = 'YES'",
            .column_names = count_column,
            .values = one_count,
            .column_count = count_column_count,
            .row_count = single_row_count,
            .context = "setup_instruments memory enabled count",
        }
    );
    failures += execute_ok(database, "USE performance_schema");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM setup_instruments",
            .column_names = count_column,
            .values = setup_count,
            .column_count = count_column_count,
            .row_count = single_row_count,
            .context = "selected setup_instruments count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM performance_schema.error_log",
            .column_names = show_columns_columns,
            .values = NULL,
            .column_count = show_columns_column_count,
            .row_count = error_log_column_count,
            .context = "show error_log columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW FULL COLUMNS FROM performance_schema.setup_instruments",
            .column_names = show_full_columns_columns,
            .values = NULL,
            .column_count = show_full_columns_column_count,
            .row_count = setup_instrument_column_count,
            .context = "show setup_instruments full columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM performance_schema.error_log",
            .column_names = show_index_columns,
            .values = NULL,
            .column_count = show_index_column_count,
            .row_count = error_log_show_index_row_count,
            .context = "show error_log index",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM performance_schema.log_status",
            .column_names = show_index_columns,
            .values = NULL,
            .column_count = show_index_column_count,
            .row_count = 0U,
            .context = "show log_status index",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM performance_schema.setup_instruments",
            .column_names = show_index_columns,
            .values = NULL,
            .column_count = show_index_column_count,
            .row_count = single_row_count,
            .context = "show setup_instruments index",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW TABLE STATUS FROM performance_schema LIKE 'setup_instruments'",
            .column_names = show_table_status_columns,
            .values = NULL,
            .column_count = show_table_status_column_count,
            .row_count = single_row_count,
            .context = "show setup_instruments table status",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, COLUMN_TYPE, "
                   "COLUMN_KEY, IS_NULLABLE "
                   "FROM information_schema.columns "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND ((TABLE_NAME = 'error_log' "
                   "AND COLUMN_NAME IN ('LOGGED','THREAD_ID','PRIO','DATA')) "
                   "OR (TABLE_NAME = 'log_status' "
                   "AND COLUMN_NAME IN ('SERVER_UUID','LOCAL')) "
                   "OR (TABLE_NAME = 'setup_instruments' "
                   "AND COLUMN_NAME IN ('NAME','TIMED','PROPERTIES','FLAGS','DOCUMENTATION'))) "
                   "ORDER BY TABLE_NAME, ORDINAL_POSITION",
            .column_names = information_schema_columns_columns,
            .values = information_schema_columns_rows,
            .column_count = information_schema_columns_projection_count,
            .row_count = information_schema_columns_row_count,
            .context = "information schema remaining columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME, "
                   "INDEX_TYPE "
                   "FROM information_schema.statistics "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME IN ('error_log','log_status','setup_instruments') "
                   "ORDER BY TABLE_NAME, INDEX_NAME, SEQ_IN_INDEX",
            .column_names = information_schema_statistics_columns,
            .values = information_schema_statistics_rows,
            .column_count = information_schema_statistics_projection_count,
            .row_count = information_schema_statistics_row_count,
            .context = "information schema remaining statistics",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED "
                   "FROM information_schema.table_constraints "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME IN ('error_log','log_status','setup_instruments') "
                   "ORDER BY TABLE_NAME, CONSTRAINT_NAME",
            .column_names = information_schema_constraints_columns,
            .values = information_schema_constraints_rows,
            .column_count = information_schema_constraints_projection_count,
            .row_count = information_schema_constraints_row_count,
            .context = "information schema remaining constraints",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION "
                   "FROM information_schema.key_column_usage "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME IN ('error_log','log_status','setup_instruments') "
                   "ORDER BY TABLE_NAME, CONSTRAINT_NAME, ORDINAL_POSITION",
            .column_names = information_schema_key_usage_columns,
            .values = information_schema_key_usage_rows,
            .column_count = information_schema_key_usage_projection_count,
            .row_count = information_schema_key_usage_row_count,
            .context = "information schema remaining key usage",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, ENGINE, ROW_FORMAT, TABLE_ROWS, TABLE_COLLATION "
                   "FROM information_schema.tables "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME IN ('error_log','log_status','setup_instruments') "
                   "ORDER BY TABLE_NAME",
            .column_names = information_schema_tables_columns,
            .values = information_schema_tables_rows,
            .column_count = information_schema_tables_projection_count,
            .row_count = remaining_table_count,
            .context = "information schema remaining tables",
        }
    );
    failures += execute_error(
        database,
        "UPDATE performance_schema.setup_instruments SET ENABLED = 'NO'",
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
