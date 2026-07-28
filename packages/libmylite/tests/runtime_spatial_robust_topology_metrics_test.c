#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include "runtime/mylite_spatial_robust.h"

#include <float.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

enum {
    polygon_query_column_count = 6,
    transformed_case_count = 11,
    near_collinear_first_exponent = 26,
    near_collinear_last_exponent = 52,
    near_collinear_exponent_step = 2,
};

static const double coordinate_two = 2.0;
static const double translated_origin = 1000.0;
static const double translated_left = 1001.0;
static const double translated_right = 1002.0;

struct expected_query {
    const char *sql;
    size_t column_count;
    const char *const *values;
    size_t row_count;
    const char *context;
};

static int test_orientation_signs(void);
static int expect_orientation_sign(
    struct mylite_spatial_robust_point origin,
    struct mylite_spatial_robust_point left,
    struct mylite_spatial_robust_point right,
    int expected,
    const char *context
);
static int test_scalar_topology_and_metrics(void);
static int test_transformed_topology_and_metrics(void);
static int test_table_backed_topology_and_metrics(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_orientation_signs();
    failures += test_scalar_topology_and_metrics();
    failures += test_transformed_topology_and_metrics();
    failures += test_table_backed_topology_and_metrics();
    return failures == 0 ? 0 : 1;
}

static int test_orientation_signs(void) {
    const double above_one = nextafter(1.0, coordinate_two);
    const double above_translated_two = nextafter(translated_right, INFINITY);
    int failures = 0;

    failures += expect_orientation_sign(
        (struct mylite_spatial_robust_point){0.0, 0.0},
        (struct mylite_spatial_robust_point){1.0, 0.0},
        (struct mylite_spatial_robust_point){0.0, 1.0},
        1,
        "ordinary counterclockwise orientation"
    );
    failures += expect_orientation_sign(
        (struct mylite_spatial_robust_point){0.0, 0.0},
        (struct mylite_spatial_robust_point){0.0, 1.0},
        (struct mylite_spatial_robust_point){1.0, 0.0},
        -1,
        "ordinary clockwise orientation"
    );
    failures += expect_orientation_sign(
        (struct mylite_spatial_robust_point){-0.0, 0.0},
        (struct mylite_spatial_robust_point){1.0, 1.0},
        (struct mylite_spatial_robust_point){coordinate_two, coordinate_two},
        0,
        "exact collinear orientation"
    );
    failures += expect_orientation_sign(
        (struct mylite_spatial_robust_point){0.0, 0.0},
        (struct mylite_spatial_robust_point){1.0, 1.0},
        (struct mylite_spatial_robust_point){above_one, 1.0},
        -1,
        "cancellation-heavy negative orientation"
    );
    failures += expect_orientation_sign(
        (struct mylite_spatial_robust_point){translated_origin, translated_origin},
        (struct mylite_spatial_robust_point){translated_left, translated_left},
        (struct mylite_spatial_robust_point){translated_right, above_translated_two},
        1,
        "translated cancellation-heavy orientation"
    );
    failures += expect_orientation_sign(
        (struct mylite_spatial_robust_point){0.0, 0.0},
        (struct mylite_spatial_robust_point){DBL_TRUE_MIN, 0.0},
        (struct mylite_spatial_robust_point){0.0, DBL_TRUE_MIN},
        1,
        "subnormal orientation"
    );
    failures += expect_orientation_sign(
        (struct mylite_spatial_robust_point){DBL_MAX, DBL_MAX},
        (struct mylite_spatial_robust_point){-DBL_MAX, DBL_MAX},
        (struct mylite_spatial_robust_point){DBL_MAX, -DBL_MAX},
        1,
        "maximum-exponent orientation"
    );
    failures += mylite_test_expect_int(
        mylite_spatial_orientation_sign(NULL, NULL, NULL),
        0,
        "invalid orientation input is neutral"
    );
    for (int exponent = near_collinear_first_exponent; exponent <= near_collinear_last_exponent;
         exponent += near_collinear_exponent_step) {
        double coordinate = ldexp(1.0, exponent);

        failures += expect_orientation_sign(
            (struct mylite_spatial_robust_point){0.0, 0.0},
            (struct mylite_spatial_robust_point){coordinate, coordinate - 1.0},
            (struct mylite_spatial_robust_point){coordinate + 1.0, coordinate},
            1,
            "exact near-collinear orientation"
        );
    }
    return failures;
}

