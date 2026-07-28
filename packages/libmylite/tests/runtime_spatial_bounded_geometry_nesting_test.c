#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include "runtime/mylite_spatial.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    geometry_depth_limit = MYLITE_SPATIAL_MAX_GEOMETRY_DEPTH,
    geometry_collection_wrapper_at_limit = geometry_depth_limit - 1,
    geometry_collection_wrapper_above_limit = geometry_depth_limit,
    internal_srid_size = 4,
    wkb_collection_header_size = 9,
    wkb_collection_count_offset = 5,
    wkb_point_size = 21,
    large_geometry_point_count = 4000,
    previous_fuzz_input_ceiling = 65536,
    spatial_byte_bit_count = 8,
    spatial_byte_mask = 0xff,
    mysql_error_invalid_gis_data = 3037,
    mysql_error_json_document_too_deep = 3157,
};

static const double simplify_distance = 0.5;

struct generated_bytes {
    unsigned char *bytes;
    size_t size;
};

static int test_direct_constructor_depths(void);
static int test_internal_geometry_depths(void);
static int test_bounded_downstream_operations(void);
static int test_large_geometry_depths(void);
static int test_sql_depth_diagnostics_and_recovery(void);
static int expect_direct_success(
    enum mylite_spatial_function_kind kind,
    const void *bytes,
    size_t byte_count,
    const char *context
);
static int expect_direct_error(
    enum mylite_spatial_function_kind kind,
    const void *bytes,
    size_t byte_count,
    int expected_code,
    const char *expected_sqlstate,
    const char *context
);
static int expect_internal_operation(
    enum mylite_spatial_function_kind kind,
    const struct generated_bytes *geometry,
    bool should_succeed,
    const char *context
);
static int expect_internal_binary_operation(
    enum mylite_spatial_function_kind kind,
    const struct generated_bytes *geometry,
    bool should_succeed,
    const char *context
);
static int expect_internal_numeric_operation(
    enum mylite_spatial_function_kind kind,
    const struct generated_bytes *geometry,
    double numeric,
    bool should_succeed,
    const char *context
);
static int expect_operation(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    bool should_succeed,
    const char *context
);
static int execute_error(
    mylite_db *database,
    const char *sql,
    int expected_code,
    const char *expected_sqlstate,
    const char *message_part
);
static int execute_scalar_one(mylite_db *database, const char *sql, const char *context);
static int execute_statement(mylite_db *database, const char *sql, const char *context);
static struct generated_bytes make_nested_internal_geometry(size_t wrapper_count);
static struct generated_bytes make_ordered_nested_internal_geometry(
    size_t wrapper_count,
    bool little_endian,
    bool alternate_endian
);
static struct generated_bytes make_wide_internal_geometry(
    size_t wrapper_count,
    uint32_t point_count
);
static char *make_nested_wkt(size_t wrapper_count);
static char *make_nested_geojson(size_t wrapper_count);
static char *make_sql(const char *prefix, const char *value, const char *suffix);
static void write_u32_endian(unsigned char *destination, uint32_t value, bool little_endian);
static void write_u32_le(unsigned char *destination, uint32_t value);

int main(void) {
    int failures = 0;

    failures += test_direct_constructor_depths();
    failures += test_internal_geometry_depths();
    failures += test_bounded_downstream_operations();
    failures += test_large_geometry_depths();
    failures += test_sql_depth_diagnostics_and_recovery();

    return failures == 0 ? 0 : 1;
}

