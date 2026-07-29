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
    path_suffix_capacity = 16,
    mysql_error_invalid_gis_data = 3037,
    mysql_error_unexpected_geometry_type = 3516,
    mysql_error_srs_not_found = 3548,
    mysql_error_cannot_get_geometry_object = 1416,
    mysql_binary_collation_id = 63,
    mysql_utf8mb4_0900_ai_ci_collation_id = 255,
    mysql_approximate_decimals = 31,
    mysql_spatial_text_display_length = 268435456,
    mysql_spatial_integer_property_display_length = 10,
    mysql_spatial_double_display_length = 23,
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

static const unsigned char point_1_2_internal[] = {
    0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xf0, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
};
static const unsigned char point_1_2_wkb[] = {
    0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xf0, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
};

static int test_scalar_spatial_functions(void);
static int test_table_backed_spatial_functions_and_reopen(void);
static int test_spatial_function_diagnostics(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_dml_ok(mylite_db *database, const char *sql, struct expected_dml_result expected);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_spatial_bytes(mylite_db *database);
static int expect_spatial_metadata(mylite_db *database);
static int open_app_database(
    mylite_db **out_database,
    const char *name,
    char *path,
    size_t path_size
);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int expect_result_bytes(
    const mylite_result *result,
    size_t row,
    size_t column,
    const unsigned char *expected,
    size_t expected_size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_scalar_spatial_functions();
    failures += test_table_backed_spatial_functions_and_reopen();
    failures += test_spatial_function_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_scalar_spatial_functions(void) {
    static const char *const scalar_values[] = {
        "POINT(1 2)",
        "POINT",
        "0",
        "1",
        "2",
        "GEOMETRYCOLLECTION EMPTY",
    };
    static const char *const constructor_values[] = {
        "LINESTRING(0 0,1 1)",
        "POLYGON((0 0,1 0,1 1,0 0))",
        "MULTIPOINT((0 0),(1 1))",
        "MULTILINESTRING((0 0,1 1))",
        "MULTIPOLYGON(((0 0,1 0,1 1,0 0)))",
        "GEOMETRYCOLLECTION(POINT(1 2),LINESTRING(0 0,1 1))",
    };
    static const char *const from_wkb_values[] = {"POINT(1 2)"};
    static const char *const null_values[] = {NULL, NULL, NULL, NULL, NULL, NULL};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += open_app_database(&database, "scalar", path, sizeof(path));
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ST_AsText(Point(1, 2)), ST_GeometryType(Point(1, 2)), "
                   "ST_SRID(Point(1, 2)), ST_X(Point(1, 2)), ST_Y(Point(1, 2)), "
                   "ST_AsText(ST_GeomFromText('GEOMETRYCOLLECTION EMPTY'))",
            .column_count = sizeof(scalar_values) / sizeof(scalar_values[0]),
            .values = scalar_values,
            .row_count = 1U,
            .context = "scalar spatial functions",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ST_AsText(LineString(Point(0, 0), Point(1, 1))), "
                   "ST_AsText(Polygon(LineString(Point(0, 0), Point(1, 0), "
                   "Point(1, 1), Point(0, 0)))), "
                   "ST_AsText(MultiPoint(Point(0, 0), Point(1, 1))), "
                   "ST_AsText(MultiLineString(LineString(Point(0, 0), Point(1, 1)))), "
                   "ST_AsText(MultiPolygon(Polygon(LineString(Point(0, 0), "
                   "Point(1, 0), Point(1, 1), Point(0, 0))))), "
                   "ST_AsText(GeometryCollection(Point(1, 2), "
                   "LineString(Point(0, 0), Point(1, 1))))",
            .column_count = sizeof(constructor_values) / sizeof(constructor_values[0]),
            .values = constructor_values,
            .row_count = 1U,
            .context = "spatial collection constructors",
        }
    );
    failures += execute_ok(database, "SELECT Point(1, 2), ST_AsWKB(Point(1, 2))", &result);
    if (failures == 0) {
        failures += mylite_test_expect_size(
            mylite_result_column_count(result),
            2U,
            "spatial bytes columns"
        );
        failures +=
            mylite_test_expect_size(mylite_result_row_count(result), 1U, "spatial bytes rows");
        failures += expect_result_bytes(
            result,
            0U,
            0U,
            point_1_2_internal,
            sizeof(point_1_2_internal),
            "Point(1, 2) internal bytes"
        );
        failures += expect_result_bytes(
            result,
            0U,
            1U,
            point_1_2_wkb,
            sizeof(point_1_2_wkb),
            "ST_AsWKB(Point(1, 2)) bytes"
        );
    }
    mylite_result_free(result);
    result = NULL;

    failures += expect_spatial_metadata(database);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ST_AsText(ST_GeomFromWKB(ST_AsWKB(Point(1, 2))))",
            .column_count = sizeof(from_wkb_values) / sizeof(from_wkb_values[0]),
            .values = from_wkb_values,
            .row_count = 1U,
            .context = "spatial WKB roundtrip",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ST_AsText(NULL), ST_AsWKB(NULL), ST_GeometryType(NULL), "
                   "ST_SRID(NULL), ST_X(NULL), ST_Y(NULL)",
            .column_count = sizeof(null_values) / sizeof(null_values[0]),
            .values = null_values,
            .row_count = 1U,
            .context = "spatial null propagation",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_spatial_functions_and_reopen(void) {
    static const char *const values_initial[] = {
        "1",
        "POINT(1 2)",
        "POINT",
        "0",
        "POINT(1 2)",
        "2",
        "LINESTRING(0 0,1 1)",
        "LINESTRING",
        "0",
        "LINESTRING(0 0,1 1)",
        "3",
        NULL,
        NULL,
        NULL,
        NULL,
    };
    static const char *const values_updated[] = {"3", "POINT(5 6)", "5", "6"};
    static const char *const values_reopen[] = {
        "1",
        "POINT(1 2)",
        "2",
        "LINESTRING(0 0,1 1)",
        "3",
        "POINT(5 6)",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "table", path, sizeof(path));
    failures += execute_ok(
        database,
        "CREATE TABLE spatial_values(id INT PRIMARY KEY, g GEOMETRY, txt VARCHAR(80))",
        NULL
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO spatial_values VALUES "
        "(1, Point(1, 2), NULL), "
        "(2, ST_GeomFromText('LINESTRING(0 0,1 1)'), NULL), "
        "(3, NULL, NULL)",
        (struct expected_dml_result){.affected_rows = 3, .warning_count = 0U}
    );
    failures += expect_spatial_bytes(database);
    failures += expect_dml_ok(
        database,
        "UPDATE spatial_values SET txt = ST_AsText(g) WHERE g IS NOT NULL",
        (struct expected_dml_result){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, ST_AsText(g), ST_GeometryType(g), ST_SRID(g), txt "
                   "FROM spatial_values ORDER BY id",
            .column_count = sizeof(values_initial) / sizeof(values_initial[0]) / 3U,
            .values = values_initial,
            .row_count = 3U,
            .context = "table-backed spatial rows",
        }
    );
    failures += expect_dml_ok(
        database,
        "UPDATE spatial_values SET g = ST_GeomFromWKB(ST_AsWKB(Point(5, 6))) WHERE id = 3",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, ST_AsText(g), ST_X(g), ST_Y(g) FROM spatial_values WHERE id = 3",
            .column_count = 4U,
            .values = values_updated,
            .row_count = 1U,
            .context = "updated spatial row",
        }
    );

    mylite_close(database);
    database = NULL;

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "reopen spatial database");
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, ST_AsText(g) FROM spatial_values ORDER BY id",
            .column_count = 2U,
            .values = values_reopen,
            .row_count = 3U,
            .context = "reopened spatial rows",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_spatial_function_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += execute_ok(
        database,
        "CREATE TABLE spatial_values(id INT PRIMARY KEY, g GEOMETRY, txt VARCHAR(80))",
        NULL
    );
    failures += execute_error(
        database,
        "SELECT ST_GeomFromText('bad')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_gis_data,
            .sqlstate = "22023",
            .message_part = "Invalid GIS data provided to function st_geomfromtext",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_PointFromText('LINESTRING(0 0,1 1)')",
        (struct expected_sql_error){
            .code = mysql_error_unexpected_geometry_type,
            .sqlstate = "22S01",
            .message_part = "geometry of unexpected type LINESTRING in st_pointfromtext",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_GeomFromWKB(X'00')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_gis_data,
            .sqlstate = "22023",
            .message_part = "Invalid GIS data provided to function st_geomfromwkb",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_GeomFromText('POINT(1 2)', 1)",
        (struct expected_sql_error){
            .code = mysql_error_srs_not_found,
            .sqlstate = "SR001",
            .message_part = "There's no spatial reference system with SRID 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO spatial_values VALUES (4, 'bad', NULL)",
        (struct expected_sql_error){
            .code = mysql_error_cannot_get_geometry_object,
            .sqlstate = "22003",
            .message_part = "Cannot get geometry object from data you send to the GEOMETRY field",
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
        fprintf(stderr, "%s: expected success, got %d: %s\n", sql, rc, mylite_errmsg(database));
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
    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, sql);
    failures += mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += mylite_test_expect_contains(mylite_errmsg(database), expected.message_part, sql);
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

    if (failures != 0) {
        return failures;
    }
    failures += mylite_test_expect_size(mylite_result_column_count(result), 0U, sql);
    failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, sql);
    failures +=
        mylite_test_expect_int64(mylite_result_affected_rows(result), expected.affected_rows, sql);
    failures +=
        mylite_test_expect_size(mylite_result_warning_count(result), expected.warning_count, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, expected.sql, &result);

    if (failures != 0) {
        return failures;
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
    for (size_t row = 0U; row < expected.row_count; ++row) {
        for (size_t column = 0U; column < expected.column_count; ++column) {
            size_t value_index = (row * expected.column_count) + column;

            failures += expect_result_value(
                result,
                row,
                column,
                expected.values[value_index],
                expected.context
            );
        }
    }
    mylite_result_free(result);
    return failures;
}

static int expect_spatial_bytes(mylite_db *database) {
    mylite_result *result = NULL;
    int failures =
        execute_ok(database, "SELECT g, ST_AsWKB(g) FROM spatial_values WHERE id = 1", &result);

    if (failures == 0) {
        failures += mylite_test_expect_size(
            mylite_result_column_count(result),
            2U,
            "stored spatial bytes columns"
        );
        failures += mylite_test_expect_size(
            mylite_result_row_count(result),
            1U,
            "stored spatial bytes rows"
        );
        failures += expect_result_bytes(
            result,
            0U,
            0U,
            point_1_2_internal,
            sizeof(point_1_2_internal),
            "stored Point(1, 2) internal bytes"
        );
        failures += expect_result_bytes(
            result,
            0U,
            1U,
            point_1_2_wkb,
            sizeof(point_1_2_wkb),
            "stored ST_AsWKB(Point(1, 2)) bytes"
        );
    }
    mylite_result_free(result);
    return failures;
}

static int expect_spatial_metadata(mylite_db *database) {
    mylite_result *result = NULL;
    int failures = execute_ok(
        database,
        "SELECT Point(1, 2), ST_AsWKB(Point(1, 2)), ST_AsText(Point(1, 2)), "
        "ST_SRID(Point(1, 2)), ST_X(Point(1, 2))",
        &result
    );

    if (failures == 0) {
        failures += mylite_test_expect_int(
            (int)mylite_result_column_type(result, 0U),
            MYLITE_RESULT_COLUMN_TYPE_GEOMETRY,
            "Point() metadata type"
        );
        failures += mylite_test_expect_int(
            (int)mylite_result_column_flags(result, 0U),
            MYLITE_RESULT_COLUMN_FLAG_BINARY,
            "Point() metadata flags"
        );
        failures += mylite_test_expect_uint32(
            mylite_result_column_collation_id(result, 0U),
            mysql_binary_collation_id,
            "Point() metadata collation"
        );
        failures += mylite_test_expect_uint64(
            mylite_result_column_display_length(result, 0U),
            UINT32_MAX,
            "Point() metadata display length"
        );
        failures += mylite_test_expect_uint16(
            mylite_result_column_decimals(result, 0U),
            0U,
            "Point() metadata decimals"
        );
        failures += mylite_test_expect_int(
            (int)mylite_result_column_type(result, 1U),
            MYLITE_RESULT_COLUMN_TYPE_LONG_BLOB,
            "ST_AsWKB() metadata type"
        );
        failures += mylite_test_expect_uint32(
            mylite_result_column_flags(result, 1U),
            MYLITE_RESULT_COLUMN_FLAG_BINARY,
            "ST_AsWKB() metadata flags"
        );
        failures += mylite_test_expect_uint32(
            mylite_result_column_collation_id(result, 1U),
            mysql_binary_collation_id,
            "ST_AsWKB() metadata collation"
        );
        failures += mylite_test_expect_uint64(
            mylite_result_column_display_length(result, 1U),
            UINT32_MAX,
            "ST_AsWKB() metadata display length"
        );
        failures += mylite_test_expect_uint16(
            mylite_result_column_decimals(result, 1U),
            mysql_approximate_decimals,
            "ST_AsWKB() metadata decimals"
        );
        failures += mylite_test_expect_int(
            (int)mylite_result_column_type(result, 2U),
            MYLITE_RESULT_COLUMN_TYPE_LONG_BLOB,
            "ST_AsText() metadata type"
        );
        failures += mylite_test_expect_uint32(
            mylite_result_column_flags(result, 2U),
            0U,
            "ST_AsText() metadata flags"
        );
        failures += mylite_test_expect_uint32(
            mylite_result_column_collation_id(result, 2U),
            mysql_utf8mb4_0900_ai_ci_collation_id,
            "ST_AsText() metadata collation"
        );
        failures += mylite_test_expect_uint64(
            mylite_result_column_display_length(result, 2U),
            mysql_spatial_text_display_length,
            "ST_AsText() metadata display length"
        );
        failures += mylite_test_expect_uint16(
            mylite_result_column_decimals(result, 2U),
            mysql_approximate_decimals,
            "ST_AsText() metadata decimals"
        );
        failures += mylite_test_expect_int(
            (int)mylite_result_column_type(result, 3U),
            MYLITE_RESULT_COLUMN_TYPE_LONGLONG,
            "ST_SRID() metadata type"
        );
        failures += mylite_test_expect_uint32(
            mylite_result_column_flags(result, 3U),
            MYLITE_RESULT_COLUMN_FLAG_BINARY | MYLITE_RESULT_COLUMN_FLAG_NUM,
            "ST_SRID() metadata flags"
        );
        failures += mylite_test_expect_uint64(
            mylite_result_column_display_length(result, 3U),
            mysql_spatial_integer_property_display_length,
            "ST_SRID() metadata display length"
        );
        failures += mylite_test_expect_uint16(
            mylite_result_column_decimals(result, 3U),
            0U,
            "ST_SRID() metadata decimals"
        );
        failures += mylite_test_expect_int(
            (int)mylite_result_column_type(result, 4U),
            MYLITE_RESULT_COLUMN_TYPE_DOUBLE,
            "ST_X() metadata type"
        );
        failures += mylite_test_expect_uint32(
            mylite_result_column_flags(result, 4U),
            MYLITE_RESULT_COLUMN_FLAG_BINARY | MYLITE_RESULT_COLUMN_FLAG_NUM,
            "ST_X() metadata flags"
        );
        failures += mylite_test_expect_uint64(
            mylite_result_column_display_length(result, 4U),
            mysql_spatial_double_display_length,
            "ST_X() metadata display length"
        );
        failures += mylite_test_expect_uint16(
            mylite_result_column_decimals(result, 4U),
            mysql_approximate_decimals,
            "ST_X() metadata decimals"
        );
    }
    mylite_result_free(result);
    return failures;
}

static int open_app_database(
    mylite_db **out_database,
    const char *name,
    char *path,
    size_t path_size
) {
    int failures = 0;

    if (mylite_test_make_path(path, path_size, name) != 0) {
        return 1;
    }
    remove_related_files(path);
    failures += mylite_test_expect_int(mylite_open(path, out_database), MYLITE_OK, name);
    if (failures == 0) {
        failures += execute_ok(*out_database, "CREATE DATABASE app", NULL);
    }
    if (failures == 0) {
        failures += execute_ok(*out_database, "USE app", NULL);
    }
    return failures;
}

static void remove_related_files(const char *path) {
    remove_with_suffix(path, "");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char buffer[test_path_capacity + path_suffix_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(buffer)) {
        (void)remove(buffer);
    }
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
            fprintf(stderr, "%s: expected NULL, got [%s]\n", context, actual);
            return 1;
        }
        return 0;
    }
    return mylite_test_expect_text(actual, expected, context);
}

static int expect_result_bytes(
    const mylite_result *result,
    size_t row,
    size_t column,
    const unsigned char *expected,
    size_t expected_size,
    const char *context
) {
    const void *actual = mylite_result_value_bytes(result, row, column);
    size_t actual_size = mylite_result_value_size(result, row, column);

    if (actual == NULL || actual_size != expected_size ||
        memcmp(actual, expected, expected_size) != 0) {
        fprintf(stderr, "%s: bytes did not match\n", context);
        return 1;
    }
    return 0;
}
