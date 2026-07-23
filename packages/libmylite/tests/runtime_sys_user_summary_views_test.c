#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include <stdio.h>
#include <string.h>

enum {
    count_column_count = 1,
    metadata_column_count = 5,
    show_columns_column_count = 6,
    information_schema_views_column_count = 5,
    information_schema_views_row_count = 12,
    information_schema_view_table_usage_column_count = 3,
    information_schema_view_table_usage_row_count = 18,
    information_schema_column_probe_column_count = 4,
    show_create_view_column_count = 4,
    user_summary_show_columns_row_count = 12,
    file_io_type_show_columns_row_count = 5,
};

struct expected_query {
    const char *sql;
    const char *const *column_names;
    size_t column_count;
    const char *const *values;
    size_t row_count;
    const char *context;
};

struct expected_query_contains {
    const char *sql;
    const char *const *column_names;
    size_t column_count;
    size_t row_count;
    size_t row_index;
    size_t column_index;
    const char *needle;
    const char *context;
};

static int test_sys_user_summary_views(void);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_query_contains(mylite_db *database, struct expected_query_contains expected);
static const char *const count_column[count_column_count] = {
    "COUNT(*)",
};

static const char *const count_zero[] = {
    "0",
};

static const char *const count_92[] = {
    "92",
};

static const char *const row_count_column[] = {
    "ROW_COUNT()",
};

static const char *const row_count_minus_one[] = {
    "-1",
};

static const char *const metadata_columns[metadata_column_count] = {
    "TABLE_NAME",
    "TABLE_TYPE",
    "ENGINE",
    "TABLE_ROWS",
    "TABLE_COMMENT",
};

static const char *const show_columns_columns[show_columns_column_count] = {
    "Field",
    "Type",
    "Null",
    "Key",
    "Default",
    "Extra",
};

static const char *const information_schema_views_columns[information_schema_views_column_count] = {
    "TABLE_NAME",
    "CHECK_OPTION",
    "IS_UPDATABLE",
    "DEFINER",
    "SECURITY_TYPE",
};

static const char *const
    information_schema_view_table_usage_columns[information_schema_view_table_usage_column_count] =
        {
            "VIEW_NAME",
            "TABLE_SCHEMA",
            "TABLE_NAME",
};

static const char
    *const information_schema_column_probe_columns[information_schema_column_probe_column_count] = {
        "COLUMN_NAME",
        "COLUMN_TYPE",
        "IS_NULLABLE",
        "COLUMN_DEFAULT",
};

static const char *const show_create_view_columns[show_create_view_column_count] = {
    "View",
    "Create View",
    "character_set_client",
    "collation_connection",
};

int main(void) {
    return test_sys_user_summary_views() == 0 ? 0 : 1;
}