static int test_direct_constructor_depths(void) {
    static const enum mylite_spatial_function_kind wkt_constructors[] = {
        MYLITE_SPATIAL_FUNCTION_ST_GEOMFROMTEXT,
        MYLITE_SPATIAL_FUNCTION_ST_GEOMETRYFROMTEXT,
        MYLITE_SPATIAL_FUNCTION_ST_POINTFROMTEXT,
        MYLITE_SPATIAL_FUNCTION_ST_LINEFROMTEXT,
        MYLITE_SPATIAL_FUNCTION_ST_LINESTRINGFROMTEXT,
        MYLITE_SPATIAL_FUNCTION_ST_POLYFROMTEXT,
        MYLITE_SPATIAL_FUNCTION_ST_POLYGONFROMTEXT,
        MYLITE_SPATIAL_FUNCTION_ST_MPOINTFROMTEXT,
        MYLITE_SPATIAL_FUNCTION_ST_MULTIPOINTFROMTEXT,
        MYLITE_SPATIAL_FUNCTION_ST_MLINEFROMTEXT,
        MYLITE_SPATIAL_FUNCTION_ST_MULTILINESTRINGFROMTEXT,
        MYLITE_SPATIAL_FUNCTION_ST_MPOLYFROMTEXT,
        MYLITE_SPATIAL_FUNCTION_ST_MULTIPOLYGONFROMTEXT,
        MYLITE_SPATIAL_FUNCTION_ST_GEOMCOLLFROMTEXT,
        MYLITE_SPATIAL_FUNCTION_ST_GEOMETRYCOLLECTIONFROMTEXT,
        MYLITE_SPATIAL_FUNCTION_ST_GEOMCOLLFROMTXT,
    };

    static const enum mylite_spatial_function_kind wkb_constructors[] = {
        MYLITE_SPATIAL_FUNCTION_ST_GEOMFROMWKB,
        MYLITE_SPATIAL_FUNCTION_ST_GEOMETRYFROMWKB,
        MYLITE_SPATIAL_FUNCTION_ST_POINTFROMWKB,
        MYLITE_SPATIAL_FUNCTION_ST_LINEFROMWKB,
        MYLITE_SPATIAL_FUNCTION_ST_LINESTRINGFROMWKB,
        MYLITE_SPATIAL_FUNCTION_ST_POLYFROMWKB,
        MYLITE_SPATIAL_FUNCTION_ST_POLYGONFROMWKB,
        MYLITE_SPATIAL_FUNCTION_ST_MPOINTFROMWKB,
        MYLITE_SPATIAL_FUNCTION_ST_MULTIPOINTFROMWKB,
        MYLITE_SPATIAL_FUNCTION_ST_MLINEFROMWKB,
        MYLITE_SPATIAL_FUNCTION_ST_MULTILINESTRINGFROMWKB,
        MYLITE_SPATIAL_FUNCTION_ST_MPOLYFROMWKB,
        MYLITE_SPATIAL_FUNCTION_ST_MULTIPOLYGONFROMWKB,
        MYLITE_SPATIAL_FUNCTION_ST_GEOMCOLLFROMWKB,
        MYLITE_SPATIAL_FUNCTION_ST_GEOMETRYCOLLECTIONFROMWKB,
    };
    struct generated_bytes below_limit =
        make_nested_internal_geometry(geometry_collection_wrapper_at_limit - 1U);
    struct generated_bytes at_limit =
        make_nested_internal_geometry(geometry_collection_wrapper_at_limit);
    struct generated_bytes above_limit =
        make_nested_internal_geometry(geometry_collection_wrapper_above_limit);
    struct generated_bytes big_endian_at_limit =
        make_ordered_nested_internal_geometry(geometry_collection_wrapper_at_limit, false, false);
    struct generated_bytes big_endian_above_limit = make_ordered_nested_internal_geometry(
        geometry_collection_wrapper_above_limit,
        false,
        false
    );
    struct generated_bytes mixed_endian_at_limit =
        make_ordered_nested_internal_geometry(geometry_collection_wrapper_at_limit, true, true);
    struct generated_bytes empty_at_limit =
        make_wide_internal_geometry(geometry_depth_limit - 1U, 0U);
    struct generated_bytes siblings_at_limit =
        make_wide_internal_geometry(geometry_depth_limit - 2U, 2U);
    char *wkt_below_limit = make_nested_wkt(geometry_collection_wrapper_at_limit - 1U);
    char *wkt_at_limit = make_nested_wkt(geometry_collection_wrapper_at_limit);
    char *wkt_above_limit = make_nested_wkt(geometry_collection_wrapper_above_limit);
    char *geojson_below_limit = make_nested_geojson(geometry_collection_wrapper_at_limit - 1U);
    char *geojson_at_limit = make_nested_geojson(geometry_collection_wrapper_at_limit);
    char *geojson_above_limit = make_nested_geojson(geometry_collection_wrapper_above_limit);
    int failures = 0;

    if (below_limit.bytes == NULL || at_limit.bytes == NULL || above_limit.bytes == NULL ||
        big_endian_at_limit.bytes == NULL || big_endian_above_limit.bytes == NULL ||
        mixed_endian_at_limit.bytes == NULL || empty_at_limit.bytes == NULL ||
        siblings_at_limit.bytes == NULL || wkt_below_limit == NULL || wkt_at_limit == NULL ||
        wkt_above_limit == NULL || geojson_below_limit == NULL || geojson_at_limit == NULL ||
        geojson_above_limit == NULL) {
        failures = 1;
        goto cleanup;
    }
    failures += expect_direct_success(
        MYLITE_SPATIAL_FUNCTION_ST_GEOMFROMTEXT,
        wkt_below_limit,
        strlen(wkt_below_limit),
        "WKT depth 49"
    );
    failures += expect_direct_success(
        MYLITE_SPATIAL_FUNCTION_ST_GEOMFROMTEXT,
        wkt_at_limit,
        strlen(wkt_at_limit),
        "WKT depth 50"
    );
    failures += expect_direct_error(
        MYLITE_SPATIAL_FUNCTION_ST_GEOMFROMTEXT,
        wkt_above_limit,
        strlen(wkt_above_limit),
        mysql_error_invalid_gis_data,
        "22023",
        "WKT depth 51"
    );
    failures += expect_direct_success(
        MYLITE_SPATIAL_FUNCTION_ST_GEOMFROMWKB,
        below_limit.bytes + internal_srid_size,
        below_limit.size - internal_srid_size,
        "WKB depth 49"
    );
    failures += expect_direct_success(
        MYLITE_SPATIAL_FUNCTION_ST_GEOMFROMWKB,
        at_limit.bytes + internal_srid_size,
        at_limit.size - internal_srid_size,
        "WKB depth 50"
    );
    failures += expect_direct_error(
        MYLITE_SPATIAL_FUNCTION_ST_GEOMFROMWKB,
        above_limit.bytes + internal_srid_size,
        above_limit.size - internal_srid_size,
        mysql_error_invalid_gis_data,
        "22023",
        "WKB depth 51"
    );
    failures += expect_direct_success(
        MYLITE_SPATIAL_FUNCTION_ST_GEOMFROMWKB,
        big_endian_at_limit.bytes + internal_srid_size,
        big_endian_at_limit.size - internal_srid_size,
        "big-endian WKB depth 50"
    );
    failures += expect_direct_error(
        MYLITE_SPATIAL_FUNCTION_ST_GEOMFROMWKB,
        big_endian_above_limit.bytes + internal_srid_size,
        big_endian_above_limit.size - internal_srid_size,
        mysql_error_invalid_gis_data,
        "22023",
        "big-endian WKB depth 51"
    );
    failures += expect_direct_success(
        MYLITE_SPATIAL_FUNCTION_ST_GEOMFROMWKB,
        mixed_endian_at_limit.bytes + internal_srid_size,
        mixed_endian_at_limit.size - internal_srid_size,
        "mixed-endian WKB depth 50"
    );
    failures += expect_direct_error(
        MYLITE_SPATIAL_FUNCTION_ST_GEOMFROMWKB,
        above_limit.bytes + internal_srid_size,
        above_limit.size - internal_srid_size - wkb_point_size,
        mysql_error_invalid_gis_data,
        "22023",
        "WKB depth 51 truncated before rejected child header"
    );
    failures += expect_direct_error(
        MYLITE_SPATIAL_FUNCTION_ST_GEOMFROMWKB,
        above_limit.bytes + internal_srid_size,
        above_limit.size - internal_srid_size - wkb_point_size + wkb_collection_count_offset,
        mysql_error_invalid_gis_data,
        "22023",
        "WKB depth 51 truncated after rejected child header"
    );
    failures += expect_direct_success(
        MYLITE_SPATIAL_FUNCTION_ST_GEOMFROMGEOJSON,
        geojson_below_limit,
        strlen(geojson_below_limit),
        "GeoJSON depth 49"
    );
    failures += expect_direct_success(
        MYLITE_SPATIAL_FUNCTION_ST_GEOMFROMGEOJSON,
        geojson_at_limit,
        strlen(geojson_at_limit),
        "GeoJSON depth 50"
    );
    failures += expect_direct_error(
        MYLITE_SPATIAL_FUNCTION_ST_GEOMFROMGEOJSON,
        geojson_above_limit,
        strlen(geojson_above_limit),
        mysql_error_json_document_too_deep,
        "22032",
        "GeoJSON depth 51"
    );
    failures += expect_direct_success(
        MYLITE_SPATIAL_FUNCTION_GEOMETRYCOLLECTION,
        below_limit.bytes,
        below_limit.size,
        "GeometryCollection constructor result depth 50"
    );
    failures += expect_direct_error(
        MYLITE_SPATIAL_FUNCTION_GEOMETRYCOLLECTION,
        at_limit.bytes,
        at_limit.size,
        mysql_error_invalid_gis_data,
        "22023",
        "GeometryCollection constructor result depth 51"
    );
    failures += mylite_test_expect_int(
        mylite_spatial_geometry_bytes_are_valid(empty_at_limit.bytes, empty_at_limit.size),
        1,
        "empty GeometryCollection at depth 50"
    );
    failures += mylite_test_expect_int(
        mylite_spatial_geometry_bytes_are_valid(siblings_at_limit.bytes, siblings_at_limit.size),
        1,
        "sibling geometries at depth 50 reuse the depth budget"
    );
    for (size_t index = 0U; index < sizeof(wkt_constructors) / sizeof(wkt_constructors[0]);
         ++index) {
        failures += expect_direct_error(
            wkt_constructors[index],
            wkt_above_limit,
            strlen(wkt_above_limit),
            mysql_error_invalid_gis_data,
            "22023",
            mylite_spatial_function_name(wkt_constructors[index])
        );
    }
    for (size_t index = 0U; index < sizeof(wkb_constructors) / sizeof(wkb_constructors[0]);
         ++index) {
        failures += expect_direct_error(
            wkb_constructors[index],
            above_limit.bytes + internal_srid_size,
            above_limit.size - internal_srid_size,
            mysql_error_invalid_gis_data,
            "22023",
            mylite_spatial_function_name(wkb_constructors[index])
        );
    }

cleanup:
    free(below_limit.bytes);
    free(at_limit.bytes);
    free(above_limit.bytes);
    free(big_endian_at_limit.bytes);
    free(big_endian_above_limit.bytes);
    free(mixed_endian_at_limit.bytes);
    free(empty_at_limit.bytes);
    free(siblings_at_limit.bytes);
    free(wkt_below_limit);
    free(wkt_at_limit);
    free(wkt_above_limit);
    free(geojson_below_limit);
    free(geojson_at_limit);
    free(geojson_above_limit);
    return failures;
}