static int expect_orientation_sign(
    struct mylite_spatial_robust_point origin,
    struct mylite_spatial_robust_point left,
    struct mylite_spatial_robust_point right,
    int expected,
    const char *context
) {
    int failures = mylite_test_expect_int(
        mylite_spatial_orientation_sign(&origin, &left, &right),
        expected,
        context
    );
    int reversed = mylite_spatial_orientation_sign(&origin, &right, &left);

    if (expected == 0) {
        failures += mylite_test_expect_int(reversed, 0, context);
    } else {
        failures += mylite_test_expect_int(reversed, -expected, context);
    }
    return failures;
}

static int test_scalar_topology_and_metrics(void) {
    static const char *const threshold_values[] = {
        "point_below",
        "5e-13",
        "0",
        "1",
        "point_at",
        "1e-12",
        "0",
        "1",
        "point_above",
        "2e-12",
        "0",
        "1",
        "parallel_lines",
        "5e-13",
        "0",
        "1",
    };
    static const char *const polygon_values[] = {
        "polygon_outside",
        "5e-13",
        "0",
        "1",
        "0",
        "0",
        "polygon_boundary",
        "0",
        "1",
        "0",
        "0",
        "1",
        "polygon_inside",
        "0",
        "1",
        "0",
        "1",
        "0",
    };
    static const char *const segment_values[] = {
        "near_endpoint",
        "5.000444502911705e-13",
        "0",
        "1",
        "near_cross",
        "0",
        "1",
        "1",
        "collinear_overlap",
        "0",
        "1",
        "1",
    };
    static const char *const exact_values[] = {
        "distinct_points",
        "5e-13",
        "0",
        "0",
        "tiny_line",
        "5e-13",
        "1",
        "1",
        "tiny_square",
        "1",
        "2",
        "2.5e-25",
        "extreme_relative_scale",
        "5e-19",
        "0",
        "1",
    };
    mylite_db *database = NULL;
    int failures =
        mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open scalar database");

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 'point_below', "
                   "ST_Distance(Point(0.5,0.0000000000005),"
                   "ST_GeomFromText('LINESTRING(0 0,1 0)')),"
                   "ST_Intersects(Point(0.5,0.0000000000005),"
                   "ST_GeomFromText('LINESTRING(0 0,1 0)')),"
                   "ST_Disjoint(Point(0.5,0.0000000000005),"
                   "ST_GeomFromText('LINESTRING(0 0,1 0)')) UNION ALL "
                   "SELECT 'point_at', "
                   "ST_Distance(Point(0.5,0.000000000001),"
                   "ST_GeomFromText('LINESTRING(0 0,1 0)')),"
                   "ST_Intersects(Point(0.5,0.000000000001),"
                   "ST_GeomFromText('LINESTRING(0 0,1 0)')),"
                   "ST_Disjoint(Point(0.5,0.000000000001),"
                   "ST_GeomFromText('LINESTRING(0 0,1 0)')) UNION ALL "
                   "SELECT 'point_above', "
                   "ST_Distance(Point(0.5,0.000000000002),"
                   "ST_GeomFromText('LINESTRING(0 0,1 0)')),"
                   "ST_Intersects(Point(0.5,0.000000000002),"
                   "ST_GeomFromText('LINESTRING(0 0,1 0)')),"
                   "ST_Disjoint(Point(0.5,0.000000000002),"
                   "ST_GeomFromText('LINESTRING(0 0,1 0)')) UNION ALL "
                   "SELECT 'parallel_lines', "
                   "ST_Distance(ST_GeomFromText('LINESTRING(0 0,1 0)'),"
                   "ST_GeomFromText('LINESTRING(0 0.0000000000005,"
                   "1 0.0000000000005)')),"
                   "ST_Intersects(ST_GeomFromText('LINESTRING(0 0,1 0)'),"
                   "ST_GeomFromText('LINESTRING(0 0.0000000000005,"
                   "1 0.0000000000005)')),"
                   "ST_Disjoint(ST_GeomFromText('LINESTRING(0 0,1 0)'),"
                   "ST_GeomFromText('LINESTRING(0 0.0000000000005,"
                   "1 0.0000000000005)'))",
            .column_count = 4U,
            .values = threshold_values,
            .row_count = 4U,
            .context = "threshold robust topology and metrics",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 'polygon_outside', "
                   "ST_Distance(ST_GeomFromText('POLYGON((0 0,1 0,1 1,0 1,0 0))'),"
                   "Point(0.5,-0.0000000000005)),"
                   "ST_Intersects(ST_GeomFromText('POLYGON((0 0,1 0,1 1,0 1,0 0))'),"
                   "Point(0.5,-0.0000000000005)),"
                   "ST_Disjoint(ST_GeomFromText('POLYGON((0 0,1 0,1 1,0 1,0 0))'),"
                   "Point(0.5,-0.0000000000005)),"
                   "ST_Contains(ST_GeomFromText('POLYGON((0 0,1 0,1 1,0 1,0 0))'),"
                   "Point(0.5,-0.0000000000005)),"
                   "ST_Touches(ST_GeomFromText('POLYGON((0 0,1 0,1 1,0 1,0 0))'),"
                   "Point(0.5,-0.0000000000005)) UNION ALL "
                   "SELECT 'polygon_boundary', "
                   "ST_Distance(ST_GeomFromText('POLYGON((0 0,1 0,1 1,0 1,0 0))'),"
                   "Point(0.5,0)),"
                   "ST_Intersects(ST_GeomFromText('POLYGON((0 0,1 0,1 1,0 1,0 0))'),"
                   "Point(0.5,0)),"
                   "ST_Disjoint(ST_GeomFromText('POLYGON((0 0,1 0,1 1,0 1,0 0))'),"
                   "Point(0.5,0)),"
                   "ST_Contains(ST_GeomFromText('POLYGON((0 0,1 0,1 1,0 1,0 0))'),"
                   "Point(0.5,0)),"
                   "ST_Touches(ST_GeomFromText('POLYGON((0 0,1 0,1 1,0 1,0 0))'),"
                   "Point(0.5,0)) UNION ALL "
                   "SELECT 'polygon_inside', "
                   "ST_Distance(ST_GeomFromText('POLYGON((0 0,1 0,1 1,0 1,0 0))'),"
                   "Point(0.5,0.0000000000005)),"
                   "ST_Intersects(ST_GeomFromText('POLYGON((0 0,1 0,1 1,0 1,0 0))'),"
                   "Point(0.5,0.0000000000005)),"
                   "ST_Disjoint(ST_GeomFromText('POLYGON((0 0,1 0,1 1,0 1,0 0))'),"
                   "Point(0.5,0.0000000000005)),"
                   "ST_Contains(ST_GeomFromText('POLYGON((0 0,1 0,1 1,0 1,0 0))'),"
                   "Point(0.5,0.0000000000005)),"
                   "ST_Touches(ST_GeomFromText('POLYGON((0 0,1 0,1 1,0 1,0 0))'),"
                   "Point(0.5,0.0000000000005))",
            .column_count = polygon_query_column_count,
            .values = polygon_values,
            .row_count = 3U,
            .context = "polygon boundary robustness",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 'near_endpoint', "
                   "ST_Distance(ST_GeomFromText('LINESTRING(0 0,1 0)'),"
                   "ST_GeomFromText('LINESTRING(1.0000000000005 -1,"
                   "1.0000000000005 1)')),"
                   "ST_Intersects(ST_GeomFromText('LINESTRING(0 0,1 0)'),"
                   "ST_GeomFromText('LINESTRING(1.0000000000005 -1,"
                   "1.0000000000005 1)')),"
                   "ST_Disjoint(ST_GeomFromText('LINESTRING(0 0,1 0)'),"
                   "ST_GeomFromText('LINESTRING(1.0000000000005 -1,"
                   "1.0000000000005 1)')) UNION ALL "
                   "SELECT 'near_cross', "
                   "ST_Distance(ST_GeomFromText('LINESTRING(0 0,1 0)'),"
                   "ST_GeomFromText('LINESTRING(0.5 -0.0000000000005,"
                   "0.5 0.0000000000005)')),"
                   "ST_Intersects(ST_GeomFromText('LINESTRING(0 0,1 0)'),"
                   "ST_GeomFromText('LINESTRING(0.5 -0.0000000000005,"
                   "0.5 0.0000000000005)')),"
                   "ST_Crosses(ST_GeomFromText('LINESTRING(0 0,1 0)'),"
                   "ST_GeomFromText('LINESTRING(0.5 -0.0000000000005,"
                   "0.5 0.0000000000005)')) UNION ALL "
                   "SELECT 'collinear_overlap',"
                   "ST_Distance(ST_GeomFromText('LINESTRING(0 0,1 0)'),"
                   "ST_GeomFromText('LINESTRING(0.9999999999995 0,2 0)')),"
                   "ST_Intersects(ST_GeomFromText('LINESTRING(0 0,1 0)'),"
                   "ST_GeomFromText('LINESTRING(0.9999999999995 0,2 0)')),"
                   "ST_Overlaps(ST_GeomFromText('LINESTRING(0 0,1 0)'),"
                   "ST_GeomFromText('LINESTRING(0.9999999999995 0,2 0)'))",
            .column_count = 4U,
            .values = segment_values,
            .row_count = 3U,
            .context = "segment intersection robustness",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 'distinct_points', "
                   "ST_Distance(Point(0,0),Point(0.0000000000005,0)),"
                   "ST_Equals(Point(0,0),Point(0.0000000000005,0)),"
                   "ST_Intersects(Point(0,0),Point(0.0000000000005,0)) UNION ALL "
                   "SELECT 'tiny_line', "
                   "ST_Length(ST_GeomFromText('LINESTRING(0 0,0.0000000000005 0)')),"
                   "ST_IsValid(ST_GeomFromText('LINESTRING(0 0,0.0000000000005 0)')),"
                   "ST_IsSimple(ST_GeomFromText('LINESTRING(0 0,0.0000000000005 0)')) "
                   "UNION ALL "
                   "SELECT 'tiny_square', "
                   "ST_IsValid(ST_GeomFromText('POLYGON((0 0,0.0000000000005 0,"
                   "0.0000000000005 0.0000000000005,0 0.0000000000005,0 0))')),"
                   "ST_Dimension(ST_ConvexHull(ST_GeomFromText('POLYGON((0 0,"
                   "0.0000000000005 0,0.0000000000005 0.0000000000005,"
                   "0 0.0000000000005,0 0))'))),"
                   "ST_Area(ST_GeomFromText('POLYGON((0 0,0.0000000000005 0,"
                   "0.0000000000005 0.0000000000005,0 0.0000000000005,0 0))')) "
                   "UNION ALL "
                   "SELECT 'extreme_relative_scale', "
                   "ST_Distance(Point(0.0000005,0.0000000000000000005),"
                   "ST_GeomFromText('LINESTRING(0 0,0.000001 0)')),"
                   "ST_Intersects(Point(0.0000005,0.0000000000000000005),"
                   "ST_GeomFromText('LINESTRING(0 0,0.000001 0)')),"
                   "ST_Disjoint(Point(0.0000005,0.0000000000000000005),"
                   "ST_GeomFromText('LINESTRING(0 0,0.000001 0)'))",
            .column_count = 4U,
            .values = exact_values,
            .row_count = 4U,
            .context = "exact represented-coordinate semantics",
        }
    );
    mylite_close(database);
    return failures;
}

