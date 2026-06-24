#include <mylite/mylite.h>

#include <stdio.h>
#include <string.h>

enum {
    count_column_count = 1,
    table_metadata_column_count = 5,
    show_columns_column_count = 6,
    information_schema_views_column_count = 5,
    information_schema_views_row_count = 4,
    information_schema_view_table_usage_column_count = 3,
    information_schema_view_table_usage_row_count = 6,
    information_schema_view_routine_usage_column_count = 3,
    information_schema_view_routine_usage_row_count = 2,
    sorting_show_columns_row_count = 13,
    temp_show_columns_row_count = 11,
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

static int test_sys_statement_sort_temp_views(void);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_query_contains(mylite_db *database, struct expected_query_contains expected);
static int expect_int(int actual, int expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text_or_null(const char *actual, const char *expected, const char *context);

static const char *const count_column[count_column_count] = {
    "COUNT(*)",
};

static const char *const count_zero[] = {
    "0",
};

static const char *const row_count_column[] = {
    "ROW_COUNT()",
};

static const char *const row_count_minus_one[] = {
    "-1",
};

static const char *const count_48[] = {
    "48",
};

static const char *const table_metadata_columns[table_metadata_column_count] = {
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
    return test_sys_statement_sort_temp_views() == 0 ? 0 : 1;
}

static int test_sys_statement_sort_temp_views(void) {
    static const char *const sorting_show_columns_values[] = {
        "query",
        "longtext",
        "YES",
        "",
        NULL,
        "",
        "db",
        "varchar(64)",
        "YES",
        "",
        NULL,
        "",
        "exec_count",
        "bigint unsigned",
        "NO",
        "",
        NULL,
        "",
        "total_latency",
        "varchar(11)",
        "YES",
        "",
        NULL,
        "",
        "sort_merge_passes",
        "bigint unsigned",
        "NO",
        "",
        NULL,
        "",
        "avg_sort_merges",
        "decimal(21,0)",
        "NO",
        "",
        "0",
        "",
        "sorts_using_scans",
        "bigint unsigned",
        "NO",
        "",
        NULL,
        "",
        "sort_using_range",
        "bigint unsigned",
        "NO",
        "",
        NULL,
        "",
        "rows_sorted",
        "bigint unsigned",
        "NO",
        "",
        NULL,
        "",
        "avg_rows_sorted",
        "decimal(21,0)",
        "NO",
        "",
        "0",
        "",
        "first_seen",
        "timestamp(6)",
        "NO",
        "",
        NULL,
        "",
        "last_seen",
        "timestamp(6)",
        "NO",
        "",
        NULL,
        "",
        "digest",
        "varchar(64)",
        "YES",
        "",
        NULL,
        "",
    };
    static const char *const x_temp_show_columns_values[] = {
        "query",
        "longtext",
        "YES",
        "",
        NULL,
        "",
        "db",
        "varchar(64)",
        "YES",
        "",
        NULL,
        "",
        "exec_count",
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
        "memory_tmp_tables",
        "bigint unsigned",
        "NO",
        "",
        NULL,
        "",
        "disk_tmp_tables",
        "bigint unsigned",
        "NO",
        "",
        NULL,
        "",
        "avg_tmp_tables_per_query",
        "decimal(21,0)",
        "NO",
        "",
        "0",
        "",
        "tmp_tables_to_disk_pct",
        "decimal(24,0)",
        "NO",
        "",
        "0",
        "",
        "first_seen",
        "timestamp(6)",
        "NO",
        "",
        NULL,
        "",
        "last_seen",
        "timestamp(6)",
        "NO",
        "",
        NULL,
        "",
        "digest",
        "varchar(64)",
        "YES",
        "",
        NULL,
        "",
    };
    static const char *const table_metadata_values[] = {
        "statements_with_sorting",       "VIEW", NULL, NULL, "VIEW",
        "statements_with_temp_tables",   "VIEW", NULL, NULL, "VIEW",
        "x$statements_with_sorting",     "VIEW", NULL, NULL, "VIEW",
        "x$statements_with_temp_tables", "VIEW", NULL, NULL, "VIEW",
    };
    static const char *const views_values[] = {
        "statements_with_sorting",       "NONE", "YES", "mysql.sys@localhost", "INVOKER",
        "statements_with_temp_tables",   "NONE", "YES", "mysql.sys@localhost", "INVOKER",
        "x$statements_with_sorting",     "NONE", "YES", "mysql.sys@localhost", "INVOKER",
        "x$statements_with_temp_tables", "NONE", "YES", "mysql.sys@localhost", "INVOKER",
    };
    static const char *const view_table_usage_values[] = {
        "statements_with_sorting",
        "performance_schema",
        "events_statements_summary_by_digest",
        "statements_with_sorting",
        "sys",
        "sys_config",
        "statements_with_temp_tables",
        "performance_schema",
        "events_statements_summary_by_digest",
        "statements_with_temp_tables",
        "sys",
        "sys_config",
        "x$statements_with_sorting",
        "performance_schema",
        "events_statements_summary_by_digest",
        "x$statements_with_temp_tables",
        "performance_schema",
        "events_statements_summary_by_digest",
    };
    static const char *const view_routine_usage_values[] = {
        "statements_with_sorting",
        "sys",
        "format_statement",
        "statements_with_temp_tables",
        "sys",
        "format_statement",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open memory database");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM sys.statements_with_sorting",
            .column_names = count_column,
            .column_count = count_column_count,
            .values = count_zero,
            .row_count = 1,
            .context = "statements_with_sorting empty rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM sys.`x$statements_with_temp_tables`",
            .column_names = count_column,
            .column_count = count_column_count,
            .values = count_zero,
            .row_count = 1,
            .context = "x statements_with_temp_tables empty rows",
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
            .context = "statement sort/temp row_count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM sys.statements_with_sorting",
            .column_names = show_columns_columns,
            .column_count = show_columns_column_count,
            .values = sorting_show_columns_values,
            .row_count = sorting_show_columns_row_count,
            .context = "statements_with_sorting SHOW COLUMNS",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "DESCRIBE sys.`x$statements_with_temp_tables`",
            .column_names = show_columns_columns,
            .column_count = show_columns_column_count,
            .values = x_temp_show_columns_values,
            .row_count = temp_show_columns_row_count,
            .context = "x statements_with_temp_tables DESCRIBE",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME IN "
                   "('statements_with_sorting', 'x$statements_with_sorting', "
                   "'statements_with_temp_tables', 'x$statements_with_temp_tables')",
            .column_names = count_column,
            .column_count = count_column_count,
            .values = count_48,
            .row_count = 1,
            .context = "statement sort/temp information_schema columns count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, TABLE_ROWS, TABLE_COMMENT "
                   "FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'sys' "
                   "AND TABLE_NAME IN ('statements_with_sorting', "
                   "'x$statements_with_sorting', 'statements_with_temp_tables', "
                   "'x$statements_with_temp_tables') ORDER BY TABLE_NAME",
            .column_names = table_metadata_columns,
            .column_count = table_metadata_column_count,
            .values = table_metadata_values,
            .row_count = information_schema_views_row_count,
            .context = "statement sort/temp information_schema tables",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, CHECK_OPTION, IS_UPDATABLE, DEFINER, SECURITY_TYPE "
                   "FROM INFORMATION_SCHEMA.VIEWS WHERE TABLE_SCHEMA = 'sys' "
                   "AND TABLE_NAME IN ('statements_with_sorting', "
                   "'x$statements_with_sorting', 'statements_with_temp_tables', "
                   "'x$statements_with_temp_tables') ORDER BY TABLE_NAME",
            .column_names = information_schema_views_columns,
            .column_count = information_schema_views_column_count,
            .values = views_values,
            .row_count = information_schema_views_row_count,
            .context = "statement sort/temp information_schema views",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT VIEW_NAME, TABLE_SCHEMA, TABLE_NAME "
                   "FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE WHERE VIEW_SCHEMA = 'sys' "
                   "AND VIEW_NAME IN ('statements_with_sorting', "
                   "'x$statements_with_sorting', 'statements_with_temp_tables', "
                   "'x$statements_with_temp_tables') "
                   "ORDER BY VIEW_NAME, TABLE_SCHEMA, TABLE_NAME",
            .column_names = information_schema_view_table_usage_columns,
            .column_count = information_schema_view_table_usage_column_count,
            .values = view_table_usage_values,
            .row_count = information_schema_view_table_usage_row_count,
            .context = "statement sort/temp view table usage",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, SPECIFIC_SCHEMA, SPECIFIC_NAME "
                   "FROM INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE WHERE TABLE_SCHEMA = 'sys' "
                   "AND TABLE_NAME IN ('statements_with_sorting', "
                   "'x$statements_with_sorting', 'statements_with_temp_tables', "
                   "'x$statements_with_temp_tables') "
                   "ORDER BY TABLE_NAME, SPECIFIC_SCHEMA, SPECIFIC_NAME",
            .column_names = information_schema_view_routine_usage_columns,
            .column_count = information_schema_view_routine_usage_column_count,
            .values = view_routine_usage_values,
            .row_count = information_schema_view_routine_usage_row_count,
            .context = "statement sort/temp view routine usage",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME IN "
                   "('statements_with_sorting', 'x$statements_with_sorting', "
                   "'statements_with_temp_tables', 'x$statements_with_temp_tables')",
            .column_names = count_column,
            .column_count = count_column_count,
            .values = count_zero,
            .row_count = 1,
            .context = "statement sort/temp empty statistics",
        }
    );
    failures += expect_query_contains(
        database,
        (struct expected_query_contains){
            .sql = "SHOW CREATE VIEW sys.statements_with_sorting",
            .column_names = show_create_view_columns,
            .column_count = show_create_view_column_count,
            .row_count = 1,
            .row_index = 0,
            .column_index = 1,
            .needle = "(`stmts`.`SUM_SORT_ROWS` > 0)",
            .context = "statements_with_sorting SHOW CREATE",
        }
    );
    failures += expect_query_contains(
        database,
        (struct expected_query_contains){
            .sql = "SHOW CREATE VIEW sys.`x$statements_with_temp_tables`",
            .column_names = show_create_view_columns,
            .column_count = show_create_view_column_count,
            .row_count = 1,
            .row_index = 0,
            .column_index = 1,
            .needle = "`stmts`.`SUM_TIMER_WAIT` AS `total_latency`",
            .context = "x statements_with_temp_tables SHOW CREATE",
        }
    );
    failures += expect_statement_ok(database, "USE sys");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM statements_with_temp_tables",
            .column_names = count_column,
            .column_count = count_column_count,
            .values = count_zero,
            .row_count = 1,
            .context = "selected schema statements_with_temp_tables count",
        }
    );

    mylite_close(database);
    return failures;
}

