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
    server_cost_column_count = 5,
    engine_cost_column_count = 7,
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
static const char engine_cost_default_expression[] =
    "(case `cost_name` when _utf8mb4\\'io_block_read_cost\\' then 1.0 when "
    "_utf8mb4\\'memory_block_read_cost\\' then 0.25 else NULL end)";
static const char server_cost_default_expression[] =
    "(case `cost_name` when _utf8mb4\\'disk_temptable_create_cost\\' then 20.0 "
    "when _utf8mb4\\'disk_temptable_row_cost\\' then 0.5 when "
    "_utf8mb4\\'key_compare_cost\\' then 0.05 when "
    "_utf8mb4\\'memory_temptable_create_cost\\' then 1.0 when "
    "_utf8mb4\\'memory_temptable_row_cost\\' then 0.1 when "
    "_utf8mb4\\'row_evaluate_cost\\' then 0.1 else NULL end)";

static int test_mysql_cost_tables(void);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_row_count_status(mylite_db *database, const char *context);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int expect_datetime_text(const char *actual, const char *context);

static const char *const server_cost_columns[server_cost_column_count] = {
    "cost_name",
    "cost_value",
    "last_update",
    "comment",
    "default_value",
};

static const char *const engine_cost_columns[engine_cost_column_count] = {
    "engine_name",
    "device_type",
    "cost_name",
    "cost_value",
    "last_update",
    "comment",
    "default_value",
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
    return test_mysql_cost_tables() == 0 ? 0 : 1;
}

