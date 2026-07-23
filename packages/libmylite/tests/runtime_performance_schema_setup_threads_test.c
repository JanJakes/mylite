#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    mysql_error_access_denied = 1044,
    setup_threads_column_count = 6,
    setup_threads_row_count = 56,
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

static int test_performance_schema_setup_threads(void);
static int execute_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query query);
static int expect_row_count_state(mylite_db *database, const char *context);

int main(void) {
    return test_performance_schema_setup_threads() == 0 ? 0 : 1;
}

static int test_performance_schema_setup_threads(void) {
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const setup_thread_columns[] = {
        "NAME",
        "ENABLED",
        "HISTORY",
        "PROPERTIES",
        "VOLATILITY",
        "DOCUMENTATION",
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
    static const char *const count_fifty_six[] = {"56"};
    static const char *const setup_thread_rows[] = {
        "thread/innodb/buf_dump_thread",
        "YES",
        "YES",
        "singleton",
        "0",
        NULL,
        "thread/innodb/buf_pool_create_thread",
        "YES",
        "YES",
        "singleton",
        "0",
        NULL,
        "thread/innodb/buf_resize_thread",
        "YES",
        "YES",
        "singleton",
        "0",
        NULL,
        "thread/innodb/bulk_alloc_thread",
        "YES",
        "YES",
        "singleton",
        "0",
        NULL,
        "thread/innodb/bulk_flusher_thread",
        "YES",
        "YES",
        "",
        "0",
        NULL,
        "thread/innodb/clone_ddl_thread",
        "YES",
        "YES",
        "singleton",
        "0",
        NULL,
        "thread/innodb/clone_gtid_thread",
        "YES",
        "YES",
        "singleton",
        "0",
        NULL,
        "thread/innodb/ddl_thread",
        "YES",
        "YES",
        "",
        "0",
        NULL,
        "thread/innodb/dict_stats_thread",
        "YES",
        "YES",
        "singleton",
        "0",
        NULL,
        "thread/innodb/fts_optimize_thread",
        "YES",
        "YES",
        "singleton",
        "0",
        NULL,
        "thread/innodb/fts_parallel_merge_thread",
        "YES",
        "YES",
        "",
        "0",
        NULL,
        "thread/innodb/fts_parallel_tokenization_thread",
        "YES",
        "YES",
        "",
        "0",
        NULL,
        "thread/innodb/io_ibuf_thread",
        "YES",
        "YES",
        "singleton",
        "0",
        NULL,
        "thread/innodb/io_read_thread",
        "YES",
        "YES",
        "",
        "0",
        NULL,
        "thread/innodb/io_write_thread",
        "YES",
        "YES",
        "",
        "0",
        NULL,
        "thread/innodb/log_archiver_thread",
        "YES",
        "YES",
        "singleton",
        "0",
        NULL,
        "thread/innodb/log_checkpointer_thread",
        "YES",
        "YES",
        "singleton",
        "0",
        NULL,
        "thread/innodb/log_files_governor_thread",
        "YES",
        "YES",
        "singleton",
        "0",
        NULL,
        "thread/innodb/log_flush_notifier_thread",
        "YES",
        "YES",
        "singleton",
        "0",
        NULL,
        "thread/innodb/log_flusher_thread",
        "YES",
        "YES",
        "singleton",
        "0",
        NULL,
        "thread/innodb/log_write_notifier_thread",
        "YES",
        "YES",
        "singleton",
        "0",
        NULL,
        "thread/innodb/log_writer_thread",
        "YES",
        "YES",
        "singleton",
        "0",
        NULL,
        "thread/innodb/meb::redo_log_archive_consumer_thread",
        "YES",
        "YES",
        "singleton",
        "0",
        NULL,
        "thread/innodb/page_archiver_thread",
        "YES",
        "YES",
        "singleton",
        "0",
        NULL,
        "thread/innodb/page_flush_coordinator_thread",
        "YES",
        "YES",
        "singleton",
        "0",
        NULL,
        "thread/innodb/page_flush_thread",
        "YES",
        "YES",
        "",
        "0",
        NULL,
        "thread/innodb/parallel_read_thread",
        "YES",
        "YES",
        "",
        "0",
        NULL,
        "thread/innodb/parallel_rseg_init_thread",
        "YES",
        "YES",
        "",
        "0",
        NULL,
        "thread/innodb/recv_writer_thread",
        "YES",
        "YES",
        "singleton",
        "0",
        NULL,
        "thread/innodb/srv_error_monitor_thread",
        "YES",
        "YES",
        "singleton",
        "0",
        NULL,
        "thread/innodb/srv_lock_timeout_thread",
        "YES",
        "YES",
        "singleton",
        "0",
        NULL,
        "thread/innodb/srv_master_thread",
        "YES",
        "YES",
        "singleton",
        "0",
        NULL,
        "thread/innodb/srv_monitor_thread",
        "YES",
        "YES",
        "singleton",
        "0",
        NULL,
        "thread/innodb/srv_purge_thread",
        "YES",
        "YES",
        "singleton",
        "0",
        NULL,
        "thread/innodb/srv_ts_alter_encrypt_thread",
        "YES",
        "YES",
        "singleton",
        "0",
        NULL,
        "thread/innodb/srv_worker_thread",
        "YES",
        "YES",
        "",
        "0",
        NULL,
        "thread/innodb/trx_recovery_rollback_thread",
        "YES",
        "YES",
        "",
        "0",
        NULL,
        "thread/myisam/find_all_keys",
        "YES",
        "YES",
        "",
        "0",
        NULL,
        "thread/mysqlx/acceptor_network",
        "YES",
        "YES",
        "",
        "0",
        NULL,
        "thread/mysqlx/worker",
        "YES",
        "YES",
        "user",
        "0",
        NULL,
        "thread/mysys/thread_timer_notifier",
        "YES",
        "YES",
        "singleton",
        "0",
        NULL,
        "thread/performance_schema/setup",
        "YES",
        "YES",
        "singleton",
        "0",
        NULL,
        "thread/sql/admin_interface",
        "YES",
        "YES",
        "user",
        "0",
        NULL,
        "thread/sql/bootstrap",
        "YES",
        "YES",
        "singleton",
        "0",
        NULL,
        "thread/sql/compress_gtid_table",
        "YES",
        "YES",
        "singleton",
        "0",
        NULL,
        "thread/sql/event_scheduler",
        "YES",
        "YES",
        "singleton",
        "0",
        NULL,
        "thread/sql/event_worker",
        "YES",
        "YES",
        "",
        "0",
        NULL,
        "thread/sql/main",
        "YES",
        "YES",
        "singleton",
        "0",
        NULL,
        "thread/sql/manager",
        "YES",
        "YES",
        "singleton",
        "0",
        NULL,
        "thread/sql/one_connection",
        "YES",
        "YES",
        "user",
        "0",
        NULL,
        "thread/sql/parser_service",
        "YES",
        "YES",
        "singleton",
        "0",
        NULL,
        "thread/sql/replica_io",
        "YES",
        "YES",
        "",
        "0",
        NULL,
        "thread/sql/replica_monitor",
        "YES",
        "YES",
        "singleton",
        "0",
        NULL,
        "thread/sql/replica_sql",
        "YES",
        "YES",
        "",
        "0",
        NULL,
        "thread/sql/replica_worker",
        "YES",
        "YES",
        "",
        "0",
        NULL,
        "thread/sql/signal_handler",
        "YES",
        "YES",
        "singleton",
        "0",
        NULL,
    };
    static const char *const selected_schema_thread[] = {"user"};
    static const char *const full_columns[] = {
        "NAME",
        "varchar(128)",
        "utf8mb4_0900_ai_ci",
        "NO",
        "PRI",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "ENABLED",
        "enum('YES','NO')",
        "utf8mb4_0900_ai_ci",
        "NO",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "HISTORY",
        "enum('YES','NO')",
        "utf8mb4_0900_ai_ci",
        "NO",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "PROPERTIES",
        "set('singleton','user')",
        "utf8mb4_0900_ai_ci",
        "NO",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "VOLATILITY",
        "int",
        NULL,
        "NO",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "DOCUMENTATION",
        "longtext",
        "utf8mb4_0900_ai_ci",
        "YES",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
    };
    static const char *const show_index[] = {
        "setup_threads",
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
        "varchar(128)",
        "PRI",
        NULL,
        "utf8mb4_0900_ai_ci",
        "ENABLED",
        "2",
        "NO",
        "enum('YES','NO')",
        "",
        NULL,
        "utf8mb4_0900_ai_ci",
        "HISTORY",
        "3",
        "NO",
        "enum('YES','NO')",
        "",
        NULL,
        "utf8mb4_0900_ai_ci",
        "PROPERTIES",
        "4",
        "NO",
        "set('singleton','user')",
        "",
        NULL,
        "utf8mb4_0900_ai_ci",
        "VOLATILITY",
        "5",
        "NO",
        "int",
        "",
        NULL,
        NULL,
        "DOCUMENTATION",
        "6",
        "YES",
        "longtext",
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
        "setup_threads",
        "BASE TABLE",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        "100",
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
        "setup_threads",
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

    failures +=
        mylite_test_expect_int(mylite_test_open_temporary(&database), MYLITE_OK, "open database");
    if (failures != 0) {
        return failures;
    }

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM performance_schema.setup_threads",
            .column_names = count_column,
            .values = count_fifty_six,
            .column_count = 1U,
            .row_count = 1U,
            .context = "setup_threads row count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT NAME, ENABLED, HISTORY, PROPERTIES, VOLATILITY, DOCUMENTATION "
                   "FROM performance_schema.setup_threads "
                   "ORDER BY NAME",
            .column_names = setup_thread_columns,
            .values = setup_thread_rows,
            .column_count = setup_threads_column_count,
            .row_count = setup_threads_row_count,
            .context = "setup_threads rows",
        }
    );
    failures += execute_ok(database, "USE performance_schema");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT PROPERTIES FROM setup_threads "
                   "WHERE NAME = 'thread/sql/one_connection'",
            .column_names = (const char *const[]){"PROPERTIES"},
            .values = selected_schema_thread,
            .column_count = 1U,
            .row_count = 1U,
            .context = "selected performance_schema table resolution",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW FULL COLUMNS FROM performance_schema.setup_threads",
            .column_names = show_full_columns_columns,
            .values = full_columns,
            .column_count = show_full_columns_column_count,
            .row_count = setup_threads_column_count,
            .context = "setup_threads show full columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM performance_schema.setup_threads",
            .column_names = show_index_columns,
            .values = show_index,
            .column_count = show_index_column_count,
            .row_count = 1U,
            .context = "setup_threads show index",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW TABLE STATUS FROM performance_schema "
                   "WHERE Name = 'setup_threads' "
                   "AND Engine = 'PERFORMANCE_SCHEMA' "
                   "AND Row_format = 'Dynamic' "
                   "AND Auto_increment IS NULL "
                   "AND Collation = 'utf8mb4_0900_ai_ci'",
            .column_names = show_table_status_columns,
            .values = NULL,
            .column_count = show_table_status_column_count,
            .row_count = 1U,
            .context = "setup_threads show table status",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, COLUMN_TYPE, "
                   "COLUMN_KEY, COLUMN_DEFAULT, COLLATION_NAME "
                   "FROM information_schema.columns "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME = 'setup_threads' "
                   "ORDER BY ORDINAL_POSITION",
            .column_names = information_schema_columns_columns,
            .values = information_schema_columns,
            .column_count = information_schema_columns_projection_count,
            .row_count = setup_threads_column_count,
            .context = "information schema columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME, "
                   "COLLATION, CARDINALITY, INDEX_TYPE "
                   "FROM information_schema.statistics "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME = 'setup_threads'",
            .column_names = information_schema_statistics_columns,
            .values = information_schema_statistics,
            .column_count = information_schema_statistics_projection_count,
            .row_count = 1U,
            .context = "information schema statistics",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, ROW_FORMAT, TABLE_ROWS, "
                   "AUTO_INCREMENT, TABLE_COLLATION "
                   "FROM information_schema.tables "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME = 'setup_threads'",
            .column_names = information_schema_tables_columns,
            .values = information_schema_tables,
            .column_count = information_schema_tables_projection_count,
            .row_count = 1U,
            .context = "information schema tables",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED "
                   "FROM information_schema.table_constraints "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME = 'setup_threads'",
            .column_names = information_schema_constraints_columns,
            .values = information_schema_constraints,
            .column_count = information_schema_constraints_projection_count,
            .row_count = 1U,
            .context = "information schema table constraints",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION "
                   "FROM information_schema.key_column_usage "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME = 'setup_threads'",
            .column_names = information_schema_key_usage_columns,
            .values = information_schema_key_usage,
            .column_count = information_schema_key_usage_projection_count,
            .row_count = 1U,
            .context = "information schema key column usage",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONSTRAINT_SCHEMA, TABLE_NAME, CONSTRAINT_NAME, "
                   "ENGINE_ATTRIBUTE, SECONDARY_ENGINE_ATTRIBUTE "
                   "FROM information_schema.table_constraints_extensions "
                   "WHERE CONSTRAINT_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME = 'setup_threads'",
            .column_names = information_schema_constraint_extensions_columns,
            .values = information_schema_constraint_extensions,
            .column_count = information_schema_constraint_extensions_projection_count,
            .row_count = 1U,
            .context = "information schema table constraint extensions",
        }
    );
    failures += execute_error(
        database,
        "UPDATE performance_schema.setup_threads SET ENABLED = 'NO'",
        access_denied
    );
    failures += expect_row_count_state(database, "row count after select");

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