static int expect_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = expect_int(rc, MYLITE_OK, sql);

    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: %s\n", sql, mylite_errmsg(database));
    }
    mylite_result_free(result);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);
    int failures = expect_int(rc, MYLITE_OK, expected.context);

    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: %s\n", expected.context, mylite_errmsg(database));
        mylite_result_free(result);
        return failures + 1;
    }

    failures +=
        expect_size(mylite_result_column_count(result), expected.column_count, expected.context);
    for (size_t column_index = 0; column_index < expected.column_count; ++column_index) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, column_index),
            expected.column_names[column_index],
            expected.context
        );
    }

    failures += expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
    for (size_t row_index = 0; row_index < expected.row_count; ++row_index) {
        for (size_t column_index = 0; column_index < expected.column_count; ++column_index) {
            const size_t value_index = (row_index * expected.column_count) + column_index;
            failures += expect_text_or_null(
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
    int failures = expect_int(rc, MYLITE_OK, expected.context);

    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: %s\n", expected.context, mylite_errmsg(database));
        mylite_result_free(result);
        return failures + 1;
    }
    failures +=
        expect_size(mylite_result_column_count(result), expected.column_count, expected.context);
    for (size_t column_index = 0; column_index < expected.column_count; ++column_index) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, column_index),
            expected.column_names[column_index],
            expected.context
        );
    }
    failures += expect_size(mylite_result_row_count(result), expected.row_count, expected.context);

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

static int expect_int(int actual, int expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_text_or_null(const char *actual, const char *expected, const char *context) {
    if (actual == NULL && expected == NULL) {
        return 0;
    }
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected [%s], got [%s]\n",
            context,
            expected == NULL ? "<NULL>" : expected,
            actual == NULL ? "<NULL>" : actual
        );
        return 1;
    }
    return 0;
}
