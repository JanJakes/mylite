#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    mysql_error_access_denied = 1044,
    setup_objects_column_count = 5,
    setup_objects_row_count = 20,
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

static int test_performance_schema_setup_objects(void);
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
    return test_performance_schema_setup_objects() == 0 ? 0 : 1;
}

static int test_performance_schema_setup_objects(void) {
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const setup_object_columns[] = {
        "OBJECT_TYPE",
        "OBJECT_SCHEMA",
        "OBJECT_NAME",
        "ENABLED",
        "TIMED",
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
    static const char *const count_twenty[] = {"20"};
    static const char *const setup_object_rows[] = {
        "EVENT",     "%",     "%", "YES", "YES", "EVENT",     "information_schema", "%", "NO", "NO",
        "EVENT",     "mysql", "%", "NO",  "NO",  "EVENT",     "performance_schema", "%", "NO", "NO",
        "FUNCTION",  "%",     "%", "YES", "YES", "FUNCTION",  "information_schema", "%", "NO", "NO",
        "FUNCTION",  "mysql", "%", "NO",  "NO",  "FUNCTION",  "performance_schema", "%", "NO", "NO",
        "PROCEDURE", "%",     "%", "YES", "YES", "PROCEDURE", "information_schema", "%", "NO", "NO",
        "PROCEDURE", "mysql", "%", "NO",  "NO",  "PROCEDURE", "performance_schema", "%", "NO", "NO",
        "TABLE",     "%",     "%", "YES", "YES", "TABLE",     "information_schema", "%", "NO", "NO",
        "TABLE",     "mysql", "%", "NO",  "NO",  "TABLE",     "performance_schema", "%", "NO", "NO",
        "TRIGGER",   "%",     "%", "YES", "YES", "TRIGGER",   "information_schema", "%", "NO", "NO",
        "TRIGGER",   "mysql", "%", "NO",  "NO",  "TRIGGER",   "performance_schema", "%", "NO", "NO",
    };
    static const char *const selected_schema_object[] = {"NO", "NO"};
    static const char *const full_columns[] = {
        "OBJECT_TYPE",
        "enum('EVENT','FUNCTION','PROCEDURE','TABLE','TRIGGER')",
        "utf8mb4_0900_ai_ci",
        "NO",
        "MUL",
        "TABLE",
        "",
        "select,insert,update,references",
        "",
        "OBJECT_SCHEMA",
        "varchar(64)",
        "utf8mb4_0900_ai_ci",
        "YES",
        "",
        "%",
        "",
        "select,insert,update,references",
        "",
        "OBJECT_NAME",
        "varchar(64)",
        "utf8mb4_0900_ai_ci",
        "NO",
        "",
        "%",
        "",
        "select,insert,update,references",
        "",
        "ENABLED",
        "enum('YES','NO')",
        "utf8mb4_0900_ai_ci",
        "NO",
        "",
        "YES",
        "",
        "select,insert,update,references",
        "",
        "TIMED",
        "enum('YES','NO')",
        "utf8mb4_0900_ai_ci",
        "NO",
        "",
        "YES",
        "",
        "select,insert,update,references",
        "",
    };
    static const char *const show_index[] = {
        "setup_objects",
        "0",
        "OBJECT",
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
        "setup_objects",
        "0",
        "OBJECT",
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
        "setup_objects",
        "0",
        "OBJECT",
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
    static const char *const information_schema_columns[] = {
        "OBJECT_TYPE",
        "1",
        "NO",
        "enum('EVENT','FUNCTION','PROCEDURE','TABLE','TRIGGER')",
        "MUL",
        "TABLE",
        "utf8mb4_0900_ai_ci",
        "OBJECT_SCHEMA",
        "2",
        "YES",
        "varchar(64)",
        "",
        "%",
        "utf8mb4_0900_ai_ci",
        "OBJECT_NAME",
        "3",
        "NO",
        "varchar(64)",
        "",
        "%",
        "utf8mb4_0900_ai_ci",
        "ENABLED",
        "4",
        "NO",
        "enum('YES','NO')",
        "",
        "YES",
        "utf8mb4_0900_ai_ci",
        "TIMED",
        "5",
        "NO",
        "enum('YES','NO')",
        "",
        "YES",
        "utf8mb4_0900_ai_ci",
    };
    static const char *const information_schema_statistics[] = {
        "OBJECT", "0", "1", "OBJECT_TYPE",   NULL, NULL, "HASH",
        "OBJECT", "0", "2", "OBJECT_SCHEMA", NULL, NULL, "HASH",
        "OBJECT", "0", "3", "OBJECT_NAME",   NULL, NULL, "HASH",
    };
    static const char *const information_schema_tables[] = {
        "setup_objects",
        "BASE TABLE",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        "128",
        NULL,
        "utf8mb4_0900_ai_ci",
    };
    static const char *const information_schema_constraints[] = {
        "OBJECT",
        "UNIQUE",
        "YES",
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
    static const char *const information_schema_constraint_extensions[] = {
        "performance_schema",
        "setup_objects",
        "OBJECT",
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
            .sql = "SELECT COUNT(*) FROM performance_schema.setup_objects",
            .column_names = count_column,
            .values = count_twenty,
            .column_count = 1U,
            .row_count = 1U,
            .context = "setup_objects row count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME, ENABLED, TIMED "
                   "FROM performance_schema.setup_objects "
                   "ORDER BY OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME",
            .column_names = setup_object_columns,
            .values = setup_object_rows,
            .column_count = setup_objects_column_count,
            .row_count = setup_objects_row_count,
            .context = "setup_objects rows",
        }
    );
    failures += execute_ok(database, "USE performance_schema");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ENABLED, TIMED FROM setup_objects "
                   "WHERE OBJECT_TYPE = 'TABLE' AND OBJECT_SCHEMA = 'mysql'",
            .column_names = (const char *const[]){"ENABLED", "TIMED"},
            .values = selected_schema_object,
            .column_count = 2U,
            .row_count = 1U,
            .context = "selected performance_schema table resolution",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW FULL COLUMNS FROM performance_schema.setup_objects",
            .column_names = show_full_columns_columns,
            .values = full_columns,
            .column_count = show_full_columns_column_count,
            .row_count = setup_objects_column_count,
            .context = "setup_objects show full columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM performance_schema.setup_objects",
            .column_names = show_index_columns,
            .values = show_index,
            .column_count = show_index_column_count,
            .row_count = 3U,
            .context = "setup_objects show index",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW TABLE STATUS FROM performance_schema "
                   "WHERE Name = 'setup_objects' "
                   "AND Engine = 'PERFORMANCE_SCHEMA' "
                   "AND Row_format = 'Dynamic' "
                   "AND Auto_increment IS NULL "
                   "AND Collation = 'utf8mb4_0900_ai_ci'",
            .column_names = show_table_status_columns,
            .values = NULL,
            .column_count = show_table_status_column_count,
            .row_count = 1U,
            .context = "setup_objects show table status",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, COLUMN_TYPE, "
                   "COLUMN_KEY, COLUMN_DEFAULT, COLLATION_NAME "
                   "FROM information_schema.columns "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME = 'setup_objects' "
                   "ORDER BY ORDINAL_POSITION",
            .column_names = information_schema_columns_columns,
            .values = information_schema_columns,
            .column_count = information_schema_columns_projection_count,
            .row_count = setup_objects_column_count,
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
                   "AND TABLE_NAME = 'setup_objects' "
                   "ORDER BY INDEX_NAME, SEQ_IN_INDEX",
            .column_names = information_schema_statistics_columns,
            .values = information_schema_statistics,
            .column_count = information_schema_statistics_projection_count,
            .row_count = 3U,
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
                   "AND TABLE_NAME = 'setup_objects'",
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
                   "AND TABLE_NAME = 'setup_objects'",
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
                   "AND TABLE_NAME = 'setup_objects' "
                   "ORDER BY ORDINAL_POSITION",
            .column_names = information_schema_key_usage_columns,
            .values = information_schema_key_usage,
            .column_count = information_schema_key_usage_projection_count,
            .row_count = 3U,
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
                   "AND TABLE_NAME = 'setup_objects'",
            .column_names = information_schema_constraint_extensions_columns,
            .values = information_schema_constraint_extensions,
            .column_count = information_schema_constraint_extensions_projection_count,
            .row_count = 1U,
            .context = "information schema table constraint extensions",
        }
    );
    failures += execute_error(
        database,
        "UPDATE performance_schema.setup_objects SET ENABLED = 'NO'",
        access_denied
    );
    failures += expect_row_count_state(database, "row count after select");

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
        expected == NULL ? "NULL" : expected,
        actual == NULL ? "NULL" : actual
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
        actual == NULL ? "NULL" : actual,
        needle == NULL ? "NULL" : needle
    );
    return 1;
}
