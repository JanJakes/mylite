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
    sys_table_statistics_with_buffer_column_count = 25,
    show_columns_column_count = 6,
    show_columns_sample_row_count = 6,
    information_schema_columns_sample_column_count = 7,
    information_schema_columns_sample_row_count = 10,
    information_schema_tables_sample_column_count = 7,
    information_schema_views_column_count = 9,
    information_schema_view_table_usage_column_count = 4,
    show_create_view_column_count = 4,
    view_table_usage_row_count = 6,
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

static int test_sys_schema_table_statistics_with_buffer_views(void);
static int seed_schema_objects(mylite_db *database);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_query_contains(mylite_db *database, struct expected_query_contains expected);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text_or_null(const char *actual, const char *expected, const char *context);

static const char *const
    sys_table_statistics_with_buffer_columns[sys_table_statistics_with_buffer_column_count] = {
        "table_schema",
        "table_name",
        "rows_fetched",
        "fetch_latency",
        "rows_inserted",
        "insert_latency",
        "rows_updated",
        "update_latency",
        "rows_deleted",
        "delete_latency",
        "io_read_requests",
        "io_read",
        "io_read_latency",
        "io_write_requests",
        "io_write",
        "io_write_latency",
        "io_misc_requests",
        "io_misc_latency",
        "innodb_buffer_allocated",
        "innodb_buffer_data",
        "innodb_buffer_free",
        "innodb_buffer_pages",
        "innodb_buffer_pages_hashed",
        "innodb_buffer_pages_old",
        "innodb_buffer_rows_cached",
};

static const char *const show_columns_columns[show_columns_column_count] = {
    "Field",
    "Type",
    "Null",
    "Key",
    "Default",
    "Extra",
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
    return test_sys_schema_table_statistics_with_buffer_views() == 0 ? 0 : 1;
}

