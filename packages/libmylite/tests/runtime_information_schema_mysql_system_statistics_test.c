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
    statistics_column_count = 18,
    mysql_system_statistics_row_count = 6,
};

struct expected_query {
    const char *sql;
    const char *const *column_names;
    size_t column_count;
    const char *const *values;
    size_t row_count;
    const char *context;
};

static int test_information_schema_mysql_system_statistics(void);
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

static const char *const statistics_columns[statistics_column_count] = {
    "TABLE_CATALOG",
    "TABLE_SCHEMA",
    "TABLE_NAME",
    "NON_UNIQUE",
    "INDEX_SCHEMA",
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

int main(void) {
    return test_information_schema_mysql_system_statistics() == 0 ? 0 : 1;
}

static int test_information_schema_mysql_system_statistics(void) {
    static const char *const statistics_values[] = {
        "def",
        "mysql",
        "innodb_index_stats",
        "0",
        "mysql",
        "PRIMARY",
        "1",
        "database_name",
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
        "def",
        "mysql",
        "innodb_index_stats",
        "0",
        "mysql",
        "PRIMARY",
        "2",
        "table_name",
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
        "def",
        "mysql",
        "innodb_index_stats",
        "0",
        "mysql",
        "PRIMARY",
        "3",
        "index_name",
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
        "def",
        "mysql",
        "innodb_index_stats",
        "0",
        "mysql",
        "PRIMARY",
        "4",
        "stat_name",
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
        "def",
        "mysql",
        "innodb_table_stats",
        "0",
        "mysql",
        "PRIMARY",
        "1",
        "database_name",
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
        "def",
        "mysql",
        "innodb_table_stats",
        "0",
        "mysql",
        "PRIMARY",
        "2",
        "table_name",
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
    };
    static const char *const tail_columns[] = {"TABLE_NAME", "COLUMN_NAME", "CARDINALITY"};
    static const char *const tail_values[] = {
        "innodb_index_stats",
        "index_name",
        "2",
        "innodb_index_stats",
        "stat_name",
        "6",
    };
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_six[] = {"6"};
    static const char *const count_zero[] = {"0"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "information-schema-mysql-system-statistics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open file");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM INFORMATION_SCHEMA.STATISTICS WHERE TABLE_SCHEMA = 'mysql' "
                   "AND TABLE_NAME IN ('innodb_table_stats', 'innodb_index_stats')",
            .column_names = statistics_columns,
            .column_count = statistics_column_count,
            .values = statistics_values,
            .row_count = mysql_system_statistics_row_count,
            .context = "mysql system statistics rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, CARDINALITY FROM "
                   "INFORMATION_SCHEMA.STATISTICS WHERE TABLE_SCHEMA = 'mysql' AND "
                   "TABLE_NAME = 'innodb_index_stats' AND SEQ_IN_INDEX >= 3 ORDER BY "
                   "SEQ_IN_INDEX",
            .column_names = tail_columns,
            .column_count = sizeof(tail_columns) / sizeof(tail_columns[0]),
            .values = tail_values,
            .row_count = 2U,
            .context = "mysql system statistics filtered tail",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS WHERE TABLE_SCHEMA = "
                   "'mysql' AND TABLE_NAME IN ('innodb_table_stats', 'innodb_index_stats')",
            .column_names = count_column,
            .column_count = sizeof(count_column) / sizeof(count_column[0]),
            .values = count_six,
            .row_count = 1U,
            .context = "mysql system statistics count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS WHERE TABLE_SCHEMA = "
                   "'mysql' AND TABLE_NAME = 'replication_group_configuration_version'",
            .column_names = count_column,
            .column_count = sizeof(count_column) / sizeof(count_column[0]),
            .values = count_zero,
            .row_count = 1U,
            .context =
                "unsupported mysql replication_group_configuration_version statistics omitted",
        }
    );
    failures += expect_row_count_status(database, "row count after mysql system statistics");

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
    failures += expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, expected.context);

    for (size_t column_index = 0U; column_index < expected.column_count; ++column_index) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, column_index),
            expected.column_names[column_index],
            expected.context
        );
    }
    for (size_t row_index = 0U; row_index < expected.row_count; ++row_index) {
        for (size_t column_index = 0U; column_index < expected.column_count; ++column_index) {
            size_t value_index = (row_index * expected.column_count) + column_index;

            failures += expect_text_or_null(
                mylite_result_value_text(result, row_index, column_index),
                expected.values[value_index],
                expected.context
            );
        }
    }

    mylite_result_free(result);
    return failures;
}

static int expect_row_count_status(mylite_db *database, const char *context) {
    static const char *const status_columns[] = {"@@warning_count", "ROW_COUNT()"};
    static const char *const status_values[] = {"0", "-1"};

    return expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count, ROW_COUNT()",
            .column_names = status_columns,
            .column_count = sizeof(status_columns) / sizeof(status_columns[0]),
            .values = status_values,
            .row_count = 1U,
            .context = context,
        }
    );
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(path, path_size, "/tmp/mylite_%s_%d.mylite", name, current_process_id());

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "failed to build test path for %s\n", name);
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
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related[test_path_capacity];
    int written = snprintf(related, sizeof(related), "%s%s", path, suffix);

    if (written > 0 && (size_t)written < sizeof(related)) {
        remove(related);
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
            "%s: expected text %s, got %s\n",
            context,
            expected == NULL ? "(null)" : expected,
            actual == NULL ? "(null)" : actual
        );
        return 1;
    }
    return 0;
}
