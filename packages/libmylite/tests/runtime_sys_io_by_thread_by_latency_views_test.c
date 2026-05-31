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
    sys_io_by_thread_by_latency_column_count = 8,
    show_columns_column_count = 6,
    show_full_columns_column_count = 9,
    show_index_column_count = 15,
    information_schema_columns_column_count = 7,
    information_schema_columns_row_count = 16,
    information_schema_tables_column_count = 7,
    information_schema_views_column_count = 6,
    information_schema_view_table_usage_column_count = 4,
    information_schema_view_routine_usage_column_count = 4,
    show_create_view_column_count = 4,
    show_table_status_column_count = 18,
    datetime_text_length = 19,
    datetime_year_month_separator = 4,
    datetime_month_day_separator = 7,
    datetime_date_time_separator = 10,
    datetime_hour_minute_separator = 13,
    datetime_minute_second_separator = 16,
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

static const char expected_datetime_value[] = "<datetime>";

static int test_sys_io_by_thread_by_latency_views(void);
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
static int expect_datetime_text(const char *actual, const char *context);

static const char *const
    sys_io_by_thread_by_latency_columns[sys_io_by_thread_by_latency_column_count] = {
        "user",
        "total",
        "total_latency",
        "min_latency",
        "avg_latency",
        "max_latency",
        "thread_id",
        "processlist_id",
};

static const char *const show_columns_columns[show_columns_column_count] = {
    "Field",
    "Type",
    "Null",
    "Key",
    "Default",
    "Extra",
};

