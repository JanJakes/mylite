#include "mylite_test_support.h"

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
    mysql_user_selected_column_count = 5,
    show_columns_column_count = 6,
    show_full_columns_column_count = 9,
    show_index_column_count = 15,
    column_order_column_count = 2,
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

static int test_mysql_user_table(void);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_row_count_status(mylite_db *database, const char *context);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int expect_datetime_text(const char *actual, const char *context);

static const char *const mysql_user_selected_columns[mysql_user_selected_column_count] = {
    "Host",
    "User",
    "Select_priv",
    "plugin",
    "User_attributes",
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

static const char *const column_order_columns[column_order_column_count] = {
    "COLUMN_NAME",
    "ORDINAL_POSITION",
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
    return test_mysql_user_table() == 0 ? 0 : 1;
}

static int test_mysql_user_table(void) {
    enum {
        mysql_user_column_count = 51,
        selected_information_schema_column_count = 10,
        show_index_row_count = 2,
        information_schema_table_constraints_column_count = 3,
        information_schema_key_column_usage_column_count = 7,
        information_schema_table_constraints_extensions_column_count = 4,
        information_schema_statistics_column_count = 13,
    };

    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_zero[] = {"0"};
    static const char *const show_host_column_values[] = {
        "Host",
        "char(255)",
        "NO",
        "PRI",
        "",
        "",
    };
    static const char *const show_plugin_full_column_values[] = {
        "plugin",
        "char(64)",
        "utf8mb3_bin",
        "NO",
        "",
        "caching_sha2_password",
        "",
        "select,insert,update,references",
        "",
    };
    static const char *const column_order_values[] = {
        "Host",
        "1",
        "User",
        "2",
        "Select_priv",
        "3",
        "Insert_priv",
        "4",
        "Update_priv",
        "5",
        "Delete_priv",
        "6",
        "Create_priv",
        "7",
        "Drop_priv",
        "8",
        "Reload_priv",
        "9",
        "Shutdown_priv",
        "10",
        "Process_priv",
        "11",
        "File_priv",
        "12",
        "Grant_priv",
        "13",
        "References_priv",
        "14",
        "Index_priv",
        "15",
        "Alter_priv",
        "16",
        "Show_db_priv",
        "17",
        "Super_priv",
        "18",
        "Create_tmp_table_priv",
        "19",
        "Lock_tables_priv",
        "20",
        "Execute_priv",
        "21",
        "Repl_slave_priv",
        "22",
        "Repl_client_priv",
        "23",
        "Create_view_priv",
        "24",
        "Show_view_priv",
        "25",
        "Create_routine_priv",
        "26",
        "Alter_routine_priv",
        "27",
        "Create_user_priv",
        "28",
        "Event_priv",
        "29",
        "Trigger_priv",
        "30",
        "Create_tablespace_priv",
        "31",
        "ssl_type",
        "32",
        "ssl_cipher",
        "33",
        "x509_issuer",
        "34",
        "x509_subject",
        "35",
        "max_questions",
        "36",
        "max_updates",
        "37",
        "max_connections",
        "38",
        "max_user_connections",
        "39",
        "plugin",
        "40",
        "authentication_string",
        "41",
        "password_expired",
        "42",
        "password_last_changed",
        "43",
        "password_lifetime",
        "44",
        "account_locked",
        "45",
        "Create_role_priv",
        "46",
        "Drop_role_priv",
        "47",
        "Password_reuse_history",
        "48",
        "Password_reuse_time",
        "49",
        "Password_require_current",
        "50",
        "User_attributes",
        "51",
    };
    static const char *const information_schema_columns_values[] = {
        "Host",
        "1",
        "",
        "NO",
        "char",
        "255",
        "255",
        NULL,
        NULL,
        NULL,
        "ascii",
        "ascii_general_ci",
        "char(255)",
        "PRI",
        "",
        "select,insert,update,references",
        "",
        "",
        "User",
        "2",
        "",
        "NO",
        "char",
        "32",
        "96",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_bin",
        "char(32)",
        "PRI",
        "",
        "select,insert,update,references",
        "",
        "",
        "Select_priv",
        "3",
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
        "enum('N','Y')",
        "",
        "",
        "select,insert,update,references",
        "",
        "",
        "ssl_cipher",
        "33",
        NULL,
        "NO",
        "blob",
        "65535",
        "65535",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "blob",
        "",
        "",
        "select,insert,update,references",
        "",
        "",
        "max_questions",
        "36",
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
        "int unsigned",
        "",
        "",
        "select,insert,update,references",
        "",
        "",
        "plugin",
        "40",
        "caching_sha2_password",
        "NO",
        "char",
        "64",
        "192",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_bin",
        "char(64)",
        "",
        "",
        "select,insert,update,references",
        "",
        "",
        "authentication_string",
        "41",
        NULL,
        "YES",
        "text",
        "65535",
        "65535",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_bin",
        "text",
        "",
        "",
        "select,insert,update,references",
        "",
        "",
        "password_last_changed",
        "43",
        NULL,
        "YES",
        "timestamp",
        NULL,
        NULL,
        NULL,
        NULL,
        "0",
        NULL,
        NULL,
        "timestamp",
        "",
        "",
        "select,insert,update,references",
        "",
        "",
        "Password_require_current",
        "50",
        NULL,
        "YES",
        "enum",
        "1",
        "3",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "enum('N','Y')",
        "",
        "",
        "select,insert,update,references",
        "",
        "",
        "User_attributes",
        "51",
        NULL,
        "YES",
        "json",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "json",
        "",
        "",
        "select,insert,update,references",
        "",
        "",
    };
    static const char *const show_index_values[] = {
        "user", "0", "PRIMARY", "1", "Host", "A", "2", NULL, NULL, "", "BTREE", "", "", "YES", NULL,
        "user", "0", "PRIMARY", "2", "User", "A", "5", NULL, NULL, "", "BTREE", "", "", "YES", NULL,
    };
    static const char *const table_constraints_columns[] = {
        "CONSTRAINT_NAME",
        "CONSTRAINT_TYPE",
        "ENFORCED",
    };
    static const char *const table_constraints_values[] = {"PRIMARY", "PRIMARY KEY", "YES"};
    static const char *const key_column_usage_columns[] = {
        "CONSTRAINT_NAME",
        "COLUMN_NAME",
        "ORDINAL_POSITION",
        "POSITION_IN_UNIQUE_CONSTRAINT",
        "REFERENCED_TABLE_SCHEMA",
        "REFERENCED_TABLE_NAME",
        "REFERENCED_COLUMN_NAME",
    };
    static const char *const key_column_usage_values[] = {
        "PRIMARY",
        "Host",
        "1",
        NULL,
        NULL,
        NULL,
        NULL,
        "PRIMARY",
        "User",
        "2",
        NULL,
        NULL,
        NULL,
        NULL,
    };
    static const char *const table_constraints_extensions_columns[] = {
        "CONSTRAINT_NAME",
        "TABLE_NAME",
        "ENGINE_ATTRIBUTE",
        "SECONDARY_ENGINE_ATTRIBUTE",
    };
    static const char *const table_constraints_extensions_values[] = {
        "PRIMARY",
        "user",
        NULL,
        NULL,
    };
    static const char *const statistics_columns[] = {
        "INDEX_NAME",
        "SEQ_IN_INDEX",
        "COLUMN_NAME",
        "COLLATION",
        "CARDINALITY",
        "SUB_PART",
        "PACKED",
        "NULLABLE",
        "INDEX_TYPE",
        "COMMENT",
        "INDEX_COMMENT",
        "IS_VISIBLE",
        "EXPRESSION",
    };
    static const char *const statistics_values[] = {
        "PRIMARY", "1", "Host", "A", "2", NULL, NULL, "", "BTREE", "", "", "YES", NULL,
        "PRIMARY", "2", "User", "A", "5", NULL, NULL, "", "BTREE", "", "", "YES", NULL,
    };
    static const char *const information_schema_tables_values[] = {
        "user",
        "BASE TABLE",
        "InnoDB",
        "10",
        "Dynamic",
        "5",
        "3276",
        "16384",
        "0",
        "0",
        "4194304",
        NULL,
        expected_datetime_value,
        NULL,
        NULL,
        "utf8mb3_bin",
        NULL,
        "row_format=DYNAMIC stats_persistent=0",
        "Users and global privileges",
    };
    static const char *const show_table_status_values[] = {
        "user",
        "InnoDB",
        "10",
        "Dynamic",
        "5",
        "3276",
        "16384",
        "0",
        "0",
        "4194304",
        NULL,
        expected_datetime_value,
        NULL,
        NULL,
        "utf8mb3_bin",
        NULL,
        "row_format=DYNAMIC stats_persistent=0",
        "Users and global privileges",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "mysql-user-table") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT Host, User, Select_priv, plugin, User_attributes "
                   "FROM mysql.user",
            .column_names = mysql_user_selected_columns,
            .column_count = mysql_user_selected_column_count,
            .values = NULL,
            .row_count = 0U,
            .context = "mysql.user direct empty read",
        }
    );
    failures += expect_row_count_status(database, "row count after mysql.user read");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM mysql.user",
            .column_names = count_column,
            .column_count = sizeof(count_column) / sizeof(count_column[0]),
            .values = count_zero,
            .row_count = 1U,
            .context = "mysql.user count",
        }
    );
    failures += expect_statement_ok(database, "USE mysql");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM user",
            .column_names = count_column,
            .column_count = sizeof(count_column) / sizeof(count_column[0]),
            .values = count_zero,
            .row_count = 1U,
            .context = "selected mysql.user count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "DESC mysql.user",
            .column_names = show_columns_columns,
            .column_count = show_columns_column_count,
            .values = NULL,
            .row_count = mysql_user_column_count,
            .context = "mysql.user describe row count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM mysql.user LIKE 'Host'",
            .column_names = show_columns_columns,
            .column_count = show_columns_column_count,
            .values = show_host_column_values,
            .row_count = 1U,
            .context = "mysql.user show columns Host",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW FULL COLUMNS FROM mysql.user WHERE Field = 'plugin'",
            .column_names = show_full_columns_columns,
            .column_count = show_full_columns_column_count,
            .values = show_plugin_full_column_values,
            .row_count = 1U,
            .context = "mysql.user show full columns plugin",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM mysql.user WHERE Key_name = 'PRIMARY'",
            .column_names = show_index_columns,
            .column_count = show_index_column_count,
            .values = show_index_values,
            .row_count = show_index_row_count,
            .context = "mysql.user show index",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, ORDINAL_POSITION FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'user' "
                   "ORDER BY ORDINAL_POSITION",
            .column_names = column_order_columns,
            .column_count = column_order_column_count,
            .values = column_order_values,
            .row_count = mysql_user_column_count,
            .context = "mysql.user information_schema.columns order",
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
                   "WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'user' "
                   "AND COLUMN_NAME IN ('Host','User','Select_priv','ssl_cipher',"
                   "'max_questions','plugin','authentication_string','password_last_changed',"
                   "'Password_require_current','User_attributes') ORDER BY ORDINAL_POSITION",
            .column_names = information_schema_columns_columns,
            .column_count = information_schema_columns_column_count,
            .values = information_schema_columns_values,
            .row_count = selected_information_schema_column_count,
            .context = "mysql.user information_schema.columns metadata",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED "
                   "FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "
                   "WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'user'",
            .column_names = table_constraints_columns,
            .column_count = information_schema_table_constraints_column_count,
            .values = table_constraints_values,
            .row_count = 1U,
            .context = "mysql.user information_schema.table_constraints",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION, "
                   "POSITION_IN_UNIQUE_CONSTRAINT, REFERENCED_TABLE_SCHEMA, "
                   "REFERENCED_TABLE_NAME, REFERENCED_COLUMN_NAME "
                   "FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE "
                   "WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'user' "
                   "ORDER BY ORDINAL_POSITION",
            .column_names = key_column_usage_columns,
            .column_count = information_schema_key_column_usage_column_count,
            .values = key_column_usage_values,
            .row_count = show_index_row_count,
            .context = "mysql.user information_schema.key_column_usage",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONSTRAINT_NAME, TABLE_NAME, ENGINE_ATTRIBUTE, "
                   "SECONDARY_ENGINE_ATTRIBUTE FROM "
                   "INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS "
                   "WHERE CONSTRAINT_SCHEMA = 'mysql' AND TABLE_NAME = 'user'",
            .column_names = table_constraints_extensions_columns,
            .column_count = information_schema_table_constraints_extensions_column_count,
            .values = table_constraints_extensions_values,
            .row_count = 1U,
            .context = "mysql.user information_schema.table_constraints_extensions",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT INDEX_NAME, SEQ_IN_INDEX, COLUMN_NAME, COLLATION, CARDINALITY, "
                   "SUB_PART, PACKED, NULLABLE, INDEX_TYPE, COMMENT, INDEX_COMMENT, "
                   "IS_VISIBLE, EXPRESSION FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'user' "
                   "ORDER BY SEQ_IN_INDEX",
            .column_names = statistics_columns,
            .column_count = information_schema_statistics_column_count,
            .values = statistics_values,
            .row_count = show_index_row_count,
            .context = "mysql.user information_schema.statistics",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS, "
                   "AVG_ROW_LENGTH, DATA_LENGTH, MAX_DATA_LENGTH, INDEX_LENGTH, DATA_FREE, "
                   "AUTO_INCREMENT, CREATE_TIME, UPDATE_TIME, CHECK_TIME, TABLE_COLLATION, "
                   "CHECKSUM, CREATE_OPTIONS, TABLE_COMMENT FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'user'",
            .column_names = information_schema_tables_columns,
            .column_count = information_schema_tables_column_count,
            .values = information_schema_tables_values,
            .row_count = 1U,
            .context = "mysql.user information_schema.tables",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW TABLE STATUS FROM mysql WHERE Name = 'user' "
                   "AND Data_free = '4194304'",
            .column_names = show_table_status_columns,
            .column_count = show_table_status_column_count,
            .values = show_table_status_values,
            .row_count = 1U,
            .context = "mysql.user show table status",
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
    if (actual == NULL) {
        fprintf(stderr, "%s: expected datetime text, got NULL\n", context);
        return 1;
    }
    if (strlen(actual) != datetime_text_length) {
        fprintf(
            stderr,
            "%s: expected datetime length %d, got [%s]\n",
            context,
            datetime_text_length,
            actual
        );
        return 1;
    }
    for (size_t index = 0U; index < datetime_text_length; ++index) {
        bool is_separator =
            index == datetime_year_month_separator || index == datetime_month_day_separator ||
            index == datetime_date_time_separator || index == datetime_hour_minute_separator ||
            index == datetime_minute_second_separator;
        char expected_separator = '\0';

        if (!is_separator) {
            if (actual[index] < '0' || actual[index] > '9') {
                fprintf(stderr, "%s: expected datetime digit, got [%s]\n", context, actual);
                return 1;
            }
            continue;
        }

        if (index == datetime_date_time_separator) {
            expected_separator = ' ';
        } else if (index < datetime_date_time_separator) {
            expected_separator = '-';
        } else {
            expected_separator = ':';
        }
        if (actual[index] != expected_separator) {
            fprintf(stderr, "%s: expected datetime separator, got [%s]\n", context, actual);
            return 1;
        }
    }
    return 0;
}
