#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdio.h>
#include <string.h>

enum {
    mysql_error_access_denied = 1044,
    table_handles_column_count = 8,
    table_handles_index_row_count = 6,
    show_columns_column_count = 6,
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

static int test_performance_schema_table_handles(void);
static int execute_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query query);
static int expect_row_count_state(mylite_db *database, const char *context);
static int expect_int(int actual, int expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

int main(void) {
    return test_performance_schema_table_handles() == 0 ? 0 : 1;
}

static int test_performance_schema_table_handles(void) {
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const table_handle_columns[] = {
        "OBJECT_TYPE",
        "OBJECT_SCHEMA",
        "OBJECT_NAME",
        "OBJECT_INSTANCE_BEGIN",
        "OWNER_THREAD_ID",
        "OWNER_EVENT_ID",
        "INTERNAL_LOCK",
        "EXTERNAL_LOCK",
    };
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
    static const char *const zero_count[] = {"0"};
    static const char *const show_columns[] = {
        "OBJECT_TYPE",           "varchar(64)",     "NO",  "MUL", NULL, "",
        "OBJECT_SCHEMA",         "varchar(64)",     "NO",  "",    NULL, "",
        "OBJECT_NAME",           "varchar(64)",     "NO",  "",    NULL, "",
        "OBJECT_INSTANCE_BEGIN", "bigint unsigned", "NO",  "PRI", NULL, "",
        "OWNER_THREAD_ID",       "bigint unsigned", "YES", "MUL", NULL, "",
        "OWNER_EVENT_ID",        "bigint unsigned", "YES", "",    NULL, "",
        "INTERNAL_LOCK",         "varchar(64)",     "YES", "",    NULL, "",
        "EXTERNAL_LOCK",         "varchar(64)",     "YES", "",    NULL, "",
    };
    static const char *const full_columns[] = {
        "OBJECT_TYPE",
        "varchar(64)",
        "utf8mb4_0900_ai_ci",
        "NO",
        "MUL",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "OBJECT_SCHEMA",
        "varchar(64)",
        "utf8mb4_0900_ai_ci",
        "NO",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "OBJECT_NAME",
        "varchar(64)",
        "utf8mb4_0900_ai_ci",
        "NO",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "OBJECT_INSTANCE_BEGIN",
        "bigint unsigned",
        NULL,
        "NO",
        "PRI",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "OWNER_THREAD_ID",
        "bigint unsigned",
        NULL,
        "YES",
        "MUL",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "OWNER_EVENT_ID",
        "bigint unsigned",
        NULL,
        "YES",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "INTERNAL_LOCK",
        "varchar(64)",
        "utf8mb4_0900_ai_ci",
        "YES",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "EXTERNAL_LOCK",
        "varchar(64)",
        "utf8mb4_0900_ai_ci",
        "YES",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
    };
    static const char *const show_index[] = {
        "table_handles",
        "0",
        "PRIMARY",
        "1",
        "OBJECT_INSTANCE_BEGIN",
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
        "table_handles",
        "1",
        "OBJECT_TYPE",
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
        "table_handles",
        "1",
        "OBJECT_TYPE",
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
        "table_handles",
        "1",
        "OBJECT_TYPE",
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
        "table_handles",
        "1",
        "OWNER_THREAD_ID",
        "1",
        "OWNER_THREAD_ID",
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
        "table_handles",
        "1",
        "OWNER_THREAD_ID",
        "2",
        "OWNER_EVENT_ID",
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
    static const char *const information_schema_columns[] = {
        "OBJECT_TYPE",           "1", "NO",  "varchar(64)",     "MUL", NULL, "utf8mb4_0900_ai_ci",
        "OBJECT_SCHEMA",         "2", "NO",  "varchar(64)",     "",    NULL, "utf8mb4_0900_ai_ci",
        "OBJECT_NAME",           "3", "NO",  "varchar(64)",     "",    NULL, "utf8mb4_0900_ai_ci",
        "OBJECT_INSTANCE_BEGIN", "4", "NO",  "bigint unsigned", "PRI", NULL, NULL,
        "OWNER_THREAD_ID",       "5", "YES", "bigint unsigned", "MUL", NULL, NULL,
        "OWNER_EVENT_ID",        "6", "YES", "bigint unsigned", "",    NULL, NULL,
        "INTERNAL_LOCK",         "7", "YES", "varchar(64)",     "",    NULL, "utf8mb4_0900_ai_ci",
        "EXTERNAL_LOCK",         "8", "YES", "varchar(64)",     "",    NULL, "utf8mb4_0900_ai_ci",
    };
    static const char *const information_schema_statistics[] = {
        "OBJECT_TYPE",     "1", "1", "OBJECT_TYPE",           NULL, NULL, "HASH",
        "OBJECT_TYPE",     "1", "2", "OBJECT_SCHEMA",         NULL, NULL, "HASH",
        "OBJECT_TYPE",     "1", "3", "OBJECT_NAME",           NULL, NULL, "HASH",
        "OWNER_THREAD_ID", "1", "1", "OWNER_THREAD_ID",       NULL, NULL, "HASH",
        "OWNER_THREAD_ID", "1", "2", "OWNER_EVENT_ID",        NULL, NULL, "HASH",
        "PRIMARY",         "0", "1", "OBJECT_INSTANCE_BEGIN", NULL, NULL, "HASH",
    };
    static const char *const information_schema_tables[] = {
        "table_handles",
        "BASE TABLE",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        "0",
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
        "OBJECT_INSTANCE_BEGIN",
        "1",
    };
    static const char *const information_schema_constraint_extensions[] = {
        "performance_schema",
        "table_handles",
        "OBJECT_TYPE",
        NULL,
        NULL,
        "performance_schema",
        "table_handles",
        "OWNER_THREAD_ID",
        NULL,
        NULL,
        "performance_schema",
        "table_handles",
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
            .sql = "SELECT COUNT(*) FROM performance_schema.table_handles",
            .column_names = count_column,
            .values = zero_count,
            .column_count = 1U,
            .row_count = 1U,
            .context = "table_handles row count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM performance_schema.table_handles "
                   "WHERE OBJECT_SCHEMA = 'mysql' ORDER BY OBJECT_NAME",
            .column_names = table_handle_columns,
            .values = NULL,
            .column_count = table_handles_column_count,
            .row_count = 0U,
            .context = "table_handles empty predicate",
        }
    );
    failures += execute_ok(database, "USE performance_schema");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM table_handles",
            .column_names = count_column,
            .values = zero_count,
            .column_count = 1U,
            .row_count = 1U,
            .context = "selected schema table_handles",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM performance_schema.table_handles",
            .column_names = show_columns_columns,
            .values = show_columns,
            .column_count = show_columns_column_count,
            .row_count = table_handles_column_count,
            .context = "table_handles show columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "DESCRIBE performance_schema.table_handles",
            .column_names = show_columns_columns,
            .values = show_columns,
            .column_count = show_columns_column_count,
            .row_count = table_handles_column_count,
            .context = "table_handles describe",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW FULL COLUMNS FROM performance_schema.table_handles",
            .column_names = show_full_columns_columns,
            .values = full_columns,
            .column_count = show_full_columns_column_count,
            .row_count = table_handles_column_count,
            .context = "table_handles show full columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM performance_schema.table_handles",
            .column_names = show_index_columns,
            .values = show_index,
            .column_count = show_index_column_count,
            .row_count = table_handles_index_row_count,
            .context = "table_handles show index",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW TABLE STATUS FROM performance_schema "
                   "WHERE Name = 'table_handles' "
                   "AND Engine = 'PERFORMANCE_SCHEMA' "
                   "AND Row_format = 'Dynamic' "
                   "AND Auto_increment IS NULL "
                   "AND Collation = 'utf8mb4_0900_ai_ci'",
            .column_names = show_table_status_columns,
            .values = NULL,
            .column_count = show_table_status_column_count,
            .row_count = 1U,
            .context = "table_handles show table status",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, COLUMN_TYPE, "
                   "COLUMN_KEY, COLUMN_DEFAULT, COLLATION_NAME "
                   "FROM information_schema.columns "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME = 'table_handles' "
                   "ORDER BY ORDINAL_POSITION",
            .column_names = information_schema_columns_columns,
            .values = information_schema_columns,
            .column_count = information_schema_columns_projection_count,
            .row_count = table_handles_column_count,
            .context = "table_handles information schema columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME, "
                   "COLLATION, CARDINALITY, INDEX_TYPE "
                   "FROM information_schema.statistics "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME = 'table_handles' "
                   "ORDER BY INDEX_NAME, SEQ_IN_INDEX",
            .column_names = information_schema_statistics_columns,
            .values = information_schema_statistics,
            .column_count = information_schema_statistics_projection_count,
            .row_count = table_handles_index_row_count,
            .context = "table_handles information schema statistics",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, ROW_FORMAT, TABLE_ROWS, "
                   "AUTO_INCREMENT, TABLE_COLLATION "
                   "FROM information_schema.tables "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME = 'table_handles'",
            .column_names = information_schema_tables_columns,
            .values = information_schema_tables,
            .column_count = information_schema_tables_projection_count,
            .row_count = 1U,
            .context = "table_handles information schema tables",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED "
                   "FROM information_schema.table_constraints "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME = 'table_handles'",
            .column_names = information_schema_constraints_columns,
            .values = information_schema_constraints,
            .column_count = information_schema_constraints_projection_count,
            .row_count = 1U,
            .context = "table_handles information schema constraints",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION "
                   "FROM information_schema.key_column_usage "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME = 'table_handles'",
            .column_names = information_schema_key_usage_columns,
            .values = information_schema_key_usage,
            .column_count = information_schema_key_usage_projection_count,
            .row_count = 1U,
            .context = "table_handles information schema key usage",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONSTRAINT_SCHEMA, TABLE_NAME, CONSTRAINT_NAME, "
                   "ENGINE_ATTRIBUTE, SECONDARY_ENGINE_ATTRIBUTE "
                   "FROM information_schema.table_constraints_extensions "
                   "WHERE CONSTRAINT_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME = 'table_handles' "
                   "ORDER BY CONSTRAINT_NAME",
            .column_names = information_schema_constraint_extensions_columns,
            .values = information_schema_constraint_extensions,
            .column_count = information_schema_constraint_extensions_projection_count,
            .row_count = 3U,
            .context = "table_handles information schema constraint extensions",
        }
    );
    failures += execute_error(
        database,
        "UPDATE performance_schema.table_handles SET INTERNAL_LOCK = 'READ'",
        access_denied
    );
    failures += expect_row_count_state(database, "row count after denied table_handles update");

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