static int test_sys_schema_table_statistics_with_buffer_views(void) {
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_zero[] = {"0"};
    static const char *const count_two[] = {"2"};
    static const char *const row_count_column[] = {"ROW_COUNT()"};
    static const char *const row_count_minus_one[] = {"-1"};
    static const char *const formatted_base_one_values[] = {
        "app",        "base_one", "0",      "  0 ps", "0",          "  0 ps",     "0",
        "  0 ps",     "0",        "  0 ps", "0",      "   0 bytes", "  0 ps",     "0",
        "   0 bytes", "  0 ps",   "0",      "  0 ps", "   0 bytes", "   0 bytes", "   0 bytes",
        "0",          "0",        "0",      "0",
    };
    static const char *const raw_base_two_values[] = {
        "app", "base_two", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0",
        "0",   "0",        "0", "0", "0", "0", "0", "0", "0", "0", "0", "0",
    };
    static const char *const selected_schema_values[] = {
        "app",
        "base_one",
    };
    static const char *const sys_config_values[] = {
        "sys",
        "sys_config",
    };
    static const char *const formatted_show_columns_values[] = {
        "table_schema",
        "varchar(64)",
        "YES",
        "",
        NULL,
        "",
        "rows_fetched",
        "bigint unsigned",
        "NO",
        "",
        NULL,
        "",
        "fetch_latency",
        "varchar(11)",
        "YES",
        "",
        NULL,
        "",
        "innodb_buffer_allocated",
        "varchar(11)",
        "YES",
        "",
        NULL,
        "",
        "innodb_buffer_pages",
        "bigint",
        "YES",
        "",
        "0",
        "",
        "innodb_buffer_rows_cached",
        "decimal(45,0)",
        "YES",
        "",
        "0",
        "",
    };
    static const char *const raw_show_columns_values[] = {
        "table_schema",
        "varchar(64)",
        "YES",
        "",
        NULL,
        "",
        "rows_fetched",
        "bigint unsigned",
        "NO",
        "",
        NULL,
        "",
        "fetch_latency",
        "bigint unsigned",
        "NO",
        "",
        NULL,
        "",
        "innodb_buffer_allocated",
        "decimal(44,0)",
        "YES",
        "",
        NULL,
        "",
        "innodb_buffer_pages",
        "bigint",
        "YES",
        "",
        "0",
        "",
        "innodb_buffer_rows_cached",
        "decimal(45,0)",
        "YES",
        "",
        "0",
        "",
    };
    static const char *const information_schema_columns_sample_values[] = {
        "schema_table_statistics_with_buffer",
        "table_schema",
        "1",
        "YES",
        "varchar(64)",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "schema_table_statistics_with_buffer",
        "rows_fetched",
        "3",
        "NO",
        "bigint unsigned",
        NULL,
        NULL,
        "schema_table_statistics_with_buffer",
        "fetch_latency",
        "4",
        "YES",
        "varchar(11)",
        "utf8mb3",
        "utf8mb3_general_ci",
        "schema_table_statistics_with_buffer",
        "innodb_buffer_allocated",
        "19",
        "YES",
        "varchar(11)",
        "utf8mb3",
        "utf8mb3_general_ci",
        "schema_table_statistics_with_buffer",
        "innodb_buffer_rows_cached",
        "25",
        "YES",
        "decimal(45,0)",
        NULL,
        NULL,
        "x$schema_table_statistics_with_buffer",
        "table_schema",
        "1",
        "YES",
        "varchar(64)",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "x$schema_table_statistics_with_buffer",
        "rows_fetched",
        "3",
        "NO",
        "bigint unsigned",
        NULL,
        NULL,
        "x$schema_table_statistics_with_buffer",
        "fetch_latency",
        "4",
        "NO",
        "bigint unsigned",
        NULL,
        NULL,
        "x$schema_table_statistics_with_buffer",
        "innodb_buffer_allocated",
        "19",
        "YES",
        "decimal(44,0)",
        NULL,
        NULL,
        "x$schema_table_statistics_with_buffer",
        "innodb_buffer_rows_cached",
        "25",
        "YES",
        "decimal(45,0)",
        NULL,
        NULL,
    };
    static const char *const information_schema_tables_sample_values[] = {
        "sys",
        "schema_table_statistics_with_buffer",
        "VIEW",
        NULL,
        NULL,
        NULL,
        "VIEW",
        "sys",
        "x$schema_table_statistics_with_buffer",
        "VIEW",
        NULL,
        NULL,
        NULL,
        "VIEW",
    };
    static const char *const information_schema_views_values[] = {
        "def",
        "sys",
        "schema_table_statistics_with_buffer",
        "NONE",
        "NO",
        "mysql.sys@localhost",
        "INVOKER",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "def",
        "sys",
        "x$schema_table_statistics_with_buffer",
        "NONE",
        "NO",
        "mysql.sys@localhost",
        "INVOKER",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
    };
    static const char *const view_table_usage_values[] = {
        "sys",
        "schema_table_statistics_with_buffer",
        "performance_schema",
        "table_io_waits_summary_by_table",
        "sys",
        "schema_table_statistics_with_buffer",
        "sys",
        "x$innodb_buffer_stats_by_table",
        "sys",
        "schema_table_statistics_with_buffer",
        "sys",
        "x$ps_schema_table_statistics_io",
        "sys",
        "x$schema_table_statistics_with_buffer",
        "performance_schema",
        "table_io_waits_summary_by_table",
        "sys",
        "x$schema_table_statistics_with_buffer",
        "sys",
        "x$innodb_buffer_stats_by_table",
        "sys",
        "x$schema_table_statistics_with_buffer",
        "sys",
        "x$ps_schema_table_statistics_io",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "main") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open file-backed database");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += seed_schema_objects(database);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM sys.schema_table_statistics_with_buffer "
                   "WHERE table_schema = 'app'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_two,
            .row_count = 1U,
            .context = "sys.schema_table_statistics_with_buffer app table count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT table_schema, table_name, rows_fetched, fetch_latency, "
                   "rows_inserted, insert_latency, rows_updated, update_latency, "
                   "rows_deleted, delete_latency, io_read_requests, io_read, "
                   "io_read_latency, io_write_requests, io_write, io_write_latency, "
                   "io_misc_requests, io_misc_latency, innodb_buffer_allocated, "
                   "innodb_buffer_data, innodb_buffer_free, innodb_buffer_pages, "
                   "innodb_buffer_pages_hashed, innodb_buffer_pages_old, "
                   "innodb_buffer_rows_cached FROM "
                   "sys.schema_table_statistics_with_buffer WHERE table_schema = 'app' "
                   "AND table_name = 'base_one'",
            .column_names = sys_table_statistics_with_buffer_columns,
            .column_count = sys_table_statistics_with_buffer_column_count,
            .values = formatted_base_one_values,
            .row_count = 1U,
            .context = "sys.schema_table_statistics_with_buffer formatted row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT table_schema, table_name, rows_fetched, fetch_latency, "
                   "rows_inserted, insert_latency, rows_updated, update_latency, "
                   "rows_deleted, delete_latency, io_read_requests, io_read, "
                   "io_read_latency, io_write_requests, io_write, io_write_latency, "
                   "io_misc_requests, io_misc_latency, innodb_buffer_allocated, "
                   "innodb_buffer_data, innodb_buffer_free, innodb_buffer_pages, "
                   "innodb_buffer_pages_hashed, innodb_buffer_pages_old, "
                   "innodb_buffer_rows_cached FROM "
                   "sys.`x$schema_table_statistics_with_buffer` WHERE table_schema = 'app' "
                   "AND table_name = 'base_two'",
            .column_names = sys_table_statistics_with_buffer_columns,
            .column_count = sys_table_statistics_with_buffer_column_count,
            .values = raw_base_two_values,
            .row_count = 1U,
            .context = "sys.x$schema_table_statistics_with_buffer raw row",
        }
    );
    failures += expect_statement_ok(database, "USE sys");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT table_schema, table_name FROM schema_table_statistics_with_buffer "
                   "WHERE table_schema = 'app' AND table_name = 'base_one'",
            .column_names = sys_table_statistics_with_buffer_columns,
            .column_count = 2U,
            .values = selected_schema_values,
            .row_count = 1U,
            .context = "sys.schema_table_statistics_with_buffer selected schema read",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT table_schema, table_name FROM `x$schema_table_statistics_with_buffer` "
                   "WHERE table_schema = 'sys' AND table_name = 'sys_config'",
            .column_names = sys_table_statistics_with_buffer_columns,
            .column_count = 2U,
            .values = sys_config_values,
            .row_count = 1U,
            .context = "sys.x$schema_table_statistics_with_buffer system row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM schema_table_statistics_with_buffer "
                   "WHERE Field IN ('table_schema', 'rows_fetched', 'fetch_latency', "
                   "'innodb_buffer_allocated', 'innodb_buffer_pages', "
                   "'innodb_buffer_rows_cached')",
            .column_names = show_columns_columns,
            .column_count = show_columns_column_count,
            .values = formatted_show_columns_values,
            .row_count = show_columns_sample_row_count,
            .context = "sys.schema_table_statistics_with_buffer show columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM `x$schema_table_statistics_with_buffer` "
                   "WHERE Field IN ('table_schema', 'rows_fetched', 'fetch_latency', "
                   "'innodb_buffer_allocated', 'innodb_buffer_pages', "
                   "'innodb_buffer_rows_cached')",
            .column_names = show_columns_columns,
            .column_count = show_columns_column_count,
            .values = raw_show_columns_values,
            .row_count = show_columns_sample_row_count,
            .context = "sys.x$schema_table_statistics_with_buffer show columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, "
                   "COLUMN_TYPE, CHARACTER_SET_NAME, COLLATION_NAME "
                   "FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = 'sys' "
                   "AND (TABLE_NAME = 'schema_table_statistics_with_buffer' OR TABLE_NAME = "
                   "'x$schema_table_statistics_with_buffer') "
                   "AND (COLUMN_NAME = 'table_schema' OR COLUMN_NAME = "
                   "'rows_fetched' OR COLUMN_NAME = 'fetch_latency' OR COLUMN_NAME = "
                   "'innodb_buffer_allocated' OR COLUMN_NAME = "
                   "'innodb_buffer_rows_cached') ORDER BY TABLE_NAME",
            .column_names = information_schema_columns_sample_columns,
            .column_count = information_schema_columns_sample_column_count,
            .values = information_schema_columns_sample_values,
            .row_count = information_schema_columns_sample_row_count,
            .context = "sys table statistics with buffer information_schema.columns sample",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, TABLE_ROWS, "
                   "DATA_LENGTH, TABLE_COMMENT FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'sys' AND (TABLE_NAME = "
                   "'schema_table_statistics_with_buffer' OR TABLE_NAME = "
                   "'x$schema_table_statistics_with_buffer') ORDER BY TABLE_NAME",
            .column_names = information_schema_tables_sample_columns,
            .column_count = information_schema_tables_sample_column_count,
            .values = information_schema_tables_sample_values,
            .row_count = 2U,
            .context = "sys table statistics with buffer information_schema.tables rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_CATALOG, TABLE_SCHEMA, TABLE_NAME, CHECK_OPTION, "
                   "IS_UPDATABLE, DEFINER, SECURITY_TYPE, CHARACTER_SET_CLIENT, "
                   "COLLATION_CONNECTION FROM INFORMATION_SCHEMA.VIEWS "
                   "WHERE TABLE_SCHEMA = 'sys' AND (TABLE_NAME = "
                   "'schema_table_statistics_with_buffer' OR TABLE_NAME = "
                   "'x$schema_table_statistics_with_buffer') ORDER BY TABLE_NAME",
            .column_names = information_schema_views_columns,
            .column_count = information_schema_views_column_count,
            .values = information_schema_views_values,
            .row_count = 2U,
            .context = "sys table statistics with buffer information_schema.views rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT VIEW_SCHEMA, VIEW_NAME, TABLE_SCHEMA, TABLE_NAME "
                   "FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE "
                   "WHERE VIEW_SCHEMA = 'sys' AND (VIEW_NAME = "
                   "'schema_table_statistics_with_buffer' OR VIEW_NAME = "
                   "'x$schema_table_statistics_with_buffer') ORDER BY VIEW_NAME",
            .column_names = information_schema_view_table_usage_columns,
            .column_count = information_schema_view_table_usage_column_count,
            .values = view_table_usage_values,
            .row_count = view_table_usage_row_count,
            .context = "sys table statistics with buffer view_table_usage rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE "
                   "WHERE TABLE_SCHEMA = 'sys' AND (TABLE_NAME = "
                   "'schema_table_statistics_with_buffer' OR TABLE_NAME = "
                   "'x$schema_table_statistics_with_buffer')",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "sys table statistics with buffer empty view_routine_usage",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'sys' AND (TABLE_NAME = "
                   "'schema_table_statistics_with_buffer' OR TABLE_NAME = "
                   "'x$schema_table_statistics_with_buffer')",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "sys table statistics with buffer empty statistics",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "
                   "WHERE TABLE_SCHEMA = 'sys' AND (TABLE_NAME = "
                   "'schema_table_statistics_with_buffer' OR TABLE_NAME = "
                   "'x$schema_table_statistics_with_buffer')",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "sys table statistics with buffer empty table_constraints",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE "
                   "WHERE TABLE_SCHEMA = 'sys' AND (TABLE_NAME = "
                   "'schema_table_statistics_with_buffer' OR TABLE_NAME = "
                   "'x$schema_table_statistics_with_buffer')",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "sys table statistics with buffer empty key_column_usage",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS "
                   "WHERE CONSTRAINT_SCHEMA = 'sys' AND (TABLE_NAME = "
                   "'schema_table_statistics_with_buffer' OR TABLE_NAME = "
                   "'x$schema_table_statistics_with_buffer')",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "sys table statistics with buffer empty table_constraints_extensions",
        }
    );
    failures += expect_query_contains(
        database,
        (struct expected_query_contains){
            .sql = "SHOW CREATE VIEW schema_table_statistics_with_buffer",
            .column_names = show_create_view_columns,
            .column_count = show_create_view_column_count,
            .row_count = 1U,
            .row_index = 0U,
            .column_index = 1U,
            .needle = "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY "
                      "INVOKER VIEW `schema_table_statistics_with_buffer`",
            .context = "sys.schema_table_statistics_with_buffer show create view",
        }
    );
    failures += expect_query_contains(
        database,
        (struct expected_query_contains){
            .sql = "SHOW CREATE VIEW schema_table_statistics_with_buffer",
            .column_names = show_create_view_columns,
            .column_count = show_create_view_column_count,
            .row_count = 1U,
            .row_index = 0U,
            .column_index = 1U,
            .needle = "format_bytes(`sys`.`ibp`.`allocated`) AS `innodb_buffer_allocated`",
            .context = "sys.schema_table_statistics_with_buffer show create definition",
        }
    );
    failures += expect_query_contains(
        database,
        (struct expected_query_contains){
            .sql = "SHOW CREATE TABLE `x$schema_table_statistics_with_buffer`",
            .column_names = show_create_view_columns,
            .column_count = show_create_view_column_count,
            .row_count = 1U,
            .row_index = 0U,
            .column_index = 1U,
            .needle = "VIEW `x$schema_table_statistics_with_buffer`",
            .context = "sys.x$schema_table_statistics_with_buffer show create table",
        }
    );
    failures += expect_query_contains(
        database,
        (struct expected_query_contains){
            .sql = "SHOW CREATE TABLE `x$schema_table_statistics_with_buffer`",
            .column_names = show_create_view_columns,
            .column_count = show_create_view_column_count,
            .row_count = 1U,
            .row_index = 0U,
            .column_index = 1U,
            .needle = "`ibp`.`allocated` AS `innodb_buffer_allocated`",
            .context = "sys.x$schema_table_statistics_with_buffer show create definition",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM sys.schema_table_statistics_with_buffer "
                   "WHERE table_schema = 'missing_schema'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "sys.schema_table_statistics_with_buffer empty filtered read",
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
            .context = "sys.schema_table_statistics_with_buffer row_count",
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
    failures += expect_statement_ok(database, "CREATE VIEW v_one AS SELECT id FROM base_one");
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

    failures +=
        expect_size(mylite_result_column_count(result), expected.column_count, expected.context);
    for (size_t column_index = 0U;
         expected.column_names != NULL && column_index < expected.column_count;
         ++column_index) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, column_index),
            expected.column_names[column_index],
            expected.context
        );
    }
    failures += expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, expected.context);

    if (expected.values != NULL) {
        for (size_t row_index = 0U; row_index < expected.row_count; ++row_index) {
            for (size_t column_index = 0U; column_index < expected.column_count; ++column_index) {
                const char *expected_value =
                    expected.values[(row_index * expected.column_count) + column_index];
                const char *actual_value =
                    mylite_result_value_text(result, row_index, column_index);

                failures += expect_text_or_null(actual_value, expected_value, expected.context);
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

    failures +=
        expect_size(mylite_result_column_count(result), expected.column_count, expected.context);
    for (size_t column_index = 0U;
         expected.column_names != NULL && column_index < expected.column_count;
         ++column_index) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, column_index),
            expected.column_names[column_index],
            expected.context
        );
    }
    failures += expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, expected.context);

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

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_runtime_sys_schema_table_statistics_with_buffer_views_%d_%s.mylite",
        current_process_id(),
        name
    );

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "test path buffer too small\n");
        return 1;
    }
    return 0;
}

static int current_process_id(void) {
#ifdef _WIN32
    return _getpid();
#else
    return getpid();
#endif
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

static int expect_int(int actual, int expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_int64(int64_t actual, int64_t expected, const char *context) {
    if (actual != expected) {
        fprintf(
            stderr,
            "%s: expected %lld, got %lld\n",
            context,
            (long long)expected,
            (long long)actual
        );
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