static int test_internal_geometry_depths(void) {
    struct generated_bytes below_limit =
        make_nested_internal_geometry(geometry_collection_wrapper_at_limit - 1U);
    struct generated_bytes at_limit =
        make_nested_internal_geometry(geometry_collection_wrapper_at_limit);
    struct generated_bytes above_limit =
        make_nested_internal_geometry(geometry_collection_wrapper_above_limit);
    int failures = 0;

    if (below_limit.bytes == NULL || at_limit.bytes == NULL || above_limit.bytes == NULL) {
        failures = 1;
        goto cleanup;
    }
    failures += mylite_test_expect_int(
        mylite_spatial_geometry_bytes_are_valid(below_limit.bytes, below_limit.size),
        1,
        "internal geometry depth 49 is valid"
    );
    failures += mylite_test_expect_int(
        mylite_spatial_geometry_bytes_are_valid(at_limit.bytes, at_limit.size),
        1,
        "internal geometry depth 50 is valid"
    );
    failures += mylite_test_expect_int(
        mylite_spatial_geometry_bytes_are_valid(above_limit.bytes, above_limit.size),
        0,
        "internal geometry depth 51 is invalid"
    );

cleanup:
    free(below_limit.bytes);
    free(at_limit.bytes);
    free(above_limit.bytes);
    return failures;
}

