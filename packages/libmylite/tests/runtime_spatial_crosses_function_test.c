#include <mylite/mylite.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    spatial_crosses_scalar_case_count = 25,
    spatial_crosses_row_count = 8,
    spatial_crosses_update_affected_row_count = 5,
    mysql_error_native_function_parameter_count = 1582,
    mysql_error_gis_different_srids = 3033,
    mysql_error_invalid_gis_data = 3037,
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

static int test_scalar_spatial_crosses_function(void);
static int test_table_backed_spatial_crosses_function(void);
static int test_spatial_crosses_diagnostics(void);
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

int main(void) {
    int failures = 0;

    failures += test_scalar_spatial_crosses_function();
    failures += test_table_backed_spatial_crosses_function();
    failures += test_spatial_crosses_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_scalar_spatial_crosses_function(void) {
    static const char *const scalar_values[] = {
        "null_left",
        NULL,
        "empty_empty",
        NULL,
        "empty_point",
        NULL,
        "point_point_same",
        NULL,
        "point_line_mid",
        "0",
        "point_line_endpoint",
        "0",
        "point_polygon_inside",
        "0",
        "point_polygon_boundary",
        "0",
        "multipoint_line_partial",
        "1",
        "multipoint_line_all",
        "0",
        "multipoint_polygon_partial",
        "1",
        "line_point_mid",
        NULL,
        "line_line_cross_mid",
        "1",
        "line_line_endpoint",
        "0",
        "line_t_endpoint_to_mid",
        "0",
        "line_line_overlap",
        "0",
        "line_line_contains",
        "0",
        "line_line_same",
        "0",
        "line_polygon_cross",
        "1",
        "line_polygon_inside",
        "0",
        "line_polygon_boundary",
        "0",
        "polygon_line_cross",
        NULL,
        "polygon_polygon_overlap",
        NULL,
        "collection_line_cross",
        "1",
        "collection_line_overlap",
        "0",
    };
    mylite_db *database = NULL;
    int failures =
        expect_int(mylite_open(":memory:", &database), MYLITE_OK, "open scalar database");

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 'null_left', ST_Crosses(NULL, Point(1,1)) UNION ALL "
                   "SELECT 'empty_empty', "
                   "ST_Crosses(ST_GeomFromText('GEOMETRYCOLLECTION EMPTY'), "
                   "ST_GeomFromText('GEOMETRYCOLLECTION EMPTY')) UNION ALL "
                   "SELECT 'empty_point', "
                   "ST_Crosses(ST_GeomFromText('GEOMETRYCOLLECTION EMPTY'), "
                   "Point(1,1)) UNION ALL "
                   "SELECT 'point_point_same', ST_Crosses(Point(1,1), Point(1,1)) "
                   "UNION ALL SELECT 'point_line_mid', "
                   "ST_Crosses(Point(1,1), ST_GeomFromText('LINESTRING(0 0,2 2)')) "
                   "UNION ALL SELECT 'point_line_endpoint', "
                   "ST_Crosses(Point(0,0), ST_GeomFromText('LINESTRING(0 0,2 2)')) "
                   "UNION ALL SELECT 'point_polygon_inside', "
                   "ST_Crosses(Point(1,1), "
                   "ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))')) UNION ALL "
                   "SELECT 'point_polygon_boundary', "
                   "ST_Crosses(Point(0,0), "
                   "ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))')) UNION ALL "
                   "SELECT 'multipoint_line_partial', "
                   "ST_Crosses(ST_GeomFromText('MULTIPOINT((1 1),(5 5))'), "
                   "ST_GeomFromText('LINESTRING(0 0,2 2)')) UNION ALL "
                   "SELECT 'multipoint_line_all', "
                   "ST_Crosses(ST_GeomFromText('MULTIPOINT((1 1),(2 2))'), "
                   "ST_GeomFromText('LINESTRING(0 0,3 3)')) UNION ALL "
                   "SELECT 'multipoint_polygon_partial', "
                   "ST_Crosses(ST_GeomFromText('MULTIPOINT((1 1),(5 5))'), "
                   "ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))')) UNION ALL "
                   "SELECT 'line_point_mid', "
                   "ST_Crosses(ST_GeomFromText('LINESTRING(0 0,2 2)'), Point(1,1)) "
                   "UNION ALL SELECT 'line_line_cross_mid', "
                   "ST_Crosses(ST_GeomFromText('LINESTRING(0 0,2 2)'), "
                   "ST_GeomFromText('LINESTRING(0 2,2 0)')) UNION ALL "
                   "SELECT 'line_line_endpoint', "
                   "ST_Crosses(ST_GeomFromText('LINESTRING(0 0,2 2)'), "
                   "ST_GeomFromText('LINESTRING(2 2,4 4)')) UNION ALL "
                   "SELECT 'line_t_endpoint_to_mid', "
                   "ST_Crosses(ST_GeomFromText('LINESTRING(0 0,2 0)'), "
                   "ST_GeomFromText('LINESTRING(1 0,1 1)')) UNION ALL "
                   "SELECT 'line_line_overlap', "
                   "ST_Crosses(ST_GeomFromText('LINESTRING(0 0,4 4)'), "
                   "ST_GeomFromText('LINESTRING(2 2,6 6)')) UNION ALL "
                   "SELECT 'line_line_contains', "
                   "ST_Crosses(ST_GeomFromText('LINESTRING(0 0,4 4)'), "
                   "ST_GeomFromText('LINESTRING(1 1,3 3)')) UNION ALL "
                   "SELECT 'line_line_same', "
                   "ST_Crosses(ST_GeomFromText('LINESTRING(0 0,4 4)'), "
                   "ST_GeomFromText('LINESTRING(4 4,0 0)')) UNION ALL "
                   "SELECT 'line_polygon_cross', "
                   "ST_Crosses(ST_GeomFromText('LINESTRING(-1 2,5 2)'), "
                   "ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))')) UNION ALL "
                   "SELECT 'line_polygon_inside', "
                   "ST_Crosses(ST_GeomFromText('LINESTRING(1 1,3 3)'), "
                   "ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))')) UNION ALL "
                   "SELECT 'line_polygon_boundary', "
                   "ST_Crosses(ST_GeomFromText('LINESTRING(0 0,4 0)'), "
                   "ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))')) UNION ALL "
                   "SELECT 'polygon_line_cross', "
                   "ST_Crosses(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))'), "
                   "ST_GeomFromText('LINESTRING(-1 2,5 2)')) UNION ALL "
                   "SELECT 'polygon_polygon_overlap', "
                   "ST_Crosses(ST_GeomFromText('POLYGON((0 0,3 0,3 3,0 3,0 0))'), "
                   "ST_GeomFromText('POLYGON((2 2,5 2,5 5,2 5,2 2))')) UNION ALL "
                   "SELECT 'collection_line_cross', "
                   "ST_Crosses(ST_GeomFromText('GEOMETRYCOLLECTION("
                   "LINESTRING(0 0,2 2),POINT(9 9))'), "
                   "ST_GeomFromText('LINESTRING(0 2,2 0)')) UNION ALL "
                   "SELECT 'collection_line_overlap', "
                   "ST_Crosses(ST_GeomFromText('GEOMETRYCOLLECTION("
                   "LINESTRING(0 0,4 4),POINT(9 9))'), "
                   "ST_GeomFromText('LINESTRING(2 2,6 6)'))",
            .column_count = 2U,
            .values = scalar_values,
            .row_count = spatial_crosses_scalar_case_count,
            .context = "scalar spatial crosses function",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_table_backed_spatial_crosses_function(void) {
    static const char *const projection_values[] = {
        "1",
        "1",
        "2",
        "1",
        "3",
        "1",
        "4",
        NULL,
        "5",
        NULL,
        "6",
        "0",
        "7",
        "0",
        "8",
        NULL,
    };
    static const char *const update_values[] = {
        "1",
        "1",
        "2",
        "1",
        "3",
        "1",
        "4",
        NULL,
        "5",
        NULL,
        "6",
        "0",
        "7",
        "0",
        "8",
        NULL,
    };
    mylite_db *database = NULL;
    int failures =
        expect_int(mylite_open(":memory:", &database), MYLITE_OK, "open row-backed database");

    failures += execute_ok(database, "CREATE DATABASE spatial_crosses", NULL);
    failures += execute_ok(database, "USE spatial_crosses", NULL);
    failures += execute_ok(
        database,
        "CREATE TABLE spatial_values("
        "id INT PRIMARY KEY, left_g GEOMETRY, right_g GEOMETRY, crosses_txt VARCHAR(8))",
        NULL
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO spatial_values VALUES "
        "(1, ST_GeomFromText('MULTIPOINT((1 1),(5 5))'), "
        "ST_GeomFromText('LINESTRING(0 0,2 2)'), NULL), "
        "(2, ST_GeomFromText('LINESTRING(0 0,2 2)'), "
        "ST_GeomFromText('LINESTRING(0 2,2 0)'), NULL), "
        "(3, ST_GeomFromText('LINESTRING(-1 2,5 2)'), "
        "ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))'), NULL), "
        "(4, ST_GeomFromText('LINESTRING(0 0,2 2)'), Point(1,1), NULL), "
        "(5, ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))'), "
        "ST_GeomFromText('LINESTRING(-1 2,5 2)'), NULL), "
        "(6, ST_GeomFromText('LINESTRING(1 1,3 3)'), "
        "ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))'), NULL), "
        "(7, ST_GeomFromText('LINESTRING(0 0,4 4)'), "
        "ST_GeomFromText('LINESTRING(2 2,6 6)'), NULL), "
        "(8, NULL, Point(1,1), NULL)",
        (struct expected_dml_result){
            .affected_rows = spatial_crosses_row_count,
            .warning_count = 0U,
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, ST_Crosses(left_g, right_g) FROM spatial_values ORDER BY id",
            .column_count = 2U,
            .values = projection_values,
            .row_count = spatial_crosses_row_count,
            .context = "row-backed spatial crosses projection",
        }
    );
    failures += expect_dml_ok(
        database,
        "UPDATE spatial_values SET crosses_txt = ST_Crosses(left_g, right_g)",
        (struct expected_dml_result){
            .affected_rows = spatial_crosses_update_affected_row_count,
            .warning_count = 0U,
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, crosses_txt FROM spatial_values ORDER BY id",
            .column_count = 2U,
            .values = update_values,
            .row_count = spatial_crosses_row_count,
            .context = "row-backed spatial crosses update",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_spatial_crosses_diagnostics(void) {
    mylite_db *database = NULL;
    int failures =
        expect_int(mylite_open(":memory:", &database), MYLITE_OK, "open diagnostics database");

    failures += execute_error(
        database,
        "SELECT ST_Crosses()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_parameter_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'st_crosses'",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_Crosses(X'010203', Point(1,1))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_gis_data,
            .sqlstate = "22023",
            .message_part = "Invalid GIS data provided to function st_crosses",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_Crosses(ST_PointFromGeoHash('mh2n0p0581',4326), Point(1,1))",
        (struct expected_sql_error){
            .code = mysql_error_gis_different_srids,
            .sqlstate = "HY000",
            .message_part =
                "Binary geometry function st_crosses given two geometries of different srids: "
                "4326 and 0, which should have been identical.",
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
