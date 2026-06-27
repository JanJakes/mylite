#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    information_schema_columns_projection_count = 6,
    information_schema_columns_row_count = 9,
    information_schema_constraints_projection_count = 4,
    information_schema_constraints_row_count = 3,
    information_schema_constraint_extensions_projection_count = 4,
    information_schema_constraint_extensions_row_count = 3,
    information_schema_key_usage_projection_count = 4,
    information_schema_key_usage_row_count = 10,
    information_schema_statistics_projection_count = 6,
    information_schema_statistics_row_count = 10,
    information_schema_tables_projection_count = 5,
    information_schema_tables_row_count = 3,
    show_columns_column_count = 6,
    show_index_usage_columns_row_count = 4,
    show_index_column_count = 15,
    show_index_usage_row_count = 4,
    show_lock_columns_row_count = 3,
    show_lock_row_count = 3,
    show_table_columns_row_count = 3,
    show_table_row_count = 3,
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

static int test_performance_schema_table_wait_summary_placeholders(void);
static int execute_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query query);
static int expect_empty_count(mylite_db *database, const char *sql, const char *context);
static int expect_row_count_state(mylite_db *database, const char *context);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

int main(void) {
    return test_performance_schema_table_wait_summary_placeholders() == 0 ? 0 : 1;
}

