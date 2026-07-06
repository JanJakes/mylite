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
    spatial_contains_basic_case_count = 9,
    spatial_contains_line_polygon_case_count = 10,
    spatial_contains_collection_case_count = 6,
    spatial_contains_row_count = 5,
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

static int test_scalar_spatial_contains_within_functions(void);
static int test_table_backed_spatial_contains_within_functions(void);
static int test_spatial_contains_within_diagnostics(void);
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

    failures += test_scalar_spatial_contains_within_functions();
    failures += test_table_backed_spatial_contains_within_functions();
    failures += test_spatial_contains_within_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_scalar_spatial_contains_within_functions(void) {
    static const char *const basic_values[] = {
        "null_left",
        NULL,
        NULL,
        "empty_left",
        NULL,
        NULL,
        "point_same",
        "1",
        "1",
        "point_far",
        "0",
        "0",
        "polygon_point_inside",
        "1",
        "1",
        "polygon_point_boundary",
        "0",
        "0",
        "polygon_point_outside",
        "0",
        "0",
        "line_point_mid",
        "1",
        "1",
        "line_point_endpoint",
        "0",
        "0",
    };
    static const char *const line_polygon_values[] = {
        "line_line_sub",
        "1",
        "1",
        "line_line_same",
        "1",
        "1",
        "polygon_line_inside",
        "1",
        "1",
        "polygon_line_boundary",
        "0",
        "0",
        "polygon_line_cross",
        "0",
        "0",
        "polygon_line_cross_hole_off_midpoint",
        "0",
        "0",
        "polygon_line_boundary_to_inside",
        "1",
        "1",
        "polygon_polygon_contains",
        "1",
        "1",
        "polygon_same",
        "1",
        "1",
        "polygon_inside_shares_boundary",
        "1",
        "1",
    };
    static const char *const collection_values[] = {
        "polygon_hole_point",
        "0",
        "0",
        "polygon_hole_boundary_point",
        "0",
        "0",
        "multipoint_contains_point",
        "1",
        "1",
        "multiline_contains_point",
        "1",
        "1",
        "multipolygon_contains_point",
        "1",
        "1",
        "collection_contains_point",
        "1",
        "1",
    };
    mylite_db *database = NULL;
    int failures = expect_int(mylite_open_memory(&database), MYLITE_OK, "open scalar database");

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 'null_left', ST_Contains(NULL, Point(1,1)), "
                   "ST_Within(NULL, Point(1,1)) UNION ALL "
                   "SELECT 'empty_left', "
                   "ST_Contains(ST_GeomFromText('GEOMETRYCOLLECTION EMPTY'), Point(1,1)), "
                   "ST_Within(ST_GeomFromText('GEOMETRYCOLLECTION EMPTY'), Point(1,1)) "
                   "UNION ALL "
                   "SELECT 'point_same', ST_Contains(Point(1,1), Point(1,1)), "
                   "ST_Within(Point(1,1), Point(1,1)) UNION ALL "
                   "SELECT 'point_far', ST_Contains(Point(1,1), Point(2,2)), "
                   "ST_Within(Point(2,2), Point(1,1)) UNION ALL "
                   "SELECT 'polygon_point_inside', "
                   "ST_Contains(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))'), "
                   "Point(1,1)), ST_Within(Point(1,1), "
                   "ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))')) UNION ALL "
                   "SELECT 'polygon_point_boundary', "
                   "ST_Contains(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))'), "
                   "Point(0,0)), ST_Within(Point(0,0), "
                   "ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))')) UNION ALL "
                   "SELECT 'polygon_point_outside', "
                   "ST_Contains(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))'), "
                   "Point(5,5)), ST_Within(Point(5,5), "
                   "ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))')) UNION ALL "
                   "SELECT 'line_point_mid', "
                   "ST_Contains(ST_GeomFromText('LINESTRING(0 0,2 2)'), Point(1,1)), "
                   "ST_Within(Point(1,1), ST_GeomFromText('LINESTRING(0 0,2 2)')) "
                   "UNION ALL "
                   "SELECT 'line_point_endpoint', "
                   "ST_Contains(ST_GeomFromText('LINESTRING(0 0,2 2)'), Point(0,0)), "
                   "ST_Within(Point(0,0), ST_GeomFromText('LINESTRING(0 0,2 2)'))",
            .column_count = 3U,
            .values = basic_values,
            .row_count = spatial_contains_basic_case_count,
            .context = "scalar spatial contains/within basic functions",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 'line_line_sub', "
                   "ST_Contains(ST_GeomFromText('LINESTRING(0 0,4 4)'), "
                   "ST_GeomFromText('LINESTRING(1 1,3 3)')), "
                   "ST_Within(ST_GeomFromText('LINESTRING(1 1,3 3)'), "
                   "ST_GeomFromText('LINESTRING(0 0,4 4)')) UNION ALL "
                   "SELECT 'line_line_same', "
                   "ST_Contains(ST_GeomFromText('LINESTRING(0 0,4 4)'), "
                   "ST_GeomFromText('LINESTRING(0 0,4 4)')), "
                   "ST_Within(ST_GeomFromText('LINESTRING(0 0,4 4)'), "
                   "ST_GeomFromText('LINESTRING(0 0,4 4)')) UNION ALL "
                   "SELECT 'polygon_line_inside', "
                   "ST_Contains(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))'), "
                   "ST_GeomFromText('LINESTRING(1 1,3 3)')), "
                   "ST_Within(ST_GeomFromText('LINESTRING(1 1,3 3)'), "
                   "ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))')) UNION ALL "
                   "SELECT 'polygon_line_boundary', "
                   "ST_Contains(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))'), "
                   "ST_GeomFromText('LINESTRING(0 0,4 0)')), "
                   "ST_Within(ST_GeomFromText('LINESTRING(0 0,4 0)'), "
                   "ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))')) UNION ALL "
                   "SELECT 'polygon_line_cross', "
                   "ST_Contains(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))'), "
                   "ST_GeomFromText('LINESTRING(1 1,5 5)')), "
                   "ST_Within(ST_GeomFromText('LINESTRING(1 1,5 5)'), "
                   "ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))')) UNION ALL "
                   "SELECT 'polygon_line_cross_hole_off_midpoint', "
                   "ST_Contains(ST_GeomFromText('POLYGON((0 0,6 0,6 6,0 6,0 0),"
                   "(1.5 1.5,2.5 1.5,2.5 2.5,1.5 2.5,1.5 1.5))'), "
                   "ST_GeomFromText('LINESTRING(1 1,5 5)')), "
                   "ST_Within(ST_GeomFromText('LINESTRING(1 1,5 5)'), "
                   "ST_GeomFromText('POLYGON((0 0,6 0,6 6,0 6,0 0),"
                   "(1.5 1.5,2.5 1.5,2.5 2.5,1.5 2.5,1.5 1.5))')) UNION ALL "
                   "SELECT 'polygon_line_boundary_to_inside', "
                   "ST_Contains(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))'), "
                   "ST_GeomFromText('LINESTRING(0 0,2 2)')), "
                   "ST_Within(ST_GeomFromText('LINESTRING(0 0,2 2)'), "
                   "ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))')) UNION ALL "
                   "SELECT 'polygon_polygon_contains', "
                   "ST_Contains(ST_GeomFromText('POLYGON((0 0,5 0,5 5,0 5,0 0))'), "
                   "ST_GeomFromText('POLYGON((1 1,2 1,2 2,1 2,1 1))')), "
                   "ST_Within(ST_GeomFromText('POLYGON((1 1,2 1,2 2,1 2,1 1))'), "
                   "ST_GeomFromText('POLYGON((0 0,5 0,5 5,0 5,0 0))')) UNION ALL "
                   "SELECT 'polygon_same', "
                   "ST_Contains(ST_GeomFromText('POLYGON((0 0,5 0,5 5,0 5,0 0))'), "
                   "ST_GeomFromText('POLYGON((0 0,5 0,5 5,0 5,0 0))')), "
                   "ST_Within(ST_GeomFromText('POLYGON((0 0,5 0,5 5,0 5,0 0))'), "
                   "ST_GeomFromText('POLYGON((0 0,5 0,5 5,0 5,0 0))')) UNION ALL "
                   "SELECT 'polygon_inside_shares_boundary', "
                   "ST_Contains(ST_GeomFromText('POLYGON((0 0,5 0,5 5,0 5,0 0))'), "
                   "ST_GeomFromText('POLYGON((0 0,3 0,3 3,0 3,0 0))')), "
                   "ST_Within(ST_GeomFromText('POLYGON((0 0,3 0,3 3,0 3,0 0))'), "
                   "ST_GeomFromText('POLYGON((0 0,5 0,5 5,0 5,0 0))'))",
            .column_count = 3U,
            .values = line_polygon_values,
            .row_count = spatial_contains_line_polygon_case_count,
            .context = "scalar spatial contains/within line and polygon functions",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 'polygon_hole_point', "
                   "ST_Contains(ST_GeomFromText('POLYGON((0 0,6 0,6 6,0 6,0 0),"
                   "(2 2,4 2,4 4,2 4,2 2))'), Point(3,3)), "
                   "ST_Within(Point(3,3), ST_GeomFromText('POLYGON((0 0,6 0,6 6,0 6,"
                   "0 0),(2 2,4 2,4 4,2 4,2 2))')) UNION ALL "
                   "SELECT 'polygon_hole_boundary_point', "
                   "ST_Contains(ST_GeomFromText('POLYGON((0 0,6 0,6 6,0 6,0 0),"
                   "(2 2,4 2,4 4,2 4,2 2))'), Point(2,3)), "
                   "ST_Within(Point(2,3), ST_GeomFromText('POLYGON((0 0,6 0,6 6,0 6,"
                   "0 0),(2 2,4 2,4 4,2 4,2 2))')) UNION ALL "
                   "SELECT 'multipoint_contains_point', "
                   "ST_Contains(ST_GeomFromText('MULTIPOINT((1 1),(2 2))'), Point(1,1)), "
                   "ST_Within(Point(1,1), ST_GeomFromText('MULTIPOINT((1 1),(2 2))')) "
                   "UNION ALL "
                   "SELECT 'multiline_contains_point', "
                   "ST_Contains(ST_GeomFromText('MULTILINESTRING((0 0,2 2),(5 5,6 6))'), "
                   "Point(1,1)), ST_Within(Point(1,1), "
                   "ST_GeomFromText('MULTILINESTRING((0 0,2 2),(5 5,6 6))')) UNION ALL "
                   "SELECT 'multipolygon_contains_point', "
                   "ST_Contains(ST_GeomFromText('MULTIPOLYGON(((0 0,4 0,4 4,0 4,0 0)))'), "
                   "Point(1,1)), ST_Within(Point(1,1), "
                   "ST_GeomFromText('MULTIPOLYGON(((0 0,4 0,4 4,0 4,0 0)))')) UNION ALL "
                   "SELECT 'collection_contains_point', "
                   "ST_Contains(ST_GeomFromText('GEOMETRYCOLLECTION(POINT(1 1),"
                   "LINESTRING(5 5,6 6))'), Point(1,1)), ST_Within(Point(1,1), "
                   "ST_GeomFromText('GEOMETRYCOLLECTION(POINT(1 1),LINESTRING(5 5,6 6))'))",
            .column_count = 3U,
            .values = collection_values,
            .row_count = spatial_contains_collection_case_count,
            .context = "scalar spatial contains/within collection functions",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_table_backed_spatial_contains_within_functions(void) {
    static const char *const projection_values[] = {
        "1",
        "1",
        "1",
        "2",
        "0",
        "0",
        "3",
        "1",
        "1",
        "4",
        NULL,
        NULL,
        "5",
        NULL,
        NULL,
    };
    static const char *const update_values[] = {
        "1",
        "1",
        "1",
        "2",
        "0",
        "0",
        "3",
        "1",
        "1",
        "4",
        NULL,
        NULL,
        "5",
        NULL,
        NULL,
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "row") != 0) {
        return 1;
    }
    remove_related_files(path);
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open row-backed database");
    failures += execute_ok(database, "CREATE DATABASE spatial_contains", NULL);
    failures += execute_ok(database, "USE spatial_contains", NULL);
    failures += execute_ok(
        database,
        "CREATE TABLE spatial_values("
        "id INT PRIMARY KEY, container_g GEOMETRY, content_g GEOMETRY, "
        "contains_txt VARCHAR(8), within_txt VARCHAR(8))",
        NULL
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO spatial_values VALUES "
        "(1, ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))'), Point(1,1), NULL, NULL), "
        "(2, ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))'), Point(0,0), NULL, NULL), "
        "(3, ST_GeomFromText('LINESTRING(0 0,2 2)'), Point(1,1), NULL, NULL), "
        "(4, ST_GeomFromText('GEOMETRYCOLLECTION EMPTY'), Point(1,1), NULL, NULL), "
        "(5, NULL, Point(1,1), NULL, NULL)",
        (struct expected_dml_result){
            .affected_rows = spatial_contains_row_count,
            .warning_count = 0U,
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, ST_Contains(container_g, content_g), "
                   "ST_Within(content_g, container_g) FROM spatial_values ORDER BY id",
            .column_count = 3U,
            .values = projection_values,
            .row_count = spatial_contains_row_count,
            .context = "row-backed spatial contains/within projection",
        }
    );
    failures += expect_dml_ok(
        database,
        "UPDATE spatial_values SET contains_txt = ST_Contains(container_g, content_g), "
        "within_txt = ST_Within(content_g, container_g)",
        (struct expected_dml_result){.affected_rows = 3, .warning_count = 0U}
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, contains_txt, within_txt FROM spatial_values ORDER BY id",
            .column_count = 3U,
            .values = update_values,
            .row_count = spatial_contains_row_count,
            .context = "row-backed spatial contains/within update",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_spatial_contains_within_diagnostics(void) {
    mylite_db *database = NULL;
    int failures =
        expect_int(mylite_open_memory(&database), MYLITE_OK, "open diagnostics database");

    failures += execute_error(
        database,
        "SELECT ST_Contains()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_parameter_count,
            .sqlstate = "42000",
            .message_part =
                "Incorrect parameter count in the call to native function 'st_contains'",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_Within()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_parameter_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'st_within'",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_Contains(X'010203', Point(1,1))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_gis_data,
            .sqlstate = "22023",
            .message_part = "Invalid GIS data provided to function st_contains",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_Within(X'010203', Point(1,1))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_gis_data,
            .sqlstate = "22023",
            .message_part = "Invalid GIS data provided to function st_within",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_Contains(ST_PointFromGeoHash('mh2n0p0581',4326), Point(1,1))",
        (struct expected_sql_error){
            .code = mysql_error_gis_different_srids,
            .sqlstate = "HY000",
            .message_part =
                "Binary geometry function st_contains given two geometries of different srids: "
                "4326 and 0, which should have been identical.",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_Within(ST_PointFromGeoHash('mh2n0p0581',4326), Point(1,1))",
        (struct expected_sql_error){
            .code = mysql_error_gis_different_srids,
            .sqlstate = "HY000",
            .message_part =
                "Binary geometry function st_within given two geometries of different srids: "
                "4326 and 0, which should have been identical.",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_Contains("
        "ST_PointFromGeoHash('mh2n0p0581',4326), ST_PointFromGeoHash('mh2n0p0581',4326))",
        (struct expected_sql_error){
            .code = mysql_error_not_implemented_for_geographic_srs,
            .sqlstate = "22S00",
            .message_part =
                "st_contains has not been implemented for geographic spatial reference systems.",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_Within("
        "ST_PointFromGeoHash('mh2n0p0581',4326), ST_PointFromGeoHash('mh2n0p0581',4326))",
        (struct expected_sql_error){
            .code = mysql_error_not_implemented_for_geographic_srs,
            .sqlstate = "22S00",
            .message_part =
                "st_within has not been implemented for geographic spatial reference systems.",
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
        "/tmp/mylite_spatial_contains_%s_%d.mylite",
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
