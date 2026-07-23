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
    sys_auto_column_count = 10,
    show_columns_column_count = 6,
    show_full_columns_column_count = 9,
    show_index_column_count = 15,
    information_schema_columns_column_count = 13,
    information_schema_tables_column_count = 19,
    information_schema_views_column_count = 9,
    information_schema_view_table_usage_column_count = 4,
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

static int test_sys_schema_auto_increment_columns_view(void);
static int seed_auto_increment_tables(mylite_db *database);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_query_contains(mylite_db *database, struct expected_query_contains expected);
static int expect_row_count_status(mylite_db *database, const char *context);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int expect_datetime_text(const char *actual, const char *context);

static const char *const sys_auto_columns[sys_auto_column_count] = {
    "table_schema",
    "table_name",
    "column_name",
    "data_type",
    "column_type",
    "is_signed",
    "is_unsigned",
    "max_value",
    "auto_increment",
    "auto_increment_ratio",
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
        "COLUMN_NAME",
        "ORDINAL_POSITION",
        "COLUMN_DEFAULT",
        "IS_NULLABLE",
        "DATA_TYPE",
        "CHARACTER_MAXIMUM_LENGTH",
        "CHARACTER_OCTET_LENGTH",
        "NUMERIC_PRECISION",
        "NUMERIC_SCALE",
        "CHARACTER_SET_NAME",
        "COLLATION_NAME",
        "COLUMN_TYPE",
        "PRIVILEGES",
};

static const char *const information_schema_tables_columns[information_schema_tables_column_count] =
    {
        "TABLE_NAME",      "TABLE_TYPE",     "ENGINE",         "VERSION",         "ROW_FORMAT",
        "TABLE_ROWS",      "AVG_ROW_LENGTH", "DATA_LENGTH",    "MAX_DATA_LENGTH", "INDEX_LENGTH",
        "DATA_FREE",       "AUTO_INCREMENT", "CREATE_TIME",    "UPDATE_TIME",     "CHECK_TIME",
        "TABLE_COLLATION", "CHECKSUM",       "CREATE_OPTIONS", "TABLE_COMMENT",
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
    return test_sys_schema_auto_increment_columns_view() == 0 ? 0 : 1;
}

