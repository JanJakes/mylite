#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include "storage/mylite_file_format.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <process.h>
#else
#  include <unistd.h>
#endif

enum {
    mysql_error_unknown_column = 1054,
    test_path_capacity = 1024,
    seed_sql_capacity = 160,
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

struct expected_sql_error {
    const char *sql;
    int code;
    const char *sqlstate;
    const char *message_part;
};

static int test_information_schema_partitions_queries(void);
static int test_information_schema_partitions_reopen_preamble_and_handles(void);
static int seed_partitions_schema(mylite_db *database, const char *schema_name);
static int format_seed_sql(
    char *buffer,
    size_t buffer_size,
    const char *format,
    const char *schema_name
);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_datetime_query(
    mylite_db *database,
    const char *sql,
    const char *const *column_names,
    size_t column_count,
    const char *context
);
static int expect_error(mylite_db *database, struct expected_sql_error expected);
static int expect_row_count_status(mylite_db *database, const char *context);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_datetime_text(const char *actual, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_information_schema_partitions_queries();
    failures += test_information_schema_partitions_reopen_preamble_and_handles();

    return failures == 0 ? 0 : 1;
}

static int test_information_schema_partitions_queries(void) {
    static const char *const all_partition_columns[] = {
        "TABLE_CATALOG",
        "TABLE_SCHEMA",
        "TABLE_NAME",
        "PARTITION_NAME",
        "SUBPARTITION_NAME",
        "PARTITION_ORDINAL_POSITION",
        "SUBPARTITION_ORDINAL_POSITION",
        "PARTITION_METHOD",
        "SUBPARTITION_METHOD",
        "PARTITION_EXPRESSION",
        "SUBPARTITION_EXPRESSION",
        "PARTITION_DESCRIPTION",
        "TABLE_ROWS",
        "AVG_ROW_LENGTH",
        "DATA_LENGTH",
        "MAX_DATA_LENGTH",
        "INDEX_LENGTH",
        "DATA_FREE",
        "CREATE_TIME",
        "UPDATE_TIME",
        "CHECK_TIME",
        "CHECKSUM",
        "PARTITION_COMMENT",
        "NODEGROUP",
        "TABLESPACE_NAME",
    };
    static const char *const system_partitions_values[] = {
        "def",        "information_schema",
        "PARTITIONS", NULL,
        NULL,         NULL,
        NULL,         NULL,
        NULL,         NULL,
        NULL,         NULL,
        "0",          "0",
        "0",          "0",
        "0",          "0",
        NULL,         NULL,
        NULL,         NULL,
        "",           "",
        NULL,
    };
    static const char *const base_partition_columns[] = {
        "TABLE_NAME",
        "PARTITION_NAME",
        "SUBPARTITION_NAME",
        "PARTITION_ORDINAL_POSITION",
        "PARTITION_METHOD",
        "PARTITION_DESCRIPTION",
        "TABLE_ROWS",
        "AVG_ROW_LENGTH",
        "DATA_LENGTH",
        "MAX_DATA_LENGTH",
        "INDEX_LENGTH",
        "DATA_FREE",
        "CHECK_TIME",
        "CHECKSUM",
        "PARTITION_COMMENT",
        "NODEGROUP",
        "TABLESPACE_NAME",
    };
    static const char *const base_partition_values[] = {
        "empty", NULL,    NULL,    NULL,    NULL,    NULL, "0",
        "0",     "16384", "0",     "0",     "0",     NULL, NULL,
        "",      "",      NULL,    "plain", NULL,    NULL, NULL,
        NULL,    NULL,    "3",     "5461",  "16384", "0",  "0",
        "0",     NULL,    NULL,    "",      "",      NULL, "secondary_keyed",
        NULL,    NULL,    NULL,    NULL,    NULL,    "1",  "16384",
        "16384", "0",     "16384", "0",     NULL,    NULL, "",
        "",      NULL,
    };
    static const char *const ordered_limit_columns[] = {
        "TABLE_NAME",
        "TABLE_ROWS",
        "AVG_ROW_LENGTH",
        "INDEX_LENGTH",
    };
    static const char *const ordered_limit_values[] = {
        "secondary_keyed",
        "1",
        "16384",
        "16384",
        "plain",
        "3",
        "5461",
        "0",
    };
    static const char *const timestamp_columns[] = {"CREATE_TIME", "UPDATE_TIME"};
    static const char *const table_name_column[] = {"TABLE_NAME"};
    static const char *const alias_limit_values[] = {"empty", "plain"};
    static const char *const selected_schema_value[] = {"empty"};
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_three[] = {"3"};
    static const char *const system_table_columns[] = {
        "TABLE_SCHEMA",
        "TABLE_NAME",
        "TABLE_TYPE",
        "ENGINE",
        "VERSION",
        "ROW_FORMAT",
        "TABLE_ROWS",
        "DATA_LENGTH",
        "AUTO_INCREMENT",
    };
    static const char *const system_table_values[] = {
        "information_schema",
        "PARTITIONS",
        "SYSTEM VIEW",
        NULL,
        "10",
        NULL,
        "0",
        "0",
        NULL,
    };
    static const char *const columns_count_values[] = {"25"};
    static const char *const columns_metadata_columns[] = {
        "COLUMN_NAME",
        "ORDINAL_POSITION",
        "DATA_TYPE",
        "COLUMN_TYPE",
        "IS_NULLABLE",
        "CHARACTER_SET_NAME",
        "COLLATION_NAME",
    };
    static const char *const first_columns_metadata_values[] = {
        "TABLE_CATALOG", "1", "varchar", "varchar(64)", "NO", "utf8mb3", "utf8mb3_bin",
        "TABLE_SCHEMA",  "2", "varchar", "varchar(64)", "NO", "utf8mb3", "utf8mb3_bin",
        "TABLE_NAME",    "3", "varchar", "varchar(64)", "NO", "utf8mb3", "utf8mb3_bin",
    };
    static const char *const last_column_metadata_values[] = {
        "TABLESPACE_NAME",
        "25",
        "varchar",
        "varchar(268)",
        "YES",
        "utf8mb3",
        "utf8mb3_bin",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;
    int rc = MYLITE_OK;

    if (mylite_test_make_path(path, sizeof(path), "queries") != 0) {
        return 1;
    }
    remove_related_files(path);

    rc = mylite_open(path, &database);

    failures += mylite_test_expect_int(rc, MYLITE_OK, "open partitions db");
    if (rc != MYLITE_OK) {
        return failures + 1;
    }

    failures += seed_partitions_schema(database, "app");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM INFORMATION_SCHEMA.PARTITIONS "
                   "WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'PARTITIONS'",
            .column_names = all_partition_columns,
            .column_count = sizeof(all_partition_columns) / sizeof(all_partition_columns[0]),
            .values = system_partitions_values,
            .row_count = 1U,
            .context = "partitions wildcard system row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, PARTITION_NAME, SUBPARTITION_NAME, "
                   "PARTITION_ORDINAL_POSITION, PARTITION_METHOD, PARTITION_DESCRIPTION, "
                   "TABLE_ROWS, AVG_ROW_LENGTH, DATA_LENGTH, MAX_DATA_LENGTH, "
                   "INDEX_LENGTH, DATA_FREE, CHECK_TIME, CHECKSUM, PARTITION_COMMENT, "
                   "NODEGROUP, TABLESPACE_NAME FROM INFORMATION_SCHEMA.PARTITIONS "
                   "WHERE TABLE_SCHEMA = 'app' ORDER BY TABLE_NAME",
            .column_names = base_partition_columns,
            .column_count = sizeof(base_partition_columns) / sizeof(base_partition_columns[0]),
            .values = base_partition_values,
            .row_count = sizeof(base_partition_values) / sizeof(base_partition_values[0]) /
                         (sizeof(base_partition_columns) / sizeof(base_partition_columns[0])),
            .context = "base table nonpartitioned rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, TABLE_ROWS, AVG_ROW_LENGTH, INDEX_LENGTH "
                   "FROM INFORMATION_SCHEMA.PARTITIONS WHERE TABLE_SCHEMA = 'app' "
                   "ORDER BY TABLE_NAME DESC LIMIT 2",
            .column_names = ordered_limit_columns,
            .column_count = sizeof(ordered_limit_columns) / sizeof(ordered_limit_columns[0]),
            .values = ordered_limit_values,
            .row_count = 2U,
            .context = "partitions ordered limit",
        }
    );
    failures += expect_datetime_query(
        database,
        "SELECT CREATE_TIME, UPDATE_TIME FROM INFORMATION_SCHEMA.PARTITIONS "
        "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'plain'",
        timestamp_columns,
        sizeof(timestamp_columns) / sizeof(timestamp_columns[0]),
        "partitions base table timestamps"
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT p.TABLE_NAME FROM INFORMATION_SCHEMA.PARTITIONS AS p "
                   "WHERE p.TABLE_SCHEMA = 'app' ORDER BY p.TABLE_NAME LIMIT 2",
            .column_names = table_name_column,
            .column_count = 1U,
            .values = alias_limit_values,
            .row_count = 2U,
            .context = "partitions alias qualified order limit",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.PARTITIONS "
                   "WHERE TABLE_SCHEMA = 'app'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_three,
            .row_count = 1U,
            .context = "partitions base row count",
        }
    );
    failures += expect_statement_ok(database, "USE information_schema");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME FROM PARTITIONS WHERE TABLE_SCHEMA = 'app' "
                   "ORDER BY TABLE_NAME LIMIT 1",
            .column_names = table_name_column,
            .column_count = 1U,
            .values = selected_schema_value,
            .row_count = 1U,
            .context = "selected information schema unqualified partitions",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, "
                   "TABLE_ROWS, DATA_LENGTH, AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'PARTITIONS'",
            .column_names = system_table_columns,
            .column_count = sizeof(system_table_columns) / sizeof(system_table_columns[0]),
            .values = system_table_values,
            .row_count = 1U,
            .context = "partitions system table metadata",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'PARTITIONS'",
            .column_names = count_column,
            .column_count = 1U,
            .values = columns_count_values,
            .row_count = 1U,
            .context = "partitions columns metadata count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, ORDINAL_POSITION, DATA_TYPE, COLUMN_TYPE, "
                   "IS_NULLABLE, CHARACTER_SET_NAME, COLLATION_NAME "
                   "FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = 'information_schema' "
                   "AND TABLE_NAME = 'PARTITIONS' ORDER BY ORDINAL_POSITION LIMIT 3",
            .column_names = columns_metadata_columns,
            .column_count = sizeof(columns_metadata_columns) / sizeof(columns_metadata_columns[0]),
            .values = first_columns_metadata_values,
            .row_count = 3U,
            .context = "partitions first columns metadata",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, ORDINAL_POSITION, DATA_TYPE, COLUMN_TYPE, "
                   "IS_NULLABLE, CHARACTER_SET_NAME, COLLATION_NAME "
                   "FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = 'information_schema' "
                   "AND TABLE_NAME = 'PARTITIONS' ORDER BY ORDINAL_POSITION DESC LIMIT 1",
            .column_names = columns_metadata_columns,
            .column_count = sizeof(columns_metadata_columns) / sizeof(columns_metadata_columns[0]),
            .values = last_column_metadata_values,
            .row_count = 1U,
            .context = "partitions last column metadata",
        }
    );
    failures += expect_row_count_status(database, "partitions row count status");
    failures += expect_error(
        database,
        (struct expected_sql_error){
            .sql = "SELECT MISSING_COLUMN FROM INFORMATION_SCHEMA.PARTITIONS",
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'MISSING_COLUMN' in 'field list'",
        }
    );
    failures += expect_error(
        database,
        (struct expected_sql_error){
            .sql = "SELECT TABLE_NAME FROM INFORMATION_SCHEMA.PARTITIONS WHERE MISSING_COLUMN = 1",
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'MISSING_COLUMN' in 'where clause'",
        }
    );
    failures += expect_error(
        database,
        (struct expected_sql_error){
            .sql = "SELECT TABLE_NAME FROM INFORMATION_SCHEMA.PARTITIONS ORDER BY MISSING_COLUMN",
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'MISSING_COLUMN' in 'order clause'",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_information_schema_partitions_reopen_preamble_and_handles(void) {
    static const char *const partition_columns[] = {
        "TABLE_SCHEMA",
        "TABLE_NAME",
        "TABLE_ROWS",
    };
    static const char *const first_values[] = {
        "first_app",
        "empty",
        "0",
        "first_app",
        "plain",
        "3",
        "first_app",
        "secondary_keyed",
        "1",
    };
    static const char *const second_values[] = {
        "second_app",
        "empty",
        "0",
        "second_app",
        "plain",
        "3",
        "second_app",
        "secondary_keyed",
        "1",
    };
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (mylite_test_make_path(first_path, sizeof(first_path), "first") != 0 ||
        mylite_test_make_path(second_path, sizeof(second_path), "second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);
    mylite_file_preamble_init(expected_preamble);

    failures += mylite_test_expect_int(
        mylite_open(first_path, &first),
        MYLITE_OK,
        "open first partitions db"
    );
    failures += mylite_test_expect_int(
        mylite_open(second_path, &second),
        MYLITE_OK,
        "open second partitions db"
    );
    failures += seed_partitions_schema(first, "first_app");
    failures += seed_partitions_schema(second, "second_app");
    failures += expect_query(
        first,
        (struct expected_query){
            .sql = "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_ROWS "
                   "FROM INFORMATION_SCHEMA.PARTITIONS WHERE TABLE_SCHEMA = 'first_app' "
                   "ORDER BY TABLE_NAME",
            .column_names = partition_columns,
            .column_count = sizeof(partition_columns) / sizeof(partition_columns[0]),
            .values = first_values,
            .row_count = 3U,
            .context = "first handle partitions rows",
        }
    );
    failures += expect_query(
        second,
        (struct expected_query){
            .sql = "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_ROWS "
                   "FROM INFORMATION_SCHEMA.PARTITIONS WHERE TABLE_SCHEMA = 'second_app' "
                   "ORDER BY TABLE_NAME",
            .column_names = partition_columns,
            .column_count = sizeof(partition_columns) / sizeof(partition_columns[0]),
            .values = second_values,
            .row_count = 3U,
            .context = "second handle partitions rows",
        }
    );
    failures += read_file_at(first_path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after partitions metadata query"
    );

    mylite_close(first);
    first = NULL;
    mylite_close(second);
    second = NULL;

    failures += mylite_test_expect_int(
        mylite_open(first_path, &first),
        MYLITE_OK,
        "reopen first partitions db"
    );
    failures += expect_query(
        first,
        (struct expected_query){
            .sql = "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_ROWS "
                   "FROM INFORMATION_SCHEMA.PARTITIONS WHERE TABLE_SCHEMA = 'first_app' "
                   "ORDER BY TABLE_NAME",
            .column_names = partition_columns,
            .column_count = sizeof(partition_columns) / sizeof(partition_columns[0]),
            .values = first_values,
            .row_count = 3U,
            .context = "reopened partitions rows",
        }
    );

    mylite_close(first);
    remove_related_files(first_path);
    remove_related_files(second_path);
    return failures;
}

static int seed_partitions_schema(mylite_db *database, const char *schema_name) {
    char create_database_sql[seed_sql_capacity];
    char empty_table_sql[seed_sql_capacity];
    char plain_table_sql[seed_sql_capacity];
    char keyed_table_sql[seed_sql_capacity];
    char insert_plain_sql[seed_sql_capacity];
    char insert_keyed_sql[seed_sql_capacity];
    int failures = 0;

    if (format_seed_sql(
            create_database_sql,
            sizeof(create_database_sql),
            "CREATE DATABASE %s",
            schema_name
        ) != 0 ||
        format_seed_sql(
            empty_table_sql,
            sizeof(empty_table_sql),
            "CREATE TABLE %s.empty(id INT)",
            schema_name
        ) != 0 ||
        format_seed_sql(
            plain_table_sql,
            sizeof(plain_table_sql),
            "CREATE TABLE %s.plain(id INT NOT NULL, v INT)",
            schema_name
        ) != 0 ||
        format_seed_sql(
            keyed_table_sql,
            sizeof(keyed_table_sql),
            "CREATE TABLE %s.secondary_keyed(id INT NOT NULL, v INT, KEY by_v (v))",
            schema_name
        ) != 0 ||
        format_seed_sql(
            insert_plain_sql,
            sizeof(insert_plain_sql),
            "INSERT INTO %s.plain VALUES (1, 10), (2, NULL), (3, 30)",
            schema_name
        ) != 0 ||
        format_seed_sql(
            insert_keyed_sql,
            sizeof(insert_keyed_sql),
            "INSERT INTO %s.secondary_keyed VALUES (1, 100)",
            schema_name
        ) != 0) {
        fprintf(stderr, "%s: failed to build seed SQL\n", schema_name);
        return 1;
    }

    failures += expect_statement_ok(database, create_database_sql);
    failures += expect_statement_ok(database, empty_table_sql);
    failures += expect_statement_ok(database, plain_table_sql);
    failures += expect_statement_ok(database, keyed_table_sql);
    failures += expect_statement_ok(database, insert_plain_sql);
    failures += expect_statement_ok(database, insert_keyed_sql);

    return failures;
}

static int format_seed_sql(
    char *buffer,
    size_t buffer_size,
    const char *format,
    const char *schema_name
) {
    int written = snprintf(buffer, buffer_size, format, schema_name);

    return written < 0 || (size_t)written >= buffer_size ? 1 : 0;
}

static int expect_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected success, got %d/%s: %s\n",
            sql,
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
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);
    int failures = 0;

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected success, got %d/%s: %s\n",
            expected.context,
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

static int expect_datetime_query(
    mylite_db *database,
    const char *sql,
    const char *const *column_names,
    size_t column_count,
    const char *context
) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected success, got %d/%s: %s\n",
            context,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }
    if (result == NULL) {
        fprintf(stderr, "%s: expected result object\n", context);
        return 1;
    }

    failures += mylite_test_expect_size(mylite_result_column_count(result), column_count, context);
    failures += mylite_test_expect_size(mylite_result_row_count(result), 1U, context);
    failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, context);
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, context);

    for (size_t column_index = 0U; column_index < column_count; ++column_index) {
        failures += mylite_test_expect_text_or_null(
            mylite_result_column_name(result, column_index),
            column_names[column_index],
            context
        );
        failures +=
            expect_datetime_text(mylite_result_value_text(result, 0U, column_index), context);
    }

    mylite_result_free(result);
    return failures;
}