static int test_bounded_downstream_operations(void) {
    static const enum mylite_spatial_function_kind kinds[] = {
        MYLITE_SPATIAL_FUNCTION_ST_ASTEXT,
        MYLITE_SPATIAL_FUNCTION_ST_ASWKB,
        MYLITE_SPATIAL_FUNCTION_ST_ASGEOJSON,
        MYLITE_SPATIAL_FUNCTION_ST_DIMENSION,
        MYLITE_SPATIAL_FUNCTION_ST_ISEMPTY,
        MYLITE_SPATIAL_FUNCTION_ST_NUMGEOMETRIES,
        MYLITE_SPATIAL_FUNCTION_ST_LENGTH,
        MYLITE_SPATIAL_FUNCTION_ST_ENVELOPE,
        MYLITE_SPATIAL_FUNCTION_ST_SWAPXY,
        MYLITE_SPATIAL_FUNCTION_ST_CENTROID,
        MYLITE_SPATIAL_FUNCTION_ST_CONVEXHULL,
        MYLITE_SPATIAL_FUNCTION_ST_ISVALID,
        MYLITE_SPATIAL_FUNCTION_ST_ISSIMPLE,
    };
    struct generated_bytes at_limit =
        make_nested_internal_geometry(geometry_collection_wrapper_at_limit);
    struct generated_bytes above_limit =
        make_nested_internal_geometry(geometry_collection_wrapper_above_limit);
    int failures = 0;

    if (at_limit.bytes == NULL || above_limit.bytes == NULL) {
        failures = 1;
        goto cleanup;
    }
    for (size_t index = 0U; index < sizeof(kinds) / sizeof(kinds[0]); ++index) {
        const char *name = mylite_spatial_function_name(kinds[index]);

        failures += expect_internal_operation(kinds[index], &at_limit, true, name);
        failures += expect_internal_operation(kinds[index], &above_limit, false, name);
    }
    failures += expect_internal_binary_operation(
        MYLITE_SPATIAL_FUNCTION_ST_DISTANCE,
        &at_limit,
        true,
        "ST_Distance depth 50"
    );
    failures += expect_internal_binary_operation(
        MYLITE_SPATIAL_FUNCTION_ST_DISTANCE,
        &above_limit,
        false,
        "ST_Distance depth 51"
    );
    failures += expect_internal_binary_operation(
        MYLITE_SPATIAL_FUNCTION_ST_INTERSECTS,
        &at_limit,
        true,
        "ST_Intersects depth 50"
    );
    failures += expect_internal_binary_operation(
        MYLITE_SPATIAL_FUNCTION_ST_INTERSECTS,
        &above_limit,
        false,
        "ST_Intersects depth 51"
    );
    failures += expect_internal_numeric_operation(
        MYLITE_SPATIAL_FUNCTION_ST_SIMPLIFY,
        &at_limit,
        simplify_distance,
        true,
        "ST_Simplify depth 50"
    );
    failures += expect_internal_numeric_operation(
        MYLITE_SPATIAL_FUNCTION_ST_SIMPLIFY,
        &above_limit,
        simplify_distance,
        false,
        "ST_Simplify depth 51"
    );
    failures += expect_internal_numeric_operation(
        MYLITE_SPATIAL_FUNCTION_ST_TRANSFORM,
        &at_limit,
        0.0,
        true,
        "ST_Transform depth 50"
    );
    failures += expect_internal_numeric_operation(
        MYLITE_SPATIAL_FUNCTION_ST_TRANSFORM,
        &above_limit,
        0.0,
        false,
        "ST_Transform depth 51"
    );

cleanup:
    free(at_limit.bytes);
    free(above_limit.bytes);
    return failures;
}

