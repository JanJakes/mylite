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

#ifndef P_tmpdir
#  define P_tmpdir "."
#endif

#define ARRAY_COUNT(items) (sizeof(items) / sizeof((items)[0]))

enum {
    test_path_capacity = 1024,
    test_path_suffix_capacity = 16,
    metadata_query_capacity = 512,
    metadata_ordinal_text_capacity = 16,
    metadata_data_type_column = 5,
    metadata_character_maximum_length_column = 6,
    metadata_character_octet_length_column = 7,
    metadata_numeric_precision_column = 8,
    metadata_numeric_scale_column = 9,
    metadata_datetime_precision_column = 10,
    metadata_character_set_name_column = 11,
    metadata_collation_name_column = 12,
    metadata_column_type_column = 13,
    metadata_privileges_column = 14,
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

struct expected_information_schema_column {
    const char *name;
    const char *is_nullable;
    const char *data_type;
    const char *character_maximum_length;
    const char *character_octet_length;
    const char *character_set_name;
    const char *collation_name;
    const char *column_type;
};

static const struct expected_information_schema_column metrics_columns[] = {
    {"NAME", "NO", "varchar", "64", "193", "utf8mb3", "utf8mb3_general_ci", "varchar(193)"},
    {"SUBSYSTEM", "NO", "varchar", "64", "193", "utf8mb3", "utf8mb3_general_ci", "varchar(193)"},
    {"COUNT", "NO", "bigint", NULL, NULL, NULL, NULL, "bigint"},
    {"MAX_COUNT", "YES", "bigint", NULL, NULL, NULL, NULL, "bigint"},
    {"MIN_COUNT", "YES", "bigint", NULL, NULL, NULL, NULL, "bigint"},
    {"AVG_COUNT", "YES", "float", NULL, NULL, NULL, NULL, "float(12,0)"},
    {"COUNT_RESET", "NO", "bigint", NULL, NULL, NULL, NULL, "bigint"},
    {"MAX_COUNT_RESET", "YES", "bigint", NULL, NULL, NULL, NULL, "bigint"},
    {"MIN_COUNT_RESET", "YES", "bigint", NULL, NULL, NULL, NULL, "bigint"},
    {"AVG_COUNT_RESET", "YES", "float", NULL, NULL, NULL, NULL, "float(12,0)"},
    {"TIME_ENABLED", "YES", "datetime", NULL, NULL, NULL, NULL, "datetime"},
    {"TIME_DISABLED", "YES", "datetime", NULL, NULL, NULL, NULL, "datetime"},
    {"TIME_ELAPSED", "YES", "bigint", NULL, NULL, NULL, NULL, "bigint"},
    {"TIME_RESET", "YES", "datetime", NULL, NULL, NULL, NULL, "datetime"},
    {"STATUS", "NO", "varchar", "64", "193", "utf8mb3", "utf8mb3_general_ci", "varchar(193)"},
    {"TYPE", "NO", "varchar", "64", "193", "utf8mb3", "utf8mb3_general_ci", "varchar(193)"},
    {"COMMENT", "NO", "varchar", "64", "193", "utf8mb3", "utf8mb3_general_ci", "varchar(193)"},
};

static int test_information_schema_innodb_metrics_queries(void);
static int setup_indexed_table(mylite_db *database);
static int expect_statement_ok(mylite_db *database, struct expected_statement expected);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_columns_metadata(mylite_db *database);
static int expect_row_count_status(mylite_db *database, const char *context);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text_or_null(const char *actual, const char *expected, const char *context);

int main(void) {
    int failures = 0;

    failures += test_information_schema_innodb_metrics_queries();

    return failures == 0 ? 0 : 1;
}

static int test_information_schema_innodb_metrics_queries(void) {
    static const char *const metrics_column_names[] = {
        "NAME",
        "SUBSYSTEM",
        "COUNT",
        "MAX_COUNT",
        "MIN_COUNT",
        "AVG_COUNT",
        "COUNT_RESET",
        "MAX_COUNT_RESET",
        "MIN_COUNT_RESET",
        "AVG_COUNT_RESET",
        "TIME_ENABLED",
        "TIME_DISABLED",
        "TIME_ELAPSED",
        "TIME_RESET",
        "STATUS",
        "TYPE",
        "COMMENT",
    };
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_zero[] = {"0"};
    static const char *const name_status_columns[] = {"NAME", "STATUS"};
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
        "INNODB_METRICS",
        "SYSTEM VIEW",
        NULL,
        "10",
        NULL,
        "0",
        "0",
        NULL,
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "queries") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open metrics db");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM INFORMATION_SCHEMA.INNODB_METRICS",
            .column_names = metrics_column_names,
            .column_count = ARRAY_COUNT(metrics_column_names),
            .values = NULL,
            .row_count = 0U,
            .context = "metrics wildcard",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_METRICS",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "metrics count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.innodb_metrics",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "case-insensitive metrics table",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT m.NAME, m.STATUS FROM INFORMATION_SCHEMA.INNODB_METRICS AS m "
                   "WHERE m.SUBSYSTEM = 'buffer' AND m.COUNT > 0 AND m.TIME_ENABLED IS NOT NULL "
                   "ORDER BY m.NAME LIMIT 1",
            .column_names = name_status_columns,
            .column_count = ARRAY_COUNT(name_status_columns),
            .values = NULL,
            .row_count = 0U,
            .context = "metrics alias predicate",
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "USE information_schema",
            .context = "use information_schema for metrics",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INNODB_METRICS WHERE STATUS = 'enabled'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "unqualified metrics count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, "
                   "TABLE_ROWS, DATA_LENGTH, AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'INNODB_METRICS'",
            .column_names = system_table_columns,
            .column_count = ARRAY_COUNT(system_table_columns),
            .values = system_table_values,
            .row_count = 1U,
            .context = "metrics system table row",
        }
    );
    failures += expect_columns_metadata(database);
    failures += expect_row_count_status(database, "metrics row count status");
    failures += setup_indexed_table(database);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_METRICS",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "metrics remains empty after table activity",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int setup_indexed_table(mylite_db *database) {
    int failures = 0;

    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "CREATE SCHEMA app",
            .context = "create metrics schema",
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "USE app",
            .context = "use metrics schema",
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "CREATE TABLE metrics_probe (id INT PRIMARY KEY, v INT, KEY v_key (v))",
            .context = "create indexed table for metrics",
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "INSERT INTO metrics_probe VALUES (1, 10), (2, 20)",
            .context = "insert indexed table rows for metrics",
        }
    );

    return failures;
}

