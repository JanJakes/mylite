#include <mylite/mylite.h>

#include <math.h>
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
    row_spatial_measure_column_count = 5,
    mysql_error_numeric_value_out_of_range = 1690,
    mysql_error_wrong_arguments = 1210,
    mysql_error_gis_different_srids = 3033,
    mysql_error_invalid_gis_data = 3037,
    mysql_error_geojson_longitude_out_of_range = 3616,
    mysql_error_geojson_latitude_out_of_range = 3617,
    mysql_error_not_implemented_for_geographic_srs = 3618,
    mysql_error_not_implemented_for_cartesian_srs = 3704,
    mysql_error_nonpositive_radius = 3706,
    mysql_error_unexpected_geometry_type = 3516,
    mysql_error_geometry_unknown_length_unit = 3882,
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
    double double_tolerance;
};

struct expected_dml_result {
    int64_t affected_rows;
    size_t warning_count;
};

static int test_scalar_spatial_measure_accessors(void);
static int test_table_backed_spatial_measure_accessors(void);
static int test_spatial_measure_accessor_diagnostics(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_dml_ok(mylite_db *database, const char *sql, struct expected_dml_result expected);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context,
    double double_tolerance
);
static int expect_result_double_value(
    const char *actual,
    const char *expected,
    const char *context,
    size_t row,
    size_t column,
    double tolerance
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

    failures += test_scalar_spatial_measure_accessors();
    failures += test_table_backed_spatial_measure_accessors();
    failures += test_spatial_measure_accessor_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_scalar_spatial_measure_accessors(void) {
    static const char *const dimension_values[] = {"0", "1", "2", NULL, "1", "0", "1"};
    static const char *const collection_values[] = {NULL, "0", "2", "LINESTRING(0 0,1 1)", NULL};
    static const char *const line_values[] = {
        "3",
        "POINT(3 4)",
        NULL,
        "POINT(0 0)",
        "POINT(6 0)",
        "0",
        "1",
        NULL,
        "10",
    };
    static const char *const polygon_values[] = {
        "1",
        "1",
        "LINESTRING(0 0,4 0,4 4,0 4,0 0)",
        "LINESTRING(1 1,2 1,2 2,1 1)",
        NULL,
        "6",
        "6.5",
    };
    static const char *const geometry_values[] = {
        "POINT(1 2)",
        "POLYGON((0 0,3 0,3 4,0 4,0 0))",
        "POLYGON((0 0,4 0,4 3,0 3,0 0))",
        "GEOMETRYCOLLECTION EMPTY",
        "POINT(2 1)",
        "LINESTRING(1 0,3 2)",
        "POLYGON((0 0,0 4,3 4,0 0))",
        "POINT(1 2)",
        "LINESTRING(1 2,3 2)",
        "POLYGON((1 2,3 2,3 4,1 4,1 2))",
    };
    static const char *const mbr_values[] = {
        "1",
        "1",
        "0",
        "1",
        "1",
        "1",
        "1",
        "1",
        "1",
        "0",
        "0",
        "1",
        "0",
        NULL,
    };
    static const char *const distance_values[] = {
        "5",
        NULL,
        NULL,
        "3",
        "0",
        "0",
        "1",
        "1",
        "1",
        "0",
        "1",
        "0",
        "3",
        "4",
        NULL,
    };
    static const char *const distance_sphere_values[] = {
        "20015042.813723423",
        "10007521.40686171",
        "3.141592653589793",
        "6.283185307179586",
        "0",
        NULL,
        NULL,
        "1111946.8229846344",
        "1111946.8229846344",
        "1111946.8229846344",
    };
    static const char *const discrete_distance_values[] = {
        "2.8284271247461903",
        "5",
        "1",
        "2.8284271247461903",
        "5",
        "5",
        "5",
        "4.242640687119285",
        "4.242640687119285",
        "6",
        NULL,
        NULL,
    };
    static const char *const centroid_values[] = {
        "POINT(2 4)",
        "POINT(2 2)",
        "POINT(1 3)",
        "POINT(1 1)",
        "POINT(5 5)",
        "POINT(6 1)",
        "POINT(3 3)",
        "POINT(1 1)",
        "POINT(4 4)",
        NULL,
        NULL,
        "POINT(1 1)",
    };
    static const char *const convex_hull_values[] = {
        "POINT(1 2)",
        "POINT(1 1)",
        "LINESTRING(0 0,2 2)",
        "POLYGON((5 0,25 0,15 25,5 0))",
        "POLYGON((0 0,2 0,2 2,0 2,0 0))",
        "POLYGON((0 0,2 0,3 1,1 1,0 0))",
        "POLYGON((0 0,4 0,4 4,0 4,0 0))",
        "POLYGON((0 0,7 0,6 3,2 2,0 0))",
        "POLYGON((0 0,3 1,2 4,1 5,0 4,0 0))",
        NULL,
        NULL,
    };
    static const char *const line_interpolation_values[] = {
        "POINT(0 5)",
        "POINT(2.5 5)",
        "POINT(5 5)",
        "POINT(0 0)",
        "MULTIPOINT((0 2.5),(0 5),(2.5 5),(5 5))",
        "MULTIPOINT((0 3),(1 5),(4 5))",
        "MULTIPOINT((0 0))",
        "MULTIPOINT((0 0))",
        "MULTIPOINT((5 5))",
        "POINT(0 0)",
        "POINT(0 5)",
        "POINT(2.5 5)",
        "POINT(5 5)",
        NULL,
        NULL,
        NULL,
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open scalar database");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ST_Dimension(Point(1,2)), "
                   "ST_Dimension(ST_GeomFromText('LINESTRING(0 0,3 4)')), "
                   "ST_Dimension(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))')), "
                   "ST_Dimension(ST_GeomFromText('GEOMETRYCOLLECTION EMPTY')), "
                   "ST_Dimension(ST_GeomFromText('GEOMETRYCOLLECTION(POINT(1 2),"
                   "LINESTRING(0 0,1 1))')), ST_IsEmpty(Point(1,2)), "
                   "ST_IsEmpty(ST_GeomFromText('GEOMETRYCOLLECTION EMPTY'))",
            .column_count = sizeof(dimension_values) / sizeof(dimension_values[0]),
            .values = dimension_values,
            .row_count = 1U,
            .context = "dimension and state functions",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ST_NumGeometries(Point(1,2)), "
                   "ST_NumGeometries(ST_GeomFromText('GEOMETRYCOLLECTION EMPTY')), "
                   "ST_NumGeometries(ST_GeomFromText('GEOMETRYCOLLECTION(POINT(1 2),"
                   "LINESTRING(0 0,1 1))')), ST_AsText(ST_GeometryN("
                   "ST_GeomFromText('GEOMETRYCOLLECTION(POINT(1 2),LINESTRING(0 0,1 1))'),2)), "
                   "ST_AsText(ST_GeometryN(Point(1,2),1))",
            .column_count = sizeof(collection_values) / sizeof(collection_values[0]),
            .values = collection_values,
            .row_count = 1U,
            .context = "collection accessors",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ST_NumPoints(ST_GeomFromText('LINESTRING(0 0,3 4,6 0)')), "
                   "ST_AsText(ST_PointN(ST_GeomFromText('LINESTRING(0 0,3 4,6 0)'),2)), "
                   "ST_AsText(ST_PointN(ST_GeomFromText('LINESTRING(0 0,3 4,6 0)'),0)), "
                   "ST_AsText(ST_StartPoint(ST_GeomFromText('LINESTRING(0 0,3 4,6 0)'))), "
                   "ST_AsText(ST_EndPoint(ST_GeomFromText('LINESTRING(0 0,3 4,6 0)'))), "
                   "ST_IsClosed(ST_GeomFromText('LINESTRING(0 0,1 1)')), "
                   "ST_IsClosed(ST_GeomFromText('LINESTRING(0 0,1 1,0 0)')), "
                   "ST_IsClosed(Point(1,2)), "
                   "ST_Length(ST_GeomFromText('LINESTRING(0 0,3 4,6 0)'))",
            .column_count = sizeof(line_values) / sizeof(line_values[0]),
            .values = line_values,
            .row_count = 1U,
            .context = "line accessors and length",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ST_NumInteriorRing(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0),"
                   "(1 1,2 1,2 2,1 1))')), "
                   "ST_NumInteriorRings(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0),"
                   "(1 1,2 1,2 2,1 1))')), "
                   "ST_AsText(ST_ExteriorRing(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0),"
                   "(1 1,2 1,2 2,1 1))'))), "
                   "ST_AsText(ST_InteriorRingN(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0),"
                   "(1 1,2 1,2 2,1 1))'),1)), "
                   "ST_AsText(ST_InteriorRingN(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0),"
                   "(1 1,2 1,2 2,1 1))'),2)), "
                   "ST_Area(ST_GeomFromText('POLYGON((0 0,4 0,4 3,0 0))')), "
                   "ST_Area(ST_GeomFromText('MULTIPOLYGON(((0 0,4 0,4 3,0 0)),"
                   "((0 0,1 0,1 1,0 0)))'))",
            .column_count = sizeof(polygon_values) / sizeof(polygon_values[0]),
            .values = polygon_values,
            .row_count = 1U,
            .context = "polygon accessors and area",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ST_AsText(ST_Envelope(Point(1,2))), "
                   "ST_AsText(ST_Envelope(ST_GeomFromText('LINESTRING(0 0,3 4)'))), "
                   "ST_AsText(ST_Envelope(ST_GeomFromText('POLYGON((0 0,4 0,4 3,0 0))'))), "
                   "ST_AsText(ST_Envelope(ST_GeomFromText('GEOMETRYCOLLECTION EMPTY'))), "
                   "ST_AsText(ST_SwapXY(Point(1,2))), "
                   "ST_AsText(ST_SwapXY(ST_GeomFromText('LINESTRING(0 1,2 3)'))), "
                   "ST_AsText(ST_SwapXY(ST_GeomFromText('POLYGON((0 0,4 0,4 3,0 0))'))), "
                   "ST_AsText(ST_MakeEnvelope(Point(1,2), Point(1,2))), "
                   "ST_AsText(ST_MakeEnvelope(Point(1,2), Point(3,2))), "
                   "ST_AsText(ST_MakeEnvelope(Point(1,2), Point(3,4)))",
            .column_count = sizeof(geometry_values) / sizeof(geometry_values[0]),
            .values = geometry_values,
            .row_count = 1U,
            .context = "envelope and swap geometry results",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT MBRContains(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))'), "
                   "Point(1,1)), MBRWithin(Point(1,1), ST_GeomFromText('POLYGON((0 0,4 0,"
                   "4 4,0 4,0 0))')), MBRIntersects(ST_GeomFromText('LINESTRING(0 0,1 1)'), "
                   "ST_GeomFromText('LINESTRING(2 2,3 3)')), MBREquals(Point(1,1), Point(1,1)), "
                   "MBRDisjoint(Point(1,1), Point(2,2)), MBRTouches(ST_GeomFromText("
                   "'LINESTRING(0 0,1 1)'), ST_GeomFromText('LINESTRING(1 1,2 2)')), "
                   "MBROverlaps(ST_GeomFromText('POLYGON((0 0,3 0,3 3,0 3,0 0))'), "
                   "ST_GeomFromText('POLYGON((2 2,5 2,5 5,2 5,2 2))')), MBRCovers("
                   "ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))'), Point(0,0)), "
                   "MBRCoveredBy(Point(0,0), ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))')), "
                   "MBRContains(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))'), Point(0,0)), "
                   "MBRWithin(Point(0,0), ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))')), "
                   "MBREquals(ST_GeomFromText('GEOMETRYCOLLECTION EMPTY'), "
                   "ST_GeomFromText('GEOMETRYCOLLECTION EMPTY')), MBREquals("
                   "ST_GeomFromText('GEOMETRYCOLLECTION EMPTY'), Point(1,1)), "
                   "MBRIntersects(ST_GeomFromText('GEOMETRYCOLLECTION EMPTY'), Point(1,1))",
            .column_count = sizeof(mbr_values) / sizeof(mbr_values[0]),
            .values = mbr_values,
            .row_count = 1U,
            .context = "MBR predicates",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ST_Distance(Point(0,0), Point(3,4)), "
                   "ST_Distance(NULL, Point(1,1)), "
                   "ST_Distance(ST_GeomFromText('GEOMETRYCOLLECTION EMPTY'), Point(1,1)), "
                   "ST_Distance(Point(0,0), ST_GeomFromText('LINESTRING(3 0,3 4)')), "
                   "ST_Distance(ST_GeomFromText('LINESTRING(0 0,4 4)'), "
                   "ST_GeomFromText('LINESTRING(0 4,4 0)')), "
                   "ST_Distance(Point(2,2), ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))')), "
                   "ST_Distance(Point(5,2), ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))')), "
                   "ST_Distance(Point(2,2), ST_GeomFromText('POLYGON((0 0,5 0,5 5,0 5,0 0),"
                   "(1 1,4 1,4 4,1 4,1 1))')), "
                   "ST_Distance(ST_GeomFromText('LINESTRING(5 0,5 4)'), "
                   "ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))')), "
                   "ST_Distance(ST_GeomFromText('LINESTRING(-1 2,1 2)'), "
                   "ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))')), "
                   "ST_Distance(ST_GeomFromText('POLYGON((0 0,2 0,2 2,0 2,0 0))'), "
                   "ST_GeomFromText('POLYGON((3 0,5 0,5 2,3 2,3 0))')), "
                   "ST_Distance(ST_GeomFromText('POLYGON((0 0,3 0,3 3,0 3,0 0))'), "
                   "ST_GeomFromText('POLYGON((2 2,5 2,5 5,2 5,2 2))')), "
                   "ST_Distance(ST_GeomFromText('MULTIPOINT(0 0,10 10)'), "
                   "ST_GeomFromText('LINESTRING(3 0,3 4)')), "
                   "ST_Distance(ST_GeomFromText('GEOMETRYCOLLECTION(POINT(10 10),"
                   "LINESTRING(0 5,5 5))'), Point(1,1)), "
                   "ST_Distance(Point(0,0), Point(1,1), NULL)",
            .column_count = sizeof(distance_values) / sizeof(distance_values[0]),
            .values = distance_values,
            .row_count = 1U,
            .context = "distance measurements",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ST_Distance_Sphere(Point(0,0), Point(180,0)), "
                   "ST_Distance_Sphere(Point(0,0), Point(0,90)), "
                   "ST_Distance_Sphere(Point(0,0), Point(180,0), 1), "
                   "ST_Distance_Sphere(Point(0,0), Point(180,0), '2'), "
                   "ST_Distance_Sphere(Point(0,0), Point(0,0)), "
                   "ST_Distance_Sphere(NULL, Point(1,1)), "
                   "ST_Distance_Sphere(Point(0,0), Point(1,1), NULL), "
                   "ST_Distance_Sphere(Point(0,0), ST_GeomFromText('MULTIPOINT(10 0,20 0)')), "
                   "ST_Distance_Sphere(ST_GeomFromText('MULTIPOINT(10 0,20 0)'), Point(0,0)), "
                   "ST_Distance_Sphere(ST_GeomFromText('MULTIPOINT(0 0,10 0)'), "
                   "ST_GeomFromText('MULTIPOINT(20 0,30 0)'))",
            .column_count = sizeof(distance_sphere_values) / sizeof(distance_sphere_values[0]),
            .values = distance_sphere_values,
            .row_count = 1U,
            .context = "distance sphere measurements",
            .double_tolerance = 1.0e-8,
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ST_FrechetDistance(ST_GeomFromText('LINESTRING(0 0,0 5,5 5)'), "
                   "ST_GeomFromText('LINESTRING(0 1,0 6,3 3,5 6)')), "
                   "ST_FrechetDistance(ST_GeomFromText('LINESTRING(1 1,1 1)'), "
                   "ST_GeomFromText('LINESTRING(4 5,4 5)')), "
                   "ST_HausdorffDistance(ST_GeomFromText('LINESTRING(0 0,0 5,5 5)'), "
                   "ST_GeomFromText('LINESTRING(0 1,0 6,3 3,5 6)')), "
                   "ST_HausdorffDistance(ST_GeomFromText('LINESTRING(0 1,0 6,3 3,5 6)'), "
                   "ST_GeomFromText('LINESTRING(0 0,0 5,5 5)')), "
                   "ST_HausdorffDistance(Point(0,0), ST_GeomFromText('MULTIPOINT(3 4,10 10)')), "
                   "ST_HausdorffDistance(ST_GeomFromText('MULTIPOINT(3 4,10 10)'), Point(0,0)), "
                   "ST_HausdorffDistance(ST_GeomFromText('MULTIPOINT(0 0,3 4)'), "
                   "ST_GeomFromText('MULTIPOINT(6 8,3 4)')), "
                   "ST_HausdorffDistance(ST_GeomFromText('LINESTRING(0 0,0 5)'), "
                   "ST_GeomFromText('MULTILINESTRING((0 1,0 6),(3 3,5 6))')), "
                   "ST_HausdorffDistance(ST_GeomFromText('MULTILINESTRING((0 1,0 6),(3 3,5 6))'), "
                   "ST_GeomFromText('LINESTRING(0 0,0 5)')), "
                   "ST_HausdorffDistance(ST_GeomFromText('MULTILINESTRING((0 0,0 5),(5 5,6 6))'), "
                   "ST_GeomFromText('MULTILINESTRING((0 1,0 6),(3 3,5 6))')), "
                   "ST_FrechetDistance(NULL, ST_GeomFromText('LINESTRING(0 0,1 1)')), "
                   "ST_HausdorffDistance(ST_GeomFromText('GEOMETRYCOLLECTION EMPTY'), Point(0,0))",
            .column_count = sizeof(discrete_distance_values) / sizeof(discrete_distance_values[0]),
            .values = discrete_distance_values,
            .row_count = 1U,
            .context = "discrete distance measurements",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql =
                "SELECT ST_AsText(ST_Centroid(Point(2,4))), "
                "ST_AsText(ST_Centroid(ST_GeomFromText('MULTIPOINT(0 0,2 2,4 4)'))), "
                "ST_AsText(ST_Centroid(ST_GeomFromText('LINESTRING(0 0,0 4,4 4)'))), "
                "ST_AsText(ST_Centroid(ST_GeomFromText('MULTILINESTRING((0 0,0 4),(0 0,4 0))'))), "
                "ST_AsText(ST_Centroid(ST_GeomFromText('POLYGON((0 0,10 0,10 10,0 10,0 0),"
                "(4 4,6 4,6 6,4 6,4 4))'))), "
                "ST_AsText(ST_Centroid(ST_GeomFromText('MULTIPOLYGON(((0 0,2 0,2 2,0 2,0 0)),"
                "((10 0,12 0,12 2,10 2,10 0)))'))), "
                "ST_AsText(ST_Centroid(ST_GeomFromText('GEOMETRYCOLLECTION(POINT(100 100),"
                "LINESTRING(0 0,0 4),POLYGON((0 0,6 0,6 6,0 6,0 0)))'))), "
                "ST_AsText(ST_Centroid(ST_GeomFromText('GEOMETRYCOLLECTION(POINT(100 100),"
                "LINESTRING(0 0,0 4),LINESTRING(0 0,4 0))'))), "
                "ST_AsText(ST_Centroid(ST_GeomFromText('GEOMETRYCOLLECTION(POINT(0 0),"
                "MULTIPOINT(4 4,8 8))'))), "
                "ST_AsText(ST_Centroid(ST_GeomFromText('GEOMETRYCOLLECTION EMPTY'))), "
                "ST_AsText(ST_Centroid(NULL)), "
                "ST_AsText(ST_Centroid(ST_GeomFromText('LINESTRING(1 1,1 1)')))",
            .column_count = sizeof(centroid_values) / sizeof(centroid_values[0]),
            .values = centroid_values,
            .row_count = 1U,
            .context = "centroid measurements",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ST_AsText(ST_ConvexHull(Point(1,2))), "
                   "ST_AsText(ST_ConvexHull(ST_GeomFromText('MULTIPOINT(1 1,1 1,1 1)'))), "
                   "ST_AsText(ST_ConvexHull(ST_GeomFromText('MULTIPOINT(0 0,1 1,2 2,1 1)'))), "
                   "ST_AsText(ST_ConvexHull(ST_GeomFromText('MULTIPOINT(5 0,25 0,15 10,15 25)'))), "
                   "ST_AsText(ST_ConvexHull(ST_GeomFromText('MULTIPOINT(0 0,0 2,2 0,2 2,1 1)'))), "
                   "ST_AsText(ST_ConvexHull(ST_GeomFromText('LINESTRING(0 0,1 1,2 0,3 1)'))), "
                   "ST_AsText(ST_ConvexHull(ST_GeomFromText('POLYGON((0 0,4 0,4 4,2 2,0 4,0 0),"
                   "(1 1,2 1,1 2,1 1))'))), "
                   "ST_AsText(ST_ConvexHull(ST_GeomFromText('MULTIPOLYGON(((0 0,2 0,2 2,0 0)),"
                   "((5 0,7 0,6 3,5 0)))'))), "
                   "ST_AsText(ST_ConvexHull(ST_GeomFromText('GEOMETRYCOLLECTION(POINT(0 0),"
                   "LINESTRING(1 2,3 1),POLYGON((0 4,2 4,1 5,0 4)))'))), "
                   "ST_AsText(ST_ConvexHull(ST_GeomFromText('GEOMETRYCOLLECTION EMPTY'))), "
                   "ST_AsText(ST_ConvexHull(NULL))",
            .column_count = sizeof(convex_hull_values) / sizeof(convex_hull_values[0]),
            .values = convex_hull_values,
            .row_count = 1U,
            .context = "convex hull measurements",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ST_AsText(ST_LineInterpolatePoint("
                   "ST_GeomFromText('LINESTRING(0 0,0 5,5 5)'), '0.5')), "
                   "ST_AsText(ST_LineInterpolatePoint("
                   "ST_GeomFromText('LINESTRING(0 0,0 5,5 5)'), '0.75')), "
                   "ST_AsText(ST_LineInterpolatePoint("
                   "ST_GeomFromText('LINESTRING(0 0,0 5,5 5)'), 1)), "
                   "ST_AsText(ST_LineInterpolatePoint("
                   "ST_GeomFromText('LINESTRING(0 0,0 5,5 5)'), 0)), "
                   "ST_AsText(ST_LineInterpolatePoints("
                   "ST_GeomFromText('LINESTRING(0 0,0 5,5 5)'), '0.25')), "
                   "ST_AsText(ST_LineInterpolatePoints("
                   "ST_GeomFromText('LINESTRING(0 0,0 5,5 5)'), '0.3')), "
                   "ST_AsText(ST_LineInterpolatePoints("
                   "ST_GeomFromText('LINESTRING(0 0,0 0,0 0)'), '0.5')), "
                   "ST_AsText(ST_LineInterpolatePoints("
                   "ST_GeomFromText('LINESTRING(0 0,0 5,5 5)'), 0)), "
                   "ST_AsText(ST_LineInterpolatePoints("
                   "ST_GeomFromText('LINESTRING(0 0,0 5,5 5)'), 1)), "
                   "ST_AsText(ST_PointAtDistance("
                   "ST_GeomFromText('LINESTRING(0 0,0 5,5 5)'), 0)), "
                   "ST_AsText(ST_PointAtDistance("
                   "ST_GeomFromText('LINESTRING(0 0,0 5,5 5)'), 5)), "
                   "ST_AsText(ST_PointAtDistance("
                   "ST_GeomFromText('LINESTRING(0 0,0 5,5 5)'), '7.5')), "
                   "ST_AsText(ST_PointAtDistance("
                   "ST_GeomFromText('LINESTRING(0 0,0 5,5 5)'), 10)), "
                   "ST_AsText(ST_LineInterpolatePoint(NULL, '0.5')), "
                   "ST_AsText(ST_LineInterpolatePoints("
                   "ST_GeomFromText('LINESTRING(0 0,0 5,5 5)'), NULL)), "
                   "ST_AsText(ST_PointAtDistance("
                   "ST_GeomFromText('LINESTRING(0 0,0 5,5 5)'), NULL))",
            .column_count =
                sizeof(line_interpolation_values) / sizeof(line_interpolation_values[0]),
            .values = line_interpolation_values,
            .row_count = 1U,
            .context = "linestring interpolation functions",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_table_backed_spatial_measure_accessors(void) {
    static const char *const projection_values[] = {
        "1",
        "0",
        "0",
        "POINT(1 2)",
        "0",
        "2",
        "1",
        "0",
        "POLYGON((0 0,3 0,3 4,0 4,0 0))",
        "0",
        "3",
        "2",
        "0",
        "POLYGON((0 0,4 0,4 3,0 3,0 0))",
        "0",
        "4",
        NULL,
        "1",
        "GEOMETRYCOLLECTION EMPTY",
        NULL,
    };
    static const char *const update_values[] = {
        "1",
        "POINT(1 2)",
        "2",
        "POLYGON((0 0,3 0,3 4,0 4,0 0))",
        "3",
        "POLYGON((0 0,4 0,4 3,0 3,0 0))",
        "4",
        "GEOMETRYCOLLECTION EMPTY",
    };
    static const char *const line_interpolation_values[] = {
        "POINT(0 5)",
        "MULTIPOINT((0 5),(5 5))",
        "POINT(2.5 5)",
    };
    static const char *const line_update_values[] = {"1", "POINT(2.5 5)"};
    static const char *const distance_sphere_values[] = {
        "1111946.8229846344",
        "2223893.645969269",
        "0",
        "1111946.8229846344",
    };
    static const char *const discrete_distance_values[] = {
        "2.8284271247461903",
        "1",
        "0",
        "0",
    };
    static const char *const centroid_values[] = {
        "POINT(2 4)",
        "POINT(1 3)",
        "POINT(5 5)",
    };
    static const char *const convex_hull_values[] = {
        "POINT(1 2)",
        "LINESTRING(0 0,3 4)",
        "POLYGON((0 0,4 0,4 3,0 0))",
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
    failures += execute_ok(database, "CREATE DATABASE spatial_measure", NULL);
    failures += execute_ok(database, "USE spatial_measure", NULL);
    failures += execute_ok(
        database,
        "CREATE TABLE spatial_values(id INT PRIMARY KEY, g GEOMETRY, txt VARCHAR(120))",
        NULL
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO spatial_values VALUES "
        "(1, Point(1,2), NULL), "
        "(2, ST_GeomFromText('LINESTRING(0 0,3 4)'), NULL), "
        "(3, ST_GeomFromText('POLYGON((0 0,4 0,4 3,0 0))'), NULL), "
        "(4, ST_GeomFromText('GEOMETRYCOLLECTION EMPTY'), NULL)",
        (struct expected_dml_result){.affected_rows = 4, .warning_count = 0U}
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, ST_Dimension(g), ST_IsEmpty(g), ST_AsText(ST_Envelope(g)) "
                   ", ST_Distance(g, g) "
                   "FROM spatial_values ORDER BY id",
            .column_count = row_spatial_measure_column_count,
            .values = projection_values,
            .row_count = 4U,
            .context = "row-backed spatial measure projection",
        }
    );
    failures += expect_dml_ok(
        database,
        "UPDATE spatial_values SET txt = ST_AsText(ST_Envelope(g))",
        (struct expected_dml_result){.affected_rows = 4, .warning_count = 0U}
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, txt FROM spatial_values ORDER BY id",
            .column_count = 2U,
            .values = update_values,
            .row_count = 4U,
            .context = "row-backed spatial measure update",
        }
    );
    failures += execute_ok(
        database,
        "CREATE TABLE line_values(id INT PRIMARY KEY, g GEOMETRY, txt VARCHAR(120))",
        NULL
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO line_values VALUES "
        "(1, ST_GeomFromText('LINESTRING(0 0,0 5,5 5)'), NULL)",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ST_AsText(ST_LineInterpolatePoint(g, '0.5')), "
                   "ST_AsText(ST_LineInterpolatePoints(g, '0.5')), "
                   "ST_AsText(ST_PointAtDistance(g, '7.5')) "
                   "FROM line_values",
            .column_count =
                sizeof(line_interpolation_values) / sizeof(line_interpolation_values[0]),
            .values = line_interpolation_values,
            .row_count = 1U,
            .context = "row-backed linestring interpolation projection",
        }
    );
    failures += expect_dml_ok(
        database,
        "UPDATE line_values SET txt = ST_AsText(ST_LineInterpolatePoint(g, '0.75'))",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, txt FROM line_values",
            .column_count = sizeof(line_update_values) / sizeof(line_update_values[0]),
            .values = line_update_values,
            .row_count = 1U,
            .context = "row-backed linestring interpolation update",
        }
    );
    failures += execute_ok(
        database,
        "CREATE TABLE sphere_values(id INT PRIMARY KEY, g GEOMETRY, txt VARCHAR(120))",
        NULL
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO sphere_values VALUES "
        "(1, Point(0,0), NULL), "
        "(2, ST_GeomFromText('MULTIPOINT(0 0,10 0)'), NULL)",
        (struct expected_dml_result){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ST_Distance_Sphere(g, Point(10,0)), "
                   "ST_Distance_Sphere(g, ST_GeomFromText('MULTIPOINT(20 0,30 0)')) "
                   "FROM sphere_values ORDER BY id",
            .column_count = 2U,
            .values = distance_sphere_values,
            .row_count = 2U,
            .context = "row-backed distance sphere projection",
            .double_tolerance = 1.0e-8,
        }
    );
    failures += execute_ok(
        database,
        "CREATE TABLE discrete_distance_values(id INT PRIMARY KEY, g GEOMETRY)",
        NULL
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO discrete_distance_values VALUES "
        "(1, ST_GeomFromText('LINESTRING(0 0,0 5,5 5)')), "
        "(2, ST_GeomFromText('LINESTRING(0 1,0 6,3 3,5 6)'))",
        (struct expected_dml_result){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ST_FrechetDistance(g, ST_GeomFromText('LINESTRING(0 1,0 6,3 3,5 6)')), "
                   "ST_HausdorffDistance(g, ST_GeomFromText('LINESTRING(0 1,0 6,3 3,5 6)')) "
                   "FROM discrete_distance_values ORDER BY id",
            .column_count = 2U,
            .values = discrete_distance_values,
            .row_count = 2U,
            .context = "row-backed discrete distance projection",
        }
    );
    failures +=
        execute_ok(database, "CREATE TABLE centroid_values(id INT PRIMARY KEY, g GEOMETRY)", NULL);
    failures += expect_dml_ok(
        database,
        "INSERT INTO centroid_values VALUES "
        "(1, Point(2,4)), "
        "(2, ST_GeomFromText('LINESTRING(0 0,0 4,4 4)')), "
        "(3, ST_GeomFromText('POLYGON((0 0,10 0,10 10,0 10,0 0),(4 4,6 4,6 6,4 6,4 4))'))",
        (struct expected_dml_result){.affected_rows = 3, .warning_count = 0U}
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ST_AsText(ST_Centroid(g)) FROM centroid_values ORDER BY id",
            .column_count = 1U,
            .values = centroid_values,
            .row_count = 3U,
            .context = "row-backed centroid projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ST_AsText(ST_ConvexHull(g)) FROM spatial_values ORDER BY id",
            .column_count = 1U,
            .values = convex_hull_values,
            .row_count = sizeof(convex_hull_values) / sizeof(convex_hull_values[0]),
            .context = "row-backed convex hull projection",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_spatial_measure_accessor_diagnostics(void) {
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open diagnostics database");
    failures += execute_error(
        database,
        "SELECT ST_Area(Point(1,2))",
        (struct expected_sql_error){
            .code = mysql_error_unexpected_geometry_type,
            .sqlstate = "22S01",
            .message_part =
                "POLYGON/MULTIPOLYGON value is a geometry of unexpected type POINT in st_area",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_AsText(ST_MakeEnvelope(Point(1,2), ST_GeomFromText('LINESTRING(0 0,1 1)')))",
        (struct expected_sql_error){
            .code = mysql_error_wrong_arguments,
            .sqlstate = "HY000",
            .message_part = "Incorrect arguments to st_makeenvelope",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_Distance(Point(0,0), Point(1,1), 'metre')",
        (struct expected_sql_error){
            .code = mysql_error_geometry_unknown_length_unit,
            .sqlstate = "SU001",
            .message_part =
                "The geometry passed to function st_distance is in SRID 0, which doesn't specify "
                "a length unit. Can't convert to 'metre'.",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_Distance(ST_PointFromGeoHash('mh2n0p0581',4326), Point(1,1))",
        (struct expected_sql_error){
            .code = mysql_error_gis_different_srids,
            .sqlstate = "HY000",
            .message_part =
                "Binary geometry function st_distance given two geometries of different srids: "
                "4326 and 0, which should have been identical.",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_AsText(ST_LineInterpolatePoint(Point(1,2), '0.5'))",
        (struct expected_sql_error){
            .code = mysql_error_unexpected_geometry_type,
            .sqlstate = "22S01",
            .message_part = "LINESTRING value is a geometry of unexpected type POINT in "
                            "st_lineinterpolatepoint",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_AsText(ST_LineInterpolatePoints(ST_GeomFromText('LINESTRING(0 0,0 5)'), "
        "'1.1'))",
        (struct expected_sql_error){
            .code = mysql_error_numeric_value_out_of_range,
            .sqlstate = "22003",
            .message_part = "Distance value is out of range in 'st_lineinterpolatepoints'",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_AsText(ST_PointAtDistance(ST_GeomFromText('LINESTRING(0 0,0 5)'), 6))",
        (struct expected_sql_error){
            .code = mysql_error_numeric_value_out_of_range,
            .sqlstate = "22003",
            .message_part = "Distance value is out of range in 'st_pointatdistance'",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_Distance_Sphere(Point(0,0), ST_GeomFromText('LINESTRING(0 0,1 1)'))",
        (struct expected_sql_error){
            .code = mysql_error_not_implemented_for_cartesian_srs,
            .sqlstate = "22S00",
            .message_part = "st_distance_sphere(POINT, LINESTRING) has not been implemented for "
                            "Cartesian spatial reference systems",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_Distance_Sphere(Point(0,0), ST_GeomFromText('GEOMETRYCOLLECTION EMPTY'))",
        (struct expected_sql_error){
            .code = mysql_error_not_implemented_for_cartesian_srs,
            .sqlstate = "22S00",
            .message_part = "st_distance_sphere(POINT, GEOMCOLLECTION) has not been "
                            "implemented for Cartesian spatial reference systems",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_Distance_Sphere(Point(-180,0), Point(0,0))",
        (struct expected_sql_error){
            .code = mysql_error_geojson_longitude_out_of_range,
            .sqlstate = "22S02",
            .message_part = "Longitude -180.000000 is out of range in function st_distance_sphere",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_Distance_Sphere(Point(0,91), Point(0,0))",
        (struct expected_sql_error){
            .code = mysql_error_geojson_latitude_out_of_range,
            .sqlstate = "22S03",
            .message_part = "Latitude 91.000000 is out of range in function st_distance_sphere",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_Distance_Sphere(Point(0,0), Point(1,1), 0)",
        (struct expected_sql_error){
            .code = mysql_error_nonpositive_radius,
            .sqlstate = "22003",
            .message_part = "Invalid radius provided to function st_distance_sphere: Radius must "
                            "be greater than zero",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_FrechetDistance(Point(0,0), ST_GeomFromText('LINESTRING(0 0,1 1)'))",
        (struct expected_sql_error){
            .code = mysql_error_not_implemented_for_cartesian_srs,
            .sqlstate = "22S00",
            .message_part = "st_frechetdistance(POINT, LINESTRING) has not been implemented for "
                            "Cartesian spatial reference systems",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_HausdorffDistance(Point(0,0), Point(1,1))",
        (struct expected_sql_error){
            .code = mysql_error_not_implemented_for_cartesian_srs,
            .sqlstate = "22S00",
            .message_part = "st_hausdorffdistance(POINT, POINT) has not been implemented for "
                            "Cartesian spatial reference systems",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_FrechetDistance(ST_GeomFromText('LINESTRING(0 0,1 1)'), "
        "ST_GeomFromText('LINESTRING(0 0,1 1)'), 'metre')",
        (struct expected_sql_error){
            .code = mysql_error_geometry_unknown_length_unit,
            .sqlstate = "SU001",
            .message_part = "The geometry passed to function st_frechetdistance is in SRID 0",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_HausdorffDistance(Point(0,0), ST_GeomFromText('MULTIPOINT(1 1)'), 'metre')",
        (struct expected_sql_error){
            .code = mysql_error_geometry_unknown_length_unit,
            .sqlstate = "SU001",
            .message_part = "The geometry passed to function st_hausdorffdistance is in SRID 0",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_AsText(ST_Centroid(ST_PointFromGeoHash('mh2n0p0581',4326)))",
        (struct expected_sql_error){
            .code = mysql_error_not_implemented_for_geographic_srs,
            .sqlstate = "22S00",
            .message_part = "st_centroid(POINT) has not been implemented for geographic "
                            "spatial reference systems",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_AsText(ST_Centroid(ST_GeomFromText('POLYGON((0 0,1 1,2 2,0 0))')))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_gis_data,
            .sqlstate = "22023",
            .message_part = "Invalid GIS data provided to function st_centroid",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_AsText(ST_ConvexHull(ST_PointFromGeoHash('mh2n0p0581',4326)))",
        (struct expected_sql_error){
            .code = mysql_error_not_implemented_for_geographic_srs,
            .sqlstate = "22S00",
            .message_part = "st_convexhull(POINT) has not been implemented for geographic "
                            "spatial reference systems",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_AsText(ST_ConvexHull(ST_GeomFromText('POLYGON((0 0,1 1,2 2,0 0))')))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_gis_data,
            .sqlstate = "22023",
            .message_part = "Invalid GIS data provided to function st_convexhull",
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

            failures += expect_result_value(
                result,
                row,
                column,
                expected.values[index],
                expected.context,
                expected.double_tolerance
            );
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
    const char *context,
    double double_tolerance
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
    if (double_tolerance > 0.0) {
        return expect_result_double_value(actual, expected, context, row, column, double_tolerance);
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

static int expect_result_double_value(
    const char *actual,
    const char *expected,
    const char *context,
    size_t row,
    size_t column,
    double tolerance
) {
    char *actual_end = NULL;
    char *expected_end = NULL;
    double actual_value = 0.0;
    double expected_value = 0.0;

    if (actual == NULL) {
        fprintf(
            stderr,
            "%s: row %zu column %zu expected %s, got NULL\n",
            context,
            row,
            column,
            expected
        );
        return 1;
    }
    actual_value = strtod(actual, &actual_end);
    expected_value = strtod(expected, &expected_end);
    if (actual_end == actual || actual_end == NULL || *actual_end != '\0' ||
        expected_end == expected || expected_end == NULL || *expected_end != '\0' ||
        !isfinite(actual_value) || !isfinite(expected_value) ||
        fabs(actual_value - expected_value) > tolerance) {
        fprintf(
            stderr,
            "%s: row %zu column %zu expected %s within %.17g, got %s\n",
            context,
            row,
            column,
            expected,
            tolerance,
            actual
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
        "/tmp/mylite_spatial_measure_accessor_%s_%d.mylite",
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
