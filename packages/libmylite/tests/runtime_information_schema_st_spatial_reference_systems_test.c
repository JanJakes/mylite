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
#  define P_tmpdir "/tmp"
#endif

#define ARRAY_COUNT(items) (sizeof(items) / sizeof((items)[0]))

enum {
    test_path_capacity = 1024,
    test_path_suffix_capacity = 16,
    metadata_query_capacity = 768,
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
    size_t warning_count;
    const char *context;
};

struct expected_information_schema_column {
    const char *name;
    const char *is_nullable;
    const char *data_type;
    const char *character_maximum_length;
    const char *character_octet_length;
    const char *numeric_precision;
    const char *numeric_scale;
    const char *character_set_name;
    const char *collation_name;
    const char *column_type;
};

static const struct expected_information_schema_column srs_columns[] = {
    {"SRS_NAME",
     "NO",
     "varchar",
     "80",
     "240",
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "varchar(80)"},
    {"SRS_ID", "NO", "int", NULL, NULL, "10", "0", NULL, NULL, "int unsigned"},
    {"ORGANIZATION",
     "YES",
     "varchar",
     "256",
     "768",
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "varchar(256)"},
    {"ORGANIZATION_COORDSYS_ID", "YES", "int", NULL, NULL, "10", "0", NULL, NULL, "int unsigned"},
    {"DEFINITION",
     "NO",
     "varchar",
     "4096",
     "12288",
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_bin",
     "varchar(4096)"},
    {"DESCRIPTION",
     "YES",
     "varchar",
     "2048",
     "6144",
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_bin",
     "varchar(2048)"},
};

static int test_information_schema_st_spatial_reference_systems_queries(void);
static int setup_spatial_table(mylite_db *database);
static int expect_statement(mylite_db *database, struct expected_statement expected);
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

    failures += test_information_schema_st_spatial_reference_systems_queries();

    return failures == 0 ? 0 : 1;
}

static int test_information_schema_st_spatial_reference_systems_queries(void) {
    static const char *const srs_column_names[] = {
        "SRS_NAME",
        "SRS_ID",
        "ORGANIZATION",
        "ORGANIZATION_COORDSYS_ID",
        "DEFINITION",
        "DESCRIPTION",
    };
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_zero[] = {"0"};
    static const char *const srs_name_id_columns[] = {"SRS_NAME", "SRS_ID"};
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
        "ST_SPATIAL_REFERENCE_SYSTEMS",
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

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open srs db");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM INFORMATION_SCHEMA.ST_SPATIAL_REFERENCE_SYSTEMS",
            .column_names = srs_column_names,
            .column_count = ARRAY_COUNT(srs_column_names),
            .values = NULL,
            .row_count = 0U,
            .context = "srs wildcard",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.ST_SPATIAL_REFERENCE_SYSTEMS",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "srs count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.st_spatial_reference_systems",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "case-insensitive srs table",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT s.SRS_NAME, s.SRS_ID "
                   "FROM INFORMATION_SCHEMA.ST_SPATIAL_REFERENCE_SYSTEMS AS s "
                   "WHERE s.ORGANIZATION = 'EPSG' AND s.ORGANIZATION_COORDSYS_ID = 4326 "
                   "AND s.DEFINITION LIKE 'GEOGCS%' ORDER BY s.SRS_ID LIMIT 1",
            .column_names = srs_name_id_columns,
            .column_count = ARRAY_COUNT(srs_name_id_columns),
            .values = NULL,
            .row_count = 0U,
            .context = "srs alias predicate",
        }
    );
    failures += expect_statement(
        database,
        (struct expected_statement){
            .sql = "USE information_schema",
            .warning_count = 0U,
            .context = "use information_schema for srs",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM ST_SPATIAL_REFERENCE_SYSTEMS WHERE SRS_ID = 4326",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "unqualified srs count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, "
                   "TABLE_ROWS, DATA_LENGTH, AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'information_schema' "
                   "AND TABLE_NAME = 'ST_SPATIAL_REFERENCE_SYSTEMS'",
            .column_names = system_table_columns,
            .column_count = ARRAY_COUNT(system_table_columns),
            .values = system_table_values,
            .row_count = 1U,
            .context = "srs system table row",
        }
    );
    failures += expect_columns_metadata(database);
    failures += expect_row_count_status(database, "srs row count status");
    failures += setup_spatial_table(database);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.ST_SPATIAL_REFERENCE_SYSTEMS",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "srs remains empty after spatial metadata",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int setup_spatial_table(mylite_db *database) {
    int failures = 0;

    failures += expect_statement(
        database,
        (struct expected_statement){
            .sql = "CREATE SCHEMA app",
            .warning_count = 0U,
            .context = "create srs schema",
        }
    );
    failures += expect_statement(
        database,
        (struct expected_statement){
            .sql = "USE app",
            .warning_count = 0U,
            .context = "use srs schema",
        }
    );
    failures += expect_statement(
        database,
        (struct expected_statement){
            .sql = "CREATE TABLE srs_probe (id INT PRIMARY KEY, g POINT NOT NULL, "
                   "SPATIAL INDEX g_spatial (g))",
            .warning_count = 1U,
            .context = "create spatial metadata table for srs",
        }
    );

    return failures;
}

