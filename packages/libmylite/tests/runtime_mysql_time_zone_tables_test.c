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
    show_columns_column_count = 6,
    show_full_columns_column_count = 9,
    show_index_column_count = 15,
    information_schema_columns_column_count = 19,
    information_schema_tables_column_count = 19,
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

static const char expected_datetime_value[] = "<datetime>";

static int test_mysql_time_zone_tables(void);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_row_count_status(mylite_db *database, const char *context);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text_or_null(const char *actual, const char *expected, const char *context);
static int expect_datetime_text(const char *actual, const char *context);

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

static const char *const information_schema_columns_columns[] = {
    "TABLE_NAME",
    "COLUMN_NAME",
    "ORDINAL_POSITION",
    "COLUMN_DEFAULT",
    "IS_NULLABLE",
    "DATA_TYPE",
    "CHARACTER_MAXIMUM_LENGTH",
    "CHARACTER_OCTET_LENGTH",
    "NUMERIC_PRECISION",
    "NUMERIC_SCALE",
    "DATETIME_PRECISION",
    "CHARACTER_SET_NAME",
    "COLLATION_NAME",
    "COLUMN_TYPE",
    "COLUMN_KEY",
    "EXTRA",
    "PRIVILEGES",
    "COLUMN_COMMENT",
    "GENERATION_EXPRESSION",
};

static const char *const information_schema_tables_columns[information_schema_tables_column_count] =
    {
        "TABLE_NAME",      "TABLE_TYPE",     "ENGINE",         "VERSION",         "ROW_FORMAT",
        "TABLE_ROWS",      "AVG_ROW_LENGTH", "DATA_LENGTH",    "MAX_DATA_LENGTH", "INDEX_LENGTH",
        "DATA_FREE",       "AUTO_INCREMENT", "CREATE_TIME",    "UPDATE_TIME",     "CHECK_TIME",
        "TABLE_COLLATION", "CHECKSUM",       "CREATE_OPTIONS", "TABLE_COMMENT",
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
    return test_mysql_time_zone_tables() == 0 ? 0 : 1;
}

