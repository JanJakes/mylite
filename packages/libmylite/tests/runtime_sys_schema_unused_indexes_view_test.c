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
    unused_indexes_column_count = 3,
    show_columns_column_count = 6,
    show_index_column_count = 15,
    information_schema_columns_sample_column_count = 7,
    information_schema_tables_sample_column_count = 7,
    information_schema_views_column_count = 9,
    information_schema_view_table_usage_column_count = 4,
    show_create_view_column_count = 4,
    show_table_status_column_count = 18,
    information_schema_columns_sample_row_count = 3,
    view_table_usage_row_count = 2,
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

static int test_sys_schema_unused_indexes_view(void);
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

static const char *const unused_indexes_columns[unused_indexes_column_count] = {
    "object_schema",
    "object_name",
    "index_name",
};

static const char *const show_columns_columns[show_columns_column_count] = {
    "Field",
    "Type",
    "Null",
    "Key",
    "Default",
    "Extra",
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
    information_schema_columns_sample_columns[information_schema_columns_sample_column_count] = {
        "TABLE_NAME",
        "COLUMN_NAME",
        "ORDINAL_POSITION",
        "IS_NULLABLE",
        "COLUMN_TYPE",
        "CHARACTER_SET_NAME",
        "COLLATION_NAME",
};

static const char *const
    information_schema_tables_sample_columns[information_schema_tables_sample_column_count] = {
        "TABLE_SCHEMA",
        "TABLE_NAME",
        "TABLE_TYPE",
        "ENGINE",
        "TABLE_ROWS",
        "DATA_LENGTH",
        "TABLE_COMMENT",
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
    return test_sys_schema_unused_indexes_view() == 0 ? 0 : 1;
}

static int test_sys_schema_unused_indexes_view(void) {
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_zero[] = {"0"};
    static const char *const count_three[] = {"3"};
    static const char *const row_count_column[] = {"ROW_COUNT()"};
    static const char *const row_count_minus_one[] = {"-1"};
    static const char *const unused_index_values[] = {
        "app",
        "idx_target",
        "body_ft",
        "app",
        "idx_target",
        "composite_idx",
        "app",
        "idx_target",
        "value_idx",
    };
    static const char *const show_columns_values[] = {
        "object_schema",
        "varchar(64)",
        "YES",
        "",
        NULL,
        "",
        "object_name",
        "varchar(64)",
        "YES",
        "",
        NULL,
        "",
        "index_name",
        "varchar(64)",
        "YES",
        "",
        NULL,
        "",
    };
    static const char *const information_schema_columns_sample_values[] = {
        "schema_unused_indexes",
        "object_schema",
        "1",
        "YES",
        "varchar(64)",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "schema_unused_indexes",
        "object_name",
        "2",
        "YES",
        "varchar(64)",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "schema_unused_indexes",
        "index_name",
        "3",
        "YES",
        "varchar(64)",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
    };
    static const char *const information_schema_tables_sample_values[] = {
        "sys",
        "schema_unused_indexes",
        "VIEW",
        NULL,
        NULL,
        NULL,
        "VIEW",
    };
    static const char *const show_table_status_values[] = {
        "schema_unused_indexes",
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
    static const char *const information_schema_views_values[] = {
        "def",
        "sys",
        "schema_unused_indexes",
        "NONE",
        "YES",
        "mysql.sys@localhost",
        "INVOKER",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
    };
    static const char *const view_table_usage_values[] = {
        "sys",
        "schema_unused_indexes",
        "information_schema",
        "STATISTICS",
        "sys",
        "schema_unused_indexes",
        "performance_schema",
        "table_io_waits_summary_by_index_usage",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "main") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open file-backed database");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE idx_target (id INT PRIMARY KEY, value_col INT, value_col_2 INT, "
        "filler VARCHAR(20), body TEXT, KEY value_idx (value_col), "
        "UNIQUE KEY filler_unique (filler), KEY composite_idx (value_col, value_col_2), "
        "FULLTEXT KEY body_ft (body))"
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT object_schema, object_name, index_name "
                   "FROM sys.schema_unused_indexes WHERE object_schema = 'app' "
                   "ORDER BY index_name",
            .column_names = unused_indexes_columns,
            .column_count = unused_indexes_column_count,
            .values = unused_index_values,
            .row_count = 3U,
            .context = "sys.schema_unused_indexes app rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM sys.schema_unused_indexes "
                   "WHERE object_schema = 'app'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_three,
            .row_count = 1U,
            .context = "sys.schema_unused_indexes app row count",
        }
    );
    failures += expect_statement_ok(database, "SELECT * FROM idx_target WHERE value_col = 42");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM sys.schema_unused_indexes "
                   "WHERE object_schema = 'app'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_three,
            .row_count = 1U,
            .context = "sys.schema_unused_indexes count after index read",
        }
    );
    failures += expect_statement_ok(database, "USE sys");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT object_schema, object_name, index_name FROM schema_unused_indexes "
                   "WHERE object_schema = 'app' ORDER BY index_name",
            .column_names = unused_indexes_columns,
            .column_count = unused_indexes_column_count,
            .values = unused_index_values,
            .row_count = 3U,
            .context = "sys.schema_unused_indexes selected schema rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM schema_unused_indexes",
            .column_names = show_columns_columns,
            .column_count = show_columns_column_count,
            .values = show_columns_values,
            .row_count = unused_indexes_column_count,
            .context = "sys.schema_unused_indexes show columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "DESCRIBE schema_unused_indexes",
            .column_names = show_columns_columns,
            .column_count = show_columns_column_count,
            .values = show_columns_values,
            .row_count = unused_indexes_column_count,
            .context = "sys.schema_unused_indexes describe",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM schema_unused_indexes",
            .column_names = show_index_columns,
            .column_count = show_index_column_count,
            .values = NULL,
            .row_count = 0U,
            .context = "sys.schema_unused_indexes show index",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, "
                   "COLUMN_TYPE, CHARACTER_SET_NAME, COLLATION_NAME "
                   "FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = 'sys' "
                   "AND TABLE_NAME = 'schema_unused_indexes' ORDER BY ORDINAL_POSITION",
            .column_names = information_schema_columns_sample_columns,
            .column_count = information_schema_columns_sample_column_count,
            .values = information_schema_columns_sample_values,
            .row_count = information_schema_columns_sample_row_count,
            .context = "sys.schema_unused_indexes information_schema.columns rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, TABLE_ROWS, "
                   "DATA_LENGTH, TABLE_COMMENT FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'schema_unused_indexes'",
            .column_names = information_schema_tables_sample_columns,
            .column_count = information_schema_tables_sample_column_count,
            .values = information_schema_tables_sample_values,
            .row_count = 1U,
            .context = "sys.schema_unused_indexes information_schema.tables row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW TABLE STATUS FROM sys LIKE 'schema_unused_indexes'",
            .column_names = show_table_status_columns,
            .column_count = show_table_status_column_count,
            .values = show_table_status_values,
            .row_count = 1U,
            .context = "sys.schema_unused_indexes show table status",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_CATALOG, TABLE_SCHEMA, TABLE_NAME, CHECK_OPTION, "
                   "IS_UPDATABLE, DEFINER, SECURITY_TYPE, CHARACTER_SET_CLIENT, "
                   "COLLATION_CONNECTION FROM INFORMATION_SCHEMA.VIEWS "
                   "WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'schema_unused_indexes'",
            .column_names = information_schema_views_columns,
            .column_count = information_schema_views_column_count,
            .values = information_schema_views_values,
            .row_count = 1U,
            .context = "sys.schema_unused_indexes information_schema.views row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT VIEW_SCHEMA, VIEW_NAME, TABLE_SCHEMA, TABLE_NAME "
                   "FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE "
                   "WHERE VIEW_SCHEMA = 'sys' AND VIEW_NAME = 'schema_unused_indexes'",
            .column_names = information_schema_view_table_usage_columns,
            .column_count = information_schema_view_table_usage_column_count,
            .values = view_table_usage_values,
            .row_count = view_table_usage_row_count,
            .context = "sys.schema_unused_indexes view_table_usage rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE "
                   "WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'schema_unused_indexes'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "sys.schema_unused_indexes empty view_routine_usage",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'schema_unused_indexes'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "sys.schema_unused_indexes empty statistics",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "
                   "WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'schema_unused_indexes'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "sys.schema_unused_indexes empty table_constraints",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE "
                   "WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'schema_unused_indexes'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "sys.schema_unused_indexes empty key_column_usage",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS "
                   "WHERE CONSTRAINT_SCHEMA = 'sys' AND TABLE_NAME = 'schema_unused_indexes'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "sys.schema_unused_indexes empty table_constraints_extensions",
        }
    );
    failures += expect_query_contains(
        database,
        (struct expected_query_contains){
            .sql = "SHOW CREATE VIEW schema_unused_indexes",
            .column_names = show_create_view_columns,
            .column_count = show_create_view_column_count,
            .row_count = 1U,
            .row_index = 0U,
            .column_index = 1U,
            .needle = "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY "
                      "INVOKER VIEW `schema_unused_indexes`",
            .context = "sys.schema_unused_indexes show create view",
        }
    );
    failures += expect_query_contains(
        database,
        (struct expected_query_contains){
            .sql = "SHOW CREATE VIEW schema_unused_indexes",
            .column_names = show_create_view_columns,
            .column_count = show_create_view_column_count,
            .row_count = 1U,
            .row_index = 0U,
            .column_index = 1U,
            .needle = "(`t`.`COUNT_STAR` = 0)",
            .context = "sys.schema_unused_indexes show create count filter",
        }
    );
    failures += expect_query_contains(
        database,
        (struct expected_query_contains){
            .sql = "SHOW CREATE TABLE schema_unused_indexes",
            .column_names = show_create_view_columns,
            .column_count = show_create_view_column_count,
            .row_count = 1U,
            .row_index = 0U,
            .column_index = 1U,
            .needle = "`information_schema`.`s`.`NON_UNIQUE` = 1",
            .context = "sys.schema_unused_indexes show create nonunique filter",
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
            .context = "sys.schema_unused_indexes row_count",
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
        "/tmp/mylite_runtime_sys_schema_unused_indexes_view_%d_%s.mylite",
        current_process_id(),
        name
    );

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "test path buffer too small\n");
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