static int test_performance_schema_table_wait_summary_placeholders(void) {
    static const char *const show_columns_columns[] = {
        "Field",
        "Type",
        "Null",
        "Key",
        "Default",
        "Extra",
    };
    static const char *const show_index_usage_columns_rows[] = {
        "OBJECT_TYPE",      "varchar(64)",     "YES", "MUL", NULL, "",
        "INDEX_NAME",       "varchar(64)",     "YES", "",    NULL, "",
        "COUNT_FETCH",      "bigint unsigned", "NO",  "",    NULL, "",
        "MAX_TIMER_DELETE", "bigint unsigned", "NO",  "",    NULL, "",
    };
    static const char *const show_table_columns_rows[] = {
        "OBJECT_TYPE",
        "varchar(64)",
        "YES",
        "MUL",
        NULL,
        "",
        "COUNT_FETCH",
        "bigint unsigned",
        "NO",
        "",
        NULL,
        "",
        "MAX_TIMER_DELETE",
        "bigint unsigned",
        "NO",
        "",
        NULL,
        "",
    };
    static const char *const show_lock_columns_rows[] = {
        "OBJECT_TYPE",
        "varchar(64)",
        "YES",
        "MUL",
        NULL,
        "",
        "COUNT_READ_WITH_SHARED_LOCKS",
        "bigint unsigned",
        "NO",
        "",
        NULL,
        "",
        "MAX_TIMER_WRITE_EXTERNAL",
        "bigint unsigned",
        "NO",
        "",
        NULL,
        "",
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
    static const char *const show_index_usage_rows[] = {
        "table_io_waits_summary_by_index_usage",
        "0",
        "OBJECT",
        "1",
        "OBJECT_TYPE",
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
        "table_io_waits_summary_by_index_usage",
        "0",
        "OBJECT",
        "2",
        "OBJECT_SCHEMA",
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
        "table_io_waits_summary_by_index_usage",
        "0",
        "OBJECT",
        "3",
        "OBJECT_NAME",
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
        "table_io_waits_summary_by_index_usage",
        "0",
        "OBJECT",
        "4",
        "INDEX_NAME",
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
    static const char *const show_table_rows[] = {
        "table_io_waits_summary_by_table",
        "0",
        "OBJECT",
        "1",
        "OBJECT_TYPE",
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
        "table_io_waits_summary_by_table",
        "0",
        "OBJECT",
        "2",
        "OBJECT_SCHEMA",
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
        "table_io_waits_summary_by_table",
        "0",
        "OBJECT",
        "3",
        "OBJECT_NAME",
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
    static const char *const show_lock_rows[] = {
        "table_lock_waits_summary_by_table",
        "0",
        "OBJECT",
        "1",
        "OBJECT_TYPE",
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
        "table_lock_waits_summary_by_table",
        "0",
        "OBJECT",
        "2",
        "OBJECT_SCHEMA",
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
        "table_lock_waits_summary_by_table",
        "0",
        "OBJECT",
        "3",
        "OBJECT_NAME",
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
    static const char *const information_schema_columns_columns[] = {
        "TABLE_NAME",
        "COLUMN_NAME",
        "ORDINAL_POSITION",
        "COLUMN_TYPE",
        "COLUMN_KEY",
        "IS_NULLABLE",
    };
    static const char *const information_schema_columns_rows[] = {
        "table_io_waits_summary_by_index_usage",
        "OBJECT_TYPE",
        "1",
        "varchar(64)",
        "MUL",
        "YES",
        "table_io_waits_summary_by_index_usage",
        "INDEX_NAME",
        "4",
        "varchar(64)",
        "",
        "YES",
        "table_io_waits_summary_by_index_usage",
        "COUNT_FETCH",
        "20",
        "bigint unsigned",
        "",
        "NO",
        "table_io_waits_summary_by_index_usage",
        "MAX_TIMER_DELETE",
        "39",
        "bigint unsigned",
        "",
        "NO",
        "table_io_waits_summary_by_table",
        "OBJECT_TYPE",
        "1",
        "varchar(64)",
        "MUL",
        "YES",
        "table_io_waits_summary_by_table",
        "COUNT_FETCH",
        "19",
        "bigint unsigned",
        "",
        "NO",
        "table_lock_waits_summary_by_table",
        "OBJECT_TYPE",
        "1",
        "varchar(64)",
        "MUL",
        "YES",
        "table_lock_waits_summary_by_table",
        "COUNT_READ_WITH_SHARED_LOCKS",
        "24",
        "bigint unsigned",
        "",
        "NO",
        "table_lock_waits_summary_by_table",
        "MAX_TIMER_WRITE_EXTERNAL",
        "68",
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
        "table_io_waits_summary_by_index_usage", "OBJECT", "0", "1", "OBJECT_TYPE",   "HASH",
        "table_io_waits_summary_by_index_usage", "OBJECT", "0", "2", "OBJECT_SCHEMA", "HASH",
        "table_io_waits_summary_by_index_usage", "OBJECT", "0", "3", "OBJECT_NAME",   "HASH",
        "table_io_waits_summary_by_index_usage", "OBJECT", "0", "4", "INDEX_NAME",    "HASH",
        "table_io_waits_summary_by_table",       "OBJECT", "0", "1", "OBJECT_TYPE",   "HASH",
        "table_io_waits_summary_by_table",       "OBJECT", "0", "2", "OBJECT_SCHEMA", "HASH",
        "table_io_waits_summary_by_table",       "OBJECT", "0", "3", "OBJECT_NAME",   "HASH",
        "table_lock_waits_summary_by_table",     "OBJECT", "0", "1", "OBJECT_TYPE",   "HASH",
        "table_lock_waits_summary_by_table",     "OBJECT", "0", "2", "OBJECT_SCHEMA", "HASH",
        "table_lock_waits_summary_by_table",     "OBJECT", "0", "3", "OBJECT_NAME",   "HASH",
    };
    static const char *const information_schema_constraints_columns[] = {
        "TABLE_NAME",
        "CONSTRAINT_NAME",
        "CONSTRAINT_TYPE",
        "ENFORCED",
    };
    static const char *const information_schema_constraints_rows[] = {
        "table_io_waits_summary_by_index_usage",
        "OBJECT",
        "UNIQUE",
        "YES",
        "table_io_waits_summary_by_table",
        "OBJECT",
        "UNIQUE",
        "YES",
        "table_lock_waits_summary_by_table",
        "OBJECT",
        "UNIQUE",
        "YES",
    };
    static const char *const information_schema_key_usage_columns[] = {
        "TABLE_NAME",
        "CONSTRAINT_NAME",
        "COLUMN_NAME",
        "ORDINAL_POSITION",
    };
    static const char *const information_schema_key_usage_rows[] = {
        "table_io_waits_summary_by_index_usage", "OBJECT", "OBJECT_TYPE",   "1",
        "table_io_waits_summary_by_index_usage", "OBJECT", "OBJECT_SCHEMA", "2",
        "table_io_waits_summary_by_index_usage", "OBJECT", "OBJECT_NAME",   "3",
        "table_io_waits_summary_by_index_usage", "OBJECT", "INDEX_NAME",    "4",
        "table_io_waits_summary_by_table",       "OBJECT", "OBJECT_TYPE",   "1",
        "table_io_waits_summary_by_table",       "OBJECT", "OBJECT_SCHEMA", "2",
        "table_io_waits_summary_by_table",       "OBJECT", "OBJECT_NAME",   "3",
        "table_lock_waits_summary_by_table",     "OBJECT", "OBJECT_TYPE",   "1",
        "table_lock_waits_summary_by_table",     "OBJECT", "OBJECT_SCHEMA", "2",
        "table_lock_waits_summary_by_table",     "OBJECT", "OBJECT_NAME",   "3",
    };
    static const char *const information_schema_constraint_extensions_columns[] = {
        "TABLE_NAME",
        "CONSTRAINT_NAME",
        "ENGINE_ATTRIBUTE",
        "SECONDARY_ENGINE_ATTRIBUTE",
    };
    static const char *const information_schema_constraint_extensions_rows[] = {
        "table_io_waits_summary_by_index_usage",
        "OBJECT",
        NULL,
        NULL,
        "table_io_waits_summary_by_table",
        "OBJECT",
        NULL,
        NULL,
        "table_lock_waits_summary_by_table",
        "OBJECT",
        NULL,
        NULL,
    };
    static const char *const information_schema_tables_columns[] = {
        "TABLE_NAME",
        "ENGINE",
        "ROW_FORMAT",
        "TABLE_ROWS",
        "AUTO_INCREMENT",
    };
    static const char *const information_schema_tables_rows[] = {
        "table_io_waits_summary_by_index_usage",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        "8192",
        NULL,
        "table_io_waits_summary_by_table",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        "4096",
        NULL,
        "table_lock_waits_summary_by_table",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        "4096",
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

    failures += expect_empty_count(
        database,
        "SELECT COUNT(*) FROM performance_schema.table_io_waits_summary_by_index_usage",
        "table_io_waits_summary_by_index_usage count"
    );
    failures += expect_empty_count(
        database,
        "SELECT COUNT(*) FROM performance_schema.table_io_waits_summary_by_table",
        "table_io_waits_summary_by_table count"
    );
    failures += expect_empty_count(
        database,
        "SELECT COUNT(*) FROM performance_schema.table_lock_waits_summary_by_table",
        "table_lock_waits_summary_by_table count"
    );
    failures += execute_ok(database, "USE performance_schema");
    failures += expect_empty_count(
        database,
        "SELECT COUNT(*) FROM table_lock_waits_summary_by_table",
        "selected performance_schema table lock summary count"
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM performance_schema.table_io_waits_summary_by_index_usage "
                   "WHERE Field IN ('OBJECT_TYPE', 'INDEX_NAME', 'COUNT_FETCH', "
                   "'MAX_TIMER_DELETE')",
            .column_names = show_columns_columns,
            .values = show_index_usage_columns_rows,
            .column_count = show_columns_column_count,
            .row_count = show_index_usage_columns_row_count,
            .context = "show table io waits by index columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM performance_schema.table_io_waits_summary_by_table "
                   "WHERE Field IN ('OBJECT_TYPE', 'COUNT_FETCH', 'MAX_TIMER_DELETE')",
            .column_names = show_columns_columns,
            .values = show_table_columns_rows,
            .column_count = show_columns_column_count,
            .row_count = show_table_columns_row_count,
            .context = "show table io waits by table columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM performance_schema.table_lock_waits_summary_by_table "
                   "WHERE Field IN ('OBJECT_TYPE', 'COUNT_READ_WITH_SHARED_LOCKS', "
                   "'MAX_TIMER_WRITE_EXTERNAL')",
            .column_names = show_columns_columns,
            .values = show_lock_columns_rows,
            .column_count = show_columns_column_count,
            .row_count = show_lock_columns_row_count,
            .context = "show table lock waits by table columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM performance_schema.table_io_waits_summary_by_index_usage",
            .column_names = show_index_columns,
            .values = show_index_usage_rows,
            .column_count = show_index_column_count,
            .row_count = show_index_usage_row_count,
            .context = "show table io waits by index index",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM performance_schema.table_io_waits_summary_by_table",
            .column_names = show_index_columns,
            .values = show_table_rows,
            .column_count = show_index_column_count,
            .row_count = show_table_row_count,
            .context = "show table io waits by table index",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM performance_schema.table_lock_waits_summary_by_table",
            .column_names = show_index_columns,
            .values = show_lock_rows,
            .column_count = show_index_column_count,
            .row_count = show_lock_row_count,
            .context = "show table lock waits by table index",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, COLUMN_TYPE, "
                   "COLUMN_KEY, IS_NULLABLE "
                   "FROM information_schema.columns "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND ((TABLE_NAME = 'table_io_waits_summary_by_index_usage' "
                   "AND COLUMN_NAME IN ('OBJECT_TYPE', 'INDEX_NAME', 'COUNT_FETCH', "
                   "'MAX_TIMER_DELETE')) "
                   "OR (TABLE_NAME = 'table_io_waits_summary_by_table' "
                   "AND COLUMN_NAME IN ('OBJECT_TYPE', 'COUNT_FETCH')) "
                   "OR (TABLE_NAME = 'table_lock_waits_summary_by_table' "
                   "AND COLUMN_NAME IN ('OBJECT_TYPE', 'COUNT_READ_WITH_SHARED_LOCKS', "
                   "'MAX_TIMER_WRITE_EXTERNAL'))) "
                   "ORDER BY TABLE_NAME, ORDINAL_POSITION",
            .column_names = information_schema_columns_columns,
            .values = information_schema_columns_rows,
            .column_count = information_schema_columns_projection_count,
            .row_count = information_schema_columns_row_count,
            .context = "information schema table wait summary columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, "
                   "COLUMN_NAME, INDEX_TYPE "
                   "FROM information_schema.statistics "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME IN ('table_io_waits_summary_by_index_usage', "
                   "'table_io_waits_summary_by_table', "
                   "'table_lock_waits_summary_by_table') "
                   "ORDER BY TABLE_NAME, INDEX_NAME, SEQ_IN_INDEX",
            .column_names = information_schema_statistics_columns,
            .values = information_schema_statistics_rows,
            .column_count = information_schema_statistics_projection_count,
            .row_count = information_schema_statistics_row_count,
            .context = "information schema table wait summary statistics",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED "
                   "FROM information_schema.table_constraints "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME IN ('table_io_waits_summary_by_index_usage', "
                   "'table_io_waits_summary_by_table', "
                   "'table_lock_waits_summary_by_table') "
                   "ORDER BY TABLE_NAME, CONSTRAINT_NAME",
            .column_names = information_schema_constraints_columns,
            .values = information_schema_constraints_rows,
            .column_count = information_schema_constraints_projection_count,
            .row_count = information_schema_constraints_row_count,
            .context = "information schema table wait summary constraints",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION "
                   "FROM information_schema.key_column_usage "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME IN ('table_io_waits_summary_by_index_usage', "
                   "'table_io_waits_summary_by_table', "
                   "'table_lock_waits_summary_by_table') "
                   "ORDER BY TABLE_NAME, CONSTRAINT_NAME, ORDINAL_POSITION",
            .column_names = information_schema_key_usage_columns,
            .values = information_schema_key_usage_rows,
            .column_count = information_schema_key_usage_projection_count,
            .row_count = information_schema_key_usage_row_count,
            .context = "information schema table wait summary key usage",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, CONSTRAINT_NAME, ENGINE_ATTRIBUTE, "
                   "SECONDARY_ENGINE_ATTRIBUTE "
                   "FROM information_schema.table_constraints_extensions "
                   "WHERE CONSTRAINT_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME IN ('table_io_waits_summary_by_index_usage', "
                   "'table_io_waits_summary_by_table', "
                   "'table_lock_waits_summary_by_table') "
                   "ORDER BY TABLE_NAME, CONSTRAINT_NAME",
            .column_names = information_schema_constraint_extensions_columns,
            .values = information_schema_constraint_extensions_rows,
            .column_count = information_schema_constraint_extensions_projection_count,
            .row_count = information_schema_constraint_extensions_row_count,
            .context = "information schema table wait summary constraint extensions",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, ENGINE, ROW_FORMAT, TABLE_ROWS, AUTO_INCREMENT "
                   "FROM information_schema.tables "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME IN ('table_io_waits_summary_by_index_usage', "
                   "'table_io_waits_summary_by_table', "
                   "'table_lock_waits_summary_by_table') "
                   "ORDER BY TABLE_NAME",
            .column_names = information_schema_tables_columns,
            .values = information_schema_tables_rows,
            .column_count = information_schema_tables_projection_count,
            .row_count = information_schema_tables_row_count,
            .context = "information schema table wait summary tables",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW TABLE STATUS FROM performance_schema "
                   "WHERE Name IN ('table_io_waits_summary_by_index_usage', "
                   "'table_io_waits_summary_by_table', "
                   "'table_lock_waits_summary_by_table') "
                   "AND Engine = 'PERFORMANCE_SCHEMA' "
                   "AND Row_format = 'Dynamic' "
                   "AND Auto_increment IS NULL "
                   "AND Collation = 'utf8mb4_0900_ai_ci'",
            .column_names = show_table_status_columns,
            .values = NULL,
            .column_count = show_table_status_column_count,
            .row_count = information_schema_tables_row_count,
            .context = "show table status for table wait summaries",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO performance_schema.table_io_waits_summary_by_index_usage "
        "(OBJECT_TYPE) VALUES ('TABLE')",
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
    int failures = expect_int(rc, MYLITE_OK, sql);

    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: %s / %s\n", sql, mylite_sqlstate(database), mylite_errmsg(database));
    }
    if (result != NULL) {
        failures += expect_size(mylite_result_column_count(result), 0U, sql);
        failures += expect_size(mylite_result_row_count(result), 0U, sql);
    }
    mylite_result_free(result);
    return failures;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    failures += expect_int(rc, MYLITE_ERROR, sql);
    failures += expect_int(mylite_errcode(database), expected.code, "error code");
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, "error sqlstate");
    failures += expect_contains(mylite_errmsg(database), expected.message_part, "error message");
    mylite_result_free(result);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, query.sql, strlen(query.sql), &result);
    int failures = 0;

    failures += expect_int(rc, MYLITE_OK, query.context);
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
        failures +=
            expect_size(mylite_result_column_count(result), query.column_count, query.context);
        failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
        failures += expect_int64(mylite_result_affected_rows(result), 0, query.context);
        for (size_t column = 0U; column < query.column_count; ++column) {
            failures += expect_text(
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
                failures += expect_text(
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
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const zero_row[] = {"0"};

    return expect_query(
        database,
        (struct expected_query){
            .sql = sql,
            .column_names = count_column,
            .values = zero_row,
            .column_count = 1U,
            .row_count = 1U,
            .context = context,
        }
    );
}

static int expect_row_count_state(mylite_db *database, const char *context) {
    static const char *const row_count_column[] = {"ROW_COUNT()"};
    static const char *const row_count_row[] = {"-1"};

    return expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT()",
            .column_names = row_count_column,
            .values = row_count_row,
            .column_count = 1U,
            .row_count = 1U,
            .context = context,
        }
    );
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
        expected == NULL ? "<NULL>" : expected,
        actual == NULL ? "<NULL>" : actual
    );
    return 1;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual != NULL && needle != NULL && strstr(actual, needle) != NULL) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected [%s] to contain [%s]\n",
        context,
        actual == NULL ? "<NULL>" : actual,
        needle == NULL ? "<NULL>" : needle
    );
    return 1;
}