static int test_mysql_time_zone_tables(void) {
    enum {
        time_zone_column_row_count = 14,
        time_zone_index_row_count = 7,
        time_zone_table_count = 5,
        count_query_capacity = 128,
        count_context_capacity = 160,
        table_constraints_column_count = 4,
        key_column_usage_column_count = 4,
        table_constraints_extensions_column_count = 4,
        statistics_column_count = 5,
    };

    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_zero[] = {"0"};
    static const char *const time_zone_table_names[time_zone_table_count] = {
        "time_zone",
        "time_zone_leap_second",
        "time_zone_name",
        "time_zone_transition",
        "time_zone_transition_type",
    };
    static const char *const show_columns_time_zone_values[] = {
        "Time_zone_id",
        "int unsigned",
        "NO",
        "PRI",
        NULL,
        "auto_increment",
        "Use_leap_seconds",
        "enum('Y','N')",
        "NO",
        "",
        "N",
        "",
    };
    static const char *const describe_time_zone_name_values[] = {
        "Name",
        "char(64)",
        "NO",
        "PRI",
        NULL,
        "",
        "Time_zone_id",
        "int unsigned",
        "NO",
        "",
        NULL,
        "",
    };
    static const char *const show_full_transition_type_values[] = {
        "Time_zone_id",
        "int unsigned",
        NULL,
        "NO",
        "PRI",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "Transition_type_id",
        "int unsigned",
        NULL,
        "NO",
        "PRI",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "Offset",
        "int",
        NULL,
        "NO",
        "",
        "0",
        "",
        "select,insert,update,references",
        "",
        "Is_DST",
        "tinyint unsigned",
        NULL,
        "NO",
        "",
        "0",
        "",
        "select,insert,update,references",
        "",
        "Abbreviation",
        "char(8)",
        "utf8mb3_general_ci",
        "NO",
        "",
        "",
        "",
        "select,insert,update,references",
        "",
    };
    static const char *const show_index_transition_type_values[] = {
        "time_zone_transition_type",
        "0",
        "PRIMARY",
        "1",
        "Time_zone_id",
        "A",
        "1954",
        NULL,
        NULL,
        "",
        "BTREE",
        "",
        "",
        "YES",
        NULL,
        "time_zone_transition_type",
        "0",
        "PRIMARY",
        "2",
        "Transition_type_id",
        "A",
        "10529",
        NULL,
        NULL,
        "",
        "BTREE",
        "",
        "",
        "YES",
        NULL,
    };
    static const char *const information_schema_columns_values[] = {
        "time_zone",
        "Time_zone_id",
        "1",
        NULL,
        "NO",
        "int",
        NULL,
        NULL,
        "10",
        "0",
        NULL,
        NULL,
        NULL,
        "int unsigned",
        "PRI",
        "auto_increment",
        "select,insert,update,references",
        "",
        "",
        "time_zone",
        "Use_leap_seconds",
        "2",
        "N",
        "NO",
        "enum",
        "1",
        "3",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "enum('Y','N')",
        "",
        "",
        "select,insert,update,references",
        "",
        "",
        "time_zone_leap_second",
        "Transition_time",
        "1",
        NULL,
        "NO",
        "bigint",
        NULL,
        NULL,
        "19",
        "0",
        NULL,
        NULL,
        NULL,
        "bigint",
        "PRI",
        "",
        "select,insert,update,references",
        "",
        "",
        "time_zone_leap_second",
        "Correction",
        "2",
        NULL,
        "NO",
        "int",
        NULL,
        NULL,
        "10",
        "0",
        NULL,
        NULL,
        NULL,
        "int",
        "",
        "",
        "select,insert,update,references",
        "",
        "",
        "time_zone_name",
        "Name",
        "1",
        NULL,
        "NO",
        "char",
        "64",
        "192",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "char(64)",
        "PRI",
        "",
        "select,insert,update,references",
        "",
        "",
        "time_zone_name",
        "Time_zone_id",
        "2",
        NULL,
        "NO",
        "int",
        NULL,
        NULL,
        "10",
        "0",
        NULL,
        NULL,
        NULL,
        "int unsigned",
        "",
        "",
        "select,insert,update,references",
        "",
        "",
        "time_zone_transition",
        "Time_zone_id",
        "1",
        NULL,
        "NO",
        "int",
        NULL,
        NULL,
        "10",
        "0",
        NULL,
        NULL,
        NULL,
        "int unsigned",
        "PRI",
        "",
        "select,insert,update,references",
        "",
        "",
        "time_zone_transition",
        "Transition_time",
        "2",
        NULL,
        "NO",
        "bigint",
        NULL,
        NULL,
        "19",
        "0",
        NULL,
        NULL,
        NULL,
        "bigint",
        "PRI",
        "",
        "select,insert,update,references",
        "",
        "",
        "time_zone_transition",
        "Transition_type_id",
        "3",
        NULL,
        "NO",
        "int",
        NULL,
        NULL,
        "10",
        "0",
        NULL,
        NULL,
        NULL,
        "int unsigned",
        "",
        "",
        "select,insert,update,references",
        "",
        "",
        "time_zone_transition_type",
        "Time_zone_id",
        "1",
        NULL,
        "NO",
        "int",
        NULL,
        NULL,
        "10",
        "0",
        NULL,
        NULL,
        NULL,
        "int unsigned",
        "PRI",
        "",
        "select,insert,update,references",
        "",
        "",
        "time_zone_transition_type",
        "Transition_type_id",
        "2",
        NULL,
        "NO",
        "int",
        NULL,
        NULL,
        "10",
        "0",
        NULL,
        NULL,
        NULL,
        "int unsigned",
        "PRI",
        "",
        "select,insert,update,references",
        "",
        "",
        "time_zone_transition_type",
        "Offset",
        "3",
        "0",
        "NO",
        "int",
        NULL,
        NULL,
        "10",
        "0",
        NULL,
        NULL,
        NULL,
        "int",
        "",
        "",
        "select,insert,update,references",
        "",
        "",
        "time_zone_transition_type",
        "Is_DST",
        "4",
        "0",
        "NO",
        "tinyint",
        NULL,
        NULL,
        "3",
        "0",
        NULL,
        NULL,
        NULL,
        "tinyint unsigned",
        "",
        "",
        "select,insert,update,references",
        "",
        "",
        "time_zone_transition_type",
        "Abbreviation",
        "5",
        "",
        "NO",
        "char",
        "8",
        "24",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "char(8)",
        "",
        "",
        "select,insert,update,references",
        "",
        "",
    };
    static const char *const table_constraints_columns[table_constraints_column_count] = {
        "TABLE_NAME",
        "CONSTRAINT_NAME",
        "CONSTRAINT_TYPE",
        "ENFORCED",
    };
    static const char *const table_constraints_values[] = {
        "time_zone",
        "PRIMARY",
        "PRIMARY KEY",
        "YES",
        "time_zone_leap_second",
        "PRIMARY",
        "PRIMARY KEY",
        "YES",
        "time_zone_name",
        "PRIMARY",
        "PRIMARY KEY",
        "YES",
        "time_zone_transition",
        "PRIMARY",
        "PRIMARY KEY",
        "YES",
        "time_zone_transition_type",
        "PRIMARY",
        "PRIMARY KEY",
        "YES",
    };
    static const char *const key_column_usage_columns[key_column_usage_column_count] = {
        "TABLE_NAME",
        "COLUMN_NAME",
        "ORDINAL_POSITION",
        "REFERENCED_TABLE_SCHEMA",
    };
    static const char *const key_column_usage_values[] = {
        "time_zone",
        "Time_zone_id",
        "1",
        NULL,
        "time_zone_leap_second",
        "Transition_time",
        "1",
        NULL,
        "time_zone_name",
        "Name",
        "1",
        NULL,
        "time_zone_transition",
        "Time_zone_id",
        "1",
        NULL,
        "time_zone_transition",
        "Transition_time",
        "2",
        NULL,
        "time_zone_transition_type",
        "Time_zone_id",
        "1",
        NULL,
        "time_zone_transition_type",
        "Transition_type_id",
        "2",
        NULL,
    };
    static const char *const
        table_constraints_extensions_columns[table_constraints_extensions_column_count] = {
            "TABLE_NAME",
            "CONSTRAINT_NAME",
            "ENGINE_ATTRIBUTE",
            "SECONDARY_ENGINE_ATTRIBUTE",
        };
    static const char *const table_constraints_extensions_values[] = {
        "time_zone",
        "PRIMARY",
        NULL,
        NULL,
        "time_zone_leap_second",
        "PRIMARY",
        NULL,
        NULL,
        "time_zone_name",
        "PRIMARY",
        NULL,
        NULL,
        "time_zone_transition",
        "PRIMARY",
        NULL,
        NULL,
        "time_zone_transition_type",
        "PRIMARY",
        NULL,
        NULL,
    };
    static const char *const statistics_columns[statistics_column_count] = {
        "TABLE_NAME",
        "INDEX_NAME",
        "SEQ_IN_INDEX",
        "COLUMN_NAME",
        "CARDINALITY",
    };
    static const char *const statistics_values[] = {
        "time_zone",
        "PRIMARY",
        "1",
        "Time_zone_id",
        "1457",
        "time_zone_leap_second",
        "PRIMARY",
        "1",
        "Transition_time",
        "0",
        "time_zone_name",
        "PRIMARY",
        "1",
        "Name",
        "1712",
        "time_zone_transition",
        "PRIMARY",
        "1",
        "Time_zone_id",
        "1252",
        "time_zone_transition",
        "PRIMARY",
        "2",
        "Transition_time",
        "119074",
        "time_zone_transition_type",
        "PRIMARY",
        "1",
        "Time_zone_id",
        "1954",
        "time_zone_transition_type",
        "PRIMARY",
        "2",
        "Transition_type_id",
        "10529",
    };
    static const char *const information_schema_tables_values[] = {
        "time_zone",
        "BASE TABLE",
        "InnoDB",
        "10",
        "Dynamic",
        "1457",
        "56",
        "81920",
        "0",
        "0",
        "4194304",
        "1796",
        expected_datetime_value,
        NULL,
        NULL,
        "utf8mb3_general_ci",
        NULL,
        "row_format=DYNAMIC stats_persistent=0",
        "Time zones",
        "time_zone_leap_second",
        "BASE TABLE",
        "InnoDB",
        "10",
        "Dynamic",
        "0",
        "0",
        "16384",
        "0",
        "0",
        "4194304",
        NULL,
        expected_datetime_value,
        NULL,
        NULL,
        "utf8mb3_general_ci",
        NULL,
        "row_format=DYNAMIC stats_persistent=0",
        "Leap seconds information for time zones",
        "time_zone_name",
        "BASE TABLE",
        "InnoDB",
        "10",
        "Dynamic",
        "1712",
        "153",
        "262144",
        "0",
        "0",
        "4194304",
        NULL,
        expected_datetime_value,
        NULL,
        NULL,
        "utf8mb3_general_ci",
        NULL,
        "row_format=DYNAMIC stats_persistent=0",
        "Time zone names",
        "time_zone_transition",
        "BASE TABLE",
        "InnoDB",
        "10",
        "Dynamic",
        "119074",
        "39",
        "4734976",
        "0",
        "0",
        "4194304",
        NULL,
        expected_datetime_value,
        NULL,
        NULL,
        "utf8mb3_general_ci",
        NULL,
        "row_format=DYNAMIC stats_persistent=0",
        "Time zone transitions",
        "time_zone_transition_type",
        "BASE TABLE",
        "InnoDB",
        "10",
        "Dynamic",
        "10529",
        "45",
        "475136",
        "0",
        "0",
        "4194304",
        NULL,
        expected_datetime_value,
        NULL,
        NULL,
        "utf8mb3_general_ci",
        NULL,
        "row_format=DYNAMIC stats_persistent=0",
        "Time zone transition types",
    };
    static const char *const show_table_status_values[] = {
        "time_zone",
        "InnoDB",
        "10",
        "Dynamic",
        "1457",
        "56",
        "81920",
        "0",
        "0",
        "4194304",
        "1796",
        expected_datetime_value,
        NULL,
        NULL,
        "utf8mb3_general_ci",
        NULL,
        "row_format=DYNAMIC stats_persistent=0",
        "Time zones",
        "time_zone_leap_second",
        "InnoDB",
        "10",
        "Dynamic",
        "0",
        "0",
        "16384",
        "0",
        "0",
        "4194304",
        NULL,
        expected_datetime_value,
        NULL,
        NULL,
        "utf8mb3_general_ci",
        NULL,
        "row_format=DYNAMIC stats_persistent=0",
        "Leap seconds information for time zones",
        "time_zone_name",
        "InnoDB",
        "10",
        "Dynamic",
        "1712",
        "153",
        "262144",
        "0",
        "0",
        "4194304",
        NULL,
        expected_datetime_value,
        NULL,
        NULL,
        "utf8mb3_general_ci",
        NULL,
        "row_format=DYNAMIC stats_persistent=0",
        "Time zone names",
        "time_zone_transition",
        "InnoDB",
        "10",
        "Dynamic",
        "119074",
        "39",
        "4734976",
        "0",
        "0",
        "4194304",
        NULL,
        expected_datetime_value,
        NULL,
        NULL,
        "utf8mb3_general_ci",
        NULL,
        "row_format=DYNAMIC stats_persistent=0",
        "Time zone transitions",
        "time_zone_transition_type",
        "InnoDB",
        "10",
        "Dynamic",
        "10529",
        "45",
        "475136",
        "0",
        "0",
        "4194304",
        NULL,
        expected_datetime_value,
        NULL,
        NULL,
        "utf8mb3_general_ci",
        NULL,
        "row_format=DYNAMIC stats_persistent=0",
        "Time zone transition types",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "mysql-time-zone-tables") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    for (size_t table_index = 0U; table_index < time_zone_table_count; ++table_index) {
        char sql[count_query_capacity];
        char context[count_context_capacity];

        (void)snprintf(
            sql,
            sizeof(sql),
            "SELECT COUNT(*) FROM mysql.%s",
            time_zone_table_names[table_index]
        );
        (void)snprintf(
            context,
            sizeof(context),
            "mysql.%s empty placeholder count",
            time_zone_table_names[table_index]
        );
        failures += expect_query(
            database,
            (struct expected_query){
                .sql = sql,
                .column_names = count_column,
                .column_count = sizeof(count_column) / sizeof(count_column[0]),
                .values = count_zero,
                .row_count = 1U,
                .context = context,
            }
        );
    }
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT Name FROM mysql.time_zone_name ORDER BY Name",
            .column_names = (const char *const[]){"Name"},
            .column_count = 1U,
            .values = NULL,
            .row_count = 0U,
            .context = "mysql.time_zone_name empty placeholder read",
        }
    );
    failures += expect_row_count_status(database, "row count after mysql.time_zone_name read");
    failures += expect_statement_ok(database, "USE mysql");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM time_zone_transition",
            .column_names = count_column,
            .column_count = sizeof(count_column) / sizeof(count_column[0]),
            .values = count_zero,
            .row_count = 1U,
            .context = "mysql.time_zone_transition unqualified count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM mysql.time_zone",
            .column_names = show_columns_columns,
            .column_count = show_columns_column_count,
            .values = show_columns_time_zone_values,
            .row_count = 2U,
            .context = "mysql.time_zone show columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "DESC mysql.time_zone_name",
            .column_names = show_columns_columns,
            .column_count = show_columns_column_count,
            .values = describe_time_zone_name_values,
            .row_count = 2U,
            .context = "mysql.time_zone_name desc",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW FULL COLUMNS FROM mysql.time_zone_transition_type",
            .column_names = show_full_columns_columns,
            .column_count = show_full_columns_column_count,
            .values = show_full_transition_type_values,
            .row_count = time_zone_table_count,
            .context = "mysql.time_zone_transition_type show full columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM mysql.time_zone_transition_type",
            .column_names = show_index_columns,
            .column_count = show_index_column_count,
            .values = show_index_transition_type_values,
            .row_count = 2U,
            .context = "mysql.time_zone_transition_type show index",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, "
                   "IS_NULLABLE, DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, "
                   "CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION, NUMERIC_SCALE, "
                   "DATETIME_PRECISION, CHARACTER_SET_NAME, COLLATION_NAME, "
                   "COLUMN_TYPE, COLUMN_KEY, EXTRA, PRIVILEGES, COLUMN_COMMENT, "
                   "GENERATION_EXPRESSION FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME LIKE 'time_zone%' "
                   "ORDER BY TABLE_NAME",
            .column_names = information_schema_columns_columns,
            .column_count = information_schema_columns_column_count,
            .values = information_schema_columns_values,
            .row_count = time_zone_column_row_count,
            .context = "mysql.time_zone information_schema.columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED "
                   "FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "
                   "WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME LIKE 'time_zone%' "
                   "ORDER BY TABLE_NAME",
            .column_names = table_constraints_columns,
            .column_count = table_constraints_column_count,
            .values = table_constraints_values,
            .row_count = time_zone_table_count,
            .context = "mysql.time_zone information_schema.table_constraints",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, "
                   "REFERENCED_TABLE_SCHEMA FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE "
                   "WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME LIKE 'time_zone%' "
                   "ORDER BY TABLE_NAME",
            .column_names = key_column_usage_columns,
            .column_count = key_column_usage_column_count,
            .values = key_column_usage_values,
            .row_count = time_zone_index_row_count,
            .context = "mysql.time_zone information_schema.key_column_usage",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, CONSTRAINT_NAME, ENGINE_ATTRIBUTE, "
                   "SECONDARY_ENGINE_ATTRIBUTE FROM "
                   "INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS "
                   "WHERE CONSTRAINT_SCHEMA = 'mysql' AND TABLE_NAME LIKE 'time_zone%' "
                   "ORDER BY TABLE_NAME",
            .column_names = table_constraints_extensions_columns,
            .column_count = table_constraints_extensions_column_count,
            .values = table_constraints_extensions_values,
            .row_count = time_zone_table_count,
            .context = "mysql.time_zone information_schema.table_constraints_extensions",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, INDEX_NAME, SEQ_IN_INDEX, COLUMN_NAME, CARDINALITY "
                   "FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME LIKE 'time_zone%' "
                   "ORDER BY TABLE_NAME",
            .column_names = statistics_columns,
            .column_count = statistics_column_count,
            .values = statistics_values,
            .row_count = time_zone_index_row_count,
            .context = "mysql.time_zone information_schema.statistics",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS, "
                   "AVG_ROW_LENGTH, DATA_LENGTH, MAX_DATA_LENGTH, INDEX_LENGTH, DATA_FREE, "
                   "AUTO_INCREMENT, CREATE_TIME, UPDATE_TIME, CHECK_TIME, TABLE_COLLATION, "
                   "CHECKSUM, CREATE_OPTIONS, TABLE_COMMENT FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME LIKE 'time_zone%' "
                   "ORDER BY TABLE_NAME",
            .column_names = information_schema_tables_columns,
            .column_count = information_schema_tables_column_count,
            .values = information_schema_tables_values,
            .row_count = time_zone_table_count,
            .context = "mysql.time_zone information_schema.tables",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW TABLE STATUS FROM mysql WHERE Name IN "
                   "('time_zone', 'time_zone_leap_second', 'time_zone_name', "
                   "'time_zone_transition', 'time_zone_transition_type')",
            .column_names = show_table_status_columns,
            .column_count = show_table_status_column_count,
            .values = show_table_status_values,
            .row_count = time_zone_table_count,
            .context = "mysql.time_zone show table status",
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

static int expect_row_count_status(mylite_db *database, const char *context) {
    static const char *const column_names[] = {"ROW_COUNT()"};
    static const char *const values[] = {"-1"};

    return expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT()",
            .column_names = column_names,
            .column_count = sizeof(column_names) / sizeof(column_names[0]),
            .values = values,
            .row_count = 1U,
            .context = context,
        }
    );
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_runtime_mysql_time_zone_tables_%d_%s.mylite",
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
    if (actual == NULL || expected == NULL) {
        if (actual != expected) {
            fprintf(
                stderr,
                "%s: expected %s, got %s\n",
                context,
                expected == NULL ? "NULL" : expected,
                actual == NULL ? "NULL" : actual
            );
            return 1;
        }
        return 0;
    }
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "%s: expected [%s], got [%s]\n", context, expected, actual);
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
        fprintf(stderr, "%s: expected datetime text, got %s\n", context, actual);
        return 1;
    }
    return 0;
}