static int expect_statement(mylite_db *database, struct expected_statement expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);

    failures += expect_int(rc, MYLITE_OK, expected.context);
    if (rc == MYLITE_OK) {
        failures += expect_size(
            mylite_result_warning_count(result),
            expected.warning_count,
            expected.context
        );
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
        "WHERE TABLE_SCHEMA = 'information_schema' "
        "AND TABLE_NAME = 'ST_SPATIAL_REFERENCE_SYSTEMS' ORDER BY ORDINAL_POSITION"
    );
    int rc = 0;

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "srs columns metadata: SQL buffer too small\n");
        return 1;
    }

    rc = mylite_execute(database, sql, strlen(sql), &result);
    failures += expect_int(rc, MYLITE_OK, "srs columns metadata");
    if (rc != MYLITE_OK) {
        fprintf(stderr, "srs columns metadata: %s\n", mylite_errmsg(database));
        mylite_result_free(result);
        return failures + 1;
    }
    if (result == NULL) {
        fprintf(stderr, "srs columns metadata: expected result object\n");
        return failures + 1;
    }

    failures += expect_size(
        mylite_result_column_count(result),
        ARRAY_COUNT(metadata_column_names),
        "srs columns metadata"
    );
    failures += expect_size(
        mylite_result_row_count(result),
        ARRAY_COUNT(srs_columns),
        "srs columns metadata"
    );
    failures += expect_int64(mylite_result_affected_rows(result), 0, "srs columns metadata");
    failures += expect_size(mylite_result_warning_count(result), 0U, "srs columns metadata");

    for (size_t column = 0U; column < ARRAY_COUNT(metadata_column_names); ++column) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, column),
            metadata_column_names[column],
            "srs columns metadata"
        );
    }
    for (size_t row = 0U; row < ARRAY_COUNT(srs_columns); ++row) {
        const struct expected_information_schema_column *expected = &srs_columns[row];
        char ordinal_text[metadata_ordinal_text_capacity];

        written = snprintf(ordinal_text, sizeof(ordinal_text), "%zu", row + 1U);
        if (written < 0 || (size_t)written >= sizeof(ordinal_text)) {
            fprintf(stderr, "srs columns metadata: ordinal overflow\n");
            failures += 1;
            continue;
        }

        failures += expect_text_or_null(
            mylite_result_value_text(result, row, 0U),
            "ST_SPATIAL_REFERENCE_SYSTEMS",
            "srs columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, 1U),
            expected->name,
            "srs columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, 2U),
            ordinal_text,
            "srs columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, 3U),
            NULL,
            "srs columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, 4U),
            expected->is_nullable,
            "srs columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, metadata_data_type_column),
            expected->data_type,
            "srs columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, metadata_character_maximum_length_column),
            expected->character_maximum_length,
            "srs columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, metadata_character_octet_length_column),
            expected->character_octet_length,
            "srs columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, metadata_numeric_precision_column),
            expected->numeric_precision,
            "srs columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, metadata_numeric_scale_column),
            expected->numeric_scale,
            "srs columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, metadata_datetime_precision_column),
            NULL,
            "srs columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, metadata_character_set_name_column),
            expected->character_set_name,
            "srs columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, metadata_collation_name_column),
            expected->collation_name,
            "srs columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, metadata_column_type_column),
            expected->column_type,
            "srs columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, metadata_privileges_column),
            "select",
            "srs columns metadata"
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
        "%s/mylite_information_schema_st_spatial_reference_systems_%d_%s.mylite",
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
