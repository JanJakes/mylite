#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    metadata_locks_column_count = 11,
    metadata_locks_index_row_count = 7,
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

static int test_performance_schema_metadata_locks(void);
static int execute_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query query);
static int expect_row_count_state(mylite_db *database, const char *context);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

int main(void) {
    return test_performance_schema_metadata_locks() == 0 ? 0 : 1;
}

static int test_performance_schema_metadata_locks(void) {
    static const char *const metadata_lock_columns[] = {
        "OBJECT_TYPE",
        "OBJECT_SCHEMA",
        "OBJECT_NAME",
        "COLUMN_NAME",
        "OBJECT_INSTANCE_BEGIN",
        "LOCK_TYPE",
        "LOCK_DURATION",
        "LOCK_STATUS",
        "SOURCE",
        "OWNER_THREAD_ID",
        "OWNER_EVENT_ID",
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
    static const char *const show_columns[] = {
        "OBJECT_TYPE",
        "varchar(64)",
        "NO",
        "MUL",
        NULL,
        "",
        "OBJECT_SCHEMA",
        "varchar(64)",
        "YES",
        "",
        NULL,
        "",
        "OBJECT_NAME",
        "varchar(64)",
        "YES",
        "",
        NULL,
        "",
        "COLUMN_NAME",
        "varchar(64)",
        "YES",
        "",
        NULL,
        "",
        "OBJECT_INSTANCE_BEGIN",
        "bigint unsigned",
        "NO",
        "PRI",
        NULL,
        "",
        "LOCK_TYPE",
        "varchar(32)",
        "NO",
        "",
        NULL,
        "",
        "LOCK_DURATION",
        "varchar(32)",
        "NO",
        "",
        NULL,
        "",
        "LOCK_STATUS",
        "varchar(32)",
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
        "OWNER_THREAD_ID",
        "bigint unsigned",
        "YES",
        "MUL",
        NULL,
        "",
        "OWNER_EVENT_ID",
        "bigint unsigned",
        "YES",
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
    static const char *const show_index[] = {
        "metadata_locks",
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
        "metadata_locks",
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
        "metadata_locks",
        "1",
        "OBJECT_TYPE",
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
        "metadata_locks",
        "1",
        "OBJECT_TYPE",
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
        "metadata_locks",
        "1",
        "OBJECT_TYPE",
        "4",
        "COLUMN_NAME",
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
        "metadata_locks",
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
        "metadata_locks",
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
    static const char *const information_schema_columns_columns[] = {
        "COLUMN_NAME",
        "ORDINAL_POSITION",
        "IS_NULLABLE",
        "COLUMN_TYPE",
        "COLUMN_KEY",
        "COLUMN_DEFAULT",
        "COLLATION_NAME",
    };
    static const char *const information_schema_columns[] = {
        "OBJECT_TYPE",
        "1",
        "NO",
        "varchar(64)",
        "MUL",
        NULL,
        "utf8mb4_0900_ai_ci",
        "OBJECT_SCHEMA",
        "2",
        "YES",
        "varchar(64)",
        "",
        NULL,
        "utf8mb4_0900_ai_ci",
        "OBJECT_NAME",
        "3",
        "YES",
        "varchar(64)",
        "",
        NULL,
        "utf8mb4_0900_ai_ci",
        "COLUMN_NAME",
        "4",
        "YES",
        "varchar(64)",
        "",
        NULL,
        "utf8mb4_0900_ai_ci",
        "OBJECT_INSTANCE_BEGIN",
        "5",
        "NO",
        "bigint unsigned",
        "PRI",
        NULL,
        NULL,
        "LOCK_TYPE",
        "6",
        "NO",
        "varchar(32)",
        "",
        NULL,
        "utf8mb4_0900_ai_ci",
        "LOCK_DURATION",
        "7",
        "NO",
        "varchar(32)",
        "",
        NULL,
        "utf8mb4_0900_ai_ci",
        "LOCK_STATUS",
        "8",
        "NO",
        "varchar(32)",
        "",
        NULL,
        "utf8mb4_0900_ai_ci",
        "SOURCE",
        "9",
        "YES",
        "varchar(64)",
        "",
        NULL,
        "utf8mb4_0900_ai_ci",
        "OWNER_THREAD_ID",
        "10",
        "YES",
        "bigint unsigned",
        "MUL",
        NULL,
        NULL,
        "OWNER_EVENT_ID",
        "11",
        "YES",
        "bigint unsigned",
        "",
        NULL,
        NULL,
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
    static const char *const information_schema_statistics[] = {
        "OBJECT_TYPE",     "1", "1", "OBJECT_TYPE",           NULL, NULL, "HASH",
        "OBJECT_TYPE",     "1", "2", "OBJECT_SCHEMA",         NULL, NULL, "HASH",
        "OBJECT_TYPE",     "1", "3", "OBJECT_NAME",           NULL, NULL, "HASH",
        "OBJECT_TYPE",     "1", "4", "COLUMN_NAME",           NULL, NULL, "HASH",
        "OWNER_THREAD_ID", "1", "1", "OWNER_THREAD_ID",       NULL, NULL, "HASH",
        "OWNER_THREAD_ID", "1", "2", "OWNER_EVENT_ID",        NULL, NULL, "HASH",
        "PRIMARY",         "0", "1", "OBJECT_INSTANCE_BEGIN", NULL, NULL, "HASH",
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
        "metadata_locks",
        "BASE TABLE",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        "1024",
        NULL,
        "utf8mb4_0900_ai_ci",
    };
    static const char *const information_schema_constraints_columns[] = {
        "CONSTRAINT_NAME",
        "CONSTRAINT_TYPE",
        "ENFORCED",
    };
    static const char *const information_schema_constraints[] = {
        "PRIMARY",
        "PRIMARY KEY",
        "YES",
    };
    static const char *const information_schema_key_usage_columns[] = {
        "CONSTRAINT_NAME",
        "COLUMN_NAME",
        "ORDINAL_POSITION",
    };
    static const char *const information_schema_key_usage[] = {
        "PRIMARY",
        "OBJECT_INSTANCE_BEGIN",
        "1",
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
        "metadata_locks",
        "OBJECT_TYPE",
        NULL,
        NULL,
        "performance_schema",
        "metadata_locks",
        "OWNER_THREAD_ID",
        NULL,
        NULL,
        "performance_schema",
        "metadata_locks",
        "PRIMARY",
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
            .sql = "SELECT COUNT(*) FROM performance_schema.metadata_locks",
            .column_names = count_column,
            .values = zero_count,
            .column_count = 1U,
            .row_count = 1U,
            .context = "metadata_locks count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM performance_schema.metadata_locks WHERE OBJECT_TYPE = 'TABLE'",
            .column_names = metadata_lock_columns,
            .values = NULL,
            .column_count = metadata_locks_column_count,
            .row_count = 0U,
            .context = "metadata_locks empty projection",
        }
    );
    failures += execute_ok(database, "USE performance_schema");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM metadata_locks",
            .column_names = count_column,
            .values = zero_count,
            .column_count = 1U,
            .row_count = 1U,
            .context = "selected performance_schema metadata_locks count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM performance_schema.metadata_locks",
            .column_names = show_columns_columns,
            .values = show_columns,
            .column_count = show_columns_column_count,
            .row_count = metadata_locks_column_count,
            .context = "show metadata_locks columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM performance_schema.metadata_locks",
            .column_names = show_index_columns,
            .values = show_index,
            .column_count = show_index_column_count,
            .row_count = metadata_locks_index_row_count,
            .context = "show metadata_locks index",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW TABLE STATUS FROM performance_schema "
                   "WHERE Name = 'metadata_locks' "
                   "AND Engine = 'PERFORMANCE_SCHEMA' "
                   "AND Row_format = 'Dynamic' "
                   "AND Auto_increment IS NULL "
                   "AND Collation = 'utf8mb4_0900_ai_ci'",
            .column_names = show_table_status_columns,
            .values = NULL,
            .column_count = show_table_status_column_count,
            .row_count = 1U,
            .context = "show table status metadata_locks",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, COLUMN_TYPE, "
                   "COLUMN_KEY, COLUMN_DEFAULT, COLLATION_NAME "
                   "FROM information_schema.columns "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME = 'metadata_locks' "
                   "ORDER BY ORDINAL_POSITION",
            .column_names = information_schema_columns_columns,
            .values = information_schema_columns,
            .column_count = information_schema_columns_projection_count,
            .row_count = metadata_locks_column_count,
            .context = "information schema metadata_locks columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME, COLLATION, "
                   "CARDINALITY, INDEX_TYPE "
                   "FROM information_schema.statistics "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME = 'metadata_locks' "
                   "ORDER BY INDEX_NAME, SEQ_IN_INDEX",
            .column_names = information_schema_statistics_columns,
            .values = information_schema_statistics,
            .column_count = information_schema_statistics_projection_count,
            .row_count = metadata_locks_index_row_count,
            .context = "information schema metadata_locks statistics",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, ROW_FORMAT, TABLE_ROWS, "
                   "AUTO_INCREMENT, TABLE_COLLATION "
                   "FROM information_schema.tables "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME = 'metadata_locks'",
            .column_names = information_schema_tables_columns,
            .values = information_schema_tables,
            .column_count = information_schema_tables_projection_count,
            .row_count = 1U,
            .context = "information schema metadata_locks tables",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED "
                   "FROM information_schema.table_constraints "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME = 'metadata_locks'",
            .column_names = information_schema_constraints_columns,
            .values = information_schema_constraints,
            .column_count = information_schema_constraints_projection_count,
            .row_count = 1U,
            .context = "information schema metadata_locks constraints",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION "
                   "FROM information_schema.key_column_usage "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME = 'metadata_locks'",
            .column_names = information_schema_key_usage_columns,
            .values = information_schema_key_usage,
            .column_count = information_schema_key_usage_projection_count,
            .row_count = 1U,
            .context = "information schema metadata_locks key usage",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONSTRAINT_SCHEMA, TABLE_NAME, CONSTRAINT_NAME, "
                   "ENGINE_ATTRIBUTE, SECONDARY_ENGINE_ATTRIBUTE "
                   "FROM information_schema.table_constraints_extensions "
                   "WHERE CONSTRAINT_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME = 'metadata_locks' "
                   "ORDER BY CONSTRAINT_NAME",
            .column_names = information_schema_constraint_extensions_columns,
            .values = information_schema_constraint_extensions,
            .column_count = information_schema_constraint_extensions_projection_count,
            .row_count = 3U,
            .context = "information schema metadata_locks constraint extensions",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO performance_schema.metadata_locks (OBJECT_TYPE) VALUES ('TABLE')",
        (struct expected_sql_error){
            .code = mysql_error_access_denied,
            .sqlstate = "42000",
            .message_part = "Access denied",
        }
    );
    failures += expect_row_count_state(database, "row count after metadata_locks write error");

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