static int test_large_geometry_depths(void) {
    struct generated_bytes shallow = make_wide_internal_geometry(0U, large_geometry_point_count);
    struct generated_bytes at_limit =
        make_wide_internal_geometry(geometry_depth_limit - 2U, large_geometry_point_count);
    struct generated_bytes above_limit =
        make_wide_internal_geometry(geometry_depth_limit - 1U, large_geometry_point_count);
    int failures = 0;

    if (shallow.bytes == NULL || at_limit.bytes == NULL || above_limit.bytes == NULL) {
        failures = 1;
        goto cleanup;
    }
    failures += mylite_test_expect_int(
        shallow.size > previous_fuzz_input_ceiling,
        1,
        "large shallow geometry exceeds old fuzz ceiling"
    );
    failures += mylite_test_expect_int(
        at_limit.size > previous_fuzz_input_ceiling,
        1,
        "large depth-50 geometry exceeds old fuzz ceiling"
    );
    failures += mylite_test_expect_int(
        mylite_spatial_geometry_bytes_are_valid(shallow.bytes, shallow.size),
        1,
        "large shallow geometry is valid"
    );
    failures += mylite_test_expect_int(
        mylite_spatial_geometry_bytes_are_valid(at_limit.bytes, at_limit.size),
        1,
        "large depth-50 geometry is valid"
    );
    failures += mylite_test_expect_int(
        mylite_spatial_geometry_bytes_are_valid(above_limit.bytes, above_limit.size),
        0,
        "large depth-51 geometry is invalid"
    );
    failures += expect_direct_success(
        MYLITE_SPATIAL_FUNCTION_ST_GEOMFROMWKB,
        at_limit.bytes + internal_srid_size,
        at_limit.size - internal_srid_size,
        "large WKB depth 50"
    );
    failures += expect_direct_error(
        MYLITE_SPATIAL_FUNCTION_ST_GEOMFROMWKB,
        above_limit.bytes + internal_srid_size,
        above_limit.size - internal_srid_size,
        mysql_error_invalid_gis_data,
        "22023",
        "large WKB depth 51"
    );

cleanup:
    free(shallow.bytes);
    free(at_limit.bytes);
    free(above_limit.bytes);
    return failures;
}