static int expect_statement_ok(mylite_db *database, struct expected_statement expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);

    failures += expect_int(rc, MYLITE_OK, expected.context);
    if (rc == MYLITE_OK) {
        failures += expect_size(mylite_result_warning_count(result), 0U, expected.context);
    } else {
        fprintf(stderr, "%s: %s\n", expected.context, mylite_errmsg(database));
    }
    mylite_result_free(result);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);

    failures += expect_int(rc, MYLITE_OK, expected.context);
    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: %s\n", expected.context, mylite_errmsg(database));
        mylite_result_free(result);
        return failures + 1;
    }
    if (result == NULL) {
        fprintf(stderr, "%s: expected result object\n", expected.context);
        return failures + 1;
    }

    failures +=
        expect_size(mylite_result_column_count(result), expected.column_count, expected.context);
    failures += expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, expected.context);
    for (size_t column = 0U; column < expected.column_count; ++column) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, column),
            expected.column_names[column],
            expected.context
        );
    }
    for (size_t row = 0U; row < expected.row_count; ++row) {
        for (size_t column = 0U; column < expected.column_count; ++column) {
            size_t value_index = (row * expected.column_count) + column;

            failures += expect_text_or_null(
                mylite_result_value_text(result, row, column),
                expected.values[value_index],
                expected.context
            );
        }
    }
    mylite_result_free(result);
    return failures;
}

