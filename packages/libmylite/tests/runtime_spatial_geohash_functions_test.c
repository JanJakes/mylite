#include <mylite/mylite.h>

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#  include <process.h>
#else
#  include <unistd.h>
#endif

enum {
    test_path_capacity = 1024,
    mysql_error_invalid_geohash = 1411,
    mysql_error_numeric_value_out_of_range = 1690,
    mysql_error_incorrect_type_for_argument = 3064,
    mysql_error_srs_not_found = 3548,
    geohash_projection_column_count = 5,
    geohash_update_column_count = 2,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_query {
    const char *sql;
    size_t column_count;
    const char *const *values;
    size_t row_count;
    const char *context;
};

struct expected_dml_result {
    int64_t affected_rows;
    size_t warning_count;
};

static int test_scalar_spatial_geohash_functions(void);
static int test_table_backed_spatial_geohash_functions(void);
static int test_spatial_geohash_diagnostics(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_dml_ok(mylite_db *database, const char *sql, struct expected_dml_result expected);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);

int main(void) {
    int failures = 0;

    failures += test_scalar_spatial_geohash_functions();
    failures += test_table_backed_spatial_geohash_functions();
    failures += test_spatial_geohash_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_scalar_spatial_geohash_functions(void) {
    static const char *const encode_values[] = {
        "xbpbpbpbpb",
        "000000000000000",
        "mh2n0p0581",
        "mh2n0p0581",
        "-20",
        "45",
        "POINT(45 -20)",
        "0",
        ("s0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
         "0"
         "0000000000"),
        "s",
        "s0",
        "s000000000",
        "s000000000",
    };
    static const char *const decode_values[] = {
        "-68",
        "-158",
        "POINT(-158 -68)",
        "22",
        "22",
        "POINT(22 22)",
        "3",
        "6",
        "POINT(6 3)",
        "-20.000007",
        "45",
    };
    static const char *const srid_values[] = {
        "POINT(-20 45)",
        "4326",
        "mh2n0p0581",
        "mh2n0p0581",
    };
    static const char *const null_values[] = {NULL, NULL, NULL, NULL};
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open geohash database");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ST_GeoHash(180,0,10), ST_GeoHash(-180,-90,15), "
                   "ST_GeoHash(45,-20,10), ST_GeoHash(Point(45,-20),10), "
                   "ST_LatFromGeoHash(ST_GeoHash(45,-20,10)), "
                   "ST_LongFromGeoHash(ST_GeoHash(45,-20,10)), "
                   "ST_AsText(ST_PointFromGeoHash(ST_GeoHash(45,-20,10),0)), "
                   "ST_SRID(ST_PointFromGeoHash(ST_GeoHash(45,-20,10),0)), "
                   "ST_GeoHash(0,0,100), ST_GeoHash(0,0,1), "
                   "ST_GeoHash(0,0,2), ST_GeoHash(0,0,10), ST_GeoHash('abc',0,10)",
            .column_count = sizeof(encode_values) / sizeof(encode_values[0]),
            .values = encode_values,
            .row_count = 1U,
            .context = "geohash encode decode basics",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ST_LatFromGeoHash('0'), ST_LongFromGeoHash('0'), "
                   "ST_AsText(ST_PointFromGeoHash('0',0)), ST_LatFromGeoHash('s'), "
                   "ST_LongFromGeoHash('s'), ST_AsText(ST_PointFromGeoHash('s',0)), "
                   "ST_LatFromGeoHash('s0'), ST_LongFromGeoHash('s0'), "
                   "ST_AsText(ST_PointFromGeoHash('s0',0)), "
                   "ST_LatFromGeoHash('mh2n0p0580'), ST_LongFromGeoHash('mh2n0p0580')",
            .column_count = sizeof(decode_values) / sizeof(decode_values[0]),
            .values = decode_values,
            .row_count = 1U,
            .context = "geohash decoded coordinate display",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ST_AsText(ST_PointFromGeoHash('mh2n0p0581',4326)), "
                   "ST_SRID(ST_PointFromGeoHash('mh2n0p0581',4326)), "
                   "ST_GeoHash(ST_PointFromGeoHash('mh2n0p0581',0),10), "
                   "ST_GeoHash(ST_PointFromGeoHash('mh2n0p0581',4326),10)",
            .column_count = sizeof(srid_values) / sizeof(srid_values[0]),
            .values = srid_values,
            .row_count = 1U,
            .context = "geohash srid 4326 axis handling",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ST_GeoHash(NULL,0,10), ST_GeoHash(0,NULL,10), "
                   "ST_GeoHash(0,0,NULL), ST_GeoHash(NULL,10)",
            .column_count = sizeof(null_values) / sizeof(null_values[0]),
            .values = null_values,
            .row_count = 1U,
            .context = "geohash null propagation",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_table_backed_spatial_geohash_functions(void) {
    static const char *const projection_values[] = {
        "1",
        "mh2n0p0581",
        "-20",
        "45",
        "POINT(45 -20)",
        "2",
        "s000000000",
        "0",
        "0",
        "POINT(0 0)",
    };
    static const char *const update_values[] = {
        "1",
        "mh2n0p0581",
        "2",
        "s000000000",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "table") != 0) {
        return 1;
    }
    remove_related_files(path);
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open table geohash database");
    failures += execute_ok(database, "CREATE DATABASE spatial_geohash", NULL);
    failures += execute_ok(database, "USE spatial_geohash", NULL);
    failures += execute_ok(
        database,
        "CREATE TABLE geohash_values(id INT PRIMARY KEY, g POINT, hash VARCHAR(120))",
        NULL
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO geohash_values VALUES (1, Point(45,-20), NULL), (2, Point(0,0), NULL)",
        (struct expected_dml_result){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, ST_GeoHash(g,10), ST_LatFromGeoHash(ST_GeoHash(g,10)), "
                   "ST_LongFromGeoHash(ST_GeoHash(g,10)), "
                   "ST_AsText(ST_PointFromGeoHash(ST_GeoHash(g,10),0)) "
                   "FROM geohash_values ORDER BY id",
            .column_count = geohash_projection_column_count,
            .values = projection_values,
            .row_count = 2U,
            .context = "row-backed geohash projection",
        }
    );
    failures += expect_dml_ok(
        database,
        "UPDATE geohash_values SET hash = ST_GeoHash(g,10)",
        (struct expected_dml_result){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, hash FROM geohash_values ORDER BY id",
            .column_count = geohash_update_column_count,
            .values = update_values,
            .row_count = 2U,
            .context = "row-backed geohash update",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_spatial_geohash_diagnostics(void) {
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        expect_int(mylite_open_memory(&database), MYLITE_OK, "open geohash diagnostics database");
    failures += execute_error(
        database,
        "SELECT ST_GeoHash(0,0,0)",
        (struct expected_sql_error){
            .code = mysql_error_numeric_value_out_of_range,
            .sqlstate = "22003",
            .message_part = "max geohash length value is out of range in 'st_geohash'",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_GeoHash(181,0,10)",
        (struct expected_sql_error){
            .code = mysql_error_numeric_value_out_of_range,
            .sqlstate = "22003",
            .message_part = "longitude value is out of range in 'st_geohash'",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_GeoHash(0,91,10)",
        (struct expected_sql_error){
            .code = mysql_error_numeric_value_out_of_range,
            .sqlstate = "22003",
            .message_part = "latitude value is out of range in 'st_geohash'",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_GeoHash(ST_GeomFromText('LINESTRING(0 0,1 1)'),10)",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_type_for_argument,
            .sqlstate = "HY000",
            .message_part = "Incorrect type for argument point in function st_geohash",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_LatFromGeoHash('')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_geohash,
            .sqlstate = "HY000",
            .message_part = "Incorrect geohash value: '' for function ST_LATFROMGEOHASH",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_LongFromGeoHash('!')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_geohash,
            .sqlstate = "HY000",
            .message_part = "Incorrect geohash value: '!' for function ST_LONGFROMGEOHASH",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_AsText(ST_PointFromGeoHash('mh2n0p0581',999999))",
        (struct expected_sql_error){
            .code = mysql_error_srs_not_found,
            .sqlstate = "SR001",
            .message_part = "There's no spatial reference system with SRID 999999.",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_AsText(ST_PointFromGeoHash('mh2n0p0581',-1))",
        (struct expected_sql_error){
            .code = mysql_error_numeric_value_out_of_range,
            .sqlstate = "22003",
            .message_part = "SRID value is out of range in 'st_pointfromgeohash'",
        }
    );
    mylite_close(database);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected success, got %d/%s/%s\n",
            sql,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }
    if (out_result != NULL) {
        *out_result = result;
        return 0;
    }
    mylite_result_free(result);
    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    if (rc == MYLITE_OK) {
        fprintf(stderr, "%s: expected error, got success\n", sql);
        mylite_result_free(result);
        return 1;
    }
    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_dml_ok(
    mylite_db *database,
    const char *sql,
    struct expected_dml_result expected
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures == 0) {
        failures += expect_int64(mylite_result_affected_rows(result), expected.affected_rows, sql);
        failures += expect_size(mylite_result_warning_count(result), expected.warning_count, sql);
    }
    mylite_result_free(result);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, expected.sql, &result);

    if (failures == 0) {
        failures += expect_size(
            mylite_result_column_count(result),
            expected.column_count,
            expected.context
        );
        failures +=
            expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
    }
    for (size_t row = 0U; failures == 0 && row < expected.row_count; ++row) {
        for (size_t column = 0U; failures == 0 && column < expected.column_count; ++column) {
            size_t index = (row * expected.column_count) + column;

            failures +=
                expect_result_value(result, row, column, expected.values[index], expected.context);
        }
    }
    mylite_result_free(result);
    return failures;
}

static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
) {
    const char *actual = mylite_result_value_text(result, row, column);

    if (expected == NULL) {
        if (actual != NULL) {
            fprintf(
                stderr,
                "%s: row %zu column %zu expected NULL, got %s\n",
                context,
                row,
                column,
                actual
            );
            return 1;
        }
        return 0;
    }
    if (actual == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: row %zu column %zu expected %s, got %s\n",
            context,
            row,
            column,
            expected,
            actual == NULL ? "NULL" : actual
        );
        return 1;
    }
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

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
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

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || needle == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected message containing %s, got %s\n",
            context,
            needle == NULL ? "NULL" : needle,
            actual == NULL ? "NULL" : actual
        );
        return 1;
    }
    return 0;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_spatial_geohash_%s_%d.mylite",
        name,
        current_process_id()
    );

    return written < 0 || (size_t)written >= path_size ? -1 : 0;
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
    char file_path[test_path_capacity];
    int written = snprintf(file_path, sizeof(file_path), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(file_path)) {
        return;
    }
    (void)remove(file_path);
}
