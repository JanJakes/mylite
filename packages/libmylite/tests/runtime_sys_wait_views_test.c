#include <mylite/mylite.h>

#include <stdio.h>
#include <string.h>

enum {
    count_column_count = 1,
    metadata_column_count = 5,
    show_columns_column_count = 6,
    information_schema_views_column_count = 5,
    information_schema_views_row_count = 10,
    information_schema_view_table_usage_column_count = 3,
    information_schema_special_column_count = 7,
    show_create_view_column_count = 4,
    wait_view_column_total = 58,
    wait_class_show_columns_row_count = 6,
    waits_global_show_columns_row_count = 5,
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

static int test_sys_wait_views(void);
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

static const char *const count_58[] = {
    "58",
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
    *const information_schema_special_columns[information_schema_special_column_count] = {
        "TABLE_NAME",
        "COLUMN_NAME",
        "COLUMN_TYPE",
        "IS_NULLABLE",
        "COLUMN_DEFAULT",
        "CHARACTER_SET_NAME",
        "COLLATION_NAME",
};

static const char *const show_create_view_columns[show_create_view_column_count] = {
    "View",
    "Create View",
    "character_set_client",
    "collation_connection",
};

int main(void) {
    return test_sys_wait_views() == 0 ? 0 : 1;
}

static int test_sys_wait_views(void) {
    static const char *const wait_class_show_columns_values[] = {
        "event_class",   "varchar(128)",  "YES", "", NULL, "",
        "total",         "decimal(42,0)", "YES", "", NULL, "",
        "total_latency", "varchar(11)",   "YES", "", NULL, "",
        "min_latency",   "varchar(11)",   "YES", "", NULL, "",
        "avg_latency",   "varchar(11)",   "YES", "", NULL, "",
        "max_latency",   "varchar(11)",   "YES", "", NULL, "",
    };
    static const char *const x_wait_class_show_columns_values[] = {
        "event_class",   "varchar(128)",    "YES", "", NULL,     "",
        "total",         "decimal(42,0)",   "YES", "", NULL,     "",
        "total_latency", "decimal(42,0)",   "YES", "", NULL,     "",
        "min_latency",   "bigint unsigned", "YES", "", NULL,     "",
        "avg_latency",   "decimal(46,4)",   "NO",  "", "0.0000", "",
        "max_latency",   "bigint unsigned", "YES", "", NULL,     "",
    };
    static const char *const waits_user_show_columns_values[] = {
        "user",          "varchar(32)",     "YES", "", NULL, "",
        "event",         "varchar(128)",    "NO",  "", NULL, "",
        "total",         "bigint unsigned", "NO",  "", NULL, "",
        "total_latency", "varchar(11)",     "YES", "", NULL, "",
        "avg_latency",   "varchar(11)",     "YES", "", NULL, "",
        "max_latency",   "varchar(11)",     "YES", "", NULL, "",
    };
    static const char *const x_waits_global_show_columns_values[] = {
        "events",        "varchar(128)",    "NO", "", NULL, "",
        "total",         "bigint unsigned", "NO", "", NULL, "",
        "total_latency", "bigint unsigned", "NO", "", NULL, "",
        "avg_latency",   "bigint unsigned", "NO", "", NULL, "",
        "max_latency",   "bigint unsigned", "NO", "", NULL, "",
    };
    static const char *const special_column_values[] = {
        "waits_by_host_by_latency",
        "host",
        "varchar(255)",
        "YES",
        NULL,
        "ascii",
        "ascii_general_ci",
        "waits_by_user_by_latency",
        "user",
        "varchar(32)",
        "YES",
        NULL,
        "utf8mb4",
        "utf8mb4_bin",
        "waits_global_by_latency",
        "events",
        "varchar(128)",
        "NO",
        NULL,
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "x$wait_classes_global_by_latency",
        "avg_latency",
        "decimal(46,4)",
        "NO",
        "0.0000",
        NULL,
        NULL,
    };
    static const char *const table_metadata_values[] = {
        "wait_classes_global_by_avg_latency",
        "VIEW",
        NULL,
        NULL,
        "VIEW",
        "wait_classes_global_by_latency",
        "VIEW",
        NULL,
        NULL,
        "VIEW",
        "waits_by_host_by_latency",
        "VIEW",
        NULL,
        NULL,
        "VIEW",
        "waits_by_user_by_latency",
        "VIEW",
        NULL,
        NULL,
        "VIEW",
        "waits_global_by_latency",
        "VIEW",
        NULL,
        NULL,
        "VIEW",
        "x$wait_classes_global_by_avg_latency",
        "VIEW",
        NULL,
        NULL,
        "VIEW",
        "x$wait_classes_global_by_latency",
        "VIEW",
        NULL,
        NULL,
        "VIEW",
        "x$waits_by_host_by_latency",
        "VIEW",
        NULL,
        NULL,
        "VIEW",
        "x$waits_by_user_by_latency",
        "VIEW",
        NULL,
        NULL,
        "VIEW",
        "x$waits_global_by_latency",
        "VIEW",
        NULL,
        NULL,
        "VIEW",
    };
    static const char *const views_values[] = {
        "wait_classes_global_by_avg_latency",
        "NONE",
        "NO",
        "mysql.sys@localhost",
        "INVOKER",
        "wait_classes_global_by_latency",
        "NONE",
        "NO",
        "mysql.sys@localhost",
        "INVOKER",
        "waits_by_host_by_latency",
        "NONE",
        "YES",
        "mysql.sys@localhost",
        "INVOKER",
        "waits_by_user_by_latency",
        "NONE",
        "YES",
        "mysql.sys@localhost",
        "INVOKER",
        "waits_global_by_latency",
        "NONE",
        "YES",
        "mysql.sys@localhost",
        "INVOKER",
        "x$wait_classes_global_by_avg_latency",
        "NONE",
        "NO",
        "mysql.sys@localhost",
        "INVOKER",
        "x$wait_classes_global_by_latency",
        "NONE",
        "NO",
        "mysql.sys@localhost",
        "INVOKER",
        "x$waits_by_host_by_latency",
        "NONE",
        "YES",
        "mysql.sys@localhost",
        "INVOKER",
        "x$waits_by_user_by_latency",
        "NONE",
        "YES",
        "mysql.sys@localhost",
        "INVOKER",
        "x$waits_global_by_latency",
        "NONE",
        "YES",
        "mysql.sys@localhost",
        "INVOKER",
    };
    static const char *const view_table_usage_values[] = {
        "wait_classes_global_by_avg_latency",
        "performance_schema",
        "events_waits_summary_global_by_event_name",
        "wait_classes_global_by_latency",
        "performance_schema",
        "events_waits_summary_global_by_event_name",
        "waits_by_host_by_latency",
        "performance_schema",
        "events_waits_summary_by_host_by_event_name",
        "waits_by_user_by_latency",
        "performance_schema",
        "events_waits_summary_by_user_by_event_name",
        "waits_global_by_latency",
        "performance_schema",
        "events_waits_summary_global_by_event_name",
        "x$wait_classes_global_by_avg_latency",
        "performance_schema",
        "events_waits_summary_global_by_event_name",
        "x$wait_classes_global_by_latency",
        "performance_schema",
        "events_waits_summary_global_by_event_name",
        "x$waits_by_host_by_latency",
        "performance_schema",
        "events_waits_summary_by_host_by_event_name",
        "x$waits_by_user_by_latency",
        "performance_schema",
        "events_waits_summary_by_user_by_event_name",
        "x$waits_global_by_latency",
        "performance_schema",
        "events_waits_summary_global_by_event_name",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open memory database");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM sys.wait_classes_global_by_latency",
            .column_names = count_column,
            .column_count = count_column_count,
            .values = count_zero,
            .row_count = 1,
            .context = "wait class empty rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM sys.`x$waits_global_by_latency`",
            .column_names = count_column,
            .column_count = count_column_count,
            .values = count_zero,
            .row_count = 1,
            .context = "x waits global empty rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM sys.wait_classes_global_by_avg_latency",
            .column_names = show_columns_columns,
            .column_count = show_columns_column_count,
            .values = wait_class_show_columns_values,
            .row_count = wait_class_show_columns_row_count,
            .context = "wait class formatted SHOW COLUMNS",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "DESCRIBE sys.`x$wait_classes_global_by_latency`",
            .column_names = show_columns_columns,
            .column_count = show_columns_column_count,
            .values = x_wait_class_show_columns_values,
            .row_count = wait_class_show_columns_row_count,
            .context = "wait class raw DESCRIBE",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM sys.waits_by_user_by_latency",
            .column_names = show_columns_columns,
            .column_count = show_columns_column_count,
            .values = waits_user_show_columns_values,
            .row_count = wait_class_show_columns_row_count,
            .context = "waits by user SHOW COLUMNS",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "DESCRIBE sys.`x$waits_global_by_latency`",
            .column_names = show_columns_columns,
            .column_count = show_columns_column_count,
            .values = x_waits_global_show_columns_values,
            .row_count = waits_global_show_columns_row_count,
            .context = "x waits global DESCRIBE",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME IN ("
                   "'wait_classes_global_by_avg_latency', "
                   "'x$wait_classes_global_by_avg_latency', "
                   "'wait_classes_global_by_latency', "
                   "'x$wait_classes_global_by_latency', "
                   "'waits_by_host_by_latency', 'x$waits_by_host_by_latency', "
                   "'waits_by_user_by_latency', 'x$waits_by_user_by_latency', "
                   "'waits_global_by_latency', 'x$waits_global_by_latency')",
            .column_names = count_column,
            .column_count = count_column_count,
            .values = count_58,
            .row_count = 1,
            .context = "wait information_schema columns count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, COLUMN_TYPE, IS_NULLABLE, COLUMN_DEFAULT, "
                   "CHARACTER_SET_NAME, COLLATION_NAME "
                   "FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = 'sys' "
                   "AND ((TABLE_NAME = 'waits_by_host_by_latency' "
                   "AND COLUMN_NAME = 'host') "
                   "OR (TABLE_NAME = 'waits_by_user_by_latency' "
                   "AND COLUMN_NAME = 'user') "
                   "OR (TABLE_NAME = 'waits_global_by_latency' "
                   "AND COLUMN_NAME = 'events') "
                   "OR (TABLE_NAME = 'x$wait_classes_global_by_latency' "
                   "AND COLUMN_NAME = 'avg_latency')) "
                   "ORDER BY TABLE_NAME, COLUMN_NAME",
            .column_names = information_schema_special_columns,
            .column_count = information_schema_special_column_count,
            .values = special_column_values,
            .row_count = 4,
            .context = "wait special column metadata",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, TABLE_ROWS, TABLE_COMMENT "
                   "FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'sys' "
                   "AND TABLE_NAME IN ("
                   "'wait_classes_global_by_avg_latency', "
                   "'x$wait_classes_global_by_avg_latency', "
                   "'wait_classes_global_by_latency', "
                   "'x$wait_classes_global_by_latency', "
                   "'waits_by_host_by_latency', 'x$waits_by_host_by_latency', "
                   "'waits_by_user_by_latency', 'x$waits_by_user_by_latency', "
                   "'waits_global_by_latency', 'x$waits_global_by_latency') "
                   "ORDER BY TABLE_NAME",
            .column_names = metadata_columns,
            .column_count = metadata_column_count,
            .values = table_metadata_values,
            .row_count = information_schema_views_row_count,
            .context = "wait information_schema tables",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, CHECK_OPTION, IS_UPDATABLE, DEFINER, SECURITY_TYPE "
                   "FROM INFORMATION_SCHEMA.VIEWS WHERE TABLE_SCHEMA = 'sys' "
                   "AND TABLE_NAME IN ("
                   "'wait_classes_global_by_avg_latency', "
                   "'x$wait_classes_global_by_avg_latency', "
                   "'wait_classes_global_by_latency', "
                   "'x$wait_classes_global_by_latency', "
                   "'waits_by_host_by_latency', 'x$waits_by_host_by_latency', "
                   "'waits_by_user_by_latency', 'x$waits_by_user_by_latency', "
                   "'waits_global_by_latency', 'x$waits_global_by_latency') "
                   "ORDER BY TABLE_NAME",
            .column_names = information_schema_views_columns,
            .column_count = information_schema_views_column_count,
            .values = views_values,
            .row_count = information_schema_views_row_count,
            .context = "wait information_schema views",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT VIEW_NAME, TABLE_SCHEMA, TABLE_NAME "
                   "FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE WHERE VIEW_SCHEMA = 'sys' "
                   "AND VIEW_NAME IN ("
                   "'wait_classes_global_by_avg_latency', "
                   "'x$wait_classes_global_by_avg_latency', "
                   "'wait_classes_global_by_latency', "
                   "'x$wait_classes_global_by_latency', "
                   "'waits_by_host_by_latency', 'x$waits_by_host_by_latency', "
                   "'waits_by_user_by_latency', 'x$waits_by_user_by_latency', "
                   "'waits_global_by_latency', 'x$waits_global_by_latency') "
                   "ORDER BY VIEW_NAME, TABLE_SCHEMA, TABLE_NAME",
            .column_names = information_schema_view_table_usage_columns,
            .column_count = information_schema_view_table_usage_column_count,
            .values = view_table_usage_values,
            .row_count = information_schema_views_row_count,
            .context = "wait view table usage",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE "
                   "WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME IN ("
                   "'wait_classes_global_by_avg_latency', "
                   "'x$wait_classes_global_by_avg_latency', "
                   "'wait_classes_global_by_latency', "
                   "'x$wait_classes_global_by_latency', "
                   "'waits_by_host_by_latency', 'x$waits_by_host_by_latency', "
                   "'waits_by_user_by_latency', 'x$waits_by_user_by_latency', "
                   "'waits_global_by_latency', 'x$waits_global_by_latency')",
            .column_names = count_column,
            .column_count = count_column_count,
            .values = count_zero,
            .row_count = 1,
            .context = "wait empty routine usage",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME IN ("
                   "'wait_classes_global_by_avg_latency', "
                   "'x$wait_classes_global_by_avg_latency', "
                   "'wait_classes_global_by_latency', "
                   "'x$wait_classes_global_by_latency', "
                   "'waits_by_host_by_latency', 'x$waits_by_host_by_latency', "
                   "'waits_by_user_by_latency', 'x$waits_by_user_by_latency', "
                   "'waits_global_by_latency', 'x$waits_global_by_latency')",
            .column_names = count_column,
            .column_count = count_column_count,
            .values = count_zero,
            .row_count = 1,
            .context = "wait empty statistics",
        }
    );
    failures += expect_query_contains(
        database,
        (struct expected_query_contains){
            .sql = "SHOW CREATE VIEW sys.`x$wait_classes_global_by_avg_latency`",
            .column_names = show_create_view_columns,
            .column_count = show_create_view_column_count,
            .row_count = 1,
            .row_index = 0,
            .column_index = 1,
            .needle = "order by ifnull((sum(`performance_schema`."
                      "`events_waits_summary_global_by_event_name`.`SUM_TIMER_WAIT`)",
            .context = "x wait class avg SHOW CREATE",
        }
    );
    failures += expect_query_contains(
        database,
        (struct expected_query_contains){
            .sql = "SHOW CREATE VIEW sys.waits_global_by_latency",
            .column_names = show_create_view_columns,
            .column_count = show_create_view_column_count,
            .row_count = 1,
            .row_index = 0,
            .column_index = 1,
            .needle = "`EVENT_NAME` AS `event`",
            .context = "waits global SHOW CREATE",
        }
    );
    failures += expect_statement_ok(database, "USE sys");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM waits_by_host_by_latency",
            .column_names = count_column,
            .column_count = count_column_count,
            .values = count_zero,
            .row_count = 1,
            .context = "selected schema waits_by_host_by_latency count",
        }
    );
    failures += expect_query_contains(
        database,
        (struct expected_query_contains){
            .sql = "SHOW CREATE TABLE waits_by_host_by_latency",
            .column_names = show_create_view_columns,
            .column_count = show_create_view_column_count,
            .row_count = 1,
            .row_index = 0,
            .column_index = 1,
            .needle = "VIEW `waits_by_host_by_latency`",
            .context = "selected schema waits_by_host_by_latency SHOW CREATE",
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