static int test_mysql_cost_tables(void) {
    enum {
        server_cost_row_count = 6,
        engine_cost_row_count = 2,
        table_constraints_column_count = 4,
        key_column_usage_column_count = 8,
        table_constraints_extensions_column_count = 4,
        statistics_column_count = 14,
    };

    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_two[] = {"2"};
    static const char *const count_six[] = {"6"};
    static const char *const server_cost_values[] = {
        "disk_temptable_create_cost",
        NULL,
        expected_datetime_value,
        NULL,
        "20",
        "disk_temptable_row_cost",
        NULL,
        expected_datetime_value,
        NULL,
        "0.5",
        "key_compare_cost",
        NULL,
        expected_datetime_value,
        NULL,
        "0.05",
        "memory_temptable_create_cost",
        NULL,
        expected_datetime_value,
        NULL,
        "1",
        "memory_temptable_row_cost",
        NULL,
        expected_datetime_value,
        NULL,
        "0.1",
        "row_evaluate_cost",
        NULL,
        expected_datetime_value,
        NULL,
        "0.1",
    };
    static const char *const engine_cost_values[] = {
        "default",
        "0",
        "io_block_read_cost",
        NULL,
        expected_datetime_value,
        NULL,
        "1",
        "default",
        "0",
        "memory_block_read_cost",
        NULL,
        expected_datetime_value,
        NULL,
        "0.25",
    };
    static const char *const server_cost_names[] = {
        "disk_temptable_create_cost",
        "disk_temptable_row_cost",
        "key_compare_cost",
        "memory_temptable_create_cost",
        "memory_temptable_row_cost",
        "row_evaluate_cost",
    };
    static const char *const server_show_columns_values[] = {
        "cost_name",
        "varchar(64)",
        "NO",
        "PRI",
        NULL,
        "",
        "cost_value",
        "float",
        "YES",
        "",
        NULL,
        "",
        "last_update",
        "timestamp",
        "NO",
        "",
        "CURRENT_TIMESTAMP",
        "DEFAULT_GENERATED on update CURRENT_TIMESTAMP",
        "comment",
        "varchar(1024)",
        "YES",
        "",
        NULL,
        "",
        "default_value",
        "float",
        "YES",
        "",
        NULL,
        "VIRTUAL GENERATED",
    };
    static const char *const engine_show_columns_values[] = {
        "engine_name",
        "varchar(64)",
        "NO",
        "PRI",
        NULL,
        "",
        "device_type",
        "int",
        "NO",
        "PRI",
        NULL,
        "",
        "cost_name",
        "varchar(64)",
        "NO",
        "PRI",
        NULL,
        "",
        "cost_value",
        "float",
        "YES",
        "",
        NULL,
        "",
        "last_update",
        "timestamp",
        "NO",
        "",
        "CURRENT_TIMESTAMP",
        "DEFAULT_GENERATED on update CURRENT_TIMESTAMP",
        "comment",
        "varchar(1024)",
        "YES",
        "",
        NULL,
        "",
        "default_value",
        "float",
        "YES",
        "",
        NULL,
        "VIRTUAL GENERATED",
    };
    static const char *const server_show_full_columns_values[] = {
        "cost_name",
        "varchar(64)",
        "utf8mb3_general_ci",
        "NO",
        "PRI",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "cost_value",
        "float",
        NULL,
        "YES",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "last_update",
        "timestamp",
        NULL,
        "NO",
        "",
        "CURRENT_TIMESTAMP",
        "DEFAULT_GENERATED on update CURRENT_TIMESTAMP",
        "select,insert,update,references",
        "",
        "comment",
        "varchar(1024)",
        "utf8mb3_general_ci",
        "YES",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "default_value",
        "float",
        NULL,
        "YES",
        "",
        NULL,
        "VIRTUAL GENERATED",
        "select,insert,update,references",
        "",
    };
    static const char *const engine_show_full_columns_values[] = {
        "engine_name",
        "varchar(64)",
        "utf8mb3_general_ci",
        "NO",
        "PRI",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "device_type",
        "int",
        NULL,
        "NO",
        "PRI",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "cost_name",
        "varchar(64)",
        "utf8mb3_general_ci",
        "NO",
        "PRI",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "cost_value",
        "float",
        NULL,
        "YES",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "last_update",
        "timestamp",
        NULL,
        "NO",
        "",
        "CURRENT_TIMESTAMP",
        "DEFAULT_GENERATED on update CURRENT_TIMESTAMP",
        "select,insert,update,references",
        "",
        "comment",
        "varchar(1024)",
        "utf8mb3_general_ci",
        "YES",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "default_value",
        "float",
        NULL,
        "YES",
        "",
        NULL,
        "VIRTUAL GENERATED",
        "select,insert,update,references",
        "",
    };
    static const char *const engine_show_index_values[] = {
        "engine_cost", "0", "PRIMARY", "1",   "cost_name",
        "A",           "2", NULL,      NULL,  "",
        "BTREE",       "",  "",        "YES", NULL,
        "engine_cost", "0", "PRIMARY", "2",   "engine_name",
        "A",           "2", NULL,      NULL,  "",
        "BTREE",       "",  "",        "YES", NULL,
        "engine_cost", "0", "PRIMARY", "3",   "device_type",
        "A",           "2", NULL,      NULL,  "",
        "BTREE",       "",  "",        "YES", NULL,
    };
    static const char *const server_show_index_values[] = {
        "server_cost",
        "0",
        "PRIMARY",
        "1",
        "cost_name",
        "A",
        "6",
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
        "engine_cost",
        "engine_name",
        "1",
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
        "PRI",
        "",
        "select,insert,update,references",
        "",
        "",
        "engine_cost",
        "device_type",
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
        "PRI",
        "",
        "select,insert,update,references",
        "",
        "",
        "engine_cost",
        "cost_name",
        "3",
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
        "PRI",
        "",
        "select,insert,update,references",
        "",
        "",
        "engine_cost",
        "cost_value",
        "4",
        NULL,
        "YES",
        "float",
        NULL,
        NULL,
        "12",
        NULL,
        NULL,
        NULL,
        NULL,
        "float",
        "",
        "",
        "select,insert,update,references",
        "",
        "",
        "engine_cost",
        "last_update",
        "5",
        "CURRENT_TIMESTAMP",
        "NO",
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
        "DEFAULT_GENERATED on update CURRENT_TIMESTAMP",
        "select,insert,update,references",
        "",
        "",
        "engine_cost",
        "comment",
        "6",
        NULL,
        "YES",
        "varchar",
        "1024",
        "3072",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(1024)",
        "",
        "",
        "select,insert,update,references",
        "",
        "",
        "engine_cost",
        "default_value",
        "7",
        NULL,
        "YES",
        "float",
        NULL,
        NULL,
        "12",
        NULL,
        NULL,
        NULL,
        NULL,
        "float",
        "",
        "VIRTUAL GENERATED",
        "select,insert,update,references",
        "",
        engine_cost_default_expression,
        "server_cost",
        "cost_name",
        "1",
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
        "PRI",
        "",
        "select,insert,update,references",
        "",
        "",
        "server_cost",
        "cost_value",
        "2",
        NULL,
        "YES",
        "float",
        NULL,
        NULL,
        "12",
        NULL,
        NULL,
        NULL,
        NULL,
        "float",
        "",
        "",
        "select,insert,update,references",
        "",
        "",
        "server_cost",
        "last_update",
        "3",
        "CURRENT_TIMESTAMP",
        "NO",
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
        "DEFAULT_GENERATED on update CURRENT_TIMESTAMP",
        "select,insert,update,references",
        "",
        "",
        "server_cost",
        "comment",
        "4",
        NULL,
        "YES",
        "varchar",
        "1024",
        "3072",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(1024)",
        "",
        "",
        "select,insert,update,references",
        "",
        "",
        "server_cost",
        "default_value",
        "5",
        NULL,
        "YES",
        "float",
        NULL,
        NULL,
        "12",
        NULL,
        NULL,
        NULL,
        NULL,
        "float",
        "",
        "VIRTUAL GENERATED",
        "select,insert,update,references",
        "",
        server_cost_default_expression,
    };
    static const char *const table_constraints_columns[] = {
        "TABLE_NAME",
        "CONSTRAINT_NAME",
        "CONSTRAINT_TYPE",
        "ENFORCED",
    };
    static const char *const table_constraints_values[] = {
        "engine_cost",
        "PRIMARY",
        "PRIMARY KEY",
        "YES",
        "server_cost",
        "PRIMARY",
        "PRIMARY KEY",
        "YES",
    };
    static const char *const key_column_usage_columns[] = {
        "TABLE_NAME",
        "CONSTRAINT_NAME",
        "COLUMN_NAME",
        "ORDINAL_POSITION",
        "POSITION_IN_UNIQUE_CONSTRAINT",
        "REFERENCED_TABLE_SCHEMA",
        "REFERENCED_TABLE_NAME",
        "REFERENCED_COLUMN_NAME",
    };
    static const char *const key_column_usage_values[] = {
        "engine_cost", "PRIMARY", "cost_name",   "1", NULL, NULL, NULL, NULL,
        "engine_cost", "PRIMARY", "engine_name", "2", NULL, NULL, NULL, NULL,
        "engine_cost", "PRIMARY", "device_type", "3", NULL, NULL, NULL, NULL,
        "server_cost", "PRIMARY", "cost_name",   "1", NULL, NULL, NULL, NULL,
    };
    static const char *const table_constraints_extensions_columns[] = {
        "TABLE_NAME",
        "CONSTRAINT_NAME",
        "ENGINE_ATTRIBUTE",
        "SECONDARY_ENGINE_ATTRIBUTE",
    };
    static const char *const table_constraints_extensions_values[] = {
        "engine_cost",
        "PRIMARY",
        NULL,
        NULL,
        "server_cost",
        "PRIMARY",
        NULL,
        NULL,
    };
    static const char *const statistics_columns[] = {
        "TABLE_NAME",
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
        "engine_cost",
        "PRIMARY",
        "1",
        "cost_name",
        "A",
        "2",
        NULL,
        NULL,
        "",
        "BTREE",
        "",
        "",
        "YES",
        NULL,
        "engine_cost",
        "PRIMARY",
        "2",
        "engine_name",
        "A",
        "2",
        NULL,
        NULL,
        "",
        "BTREE",
        "",
        "",
        "YES",
        NULL,
        "engine_cost",
        "PRIMARY",
        "3",
        "device_type",
        "A",
        "2",
        NULL,
        NULL,
        "",
        "BTREE",
        "",
        "",
        "YES",
        NULL,
        "server_cost",
        "PRIMARY",
        "1",
        "cost_name",
        "A",
        "6",
        NULL,
        NULL,
        "",
        "BTREE",
        "",
        "",
        "YES",
        NULL,
    };
    static const char *const information_schema_tables_values[] = {
        "engine_cost",
        "BASE TABLE",
        "InnoDB",
        "10",
        "Dynamic",
        "2",
        "8192",
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
        "",
        "server_cost",
        "BASE TABLE",
        "InnoDB",
        "10",
        "Dynamic",
        "6",
        "2730",
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
        "",
    };
    static const char *const engine_show_table_status_values[] = {
        "engine_cost",
        "InnoDB",
        "10",
        "Dynamic",
        "2",
        "8192",
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
        "",
    };
    static const char *const server_show_table_status_values[] = {
        "server_cost",
        "InnoDB",
        "10",
        "Dynamic",
        "6",
        "2730",
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
        "",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "mysql-cost-tables") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT cost_name, cost_value, last_update, comment, default_value "
                   "FROM mysql.server_cost ORDER BY cost_name",
            .column_names = server_cost_columns,
            .column_count = server_cost_column_count,
            .values = server_cost_values,
            .row_count = server_cost_row_count,
            .context = "mysql.server_cost direct rows",
        }
    );
    failures += expect_row_count_status(database, "row count after mysql.server_cost read");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT engine_name, device_type, cost_name, cost_value, last_update, "
                   "comment, default_value FROM mysql.engine_cost "
                   "ORDER BY cost_name",
            .column_names = engine_cost_columns,
            .column_count = engine_cost_column_count,
            .values = engine_cost_values,
            .row_count = engine_cost_row_count,
            .context = "mysql.engine_cost direct rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM mysql.server_cost",
            .column_names = count_column,
            .column_count = sizeof(count_column) / sizeof(count_column[0]),
            .values = count_six,
            .row_count = 1U,
            .context = "mysql.server_cost count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM mysql.engine_cost",
            .column_names = count_column,
            .column_count = sizeof(count_column) / sizeof(count_column[0]),
            .values = count_two,
            .row_count = 1U,
            .context = "mysql.engine_cost count",
        }
    );
    failures += expect_statement_ok(database, "USE mysql");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT cost_name FROM server_cost WHERE default_value IS NOT NULL "
                   "ORDER BY cost_name",
            .column_names = &server_cost_columns[0],
            .column_count = 1U,
            .values = server_cost_names,
            .row_count = server_cost_row_count,
            .context = "mysql.server_cost unqualified filtered names",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM mysql.server_cost",
            .column_names = show_columns_columns,
            .column_count = show_columns_column_count,
            .values = server_show_columns_values,
            .row_count = server_cost_column_count,
            .context = "mysql.server_cost show columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM mysql.engine_cost",
            .column_names = show_columns_columns,
            .column_count = show_columns_column_count,
            .values = engine_show_columns_values,
            .row_count = engine_cost_column_count,
            .context = "mysql.engine_cost show columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW FULL COLUMNS FROM mysql.server_cost",
            .column_names = show_full_columns_columns,
            .column_count = show_full_columns_column_count,
            .values = server_show_full_columns_values,
            .row_count = server_cost_column_count,
            .context = "mysql.server_cost show full columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW FULL COLUMNS FROM mysql.engine_cost",
            .column_names = show_full_columns_columns,
            .column_count = show_full_columns_column_count,
            .values = engine_show_full_columns_values,
            .row_count = engine_cost_column_count,
            .context = "mysql.engine_cost show full columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM mysql.engine_cost",
            .column_names = show_index_columns,
            .column_count = show_index_column_count,
            .values = engine_show_index_values,
            .row_count = 3U,
            .context = "mysql.engine_cost show index order",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM mysql.server_cost",
            .column_names = show_index_columns,
            .column_count = show_index_column_count,
            .values = server_show_index_values,
            .row_count = 1U,
            .context = "mysql.server_cost show index",
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
                   "AND TABLE_NAME IN ('server_cost', 'engine_cost') "
                   "ORDER BY TABLE_NAME",
            .column_names = information_schema_columns_columns,
            .column_count = information_schema_columns_column_count,
            .values = information_schema_columns_values,
            .row_count = server_cost_column_count + engine_cost_column_count,
            .context = "mysql cost tables information_schema.columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED "
                   "FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "
                   "WHERE TABLE_SCHEMA = 'mysql' "
                   "AND TABLE_NAME IN ('server_cost', 'engine_cost') ORDER BY TABLE_NAME",
            .column_names = table_constraints_columns,
            .column_count = table_constraints_column_count,
            .values = table_constraints_values,
            .row_count = 2U,
            .context = "mysql cost tables information_schema.table_constraints",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION, "
                   "POSITION_IN_UNIQUE_CONSTRAINT, REFERENCED_TABLE_SCHEMA, "
                   "REFERENCED_TABLE_NAME, REFERENCED_COLUMN_NAME "
                   "FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE WHERE TABLE_SCHEMA = 'mysql' "
                   "AND TABLE_NAME IN ('server_cost', 'engine_cost') "
                   "ORDER BY TABLE_NAME",
            .column_names = key_column_usage_columns,
            .column_count = key_column_usage_column_count,
            .values = key_column_usage_values,
            .row_count = 4U,
            .context = "mysql cost tables information_schema.key_column_usage",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, CONSTRAINT_NAME, ENGINE_ATTRIBUTE, "
                   "SECONDARY_ENGINE_ATTRIBUTE FROM "
                   "INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS "
                   "WHERE CONSTRAINT_SCHEMA = 'mysql' "
                   "AND TABLE_NAME IN ('server_cost', 'engine_cost') ORDER BY TABLE_NAME",
            .column_names = table_constraints_extensions_columns,
            .column_count = table_constraints_extensions_column_count,
            .values = table_constraints_extensions_values,
            .row_count = 2U,
            .context = "mysql cost tables information_schema.table_constraints_extensions",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, INDEX_NAME, SEQ_IN_INDEX, COLUMN_NAME, COLLATION, "
                   "CARDINALITY, SUB_PART, PACKED, NULLABLE, INDEX_TYPE, COMMENT, "
                   "INDEX_COMMENT, IS_VISIBLE, EXPRESSION FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'mysql' "
                   "AND TABLE_NAME IN ('server_cost', 'engine_cost') "
                   "ORDER BY TABLE_NAME",
            .column_names = statistics_columns,
            .column_count = statistics_column_count,
            .values = statistics_values,
            .row_count = 4U,
            .context = "mysql cost tables information_schema.statistics",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS, "
                   "AVG_ROW_LENGTH, DATA_LENGTH, MAX_DATA_LENGTH, INDEX_LENGTH, DATA_FREE, "
                   "AUTO_INCREMENT, CREATE_TIME, UPDATE_TIME, CHECK_TIME, TABLE_COLLATION, "
                   "CHECKSUM, CREATE_OPTIONS, TABLE_COMMENT FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'mysql' "
                   "AND TABLE_NAME IN ('server_cost', 'engine_cost') ORDER BY TABLE_NAME",
            .column_names = information_schema_tables_columns,
            .column_count = information_schema_tables_column_count,
            .values = information_schema_tables_values,
            .row_count = 2U,
            .context = "mysql cost tables information_schema.tables",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW TABLE STATUS FROM mysql LIKE 'engine_cost'",
            .column_names = show_table_status_columns,
            .column_count = show_table_status_column_count,
            .values = engine_show_table_status_values,
            .row_count = 1U,
            .context = "mysql.engine_cost show table status",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW TABLE STATUS FROM mysql LIKE 'server_cost'",
            .column_names = show_table_status_columns,
            .column_count = show_table_status_column_count,
            .values = server_show_table_status_values,
            .row_count = 1U,
            .context = "mysql.server_cost show table status",
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