static int test_transformed_topology_and_metrics(void) {
    static const char *const values[] = {
        "base",
        "5e-13",
        "0",
        "1",
        "half",
        "2.5e-13",
        "0",
        "1",
        "double",
        "1e-12",
        "0",
        "1",
        "translated",
        "4.547473508864641e-13",
        "0",
        "1",
        "half_polygon",
        "2.5e-13",
        "0",
        "1",
        "double_polygon",
        "1e-12",
        "0",
        "1",
        "translated_polygon",
        "4.547473508864641e-13",
        "0",
        "1",
        "half_near_endpoint",
        "2.5002222514558525e-13",
        "0",
        "1",
        "double_near_endpoint",
        "1.000088900582341e-12",
        "0",
        "1",
        "translated_near_endpoint",
        "4.547473508864641e-13",
        "0",
        "1",
        "collapsed",
        "0",
        "1",
        "0",
    };
    mylite_db *database = NULL;
    int failures = mylite_test_expect_int(
        mylite_open_memory(&database),
        MYLITE_OK,
        "open transformed database"
    );

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 'base',"
                   "ST_Distance(Point(0.5,0.0000000000005),"
                   "ST_GeomFromText('LINESTRING(0 0,1 0)')),"
                   "ST_Intersects(Point(0.5,0.0000000000005),"
                   "ST_GeomFromText('LINESTRING(0 0,1 0)')),"
                   "ST_Disjoint(Point(0.5,0.0000000000005),"
                   "ST_GeomFromText('LINESTRING(0 0,1 0)')) UNION ALL "
                   "SELECT 'half',"
                   "ST_Distance(Point(0.25,0.00000000000025),"
                   "ST_GeomFromText('LINESTRING(0 0,0.5 0)')),"
                   "ST_Intersects(Point(0.25,0.00000000000025),"
                   "ST_GeomFromText('LINESTRING(0 0,0.5 0)')),"
                   "ST_Disjoint(Point(0.25,0.00000000000025),"
                   "ST_GeomFromText('LINESTRING(0 0,0.5 0)')) UNION ALL "
                   "SELECT 'double',"
                   "ST_Distance(Point(1,0.000000000001),"
                   "ST_GeomFromText('LINESTRING(0 0,2 0)')),"
                   "ST_Intersects(Point(1,0.000000000001),"
                   "ST_GeomFromText('LINESTRING(0 0,2 0)')),"
                   "ST_Disjoint(Point(1,0.000000000001),"
                   "ST_GeomFromText('LINESTRING(0 0,2 0)')) UNION ALL "
                   "SELECT 'translated',"
                   "ST_Distance(Point(1000.5,1000.0000000000005),"
                   "ST_GeomFromText('LINESTRING(1000 1000,1001 1000)')),"
                   "ST_Intersects(Point(1000.5,1000.0000000000005),"
                   "ST_GeomFromText('LINESTRING(1000 1000,1001 1000)')),"
                   "ST_Disjoint(Point(1000.5,1000.0000000000005),"
                   "ST_GeomFromText('LINESTRING(1000 1000,1001 1000)')) UNION ALL "
                   "SELECT 'half_polygon',"
                   "ST_Distance(Point(0.25,-0.00000000000025),"
                   "ST_GeomFromText('POLYGON((0 0,0.5 0,0.5 0.5,0 0.5,0 0))')),"
                   "ST_Intersects(Point(0.25,-0.00000000000025),"
                   "ST_GeomFromText('POLYGON((0 0,0.5 0,0.5 0.5,0 0.5,0 0))')),"
                   "ST_Disjoint(Point(0.25,-0.00000000000025),"
                   "ST_GeomFromText('POLYGON((0 0,0.5 0,0.5 0.5,0 0.5,0 0))')) UNION ALL "
                   "SELECT 'double_polygon',"
                   "ST_Distance(Point(1,-0.000000000001),"
                   "ST_GeomFromText('POLYGON((0 0,2 0,2 2,0 2,0 0))')),"
                   "ST_Intersects(Point(1,-0.000000000001),"
                   "ST_GeomFromText('POLYGON((0 0,2 0,2 2,0 2,0 0))')),"
                   "ST_Disjoint(Point(1,-0.000000000001),"
                   "ST_GeomFromText('POLYGON((0 0,2 0,2 2,0 2,0 0))')) UNION ALL "
                   "SELECT 'translated_polygon',"
                   "ST_Distance(Point(1000.5,999.9999999999995),"
                   "ST_GeomFromText('POLYGON((1000 1000,1001 1000,1001 1001,"
                   "1000 1001,1000 1000))')),"
                   "ST_Intersects(Point(1000.5,999.9999999999995),"
                   "ST_GeomFromText('POLYGON((1000 1000,1001 1000,1001 1001,"
                   "1000 1001,1000 1000))')),"
                   "ST_Disjoint(Point(1000.5,999.9999999999995),"
                   "ST_GeomFromText('POLYGON((1000 1000,1001 1000,1001 1001,"
                   "1000 1001,1000 1000))')) UNION ALL "
                   "SELECT 'half_near_endpoint',"
                   "ST_Distance(ST_GeomFromText('LINESTRING(0 0,0.5 0)'),"
                   "ST_GeomFromText('LINESTRING(0.50000000000025 -0.5,"
                   "0.50000000000025 0.5)')),"
                   "ST_Intersects(ST_GeomFromText('LINESTRING(0 0,0.5 0)'),"
                   "ST_GeomFromText('LINESTRING(0.50000000000025 -0.5,"
                   "0.50000000000025 0.5)')),"
                   "ST_Disjoint(ST_GeomFromText('LINESTRING(0 0,0.5 0)'),"
                   "ST_GeomFromText('LINESTRING(0.50000000000025 -0.5,"
                   "0.50000000000025 0.5)')) UNION ALL "
                   "SELECT 'double_near_endpoint',"
                   "ST_Distance(ST_GeomFromText('LINESTRING(0 0,2 0)'),"
                   "ST_GeomFromText('LINESTRING(2.000000000001 -2,2.000000000001 2)')),"
                   "ST_Intersects(ST_GeomFromText('LINESTRING(0 0,2 0)'),"
                   "ST_GeomFromText('LINESTRING(2.000000000001 -2,2.000000000001 2)')),"
                   "ST_Disjoint(ST_GeomFromText('LINESTRING(0 0,2 0)'),"
                   "ST_GeomFromText('LINESTRING(2.000000000001 -2,2.000000000001 2)')) "
                   "UNION ALL "
                   "SELECT 'translated_near_endpoint',"
                   "ST_Distance(ST_GeomFromText('LINESTRING(1000 1000,1001 1000)'),"
                   "ST_GeomFromText('LINESTRING(1001.0000000000005 999,"
                   "1001.0000000000005 1001)')),"
                   "ST_Intersects(ST_GeomFromText('LINESTRING(1000 1000,1001 1000)'),"
                   "ST_GeomFromText('LINESTRING(1001.0000000000005 999,"
                   "1001.0000000000005 1001)')),"
                   "ST_Disjoint(ST_GeomFromText('LINESTRING(1000 1000,1001 1000)'),"
                   "ST_GeomFromText('LINESTRING(1001.0000000000005 999,"
                   "1001.0000000000005 1001)')) UNION ALL "
                   "SELECT 'collapsed',"
                   "ST_Distance(Point(1000000000.5,1000000000.0000000000005),"
                   "ST_GeomFromText('LINESTRING(1000000000 1000000000,"
                   "1000000001 1000000000)')),"
                   "ST_Intersects(Point(1000000000.5,1000000000.0000000000005),"
                   "ST_GeomFromText('LINESTRING(1000000000 1000000000,"
                   "1000000001 1000000000)')),"
                   "ST_Disjoint(Point(1000000000.5,1000000000.0000000000005),"
                   "ST_GeomFromText('LINESTRING(1000000000 1000000000,"
                   "1000000001 1000000000)'))",
            .column_count = 4U,
            .values = values,
            .row_count = transformed_case_count,
            .context = "translated and scaled robust topology",
        }
    );
    mylite_close(database);
    return failures;
}