static int test_sys_user_summary_views(void) {
    static const char *const user_summary_show_columns_values[] = {
        "user",
        "varchar(32)",
        "YES",
        "",
        NULL,
        "",
        "statements",
        "decimal(64,0)",
        "YES",
        "",
        NULL,
        "",
        "statement_latency",
        "varchar(11)",
        "YES",
        "",
        NULL,
        "",
        "statement_avg_latency",
        "varchar(11)",
        "YES",
        "",
        NULL,
        "",
        "table_scans",
        "decimal(65,0)",
        "YES",
        "",
        NULL,
        "",
        "file_ios",
        "decimal(64,0)",
        "YES",
        "",
        NULL,
        "",
        "file_io_latency",
        "varchar(11)",
        "YES",
        "",
        NULL,
        "",
        "current_connections",
        "decimal(41,0)",
        "YES",
        "",
        NULL,
        "",
        "total_connections",
        "decimal(41,0)",
        "YES",
        "",
        NULL,
        "",
        "unique_hosts",
        "bigint",
        "NO",
        "",
        "0",
        "",
        "current_memory",
        "varchar(11)",
        "YES",
        "",
        NULL,
        "",
        "total_memory_allocated",
        "varchar(11)",
        "YES",
        "",
        NULL,
        "",
    };
    static const char *const x_file_io_type_show_columns_values[] = {
        "user",        "varchar(32)",     "YES", "", NULL, "",
        "event_name",  "varchar(128)",    "NO",  "", NULL, "",
        "total",       "bigint unsigned", "NO",  "", NULL, "",
        "latency",     "bigint unsigned", "NO",  "", NULL, "",
        "max_latency", "bigint unsigned", "NO",  "", NULL, "",
    };
    static const char *const column_probe_values[] = {
        "statement_avg_latency",
        "decimal(65,4)",
        "NO",
        "0.0000",
        "max_latency",
        "decimal(42,0)",
        "YES",
        NULL,
    };
    static const char *const table_metadata_values[] = {
        "user_summary",
        "VIEW",
        NULL,
        NULL,
        "VIEW",
        "user_summary_by_file_io",
        "VIEW",
        NULL,
        NULL,
        "VIEW",
        "user_summary_by_file_io_type",
        "VIEW",
        NULL,
        NULL,
        "VIEW",
        "user_summary_by_stages",
        "VIEW",
        NULL,
        NULL,
        "VIEW",
        "user_summary_by_statement_latency",
        "VIEW",
        NULL,
        NULL,
        "VIEW",
        "user_summary_by_statement_type",
        "VIEW",
        NULL,
        NULL,
        "VIEW",
        "x$user_summary",
        "VIEW",
        NULL,
        NULL,
        "VIEW",
        "x$user_summary_by_file_io",
        "VIEW",
        NULL,
        NULL,
        "VIEW",
        "x$user_summary_by_file_io_type",
        "VIEW",
        NULL,
        NULL,
        "VIEW",
        "x$user_summary_by_stages",
        "VIEW",
        NULL,
        NULL,
        "VIEW",
        "x$user_summary_by_statement_latency",
        "VIEW",
        NULL,
        NULL,
        "VIEW",
        "x$user_summary_by_statement_type",
        "VIEW",
        NULL,
        NULL,
        "VIEW",
    };
    static const char *const views_values[] = {
        "user_summary",
        "NONE",
        "NO",
        "mysql.sys@localhost",
        "INVOKER",
        "user_summary_by_file_io",
        "NONE",
        "NO",
        "mysql.sys@localhost",
        "INVOKER",
        "user_summary_by_file_io_type",
        "NONE",
        "YES",
        "mysql.sys@localhost",
        "INVOKER",
        "user_summary_by_stages",
        "NONE",
        "YES",
        "mysql.sys@localhost",
        "INVOKER",
        "user_summary_by_statement_latency",
        "NONE",
        "NO",
        "mysql.sys@localhost",
        "INVOKER",
        "user_summary_by_statement_type",
        "NONE",
        "YES",
        "mysql.sys@localhost",
        "INVOKER",
        "x$user_summary",
        "NONE",
        "NO",
        "mysql.sys@localhost",
        "INVOKER",
        "x$user_summary_by_file_io",
        "NONE",
        "NO",
        "mysql.sys@localhost",
        "INVOKER",
        "x$user_summary_by_file_io_type",
        "NONE",
        "YES",
        "mysql.sys@localhost",
        "INVOKER",
        "x$user_summary_by_stages",
        "NONE",
        "YES",
        "mysql.sys@localhost",
        "INVOKER",
        "x$user_summary_by_statement_latency",
        "NONE",
        "NO",
        "mysql.sys@localhost",
        "INVOKER",
        "x$user_summary_by_statement_type",
        "NONE",
        "YES",
        "mysql.sys@localhost",
        "INVOKER",
    };
    static const char *const view_table_usage_values[] = {
        "user_summary",
        "performance_schema",
        "accounts",
        "user_summary",
        "sys",
        "x$memory_by_user_by_current_bytes",
        "user_summary",
        "sys",
        "x$user_summary_by_file_io",
        "user_summary",
        "sys",
        "x$user_summary_by_statement_latency",
        "user_summary_by_file_io",
        "performance_schema",
        "events_waits_summary_by_user_by_event_name",
        "user_summary_by_file_io_type",
        "performance_schema",
        "events_waits_summary_by_user_by_event_name",
        "user_summary_by_stages",
        "performance_schema",
        "events_stages_summary_by_user_by_event_name",
        "user_summary_by_statement_latency",
        "performance_schema",
        "events_statements_summary_by_user_by_event_name",
        "user_summary_by_statement_type",
        "performance_schema",
        "events_statements_summary_by_user_by_event_name",
        "x$user_summary",
        "performance_schema",
        "accounts",
        "x$user_summary",
        "sys",
        "x$memory_by_user_by_current_bytes",
        "x$user_summary",
        "sys",
        "x$user_summary_by_file_io",
        "x$user_summary",
        "sys",
        "x$user_summary_by_statement_latency",
        "x$user_summary_by_file_io",
        "performance_schema",
        "events_waits_summary_by_user_by_event_name",
        "x$user_summary_by_file_io_type",
        "performance_schema",
        "events_waits_summary_by_user_by_event_name",
        "x$user_summary_by_stages",
        "performance_schema",
        "events_stages_summary_by_user_by_event_name",
        "x$user_summary_by_statement_latency",
        "performance_schema",
        "events_statements_summary_by_user_by_event_name",
        "x$user_summary_by_statement_type",
        "performance_schema",
        "events_statements_summary_by_user_by_event_name",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open memory database");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM sys.user_summary",
            .column_names = count_column,
            .column_count = count_column_count,
            .values = count_zero,
            .row_count = 1,
            .context = "user_summary empty rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM sys.`x$user_summary_by_statement_type`",
            .column_names = count_column,
            .column_count = count_column_count,
            .values = count_zero,
            .row_count = 1,
            .context = "x user_summary_by_statement_type empty rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT()",
            .column_names = row_count_column,
            .column_count = count_column_count,
            .values = row_count_minus_one,
            .row_count = 1,
            .context = "user summary row_count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM sys.user_summary",
            .column_names = show_columns_columns,
            .column_count = show_columns_column_count,
            .values = user_summary_show_columns_values,
            .row_count = user_summary_show_columns_row_count,
            .context = "user_summary SHOW COLUMNS",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "DESCRIBE sys.`x$user_summary_by_file_io_type`",
            .column_names = show_columns_columns,
            .column_count = show_columns_column_count,
            .values = x_file_io_type_show_columns_values,
            .row_count = file_io_type_show_columns_row_count,
            .context = "x user_summary_by_file_io_type DESCRIBE",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME IN ("
                   "'user_summary', 'x$user_summary', "
                   "'user_summary_by_file_io', 'x$user_summary_by_file_io', "
                   "'user_summary_by_file_io_type', 'x$user_summary_by_file_io_type', "
                   "'user_summary_by_stages', 'x$user_summary_by_stages', "
                   "'user_summary_by_statement_latency', "
                   "'x$user_summary_by_statement_latency', "
                   "'user_summary_by_statement_type', "
                   "'x$user_summary_by_statement_type')",
            .column_names = count_column,
            .column_count = count_column_count,
            .values = count_92,
            .row_count = 1,
            .context = "user summary information_schema columns count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, COLUMN_TYPE, IS_NULLABLE, COLUMN_DEFAULT "
                   "FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = 'sys' "
                   "AND ((TABLE_NAME = 'x$user_summary' "
                   "AND COLUMN_NAME = 'statement_avg_latency') "
                   "OR (TABLE_NAME = 'x$user_summary_by_statement_latency' "
                   "AND COLUMN_NAME = 'max_latency')) "
                   "ORDER BY TABLE_NAME, ORDINAL_POSITION",
            .column_names = information_schema_column_probe_columns,
            .column_count = information_schema_column_probe_column_count,
            .values = column_probe_values,
            .row_count = 2,
            .context = "user summary raw column probe",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, TABLE_ROWS, TABLE_COMMENT "
                   "FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'sys' "
                   "AND TABLE_NAME IN ("
                   "'user_summary', 'x$user_summary', "
                   "'user_summary_by_file_io', 'x$user_summary_by_file_io', "
                   "'user_summary_by_file_io_type', 'x$user_summary_by_file_io_type', "
                   "'user_summary_by_stages', 'x$user_summary_by_stages', "
                   "'user_summary_by_statement_latency', "
                   "'x$user_summary_by_statement_latency', "
                   "'user_summary_by_statement_type', "
                   "'x$user_summary_by_statement_type') "
                   "ORDER BY TABLE_NAME",
            .column_names = metadata_columns,
            .column_count = metadata_column_count,
            .values = table_metadata_values,
            .row_count = information_schema_views_row_count,
            .context = "user summary information_schema tables",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, CHECK_OPTION, IS_UPDATABLE, DEFINER, SECURITY_TYPE "
                   "FROM INFORMATION_SCHEMA.VIEWS WHERE TABLE_SCHEMA = 'sys' "
                   "AND TABLE_NAME IN ("
                   "'user_summary', 'x$user_summary', "
                   "'user_summary_by_file_io', 'x$user_summary_by_file_io', "
                   "'user_summary_by_file_io_type', 'x$user_summary_by_file_io_type', "
                   "'user_summary_by_stages', 'x$user_summary_by_stages', "
                   "'user_summary_by_statement_latency', "
                   "'x$user_summary_by_statement_latency', "
                   "'user_summary_by_statement_type', "
                   "'x$user_summary_by_statement_type') "
                   "ORDER BY TABLE_NAME",
            .column_names = information_schema_views_columns,
            .column_count = information_schema_views_column_count,
            .values = views_values,
            .row_count = information_schema_views_row_count,
            .context = "user summary information_schema views",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT VIEW_NAME, TABLE_SCHEMA, TABLE_NAME "
                   "FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE WHERE VIEW_SCHEMA = 'sys' "
                   "AND VIEW_NAME IN ("
                   "'user_summary', 'x$user_summary', "
                   "'user_summary_by_file_io', 'x$user_summary_by_file_io', "
                   "'user_summary_by_file_io_type', 'x$user_summary_by_file_io_type', "
                   "'user_summary_by_stages', 'x$user_summary_by_stages', "
                   "'user_summary_by_statement_latency', "
                   "'x$user_summary_by_statement_latency', "
                   "'user_summary_by_statement_type', "
                   "'x$user_summary_by_statement_type') "
                   "ORDER BY VIEW_NAME, TABLE_SCHEMA, TABLE_NAME",
            .column_names = information_schema_view_table_usage_columns,
            .column_count = information_schema_view_table_usage_column_count,
            .values = view_table_usage_values,
            .row_count = information_schema_view_table_usage_row_count,
            .context = "user summary view table usage",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE "
                   "WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME IN ("
                   "'user_summary', 'x$user_summary', "
                   "'user_summary_by_file_io', 'x$user_summary_by_file_io', "
                   "'user_summary_by_file_io_type', 'x$user_summary_by_file_io_type', "
                   "'user_summary_by_stages', 'x$user_summary_by_stages', "
                   "'user_summary_by_statement_latency', "
                   "'x$user_summary_by_statement_latency', "
                   "'user_summary_by_statement_type', "
                   "'x$user_summary_by_statement_type')",
            .column_names = count_column,
            .column_count = count_column_count,
            .values = count_zero,
            .row_count = 1,
            .context = "user summary empty routine usage",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME IN ("
                   "'user_summary', 'x$user_summary', "
                   "'user_summary_by_file_io', 'x$user_summary_by_file_io', "
                   "'user_summary_by_file_io_type', 'x$user_summary_by_file_io_type', "
                   "'user_summary_by_stages', 'x$user_summary_by_stages', "
                   "'user_summary_by_statement_latency', "
                   "'x$user_summary_by_statement_latency', "
                   "'user_summary_by_statement_type', "
                   "'x$user_summary_by_statement_type')",
            .column_names = count_column,
            .column_count = count_column_count,
            .values = count_zero,
            .row_count = 1,
            .context = "user summary empty statistics",
        }
    );
    failures += expect_query_contains(
        database,
        (struct expected_query_contains){
            .sql = "SHOW CREATE VIEW sys.user_summary",
            .column_names = show_create_view_columns,
            .column_count = show_create_view_column_count,
            .row_count = 1,
            .row_index = 0,
            .column_index = 1,
            .needle = "left join `sys`.`x$user_summary_by_statement_latency` `stmt`",
            .context = "user_summary SHOW CREATE",
        }
    );
    failures += expect_query_contains(
        database,
        (struct expected_query_contains){
            .sql = "SHOW CREATE VIEW sys.`x$user_summary_by_file_io_type`",
            .column_names = show_create_view_columns,
            .column_count = show_create_view_column_count,
            .row_count = 1,
            .row_index = 0,
            .column_index = 1,
            .needle = "`SUM_TIMER_WAIT` AS `latency`",
            .context = "x user_summary_by_file_io_type SHOW CREATE",
        }
    );
    failures += expect_statement_ok(database, "USE sys");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM user_summary_by_stages",
            .column_names = count_column,
            .column_count = count_column_count,
            .values = count_zero,
            .row_count = 1,
            .context = "selected schema user_summary_by_stages count",
        }
    );
    failures += expect_query_contains(
        database,
        (struct expected_query_contains){
            .sql = "SHOW CREATE TABLE user_summary",
            .column_names = show_create_view_columns,
            .column_count = show_create_view_column_count,
            .row_count = 1,
            .row_index = 0,
            .column_index = 1,
            .needle = "VIEW `user_summary`",
            .context = "selected schema user_summary SHOW CREATE",
        }
    );

    mylite_close(database);
    return failures;
}

