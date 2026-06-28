#include <mylite/mylite.h>

#include "runtime/mylite_spatial.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#  include <process.h>
#else
#  include <unistd.h>
#endif

enum {
    test_path_capacity = 1024,
    path_suffix_capacity = 16,
    mysql_error_invalid_geojson_option = 1411,
    mysql_error_numeric_value_out_of_range = 1690,
    mysql_error_incorrect_type_for_argument = 3064,
    mysql_error_invalid_json_text = 3141,
    mysql_error_invalid_geojson_missing_member = 3070,
    mysql_error_invalid_geojson_data = 3072,
    mysql_error_unsupported_geojson_dimensions = 3073,
    mysql_error_srs_not_found = 3548,
    mysql_error_geojson_longitude_out_of_range = 3616,
    mysql_error_geojson_latitude_out_of_range = 3617,
};

static const double direct_geojson_max_dec_digits = 2.0;
static const double direct_geojson_fractional_option = 1.5;

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

static const unsigned char point_1_2_internal[] = {
    0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xf0, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
};

static int test_scalar_spatial_geojson_functions(void);
static int test_table_backed_spatial_geojson_functions(void);
static int test_spatial_geojson_diagnostics(void);
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

    failures += test_scalar_spatial_geojson_functions();
    failures += test_table_backed_spatial_geojson_functions();
    failures += test_spatial_geojson_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_scalar_spatial_geojson_functions(void) {
    static const char *const as_basic_values[] = {
        "{\"type\": \"Point\", \"coordinates\": [1.0, 2.0]}",
        "{\"type\": \"LineString\", \"coordinates\": [[0.0, 0.0], [1.0, 1.0]]}",
        ("{\"type\": \"Polygon\", \"coordinates\": [[[0.0, 0.0], [1.0, 0.0], "
         "[1.0, 1.0], [0.0, 0.0]]]}"),
    };
    static const char *const as_collection_values[] = {
        "{\"type\": \"MultiPoint\", \"coordinates\": [[0.0, 0.0], [1.0, 1.0]]}",
        "{\"type\": \"MultiLineString\", \"coordinates\": [[[0.0, 0.0], [1.0, "
        "1.0]], [[2.0, 2.0], [3.0, 3.0]]]}",
        "{\"type\": \"MultiPolygon\", \"coordinates\": [[[[0.0, 0.0], [1.0, "
        "0.0], [1.0, 1.0], [0.0, 0.0]]]]}",
        "{\"type\": \"GeometryCollection\", \"geometries\": [{\"type\": \"Point\", "
        "\"coordinates\": [1.0, 2.0]}, {\"type\": \"LineString\", \"coordinates\": "
        "[[0.0, 0.0], [1.0, 1.0]]}]}",
        "{\"type\": \"GeometryCollection\", \"geometries\": []}",
    };
    static const char *const options_values[] = {
        "{\"type\": \"Point\", \"coordinates\": [11.11, 12.22]}",
        "{\"type\": \"Point\", \"coordinates\": [11.0, 12.0]}",
        "{\"bbox\": [1.0, 2.0, 1.0, 2.0], \"type\": \"Point\", \"coordinates\": [1.0, 2.0]}",
        "{\"crs\": {\"type\": \"name\", \"properties\": {\"name\": \"EPSG:4326\"}}, "
        "\"type\": \"Point\", \"coordinates\": [102.0, 0.0]}",
        "{\"crs\": {\"type\": \"name\", \"properties\": {\"name\": "
        "\"urn:ogc:def:crs:EPSG::4326\"}}, \"bbox\": [102.0, 0.0, 102.0, 0.0], "
        "\"type\": \"Point\", \"coordinates\": [102.0, 0.0]}",
    };
    static const char *const from_values[] = {
        "POINT(0 102)",
        "4326",
        "POINT(102 0)",
        "0",
        "POINT(102.5 0)",
        "POINT(100 0)",
        "LINESTRING(0 0,1 1)",
        "POLYGON((0 0,1 0,1 1,0 0))",
        "MULTIPOINT((0 0),(1 1))",
        "MULTILINESTRING((0 0,1 1))",
        "MULTIPOLYGON(((0 0,1 0,1 1,0 0)))",
        "GEOMETRYCOLLECTION(POINT(1 2),LINESTRING(0 0,1 1))",
    };
    static const char *const feature_values[] = {
        "POINT(1 2)",
        "GEOMETRYCOLLECTION(POINT(1 2),LINESTRING(0 0,1 1))",
        "POINT(1 2)",
        "LINESTRING(0 0,1 1)",
        NULL,
        NULL,
        NULL,
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open GeoJSON database");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ST_AsGeoJSON(Point(1,2)), "
                   "ST_AsGeoJSON(ST_GeomFromText('LINESTRING(0 0,1 1)')), "
                   "ST_AsGeoJSON(ST_GeomFromText('POLYGON((0 0,1 0,1 1,0 0))'))",
            .column_count = sizeof(as_basic_values) / sizeof(as_basic_values[0]),
            .values = as_basic_values,
            .row_count = 1U,
            .context = "GeoJSON basic output",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ST_AsGeoJSON(ST_GeomFromText('MULTIPOINT((0 0),(1 1))')), "
                   "ST_AsGeoJSON(ST_GeomFromText('MULTILINESTRING((0 0,1 1),(2 2,3 3))')), "
                   "ST_AsGeoJSON(ST_GeomFromText('MULTIPOLYGON(((0 0,1 0,1 1,0 0)))')), "
                   "ST_AsGeoJSON(ST_GeomFromText('GEOMETRYCOLLECTION(POINT(1 2),"
                   "LINESTRING(0 0,1 1))')), "
                   "ST_AsGeoJSON(ST_GeomFromText('GEOMETRYCOLLECTION EMPTY'))",
            .column_count = sizeof(as_collection_values) / sizeof(as_collection_values[0]),
            .values = as_collection_values,
            .row_count = 1U,
            .context = "GeoJSON collection output",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ST_AsGeoJSON(ST_GeomFromText('POINT(11.11111 12.22222)'),2), "
                   "ST_AsGeoJSON(ST_GeomFromText('POINT(11.11111 12.22222)'),0), "
                   "ST_AsGeoJSON(ST_GeomFromText('POINT(1 2)',0),4,1), "
                   "ST_AsGeoJSON(ST_GeomFromGeoJSON('{\"type\":\"Point\","
                   "\"coordinates\":[102.0,0.0]}'),4,2), "
                   "ST_AsGeoJSON(ST_GeomFromGeoJSON('{\"type\":\"Point\","
                   "\"coordinates\":[102.0,0.0]}'),4,5)",
            .column_count = sizeof(options_values) / sizeof(options_values[0]),
            .values = options_values,
            .row_count = 1U,
            .context = "GeoJSON precision options",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ST_AsText(ST_GeomFromGeoJSON('{\"type\":\"Point\","
                   "\"coordinates\":[102.0,0.0]}')), "
                   "ST_SRID(ST_GeomFromGeoJSON('{\"type\":\"Point\","
                   "\"coordinates\":[102.0,0.0]}')), "
                   "ST_AsText(ST_GeomFromGeoJSON('{\"type\":\"Point\","
                   "\"coordinates\":[102.0,0.0]}',1,0)), "
                   "ST_SRID(ST_GeomFromGeoJSON('{\"type\":\"Point\","
                   "\"coordinates\":[102.0,0.0]}',1,0)), "
                   "ST_AsText(ST_GeomFromGeoJSON('{\"type\":\"Point\","
                   "\"coordinates\":[102.5,0]}',1,0)), "
                   "ST_AsText(ST_GeomFromGeoJSON('{\"type\":\"Point\","
                   "\"coordinates\":[1e2,0]}',1,0)), "
                   "ST_AsText(ST_GeomFromGeoJSON('{\"type\":\"LineString\","
                   "\"coordinates\":[[0,0],[1,1]]}',1,0)), "
                   "ST_AsText(ST_GeomFromGeoJSON('{\"type\":\"Polygon\","
                   "\"coordinates\":[[[0,0],[1,0],[1,1],[0,0]]]}',1,0)), "
                   "ST_AsText(ST_GeomFromGeoJSON('{\"type\":\"MultiPoint\","
                   "\"coordinates\":[[0,0],[1,1]]}',1,0)), "
                   "ST_AsText(ST_GeomFromGeoJSON('{\"type\":\"MultiLineString\","
                   "\"coordinates\":[[[0,0],[1,1]]]}',1,0)), "
                   "ST_AsText(ST_GeomFromGeoJSON('{\"type\":\"MultiPolygon\","
                   "\"coordinates\":[[[[0,0],[1,0],[1,1],[0,0]]]]}',1,0)), "
                   "ST_AsText(ST_GeomFromGeoJSON('{\"type\":\"GeometryCollection\","
                   "\"geometries\":[{\"type\":\"Point\",\"coordinates\":[1,2]},"
                   "{\"type\":\"LineString\",\"coordinates\":[[0,0],[1,1]]}]}',1,0))",
            .column_count = sizeof(from_values) / sizeof(from_values[0]),
            .values = from_values,
            .row_count = 1U,
            .context = "GeoJSON input values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ST_AsText(ST_GeomFromGeoJSON('{\"type\":\"Feature\","
                   "\"geometry\":{\"type\":\"Point\",\"coordinates\":[1,2]},"
                   "\"properties\":{\"a\":1}}',1,0)), "
                   "ST_AsText(ST_GeomFromGeoJSON('{\"type\":\"FeatureCollection\","
                   "\"features\":[{\"type\":\"Feature\",\"geometry\":{\"type\":\"Point\","
                   "\"coordinates\":[1,2]},\"properties\":{}},{\"type\":\"Feature\","
                   "\"geometry\":{\"type\":\"LineString\",\"coordinates\":[[0,0],[1,1]]},"
                   "\"properties\":{}}]}',1,0)), "
                   "ST_AsText(ST_GeomFromGeoJSON('{\"type\":\"Point\","
                   "\"coordinates\":[1,2,3]}',2,0)), "
                   "ST_AsText(ST_GeomFromGeoJSON('{\"type\":\"LineString\","
                   "\"coordinates\":[[0,0,9],[1,1,8]]}',2,0)), "
                   "ST_AsGeoJSON(NULL), ST_AsText(ST_GeomFromGeoJSON(NULL)), "
                   "ST_AsText(ST_GeomFromGeoJSON('{\"type\":\"Feature\","
                   "\"geometry\":null,\"properties\":{}}',1,0))",
            .column_count = sizeof(feature_values) / sizeof(feature_values[0]),
            .values = feature_values,
            .row_count = 1U,
            .context = "GeoJSON features dimensions nulls",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_table_backed_spatial_geojson_functions(void) {
    static const char *const projection_values[] = {
        "1",
        "{\"type\": \"Point\", \"coordinates\": [1.0, 2.0]}",
        "POINT(1 2)",
        "2",
        "{\"type\": \"LineString\", \"coordinates\": [[0.0, 0.0], [1.0, 1.0]]}",
        "LINESTRING(0 0,1 1)",
    };
    static const char *const update_values[] = {
        "1",
        "{\"type\": \"Point\", \"coordinates\": [1.0, 2.0]}",
        "2",
        "{\"type\": \"LineString\", \"coordinates\": [[0.0, 0.0], [1.0, 1.0]]}",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "table") != 0) {
        return 1;
    }
    remove_related_files(path);
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open GeoJSON table db");
    failures += execute_ok(database, "CREATE DATABASE spatial_geojson", NULL);
    failures += execute_ok(database, "USE spatial_geojson", NULL);
    failures += execute_ok(
        database,
        "CREATE TABLE geojson_values(id INT PRIMARY KEY, g GEOMETRY, doc VARCHAR(500))",
        NULL
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO geojson_values VALUES "
        "(1, ST_GeomFromGeoJSON('{\"type\":\"Point\",\"coordinates\":[1,2]}',1,0), NULL), "
        "(2, ST_GeomFromGeoJSON('{\"type\":\"LineString\","
        "\"coordinates\":[[0,0],[1,1]]}',1,0), NULL)",
        (struct expected_dml_result){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, ST_AsGeoJSON(g), ST_AsText(ST_GeomFromGeoJSON(ST_AsGeoJSON(g),1,0)) "
                   "FROM geojson_values ORDER BY id",
            .column_count = 3U,
            .values = projection_values,
            .row_count = 2U,
            .context = "row-backed GeoJSON projection",
        }
    );
    failures += expect_dml_ok(
        database,
        "UPDATE geojson_values SET doc = ST_AsGeoJSON(g)",
        (struct expected_dml_result){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, doc FROM geojson_values ORDER BY id",
            .column_count = 2U,
            .values = update_values,
            .row_count = 2U,
            .context = "row-backed GeoJSON update",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_spatial_geojson_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open GeoJSON diagnostics db");
    {
        struct mylite_spatial_argument arguments[] = {
            {.bytes = point_1_2_internal, .byte_count = sizeof(point_1_2_internal)},
            {.numeric = direct_geojson_max_dec_digits, .has_numeric = true},
            {.numeric = direct_geojson_fractional_option, .has_numeric = true},
        };
        struct mylite_spatial_result result = {0};
        struct mylite_spatial_error error = {0};

        failures += expect_int(
            mylite_spatial_evaluate(
                MYLITE_SPATIAL_FUNCTION_ST_ASGEOJSON,
                arguments,
                sizeof(arguments) / sizeof(arguments[0]),
                &result,
                &error
            ),
            -1,
            "direct ST_AsGeoJSON fractional option"
        );
        failures += expect_int(
            error.code,
            mysql_error_incorrect_type_for_argument,
            "direct ST_AsGeoJSON fractional option code"
        );
        failures +=
            expect_text(error.sqlstate, "HY000", "direct ST_AsGeoJSON fractional option sqlstate");
        failures += expect_contains(
            error.message,
            "Incorrect type for argument options in function st_asgeojson",
            "direct ST_AsGeoJSON fractional option message"
        );
        mylite_spatial_result_deinit(&result);
    }
    failures += execute_error(
        database,
        "SELECT ST_AsText(ST_GeomFromGeoJSON('{bad}',1,0))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_text,
            .sqlstate = "22032",
            .message_part = "Invalid JSON text in argument 1 to function st_geomfromgeojson",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_AsText(ST_GeomFromGeoJSON('{\"type\":\"point\","
        "\"coordinates\":[1,2]}',1,0))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_geojson_data,
            .sqlstate = "HY000",
            .message_part = "Invalid GeoJSON data provided to function st_geomfromgeojson",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_AsText(ST_GeomFromGeoJSON('{\"type\":\"Point\"}',1,0))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_geojson_missing_member,
            .sqlstate = "HY000",
            .message_part = "Missing required member 'coordinates'",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_AsText(ST_GeomFromGeoJSON('{\"type\":\"Point\","
        "\"coordinates\":[1,2,3]}',1,0))",
        (struct expected_sql_error){
            .code = mysql_error_unsupported_geojson_dimensions,
            .sqlstate = "HY000",
            .message_part = "Unsupported number of coordinate dimensions",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_AsText(ST_GeomFromGeoJSON('{\"type\":\"Point\","
        "\"coordinates\":[1,2]}',0,0))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_geojson_option,
            .sqlstate = "HY000",
            .message_part = "Incorrect option value: '0' for function st_geomfromgeojson",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_AsText(ST_GeomFromGeoJSON(NULL,0,0))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_geojson_option,
            .sqlstate = "HY000",
            .message_part = "Incorrect option value: '0' for function st_geomfromgeojson",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_AsGeoJSON(Point(1,2),2,8)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_geojson_option,
            .sqlstate = "HY000",
            .message_part = "Incorrect options value: '8' for function st_asgeojson",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_AsGeoJSON(Point(1,2),-1)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_geojson_option,
            .sqlstate = "HY000",
            .message_part = "Incorrect max decimal digits value: '-1' for function st_asgeojson",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_AsText(ST_GeomFromGeoJSON('{\"type\":\"Point\","
        "\"coordinates\":[1,2]}',1,999999))",
        (struct expected_sql_error){
            .code = mysql_error_srs_not_found,
            .sqlstate = "SR001",
            .message_part = "There's no spatial reference system with SRID 999999.",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_AsText(ST_GeomFromGeoJSON('{\"type\":\"Point\","
        "\"coordinates\":[1,2]}',1,-1))",
        (struct expected_sql_error){
            .code = mysql_error_numeric_value_out_of_range,
            .sqlstate = "22003",
            .message_part = "SRID value is out of range in 'st_geomfromgeojson'",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_AsText(ST_GeomFromGeoJSON('{\"type\":\"Point\","
        "\"coordinates\":[181,0]}'))",
        (struct expected_sql_error){
            .code = mysql_error_geojson_longitude_out_of_range,
            .sqlstate = "22S02",
            .message_part = "Longitude 181.000000 is out of range",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_AsText(ST_GeomFromGeoJSON('{\"type\":\"Point\","
        "\"coordinates\":[0,91]}'))",
        (struct expected_sql_error){
            .code = mysql_error_geojson_latitude_out_of_range,
            .sqlstate = "22S03",
            .message_part = "Latitude 91.000000 is out of range",
        }
    );

    mylite_close(database);
    remove_related_files(path);
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
        "/tmp/mylite_spatial_geojson_%s_%d.mylite",
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
    remove_with_suffix(path, "-shm");
    remove_with_suffix(path, "-wal");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char buffer[test_path_capacity + path_suffix_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(buffer)) {
        (void)remove(buffer);
    }
}
