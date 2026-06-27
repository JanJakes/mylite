#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    mysql_error_access_denied = 1044,
    setup_loggers_column_count = 3,
    setup_meters_column_count = 4,
    setup_meters_row_count = 12,
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

static int test_performance_schema_setup_logger_meter_tables(void);
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
    return test_performance_schema_setup_logger_meter_tables() == 0 ? 0 : 1;
}

static int test_performance_schema_setup_logger_meter_tables(void) {
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const level_column[] = {"LEVEL"};
    static const char *const frequency_column[] = {"FREQUENCY"};
    static const char *const setup_logger_columns[] = {
        "NAME",
        "LEVEL",
        "DESCRIPTION",
    };
    static const char *const setup_meter_columns[] = {
        "NAME",
        "FREQUENCY",
        "ENABLED",
        "DESCRIPTION",
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
    static const char *const one_count[] = {"1"};
    static const char *const twelve_count[] = {"12"};
    static const char *const logger_level[] = {"info"};
    static const char *const meter_frequency[] = {"10"};
    static const char *const logger_rows[] = {
        "logger/error/error_log",
        "info",
        "MySQL error logger",
    };
    static const char *const meter_rows[] = {
        "mysql.inno",
        "10",
        "YES",
        "MySql InnoDB metrics",
        "mysql.inno.buffer_pool",
        "10",
        "YES",
        "MySql InnoDB buffer pool metrics",
        "mysql.inno.data",
        "10",
        "YES",
        "MySql InnoDB data metrics",
        "mysql.myisam",
        "10",
        "YES",
        "MySql MyISAM storage engine stats",
        "mysql.perf_schema",
        "10",
        "YES",
        "MySql performance_schema lost instruments",
        "mysql.stats",
        "10",
        "YES",
        "MySql core metrics",
        "mysql.stats.com",
        "10",
        "YES",
        "MySql command stats",
        "mysql.stats.connection",
        "10",
        "YES",
        "MySql connection stats",
        "mysql.stats.handler",
        "10",
        "YES",
        "MySql handler stats",
        "mysql.stats.ssl",
        "10",
        "YES",
        "MySql TLS related stats",
        "mysql.x",
        "10",
        "YES",
        "MySql X plugin metrics",
        "mysql.x.stmt",
        "10",
        "YES",
        "MySql X plugin statement statistics",
    };
    static const char *const logger_full_columns[] = {
        "NAME",
        "varchar(128)",
        "utf8mb4_0900_ai_ci",
        "NO",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "LEVEL",
        "enum('none','error','warn','info','debug')",
        "utf8mb4_0900_ai_ci",
        "NO",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "DESCRIPTION",
        "varchar(1023)",
        "utf8mb4_0900_ai_ci",
        "YES",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
    };
    static const char *const meter_full_columns[] = {
        "NAME",
        "varchar(63)",
        "utf8mb4_0900_ai_ci",
        "NO",
        "PRI",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "FREQUENCY",
        "mediumint unsigned",
        NULL,
        "NO",
        "",
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
        "DESCRIPTION",
        "varchar(1023)",
        "utf8mb4_0900_ai_ci",
        "YES",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
    };
    static const char *const meter_show_index[] = {
        "setup_meters",
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
    static const char *const logger_information_schema_columns[] = {
        "NAME",
        "1",
        "NO",
        "varchar(128)",
        "",
        NULL,
        "utf8mb4_0900_ai_ci",
        "LEVEL",
        "2",
        "NO",
        "enum('none','error','warn','info','debug')",
        "",
        NULL,
        "utf8mb4_0900_ai_ci",
        "DESCRIPTION",
        "3",
        "YES",
        "varchar(1023)",
        "",
        NULL,
        "utf8mb4_0900_ai_ci",
    };
    static const char *const meter_information_schema_columns[] = {
        "NAME",        "1", "NO",  "varchar(63)",        "PRI", NULL, "utf8mb4_0900_ai_ci",
        "FREQUENCY",   "2", "NO",  "mediumint unsigned", "",    NULL, NULL,
        "ENABLED",     "3", "NO",  "enum('YES','NO')",   "",    NULL, "utf8mb4_0900_ai_ci",
        "DESCRIPTION", "4", "YES", "varchar(1023)",      "",    NULL, "utf8mb4_0900_ai_ci",
    };
    static const char *const meter_information_schema_statistics[] = {
        "PRIMARY",
        "0",
        "1",
        "NAME",
        NULL,
        NULL,
        "HASH",
    };
    static const char *const logger_information_schema_tables[] = {
        "setup_loggers",
        "BASE TABLE",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        "1",
        NULL,
        "utf8mb4_0900_ai_ci",
    };
    static const char *const meter_information_schema_tables[] = {
        "setup_meters",
        "BASE TABLE",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        "12",
        NULL,
        "utf8mb4_0900_ai_ci",
    };
    static const char *const meter_information_schema_constraints[] = {
        "PRIMARY",
        "PRIMARY KEY",
        "YES",
    };
    static const char *const meter_information_schema_key_usage[] = {
        "PRIMARY",
        "NAME",
        "1",
    };
    static const char *const meter_information_schema_constraint_extensions[] = {
        "performance_schema",
        "setup_meters",
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
            .sql = "SELECT COUNT(*) FROM performance_schema.setup_loggers",
            .column_names = count_column,
            .values = one_count,
            .column_count = 1U,
            .row_count = 1U,
            .context = "setup_loggers row count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT NAME, LEVEL, DESCRIPTION "
                   "FROM performance_schema.setup_loggers "
                   "ORDER BY NAME",
            .column_names = setup_logger_columns,
            .values = logger_rows,
            .column_count = setup_loggers_column_count,
            .row_count = 1U,
            .context = "setup_loggers rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM performance_schema.setup_meters",
            .column_names = count_column,
            .values = twelve_count,
            .column_count = 1U,
            .row_count = 1U,
            .context = "setup_meters row count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT NAME, FREQUENCY, ENABLED, DESCRIPTION "
                   "FROM performance_schema.setup_meters "
                   "ORDER BY NAME",
            .column_names = setup_meter_columns,
            .values = meter_rows,
            .column_count = setup_meters_column_count,
            .row_count = setup_meters_row_count,
            .context = "setup_meters rows",
        }
    );
    failures += execute_ok(database, "USE performance_schema");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT LEVEL FROM setup_loggers WHERE NAME = 'logger/error/error_log'",
            .column_names = level_column,
            .values = logger_level,
            .column_count = 1U,
            .row_count = 1U,
            .context = "selected schema setup_loggers",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT FREQUENCY FROM setup_meters WHERE NAME = 'mysql.stats'",
            .column_names = frequency_column,
            .values = meter_frequency,
            .column_count = 1U,
            .row_count = 1U,
            .context = "selected schema setup_meters",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW FULL COLUMNS FROM performance_schema.setup_loggers",
            .column_names = show_full_columns_columns,
            .values = logger_full_columns,
            .column_count = show_full_columns_column_count,
            .row_count = setup_loggers_column_count,
            .context = "setup_loggers show full columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM performance_schema.setup_loggers",
            .column_names = show_index_columns,
            .values = NULL,
            .column_count = show_index_column_count,
            .row_count = 0U,
            .context = "setup_loggers show index",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW TABLE STATUS FROM performance_schema "
                   "WHERE Name = 'setup_loggers' "
                   "AND Engine = 'PERFORMANCE_SCHEMA' "
                   "AND Row_format = 'Dynamic' "
                   "AND Auto_increment IS NULL "
                   "AND Collation = 'utf8mb4_0900_ai_ci'",
            .column_names = show_table_status_columns,
            .values = NULL,
            .column_count = show_table_status_column_count,
            .row_count = 1U,
            .context = "setup_loggers show table status",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW FULL COLUMNS FROM performance_schema.setup_meters",
            .column_names = show_full_columns_columns,
            .values = meter_full_columns,
            .column_count = show_full_columns_column_count,
            .row_count = setup_meters_column_count,
            .context = "setup_meters show full columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM performance_schema.setup_meters",
            .column_names = show_index_columns,
            .values = meter_show_index,
            .column_count = show_index_column_count,
            .row_count = 1U,
            .context = "setup_meters show index",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW TABLE STATUS FROM performance_schema "
                   "WHERE Name = 'setup_meters' "
                   "AND Engine = 'PERFORMANCE_SCHEMA' "
                   "AND Row_format = 'Dynamic' "
                   "AND Auto_increment IS NULL "
                   "AND Collation = 'utf8mb4_0900_ai_ci'",
            .column_names = show_table_status_columns,
            .values = NULL,
            .column_count = show_table_status_column_count,
            .row_count = 1U,
            .context = "setup_meters show table status",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, COLUMN_TYPE, "
                   "COLUMN_KEY, COLUMN_DEFAULT, COLLATION_NAME "
                   "FROM information_schema.columns "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME = 'setup_loggers' "
                   "ORDER BY ORDINAL_POSITION",
            .column_names = information_schema_columns_columns,
            .values = logger_information_schema_columns,
            .column_count = information_schema_columns_projection_count,
            .row_count = setup_loggers_column_count,
            .context = "setup_loggers information schema columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, COLUMN_TYPE, "
                   "COLUMN_KEY, COLUMN_DEFAULT, COLLATION_NAME "
                   "FROM information_schema.columns "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME = 'setup_meters' "
                   "ORDER BY ORDINAL_POSITION",
            .column_names = information_schema_columns_columns,
            .values = meter_information_schema_columns,
            .column_count = information_schema_columns_projection_count,
            .row_count = setup_meters_column_count,
            .context = "setup_meters information schema columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, ROW_FORMAT, TABLE_ROWS, "
                   "AUTO_INCREMENT, TABLE_COLLATION "
                   "FROM information_schema.tables "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME = 'setup_loggers'",
            .column_names = information_schema_tables_columns,
            .values = logger_information_schema_tables,
            .column_count = information_schema_tables_projection_count,
            .row_count = 1U,
            .context = "setup_loggers information schema tables",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, ROW_FORMAT, TABLE_ROWS, "
                   "AUTO_INCREMENT, TABLE_COLLATION "
                   "FROM information_schema.tables "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME = 'setup_meters'",
            .column_names = information_schema_tables_columns,
            .values = meter_information_schema_tables,
            .column_count = information_schema_tables_projection_count,
            .row_count = 1U,
            .context = "setup_meters information schema tables",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM information_schema.statistics "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME = 'setup_loggers'",
            .column_names = count_column,
            .values = zero_count,
            .column_count = 1U,
            .row_count = 1U,
            .context = "setup_loggers information schema statistics",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME, "
                   "COLLATION, CARDINALITY, INDEX_TYPE "
                   "FROM information_schema.statistics "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME = 'setup_meters'",
            .column_names = information_schema_statistics_columns,
            .values = meter_information_schema_statistics,
            .column_count = information_schema_statistics_projection_count,
            .row_count = 1U,
            .context = "setup_meters information schema statistics",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED "
                   "FROM information_schema.table_constraints "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME = 'setup_meters'",
            .column_names = information_schema_constraints_columns,
            .values = meter_information_schema_constraints,
            .column_count = information_schema_constraints_projection_count,
            .row_count = 1U,
            .context = "setup_meters information schema table constraints",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION "
                   "FROM information_schema.key_column_usage "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME = 'setup_meters'",
            .column_names = information_schema_key_usage_columns,
            .values = meter_information_schema_key_usage,
            .column_count = information_schema_key_usage_projection_count,
            .row_count = 1U,
            .context = "setup_meters information schema key column usage",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONSTRAINT_SCHEMA, TABLE_NAME, CONSTRAINT_NAME, "
                   "ENGINE_ATTRIBUTE, SECONDARY_ENGINE_ATTRIBUTE "
                   "FROM information_schema.table_constraints_extensions "
                   "WHERE CONSTRAINT_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME = 'setup_meters'",
            .column_names = information_schema_constraint_extensions_columns,
            .values = meter_information_schema_constraint_extensions,
            .column_count = information_schema_constraint_extensions_projection_count,
            .row_count = 1U,
            .context = "setup_meters information schema table constraint extensions",
        }
    );
    failures += execute_error(
        database,
        "UPDATE performance_schema.setup_meters SET ENABLED = 'NO'",
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