static int expect_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = mylite_test_expect_int(rc, MYLITE_OK, sql);

    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: %s\n", sql, mylite_errmsg(database));
    }
    mylite_result_free(result);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);
    int failures = mylite_test_expect_int(rc, MYLITE_OK, expected.context);

    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: %s\n", expected.context, mylite_errmsg(database));
        mylite_result_free(result);
        return failures + 1;
    }

    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        expected.column_count,
        expected.context
    );
    for (size_t column_index = 0; column_index < expected.column_count; ++column_index) {
        failures += mylite_test_expect_text_or_null(
            mylite_result_column_name(result, column_index),
            expected.column_names[column_index],
            expected.context
        );
    }

    failures += mylite_test_expect_size(
        mylite_result_row_count(result),
        expected.row_count,
        expected.context
    );
    for (size_t row_index = 0; row_index < expected.row_count; ++row_index) {
        for (size_t column_index = 0; column_index < expected.column_count; ++column_index) {
            const size_t value_index = (row_index * expected.column_count) + column_index;
            failures += mylite_test_expect_text_or_null(
                mylite_result_value_text(result, row_index, column_index),
                expected.values[value_index],
                expected.context
            );
        }
    }

    mylite_result_free(result);
    return failures;
}

static int expect_query_contains(mylite_db *database, struct expected_query_contains expected) {
    mylite_result *result = NULL;
    const char *value = NULL;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);
    int failures = mylite_test_expect_int(rc, MYLITE_OK, expected.context);

    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: %s\n", expected.context, mylite_errmsg(database));
        mylite_result_free(result);
        return failures + 1;
    }
    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        expected.column_count,
        expected.context
    );
    for (size_t column_index = 0; column_index < expected.column_count; ++column_index) {
        failures += mylite_test_expect_text_or_null(
            mylite_result_column_name(result, column_index),
            expected.column_names[column_index],
            expected.context
        );
    }
    failures += mylite_test_expect_size(
        mylite_result_row_count(result),
        expected.row_count,
        expected.context
    );

    value = mylite_result_value_text(result, expected.row_index, expected.column_index);
    if (value == NULL || strstr(value, expected.needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected [%s] to contain [%s]\n",
            expected.context,
            value == NULL ? "<NULL>" : value,
            expected.needle
        );
        failures += 1;
    }

    mylite_result_free(result);
    return failures;
}
