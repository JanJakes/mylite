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

static int test_mysql_system_stats_table_status(void);
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
    return test_mysql_system_stats_table_status() == 0 ? 0 : 1;
}

static int test_mysql_system_stats_table_status(void) {
    static const char *const information_schema_tables_values[] = {
        "innodb_index_stats",
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
        expected_datetime_value,
        NULL,
        "utf8mb3_bin",
        NULL,
        "row_format=DYNAMIC stats_persistent=0",
        "",
        "innodb_table_stats",
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
        expected_datetime_value,
        NULL,
        "utf8mb3_bin",
        NULL,
        "row_format=DYNAMIC stats_persistent=0",
        "",
    };
    static const char *const show_table_status_values[] = {
        "innodb_index_stats",
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
        expected_datetime_value,
        NULL,
        "utf8mb3_bin",
        NULL,
        "row_format=DYNAMIC stats_persistent=0",
        "",
        "innodb_table_stats",
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
        expected_datetime_value,
        NULL,
        "utf8mb3_bin",
        NULL,
        "row_format=DYNAMIC stats_persistent=0",
        "",
    };
    static const char *const show_table_status_index_values[] = {
        "innodb_index_stats",
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
        expected_datetime_value,
        NULL,
        "utf8mb3_bin",
        NULL,
        "row_format=DYNAMIC stats_persistent=0",
        "",
    };
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_two[] = {"2"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "mysql-system-stats-table-status") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open file");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS, "
                   "AVG_ROW_LENGTH, DATA_LENGTH, MAX_DATA_LENGTH, INDEX_LENGTH, DATA_FREE, "
                   "AUTO_INCREMENT, CREATE_TIME, UPDATE_TIME, CHECK_TIME, TABLE_COLLATION, "
                   "CHECKSUM, CREATE_OPTIONS, TABLE_COMMENT FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME IN ('innodb_table_stats', "
                   "'innodb_index_stats') ORDER BY TABLE_NAME",
            .column_names = information_schema_tables_columns,
            .column_count = information_schema_tables_column_count,
            .values = information_schema_tables_values,
            .row_count = 2U,
            .context = "mysql system stats information_schema.tables rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW TABLE STATUS FROM mysql WHERE Name IN ('innodb_index_stats', "
                   "'innodb_table_stats')",
            .column_names = show_table_status_columns,
            .column_count = show_table_status_column_count,
            .values = show_table_status_values,
            .row_count = 2U,
            .context = "mysql system stats show table status rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW TABLE STATUS FROM mysql WHERE Name = 'innodb_index_stats' AND "
                   "Update_time IS NOT NULL AND Data_free = '4194304'",
            .column_names = show_table_status_columns,
            .column_count = show_table_status_column_count,
            .values = show_table_status_index_values,
            .row_count = 1U,
            .context = "mysql system stats show table status filtered row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = "
                   "'mysql' AND TABLE_NAME IN ('innodb_table_stats', 'innodb_index_stats') "
                   "AND UPDATE_TIME IS NOT NULL AND DATA_FREE = '4194304'",
            .column_names = count_column,
            .column_count = sizeof(count_column) / sizeof(count_column[0]),
            .values = count_two,
            .row_count = 1U,
            .context = "mysql system stats information_schema.tables filtered count",
        }
    );
    failures += expect_row_count_status(database, "row count after mysql system stats status");

    mylite_close(database);
    remove_related_files(path);

    return failures;
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
        "/tmp/mylite_runtime_mysql_system_stats_table_status_%d_%s.mylite",
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
