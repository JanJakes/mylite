#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    object_summary_column_count = 8,
    tls_channel_column_count = 3,
    object_summary_index_row_count = 3,
    mysql_error_access_denied = 1044,
    show_columns_column_count = 6,
    show_table_status_column_count = 18,
    show_index_column_count = 15,
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

static int test_performance_schema_object_tls_placeholders(void);
static int execute_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query query);
static int expect_row_count_state(mylite_db *database, const char *context);

int main(void) {
    return test_performance_schema_object_tls_placeholders() == 0 ? 0 : 1;
}

static int test_performance_schema_object_tls_placeholders(void) {
    static const char *const object_summary_columns[] = {
        "OBJECT_TYPE",
        "OBJECT_SCHEMA",
        "OBJECT_NAME",
        "COUNT_STAR",
        "SUM_TIMER_WAIT",
        "MIN_TIMER_WAIT",
        "AVG_TIMER_WAIT",
        "MAX_TIMER_WAIT",
    };
    static const char *const tls_channel_columns[] = {
        "CHANNEL",
        "PROPERTY",
        "VALUE",
    };
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const zero_count[] = {"0"};
    static const char *const show_columns_columns[] = {
        "Field",
        "Type",
        "Null",
        "Key",
        "Default",
        "Extra",
    };
    static const char *const show_object_columns[] = {
        "OBJECT_TYPE",    "varchar(64)",     "YES", "MUL", NULL, "",
        "OBJECT_SCHEMA",  "varchar(64)",     "YES", "",    NULL, "",
        "OBJECT_NAME",    "varchar(64)",     "YES", "",    NULL, "",
        "COUNT_STAR",     "bigint unsigned", "NO",  "",    NULL, "",
        "SUM_TIMER_WAIT", "bigint unsigned", "NO",  "",    NULL, "",
        "MIN_TIMER_WAIT", "bigint unsigned", "NO",  "",    NULL, "",
        "AVG_TIMER_WAIT", "bigint unsigned", "NO",  "",    NULL, "",
        "MAX_TIMER_WAIT", "bigint unsigned", "NO",  "",    NULL, "",
    };
    static const char *const show_tls_columns[] = {
        "CHANNEL",
        "varchar(128)",
        "NO",
        "",
        NULL,
        "",
        "PROPERTY",
        "varchar(128)",
        "NO",
        "",
        NULL,
        "",
        "VALUE",
        "varchar(2048)",
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
    static const char *const show_object_index[] = {
        "objects_summary_global_by_type",
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
        "objects_summary_global_by_type",
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
        "objects_summary_global_by_type",
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
        "IS_NULLABLE",
        "COLUMN_TYPE",
        "COLUMN_KEY",
        "COLLATION_NAME",
    };
    static const char *const information_schema_columns[] = {
        "objects_summary_global_by_type",
        "OBJECT_TYPE",
        "1",
        "YES",
        "varchar(64)",
        "MUL",
        "utf8mb4_0900_ai_ci",
        "objects_summary_global_by_type",
        "OBJECT_SCHEMA",
        "2",
        "YES",
        "varchar(64)",
        "",
        "utf8mb4_0900_ai_ci",
        "objects_summary_global_by_type",
        "OBJECT_NAME",
        "3",
        "YES",
        "varchar(64)",
        "",
        "utf8mb4_0900_ai_ci",
        "objects_summary_global_by_type",
        "COUNT_STAR",
        "4",
        "NO",
        "bigint unsigned",
        "",
        NULL,
        "objects_summary_global_by_type",
        "SUM_TIMER_WAIT",
        "5",
        "NO",
        "bigint unsigned",
        "",
        NULL,
        "objects_summary_global_by_type",
        "MIN_TIMER_WAIT",
        "6",
        "NO",
        "bigint unsigned",
        "",
        NULL,
        "objects_summary_global_by_type",
        "AVG_TIMER_WAIT",
        "7",
        "NO",
        "bigint unsigned",
        "",
        NULL,
        "objects_summary_global_by_type",
        "MAX_TIMER_WAIT",
        "8",
        "NO",
        "bigint unsigned",
        "",
        NULL,
        "tls_channel_status",
        "CHANNEL",
        "1",
        "NO",
        "varchar(128)",
        "",
        "utf8mb4_0900_ai_ci",
        "tls_channel_status",
        "PROPERTY",
        "2",
        "NO",
        "varchar(128)",
        "",
        "utf8mb4_0900_ai_ci",
        "tls_channel_status",
        "VALUE",
        "3",
        "NO",
        "varchar(2048)",
        "",
        "utf8mb4_0900_ai_ci",
    };
    static const char *const information_schema_statistics_columns[] = {
        "TABLE_NAME",
        "INDEX_NAME",
        "NON_UNIQUE",
        "SEQ_IN_INDEX",
        "COLUMN_NAME",
        "COLLATION",
        "INDEX_TYPE",
    };
    static const char *const information_schema_statistics[] = {
        "objects_summary_global_by_type", "OBJECT", "0", "1", "OBJECT_TYPE",   NULL, "HASH",
        "objects_summary_global_by_type", "OBJECT", "0", "2", "OBJECT_SCHEMA", NULL, "HASH",
        "objects_summary_global_by_type", "OBJECT", "0", "3", "OBJECT_NAME",   NULL, "HASH",
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
    static const char *const information_schema_tables[] = {
        "objects_summary_global_by_type",
        "BASE TABLE",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        "4096",
        NULL,
        "utf8mb4_0900_ai_ci",
        "tls_channel_status",
        "BASE TABLE",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        "96",
        NULL,
        "utf8mb4_0900_ai_ci",
    };
    static const char *const information_schema_constraints_columns[] = {
        "CONSTRAINT_NAME",
        "CONSTRAINT_TYPE",
        "ENFORCED",
    };
    static const char *const information_schema_constraints[] = {
        "OBJECT",
        "UNIQUE",
        "YES",
    };
    static const char *const information_schema_key_usage_columns[] = {
        "CONSTRAINT_NAME",
        "COLUMN_NAME",
        "ORDINAL_POSITION",
    };
    static const char *const information_schema_key_usage[] = {
        "OBJECT",
        "OBJECT_TYPE",
        "1",
        "OBJECT",
        "OBJECT_SCHEMA",
        "2",
        "OBJECT",
        "OBJECT_NAME",
        "3",
    };
    static const char *const information_schema_constraint_extensions_columns[] = {
        "CONSTRAINT_SCHEMA",
        "TABLE_NAME",
        "CONSTRAINT_NAME",
        "ENGINE_ATTRIBUTE",
        "SECONDARY_ENGINE_ATTRIBUTE",
    };
    static const char *const information_schema_constraint_extensions[] = {
        "performance_schema",
        "objects_summary_global_by_type",
        "OBJECT",
        NULL,
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

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM performance_schema.objects_summary_global_by_type",
            .column_names = count_column,
            .values = zero_count,
            .column_count = 1U,
            .row_count = 1U,
            .context = "objects_summary_global_by_type count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM performance_schema.objects_summary_global_by_type "
                   "WHERE OBJECT_TYPE = 'TABLE'",
            .column_names = object_summary_columns,
            .values = NULL,
            .column_count = object_summary_column_count,
            .row_count = 0U,
            .context = "objects_summary_global_by_type empty projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM performance_schema.tls_channel_status",
            .column_names = count_column,
            .values = zero_count,
            .column_count = 1U,
            .row_count = 1U,
            .context = "tls_channel_status count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql =
                "SELECT * FROM performance_schema.tls_channel_status WHERE CHANNEL = 'mysql_main'",
            .column_names = tls_channel_columns,
            .values = NULL,
            .column_count = tls_channel_column_count,
            .row_count = 0U,
            .context = "tls_channel_status empty projection",
        }
    );
    failures += execute_ok(database, "USE performance_schema");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM objects_summary_global_by_type",
            .column_names = count_column,
            .values = zero_count,
            .column_count = 1U,
            .row_count = 1U,
            .context = "selected objects_summary_global_by_type count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM tls_channel_status",
            .column_names = count_column,
            .values = zero_count,
            .column_count = 1U,
            .row_count = 1U,
            .context = "selected tls_channel_status count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM performance_schema.objects_summary_global_by_type",
            .column_names = show_columns_columns,
            .values = show_object_columns,
            .column_count = show_columns_column_count,
            .row_count = object_summary_column_count,
            .context = "show objects_summary_global_by_type columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM performance_schema.tls_channel_status",
            .column_names = show_columns_columns,
            .values = show_tls_columns,
            .column_count = show_columns_column_count,
            .row_count = tls_channel_column_count,
            .context = "show tls_channel_status columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM performance_schema.objects_summary_global_by_type",
            .column_names = show_index_columns,
            .values = show_object_index,
            .column_count = show_index_column_count,
            .row_count = object_summary_index_row_count,
            .context = "show objects_summary_global_by_type index",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM performance_schema.tls_channel_status",
            .column_names = show_index_columns,
            .values = NULL,
            .column_count = show_index_column_count,
            .row_count = 0U,
            .context = "show tls_channel_status index",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW TABLE STATUS FROM performance_schema "
                   "LIKE 'objects_summary_global_by_type'",
            .column_names = show_table_status_columns,
            .values = NULL,
            .column_count = show_table_status_column_count,
            .row_count = 1U,
            .context = "show table status objects_summary_global_by_type",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW TABLE STATUS FROM performance_schema LIKE 'tls_channel_status'",
            .column_names = show_table_status_columns,
            .values = NULL,
            .column_count = show_table_status_column_count,
            .row_count = 1U,
            .context = "show table status tls_channel_status",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, "
                   "COLUMN_TYPE, COLUMN_KEY, COLLATION_NAME "
                   "FROM information_schema.columns "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME IN ('objects_summary_global_by_type','tls_channel_status') "
                   "ORDER BY TABLE_NAME, ORDINAL_POSITION",
            .column_names = information_schema_columns_columns,
            .values = information_schema_columns,
            .column_count = information_schema_columns_projection_count,
            .row_count = object_summary_column_count + tls_channel_column_count,
            .context = "information schema object/tls columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME, "
                   "COLLATION, INDEX_TYPE "
                   "FROM information_schema.statistics "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME IN ('objects_summary_global_by_type','tls_channel_status') "
                   "ORDER BY TABLE_NAME, INDEX_NAME, SEQ_IN_INDEX",
            .column_names = information_schema_statistics_columns,
            .values = information_schema_statistics,
            .column_count = information_schema_statistics_projection_count,
            .row_count = object_summary_index_row_count,
            .context = "information schema object/tls statistics",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, ROW_FORMAT, TABLE_ROWS, "
                   "AUTO_INCREMENT, TABLE_COLLATION "
                   "FROM information_schema.tables "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME IN ('objects_summary_global_by_type','tls_channel_status') "
                   "ORDER BY TABLE_NAME",
            .column_names = information_schema_tables_columns,
            .values = information_schema_tables,
            .column_count = information_schema_tables_projection_count,
            .row_count = 2U,
            .context = "information schema object/tls tables",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED "
                   "FROM information_schema.table_constraints "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME IN ('objects_summary_global_by_type','tls_channel_status') "
                   "ORDER BY TABLE_NAME, CONSTRAINT_NAME",
            .column_names = information_schema_constraints_columns,
            .values = information_schema_constraints,
            .column_count = information_schema_constraints_projection_count,
            .row_count = 1U,
            .context = "information schema object/tls constraints",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION "
                   "FROM information_schema.key_column_usage "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME IN ('objects_summary_global_by_type','tls_channel_status') "
                   "ORDER BY TABLE_NAME, CONSTRAINT_NAME, ORDINAL_POSITION",
            .column_names = information_schema_key_usage_columns,
            .values = information_schema_key_usage,
            .column_count = information_schema_key_usage_projection_count,
            .row_count = object_summary_index_row_count,
            .context = "information schema object/tls key usage",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONSTRAINT_SCHEMA, TABLE_NAME, CONSTRAINT_NAME, "
                   "ENGINE_ATTRIBUTE, SECONDARY_ENGINE_ATTRIBUTE "
                   "FROM information_schema.table_constraints_extensions "
                   "WHERE CONSTRAINT_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME IN ('objects_summary_global_by_type','tls_channel_status') "
                   "ORDER BY TABLE_NAME, CONSTRAINT_NAME",
            .column_names = information_schema_constraint_extensions_columns,
            .values = information_schema_constraint_extensions,
            .column_count = information_schema_constraint_extensions_projection_count,
            .row_count = 1U,
            .context = "information schema object/tls constraint extensions",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO performance_schema.objects_summary_global_by_type (OBJECT_TYPE) "
        "VALUES ('TABLE')",
        (struct expected_sql_error){
            .code = mysql_error_access_denied,
            .sqlstate = "42000",
            .message_part = "Access denied",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO performance_schema.tls_channel_status (CHANNEL) VALUES ('mysql_main')",
        (struct expected_sql_error){
            .code = mysql_error_access_denied,
            .sqlstate = "42000",
            .message_part = "Access denied",
        }
    );
    failures += expect_row_count_state(database, "row count after object/tls write errors");

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