static int test_table_backed_topology_and_metrics(void) {
    static const char *const values[] = {
        "1",
        "5e-13",
        "0",
        "1",
        "2",
        "5e-13",
        "0",
        "1",
        "3",
        "5e-13",
        "0",
        "1",
        "4",
        "0",
        "1",
        "0",
    };
    mylite_db *database = NULL;
    int failures = mylite_test_expect_int(
        mylite_test_open_temporary(&database),
        MYLITE_OK,
        "open table-backed database"
    );

    failures += execute_ok(database, "CREATE DATABASE spatial_robust", NULL);
    failures += execute_ok(database, "USE spatial_robust", NULL);
    failures += execute_ok(
        database,
        "CREATE TABLE robust_cases ("
        "id INT PRIMARY KEY, left_geometry GEOMETRY, right_geometry GEOMETRY)",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO robust_cases VALUES "
        "(1,Point(0.5,0.0000000000005),"
        "ST_GeomFromText('LINESTRING(0 0,1 0)')),"
        "(2,ST_GeomFromText('LINESTRING(0 0,1 0)'),"
        "ST_GeomFromText('LINESTRING(0 0.0000000000005,1 0.0000000000005)')),"
        "(3,Point(0.5,-0.0000000000005),"
        "ST_GeomFromText('POLYGON((0 0,1 0,1 1,0 1,0 0))')),"
        "(4,ST_GeomFromText('LINESTRING(0 0,1 0)'),"
        "ST_GeomFromText('LINESTRING(0.5 -0.0000000000005,0.5 0.0000000000005)'))",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, ST_Distance(left_geometry,right_geometry),"
                   "ST_Intersects(left_geometry,right_geometry),"
                   "ST_Disjoint(left_geometry,right_geometry) "
                   "FROM robust_cases ORDER BY id",
            .column_count = 4U,
            .values = values,
            .row_count = 4U,
            .context = "table-backed robust topology and metrics",
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
