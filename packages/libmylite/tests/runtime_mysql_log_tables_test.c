#include <mylite/mylite.h>

#include <stdbool.h>
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
    general_log_column_count = 6,
    slow_log_column_count = 12,
    show_columns_column_count = 6,
    show_full_columns_column_count = 9,
    show_index_column_count = 15,
    information_schema_columns_column_count = 18,
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

static int test_mysql_log_tables(void);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_count_zero(mylite_db *database, const char *sql, const char *context);
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

static const char *const count_column[] = {"COUNT(*)"};
static const char *const count_zero[] = {"0"};

static const char *const general_log_columns[general_log_column_count] = {
    "event_time",
    "user_host",
    "thread_id",
    "server_id",
    "command_type",
    "argument",
};

static const char *const slow_log_columns[slow_log_column_count] = {
    "start_time",
    "user_host",
    "query_time",
    "lock_time",
    "rows_sent",
    "rows_examined",
    "db",
    "last_insert_id",
    "insert_id",
    "server_id",
    "sql_text",
    "thread_id",
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
    return test_mysql_log_tables() == 0 ? 0 : 1;
}

static int test_mysql_log_tables(void) {
    static const char *const general_show_columns_values[] = {
        "event_time",
        "timestamp(6)",
        "NO",
        "",
        "CURRENT_TIMESTAMP(6)",
        "DEFAULT_GENERATED on update CURRENT_TIMESTAMP(6)",
        "user_host",
        "mediumtext",
        "NO",
        "",
        NULL,
        "",
        "thread_id",
        "bigint unsigned",
        "NO",
        "",
        NULL,
        "",
        "server_id",
        "int unsigned",
        "NO",
        "",
        NULL,
        "",
        "command_type",
        "varchar(64)",
        "NO",
        "",
        NULL,
        "",
        "argument",
        "mediumblob",
        "NO",
        "",
        NULL,
        "",
    };
    static const char *const general_show_full_columns_values[] = {
        "event_time",
        "timestamp(6)",
        NULL,
        "NO",
        "",
        "CURRENT_TIMESTAMP(6)",
        "DEFAULT_GENERATED on update CURRENT_TIMESTAMP(6)",
        "select,insert,update,references",
        "",
        "user_host",
        "mediumtext",
        "utf8mb3_general_ci",
        "NO",
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
        "server_id",
        "int unsigned",
        NULL,
        "NO",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "command_type",
        "varchar(64)",
        "utf8mb3_general_ci",
        "NO",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "argument",
        "mediumblob",
        NULL,
        "NO",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
    };
    static const char *const general_information_schema_columns_values[] = {
        "event_time",
        "1",
        "CURRENT_TIMESTAMP(6)",
        "NO",
        "timestamp",
        NULL,
        NULL,
        NULL,
        NULL,
        "6",
        NULL,
        NULL,
        "timestamp(6)",
        "",
        "DEFAULT_GENERATED on update CURRENT_TIMESTAMP(6)",
        "select,insert,update,references",
        "",
        "",
        "user_host",
        "2",
        NULL,
        "NO",
        "mediumtext",
        "16777215",
        "16777215",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "mediumtext",
        "",
        "",
        "select,insert,update,references",
        "",
        "",
        "thread_id",
        "3",
        NULL,
        "NO",
        "bigint",
        NULL,
        NULL,
        "20",
        "0",
        NULL,
        NULL,
        NULL,
        "bigint unsigned",
        "",
        "",
        "select,insert,update,references",
        "",
        "",
        "server_id",
        "4",
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
        "command_type",
        "5",
        NULL,
        "NO",
        "varchar",
        "64",
        "192",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(64)",
        "",
        "",
        "select,insert,update,references",
        "",
        "",
        "argument",
        "6",
        NULL,
        "NO",
        "mediumblob",
        "16777215",
        "16777215",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "mediumblob",
        "",
        "",
        "select,insert,update,references",
        "",
        "",
    };
    static const char *const slow_show_columns_values[] = {
        "start_time",
        "timestamp(6)",
        "NO",
        "",
        "CURRENT_TIMESTAMP(6)",
        "DEFAULT_GENERATED on update CURRENT_TIMESTAMP(6)",
        "user_host",
        "mediumtext",
        "NO",
        "",
        NULL,
        "",
        "query_time",
        "time(6)",
        "NO",
        "",
        NULL,
        "",
        "lock_time",
        "time(6)",
        "NO",
        "",
        NULL,
        "",
        "rows_sent",
        "int",
        "NO",
        "",
        NULL,
        "",
        "rows_examined",
        "int",
        "NO",
        "",
        NULL,
        "",
        "db",
        "varchar(512)",
        "NO",
        "",
        NULL,
        "",
        "last_insert_id",
        "int",
        "NO",
        "",
        NULL,
        "",
        "insert_id",
        "int",
        "NO",
        "",
        NULL,
        "",
        "server_id",
        "int unsigned",
        "NO",
        "",
        NULL,
        "",
        "sql_text",
        "mediumblob",
        "NO",
        "",
        NULL,
        "",
        "thread_id",
        "bigint unsigned",
        "NO",
        "",
        NULL,
        "",
    };
    static const char *const slow_show_full_query_time_values[] = {
        "query_time",
        "time(6)",
        NULL,
        "NO",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
    };
    static const char *const slow_information_schema_columns_values[] = {
        "start_time",
        "1",
        "CURRENT_TIMESTAMP(6)",
        "NO",
        "timestamp",
        NULL,
        NULL,
        NULL,
        NULL,
        "6",
        NULL,
        NULL,
        "timestamp(6)",
        "",
        "DEFAULT_GENERATED on update CURRENT_TIMESTAMP(6)",
        "select,insert,update,references",
        "",
        "",
        "user_host",
        "2",
        NULL,
        "NO",
        "mediumtext",
        "16777215",
        "16777215",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "mediumtext",
        "",
        "",
        "select,insert,update,references",
        "",
        "",
        "query_time",
        "3",
        NULL,
        "NO",
        "time",
        NULL,
        NULL,
        NULL,
        NULL,
        "6",
        NULL,
        NULL,
        "time(6)",
        "",
        "",
        "select,insert,update,references",
        "",
        "",
        "lock_time",
        "4",
        NULL,
        "NO",
        "time",
        NULL,
        NULL,
        NULL,
        NULL,
        "6",
        NULL,
        NULL,
        "time(6)",
        "",
        "",
        "select,insert,update,references",
        "",
        "",
        "rows_sent",
        "5",
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
        "rows_examined",
        "6",
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
        "db",
        "7",
        NULL,
        "NO",
        "varchar",
        "512",
        "1536",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(512)",
        "",
        "",
        "select,insert,update,references",
        "",
        "",
        "last_insert_id",
        "8",
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
        "insert_id",
        "9",
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
        "server_id",
        "10",
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
        "sql_text",
        "11",
        NULL,
        "NO",
        "mediumblob",
        "16777215",
        "16777215",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "mediumblob",
        "",
        "",
        "select,insert,update,references",
        "",
        "",
        "thread_id",
        "12",
        NULL,
        "NO",
        "bigint",
        NULL,
        NULL,
        "20",
        "0",
        NULL,
        NULL,
        NULL,
        "bigint unsigned",
        "",
        "",
        "select,insert,update,references",
        "",
        "",
    };
    static const char *const general_information_schema_tables_values[] = {
        "general_log",
        "BASE TABLE",
        "CSV",
        "10",
        "Dynamic",
        "2",
        "0",
        "0",
        "0",
        "0",
        "0",
        NULL,
        expected_datetime_value,
        NULL,
        NULL,
        "utf8mb3_general_ci",
        NULL,
        "",
        "General log",
    };
    static const char *const slow_information_schema_tables_values[] = {
        "slow_log",
        "BASE TABLE",
        "CSV",
        "10",
        "Dynamic",
        "2",
        "0",
        "0",
        "0",
        "0",
        "0",
        NULL,
        expected_datetime_value,
        NULL,
        NULL,
        "utf8mb3_general_ci",
        NULL,
        "",
        "Slow log",
    };
    static const char *const general_show_table_status_values[] = {
        "general_log",
        "CSV",
        "10",
        "Dynamic",
        "2",
        "0",
        "0",
        "0",
        "0",
        "0",
        NULL,
        expected_datetime_value,
        NULL,
        NULL,
        "utf8mb3_general_ci",
        NULL,
        "",
        "General log",
    };
    static const char *const slow_show_table_status_values[] = {
        "slow_log",
        "CSV",
        "10",
        "Dynamic",
        "2",
        "0",
        "0",
        "0",
        "0",
        "0",
        NULL,
        expected_datetime_value,
        NULL,
        NULL,
        "utf8mb3_general_ci",
        NULL,
        "",
        "Slow log",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "mysql-log-tables") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(database, "CREATE TABLE t (id INT PRIMARY KEY)");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT event_time, user_host, thread_id, server_id, command_type, argument "
                   "FROM mysql.general_log",
            .column_names = general_log_columns,
            .column_count = general_log_column_count,
            .values = NULL,
            .row_count = 0U,
            .context = "mysql.general_log direct empty read",
        }
    );
    failures += expect_row_count_status(database, "row count after mysql.general_log read");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT start_time, user_host, query_time, lock_time, rows_sent, "
                   "rows_examined, db, last_insert_id, insert_id, server_id, sql_text, "
                   "thread_id FROM mysql.slow_log",
            .column_names = slow_log_columns,
            .column_count = slow_log_column_count,
            .values = NULL,
            .row_count = 0U,
            .context = "mysql.slow_log direct empty read",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM mysql.general_log",
            .column_names = count_column,
            .column_count = sizeof(count_column) / sizeof(count_column[0]),
            .values = count_zero,
            .row_count = 1U,
            .context = "mysql.general_log count",
        }
    );
    failures += expect_statement_ok(database, "USE mysql");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM slow_log",
            .column_names = count_column,
            .column_count = sizeof(count_column) / sizeof(count_column[0]),
            .values = count_zero,
            .row_count = 1U,
            .context = "mysql.slow_log unqualified count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM mysql.general_log",
            .column_names = show_columns_columns,
            .column_count = show_columns_column_count,
            .values = general_show_columns_values,
            .row_count = general_log_column_count,
            .context = "mysql.general_log show columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "DESCRIBE general_log",
            .column_names = show_columns_columns,
            .column_count = show_columns_column_count,
            .values = general_show_columns_values,
            .row_count = general_log_column_count,
            .context = "mysql.general_log describe",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW FULL COLUMNS FROM mysql.general_log",
            .column_names = show_full_columns_columns,
            .column_count = show_full_columns_column_count,
            .values = general_show_full_columns_values,
            .row_count = general_log_column_count,
            .context = "mysql.general_log show full columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM mysql.slow_log",
            .column_names = show_columns_columns,
            .column_count = show_columns_column_count,
            .values = slow_show_columns_values,
            .row_count = slow_log_column_count,
            .context = "mysql.slow_log show columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW FULL COLUMNS FROM mysql.slow_log WHERE Field = 'query_time'",
            .column_names = show_full_columns_columns,
            .column_count = show_full_columns_column_count,
            .values = slow_show_full_query_time_values,
            .row_count = 1U,
            .context = "mysql.slow_log show full columns where",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM mysql.general_log",
            .column_names = show_index_columns,
            .column_count = show_index_column_count,
            .values = NULL,
            .row_count = 0U,
            .context = "mysql.general_log show index empty",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM mysql.slow_log WHERE Key_name = 'PRIMARY'",
            .column_names = show_index_columns,
            .column_count = show_index_column_count,
            .values = NULL,
            .row_count = 0U,
            .context = "mysql.slow_log show index empty",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE, DATA_TYPE, "
                   "CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION, "
                   "NUMERIC_SCALE, DATETIME_PRECISION, CHARACTER_SET_NAME, COLLATION_NAME, "
                   "COLUMN_TYPE, COLUMN_KEY, EXTRA, PRIVILEGES, COLUMN_COMMENT, "
                   "GENERATION_EXPRESSION FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'general_log' "
                   "ORDER BY ORDINAL_POSITION",
            .column_names = information_schema_columns_columns,
            .column_count = information_schema_columns_column_count,
            .values = general_information_schema_columns_values,
            .row_count = general_log_column_count,
            .context = "mysql.general_log information_schema.columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE, DATA_TYPE, "
                   "CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION, "
                   "NUMERIC_SCALE, DATETIME_PRECISION, CHARACTER_SET_NAME, COLLATION_NAME, "
                   "COLUMN_TYPE, COLUMN_KEY, EXTRA, PRIVILEGES, COLUMN_COMMENT, "
                   "GENERATION_EXPRESSION FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'slow_log' "
                   "ORDER BY ORDINAL_POSITION",
            .column_names = information_schema_columns_columns,
            .column_count = information_schema_columns_column_count,
            .values = slow_information_schema_columns_values,
            .row_count = slow_log_column_count,
            .context = "mysql.slow_log information_schema.columns",
        }
    );
    failures += expect_count_zero(
        database,
        "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
        "WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'general_log'",
        "mysql.general_log information_schema.statistics empty"
    );
    failures += expect_count_zero(
        database,
        "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
        "WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'slow_log'",
        "mysql.slow_log information_schema.statistics empty"
    );
    failures += expect_count_zero(
        database,
        "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "
        "WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'general_log'",
        "mysql.general_log information_schema.table_constraints empty"
    );
    failures += expect_count_zero(
        database,
        "SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE "
        "WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'slow_log'",
        "mysql.slow_log information_schema.key_column_usage empty"
    );
    failures += expect_count_zero(
        database,
        "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS "
        "WHERE CONSTRAINT_SCHEMA = 'mysql' AND TABLE_NAME = 'slow_log'",
        "mysql.slow_log information_schema.table_constraints_extensions empty"
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS, "
                   "AVG_ROW_LENGTH, DATA_LENGTH, MAX_DATA_LENGTH, INDEX_LENGTH, DATA_FREE, "
                   "AUTO_INCREMENT, CREATE_TIME, UPDATE_TIME, CHECK_TIME, TABLE_COLLATION, "
                   "CHECKSUM, CREATE_OPTIONS, TABLE_COMMENT FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'general_log'",
            .column_names = information_schema_tables_columns,
            .column_count = information_schema_tables_column_count,
            .values = general_information_schema_tables_values,
            .row_count = 1U,
            .context = "mysql.general_log information_schema.tables",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS, "
                   "AVG_ROW_LENGTH, DATA_LENGTH, MAX_DATA_LENGTH, INDEX_LENGTH, DATA_FREE, "
                   "AUTO_INCREMENT, CREATE_TIME, UPDATE_TIME, CHECK_TIME, TABLE_COLLATION, "
                   "CHECKSUM, CREATE_OPTIONS, TABLE_COMMENT FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'slow_log'",
            .column_names = information_schema_tables_columns,
            .column_count = information_schema_tables_column_count,
            .values = slow_information_schema_tables_values,
            .row_count = 1U,
            .context = "mysql.slow_log information_schema.tables",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW TABLE STATUS FROM mysql LIKE 'general_log'",
            .column_names = show_table_status_columns,
            .column_count = show_table_status_column_count,
            .values = general_show_table_status_values,
            .row_count = 1U,
            .context = "mysql.general_log show table status",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW TABLE STATUS FROM mysql LIKE 'slow_log'",
            .column_names = show_table_status_columns,
            .column_count = show_table_status_column_count,
            .values = slow_show_table_status_values,
            .row_count = 1U,
            .context = "mysql.slow_log show table status",
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

static int expect_count_zero(mylite_db *database, const char *sql, const char *context) {
    return expect_query(
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
        "/tmp/mylite_runtime_mysql_log_tables_%d_%s.mylite",
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
