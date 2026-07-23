#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include <stdio.h>
#include <string.h>

enum {
    count_column_count = 1,
    show_columns_column_count = 6,
    information_schema_views_column_count = 5,
    information_schema_views_row_count = 8,
    information_schema_view_table_usage_column_count = 3,
    information_schema_view_table_usage_row_count = 14,
    information_schema_view_routine_usage_column_count = 3,
    information_schema_view_routine_usage_row_count = 4,
    statement_analysis_show_columns_row_count = 5,
    statements_errors_show_columns_row_count = 10,
    show_create_view_column_count = 4,
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

static int test_sys_statement_digest_views(void);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_query_contains(mylite_db *database, struct expected_query_contains expected);
static const char *const count_column[count_column_count] = {
    "COUNT(*)",
};

static const char *const count_zero[] = {
    "0",
};

static const char *const count_one[] = {
    "1",
};

static const char *const row_count_column[] = {
    "ROW_COUNT()",
};

static const char *const row_count_minus_one[] = {
    "-1",
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

static const char *const information_schema_view_routine_usage_columns
    [information_schema_view_routine_usage_column_count] = {
        "TABLE_NAME",
        "SPECIFIC_SCHEMA",
        "SPECIFIC_NAME",
};

static const char *const show_create_view_columns[show_create_view_column_count] = {
    "View",
    "Create View",
    "character_set_client",
    "collation_connection",
};

int main(void) {
    return test_sys_statement_digest_views() == 0 ? 0 : 1;
}

static int test_sys_statement_digest_views(void) {
    static const char *const statement_analysis_show_columns_values[] = {
        "query",
        "longtext",
        "YES",
        "",
        NULL,
        "",
        "full_scan",
        "varchar(1)",
        "NO",
        "",
        "",
        "",
        "total_latency",
        "varchar(11)",
        "YES",
        "",
        NULL,
        "",
        "max_controlled_memory",
        "varchar(11)",
        "YES",
        "",
        NULL,
        "",
        "first_seen",
        "timestamp(6)",
        "NO",
        "",
        NULL,
        "",
    };
    static const char *const x_statement_analysis_show_columns_values[] = {
        "exec_secondary_count",
        "bigint unsigned",
        "NO",
        "",
        NULL,
        "",
        "total_latency",
        "bigint unsigned",
        "NO",
        "",
        NULL,
        "",
        "max_controlled_memory",
        "bigint unsigned",
        "NO",
        "",
        NULL,
        "",
        "first_seen",
        "timestamp(6)",
        "NO",
        "",
        NULL,
        "",
    };
    static const char *const statements_errors_show_columns_values[] = {
        "query",       "longtext",        "YES", "", NULL,     "",
        "db",          "varchar(64)",     "YES", "", NULL,     "",
        "exec_count",  "bigint unsigned", "NO",  "", NULL,     "",
        "errors",      "bigint unsigned", "NO",  "", NULL,     "",
        "error_pct",   "decimal(27,4)",   "NO",  "", "0.0000", "",
        "warnings",    "bigint unsigned", "NO",  "", NULL,     "",
        "warning_pct", "decimal(27,4)",   "NO",  "", "0.0000", "",
        "first_seen",  "timestamp(6)",    "NO",  "", NULL,     "",
        "last_seen",   "timestamp(6)",    "NO",  "", NULL,     "",
        "digest",      "varchar(64)",     "YES", "", NULL,     "",
    };
    static const char *const statement_analysis_column_count_values[] = {
        "26",
    };
    static const char *const x_statement_analysis_column_count_values[] = {
        "27",
    };
    static const char *const views_values[] = {
        "statement_analysis",
        "NONE",
        "YES",
        "mysql.sys@localhost",
        "INVOKER",
        "statements_with_errors_or_warnings",
        "NONE",
        "YES",
        "mysql.sys@localhost",
        "INVOKER",
        "statements_with_full_table_scans",
        "NONE",
        "YES",
        "mysql.sys@localhost",
        "INVOKER",
        "statements_with_runtimes_in_95th_percentile",
        "NONE",
        "YES",
        "mysql.sys@localhost",
        "INVOKER",
        "x$statement_analysis",
        "NONE",
        "YES",
        "mysql.sys@localhost",
        "INVOKER",
        "x$statements_with_errors_or_warnings",
        "NONE",
        "YES",
        "mysql.sys@localhost",
        "INVOKER",
        "x$statements_with_full_table_scans",
        "NONE",
        "YES",
        "mysql.sys@localhost",
        "INVOKER",
        "x$statements_with_runtimes_in_95th_percentile",
        "NONE",
        "YES",
        "mysql.sys@localhost",
        "INVOKER",
    };
    static const char *const view_table_usage_values[] = {
        "statement_analysis",
        "performance_schema",
        "events_statements_summary_by_digest",
        "statement_analysis",
        "sys",
        "sys_config",
        "statements_with_errors_or_warnings",
        "performance_schema",
        "events_statements_summary_by_digest",
        "statements_with_errors_or_warnings",
        "sys",
        "sys_config",
        "statements_with_full_table_scans",
        "performance_schema",
        "events_statements_summary_by_digest",
        "statements_with_full_table_scans",
        "sys",
        "sys_config",
        "statements_with_runtimes_in_95th_percentile",
        "performance_schema",
        "events_statements_summary_by_digest",
        "statements_with_runtimes_in_95th_percentile",
        "sys",
        "sys_config",
        "statements_with_runtimes_in_95th_percentile",
        "sys",
        "x$ps_digest_95th_percentile_by_avg_us",
        "x$statement_analysis",
        "performance_schema",
        "events_statements_summary_by_digest",
        "x$statements_with_errors_or_warnings",
        "performance_schema",
        "events_statements_summary_by_digest",
        "x$statements_with_full_table_scans",
        "performance_schema",
        "events_statements_summary_by_digest",
        "x$statements_with_runtimes_in_95th_percentile",
        "performance_schema",
        "events_statements_summary_by_digest",
        "x$statements_with_runtimes_in_95th_percentile",
        "sys",
        "x$ps_digest_95th_percentile_by_avg_us",
    };
    static const char *const view_routine_usage_values[] = {
        "statement_analysis",
        "sys",
        "format_statement",
        "statements_with_errors_or_warnings",
        "sys",
        "format_statement",
        "statements_with_full_table_scans",
        "sys",
        "format_statement",
        "statements_with_runtimes_in_95th_percentile",
        "sys",
        "format_statement",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open memory database");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM sys.statement_analysis",
            .column_names = count_column,
            .column_count = count_column_count,
            .values = count_zero,
            .row_count = 1,
            .context = "statement_analysis empty rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM sys.`x$statement_analysis`",
            .column_names = count_column,
            .column_count = count_column_count,
            .values = count_zero,
            .row_count = 1,
            .context = "x statement_analysis empty rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM sys.statements_with_errors_or_warnings",
            .column_names = count_column,
            .column_count = count_column_count,
            .values = count_zero,
            .row_count = 1,
            .context = "statements_with_errors_or_warnings empty rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM sys.statements_with_full_table_scans",
            .column_names = count_column,
            .column_count = count_column_count,
            .values = count_zero,
            .row_count = 1,
            .context = "statements_with_full_table_scans empty rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM sys.statements_with_runtimes_in_95th_percentile",
            .column_names = count_column,
            .column_count = count_column_count,
            .values = count_zero,
            .row_count = 1,
            .context = "statements_with_runtimes_in_95th_percentile empty rows",
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
            .context = "statement digest row_count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM sys.statement_analysis "
                   "WHERE Field IN ('query', 'full_scan', 'total_latency', "
                   "'max_controlled_memory', 'first_seen')",
            .column_names = show_columns_columns,
            .column_count = show_columns_column_count,
            .values = statement_analysis_show_columns_values,
            .row_count = statement_analysis_show_columns_row_count,
            .context = "statement_analysis SHOW COLUMNS",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM sys.`x$statement_analysis` "
                   "WHERE Field IN ('exec_secondary_count', 'total_latency', "
                   "'max_controlled_memory', 'first_seen')",
            .column_names = show_columns_columns,
            .column_count = show_columns_column_count,
            .values = x_statement_analysis_show_columns_values,
            .row_count = 4,
            .context = "x statement_analysis SHOW COLUMNS",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "DESCRIBE sys.statements_with_errors_or_warnings",
            .column_names = show_columns_columns,
            .column_count = show_columns_column_count,
            .values = statements_errors_show_columns_values,
            .row_count = statements_errors_show_columns_row_count,
            .context = "statements_with_errors_or_warnings DESCRIBE",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'statement_analysis'",
            .column_names = count_column,
            .column_count = count_column_count,
            .values = statement_analysis_column_count_values,
            .row_count = 1,
            .context = "statement_analysis information_schema columns count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'x$statement_analysis'",
            .column_names = count_column,
            .column_count = count_column_count,
            .values = x_statement_analysis_column_count_values,
            .row_count = 1,
            .context = "x statement_analysis information_schema columns count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, CHECK_OPTION, IS_UPDATABLE, DEFINER, SECURITY_TYPE "
                   "FROM INFORMATION_SCHEMA.VIEWS WHERE TABLE_SCHEMA = 'sys' "
                   "AND TABLE_NAME IN ('statement_analysis', 'x$statement_analysis', "
                   "'statements_with_errors_or_warnings', "
                   "'x$statements_with_errors_or_warnings', "
                   "'statements_with_full_table_scans', "
                   "'x$statements_with_full_table_scans', "
                   "'statements_with_runtimes_in_95th_percentile', "
                   "'x$statements_with_runtimes_in_95th_percentile') ORDER BY TABLE_NAME",
            .column_names = information_schema_views_columns,
            .column_count = information_schema_views_column_count,
            .values = views_values,
            .row_count = information_schema_views_row_count,
            .context = "statement digest information_schema views",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT VIEW_NAME, TABLE_SCHEMA, TABLE_NAME "
                   "FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE WHERE VIEW_SCHEMA = 'sys' "
                   "AND VIEW_NAME IN ('statement_analysis', 'x$statement_analysis', "
                   "'statements_with_errors_or_warnings', "
                   "'x$statements_with_errors_or_warnings', "
                   "'statements_with_full_table_scans', "
                   "'x$statements_with_full_table_scans', "
                   "'statements_with_runtimes_in_95th_percentile', "
                   "'x$statements_with_runtimes_in_95th_percentile') "
                   "ORDER BY VIEW_NAME, TABLE_SCHEMA, TABLE_NAME",
            .column_names = information_schema_view_table_usage_columns,
            .column_count = information_schema_view_table_usage_column_count,
            .values = view_table_usage_values,
            .row_count = information_schema_view_table_usage_row_count,
            .context = "statement digest view table usage",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, SPECIFIC_SCHEMA, SPECIFIC_NAME "
                   "FROM INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE WHERE TABLE_SCHEMA = 'sys' "
                   "AND TABLE_NAME IN ('statement_analysis', 'x$statement_analysis', "
                   "'statements_with_errors_or_warnings', "
                   "'x$statements_with_errors_or_warnings', "
                   "'statements_with_full_table_scans', "
                   "'x$statements_with_full_table_scans', "
                   "'statements_with_runtimes_in_95th_percentile', "
                   "'x$statements_with_runtimes_in_95th_percentile') "
                   "ORDER BY TABLE_NAME, SPECIFIC_SCHEMA, SPECIFIC_NAME",
            .column_names = information_schema_view_routine_usage_columns,
            .column_count = information_schema_view_routine_usage_column_count,
            .values = view_routine_usage_values,
            .row_count = information_schema_view_routine_usage_row_count,
            .context = "statement digest view routine usage",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME IN "
                   "('statement_analysis', 'x$statement_analysis', "
                   "'statements_with_errors_or_warnings', "
                   "'x$statements_with_errors_or_warnings', "
                   "'statements_with_full_table_scans', "
                   "'x$statements_with_full_table_scans', "
                   "'statements_with_runtimes_in_95th_percentile', "
                   "'x$statements_with_runtimes_in_95th_percentile')",
            .column_names = count_column,
            .column_count = count_column_count,
            .values = count_zero,
            .row_count = 1,
            .context = "statement digest empty statistics",
        }
    );
    failures += expect_query_contains(
        database,
        (struct expected_query_contains){
            .sql = "SHOW CREATE VIEW sys.statement_analysis",
            .column_names = show_create_view_columns,
            .column_count = show_create_view_column_count,
            .row_count = 1,
            .row_index = 0,
            .column_index = 1,
            .needle = "`sys`.`format_statement`(`stmts`.`DIGEST_TEXT`) AS `query`",
            .context = "statement_analysis SHOW CREATE",
        }
    );
    failures += expect_query_contains(
        database,
        (struct expected_query_contains){
            .sql = "SHOW CREATE VIEW sys.`x$statement_analysis`",
            .column_names = show_create_view_columns,
            .column_count = show_create_view_column_count,
            .row_count = 1,
            .row_index = 0,
            .column_index = 1,
            .needle = "`stmts`.`COUNT_SECONDARY` AS `exec_secondary_count`",
            .context = "x statement_analysis SHOW CREATE",
        }
    );
    failures += expect_query_contains(
        database,
        (struct expected_query_contains){
            .sql = "SHOW CREATE VIEW sys.statements_with_runtimes_in_95th_percentile",
            .column_names = show_create_view_columns,
            .column_count = show_create_view_column_count,
            .row_count = 1,
            .row_index = 0,
            .column_index = 1,
            .needle = "`sys`.`x$ps_digest_95th_percentile_by_avg_us` `top_percentile`",
            .context = "runtime percentile SHOW CREATE",
        }
    );
    failures += expect_statement_ok(database, "USE sys");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM statements_with_full_table_scans",
            .column_names = count_column,
            .column_count = count_column_count,
            .values = count_zero,
            .row_count = 1,
            .context = "selected schema statements_with_full_table_scans count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.VIEWS "
                   "WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = "
                   "'statements_with_full_table_scans'",
            .column_names = count_column,
            .column_count = count_column_count,
            .values = count_one,
            .row_count = 1,
            .context = "statement digest views count",
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