static int test_sys_schema_auto_increment_columns_view(void) {
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_zero[] = {"0"};
    static const char *const default_order_column[] = {"table_name"};
    static const char *const default_order_values[] = {"signed_tiny"};
    static const char *const show_columns_values[] = {
        "table_schema",
        "varchar(64)",
        "NO",
        "",
        NULL,
        "",
        "table_name",
        "varchar(64)",
        "NO",
        "",
        NULL,
        "",
        "column_name",
        "varchar(64)",
        "YES",
        "",
        NULL,
        "",
        "data_type",
        "longtext",
        "YES",
        "",
        NULL,
        "",
        "column_type",
        "mediumtext",
        "NO",
        "",
        NULL,
        "",
        "is_signed",
        "int",
        "NO",
        "",
        "0",
        "",
        "is_unsigned",
        "int",
        "NO",
        "",
        "0",
        "",
        "max_value",
        "bigint unsigned",
        "YES",
        "",
        NULL,
        "",
        "auto_increment",
        "bigint unsigned",
        "YES",
        "",
        NULL,
        "",
        "auto_increment_ratio",
        "decimal(25,4) unsigned",
        "YES",
        "",
        NULL,
        "",
    };
    static const char *const show_full_columns_values[] = {
        "table_schema",
        "varchar(64)",
        "utf8mb3_bin",
        "NO",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "table_name",
        "varchar(64)",
        "utf8mb3_bin",
        "NO",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "column_name",
        "varchar(64)",
        "utf8mb3_tolower_ci",
        "YES",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "data_type",
        "longtext",
        "utf8mb3_bin",
        "YES",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "column_type",
        "mediumtext",
        "utf8mb3_bin",
        "NO",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "is_signed",
        "int",
        NULL,
        "NO",
        "",
        "0",
        "",
        "select,insert,update,references",
        "",
        "is_unsigned",
        "int",
        NULL,
        "NO",
        "",
        "0",
        "",
        "select,insert,update,references",
        "",
        "max_value",
        "bigint unsigned",
        NULL,
        "YES",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "auto_increment",
        "bigint unsigned",
        NULL,
        "YES",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "auto_increment_ratio",
        "decimal(25,4) unsigned",
        NULL,
        "YES",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
    };
    static const char *const columns_metadata_values[] = {
        "table_schema",
        "1",
        NULL,
        "NO",
        "varchar",
        "64",
        "192",
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_bin",
        "varchar(64)",
        "select,insert,update,references",
        "table_name",
        "2",
        NULL,
        "NO",
        "varchar",
        "64",
        "192",
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_bin",
        "varchar(64)",
        "select,insert,update,references",
        "column_name",
        "3",
        NULL,
        "YES",
        "varchar",
        "64",
        "192",
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_tolower_ci",
        "varchar(64)",
        "select,insert,update,references",
        "data_type",
        "4",
        NULL,
        "YES",
        "longtext",
        "4294967295",
        "4294967295",
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_bin",
        "longtext",
        "select,insert,update,references",
        "column_type",
        "5",
        NULL,
        "NO",
        "mediumtext",
        "16777215",
        "16777215",
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_bin",
        "mediumtext",
        "select,insert,update,references",
        "is_signed",
        "6",
        "0",
        "NO",
        "int",
        NULL,
        NULL,
        "10",
        "0",
        NULL,
        NULL,
        "int",
        "select,insert,update,references",
        "is_unsigned",
        "7",
        "0",
        "NO",
        "int",
        NULL,
        NULL,
        "10",
        "0",
        NULL,
        NULL,
        "int",
        "select,insert,update,references",
        "max_value",
        "8",
        NULL,
        "YES",
        "bigint",
        NULL,
        NULL,
        "20",
        "0",
        NULL,
        NULL,
        "bigint unsigned",
        "select,insert,update,references",
        "auto_increment",
        "9",
        NULL,
        "YES",
        "bigint",
        NULL,
        NULL,
        "20",
        "0",
        NULL,
        NULL,
        "bigint unsigned",
        "select,insert,update,references",
        "auto_increment_ratio",
        "10",
        NULL,
        "YES",
        "decimal",
        NULL,
        NULL,
        "25",
        "4",
        NULL,
        NULL,
        "decimal(25,4) unsigned",
        "select,insert,update,references",
    };
    static const char *const auto_rows_values[] = {
        "app",
        "empty_default",
        "id",
        "int",
        "int",
        "1",
        "0",
        "2147483647",
        NULL,
        NULL,
        "app",
        "explicit_next",
        "id",
        "int",
        "int",
        "1",
        "0",
        "2147483647",
        "100",
        "0.0000",
        "app",
        "signed_int",
        "id",
        "int",
        "int",
        "1",
        "0",
        "2147483647",
        "3",
        "0.0000",
        "app",
        "signed_tiny",
        "id",
        "tinyint",
        "tinyint",
        "1",
        "0",
        "127",
        "4",
        "0.0315",
        "app",
        "unsigned_big",
        "id",
        "bigint",
        "bigint unsigned",
        "0",
        "1",
        "18446744073709551615",
        "8",
        "0.0000",
    };
    static const char *const selected_schema_values[] = {
        "explicit_next",
        "100",
        "signed_tiny",
        "4",
    };
    static const char *const views_values[] = {
        "def",
        "sys",
        "schema_auto_increment_columns",
        "NONE",
        "NO",
        "mysql.sys@localhost",
        "INVOKER",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
    };
    static const char *const view_usage_values[] = {
        "sys",
        "schema_auto_increment_columns",
        "information_schema",
        "COLUMNS",
        "sys",
        "schema_auto_increment_columns",
        "information_schema",
        "TABLES",
    };
    static const char *const tables_values[] = {
        "schema_auto_increment_columns",
        "VIEW",
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
    static const char *const show_table_status_values[] = {
        "schema_auto_increment_columns",
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

    if (mylite_test_make_path(path, sizeof(path), "sys-schema-auto-increment-columns-view") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += seed_auto_increment_tables(database);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT table_schema, table_name, column_name, data_type, column_type, "
                   "is_signed, is_unsigned, max_value, auto_increment, auto_increment_ratio "
                   "FROM sys.schema_auto_increment_columns WHERE table_schema = 'app' "
                   "ORDER BY table_name",
            .column_names = sys_auto_columns,
            .column_count = sys_auto_column_count,
            .values = auto_rows_values,
            .row_count =
                sizeof(auto_rows_values) / sizeof(auto_rows_values[0]) / sys_auto_column_count,
            .context = "sys.schema_auto_increment_columns row values",
        }
    );
    failures +=
        expect_row_count_status(database, "row count after sys.schema_auto_increment_columns read");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT table_name FROM sys.schema_auto_increment_columns "
                   "WHERE table_schema = 'app' LIMIT 1",
            .column_names = default_order_column,
            .column_count = 1U,
            .values = default_order_values,
            .row_count = 1U,
            .context = "sys.schema_auto_increment_columns default order",
        }
    );
    failures += expect_statement_ok(database, "USE sys");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT table_name, auto_increment FROM schema_auto_increment_columns "
                   "WHERE table_schema = 'app' "
                   "AND table_name IN ('explicit_next', 'signed_tiny') ORDER BY table_name",
            .column_names = (const char *const[]){"table_name", "auto_increment"},
            .column_count = 2U,
            .values = selected_schema_values,
            .row_count = sizeof(selected_schema_values) / sizeof(selected_schema_values[0]) / 2U,
            .context = "sys.schema_auto_increment_columns selected schema read",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM schema_auto_increment_columns",
            .column_names = show_columns_columns,
            .column_count = show_columns_column_count,
            .values = show_columns_values,
            .row_count = sizeof(show_columns_values) / sizeof(show_columns_values[0]) /
                         show_columns_column_count,
            .context = "sys.schema_auto_increment_columns show columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "DESCRIBE sys.schema_auto_increment_columns",
            .column_names = show_columns_columns,
            .column_count = show_columns_column_count,
            .values = show_columns_values,
            .row_count = sizeof(show_columns_values) / sizeof(show_columns_values[0]) /
                         show_columns_column_count,
            .context = "sys.schema_auto_increment_columns describe",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW FULL COLUMNS FROM sys.schema_auto_increment_columns",
            .column_names = show_full_columns_columns,
            .column_count = show_full_columns_column_count,
            .values = show_full_columns_values,
            .row_count = sizeof(show_full_columns_values) / sizeof(show_full_columns_values[0]) /
                         show_full_columns_column_count,
            .context = "sys.schema_auto_increment_columns show full columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE, "
                   "DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, "
                   "NUMERIC_PRECISION, NUMERIC_SCALE, CHARACTER_SET_NAME, COLLATION_NAME, "
                   "COLUMN_TYPE, PRIVILEGES FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'schema_auto_increment_columns' "
                   "ORDER BY ORDINAL_POSITION",
            .column_names = information_schema_columns_columns,
            .column_count = information_schema_columns_column_count,
            .values = columns_metadata_values,
            .row_count = sizeof(columns_metadata_values) / sizeof(columns_metadata_values[0]) /
                         information_schema_columns_column_count,
            .context = "sys.schema_auto_increment_columns information_schema.columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_CATALOG, TABLE_SCHEMA, TABLE_NAME, CHECK_OPTION, "
                   "IS_UPDATABLE, DEFINER, SECURITY_TYPE, CHARACTER_SET_CLIENT, "
                   "COLLATION_CONNECTION FROM INFORMATION_SCHEMA.VIEWS "
                   "WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'schema_auto_increment_columns'",
            .column_names = information_schema_views_columns,
            .column_count = information_schema_views_column_count,
            .values = views_values,
            .row_count = 1U,
            .context = "sys.schema_auto_increment_columns information_schema.views",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT VIEW_SCHEMA, VIEW_NAME, TABLE_SCHEMA, TABLE_NAME "
                   "FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE "
                   "WHERE VIEW_SCHEMA = 'sys' AND VIEW_NAME = 'schema_auto_increment_columns' "
                   "ORDER BY TABLE_NAME",
            .column_names = information_schema_view_table_usage_columns,
            .column_count = information_schema_view_table_usage_column_count,
            .values = view_usage_values,
            .row_count = sizeof(view_usage_values) / sizeof(view_usage_values[0]) /
                         information_schema_view_table_usage_column_count,
            .context = "sys.schema_auto_increment_columns information_schema.view_table_usage",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM sys.schema_auto_increment_columns",
            .column_names = show_index_columns,
            .column_count = show_index_column_count,
            .values = NULL,
            .row_count = 0U,
            .context = "sys.schema_auto_increment_columns show index",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'schema_auto_increment_columns'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "sys.schema_auto_increment_columns statistics count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "
                   "WHERE TABLE_SCHEMA = 'sys' "
                   "AND TABLE_NAME = 'schema_auto_increment_columns'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "sys.schema_auto_increment_columns table_constraints count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE "
                   "WHERE TABLE_SCHEMA = 'sys' "
                   "AND TABLE_NAME = 'schema_auto_increment_columns'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "sys.schema_auto_increment_columns key_column_usage count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS "
                   "WHERE CONSTRAINT_SCHEMA = 'sys' "
                   "AND TABLE_NAME = 'schema_auto_increment_columns'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "sys.schema_auto_increment_columns table_constraints_extensions count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS, "
                   "AVG_ROW_LENGTH, DATA_LENGTH, MAX_DATA_LENGTH, INDEX_LENGTH, DATA_FREE, "
                   "AUTO_INCREMENT, CREATE_TIME, UPDATE_TIME, CHECK_TIME, TABLE_COLLATION, "
                   "CHECKSUM, CREATE_OPTIONS, TABLE_COMMENT FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'schema_auto_increment_columns'",
            .column_names = information_schema_tables_columns,
            .column_count = information_schema_tables_column_count,
            .values = tables_values,
            .row_count = 1U,
            .context = "sys.schema_auto_increment_columns information_schema.tables",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW TABLE STATUS FROM sys LIKE 'schema_auto_increment_columns'",
            .column_names = show_table_status_columns,
            .column_count = show_table_status_column_count,
            .values = show_table_status_values,
            .row_count = 1U,
            .context = "sys.schema_auto_increment_columns show table status",
        }
    );
    failures += expect_query_contains(
        database,
        (struct expected_query_contains){
            .sql = "SHOW CREATE VIEW sys.schema_auto_increment_columns",
            .column_names = show_create_view_columns,
            .column_count = show_create_view_column_count,
            .row_count = 1U,
            .row_index = 0U,
            .column_index = 1U,
            .needle = "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY "
                      "INVOKER VIEW `sys`.`schema_auto_increment_columns`",
            .context = "sys.schema_auto_increment_columns show create view",
        }
    );
    failures += expect_query_contains(
        database,
        (struct expected_query_contains){
            .sql = "SHOW CREATE TABLE schema_auto_increment_columns",
            .column_names = show_create_view_columns,
            .column_count = show_create_view_column_count,
            .row_count = 1U,
            .row_index = 0U,
            .column_index = 1U,
            .needle = "VIEW `schema_auto_increment_columns` (`table_schema`,`table_name`,"
                      "`column_name`,`data_type`,`column_type`,`is_signed`,`is_unsigned`,"
                      "`max_value`,`auto_increment`,`auto_increment_ratio`) AS select",
            .context = "sys.schema_auto_increment_columns show create table",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int seed_auto_increment_tables(mylite_db *database) {
    int failures = 0;

    failures += expect_statement_ok(
        database,
        "CREATE TABLE empty_default (id INT AUTO_INCREMENT PRIMARY KEY)"
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE explicit_next (id INT AUTO_INCREMENT PRIMARY KEY) AUTO_INCREMENT = 100"
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE signed_int (id INT AUTO_INCREMENT PRIMARY KEY, v INT)"
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE signed_tiny (id TINYINT AUTO_INCREMENT PRIMARY KEY, v INT)"
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE unsigned_big (id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY, v INT)"
    );
    failures += expect_statement_ok(database, "INSERT INTO signed_int (v) VALUES (10),(20)");
    failures += expect_statement_ok(database, "INSERT INTO signed_tiny (v) VALUES (1),(2),(3)");
    failures += expect_statement_ok(database, "INSERT INTO unsigned_big (id, v) VALUES (7, 70)");
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

                if (expected_value == expected_datetime_value) {
                    failures += expect_datetime_text(actual_value, expected.context);
                } else {
                    failures += mylite_test_expect_text_or_null(
                        actual_value,
                        expected_value,
                        expected.context
                    );
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
