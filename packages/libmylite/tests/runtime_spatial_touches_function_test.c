#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#  include <process.h>
#else
#  include <unistd.h>
#endif

enum {
    test_path_capacity = 1024,
    spatial_touches_scalar_case_count = 29,
    spatial_touches_row_count = 6,
    mysql_error_native_function_parameter_count = 1582,
    mysql_error_gis_different_srids = 3033,
    mysql_error_invalid_gis_data = 3037,
    mysql_error_not_implemented_for_geographic_srs = 3618,
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

static int test_scalar_spatial_touches_function(void);
static int test_table_backed_spatial_touches_function(void);
static int test_spatial_touches_diagnostics(void);
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
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);

int main(void) {
    int failures = 0;

    failures += test_scalar_spatial_touches_function();
    failures += test_table_backed_spatial_touches_function();
    failures += test_spatial_touches_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_scalar_spatial_touches_function(void) {
    static const char *const scalar_values[] = {
        "null_left",
        NULL,
        "empty_empty",
        NULL,
        "empty_point",
        NULL,
        "point_same",
        NULL,
        "point_far",
        NULL,
        "line_point_endpoint",
        "1",
        "line_point_mid",
        "0",
        "polygon_point_boundary",
        "1",
        "polygon_point_inside",
        "0",
        "polygon_point_outside",
        "0",
        "polygon_hole_boundary_point",
        "1",
        "polygon_hole_inside_point",
        "0",
        "line_line_endpoint",
        "1",
        "line_t_endpoint_to_mid",
        "1",
        "line_line_cross_mid",
        "0",
        "line_line_overlap",
        "0",
        "line_line_same",
        "0",
        "polygon_line_boundary",
        "1",
        "polygon_line_boundary_to_inside",
        "0",
        "polygon_line_cross",
        "0",
        "polygon_polygon_edge",
        "1",
        "polygon_polygon_point",
        "1",
        "polygon_polygon_overlap",
        "0",
        "polygon_polygon_contains",
        "0",
        "polygon_same",
        "0",
        "multipoint_line_endpoint",
        "1",
        "multipoint_line_endpoint_mid",
        "0",
        "collection_touch_and_disjoint",
        "1",
        "collection_touch_and_inside",
        "0",
    };
    mylite_db *database = NULL;
    int failures =
        mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open scalar database");

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 'null_left', ST_Touches(NULL, Point(1,1)) UNION ALL "
                   "SELECT 'empty_empty', "
                   "ST_Touches(ST_GeomFromText('GEOMETRYCOLLECTION EMPTY'), "
                   "ST_GeomFromText('GEOMETRYCOLLECTION EMPTY')) UNION ALL "
                   "SELECT 'empty_point', "
                   "ST_Touches(ST_GeomFromText('GEOMETRYCOLLECTION EMPTY'), Point(1,1)) "
                   "UNION ALL SELECT 'point_same', ST_Touches(Point(1,1), Point(1,1)) "
                   "UNION ALL SELECT 'point_far', ST_Touches(Point(1,1), Point(2,2)) "
                   "UNION ALL SELECT 'line_point_endpoint', "
                   "ST_Touches(ST_GeomFromText('LINESTRING(0 0,2 2)'), Point(0,0)) "
                   "UNION ALL SELECT 'line_point_mid', "
                   "ST_Touches(ST_GeomFromText('LINESTRING(0 0,2 2)'), Point(1,1)) "
                   "UNION ALL SELECT 'polygon_point_boundary', "
                   "ST_Touches(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))'), "
                   "Point(0,0)) UNION ALL "
                   "SELECT 'polygon_point_inside', "
                   "ST_Touches(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))'), "
                   "Point(1,1)) UNION ALL "
                   "SELECT 'polygon_point_outside', "
                   "ST_Touches(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))'), "
                   "Point(5,5)) UNION ALL "
                   "SELECT 'polygon_hole_boundary_point', "
                   "ST_Touches(ST_GeomFromText('POLYGON((0 0,6 0,6 6,0 6,0 0),"
                   "(2 2,4 2,4 4,2 4,2 2))'), Point(2,3)) UNION ALL "
                   "SELECT 'polygon_hole_inside_point', "
                   "ST_Touches(ST_GeomFromText('POLYGON((0 0,6 0,6 6,0 6,0 0),"
                   "(2 2,4 2,4 4,2 4,2 2))'), Point(3,3)) UNION ALL "
                   "SELECT 'line_line_endpoint', "
                   "ST_Touches(ST_GeomFromText('LINESTRING(0 0,2 2)'), "
                   "ST_GeomFromText('LINESTRING(2 2,4 4)')) UNION ALL "
                   "SELECT 'line_t_endpoint_to_mid', "
                   "ST_Touches(ST_GeomFromText('LINESTRING(0 0,2 0)'), "
                   "ST_GeomFromText('LINESTRING(1 0,1 1)')) UNION ALL "
                   "SELECT 'line_line_cross_mid', "
                   "ST_Touches(ST_GeomFromText('LINESTRING(0 0,2 2)'), "
                   "ST_GeomFromText('LINESTRING(0 2,2 0)')) UNION ALL "
                   "SELECT 'line_line_overlap', "
                   "ST_Touches(ST_GeomFromText('LINESTRING(0 0,4 4)'), "
                   "ST_GeomFromText('LINESTRING(2 2,6 6)')) UNION ALL "
                   "SELECT 'line_line_same', "
                   "ST_Touches(ST_GeomFromText('LINESTRING(0 0,4 4)'), "
                   "ST_GeomFromText('LINESTRING(0 0,4 4)')) UNION ALL "
                   "SELECT 'polygon_line_boundary', "
                   "ST_Touches(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))'), "
                   "ST_GeomFromText('LINESTRING(0 0,4 0)')) UNION ALL "
                   "SELECT 'polygon_line_boundary_to_inside', "
                   "ST_Touches(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))'), "
                   "ST_GeomFromText('LINESTRING(0 0,2 2)')) UNION ALL "
                   "SELECT 'polygon_line_cross', "
                   "ST_Touches(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))'), "
                   "ST_GeomFromText('LINESTRING(-1 2,5 2)')) UNION ALL "
                   "SELECT 'polygon_polygon_edge', "
                   "ST_Touches(ST_GeomFromText('POLYGON((0 0,2 0,2 2,0 2,0 0))'), "
                   "ST_GeomFromText('POLYGON((2 0,4 0,4 2,2 2,2 0))')) UNION ALL "
                   "SELECT 'polygon_polygon_point', "
                   "ST_Touches(ST_GeomFromText('POLYGON((0 0,2 0,2 2,0 2,0 0))'), "
                   "ST_GeomFromText('POLYGON((2 2,4 2,4 4,2 4,2 2))')) UNION ALL "
                   "SELECT 'polygon_polygon_overlap', "
                   "ST_Touches(ST_GeomFromText('POLYGON((0 0,3 0,3 3,0 3,0 0))'), "
                   "ST_GeomFromText('POLYGON((2 2,5 2,5 5,2 5,2 2))')) UNION ALL "
                   "SELECT 'polygon_polygon_contains', "
                   "ST_Touches(ST_GeomFromText('POLYGON((0 0,5 0,5 5,0 5,0 0))'), "
                   "ST_GeomFromText('POLYGON((1 1,2 1,2 2,1 2,1 1))')) UNION ALL "
                   "SELECT 'polygon_same', "
                   "ST_Touches(ST_GeomFromText('POLYGON((0 0,5 0,5 5,0 5,0 0))'), "
                   "ST_GeomFromText('POLYGON((0 0,5 0,5 5,0 5,0 0))')) UNION ALL "
                   "SELECT 'multipoint_line_endpoint', "
                   "ST_Touches(ST_GeomFromText('MULTIPOINT((0 0),(5 5))'), "
                   "ST_GeomFromText('LINESTRING(0 0,2 2)')) UNION ALL "
                   "SELECT 'multipoint_line_endpoint_mid', "
                   "ST_Touches(ST_GeomFromText('MULTIPOINT((0 0),(1 1))'), "
                   "ST_GeomFromText('LINESTRING(0 0,2 2)')) UNION ALL "
                   "SELECT 'collection_touch_and_disjoint', "
                   "ST_Touches(ST_GeomFromText('GEOMETRYCOLLECTION(POINT(0 0),POINT(9 9))'), "
                   "ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))')) UNION ALL "
                   "SELECT 'collection_touch_and_inside', "
                   "ST_Touches(ST_GeomFromText('GEOMETRYCOLLECTION(POINT(0 0),POINT(1 1))'), "
                   "ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))'))",
            .column_count = 2U,
            .values = scalar_values,
            .row_count = spatial_touches_scalar_case_count,
            .context = "scalar spatial touches function",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_table_backed_spatial_touches_function(void) {
    static const char *const projection_values[] = {
        "1",
        "1",
        "2",
        "0",
        "3",
        NULL,
        "4",
        NULL,
        "5",
        "1",
        "6",
        "0",
    };
    static const char *const update_values[] = {
        "1",
        "1",
        "2",
        "0",
        "3",
        NULL,
        "4",
        NULL,
        "5",
        "1",
        "6",
        "0",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "row") != 0) {
        return 1;
    }
    remove_related_files(path);
    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open row-backed database");
    failures += execute_ok(database, "CREATE DATABASE spatial_touches", NULL);
    failures += execute_ok(database, "USE spatial_touches", NULL);
    failures += execute_ok(
        database,
        "CREATE TABLE spatial_values("
        "id INT PRIMARY KEY, left_g GEOMETRY, right_g GEOMETRY, touches_txt VARCHAR(8))",
        NULL
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO spatial_values VALUES "
        "(1, ST_GeomFromText('LINESTRING(0 0,2 2)'), Point(0,0), NULL), "
        "(2, ST_GeomFromText('LINESTRING(0 0,2 2)'), Point(1,1), NULL), "
        "(3, Point(1,1), Point(1,1), NULL), "
        "(4, ST_GeomFromText('GEOMETRYCOLLECTION EMPTY'), Point(1,1), NULL), "
        "(5, ST_GeomFromText('GEOMETRYCOLLECTION(POINT(0 0),POINT(9 9))'), "
        "ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))'), NULL), "
        "(6, ST_GeomFromText('GEOMETRYCOLLECTION(POINT(0 0),POINT(1 1))'), "
        "ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))'), NULL)",
        (struct expected_dml_result){
            .affected_rows = spatial_touches_row_count,
            .warning_count = 0U,
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, ST_Touches(left_g, right_g) FROM spatial_values ORDER BY id",
            .column_count = 2U,
            .values = projection_values,
            .row_count = spatial_touches_row_count,
            .context = "row-backed spatial touches projection",
        }
    );
    failures += expect_dml_ok(
        database,
        "UPDATE spatial_values SET touches_txt = ST_Touches(left_g, right_g)",
        (struct expected_dml_result){.affected_rows = 4, .warning_count = 0U}
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, touches_txt FROM spatial_values ORDER BY id",
            .column_count = 2U,
            .values = update_values,
            .row_count = spatial_touches_row_count,
            .context = "row-backed spatial touches update",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_spatial_touches_diagnostics(void) {
    mylite_db *database = NULL;
    int failures = mylite_test_expect_int(
        mylite_open_memory(&database),
        MYLITE_OK,
        "open diagnostics database"
    );

    failures += execute_error(
        database,
        "SELECT ST_Touches()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_parameter_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'st_touches'",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_Touches(X'010203', Point(1,1))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_gis_data,
            .sqlstate = "22023",
            .message_part = "Invalid GIS data provided to function st_touches",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_Touches(ST_PointFromGeoHash('mh2n0p0581',4326), Point(1,1))",
        (struct expected_sql_error){
            .code = mysql_error_gis_different_srids,
            .sqlstate = "HY000",
            .message_part =
                "Binary geometry function st_touches given two geometries of different srids: "
                "4326 and 0, which should have been identical.",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_Touches("
        "ST_PointFromGeoHash('mh2n0p0581',4326), ST_PointFromGeoHash('mh2n0p0581',4326))",
        (struct expected_sql_error){
            .code = mysql_error_not_implemented_for_geographic_srs,
            .sqlstate = "22S00",
            .message_part =
                "st_touches has not been implemented for geographic spatial reference systems.",
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

    if (failures == 0) {
        failures += mylite_test_expect_int64(
            mylite_result_affected_rows(result),
            expected.affected_rows,
            sql
        );
        failures += mylite_test_expect_size(
            mylite_result_warning_count(result),
            expected.warning_count,
            sql
        );
    }
    mylite_result_free(result);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, expected.sql, &result);

    if (failures == 0) {
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
