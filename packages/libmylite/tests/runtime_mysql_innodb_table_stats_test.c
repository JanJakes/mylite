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
    mysql_error_unknown_column = 1054,
    mysql_error_system_schema_access = 3552,
    mysql_innodb_table_stats_column_count = 6,
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

static int test_mysql_innodb_table_stats_rows(void);
static int setup_stats_schema(mylite_db *database);
static int expect_statement_ok(mylite_db *database, struct expected_statement expected);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_row_count_status(mylite_db *database, const char *context);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);

int main(void) {
    return test_mysql_innodb_table_stats_rows() == 0 ? 0 : 1;
}

static int test_mysql_innodb_table_stats_rows(void) {
    static const char *const stats_columns[] = {
        "database_name",
        "table_name",
        "n_rows",
        "clustered_index_size",
        "sum_of_other_index_sizes",
    };
    static const char *const stats_values[] = {
        "app",
        "t_plain",
        "2",
        "1",
        "0",
        "app",
        "t_primary",
        "3",
        "1",
        "2",
    };
    static const char *const builtin_values[] = {
        "mysql",
        "component",
        "0",
        "1",
        "0",
        "sys",
        "sys_config",
        "6",
        "1",
        "0",
    };
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_two[] = {"2"};
    static const char *const count_one[] = {"1"};
    static const char *const count_zero[] = {"0"};
    static const char *const alias_columns[] = {"table_name", "sum_of_other_index_sizes"};
    static const char *const alias_values[] = {"t_primary", "2"};
    static const char *const updated_row_values[] = {"app", "t_plain", "3", "1", "0"};
    static const char *const columns_metadata_columns[] = {
        "TABLE_SCHEMA",
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
    };
    static const char *const columns_metadata_values[] = {
        "mysql",
        "innodb_table_stats",
        "database_name",
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
        "utf8mb3_bin",
        "varchar(64)",
        "PRI",
        "",
        "select,insert,update,references",
        "mysql",
        "innodb_table_stats",
        "table_name",
        "2",
        NULL,
        "NO",
        "varchar",
        "199",
        "597",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_bin",
        "varchar(199)",
        "PRI",
        "",
        "select,insert,update,references",
        "mysql",
        "innodb_table_stats",
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
        "mysql",
        "innodb_table_stats",
        "n_rows",
        "4",
        NULL,
        "NO",
        "bigint",
        NULL,
        NULL,
        "20",
        "0",
        NULL,
        NULL,
        NULL,
        "bigint unsigned",
        "",
        "",
        "select,insert,update,references",
        "mysql",
        "innodb_table_stats",
        "clustered_index_size",
        "5",
        NULL,
        "NO",
        "bigint",
        NULL,
        NULL,
        "20",
        "0",
        NULL,
        NULL,
        NULL,
        "bigint unsigned",
        "",
        "",
        "select,insert,update,references",
        "mysql",
        "innodb_table_stats",
        "sum_of_other_index_sizes",
        "6",
        NULL,
        "NO",
        "bigint",
        NULL,
        NULL,
        "20",
        "0",
        NULL,
        NULL,
        NULL,
        "bigint unsigned",
        "",
        "",
        "select,insert,update,references",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "mysql-innodb-table-stats") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open file");
    failures += setup_stats_schema(database);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT database_name, table_name, n_rows, clustered_index_size, "
                   "sum_of_other_index_sizes "
                   "FROM mysql.innodb_table_stats "
                   "WHERE database_name = 'app' "
                   "ORDER BY table_name",
            .column_names = stats_columns,
            .column_count = sizeof(stats_columns) / sizeof(stats_columns[0]),
            .values = stats_values,
            .row_count = 2U,
            .context = "descriptor stats rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT database_name, table_name, n_rows, clustered_index_size, "
                   "sum_of_other_index_sizes "
                   "FROM mysql.innodb_table_stats "
                   "WHERE database_name IN ('mysql','sys') "
                   "ORDER BY database_name",
            .column_names = stats_columns,
            .column_count = sizeof(stats_columns) / sizeof(stats_columns[0]),
            .values = builtin_values,
            .row_count = 2U,
            .context = "built-in stats rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM mysql.innodb_table_stats "
                   "WHERE database_name = 'app' AND last_update IS NOT NULL",
            .column_names = count_column,
            .column_count = sizeof(count_column) / sizeof(count_column[0]),
            .values = count_two,
            .row_count = 1U,
            .context = "last update not null",
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "SET time_zone = '+00:00'",
            .context = "set UTC time zone",
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "SET timestamp = 1700000000",
            .context = "set timestamp override",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM mysql.innodb_table_stats "
                   "WHERE database_name = 'app' "
                   "AND last_update = '2023-11-14 22:13:20'",
            .column_names = count_column,
            .column_count = sizeof(count_column) / sizeof(count_column[0]),
            .values = count_zero,
            .row_count = 1U,
            .context = "last update ignores SET timestamp",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT s.table_name, s.sum_of_other_index_sizes "
                   "FROM mysql.innodb_table_stats AS s "
                   "WHERE s.database_name = 'app' "
                   "ORDER BY s.table_name DESC LIMIT 1",
            .column_names = alias_columns,
            .column_count = sizeof(alias_columns) / sizeof(alias_columns[0]),
            .values = alias_values,
            .row_count = 1U,
            .context = "alias order limit",
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){.sql = "USE mysql", .context = "use mysql"}
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM innodb_table_stats WHERE database_name = 'app'",
            .column_names = count_column,
            .column_count = sizeof(count_column) / sizeof(count_column[0]),
            .values = count_two,
            .row_count = 1U,
            .context = "unqualified selected mysql schema read",
        }
    );
    failures += expect_row_count_status(database, "mysql.innodb_table_stats status");
    failures += expect_statement_ok(
        database,
        (struct expected_statement){.sql = "USE app", .context = "use app"}
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "INSERT INTO t_plain VALUES (5,6)",
            .context = "insert t_plain row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT database_name, table_name, n_rows, clustered_index_size, "
                   "sum_of_other_index_sizes "
                   "FROM mysql.innodb_table_stats "
                   "WHERE database_name = 'app' AND table_name = 't_plain'",
            .column_names = stats_columns,
            .column_count = sizeof(stats_columns) / sizeof(stats_columns[0]),
            .values = updated_row_values,
            .row_count = 1U,
            .context = "updated descriptor row count",
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){.sql = "DROP TABLE t_plain", .context = "drop t_plain"}
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM mysql.innodb_table_stats WHERE database_name = 'app'",
            .column_names = count_column,
            .column_count = sizeof(count_column) / sizeof(count_column[0]),
            .values = count_one,
            .row_count = 1U,
            .context = "dropped table removed",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_SCHEMA, TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, "
                   "COLUMN_DEFAULT, IS_NULLABLE, DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, "
                   "CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION, NUMERIC_SCALE, "
                   "DATETIME_PRECISION, CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE, "
                   "COLUMN_KEY, EXTRA, PRIVILEGES "
                   "FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'innodb_table_stats' "
                   "ORDER BY ORDINAL_POSITION",
            .column_names = columns_metadata_columns,
            .column_count = sizeof(columns_metadata_columns) / sizeof(columns_metadata_columns[0]),
            .values = columns_metadata_values,
            .row_count = mysql_innodb_table_stats_column_count,
            .context = "mysql.innodb_table_stats columns metadata",
        }
    );
    failures += expect_error(
        database,
        "SELECT missing FROM mysql.innodb_table_stats",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing'",
        }
    );
    failures += expect_error(
        database,
        "INSERT INTO mysql.innodb_table_stats(database_name, table_name, last_update, n_rows, "
        "clustered_index_size, sum_of_other_index_sizes) "
        "VALUES ('x', 'y', '2024-01-01 00:00:00', 0, 1, 0)",
        (struct expected_sql_error){
            .code = mysql_error_system_schema_access,
            .sqlstate = "HY000",
            .message_part = "Access to system schema 'mysql' is rejected.",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int setup_stats_schema(mylite_db *database) {
    static const struct expected_statement statements[] = {
        {.sql = "CREATE DATABASE app", .context = "create app schema"},
        {.sql = "USE app", .context = "use app schema"},
        {.sql = "CREATE TABLE t_primary("
                "id INT PRIMARY KEY,"
                "v INT,"
                "KEY ix_v(v),"
                "KEY ix_v_id(v,id)"
                ")",
         .context = "create t_primary"},
        {.sql = "CREATE TABLE t_plain(a INT, b INT)", .context = "create t_plain"},
        {.sql = "INSERT INTO t_primary VALUES (1,10),(2,20),(3,20)",
         .context = "insert t_primary rows"},
        {.sql = "INSERT INTO t_plain VALUES (1,2),(3,4)", .context = "insert t_plain rows"},
    };
    int failures = 0;

    for (size_t index = 0U; index < sizeof(statements) / sizeof(statements[0]); ++index) {
        failures += expect_statement_ok(database, statements[index]);
    }
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