static int test_sql_depth_diagnostics_and_recovery(void) {
    char *wkt_at_limit = make_nested_wkt(geometry_collection_wrapper_at_limit);
    char *wkt_above_limit = make_nested_wkt(geometry_collection_wrapper_above_limit);
    char *at_limit_sql = make_sql("SELECT ST_Dimension(ST_GeomFromText('", wkt_at_limit, "'))");
    char *above_limit_sql =
        make_sql("SELECT ST_Dimension(ST_GeomFromText('", wkt_above_limit, "'))");
    char *insert_at_limit_sql =
        make_sql("INSERT INTO nesting_shapes VALUES (ST_GeomFromText('", wkt_at_limit, "'))");
    mylite_db *database = NULL;
    int failures = 0;

    if (wkt_at_limit == NULL || wkt_above_limit == NULL || at_limit_sql == NULL ||
        above_limit_sql == NULL || insert_at_limit_sql == NULL) {
        failures = 1;
        goto cleanup;
    }
    failures += mylite_test_expect_int(
        mylite_open_memory(&database),
        MYLITE_OK,
        "open bounded geometry database"
    );
    if (database == NULL) {
        failures = 1;
        goto cleanup;
    }
    failures += execute_statement(database, "CREATE DATABASE app", "create aggregate database");
    failures += execute_statement(database, "USE app", "select aggregate database");
    failures += execute_scalar_one(database, at_limit_sql, "SQL WKT depth 50");
    failures += execute_error(
        database,
        above_limit_sql,
        mysql_error_invalid_gis_data,
        "22023",
        "Invalid GIS data provided to function st_geomfromtext"
    );
    failures += execute_statement(
        database,
        "CREATE TABLE nesting_shapes (g GEOMETRY)",
        "create nesting aggregate table"
    );
    failures += execute_statement(database, insert_at_limit_sql, "insert depth-50 aggregate input");
    failures += execute_error(
        database,
        "SELECT ST_Collect(g) FROM nesting_shapes",
        mysql_error_invalid_gis_data,
        "22023",
        "Invalid GIS data provided to function st_collect"
    );
    failures += execute_scalar_one(database, "SELECT 1", "handle usable after depth rejection");

cleanup:
    mylite_close(database);
    free(wkt_at_limit);
    free(wkt_above_limit);
    free(at_limit_sql);
    free(above_limit_sql);
    free(insert_at_limit_sql);
    return failures;
}

static int expect_direct_success(
    enum mylite_spatial_function_kind kind,
    const void *bytes,
    size_t byte_count,
    const char *context
) {
    struct mylite_spatial_argument argument = {.bytes = bytes, .byte_count = byte_count};
    struct mylite_spatial_result result = {0};
    struct mylite_spatial_error error = {0};
    int rc = mylite_spatial_evaluate(kind, &argument, 1U, &result, &error);
    int failures = mylite_test_expect_int(rc, 0, context);

    if (rc == 0) {
        failures +=
            mylite_test_expect_int((int)result.kind, MYLITE_SPATIAL_RESULT_GEOMETRY, context);
    }
    mylite_spatial_result_deinit(&result);
    return failures;
}

// NOLINTBEGIN(bugprone-easily-swappable-parameters): test expectation helper.
static int expect_direct_error(
    enum mylite_spatial_function_kind kind,
    const void *bytes,
    size_t byte_count,
    int expected_code,
    const char *expected_sqlstate,
    const char *context
) {
    struct mylite_spatial_argument argument = {.bytes = bytes, .byte_count = byte_count};
    struct mylite_spatial_result result = {0};
    struct mylite_spatial_error error = {0};
    int rc = mylite_spatial_evaluate(kind, &argument, 1U, &result, &error);
    int failures = mylite_test_expect_int(rc, -1, context);

    failures += mylite_test_expect_int(error.code, expected_code, context);
    failures += mylite_test_expect_text(error.sqlstate, expected_sqlstate, context);
    failures += mylite_test_expect_int((int)result.kind, MYLITE_SPATIAL_RESULT_NULL, context);
    mylite_spatial_result_deinit(&result);
    return failures;
}

// NOLINTEND(bugprone-easily-swappable-parameters)

static int expect_internal_operation(
    enum mylite_spatial_function_kind kind,
    const struct generated_bytes *geometry,
    bool should_succeed,
    const char *context
) {
    struct mylite_spatial_argument argument = {
        .bytes = geometry->bytes,
        .byte_count = geometry->size,
    };

    return expect_operation(kind, &argument, 1U, should_succeed, context);
}

static int expect_internal_binary_operation(
    enum mylite_spatial_function_kind kind,
    const struct generated_bytes *geometry,
    bool should_succeed,
    const char *context
) {
    struct mylite_spatial_argument arguments[] = {
        {.bytes = geometry->bytes, .byte_count = geometry->size},
        {.bytes = geometry->bytes, .byte_count = geometry->size},
    };

    return expect_operation(kind, arguments, 2U, should_succeed, context);
}