static int expect_columns_metadata(mylite_db *database) {
    static const char *const metadata_column_names[] = {
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
        "PRIVILEGES",
    };
    mylite_result *result = NULL;
    char sql[metadata_query_capacity];
    int failures = 0;
    int written = snprintf(
        sql,
        sizeof(sql),
        "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE, "
        "DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION, "
        "NUMERIC_SCALE, DATETIME_PRECISION, CHARACTER_SET_NAME, COLLATION_NAME, "
        "COLUMN_TYPE, PRIVILEGES FROM INFORMATION_SCHEMA.COLUMNS "
        "WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'INNODB_METRICS' "
        "ORDER BY ORDINAL_POSITION"
    );
    int rc = 0;

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "metrics columns metadata: SQL buffer too small\n");
        return 1;
    }

    rc = mylite_execute(database, sql, strlen(sql), &result);
    failures += expect_int(rc, MYLITE_OK, "metrics columns metadata");
    if (rc != MYLITE_OK) {
        fprintf(stderr, "metrics columns metadata: %s\n", mylite_errmsg(database));
        mylite_result_free(result);
        return failures + 1;
    }
    if (result == NULL) {
        fprintf(stderr, "metrics columns metadata: expected result object\n");
        return failures + 1;
    }

    failures += expect_size(
        mylite_result_column_count(result),
        ARRAY_COUNT(metadata_column_names),
        "metrics columns metadata"
    );
    failures += expect_size(
        mylite_result_row_count(result),
        ARRAY_COUNT(metrics_columns),
        "metrics columns metadata"
    );
    failures += expect_int64(mylite_result_affected_rows(result), 0, "metrics columns metadata");
    failures += expect_size(mylite_result_warning_count(result), 0U, "metrics columns metadata");

    for (size_t column = 0U; column < ARRAY_COUNT(metadata_column_names); ++column) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, column),
            metadata_column_names[column],
            "metrics columns metadata"
        );
    }
    for (size_t row = 0U; row < ARRAY_COUNT(metrics_columns); ++row) {
        const struct expected_information_schema_column *expected = &metrics_columns[row];
        char ordinal_text[metadata_ordinal_text_capacity];

        written = snprintf(ordinal_text, sizeof(ordinal_text), "%zu", row + 1U);
        if (written < 0 || (size_t)written >= sizeof(ordinal_text)) {
            fprintf(stderr, "metrics columns metadata: ordinal overflow\n");
            failures += 1;
            continue;
        }

        failures += expect_text_or_null(
            mylite_result_value_text(result, row, 0U),
            "INNODB_METRICS",
            "metrics columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, 1U),
            expected->name,
            "metrics columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, 2U),
            ordinal_text,
            "metrics columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, 3U),
            "",
            "metrics columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, 4U),
            expected->is_nullable,
            "metrics columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, metadata_data_type_column),
            expected->data_type,
            "metrics columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, metadata_character_maximum_length_column),
            expected->character_maximum_length,
            "metrics columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, metadata_character_octet_length_column),
            expected->character_octet_length,
            "metrics columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, metadata_numeric_precision_column),
            NULL,
            "metrics columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, metadata_numeric_scale_column),
            NULL,
            "metrics columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, metadata_datetime_precision_column),
            NULL,
            "metrics columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, metadata_character_set_name_column),
            expected->character_set_name,
            "metrics columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, metadata_collation_name_column),
            expected->collation_name,
            "metrics columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, metadata_column_type_column),
            expected->column_type,
            "metrics columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, metadata_privileges_column),
            "select",
            "metrics columns metadata"
        );
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

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "%s/mylite_information_schema_innodb_metrics_%d_%s.mylite",
        P_tmpdir,
        current_process_id(),
        name
    );

    return written < 0 || (size_t)written >= path_size ? 1 : 0;
}

static int current_process_id(void) {
#ifdef _WIN32
    return _getpid();
#else
    return getpid();
#endif
}

static void remove_related_files(const char *path) {
    remove(path);
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char buffer[test_path_capacity + test_path_suffix_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written > 0 && (size_t)written < sizeof(buffer)) {
        remove(buffer);
    }
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
    return 1;
}

static int expect_int64(int64_t actual, int64_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected %lld, got %lld\n",
        context,
        (long long)expected,
        (long long)actual
    );
    return 1;
}

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
    return 1;
}

static int expect_text_or_null(const char *actual, const char *expected, const char *context) {
    if (actual == NULL && expected == NULL) {
        return 0;
    }
    if (actual != NULL && expected != NULL && strcmp(actual, expected) == 0) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected [%s], got [%s]\n",
        context,
        expected == NULL ? "NULL" : expected,
        actual == NULL ? "NULL" : actual
    );
    return 1;
}
