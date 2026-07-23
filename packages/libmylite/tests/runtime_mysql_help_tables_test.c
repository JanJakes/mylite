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

static int test_mysql_help_tables(void);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_query(mylite_db *database, struct expected_query expected);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
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
    return test_mysql_help_tables() == 0 ? 0 : 1;
}

static int test_mysql_help_tables(void) {
    enum {
        help_table_count = 4,
        count_query_capacity = 96,
        count_context_capacity = 128,
        table_constraints_column_count = 4,
        key_column_usage_column_count = 4,
        table_constraints_extensions_column_count = 4,
        statistics_column_count = 5,
        count_query_row_count = 1,
        show_columns_help_category_row_count = 4,
        describe_help_relation_row_count = 2,
        show_full_help_topic_row_count = 6,
        show_index_help_category_row_count = 2,
        show_index_help_relation_row_count = 2,
        information_schema_columns_row_count = 14,
        statistics_row_count = 8,
        table_constraints_row_count = 7,
        key_column_usage_row_count = 8,
        table_constraints_extensions_row_count = 7,
        information_schema_tables_row_count = 4,
        show_table_status_row_count = 4,
    };

    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_zero[] = {"0"};
    static const char *const help_table_names[help_table_count] = {
        "help_category",
        "help_keyword",
        "help_relation",
        "help_topic",
    };
    static const char *const show_columns_help_category_values[] = {
        "help_category_id",
        "smallint unsigned",
        "NO",
        "PRI",
        NULL,
        "",
        "name",
        "char(64)",
        "NO",
        "UNI",
        NULL,
        "",
        "parent_category_id",
        "smallint unsigned",
        "YES",
        "",
        NULL,
        "",
        "url",
        "text",
        "NO",
        "",
        NULL,
        "",
    };
    static const char *const describe_help_relation_values[] = {
        "help_topic_id",
        "int unsigned",
        "NO",
        "PRI",
        NULL,
        "",
        "help_keyword_id",
        "int unsigned",
        "NO",
        "PRI",
        NULL,
        "",
    };
    static const char *const show_full_help_topic_values[] = {
        "help_topic_id",
        "int unsigned",
        NULL,
        "NO",
        "PRI",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "name",
        "char(64)",
        "utf8mb3_general_ci",
        "NO",
        "UNI",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "help_category_id",
        "smallint unsigned",
        NULL,
        "NO",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "description",
        "text",
        "utf8mb3_general_ci",
        "NO",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "example",
        "text",
        "utf8mb3_general_ci",
        "NO",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "url",
        "text",
        "utf8mb3_general_ci",
        "NO",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
    };
    static const char *const show_index_help_category_values[] = {
        "help_category",
        "0",
        "PRIMARY",
        "1",
        "help_category_id",
        "A",
        "53",
        NULL,
        NULL,
        "",
        "BTREE",
        "",
        "",
        "YES",
        NULL,
        "help_category",
        "0",
        "name",
        "1",
        "name",
        "A",
        "53",
        NULL,
        NULL,
        "",
        "BTREE",
        "",
        "",
        "YES",
        NULL,
    };
    static const char *const show_index_help_relation_values[] = {
        "help_relation",
        "0",
        "PRIMARY",
        "1",
        "help_keyword_id",
        "A",
        "1393",
        NULL,
        NULL,
        "",
        "BTREE",
        "",
        "",
        "YES",
        NULL,
        "help_relation",
        "0",
        "PRIMARY",
        "2",
        "help_topic_id",
        "A",
        "2258",
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
        "help_category",
        "help_category_id",
        "1",
        NULL,
        "NO",
        "smallint",
        NULL,
        NULL,
        "5",
        "0",
        NULL,
        NULL,
        NULL,
        "smallint unsigned",
        "PRI",
        "",
        "select,insert,update,references",
        "",
        "",
        "help_category",
        "name",
        "2",
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
        "UNI",
        "",
        "select,insert,update,references",
        "",
        "",
        "help_category",
        "parent_category_id",
        "3",
        NULL,
        "YES",
        "smallint",
        NULL,
        NULL,
        "5",
        "0",
        NULL,
        NULL,
        NULL,
        "smallint unsigned",
        "",
        "",
        "select,insert,update,references",
        "",
        "",
        "help_category",
        "url",
        "4",
        NULL,
        "NO",
        "text",
        "65535",
        "65535",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "text",
        "",
        "",
        "select,insert,update,references",
        "",
        "",
        "help_keyword",
        "help_keyword_id",
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
        "help_keyword",
        "name",
        "2",
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
        "UNI",
        "",
        "select,insert,update,references",
        "",
        "",
        "help_relation",
        "help_topic_id",
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
        "help_relation",
        "help_keyword_id",
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
        "help_topic",
        "help_topic_id",
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
        "help_topic",
        "name",
        "2",
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
        "UNI",
        "",
        "select,insert,update,references",
        "",
        "",
        "help_topic",
        "help_category_id",
        "3",
        NULL,
        "NO",
        "smallint",
        NULL,
        NULL,
        "5",
        "0",
        NULL,
        NULL,
        NULL,
        "smallint unsigned",
        "",
        "",
        "select,insert,update,references",
        "",
        "",
        "help_topic",
        "description",
        "4",
        NULL,
        "NO",
        "text",
        "65535",
        "65535",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "text",
        "",
        "",
        "select,insert,update,references",
        "",
        "",
        "help_topic",
        "example",
        "5",
        NULL,
        "NO",
        "text",
        "65535",
        "65535",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "text",
        "",
        "",
        "select,insert,update,references",
        "",
        "",
        "help_topic",
        "url",
        "6",
        NULL,
        "NO",
        "text",
        "65535",
        "65535",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "text",
        "",
        "",
        "select,insert,update,references",
        "",
        "",
    };
    static const char *const statistics_columns[] = {
        "TABLE_NAME",
        "INDEX_NAME",
        "SEQ_IN_INDEX",
        "COLUMN_NAME",
        "CARDINALITY",
    };
    static const char *const statistics_values[] = {
        "help_category",
        "PRIMARY",
        "1",
        "help_category_id",
        "53",
        "help_category",
        "name",
        "1",
        "name",
        "53",
        "help_keyword",
        "PRIMARY",
        "1",
        "help_keyword_id",
        "551",
        "help_keyword",
        "name",
        "1",
        "name",
        "551",
        "help_relation",
        "PRIMARY",
        "1",
        "help_keyword_id",
        "1393",
        "help_relation",
        "PRIMARY",
        "2",
        "help_topic_id",
        "2258",
        "help_topic",
        "PRIMARY",
        "1",
        "help_topic_id",
        "596",
        "help_topic",
        "name",
        "1",
        "name",
        "596",
    };
    static const char *const table_constraints_columns[] = {
        "TABLE_NAME",
        "CONSTRAINT_NAME",
        "CONSTRAINT_TYPE",
        "ENFORCED",
    };
    static const char *const table_constraints_values[] = {
        "help_category", "PRIMARY", "PRIMARY KEY",  "YES",     "help_category", "name",
        "UNIQUE",        "YES",     "help_keyword", "PRIMARY", "PRIMARY KEY",   "YES",
        "help_keyword",  "name",    "UNIQUE",       "YES",     "help_relation", "PRIMARY",
        "PRIMARY KEY",   "YES",     "help_topic",   "PRIMARY", "PRIMARY KEY",   "YES",
        "help_topic",    "name",    "UNIQUE",       "YES",
    };
    static const char *const key_column_usage_columns[] = {
        "TABLE_NAME",
        "CONSTRAINT_NAME",
        "COLUMN_NAME",
        "ORDINAL_POSITION",
    };
    static const char *const key_column_usage_values[] = {
        "help_category",
        "PRIMARY",
        "help_category_id",
        "1",
        "help_category",
        "name",
        "name",
        "1",
        "help_keyword",
        "PRIMARY",
        "help_keyword_id",
        "1",
        "help_keyword",
        "name",
        "name",
        "1",
        "help_relation",
        "PRIMARY",
        "help_keyword_id",
        "1",
        "help_relation",
        "PRIMARY",
        "help_topic_id",
        "2",
        "help_topic",
        "PRIMARY",
        "help_topic_id",
        "1",
        "help_topic",
        "name",
        "name",
        "1",
    };
    static const char *const table_constraints_extensions_columns[] = {
        "TABLE_NAME",
        "CONSTRAINT_NAME",
        "ENGINE_ATTRIBUTE",
        "SECONDARY_ENGINE_ATTRIBUTE",
    };
    static const char *const table_constraints_extensions_values[] = {
        "help_category", "PRIMARY", NULL, NULL, "help_category", "name",    NULL, NULL,
        "help_keyword",  "PRIMARY", NULL, NULL, "help_keyword",  "name",    NULL, NULL,
        "help_relation", "PRIMARY", NULL, NULL, "help_topic",    "PRIMARY", NULL, NULL,
        "help_topic",    "name",    NULL, NULL,
    };
    static const char *const information_schema_tables_values[] = {
        "help_category",
        "BASE TABLE",
        "InnoDB",
        "10",
        "Dynamic",
        "53",
        "309",
        "16384",
        "0",
        "16384",
        "4194304",
        NULL,
        expected_datetime_value,
        NULL,
        NULL,
        "utf8mb3_general_ci",
        NULL,
        "row_format=DYNAMIC stats_persistent=0",
        "help categories",
        "help_keyword",
        "BASE TABLE",
        "InnoDB",
        "10",
        "Dynamic",
        "1142",
        "114",
        "131072",
        "0",
        "147456",
        "4194304",
        NULL,
        expected_datetime_value,
        NULL,
        NULL,
        "utf8mb3_general_ci",
        NULL,
        "row_format=DYNAMIC stats_persistent=0",
        "help keywords",
        "help_relation",
        "BASE TABLE",
        "InnoDB",
        "10",
        "Dynamic",
        "1608",
        "50",
        "81920",
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
        "keyword-topic relation",
        "help_topic",
        "BASE TABLE",
        "InnoDB",
        "10",
        "Dynamic",
        "902",
        "1761",
        "1589248",
        "0",
        "98304",
        "4194304",
        NULL,
        expected_datetime_value,
        NULL,
        NULL,
        "utf8mb3_general_ci",
        NULL,
        "row_format=DYNAMIC stats_persistent=0",
        "help topics",
    };
    static const char *const show_table_status_values[] = {
        "help_category",
        "InnoDB",
        "10",
        "Dynamic",
        "53",
        "309",
        "16384",
        "0",
        "16384",
        "4194304",
        NULL,
        expected_datetime_value,
        NULL,
        NULL,
        "utf8mb3_general_ci",
        NULL,
        "row_format=DYNAMIC stats_persistent=0",
        "help categories",
        "help_keyword",
        "InnoDB",
        "10",
        "Dynamic",
        "1142",
        "114",
        "131072",
        "0",
        "147456",
        "4194304",
        NULL,
        expected_datetime_value,
        NULL,
        NULL,
        "utf8mb3_general_ci",
        NULL,
        "row_format=DYNAMIC stats_persistent=0",
        "help keywords",
        "help_relation",
        "InnoDB",
        "10",
        "Dynamic",
        "1608",
        "50",
        "81920",
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
        "keyword-topic relation",
        "help_topic",
        "InnoDB",
        "10",
        "Dynamic",
        "902",
        "1761",
        "1589248",
        "0",
        "98304",
        "4194304",
        NULL,
        expected_datetime_value,
        NULL,
        NULL,
        "utf8mb3_general_ci",
        NULL,
        "row_format=DYNAMIC stats_persistent=0",
        "help topics",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "mysql-help-tables") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");

    for (size_t table_index = 0U; table_index < help_table_count; ++table_index) {
        char sql[count_query_capacity];
        char context[count_context_capacity];
        const char *table_name = help_table_names[table_index];

        (void)snprintf(sql, sizeof(sql), "SELECT COUNT(*) FROM mysql.%s", table_name);
        (void)snprintf(context, sizeof(context), "empty mysql.%s placeholder", table_name);
        failures += expect_query(
            database,
            (struct expected_query){
                .sql = sql,
                .column_names = count_column,
                .column_count = sizeof(count_column) / sizeof(count_column[0]),
                .values = count_zero,
                .row_count = count_query_row_count,
                .context = context,
            }
        );
    }

    failures += expect_statement_ok(database, "USE mysql");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM help_topic",
            .column_names = count_column,
            .column_count = sizeof(count_column) / sizeof(count_column[0]),
            .values = count_zero,
            .row_count = count_query_row_count,
            .context = "empty selected mysql.help_topic placeholder",
        }
    );
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM mysql.help_category",
            .column_names = show_columns_columns,
            .column_count = show_columns_column_count,
            .values = show_columns_help_category_values,
            .row_count = show_columns_help_category_row_count,
            .context = "SHOW COLUMNS mysql.help_category",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "DESC mysql.help_relation",
            .column_names = show_columns_columns,
            .column_count = show_columns_column_count,
            .values = describe_help_relation_values,
            .row_count = describe_help_relation_row_count,
            .context = "DESC mysql.help_relation",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW FULL COLUMNS FROM mysql.help_topic",
            .column_names = show_full_columns_columns,
            .column_count = show_full_columns_column_count,
            .values = show_full_help_topic_values,
            .row_count = show_full_help_topic_row_count,
            .context = "SHOW FULL COLUMNS mysql.help_topic",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM mysql.help_category",
            .column_names = show_index_columns,
            .column_count = show_index_column_count,
            .values = show_index_help_category_values,
            .row_count = show_index_help_category_row_count,
            .context = "SHOW INDEX mysql.help_category",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM mysql.help_relation",
            .column_names = show_index_columns,
            .column_count = show_index_column_count,
            .values = show_index_help_relation_values,
            .row_count = show_index_help_relation_row_count,
            .context = "SHOW INDEX mysql.help_relation",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, "
                   "IS_NULLABLE, DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, "
                   "CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION, NUMERIC_SCALE, "
                   "DATETIME_PRECISION, CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE, "
                   "COLUMN_KEY, EXTRA, PRIVILEGES, COLUMN_COMMENT, GENERATION_EXPRESSION "
                   "FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = 'mysql' "
                   "AND TABLE_NAME IN ('help_category','help_keyword','help_relation',"
                   "'help_topic') ORDER BY TABLE_NAME",
            .column_names = information_schema_columns_columns,
            .column_count = information_schema_columns_column_count,
            .values = information_schema_columns_values,
            .row_count = information_schema_columns_row_count,
            .context = "INFORMATION_SCHEMA.COLUMNS mysql help tables",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, INDEX_NAME, SEQ_IN_INDEX, COLUMN_NAME, CARDINALITY "
                   "FROM INFORMATION_SCHEMA.STATISTICS WHERE TABLE_SCHEMA = 'mysql' "
                   "AND TABLE_NAME IN ('help_category','help_keyword','help_relation',"
                   "'help_topic') ORDER BY TABLE_NAME",
            .column_names = statistics_columns,
            .column_count = statistics_column_count,
            .values = statistics_values,
            .row_count = statistics_row_count,
            .context = "INFORMATION_SCHEMA.STATISTICS mysql help tables",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED "
                   "FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS WHERE TABLE_SCHEMA = 'mysql' "
                   "AND TABLE_NAME IN ('help_category','help_keyword','help_relation',"
                   "'help_topic') ORDER BY TABLE_NAME",
            .column_names = table_constraints_columns,
            .column_count = table_constraints_column_count,
            .values = table_constraints_values,
            .row_count = table_constraints_row_count,
            .context = "INFORMATION_SCHEMA.TABLE_CONSTRAINTS mysql help tables",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION "
                   "FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE WHERE TABLE_SCHEMA = 'mysql' "
                   "AND TABLE_NAME IN ('help_category','help_keyword','help_relation',"
                   "'help_topic') ORDER BY TABLE_NAME",
            .column_names = key_column_usage_columns,
            .column_count = key_column_usage_column_count,
            .values = key_column_usage_values,
            .row_count = key_column_usage_row_count,
            .context = "INFORMATION_SCHEMA.KEY_COLUMN_USAGE mysql help tables",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, CONSTRAINT_NAME, ENGINE_ATTRIBUTE, "
                   "SECONDARY_ENGINE_ATTRIBUTE "
                   "FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS "
                   "WHERE CONSTRAINT_SCHEMA = 'mysql' "
                   "AND TABLE_NAME IN ('help_category','help_keyword','help_relation',"
                   "'help_topic') ORDER BY TABLE_NAME",
            .column_names = table_constraints_extensions_columns,
            .column_count = table_constraints_extensions_column_count,
            .values = table_constraints_extensions_values,
            .row_count = table_constraints_extensions_row_count,
            .context = "INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS mysql help tables",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS, "
                   "AVG_ROW_LENGTH, DATA_LENGTH, MAX_DATA_LENGTH, INDEX_LENGTH, DATA_FREE, "
                   "AUTO_INCREMENT, CREATE_TIME, UPDATE_TIME, CHECK_TIME, TABLE_COLLATION, "
                   "CHECKSUM, CREATE_OPTIONS, TABLE_COMMENT FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME IN ('help_category',"
                   "'help_keyword','help_relation','help_topic') ORDER BY TABLE_NAME",
            .column_names = information_schema_tables_columns,
            .column_count = information_schema_tables_column_count,
            .values = information_schema_tables_values,
            .row_count = information_schema_tables_row_count,
            .context = "INFORMATION_SCHEMA.TABLES mysql help tables",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW TABLE STATUS FROM mysql WHERE Name IN ('help_category','help_keyword',"
                   "'help_relation','help_topic')",
            .column_names = show_table_status_columns,
            .column_count = show_table_status_column_count,
            .values = show_table_status_values,
            .row_count = show_table_status_row_count,
            .context = "SHOW TABLE STATUS mysql help tables",
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
    failures += mylite_test_expect_size(
        mylite_result_row_count(result),
        expected.row_count,
        expected.context
    );
    failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, expected.context);
    for (size_t column_index = 0U; column_index < expected.column_count; ++column_index) {
        failures += mylite_test_expect_text_or_null(
            mylite_result_column_name(result, column_index),
            expected.column_names[column_index],
            expected.context
        );
    }
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

static void remove_related_files(const char *path) {
    remove(path);
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char buffer[test_path_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written > 0 && (size_t)written < sizeof(buffer)) {
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
        fprintf(stderr, "%s: expected datetime text, got %s\n", context, actual);
        return 1;
    }
    return 0;
}