static int expect_error(mylite_db *database, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);
    int failures = 0;

    if (rc == MYLITE_OK) {
        fprintf(stderr, "%s: expected error, got success\n", expected.sql);
        mylite_result_free(result);
        return 1;
    }

    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, expected.sql);
    failures +=
        mylite_test_expect_text_or_null(mylite_sqlstate(database), expected.sqlstate, expected.sql);
    if (strstr(mylite_errmsg(database), expected.message_part) == NULL) {
        fprintf(
            stderr,
            "%s: expected message containing %s, got %s\n",
            expected.sql,
            expected.message_part,
            mylite_errmsg(database)
        );
        ++failures;
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

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        fprintf(stderr, "%s: expected readable file\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0 || fread(buffer, 1U, size, file) != size) {
        fprintf(stderr, "%s: failed to read file bytes\n", path);
        fclose(file);
        return 1;
    }

    fclose(file);
    return 0;
}

static int expect_datetime_text(const char *actual, const char *context) {
    if (actual == NULL || strlen(actual) != datetime_text_length ||
        actual[datetime_year_month_separator] != '-' ||
        actual[datetime_month_day_separator] != '-' ||
        actual[datetime_date_time_separator] != ' ' ||
        actual[datetime_hour_minute_separator] != ':' ||
        actual[datetime_minute_second_separator] != ':') {
        fprintf(
            stderr,
            "%s: expected datetime text, got %s\n",
            context,
            actual == NULL ? "NULL" : actual
        );
        return 1;
    }
    return 0;
}

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: bytes differ\n", context);
        return 1;
    }
    return 0;
}
