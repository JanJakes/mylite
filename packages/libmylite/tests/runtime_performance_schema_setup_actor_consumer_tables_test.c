#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    mysql_error_access_denied = 1044,
    setup_actor_column_count = 5,
    setup_actor_row_count = 1,
    setup_consumer_column_count = 2,
    setup_consumer_row_count = 16,
    show_full_columns_column_count = 9,
    show_index_column_count = 15,
    show_table_status_column_count = 18,
    information_schema_columns_projection_count = 7,
    information_schema_statistics_projection_count = 7,
    information_schema_tables_projection_count = 7,
    information_schema_constraints_projection_count = 4,
    information_schema_key_usage_projection_count = 4,
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

static int test_performance_schema_setup_actor_consumer_tables(void);
static int execute_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query query);
static int expect_row_count_state(mylite_db *database, const char *context);

int main(void) {
    return test_performance_schema_setup_actor_consumer_tables() == 0 ? 0 : 1;
}

static int test_performance_schema_setup_actor_consumer_tables(void) {
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const actor_columns[] = {"HOST", "USER", "ROLE", "ENABLED", "HISTORY"};
    static const char *const consumer_columns[] = {"NAME", "ENABLED"};
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
        "TABLE_NAME",
        "COLUMN_NAME",
        "ORDINAL_POSITION",
        "IS_NULLABLE",
        "COLUMN_TYPE",
        "COLUMN_KEY",
        "COLUMN_DEFAULT",
    };
    static const char *const information_schema_statistics_columns[] = {
        "TABLE_NAME",
        "INDEX_NAME",
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
        "TABLE_NAME",
        "CONSTRAINT_NAME",
        "CONSTRAINT_TYPE",
        "ENFORCED",
    };
    static const char *const information_schema_key_usage_columns[] = {
        "TABLE_NAME",
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
    static const char *const actor_count[] = {"1"};
    static const char *const consumer_count[] = {"16"};
    static const char *const actor_rows[] = {"%", "%", "%", "YES", "YES"};
    static const char *const consumer_rows[] = {
        "events_stages_current",
        "NO",
        "events_stages_history",
        "NO",
        "events_stages_history_long",
        "NO",
        "events_statements_cpu",
        "NO",
        "events_statements_current",
        "YES",
        "events_statements_history",
        "YES",
        "events_statements_history_long",
        "NO",
        "events_transactions_current",
        "YES",
        "events_transactions_history",
        "YES",
        "events_transactions_history_long",
        "NO",
        "events_waits_current",
        "NO",
        "events_waits_history",
        "NO",
        "events_waits_history_long",
        "NO",
        "global_instrumentation",
        "YES",
        "statements_digest",
        "YES",
        "thread_instrumentation",
        "YES",
    };
    static const char *const selected_schema_consumer[] = {"YES"};
    static const char *const actor_full_columns[] = {
        "HOST",
        "char(255)",
        "ascii_general_ci",
        "NO",
        "PRI",
        "%",
        "",
        "select,insert,update,references",
        "",
        "USER",
        "char(32)",
        "utf8mb4_bin",
        "NO",
        "PRI",
        "%",
        "",
        "select,insert,update,references",
        "",
        "ROLE",
        "char(32)",
        "utf8mb4_bin",
        "NO",
        "PRI",
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
        "HISTORY",
        "enum('YES','NO')",
        "utf8mb4_0900_ai_ci",
        "NO",
        "",
        "YES",
        "",
        "select,insert,update,references",
        "",
    };
    static const char *const consumer_full_columns[] = {
        "NAME",
        "varchar(64)",
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
    };
    static const char *const actor_show_index[] = {
        "setup_actors", "0",  "PRIMARY", "1",   "HOST",
        NULL,           NULL, NULL,      NULL,  "",
        "HASH",         "",   "",        "YES", NULL,
        "setup_actors", "0",  "PRIMARY", "2",   "USER",
        NULL,           NULL, NULL,      NULL,  "",
        "HASH",         "",   "",        "YES", NULL,
        "setup_actors", "0",  "PRIMARY", "3",   "ROLE",
        NULL,           NULL, NULL,      NULL,  "",
        "HASH",         "",   "",        "YES", NULL,
    };
    static const char *const consumer_show_index[] = {
        "setup_consumers",
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
        "setup_actors",    "HOST",    "1", "NO", "char(255)",        "PRI", "%",
        "setup_actors",    "USER",    "2", "NO", "char(32)",         "PRI", "%",
        "setup_actors",    "ROLE",    "3", "NO", "char(32)",         "PRI", "%",
        "setup_actors",    "ENABLED", "4", "NO", "enum('YES','NO')", "",    "YES",
        "setup_actors",    "HISTORY", "5", "NO", "enum('YES','NO')", "",    "YES",
        "setup_consumers", "NAME",    "1", "NO", "varchar(64)",      "PRI", NULL,
        "setup_consumers", "ENABLED", "2", "NO", "enum('YES','NO')", "",    NULL,
    };
    static const char *const information_schema_statistics[] = {
        "setup_actors",    "PRIMARY", "1", "HOST", NULL, NULL, "HASH",
        "setup_actors",    "PRIMARY", "2", "USER", NULL, NULL, "HASH",
        "setup_actors",    "PRIMARY", "3", "ROLE", NULL, NULL, "HASH",
        "setup_consumers", "PRIMARY", "1", "NAME", NULL, NULL, "HASH",
    };
    static const char *const information_schema_tables[] = {
        "setup_actors",
        "BASE TABLE",
        "PERFORMANCE_SCHEMA",
        "Fixed",
        "128",
        NULL,
        "utf8mb4_0900_ai_ci",
        "setup_consumers",
        "BASE TABLE",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        "16",
        NULL,
        "utf8mb4_0900_ai_ci",
    };
    static const char *const information_schema_constraints[] = {
        "setup_actors",
        "PRIMARY",
        "PRIMARY KEY",
        "YES",
        "setup_consumers",
        "PRIMARY",
        "PRIMARY KEY",
        "YES",
    };
    static const char *const information_schema_key_usage[] = {
        "setup_actors",
        "PRIMARY",
        "HOST",
        "1",
        "setup_actors",
        "PRIMARY",
        "USER",
        "2",
        "setup_actors",
        "PRIMARY",
        "ROLE",
        "3",
        "setup_consumers",
        "PRIMARY",
        "NAME",
        "1",
    };
    static const char *const information_schema_constraint_extensions[] = {
        "performance_schema",
        "setup_actors",
        "PRIMARY",
        NULL,
        NULL,
        "performance_schema",
        "setup_consumers",
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
            .sql = "SELECT COUNT(*) FROM performance_schema.setup_actors",
            .column_names = count_column,
            .values = actor_count,
            .column_count = 1U,
            .row_count = 1U,
            .context = "setup_actors row count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT HOST, USER, ROLE, ENABLED, HISTORY "
                   "FROM performance_schema.setup_actors",
            .column_names = actor_columns,
            .values = actor_rows,
            .column_count = setup_actor_column_count,
            .row_count = setup_actor_row_count,
            .context = "setup_actors rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM performance_schema.setup_consumers",
            .column_names = count_column,
            .values = consumer_count,
            .column_count = 1U,
            .row_count = 1U,
            .context = "setup_consumers row count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT NAME, ENABLED FROM performance_schema.setup_consumers ORDER BY NAME",
            .column_names = consumer_columns,
            .values = consumer_rows,
            .column_count = setup_consumer_column_count,
            .row_count = setup_consumer_row_count,
            .context = "setup_consumers rows",
        }
    );
    failures += execute_ok(database, "USE performance_schema");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ENABLED FROM setup_consumers WHERE NAME = 'statements_digest'",
            .column_names = (const char *const[]){"ENABLED"},
            .values = selected_schema_consumer,
            .column_count = 1U,
            .row_count = 1U,
            .context = "selected performance_schema table resolution",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW FULL COLUMNS FROM performance_schema.setup_actors",
            .column_names = show_full_columns_columns,
            .values = actor_full_columns,
            .column_count = show_full_columns_column_count,
            .row_count = setup_actor_column_count,
            .context = "setup_actors show full columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW FULL COLUMNS FROM performance_schema.setup_consumers",
            .column_names = show_full_columns_columns,
            .values = consumer_full_columns,
            .column_count = show_full_columns_column_count,
            .row_count = setup_consumer_column_count,
            .context = "setup_consumers show full columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM performance_schema.setup_actors",
            .column_names = show_index_columns,
            .values = actor_show_index,
            .column_count = show_index_column_count,
            .row_count = 3U,
            .context = "setup_actors show index",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM performance_schema.setup_consumers",
            .column_names = show_index_columns,
            .values = consumer_show_index,
            .column_count = show_index_column_count,
            .row_count = 1U,
            .context = "setup_consumers show index",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW TABLE STATUS FROM performance_schema "
                   "WHERE Name = 'setup_actors' "
                   "AND Engine = 'PERFORMANCE_SCHEMA' "
                   "AND Row_format = 'Fixed' "
                   "AND Auto_increment IS NULL "
                   "AND Collation = 'utf8mb4_0900_ai_ci'",
            .column_names = show_table_status_columns,
            .values = NULL,
            .column_count = show_table_status_column_count,
            .row_count = 1U,
            .context = "setup_actors show table status",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW TABLE STATUS FROM performance_schema "
                   "WHERE Name = 'setup_consumers' "
                   "AND Engine = 'PERFORMANCE_SCHEMA' "
                   "AND Row_format = 'Dynamic' "
                   "AND Auto_increment IS NULL "
                   "AND Collation = 'utf8mb4_0900_ai_ci'",
            .column_names = show_table_status_columns,
            .values = NULL,
            .column_count = show_table_status_column_count,
            .row_count = 1U,
            .context = "setup_consumers show table status",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, "
                   "COLUMN_TYPE, COLUMN_KEY, COLUMN_DEFAULT "
                   "FROM information_schema.columns "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME IN ('setup_actors', 'setup_consumers') "
                   "ORDER BY TABLE_NAME, ORDINAL_POSITION",
            .column_names = information_schema_columns_columns,
            .values = information_schema_columns,
            .column_count = information_schema_columns_projection_count,
            .row_count = setup_actor_column_count + setup_consumer_column_count,
            .context = "information schema columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, INDEX_NAME, SEQ_IN_INDEX, COLUMN_NAME, "
                   "COLLATION, CARDINALITY, INDEX_TYPE "
                   "FROM information_schema.statistics "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME IN ('setup_actors', 'setup_consumers') "
                   "ORDER BY TABLE_NAME, SEQ_IN_INDEX",
            .column_names = information_schema_statistics_columns,
            .values = information_schema_statistics,
            .column_count = information_schema_statistics_projection_count,
            .row_count = 4U,
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
                   "AND TABLE_NAME IN ('setup_actors', 'setup_consumers') "
                   "ORDER BY TABLE_NAME",
            .column_names = information_schema_tables_columns,
            .values = information_schema_tables,
            .column_count = information_schema_tables_projection_count,
            .row_count = 2U,
            .context = "information schema tables",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED "
                   "FROM information_schema.table_constraints "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME IN ('setup_actors', 'setup_consumers') "
                   "ORDER BY TABLE_NAME, CONSTRAINT_NAME",
            .column_names = information_schema_constraints_columns,
            .values = information_schema_constraints,
            .column_count = information_schema_constraints_projection_count,
            .row_count = 2U,
            .context = "information schema table constraints",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION "
                   "FROM information_schema.key_column_usage "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME IN ('setup_actors', 'setup_consumers') "
                   "ORDER BY TABLE_NAME, ORDINAL_POSITION",
            .column_names = information_schema_key_usage_columns,
            .values = information_schema_key_usage,
            .column_count = information_schema_key_usage_projection_count,
            .row_count = 4U,
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
                   "AND TABLE_NAME IN ('setup_actors', 'setup_consumers') "
                   "ORDER BY TABLE_NAME, CONSTRAINT_NAME",
            .column_names = information_schema_constraint_extensions_columns,
            .values = information_schema_constraint_extensions,
            .column_count = information_schema_constraint_extensions_projection_count,
            .row_count = 2U,
            .context = "information schema table constraint extensions",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO performance_schema.setup_consumers VALUES ('x', 'YES')",
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