static const char *const show_full_columns_columns[show_full_columns_column_count] = {
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
    information_schema_columns_columns[information_schema_columns_column_count] = {
        "TABLE_NAME",
        "COLUMN_NAME",
        "ORDINAL_POSITION",
        "IS_NULLABLE",
        "COLUMN_TYPE",
        "CHARACTER_SET_NAME",
        "COLLATION_NAME",
};

static const char *const information_schema_tables_columns[information_schema_tables_column_count] =
    {
        "TABLE_SCHEMA",
        "TABLE_NAME",
        "TABLE_TYPE",
        "ENGINE",
        "TABLE_ROWS",
        "DATA_LENGTH",
        "TABLE_COMMENT",
};

static const char *const information_schema_views_columns[information_schema_views_column_count] = {
    "TABLE_SCHEMA",
    "TABLE_NAME",
    "CHECK_OPTION",
    "IS_UPDATABLE",
    "DEFINER",
    "SECURITY_TYPE",
};

static const char *const
    information_schema_view_table_usage_columns[information_schema_view_table_usage_column_count] =
        {
            "VIEW_SCHEMA",
            "VIEW_NAME",
            "TABLE_SCHEMA",
            "TABLE_NAME",
};

static const char *const information_schema_view_routine_usage_columns
    [information_schema_view_routine_usage_column_count] = {
        "TABLE_SCHEMA",
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

static const char *const show_table_status_columns[show_table_status_column_count] = {
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

int main(void) {
    return test_sys_io_by_thread_by_latency_views() == 0 ? 0 : 1;
}

static int test_sys_io_by_thread_by_latency_views(void) {
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_zero[] = {"0"};
    static const char *const count_eight[] = {"8"};
    static const char *const count_sixteen[] = {"16"};
    static const char *const row_count_column[] = {"ROW_COUNT()"};
    static const char *const row_count_minus_one[] = {"-1"};
    static const char *const formatted_show_columns_values[] = {
        "user",           "varchar(288)",    "YES", "", NULL, "",
        "total",          "decimal(42,0)",   "YES", "", NULL, "",
        "total_latency",  "varchar(11)",     "YES", "", NULL, "",
        "min_latency",    "varchar(11)",     "YES", "", NULL, "",
        "avg_latency",    "varchar(11)",     "YES", "", NULL, "",
        "max_latency",    "varchar(11)",     "YES", "", NULL, "",
        "thread_id",      "bigint unsigned", "NO",  "", NULL, "",
        "processlist_id", "bigint unsigned", "YES", "", NULL, "",
    };
    static const char *const raw_show_columns_values[] = {
        "user",           "varchar(288)",    "YES", "", NULL, "",
        "total",          "decimal(42,0)",   "YES", "", NULL, "",
        "total_latency",  "decimal(42,0)",   "YES", "", NULL, "",
        "min_latency",    "bigint unsigned", "YES", "", NULL, "",
        "avg_latency",    "decimal(24,4)",   "YES", "", NULL, "",
        "max_latency",    "bigint unsigned", "YES", "", NULL, "",
        "thread_id",      "bigint unsigned", "NO",  "", NULL, "",
        "processlist_id", "bigint unsigned", "YES", "", NULL, "",
    };
    static const char *const formatted_show_full_columns_values[] = {
        "user",
        "varchar(288)",
        "utf8mb4_0900_ai_ci",
        "YES",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "total_latency",
        "varchar(11)",
        "utf8mb3_general_ci",
        "YES",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "min_latency",
        "varchar(11)",
        "utf8mb3_general_ci",
        "YES",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "thread_id",
        "bigint unsigned",
        NULL,
        "NO",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
    };
    static const char *const raw_show_full_columns_values[] = {
        "user",
        "varchar(288)",
        "utf8mb4_0900_ai_ci",
        "YES",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "total_latency",
        "decimal(42,0)",
        NULL,
        "YES",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "min_latency",
        "bigint unsigned",
        NULL,
        "YES",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "thread_id",
        "bigint unsigned",
        NULL,
        "NO",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
    };
    static const char *const information_schema_columns_values[] = {
        "io_by_thread_by_latency",
        "user",
        "1",
        "YES",
        "varchar(288)",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "io_by_thread_by_latency",
        "total",
        "2",
        "YES",
        "decimal(42,0)",
        NULL,
        NULL,
        "io_by_thread_by_latency",
        "total_latency",
        "3",
        "YES",
        "varchar(11)",
        "utf8mb3",
        "utf8mb3_general_ci",
        "io_by_thread_by_latency",
        "min_latency",
        "4",
        "YES",
        "varchar(11)",
        "utf8mb3",
        "utf8mb3_general_ci",
        "io_by_thread_by_latency",
        "avg_latency",
        "5",
        "YES",
        "varchar(11)",
        "utf8mb3",
        "utf8mb3_general_ci",
        "io_by_thread_by_latency",
        "max_latency",
        "6",
        "YES",
        "varchar(11)",
        "utf8mb3",
        "utf8mb3_general_ci",
        "io_by_thread_by_latency",
        "thread_id",
        "7",
        "NO",
        "bigint unsigned",
        NULL,
        NULL,
        "io_by_thread_by_latency",
        "processlist_id",
        "8",
        "YES",
        "bigint unsigned",
        NULL,
        NULL,
        "x$io_by_thread_by_latency",
        "user",
        "1",
        "YES",
        "varchar(288)",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "x$io_by_thread_by_latency",
        "total",
        "2",
        "YES",
        "decimal(42,0)",
        NULL,
        NULL,
        "x$io_by_thread_by_latency",
        "total_latency",
        "3",
        "YES",
        "decimal(42,0)",
        NULL,
        NULL,
        "x$io_by_thread_by_latency",
        "min_latency",
        "4",
        "YES",
        "bigint unsigned",
        NULL,
        NULL,
        "x$io_by_thread_by_latency",
        "avg_latency",
        "5",
        "YES",
        "decimal(24,4)",
        NULL,
        NULL,
        "x$io_by_thread_by_latency",
        "max_latency",
        "6",
        "YES",
        "bigint unsigned",
        NULL,
        NULL,
        "x$io_by_thread_by_latency",
        "thread_id",
        "7",
        "NO",
        "bigint unsigned",
        NULL,
        NULL,
        "x$io_by_thread_by_latency",
        "processlist_id",
        "8",
        "YES",
        "bigint unsigned",
        NULL,
        NULL,
    };
    static const char *const information_schema_tables_values[] = {
        "sys",
        "io_by_thread_by_latency",
        "VIEW",
        NULL,
        NULL,
        NULL,
        "VIEW",
        "sys",
        "x$io_by_thread_by_latency",
        "VIEW",
        NULL,
        NULL,
        NULL,
        "VIEW",
    };
    static const char *const information_schema_views_values[] = {
        "sys",
        "io_by_thread_by_latency",
        "NONE",
        "NO",
        "mysql.sys@localhost",
        "INVOKER",
        "sys",
        "x$io_by_thread_by_latency",
        "NONE",
        "NO",
        "mysql.sys@localhost",
        "INVOKER",
    };
    static const char *const view_table_usage_values[] = {
        "sys",
        "io_by_thread_by_latency",
        "performance_schema",
        "events_waits_summary_by_thread_by_event_name",
        "sys",
        "io_by_thread_by_latency",
        "performance_schema",
        "threads",
        "sys",
        "x$io_by_thread_by_latency",
        "performance_schema",
        "events_waits_summary_by_thread_by_event_name",
        "sys",
        "x$io_by_thread_by_latency",
        "performance_schema",
        "threads",
    };
    static const char *const formatted_show_table_status_values[] = {
        "io_by_thread_by_latency",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        expected_datetime_value,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "VIEW",
    };
    static const char *const raw_show_table_status_values[] = {
        "x$io_by_thread_by_latency",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        expected_datetime_value,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "VIEW",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "main") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open file-backed database");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM sys.io_by_thread_by_latency",
            .column_names = sys_io_by_thread_by_latency_columns,
            .column_count = sys_io_by_thread_by_latency_column_count,
            .values = NULL,
            .row_count = 0U,
            .context = "sys io by thread by latency rows",
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
            .context = "sys io by thread by latency row_count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM sys.io_by_thread_by_latency",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "sys io by thread by latency count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM sys.`x$io_by_thread_by_latency`",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "sys x io by thread by latency count",
        }
    );
    failures += expect_statement_ok(database, "USE sys");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM io_by_thread_by_latency",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "sys io by thread by latency selected count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM `x$io_by_thread_by_latency`",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "sys x io by thread by latency selected count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM io_by_thread_by_latency",
            .column_names = show_columns_columns,
            .column_count = show_columns_column_count,
            .values = formatted_show_columns_values,
            .row_count = sys_io_by_thread_by_latency_column_count,
            .context = "sys io by thread by latency show columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM `x$io_by_thread_by_latency`",
            .column_names = show_columns_columns,
            .column_count = show_columns_column_count,
            .values = raw_show_columns_values,
            .row_count = sys_io_by_thread_by_latency_column_count,
            .context = "sys x io by thread by latency show columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "DESCRIBE io_by_thread_by_latency",
            .column_names = show_columns_columns,
            .column_count = show_columns_column_count,
            .values = formatted_show_columns_values,
            .row_count = sys_io_by_thread_by_latency_column_count,
            .context = "sys io by thread by latency describe",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW FULL COLUMNS FROM io_by_thread_by_latency "
                   "WHERE Field IN ('user', 'total_latency', 'min_latency', 'thread_id')",
            .column_names = show_full_columns_columns,
            .column_count = show_full_columns_column_count,
            .values = formatted_show_full_columns_values,
            .row_count = 4U,
            .context = "sys io by thread by latency show full columns sample",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW FULL COLUMNS FROM `x$io_by_thread_by_latency` "
                   "WHERE Field IN ('user', 'total_latency', 'min_latency', 'thread_id')",
            .column_names = show_full_columns_columns,
            .column_count = show_full_columns_column_count,
            .values = raw_show_full_columns_values,
            .row_count = 4U,
            .context = "sys x io by thread by latency show full columns sample",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM io_by_thread_by_latency",
            .column_names = show_index_columns,
            .column_count = show_index_column_count,
            .values = NULL,
            .row_count = 0U,
            .context = "sys io by thread by latency show index",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM `x$io_by_thread_by_latency`",
            .column_names = show_index_columns,
            .column_count = show_index_column_count,
            .values = NULL,
            .row_count = 0U,
            .context = "sys x io by thread by latency show index",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, "
                   "COLUMN_TYPE, CHARACTER_SET_NAME, COLLATION_NAME "
                   "FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = 'sys' "
                   "AND ((TABLE_NAME = 'io_by_thread_by_latency') OR "
                   "(TABLE_NAME = 'x$io_by_thread_by_latency'))",
            .column_names = information_schema_columns_columns,
            .column_count = information_schema_columns_column_count,
            .values = information_schema_columns_values,
            .row_count = information_schema_columns_row_count,
            .context = "sys io by thread by latency information_schema.columns rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'io_by_thread_by_latency'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_eight,
            .row_count = 1U,
            .context = "sys io by thread by latency information_schema.columns count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'x$io_by_thread_by_latency'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_eight,
            .row_count = 1U,
            .context = "sys x io by thread by latency information_schema.columns count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'sys' AND "
                   "((TABLE_NAME = 'io_by_thread_by_latency') OR "
                   "(TABLE_NAME = 'x$io_by_thread_by_latency'))",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_sixteen,
            .row_count = 1U,
            .context = "sys io by thread by latency information_schema.columns combined count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, TABLE_ROWS, "
                   "DATA_LENGTH, TABLE_COMMENT FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'sys' AND "
                   "((TABLE_NAME = 'io_by_thread_by_latency') OR "
                   "(TABLE_NAME = 'x$io_by_thread_by_latency')) "
                   "ORDER BY TABLE_NAME",
            .column_names = information_schema_tables_columns,
            .column_count = information_schema_tables_column_count,
            .values = information_schema_tables_values,
            .row_count = 2U,
            .context = "sys io by thread by latency information_schema.tables rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW TABLE STATUS FROM sys WHERE Name = 'io_by_thread_by_latency'",
            .column_names = show_table_status_columns,
            .column_count = show_table_status_column_count,
            .values = formatted_show_table_status_values,
            .row_count = 1U,
            .context = "sys io by thread by latency show table status",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW TABLE STATUS FROM sys WHERE Name = 'x$io_by_thread_by_latency'",
            .column_names = show_table_status_columns,
            .column_count = show_table_status_column_count,
            .values = raw_show_table_status_values,
            .row_count = 1U,
            .context = "sys x io by thread by latency show table status",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_SCHEMA, TABLE_NAME, CHECK_OPTION, IS_UPDATABLE, DEFINER, "
                   "SECURITY_TYPE FROM INFORMATION_SCHEMA.VIEWS WHERE TABLE_SCHEMA = 'sys' "
                   "AND ((TABLE_NAME = 'io_by_thread_by_latency') OR "
                   "(TABLE_NAME = 'x$io_by_thread_by_latency')) ORDER BY TABLE_NAME",
            .column_names = information_schema_views_columns,
            .column_count = information_schema_views_column_count,
            .values = information_schema_views_values,
            .row_count = 2U,
            .context = "sys io by thread by latency information_schema.views rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT VIEW_SCHEMA, VIEW_NAME, TABLE_SCHEMA, TABLE_NAME "
                   "FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE WHERE VIEW_SCHEMA = 'sys' "
                   "AND ((VIEW_NAME = 'io_by_thread_by_latency') OR "
                   "(VIEW_NAME = 'x$io_by_thread_by_latency'))",
            .column_names = information_schema_view_table_usage_columns,
            .column_count = information_schema_view_table_usage_column_count,
            .values = view_table_usage_values,
            .row_count = 4U,
            .context = "sys io by thread by latency view_table_usage rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_SCHEMA, TABLE_NAME, SPECIFIC_SCHEMA, SPECIFIC_NAME "
                   "FROM INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE WHERE TABLE_SCHEMA = 'sys' "
                   "AND ((TABLE_NAME = 'io_by_thread_by_latency') OR "
                   "(TABLE_NAME = 'x$io_by_thread_by_latency'))",
            .column_names = information_schema_view_routine_usage_columns,
            .column_count = information_schema_view_routine_usage_column_count,
            .values = NULL,
            .row_count = 0U,
            .context = "sys io by thread by latency empty view_routine_usage rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'sys' AND "
                   "((TABLE_NAME = 'io_by_thread_by_latency') OR "
                   "(TABLE_NAME = 'x$io_by_thread_by_latency'))",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "sys io by thread by latency empty statistics",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "
                   "WHERE TABLE_SCHEMA = 'sys' AND "
                   "((TABLE_NAME = 'io_by_thread_by_latency') OR "
                   "(TABLE_NAME = 'x$io_by_thread_by_latency'))",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "sys io by thread by latency empty constraints",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE "
                   "WHERE TABLE_SCHEMA = 'sys' AND "
                   "((TABLE_NAME = 'io_by_thread_by_latency') OR "
                   "(TABLE_NAME = 'x$io_by_thread_by_latency'))",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "sys io by thread by latency empty key column usage",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS "
                   "WHERE CONSTRAINT_SCHEMA = 'sys' AND "
                   "((TABLE_NAME = 'io_by_thread_by_latency') OR "
                   "(TABLE_NAME = 'x$io_by_thread_by_latency'))",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "sys io by thread by latency empty constraint extensions",
        }
    );
    failures += expect_query_contains(
        database,
        (struct expected_query_contains){
            .sql = "SHOW CREATE VIEW io_by_thread_by_latency",
            .column_names = show_create_view_columns,
            .column_count = show_create_view_column_count,
            .row_count = 1U,
            .row_index = 0U,
            .column_index = 1U,
            .needle = "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY "
                      "INVOKER VIEW `io_by_thread_by_latency`",
            .context = "sys io by thread by latency show create view",
        }
    );
    failures += expect_query_contains(
        database,
        (struct expected_query_contains){
            .sql = "SHOW CREATE VIEW `x$io_by_thread_by_latency`",
            .column_names = show_create_view_columns,
            .column_count = show_create_view_column_count,
            .row_count = 1U,
            .row_index = 0U,
            .column_index = 1U,
            .needle = "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY "
                      "INVOKER VIEW `x$io_by_thread_by_latency`",
            .context = "sys x io by thread by latency show create view",
        }
    );
    failures += expect_query_contains(
        database,
        (struct expected_query_contains){
            .sql = "SHOW CREATE TABLE sys.io_by_thread_by_latency",
            .column_names = show_create_view_columns,
            .column_count = show_create_view_column_count,
            .row_count = 1U,
            .row_index = 0U,
            .column_index = 1U,
            .needle = "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY "
                      "INVOKER VIEW `sys`.`io_by_thread_by_latency`",
            .context = "sys io by thread by latency qualified show create table",
        }
    );
    failures += expect_query_contains(
        database,
        (struct expected_query_contains){
            .sql = "SHOW CREATE TABLE `x$io_by_thread_by_latency`",
            .column_names = show_create_view_columns,
            .column_count = show_create_view_column_count,
            .row_count = 1U,
            .row_index = 0U,
            .column_index = 1U,
            .needle = "from (`performance_schema`.`events_waits_summary_by_thread_by_event_name` "
                      "left join `performance_schema`.`threads`",
            .context = "sys x io by thread by latency show create source",
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

                if (expected_value == expected_datetime_value) {
                    failures += expect_datetime_text(actual_value, expected.context);
                } else {
                    failures += expect_text_or_null(actual_value, expected_value, expected.context);
                }
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
        "/tmp/mylite_runtime_sys_io_by_thread_by_latency_views_%d_%s.mylite",
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

static int expect_datetime_text(const char *actual, const char *context) {
    if (actual == NULL || strlen(actual) != datetime_text_length ||
        actual[datetime_year_month_separator] != '-' ||
        actual[datetime_month_day_separator] != '-' ||
        actual[datetime_date_time_separator] != ' ' ||
        actual[datetime_hour_minute_separator] != ':' ||
        actual[datetime_minute_second_separator] != ':') {
        fprintf(stderr, "%s: expected datetime text, got [%s]\n", context, actual);
        return 1;
    }
    return 0;
}