static int expect_internal_numeric_operation(
    enum mylite_spatial_function_kind kind,
    const struct generated_bytes *geometry,
    double numeric,
    bool should_succeed,
    const char *context
) {
    struct mylite_spatial_argument arguments[] = {
        {.bytes = geometry->bytes, .byte_count = geometry->size},
        {.numeric = numeric, .has_numeric = true},
    };

    return expect_operation(kind, arguments, 2U, should_succeed, context);
}

static int expect_operation(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    bool should_succeed,
    const char *context
) {
    struct mylite_spatial_result result = {0};
    struct mylite_spatial_error error = {0};
    int rc = mylite_spatial_evaluate(kind, arguments, argument_count, &result, &error);
    int failures = mylite_test_expect_int(rc, should_succeed ? 0 : -1, context);

    if (!should_succeed) {
        failures += mylite_test_expect_int(error.code, mysql_error_invalid_gis_data, context);
        failures += mylite_test_expect_text(error.sqlstate, "22023", context);
    }
    mylite_spatial_result_deinit(&result);
    return failures;
}

// NOLINTBEGIN(bugprone-easily-swappable-parameters): test execution helpers.
static int execute_error(
    mylite_db *database,
    const char *sql,
    int expected_code,
    const char *expected_sqlstate,
    const char *message_part
) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = mylite_test_expect_int(rc, MYLITE_ERROR, sql);

    failures += mylite_test_expect_int(mylite_errcode(database), expected_code, sql);
    failures += mylite_test_expect_text(mylite_sqlstate(database), expected_sqlstate, sql);
    failures += mylite_test_expect_contains(mylite_errmsg(database), message_part, sql);
    mylite_result_free(result);
    return failures;
}

static int execute_scalar_one(mylite_db *database, const char *sql, const char *context) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = mylite_test_expect_int(rc, MYLITE_OK, context);

    if (rc == MYLITE_OK) {
        failures += mylite_test_expect_size(mylite_result_row_count(result), 1U, context);
        failures += mylite_test_expect_size(mylite_result_column_count(result), 1U, context);
    }
    mylite_result_free(result);
    return failures;
}

static int execute_statement(mylite_db *database, const char *sql, const char *context) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = mylite_test_expect_int(rc, MYLITE_OK, context);

    mylite_result_free(result);
    return failures;
}

// NOLINTEND(bugprone-easily-swappable-parameters)

static struct generated_bytes make_nested_internal_geometry(size_t wrapper_count) {
    return make_ordered_nested_internal_geometry(wrapper_count, true, false);
}

static struct generated_bytes make_ordered_nested_internal_geometry(
    size_t wrapper_count,
    bool little_endian,
    bool alternate_endian
) {
    size_t wkb_size = wkb_point_size + (wrapper_count * wkb_collection_header_size);
    size_t total_size = internal_srid_size + wkb_size;
    unsigned char *bytes = calloc(total_size, 1U);
    size_t offset = internal_srid_size;

    if (bytes == NULL) {
        return (struct generated_bytes){0};
    }
    for (size_t index = 0U; index < wrapper_count; ++index) {
        bool node_is_little_endian = alternate_endian ? ((index % 2U) == 0U) : little_endian;

        bytes[offset] = node_is_little_endian ? 1U : 0U;
        write_u32_endian(
            bytes + offset + 1U,
            MYLITE_SPATIAL_GEOMETRY_GEOMETRYCOLLECTION,
            node_is_little_endian
        );
        write_u32_endian(bytes + offset + wkb_collection_count_offset, 1U, node_is_little_endian);
        offset += wkb_collection_header_size;
    }
    little_endian = alternate_endian ? ((wrapper_count % 2U) == 0U) : little_endian;
    bytes[offset] = little_endian ? 1U : 0U;
    write_u32_endian(bytes + offset + 1U, MYLITE_SPATIAL_GEOMETRY_POINT, little_endian);
    return (struct generated_bytes){.bytes = bytes, .size = total_size};
}

