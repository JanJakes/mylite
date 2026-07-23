#include "mylite_test_support.h"

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
    memory_global_by_current_bytes_column_count = 7,
    show_columns_column_count = 6,
    show_full_columns_column_count = 9,
    show_index_column_count = 15,
    information_schema_columns_column_count = 7,
    information_schema_columns_row_count = 14,
    information_schema_tables_column_count = 7,
    information_schema_views_column_count = 6,
    information_schema_view_table_usage_column_count = 4,
    information_schema_view_routine_usage_column_count = 4,
    memory_global_by_current_bytes_view_table_usage_row_count = 2,
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

static int test_sys_memory_global_by_current_bytes_views(void);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_query_contains(mylite_db *database, struct expected_query_contains expected);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int expect_datetime_text(const char *actual, const char *context);

static const char
    *const memory_global_by_current_bytes_columns[memory_global_by_current_bytes_column_count] = {
        "event_name",
        "current_count",
        "current_alloc",
        "current_avg_alloc",
        "high_count",
        "high_alloc",
        "high_avg_alloc",
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
    return test_sys_memory_global_by_current_bytes_views() == 0 ? 0 : 1;
}

static int test_sys_memory_global_by_current_bytes_views(void) {
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_zero[] = {"0"};
    static const char *const count_seven[] = {"7"};
    static const char *const count_fourteen[] = {"14"};
    static const char *const row_count_column[] = {"ROW_COUNT()"};
    static const char *const row_count_minus_one[] = {"-1"};
    static const char *const formatted_show_columns_values[] = {
        "event_name",        "varchar(128)", "NO",  "", NULL, "",
        "current_count",     "bigint",       "NO",  "", NULL, "",
        "current_alloc",     "varchar(11)",  "YES", "", NULL, "",
        "current_avg_alloc", "varchar(11)",  "YES", "", NULL, "",
        "high_count",        "bigint",       "NO",  "", NULL, "",
        "high_alloc",        "varchar(11)",  "YES", "", NULL, "",
        "high_avg_alloc",    "varchar(11)",  "YES", "", NULL, "",
    };
    static const char *const raw_show_columns_values[] = {
        "event_name",        "varchar(128)",  "NO", "", NULL,     "",
        "current_count",     "bigint",        "NO", "", NULL,     "",
        "current_alloc",     "bigint",        "NO", "", NULL,     "",
        "current_avg_alloc", "decimal(23,4)", "NO", "", "0.0000", "",
        "high_count",        "bigint",        "NO", "", NULL,     "",
        "high_alloc",        "bigint",        "NO", "", NULL,     "",
        "high_avg_alloc",    "decimal(23,4)", "NO", "", "0.0000", "",
    };
    static const char *const formatted_show_full_columns_values[] = {
        "event_name",
        "varchar(128)",
        "utf8mb4_0900_ai_ci",
        "NO",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "current_count",
        "bigint",
        NULL,
        "NO",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "current_alloc",
        "varchar(11)",
        "utf8mb3_general_ci",
        "YES",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "current_avg_alloc",
        "varchar(11)",
        "utf8mb3_general_ci",
        "YES",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "high_count",
        "bigint",
        NULL,
        "NO",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "high_alloc",
        "varchar(11)",
        "utf8mb3_general_ci",
        "YES",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "high_avg_alloc",
        "varchar(11)",
        "utf8mb3_general_ci",
        "YES",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
    };
    static const char *const raw_show_full_columns_values[] = {
        "event_name",
        "varchar(128)",
        "utf8mb4_0900_ai_ci",
        "NO",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "current_count",
        "bigint",
        NULL,
        "NO",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "current_alloc",
        "bigint",
        NULL,
        "NO",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "current_avg_alloc",
        "decimal(23,4)",
        NULL,
        "NO",
        "",
        "0.0000",
        "",
        "select,insert,update,references",
        "",
        "high_count",
        "bigint",
        NULL,
        "NO",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "high_alloc",
        "bigint",
        NULL,
        "NO",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "high_avg_alloc",
        "decimal(23,4)",
        NULL,
        "NO",
        "",
        "0.0000",
        "",
        "select,insert,update,references",
        "",
    };
    static const char *const information_schema_columns_values[] = {
        "memory_global_by_current_bytes",
        "event_name",
        "1",
        "NO",
        "varchar(128)",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "memory_global_by_current_bytes",
        "current_count",
        "2",
        "NO",
        "bigint",
        NULL,
        NULL,
        "memory_global_by_current_bytes",
        "current_alloc",
        "3",
        "YES",
        "varchar(11)",
        "utf8mb3",
        "utf8mb3_general_ci",
        "memory_global_by_current_bytes",
        "current_avg_alloc",
        "4",
        "YES",
        "varchar(11)",
        "utf8mb3",
        "utf8mb3_general_ci",
        "memory_global_by_current_bytes",
        "high_count",
        "5",
        "NO",
        "bigint",
        NULL,
        NULL,
        "memory_global_by_current_bytes",
        "high_alloc",
        "6",
        "YES",
        "varchar(11)",
        "utf8mb3",
        "utf8mb3_general_ci",
        "memory_global_by_current_bytes",
        "high_avg_alloc",
        "7",
        "YES",
        "varchar(11)",
        "utf8mb3",
        "utf8mb3_general_ci",
        "x$memory_global_by_current_bytes",
        "event_name",
        "1",
        "NO",
        "varchar(128)",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "x$memory_global_by_current_bytes",
        "current_count",
        "2",
        "NO",
        "bigint",
        NULL,
        NULL,
        "x$memory_global_by_current_bytes",
        "current_alloc",
        "3",
        "NO",
        "bigint",
        NULL,
        NULL,
        "x$memory_global_by_current_bytes",
        "current_avg_alloc",
        "4",
        "NO",
        "decimal(23,4)",
        NULL,
        NULL,
        "x$memory_global_by_current_bytes",
        "high_count",
        "5",
        "NO",
        "bigint",
        NULL,
        NULL,
        "x$memory_global_by_current_bytes",
        "high_alloc",
        "6",
        "NO",
        "bigint",
        NULL,
        NULL,
        "x$memory_global_by_current_bytes",
        "high_avg_alloc",
        "7",
        "NO",
        "decimal(23,4)",
        NULL,
        NULL,
    };
    static const char *const information_schema_tables_values[] = {
        "sys",
        "memory_global_by_current_bytes",
        "VIEW",
        NULL,
        NULL,
        NULL,
        "VIEW",
        "sys",
        "x$memory_global_by_current_bytes",
        "VIEW",
        NULL,
        NULL,
        NULL,
        "VIEW",
    };
    static const char *const information_schema_views_values[] = {
        "sys",
        "memory_global_by_current_bytes",
        "NONE",
        "YES",
        "mysql.sys@localhost",
        "INVOKER",
        "sys",
        "x$memory_global_by_current_bytes",
        "NONE",
        "YES",
        "mysql.sys@localhost",
        "INVOKER",
    };
    static const char *const view_table_usage_values[] = {
        "sys",
        "memory_global_by_current_bytes",
        "performance_schema",
        "memory_summary_global_by_event_name",
        "sys",
        "x$memory_global_by_current_bytes",
        "performance_schema",
        "memory_summary_global_by_event_name",
    };
    static const char *const formatted_show_table_status_values[] = {
        "memory_global_by_current_bytes",
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
        "x$memory_global_by_current_bytes",
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

    if (mylite_test_make_path(path, sizeof(path), "main") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "open file-backed database"
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM sys.memory_global_by_current_bytes",
            .column_names = memory_global_by_current_bytes_columns,
            .column_count = memory_global_by_current_bytes_column_count,
            .values = NULL,
            .row_count = 0U,
            .context = "sys memory global current bytes rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM sys.`x$memory_global_by_current_bytes`",
            .column_names = memory_global_by_current_bytes_columns,
            .column_count = memory_global_by_current_bytes_column_count,
            .values = NULL,
            .row_count = 0U,
            .context = "sys x memory global current bytes rows",
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
            .context = "sys memory global current bytes row_count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM sys.memory_global_by_current_bytes",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "sys memory global current bytes count",
        }
    );
    failures += expect_statement_ok(database, "USE sys");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM memory_global_by_current_bytes",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "sys memory global current bytes selected count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM `x$memory_global_by_current_bytes`",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "sys x memory global current bytes selected count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM memory_global_by_current_bytes",
            .column_names = show_columns_columns,
            .column_count = show_columns_column_count,
            .values = formatted_show_columns_values,
            .row_count = memory_global_by_current_bytes_column_count,
            .context = "sys memory global current bytes show columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "DESCRIBE `x$memory_global_by_current_bytes`",
            .column_names = show_columns_columns,
            .column_count = show_columns_column_count,
            .values = raw_show_columns_values,
            .row_count = memory_global_by_current_bytes_column_count,
            .context = "sys x memory global current bytes describe",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW FULL COLUMNS FROM memory_global_by_current_bytes",
            .column_names = show_full_columns_columns,
            .column_count = show_full_columns_column_count,
            .values = formatted_show_full_columns_values,
            .row_count = memory_global_by_current_bytes_column_count,
            .context = "sys memory global current bytes show full columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW FULL COLUMNS FROM `x$memory_global_by_current_bytes`",
            .column_names = show_full_columns_columns,
            .column_count = show_full_columns_column_count,
            .values = raw_show_full_columns_values,
            .row_count = memory_global_by_current_bytes_column_count,
            .context = "sys x memory global current bytes show full columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM memory_global_by_current_bytes",
            .column_names = show_index_columns,
            .column_count = show_index_column_count,
            .values = NULL,
            .row_count = 0U,
            .context = "sys memory global current bytes show index",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM `x$memory_global_by_current_bytes`",
            .column_names = show_index_columns,
            .column_count = show_index_column_count,
            .values = NULL,
            .row_count = 0U,
            .context = "sys x memory global current bytes show index",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, "
                   "COLUMN_TYPE, CHARACTER_SET_NAME, COLLATION_NAME "
                   "FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = 'sys' "
                   "AND TABLE_NAME IN ('memory_global_by_current_bytes', "
                   "'x$memory_global_by_current_bytes') "
                   "ORDER BY TABLE_NAME",
            .column_names = information_schema_columns_columns,
            .column_count = information_schema_columns_column_count,
            .values = information_schema_columns_values,
            .row_count = information_schema_columns_row_count,
            .context = "sys memory global current bytes information_schema.columns rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = "
                   "'memory_global_by_current_bytes'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_seven,
            .row_count = 1U,
            .context = "sys memory global current bytes information_schema.columns count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME IN "
                   "('memory_global_by_current_bytes', 'x$memory_global_by_current_bytes')",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_fourteen,
            .row_count = 1U,
            .context = "sys memory global current bytes information_schema.columns combined count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, TABLE_ROWS, "
                   "DATA_LENGTH, TABLE_COMMENT FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME IN "
                   "('memory_global_by_current_bytes', 'x$memory_global_by_current_bytes') "
                   "ORDER BY TABLE_NAME",
            .column_names = information_schema_tables_columns,
            .column_count = information_schema_tables_column_count,
            .values = information_schema_tables_values,
            .row_count = 2U,
            .context = "sys memory global current bytes information_schema.tables rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW TABLE STATUS FROM sys WHERE Name = 'memory_global_by_current_bytes'",
            .column_names = show_table_status_columns,
            .column_count = show_table_status_column_count,
            .values = formatted_show_table_status_values,
            .row_count = 1U,
            .context = "sys memory global current bytes show table status",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW TABLE STATUS FROM sys WHERE Name = 'x$memory_global_by_current_bytes'",
            .column_names = show_table_status_columns,
            .column_count = show_table_status_column_count,
            .values = raw_show_table_status_values,
            .row_count = 1U,
            .context = "sys x memory global current bytes show table status",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_SCHEMA, TABLE_NAME, CHECK_OPTION, IS_UPDATABLE, DEFINER, "
                   "SECURITY_TYPE FROM INFORMATION_SCHEMA.VIEWS WHERE TABLE_SCHEMA = 'sys' "
                   "AND TABLE_NAME IN ('memory_global_by_current_bytes', "
                   "'x$memory_global_by_current_bytes') ORDER BY TABLE_NAME",
            .column_names = information_schema_views_columns,
            .column_count = information_schema_views_column_count,
            .values = information_schema_views_values,
            .row_count = 2U,
            .context = "sys memory global current bytes information_schema.views rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT VIEW_SCHEMA, VIEW_NAME, TABLE_SCHEMA, TABLE_NAME "
                   "FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE WHERE VIEW_SCHEMA = 'sys' "
                   "AND VIEW_NAME IN ('memory_global_by_current_bytes', "
                   "'x$memory_global_by_current_bytes') "
                   "ORDER BY VIEW_NAME",
            .column_names = information_schema_view_table_usage_columns,
            .column_count = information_schema_view_table_usage_column_count,
            .values = view_table_usage_values,
            .row_count = memory_global_by_current_bytes_view_table_usage_row_count,
            .context = "sys memory global current bytes view_table_usage rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_SCHEMA, TABLE_NAME, SPECIFIC_SCHEMA, SPECIFIC_NAME "
                   "FROM INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE WHERE TABLE_SCHEMA = 'sys' "
                   "AND TABLE_NAME IN ('memory_global_by_current_bytes', "
                   "'x$memory_global_by_current_bytes')",
            .column_names = information_schema_view_routine_usage_columns,
            .column_count = information_schema_view_routine_usage_column_count,
            .values = NULL,
            .row_count = 0U,
            .context = "sys memory global current bytes empty view_routine_usage rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME IN "
                   "('memory_global_by_current_bytes', 'x$memory_global_by_current_bytes')",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "sys memory global current bytes empty statistics",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "
                   "WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME IN "
                   "('memory_global_by_current_bytes', 'x$memory_global_by_current_bytes')",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "sys memory global current bytes empty constraints",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE "
                   "WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME IN "
                   "('memory_global_by_current_bytes', 'x$memory_global_by_current_bytes')",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "sys memory global current bytes empty key column usage",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS "
                   "WHERE CONSTRAINT_SCHEMA = 'sys' AND TABLE_NAME IN "
                   "('memory_global_by_current_bytes', 'x$memory_global_by_current_bytes')",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "sys memory global current bytes empty constraint extensions",
        }
    );
    failures += expect_query_contains(
        database,
        (struct expected_query_contains){
            .sql = "SHOW CREATE VIEW memory_global_by_current_bytes",
            .column_names = show_create_view_columns,
            .column_count = show_create_view_column_count,
            .row_count = 1U,
            .row_index = 0U,
            .column_index = 1U,
            .needle = "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL "
                      "SECURITY INVOKER VIEW `memory_global_by_current_bytes`",
            .context = "sys memory global current bytes show create view",
        }
    );
    failures += expect_query_contains(
        database,
        (struct expected_query_contains){
            .sql = "SHOW CREATE VIEW `x$memory_global_by_current_bytes`",
            .column_names = show_create_view_columns,
            .column_count = show_create_view_column_count,
            .row_count = 1U,
            .row_index = 0U,
            .column_index = 1U,
            .needle = "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL "
                      "SECURITY INVOKER VIEW `x$memory_global_by_current_bytes`",
            .context = "sys x memory global current bytes show create view",
        }
    );
    failures += expect_query_contains(
        database,
        (struct expected_query_contains){
            .sql = "SHOW CREATE TABLE sys.memory_global_by_current_bytes",
            .column_names = show_create_view_columns,
            .column_count = show_create_view_column_count,
            .row_count = 1U,
            .row_index = 0U,
            .column_index = 1U,
            .needle = "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL "
                      "SECURITY INVOKER VIEW `sys`.`memory_global_by_current_bytes`",
            .context = "sys memory global current bytes qualified show create table",
        }
    );
    failures += expect_query_contains(
        database,
        (struct expected_query_contains){
            .sql = "SHOW CREATE TABLE `x$memory_global_by_current_bytes`",
            .column_names = show_create_view_columns,
            .column_count = show_create_view_column_count,
            .row_count = 1U,
            .row_index = 0U,
            .column_index = 1U,
            .needle = "from `performance_schema`.`memory_summary_global_by_event_name` "
                      "where",
            .context = "sys x memory global current bytes show create source",
        }
    );
    failures += expect_query_contains(
        database,
        (struct expected_query_contains){
            .sql = "SHOW CREATE VIEW sys.memory_global_by_current_bytes",
            .column_names = show_create_view_columns,
            .column_count = show_create_view_column_count,
            .row_count = 1U,
            .row_index = 0U,
            .column_index = 1U,
            .needle = "where (`performance_schema`.`memory_summary_global_by_event_name`."
                      "`CURRENT_NUMBER_OF_BYTES_USED` > 0) order by `performance_schema`."
                      "`memory_summary_global_by_event_name`.`CURRENT_NUMBER_OF_BYTES_USED` desc",
            .context = "sys memory global current bytes show create ordering",
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

    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        expected.column_count,
        expected.context
    );
    failures += mylite_test_expect_size(
        mylite_result_row_count(result),
        expected.row_count,
        expected.context
    );

    for (size_t column_index = 0U; column_index < expected.column_count; ++column_index) {
        failures += mylite_test_expect_text_or_null(
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

    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        expected.column_count,
        expected.context
    );
    failures += mylite_test_expect_size(
        mylite_result_row_count(result),
        expected.row_count,
        expected.context
    );

    for (size_t column_index = 0U; column_index < expected.column_count; ++column_index) {
        failures += mylite_test_expect_text_or_null(
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
