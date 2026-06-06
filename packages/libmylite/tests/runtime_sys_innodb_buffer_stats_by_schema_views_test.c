#include <mylite/mylite.h>

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#  include <process.h>
#else
#  include <unistd.h>
#endif

enum {
    test_path_capacity = 1024,
    sys_innodb_buffer_stats_by_schema_column_count = 7,
    show_columns_column_count = 6,
    show_full_columns_column_count = 9,
    show_index_column_count = 15,
    information_schema_columns_column_count = 7,
    information_schema_columns_row_count = 14,
    information_schema_tables_column_count = 7,
    information_schema_views_column_count = 6,
    information_schema_view_table_usage_column_count = 4,
    information_schema_view_routine_usage_column_count = 4,
    innodb_buffer_stats_by_schema_view_table_usage_row_count = 2,
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

static int test_sys_innodb_buffer_stats_by_schema_views(void);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_query_contains(mylite_db *database, struct expected_query_contains expected);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int expect_int(int actual, int expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text_or_null(const char *actual, const char *expected, const char *context);
static int expect_datetime_text(const char *actual, const char *context);

static const char *const
    sys_innodb_buffer_stats_by_schema_columns[sys_innodb_buffer_stats_by_schema_column_count] = {
        "object_schema",
        "allocated",
        "data",
        "pages",
        "pages_hashed",
        "pages_old",
        "rows_cached",
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

static const char
    *const information_schema_columns_columns[information_schema_columns_column_count] = {
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
    return test_sys_innodb_buffer_stats_by_schema_views() == 0 ? 0 : 1;
}

static int test_sys_innodb_buffer_stats_by_schema_views(void) {
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_zero[] = {"0"};
    static const char *const count_seven[] = {"7"};
    static const char *const count_fourteen[] = {"14"};
    static const char *const row_count_column[] = {"ROW_COUNT()"};
    static const char *const row_count_minus_one[] = {"-1"};
    static const char *const formatted_show_columns_values[] = {
        "object_schema", "text",          "YES", "", NULL, "",
        "allocated",     "varchar(11)",   "YES", "", NULL, "",
        "data",          "varchar(11)",   "YES", "", NULL, "",
        "pages",         "bigint",        "NO",  "", "0",  "",
        "pages_hashed",  "bigint",        "NO",  "", "0",  "",
        "pages_old",     "bigint",        "NO",  "", "0",  "",
        "rows_cached",   "decimal(45,0)", "YES", "", NULL, "",
    };
    static const char *const raw_show_columns_values[] = {
        "object_schema", "text",          "YES", "", NULL, "",
        "allocated",     "decimal(44,0)", "YES", "", NULL, "",
        "data",          "decimal(44,0)", "YES", "", NULL, "",
        "pages",         "bigint",        "NO",  "", "0",  "",
        "pages_hashed",  "bigint",        "NO",  "", "0",  "",
        "pages_old",     "bigint",        "NO",  "", "0",  "",
        "rows_cached",   "decimal(45,0)", "NO",  "", "0",  "",
    };
    static const char *const formatted_show_full_columns_values[] = {
        "object_schema",
        "text",
        "utf8mb3_general_ci",
        "YES",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "allocated",
        "varchar(11)",
        "utf8mb3_general_ci",
        "YES",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "data",
        "varchar(11)",
        "utf8mb3_general_ci",
        "YES",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "pages",
        "bigint",
        NULL,
        "NO",
        "",
        "0",
        "",
        "select,insert,update,references",
        "",
        "pages_hashed",
        "bigint",
        NULL,
        "NO",
        "",
        "0",
        "",
        "select,insert,update,references",
        "",
        "pages_old",
        "bigint",
        NULL,
        "NO",
        "",
        "0",
        "",
        "select,insert,update,references",
        "",
        "rows_cached",
        "decimal(45,0)",
        NULL,
        "YES",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
    };
    static const char *const raw_show_full_columns_values[] = {
        "object_schema",
        "text",
        "utf8mb3_general_ci",
        "YES",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "allocated",
        "decimal(44,0)",
        NULL,
        "YES",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "data",
        "decimal(44,0)",
        NULL,
        "YES",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "pages",
        "bigint",
        NULL,
        "NO",
        "",
        "0",
        "",
        "select,insert,update,references",
        "",
        "pages_hashed",
        "bigint",
        NULL,
        "NO",
        "",
        "0",
        "",
        "select,insert,update,references",
        "",
        "pages_old",
        "bigint",
        NULL,
        "NO",
        "",
        "0",
        "",
        "select,insert,update,references",
        "",
        "rows_cached",
        "decimal(45,0)",
        NULL,
        "NO",
        "",
        "0",
        "",
        "select,insert,update,references",
        "",
    };
    static const char *const information_schema_columns_values[] = {
        "innodb_buffer_stats_by_schema",
        "object_schema",
        "1",
        "YES",
        "text",
        "utf8mb3",
        "utf8mb3_general_ci",
        "innodb_buffer_stats_by_schema",
        "allocated",
        "2",
        "YES",
        "varchar(11)",
        "utf8mb3",
        "utf8mb3_general_ci",
        "innodb_buffer_stats_by_schema",
        "data",
        "3",
        "YES",
        "varchar(11)",
        "utf8mb3",
        "utf8mb3_general_ci",
        "innodb_buffer_stats_by_schema",
        "pages",
        "4",
        "NO",
        "bigint",
        NULL,
        NULL,
        "innodb_buffer_stats_by_schema",
        "pages_hashed",
        "5",
        "NO",
        "bigint",
        NULL,
        NULL,
        "innodb_buffer_stats_by_schema",
        "pages_old",
        "6",
        "NO",
        "bigint",
        NULL,
        NULL,
        "innodb_buffer_stats_by_schema",
        "rows_cached",
        "7",
        "YES",
        "decimal(45,0)",
        NULL,
        NULL,
        "x$innodb_buffer_stats_by_schema",
        "object_schema",
        "1",
        "YES",
        "text",
        "utf8mb3",
        "utf8mb3_general_ci",
        "x$innodb_buffer_stats_by_schema",
        "allocated",
        "2",
        "YES",
        "decimal(44,0)",
        NULL,
        NULL,
        "x$innodb_buffer_stats_by_schema",
        "data",
        "3",
        "YES",
        "decimal(44,0)",
        NULL,
        NULL,
        "x$innodb_buffer_stats_by_schema",
        "pages",
        "4",
        "NO",
        "bigint",
        NULL,
        NULL,
        "x$innodb_buffer_stats_by_schema",
        "pages_hashed",
        "5",
        "NO",
        "bigint",
        NULL,
        NULL,
        "x$innodb_buffer_stats_by_schema",
        "pages_old",
        "6",
        "NO",
        "bigint",
        NULL,
        NULL,
        "x$innodb_buffer_stats_by_schema",
        "rows_cached",
        "7",
        "NO",
        "decimal(45,0)",
        NULL,
        NULL,
    };
    static const char *const information_schema_tables_values[] = {
        "sys",
        "innodb_buffer_stats_by_schema",
        "VIEW",
        NULL,
        NULL,
        NULL,
        "VIEW",
        "sys",
        "x$innodb_buffer_stats_by_schema",
        "VIEW",
        NULL,
        NULL,
        NULL,
        "VIEW",
    };
    static const char *const information_schema_views_values[] = {
        "sys",
        "innodb_buffer_stats_by_schema",
        "NONE",
        "NO",
        "mysql.sys@localhost",
        "INVOKER",
        "sys",
        "x$innodb_buffer_stats_by_schema",
        "NONE",
        "NO",
        "mysql.sys@localhost",
        "INVOKER",
    };
    static const char *const view_table_usage_values[] = {
        "sys",
        "innodb_buffer_stats_by_schema",
        "information_schema",
        "INNODB_BUFFER_PAGE",
        "sys",
        "x$innodb_buffer_stats_by_schema",
        "information_schema",
        "INNODB_BUFFER_PAGE",
    };
    static const char *const formatted_show_table_status_values[] = {
        "innodb_buffer_stats_by_schema",
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
        "x$innodb_buffer_stats_by_schema",
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
            .sql = "SELECT * FROM sys.innodb_buffer_stats_by_schema",
            .column_names = sys_innodb_buffer_stats_by_schema_columns,
            .column_count = sys_innodb_buffer_stats_by_schema_column_count,
            .values = NULL,
            .row_count = 0U,
            .context = "sys innodb buffer stats by schema rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM sys.`x$innodb_buffer_stats_by_schema`",
            .column_names = sys_innodb_buffer_stats_by_schema_columns,
            .column_count = sys_innodb_buffer_stats_by_schema_column_count,
            .values = NULL,
            .row_count = 0U,
            .context = "sys x innodb buffer stats by schema rows",
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
            .context = "sys innodb buffer stats by schema row_count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM sys.innodb_buffer_stats_by_schema",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "sys innodb buffer stats by schema count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM sys.`x$innodb_buffer_stats_by_schema`",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "sys x innodb buffer stats by schema count",
        }
    );
    failures += expect_statement_ok(database, "USE sys");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM innodb_buffer_stats_by_schema",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "sys innodb buffer stats by schema selected count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM `x$innodb_buffer_stats_by_schema`",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "sys x innodb buffer stats by schema selected count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM innodb_buffer_stats_by_schema",
            .column_names = show_columns_columns,
            .column_count = show_columns_column_count,
            .values = formatted_show_columns_values,
            .row_count = sys_innodb_buffer_stats_by_schema_column_count,
            .context = "sys innodb buffer stats by schema show columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM `x$innodb_buffer_stats_by_schema`",
            .column_names = show_columns_columns,
            .column_count = show_columns_column_count,
            .values = raw_show_columns_values,
            .row_count = sys_innodb_buffer_stats_by_schema_column_count,
            .context = "sys x innodb buffer stats by schema show columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "DESCRIBE innodb_buffer_stats_by_schema",
            .column_names = show_columns_columns,
            .column_count = show_columns_column_count,
            .values = formatted_show_columns_values,
            .row_count = sys_innodb_buffer_stats_by_schema_column_count,
            .context = "sys innodb buffer stats by schema describe",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW FULL COLUMNS FROM innodb_buffer_stats_by_schema",
            .column_names = show_full_columns_columns,
            .column_count = show_full_columns_column_count,
            .values = formatted_show_full_columns_values,
            .row_count = sys_innodb_buffer_stats_by_schema_column_count,
            .context = "sys innodb buffer stats by schema show full columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW FULL COLUMNS FROM `x$innodb_buffer_stats_by_schema`",
            .column_names = show_full_columns_columns,
            .column_count = show_full_columns_column_count,
            .values = raw_show_full_columns_values,
            .row_count = sys_innodb_buffer_stats_by_schema_column_count,
            .context = "sys x innodb buffer stats by schema show full columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM innodb_buffer_stats_by_schema",
            .column_names = show_index_columns,
            .column_count = show_index_column_count,
            .values = NULL,
            .row_count = 0U,
            .context = "sys innodb buffer stats by schema show index",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM `x$innodb_buffer_stats_by_schema`",
            .column_names = show_index_columns,
            .column_count = show_index_column_count,
            .values = NULL,
            .row_count = 0U,
            .context = "sys x innodb buffer stats by schema show index",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, "
                   "COLUMN_TYPE, CHARACTER_SET_NAME, COLLATION_NAME "
                   "FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = 'sys' "
                   "AND ((TABLE_NAME = 'innodb_buffer_stats_by_schema') OR "
                   "(TABLE_NAME = 'x$innodb_buffer_stats_by_schema'))",
            .column_names = information_schema_columns_columns,
            .column_count = information_schema_columns_column_count,
            .values = information_schema_columns_values,
            .row_count = information_schema_columns_row_count,
            .context = "sys innodb buffer stats by schema information_schema.columns rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'innodb_buffer_stats_by_schema'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_seven,
            .row_count = 1U,
            .context = "sys innodb buffer stats by schema information_schema.columns count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'x$innodb_buffer_stats_by_schema'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_seven,
            .row_count = 1U,
            .context = "sys x innodb buffer stats by schema information_schema.columns count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'sys' AND ((TABLE_NAME = "
                   "'innodb_buffer_stats_by_schema') OR (TABLE_NAME = "
                   "'x$innodb_buffer_stats_by_schema'))",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_fourteen,
            .row_count = 1U,
            .context =
                "sys innodb buffer stats by schema information_schema.columns combined count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, TABLE_ROWS, "
                   "DATA_LENGTH, TABLE_COMMENT FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'sys' AND ((TABLE_NAME = "
                   "'innodb_buffer_stats_by_schema') "
                   "OR (TABLE_NAME = 'x$innodb_buffer_stats_by_schema')) ORDER BY TABLE_NAME",
            .column_names = information_schema_tables_columns,
            .column_count = information_schema_tables_column_count,
            .values = information_schema_tables_values,
            .row_count = 2U,
            .context = "sys innodb buffer stats by schema information_schema.tables rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW TABLE STATUS FROM sys WHERE Name = 'innodb_buffer_stats_by_schema'",
            .column_names = show_table_status_columns,
            .column_count = show_table_status_column_count,
            .values = formatted_show_table_status_values,
            .row_count = 1U,
            .context = "sys innodb buffer stats by schema show table status",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW TABLE STATUS FROM sys WHERE Name = 'x$innodb_buffer_stats_by_schema'",
            .column_names = show_table_status_columns,
            .column_count = show_table_status_column_count,
            .values = raw_show_table_status_values,
            .row_count = 1U,
            .context = "sys x innodb buffer stats by schema show table status",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_SCHEMA, TABLE_NAME, CHECK_OPTION, IS_UPDATABLE, DEFINER, "
                   "SECURITY_TYPE FROM INFORMATION_SCHEMA.VIEWS WHERE TABLE_SCHEMA = 'sys' "
                   "AND ((TABLE_NAME = 'innodb_buffer_stats_by_schema') OR "
                   "(TABLE_NAME = 'x$innodb_buffer_stats_by_schema')) ORDER BY TABLE_NAME",
            .column_names = information_schema_views_columns,
            .column_count = information_schema_views_column_count,
            .values = information_schema_views_values,
            .row_count = 2U,
            .context = "sys innodb buffer stats by schema information_schema.views rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT VIEW_SCHEMA, VIEW_NAME, TABLE_SCHEMA, TABLE_NAME "
                   "FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE WHERE VIEW_SCHEMA = 'sys' "
                   "AND ((VIEW_NAME = 'innodb_buffer_stats_by_schema') OR "
                   "(VIEW_NAME = 'x$innodb_buffer_stats_by_schema'))",
            .column_names = information_schema_view_table_usage_columns,
            .column_count = information_schema_view_table_usage_column_count,
            .values = view_table_usage_values,
            .row_count = innodb_buffer_stats_by_schema_view_table_usage_row_count,
            .context = "sys innodb buffer stats by schema view_table_usage rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_SCHEMA, TABLE_NAME, SPECIFIC_SCHEMA, SPECIFIC_NAME "
                   "FROM INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE WHERE TABLE_SCHEMA = 'sys' "
                   "AND ((TABLE_NAME = 'innodb_buffer_stats_by_schema') OR "
                   "(TABLE_NAME = 'x$innodb_buffer_stats_by_schema'))",
            .column_names = information_schema_view_routine_usage_columns,
            .column_count = information_schema_view_routine_usage_column_count,
            .values = NULL,
            .row_count = 0U,
            .context = "sys innodb buffer stats by schema empty view_routine_usage rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'sys' AND ((TABLE_NAME = "
                   "'innodb_buffer_stats_by_schema') "
                   "OR (TABLE_NAME = 'x$innodb_buffer_stats_by_schema'))",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "sys innodb buffer stats by schema empty statistics",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "
                   "WHERE TABLE_SCHEMA = 'sys' AND ((TABLE_NAME = "
                   "'innodb_buffer_stats_by_schema') OR (TABLE_NAME = "
                   "'x$innodb_buffer_stats_by_schema'))",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "sys innodb buffer stats by schema empty constraints",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE "
                   "WHERE TABLE_SCHEMA = 'sys' AND ((TABLE_NAME = "
                   "'innodb_buffer_stats_by_schema') OR (TABLE_NAME = "
                   "'x$innodb_buffer_stats_by_schema'))",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "sys innodb buffer stats by schema empty key column usage",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS "
                   "WHERE CONSTRAINT_SCHEMA = 'sys' AND ((TABLE_NAME = "
                   "'innodb_buffer_stats_by_schema') OR (TABLE_NAME = "
                   "'x$innodb_buffer_stats_by_schema'))",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "sys innodb buffer stats by schema empty constraint extensions",
        }
    );
    failures += expect_query_contains(
        database,
        (struct expected_query_contains){
            .sql = "SHOW CREATE VIEW innodb_buffer_stats_by_schema",
            .column_names = show_create_view_columns,
            .column_count = show_create_view_column_count,
            .row_count = 1U,
            .row_index = 0U,
            .column_index = 1U,
            .needle = "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL "
                      "SECURITY INVOKER VIEW `innodb_buffer_stats_by_schema`",
            .context = "sys innodb buffer stats by schema show create view",
        }
    );
    failures += expect_query_contains(
        database,
        (struct expected_query_contains){
            .sql = "SHOW CREATE VIEW `x$innodb_buffer_stats_by_schema`",
            .column_names = show_create_view_columns,
            .column_count = show_create_view_column_count,
            .row_count = 1U,
            .row_index = 0U,
            .column_index = 1U,
            .needle = "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL "
                      "SECURITY INVOKER VIEW `x$innodb_buffer_stats_by_schema`",
            .context = "sys x innodb buffer stats by schema show create view",
        }
    );
    failures += expect_query_contains(
        database,
        (struct expected_query_contains){
            .sql = "SHOW CREATE TABLE sys.innodb_buffer_stats_by_schema",
            .column_names = show_create_view_columns,
            .column_count = show_create_view_column_count,
            .row_count = 1U,
            .row_index = 0U,
            .column_index = 1U,
            .needle = "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL "
                      "SECURITY INVOKER VIEW `sys`.`innodb_buffer_stats_by_schema`",
            .context = "sys innodb buffer stats by schema qualified show create table",
        }
    );
    failures += expect_query_contains(
        database,
        (struct expected_query_contains){
            .sql = "SHOW CREATE TABLE `x$innodb_buffer_stats_by_schema`",
            .column_names = show_create_view_columns,
            .column_count = show_create_view_column_count,
            .row_count = 1U,
            .row_index = 0U,
            .column_index = 1U,
            .needle = "from `information_schema`.`INNODB_BUFFER_PAGE` `ibp` where",
            .context = "sys x innodb buffer stats by schema show create source",
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
            "%s: expected query OK, got %d / %d %s %s\n",
            expected.context,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }

    failures +=
        expect_size(mylite_result_column_count(result), expected.column_count, expected.context);
    failures += expect_size(mylite_result_row_count(result), expected.row_count, expected.context);

    for (size_t column_index = 0U; column_index < expected.column_count; ++column_index) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, column_index),
            expected.column_names[column_index],
            expected.context
        );
    }

    for (size_t row_index = 0U; row_index < expected.row_count; ++row_index) {
        for (size_t column_index = 0U; column_index < expected.column_count; ++column_index) {
            const char *expected_value =
                expected.values[(row_index * expected.column_count) + column_index];
            const char *actual_value = mylite_result_value_text(result, row_index, column_index);

            if (expected_value == expected_datetime_value) {
                failures += expect_datetime_text(actual_value, expected.context);
            } else {
                failures += expect_text_or_null(actual_value, expected_value, expected.context);
            }
        }
    }

    mylite_result_free(result);
    return failures;
}