static struct generated_bytes make_wide_internal_geometry(
    size_t wrapper_count,
    uint32_t point_count
) {
    size_t fixed_size = internal_srid_size + wkb_collection_header_size;
    size_t offset = internal_srid_size;
    size_t total_size = 0U;
    unsigned char *bytes = NULL;

    if (wrapper_count > (SIZE_MAX - fixed_size) / wkb_collection_header_size ||
        (size_t)point_count > (SIZE_MAX - fixed_size - (wrapper_count * wkb_collection_header_size)
                              ) / wkb_point_size) {
        return (struct generated_bytes){0};
    }
    total_size = fixed_size + (wrapper_count * wkb_collection_header_size) +
                 ((size_t)point_count * wkb_point_size);
    bytes = calloc(total_size, 1U);
    if (bytes == NULL) {
        return (struct generated_bytes){0};
    }
    for (size_t index = 0U; index < wrapper_count; ++index) {
        bytes[offset] = 1U;
        write_u32_le(bytes + offset + 1U, MYLITE_SPATIAL_GEOMETRY_GEOMETRYCOLLECTION);
        write_u32_le(bytes + offset + wkb_collection_count_offset, 1U);
        offset += wkb_collection_header_size;
    }
    bytes[offset] = 1U;
    write_u32_le(bytes + offset + 1U, MYLITE_SPATIAL_GEOMETRY_GEOMETRYCOLLECTION);
    write_u32_le(bytes + offset + wkb_collection_count_offset, point_count);
    offset += wkb_collection_header_size;
    for (uint32_t index = 0U; index < point_count; ++index) {
        bytes[offset] = 1U;
        write_u32_le(bytes + offset + 1U, MYLITE_SPATIAL_GEOMETRY_POINT);
        offset += wkb_point_size;
    }
    return (struct generated_bytes){.bytes = bytes, .size = total_size};
}

static char *make_nested_wkt(size_t wrapper_count) {
    static const char prefix[] = "GEOMETRYCOLLECTION(";
    static const char point[] = "POINT(0 0)";
    const size_t wrapper_size = sizeof(prefix);
    size_t size = 0U;
    char *text = NULL;
    size_t offset = 0U;

    if (wrapper_count > (SIZE_MAX - sizeof(point)) / wrapper_size) {
        return NULL;
    }
    size = (wrapper_count * wrapper_size) + sizeof(point);
    text = malloc(size);

    if (text == NULL) {
        return NULL;
    }
    for (size_t index = 0U; index < wrapper_count; ++index) {
        memcpy(text + offset, prefix, sizeof(prefix) - 1U);
        offset += sizeof(prefix) - 1U;
    }
    memcpy(text + offset, point, sizeof(point) - 1U);
    offset += sizeof(point) - 1U;
    for (size_t index = 0U; index < wrapper_count; ++index) {
        text[offset++] = ')';
    }
    text[offset] = '\0';
    return text;
}

static char *make_nested_geojson(size_t wrapper_count) {
    static const char prefix[] = "{\"type\":\"GeometryCollection\",\"geometries\":[";
    static const char suffix[] = "]}";
    static const char point[] = "{\"type\":\"Point\",\"coordinates\":[0,0]}";
    const size_t wrapper_size = sizeof(prefix) - 1U + sizeof(suffix) - 1U;
    size_t size = 0U;
    char *text = NULL;
    size_t offset = 0U;

    if (wrapper_count > (SIZE_MAX - sizeof(point) - sizeof(suffix)) / wrapper_size) {
        return NULL;
    }
    size = (wrapper_count * wrapper_size) + sizeof(point);
    text = malloc(size + sizeof(suffix));
    if (text == NULL) {
        return NULL;
    }
    for (size_t index = 0U; index < wrapper_count; ++index) {
        memcpy(text + offset, prefix, sizeof(prefix) - 1U);
        offset += sizeof(prefix) - 1U;
    }
    memcpy(text + offset, point, sizeof(point) - 1U);
    offset += sizeof(point) - 1U;
    for (size_t index = 0U; index < wrapper_count; ++index) {
        memcpy(text + offset, suffix, sizeof(suffix) - 1U);
        offset += sizeof(suffix) - 1U;
    }
    text[offset] = '\0';
    return text;
}

static char *make_sql(const char *prefix, const char *value, const char *suffix) {
    size_t prefix_size = strlen(prefix);
    size_t value_size = strlen(value);
    size_t suffix_size = strlen(suffix);
    char *sql = malloc(prefix_size + value_size + suffix_size + 1U);

    if (sql == NULL) {
        return NULL;
    }
    memcpy(sql, prefix, prefix_size); // NOLINT(bugprone-not-null-terminated-result)
    // NOLINTNEXTLINE(bugprone-not-null-terminated-result): suffix adds the terminator.
    memcpy(sql + prefix_size, value, value_size);
    memcpy(sql + prefix_size + value_size, suffix, suffix_size + 1U);
    return sql;
}

static void write_u32_le(unsigned char *destination, uint32_t value) {
    write_u32_endian(destination, value, true);
}

static void write_u32_endian(unsigned char *destination, uint32_t value, bool little_endian) {
    for (size_t index = 0U; index < sizeof(value); ++index) {
        size_t destination_index = little_endian ? index : sizeof(value) - index - 1U;

        destination[destination_index] =
            (unsigned char)((value >> (index * spatial_byte_bit_count)) & spatial_byte_mask);
    }
}
