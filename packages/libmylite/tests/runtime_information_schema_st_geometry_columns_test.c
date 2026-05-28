#include <mylite/mylite.h>

#include "storage/mylite_file_format.h"

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
};

struct expected_query {
    const char *sql;
    const char *const *column_names;
    size_t column_count;
    const char *const *values;
    size_t row_count;
    size_t warning_count;
    const char *context;
};

struct expected_status {
    const char *warning_count;
    const char *row_count;
    const char *context;
};

static int test_information_schema_st_geometry_columns_queries(void);
static int test_information_schema_st_geometry_columns_reopen_and_handles(void);
static int seed_spatial_schema(mylite_db *database);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_status(mylite_db *database, struct expected_status expected);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text_or_null(const char *actual, const char *expected, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_information_schema_st_geometry_columns_queries();
    failures += test_information_schema_st_geometry_columns_reopen_and_handles();

    return failures == 0 ? 0 : 1;
}

static int test_information_schema_st_geometry_columns_queries(void) {
    static const char *const wildcard_columns[] = {
        "TABLE_CATALOG",
        "TABLE_SCHEMA",
        "TABLE_NAME",
        "COLUMN_NAME",
        "SRS_NAME",
        "SRS_ID",
        "GEOMETRY_TYPE_NAME",
    };
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_zero[] = {"0"};
    static const char *const count_eight[] = {"8"};
    static const char *const geometry_rows[] = {
        "def", "app", "spatial_meta", "g",     NULL, NULL, "geometry",
        "def", "app", "spatial_meta", "gc",    NULL, NULL, "geomcollection",
        "def", "app", "spatial_meta", "ls",    NULL, NULL, "linestring",
        "def", "app", "spatial_meta", "mls",   NULL, NULL, "multilinestring",
        "def", "app", "spatial_meta", "mp",    NULL, NULL, "multipoint",
        "def", "app", "spatial_meta", "mpoly", NULL, NULL, "multipolygon",
        "def", "app", "spatial_meta", "p",     NULL, NULL, "point",
        "def", "app", "spatial_meta", "poly",  NULL, NULL, "polygon",
    };
    static const char *const type_column[] = {"GEOMETRY_TYPE_NAME"};
    static const char *const point_type_value[] = {"point"};
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
        "ST_GEOMETRY_COLUMNS",
        "SYSTEM VIEW",
        NULL,
        "10",
        NULL,
        "0",
        "0",
        NULL,
    };
    static const char *const columns_metadata_columns[] = {
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
    static const char *const columns_metadata_values[] = {
        "ST_GEOMETRY_COLUMNS",
        "TABLE_CATALOG",
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
        "select",
        "ST_GEOMETRY_COLUMNS",
        "TABLE_SCHEMA",
        "2",
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
        "select",
        "ST_GEOMETRY_COLUMNS",
        "TABLE_NAME",
        "3",
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
        "select",
        "ST_GEOMETRY_COLUMNS",
        "COLUMN_NAME",
        "4",
        NULL,
        "YES",
        "varchar",
        "64",
        "192",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_tolower_ci",
        "varchar(64)",
        "select",
        "ST_GEOMETRY_COLUMNS",
        "SRS_NAME",
        "5",
        NULL,
        "YES",
        "varchar",
        "80",
        "240",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(80)",
        "select",
        "ST_GEOMETRY_COLUMNS",
        "SRS_ID",
        "6",
        NULL,
        "YES",
        "int",
        NULL,
        NULL,
        "10",
        "0",
        NULL,
        NULL,
        NULL,
        "int unsigned",
        "select",
        "ST_GEOMETRY_COLUMNS",
        "GEOMETRY_TYPE_NAME",
        "7",
        NULL,
        "YES",
        "longtext",
        "4294967295",
        "4294967295",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_bin",
        "longtext",
        "select",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "queries") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open st geometry db");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM INFORMATION_SCHEMA.ST_GEOMETRY_COLUMNS LIMIT 0",
            .column_names = wildcard_columns,
            .column_count = sizeof(wildcard_columns) / sizeof(wildcard_columns[0]),
            .values = NULL,
            .row_count = 0U,
            .warning_count = 0U,
            .context = "st geometry wildcard columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.ST_GEOMETRY_COLUMNS "
                   "WHERE TABLE_SCHEMA = 'information_schema'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "st geometry information schema empty rows",
        }
    );
    failures += seed_spatial_schema(database);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM information_schema.st_geometry_columns",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_eight,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "case-insensitive st geometry count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_CATALOG, TABLE_SCHEMA, TABLE_NAME, COLUMN_NAME, "
                   "SRS_NAME, SRS_ID, GEOMETRY_TYPE_NAME "
                   "FROM INFORMATION_SCHEMA.ST_GEOMETRY_COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'spatial_meta' "
                   "ORDER BY COLUMN_NAME",
            .column_names = wildcard_columns,
            .column_count = sizeof(wildcard_columns) / sizeof(wildcard_columns[0]),
            .values = geometry_rows,
            .row_count = sizeof(geometry_rows) / sizeof(geometry_rows[0]) /
                         (sizeof(wildcard_columns) / sizeof(wildcard_columns[0])),
            .warning_count = 0U,
            .context = "st geometry descriptor rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT s.GEOMETRY_TYPE_NAME "
                   "FROM INFORMATION_SCHEMA.ST_GEOMETRY_COLUMNS AS s "
                   "WHERE s.TABLE_SCHEMA = 'app' AND s.COLUMN_NAME = 'p'",
            .column_names = type_column,
            .column_count = 1U,
            .values = point_type_value,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "st geometry alias projection",
        }
    );
    failures += expect_status(
        database,
        (struct expected_status){
            .warning_count = "0",
            .row_count = "-1",
            .context = "st geometry alias status",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, "
                   "TABLE_ROWS, DATA_LENGTH, AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'information_schema' "
                   "AND TABLE_NAME = 'ST_GEOMETRY_COLUMNS'",
            .column_names = system_table_columns,
            .column_count = sizeof(system_table_columns) / sizeof(system_table_columns[0]),
            .values = system_table_values,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "st geometry system table row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, "
                   "IS_NULLABLE, DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, "
                   "CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION, NUMERIC_SCALE, "
                   "DATETIME_PRECISION, CHARACTER_SET_NAME, COLLATION_NAME, "
                   "COLUMN_TYPE, PRIVILEGES FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'information_schema' "
                   "AND TABLE_NAME = 'ST_GEOMETRY_COLUMNS' ORDER BY ORDINAL_POSITION",
            .column_names = columns_metadata_columns,
            .column_count = sizeof(columns_metadata_columns) / sizeof(columns_metadata_columns[0]),
            .values = columns_metadata_values,
            .row_count = sizeof(columns_metadata_values) / sizeof(columns_metadata_values[0]) /
                         (sizeof(columns_metadata_columns) / sizeof(columns_metadata_columns[0])),
            .warning_count = 0U,
            .context = "st geometry columns metadata",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_information_schema_st_geometry_columns_reopen_and_handles(void) {
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_zero[] = {"0"};
    static const char *const count_eight[] = {"8"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "first") != 0 ||
        make_test_path(second_path, sizeof(second_path), "second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first st geometry db");
    failures +=
        expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second st geometry db");
    failures += seed_spatial_schema(first);
    failures += expect_query(
        first,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.ST_GEOMETRY_COLUMNS",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_eight,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "first handle st geometry count",
        }
    );
    failures += expect_query(
        second,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.ST_GEOMETRY_COLUMNS",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "second handle st geometry count",
        }
    );
    failures += read_file_at(first_path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after st geometry metadata query"
    );

    mylite_close(first);
    first = NULL;
    mylite_close(second);
    second = NULL;

    failures +=
        expect_int(mylite_open(first_path, &first), MYLITE_OK, "reopen first st geometry db");
    failures += expect_query(
        first,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.ST_GEOMETRY_COLUMNS",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_eight,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "reopened st geometry count",
        }
    );

    mylite_close(first);
    remove_related_files(first_path);
    remove_related_files(second_path);
    return failures;
}