static int expect_query_contains(mylite_db *database, struct expected_query_contains expected) {
    mylite_result *result = NULL;
    const char *value = NULL;
    int failures = 0;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected query OK, got %d / %d %s %s\n",
            expected.context,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }

    failures +=
        expect_size(mylite_result_column_count(result), expected.column_count, expected.context);
    failures += expect_size(mylite_result_row_count(result), expected.row_count, expected.context);

    for (size_t column_index = 0U; column_index < expected.column_count; ++column_index) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, column_index),
            expected.column_names[column_index],
            expected.context
        );
    }

    value = mylite_result_value_text(result, expected.row_index, expected.column_index);
    if (value == NULL || strstr(value, expected.needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected column %zu to contain <%s>, got <%s>\n",
            expected.context,
            expected.column_index,
            expected.needle,
            value == NULL ? "NULL" : value
        );
        ++failures;
    }

    mylite_result_free(result);
    return failures;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_runtime_sys_innodb_buffer_stats_by_schema_views_%d_%s.mylite",
        current_process_id(),
        name
    );

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "test path truncated\n");
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
    remove(path);
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
    remove_with_suffix(path, "-journal");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char buffer[test_path_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(buffer)) {
        remove(buffer);
    }
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual == expected) {
        return 0;
    }

    fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
    return 1;
}

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }

    fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
    return 1;
}

static int expect_text_or_null(const char *actual, const char *expected, const char *context) {
    if (actual == NULL && expected == NULL) {
        return 0;
    }
    if (actual != NULL && expected != NULL && strcmp(actual, expected) == 0) {
        return 0;
    }

    fprintf(
        stderr,
        "%s: expected <%s>, got <%s>\n",
        context,
        expected == NULL ? "NULL" : expected,
        actual == NULL ? "NULL" : actual
    );
    return 1;
}

static int expect_datetime_text(const char *actual, const char *context) {
    if (actual == NULL || strlen(actual) != datetime_text_length ||
        actual[datetime_year_month_separator] != '-' ||
        actual[datetime_month_day_separator] != '-' ||
        actual[datetime_date_time_separator] != ' ' ||
        actual[datetime_hour_minute_separator] != ':' ||
        actual[datetime_minute_second_separator] != ':') {
        fprintf(
            stderr,
            "%s: expected datetime text, got <%s>\n",
            context,
            actual == NULL ? "NULL" : actual
        );
        return 1;
    }

    return 0;
}
