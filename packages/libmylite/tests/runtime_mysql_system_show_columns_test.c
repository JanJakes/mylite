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
    show_columns_column_count = 6,
    show_full_columns_column_count = 9,
    mysql_innodb_table_stats_show_row_count = 6,
    mysql_innodb_index_stats_show_row_count = 8,
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

static int test_mysql_system_show_columns(void);
static int expect_statement_ok(mylite_db *database, struct expected_statement expected);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_error_status(mylite_db *database, const char *context);
static int expect_row_count_status(mylite_db *database, const char *context);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text_or_null(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *text, const char *needle, const char *context);

static const char *const show_columns_names[show_columns_column_count] = {
    "Field",
    "Type",
    "Null",
    "Key",
    "Default",
    "Extra",
};

static const char *const show_full_columns_names[show_full_columns_column_count] = {
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

int main(void) {
    return test_mysql_system_show_columns() == 0 ? 0 : 1;
}

static int test_mysql_system_show_columns(void) {
    static const char *const table_columns_values[] = {
        "database_name",
        "varchar(64)",
        "NO",
        "PRI",
        NULL,
        "",
        "table_name",
        "varchar(199)",
        "NO",
        "PRI",
        NULL,
        "",
        "last_update",
        "timestamp",
        "NO",
        "",
        "CURRENT_TIMESTAMP",
        "DEFAULT_GENERATED on update CURRENT_TIMESTAMP",
        "n_rows",
        "bigint unsigned",
        "NO",
        "",
        NULL,
        "",
        "clustered_index_size",
        "bigint unsigned",
        "NO",
        "",
        NULL,
        "",
        "sum_of_other_index_sizes",
        "bigint unsigned",
        "NO",
        "",
        NULL,
        "",
    };
    static const char *const index_full_values[] = {
        "database_name",
        "varchar(64)",
        "utf8mb3_bin",
        "NO",
        "PRI",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "table_name",
        "varchar(199)",
        "utf8mb3_bin",
        "NO",
        "PRI",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "index_name",
        "varchar(64)",
        "utf8mb3_bin",
        "NO",
        "PRI",
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
        "stat_name",
        "varchar(64)",
        "utf8mb3_bin",
        "NO",
        "PRI",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "stat_value",
        "bigint unsigned",
        NULL,
        "NO",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "sample_size",
        "bigint unsigned",
        NULL,
        "YES",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "stat_description",
        "varchar(1024)",
        "utf8mb3_bin",
        "NO",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
    };
    static const char *const selected_like_values[] = {
        "n_rows",
        "bigint unsigned",
        "NO",
        "",
        NULL,
        "",
    };
    static const char *const where_values[] = {
        "stat_name",
        "varchar(64)",
        "NO",
        "PRI",
        NULL,
        "",
    };
    static const char *const full_where_values[] = {
        "database_name",
        "varchar(64)",
        "utf8mb3_bin",
        "NO",
        "PRI",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "table_name",
        "varchar(199)",
        "utf8mb3_bin",
        "NO",
        "PRI",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "index_name",
        "varchar(64)",
        "utf8mb3_bin",
        "NO",
        "PRI",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "stat_name",
        "varchar(64)",
        "utf8mb3_bin",
        "NO",
        "PRI",
        NULL,
        "",
        "select,insert,update,references",
        "",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "mysql-system-show-columns") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open file");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM mysql.innodb_table_stats",
            .column_names = show_columns_names,
            .column_count = show_columns_column_count,
            .values = table_columns_values,
            .row_count = mysql_innodb_table_stats_show_row_count,
            .context = "qualified mysql.innodb_table_stats columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "DESCRIBE mysql.innodb_table_stats",
            .column_names = show_columns_names,
            .column_count = show_columns_column_count,
            .values = table_columns_values,
            .row_count = mysql_innodb_table_stats_show_row_count,
            .context = "describe mysql.innodb_table_stats",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS IN innodb_table_stats IN mysql",
            .column_names = show_columns_names,
            .column_count = show_columns_column_count,
            .values = table_columns_values,
            .row_count = mysql_innodb_table_stats_show_row_count,
            .context = "explicit mysql schema innodb_table_stats columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW FULL COLUMNS FROM mysql.innodb_index_stats",
            .column_names = show_full_columns_names,
            .column_count = show_full_columns_column_count,
            .values = index_full_values,
            .row_count = mysql_innodb_index_stats_show_row_count,
            .context = "qualified mysql.innodb_index_stats full columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW FIELDS FROM mysql.innodb_index_stats WHERE Field = 'stat_name'",
            .column_names = show_columns_names,
            .column_count = show_columns_column_count,
            .values = where_values,
            .row_count = 1U,
            .context = "mysql.innodb_index_stats fields where",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW FULL COLUMNS FROM mysql.innodb_index_stats "
                   "WHERE Collation = 'utf8mb3_bin' AND Field LIKE '%name'",
            .column_names = show_full_columns_names,
            .column_count = show_full_columns_column_count,
            .values = full_where_values,
            .row_count = 4U,
            .context = "mysql.innodb_index_stats full columns where",
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){.sql = "USE mysql", .context = "use mysql"}
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM innodb_table_stats LIKE 'n%'",
            .column_names = show_columns_names,
            .column_count = show_columns_column_count,
            .values = selected_like_values,
            .row_count = 1U,
            .context = "selected mysql innodb_table_stats like",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "DESC innodb_table_stats",
            .column_names = show_columns_names,
            .column_count = show_columns_column_count,
            .values = table_columns_values,
            .row_count = mysql_innodb_table_stats_show_row_count,
            .context = "selected mysql desc innodb_table_stats",
        }
    );
    failures += expect_row_count_status(database, "row count after mysql system show columns");
    failures += expect_error(
        database,
        "SHOW COLUMNS FROM mysql.user",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SHOW COLUMNS supports selected mysql system tables",
        }
    );
    failures += expect_error_status(database, "status after unsupported mysql system table");
    failures += expect_error(
        database,
        "SHOW COLUMNS FROM mysql.no_such_table",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'mysql.no_such_table' doesn't exist",
        }
    );
    failures += expect_error_status(database, "status after missing mysql system table");

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

static int expect_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_ERROR) {
        fprintf(stderr, "execute '%s': expected MYLITE_ERROR, got %d\n", sql, rc);
        failures += 1;
    }
    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text_or_null(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, sql);
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

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(path, path_size, "/tmp/mylite_%s_%d.mylite", name, current_process_id());

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "failed to build test path for %s\n", name);
        return 1;
    }
    return 0;
}

static int expect_contains(const char *text, const char *needle, const char *context) {
    if (text == NULL || needle == NULL || strstr(text, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected [%s] to contain [%s]\n",
            context,
            text == NULL ? "(null)" : text,
            needle == NULL ? "(null)" : needle
        );
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