static int seed_spatial_schema(mylite_db *database) {
    int failures = 0;

    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE spatial_meta ("
        "id INT, g GEOMETRY, p POINT NOT NULL, ls LINESTRING, poly POLYGON, "
        "mp MULTIPOINT, mls MULTILINESTRING, mpoly MULTIPOLYGON, gc GEOMETRYCOLLECTION)"
    );
    return failures;
}

static int expect_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

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
    if (result != NULL) {
        failures += expect_size(mylite_result_warning_count(result), 0U, sql);
    }
    mylite_result_free(result);
    return failures;
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

    failures +=
        expect_size(mylite_result_column_count(result), expected.column_count, expected.context);
    failures += expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures +=
        expect_size(mylite_result_warning_count(result), expected.warning_count, expected.context);

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

static int expect_status(mylite_db *database, struct expected_status expected) {
    static const char *const status_columns[] = {"@@warning_count", "ROW_COUNT()"};
    const char *const status_values[] = {expected.warning_count, expected.row_count};

    return expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count, ROW_COUNT()",
            .column_names = status_columns,
            .column_count = 2U,
            .values = status_values,
            .row_count = 1U,
            .warning_count = 0U,
            .context = expected.context,
        }
    );
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_information_schema_st_geometry_columns_%s_%d.mylite",
        name,
        current_process_id()
    );

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

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        fprintf(stderr, "%s: expected readable file\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0 || fread(buffer, 1U, size, file) != size) {
        fprintf(stderr, "%s: failed to read expected bytes\n", path);
        fclose(file);
        return 1;
    }
    fclose(file);
    return 0;
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
    if (actual != NULL && expected != NULL && strcmp(actual, expected) == 0) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected %s, got %s\n",
        context,
        expected == NULL ? "NULL" : expected,
        actual == NULL ? "NULL" : actual
    );
    return 1;
}

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: byte mismatch\n", context);
        return 1;
    }
    return 0;
}
