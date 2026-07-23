#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#  include <process.h>
#else
#  include <unistd.h>
#endif

enum {
    test_path_capacity = 1024,
    tables_with_full_table_scans_column_count = 4,
    show_columns_column_count = 6,
    show_index_column_count = 15,
    information_schema_columns_sample_column_count = 7,
    information_schema_tables_sample_column_count = 7,
    information_schema_views_column_count = 9,
    information_schema_view_table_usage_column_count = 4,
    show_create_view_column_count = 4,
    information_schema_columns_sample_row_count = 8,
    view_table_usage_row_count = 2,
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

static int test_sys_schema_tables_with_full_table_scans_views(void);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_query_contains(mylite_db *database, struct expected_query_contains expected);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static const char
    *const tables_with_full_table_scans_columns[tables_with_full_table_scans_column_count] = {
        "object_schema",
        "object_name",
        "rows_full_scanned",
        "latency",
};

static const char *const show_columns_columns[show_columns_column_count] = {
    "Field",
    "Type",
    "Null",
    "Key",
    "Default",
    "Extra",
};

static const char *const show_index_columns[show_index_column_count] = {
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

static const char *const
    information_schema_columns_sample_columns[information_schema_columns_sample_column_count] = {
        "TABLE_NAME",
        "COLUMN_NAME",
        "ORDINAL_POSITION",
        "IS_NULLABLE",
        "COLUMN_TYPE",
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
    return test_sys_schema_tables_with_full_table_scans_views() == 0 ? 0 : 1;
}

static int test_sys_schema_tables_with_full_table_scans_views(void) {
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_zero[] = {"0"};
    static const char *const row_count_column[] = {"ROW_COUNT()"};
    static const char *const row_count_minus_one[] = {"-1"};
    static const char *const formatted_show_columns_values[] = {
        "object_schema",     "varchar(64)",     "YES", "", NULL, "",
        "object_name",       "varchar(64)",     "YES", "", NULL, "",
        "rows_full_scanned", "bigint unsigned", "NO",  "", NULL, "",
        "latency",           "varchar(11)",     "YES", "", NULL, "",
    };
    static const char *const raw_show_columns_values[] = {
        "object_schema",     "varchar(64)",     "YES", "", NULL, "",
        "object_name",       "varchar(64)",     "YES", "", NULL, "",
        "rows_full_scanned", "bigint unsigned", "NO",  "", NULL, "",
        "latency",           "bigint unsigned", "NO",  "", NULL, "",
    };
    static const char *const information_schema_columns_sample_values[] = {
        "schema_tables_with_full_table_scans",
        "object_schema",
        "1",
        "YES",
        "varchar(64)",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "schema_tables_with_full_table_scans",
        "object_name",
        "2",
        "YES",
        "varchar(64)",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "schema_tables_with_full_table_scans",
        "rows_full_scanned",
        "3",
        "NO",
        "bigint unsigned",
        NULL,
        NULL,
        "schema_tables_with_full_table_scans",
        "latency",
        "4",
        "YES",
        "varchar(11)",
        "utf8mb3",
        "utf8mb3_general_ci",
        "x$schema_tables_with_full_table_scans",
        "object_schema",
        "1",
        "YES",
        "varchar(64)",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "x$schema_tables_with_full_table_scans",
        "object_name",
        "2",
        "YES",
        "varchar(64)",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "x$schema_tables_with_full_table_scans",
        "rows_full_scanned",
        "3",
        "NO",
        "bigint unsigned",
        NULL,
        NULL,
        "x$schema_tables_with_full_table_scans",
        "latency",
        "4",
        "NO",
        "bigint unsigned",
        NULL,
        NULL,
    };
    static const char *const information_schema_tables_sample_values[] = {
        "sys",
        "schema_tables_with_full_table_scans",
        "VIEW",
        NULL,
        NULL,
        NULL,
        "VIEW",
        "sys",
        "x$schema_tables_with_full_table_scans",
        "VIEW",
        NULL,
        NULL,
        NULL,
        "VIEW",
    };
    static const char *const information_schema_views_values[] = {
        "def",
        "sys",
        "schema_tables_with_full_table_scans",
        "NONE",
        "YES",
        "mysql.sys@localhost",
        "INVOKER",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "def",
        "sys",
        "x$schema_tables_with_full_table_scans",
        "NONE",
        "YES",
        "mysql.sys@localhost",
        "INVOKER",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
    };
    static const char *const view_table_usage_values[] = {
        "sys",
        "schema_tables_with_full_table_scans",
        "performance_schema",
        "table_io_waits_summary_by_index_usage",
        "sys",
        "x$schema_tables_with_full_table_scans",
        "performance_schema",
        "table_io_waits_summary_by_index_usage",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "main") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "open file-backed database"
    );
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE scan_target (id INT PRIMARY KEY, value_col INT, filler VARCHAR(20))"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO scan_target VALUES (1, 10, 'a'), (2, 20, 'b'), (3, 20, 'c')"
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM sys.schema_tables_with_full_table_scans",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "sys.schema_tables_with_full_table_scans empty read",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT object_schema, object_name, rows_full_scanned, latency FROM "
                   "sys.`x$schema_tables_with_full_table_scans`",
            .column_names = tables_with_full_table_scans_columns,
            .column_count = tables_with_full_table_scans_column_count,
            .values = NULL,
            .row_count = 0U,
            .context = "sys.x$schema_tables_with_full_table_scans empty read",
        }
    );
    failures += expect_statement_ok(database, "USE sys");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM schema_tables_with_full_table_scans",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "sys.schema_tables_with_full_table_scans selected schema read",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM `x$schema_tables_with_full_table_scans`",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "sys.x$schema_tables_with_full_table_scans selected schema read",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM schema_tables_with_full_table_scans",
            .column_names = show_columns_columns,
            .column_count = show_columns_column_count,
            .values = formatted_show_columns_values,
            .row_count = tables_with_full_table_scans_column_count,
            .context = "sys.schema_tables_with_full_table_scans show columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM `x$schema_tables_with_full_table_scans`",
            .column_names = show_columns_columns,
            .column_count = show_columns_column_count,
            .values = raw_show_columns_values,
            .row_count = tables_with_full_table_scans_column_count,
            .context = "sys.x$schema_tables_with_full_table_scans show columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "DESCRIBE schema_tables_with_full_table_scans",
            .column_names = show_columns_columns,
            .column_count = show_columns_column_count,
            .values = formatted_show_columns_values,
            .row_count = tables_with_full_table_scans_column_count,
            .context = "sys.schema_tables_with_full_table_scans describe",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "DESCRIBE `x$schema_tables_with_full_table_scans`",
            .column_names = show_columns_columns,
            .column_count = show_columns_column_count,
            .values = raw_show_columns_values,
            .row_count = tables_with_full_table_scans_column_count,
            .context = "sys.x$schema_tables_with_full_table_scans describe",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM schema_tables_with_full_table_scans",
            .column_names = show_index_columns,
            .column_count = show_index_column_count,
            .values = NULL,
            .row_count = 0U,
            .context = "sys.schema_tables_with_full_table_scans show index",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM `x$schema_tables_with_full_table_scans`",
            .column_names = show_index_columns,
            .column_count = show_index_column_count,
            .values = NULL,
            .row_count = 0U,
            .context = "sys.x$schema_tables_with_full_table_scans show index",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, "
                   "COLUMN_TYPE, CHARACTER_SET_NAME, COLLATION_NAME "
                   "FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = 'sys' "
                   "AND TABLE_NAME IN ('schema_tables_with_full_table_scans', "
                   "'x$schema_tables_with_full_table_scans') ORDER BY TABLE_NAME",
            .column_names = information_schema_columns_sample_columns,
            .column_count = information_schema_columns_sample_column_count,
            .values = information_schema_columns_sample_values,
            .row_count = information_schema_columns_sample_row_count,
            .context = "sys tables with full table scans information_schema.columns rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, TABLE_ROWS, "
                   "DATA_LENGTH, TABLE_COMMENT FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME IN "
                   "('schema_tables_with_full_table_scans', "
                   "'x$schema_tables_with_full_table_scans') ORDER BY TABLE_NAME",
            .column_names = information_schema_tables_sample_columns,
            .column_count = information_schema_tables_sample_column_count,
            .values = information_schema_tables_sample_values,
            .row_count = 2U,
            .context = "sys tables with full table scans information_schema.tables rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_CATALOG, TABLE_SCHEMA, TABLE_NAME, CHECK_OPTION, "
                   "IS_UPDATABLE, DEFINER, SECURITY_TYPE, CHARACTER_SET_CLIENT, "
                   "COLLATION_CONNECTION FROM INFORMATION_SCHEMA.VIEWS "
                   "WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME IN "
                   "('schema_tables_with_full_table_scans', "
                   "'x$schema_tables_with_full_table_scans') ORDER BY TABLE_NAME",
            .column_names = information_schema_views_columns,
            .column_count = information_schema_views_column_count,
            .values = information_schema_views_values,
            .row_count = 2U,
            .context = "sys tables with full table scans information_schema.views rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT VIEW_SCHEMA, VIEW_NAME, TABLE_SCHEMA, TABLE_NAME "
                   "FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE "
                   "WHERE VIEW_SCHEMA = 'sys' AND VIEW_NAME IN "
                   "('schema_tables_with_full_table_scans', "
                   "'x$schema_tables_with_full_table_scans') ORDER BY VIEW_NAME",
            .column_names = information_schema_view_table_usage_columns,
            .column_count = information_schema_view_table_usage_column_count,
            .values = view_table_usage_values,
            .row_count = view_table_usage_row_count,
            .context = "sys tables with full table scans view_table_usage rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE "
                   "WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME IN "
                   "('schema_tables_with_full_table_scans', "
                   "'x$schema_tables_with_full_table_scans')",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "sys tables with full table scans empty view_routine_usage",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME IN "
                   "('schema_tables_with_full_table_scans', "
                   "'x$schema_tables_with_full_table_scans')",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "sys tables with full table scans empty statistics",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "
                   "WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME IN "
                   "('schema_tables_with_full_table_scans', "
                   "'x$schema_tables_with_full_table_scans')",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "sys tables with full table scans empty table_constraints",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE "
                   "WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME IN "
                   "('schema_tables_with_full_table_scans', "
                   "'x$schema_tables_with_full_table_scans')",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "sys tables with full table scans empty key_column_usage",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS "
                   "WHERE CONSTRAINT_SCHEMA = 'sys' AND TABLE_NAME IN "
                   "('schema_tables_with_full_table_scans', "
                   "'x$schema_tables_with_full_table_scans')",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "sys tables with full table scans empty table_constraints_extensions",
        }
    );
    failures += expect_query_contains(
        database,
        (struct expected_query_contains){
            .sql = "SHOW CREATE VIEW schema_tables_with_full_table_scans",
            .column_names = show_create_view_columns,
            .column_count = show_create_view_column_count,
            .row_count = 1U,
            .row_index = 0U,
            .column_index = 1U,
            .needle = "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY "
                      "INVOKER VIEW `schema_tables_with_full_table_scans`",
            .context = "sys.schema_tables_with_full_table_scans show create view",
        }
    );
    failures += expect_query_contains(
        database,
        (struct expected_query_contains){
            .sql = "SHOW CREATE VIEW schema_tables_with_full_table_scans",
            .column_names = show_create_view_columns,
            .column_count = show_create_view_column_count,
            .row_count = 1U,
            .row_index = 0U,
            .column_index = 1U,
            .needle = "format_pico_time(`performance_schema`."
                      "`table_io_waits_summary_by_index_usage`.`SUM_TIMER_WAIT`) AS `latency`",
            .context = "sys.schema_tables_with_full_table_scans show create definition",
        }
    );
    failures += expect_query_contains(
        database,
        (struct expected_query_contains){
            .sql = "SHOW CREATE TABLE `x$schema_tables_with_full_table_scans`",
            .column_names = show_create_view_columns,
            .column_count = show_create_view_column_count,
            .row_count = 1U,
            .row_index = 0U,
            .column_index = 1U,
            .needle = "VIEW `x$schema_tables_with_full_table_scans`",
            .context = "sys.x$schema_tables_with_full_table_scans show create table",
        }
    );
    failures += expect_query_contains(
        database,
        (struct expected_query_contains){
            .sql = "SHOW CREATE TABLE `x$schema_tables_with_full_table_scans`",
            .column_names = show_create_view_columns,
            .column_count = show_create_view_column_count,
            .row_count = 1U,
            .row_index = 0U,
            .column_index = 1U,
            .needle = "`performance_schema`.`table_io_waits_summary_by_index_usage`."
                      "`SUM_TIMER_WAIT` AS `latency`",
            .context = "sys.x$schema_tables_with_full_table_scans show create definition",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT()",
            .column_names = row_count_column,
            .column_count = 1U,
            .values = row_count_minus_one,
            .row_count = 1U,
            .context = "sys.schema_tables_with_full_table_scans row_count",
        }
    );

    mylite_close(database);
    remove_related_files(path);
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
