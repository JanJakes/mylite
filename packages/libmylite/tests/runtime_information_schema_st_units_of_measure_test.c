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

static int test_information_schema_st_units_of_measure_queries(void);
static int test_information_schema_st_units_of_measure_file_backed_safety(void);
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

    failures += test_information_schema_st_units_of_measure_queries();
    failures += test_information_schema_st_units_of_measure_file_backed_safety();

    return failures == 0 ? 0 : 1;
}

static int test_information_schema_st_units_of_measure_queries(void) {
    static const char *const unit_columns[] = {
        "UNIT_NAME",
        "UNIT_TYPE",
        "CONVERSION_FACTOR",
        "DESCRIPTION",
    };
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_forty_seven[] = {"47"};
    static const char *const factor_column[] = {"CONVERSION_FACTOR"};
    static const char *const metre_factor[] = {"1"};
    static const char *const unit_rows[] = {
        "British chain (Benoit 1895 A)",
        "LINEAR",
        "20.1167824",
        "",
        "British chain (Benoit 1895 B)",
        "LINEAR",
        "20.116782494375872",
        "",
        "British chain (Sears 1922 truncated)",
        "LINEAR",
        "20.116756",
        "",
        "British chain (Sears 1922)",
        "LINEAR",
        "20.116765121552632",
        "",
        "British foot (1865)",
        "LINEAR",
        "0.30480083333333335",
        "",
        "British foot (1936)",
        "LINEAR",
        "0.3048007491",
        "",
        "British foot (Benoit 1895 A)",
        "LINEAR",
        "0.3047997333333333",
        "",
        "British foot (Benoit 1895 B)",
        "LINEAR",
        "0.30479973476327077",
        "",
        "British foot (Sears 1922 truncated)",
        "LINEAR",
        "0.30479933333333337",
        "",
        "British foot (Sears 1922)",
        "LINEAR",
        "0.3047994715386762",
        "",
        "British link (Benoit 1895 A)",
        "LINEAR",
        "0.201167824",
        "",
        "British link (Benoit 1895 B)",
        "LINEAR",
        "0.2011678249437587",
        "",
        "British link (Sears 1922 truncated)",
        "LINEAR",
        "0.20116756",
        "",
        "British link (Sears 1922)",
        "LINEAR",
        "0.2011676512155263",
        "",
        "British yard (Benoit 1895 A)",
        "LINEAR",
        "0.9143992",
        "",
        "British yard (Benoit 1895 B)",
        "LINEAR",
        "0.9143992042898124",
        "",
        "British yard (Sears 1922 truncated)",
        "LINEAR",
        "0.914398",
        "",
        "British yard (Sears 1922)",
        "LINEAR",
        "0.9143984146160288",
        "",
        "centimetre",
        "LINEAR",
        "0.01",
        "",
        "chain",
        "LINEAR",
        "20.1168",
        "",
        "Clarke's chain",
        "LINEAR",
        "20.1166195164",
        "",
        "Clarke's foot",
        "LINEAR",
        "0.3047972654",
        "",
        "Clarke's link",
        "LINEAR",
        "0.201166195164",
        "",
        "Clarke's yard",
        "LINEAR",
        "0.9143917962",
        "",
        "fathom",
        "LINEAR",
        "1.8288",
        "",
        "foot",
        "LINEAR",
        "0.3048",
        "",
        "German legal metre",
        "LINEAR",
        "1.0000135965",
        "",
        "Gold Coast foot",
        "LINEAR",
        "0.3047997101815088",
        "",
        "Indian foot",
        "LINEAR",
        "0.30479951024814694",
        "",
        "Indian foot (1937)",
        "LINEAR",
        "0.30479841",
        "",
        "Indian foot (1962)",
        "LINEAR",
        "0.3047996",
        "",
        "Indian foot (1975)",
        "LINEAR",
        "0.3047995",
        "",
        "Indian yard",
        "LINEAR",
        "0.9143985307444408",
        "",
        "Indian yard (1937)",
        "LINEAR",
        "0.91439523",
        "",
        "Indian yard (1962)",
        "LINEAR",
        "0.9143988",
        "",
        "Indian yard (1975)",
        "LINEAR",
        "0.9143985",
        "",
        "kilometre",
        "LINEAR",
        "1000",
        "",
        "link",
        "LINEAR",
        "0.201168",
        "",
        "metre",
        "LINEAR",
        "1",
        "",
        "millimetre",
        "LINEAR",
        "0.001",
        "",
        "nautical mile",
        "LINEAR",
        "1852",
        "",
        "Statute mile",
        "LINEAR",
        "1609.344",
        "",
        "US survey chain",
        "LINEAR",
        "20.11684023368047",
        "",
        "US survey foot",
        "LINEAR",
        "0.30480060960121924",
        "",
        "US survey link",
        "LINEAR",
        "0.2011684023368047",
        "",
        "US survey mile",
        "LINEAR",
        "1609.3472186944375",
        "",
        "yard",
        "LINEAR",
        "0.9144",
        "",
    };
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
        "ST_UNITS_OF_MEASURE",
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
        "ST_UNITS_OF_MEASURE",
        "UNIT_NAME",
        "1",
        NULL,
        "YES",
        "varchar",
        "255",
        "1020",
        NULL,
        NULL,
        NULL,
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "varchar(255)",
        "select",
        "ST_UNITS_OF_MEASURE",
        "UNIT_TYPE",
        "2",
        NULL,
        "YES",
        "varchar",
        "7",
        "28",
        NULL,
        NULL,
        NULL,
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "varchar(7)",
        "select",
        "ST_UNITS_OF_MEASURE",
        "CONVERSION_FACTOR",
        "3",
        NULL,
        "YES",
        "double",
        NULL,
        NULL,
        "22",
        NULL,
        NULL,
        NULL,
        NULL,
        "double",
        "select",
        "ST_UNITS_OF_MEASURE",
        "DESCRIPTION",
        "4",
        NULL,
        "YES",
        "varchar",
        "255",
        "1020",
        NULL,
        NULL,
        NULL,
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "varchar(255)",
        "select",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open(":memory:", &database), MYLITE_OK, "open st units db");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM INFORMATION_SCHEMA.ST_UNITS_OF_MEASURE LIMIT 0",
            .column_names = unit_columns,
            .column_count = sizeof(unit_columns) / sizeof(unit_columns[0]),
            .values = NULL,
            .row_count = 0U,
            .warning_count = 0U,
            .context = "st units wildcard columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT UNIT_NAME, UNIT_TYPE, CONVERSION_FACTOR, DESCRIPTION "
                   "FROM INFORMATION_SCHEMA.ST_UNITS_OF_MEASURE ORDER BY UNIT_NAME",
            .column_names = unit_columns,
            .column_count = sizeof(unit_columns) / sizeof(unit_columns[0]),
            .values = unit_rows,
            .row_count = sizeof(unit_rows) / sizeof(unit_rows[0]) /
                         (sizeof(unit_columns) / sizeof(unit_columns[0])),
            .warning_count = 0U,
            .context = "st units exact rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM information_schema.st_units_of_measure "
                   "WHERE UNIT_TYPE = 'LINEAR'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_forty_seven,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "st units linear count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.ST_UNITS_OF_MEASURE "
                   "WHERE DESCRIPTION = ''",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_forty_seven,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "st units empty description count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT u.CONVERSION_FACTOR "
                   "FROM INFORMATION_SCHEMA.ST_UNITS_OF_MEASURE AS u "
                   "WHERE u.UNIT_NAME = 'metre'",
            .column_names = factor_column,
            .column_count = 1U,
            .values = metre_factor,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "st units alias projection",
        }
    );
    failures += expect_status(
        database,
        (struct expected_status){
            .warning_count = "0",
            .row_count = "-1",
            .context = "st units alias status",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, "
                   "TABLE_ROWS, DATA_LENGTH, AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'information_schema' "
                   "AND TABLE_NAME = 'ST_UNITS_OF_MEASURE'",
            .column_names = system_table_columns,
            .column_count = sizeof(system_table_columns) / sizeof(system_table_columns[0]),
            .values = system_table_values,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "st units system table row",
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
                   "AND TABLE_NAME = 'ST_UNITS_OF_MEASURE' ORDER BY ORDINAL_POSITION",
            .column_names = columns_metadata_columns,
            .column_count = sizeof(columns_metadata_columns) / sizeof(columns_metadata_columns[0]),
            .values = columns_metadata_values,
            .row_count = sizeof(columns_metadata_values) / sizeof(columns_metadata_values[0]) /
                         (sizeof(columns_metadata_columns) / sizeof(columns_metadata_columns[0])),
            .warning_count = 0U,
            .context = "st units columns metadata",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_information_schema_st_units_of_measure_file_backed_safety(void) {
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_forty_seven[] = {"47"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "file") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open file st units db");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.ST_UNITS_OF_MEASURE",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_forty_seven,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "file st units count",
        }
    );
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after st units metadata query"
    );

    mylite_close(database);
    remove_related_files(path);
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
        "/tmp/mylite_information_schema_st_units_of_measure_%s_%d.mylite",
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
