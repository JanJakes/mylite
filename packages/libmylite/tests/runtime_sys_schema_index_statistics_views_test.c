#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <process.h>
#else
#  include <unistd.h>
#endif

enum {
    test_path_capacity = 1024,
    sys_index_statistics_column_count = 11,
    show_columns_column_count = 6,
    information_schema_columns_sample_column_count = 6,
    information_schema_tables_sample_column_count = 7,
    information_schema_views_column_count = 9,
    information_schema_view_table_usage_column_count = 4,
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

static int test_sys_schema_index_statistics_views(void);
static int seed_schema_objects(mylite_db *database);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_query_contains(mylite_db *database, struct expected_query_contains expected);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static const char *const sys_index_statistics_columns[sys_index_statistics_column_count] = {
    "table_schema",
    "table_name",
    "index_name",
    "rows_selected",
    "select_latency",
    "rows_inserted",
    "insert_latency",
    "rows_updated",
    "update_latency",
    "rows_deleted",
    "delete_latency",
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
    "TABLE_CATALOG",
    "TABLE_SCHEMA",
    "TABLE_NAME",
    "CHECK_OPTION",
    "IS_UPDATABLE",
    "DEFINER",
    "SECURITY_TYPE",
    "CHARACTER_SET_CLIENT",
    "COLLATION_CONNECTION",
};

static const char *const
    information_schema_columns_sample_columns[information_schema_columns_sample_column_count] = {
        "TABLE_NAME",
        "COLUMN_NAME",
        "COLUMN_TYPE",
        "IS_NULLABLE",
        "CHARACTER_SET_NAME",
        "COLLATION_NAME",
};

static const char *const
    information_schema_tables_sample_columns[information_schema_tables_sample_column_count] = {
        "TABLE_SCHEMA",
        "TABLE_NAME",
        "TABLE_TYPE",
        "ENGINE",
        "TABLE_ROWS",
        "DATA_LENGTH",
        "TABLE_COMMENT",
};

static const char *const
    information_schema_view_table_usage_columns[information_schema_view_table_usage_column_count] =
        {
            "VIEW_SCHEMA",
            "VIEW_NAME",
            "TABLE_SCHEMA",
            "TABLE_NAME",
};

static const char *const show_create_view_columns[show_create_view_column_count] = {
    "View",
    "Create View",
    "character_set_client",
    "collation_connection",
};

int main(void) {
    return test_sys_schema_index_statistics_views() == 0 ? 0 : 1;
}

static int test_sys_schema_index_statistics_views(void) {
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_zero[] = {"0"};
    static const char *const count_three[] = {"3"};
    static const char *const formatted_idx_a_values[] = {
        "app",
        "base_one",
        "idx_a",
        "0",
        "  0 ps",
        "0",
        "  0 ps",
        "0",
        "  0 ps",
        "0",
        "  0 ps",
    };
    static const char *const raw_primary_values[] = {
        "app",
        "base_one",
        "PRIMARY",
        "0",
        "0",
        "0",
        "0",
        "0",
        "0",
        "0",
        "0",
    };
    static const char *const selected_schema_values[] = {
        "app",
        "base_one",
        "uq_b",
    };
    static const char *const sys_config_values[] = {
        "sys",
        "sys_config",
        "PRIMARY",
    };
    static const char *const formatted_show_columns_values[] = {
        "table_schema",   "varchar(64)",     "YES", "", NULL, "",
        "table_name",     "varchar(64)",     "YES", "", NULL, "",
        "index_name",     "varchar(64)",     "YES", "", NULL, "",
        "rows_selected",  "bigint unsigned", "NO",  "", NULL, "",
        "select_latency", "varchar(11)",     "YES", "", NULL, "",
        "rows_inserted",  "bigint unsigned", "NO",  "", NULL, "",
        "insert_latency", "varchar(11)",     "YES", "", NULL, "",
        "rows_updated",   "bigint unsigned", "NO",  "", NULL, "",
        "update_latency", "varchar(11)",     "YES", "", NULL, "",
        "rows_deleted",   "bigint unsigned", "NO",  "", NULL, "",
        "delete_latency", "varchar(11)",     "YES", "", NULL, "",
    };
    static const char *const raw_show_columns_values[] = {
        "table_schema",   "varchar(64)",     "YES", "", NULL, "",
        "table_name",     "varchar(64)",     "YES", "", NULL, "",
        "index_name",     "varchar(64)",     "YES", "", NULL, "",
        "rows_selected",  "bigint unsigned", "NO",  "", NULL, "",
        "select_latency", "bigint unsigned", "NO",  "", NULL, "",
        "rows_inserted",  "bigint unsigned", "NO",  "", NULL, "",
        "insert_latency", "bigint unsigned", "NO",  "", NULL, "",
        "rows_updated",   "bigint unsigned", "NO",  "", NULL, "",
        "update_latency", "bigint unsigned", "NO",  "", NULL, "",
        "rows_deleted",   "bigint unsigned", "NO",  "", NULL, "",
        "delete_latency", "bigint unsigned", "NO",  "", NULL, "",
    };
    static const char *const formatted_views_values[] = {
        "def",
        "sys",
        "schema_index_statistics",
        "NONE",
        "YES",
        "mysql.sys@localhost",
        "INVOKER",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
    };
    static const char *const raw_views_values[] = {
        "def",
        "sys",
        "x$schema_index_statistics",
        "NONE",
        "YES",
        "mysql.sys@localhost",
        "INVOKER",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
    };
    static const char *const formatted_information_schema_columns_sample_values[] = {
        "schema_index_statistics",
        "select_latency",
        "varchar(11)",
        "YES",
        "utf8mb3",
        "utf8mb3_general_ci",
        "schema_index_statistics",
        "delete_latency",
        "varchar(11)",
        "YES",
        "utf8mb3",
        "utf8mb3_general_ci",
    };
    static const char *const raw_information_schema_columns_sample_values[] = {
        "x$schema_index_statistics",
        "select_latency",
        "bigint unsigned",
        "NO",
        NULL,
        NULL,
        "x$schema_index_statistics",
        "delete_latency",
        "bigint unsigned",
        "NO",
        NULL,
        NULL,
    };
    static const char *const information_schema_tables_sample_values[] = {
        "sys",
        "schema_index_statistics",
        "VIEW",
        NULL,
        NULL,
        NULL,
        "VIEW",
        "sys",
        "x$schema_index_statistics",
        "VIEW",
        NULL,
        NULL,
        NULL,
        "VIEW",
    };
    static const char *const formatted_usage_values[] = {
        "sys",
        "schema_index_statistics",
        "performance_schema",
        "table_io_waits_summary_by_index_usage",
    };
    static const char *const raw_usage_values[] = {
        "sys",
        "x$schema_index_statistics",
        "performance_schema",
        "table_io_waits_summary_by_index_usage",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "sys-schema-index-statistics-views") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += seed_schema_objects(database);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM sys.schema_index_statistics WHERE table_schema = 'app'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_three,
            .row_count = 1U,
            .context = "sys.schema_index_statistics app index count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT table_schema, table_name, index_name, rows_selected, select_latency, "
                   "rows_inserted, insert_latency, rows_updated, update_latency, rows_deleted, "
                   "delete_latency FROM sys.schema_index_statistics "
                   "WHERE table_schema = 'app' AND table_name = 'base_one' "
                   "AND index_name = 'idx_a'",
            .column_names = sys_index_statistics_columns,
            .column_count = sys_index_statistics_column_count,
            .values = formatted_idx_a_values,
            .row_count = 1U,
            .context = "sys.schema_index_statistics formatted row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT table_schema, table_name, index_name, rows_selected, select_latency, "
                   "rows_inserted, insert_latency, rows_updated, update_latency, rows_deleted, "
                   "delete_latency FROM sys.`x$schema_index_statistics` "
                   "WHERE table_schema = 'app' AND table_name = 'base_one' "
                   "AND index_name = 'PRIMARY'",
            .column_names = sys_index_statistics_columns,
            .column_count = sys_index_statistics_column_count,
            .values = raw_primary_values,
            .row_count = 1U,
            .context = "sys.x$schema_index_statistics raw row",
        }
    );
    failures += expect_statement_ok(database, "USE sys");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT table_schema, table_name, index_name FROM `x$schema_index_statistics` "
                   "WHERE table_schema = 'app' AND index_name = 'uq_b'",
            .column_names = sys_index_statistics_columns,
            .column_count = 3U,
            .values = selected_schema_values,
            .row_count = 1U,
            .context = "sys.x$schema_index_statistics selected schema read",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT table_schema, table_name, index_name FROM schema_index_statistics "
                   "WHERE table_schema = 'sys' AND table_name = 'sys_config'",
            .column_names = sys_index_statistics_columns,
            .column_count = 3U,
            .values = sys_config_values,
            .row_count = 1U,
            .context = "sys.schema_index_statistics system row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM schema_index_statistics",
            .column_names = show_columns_columns,
            .column_count = show_columns_column_count,
            .values = formatted_show_columns_values,
            .row_count = sizeof(formatted_show_columns_values) /
                         sizeof(formatted_show_columns_values[0]) / show_columns_column_count,
            .context = "sys.schema_index_statistics show columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM `x$schema_index_statistics`",
            .column_names = show_columns_columns,
            .column_count = show_columns_column_count,
            .values = raw_show_columns_values,
            .row_count = sizeof(raw_show_columns_values) / sizeof(raw_show_columns_values[0]) /
                         show_columns_column_count,
            .context = "sys.x$schema_index_statistics show columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, COLUMN_TYPE, IS_NULLABLE, "
                   "CHARACTER_SET_NAME, COLLATION_NAME FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'schema_index_statistics' "
                   "AND COLUMN_NAME IN ('select_latency', 'delete_latency') "
                   "ORDER BY ORDINAL_POSITION",
            .column_names = information_schema_columns_sample_columns,
            .column_count = information_schema_columns_sample_column_count,
            .values = formatted_information_schema_columns_sample_values,
            .row_count = sizeof(formatted_information_schema_columns_sample_values) /
                         sizeof(formatted_information_schema_columns_sample_values[0]) /
                         information_schema_columns_sample_column_count,
            .context = "sys.schema_index_statistics information_schema.columns sample",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, COLUMN_TYPE, IS_NULLABLE, "
                   "CHARACTER_SET_NAME, COLLATION_NAME FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'x$schema_index_statistics' "
                   "AND COLUMN_NAME IN ('select_latency', 'delete_latency') "
                   "ORDER BY ORDINAL_POSITION",
            .column_names = information_schema_columns_sample_columns,
            .column_count = information_schema_columns_sample_column_count,
            .values = raw_information_schema_columns_sample_values,
            .row_count = sizeof(raw_information_schema_columns_sample_values) /
                         sizeof(raw_information_schema_columns_sample_values[0]) /
                         information_schema_columns_sample_column_count,
            .context = "sys.x$schema_index_statistics information_schema.columns sample",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_CATALOG, TABLE_SCHEMA, TABLE_NAME, CHECK_OPTION, "
                   "IS_UPDATABLE, DEFINER, SECURITY_TYPE, CHARACTER_SET_CLIENT, "
                   "COLLATION_CONNECTION FROM INFORMATION_SCHEMA.VIEWS "
                   "WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'schema_index_statistics'",
            .column_names = information_schema_views_columns,
            .column_count = information_schema_views_column_count,
            .values = formatted_views_values,
            .row_count = 1U,
            .context = "sys.schema_index_statistics information_schema.views",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_CATALOG, TABLE_SCHEMA, TABLE_NAME, CHECK_OPTION, "
                   "IS_UPDATABLE, DEFINER, SECURITY_TYPE, CHARACTER_SET_CLIENT, "
                   "COLLATION_CONNECTION FROM INFORMATION_SCHEMA.VIEWS "
                   "WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'x$schema_index_statistics'",
            .column_names = information_schema_views_columns,
            .column_count = information_schema_views_column_count,
            .values = raw_views_values,
            .row_count = 1U,
            .context = "sys.x$schema_index_statistics information_schema.views",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT VIEW_SCHEMA, VIEW_NAME, TABLE_SCHEMA, TABLE_NAME "
                   "FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE "
                   "WHERE VIEW_SCHEMA = 'sys' AND VIEW_NAME = 'schema_index_statistics'",
            .column_names = information_schema_view_table_usage_columns,
            .column_count = information_schema_view_table_usage_column_count,
            .values = formatted_usage_values,
            .row_count = 1U,
            .context = "sys.schema_index_statistics information_schema.view_table_usage",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT VIEW_SCHEMA, VIEW_NAME, TABLE_SCHEMA, TABLE_NAME "
                   "FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE "
                   "WHERE VIEW_SCHEMA = 'sys' AND VIEW_NAME = 'x$schema_index_statistics'",
            .column_names = information_schema_view_table_usage_columns,
            .column_count = information_schema_view_table_usage_column_count,
            .values = raw_usage_values,
            .row_count = 1U,
            .context = "sys.x$schema_index_statistics information_schema.view_table_usage",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'sys' "
                   "AND TABLE_NAME IN ('schema_index_statistics', 'x$schema_index_statistics')",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "sys schema index statistics statistics count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "
                   "WHERE TABLE_SCHEMA = 'sys' "
                   "AND TABLE_NAME IN ('schema_index_statistics', 'x$schema_index_statistics')",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "sys schema index statistics table_constraints count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE "
                   "WHERE TABLE_SCHEMA = 'sys' "
                   "AND TABLE_NAME IN ('schema_index_statistics', 'x$schema_index_statistics')",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "sys schema index statistics key_column_usage count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS "
                   "WHERE CONSTRAINT_SCHEMA = 'sys' "
                   "AND TABLE_NAME IN ('schema_index_statistics', 'x$schema_index_statistics')",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "sys schema index statistics table_constraints_extensions count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE "
                   "WHERE TABLE_SCHEMA = 'sys' "
                   "AND TABLE_NAME IN ('schema_index_statistics', 'x$schema_index_statistics')",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "sys schema index statistics view_routine_usage count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, TABLE_ROWS, "
                   "DATA_LENGTH, TABLE_COMMENT FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'sys' "
                   "AND TABLE_NAME IN ('schema_index_statistics', 'x$schema_index_statistics') "
                   "ORDER BY TABLE_NAME",
            .column_names = information_schema_tables_sample_columns,
            .column_count = information_schema_tables_sample_column_count,
            .values = information_schema_tables_sample_values,
            .row_count = sizeof(information_schema_tables_sample_values) /
                         sizeof(information_schema_tables_sample_values[0]) /
                         information_schema_tables_sample_column_count,
            .context = "sys schema index statistics information_schema.tables sample",
        }
    );
    failures += expect_query_contains(
        database,
        (struct expected_query_contains){
            .sql = "SHOW CREATE VIEW schema_index_statistics",
            .column_names = show_create_view_columns,
            .column_count = show_create_view_column_count,
            .row_count = 1U,
            .row_index = 0U,
            .column_index = 1U,
            .needle = "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY "
                      "INVOKER VIEW `schema_index_statistics`",
            .context = "sys.schema_index_statistics show create view",
        }
    );
    failures += expect_query_contains(
        database,
        (struct expected_query_contains){
            .sql = "SHOW CREATE TABLE `x$schema_index_statistics`",
            .column_names = show_create_view_columns,
            .column_count = show_create_view_column_count,
            .row_count = 1U,
            .row_index = 0U,
            .column_index = 1U,
            .needle = "VIEW `x$schema_index_statistics`",
            .context = "sys.x$schema_index_statistics show create table",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int seed_schema_objects(mylite_db *database) {
    int failures = 0;

    failures += expect_statement_ok(
        database,
        "CREATE TABLE base_one (id INT PRIMARY KEY, a INT, b INT, KEY idx_a (a), "
        "UNIQUE KEY uq_b (b))"
    );
    failures += expect_statement_ok(database, "CREATE TABLE base_two (id INT, c VARCHAR(20))");
    return failures;
}

static int expect_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected OK, got %d / %d %s %s\n",
            sql,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }
    mylite_result_free(result);
    return 0;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected OK, got %d / %d %s %s\n",
            expected.context,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }
    if (result == NULL) {
        fprintf(stderr, "%s: expected result object\n", expected.context);
        return 1;
    }

    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        expected.column_count,
        expected.context
    );
    for (size_t column_index = 0U;
         expected.column_names != NULL && column_index < expected.column_count;
         ++column_index) {
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
    failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, expected.context);

    if (expected.values != NULL) {
        for (size_t row_index = 0U; row_index < expected.row_count; ++row_index) {
            for (size_t column_index = 0U; column_index < expected.column_count; ++column_index) {
                const char *expected_value =
                    expected.values[(row_index * expected.column_count) + column_index];
                const char *actual_value =
                    mylite_result_value_text(result, row_index, column_index);

                failures +=
                    mylite_test_expect_text_or_null(actual_value, expected_value, expected.context);
            }
        }
    }

    mylite_result_free(result);
    return failures;
}

static int expect_query_contains(mylite_db *database, struct expected_query_contains expected) {
    mylite_result *result = NULL;
    const char *actual = NULL;
    int failures = 0;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected OK, got %d / %d %s %s\n",
            expected.context,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }
    if (result == NULL) {
        fprintf(stderr, "%s: expected result object\n", expected.context);
        return 1;
    }

    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        expected.column_count,
        expected.context
    );
    for (size_t column_index = 0U;
         expected.column_names != NULL && column_index < expected.column_count;
         ++column_index) {
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
    failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, expected.context);

    if (failures == 0) {
        actual = mylite_result_value_text(result, expected.row_index, expected.column_index);
        if (actual == NULL || strstr(actual, expected.needle) == NULL) {
            fprintf(
                stderr,
                "%s: expected value to contain [%s], got [%s]\n",
                expected.context,
                expected.needle,
                actual == NULL ? "<NULL>" : actual
            );
            ++failures;
        }
    }

    mylite_result_free(result);
    return failures;
}

static void remove_related_files(const char *path) {
    remove_with_suffix(path, "");
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char buffer[test_path_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(buffer)) {
        (void)remove(buffer);
    }
}
