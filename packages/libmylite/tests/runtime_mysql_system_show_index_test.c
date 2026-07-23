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
    mysql_error_parse = 1064,
    mysql_error_table_does_not_exist = 1146,
    show_index_column_count = 15,
    mysql_innodb_table_stats_show_index_row_count = 2,
    mysql_innodb_index_stats_show_index_row_count = 4,
};

struct expected_query {
    const char *sql;
    const char *const *column_names;
    size_t column_count;
    const char *const *values;
    size_t row_count;
    const char *context;
};

struct expected_statement {
    const char *sql;
    const char *context;
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

static int test_mysql_system_show_index(void);
static int expect_statement_ok(mylite_db *database, struct expected_statement expected);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_error_status(mylite_db *database, const char *context);
static int expect_row_count_status(mylite_db *database, const char *context);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static const char *const show_index_names[show_index_column_count] = {
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

int main(void) {
    return test_mysql_system_show_index() == 0 ? 0 : 1;
}

static int test_mysql_system_show_index(void) {
    static const char *const table_index_values[] = {
        "innodb_table_stats",
        "0",
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
        "innodb_table_stats",
        "0",
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
    static const char *const index_values[] = {
        "innodb_index_stats",
        "0",
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
        "innodb_index_stats",
        "0",
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
        "innodb_index_stats",
        "0",
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
        "innodb_index_stats",
        "0",
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
    };
    static const char *const index_tail_values[] = {
        "innodb_index_stats",
        "0",
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
        "innodb_index_stats",
        "0",
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
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "mysql-system-show-index") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open file");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM mysql.innodb_table_stats",
            .column_names = show_index_names,
            .column_count = show_index_column_count,
            .values = table_index_values,
            .row_count = mysql_innodb_table_stats_show_index_row_count,
            .context = "qualified mysql.innodb_table_stats index",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW INDEXES FROM mysql.innodb_index_stats",
            .column_names = show_index_names,
            .column_count = show_index_column_count,
            .values = index_values,
            .row_count = mysql_innodb_index_stats_show_index_row_count,
            .context = "qualified mysql.innodb_index_stats indexes",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW INDEXES IN innodb_index_stats IN mysql WHERE Seq_in_index >= '3'",
            .column_names = show_index_names,
            .column_count = show_index_column_count,
            .values = index_tail_values,
            .row_count = 2U,
            .context = "explicit mysql schema innodb_index_stats index where",
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){.sql = "USE mysql", .context = "use mysql"}
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW KEYS FROM innodb_table_stats WHERE Key_name = 'PRIMARY'",
            .column_names = show_index_names,
            .column_count = show_index_column_count,
            .values = table_index_values,
            .row_count = mysql_innodb_table_stats_show_index_row_count,
            .context = "selected mysql innodb_table_stats keys",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM innodb_index_stats WHERE Column_name LIKE '%name'",
            .column_names = show_index_names,
            .column_count = show_index_column_count,
            .values = index_values,
            .row_count = mysql_innodb_index_stats_show_index_row_count,
            .context = "selected mysql innodb_index_stats column-name where",
        }
    );
    failures += expect_row_count_status(database, "row count after mysql system show index");
    failures += expect_error(
        database,
        "SHOW INDEX FROM mysql.replication_group_configuration_version",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SHOW INDEX supports selected mysql system tables",
        }
    );
    failures += expect_error_status(
        database,
        "status after unsupported mysql replication_group_configuration_version index"
    );
    failures += expect_error(
        database,
        "SHOW INDEX FROM mysql.no_such_table",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'mysql.no_such_table' doesn't exist",
        }
    );
    failures += expect_error_status(database, "status after missing mysql system index");

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int expect_statement_ok(mylite_db *database, struct expected_statement expected) {
    mylite_result *result = NULL;
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
    for (size_t row_index = 0U; row_index < expected.row_count; ++row_index) {
        for (size_t column_index = 0U; column_index < expected.column_count; ++column_index) {
            size_t value_index = (row_index * expected.column_count) + column_index;

            failures += mylite_test_expect_text_or_null(
                mylite_result_value_text(result, row_index, column_index),
                expected.values[value_index],
                expected.context
            );
        }
    }

    mylite_result_free(result);
    return failures;
}

static int expect_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_ERROR) {
        fprintf(stderr, "execute '%s': expected MYLITE_ERROR, got %d\n", sql, rc);
        failures += 1;
    }
    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, sql);
    failures += mylite_test_expect_text_or_null(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += mylite_test_expect_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);

    return failures;
}

static int expect_error_status(mylite_db *database, const char *context) {
    static const char *const status_columns[] = {"@@warning_count", "ROW_COUNT()"};
    static const char *const status_values[] = {"1", "-1"};

    return expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count, ROW_COUNT()",
            .column_names = status_columns,
            .column_count = 2U,
            .values = status_values,
            .row_count = 1U,
            .context = context,
        }
    );
}

static int expect_row_count_status(mylite_db *database, const char *context) {
    static const char *const status_columns[] = {"@@warning_count", "ROW_COUNT()"};
    static const char *const status_values[] = {"0", "-1"};

    return expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count, ROW_COUNT()",
            .column_names = status_columns,
            .column_count = 2U,
            .values = status_values,
            .row_count = 1U,
            .context = context,
        }
    );
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
