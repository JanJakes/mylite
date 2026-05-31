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
    mysql_role_edges_column_count = 5,
    show_columns_column_count = 6,
    show_full_columns_column_count = 9,
    show_index_column_count = 15,
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

static int test_mysql_role_edges_table(void);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_query(mylite_db *database, struct expected_query expected);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text_or_null(const char *actual, const char *expected, const char *context);
static int expect_datetime_text(const char *actual, const char *context);

static const char *const mysql_role_edges_columns[mysql_role_edges_column_count] = {
    "FROM_HOST",
    "FROM_USER",
    "TO_HOST",
    "TO_USER",
    "WITH_ADMIN_OPTION",
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
    return test_mysql_role_edges_table() == 0 ? 0 : 1;
}

static int test_mysql_role_edges_table(void) {
    enum {
        show_index_row_count = 4,
        information_schema_columns_sample_column_count = 7,
        information_schema_table_constraints_column_count = 3,
        information_schema_key_column_usage_column_count = 7,
        information_schema_key_column_usage_row_count = 4,
        information_schema_table_constraints_extensions_column_count = 4,
        information_schema_statistics_column_count = 14,
        information_schema_columns_sample_row_count = 5,
    };

    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_zero[] = {"0"};
    static const char *const count_five[] = {"5"};
    static const char *const show_columns_with_admin_option_values[] = {
        "WITH_ADMIN_OPTION",
        "enum('N','Y')",
        "NO",
        "",
        "N",
        "",
    };
    static const char *const show_full_columns_with_admin_option_values[] = {
        "WITH_ADMIN_OPTION",
        "enum('N','Y')",
        "utf8mb3_general_ci",
        "NO",
        "",
        "N",
        "",
        "select,insert,update,references",
        "",
    };
    static const char *const show_index_values[] = {
        "role_edges", "0",          "PRIMARY",    "1",          "FROM_HOST", "A",         "0",
        NULL,         NULL,         "",           "BTREE",      "",          "",          "YES",
        NULL,         "role_edges", "0",          "PRIMARY",    "2",         "FROM_USER", "A",
        "0",          NULL,         NULL,         "",           "BTREE",     "",          "",
        "YES",        NULL,         "role_edges", "0",          "PRIMARY",   "3",         "TO_HOST",
        "A",          "0",          NULL,         NULL,         "",          "BTREE",     "",
        "",           "YES",        NULL,         "role_edges", "0",         "PRIMARY",   "4",
        "TO_USER",    "A",          "0",          NULL,         NULL,        "",          "BTREE",
        "",           "",           "YES",        NULL,
    };
    static const char *const information_schema_columns_sample_columns[] = {
        "COLUMN_NAME",
        "ORDINAL_POSITION",
        "COLUMN_DEFAULT",
        "COLUMN_KEY",
        "EXTRA",
        "COLLATION_NAME",
        "COLUMN_TYPE",
    };
    static const char *const information_schema_columns_sample_values[] = {
        "FROM_HOST",         "1", "",  "PRI", "", "ascii_general_ci",   "char(255)",
        "FROM_USER",         "2", "",  "PRI", "", "utf8mb3_bin",        "char(32)",
        "TO_HOST",           "3", "",  "PRI", "", "ascii_general_ci",   "char(255)",
        "TO_USER",           "4", "",  "PRI", "", "utf8mb3_bin",        "char(32)",
        "WITH_ADMIN_OPTION", "5", "N", "",    "", "utf8mb3_general_ci", "enum('N','Y')",
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
        "PRIMARY", "FROM_HOST", "1", NULL, NULL, NULL, NULL,
        "PRIMARY", "FROM_USER", "2", NULL, NULL, NULL, NULL,
        "PRIMARY", "TO_HOST",   "3", NULL, NULL, NULL, NULL,
        "PRIMARY", "TO_USER",   "4", NULL, NULL, NULL, NULL,
    };
    static const char *const table_constraints_extensions_columns[] = {
        "CONSTRAINT_NAME",
        "TABLE_NAME",
        "ENGINE_ATTRIBUTE",
        "SECONDARY_ENGINE_ATTRIBUTE",
    };
    static const char *const table_constraints_extensions_values[] = {
        "PRIMARY",
        "role_edges",
        NULL,
        NULL,
    };
    static const char *const statistics_columns[] = {
        "NON_UNIQUE",
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
        "0", "PRIMARY", "1", "FROM_HOST", "A", "0", NULL, NULL, "", "BTREE", "", "", "YES", NULL,
        "0", "PRIMARY", "2", "FROM_USER", "A", "0", NULL, NULL, "", "BTREE", "", "", "YES", NULL,
        "0", "PRIMARY", "3", "TO_HOST",   "A", "0", NULL, NULL, "", "BTREE", "", "", "YES", NULL,
        "0", "PRIMARY", "4", "TO_USER",   "A", "0", NULL, NULL, "", "BTREE", "", "", "YES", NULL,
    };
    static const char *const information_schema_tables_values[] = {
        "role_edges",
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
        "utf8mb3_bin",
        NULL,
        "row_format=DYNAMIC stats_persistent=0",
        "Role hierarchy and role grants",
    };
    static const char *const show_table_status_values[] = {
        "role_edges",
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
        "utf8mb3_bin",
        NULL,
        "row_format=DYNAMIC stats_persistent=0",
        "Role hierarchy and role grants",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "mysql-role-edges-table") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM mysql.role_edges",
            .column_names = mysql_role_edges_columns,
            .column_count = mysql_role_edges_column_count,
            .values = NULL,
            .row_count = 0U,
            .context = "mysql.role_edges direct empty read",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM mysql.role_edges",
            .column_names = count_column,
            .column_count = sizeof(count_column) / sizeof(count_column[0]),
            .values = count_zero,
            .row_count = 1U,
            .context = "mysql.role_edges count",
        }
    );
    failures += expect_statement_ok(database, "USE mysql");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM role_edges",
            .column_names = count_column,
            .column_count = sizeof(count_column) / sizeof(count_column[0]),
            .values = count_zero,
            .row_count = 1U,
            .context = "selected mysql.role_edges count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "DESC mysql.role_edges",
            .column_names = show_columns_columns,
            .column_count = show_columns_column_count,
            .values = NULL,
            .row_count = mysql_role_edges_column_count,
            .context = "mysql.role_edges describe",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM mysql.role_edges LIKE 'WITH_ADMIN_OPTION'",
            .column_names = show_columns_columns,
            .column_count = show_columns_column_count,
            .values = show_columns_with_admin_option_values,
            .row_count = 1U,
            .context = "mysql.role_edges show columns with admin option",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW FULL COLUMNS FROM mysql.role_edges WHERE Field = 'WITH_ADMIN_OPTION'",
            .column_names = show_full_columns_columns,
            .column_count = show_full_columns_column_count,
            .values = show_full_columns_with_admin_option_values,
            .row_count = 1U,
            .context = "mysql.role_edges show full columns with admin option",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM mysql.role_edges",
            .column_names = show_index_columns,
            .column_count = show_index_column_count,
            .values = show_index_values,
            .row_count = show_index_row_count,
            .context = "mysql.role_edges show index",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'role_edges'",
            .column_names = count_column,
            .column_count = sizeof(count_column) / sizeof(count_column[0]),
            .values = count_five,
            .row_count = 1U,
            .context = "mysql.role_edges information_schema.columns count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, COLUMN_KEY, EXTRA, "
                   "COLLATION_NAME, COLUMN_TYPE FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'role_edges' "
                   "ORDER BY ORDINAL_POSITION",
            .column_names = information_schema_columns_sample_columns,
            .column_count = information_schema_columns_sample_column_count,
            .values = information_schema_columns_sample_values,
            .row_count = information_schema_columns_sample_row_count,
            .context = "mysql.role_edges information_schema.columns sample",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED "
                   "FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "
                   "WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'role_edges'",
            .column_names = table_constraints_columns,
            .column_count = information_schema_table_constraints_column_count,
            .values = table_constraints_values,
            .row_count = 1U,
            .context = "mysql.role_edges information_schema.table_constraints",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION, "
                   "POSITION_IN_UNIQUE_CONSTRAINT, REFERENCED_TABLE_SCHEMA, "
                   "REFERENCED_TABLE_NAME, REFERENCED_COLUMN_NAME "
                   "FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE "
                   "WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'role_edges' "
                   "ORDER BY ORDINAL_POSITION",
            .column_names = key_column_usage_columns,
            .column_count = information_schema_key_column_usage_column_count,
            .values = key_column_usage_values,
            .row_count = information_schema_key_column_usage_row_count,
            .context = "mysql.role_edges information_schema.key_column_usage",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONSTRAINT_NAME, TABLE_NAME, ENGINE_ATTRIBUTE, "
                   "SECONDARY_ENGINE_ATTRIBUTE FROM "
                   "INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS "
                   "WHERE CONSTRAINT_SCHEMA = 'mysql' AND TABLE_NAME = 'role_edges'",
            .column_names = table_constraints_extensions_columns,
            .column_count = information_schema_table_constraints_extensions_column_count,
            .values = table_constraints_extensions_values,
            .row_count = 1U,
            .context = "mysql.role_edges information_schema.table_constraints_extensions",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT NON_UNIQUE, INDEX_NAME, SEQ_IN_INDEX, COLUMN_NAME, COLLATION, "
                   "CARDINALITY, SUB_PART, PACKED, NULLABLE, INDEX_TYPE, COMMENT, "
                   "INDEX_COMMENT, IS_VISIBLE, EXPRESSION FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'role_edges'",
            .column_names = statistics_columns,
            .column_count = information_schema_statistics_column_count,
            .values = statistics_values,
            .row_count = show_index_row_count,
            .context = "mysql.role_edges information_schema.statistics",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS, "
                   "AVG_ROW_LENGTH, DATA_LENGTH, MAX_DATA_LENGTH, INDEX_LENGTH, DATA_FREE, "
                   "AUTO_INCREMENT, CREATE_TIME, UPDATE_TIME, CHECK_TIME, TABLE_COLLATION, "
                   "CHECKSUM, CREATE_OPTIONS, TABLE_COMMENT FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'role_edges'",
            .column_names = information_schema_tables_columns,
            .column_count = information_schema_tables_column_count,
            .values = information_schema_tables_values,
            .row_count = 1U,
            .context = "mysql.role_edges information_schema.tables",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW TABLE STATUS FROM mysql WHERE Name = 'role_edges'",
            .column_names = show_table_status_columns,
            .column_count = show_table_status_column_count,
            .values = show_table_status_values,
            .row_count = 1U,
            .context = "mysql.role_edges show table status",
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

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_runtime_mysql_role_edges_table_%d_%s.mylite",
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
