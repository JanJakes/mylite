#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    mysql_error_access_denied = 1044,
    user_defined_functions_column_count = 5,
    user_defined_functions_row_count = 16,
    describe_column_count = 6,
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

static int test_performance_schema_user_defined_functions(void);
static int execute_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query query);
static int expect_row_count_state(mylite_db *database, const char *context);

int main(void) {
    return test_performance_schema_user_defined_functions() == 0 ? 0 : 1;
}

static int test_performance_schema_user_defined_functions(void) {
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const udf_return_type_column[] = {"UDF_RETURN_TYPE"};
    static const char *const user_defined_function_columns[] = {
        "UDF_NAME",
        "UDF_RETURN_TYPE",
        "UDF_TYPE",
        "UDF_LIBRARY",
        "UDF_USAGE_COUNT",
    };
    static const char *const describe_columns[] = {
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
    static const char *const count_sixteen[] = {"16"};
    static const char *const mysqlx_error_return_type[] = {"char"};
    static const char *const user_defined_function_rows[] = {
        "asynchronous_connection_failover_add_managed",
        "char",
        "function",
        NULL,
        "1",
        "asynchronous_connection_failover_add_source",
        "char",
        "function",
        NULL,
        "1",
        "asynchronous_connection_failover_delete_managed",
        "char",
        "function",
        NULL,
        "1",
        "asynchronous_connection_failover_delete_source",
        "char",
        "function",
        NULL,
        "1",
        "asynchronous_connection_failover_reset",
        "char",
        "function",
        NULL,
        "1",
        "innodb_redo_log_archive_flush",
        "integer",
        "function",
        NULL,
        "1",
        "innodb_redo_log_archive_start",
        "integer",
        "function",
        NULL,
        "1",
        "innodb_redo_log_archive_stop",
        "integer",
        "function",
        NULL,
        "1",
        "innodb_redo_log_consumer_advance",
        "integer",
        "function",
        NULL,
        "1",
        "innodb_redo_log_consumer_register",
        "integer",
        "function",
        NULL,
        "1",
        "innodb_redo_log_consumer_unregister",
        "integer",
        "function",
        NULL,
        "1",
        "innodb_redo_log_sharp_checkpoint",
        "integer",
        "function",
        NULL,
        "1",
        "innodb_set_open_files_limit",
        "integer",
        "function",
        NULL,
        "1",
        "mysqlx_error",
        "char",
        "function",
        NULL,
        "1",
        "mysqlx_generate_document_id",
        "char",
        "function",
        NULL,
        "1",
        "mysqlx_get_prepared_statement_id",
        "integer",
        "function",
        NULL,
        "1",
    };
    static const char *const describe_values[] = {
        "UDF_NAME",        "varchar(64)",   "NO",  "PRI", NULL, "",
        "UDF_RETURN_TYPE", "varchar(20)",   "NO",  "",    NULL, "",
        "UDF_TYPE",        "varchar(20)",   "NO",  "",    NULL, "",
        "UDF_LIBRARY",     "varchar(1024)", "YES", "",    NULL, "",
        "UDF_USAGE_COUNT", "bigint",        "YES", "",    NULL, "",
    };
    static const char *const full_columns[] = {
        "UDF_NAME",
        "varchar(64)",
        "utf8mb4_0900_ai_ci",
        "NO",
        "PRI",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "UDF_RETURN_TYPE",
        "varchar(20)",
        "utf8mb4_0900_ai_ci",
        "NO",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "UDF_TYPE",
        "varchar(20)",
        "utf8mb4_0900_ai_ci",
        "NO",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "UDF_LIBRARY",
        "varchar(1024)",
        "utf8mb4_0900_ai_ci",
        "YES",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "UDF_USAGE_COUNT",
        "bigint",
        NULL,
        "YES",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
    };
    static const char *const show_index[] = {
        "user_defined_functions",
        "0",
        "PRIMARY",
        "1",
        "UDF_NAME",
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
        "UDF_NAME",        "1", "NO",  "varchar(64)",   "PRI", NULL, "utf8mb4_0900_ai_ci",
        "UDF_RETURN_TYPE", "2", "NO",  "varchar(20)",   "",    NULL, "utf8mb4_0900_ai_ci",
        "UDF_TYPE",        "3", "NO",  "varchar(20)",   "",    NULL, "utf8mb4_0900_ai_ci",
        "UDF_LIBRARY",     "4", "YES", "varchar(1024)", "",    NULL, "utf8mb4_0900_ai_ci",
        "UDF_USAGE_COUNT", "5", "YES", "bigint",        "",    NULL, NULL,
    };
    static const char *const information_schema_statistics[] = {
        "PRIMARY",
        "0",
        "1",
        "UDF_NAME",
        NULL,
        NULL,
        "HASH",
    };
    static const char *const information_schema_tables[] = {
        "user_defined_functions",
        "BASE TABLE",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        "16",
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
        "UDF_NAME",
        "1",
    };
    static const char *const information_schema_constraint_extensions[] = {
        "performance_schema",
        "user_defined_functions",
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
            .sql = "SELECT COUNT(*) FROM performance_schema.user_defined_functions",
            .column_names = count_column,
            .values = count_sixteen,
            .column_count = 1U,
            .row_count = 1U,
            .context = "user_defined_functions row count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT UDF_NAME, UDF_RETURN_TYPE, UDF_TYPE, UDF_LIBRARY, "
                   "UDF_USAGE_COUNT "
                   "FROM performance_schema.user_defined_functions "
                   "ORDER BY UDF_NAME",
            .column_names = user_defined_function_columns,
            .values = user_defined_function_rows,
            .column_count = user_defined_functions_column_count,
            .row_count = user_defined_functions_row_count,
            .context = "user_defined_functions rows",
        }
    );
    failures += execute_ok(database, "USE performance_schema");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT UDF_RETURN_TYPE "
                   "FROM user_defined_functions "
                   "WHERE UDF_NAME = 'mysqlx_error'",
            .column_names = udf_return_type_column,
            .values = mysqlx_error_return_type,
            .column_count = 1U,
            .row_count = 1U,
            .context = "selected performance_schema table resolution",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "DESCRIBE performance_schema.user_defined_functions",
            .column_names = describe_columns,
            .values = describe_values,
            .column_count = describe_column_count,
            .row_count = user_defined_functions_column_count,
            .context = "describe",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW FULL COLUMNS FROM performance_schema.user_defined_functions",
            .column_names = show_full_columns_columns,
            .values = full_columns,
            .column_count = show_full_columns_column_count,
            .row_count = user_defined_functions_column_count,
            .context = "show full columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM performance_schema.user_defined_functions",
            .column_names = show_index_columns,
            .values = show_index,
            .column_count = show_index_column_count,
            .row_count = 1U,
            .context = "show index",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW TABLE STATUS FROM performance_schema "
                   "WHERE Name = 'user_defined_functions' "
                   "AND Engine = 'PERFORMANCE_SCHEMA' "
                   "AND Row_format = 'Dynamic' "
                   "AND Auto_increment IS NULL "
                   "AND Collation = 'utf8mb4_0900_ai_ci'",
            .column_names = show_table_status_columns,
            .values = NULL,
            .column_count = show_table_status_column_count,
            .row_count = 1U,
            .context = "show table status",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, COLUMN_TYPE, "
                   "COLUMN_KEY, COLUMN_DEFAULT, COLLATION_NAME "
                   "FROM information_schema.columns "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME = 'user_defined_functions' "
                   "ORDER BY ORDINAL_POSITION",
            .column_names = information_schema_columns_columns,
            .values = information_schema_columns,
            .column_count = information_schema_columns_projection_count,
            .row_count = user_defined_functions_column_count,
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
                   "AND TABLE_NAME = 'user_defined_functions'",
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
                   "AND TABLE_NAME = 'user_defined_functions'",
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
                   "AND TABLE_NAME = 'user_defined_functions'",
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
                   "AND TABLE_NAME = 'user_defined_functions'",
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
                   "AND TABLE_NAME = 'user_defined_functions'",
            .column_names = information_schema_constraint_extensions_columns,
            .values = information_schema_constraint_extensions,
            .column_count = information_schema_constraint_extensions_projection_count,
            .row_count = 1U,
            .context = "information schema table constraint extensions",
        }
    );
    failures += execute_error(
        database,
        "DELETE FROM performance_schema.user_defined_functions",
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
