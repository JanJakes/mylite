#include "mylite_spatial.h"

#include "mylite_json_internal.h"

#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    spatial_internal_srid_size = 4,
    spatial_wkb_header_size = 5,
    spatial_coordinate_size = 16,
    spatial_segment_endpoint_count = 2,
    spatial_rectangle_ring_point_count = 5,
    spatial_byte_bit_count = 8,
    spatial_u32_second_byte_shift = 8,
    spatial_u32_third_byte_shift = 16,
    spatial_u32_fourth_byte_shift = 24,
    spatial_u32_byte_mask = 0xffU,
    spatial_buffer_initial_capacity = 64,
    spatial_diagnostic_function_name_capacity = 64,
    spatial_double_text_capacity = 64,
    spatial_geohash_max_length = 100,
    spatial_geohash_decode_max_length = 433,
    spatial_geohash_error_preview_length = 48,
    spatial_geohash_bits_per_character = 5,
    spatial_geohash_high_bit = 16,
    spatial_geohash_max_round_decimals = 6,
    spatial_geojson_double_text_capacity = 80,
    spatial_srid_wgs84 = 4326,
    spatial_geojson_as_max_option = 7,
    spatial_geojson_from_min_option = 1,
    spatial_geojson_from_max_option = 4,
    spatial_geojson_bbox_option = 1,
    spatial_geojson_short_crs_option = 2,
    spatial_geojson_long_crs_option = 4,
    spatial_geojson_default_max_dec_digits = 15,
    mysql_error_incorrect_type_for_argument = 3064,
    mysql_error_invalid_geohash = 1411,
    mysql_error_invalid_json_text_in_function = 3141,
    mysql_error_invalid_geojson_missing_member = 3070,
    mysql_error_invalid_geojson_data = 3072,
    mysql_error_unsupported_geojson_dimensions = 3073,
    mysql_error_numeric_value_out_of_range = 1690,
    mysql_error_native_function_parameter_count = 1582,
    mysql_error_wrong_arguments = 1210,
    mysql_error_gis_different_srids = 3033,
    mysql_error_invalid_gis_data = 3037,
    mysql_error_unexpected_geometry_type = 3516,
    mysql_error_srs_not_found = 3548,
    mysql_error_geojson_longitude_out_of_range = 3616,
    mysql_error_geojson_latitude_out_of_range = 3617,
    mysql_error_not_implemented_for_geographic_srs = 3618,
    mysql_error_not_implemented_for_cartesian_srs = 3704,
    mysql_error_nonpositive_radius = 3706,
    mysql_error_srs_not_geographic = 3726,
    mysql_error_geometry_unknown_length_unit = 3882,
};

static const double spatial_shoelace_area_divisor = 2.0;
static const double spatial_distance_epsilon = 0.000000000001;
static const double spatial_midpoint_divisor = 2.0;
static const double spatial_centroid_denominator_multiplier = 3.0;
static const double spatial_distance_sphere_default_radius = 6370986.0;
static const double spatial_degrees_to_radians_divisor = 180.0;
static const double spatial_haversine_half_divisor = 2.0;
static const double spatial_geohash_longitude_min = -180.0;
static const double spatial_geohash_longitude_max = 180.0;
static const double spatial_geohash_latitude_min = -90.0;
static const double spatial_geohash_latitude_max = 90.0;
static const double spatial_geohash_interval_midpoint_divisor = 2.0;
static const double spatial_geohash_integer_snap_width = 0.0001;
static const double spatial_decimal_base = 10.0;
static const char spatial_geohash_alphabet[] = "0123456789bcdefghjkmnpqrstuvwxyz";

struct spatial_function_descriptor {
    const char *name;
    enum mylite_spatial_function_kind kind;
};

struct spatial_buffer {
    unsigned char *bytes;
    size_t size;
    size_t capacity;
};

struct spatial_wkb_cursor {
    const unsigned char *bytes;
    size_t size;
    size_t offset;
};

struct spatial_wkt_parser {
    const char *text;
    size_t size;
    size_t offset;
    const char *function_name;
    struct mylite_spatial_error *error;
};

struct spatial_geometry_view {
    const unsigned char *wkb;
    size_t wkb_size;
    enum mylite_spatial_geometry_type type;
    uint32_t srid;
};

struct spatial_point {
    double coordinate_x;
    double coordinate_y;
};

struct spatial_box {
    double min_x;
    double min_y;
    double max_x;
    double max_y;
    bool has_value;
};

struct spatial_segment {
    struct spatial_point start;
    struct spatial_point end;
};

struct spatial_distance_ring {
    struct spatial_point *points;
    uint32_t point_count;
};

struct spatial_distance_geometry {
    enum mylite_spatial_geometry_type type;
    struct spatial_point point;
    struct spatial_point *points;
    uint32_t point_count;
    struct spatial_distance_ring *rings;
    uint32_t ring_count;
    struct spatial_distance_geometry *children;
    uint32_t child_count;
};

struct spatial_point_collection {
    enum mylite_spatial_geometry_type type;
    struct spatial_point *points;
    uint32_t point_count;
};

struct spatial_discrete_point_set {
    struct spatial_point *points;
    uint32_t point_count;
};

struct spatial_simplify_range {
    uint32_t first;
    uint32_t last;
};

enum spatial_centroid_dimension {
    SPATIAL_CENTROID_DIMENSION_NONE = -1,
    SPATIAL_CENTROID_DIMENSION_POINT = 0,
    SPATIAL_CENTROID_DIMENSION_LINE = 1,
    SPATIAL_CENTROID_DIMENSION_POLYGON = 2,
};

struct spatial_centroid_accumulator {
    enum spatial_centroid_dimension dimension;
    double weighted_x;
    double weighted_y;
    double weight;
    struct spatial_point fallback_point;
    bool has_fallback_point;
};

enum spatial_point_ring_relation {
    SPATIAL_POINT_RING_OUTSIDE = 0,
    SPATIAL_POINT_RING_BOUNDARY = 1,
    SPATIAL_POINT_RING_INSIDE = 2,
};

struct geojson_parse_context {
    uint32_t srid;
    bool strip_extra_dimensions;
    const char *function_name;
    struct mylite_spatial_error *error;
};

struct spatial_type_name {
    const char *text;
    enum mylite_spatial_geometry_type type;
};

static const struct spatial_function_descriptor spatial_function_descriptors[] = {
    {"Point", MYLITE_SPATIAL_FUNCTION_POINT},
    {"LineString", MYLITE_SPATIAL_FUNCTION_LINESTRING},
    {"Polygon", MYLITE_SPATIAL_FUNCTION_POLYGON},
    {"MultiPoint", MYLITE_SPATIAL_FUNCTION_MULTIPOINT},
    {"MultiLineString", MYLITE_SPATIAL_FUNCTION_MULTILINESTRING},
    {"MultiPolygon", MYLITE_SPATIAL_FUNCTION_MULTIPOLYGON},
    {"GeometryCollection", MYLITE_SPATIAL_FUNCTION_GEOMETRYCOLLECTION},
    {"GeomCollection", MYLITE_SPATIAL_FUNCTION_GEOMCOLLECTION},
    {"ST_AsText", MYLITE_SPATIAL_FUNCTION_ST_ASTEXT},
    {"ST_AsWKT", MYLITE_SPATIAL_FUNCTION_ST_ASWKT},
    {"ST_AsBinary", MYLITE_SPATIAL_FUNCTION_ST_ASBINARY},
    {"ST_AsWKB", MYLITE_SPATIAL_FUNCTION_ST_ASWKB},
    {"ST_GeometryType", MYLITE_SPATIAL_FUNCTION_ST_GEOMETRYTYPE},
    {"ST_SRID", MYLITE_SPATIAL_FUNCTION_ST_SRID},
    {"ST_X", MYLITE_SPATIAL_FUNCTION_ST_X},
    {"ST_Y", MYLITE_SPATIAL_FUNCTION_ST_Y},
    {"ST_GeomFromText", MYLITE_SPATIAL_FUNCTION_ST_GEOMFROMTEXT},
    {"ST_GeometryFromText", MYLITE_SPATIAL_FUNCTION_ST_GEOMETRYFROMTEXT},
    {"ST_PointFromText", MYLITE_SPATIAL_FUNCTION_ST_POINTFROMTEXT},
    {"ST_LineFromText", MYLITE_SPATIAL_FUNCTION_ST_LINEFROMTEXT},
    {"ST_LineStringFromText", MYLITE_SPATIAL_FUNCTION_ST_LINESTRINGFROMTEXT},
    {"ST_PolyFromText", MYLITE_SPATIAL_FUNCTION_ST_POLYFROMTEXT},
    {"ST_PolygonFromText", MYLITE_SPATIAL_FUNCTION_ST_POLYGONFROMTEXT},
    {"ST_MPointFromText", MYLITE_SPATIAL_FUNCTION_ST_MPOINTFROMTEXT},
    {"ST_MultiPointFromText", MYLITE_SPATIAL_FUNCTION_ST_MULTIPOINTFROMTEXT},
    {"ST_MLineFromText", MYLITE_SPATIAL_FUNCTION_ST_MLINEFROMTEXT},
    {"ST_MultiLineStringFromText", MYLITE_SPATIAL_FUNCTION_ST_MULTILINESTRINGFROMTEXT},
    {"ST_MPolyFromText", MYLITE_SPATIAL_FUNCTION_ST_MPOLYFROMTEXT},
    {"ST_MultiPolygonFromText", MYLITE_SPATIAL_FUNCTION_ST_MULTIPOLYGONFROMTEXT},
    {"ST_GeomCollFromText", MYLITE_SPATIAL_FUNCTION_ST_GEOMCOLLFROMTEXT},
    {"ST_GeometryCollectionFromText", MYLITE_SPATIAL_FUNCTION_ST_GEOMETRYCOLLECTIONFROMTEXT},
    {"ST_GeomCollFromTxt", MYLITE_SPATIAL_FUNCTION_ST_GEOMCOLLFROMTXT},
    {"ST_GeomFromWKB", MYLITE_SPATIAL_FUNCTION_ST_GEOMFROMWKB},
    {"ST_GeometryFromWKB", MYLITE_SPATIAL_FUNCTION_ST_GEOMETRYFROMWKB},
    {"ST_PointFromWKB", MYLITE_SPATIAL_FUNCTION_ST_POINTFROMWKB},
    {"ST_LineFromWKB", MYLITE_SPATIAL_FUNCTION_ST_LINEFROMWKB},
    {"ST_LineStringFromWKB", MYLITE_SPATIAL_FUNCTION_ST_LINESTRINGFROMWKB},
    {"ST_PolyFromWKB", MYLITE_SPATIAL_FUNCTION_ST_POLYFROMWKB},
    {"ST_PolygonFromWKB", MYLITE_SPATIAL_FUNCTION_ST_POLYGONFROMWKB},
    {"ST_MPointFromWKB", MYLITE_SPATIAL_FUNCTION_ST_MPOINTFROMWKB},
    {"ST_MultiPointFromWKB", MYLITE_SPATIAL_FUNCTION_ST_MULTIPOINTFROMWKB},
    {"ST_MLineFromWKB", MYLITE_SPATIAL_FUNCTION_ST_MLINEFROMWKB},
    {"ST_MultiLineStringFromWKB", MYLITE_SPATIAL_FUNCTION_ST_MULTILINESTRINGFROMWKB},
    {"ST_MPolyFromWKB", MYLITE_SPATIAL_FUNCTION_ST_MPOLYFROMWKB},
    {"ST_MultiPolygonFromWKB", MYLITE_SPATIAL_FUNCTION_ST_MULTIPOLYGONFROMWKB},
    {"ST_GeomCollFromWKB", MYLITE_SPATIAL_FUNCTION_ST_GEOMCOLLFROMWKB},
    {"ST_GeometryCollectionFromWKB", MYLITE_SPATIAL_FUNCTION_ST_GEOMETRYCOLLECTIONFROMWKB},
    {"ST_Dimension", MYLITE_SPATIAL_FUNCTION_ST_DIMENSION},
    {"ST_IsEmpty", MYLITE_SPATIAL_FUNCTION_ST_ISEMPTY},
    {"ST_IsSimple", MYLITE_SPATIAL_FUNCTION_ST_ISSIMPLE},
    {"ST_IsValid", MYLITE_SPATIAL_FUNCTION_ST_ISVALID},
    {"ST_Validate", MYLITE_SPATIAL_FUNCTION_ST_VALIDATE},
    {"ST_Disjoint", MYLITE_SPATIAL_FUNCTION_ST_DISJOINT},
    {"ST_Intersects", MYLITE_SPATIAL_FUNCTION_ST_INTERSECTS},
    {"ST_Contains", MYLITE_SPATIAL_FUNCTION_ST_CONTAINS},
    {"ST_Within", MYLITE_SPATIAL_FUNCTION_ST_WITHIN},
    {"ST_Equals", MYLITE_SPATIAL_FUNCTION_ST_EQUALS},
    {"ST_Touches", MYLITE_SPATIAL_FUNCTION_ST_TOUCHES},
    {"ST_Overlaps", MYLITE_SPATIAL_FUNCTION_ST_OVERLAPS},
    {"ST_Crosses", MYLITE_SPATIAL_FUNCTION_ST_CROSSES},
    {"ST_Simplify", MYLITE_SPATIAL_FUNCTION_ST_SIMPLIFY},
    {"ST_IsClosed", MYLITE_SPATIAL_FUNCTION_ST_ISCLOSED},
    {"ST_NumGeometries", MYLITE_SPATIAL_FUNCTION_ST_NUMGEOMETRIES},
    {"ST_GeometryN", MYLITE_SPATIAL_FUNCTION_ST_GEOMETRYN},
    {"ST_NumPoints", MYLITE_SPATIAL_FUNCTION_ST_NUMPOINTS},
    {"ST_PointN", MYLITE_SPATIAL_FUNCTION_ST_POINTN},
    {"ST_StartPoint", MYLITE_SPATIAL_FUNCTION_ST_STARTPOINT},
    {"ST_EndPoint", MYLITE_SPATIAL_FUNCTION_ST_ENDPOINT},
    {"ST_ExteriorRing", MYLITE_SPATIAL_FUNCTION_ST_EXTERIORRING},
    {"ST_InteriorRingN", MYLITE_SPATIAL_FUNCTION_ST_INTERIORRINGN},
    {"ST_NumInteriorRing", MYLITE_SPATIAL_FUNCTION_ST_NUMINTERIORRING},
    {"ST_NumInteriorRings", MYLITE_SPATIAL_FUNCTION_ST_NUMINTERIORRINGS},
    {"ST_Length", MYLITE_SPATIAL_FUNCTION_ST_LENGTH},
    {"ST_Area", MYLITE_SPATIAL_FUNCTION_ST_AREA},
    {"ST_Envelope", MYLITE_SPATIAL_FUNCTION_ST_ENVELOPE},
    {"ST_SwapXY", MYLITE_SPATIAL_FUNCTION_ST_SWAPXY},
    {"ST_MakeEnvelope", MYLITE_SPATIAL_FUNCTION_ST_MAKEENVELOPE},
    {"MBRContains", MYLITE_SPATIAL_FUNCTION_MBRCONTAINS},
    {"MBRCoveredBy", MYLITE_SPATIAL_FUNCTION_MBRCOVEREDBY},
    {"MBRCovers", MYLITE_SPATIAL_FUNCTION_MBRCOVERS},
    {"MBRDisjoint", MYLITE_SPATIAL_FUNCTION_MBRDISJOINT},
    {"MBREquals", MYLITE_SPATIAL_FUNCTION_MBREQUALS},
    {"MBRIntersects", MYLITE_SPATIAL_FUNCTION_MBRINTERSECTS},
    {"MBROverlaps", MYLITE_SPATIAL_FUNCTION_MBROVERLAPS},
    {"MBRTouches", MYLITE_SPATIAL_FUNCTION_MBRTOUCHES},
    {"MBRWithin", MYLITE_SPATIAL_FUNCTION_MBRWITHIN},
    {"ST_GeoHash", MYLITE_SPATIAL_FUNCTION_ST_GEOHASH},
    {"ST_LatFromGeoHash", MYLITE_SPATIAL_FUNCTION_ST_LATFROMGEOHASH},
    {"ST_LongFromGeoHash", MYLITE_SPATIAL_FUNCTION_ST_LONGFROMGEOHASH},
    {"ST_PointFromGeoHash", MYLITE_SPATIAL_FUNCTION_ST_POINTFROMGEOHASH},
    {"ST_AsGeoJSON", MYLITE_SPATIAL_FUNCTION_ST_ASGEOJSON},
    {"ST_GeomFromGeoJSON", MYLITE_SPATIAL_FUNCTION_ST_GEOMFROMGEOJSON},
    {"ST_Latitude", MYLITE_SPATIAL_FUNCTION_ST_LATITUDE},
    {"ST_Longitude", MYLITE_SPATIAL_FUNCTION_ST_LONGITUDE},
    {"ST_Distance", MYLITE_SPATIAL_FUNCTION_ST_DISTANCE},
    {"ST_LineInterpolatePoint", MYLITE_SPATIAL_FUNCTION_ST_LINEINTERPOLATEPOINT},
    {"ST_LineInterpolatePoints", MYLITE_SPATIAL_FUNCTION_ST_LINEINTERPOLATEPOINTS},
    {"ST_PointAtDistance", MYLITE_SPATIAL_FUNCTION_ST_POINTATDISTANCE},
    {"ST_Distance_Sphere", MYLITE_SPATIAL_FUNCTION_ST_DISTANCESPHERE},
    {"ST_Centroid", MYLITE_SPATIAL_FUNCTION_ST_CENTROID},
    {"ST_FrechetDistance", MYLITE_SPATIAL_FUNCTION_ST_FRECHETDISTANCE},
    {"ST_HausdorffDistance", MYLITE_SPATIAL_FUNCTION_ST_HAUSDORFFDISTANCE},
    {"ST_ConvexHull", MYLITE_SPATIAL_FUNCTION_ST_CONVEXHULL},
};

static int evaluate_point_constructor(
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
);
static int evaluate_sequence_constructor(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
);
static int evaluate_from_text(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
);
static int evaluate_from_wkb(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
);
static int evaluate_as_text(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
);
static int evaluate_as_wkb(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
);
static int evaluate_geometry_type(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
);
static int evaluate_srid(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
);
static int evaluate_point_coordinate(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
);
static int evaluate_point_geographic_coordinate(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
);
static int evaluate_dimension(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
);
static int evaluate_is_empty(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
);
static int evaluate_is_simple(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
);
static int evaluate_is_valid(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
);
static int evaluate_validate(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
);
static int evaluate_is_closed(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
);
static int evaluate_num_geometries(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
);
static int evaluate_geometry_n(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
);
static int evaluate_num_points(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
);
static int evaluate_line_point_accessor(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
);
static int evaluate_polygon_ring_accessor(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
);
static int evaluate_length(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
);
static int evaluate_area(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
);
static int evaluate_centroid(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
);
static int evaluate_convex_hull(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
);
static int evaluate_simplify(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
);
static int evaluate_distance(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
);
static int evaluate_relation_predicate(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
);
static int evaluate_distance_sphere(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
);
static int evaluate_discrete_distance(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
);
static int evaluate_line_interpolation(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
);
static int evaluate_envelope(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
);
static int evaluate_swap_xy(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
);
static int evaluate_make_envelope(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
);
static int evaluate_mbr_predicate(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
);
static int evaluate_geohash(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
);
static int evaluate_coordinate_from_geohash(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
);
static int evaluate_point_from_geohash(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
);
static int evaluate_as_geojson(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
);
static int evaluate_from_geojson(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
);
static bool function_name_matches(const char *name, size_t name_size, const char *expected);
static bool function_kind_is_from_text(enum mylite_spatial_function_kind kind);
static bool function_kind_is_from_wkb(enum mylite_spatial_function_kind kind);
static enum mylite_spatial_geometry_type expected_text_wkb_type(
    enum mylite_spatial_function_kind kind
);
static enum mylite_spatial_geometry_type constructor_result_type(
    enum mylite_spatial_function_kind kind
);
static int set_spatial_error(
    struct mylite_spatial_error *error,
    int code,
    const char *sqlstate, // NOLINT(bugprone-easily-swappable-parameters): diagnostic shape.
    const char *format,
    ...
);
static int set_nomem_error(struct mylite_spatial_error *error);
static const char *spatial_diagnostic_function_name(
    const char *function_name,
    char *buffer,
    size_t buffer_size
);
static int set_invalid_gis_data_error(
    struct mylite_spatial_error *error,
    const char *function_name
);
static int set_unexpected_geometry_type_error(
    struct mylite_spatial_error *error,
    const char *subject,
    enum mylite_spatial_geometry_type actual_type,
    const char *function_name
);
static int set_wrong_arguments_error(struct mylite_spatial_error *error, const char *function_name);
static int set_incorrect_argument_type_error(
    struct mylite_spatial_error *error,
    const char *argument_name,
    const char *function_name
);
static int set_srs_not_geographic_error(
    struct mylite_spatial_error *error,
    uint32_t srid,
    const char *function_name
);
static int set_gis_different_srids_error(
    struct mylite_spatial_error *error,
    uint32_t left_srid,
    uint32_t right_srid,
    const char *function_name
);
static int set_not_implemented_for_geographic_srs_error(
    struct mylite_spatial_error *error,
    const char *function_name
);
static int set_not_implemented_for_geographic_srs_geometry_error(
    struct mylite_spatial_error *error,
    enum mylite_spatial_geometry_type type,
    const char *function_name
);
static int set_not_implemented_for_geographic_srs_geometry_argument_error(
    struct mylite_spatial_error *error,
    enum mylite_spatial_geometry_type type,
    const char *function_name
);
static int set_not_implemented_for_cartesian_srs_error(
    struct mylite_spatial_error *error,
    enum mylite_spatial_geometry_type left_type,
    enum mylite_spatial_geometry_type right_type,
    const char *function_name
);
static int set_unknown_length_unit_error(
    struct mylite_spatial_error *error,
    const struct mylite_spatial_argument *unit_argument,
    const char *function_name
);
static int set_distance_sphere_longitude_error(
    struct mylite_spatial_error *error,
    double longitude,
    const char *function_name
);
static int set_distance_sphere_latitude_error(
    struct mylite_spatial_error *error,
    double latitude,
    const char *function_name
);
static int set_nonpositive_radius_error(
    struct mylite_spatial_error *error,
    const char *function_name
);
static int set_distance_range_error(struct mylite_spatial_error *error, const char *function_name);
static int set_geohash_range_error(
    struct mylite_spatial_error *error,
    const char *subject,
    const char *function_name
);
static int set_invalid_geohash_error(
    struct mylite_spatial_error *error,
    const struct mylite_spatial_argument *argument,
    const char *function_name
);
static int set_invalid_json_text_error(
    struct mylite_spatial_error *error,
    const struct mylite_json_normalize_result *result,
    const char *function_name
);
static int set_invalid_geojson_missing_member_error(
    struct mylite_spatial_error *error,
    const char *member,
    const char *function_name
);
static int set_invalid_geojson_data_error(
    struct mylite_spatial_error *error,
    const char *function_name
);
static int set_unsupported_geojson_dimensions_error(
    struct mylite_spatial_error *error,
    size_t dimension_count,
    const char *function_name
);
static int set_geojson_option_error(
    struct mylite_spatial_error *error,
    const char *value,
    const char *function_name
);
static int set_geojson_max_dec_digits_error(
    struct mylite_spatial_error *error,
    const char *value,
    const char *function_name
);
static int set_geojson_longitude_error(
    struct mylite_spatial_error *error,
    double longitude,
    const char *function_name
);
static int set_geojson_latitude_error(
    struct mylite_spatial_error *error,
    double latitude,
    const char *function_name
);
static int set_parameter_count_error(
    struct mylite_spatial_error *error,
    enum mylite_spatial_function_kind kind
);
static int validate_argument_count(
    enum mylite_spatial_function_kind kind, // NOLINT(bugprone-easily-swappable-parameters)
    size_t argument_count,
    size_t min_argument_count,
    size_t max_argument_count,
    struct mylite_spatial_error *error
);
static bool any_argument_is_null(
    const struct mylite_spatial_argument *arguments,
    size_t argument_count
);
static int argument_numeric(
    const struct mylite_spatial_argument *argument,
    double *out_value,
    struct mylite_spatial_error *error,
    const char *function_name
);
static int argument_distance(
    const struct mylite_spatial_argument *argument,
    double *out_value,
    struct mylite_spatial_error *error,
    const char *function_name
);
static int argument_geohash_coordinate(
    const struct mylite_spatial_argument *argument,
    double *out_value,
    struct mylite_spatial_error *error,
    const char *function_name,
    const char *range_subject
);
static int argument_srid(
    const struct mylite_spatial_argument *argument,
    uint32_t *out_srid,
    bool *out_is_null,
    struct mylite_spatial_error *error,
    const char *function_name
);
static int argument_geohash_uint32(
    const struct mylite_spatial_argument *argument,
    uint32_t *out_value,
    bool *out_is_null,
    struct mylite_spatial_error *error,
    const char *function_name,
    const char *argument_name, // NOLINT(bugprone-easily-swappable-parameters): diagnostic labels.
    const char *range_subject
);
static int argument_geojson_uint32(
    const struct mylite_spatial_argument *argument,
    uint32_t *out_value,
    bool *out_is_null,
    struct mylite_spatial_error *error,
    const char *function_name,
    const char *argument_name
);
static int argument_geojson_srid(
    const struct mylite_spatial_argument *argument,
    uint32_t *out_srid,
    bool *out_is_null,
    struct mylite_spatial_error *error,
    const char *function_name
);
static int argument_index(
    const struct mylite_spatial_argument *argument,
    uint32_t *out_index,
    bool *out_is_null,
    struct mylite_spatial_error *error,
    const char *function_name
);
static int assign_null_result(struct mylite_spatial_result *out_result);
static int assign_integer_result(struct mylite_spatial_result *out_result, int64_t value);
static int assign_double_result(struct mylite_spatial_result *out_result, double value);
static int assign_owned_bytes_result(
    struct mylite_spatial_result *out_result,
    enum mylite_spatial_result_kind kind,
    unsigned char *bytes, // NOLINT(readability-non-const-parameter): transfers ownership.
    size_t byte_count
);
static int assign_copied_bytes_result(
    struct mylite_spatial_result *out_result,
    enum mylite_spatial_result_kind kind,
    const void *bytes,
    size_t byte_count,
    struct mylite_spatial_error *error
);
static int assign_copied_text_result(
    struct mylite_spatial_result *out_result,
    const char *text,
    size_t text_size,
    struct mylite_spatial_error *error
);
static int make_internal_geometry_from_wkb(
    const unsigned char *wkb,
    size_t wkb_size, // NOLINT(bugprone-easily-swappable-parameters): WKB payload then SRID.
    uint32_t srid,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    struct mylite_spatial_error *error
);
static int make_point_internal_geometry(
    double coordinate_x,
    double coordinate_y,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    struct mylite_spatial_error *error
);
static int make_point_internal_geometry_with_srid(
    uint32_t srid, // NOLINT(bugprone-easily-swappable-parameters): internal point builder.
    double coordinate_x,
    double coordinate_y,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    struct mylite_spatial_error *error
);
static int make_linestring_internal_geometry(
    const struct spatial_point *points,
    uint32_t point_count,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    struct mylite_spatial_error *error
);
static int make_polygon_internal_geometry_with_srid(
    uint32_t srid, // NOLINT(bugprone-easily-swappable-parameters): internal polygon builder.
    const struct spatial_point *points,
    uint32_t point_count,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    struct mylite_spatial_error *error
);
static int make_multipoint_internal_geometry_with_srid(
    uint32_t srid, // NOLINT(bugprone-easily-swappable-parameters): internal collection builder.
    const struct spatial_point *points,
    uint32_t point_count,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    struct mylite_spatial_error *error
);
static int make_envelope_internal_geometry(
    const struct spatial_box *box,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    struct mylite_spatial_error *error
);
static int append_polygon_rectangle_wkb(
    struct spatial_buffer *buffer,
    const struct spatial_box *box
);
static int append_internal_prefix(struct spatial_buffer *buffer, uint32_t srid);
static int spatial_buffer_append(struct spatial_buffer *buffer, const void *bytes, size_t size);
static int spatial_buffer_append_byte(struct spatial_buffer *buffer, unsigned char byte);
static int spatial_buffer_append_u32_le(struct spatial_buffer *buffer, uint32_t value);
static int spatial_buffer_append_double_le(struct spatial_buffer *buffer, double value);
static int spatial_buffer_reserve(struct spatial_buffer *buffer, size_t required);
static void spatial_buffer_deinit(struct spatial_buffer *buffer);
static uint32_t read_u32_endian(const unsigned char *bytes, bool little_endian);
static double read_double_endian(const unsigned char *bytes, bool little_endian);
static int validate_internal_geometry(
    const void *bytes,
    size_t byte_count,
    enum mylite_spatial_geometry_type *out_type,
    uint32_t *out_srid,
    struct mylite_spatial_error *error,
    const char *function_name
);
static int validate_wkb(
    const unsigned char *wkb,
    size_t wkb_size,
    enum mylite_spatial_geometry_type *out_type,
    struct mylite_spatial_error *error,
    const char *function_name
);
static int validate_wkb_at(
    struct spatial_wkb_cursor *cursor,
    enum mylite_spatial_geometry_type *out_type,
    struct mylite_spatial_error *error,
    const char *function_name
);
static enum mylite_spatial_geometry_type collection_expected_nested_type(
    enum mylite_spatial_geometry_type type
);
static int skip_wkb_points(
    struct spatial_wkb_cursor *cursor,
    uint32_t point_count,
    struct mylite_spatial_error *error,
    const char *function_name
);
static int cursor_read_header(
    struct spatial_wkb_cursor *cursor,
    bool *out_little_endian,
    enum mylite_spatial_geometry_type *out_type,
    struct mylite_spatial_error *error,
    const char *function_name
);
static int cursor_read_u32(
    struct spatial_wkb_cursor *cursor,
    bool little_endian,
    uint32_t *out_value,
    struct mylite_spatial_error *error,
    const char *function_name
);
static int cursor_read_double(
    struct spatial_wkb_cursor *cursor,
    bool little_endian,
    double *out_value,
    struct mylite_spatial_error *error,
    const char *function_name
);
static int cursor_skip(
    struct spatial_wkb_cursor *cursor,
    size_t size,
    struct mylite_spatial_error *error,
    const char *function_name
);
static const char *geometry_type_name(enum mylite_spatial_geometry_type type);
static const char *cartesian_srs_not_implemented_geometry_type_name(
    enum mylite_spatial_geometry_type type
);
static int geometry_point_coordinates(
    const unsigned char *wkb,
    size_t wkb_size,
    double *out_x,
    double *out_y,
    struct mylite_spatial_error *error,
    const char *function_name
);
static int read_single_geometry_argument(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct spatial_geometry_view *out_geometry,
    bool *out_is_null,
    struct mylite_spatial_error *error
);
static int read_two_geometry_arguments(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct spatial_geometry_view *out_left,
    struct spatial_geometry_view *out_right,
    bool *out_is_null,
    struct mylite_spatial_error *error
);
static bool geometry_type_is_empty_collection(
    const struct spatial_geometry_view *geometry,
    struct mylite_spatial_error *error,
    const char *function_name
);
static int wkb_dimension_at(
    struct spatial_wkb_cursor *cursor,
    int *out_dimension,
    struct mylite_spatial_error *error,
    const char *function_name
);
static int wkb_bounds_at(
    struct spatial_wkb_cursor *cursor,
    struct spatial_box *box,
    struct mylite_spatial_error *error,
    const char *function_name
);
static int wkb_length_at(
    struct spatial_wkb_cursor *cursor,
    double *out_length,
    bool *out_supported,
    struct mylite_spatial_error *error,
    const char *function_name
);
static int wkb_area_at(
    struct spatial_wkb_cursor *cursor,
    double *out_area,
    bool *out_supported,
    struct mylite_spatial_error *error,
    const char *function_name
);
static int distance_geometry_from_view(
    const struct spatial_geometry_view *view,
    struct spatial_distance_geometry *out_geometry,
    struct mylite_spatial_error *error,
    const char *function_name
);
static int point_collection_from_geometry(
    const struct spatial_geometry_view *geometry,
    struct spatial_point_collection *out_collection,
    struct mylite_spatial_error *error,
    const char *function_name
);
static int point_collection_read_multipoint(
    struct spatial_wkb_cursor *cursor,
    bool little_endian,
    struct spatial_point_collection *out_collection,
    struct mylite_spatial_error *error,
    const char *function_name
);
static void point_collection_deinit(struct spatial_point_collection *collection);
static int validate_distance_sphere_coordinates(
    const struct spatial_point_collection *collection,
    struct mylite_spatial_error *error,
    const char *function_name
);
static double point_distance_sphere(
    const struct spatial_point *left,
    const struct spatial_point *right,
    double radius
);
static int distance_sphere_between_collections(
    const struct spatial_point_collection *left,
    const struct spatial_point_collection *right,
    double radius,
    double *out_distance,
    bool *out_has_distance
);
static bool frechet_distance_supports_types(
    enum mylite_spatial_geometry_type left_type, // NOLINT(bugprone-easily-swappable-parameters)
    enum mylite_spatial_geometry_type right_type
);
static bool hausdorff_distance_supports_types(
    enum mylite_spatial_geometry_type left_type, // NOLINT(bugprone-easily-swappable-parameters)
    enum mylite_spatial_geometry_type right_type
);
static int discrete_point_set_from_geometry(
    const struct spatial_distance_geometry *geometry,
    struct spatial_discrete_point_set *out_set,
    struct mylite_spatial_error *error,
    const char *function_name
);
static size_t discrete_point_set_count_geometry(const struct spatial_distance_geometry *geometry);
static int discrete_point_set_append_geometry(
    const struct spatial_distance_geometry *geometry,
    struct spatial_discrete_point_set *set,
    uint32_t *io_offset,
    struct mylite_spatial_error *error,
    const char *function_name
);
static void discrete_point_set_deinit(struct spatial_discrete_point_set *set);
static int frechet_distance_between_lines(
    const struct spatial_distance_geometry *left,
    const struct spatial_distance_geometry *right,
    double *out_distance,
    struct mylite_spatial_error *error
);
static int hausdorff_distance_between_supported_geometries(
    const struct spatial_distance_geometry *left,
    const struct spatial_distance_geometry *right,
    double *out_distance,
    struct mylite_spatial_error *error,
    const char *function_name
);
static int hausdorff_distance_line_to_multiline(
    const struct spatial_distance_geometry *line,
    const struct spatial_distance_geometry *multiline,
    double *out_distance,
    struct mylite_spatial_error *error,
    const char *function_name
);
static int hausdorff_distance_multiline_to_multiline(
    const struct spatial_distance_geometry *left,
    const struct spatial_distance_geometry *right,
    double *out_distance,
    struct mylite_spatial_error *error,
    const char *function_name
);
static int hausdorff_distance_point_to_discrete_geometry(
    const struct spatial_point *point,
    const struct spatial_distance_geometry *geometry,
    double *out_distance,
    struct mylite_spatial_error *error,
    const char *function_name
);
static int hausdorff_distance_between_line_points(
    const struct spatial_distance_geometry *left,
    const struct spatial_distance_geometry *right,
    double *out_distance,
    struct mylite_spatial_error *error,
    const char *function_name
);
static int hausdorff_distance_between_point_sets(
    const struct spatial_discrete_point_set *left,
    const struct spatial_discrete_point_set *right,
    double *out_distance
);
static int centroid_accumulate_geometry(
    const struct spatial_distance_geometry *geometry,
    struct spatial_centroid_accumulator *accumulator,
    struct mylite_spatial_error *error,
    const char *function_name
);
static void centroid_accumulate_point(
    struct spatial_centroid_accumulator *accumulator,
    const struct spatial_point *point
);
static void centroid_accumulate_line(
    const struct spatial_distance_geometry *line,
    struct spatial_centroid_accumulator *accumulator
);
static int centroid_accumulate_polygon(
    const struct spatial_distance_geometry *polygon,
    struct spatial_centroid_accumulator *accumulator,
    struct mylite_spatial_error *error,
    const char *function_name
);
static int centroid_polygon_value(
    const struct spatial_distance_geometry *polygon,
    struct spatial_point *out_centroid,
    double *out_area,
    struct mylite_spatial_error *error,
    const char *function_name
);
static int centroid_ring_value(
    const struct spatial_distance_ring *ring,
    struct spatial_point *out_centroid,
    double *out_area,
    struct mylite_spatial_error *error,
    const char *function_name
);
static void centroid_accumulator_add_weighted(
    struct spatial_centroid_accumulator *accumulator,
    enum spatial_centroid_dimension dimension,
    const struct spatial_point *point,
    double weight
);
static void centroid_accumulator_add_fallback(
    struct spatial_centroid_accumulator *accumulator,
    enum spatial_centroid_dimension dimension,
    const struct spatial_point *point
);
static bool centroid_accumulator_accepts(
    struct spatial_centroid_accumulator *accumulator,
    enum spatial_centroid_dimension dimension
);
static int convex_hull_point_set_from_geometry(
    const struct spatial_distance_geometry *geometry,
    struct spatial_discrete_point_set *out_set,
    struct mylite_spatial_error *error,
    const char *function_name
);
static size_t convex_hull_point_set_count_geometry(const struct spatial_distance_geometry *geometry
);
static int convex_hull_point_set_append_geometry(
    const struct spatial_distance_geometry *geometry,
    struct spatial_discrete_point_set *set,
    uint32_t *io_offset,
    struct mylite_spatial_error *error,
    const char *function_name
);
static int convex_hull_validate_geometry_rings(
    const struct spatial_distance_geometry *geometry,
    struct mylite_spatial_error *error,
    const char *function_name
);
static bool spatial_ring_has_noncollinear_points(const struct spatial_distance_ring *ring);
static int convex_hull_build(
    struct spatial_discrete_point_set *points,
    struct spatial_discrete_point_set *out_hull,
    struct mylite_spatial_error *error,
    const char *function_name
);
static int spatial_point_compare_for_hull(
    const void *left, // NOLINT(bugprone-easily-swappable-parameters): qsort comparator ABI.
    const void *right
);
static uint32_t convex_hull_unique_points(struct spatial_point *points, uint32_t point_count);
static bool spatial_points_are_equal(
    const struct spatial_point *left,
    const struct spatial_point *right
);
static bool convex_hull_turn_is_clockwise_or_collinear(
    const struct spatial_point *origin,
    const struct spatial_point *middle,
    const struct spatial_point *candidate
);
static int simplify_geometry(
    const struct spatial_distance_geometry *source,
    double max_distance,
    struct spatial_distance_geometry *out_geometry,
    bool *out_has_geometry,
    struct mylite_spatial_error *error,
    const char *function_name
);
static int simplify_point_geometry(
    const struct spatial_distance_geometry *source,
    struct spatial_distance_geometry *out_geometry,
    bool *out_has_geometry,
    struct mylite_spatial_error *error,
    const char *function_name
);
static int simplify_line_geometry(
    const struct spatial_distance_geometry *source,
    double max_distance,
    struct spatial_distance_geometry *out_geometry,
    bool *out_has_geometry,
    struct mylite_spatial_error *error,
    const char *function_name
);
static int simplify_polygon_geometry(
    const struct spatial_distance_geometry *source,
    double max_distance,
    struct spatial_distance_geometry *out_geometry,
    bool *out_has_geometry,
    struct mylite_spatial_error *error,
    const char *function_name
);
static int simplify_collection_geometry(
    const struct spatial_distance_geometry *source,
    double max_distance,
    struct spatial_distance_geometry *out_geometry,
    bool *out_has_geometry,
    struct mylite_spatial_error *error,
    const char *function_name
);
static int simplify_points(
    const struct spatial_point *points,
    uint32_t point_count,
    double max_distance,
    struct spatial_point **out_points,
    uint32_t *out_point_count,
    struct mylite_spatial_error *error
);
static int simplify_ring_points(
    const struct spatial_point *points,
    uint32_t point_count,
    double max_distance,
    struct spatial_point **out_points,
    uint32_t *out_point_count,
    bool *out_has_ring,
    struct mylite_spatial_error *error
);
static int simplify_point_copy(
    const struct spatial_point *points,
    uint32_t point_count,
    struct spatial_point **out_points,
    uint32_t *out_point_count,
    struct mylite_spatial_error *error
);
static int simplify_keep_mask(
    const struct spatial_point *points,
    uint32_t point_count, // NOLINT(bugprone-easily-swappable-parameters)
    double max_distance,
    bool *keep,
    struct mylite_spatial_error *error
);
static bool simplify_range_push(
    struct spatial_simplify_range *ranges,
    uint32_t capacity,
    uint32_t *io_count,
    struct spatial_simplify_range range
);
static uint32_t simplify_kept_point_count(const bool *keep, uint32_t point_count);
static void simplify_ring_canonicalize(struct spatial_point *points, uint32_t point_count);
static bool simplify_ring_is_valid(const struct spatial_point *points, uint32_t point_count);
static int make_internal_geometry_from_distance_geometry(
    uint32_t srid,
    const struct spatial_distance_geometry *geometry,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    struct mylite_spatial_error *error
);
static int append_distance_geometry_wkb(
    struct spatial_buffer *buffer,
    const struct spatial_distance_geometry *geometry
);
static int append_distance_geometry_points(
    struct spatial_buffer *buffer,
    const struct spatial_point *points,
    uint32_t point_count
);
static int argument_simplify_distance(
    const struct mylite_spatial_argument *argument,
    double *out_value,
    struct mylite_spatial_error *error,
    const char *function_name
);
static int distance_geometry_read(
    struct spatial_wkb_cursor *cursor,
    struct spatial_distance_geometry *out_geometry,
    struct mylite_spatial_error *error,
    const char *function_name
);
static int distance_geometry_read_points(
    struct spatial_wkb_cursor *cursor,
    bool little_endian,
    uint32_t point_count,
    struct spatial_point **out_points,
    struct mylite_spatial_error *error,
    const char *function_name
);
static int distance_geometry_read_children(
    struct spatial_wkb_cursor *cursor,
    bool little_endian,
    enum mylite_spatial_geometry_type type,
    struct spatial_distance_geometry *out_geometry,
    struct mylite_spatial_error *error,
    const char *function_name
);
static void distance_geometry_deinit(struct spatial_distance_geometry *geometry);
static int distance_between_geometries(
    const struct spatial_distance_geometry *left,
    const struct spatial_distance_geometry *right,
    double *out_distance,
    bool *out_has_distance
);
static int distance_between_simple_geometries(
    const struct spatial_distance_geometry *left,
    const struct spatial_distance_geometry *right,
    double *out_distance
);
static double distance_point_to_geometry(
    const struct spatial_point *point,
    const struct spatial_distance_geometry *geometry
);
static double distance_line_to_geometry(
    const struct spatial_distance_geometry *line,
    const struct spatial_distance_geometry *geometry
);
static double distance_polygon_to_geometry(
    const struct spatial_distance_geometry *polygon,
    const struct spatial_distance_geometry *geometry
);
static double distance_point_to_line(
    const struct spatial_point *point,
    const struct spatial_distance_geometry *line
);
static double distance_point_to_polygon(
    const struct spatial_point *point,
    const struct spatial_distance_geometry *polygon
);
static double distance_line_to_line(
    const struct spatial_distance_geometry *left,
    const struct spatial_distance_geometry *right
);
static double distance_line_to_polygon(
    const struct spatial_distance_geometry *line,
    const struct spatial_distance_geometry *polygon
);
static double distance_polygon_to_polygon(
    const struct spatial_distance_geometry *left,
    const struct spatial_distance_geometry *right
);
static bool relation_geometry_contains(
    const struct spatial_distance_geometry *container,
    const struct spatial_distance_geometry *content
);
static int relation_geometry_dimension(const struct spatial_distance_geometry *geometry);
static bool relation_geometry_interiors_intersect(
    const struct spatial_distance_geometry *left,
    const struct spatial_distance_geometry *right
);
static bool relation_geometry_crosses(
    const struct spatial_distance_geometry *left,
    const struct spatial_distance_geometry *right,
    int left_dimension,
    int right_dimension
);
static bool relation_lines_cross(
    const struct spatial_distance_geometry *left,
    const struct spatial_distance_geometry *right
);
static bool relation_line_segments_cross_at_point(
    const struct spatial_distance_geometry *left,
    const struct spatial_distance_geometry *right
);
static bool relation_geometry_overlaps(
    const struct spatial_distance_geometry *left,
    const struct spatial_distance_geometry *right,
    int dimension
);
static bool relation_point_sets_overlap(
    const struct spatial_distance_geometry *left,
    const struct spatial_distance_geometry *right
);
static size_t relation_point_set_common_count(
    const struct spatial_distance_geometry *left,
    const struct spatial_distance_geometry *right
);
static bool relation_geometry_has_point(
    const struct spatial_distance_geometry *geometry,
    const struct spatial_point *point
);
static bool relation_lines_overlap(
    const struct spatial_distance_geometry *left,
    const struct spatial_distance_geometry *right
);
static bool relation_line_segments_overlap_with_length(
    const struct spatial_distance_geometry *left,
    const struct spatial_distance_geometry *right
);
static bool relation_polygons_overlap(
    const struct spatial_distance_geometry *left,
    const struct spatial_distance_geometry *right
);
static bool relation_simple_geometry_interiors_intersect(
    const struct spatial_distance_geometry *left,
    const struct spatial_distance_geometry *right
);
static bool relation_lines_have_interior_intersection(
    const struct spatial_distance_geometry *left,
    const struct spatial_distance_geometry *right
);
static bool relation_line_segments_have_interior_intersection(
    const struct spatial_distance_geometry *left_line,
    const struct spatial_segment *left_segment,
    const struct spatial_distance_geometry *right_line,
    const struct spatial_segment *right_segment
);
static bool relation_collinear_segments_overlap_with_length(
    const struct spatial_segment *left,
    const struct spatial_segment *right
);
static bool relation_segment_intersection_endpoint_has_line_interiors(
    const struct spatial_distance_geometry *left_line,
    const struct spatial_segment *left_segment,
    const struct spatial_distance_geometry *right_line,
    const struct spatial_segment *right_segment
);
static bool relation_line_polygon_interiors_intersect(
    const struct spatial_distance_geometry *line,
    const struct spatial_distance_geometry *polygon
);
static bool relation_polygons_have_interior_intersection(
    const struct spatial_distance_geometry *left,
    const struct spatial_distance_geometry *right
);
static bool relation_polygon_boundaries_cross(
    const struct spatial_distance_geometry *left,
    const struct spatial_distance_geometry *right
);
static bool relation_simple_geometry_contains(
    const struct spatial_distance_geometry *container,
    const struct spatial_distance_geometry *content
);
static bool relation_line_contains_point(
    const struct spatial_distance_geometry *line,
    const struct spatial_point *point
);
static bool relation_point_is_on_line_interior(
    const struct spatial_point *point,
    const struct spatial_distance_geometry *line
);
static bool relation_line_contains_line(
    const struct spatial_distance_geometry *container,
    const struct spatial_distance_geometry *content
);
static bool relation_polygon_contains_line(
    const struct spatial_distance_geometry *polygon,
    const struct spatial_distance_geometry *line
);
static bool relation_polygon_contains_polygon(
    const struct spatial_distance_geometry *container,
    const struct spatial_distance_geometry *content
);
static bool relation_polygon_surface_contains_container(
    const struct spatial_distance_geometry *content, // NOLINT(bugprone-easily-swappable-parameters)
    const struct spatial_distance_geometry *container
);
static bool relation_polygon_surface_contains_polygon(
    const struct spatial_distance_geometry *container,
    const struct spatial_distance_geometry *content,
    bool *out_has_interior_sample
);
static bool relation_polygon_contains_ring_samples(
    const struct spatial_distance_geometry *container,
    const struct spatial_distance_ring *ring,
    bool *io_has_interior_sample
);
static bool relation_polygon_contains_point_sample(
    const struct spatial_distance_geometry *container,
    const struct spatial_point *point,
    bool *io_has_interior_sample
);
static bool relation_segment_crosses_polygon_boundary(
    const struct spatial_distance_geometry *polygon,
    const struct spatial_segment *segment
);
static bool relation_segments_overlap_collinearly(
    const struct spatial_segment *left, // NOLINT(bugprone-easily-swappable-parameters)
    const struct spatial_segment *right
);
static struct spatial_point segment_midpoint(const struct spatial_segment *segment);
static bool distance_geometry_is_collection(const struct spatial_distance_geometry *geometry);
static bool distance_geometry_is_empty(const struct spatial_distance_geometry *geometry);
static bool polygon_contains_point_surface(
    const struct spatial_distance_geometry *polygon,
    const struct spatial_point *point
);
static enum spatial_point_ring_relation point_ring_relation(
    const struct spatial_distance_ring *ring,
    const struct spatial_point *point
);
static bool line_intersects_polygon(
    const struct spatial_distance_geometry *line, // NOLINT(bugprone-easily-swappable-parameters)
    const struct spatial_distance_geometry *polygon
);
static bool polygon_rings_intersect(
    const struct spatial_distance_geometry *left,
    const struct spatial_distance_geometry *right
);
static bool simplicity_geometry_is_simple(const struct spatial_distance_geometry *geometry);
static bool simplicity_line_is_simple(const struct spatial_point *points, uint32_t point_count);
static bool simplicity_multipoint_is_simple(const struct spatial_distance_geometry *geometry);
static bool simplicity_multiline_is_simple(const struct spatial_distance_geometry *geometry);
static bool simplicity_collection_is_simple(const struct spatial_distance_geometry *geometry);
static bool simplicity_child_pair_intersects_invalidly(
    const struct spatial_distance_geometry *left,
    const struct spatial_distance_geometry *right
);
static bool simplicity_geometries_intersect_invalidly(
    const struct spatial_distance_geometry *left,
    const struct spatial_distance_geometry *right
);
static bool simplicity_point_line_intersection_invalid(
    const struct spatial_point *point,
    const struct spatial_distance_geometry *line
);
static bool simplicity_point_polygon_intersection_invalid(
    const struct spatial_point *point,
    const struct spatial_distance_geometry *polygon
);
static bool simplicity_line_pair_intersects_invalidly(
    const struct spatial_distance_geometry *left,
    const struct spatial_distance_geometry *right
);
static bool simplicity_line_polygon_intersection_invalid(
    const struct spatial_distance_geometry *line,
    const struct spatial_distance_geometry *polygon
);
static bool simplicity_polygon_polygon_intersection_invalid(
    const struct spatial_distance_geometry *left,
    const struct spatial_distance_geometry *right
);
static bool simplicity_segment_midpoint_is_polygon_interior(
    const struct spatial_segment *segment,
    const struct spatial_distance_geometry *polygon
);
static bool simplicity_line_segments_share_boundary_point(
    const struct spatial_distance_geometry *left_line,
    const struct spatial_segment *left_segment,
    const struct spatial_distance_geometry *right_line,
    const struct spatial_segment *right_segment
);
static bool simplicity_point_is_line_boundary(
    const struct spatial_point *point,
    const struct spatial_distance_geometry *line
);
static bool simplicity_line_is_closed(const struct spatial_distance_geometry *line);
static bool simplicity_point_is_on_line(
    const struct spatial_point *point,
    const struct spatial_distance_geometry *line
);
static bool validity_geometry_is_valid(const struct spatial_distance_geometry *geometry);
static bool validity_point_is_valid(const struct spatial_point *point);
static bool validity_line_is_valid(const struct spatial_point *points, uint32_t point_count);
static bool validity_polygon_is_valid(const struct spatial_distance_geometry *polygon);
static bool validity_ring_is_valid(const struct spatial_distance_ring *ring);
static bool validity_ring_is_simple(const struct spatial_distance_ring *ring);
static bool validity_ring_segments_intersect_invalidly(
    const struct spatial_distance_ring *ring,
    uint32_t left_index,
    uint32_t right_index
);
static bool validity_polygon_holes_are_valid(const struct spatial_distance_geometry *polygon);
static bool validity_ring_point_is_inside_or_on_boundary(
    const struct spatial_distance_ring *ring,
    const struct spatial_point *point
);
static bool validity_ring_contains_point_interior(
    const struct spatial_distance_ring *ring,
    const struct spatial_point *point
);
static bool validity_rings_intersect_invalidly(
    const struct spatial_distance_ring *left,
    const struct spatial_distance_ring *right
);
static bool validity_polygons_intersect_invalidly(
    const struct spatial_distance_geometry *left,
    const struct spatial_distance_geometry *right
);
static bool validity_polygon_contains_point_interior(
    const struct spatial_distance_geometry *polygon,
    const struct spatial_point *point
);
static bool validity_segment_intersection_is_invalid(
    const struct spatial_segment *left, // NOLINT(bugprone-easily-swappable-parameters)
    const struct spatial_segment *right,
    bool allow_endpoint_touch
);
static bool spatial_segments_are_same_undirected(
    const struct spatial_segment *left,
    const struct spatial_segment *right
);
static bool spatial_point_is_segment_endpoint(
    const struct spatial_point *point,
    const struct spatial_segment *segment
);
static bool spatial_point_is_on_segment_interior(
    const struct spatial_point *point,
    const struct spatial_segment *segment
);
static double distance_line_to_ring(
    const struct spatial_distance_geometry *line,
    const struct spatial_distance_ring *ring
);
static double distance_ring_to_ring(
    const struct spatial_distance_ring *left,
    const struct spatial_distance_ring *right
);
static double distance_point_to_ring(
    const struct spatial_point *point,
    const struct spatial_distance_ring *ring
);
static bool ring_has_segment(const struct spatial_distance_ring *ring);
static bool line_has_segment(const struct spatial_distance_geometry *line);
static struct spatial_segment ring_segment(
    const struct spatial_distance_ring *ring,
    uint32_t index
);
static struct spatial_segment line_segment(
    const struct spatial_distance_geometry *line,
    uint32_t index
);
static double distance_point_to_segment(
    const struct spatial_point *point,
    const struct spatial_segment *segment
);
static double distance_segment_to_segment(
    const struct spatial_segment *left,
    const struct spatial_segment *right
);
static double distance_point_to_point(
    const struct spatial_point *left,
    const struct spatial_point *right
);
static bool segments_intersect(
    const struct spatial_segment *left,
    const struct spatial_segment *right
);
static bool point_on_segment(
    const struct spatial_point *point,
    const struct spatial_segment *segment
);
static double point_cross_product(
    const struct spatial_point *origin,
    const struct spatial_point *left,
    const struct spatial_point *right
);
static bool double_near_zero(double value);
static void distance_consider(double candidate, double *io_distance, bool *io_has_distance);
static int wkb_swap_xy_at(
    struct spatial_wkb_cursor *cursor,
    struct spatial_buffer *out_wkb,
    struct mylite_spatial_error *error,
    const char *function_name
);
static int line_point_from_wkb(
    const struct spatial_geometry_view *geometry,
    uint32_t point_index,
    struct spatial_point *out_point,
    bool *out_found,
    struct mylite_spatial_error *error,
    const char *function_name
);
static int line_points_from_wkb(
    const struct spatial_geometry_view *geometry,
    struct spatial_point **out_points,
    uint32_t *out_point_count,
    struct mylite_spatial_error *error,
    const char *function_name
);
static double line_points_length(const struct spatial_point *points, uint32_t point_count);
static struct spatial_point line_point_at_distance(
    const struct spatial_point *points,
    uint32_t point_count,
    double target_distance
);
static int interpolated_line_points(
    const struct spatial_point *line_points,
    uint32_t line_point_count, // NOLINT(bugprone-easily-swappable-parameters)
    double fraction,
    struct spatial_point **out_points,
    uint32_t *out_point_count,
    struct mylite_spatial_error *error,
    const char *function_name
);
static int polygon_ring_from_wkb(
    const struct spatial_geometry_view *geometry,
    uint32_t ring_index,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    bool *out_found,
    struct mylite_spatial_error *error,
    const char *function_name
);
static void spatial_box_include_point(
    struct spatial_box *box,
    double coordinate_x,
    double coordinate_y
);
static bool spatial_box_equals(const struct spatial_box *left, const struct spatial_box *right);
static bool spatial_box_covers(const struct spatial_box *left, const struct spatial_box *right);
static bool spatial_box_intersects(const struct spatial_box *left, const struct spatial_box *right);
static bool spatial_box_interiors_intersect(
    const struct spatial_box *left,
    const struct spatial_box *right
);
static int geohash_encode(
    double longitude, // NOLINT(bugprone-easily-swappable-parameters): geohash inputs.
    double latitude,
    uint32_t max_length,
    unsigned char **out_text,
    size_t *out_text_length,
    struct mylite_spatial_error *error
);
static int geohash_decode(
    const struct mylite_spatial_argument *argument,
    double *out_longitude,
    double *out_latitude,
    struct mylite_spatial_error *error,
    const char *function_name
);
static double geohash_round_coordinate(
    double value, // NOLINT(bugprone-easily-swappable-parameters): interval rounding.
    double minimum,
    double maximum
);
static int geohash_character_value(unsigned char byte);
static int append_wkb_as_wkt(
    struct spatial_buffer *buffer,
    struct spatial_wkb_cursor *cursor,
    struct mylite_spatial_error *error,
    const char *function_name
);
static int append_wkb_point_body_as_wkt(
    struct spatial_buffer *buffer,
    struct spatial_wkb_cursor *cursor,
    bool little_endian,
    struct mylite_spatial_error *error,
    const char *function_name
);
static int append_wkb_line_body_as_wkt(
    struct spatial_buffer *buffer,
    struct spatial_wkb_cursor *cursor,
    bool little_endian,
    struct mylite_spatial_error *error,
    const char *function_name
);
static int append_wkb_polygon_body_as_wkt(
    struct spatial_buffer *buffer,
    struct spatial_wkb_cursor *cursor,
    bool little_endian,
    struct mylite_spatial_error *error,
    const char *function_name
);
static int append_wkb_collection_body_as_wkt(
    struct spatial_buffer *buffer,
    struct spatial_wkb_cursor *cursor,
    bool little_endian,
    enum mylite_spatial_geometry_type type,
    struct mylite_spatial_error *error,
    const char *function_name
);
static int append_point_coordinates_as_wkt(
    struct spatial_buffer *buffer,
    struct spatial_wkb_cursor *cursor,
    bool little_endian,
    struct mylite_spatial_error *error,
    const char *function_name
);
static int append_double_as_text(struct spatial_buffer *buffer, double value);
static int append_geojson_geometry(
    struct spatial_buffer *buffer,
    struct spatial_wkb_cursor *cursor,
    uint32_t srid,
    uint32_t max_dec_digits,
    struct mylite_spatial_error *error,
    const char *function_name
);
static int append_geojson_coordinates(
    struct spatial_buffer *buffer,
    struct spatial_wkb_cursor *cursor,
    enum mylite_spatial_geometry_type type,
    bool little_endian,
    uint32_t srid,
    uint32_t max_dec_digits,
    struct mylite_spatial_error *error,
    const char *function_name
);
static int append_geojson_point_coordinates(
    struct spatial_buffer *buffer,
    struct spatial_wkb_cursor *cursor,
    bool little_endian,
    uint32_t srid, // NOLINT(bugprone-easily-swappable-parameters): GeoJSON axis options.
    uint32_t max_dec_digits,
    struct mylite_spatial_error *error,
    const char *function_name
);
static int append_geojson_line_coordinates(
    struct spatial_buffer *buffer,
    struct spatial_wkb_cursor *cursor,
    bool little_endian,
    uint32_t srid,
    uint32_t max_dec_digits,
    struct mylite_spatial_error *error,
    const char *function_name
);
static int append_geojson_polygon_coordinates(
    struct spatial_buffer *buffer,
    struct spatial_wkb_cursor *cursor,
    bool little_endian,
    uint32_t srid,
    uint32_t max_dec_digits,
    struct mylite_spatial_error *error,
    const char *function_name
);
static int append_geojson_collection_coordinates(
    struct spatial_buffer *buffer,
    struct spatial_wkb_cursor *cursor,
    enum mylite_spatial_geometry_type type,
    bool little_endian,
    uint32_t srid,
    uint32_t max_dec_digits,
    struct mylite_spatial_error *error,
    const char *function_name
);
static int append_geojson_number(
    struct spatial_buffer *buffer,
    double value,
    uint32_t max_dec_digits
);
static int append_geojson_bbox(
    struct spatial_buffer *buffer,
    const struct spatial_box *box,
    uint32_t srid, // NOLINT(bugprone-easily-swappable-parameters): GeoJSON axis options.
    uint32_t max_dec_digits
);
static int append_geojson_geometry_as_wkt(
    const struct json_value *value,
    struct geojson_parse_context *context,
    struct spatial_buffer *out_wkt,
    bool *out_is_null
);
static int append_geojson_feature_as_wkt(
    const struct json_value *value,
    struct geojson_parse_context *context,
    struct spatial_buffer *out_wkt,
    bool *out_is_null
);
static int append_geojson_feature_collection_as_wkt(
    const struct json_value *value,
    struct geojson_parse_context *context,
    struct spatial_buffer *out_wkt
);
static int append_geojson_coordinate_as_wkt(
    const struct json_value *value,
    struct geojson_parse_context *context,
    struct spatial_buffer *out_wkt
);
static int append_geojson_coordinate_list_as_wkt(
    const struct json_value *value,
    struct geojson_parse_context *context,
    struct spatial_buffer *out_wkt
);
static int append_geojson_ring_list_as_wkt(
    const struct json_value *value,
    struct geojson_parse_context *context,
    struct spatial_buffer *out_wkt
);
static int append_geojson_polygon_list_as_wkt(
    const struct json_value *value,
    struct geojson_parse_context *context,
    struct spatial_buffer *out_wkt
);
static int geojson_coordinate_number(
    const struct json_value *value,
    double *out_value,
    struct geojson_parse_context *context
);
static const char *geojson_geometry_type_name(enum mylite_spatial_geometry_type type);
static const struct json_value *geojson_member_value(
    const struct json_value *value,
    const char *member
);
static bool geojson_member_name_equals(const char *left, size_t left_length, const char *right);
static bool geojson_string_equals(const struct json_value *value, const char *expected);
static int append_cstring(struct spatial_buffer *buffer, const char *text);
static int parse_wkt_to_internal(
    const char *text,
    size_t text_size,
    const char *function_name,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    enum mylite_spatial_geometry_type *out_type,
    struct mylite_spatial_error *error
);
static int parse_wkt_geometry(
    struct spatial_wkt_parser *parser,
    struct spatial_buffer *out_wkb,
    enum mylite_spatial_geometry_type *out_type
);
static int parse_wkt_point(struct spatial_wkt_parser *parser, struct spatial_buffer *out_wkb);
static int parse_wkt_linestring(struct spatial_wkt_parser *parser, struct spatial_buffer *out_wkb);
static int parse_wkt_polygon(struct spatial_wkt_parser *parser, struct spatial_buffer *out_wkb);
static int parse_wkt_multipoint(struct spatial_wkt_parser *parser, struct spatial_buffer *out_wkb);
static int parse_wkt_multilinestring(
    struct spatial_wkt_parser *parser,
    struct spatial_buffer *out_wkb
);
static int parse_wkt_multipolygon(
    struct spatial_wkt_parser *parser,
    struct spatial_buffer *out_wkb
);
static int parse_wkt_geometrycollection(
    struct spatial_wkt_parser *parser,
    struct spatial_buffer *out_wkb
);
static int parse_wkt_coordinate(struct spatial_wkt_parser *parser, double *out_x, double *out_y);
static int parse_wkt_coordinate_list(
    struct spatial_wkt_parser *parser,
    struct spatial_buffer *out_points,
    uint32_t *out_point_count
);
static int parse_wkt_ring_list(
    struct spatial_wkt_parser *parser,
    struct spatial_buffer *out_rings,
    uint32_t *out_ring_count
);
static bool wkt_match_type(
    struct spatial_wkt_parser *parser,
    enum mylite_spatial_geometry_type *out_type
);
static bool wkt_match_keyword(struct spatial_wkt_parser *parser, const char *keyword);
static int wkt_expect_byte(struct spatial_wkt_parser *parser, char expected);
static bool wkt_consume_byte(struct spatial_wkt_parser *parser, char expected);
static void wkt_skip_space(struct spatial_wkt_parser *parser);
static bool wkt_at_end(struct spatial_wkt_parser *parser);

bool mylite_spatial_function_kind_from_name(
    const char *name,
    size_t name_size,
    enum mylite_spatial_function_kind *out_kind
) {
    if (out_kind == NULL) {
        return false;
    }
    *out_kind = MYLITE_SPATIAL_FUNCTION_NONE;
    if (name == NULL) {
        return false;
    }
    for (size_t index = 0U;
         index < sizeof(spatial_function_descriptors) / sizeof(spatial_function_descriptors[0]);
         ++index) {
        if (function_name_matches(name, name_size, spatial_function_descriptors[index].name)) {
            *out_kind = spatial_function_descriptors[index].kind;
            return true;
        }
    }
    return false;
}

const char *mylite_spatial_function_name(enum mylite_spatial_function_kind kind) {
    for (size_t index = 0U;
         index < sizeof(spatial_function_descriptors) / sizeof(spatial_function_descriptors[0]);
         ++index) {
        if (spatial_function_descriptors[index].kind == kind) {
            return spatial_function_descriptors[index].name;
        }
    }
    return "spatial";
}

bool mylite_spatial_function_is_constructor(enum mylite_spatial_function_kind kind) {
    return kind >= MYLITE_SPATIAL_FUNCTION_POINT && kind <= MYLITE_SPATIAL_FUNCTION_GEOMCOLLECTION;
}

bool mylite_spatial_function_returns_geometry(enum mylite_spatial_function_kind kind) {
    return mylite_spatial_function_is_constructor(kind) || function_kind_is_from_text(kind) ||
           function_kind_is_from_wkb(kind) || kind == MYLITE_SPATIAL_FUNCTION_ST_GEOMETRYN ||
           kind == MYLITE_SPATIAL_FUNCTION_ST_POINTN ||
           kind == MYLITE_SPATIAL_FUNCTION_ST_STARTPOINT ||
           kind == MYLITE_SPATIAL_FUNCTION_ST_ENDPOINT ||
           kind == MYLITE_SPATIAL_FUNCTION_ST_EXTERIORRING ||
           kind == MYLITE_SPATIAL_FUNCTION_ST_INTERIORRINGN ||
           kind == MYLITE_SPATIAL_FUNCTION_ST_ENVELOPE ||
           kind == MYLITE_SPATIAL_FUNCTION_ST_SWAPXY ||
           kind == MYLITE_SPATIAL_FUNCTION_ST_MAKEENVELOPE ||
           kind == MYLITE_SPATIAL_FUNCTION_ST_POINTFROMGEOHASH ||
           kind == MYLITE_SPATIAL_FUNCTION_ST_GEOMFROMGEOJSON ||
           kind == MYLITE_SPATIAL_FUNCTION_ST_LINEINTERPOLATEPOINT ||
           kind == MYLITE_SPATIAL_FUNCTION_ST_LINEINTERPOLATEPOINTS ||
           kind == MYLITE_SPATIAL_FUNCTION_ST_POINTATDISTANCE ||
           kind == MYLITE_SPATIAL_FUNCTION_ST_CENTROID ||
           kind == MYLITE_SPATIAL_FUNCTION_ST_CONVEXHULL ||
           kind == MYLITE_SPATIAL_FUNCTION_ST_SIMPLIFY ||
           kind == MYLITE_SPATIAL_FUNCTION_ST_VALIDATE;
}

bool mylite_spatial_function_returns_text(enum mylite_spatial_function_kind kind) {
    return kind == MYLITE_SPATIAL_FUNCTION_ST_ASTEXT || kind == MYLITE_SPATIAL_FUNCTION_ST_ASWKT ||
           kind == MYLITE_SPATIAL_FUNCTION_ST_GEOMETRYTYPE ||
           kind == MYLITE_SPATIAL_FUNCTION_ST_GEOHASH ||
           kind == MYLITE_SPATIAL_FUNCTION_ST_ASGEOJSON;
}

bool mylite_spatial_function_returns_wkb(enum mylite_spatial_function_kind kind) {
    return kind == MYLITE_SPATIAL_FUNCTION_ST_ASBINARY || kind == MYLITE_SPATIAL_FUNCTION_ST_ASWKB;
}

bool mylite_spatial_function_returns_integer(enum mylite_spatial_function_kind kind) {
    return kind == MYLITE_SPATIAL_FUNCTION_ST_SRID ||
           kind == MYLITE_SPATIAL_FUNCTION_ST_DIMENSION ||
           kind == MYLITE_SPATIAL_FUNCTION_ST_ISEMPTY ||
           kind == MYLITE_SPATIAL_FUNCTION_ST_ISSIMPLE ||
           kind == MYLITE_SPATIAL_FUNCTION_ST_ISVALID ||
           kind == MYLITE_SPATIAL_FUNCTION_ST_DISJOINT ||
           kind == MYLITE_SPATIAL_FUNCTION_ST_INTERSECTS ||
           kind == MYLITE_SPATIAL_FUNCTION_ST_CONTAINS ||
           kind == MYLITE_SPATIAL_FUNCTION_ST_WITHIN || kind == MYLITE_SPATIAL_FUNCTION_ST_EQUALS ||
           kind == MYLITE_SPATIAL_FUNCTION_ST_TOUCHES ||
           kind == MYLITE_SPATIAL_FUNCTION_ST_OVERLAPS ||
           kind == MYLITE_SPATIAL_FUNCTION_ST_CROSSES ||
           kind == MYLITE_SPATIAL_FUNCTION_ST_ISCLOSED ||
           kind == MYLITE_SPATIAL_FUNCTION_ST_NUMGEOMETRIES ||
           kind == MYLITE_SPATIAL_FUNCTION_ST_NUMPOINTS ||
           kind == MYLITE_SPATIAL_FUNCTION_ST_NUMINTERIORRING ||
           kind == MYLITE_SPATIAL_FUNCTION_ST_NUMINTERIORRINGS ||
           (kind >= MYLITE_SPATIAL_FUNCTION_MBRCONTAINS && kind <= MYLITE_SPATIAL_FUNCTION_MBRWITHIN
           );
}

bool mylite_spatial_function_returns_double(enum mylite_spatial_function_kind kind) {
    return kind == MYLITE_SPATIAL_FUNCTION_ST_X || kind == MYLITE_SPATIAL_FUNCTION_ST_Y ||
           kind == MYLITE_SPATIAL_FUNCTION_ST_LENGTH || kind == MYLITE_SPATIAL_FUNCTION_ST_AREA ||
           kind == MYLITE_SPATIAL_FUNCTION_ST_DISTANCE ||
           kind == MYLITE_SPATIAL_FUNCTION_ST_DISTANCESPHERE ||
           kind == MYLITE_SPATIAL_FUNCTION_ST_FRECHETDISTANCE ||
           kind == MYLITE_SPATIAL_FUNCTION_ST_HAUSDORFFDISTANCE ||
           kind == MYLITE_SPATIAL_FUNCTION_ST_LATFROMGEOHASH ||
           kind == MYLITE_SPATIAL_FUNCTION_ST_LONGFROMGEOHASH ||
           kind == MYLITE_SPATIAL_FUNCTION_ST_LATITUDE ||
           kind == MYLITE_SPATIAL_FUNCTION_ST_LONGITUDE;
}

int mylite_spatial_evaluate(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *out_error
) {
    if (out_result == NULL) {
        return set_spatial_error(
            out_error,
            mysql_error_invalid_gis_data,
            "22023",
            "Invalid GIS data"
        );
    }
    *out_result = (struct mylite_spatial_result){0};

    if (kind == MYLITE_SPATIAL_FUNCTION_POINT) {
        return evaluate_point_constructor(arguments, argument_count, out_result, out_error);
    }
    if (mylite_spatial_function_is_constructor(kind)) {
        return evaluate_sequence_constructor(
            kind,
            arguments,
            argument_count,
            out_result,
            out_error
        );
    }
    if (function_kind_is_from_text(kind)) {
        return evaluate_from_text(kind, arguments, argument_count, out_result, out_error);
    }
    if (function_kind_is_from_wkb(kind)) {
        return evaluate_from_wkb(kind, arguments, argument_count, out_result, out_error);
    }
    switch (kind) {
    case MYLITE_SPATIAL_FUNCTION_ST_ASTEXT:
    case MYLITE_SPATIAL_FUNCTION_ST_ASWKT:
        return evaluate_as_text(kind, arguments, argument_count, out_result, out_error);
    case MYLITE_SPATIAL_FUNCTION_ST_ASBINARY:
    case MYLITE_SPATIAL_FUNCTION_ST_ASWKB:
        return evaluate_as_wkb(kind, arguments, argument_count, out_result, out_error);
    case MYLITE_SPATIAL_FUNCTION_ST_GEOMETRYTYPE:
        return evaluate_geometry_type(kind, arguments, argument_count, out_result, out_error);
    case MYLITE_SPATIAL_FUNCTION_ST_SRID:
        return evaluate_srid(kind, arguments, argument_count, out_result, out_error);
    case MYLITE_SPATIAL_FUNCTION_ST_X:
    case MYLITE_SPATIAL_FUNCTION_ST_Y:
        return evaluate_point_coordinate(kind, arguments, argument_count, out_result, out_error);
    case MYLITE_SPATIAL_FUNCTION_ST_LATITUDE:
    case MYLITE_SPATIAL_FUNCTION_ST_LONGITUDE:
        return evaluate_point_geographic_coordinate(
            kind,
            arguments,
            argument_count,
            out_result,
            out_error
        );
    case MYLITE_SPATIAL_FUNCTION_ST_DIMENSION:
        return evaluate_dimension(kind, arguments, argument_count, out_result, out_error);
    case MYLITE_SPATIAL_FUNCTION_ST_ISEMPTY:
        return evaluate_is_empty(kind, arguments, argument_count, out_result, out_error);
    case MYLITE_SPATIAL_FUNCTION_ST_ISSIMPLE:
        return evaluate_is_simple(kind, arguments, argument_count, out_result, out_error);
    case MYLITE_SPATIAL_FUNCTION_ST_ISVALID:
        return evaluate_is_valid(kind, arguments, argument_count, out_result, out_error);
    case MYLITE_SPATIAL_FUNCTION_ST_VALIDATE:
        return evaluate_validate(kind, arguments, argument_count, out_result, out_error);
    case MYLITE_SPATIAL_FUNCTION_ST_ISCLOSED:
        return evaluate_is_closed(kind, arguments, argument_count, out_result, out_error);
    case MYLITE_SPATIAL_FUNCTION_ST_NUMGEOMETRIES:
        return evaluate_num_geometries(kind, arguments, argument_count, out_result, out_error);
    case MYLITE_SPATIAL_FUNCTION_ST_GEOMETRYN:
        return evaluate_geometry_n(kind, arguments, argument_count, out_result, out_error);
    case MYLITE_SPATIAL_FUNCTION_ST_NUMPOINTS:
        return evaluate_num_points(kind, arguments, argument_count, out_result, out_error);
    case MYLITE_SPATIAL_FUNCTION_ST_POINTN:
    case MYLITE_SPATIAL_FUNCTION_ST_STARTPOINT:
    case MYLITE_SPATIAL_FUNCTION_ST_ENDPOINT:
        return evaluate_line_point_accessor(kind, arguments, argument_count, out_result, out_error);
    case MYLITE_SPATIAL_FUNCTION_ST_EXTERIORRING:
    case MYLITE_SPATIAL_FUNCTION_ST_INTERIORRINGN:
    case MYLITE_SPATIAL_FUNCTION_ST_NUMINTERIORRING:
    case MYLITE_SPATIAL_FUNCTION_ST_NUMINTERIORRINGS:
        return evaluate_polygon_ring_accessor(
            kind,
            arguments,
            argument_count,
            out_result,
            out_error
        );
    case MYLITE_SPATIAL_FUNCTION_ST_LENGTH:
        return evaluate_length(kind, arguments, argument_count, out_result, out_error);
    case MYLITE_SPATIAL_FUNCTION_ST_AREA:
        return evaluate_area(kind, arguments, argument_count, out_result, out_error);
    case MYLITE_SPATIAL_FUNCTION_ST_CENTROID:
        return evaluate_centroid(kind, arguments, argument_count, out_result, out_error);
    case MYLITE_SPATIAL_FUNCTION_ST_CONVEXHULL:
        return evaluate_convex_hull(kind, arguments, argument_count, out_result, out_error);
    case MYLITE_SPATIAL_FUNCTION_ST_SIMPLIFY:
        return evaluate_simplify(kind, arguments, argument_count, out_result, out_error);
    case MYLITE_SPATIAL_FUNCTION_ST_DISTANCE:
        return evaluate_distance(kind, arguments, argument_count, out_result, out_error);
    case MYLITE_SPATIAL_FUNCTION_ST_DISJOINT:
    case MYLITE_SPATIAL_FUNCTION_ST_INTERSECTS:
    case MYLITE_SPATIAL_FUNCTION_ST_CONTAINS:
    case MYLITE_SPATIAL_FUNCTION_ST_WITHIN:
    case MYLITE_SPATIAL_FUNCTION_ST_EQUALS:
    case MYLITE_SPATIAL_FUNCTION_ST_TOUCHES:
    case MYLITE_SPATIAL_FUNCTION_ST_OVERLAPS:
    case MYLITE_SPATIAL_FUNCTION_ST_CROSSES:
        return evaluate_relation_predicate(kind, arguments, argument_count, out_result, out_error);
    case MYLITE_SPATIAL_FUNCTION_ST_DISTANCESPHERE:
        return evaluate_distance_sphere(kind, arguments, argument_count, out_result, out_error);
    case MYLITE_SPATIAL_FUNCTION_ST_FRECHETDISTANCE:
    case MYLITE_SPATIAL_FUNCTION_ST_HAUSDORFFDISTANCE:
        return evaluate_discrete_distance(kind, arguments, argument_count, out_result, out_error);
    case MYLITE_SPATIAL_FUNCTION_ST_LINEINTERPOLATEPOINT:
    case MYLITE_SPATIAL_FUNCTION_ST_LINEINTERPOLATEPOINTS:
    case MYLITE_SPATIAL_FUNCTION_ST_POINTATDISTANCE:
        return evaluate_line_interpolation(kind, arguments, argument_count, out_result, out_error);
    case MYLITE_SPATIAL_FUNCTION_ST_ENVELOPE:
        return evaluate_envelope(kind, arguments, argument_count, out_result, out_error);
    case MYLITE_SPATIAL_FUNCTION_ST_SWAPXY:
        return evaluate_swap_xy(kind, arguments, argument_count, out_result, out_error);
    case MYLITE_SPATIAL_FUNCTION_ST_MAKEENVELOPE:
        return evaluate_make_envelope(kind, arguments, argument_count, out_result, out_error);
    case MYLITE_SPATIAL_FUNCTION_MBRCONTAINS:
    case MYLITE_SPATIAL_FUNCTION_MBRCOVEREDBY:
    case MYLITE_SPATIAL_FUNCTION_MBRCOVERS:
    case MYLITE_SPATIAL_FUNCTION_MBRDISJOINT:
    case MYLITE_SPATIAL_FUNCTION_MBREQUALS:
    case MYLITE_SPATIAL_FUNCTION_MBRINTERSECTS:
    case MYLITE_SPATIAL_FUNCTION_MBROVERLAPS:
    case MYLITE_SPATIAL_FUNCTION_MBRTOUCHES:
    case MYLITE_SPATIAL_FUNCTION_MBRWITHIN:
        return evaluate_mbr_predicate(kind, arguments, argument_count, out_result, out_error);
    case MYLITE_SPATIAL_FUNCTION_ST_GEOHASH:
        return evaluate_geohash(kind, arguments, argument_count, out_result, out_error);
    case MYLITE_SPATIAL_FUNCTION_ST_LATFROMGEOHASH:
    case MYLITE_SPATIAL_FUNCTION_ST_LONGFROMGEOHASH:
        return evaluate_coordinate_from_geohash(
            kind,
            arguments,
            argument_count,
            out_result,
            out_error
        );
    case MYLITE_SPATIAL_FUNCTION_ST_POINTFROMGEOHASH:
        return evaluate_point_from_geohash(kind, arguments, argument_count, out_result, out_error);
    case MYLITE_SPATIAL_FUNCTION_ST_ASGEOJSON:
        return evaluate_as_geojson(kind, arguments, argument_count, out_result, out_error);
    case MYLITE_SPATIAL_FUNCTION_ST_GEOMFROMGEOJSON:
        return evaluate_from_geojson(kind, arguments, argument_count, out_result, out_error);
    default:
        break;
    }
    return set_parameter_count_error(out_error, kind);
}

void mylite_spatial_result_deinit(struct mylite_spatial_result *result) {
    if (result == NULL) {
        return;
    }
    free(result->bytes);
    *result = (struct mylite_spatial_result){0};
}

bool mylite_spatial_geometry_bytes_are_valid(const void *bytes, size_t byte_count) {
    return validate_internal_geometry(bytes, byte_count, NULL, NULL, NULL, "spatial") == 0;
}

enum mylite_spatial_geometry_type mylite_spatial_geometry_bytes_type(
    const void *bytes,
    size_t byte_count
) {
    enum mylite_spatial_geometry_type type = MYLITE_SPATIAL_GEOMETRY_NONE;

    (void)validate_internal_geometry(bytes, byte_count, &type, NULL, NULL, "spatial");
    return type;
}

uint32_t mylite_spatial_geometry_bytes_srid(const void *bytes, size_t byte_count) {
    uint32_t srid = 0U;

    (void)validate_internal_geometry(bytes, byte_count, NULL, &srid, NULL, "spatial");
    return srid;
}

static int evaluate_point_constructor(
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
) {
    double coordinate_x = 0.0;
    double coordinate_y = 0.0;
    unsigned char *bytes = NULL;
    size_t byte_count = 0U;
    int rc = validate_argument_count(MYLITE_SPATIAL_FUNCTION_POINT, argument_count, 2U, 2U, error);

    if (rc != 0) {
        return rc;
    }
    if (any_argument_is_null(arguments, argument_count)) {
        return assign_null_result(out_result);
    }
    rc = argument_numeric(&arguments[0], &coordinate_x, error, "point");
    if (rc == 0) {
        rc = argument_numeric(&arguments[1], &coordinate_y, error, "point");
    }
    if (rc == 0) {
        rc = make_point_internal_geometry(coordinate_x, coordinate_y, &bytes, &byte_count, error);
    }
    if (rc != 0) {
        free(bytes);
        return rc;
    }
    return assign_owned_bytes_result(out_result, MYLITE_SPATIAL_RESULT_GEOMETRY, bytes, byte_count);
}

static int evaluate_as_geojson(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
) {
    struct spatial_geometry_view geometry = {0};
    struct spatial_wkb_cursor cursor = {0};
    struct spatial_wkb_cursor bounds_cursor = {0};
    struct spatial_box box = {0};
    struct spatial_buffer buffer = {0};
    bool is_null = false;
    bool max_digits_is_null = false;
    bool options_is_null = false;
    uint32_t max_dec_digits = spatial_geojson_default_max_dec_digits;
    uint32_t options = 0U;
    int rc = validate_argument_count(kind, argument_count, 1U, 3U, error);

    if (rc != 0) {
        return rc;
    }
    if (argument_count > 1U) {
        rc = argument_geojson_uint32(
            &arguments[1],
            &max_dec_digits,
            &max_digits_is_null,
            error,
            mylite_spatial_function_name(kind),
            "max decimal digits"
        );
    }
    if (rc == 0 && argument_count > 2U) {
        rc = argument_geojson_uint32(
            &arguments[2],
            &options,
            &options_is_null,
            error,
            mylite_spatial_function_name(kind),
            "options"
        );
    }
    if (rc != 0) {
        return rc;
    }
    if (arguments[0].is_null || max_digits_is_null || options_is_null) {
        return assign_null_result(out_result);
    }
    if (argument_count > 2U && options > spatial_geojson_as_max_option) {
        char text[spatial_double_text_capacity];

        snprintf(text, sizeof(text), "%u", options);
        return set_geojson_option_error(error, text, mylite_spatial_function_name(kind));
    }
    rc = read_single_geometry_argument(kind, arguments, 1U, &geometry, &is_null, error);
    if (rc != 0) {
        return rc;
    }
    if (is_null) {
        return assign_null_result(out_result);
    }
    cursor = (struct spatial_wkb_cursor){.bytes = geometry.wkb, .size = geometry.wkb_size};
    bounds_cursor = cursor;

    rc = spatial_buffer_append_byte(&buffer, '{');
    if (rc == 0 && geometry.srid != 0U &&
        (options & (spatial_geojson_short_crs_option | spatial_geojson_long_crs_option)) != 0U) {
        char srid_text[spatial_double_text_capacity];
        const char *prefix = (options & spatial_geojson_long_crs_option) != 0U
                                 ? "\"crs\": {\"type\": \"name\", \"properties\": {\"name\": "
                                   "\"urn:ogc:def:crs:EPSG::"
                                 : "\"crs\": {\"type\": \"name\", \"properties\": {\"name\": "
                                   "\"EPSG:";

        rc = append_cstring(&buffer, prefix);
        snprintf(srid_text, sizeof(srid_text), "%u", geometry.srid);
        if (rc == 0) {
            rc = spatial_buffer_append(&buffer, srid_text, strlen(srid_text));
        }
        if (rc == 0) {
            rc = append_cstring(&buffer, "\"}}, ");
        }
    }
    if (rc == 0 && (options & spatial_geojson_bbox_option) != 0U) {
        rc = wkb_bounds_at(&bounds_cursor, &box, error, mylite_spatial_function_name(kind));
        if (rc == 0) {
            rc = append_geojson_bbox(&buffer, &box, geometry.srid, max_dec_digits);
        }
        if (rc == 0) {
            rc = append_cstring(&buffer, ", ");
        }
    }
    if (rc == 0) {
        rc = append_geojson_geometry(
            &buffer,
            &cursor,
            geometry.srid,
            max_dec_digits,
            error,
            mylite_spatial_function_name(kind)
        );
    }
    if (rc == 0) {
        rc = spatial_buffer_append_byte(&buffer, '}');
    }
    if (rc != 0) {
        spatial_buffer_deinit(&buffer);
        if (error != NULL && error->code == 0 && !error->is_nomem) {
            return set_nomem_error(error);
        }
        return rc;
    }
    return assign_owned_bytes_result(
        out_result,
        MYLITE_SPATIAL_RESULT_TEXT,
        buffer.bytes,
        buffer.size
    );
}

static int evaluate_from_geojson(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
) {
    struct json_parser parser = {0};
    struct json_value value = {0};
    struct geojson_parse_context context = {0};
    struct spatial_buffer wkt = {0};
    enum mylite_spatial_geometry_type actual_type = MYLITE_SPATIAL_GEOMETRY_NONE;
    unsigned char *bytes = NULL;
    size_t byte_count = 0U;
    bool options_is_null = false;
    bool srid_is_null = false;
    bool geometry_is_null = false;
    uint32_t options = spatial_geojson_from_min_option;
    uint32_t srid = spatial_srid_wgs84;
    int rc = validate_argument_count(kind, argument_count, 1U, 3U, error);

    if (rc != 0) {
        return rc;
    }
    if (argument_count > 1U) {
        rc = argument_geojson_uint32(
            &arguments[1],
            &options,
            &options_is_null,
            error,
            mylite_spatial_function_name(kind),
            "options"
        );
    }
    if (rc == 0 && argument_count > 2U) {
        rc = argument_geojson_srid(
            &arguments[2],
            &srid,
            &srid_is_null,
            error,
            mylite_spatial_function_name(kind)
        );
    }
    if (rc != 0) {
        return rc;
    }
    if (options_is_null || srid_is_null) {
        return assign_null_result(out_result);
    }
    if (options < spatial_geojson_from_min_option || options > spatial_geojson_from_max_option) {
        char text[spatial_double_text_capacity];

        snprintf(text, sizeof(text), "%u", options);
        return set_geojson_option_error(error, text, mylite_spatial_function_name(kind));
    }
    if (srid != 0U && srid != spatial_srid_wgs84) {
        return set_spatial_error(
            error,
            mysql_error_srs_not_found,
            "SR001",
            "There's no spatial reference system with SRID %u.",
            srid
        );
    }
    if (arguments[0].is_null) {
        return assign_null_result(out_result);
    }

    parser = (struct json_parser){
        .text = (const char *)arguments[0].bytes,
        .length = arguments[0].byte_count,
        .position = 0U,
        .result = {.status = MYLITE_JSON_NORMALIZE_OK, .position = 0U},
    };
    rc = mylite_json_internal_parse_document(&parser, &value);
    if (rc != MYLITE_OK) {
        mylite_json_internal_value_deinit(&value);
        return set_invalid_json_text_error(
            error,
            &parser.result,
            mylite_spatial_function_name(kind)
        );
    }
    context = (struct geojson_parse_context){
        .srid = srid,
        .strip_extra_dimensions = options != spatial_geojson_from_min_option,
        .function_name = mylite_spatial_function_name(kind),
        .error = error,
    };
    rc = append_geojson_geometry_as_wkt(&value, &context, &wkt, &geometry_is_null);
    if (rc == 0 && geometry_is_null) {
        mylite_json_internal_value_deinit(&value);
        spatial_buffer_deinit(&wkt);
        return assign_null_result(out_result);
    }
    if (rc == 0) {
        rc = spatial_buffer_append_byte(&wkt, '\0');
    }
    if (rc == 0) {
        rc = parse_wkt_to_internal(
            (const char *)wkt.bytes,
            wkt.size - 1U,
            mylite_spatial_function_name(kind),
            &bytes,
            &byte_count,
            &actual_type,
            error
        );
        (void)actual_type;
    }
    if (rc == 0 && srid != 0U) {
        unsigned char *srid_bytes = NULL;
        size_t srid_byte_count = 0U;
        struct spatial_geometry_view parsed = {
            .wkb = bytes + spatial_internal_srid_size,
            .wkb_size = byte_count - spatial_internal_srid_size,
            .srid = 0U,
        };

        rc = make_internal_geometry_from_wkb(
            parsed.wkb,
            parsed.wkb_size,
            srid,
            &srid_bytes,
            &srid_byte_count,
            error
        );
        if (rc == 0) {
            free(bytes);
            bytes = srid_bytes;
            byte_count = srid_byte_count;
        } else {
            free(srid_bytes);
        }
    }
    mylite_json_internal_value_deinit(&value);
    spatial_buffer_deinit(&wkt);
    if (rc != 0) {
        free(bytes);
        if (error != NULL && error->code == 0 && !error->is_nomem) {
            return set_nomem_error(error);
        }
        return rc;
    }
    return assign_owned_bytes_result(out_result, MYLITE_SPATIAL_RESULT_GEOMETRY, bytes, byte_count);
}

static int evaluate_sequence_constructor(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
) {
    enum mylite_spatial_geometry_type result_type = constructor_result_type(kind);
    enum mylite_spatial_geometry_type expected_type = MYLITE_SPATIAL_GEOMETRY_NONE;
    struct spatial_buffer wkb = {0};
    struct spatial_buffer internal = {0};
    int rc = 0;

    if (kind == MYLITE_SPATIAL_FUNCTION_GEOMETRYCOLLECTION ||
        kind == MYLITE_SPATIAL_FUNCTION_GEOMCOLLECTION) {
        rc = validate_argument_count(kind, argument_count, 0U, SIZE_MAX, error);
    } else {
        rc = validate_argument_count(kind, argument_count, 1U, SIZE_MAX, error);
    }
    if (rc != 0) {
        return rc;
    }
    if (any_argument_is_null(arguments, argument_count)) {
        return assign_null_result(out_result);
    }

    switch (result_type) {
    case MYLITE_SPATIAL_GEOMETRY_LINESTRING:
        expected_type = MYLITE_SPATIAL_GEOMETRY_POINT;
        break;
    case MYLITE_SPATIAL_GEOMETRY_POLYGON:
        expected_type = MYLITE_SPATIAL_GEOMETRY_LINESTRING;
        break;
    case MYLITE_SPATIAL_GEOMETRY_MULTIPOINT:
        expected_type = MYLITE_SPATIAL_GEOMETRY_POINT;
        break;
    case MYLITE_SPATIAL_GEOMETRY_MULTILINESTRING:
        expected_type = MYLITE_SPATIAL_GEOMETRY_LINESTRING;
        break;
    case MYLITE_SPATIAL_GEOMETRY_MULTIPOLYGON:
        expected_type = MYLITE_SPATIAL_GEOMETRY_POLYGON;
        break;
    default:
        break;
    }

    rc = spatial_buffer_append_byte(&wkb, 1U);
    if (rc == 0) {
        rc = spatial_buffer_append_u32_le(&wkb, (uint32_t)result_type);
    }
    if (rc == 0) {
        rc = spatial_buffer_append_u32_le(&wkb, (uint32_t)argument_count);
    }
    for (size_t index = 0U; rc == 0 && index < argument_count; ++index) {
        enum mylite_spatial_geometry_type argument_type = MYLITE_SPATIAL_GEOMETRY_NONE;
        const unsigned char *bytes = (const unsigned char *)arguments[index].bytes;
        size_t byte_count = arguments[index].byte_count;

        rc = validate_internal_geometry(
            bytes,
            byte_count,
            &argument_type,
            NULL,
            error,
            mylite_spatial_function_name(kind)
        );
        if (rc != 0) {
            break;
        }
        if (expected_type != MYLITE_SPATIAL_GEOMETRY_NONE && argument_type != expected_type) {
            rc = set_unexpected_geometry_type_error(
                error,
                geometry_type_name(expected_type),
                argument_type,
                mylite_spatial_function_name(kind)
            );
            break;
        }
        if (result_type == MYLITE_SPATIAL_GEOMETRY_LINESTRING) {
            rc = spatial_buffer_append(
                &wkb,
                bytes + spatial_internal_srid_size + spatial_wkb_header_size,
                spatial_coordinate_size
            );
        } else if (result_type == MYLITE_SPATIAL_GEOMETRY_POLYGON) {
            rc = spatial_buffer_append(
                &wkb,
                bytes + spatial_internal_srid_size + spatial_wkb_header_size,
                byte_count - spatial_internal_srid_size - spatial_wkb_header_size
            );
        } else {
            rc = spatial_buffer_append(
                &wkb,
                bytes + spatial_internal_srid_size,
                byte_count - spatial_internal_srid_size
            );
        }
    }
    if (rc == 0 && result_type == MYLITE_SPATIAL_GEOMETRY_LINESTRING && argument_count < 2U) {
        rc = set_invalid_gis_data_error(error, mylite_spatial_function_name(kind));
    }
    if (rc == 0) {
        rc = append_internal_prefix(&internal, 0U);
    }
    if (rc == 0) {
        rc = spatial_buffer_append(&internal, wkb.bytes, wkb.size);
    }
    spatial_buffer_deinit(&wkb);
    if (rc != 0) {
        spatial_buffer_deinit(&internal);
        if (error != NULL && error->code == 0) {
            return set_nomem_error(error);
        }
        return rc;
    }
    return assign_owned_bytes_result(
        out_result,
        MYLITE_SPATIAL_RESULT_GEOMETRY,
        internal.bytes,
        internal.size
    );
}

static int evaluate_from_text(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
) {
    enum mylite_spatial_geometry_type actual_type = MYLITE_SPATIAL_GEOMETRY_NONE;
    enum mylite_spatial_geometry_type expected_type = expected_text_wkb_type(kind);
    unsigned char *bytes = NULL;
    size_t byte_count = 0U;
    uint32_t srid = 0U;
    bool srid_is_null = false;
    int rc = validate_argument_count(kind, argument_count, 1U, 2U, error);

    if (rc != 0) {
        return rc;
    }
    if (arguments[0].is_null || (argument_count == 2U && arguments[1].is_null)) {
        return assign_null_result(out_result);
    }
    if (argument_count == 2U) {
        rc = argument_srid(
            &arguments[1],
            &srid,
            &srid_is_null,
            error,
            mylite_spatial_function_name(kind)
        );
        if (rc != 0) {
            return rc;
        }
        if (srid_is_null) {
            return assign_null_result(out_result);
        }
        if (srid != 0U) {
            return set_spatial_error(
                error,
                mysql_error_srs_not_found,
                "SR001",
                "There's no spatial reference system with SRID %u.",
                srid
            );
        }
    }
    rc = parse_wkt_to_internal(
        (const char *)arguments[0].bytes,
        arguments[0].byte_count,
        mylite_spatial_function_name(kind),
        &bytes,
        &byte_count,
        &actual_type,
        error
    );
    if (rc != 0) {
        free(bytes);
        return rc;
    }
    if (expected_type != MYLITE_SPATIAL_GEOMETRY_NONE && actual_type != expected_type) {
        free(bytes);
        return set_unexpected_geometry_type_error(
            error,
            "WKT value",
            actual_type,
            mylite_spatial_function_name(kind)
        );
    }
    return assign_owned_bytes_result(out_result, MYLITE_SPATIAL_RESULT_GEOMETRY, bytes, byte_count);
}

static int evaluate_from_wkb(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
) {
    enum mylite_spatial_geometry_type actual_type = MYLITE_SPATIAL_GEOMETRY_NONE;
    enum mylite_spatial_geometry_type expected_type = expected_text_wkb_type(kind);
    unsigned char *bytes = NULL;
    size_t byte_count = 0U;
    uint32_t srid = 0U;
    bool srid_is_null = false;
    int rc = validate_argument_count(kind, argument_count, 1U, 2U, error);

    if (rc != 0) {
        return rc;
    }
    if (arguments[0].is_null || (argument_count == 2U && arguments[1].is_null)) {
        return assign_null_result(out_result);
    }
    if (argument_count == 2U) {
        rc = argument_srid(
            &arguments[1],
            &srid,
            &srid_is_null,
            error,
            mylite_spatial_function_name(kind)
        );
        if (rc != 0) {
            return rc;
        }
        if (srid_is_null) {
            return assign_null_result(out_result);
        }
        if (srid != 0U) {
            return set_spatial_error(
                error,
                mysql_error_srs_not_found,
                "SR001",
                "There's no spatial reference system with SRID %u.",
                srid
            );
        }
    }
    rc = validate_wkb(
        (const unsigned char *)arguments[0].bytes,
        arguments[0].byte_count,
        &actual_type,
        error,
        mylite_spatial_function_name(kind)
    );
    if (rc != 0) {
        return rc;
    }
    if (expected_type != MYLITE_SPATIAL_GEOMETRY_NONE && actual_type != expected_type) {
        return set_unexpected_geometry_type_error(
            error,
            "WKB value",
            actual_type,
            mylite_spatial_function_name(kind)
        );
    }
    rc = make_internal_geometry_from_wkb(
        (const unsigned char *)arguments[0].bytes,
        arguments[0].byte_count,
        srid,
        &bytes,
        &byte_count,
        error
    );
    if (rc != 0) {
        free(bytes);
        return rc;
    }
    return assign_owned_bytes_result(out_result, MYLITE_SPATIAL_RESULT_GEOMETRY, bytes, byte_count);
}

static int evaluate_as_text(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
) {
    enum mylite_spatial_geometry_type type = MYLITE_SPATIAL_GEOMETRY_NONE;
    struct spatial_wkb_cursor cursor = {0};
    struct spatial_buffer buffer = {0};
    int rc = validate_argument_count(kind, argument_count, 1U, 1U, error);

    if (rc != 0) {
        return rc;
    }
    if (arguments[0].is_null) {
        return assign_null_result(out_result);
    }
    rc = validate_internal_geometry(
        arguments[0].bytes,
        arguments[0].byte_count,
        &type,
        NULL,
        error,
        mylite_spatial_function_name(kind)
    );
    (void)type;
    if (rc != 0) {
        return rc;
    }
    cursor = (struct spatial_wkb_cursor){
        .bytes = (const unsigned char *)arguments[0].bytes + spatial_internal_srid_size,
        .size = arguments[0].byte_count - spatial_internal_srid_size,
        .offset = 0U,
    };
    rc = append_wkb_as_wkt(&buffer, &cursor, error, mylite_spatial_function_name(kind));
    if (rc == 0) {
        rc = spatial_buffer_append_byte(&buffer, '\0');
    }
    if (rc != 0) {
        spatial_buffer_deinit(&buffer);
        if (error != NULL && error->code == 0) {
            return set_nomem_error(error);
        }
        return rc;
    }
    buffer.size -= 1U;
    return assign_owned_bytes_result(
        out_result,
        MYLITE_SPATIAL_RESULT_TEXT,
        buffer.bytes,
        buffer.size
    );
}

static int evaluate_as_wkb(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
) {
    int rc = validate_argument_count(kind, argument_count, 1U, 1U, error);

    if (rc != 0) {
        return rc;
    }
    if (arguments[0].is_null) {
        return assign_null_result(out_result);
    }
    rc = validate_internal_geometry(
        arguments[0].bytes,
        arguments[0].byte_count,
        NULL,
        NULL,
        error,
        mylite_spatial_function_name(kind)
    );
    if (rc != 0) {
        return rc;
    }
    return assign_copied_bytes_result(
        out_result,
        MYLITE_SPATIAL_RESULT_BLOB,
        (const unsigned char *)arguments[0].bytes + spatial_internal_srid_size,
        arguments[0].byte_count - spatial_internal_srid_size,
        error
    );
}

static int evaluate_geometry_type(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
) {
    enum mylite_spatial_geometry_type type = MYLITE_SPATIAL_GEOMETRY_NONE;
    const char *type_name = NULL;
    int rc = validate_argument_count(kind, argument_count, 1U, 1U, error);

    if (rc != 0) {
        return rc;
    }
    if (arguments[0].is_null) {
        return assign_null_result(out_result);
    }
    rc = validate_internal_geometry(
        arguments[0].bytes,
        arguments[0].byte_count,
        &type,
        NULL,
        error,
        mylite_spatial_function_name(kind)
    );
    if (rc != 0) {
        return rc;
    }
    type_name = type == MYLITE_SPATIAL_GEOMETRY_GEOMETRYCOLLECTION ? "GEOMCOLLECTION"
                                                                   : geometry_type_name(type);
    return assign_copied_text_result(out_result, type_name, strlen(type_name), error);
}

static int evaluate_srid(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
) {
    uint32_t srid = 0U;
    int rc = validate_argument_count(kind, argument_count, 1U, 1U, error);

    if (rc != 0) {
        return rc;
    }
    if (arguments[0].is_null) {
        return assign_null_result(out_result);
    }
    rc = validate_internal_geometry(
        arguments[0].bytes,
        arguments[0].byte_count,
        NULL,
        &srid,
        error,
        mylite_spatial_function_name(kind)
    );
    if (rc != 0) {
        return rc;
    }
    return assign_integer_result(out_result, (int64_t)srid);
}

static int evaluate_point_coordinate(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
) {
    enum mylite_spatial_geometry_type type = MYLITE_SPATIAL_GEOMETRY_NONE;
    double coordinate_x = 0.0;
    double coordinate_y = 0.0;
    int rc = validate_argument_count(kind, argument_count, 1U, 1U, error);

    if (rc != 0) {
        return rc;
    }
    if (arguments[0].is_null) {
        return assign_null_result(out_result);
    }
    rc = validate_internal_geometry(
        arguments[0].bytes,
        arguments[0].byte_count,
        &type,
        NULL,
        error,
        mylite_spatial_function_name(kind)
    );
    if (rc != 0) {
        return rc;
    }
    if (type != MYLITE_SPATIAL_GEOMETRY_POINT) {
        return set_unexpected_geometry_type_error(
            error,
            "POINT value",
            type,
            mylite_spatial_function_name(kind)
        );
    }
    rc = geometry_point_coordinates(
        (const unsigned char *)arguments[0].bytes + spatial_internal_srid_size,
        arguments[0].byte_count - spatial_internal_srid_size,
        &coordinate_x,
        &coordinate_y,
        error,
        mylite_spatial_function_name(kind)
    );
    if (rc != 0) {
        return rc;
    }
    return assign_double_result(
        out_result,
        kind == MYLITE_SPATIAL_FUNCTION_ST_X ? coordinate_x : coordinate_y
    );
}

static int evaluate_point_geographic_coordinate(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
) {
    enum mylite_spatial_geometry_type type = MYLITE_SPATIAL_GEOMETRY_NONE;
    uint32_t srid = 0U;
    double coordinate_x = 0.0;
    double coordinate_y = 0.0;
    int rc = validate_argument_count(kind, argument_count, 1U, 1U, error);

    if (rc != 0) {
        return rc;
    }
    if (arguments[0].is_null) {
        return assign_null_result(out_result);
    }
    rc = validate_internal_geometry(
        arguments[0].bytes,
        arguments[0].byte_count,
        &type,
        &srid,
        error,
        mylite_spatial_function_name(kind)
    );
    if (rc != 0) {
        return rc;
    }
    if (type != MYLITE_SPATIAL_GEOMETRY_POINT) {
        return set_unexpected_geometry_type_error(
            error,
            "POINT value",
            type,
            mylite_spatial_function_name(kind)
        );
    }
    if (srid == 0U) {
        return set_srs_not_geographic_error(error, srid, mylite_spatial_function_name(kind));
    }
    if (srid != spatial_srid_wgs84) {
        return set_spatial_error(
            error,
            mysql_error_srs_not_found,
            "SR001",
            "There's no spatial reference system with SRID %u.",
            srid
        );
    }
    rc = geometry_point_coordinates(
        (const unsigned char *)arguments[0].bytes + spatial_internal_srid_size,
        arguments[0].byte_count - spatial_internal_srid_size,
        &coordinate_x,
        &coordinate_y,
        error,
        mylite_spatial_function_name(kind)
    );
    if (rc != 0) {
        return rc;
    }
    return assign_double_result(
        out_result,
        kind == MYLITE_SPATIAL_FUNCTION_ST_LATITUDE ? coordinate_x : coordinate_y
    );
}

static int evaluate_dimension(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
) {
    struct spatial_geometry_view geometry = {0};
    struct spatial_wkb_cursor cursor = {0};
    bool is_null = false;
    int dimension = -1;
    int rc =
        read_single_geometry_argument(kind, arguments, argument_count, &geometry, &is_null, error);

    if (rc != 0) {
        return rc;
    }
    if (is_null) {
        return assign_null_result(out_result);
    }
    cursor = (struct spatial_wkb_cursor){.bytes = geometry.wkb, .size = geometry.wkb_size};
    rc = wkb_dimension_at(&cursor, &dimension, error, mylite_spatial_function_name(kind));
    if (rc != 0) {
        return rc;
    }
    if (dimension < 0) {
        return assign_null_result(out_result);
    }
    return assign_integer_result(out_result, dimension);
}

static int evaluate_is_empty(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
) {
    struct spatial_geometry_view geometry = {0};
    bool is_null = false;
    int rc =
        read_single_geometry_argument(kind, arguments, argument_count, &geometry, &is_null, error);

    if (rc != 0) {
        return rc;
    }
    if (is_null) {
        return assign_null_result(out_result);
    }
    return assign_integer_result(
        out_result,
        geometry_type_is_empty_collection(&geometry, error, mylite_spatial_function_name(kind)) ? 1
                                                                                                : 0
    );
}

static int evaluate_is_simple(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
) {
    struct spatial_geometry_view geometry = {0};
    struct spatial_distance_geometry decoded = {0};
    bool is_null = false;
    bool is_simple = false;
    int rc =
        read_single_geometry_argument(kind, arguments, argument_count, &geometry, &is_null, error);

    if (rc != 0) {
        return rc;
    }
    if (is_null) {
        return assign_null_result(out_result);
    }
    rc =
        distance_geometry_from_view(&geometry, &decoded, error, mylite_spatial_function_name(kind));
    if (rc != 0) {
        return rc;
    }
    is_simple = simplicity_geometry_is_simple(&decoded);
    distance_geometry_deinit(&decoded);
    return assign_integer_result(out_result, is_simple ? 1 : 0);
}

static int evaluate_is_valid(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
) {
    struct spatial_geometry_view geometry = {0};
    struct spatial_distance_geometry decoded = {0};
    bool is_null = false;
    bool is_valid = false;
    int rc =
        read_single_geometry_argument(kind, arguments, argument_count, &geometry, &is_null, error);

    if (rc != 0) {
        return rc;
    }
    if (is_null) {
        return assign_null_result(out_result);
    }
    rc =
        distance_geometry_from_view(&geometry, &decoded, error, mylite_spatial_function_name(kind));
    if (rc != 0) {
        return rc;
    }
    is_valid = validity_geometry_is_valid(&decoded);
    distance_geometry_deinit(&decoded);
    return assign_integer_result(out_result, is_valid ? 1 : 0);
}

static int evaluate_validate(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
) {
    struct spatial_geometry_view geometry = {0};
    struct spatial_distance_geometry decoded = {0};
    bool is_null = false;
    bool is_valid = false;
    int rc =
        read_single_geometry_argument(kind, arguments, argument_count, &geometry, &is_null, error);

    if (rc != 0) {
        return rc;
    }
    if (is_null) {
        return assign_null_result(out_result);
    }
    rc =
        distance_geometry_from_view(&geometry, &decoded, error, mylite_spatial_function_name(kind));
    if (rc != 0) {
        return rc;
    }
    is_valid = validity_geometry_is_valid(&decoded);
    distance_geometry_deinit(&decoded);
    if (!is_valid) {
        return assign_null_result(out_result);
    }
    return assign_copied_bytes_result(
        out_result,
        MYLITE_SPATIAL_RESULT_GEOMETRY,
        arguments[0].bytes,
        arguments[0].byte_count,
        error
    );
}

static int evaluate_is_closed(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
) {
    struct spatial_geometry_view geometry = {0};
    struct spatial_wkb_cursor cursor = {0};
    bool is_null = false;
    bool little_endian = false;
    enum mylite_spatial_geometry_type type = MYLITE_SPATIAL_GEOMETRY_NONE;
    uint32_t line_count = 1U;
    int rc =
        read_single_geometry_argument(kind, arguments, argument_count, &geometry, &is_null, error);

    if (rc != 0) {
        return rc;
    }
    if (is_null ||
        geometry_type_is_empty_collection(&geometry, error, mylite_spatial_function_name(kind))) {
        return assign_null_result(out_result);
    }
    if (geometry.type != MYLITE_SPATIAL_GEOMETRY_LINESTRING &&
        geometry.type != MYLITE_SPATIAL_GEOMETRY_MULTILINESTRING) {
        return assign_null_result(out_result);
    }
    cursor = (struct spatial_wkb_cursor){.bytes = geometry.wkb, .size = geometry.wkb_size};
    rc = cursor_read_header(
        &cursor,
        &little_endian,
        &type,
        error,
        mylite_spatial_function_name(kind)
    );
    if (rc != 0) {
        return rc;
    }
    if (type == MYLITE_SPATIAL_GEOMETRY_MULTILINESTRING) {
        rc = cursor_read_u32(
            &cursor,
            little_endian,
            &line_count,
            error,
            mylite_spatial_function_name(kind)
        );
    }
    for (uint32_t line_index = 0U; rc == 0 && line_index < line_count; ++line_index) {
        bool line_little_endian = little_endian;
        uint32_t point_count = 0U;
        struct spatial_point first = {0};
        struct spatial_point last = {0};

        if (type == MYLITE_SPATIAL_GEOMETRY_MULTILINESTRING) {
            enum mylite_spatial_geometry_type nested_type = MYLITE_SPATIAL_GEOMETRY_NONE;

            rc = cursor_read_header(
                &cursor,
                &line_little_endian,
                &nested_type,
                error,
                mylite_spatial_function_name(kind)
            );
            if (rc == 0 && nested_type != MYLITE_SPATIAL_GEOMETRY_LINESTRING) {
                rc = set_invalid_gis_data_error(error, mylite_spatial_function_name(kind));
            }
        }
        if (rc == 0) {
            rc = cursor_read_u32(
                &cursor,
                line_little_endian,
                &point_count,
                error,
                mylite_spatial_function_name(kind)
            );
        }
        for (uint32_t point_index = 0U; rc == 0 && point_index < point_count; ++point_index) {
            struct spatial_point point = {0};

            rc = cursor_read_double(
                &cursor,
                line_little_endian,
                &point.coordinate_x,
                error,
                mylite_spatial_function_name(kind)
            );
            if (rc == 0) {
                rc = cursor_read_double(
                    &cursor,
                    line_little_endian,
                    &point.coordinate_y,
                    error,
                    mylite_spatial_function_name(kind)
                );
            }
            if (rc == 0 && point_index == 0U) {
                first = point;
            }
            if (rc == 0) {
                last = point;
            }
        }
        if (rc == 0 && (point_count == 0U || first.coordinate_x != last.coordinate_x ||
                        first.coordinate_y != last.coordinate_y)) {
            return assign_integer_result(out_result, 0);
        }
    }
    if (rc != 0) {
        return rc;
    }
    return assign_integer_result(out_result, 1);
}

static int evaluate_num_geometries(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
) {
    struct spatial_geometry_view geometry = {0};
    struct spatial_wkb_cursor cursor = {0};
    bool is_null = false;
    bool little_endian = false;
    enum mylite_spatial_geometry_type type = MYLITE_SPATIAL_GEOMETRY_NONE;
    uint32_t count = 0U;
    int rc =
        read_single_geometry_argument(kind, arguments, argument_count, &geometry, &is_null, error);

    if (rc != 0) {
        return rc;
    }
    if (is_null || geometry.type != MYLITE_SPATIAL_GEOMETRY_GEOMETRYCOLLECTION) {
        return assign_null_result(out_result);
    }
    cursor = (struct spatial_wkb_cursor){.bytes = geometry.wkb, .size = geometry.wkb_size};
    rc = cursor_read_header(
        &cursor,
        &little_endian,
        &type,
        error,
        mylite_spatial_function_name(kind)
    );
    if (rc == 0) {
        rc = cursor_read_u32(
            &cursor,
            little_endian,
            &count,
            error,
            mylite_spatial_function_name(kind)
        );
    }
    if (rc != 0) {
        return rc;
    }
    return assign_integer_result(out_result, count);
}

static int evaluate_geometry_n(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
) {
    struct spatial_geometry_view geometry = {0};
    struct spatial_wkb_cursor cursor = {0};
    bool is_null = false;
    bool index_is_null = false;
    bool little_endian = false;
    enum mylite_spatial_geometry_type type = MYLITE_SPATIAL_GEOMETRY_NONE;
    uint32_t count = 0U;
    uint32_t requested_index = 0U;
    int rc = validate_argument_count(kind, argument_count, 2U, 2U, error);

    if (rc != 0) {
        return rc;
    }
    rc = read_single_geometry_argument(kind, arguments, 1U, &geometry, &is_null, error);
    if (rc == 0) {
        rc = argument_index(
            &arguments[1],
            &requested_index,
            &index_is_null,
            error,
            mylite_spatial_function_name(kind)
        );
    }
    if (rc != 0) {
        return rc;
    }
    if (is_null || index_is_null || requested_index == 0U ||
        geometry.type != MYLITE_SPATIAL_GEOMETRY_GEOMETRYCOLLECTION) {
        return assign_null_result(out_result);
    }
    cursor = (struct spatial_wkb_cursor){.bytes = geometry.wkb, .size = geometry.wkb_size};
    rc = cursor_read_header(
        &cursor,
        &little_endian,
        &type,
        error,
        mylite_spatial_function_name(kind)
    );
    if (rc == 0) {
        rc = cursor_read_u32(
            &cursor,
            little_endian,
            &count,
            error,
            mylite_spatial_function_name(kind)
        );
    }
    for (uint32_t index = 1U; rc == 0 && index <= count; ++index) {
        size_t start = cursor.offset;
        enum mylite_spatial_geometry_type nested_type = MYLITE_SPATIAL_GEOMETRY_NONE;

        rc = validate_wkb_at(&cursor, &nested_type, error, mylite_spatial_function_name(kind));
        if (rc == 0 && index == requested_index) {
            unsigned char *bytes = NULL;
            size_t byte_count = 0U;

            rc = make_internal_geometry_from_wkb(
                geometry.wkb + start,
                cursor.offset - start,
                geometry.srid,
                &bytes,
                &byte_count,
                error
            );
            if (rc != 0) {
                free(bytes);
                return rc;
            }
            return assign_owned_bytes_result(
                out_result,
                MYLITE_SPATIAL_RESULT_GEOMETRY,
                bytes,
                byte_count
            );
        }
    }
    if (rc != 0) {
        return rc;
    }
    return assign_null_result(out_result);
}

static int evaluate_num_points(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
) {
    struct spatial_geometry_view geometry = {0};
    struct spatial_wkb_cursor cursor = {0};
    bool is_null = false;
    bool little_endian = false;
    enum mylite_spatial_geometry_type type = MYLITE_SPATIAL_GEOMETRY_NONE;
    uint32_t point_count = 0U;
    int rc =
        read_single_geometry_argument(kind, arguments, argument_count, &geometry, &is_null, error);

    if (rc != 0) {
        return rc;
    }
    if (is_null || geometry.type != MYLITE_SPATIAL_GEOMETRY_LINESTRING) {
        return assign_null_result(out_result);
    }
    cursor = (struct spatial_wkb_cursor){.bytes = geometry.wkb, .size = geometry.wkb_size};
    rc = cursor_read_header(
        &cursor,
        &little_endian,
        &type,
        error,
        mylite_spatial_function_name(kind)
    );
    if (rc == 0) {
        rc = cursor_read_u32(
            &cursor,
            little_endian,
            &point_count,
            error,
            mylite_spatial_function_name(kind)
        );
    }
    if (rc != 0) {
        return rc;
    }
    return assign_integer_result(out_result, point_count);
}

static int evaluate_line_point_accessor(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
) {
    struct spatial_geometry_view geometry = {0};
    struct spatial_point point = {0};
    bool is_null = false;
    bool index_is_null = false;
    bool found = false;
    uint32_t point_index = 1U;
    unsigned char *bytes = NULL;
    size_t byte_count = 0U;
    int rc = 0;

    if (kind == MYLITE_SPATIAL_FUNCTION_ST_POINTN) {
        rc = validate_argument_count(kind, argument_count, 2U, 2U, error);
    } else {
        rc = validate_argument_count(kind, argument_count, 1U, 1U, error);
    }
    if (rc != 0) {
        return rc;
    }
    rc = read_single_geometry_argument(kind, arguments, 1U, &geometry, &is_null, error);
    if (rc == 0 && kind == MYLITE_SPATIAL_FUNCTION_ST_POINTN) {
        rc = argument_index(
            &arguments[1],
            &point_index,
            &index_is_null,
            error,
            mylite_spatial_function_name(kind)
        );
    }
    if (rc != 0) {
        return rc;
    }
    if (is_null || index_is_null || point_index == 0U ||
        geometry.type != MYLITE_SPATIAL_GEOMETRY_LINESTRING) {
        return assign_null_result(out_result);
    }
    if (kind == MYLITE_SPATIAL_FUNCTION_ST_ENDPOINT) {
        point_index = UINT32_MAX;
    }
    rc = line_point_from_wkb(
        &geometry,
        point_index,
        &point,
        &found,
        error,
        mylite_spatial_function_name(kind)
    );
    if (rc != 0) {
        return rc;
    }
    if (!found) {
        return assign_null_result(out_result);
    }
    rc = make_point_internal_geometry(
        point.coordinate_x,
        point.coordinate_y,
        &bytes,
        &byte_count,
        error
    );
    if (rc != 0) {
        free(bytes);
        return rc;
    }
    return assign_owned_bytes_result(out_result, MYLITE_SPATIAL_RESULT_GEOMETRY, bytes, byte_count);
}

static int evaluate_polygon_ring_accessor(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
) {
    struct spatial_geometry_view geometry = {0};
    struct spatial_wkb_cursor cursor = {0};
    bool is_null = false;
    bool index_is_null = false;
    bool little_endian = false;
    bool found = false;
    enum mylite_spatial_geometry_type type = MYLITE_SPATIAL_GEOMETRY_NONE;
    uint32_t ring_count = 0U;
    uint32_t ring_index = 0U;
    unsigned char *bytes = NULL;
    size_t byte_count = 0U;
    int rc = 0;

    if (kind == MYLITE_SPATIAL_FUNCTION_ST_INTERIORRINGN) {
        rc = validate_argument_count(kind, argument_count, 2U, 2U, error);
    } else {
        rc = validate_argument_count(kind, argument_count, 1U, 1U, error);
    }
    if (rc != 0) {
        return rc;
    }
    rc = read_single_geometry_argument(kind, arguments, 1U, &geometry, &is_null, error);
    if (rc == 0 && kind == MYLITE_SPATIAL_FUNCTION_ST_INTERIORRINGN) {
        rc = argument_index(
            &arguments[1],
            &ring_index,
            &index_is_null,
            error,
            mylite_spatial_function_name(kind)
        );
    }
    if (rc != 0) {
        return rc;
    }
    if (is_null || index_is_null || geometry.type != MYLITE_SPATIAL_GEOMETRY_POLYGON) {
        return assign_null_result(out_result);
    }
    cursor = (struct spatial_wkb_cursor){.bytes = geometry.wkb, .size = geometry.wkb_size};
    rc = cursor_read_header(
        &cursor,
        &little_endian,
        &type,
        error,
        mylite_spatial_function_name(kind)
    );
    if (rc == 0) {
        rc = cursor_read_u32(
            &cursor,
            little_endian,
            &ring_count,
            error,
            mylite_spatial_function_name(kind)
        );
    }
    if (rc != 0) {
        return rc;
    }
    if (kind == MYLITE_SPATIAL_FUNCTION_ST_NUMINTERIORRING ||
        kind == MYLITE_SPATIAL_FUNCTION_ST_NUMINTERIORRINGS) {
        return assign_integer_result(out_result, ring_count == 0U ? 0 : (int64_t)ring_count - 1);
    }
    if (kind == MYLITE_SPATIAL_FUNCTION_ST_EXTERIORRING) {
        ring_index = 0U;
    }
    if (kind == MYLITE_SPATIAL_FUNCTION_ST_INTERIORRINGN && ring_index == 0U) {
        return assign_null_result(out_result);
    }
    rc = polygon_ring_from_wkb(
        &geometry,
        ring_index,
        &bytes,
        &byte_count,
        &found,
        error,
        mylite_spatial_function_name(kind)
    );
    if (rc != 0) {
        free(bytes);
        return rc;
    }
    if (!found) {
        return assign_null_result(out_result);
    }
    return assign_owned_bytes_result(out_result, MYLITE_SPATIAL_RESULT_GEOMETRY, bytes, byte_count);
}

static int evaluate_length(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
) {
    struct spatial_geometry_view geometry = {0};
    struct spatial_wkb_cursor cursor = {0};
    bool is_null = false;
    bool supported = false;
    double length = 0.0;
    int rc =
        read_single_geometry_argument(kind, arguments, argument_count, &geometry, &is_null, error);

    if (rc != 0) {
        return rc;
    }
    if (is_null ||
        geometry_type_is_empty_collection(&geometry, error, mylite_spatial_function_name(kind))) {
        return assign_null_result(out_result);
    }
    cursor = (struct spatial_wkb_cursor){.bytes = geometry.wkb, .size = geometry.wkb_size};
    rc = wkb_length_at(&cursor, &length, &supported, error, mylite_spatial_function_name(kind));
    if (rc != 0) {
        return rc;
    }
    if (!supported) {
        return assign_null_result(out_result);
    }
    return assign_double_result(out_result, length);
}

static int evaluate_area(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
) {
    struct spatial_geometry_view geometry = {0};
    struct spatial_wkb_cursor cursor = {0};
    bool is_null = false;
    bool supported = false;
    double area = 0.0;
    int rc =
        read_single_geometry_argument(kind, arguments, argument_count, &geometry, &is_null, error);

    if (rc != 0) {
        return rc;
    }
    if (is_null ||
        geometry_type_is_empty_collection(&geometry, error, mylite_spatial_function_name(kind))) {
        return assign_null_result(out_result);
    }
    cursor = (struct spatial_wkb_cursor){.bytes = geometry.wkb, .size = geometry.wkb_size};
    rc = wkb_area_at(&cursor, &area, &supported, error, mylite_spatial_function_name(kind));
    if (rc != 0) {
        return rc;
    }
    if (!supported) {
        return set_unexpected_geometry_type_error(
            error,
            "POLYGON/MULTIPOLYGON value",
            geometry.type,
            mylite_spatial_function_name(kind)
        );
    }
    return assign_double_result(out_result, area);
}

static int evaluate_centroid(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
) {
    struct spatial_geometry_view geometry = {0};
    struct spatial_distance_geometry distance_geometry = {0};
    struct spatial_centroid_accumulator accumulator = {
        .dimension = SPATIAL_CENTROID_DIMENSION_NONE,
    };
    struct spatial_point centroid = {0};
    bool is_null = false;
    unsigned char *bytes = NULL;
    size_t byte_count = 0U;
    int rc =
        read_single_geometry_argument(kind, arguments, argument_count, &geometry, &is_null, error);

    if (rc != 0) {
        return rc;
    }
    if (is_null) {
        return assign_null_result(out_result);
    }
    if (geometry.srid != 0U) {
        return set_not_implemented_for_geographic_srs_geometry_error(
            error,
            geometry.type,
            mylite_spatial_function_name(kind)
        );
    }
    rc = distance_geometry_from_view(
        &geometry,
        &distance_geometry,
        error,
        mylite_spatial_function_name(kind)
    );
    if (rc == 0) {
        rc = centroid_accumulate_geometry(
            &distance_geometry,
            &accumulator,
            error,
            mylite_spatial_function_name(kind)
        );
    }
    distance_geometry_deinit(&distance_geometry);
    if (rc != 0) {
        return rc;
    }
    if (accumulator.dimension == SPATIAL_CENTROID_DIMENSION_NONE) {
        return assign_null_result(out_result);
    }
    if (accumulator.weight > 0.0) {
        centroid = (struct spatial_point){
            .coordinate_x = accumulator.weighted_x / accumulator.weight,
            .coordinate_y = accumulator.weighted_y / accumulator.weight,
        };
    } else if (accumulator.has_fallback_point) {
        centroid = accumulator.fallback_point;
    } else {
        return assign_null_result(out_result);
    }
    if (!isfinite(centroid.coordinate_x) || !isfinite(centroid.coordinate_y)) {
        return set_invalid_gis_data_error(error, mylite_spatial_function_name(kind));
    }
    rc = make_point_internal_geometry(
        centroid.coordinate_x,
        centroid.coordinate_y,
        &bytes,
        &byte_count,
        error
    );
    if (rc != 0) {
        free(bytes);
        return rc;
    }
    return assign_owned_bytes_result(out_result, MYLITE_SPATIAL_RESULT_GEOMETRY, bytes, byte_count);
}

static int evaluate_convex_hull(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
) {
    const char *function_name = mylite_spatial_function_name(kind);
    struct spatial_geometry_view geometry = {0};
    struct spatial_distance_geometry distance_geometry = {0};
    struct spatial_discrete_point_set points = {0};
    struct spatial_discrete_point_set hull = {0};
    bool is_null = false;
    unsigned char *bytes = NULL;
    size_t byte_count = 0U;
    int rc =
        read_single_geometry_argument(kind, arguments, argument_count, &geometry, &is_null, error);

    if (rc != 0) {
        return rc;
    }
    if (is_null) {
        return assign_null_result(out_result);
    }
    if (geometry.srid != 0U) {
        return set_not_implemented_for_geographic_srs_geometry_error(
            error,
            geometry.type,
            function_name
        );
    }
    rc = distance_geometry_from_view(&geometry, &distance_geometry, error, function_name);
    if (rc == 0) {
        rc = convex_hull_validate_geometry_rings(&distance_geometry, error, function_name);
    }
    if (rc == 0) {
        rc = convex_hull_point_set_from_geometry(&distance_geometry, &points, error, function_name);
    }
    distance_geometry_deinit(&distance_geometry);
    if (rc == 0 && points.point_count == 0U) {
        discrete_point_set_deinit(&points);
        return assign_null_result(out_result);
    }
    if (rc == 0) {
        rc = convex_hull_build(&points, &hull, error, function_name);
    }
    discrete_point_set_deinit(&points);
    if (rc == 0 && hull.point_count == 1U) {
        rc = make_point_internal_geometry(
            hull.points[0].coordinate_x,
            hull.points[0].coordinate_y,
            &bytes,
            &byte_count,
            error
        );
    } else if (rc == 0 && hull.point_count == 2U) {
        rc = make_linestring_internal_geometry(
            hull.points,
            hull.point_count,
            &bytes,
            &byte_count,
            error
        );
    } else if (rc == 0) {
        rc = make_polygon_internal_geometry_with_srid(
            0U,
            hull.points,
            hull.point_count,
            &bytes,
            &byte_count,
            error
        );
    }
    discrete_point_set_deinit(&hull);
    if (rc != 0) {
        free(bytes);
        return rc;
    }
    return assign_owned_bytes_result(out_result, MYLITE_SPATIAL_RESULT_GEOMETRY, bytes, byte_count);
}

static int evaluate_simplify(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
) {
    const char *function_name = mylite_spatial_function_name(kind);
    struct spatial_geometry_view geometry = {0};
    struct spatial_distance_geometry source_geometry = {0};
    struct spatial_distance_geometry simplified_geometry = {0};
    bool is_null = false;
    bool has_geometry = false;
    double max_distance = 0.0;
    unsigned char *bytes = NULL;
    size_t byte_count = 0U;
    int rc = validate_argument_count(kind, argument_count, 2U, 2U, error);

    if (rc != 0) {
        return rc;
    }
    if (any_argument_is_null(arguments, argument_count)) {
        return assign_null_result(out_result);
    }
    rc = read_single_geometry_argument(kind, arguments, 1U, &geometry, &is_null, error);
    if (rc == 0) {
        rc = argument_simplify_distance(&arguments[1], &max_distance, error, function_name);
    }
    if (rc != 0) {
        return rc;
    }
    if (is_null) {
        return assign_null_result(out_result);
    }
    if (max_distance <= 0.0 || !isfinite(max_distance)) {
        return set_wrong_arguments_error(error, function_name);
    }
    if (geometry.srid != 0U) {
        return set_not_implemented_for_geographic_srs_geometry_argument_error(
            error,
            geometry.type,
            function_name
        );
    }
    rc = distance_geometry_from_view(&geometry, &source_geometry, error, function_name);
    if (rc == 0) {
        rc = simplify_geometry(
            &source_geometry,
            max_distance,
            &simplified_geometry,
            &has_geometry,
            error,
            function_name
        );
    }
    distance_geometry_deinit(&source_geometry);
    if (rc != 0) {
        distance_geometry_deinit(&simplified_geometry);
        return rc;
    }
    if (!has_geometry) {
        distance_geometry_deinit(&simplified_geometry);
        return assign_null_result(out_result);
    }
    rc = make_internal_geometry_from_distance_geometry(
        geometry.srid,
        &simplified_geometry,
        &bytes,
        &byte_count,
        error
    );
    distance_geometry_deinit(&simplified_geometry);
    if (rc != 0) {
        free(bytes);
        return rc;
    }
    return assign_owned_bytes_result(out_result, MYLITE_SPATIAL_RESULT_GEOMETRY, bytes, byte_count);
}

static int evaluate_distance(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
) {
    struct spatial_geometry_view left = {0};
    struct spatial_geometry_view right = {0};
    struct spatial_distance_geometry left_geometry = {0};
    struct spatial_distance_geometry right_geometry = {0};
    bool left_is_null = false;
    bool right_is_null = false;
    bool has_distance = false;
    double distance = 0.0;
    int rc = validate_argument_count(kind, argument_count, 2U, 3U, error);

    if (rc != 0) {
        return rc;
    }
    if (any_argument_is_null(arguments, argument_count)) {
        return assign_null_result(out_result);
    }
    rc = read_single_geometry_argument(kind, arguments, 1U, &left, &left_is_null, error);
    if (rc == 0) {
        rc = read_single_geometry_argument(kind, arguments + 1U, 1U, &right, &right_is_null, error);
    }
    if (rc != 0) {
        return rc;
    }
    if (left_is_null || right_is_null) {
        return assign_null_result(out_result);
    }
    if (left.srid != right.srid) {
        return set_gis_different_srids_error(
            error,
            left.srid,
            right.srid,
            mylite_spatial_function_name(kind)
        );
    }
    if (left.srid != 0U) {
        return set_not_implemented_for_geographic_srs_error(
            error,
            mylite_spatial_function_name(kind)
        );
    }
    if (argument_count == 3U) {
        return set_unknown_length_unit_error(
            error,
            &arguments[2],
            mylite_spatial_function_name(kind)
        );
    }
    rc = distance_geometry_from_view(
        &left,
        &left_geometry,
        error,
        mylite_spatial_function_name(kind)
    );
    if (rc == 0) {
        rc = distance_geometry_from_view(
            &right,
            &right_geometry,
            error,
            mylite_spatial_function_name(kind)
        );
    }
    if (rc == 0) {
        rc = distance_between_geometries(&left_geometry, &right_geometry, &distance, &has_distance);
    }
    distance_geometry_deinit(&left_geometry);
    distance_geometry_deinit(&right_geometry);
    if (rc != 0) {
        return rc;
    }
    if (!has_distance) {
        return assign_null_result(out_result);
    }
    return assign_double_result(out_result, distance);
}

static int evaluate_relation_predicate(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
) {
    struct spatial_geometry_view left = {0};
    struct spatial_geometry_view right = {0};
    struct spatial_distance_geometry left_geometry = {0};
    struct spatial_distance_geometry right_geometry = {0};
    bool is_null = false;
    bool has_distance = false;
    bool intersects = false;
    bool contains = false;
    bool touches = false;
    double distance = 0.0;
    int rc = read_two_geometry_arguments(
        kind,
        arguments,
        argument_count,
        &left,
        &right,
        &is_null,
        error
    );

    if (rc != 0) {
        return rc;
    }
    if (is_null) {
        return assign_null_result(out_result);
    }
    if (left.srid != right.srid) {
        return set_gis_different_srids_error(
            error,
            left.srid,
            right.srid,
            mylite_spatial_function_name(kind)
        );
    }
    if (left.srid != 0U) {
        return set_not_implemented_for_geographic_srs_error(
            error,
            mylite_spatial_function_name(kind)
        );
    }
    rc = distance_geometry_from_view(
        &left,
        &left_geometry,
        error,
        mylite_spatial_function_name(kind)
    );
    if (rc == 0) {
        rc = distance_geometry_from_view(
            &right,
            &right_geometry,
            error,
            mylite_spatial_function_name(kind)
        );
    }
    if (rc == 0 && kind == MYLITE_SPATIAL_FUNCTION_ST_EQUALS) {
        bool left_is_empty = distance_geometry_is_empty(&left_geometry);
        bool right_is_empty = distance_geometry_is_empty(&right_geometry);
        bool equals = false;

        if (left_is_empty || right_is_empty) {
            equals = left_is_empty && right_is_empty;
        } else {
            equals = relation_geometry_contains(&left_geometry, &right_geometry) &&
                     relation_geometry_contains(&right_geometry, &left_geometry);
        }
        distance_geometry_deinit(&left_geometry);
        distance_geometry_deinit(&right_geometry);
        return assign_integer_result(out_result, equals ? 1 : 0);
    }
    if (rc == 0 && (distance_geometry_is_empty(&left_geometry) ||
                    distance_geometry_is_empty(&right_geometry))) {
        distance_geometry_deinit(&left_geometry);
        distance_geometry_deinit(&right_geometry);
        return assign_null_result(out_result);
    }
    if (rc == 0 && kind == MYLITE_SPATIAL_FUNCTION_ST_CROSSES) {
        int left_dimension = relation_geometry_dimension(&left_geometry);
        int right_dimension = relation_geometry_dimension(&right_geometry);
        bool crosses = false;

        if (left_dimension == 2 || right_dimension == 0) {
            distance_geometry_deinit(&left_geometry);
            distance_geometry_deinit(&right_geometry);
            return assign_null_result(out_result);
        }
        crosses = relation_geometry_crosses(
            &left_geometry,
            &right_geometry,
            left_dimension,
            right_dimension
        );
        distance_geometry_deinit(&left_geometry);
        distance_geometry_deinit(&right_geometry);
        return assign_integer_result(out_result, crosses ? 1 : 0);
    }
    if (rc == 0 && kind == MYLITE_SPATIAL_FUNCTION_ST_TOUCHES) {
        if (relation_geometry_dimension(&left_geometry) == 0 &&
            relation_geometry_dimension(&right_geometry) == 0) {
            distance_geometry_deinit(&left_geometry);
            distance_geometry_deinit(&right_geometry);
            return assign_null_result(out_result);
        }
        rc = distance_between_geometries(&left_geometry, &right_geometry, &distance, &has_distance);
        if (rc == 0 && has_distance && double_near_zero(distance)) {
            touches = !relation_geometry_interiors_intersect(&left_geometry, &right_geometry);
        }
        distance_geometry_deinit(&left_geometry);
        distance_geometry_deinit(&right_geometry);
        if (rc != 0) {
            return rc;
        }
        if (!has_distance) {
            return assign_null_result(out_result);
        }
        return assign_integer_result(out_result, touches ? 1 : 0);
    }
    if (rc == 0 && kind == MYLITE_SPATIAL_FUNCTION_ST_OVERLAPS) {
        int left_dimension = relation_geometry_dimension(&left_geometry);
        int right_dimension = relation_geometry_dimension(&right_geometry);
        bool overlaps = false;

        if (left_dimension != right_dimension) {
            distance_geometry_deinit(&left_geometry);
            distance_geometry_deinit(&right_geometry);
            return assign_null_result(out_result);
        }
        overlaps = relation_geometry_overlaps(&left_geometry, &right_geometry, left_dimension);
        distance_geometry_deinit(&left_geometry);
        distance_geometry_deinit(&right_geometry);
        return assign_integer_result(out_result, overlaps ? 1 : 0);
    }
    if (rc == 0 && kind == MYLITE_SPATIAL_FUNCTION_ST_CONTAINS) {
        contains = relation_geometry_contains(&left_geometry, &right_geometry);
        distance_geometry_deinit(&left_geometry);
        distance_geometry_deinit(&right_geometry);
        return assign_integer_result(out_result, contains ? 1 : 0);
    }
    if (rc == 0 && kind == MYLITE_SPATIAL_FUNCTION_ST_WITHIN) {
        contains = relation_geometry_contains(&right_geometry, &left_geometry);
        distance_geometry_deinit(&left_geometry);
        distance_geometry_deinit(&right_geometry);
        return assign_integer_result(out_result, contains ? 1 : 0);
    }
    if (rc == 0) {
        rc = distance_between_geometries(&left_geometry, &right_geometry, &distance, &has_distance);
    }
    distance_geometry_deinit(&left_geometry);
    distance_geometry_deinit(&right_geometry);
    if (rc != 0) {
        return rc;
    }
    if (!has_distance) {
        return assign_null_result(out_result);
    }
    intersects = double_near_zero(distance);
    if (kind == MYLITE_SPATIAL_FUNCTION_ST_INTERSECTS) {
        return assign_integer_result(out_result, intersects ? 1 : 0);
    }
    return assign_integer_result(out_result, intersects ? 0 : 1);
}

static int evaluate_distance_sphere(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
) {
    struct spatial_geometry_view left = {0};
    struct spatial_geometry_view right = {0};
    struct spatial_point_collection left_points = {0};
    struct spatial_point_collection right_points = {0};
    bool left_is_null = false;
    bool right_is_null = false;
    bool has_distance = false;
    double radius = spatial_distance_sphere_default_radius;
    double distance = 0.0;
    int rc = validate_argument_count(kind, argument_count, 2U, 3U, error);

    if (rc != 0) {
        return rc;
    }
    if (any_argument_is_null(arguments, argument_count)) {
        return assign_null_result(out_result);
    }
    rc = read_single_geometry_argument(kind, arguments, 1U, &left, &left_is_null, error);
    if (rc == 0) {
        rc = read_single_geometry_argument(kind, arguments + 1U, 1U, &right, &right_is_null, error);
    }
    if (rc == 0 && argument_count == 3U) {
        rc = argument_distance(&arguments[2], &radius, error, mylite_spatial_function_name(kind));
    }
    if (rc != 0) {
        return rc;
    }
    if (left_is_null || right_is_null) {
        return assign_null_result(out_result);
    }
    if (left.srid != right.srid) {
        return set_gis_different_srids_error(
            error,
            left.srid,
            right.srid,
            mylite_spatial_function_name(kind)
        );
    }
    if (left.srid != 0U) {
        return set_not_implemented_for_geographic_srs_error(
            error,
            mylite_spatial_function_name(kind)
        );
    }
    if (radius <= 0.0 || !isfinite(radius)) {
        return set_nonpositive_radius_error(error, mylite_spatial_function_name(kind));
    }
    if ((left.type != MYLITE_SPATIAL_GEOMETRY_POINT &&
         left.type != MYLITE_SPATIAL_GEOMETRY_MULTIPOINT) ||
        (right.type != MYLITE_SPATIAL_GEOMETRY_POINT &&
         right.type != MYLITE_SPATIAL_GEOMETRY_MULTIPOINT)) {
        return set_not_implemented_for_cartesian_srs_error(
            error,
            left.type,
            right.type,
            mylite_spatial_function_name(kind)
        );
    }
    rc = point_collection_from_geometry(
        &left,
        &left_points,
        error,
        mylite_spatial_function_name(kind)
    );
    if (rc == 0) {
        rc = point_collection_from_geometry(
            &right,
            &right_points,
            error,
            mylite_spatial_function_name(kind)
        );
    }
    if (rc == 0) {
        rc = validate_distance_sphere_coordinates(
            &left_points,
            error,
            mylite_spatial_function_name(kind)
        );
    }
    if (rc == 0) {
        rc = validate_distance_sphere_coordinates(
            &right_points,
            error,
            mylite_spatial_function_name(kind)
        );
    }
    if (rc == 0) {
        rc = distance_sphere_between_collections(
            &left_points,
            &right_points,
            radius,
            &distance,
            &has_distance
        );
    }
    point_collection_deinit(&left_points);
    point_collection_deinit(&right_points);
    if (rc != 0) {
        return rc;
    }
    if (!has_distance) {
        return assign_null_result(out_result);
    }
    if (!isfinite(distance)) {
        return set_distance_range_error(error, mylite_spatial_function_name(kind));
    }
    return assign_double_result(out_result, distance);
}

static int evaluate_discrete_distance(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
) {
    const char *function_name = mylite_spatial_function_name(kind);
    struct spatial_geometry_view left = {0};
    struct spatial_geometry_view right = {0};
    struct spatial_distance_geometry left_geometry = {0};
    struct spatial_distance_geometry right_geometry = {0};
    bool left_is_null = false;
    bool right_is_null = false;
    bool has_distance = false;
    double distance = 0.0;
    int rc = validate_argument_count(kind, argument_count, 2U, 3U, error);

    if (rc != 0) {
        return rc;
    }
    if (any_argument_is_null(arguments, argument_count)) {
        return assign_null_result(out_result);
    }
    rc = read_single_geometry_argument(kind, arguments, 1U, &left, &left_is_null, error);
    if (rc == 0) {
        rc = read_single_geometry_argument(kind, arguments + 1U, 1U, &right, &right_is_null, error);
    }
    if (rc != 0) {
        return rc;
    }
    if (left_is_null || right_is_null) {
        return assign_null_result(out_result);
    }
    if (left.srid != right.srid) {
        return set_gis_different_srids_error(error, left.srid, right.srid, function_name);
    }
    if (left.srid != 0U) {
        return set_not_implemented_for_geographic_srs_error(error, function_name);
    }
    if (argument_count == 3U) {
        return set_unknown_length_unit_error(error, &arguments[2], function_name);
    }
    rc = distance_geometry_from_view(&left, &left_geometry, error, function_name);
    if (rc == 0) {
        rc = distance_geometry_from_view(&right, &right_geometry, error, function_name);
    }
    if (rc == 0 && (distance_geometry_is_empty(&left_geometry) ||
                    distance_geometry_is_empty(&right_geometry))) {
        has_distance = false;
    } else if (rc == 0 && kind == MYLITE_SPATIAL_FUNCTION_ST_FRECHETDISTANCE) {
        if (!frechet_distance_supports_types(left_geometry.type, right_geometry.type)) {
            rc = set_not_implemented_for_cartesian_srs_error(
                error,
                left_geometry.type,
                right_geometry.type,
                function_name
            );
        } else if (left_geometry.point_count == 0U || right_geometry.point_count == 0U) {
            has_distance = false;
        } else {
            rc = frechet_distance_between_lines(&left_geometry, &right_geometry, &distance, error);
            has_distance = rc == 0;
        }
    } else if (rc == 0) {
        if (!hausdorff_distance_supports_types(left_geometry.type, right_geometry.type)) {
            rc = set_not_implemented_for_cartesian_srs_error(
                error,
                left_geometry.type,
                right_geometry.type,
                function_name
            );
        }
        if (rc == 0) {
            rc = hausdorff_distance_between_supported_geometries(
                &left_geometry,
                &right_geometry,
                &distance,
                error,
                function_name
            );
            has_distance = rc == 0;
        }
    }
    distance_geometry_deinit(&left_geometry);
    distance_geometry_deinit(&right_geometry);
    if (rc != 0) {
        return rc;
    }
    if (!has_distance) {
        return assign_null_result(out_result);
    }
    if (!isfinite(distance)) {
        return set_invalid_gis_data_error(error, function_name);
    }
    return assign_double_result(out_result, distance);
}

static int evaluate_line_interpolation(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
) {
    struct spatial_geometry_view geometry = {0};
    struct spatial_point *line_points = NULL;
    struct spatial_point *result_points = NULL;
    struct spatial_point point = {0};
    bool is_null = false;
    uint32_t line_point_count = 0U;
    uint32_t result_point_count = 0U;
    double distance_value = 0.0;
    double total_length = 0.0;
    unsigned char *bytes = NULL;
    size_t byte_count = 0U;
    int rc = validate_argument_count(kind, argument_count, 2U, 2U, error);

    if (rc != 0) {
        return rc;
    }
    if (any_argument_is_null(arguments, argument_count)) {
        return assign_null_result(out_result);
    }
    rc = read_single_geometry_argument(kind, arguments, 1U, &geometry, &is_null, error);
    if (rc == 0) {
        rc = argument_distance(
            &arguments[1],
            &distance_value,
            error,
            mylite_spatial_function_name(kind)
        );
    }
    if (rc != 0) {
        return rc;
    }
    if (is_null) {
        return assign_null_result(out_result);
    }
    if (geometry.type != MYLITE_SPATIAL_GEOMETRY_LINESTRING) {
        return set_unexpected_geometry_type_error(
            error,
            "LINESTRING value",
            geometry.type,
            mylite_spatial_function_name(kind)
        );
    }
    rc = line_points_from_wkb(
        &geometry,
        &line_points,
        &line_point_count,
        error,
        mylite_spatial_function_name(kind)
    );
    if (rc != 0) {
        free(line_points);
        return rc;
    }
    total_length = line_points_length(line_points, line_point_count);
    if (kind == MYLITE_SPATIAL_FUNCTION_ST_POINTATDISTANCE) {
        if (distance_value < 0.0 || distance_value > total_length) {
            free(line_points);
            return set_distance_range_error(error, mylite_spatial_function_name(kind));
        }
        point = line_point_at_distance(line_points, line_point_count, distance_value);
        rc = make_point_internal_geometry_with_srid(
            geometry.srid,
            point.coordinate_x,
            point.coordinate_y,
            &bytes,
            &byte_count,
            error
        );
    } else if (distance_value < 0.0 || distance_value > 1.0) {
        free(line_points);
        return set_distance_range_error(error, mylite_spatial_function_name(kind));
    } else if (kind == MYLITE_SPATIAL_FUNCTION_ST_LINEINTERPOLATEPOINT) {
        point =
            line_point_at_distance(line_points, line_point_count, total_length * distance_value);
        rc = make_point_internal_geometry_with_srid(
            geometry.srid,
            point.coordinate_x,
            point.coordinate_y,
            &bytes,
            &byte_count,
            error
        );
    } else {
        rc = interpolated_line_points(
            line_points,
            line_point_count,
            distance_value,
            &result_points,
            &result_point_count,
            error,
            mylite_spatial_function_name(kind)
        );
        if (rc == 0) {
            rc = make_multipoint_internal_geometry_with_srid(
                geometry.srid,
                result_points,
                result_point_count,
                &bytes,
                &byte_count,
                error
            );
        }
    }
    free(result_points);
    free(line_points);
    if (rc != 0) {
        free(bytes);
        return rc;
    }
    return assign_owned_bytes_result(out_result, MYLITE_SPATIAL_RESULT_GEOMETRY, bytes, byte_count);
}

static int evaluate_envelope(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
) {
    struct spatial_geometry_view geometry = {0};
    struct spatial_wkb_cursor cursor = {0};
    struct spatial_box box = {0};
    bool is_null = false;
    unsigned char *bytes = NULL;
    size_t byte_count = 0U;
    int rc =
        read_single_geometry_argument(kind, arguments, argument_count, &geometry, &is_null, error);

    if (rc != 0) {
        return rc;
    }
    if (is_null) {
        return assign_null_result(out_result);
    }
    cursor = (struct spatial_wkb_cursor){.bytes = geometry.wkb, .size = geometry.wkb_size};
    rc = wkb_bounds_at(&cursor, &box, error, mylite_spatial_function_name(kind));
    if (rc == 0) {
        rc = make_envelope_internal_geometry(&box, &bytes, &byte_count, error);
    }
    if (rc != 0) {
        free(bytes);
        return rc;
    }
    return assign_owned_bytes_result(out_result, MYLITE_SPATIAL_RESULT_GEOMETRY, bytes, byte_count);
}

static int evaluate_swap_xy(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
) {
    struct spatial_geometry_view geometry = {0};
    struct spatial_wkb_cursor cursor = {0};
    struct spatial_buffer wkb = {0};
    unsigned char *bytes = NULL;
    size_t byte_count = 0U;
    bool is_null = false;
    int rc =
        read_single_geometry_argument(kind, arguments, argument_count, &geometry, &is_null, error);

    if (rc != 0) {
        return rc;
    }
    if (is_null) {
        return assign_null_result(out_result);
    }
    cursor = (struct spatial_wkb_cursor){.bytes = geometry.wkb, .size = geometry.wkb_size};
    rc = wkb_swap_xy_at(&cursor, &wkb, error, mylite_spatial_function_name(kind));
    if (rc == 0) {
        rc = make_internal_geometry_from_wkb(
            wkb.bytes,
            wkb.size,
            geometry.srid,
            &bytes,
            &byte_count,
            error
        );
    }
    spatial_buffer_deinit(&wkb);
    if (rc != 0) {
        free(bytes);
        return rc;
    }
    return assign_owned_bytes_result(out_result, MYLITE_SPATIAL_RESULT_GEOMETRY, bytes, byte_count);
}

static int evaluate_make_envelope(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
) {
    struct spatial_geometry_view left = {0};
    struct spatial_geometry_view right = {0};
    struct spatial_point left_point = {0};
    struct spatial_point right_point = {0};
    struct spatial_box box = {0};
    bool is_null = false;
    unsigned char *bytes = NULL;
    size_t byte_count = 0U;
    int rc = read_two_geometry_arguments(
        kind,
        arguments,
        argument_count,
        &left,
        &right,
        &is_null,
        error
    );

    if (rc != 0) {
        return rc;
    }
    if (is_null) {
        return assign_null_result(out_result);
    }
    if (left.type != MYLITE_SPATIAL_GEOMETRY_POINT || right.type != MYLITE_SPATIAL_GEOMETRY_POINT) {
        return set_wrong_arguments_error(error, mylite_spatial_function_name(kind));
    }
    rc = geometry_point_coordinates(
        left.wkb,
        left.wkb_size,
        &left_point.coordinate_x,
        &left_point.coordinate_y,
        error,
        mylite_spatial_function_name(kind)
    );
    if (rc == 0) {
        rc = geometry_point_coordinates(
            right.wkb,
            right.wkb_size,
            &right_point.coordinate_x,
            &right_point.coordinate_y,
            error,
            mylite_spatial_function_name(kind)
        );
    }
    if (rc == 0) {
        spatial_box_include_point(&box, left_point.coordinate_x, left_point.coordinate_y);
        spatial_box_include_point(&box, right_point.coordinate_x, right_point.coordinate_y);
        rc = make_envelope_internal_geometry(&box, &bytes, &byte_count, error);
    }
    if (rc != 0) {
        free(bytes);
        return rc;
    }
    return assign_owned_bytes_result(out_result, MYLITE_SPATIAL_RESULT_GEOMETRY, bytes, byte_count);
}

static int evaluate_mbr_predicate(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
) {
    struct spatial_geometry_view left = {0};
    struct spatial_geometry_view right = {0};
    struct spatial_wkb_cursor left_cursor = {0};
    struct spatial_wkb_cursor right_cursor = {0};
    struct spatial_box left_box = {0};
    struct spatial_box right_box = {0};
    bool is_null = false;
    bool result = false;
    int rc = read_two_geometry_arguments(
        kind,
        arguments,
        argument_count,
        &left,
        &right,
        &is_null,
        error
    );

    if (rc != 0) {
        return rc;
    }
    if (is_null) {
        return assign_null_result(out_result);
    }
    left_cursor = (struct spatial_wkb_cursor){.bytes = left.wkb, .size = left.wkb_size};
    right_cursor = (struct spatial_wkb_cursor){.bytes = right.wkb, .size = right.wkb_size};
    rc = wkb_bounds_at(&left_cursor, &left_box, error, mylite_spatial_function_name(kind));
    if (rc == 0) {
        rc = wkb_bounds_at(&right_cursor, &right_box, error, mylite_spatial_function_name(kind));
    }
    if (rc != 0) {
        return rc;
    }
    if (!left_box.has_value || !right_box.has_value) {
        if (kind == MYLITE_SPATIAL_FUNCTION_MBREQUALS) {
            return assign_integer_result(
                out_result,
                left_box.has_value == right_box.has_value ? 1 : 0
            );
        }
        return assign_null_result(out_result);
    }

    switch (kind) {
    case MYLITE_SPATIAL_FUNCTION_MBRCONTAINS:
        result = spatial_box_covers(&left_box, &right_box) &&
                 (spatial_box_equals(&left_box, &right_box) ||
                  (left_box.min_x < right_box.min_x && left_box.max_x > right_box.max_x &&
                   left_box.min_y < right_box.min_y && left_box.max_y > right_box.max_y));
        break;
    case MYLITE_SPATIAL_FUNCTION_MBRWITHIN:
        result = spatial_box_covers(&right_box, &left_box) &&
                 (spatial_box_equals(&left_box, &right_box) ||
                  (right_box.min_x < left_box.min_x && right_box.max_x > left_box.max_x &&
                   right_box.min_y < left_box.min_y && right_box.max_y > left_box.max_y));
        break;
    case MYLITE_SPATIAL_FUNCTION_MBRCOVERS:
        result = spatial_box_covers(&left_box, &right_box);
        break;
    case MYLITE_SPATIAL_FUNCTION_MBRCOVEREDBY:
        result = spatial_box_covers(&right_box, &left_box);
        break;
    case MYLITE_SPATIAL_FUNCTION_MBRDISJOINT:
        result = !spatial_box_intersects(&left_box, &right_box);
        break;
    case MYLITE_SPATIAL_FUNCTION_MBREQUALS:
        result = spatial_box_equals(&left_box, &right_box);
        break;
    case MYLITE_SPATIAL_FUNCTION_MBRINTERSECTS:
        result = spatial_box_intersects(&left_box, &right_box);
        break;
    case MYLITE_SPATIAL_FUNCTION_MBROVERLAPS:
        result = spatial_box_interiors_intersect(&left_box, &right_box) &&
                 !spatial_box_covers(&left_box, &right_box) &&
                 !spatial_box_covers(&right_box, &left_box);
        break;
    case MYLITE_SPATIAL_FUNCTION_MBRTOUCHES:
        result = spatial_box_intersects(&left_box, &right_box) &&
                 !spatial_box_interiors_intersect(&left_box, &right_box) &&
                 !spatial_box_equals(&left_box, &right_box);
        break;
    default:
        break;
    }
    return assign_integer_result(out_result, result ? 1 : 0);
}

static int evaluate_geohash(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
) {
    struct spatial_geometry_view geometry = {0};
    bool is_null = false;
    bool max_length_is_null = false;
    uint32_t max_length = 0U;
    double longitude = 0.0;
    double latitude = 0.0;
    unsigned char *text = NULL;
    size_t text_length = 0U;
    int rc = validate_argument_count(kind, argument_count, 2U, 3U, error);

    if (rc != 0) {
        return rc;
    }
    if (argument_count == 2U) {
        rc = read_single_geometry_argument(kind, arguments, 1U, &geometry, &is_null, error);
        if (rc == 0) {
            rc = argument_geohash_uint32(
                &arguments[1],
                &max_length,
                &max_length_is_null,
                error,
                mylite_spatial_function_name(kind),
                "geohash max length",
                "max geohash length"
            );
        }
        if (rc != 0) {
            return rc;
        }
        if (is_null || max_length_is_null) {
            return assign_null_result(out_result);
        }
        if (max_length < 1U || max_length > spatial_geohash_max_length) {
            return set_geohash_range_error(
                error,
                "max geohash length",
                mylite_spatial_function_name(kind)
            );
        }
        if (geometry.type != MYLITE_SPATIAL_GEOMETRY_POINT) {
            return set_incorrect_argument_type_error(
                error,
                "point",
                mylite_spatial_function_name(kind)
            );
        }
        if (geometry.srid != 0U && geometry.srid != spatial_srid_wgs84) {
            return set_spatial_error(
                error,
                mysql_error_srs_not_found,
                "SR001",
                "There's no spatial reference system with SRID %u.",
                geometry.srid
            );
        }
        rc = geometry_point_coordinates(
            geometry.wkb,
            geometry.wkb_size,
            &longitude,
            &latitude,
            error,
            mylite_spatial_function_name(kind)
        );
        if (rc != 0) {
            return rc;
        }
        if (geometry.srid == spatial_srid_wgs84) {
            double coordinate_latitude = longitude;

            longitude = latitude;
            latitude = coordinate_latitude;
        }
    } else {
        bool longitude_is_null = arguments[0].is_null;
        bool latitude_is_null = arguments[1].is_null;

        if (longitude_is_null || latitude_is_null || arguments[2].is_null) {
            return assign_null_result(out_result);
        }
        rc = argument_geohash_coordinate(
            &arguments[0],
            &longitude,
            error,
            mylite_spatial_function_name(kind),
            "longitude"
        );
        if (rc == 0) {
            rc = argument_geohash_coordinate(
                &arguments[1],
                &latitude,
                error,
                mylite_spatial_function_name(kind),
                "latitude"
            );
        }
        if (rc == 0) {
            rc = argument_geohash_uint32(
                &arguments[2],
                &max_length,
                &max_length_is_null,
                error,
                mylite_spatial_function_name(kind),
                "geohash max length",
                "max geohash length"
            );
        }
        if (rc != 0) {
            return rc;
        }
        if (max_length_is_null) {
            return assign_null_result(out_result);
        }
        if (max_length < 1U || max_length > spatial_geohash_max_length) {
            return set_geohash_range_error(
                error,
                "max geohash length",
                mylite_spatial_function_name(kind)
            );
        }
    }
    if (longitude < spatial_geohash_longitude_min || longitude > spatial_geohash_longitude_max) {
        return set_geohash_range_error(error, "longitude", mylite_spatial_function_name(kind));
    }
    if (latitude < spatial_geohash_latitude_min || latitude > spatial_geohash_latitude_max) {
        return set_geohash_range_error(error, "latitude", mylite_spatial_function_name(kind));
    }
    rc = geohash_encode(longitude, latitude, max_length, &text, &text_length, error);
    if (rc != 0) {
        free(text);
        return rc;
    }
    return assign_owned_bytes_result(out_result, MYLITE_SPATIAL_RESULT_TEXT, text, text_length);
}

static int evaluate_coordinate_from_geohash(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
) {
    double longitude = 0.0;
    double latitude = 0.0;
    int rc = validate_argument_count(kind, argument_count, 1U, 1U, error);

    if (rc != 0) {
        return rc;
    }
    if (arguments[0].is_null) {
        return assign_null_result(out_result);
    }
    rc = geohash_decode(
        &arguments[0],
        &longitude,
        &latitude,
        error,
        mylite_spatial_function_name(kind)
    );
    if (rc != 0) {
        return rc;
    }
    return assign_double_result(
        out_result,
        kind == MYLITE_SPATIAL_FUNCTION_ST_LATFROMGEOHASH ? latitude : longitude
    );
}

static int evaluate_point_from_geohash(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct mylite_spatial_result *out_result,
    struct mylite_spatial_error *error
) {
    bool srid_is_null = false;
    uint32_t srid = 0U;
    double longitude = 0.0;
    double latitude = 0.0;
    unsigned char *bytes = NULL;
    size_t byte_count = 0U;
    int rc = validate_argument_count(kind, argument_count, 2U, 2U, error);

    if (rc != 0) {
        return rc;
    }
    if (arguments[0].is_null || arguments[1].is_null) {
        return assign_null_result(out_result);
    }
    rc = geohash_decode(
        &arguments[0],
        &longitude,
        &latitude,
        error,
        mylite_spatial_function_name(kind)
    );
    if (rc == 0) {
        rc = argument_geohash_uint32(
            &arguments[1],
            &srid,
            &srid_is_null,
            error,
            mylite_spatial_function_name(kind),
            "SRID",
            "SRID"
        );
    }
    if (rc != 0) {
        return rc;
    }
    if (srid_is_null) {
        return assign_null_result(out_result);
    }
    if (srid != 0U && srid != spatial_srid_wgs84) {
        return set_spatial_error(
            error,
            mysql_error_srs_not_found,
            "SR001",
            "There's no spatial reference system with SRID %u.",
            srid
        );
    }
    if (srid == spatial_srid_wgs84) {
        rc = make_point_internal_geometry_with_srid(
            srid,
            latitude,
            longitude,
            &bytes,
            &byte_count,
            error
        );
    } else {
        rc = make_point_internal_geometry_with_srid(
            srid,
            longitude,
            latitude,
            &bytes,
            &byte_count,
            error
        );
    }
    if (rc != 0) {
        free(bytes);
        return rc;
    }
    return assign_owned_bytes_result(out_result, MYLITE_SPATIAL_RESULT_GEOMETRY, bytes, byte_count);
}

static bool function_name_matches(const char *name, size_t name_size, const char *expected) {
    size_t index = 0U;

    if (expected == NULL || strlen(expected) != name_size) {
        return false;
    }
    for (index = 0U; index < name_size; ++index) {
        if (tolower((unsigned char)name[index]) != tolower((unsigned char)expected[index])) {
            return false;
        }
    }
    return true;
}

static bool function_kind_is_from_text(enum mylite_spatial_function_kind kind) {
    return kind >= MYLITE_SPATIAL_FUNCTION_ST_GEOMFROMTEXT &&
           kind <= MYLITE_SPATIAL_FUNCTION_ST_GEOMCOLLFROMTXT;
}

static bool function_kind_is_from_wkb(enum mylite_spatial_function_kind kind) {
    return kind >= MYLITE_SPATIAL_FUNCTION_ST_GEOMFROMWKB &&
           kind <= MYLITE_SPATIAL_FUNCTION_ST_GEOMETRYCOLLECTIONFROMWKB;
}

static enum mylite_spatial_geometry_type expected_text_wkb_type(
    enum mylite_spatial_function_kind kind
) {
    switch (kind) {
    case MYLITE_SPATIAL_FUNCTION_ST_POINTFROMTEXT:
    case MYLITE_SPATIAL_FUNCTION_ST_POINTFROMWKB:
        return MYLITE_SPATIAL_GEOMETRY_POINT;
    case MYLITE_SPATIAL_FUNCTION_ST_LINEFROMTEXT:
    case MYLITE_SPATIAL_FUNCTION_ST_LINESTRINGFROMTEXT:
    case MYLITE_SPATIAL_FUNCTION_ST_LINEFROMWKB:
    case MYLITE_SPATIAL_FUNCTION_ST_LINESTRINGFROMWKB:
        return MYLITE_SPATIAL_GEOMETRY_LINESTRING;
    case MYLITE_SPATIAL_FUNCTION_ST_POLYFROMTEXT:
    case MYLITE_SPATIAL_FUNCTION_ST_POLYGONFROMTEXT:
    case MYLITE_SPATIAL_FUNCTION_ST_POLYFROMWKB:
    case MYLITE_SPATIAL_FUNCTION_ST_POLYGONFROMWKB:
        return MYLITE_SPATIAL_GEOMETRY_POLYGON;
    case MYLITE_SPATIAL_FUNCTION_ST_MPOINTFROMTEXT:
    case MYLITE_SPATIAL_FUNCTION_ST_MULTIPOINTFROMTEXT:
    case MYLITE_SPATIAL_FUNCTION_ST_MPOINTFROMWKB:
    case MYLITE_SPATIAL_FUNCTION_ST_MULTIPOINTFROMWKB:
        return MYLITE_SPATIAL_GEOMETRY_MULTIPOINT;
    case MYLITE_SPATIAL_FUNCTION_ST_MLINEFROMTEXT:
    case MYLITE_SPATIAL_FUNCTION_ST_MULTILINESTRINGFROMTEXT:
    case MYLITE_SPATIAL_FUNCTION_ST_MLINEFROMWKB:
    case MYLITE_SPATIAL_FUNCTION_ST_MULTILINESTRINGFROMWKB:
        return MYLITE_SPATIAL_GEOMETRY_MULTILINESTRING;
    case MYLITE_SPATIAL_FUNCTION_ST_MPOLYFROMTEXT:
    case MYLITE_SPATIAL_FUNCTION_ST_MULTIPOLYGONFROMTEXT:
    case MYLITE_SPATIAL_FUNCTION_ST_MPOLYFROMWKB:
    case MYLITE_SPATIAL_FUNCTION_ST_MULTIPOLYGONFROMWKB:
        return MYLITE_SPATIAL_GEOMETRY_MULTIPOLYGON;
    case MYLITE_SPATIAL_FUNCTION_ST_GEOMCOLLFROMTEXT:
    case MYLITE_SPATIAL_FUNCTION_ST_GEOMETRYCOLLECTIONFROMTEXT:
    case MYLITE_SPATIAL_FUNCTION_ST_GEOMCOLLFROMTXT:
    case MYLITE_SPATIAL_FUNCTION_ST_GEOMCOLLFROMWKB:
    case MYLITE_SPATIAL_FUNCTION_ST_GEOMETRYCOLLECTIONFROMWKB:
        return MYLITE_SPATIAL_GEOMETRY_GEOMETRYCOLLECTION;
    default:
        break;
    }
    return MYLITE_SPATIAL_GEOMETRY_NONE;
}

static enum mylite_spatial_geometry_type constructor_result_type(
    enum mylite_spatial_function_kind kind
) {
    switch (kind) {
    case MYLITE_SPATIAL_FUNCTION_LINESTRING:
        return MYLITE_SPATIAL_GEOMETRY_LINESTRING;
    case MYLITE_SPATIAL_FUNCTION_POLYGON:
        return MYLITE_SPATIAL_GEOMETRY_POLYGON;
    case MYLITE_SPATIAL_FUNCTION_MULTIPOINT:
        return MYLITE_SPATIAL_GEOMETRY_MULTIPOINT;
    case MYLITE_SPATIAL_FUNCTION_MULTILINESTRING:
        return MYLITE_SPATIAL_GEOMETRY_MULTILINESTRING;
    case MYLITE_SPATIAL_FUNCTION_MULTIPOLYGON:
        return MYLITE_SPATIAL_GEOMETRY_MULTIPOLYGON;
    case MYLITE_SPATIAL_FUNCTION_GEOMETRYCOLLECTION:
    case MYLITE_SPATIAL_FUNCTION_GEOMCOLLECTION:
        return MYLITE_SPATIAL_GEOMETRY_GEOMETRYCOLLECTION;
    default:
        break;
    }
    return MYLITE_SPATIAL_GEOMETRY_NONE;
}

static int set_spatial_error(
    struct mylite_spatial_error *error,
    int code,
    const char *sqlstate, // NOLINT(bugprone-easily-swappable-parameters): diagnostic shape.
    const char *format,
    ...
) {
    va_list args;

    if (error == NULL) {
        return -1;
    }
    *error = (struct mylite_spatial_error){0};
    error->code = code;
    error->sqlstate = sqlstate;
    va_start(args, format);
    int written = vsnprintf(error->message, sizeof(error->message), format, args);
    va_end(args);
    if (written < 0 || (size_t)written >= sizeof(error->message)) {
        error->message[0] = '\0';
    }
    return -1;
}

static int set_nomem_error(struct mylite_spatial_error *error) {
    if (error != NULL) {
        *error = (struct mylite_spatial_error){
            .code = 0,
            .sqlstate = "HY001",
            .message = "Out of memory",
            .is_nomem = true,
        };
    }
    return -1;
}

static const char *spatial_diagnostic_function_name(
    const char *function_name,
    char *buffer,
    size_t buffer_size
) {
    size_t index = 0U;

    if (function_name == NULL) {
        return "spatial";
    }
    if (buffer == NULL || buffer_size == 0U) {
        return function_name;
    }
    for (; function_name[index] != '\0' && index + 1U < buffer_size; ++index) {
        buffer[index] = (char)tolower((unsigned char)function_name[index]);
    }
    if (function_name[index] != '\0') {
        return function_name;
    }
    buffer[index] = '\0';
    return buffer;
}

static int set_invalid_gis_data_error(
    struct mylite_spatial_error *error,
    const char *function_name
) {
    char diagnostic_function_name[spatial_diagnostic_function_name_capacity];

    return set_spatial_error(
        error,
        mysql_error_invalid_gis_data,
        "22023",
        "Invalid GIS data provided to function %s.",
        spatial_diagnostic_function_name(
            function_name,
            diagnostic_function_name,
            sizeof(diagnostic_function_name)
        )
    );
}

static int set_unexpected_geometry_type_error(
    struct mylite_spatial_error *error,
    const char *subject,
    enum mylite_spatial_geometry_type actual_type,
    const char *function_name
) {
    char diagnostic_function_name[spatial_diagnostic_function_name_capacity];

    return set_spatial_error(
        error,
        mysql_error_unexpected_geometry_type,
        "22S01",
        "%s is a geometry of unexpected type %s in %s.",
        subject == NULL ? "Geometry value" : subject,
        geometry_type_name(actual_type),
        spatial_diagnostic_function_name(
            function_name,
            diagnostic_function_name,
            sizeof(diagnostic_function_name)
        )
    );
}

static int set_wrong_arguments_error(
    struct mylite_spatial_error *error,
    const char *function_name
) {
    char diagnostic_function_name[spatial_diagnostic_function_name_capacity];

    return set_spatial_error(
        error,
        mysql_error_wrong_arguments,
        "HY000",
        "Incorrect arguments to %s",
        spatial_diagnostic_function_name(
            function_name,
            diagnostic_function_name,
            sizeof(diagnostic_function_name)
        )
    );
}

static int set_incorrect_argument_type_error(
    struct mylite_spatial_error *error,
    const char *argument_name,
    const char *function_name
) {
    char diagnostic_function_name[spatial_diagnostic_function_name_capacity];

    return set_spatial_error(
        error,
        mysql_error_incorrect_type_for_argument,
        "HY000",
        "Incorrect type for argument %s in function %s.",
        argument_name == NULL ? "value" : argument_name,
        spatial_diagnostic_function_name(
            function_name,
            diagnostic_function_name,
            sizeof(diagnostic_function_name)
        )
    );
}

static int set_srs_not_geographic_error(
    struct mylite_spatial_error *error,
    uint32_t srid,
    const char *function_name
) {
    char diagnostic_function_name[spatial_diagnostic_function_name_capacity];
    const char *lower_function_name = spatial_diagnostic_function_name(
        function_name,
        diagnostic_function_name,
        sizeof(diagnostic_function_name)
    );

    return set_spatial_error(
        error,
        mysql_error_srs_not_geographic,
        "22S00",
        "Function %s is only defined for geographic spatial reference systems, but one of its "
        "arguments is in SRID %u, which is not geographic.",
        lower_function_name,
        srid
    );
}

static int set_gis_different_srids_error(
    struct mylite_spatial_error *error,
    uint32_t left_srid,
    uint32_t right_srid,
    const char *function_name
) {
    char diagnostic_function_name[spatial_diagnostic_function_name_capacity];

    return set_spatial_error(
        error,
        mysql_error_gis_different_srids,
        "HY000",
        "Binary geometry function %s given two geometries of different srids: %u and %u, which "
        "should have been identical.",
        spatial_diagnostic_function_name(
            function_name,
            diagnostic_function_name,
            sizeof(diagnostic_function_name)
        ),
        left_srid,
        right_srid
    );
}

static int set_not_implemented_for_geographic_srs_error(
    struct mylite_spatial_error *error,
    const char *function_name
) {
    char diagnostic_function_name[spatial_diagnostic_function_name_capacity];

    return set_spatial_error(
        error,
        mysql_error_not_implemented_for_geographic_srs,
        "22S00",
        "%s has not been implemented for geographic spatial reference systems.",
        spatial_diagnostic_function_name(
            function_name,
            diagnostic_function_name,
            sizeof(diagnostic_function_name)
        )
    );
}

static int set_not_implemented_for_geographic_srs_geometry_error(
    struct mylite_spatial_error *error,
    enum mylite_spatial_geometry_type type,
    const char *function_name
) {
    char diagnostic_function_name[spatial_diagnostic_function_name_capacity];

    return set_spatial_error(
        error,
        mysql_error_not_implemented_for_geographic_srs,
        "22S00",
        "%s(%s) has not been implemented for geographic spatial reference systems.",
        spatial_diagnostic_function_name(
            function_name,
            diagnostic_function_name,
            sizeof(diagnostic_function_name)
        ),
        geometry_type_name(type)
    );
}

static int set_not_implemented_for_geographic_srs_geometry_argument_error(
    struct mylite_spatial_error *error,
    enum mylite_spatial_geometry_type type,
    const char *function_name
) {
    char diagnostic_function_name[spatial_diagnostic_function_name_capacity];

    return set_spatial_error(
        error,
        mysql_error_not_implemented_for_geographic_srs,
        "22S00",
        "%s(%s, ...) has not been implemented for geographic spatial reference systems.",
        spatial_diagnostic_function_name(
            function_name,
            diagnostic_function_name,
            sizeof(diagnostic_function_name)
        ),
        geometry_type_name(type)
    );
}

static int set_not_implemented_for_cartesian_srs_error(
    struct mylite_spatial_error *error,
    enum mylite_spatial_geometry_type left_type,
    enum mylite_spatial_geometry_type right_type,
    const char *function_name
) {
    char diagnostic_function_name[spatial_diagnostic_function_name_capacity];

    return set_spatial_error(
        error,
        mysql_error_not_implemented_for_cartesian_srs,
        "22S00",
        "%s(%s, %s) has not been implemented for Cartesian spatial reference systems.",
        spatial_diagnostic_function_name(
            function_name,
            diagnostic_function_name,
            sizeof(diagnostic_function_name)
        ),
        cartesian_srs_not_implemented_geometry_type_name(left_type),
        cartesian_srs_not_implemented_geometry_type_name(right_type)
    );
}

static int set_unknown_length_unit_error(
    struct mylite_spatial_error *error,
    const struct mylite_spatial_argument *unit_argument,
    const char *function_name
) {
    char diagnostic_function_name[spatial_diagnostic_function_name_capacity];
    const char *unit = unit_argument == NULL ? "" : (const char *)unit_argument->bytes;
    size_t unit_size = unit_argument == NULL ? 0U : unit_argument->byte_count;

    if (unit_size > (size_t)INT_MAX) {
        unit_size = (size_t)INT_MAX;
    }
    return set_spatial_error(
        error,
        mysql_error_geometry_unknown_length_unit,
        "SU001",
        "The geometry passed to function %s is in SRID 0, which doesn't specify a length unit. "
        "Can't convert to '%.*s'.",
        spatial_diagnostic_function_name(
            function_name,
            diagnostic_function_name,
            sizeof(diagnostic_function_name)
        ),
        (int)unit_size,
        unit == NULL ? "" : unit
    );
}

static int set_distance_sphere_longitude_error(
    struct mylite_spatial_error *error,
    double longitude,
    const char *function_name
) {
    char diagnostic_function_name[spatial_diagnostic_function_name_capacity];

    return set_spatial_error(
        error,
        mysql_error_geojson_longitude_out_of_range,
        "22S02",
        "Longitude %.6f is out of range in function %s. It must be within (-180.000000, "
        "180.000000].",
        longitude,
        spatial_diagnostic_function_name(
            function_name,
            diagnostic_function_name,
            sizeof(diagnostic_function_name)
        )
    );
}

static int set_distance_sphere_latitude_error(
    struct mylite_spatial_error *error,
    double latitude,
    const char *function_name
) {
    char diagnostic_function_name[spatial_diagnostic_function_name_capacity];

    return set_spatial_error(
        error,
        mysql_error_geojson_latitude_out_of_range,
        "22S03",
        "Latitude %.6f is out of range in function %s. It must be within [-90.000000, "
        "90.000000].",
        latitude,
        spatial_diagnostic_function_name(
            function_name,
            diagnostic_function_name,
            sizeof(diagnostic_function_name)
        )
    );
}

static int set_nonpositive_radius_error(
    struct mylite_spatial_error *error,
    const char *function_name
) {
    char diagnostic_function_name[spatial_diagnostic_function_name_capacity];

    return set_spatial_error(
        error,
        mysql_error_nonpositive_radius,
        "22003",
        "Invalid radius provided to function %s: Radius must be greater than zero.",
        spatial_diagnostic_function_name(
            function_name,
            diagnostic_function_name,
            sizeof(diagnostic_function_name)
        )
    );
}

static int set_distance_range_error(struct mylite_spatial_error *error, const char *function_name) {
    char diagnostic_function_name[spatial_diagnostic_function_name_capacity];

    return set_spatial_error(
        error,
        mysql_error_numeric_value_out_of_range,
        "22003",
        "Distance value is out of range in '%s'",
        spatial_diagnostic_function_name(
            function_name,
            diagnostic_function_name,
            sizeof(diagnostic_function_name)
        )
    );
}

static int set_geohash_range_error(
    struct mylite_spatial_error *error,
    const char *subject,
    const char *function_name
) {
    char diagnostic_function_name[spatial_diagnostic_function_name_capacity];

    return set_spatial_error(
        error,
        mysql_error_numeric_value_out_of_range,
        "22003",
        "%s value is out of range in '%s'",
        subject == NULL ? "geohash" : subject,
        spatial_diagnostic_function_name(
            function_name,
            diagnostic_function_name,
            sizeof(diagnostic_function_name)
        )
    );
}

static int set_invalid_geohash_error(
    struct mylite_spatial_error *error,
    const struct mylite_spatial_argument *argument,
    const char *function_name
) {
    char diagnostic_function_name[spatial_diagnostic_function_name_capacity];
    char upper_function_name[spatial_diagnostic_function_name_capacity];
    const char *text = argument == NULL ? "" : (const char *)argument->bytes;
    size_t text_length = argument == NULL ? 0U : argument->byte_count;
    size_t index = 0U;

    if (function_name == NULL) {
        function_name = "spatial";
    }
    for (; function_name[index] != '\0' && index + 1U < sizeof(upper_function_name); ++index) {
        upper_function_name[index] = (char)toupper((unsigned char)function_name[index]);
    }
    upper_function_name[index] = '\0';
    return set_spatial_error(
        error,
        mysql_error_invalid_geohash,
        "HY000",
        "Incorrect geohash value: '%.*s' for function %s",
        (int)(text_length > spatial_geohash_error_preview_length
                  ? spatial_geohash_error_preview_length
                  : text_length),
        text == NULL ? "" : text,
        upper_function_name[0] == '\0' ? spatial_diagnostic_function_name(
                                             function_name,
                                             diagnostic_function_name,
                                             sizeof(diagnostic_function_name)
                                         )
                                       : upper_function_name
    );
}

static int set_invalid_json_text_error(
    struct mylite_spatial_error *error,
    const struct mylite_json_normalize_result *result,
    const char *function_name
) {
    char diagnostic_function_name[spatial_diagnostic_function_name_capacity];

    return set_spatial_error(
        error,
        mysql_error_invalid_json_text_in_function,
        "22032",
        "Invalid JSON text in argument 1 to function %s: \"%s\" at position %zu.",
        spatial_diagnostic_function_name(
            function_name,
            diagnostic_function_name,
            sizeof(diagnostic_function_name)
        ),
        mylite_json_invalid_text_error_message(result),
        result == NULL ? 0U : result->position
    );
}

static int set_invalid_geojson_missing_member_error(
    struct mylite_spatial_error *error,
    const char *member,
    const char *function_name
) {
    char diagnostic_function_name[spatial_diagnostic_function_name_capacity];

    return set_spatial_error(
        error,
        mysql_error_invalid_geojson_missing_member,
        "HY000",
        "Invalid GeoJSON data provided to function %s: Missing required member '%s'",
        spatial_diagnostic_function_name(
            function_name,
            diagnostic_function_name,
            sizeof(diagnostic_function_name)
        ),
        member == NULL ? "" : member
    );
}

static int set_invalid_geojson_data_error(
    struct mylite_spatial_error *error,
    const char *function_name
) {
    char diagnostic_function_name[spatial_diagnostic_function_name_capacity];

    return set_spatial_error(
        error,
        mysql_error_invalid_geojson_data,
        "HY000",
        "Invalid GeoJSON data provided to function %s",
        spatial_diagnostic_function_name(
            function_name,
            diagnostic_function_name,
            sizeof(diagnostic_function_name)
        )
    );
}

static int set_unsupported_geojson_dimensions_error(
    struct mylite_spatial_error *error,
    size_t dimension_count,
    const char *function_name
) {
    char diagnostic_function_name[spatial_diagnostic_function_name_capacity];

    return set_spatial_error(
        error,
        mysql_error_unsupported_geojson_dimensions,
        "HY000",
        "Unsupported number of coordinate dimensions in function %s: Found %zu, expected 2",
        spatial_diagnostic_function_name(
            function_name,
            diagnostic_function_name,
            sizeof(diagnostic_function_name)
        ),
        dimension_count
    );
}

static int set_geojson_option_error(
    struct mylite_spatial_error *error,
    const char *value,
    const char *function_name
) {
    char diagnostic_function_name[spatial_diagnostic_function_name_capacity];
    const char *argument_name = "options";

    if (function_name != NULL &&
        function_name_matches(function_name, strlen(function_name), "ST_GeomFromGeoJSON")) {
        argument_name = "option";
    }
    return set_spatial_error(
        error,
        mysql_error_invalid_geohash,
        "HY000",
        "Incorrect %s value: '%s' for function %s",
        argument_name,
        value == NULL ? "" : value,
        spatial_diagnostic_function_name(
            function_name,
            diagnostic_function_name,
            sizeof(diagnostic_function_name)
        )
    );
}

static int set_geojson_max_dec_digits_error(
    struct mylite_spatial_error *error,
    const char *value,
    const char *function_name
) {
    char diagnostic_function_name[spatial_diagnostic_function_name_capacity];

    return set_spatial_error(
        error,
        mysql_error_invalid_geohash,
        "HY000",
        "Incorrect max decimal digits value: '%s' for function %s",
        value == NULL ? "" : value,
        spatial_diagnostic_function_name(
            function_name,
            diagnostic_function_name,
            sizeof(diagnostic_function_name)
        )
    );
}

static int set_geojson_longitude_error(
    struct mylite_spatial_error *error,
    double longitude,
    const char *function_name
) {
    char diagnostic_function_name[spatial_diagnostic_function_name_capacity];

    return set_spatial_error(
        error,
        mysql_error_geojson_longitude_out_of_range,
        "22S02",
        "Longitude %.6f is out of range in function %s. It must be within (-180.000000, "
        "180.000000].",
        longitude,
        spatial_diagnostic_function_name(
            function_name,
            diagnostic_function_name,
            sizeof(diagnostic_function_name)
        )
    );
}

static int set_geojson_latitude_error(
    struct mylite_spatial_error *error,
    double latitude,
    const char *function_name
) {
    char diagnostic_function_name[spatial_diagnostic_function_name_capacity];

    return set_spatial_error(
        error,
        mysql_error_geojson_latitude_out_of_range,
        "22S03",
        "Latitude %.6f is out of range in function %s. It must be within [-90.000000, 90.000000].",
        latitude,
        spatial_diagnostic_function_name(
            function_name,
            diagnostic_function_name,
            sizeof(diagnostic_function_name)
        )
    );
}

static int set_parameter_count_error(
    struct mylite_spatial_error *error,
    enum mylite_spatial_function_kind kind
) {
    char diagnostic_function_name[spatial_diagnostic_function_name_capacity];

    return set_spatial_error(
        error,
        mysql_error_native_function_parameter_count,
        "42000",
        "Incorrect parameter count in the call to native function '%s'",
        spatial_diagnostic_function_name(
            mylite_spatial_function_name(kind),
            diagnostic_function_name,
            sizeof(diagnostic_function_name)
        )
    );
}

static int validate_argument_count(
    enum mylite_spatial_function_kind kind, // NOLINT(bugprone-easily-swappable-parameters)
    size_t argument_count,
    size_t min_argument_count,
    size_t max_argument_count,
    struct mylite_spatial_error *error
) {
    if (argument_count < min_argument_count || argument_count > max_argument_count) {
        return set_parameter_count_error(error, kind);
    }
    return 0;
}

static bool any_argument_is_null(
    const struct mylite_spatial_argument *arguments,
    size_t argument_count
) {
    for (size_t index = 0U; index < argument_count; ++index) {
        if (arguments[index].is_null) {
            return true;
        }
    }
    return false;
}

static int argument_numeric(
    const struct mylite_spatial_argument *argument,
    double *out_value,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    char *text = NULL;
    char *end = NULL;
    double value = 0.0;

    if (argument == NULL || out_value == NULL) {
        return set_invalid_gis_data_error(error, function_name);
    }
    if (argument->has_numeric) {
        if (!isfinite(argument->numeric)) {
            return set_invalid_gis_data_error(error, function_name);
        }
        *out_value = argument->numeric;
        return 0;
    }
    if (argument->byte_count == SIZE_MAX) {
        return set_nomem_error(error);
    }
    text = (char *)malloc(argument->byte_count + 1U);
    if (text == NULL) {
        return set_nomem_error(error);
    }
    if (argument->byte_count != 0U) {
        memcpy(text, argument->bytes, argument->byte_count);
    }
    text[argument->byte_count] = '\0';
    errno = 0;
    value = strtod(text, &end);
    while (end != NULL && *end != '\0' && isspace((unsigned char)*end)) {
        ++end;
    }
    if (end == text || (end != NULL && *end != '\0') || errno == ERANGE || !isfinite(value)) {
        free(text);
        return set_invalid_gis_data_error(error, function_name);
    }
    free(text);
    *out_value = value;
    return 0;
}

static int argument_distance(
    const struct mylite_spatial_argument *argument,
    double *out_value,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    char *text = NULL;
    char *end = NULL;
    double value = 0.0;

    if (argument == NULL || out_value == NULL) {
        return set_invalid_gis_data_error(error, function_name);
    }
    if (argument->has_numeric) {
        if (!isfinite(argument->numeric)) {
            return set_distance_range_error(error, function_name);
        }
        *out_value = argument->numeric;
        return 0;
    }
    if (argument->byte_count == SIZE_MAX) {
        return set_nomem_error(error);
    }
    text = (char *)malloc(argument->byte_count + 1U);
    if (text == NULL) {
        return set_nomem_error(error);
    }
    if (argument->byte_count != 0U) {
        memcpy(text, argument->bytes, argument->byte_count);
    }
    text[argument->byte_count] = '\0';
    errno = 0;
    value = strtod(text, &end);
    if (end == text) {
        value = 0.0;
    } else if (errno == ERANGE || !isfinite(value)) {
        free(text);
        return set_distance_range_error(error, function_name);
    }
    free(text);
    *out_value = value;
    return 0;
}

static int argument_geohash_coordinate(
    const struct mylite_spatial_argument *argument,
    double *out_value,
    struct mylite_spatial_error *error,
    const char *function_name,
    const char *range_subject
) {
    char *text = NULL;
    char *end = NULL;
    double value = 0.0;

    if (argument == NULL || out_value == NULL) {
        return set_invalid_gis_data_error(error, function_name);
    }
    if (argument->has_numeric) {
        if (!isfinite(argument->numeric)) {
            return set_geohash_range_error(error, range_subject, function_name);
        }
        *out_value = argument->numeric;
        return 0;
    }
    if (argument->byte_count == SIZE_MAX) {
        return set_nomem_error(error);
    }
    text = (char *)malloc(argument->byte_count + 1U);
    if (text == NULL) {
        return set_nomem_error(error);
    }
    if (argument->byte_count != 0U) {
        memcpy(text, argument->bytes, argument->byte_count);
    }
    text[argument->byte_count] = '\0';
    errno = 0;
    value = strtod(text, &end);
    if (end == text) {
        value = 0.0;
    } else if (errno == ERANGE || !isfinite(value)) {
        free(text);
        return set_geohash_range_error(error, range_subject, function_name);
    }
    free(text);
    *out_value = value;
    return 0;
}

static int argument_srid(
    const struct mylite_spatial_argument *argument,
    uint32_t *out_srid,
    bool *out_is_null,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    double value = 0.0;

    if (out_srid == NULL || out_is_null == NULL) {
        return set_invalid_gis_data_error(error, function_name);
    }
    *out_srid = 0U;
    *out_is_null = argument == NULL || argument->is_null;
    if (*out_is_null) {
        return 0;
    }
    if (argument_numeric(argument, &value, error, function_name) != 0) {
        return -1;
    }
    if (value < 0.0 || value > (double)UINT32_MAX || floor(value) != value) {
        return set_invalid_gis_data_error(error, function_name);
    }
    *out_srid = (uint32_t)value;
    return 0;
}

static int argument_geohash_uint32(
    const struct mylite_spatial_argument *argument,
    uint32_t *out_value,
    bool *out_is_null,
    struct mylite_spatial_error *error,
    const char *function_name,
    const char *argument_name, // NOLINT(bugprone-easily-swappable-parameters): diagnostic labels.
    const char *range_subject
) {
    char *text = NULL;
    char *end = NULL;
    double value = 0.0;

    if (out_value == NULL || out_is_null == NULL) {
        return set_invalid_gis_data_error(error, function_name);
    }
    *out_value = 0U;
    *out_is_null = argument == NULL || argument->is_null;
    if (*out_is_null) {
        return 0;
    }
    if (argument->has_numeric) {
        value = argument->numeric;
    } else {
        if (argument->byte_count == SIZE_MAX) {
            return set_nomem_error(error);
        }
        text = (char *)malloc(argument->byte_count + 1U);
        if (text == NULL) {
            return set_nomem_error(error);
        }
        if (argument->byte_count != 0U) {
            memcpy(text, argument->bytes, argument->byte_count);
        }
        text[argument->byte_count] = '\0';
        errno = 0;
        value = strtod(text, &end);
        while (end != NULL && *end != '\0' && isspace((unsigned char)*end)) {
            ++end;
        }
        if (end == text || (end != NULL && *end != '\0') || errno == ERANGE) {
            free(text);
            return set_geohash_range_error(error, range_subject, function_name);
        }
        free(text);
    }
    if (!isfinite(value) || value < 0.0 || value > (double)UINT32_MAX) {
        return set_geohash_range_error(error, range_subject, function_name);
    }
    if (floor(value) != value) {
        return set_incorrect_argument_type_error(error, argument_name, function_name);
    }
    *out_value = (uint32_t)value;
    return 0;
}

static int argument_geojson_uint32(
    const struct mylite_spatial_argument *argument,
    uint32_t *out_value,
    bool *out_is_null,
    struct mylite_spatial_error *error,
    const char *function_name,
    const char *argument_name
) {
    char *text = NULL;
    char *end = NULL;
    char display[spatial_double_text_capacity];
    double value = 0.0;

    if (out_value == NULL || out_is_null == NULL) {
        return set_invalid_gis_data_error(error, function_name);
    }
    *out_value = 0U;
    *out_is_null = argument == NULL || argument->is_null;
    if (*out_is_null) {
        return 0;
    }
    if (argument->has_numeric) {
        value = argument->numeric;
        snprintf(display, sizeof(display), "%.15g", value);
        text = display;
        if (!isfinite(value) || value < 0.0 || value > (double)UINT32_MAX) {
            if (strcmp(argument_name, "max decimal digits") == 0) {
                return set_geojson_max_dec_digits_error(error, text, function_name);
            }
            return set_geojson_option_error(error, text, function_name);
        }
        if (floor(value) != value) {
            return set_incorrect_argument_type_error(error, argument_name, function_name);
        }
    } else {
        if (argument->byte_count == SIZE_MAX) {
            return set_nomem_error(error);
        }
        text = malloc(argument->byte_count + 1U);
        if (text == NULL) {
            return set_nomem_error(error);
        }
        if (argument->byte_count != 0U) {
            memcpy(text, argument->bytes, argument->byte_count);
        }
        text[argument->byte_count] = '\0';
        errno = 0;
        value = strtod(text, &end);
        if (end == text) {
            value = 0.0;
        } else if (errno == ERANGE || !isfinite(value)) {
            int rc = strcmp(argument_name, "max decimal digits") == 0
                         ? set_geojson_max_dec_digits_error(error, text, function_name)
                         : set_geojson_option_error(error, text, function_name);

            free(text);
            return rc;
        }
        if (value < 0.0 || value > (double)UINT32_MAX) {
            int rc = strcmp(argument_name, "max decimal digits") == 0
                         ? set_geojson_max_dec_digits_error(error, text, function_name)
                         : set_geojson_option_error(error, text, function_name);

            free(text);
            return rc;
        }
    }
    *out_value = (uint32_t)value;
    if (!argument->has_numeric) {
        free(text);
    }
    return 0;
}

static int argument_geojson_srid(
    const struct mylite_spatial_argument *argument,
    uint32_t *out_srid,
    bool *out_is_null,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    char *text = NULL;
    char *end = NULL;
    double value = 0.0;
    bool from_numeric_argument = false;

    if (out_srid == NULL || out_is_null == NULL) {
        return set_invalid_gis_data_error(error, function_name);
    }
    *out_srid = 0U;
    *out_is_null = argument == NULL || argument->is_null;
    if (*out_is_null) {
        return 0;
    }
    if (argument->has_numeric) {
        value = argument->numeric;
        from_numeric_argument = true;
    } else {
        if (argument->byte_count == SIZE_MAX) {
            return set_nomem_error(error);
        }
        text = malloc(argument->byte_count + 1U);
        if (text == NULL) {
            return set_nomem_error(error);
        }
        if (argument->byte_count != 0U) {
            memcpy(text, argument->bytes, argument->byte_count);
        }
        text[argument->byte_count] = '\0';
        errno = 0;
        value = strtod(text, &end);
        if (end == text) {
            value = 0.0;
        }
        free(text);
    }
    if (!isfinite(value) || value < 0.0 || value > (double)UINT32_MAX ||
        (from_numeric_argument && floor(value) != value)) {
        char diagnostic_function_name[spatial_diagnostic_function_name_capacity];

        return set_spatial_error(
            error,
            mysql_error_numeric_value_out_of_range,
            "22003",
            "SRID value is out of range in '%s'",
            spatial_diagnostic_function_name(
                function_name,
                diagnostic_function_name,
                sizeof(diagnostic_function_name)
            )
        );
    }
    *out_srid = (uint32_t)value;
    return 0;
}

static int argument_index(
    const struct mylite_spatial_argument *argument,
    uint32_t *out_index,
    bool *out_is_null,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    double value = 0.0;

    if (out_index == NULL || out_is_null == NULL) {
        return set_invalid_gis_data_error(error, function_name);
    }
    *out_index = 0U;
    *out_is_null = argument == NULL || argument->is_null;
    if (*out_is_null) {
        return 0;
    }
    if (argument_numeric(argument, &value, error, function_name) != 0) {
        return -1;
    }
    if (value < 1.0 || value > (double)UINT32_MAX || floor(value) != value) {
        return 0;
    }
    *out_index = (uint32_t)value;
    return 0;
}

static int assign_null_result(struct mylite_spatial_result *out_result) {
    *out_result = (struct mylite_spatial_result){.kind = MYLITE_SPATIAL_RESULT_NULL};
    return 0;
}

static int assign_integer_result(struct mylite_spatial_result *out_result, int64_t value) {
    *out_result = (struct mylite_spatial_result){
        .kind = MYLITE_SPATIAL_RESULT_INTEGER,
        .integer = value,
    };
    return 0;
}

static int assign_double_result(struct mylite_spatial_result *out_result, double value) {
    *out_result = (struct mylite_spatial_result){
        .kind = MYLITE_SPATIAL_RESULT_DOUBLE,
        .real = value,
    };
    return 0;
}

static int assign_owned_bytes_result(
    struct mylite_spatial_result *out_result,
    enum mylite_spatial_result_kind kind,
    unsigned char *bytes, // NOLINT(readability-non-const-parameter): transfers ownership.
    size_t byte_count
) {
    *out_result = (struct mylite_spatial_result){
        .kind = kind,
        .bytes = bytes,
        .byte_count = byte_count,
    };
    return 0;
}

static int assign_copied_bytes_result(
    struct mylite_spatial_result *out_result,
    enum mylite_spatial_result_kind kind,
    const void *bytes,
    size_t byte_count,
    struct mylite_spatial_error *error
) {
    unsigned char *copy = (unsigned char *)malloc(byte_count == 0U ? 1U : byte_count);

    if (copy == NULL) {
        return set_nomem_error(error);
    }
    if (byte_count != 0U) {
        memcpy(copy, bytes, byte_count);
    }
    return assign_owned_bytes_result(out_result, kind, copy, byte_count);
}

static int assign_copied_text_result(
    struct mylite_spatial_result *out_result,
    const char *text,
    size_t text_size,
    struct mylite_spatial_error *error
) {
    return assign_copied_bytes_result(
        out_result,
        MYLITE_SPATIAL_RESULT_TEXT,
        text,
        text_size,
        error
    );
}

static int make_internal_geometry_from_wkb(
    const unsigned char *wkb,
    size_t wkb_size, // NOLINT(bugprone-easily-swappable-parameters): WKB payload then SRID.
    uint32_t srid,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    struct mylite_spatial_error *error
) {
    struct spatial_buffer buffer = {0};
    int rc = append_internal_prefix(&buffer, srid);

    if (rc == 0) {
        rc = spatial_buffer_append(&buffer, wkb, wkb_size);
    }
    if (rc != 0) {
        spatial_buffer_deinit(&buffer);
        return set_nomem_error(error);
    }
    *out_bytes = buffer.bytes;
    *out_byte_count = buffer.size;
    return 0;
}

static int make_point_internal_geometry(
    double coordinate_x,
    double coordinate_y,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    struct mylite_spatial_error *error
) {
    return make_point_internal_geometry_with_srid(
        0U,
        coordinate_x,
        coordinate_y,
        out_bytes,
        out_byte_count,
        error
    );
}

static int make_point_internal_geometry_with_srid(
    uint32_t srid, // NOLINT(bugprone-easily-swappable-parameters): internal point builder.
    double coordinate_x,
    double coordinate_y,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    struct mylite_spatial_error *error
) {
    struct spatial_buffer buffer = {0};
    int rc = append_internal_prefix(&buffer, srid);

    if (rc == 0) {
        rc = spatial_buffer_append_byte(&buffer, 1U);
    }
    if (rc == 0) {
        rc = spatial_buffer_append_u32_le(&buffer, (uint32_t)MYLITE_SPATIAL_GEOMETRY_POINT);
    }
    if (rc == 0) {
        rc = spatial_buffer_append_double_le(&buffer, coordinate_x);
    }
    if (rc == 0) {
        rc = spatial_buffer_append_double_le(&buffer, coordinate_y);
    }
    if (rc != 0) {
        spatial_buffer_deinit(&buffer);
        return set_nomem_error(error);
    }
    *out_bytes = buffer.bytes;
    *out_byte_count = buffer.size;
    return 0;
}

static int make_linestring_internal_geometry(
    const struct spatial_point *points,
    uint32_t point_count,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    struct mylite_spatial_error *error
) {
    struct spatial_buffer buffer = {0};
    int rc = append_internal_prefix(&buffer, 0U);

    if (rc == 0) {
        rc = spatial_buffer_append_byte(&buffer, 1U);
    }
    if (rc == 0) {
        rc = spatial_buffer_append_u32_le(&buffer, (uint32_t)MYLITE_SPATIAL_GEOMETRY_LINESTRING);
    }
    if (rc == 0) {
        rc = spatial_buffer_append_u32_le(&buffer, point_count);
    }
    for (uint32_t index = 0U; rc == 0 && index < point_count; ++index) {
        rc = spatial_buffer_append_double_le(&buffer, points[index].coordinate_x);
        if (rc == 0) {
            rc = spatial_buffer_append_double_le(&buffer, points[index].coordinate_y);
        }
    }
    if (rc != 0) {
        spatial_buffer_deinit(&buffer);
        return set_nomem_error(error);
    }
    *out_bytes = buffer.bytes;
    *out_byte_count = buffer.size;
    return 0;
}

static int make_polygon_internal_geometry_with_srid(
    uint32_t srid, // NOLINT(bugprone-easily-swappable-parameters): internal polygon builder.
    const struct spatial_point *points,
    uint32_t point_count,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    struct mylite_spatial_error *error
) {
    struct spatial_buffer buffer = {0};
    int rc = append_internal_prefix(&buffer, srid);

    if (points == NULL || point_count < 3U) {
        spatial_buffer_deinit(&buffer);
        return set_invalid_gis_data_error(error, "st_convexhull");
    }
    if (point_count == UINT32_MAX) {
        spatial_buffer_deinit(&buffer);
        return set_nomem_error(error);
    }
    if (rc == 0) {
        rc = spatial_buffer_append_byte(&buffer, 1U);
    }
    if (rc == 0) {
        rc = spatial_buffer_append_u32_le(&buffer, (uint32_t)MYLITE_SPATIAL_GEOMETRY_POLYGON);
    }
    if (rc == 0) {
        rc = spatial_buffer_append_u32_le(&buffer, 1U);
    }
    if (rc == 0) {
        rc = spatial_buffer_append_u32_le(&buffer, point_count + 1U);
    }
    for (uint32_t index = 0U; rc == 0 && index < point_count; ++index) {
        rc = spatial_buffer_append_double_le(&buffer, points[index].coordinate_x);
        if (rc == 0) {
            rc = spatial_buffer_append_double_le(&buffer, points[index].coordinate_y);
        }
    }
    if (rc == 0) {
        rc = spatial_buffer_append_double_le(&buffer, points[0].coordinate_x);
    }
    if (rc == 0) {
        rc = spatial_buffer_append_double_le(&buffer, points[0].coordinate_y);
    }
    if (rc != 0) {
        spatial_buffer_deinit(&buffer);
        return set_nomem_error(error);
    }
    *out_bytes = buffer.bytes;
    *out_byte_count = buffer.size;
    return 0;
}

static int make_multipoint_internal_geometry_with_srid(
    uint32_t srid, // NOLINT(bugprone-easily-swappable-parameters): internal collection builder.
    const struct spatial_point *points,
    uint32_t point_count,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    struct mylite_spatial_error *error
) {
    struct spatial_buffer buffer = {0};
    int rc = append_internal_prefix(&buffer, srid);

    if (rc == 0) {
        rc = spatial_buffer_append_byte(&buffer, 1U);
    }
    if (rc == 0) {
        rc = spatial_buffer_append_u32_le(&buffer, (uint32_t)MYLITE_SPATIAL_GEOMETRY_MULTIPOINT);
    }
    if (rc == 0) {
        rc = spatial_buffer_append_u32_le(&buffer, point_count);
    }
    for (uint32_t index = 0U; rc == 0 && index < point_count; ++index) {
        rc = spatial_buffer_append_byte(&buffer, 1U);
        if (rc == 0) {
            rc = spatial_buffer_append_u32_le(&buffer, (uint32_t)MYLITE_SPATIAL_GEOMETRY_POINT);
        }
        if (rc == 0) {
            rc = spatial_buffer_append_double_le(&buffer, points[index].coordinate_x);
        }
        if (rc == 0) {
            rc = spatial_buffer_append_double_le(&buffer, points[index].coordinate_y);
        }
    }
    if (rc != 0) {
        spatial_buffer_deinit(&buffer);
        return set_nomem_error(error);
    }
    *out_bytes = buffer.bytes;
    *out_byte_count = buffer.size;
    return 0;
}

static int make_envelope_internal_geometry(
    const struct spatial_box *box,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    struct mylite_spatial_error *error
) {
    struct spatial_buffer buffer = {0};
    int rc = append_internal_prefix(&buffer, 0U);

    if (box == NULL) {
        return set_invalid_gis_data_error(error, "st_envelope");
    }
    if (rc == 0 && !box->has_value) {
        rc = spatial_buffer_append_byte(&buffer, 1U);
        if (rc == 0) {
            rc = spatial_buffer_append_u32_le(
                &buffer,
                (uint32_t)MYLITE_SPATIAL_GEOMETRY_GEOMETRYCOLLECTION
            );
        }
        if (rc == 0) {
            rc = spatial_buffer_append_u32_le(&buffer, 0U);
        }
    } else if (rc == 0 && box->min_x == box->max_x && box->min_y == box->max_y) {
        spatial_buffer_deinit(&buffer);
        return make_point_internal_geometry(
            box->min_x,
            box->min_y,
            out_bytes,
            out_byte_count,
            error
        );
    } else if (rc == 0 && (box->min_x == box->max_x || box->min_y == box->max_y)) {
        const struct spatial_point points[] = {
            {.coordinate_x = box->min_x, .coordinate_y = box->min_y},
            {.coordinate_x = box->max_x, .coordinate_y = box->max_y},
        };

        spatial_buffer_deinit(&buffer);
        return make_linestring_internal_geometry(points, 2U, out_bytes, out_byte_count, error);
    } else if (rc == 0) {
        rc = append_polygon_rectangle_wkb(&buffer, box);
    }
    if (rc != 0) {
        spatial_buffer_deinit(&buffer);
        return set_nomem_error(error);
    }
    *out_bytes = buffer.bytes;
    *out_byte_count = buffer.size;
    return 0;
}

static int append_polygon_rectangle_wkb(
    struct spatial_buffer *buffer,
    const struct spatial_box *box
) {
    const struct spatial_point points[] = {
        {.coordinate_x = box->min_x, .coordinate_y = box->min_y},
        {.coordinate_x = box->max_x, .coordinate_y = box->min_y},
        {.coordinate_x = box->max_x, .coordinate_y = box->max_y},
        {.coordinate_x = box->min_x, .coordinate_y = box->max_y},
        {.coordinate_x = box->min_x, .coordinate_y = box->min_y},
    };
    int rc = spatial_buffer_append_byte(buffer, 1U);

    if (rc == 0) {
        rc = spatial_buffer_append_u32_le(buffer, (uint32_t)MYLITE_SPATIAL_GEOMETRY_POLYGON);
    }
    if (rc == 0) {
        rc = spatial_buffer_append_u32_le(buffer, 1U);
    }
    if (rc == 0) {
        rc = spatial_buffer_append_u32_le(buffer, spatial_rectangle_ring_point_count);
    }
    for (size_t index = 0U; rc == 0 && index < sizeof(points) / sizeof(points[0]); ++index) {
        rc = spatial_buffer_append_double_le(buffer, points[index].coordinate_x);
        if (rc == 0) {
            rc = spatial_buffer_append_double_le(buffer, points[index].coordinate_y);
        }
    }
    return rc;
}

static int wkb_length_at(
    struct spatial_wkb_cursor *cursor,
    double *out_length,
    bool *out_supported,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    bool little_endian = false;
    enum mylite_spatial_geometry_type type = MYLITE_SPATIAL_GEOMETRY_NONE;
    uint32_t count = 0U;
    int rc = cursor_read_header(cursor, &little_endian, &type, error, function_name);

    if (rc != 0) {
        return rc;
    }
    if (type == MYLITE_SPATIAL_GEOMETRY_LINESTRING) {
        struct spatial_point previous = {0};

        *out_supported = true;
        rc = cursor_read_u32(cursor, little_endian, &count, error, function_name);
        for (uint32_t index = 0U; rc == 0 && index < count; ++index) {
            struct spatial_point current = {0};

            rc = cursor_read_double(
                cursor,
                little_endian,
                &current.coordinate_x,
                error,
                function_name
            );
            if (rc == 0) {
                rc = cursor_read_double(
                    cursor,
                    little_endian,
                    &current.coordinate_y,
                    error,
                    function_name
                );
            }
            if (rc == 0 && index != 0U) {
                double delta_x = current.coordinate_x - previous.coordinate_x;
                double delta_y = current.coordinate_y - previous.coordinate_y;

                *out_length += sqrt((delta_x * delta_x) + (delta_y * delta_y));
            }
            previous = current;
        }
        return rc;
    }
    if (type == MYLITE_SPATIAL_GEOMETRY_MULTILINESTRING) {
        *out_supported = true;
        rc = cursor_read_u32(cursor, little_endian, &count, error, function_name);
        for (uint32_t index = 0U; rc == 0 && index < count; ++index) {
            rc = wkb_length_at(cursor, out_length, out_supported, error, function_name);
        }
        return rc;
    }
    (void)count;
    return 0;
}

static int wkb_area_at(
    struct spatial_wkb_cursor *cursor,
    double *out_area,
    bool *out_supported,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    bool little_endian = false;
    enum mylite_spatial_geometry_type type = MYLITE_SPATIAL_GEOMETRY_NONE;
    uint32_t count = 0U;
    int rc = cursor_read_header(cursor, &little_endian, &type, error, function_name);

    if (rc != 0) {
        return rc;
    }
    if (type == MYLITE_SPATIAL_GEOMETRY_POLYGON) {
        double polygon_area = 0.0;

        *out_supported = true;
        rc = cursor_read_u32(cursor, little_endian, &count, error, function_name);
        for (uint32_t ring_index = 0U; rc == 0 && ring_index < count; ++ring_index) {
            uint32_t point_count = 0U;
            struct spatial_point first = {0};
            struct spatial_point previous = {0};
            double ring_twice_area = 0.0;

            rc = cursor_read_u32(cursor, little_endian, &point_count, error, function_name);
            for (uint32_t point_index = 0U; rc == 0 && point_index < point_count; ++point_index) {
                struct spatial_point current = {0};

                rc = cursor_read_double(
                    cursor,
                    little_endian,
                    &current.coordinate_x,
                    error,
                    function_name
                );
                if (rc == 0) {
                    rc = cursor_read_double(
                        cursor,
                        little_endian,
                        &current.coordinate_y,
                        error,
                        function_name
                    );
                }
                if (rc == 0 && point_index == 0U) {
                    first = current;
                }
                if (rc == 0 && point_index != 0U) {
                    ring_twice_area += (previous.coordinate_x * current.coordinate_y) -
                                       (current.coordinate_x * previous.coordinate_y);
                }
                previous = current;
            }
            if (rc == 0 && point_count > 1U) {
                ring_twice_area += (previous.coordinate_x * first.coordinate_y) -
                                   (first.coordinate_x * previous.coordinate_y);
                if (ring_index == 0U) {
                    polygon_area += fabs(ring_twice_area) / spatial_shoelace_area_divisor;
                } else {
                    polygon_area -= fabs(ring_twice_area) / spatial_shoelace_area_divisor;
                }
            }
        }
        if (rc == 0) {
            *out_area += polygon_area;
        }
        return rc;
    }
    if (type == MYLITE_SPATIAL_GEOMETRY_MULTIPOLYGON) {
        *out_supported = true;
        rc = cursor_read_u32(cursor, little_endian, &count, error, function_name);
        for (uint32_t index = 0U; rc == 0 && index < count; ++index) {
            rc = wkb_area_at(cursor, out_area, out_supported, error, function_name);
        }
        return rc;
    }
    return 0;
}

static int distance_geometry_from_view(
    const struct spatial_geometry_view *view,
    struct spatial_distance_geometry *out_geometry,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    struct spatial_wkb_cursor cursor = {0};
    int rc = 0;

    if (view == NULL || out_geometry == NULL) {
        return set_invalid_gis_data_error(error, function_name);
    }
    *out_geometry = (struct spatial_distance_geometry){0};
    cursor = (struct spatial_wkb_cursor){.bytes = view->wkb, .size = view->wkb_size};
    rc = distance_geometry_read(&cursor, out_geometry, error, function_name);
    if (rc != 0 || cursor.offset != cursor.size) {
        distance_geometry_deinit(out_geometry);
        return rc != 0 ? rc : set_invalid_gis_data_error(error, function_name);
    }
    return 0;
}

static int point_collection_from_geometry(
    const struct spatial_geometry_view *geometry,
    struct spatial_point_collection *out_collection,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    struct spatial_wkb_cursor cursor = {0};
    bool little_endian = false;
    enum mylite_spatial_geometry_type type = MYLITE_SPATIAL_GEOMETRY_NONE;
    int rc = 0;

    if (geometry == NULL || out_collection == NULL) {
        return set_invalid_gis_data_error(error, function_name);
    }
    *out_collection = (struct spatial_point_collection){.type = geometry->type};
    if (geometry->type == MYLITE_SPATIAL_GEOMETRY_POINT) {
        out_collection->points = calloc(1U, sizeof(*out_collection->points));
        if (out_collection->points == NULL) {
            return set_nomem_error(error);
        }
        out_collection->point_count = 1U;
        rc = geometry_point_coordinates(
            geometry->wkb,
            geometry->wkb_size,
            &out_collection->points[0].coordinate_x,
            &out_collection->points[0].coordinate_y,
            error,
            function_name
        );
        if (rc != 0) {
            point_collection_deinit(out_collection);
        }
        return rc;
    }
    if (geometry->type != MYLITE_SPATIAL_GEOMETRY_MULTIPOINT) {
        return set_invalid_gis_data_error(error, function_name);
    }
    cursor = (struct spatial_wkb_cursor){.bytes = geometry->wkb, .size = geometry->wkb_size};
    rc = cursor_read_header(&cursor, &little_endian, &type, error, function_name);
    if (rc == 0 && type != MYLITE_SPATIAL_GEOMETRY_MULTIPOINT) {
        rc = set_invalid_gis_data_error(error, function_name);
    }
    if (rc == 0) {
        rc = point_collection_read_multipoint(
            &cursor,
            little_endian,
            out_collection,
            error,
            function_name
        );
    }
    if (rc == 0 && cursor.offset != cursor.size) {
        rc = set_invalid_gis_data_error(error, function_name);
    }
    if (rc != 0) {
        point_collection_deinit(out_collection);
    }
    return rc;
}

static int point_collection_read_multipoint(
    struct spatial_wkb_cursor *cursor,
    bool little_endian,
    struct spatial_point_collection *out_collection,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    struct spatial_point *points = NULL;
    uint32_t point_count = 0U;
    int rc = cursor_read_u32(cursor, little_endian, &point_count, error, function_name);

    if (rc != 0) {
        return rc;
    }
    if (point_count == 0U) {
        return 0;
    }
    if ((size_t)point_count > SIZE_MAX / sizeof(*points)) {
        return set_nomem_error(error);
    }
    points = calloc((size_t)point_count, sizeof(*points));
    if (points == NULL) {
        return set_nomem_error(error);
    }
    for (uint32_t index = 0U; rc == 0 && index < point_count; ++index) {
        bool point_little_endian = false;
        enum mylite_spatial_geometry_type point_type = MYLITE_SPATIAL_GEOMETRY_NONE;

        rc = cursor_read_header(cursor, &point_little_endian, &point_type, error, function_name);
        if (rc == 0 && point_type != MYLITE_SPATIAL_GEOMETRY_POINT) {
            rc = set_invalid_gis_data_error(error, function_name);
        }
        if (rc == 0) {
            rc = cursor_read_double(
                cursor,
                point_little_endian,
                &points[index].coordinate_x,
                error,
                function_name
            );
        }
        if (rc == 0) {
            rc = cursor_read_double(
                cursor,
                point_little_endian,
                &points[index].coordinate_y,
                error,
                function_name
            );
        }
    }
    if (rc != 0) {
        free(points);
        return rc;
    }
    out_collection->points = points;
    out_collection->point_count = point_count;
    return 0;
}

static void point_collection_deinit(struct spatial_point_collection *collection) {
    if (collection == NULL) {
        return;
    }
    free(collection->points);
    *collection = (struct spatial_point_collection){0};
}

static int validate_distance_sphere_coordinates(
    const struct spatial_point_collection *collection,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    if (collection == NULL) {
        return set_invalid_gis_data_error(error, function_name);
    }
    for (uint32_t index = 0U; index < collection->point_count; ++index) {
        double longitude = collection->points[index].coordinate_x;
        double latitude = collection->points[index].coordinate_y;

        if (longitude <= spatial_geohash_longitude_min ||
            longitude > spatial_geohash_longitude_max || !isfinite(longitude)) {
            return set_distance_sphere_longitude_error(error, longitude, function_name);
        }
        if (latitude < spatial_geohash_latitude_min || latitude > spatial_geohash_latitude_max ||
            !isfinite(latitude)) {
            return set_distance_sphere_latitude_error(error, latitude, function_name);
        }
    }
    return 0;
}

static double point_distance_sphere(
    const struct spatial_point *left,
    const struct spatial_point *right,
    double radius
) {
    double pi = acos(-1.0);
    double left_longitude = left->coordinate_x * pi / spatial_degrees_to_radians_divisor;
    double left_latitude = left->coordinate_y * pi / spatial_degrees_to_radians_divisor;
    double right_longitude = right->coordinate_x * pi / spatial_degrees_to_radians_divisor;
    double right_latitude = right->coordinate_y * pi / spatial_degrees_to_radians_divisor;
    double delta_longitude = right_longitude - left_longitude;
    double delta_latitude = right_latitude - left_latitude;
    double sin_half_latitude = sin(delta_latitude / spatial_haversine_half_divisor);
    double sin_half_longitude = sin(delta_longitude / spatial_haversine_half_divisor);
    double haversine =
        (sin_half_latitude * sin_half_latitude) +
        (cos(left_latitude) * cos(right_latitude) * sin_half_longitude * sin_half_longitude);

    if (haversine < 0.0) {
        haversine = 0.0;
    } else if (haversine > 1.0) {
        haversine = 1.0;
    }
    return radius * spatial_haversine_half_divisor * asin(sqrt(haversine));
}

static int distance_sphere_between_collections(
    const struct spatial_point_collection *left,
    const struct spatial_point_collection *right,
    double radius,
    double *out_distance,
    bool *out_has_distance
) {
    if (left == NULL || right == NULL || out_distance == NULL || out_has_distance == NULL) {
        return -1;
    }
    *out_distance = 0.0;
    *out_has_distance = false;
    for (uint32_t left_index = 0U; left_index < left->point_count; ++left_index) {
        for (uint32_t right_index = 0U; right_index < right->point_count; ++right_index) {
            double distance = point_distance_sphere(
                &left->points[left_index],
                &right->points[right_index],
                radius
            );

            if (!*out_has_distance || distance < *out_distance) {
                *out_distance = distance;
                *out_has_distance = true;
            }
        }
    }
    return 0;
}

static bool frechet_distance_supports_types(
    enum mylite_spatial_geometry_type left_type, // NOLINT(bugprone-easily-swappable-parameters)
    enum mylite_spatial_geometry_type right_type
) {
    return left_type == MYLITE_SPATIAL_GEOMETRY_LINESTRING &&
           right_type == MYLITE_SPATIAL_GEOMETRY_LINESTRING;
}

static bool hausdorff_distance_supports_types(
    enum mylite_spatial_geometry_type left_type, // NOLINT(bugprone-easily-swappable-parameters)
    enum mylite_spatial_geometry_type right_type
) {
    if (left_type == MYLITE_SPATIAL_GEOMETRY_LINESTRING) {
        return right_type == MYLITE_SPATIAL_GEOMETRY_LINESTRING ||
               right_type == MYLITE_SPATIAL_GEOMETRY_MULTILINESTRING;
    }
    if (left_type == MYLITE_SPATIAL_GEOMETRY_MULTILINESTRING) {
        return right_type == MYLITE_SPATIAL_GEOMETRY_LINESTRING ||
               right_type == MYLITE_SPATIAL_GEOMETRY_MULTILINESTRING;
    }
    if (left_type == MYLITE_SPATIAL_GEOMETRY_POINT) {
        return right_type == MYLITE_SPATIAL_GEOMETRY_MULTIPOINT;
    }
    if (left_type == MYLITE_SPATIAL_GEOMETRY_MULTIPOINT) {
        return right_type == MYLITE_SPATIAL_GEOMETRY_POINT ||
               right_type == MYLITE_SPATIAL_GEOMETRY_MULTIPOINT;
    }
    return false;
}

static int discrete_point_set_from_geometry(
    const struct spatial_distance_geometry *geometry,
    struct spatial_discrete_point_set *out_set,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    size_t point_count = 0U;
    uint32_t offset = 0U;
    int rc = 0;

    if (geometry == NULL || out_set == NULL) {
        return set_invalid_gis_data_error(error, function_name);
    }
    *out_set = (struct spatial_discrete_point_set){0};
    point_count = discrete_point_set_count_geometry(geometry);
    if (point_count == 0U) {
        return 0;
    }
    if (point_count > UINT32_MAX || point_count > SIZE_MAX / sizeof(*out_set->points)) {
        return set_nomem_error(error);
    }
    out_set->points = calloc(point_count, sizeof(*out_set->points));
    if (out_set->points == NULL) {
        return set_nomem_error(error);
    }
    out_set->point_count = (uint32_t)point_count;
    rc = discrete_point_set_append_geometry(geometry, out_set, &offset, error, function_name);
    if (rc == 0 && offset != out_set->point_count) {
        rc = set_invalid_gis_data_error(error, function_name);
    }
    if (rc != 0) {
        discrete_point_set_deinit(out_set);
    }
    return rc;
}

static size_t discrete_point_set_count_geometry(const struct spatial_distance_geometry *geometry) {
    size_t point_count = 0U;

    if (geometry == NULL) {
        return 0U;
    }
    switch (geometry->type) {
    case MYLITE_SPATIAL_GEOMETRY_POINT:
        return 1U;
    case MYLITE_SPATIAL_GEOMETRY_LINESTRING:
        return geometry->point_count;
    case MYLITE_SPATIAL_GEOMETRY_MULTIPOINT:
    case MYLITE_SPATIAL_GEOMETRY_MULTILINESTRING:
        for (uint32_t index = 0U; index < geometry->child_count; ++index) {
            size_t child_count = discrete_point_set_count_geometry(&geometry->children[index]);

            if (child_count > SIZE_MAX - point_count) {
                return SIZE_MAX;
            }
            point_count += child_count;
        }
        return point_count;
    default:
        break;
    }
    return 0U;
}

static int discrete_point_set_append_geometry(
    const struct spatial_distance_geometry *geometry,
    struct spatial_discrete_point_set *set,
    uint32_t *io_offset,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    if (geometry == NULL || set == NULL || io_offset == NULL) {
        return set_invalid_gis_data_error(error, function_name);
    }
    switch (geometry->type) {
    case MYLITE_SPATIAL_GEOMETRY_POINT:
        if (*io_offset >= set->point_count) {
            return set_invalid_gis_data_error(error, function_name);
        }
        set->points[*io_offset] = geometry->point;
        ++(*io_offset);
        return 0;
    case MYLITE_SPATIAL_GEOMETRY_LINESTRING:
        if (geometry->point_count > set->point_count - *io_offset) {
            return set_invalid_gis_data_error(error, function_name);
        }
        memcpy(
            set->points + *io_offset,
            geometry->points,
            (size_t)geometry->point_count * sizeof(*set->points)
        );
        *io_offset += geometry->point_count;
        return 0;
    case MYLITE_SPATIAL_GEOMETRY_MULTIPOINT:
    case MYLITE_SPATIAL_GEOMETRY_MULTILINESTRING:
        for (uint32_t index = 0U; index < geometry->child_count; ++index) {
            int rc = discrete_point_set_append_geometry(
                &geometry->children[index],
                set,
                io_offset,
                error,
                function_name
            );

            if (rc != 0) {
                return rc;
            }
        }
        return 0;
    default:
        break;
    }
    return set_invalid_gis_data_error(error, function_name);
}

static void discrete_point_set_deinit(struct spatial_discrete_point_set *set) {
    if (set == NULL) {
        return;
    }
    free(set->points);
    *set = (struct spatial_discrete_point_set){0};
}

static int frechet_distance_between_lines(
    const struct spatial_distance_geometry *left,
    const struct spatial_distance_geometry *right,
    double *out_distance,
    struct mylite_spatial_error *error
) {
    double *row_a = NULL;
    double *row_b = NULL;
    double *previous = NULL;
    double *current = NULL;
    size_t right_count = 0U;

    if (left == NULL || right == NULL || out_distance == NULL || left->point_count == 0U ||
        right->point_count == 0U) {
        return -1;
    }
    right_count = right->point_count;
    if (right_count > SIZE_MAX / sizeof(*row_a)) {
        return set_nomem_error(error);
    }
    row_a = calloc(right_count, sizeof(*row_a));
    row_b = calloc(right_count, sizeof(*row_b));
    if (row_a == NULL || row_b == NULL) {
        free(row_a);
        free(row_b);
        return set_nomem_error(error);
    }
    previous = row_a;
    current = row_b;
    previous[0] = distance_point_to_point(&left->points[0], &right->points[0]);
    for (size_t right_index = 1U; right_index < right_count; ++right_index) {
        double point_distance =
            distance_point_to_point(&left->points[0], &right->points[right_index]);

        previous[right_index] = fmax(previous[right_index - 1U], point_distance);
    }
    for (uint32_t left_index = 1U; left_index < left->point_count; ++left_index) {
        double point_distance =
            distance_point_to_point(&left->points[left_index], &right->points[0]);

        current[0] = fmax(previous[0], point_distance);
        for (size_t right_index = 1U; right_index < right_count; ++right_index) {
            double prior = fmin(
                fmin(previous[right_index], previous[right_index - 1U]),
                current[right_index - 1U]
            );

            point_distance =
                distance_point_to_point(&left->points[left_index], &right->points[right_index]);
            current[right_index] = fmax(prior, point_distance);
        }
        double *swap = previous;

        previous = current;
        current = swap;
    }
    *out_distance = previous[right_count - 1U];
    free(row_a);
    free(row_b);
    return 0;
}

static int hausdorff_distance_between_supported_geometries(
    const struct spatial_distance_geometry *left,
    const struct spatial_distance_geometry *right,
    double *out_distance,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    struct spatial_discrete_point_set left_points = {0};
    struct spatial_discrete_point_set right_points = {0};
    int rc = 0;

    if (left == NULL || right == NULL || out_distance == NULL) {
        return set_invalid_gis_data_error(error, function_name);
    }
    if (left->type == MYLITE_SPATIAL_GEOMETRY_LINESTRING &&
        right->type == MYLITE_SPATIAL_GEOMETRY_LINESTRING) {
        return hausdorff_distance_between_line_points(
            left,
            right,
            out_distance,
            error,
            function_name
        );
    }
    if (left->type == MYLITE_SPATIAL_GEOMETRY_POINT &&
        right->type == MYLITE_SPATIAL_GEOMETRY_MULTIPOINT) {
        return hausdorff_distance_point_to_discrete_geometry(
            &left->point,
            right,
            out_distance,
            error,
            function_name
        );
    }
    if (left->type == MYLITE_SPATIAL_GEOMETRY_MULTIPOINT &&
        right->type == MYLITE_SPATIAL_GEOMETRY_POINT) {
        return hausdorff_distance_point_to_discrete_geometry(
            &right->point,
            left,
            out_distance,
            error,
            function_name
        );
    }
    if (left->type == MYLITE_SPATIAL_GEOMETRY_MULTIPOINT &&
        right->type == MYLITE_SPATIAL_GEOMETRY_MULTIPOINT) {
        rc = discrete_point_set_from_geometry(left, &left_points, error, function_name);
        if (rc == 0) {
            rc = discrete_point_set_from_geometry(right, &right_points, error, function_name);
        }
        if (rc == 0) {
            rc = hausdorff_distance_between_point_sets(&left_points, &right_points, out_distance);
        }
        discrete_point_set_deinit(&left_points);
        discrete_point_set_deinit(&right_points);
        return rc == 0 ? 0 : set_invalid_gis_data_error(error, function_name);
    }
    if (left->type == MYLITE_SPATIAL_GEOMETRY_LINESTRING &&
        right->type == MYLITE_SPATIAL_GEOMETRY_MULTILINESTRING) {
        return hausdorff_distance_line_to_multiline(
            left,
            right,
            out_distance,
            error,
            function_name
        );
    }
    if (left->type == MYLITE_SPATIAL_GEOMETRY_MULTILINESTRING &&
        right->type == MYLITE_SPATIAL_GEOMETRY_LINESTRING) {
        return hausdorff_distance_line_to_multiline(
            right,
            left,
            out_distance,
            error,
            function_name
        );
    }
    if (left->type == MYLITE_SPATIAL_GEOMETRY_MULTILINESTRING &&
        right->type == MYLITE_SPATIAL_GEOMETRY_MULTILINESTRING) {
        return hausdorff_distance_multiline_to_multiline(
            left,
            right,
            out_distance,
            error,
            function_name
        );
    }
    return set_invalid_gis_data_error(error, function_name);
}

static int hausdorff_distance_line_to_multiline(
    const struct spatial_distance_geometry *line,
    const struct spatial_distance_geometry *multiline,
    double *out_distance,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    bool has_distance = false;
    double distance = 0.0;

    if (line == NULL || multiline == NULL || out_distance == NULL) {
        return set_invalid_gis_data_error(error, function_name);
    }
    for (uint32_t index = 0U; index < multiline->child_count; ++index) {
        double child_distance = 0.0;
        int rc = hausdorff_distance_between_line_points(
            line,
            &multiline->children[index],
            &child_distance,
            error,
            function_name
        );

        if (rc != 0) {
            return rc;
        }
        if (!has_distance || child_distance > distance) {
            distance = child_distance;
            has_distance = true;
        }
    }
    if (!has_distance) {
        return set_invalid_gis_data_error(error, function_name);
    }
    *out_distance = distance;
    return 0;
}

static int hausdorff_distance_multiline_to_multiline(
    const struct spatial_distance_geometry *left,
    const struct spatial_distance_geometry *right,
    double *out_distance,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    bool has_distance = false;
    double distance = 0.0;

    if (left == NULL || right == NULL || out_distance == NULL) {
        return set_invalid_gis_data_error(error, function_name);
    }
    for (uint32_t left_index = 0U; left_index < left->child_count; ++left_index) {
        for (uint32_t right_index = 0U; right_index < right->child_count; ++right_index) {
            double child_distance = 0.0;
            int rc = hausdorff_distance_between_line_points(
                &left->children[left_index],
                &right->children[right_index],
                &child_distance,
                error,
                function_name
            );

            if (rc != 0) {
                return rc;
            }
            if (!has_distance || child_distance > distance) {
                distance = child_distance;
                has_distance = true;
            }
        }
    }
    if (!has_distance) {
        return set_invalid_gis_data_error(error, function_name);
    }
    *out_distance = distance;
    return 0;
}

static int hausdorff_distance_point_to_discrete_geometry(
    const struct spatial_point *point,
    const struct spatial_distance_geometry *geometry,
    double *out_distance,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    struct spatial_discrete_point_set points = {0};
    double distance = DBL_MAX;
    int rc = 0;

    if (point == NULL || geometry == NULL || out_distance == NULL) {
        return set_invalid_gis_data_error(error, function_name);
    }
    rc = discrete_point_set_from_geometry(geometry, &points, error, function_name);
    if (rc != 0) {
        return rc;
    }
    for (uint32_t index = 0U; index < points.point_count; ++index) {
        double candidate = distance_point_to_point(point, &points.points[index]);

        if (candidate < distance) {
            distance = candidate;
        }
    }
    discrete_point_set_deinit(&points);
    if (distance == DBL_MAX) {
        return set_invalid_gis_data_error(error, function_name);
    }
    *out_distance = distance;
    return 0;
}

static int hausdorff_distance_between_line_points(
    const struct spatial_distance_geometry *left,
    const struct spatial_distance_geometry *right,
    double *out_distance,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    struct spatial_discrete_point_set left_points = {0};
    struct spatial_discrete_point_set right_points = {0};
    int rc = 0;

    if (left == NULL || right == NULL || out_distance == NULL ||
        left->type != MYLITE_SPATIAL_GEOMETRY_LINESTRING ||
        right->type != MYLITE_SPATIAL_GEOMETRY_LINESTRING || left->point_count == 0U ||
        right->point_count == 0U) {
        return set_invalid_gis_data_error(error, function_name);
    }
    left_points = (struct spatial_discrete_point_set){
        .points = left->points,
        .point_count = left->point_count,
    };
    right_points = (struct spatial_discrete_point_set){
        .points = right->points,
        .point_count = right->point_count,
    };
    rc = hausdorff_distance_between_point_sets(&left_points, &right_points, out_distance);
    return rc == 0 ? 0 : set_invalid_gis_data_error(error, function_name);
}

static int hausdorff_distance_between_point_sets(
    const struct spatial_discrete_point_set *left,
    const struct spatial_discrete_point_set *right,
    double *out_distance
) {
    double distance = 0.0;

    if (left == NULL || right == NULL || out_distance == NULL || left->point_count == 0U ||
        right->point_count == 0U) {
        return -1;
    }
    for (uint32_t left_index = 0U; left_index < left->point_count; ++left_index) {
        double nearest = DBL_MAX;

        for (uint32_t right_index = 0U; right_index < right->point_count; ++right_index) {
            double candidate =
                distance_point_to_point(&left->points[left_index], &right->points[right_index]);

            if (candidate < nearest) {
                nearest = candidate;
            }
        }
        if (nearest > distance) {
            distance = nearest;
        }
    }
    *out_distance = distance;
    return 0;
}

static int centroid_accumulate_geometry(
    const struct spatial_distance_geometry *geometry,
    struct spatial_centroid_accumulator *accumulator,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    if (geometry == NULL || accumulator == NULL) {
        return set_invalid_gis_data_error(error, function_name);
    }
    switch (geometry->type) {
    case MYLITE_SPATIAL_GEOMETRY_POINT:
        centroid_accumulate_point(accumulator, &geometry->point);
        return 0;
    case MYLITE_SPATIAL_GEOMETRY_LINESTRING:
        centroid_accumulate_line(geometry, accumulator);
        return 0;
    case MYLITE_SPATIAL_GEOMETRY_POLYGON:
        return centroid_accumulate_polygon(geometry, accumulator, error, function_name);
    case MYLITE_SPATIAL_GEOMETRY_MULTIPOINT:
    case MYLITE_SPATIAL_GEOMETRY_MULTILINESTRING:
    case MYLITE_SPATIAL_GEOMETRY_MULTIPOLYGON:
    case MYLITE_SPATIAL_GEOMETRY_GEOMETRYCOLLECTION:
        for (uint32_t index = 0U; index < geometry->child_count; ++index) {
            int rc = centroid_accumulate_geometry(
                &geometry->children[index],
                accumulator,
                error,
                function_name
            );

            if (rc != 0) {
                return rc;
            }
        }
        return 0;
    default:
        break;
    }
    return set_invalid_gis_data_error(error, function_name);
}

static void centroid_accumulate_point(
    struct spatial_centroid_accumulator *accumulator,
    const struct spatial_point *point
) {
    centroid_accumulator_add_weighted(accumulator, SPATIAL_CENTROID_DIMENSION_POINT, point, 1.0);
}

static void centroid_accumulate_line(
    const struct spatial_distance_geometry *line,
    struct spatial_centroid_accumulator *accumulator
) {
    if (line == NULL || accumulator == NULL || line->point_count == 0U) {
        return;
    }
    centroid_accumulator_add_fallback(
        accumulator,
        SPATIAL_CENTROID_DIMENSION_LINE,
        &line->points[0]
    );
    for (uint32_t index = 0U; index + 1U < line->point_count; ++index) {
        const struct spatial_point *start = &line->points[index];
        const struct spatial_point *end = &line->points[index + 1U];
        double delta_x = end->coordinate_x - start->coordinate_x;
        double delta_y = end->coordinate_y - start->coordinate_y;
        double length = sqrt((delta_x * delta_x) + (delta_y * delta_y));
        struct spatial_point midpoint = {
            .coordinate_x = (start->coordinate_x + end->coordinate_x) / spatial_midpoint_divisor,
            .coordinate_y = (start->coordinate_y + end->coordinate_y) / spatial_midpoint_divisor,
        };

        centroid_accumulator_add_weighted(
            accumulator,
            SPATIAL_CENTROID_DIMENSION_LINE,
            &midpoint,
            length
        );
    }
}

static int centroid_accumulate_polygon(
    const struct spatial_distance_geometry *polygon,
    struct spatial_centroid_accumulator *accumulator,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    struct spatial_point centroid = {0};
    double area = 0.0;
    int rc = centroid_polygon_value(polygon, &centroid, &area, error, function_name);

    if (rc != 0) {
        return rc;
    }
    centroid_accumulator_add_weighted(
        accumulator,
        SPATIAL_CENTROID_DIMENSION_POLYGON,
        &centroid,
        area
    );
    return 0;
}

static int centroid_polygon_value(
    const struct spatial_distance_geometry *polygon,
    struct spatial_point *out_centroid,
    double *out_area,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    double weighted_x = 0.0;
    double weighted_y = 0.0;
    double total_area = 0.0;

    if (polygon == NULL || out_centroid == NULL || out_area == NULL || polygon->ring_count == 0U) {
        return set_invalid_gis_data_error(error, function_name);
    }
    for (uint32_t ring_index = 0U; ring_index < polygon->ring_count; ++ring_index) {
        struct spatial_point ring_centroid = {0};
        double ring_area = 0.0;
        double sign = ring_index == 0U ? 1.0 : -1.0;
        int rc = centroid_ring_value(
            &polygon->rings[ring_index],
            &ring_centroid,
            &ring_area,
            error,
            function_name
        );

        if (rc != 0) {
            return rc;
        }
        total_area += sign * ring_area;
        weighted_x += sign * ring_area * ring_centroid.coordinate_x;
        weighted_y += sign * ring_area * ring_centroid.coordinate_y;
    }
    if (double_near_zero(total_area) || total_area < 0.0) {
        return set_invalid_gis_data_error(error, function_name);
    }
    *out_centroid = (struct spatial_point){
        .coordinate_x = weighted_x / total_area,
        .coordinate_y = weighted_y / total_area,
    };
    *out_area = total_area;
    if (!isfinite(out_centroid->coordinate_x) || !isfinite(out_centroid->coordinate_y)) {
        return set_invalid_gis_data_error(error, function_name);
    }
    return 0;
}

static int centroid_ring_value(
    const struct spatial_distance_ring *ring,
    struct spatial_point *out_centroid,
    double *out_area,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    double twice_area = 0.0;
    double centroid_x_numerator = 0.0;
    double centroid_y_numerator = 0.0;

    if (ring == NULL || out_centroid == NULL || out_area == NULL || ring->point_count < 4U) {
        return set_invalid_gis_data_error(error, function_name);
    }
    for (uint32_t index = 0U; index + 1U < ring->point_count; ++index) {
        const struct spatial_point *current = &ring->points[index];
        const struct spatial_point *next = &ring->points[index + 1U];
        double cross = (current->coordinate_x * next->coordinate_y) -
                       (next->coordinate_x * current->coordinate_y);

        twice_area += cross;
        centroid_x_numerator += (current->coordinate_x + next->coordinate_x) * cross;
        centroid_y_numerator += (current->coordinate_y + next->coordinate_y) * cross;
    }
    if (double_near_zero(twice_area)) {
        return set_invalid_gis_data_error(error, function_name);
    }
    *out_centroid = (struct spatial_point){
        .coordinate_x =
            centroid_x_numerator / (spatial_centroid_denominator_multiplier * twice_area),
        .coordinate_y =
            centroid_y_numerator / (spatial_centroid_denominator_multiplier * twice_area),
    };
    *out_area = fabs(twice_area) / spatial_shoelace_area_divisor;
    if (!isfinite(out_centroid->coordinate_x) || !isfinite(out_centroid->coordinate_y)) {
        return set_invalid_gis_data_error(error, function_name);
    }
    return 0;
}

static void centroid_accumulator_add_weighted(
    struct spatial_centroid_accumulator *accumulator,
    enum spatial_centroid_dimension dimension,
    const struct spatial_point *point,
    double weight
) {
    if (point == NULL || !centroid_accumulator_accepts(accumulator, dimension)) {
        return;
    }
    centroid_accumulator_add_fallback(accumulator, dimension, point);
    if (weight <= 0.0 || !isfinite(weight)) {
        return;
    }
    accumulator->weighted_x += point->coordinate_x * weight;
    accumulator->weighted_y += point->coordinate_y * weight;
    accumulator->weight += weight;
}

static void centroid_accumulator_add_fallback(
    struct spatial_centroid_accumulator *accumulator,
    enum spatial_centroid_dimension dimension,
    const struct spatial_point *point
) {
    if (point == NULL || !centroid_accumulator_accepts(accumulator, dimension)) {
        return;
    }
    if (!accumulator->has_fallback_point) {
        accumulator->fallback_point = *point;
        accumulator->has_fallback_point = true;
    }
}

static bool centroid_accumulator_accepts(
    struct spatial_centroid_accumulator *accumulator,
    enum spatial_centroid_dimension dimension
) {
    if (accumulator == NULL) {
        return false;
    }
    if (dimension < accumulator->dimension) {
        return false;
    }
    if (dimension > accumulator->dimension) {
        *accumulator = (struct spatial_centroid_accumulator){
            .dimension = dimension,
        };
    }
    return true;
}

static int convex_hull_point_set_from_geometry(
    const struct spatial_distance_geometry *geometry,
    struct spatial_discrete_point_set *out_set,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    size_t point_count = 0U;
    uint32_t offset = 0U;
    int rc = 0;

    if (geometry == NULL || out_set == NULL) {
        return set_invalid_gis_data_error(error, function_name);
    }
    *out_set = (struct spatial_discrete_point_set){0};
    point_count = convex_hull_point_set_count_geometry(geometry);
    if (point_count == 0U) {
        return 0;
    }
    if (point_count > UINT32_MAX || point_count > SIZE_MAX / sizeof(*out_set->points)) {
        return set_nomem_error(error);
    }
    out_set->points = calloc(point_count, sizeof(*out_set->points));
    if (out_set->points == NULL) {
        return set_nomem_error(error);
    }
    out_set->point_count = (uint32_t)point_count;
    rc = convex_hull_point_set_append_geometry(geometry, out_set, &offset, error, function_name);
    if (rc == 0 && offset != out_set->point_count) {
        rc = set_invalid_gis_data_error(error, function_name);
    }
    if (rc != 0) {
        discrete_point_set_deinit(out_set);
    }
    return rc;
}

static size_t convex_hull_point_set_count_geometry(const struct spatial_distance_geometry *geometry
) {
    size_t point_count = 0U;

    if (geometry == NULL) {
        return 0U;
    }
    switch (geometry->type) {
    case MYLITE_SPATIAL_GEOMETRY_POINT:
        return 1U;
    case MYLITE_SPATIAL_GEOMETRY_LINESTRING:
        return geometry->point_count;
    case MYLITE_SPATIAL_GEOMETRY_POLYGON:
        for (uint32_t index = 0U; index < geometry->ring_count; ++index) {
            if ((size_t)geometry->rings[index].point_count > SIZE_MAX - point_count) {
                return SIZE_MAX;
            }
            point_count += geometry->rings[index].point_count;
        }
        return point_count;
    case MYLITE_SPATIAL_GEOMETRY_MULTIPOINT:
    case MYLITE_SPATIAL_GEOMETRY_MULTILINESTRING:
    case MYLITE_SPATIAL_GEOMETRY_MULTIPOLYGON:
    case MYLITE_SPATIAL_GEOMETRY_GEOMETRYCOLLECTION:
        for (uint32_t index = 0U; index < geometry->child_count; ++index) {
            size_t child_count = convex_hull_point_set_count_geometry(&geometry->children[index]);

            if (child_count > SIZE_MAX - point_count) {
                return SIZE_MAX;
            }
            point_count += child_count;
        }
        return point_count;
    default:
        break;
    }
    return 0U;
}

static int convex_hull_point_set_append_geometry(
    const struct spatial_distance_geometry *geometry,
    struct spatial_discrete_point_set *set,
    uint32_t *io_offset,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    if (geometry == NULL || set == NULL || io_offset == NULL) {
        return set_invalid_gis_data_error(error, function_name);
    }
    switch (geometry->type) {
    case MYLITE_SPATIAL_GEOMETRY_POINT:
        if (*io_offset >= set->point_count) {
            return set_invalid_gis_data_error(error, function_name);
        }
        set->points[*io_offset] = geometry->point;
        ++(*io_offset);
        return 0;
    case MYLITE_SPATIAL_GEOMETRY_LINESTRING:
        if (geometry->point_count > set->point_count - *io_offset) {
            return set_invalid_gis_data_error(error, function_name);
        }
        memcpy(
            set->points + *io_offset,
            geometry->points,
            (size_t)geometry->point_count * sizeof(*set->points)
        );
        *io_offset += geometry->point_count;
        return 0;
    case MYLITE_SPATIAL_GEOMETRY_POLYGON:
        for (uint32_t index = 0U; index < geometry->ring_count; ++index) {
            const struct spatial_distance_ring *ring = &geometry->rings[index];

            if (ring->point_count > set->point_count - *io_offset) {
                return set_invalid_gis_data_error(error, function_name);
            }
            memcpy(
                set->points + *io_offset,
                ring->points,
                (size_t)ring->point_count * sizeof(*set->points)
            );
            *io_offset += ring->point_count;
        }
        return 0;
    case MYLITE_SPATIAL_GEOMETRY_MULTIPOINT:
    case MYLITE_SPATIAL_GEOMETRY_MULTILINESTRING:
    case MYLITE_SPATIAL_GEOMETRY_MULTIPOLYGON:
    case MYLITE_SPATIAL_GEOMETRY_GEOMETRYCOLLECTION:
        for (uint32_t index = 0U; index < geometry->child_count; ++index) {
            int rc = convex_hull_point_set_append_geometry(
                &geometry->children[index],
                set,
                io_offset,
                error,
                function_name
            );

            if (rc != 0) {
                return rc;
            }
        }
        return 0;
    default:
        break;
    }
    return set_invalid_gis_data_error(error, function_name);
}

static int convex_hull_validate_geometry_rings(
    const struct spatial_distance_geometry *geometry,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    if (geometry == NULL) {
        return set_invalid_gis_data_error(error, function_name);
    }
    if (geometry->type == MYLITE_SPATIAL_GEOMETRY_POLYGON) {
        if (geometry->ring_count == 0U) {
            return set_invalid_gis_data_error(error, function_name);
        }
        for (uint32_t index = 0U; index < geometry->ring_count; ++index) {
            if (!spatial_ring_has_noncollinear_points(&geometry->rings[index])) {
                return set_invalid_gis_data_error(error, function_name);
            }
        }
        return 0;
    }
    if (distance_geometry_is_collection(geometry)) {
        for (uint32_t index = 0U; index < geometry->child_count; ++index) {
            int rc = convex_hull_validate_geometry_rings(
                &geometry->children[index],
                error,
                function_name
            );

            if (rc != 0) {
                return rc;
            }
        }
    }
    return 0;
}

static bool spatial_ring_has_noncollinear_points(const struct spatial_distance_ring *ring) {
    if (ring == NULL || ring->point_count < 4U) {
        return false;
    }
    for (uint32_t origin_index = 0U; origin_index < ring->point_count; ++origin_index) {
        for (uint32_t left_index = origin_index + 1U; left_index < ring->point_count;
             ++left_index) {
            if (spatial_points_are_equal(&ring->points[origin_index], &ring->points[left_index])) {
                continue;
            }
            for (uint32_t right_index = left_index + 1U; right_index < ring->point_count;
                 ++right_index) {
                if (!double_near_zero(point_cross_product(
                        &ring->points[origin_index],
                        &ring->points[left_index],
                        &ring->points[right_index]
                    ))) {
                    return true;
                }
            }
        }
    }
    return false;
}

static int convex_hull_build(
    struct spatial_discrete_point_set *points,
    struct spatial_discrete_point_set *out_hull,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    size_t hull_capacity = 0U;
    uint32_t unique_count = 0U;
    uint32_t hull_count = 0U;
    uint32_t upper_start = 0U;
    struct spatial_point *hull_points = NULL;

    if (points == NULL || out_hull == NULL) {
        return set_invalid_gis_data_error(error, function_name);
    }
    *out_hull = (struct spatial_discrete_point_set){0};
    for (uint32_t index = 0U; index < points->point_count; ++index) {
        if (!isfinite(points->points[index].coordinate_x) ||
            !isfinite(points->points[index].coordinate_y)) {
            return set_invalid_gis_data_error(error, function_name);
        }
    }
    qsort(
        points->points,
        points->point_count,
        sizeof(*points->points),
        spatial_point_compare_for_hull
    );
    unique_count = convex_hull_unique_points(points->points, points->point_count);
    if (unique_count <= 2U) {
        out_hull->points =
            calloc(unique_count == 0U ? 1U : unique_count, sizeof(*out_hull->points));
        if (out_hull->points == NULL) {
            return set_nomem_error(error);
        }
        memcpy(out_hull->points, points->points, (size_t)unique_count * sizeof(*out_hull->points));
        out_hull->point_count = unique_count;
        return 0;
    }
    hull_capacity = (size_t)unique_count * 2U;
    if (hull_capacity > SIZE_MAX / sizeof(*hull_points)) {
        return set_nomem_error(error);
    }
    hull_points = calloc(hull_capacity, sizeof(*hull_points));
    if (hull_points == NULL) {
        return set_nomem_error(error);
    }
    for (uint32_t index = 0U; index < unique_count; ++index) {
        while (hull_count >= 2U && convex_hull_turn_is_clockwise_or_collinear(
                                       &hull_points[hull_count - 2U],
                                       &hull_points[hull_count - 1U],
                                       &points->points[index]
                                   )) {
            --hull_count;
        }
        hull_points[hull_count] = points->points[index];
        ++hull_count;
    }
    upper_start = hull_count + 1U;
    for (uint32_t index = unique_count - 2U;; --index) {
        while (hull_count >= upper_start && convex_hull_turn_is_clockwise_or_collinear(
                                                &hull_points[hull_count - 2U],
                                                &hull_points[hull_count - 1U],
                                                &points->points[index]
                                            )) {
            --hull_count;
        }
        hull_points[hull_count] = points->points[index];
        ++hull_count;
        if (index == 0U) {
            break;
        }
    }
    if (hull_count > 1U) {
        --hull_count;
    }
    out_hull->points = hull_points;
    out_hull->point_count = hull_count;
    return 0;
}

static int spatial_point_compare_for_hull(
    const void *left, // NOLINT(bugprone-easily-swappable-parameters): qsort comparator ABI.
    const void *right
) {
    const struct spatial_point *left_point = left;
    const struct spatial_point *right_point = right;

    if (left_point->coordinate_x < right_point->coordinate_x) {
        return -1;
    }
    if (left_point->coordinate_x > right_point->coordinate_x) {
        return 1;
    }
    if (left_point->coordinate_y < right_point->coordinate_y) {
        return -1;
    }
    if (left_point->coordinate_y > right_point->coordinate_y) {
        return 1;
    }
    return 0;
}

static uint32_t convex_hull_unique_points(struct spatial_point *points, uint32_t point_count) {
    uint32_t unique_count = 0U;

    if (points == NULL || point_count == 0U) {
        return 0U;
    }
    for (uint32_t index = 0U; index < point_count; ++index) {
        if (unique_count == 0U ||
            !spatial_points_are_equal(&points[index], &points[unique_count - 1U])) {
            points[unique_count] = points[index];
            ++unique_count;
        }
    }
    return unique_count;
}

static bool spatial_points_are_equal(
    const struct spatial_point *left,
    const struct spatial_point *right
) {
    return left != NULL && right != NULL && left->coordinate_x == right->coordinate_x &&
           left->coordinate_y == right->coordinate_y;
}

static bool convex_hull_turn_is_clockwise_or_collinear(
    const struct spatial_point *origin,
    const struct spatial_point *middle,
    const struct spatial_point *candidate
) {
    return point_cross_product(origin, middle, candidate) <= spatial_distance_epsilon;
}

static int simplify_geometry(
    const struct spatial_distance_geometry *source,
    double max_distance,
    struct spatial_distance_geometry *out_geometry,
    bool *out_has_geometry,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    if (source == NULL || out_geometry == NULL || out_has_geometry == NULL) {
        return set_invalid_gis_data_error(error, function_name);
    }
    *out_geometry = (struct spatial_distance_geometry){0};
    *out_has_geometry = false;
    switch (source->type) {
    case MYLITE_SPATIAL_GEOMETRY_POINT:
        return simplify_point_geometry(
            source,
            out_geometry,
            out_has_geometry,
            error,
            function_name
        );
    case MYLITE_SPATIAL_GEOMETRY_LINESTRING:
        return simplify_line_geometry(
            source,
            max_distance,
            out_geometry,
            out_has_geometry,
            error,
            function_name
        );
    case MYLITE_SPATIAL_GEOMETRY_POLYGON:
        return simplify_polygon_geometry(
            source,
            max_distance,
            out_geometry,
            out_has_geometry,
            error,
            function_name
        );
    case MYLITE_SPATIAL_GEOMETRY_MULTIPOINT:
    case MYLITE_SPATIAL_GEOMETRY_MULTILINESTRING:
    case MYLITE_SPATIAL_GEOMETRY_MULTIPOLYGON:
    case MYLITE_SPATIAL_GEOMETRY_GEOMETRYCOLLECTION:
        return simplify_collection_geometry(
            source,
            max_distance,
            out_geometry,
            out_has_geometry,
            error,
            function_name
        );
    default:
        break;
    }
    return set_invalid_gis_data_error(error, function_name);
}

static int simplify_point_geometry(
    const struct spatial_distance_geometry *source,
    struct spatial_distance_geometry *out_geometry,
    bool *out_has_geometry,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    if (source == NULL || out_geometry == NULL || out_has_geometry == NULL) {
        return set_invalid_gis_data_error(error, function_name);
    }
    *out_geometry = (struct spatial_distance_geometry){
        .type = MYLITE_SPATIAL_GEOMETRY_POINT,
        .point = source->point,
    };
    *out_has_geometry = true;
    return 0;
}

static int simplify_line_geometry(
    const struct spatial_distance_geometry *source,
    double max_distance,
    struct spatial_distance_geometry *out_geometry,
    bool *out_has_geometry,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    struct spatial_point *points = NULL;
    uint32_t point_count = 0U;
    int rc = 0;

    if (source == NULL || out_geometry == NULL || out_has_geometry == NULL) {
        return set_invalid_gis_data_error(error, function_name);
    }
    rc = simplify_points(
        source->points,
        source->point_count,
        max_distance,
        &points,
        &point_count,
        error
    );
    if (rc != 0) {
        free(points);
        return rc;
    }
    *out_geometry = (struct spatial_distance_geometry){
        .type = MYLITE_SPATIAL_GEOMETRY_LINESTRING,
        .points = points,
        .point_count = point_count,
    };
    *out_has_geometry = true;
    return 0;
}

static int simplify_polygon_geometry(
    const struct spatial_distance_geometry *source,
    double max_distance,
    struct spatial_distance_geometry *out_geometry,
    bool *out_has_geometry,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    struct spatial_distance_ring *rings = NULL;
    uint32_t ring_count = 0U;
    int rc = 0;

    if (source == NULL || out_geometry == NULL || out_has_geometry == NULL ||
        source->ring_count == 0U) {
        return set_invalid_gis_data_error(error, function_name);
    }
    if ((size_t)source->ring_count > SIZE_MAX / sizeof(*rings)) {
        return set_nomem_error(error);
    }
    rings = calloc(source->ring_count, sizeof(*rings));
    if (rings == NULL) {
        return set_nomem_error(error);
    }
    for (uint32_t index = 0U; rc == 0 && index < source->ring_count; ++index) {
        struct spatial_point *points = NULL;
        uint32_t point_count = 0U;
        bool has_ring = false;

        rc = simplify_ring_points(
            source->rings[index].points,
            source->rings[index].point_count,
            max_distance,
            &points,
            &point_count,
            &has_ring,
            error
        );
        if (rc != 0) {
            free(points);
            break;
        }
        if (!has_ring) {
            free(points);
            if (index == 0U) {
                break;
            }
            continue;
        }
        rings[ring_count] = (struct spatial_distance_ring){
            .points = points,
            .point_count = point_count,
        };
        ++ring_count;
    }
    if (rc != 0) {
        for (uint32_t index = 0U; index < ring_count; ++index) {
            free(rings[index].points);
        }
        free(rings);
        return rc;
    }
    if (ring_count == 0U) {
        free(rings);
        *out_has_geometry = false;
        return 0;
    }
    *out_geometry = (struct spatial_distance_geometry){
        .type = MYLITE_SPATIAL_GEOMETRY_POLYGON,
        .rings = rings,
        .ring_count = ring_count,
    };
    *out_has_geometry = true;
    return 0;
}

static int simplify_collection_geometry(
    const struct spatial_distance_geometry *source,
    double max_distance,
    struct spatial_distance_geometry *out_geometry,
    bool *out_has_geometry,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    struct spatial_distance_geometry *children = NULL;
    uint32_t child_count = 0U;
    int rc = 0;

    if (source == NULL || out_geometry == NULL || out_has_geometry == NULL) {
        return set_invalid_gis_data_error(error, function_name);
    }
    if (source->child_count == 0U) {
        *out_has_geometry = false;
        return 0;
    }
    if ((size_t)source->child_count > SIZE_MAX / sizeof(*children)) {
        return set_nomem_error(error);
    }
    children = calloc(source->child_count, sizeof(*children));
    if (children == NULL) {
        return set_nomem_error(error);
    }
    for (uint32_t index = 0U; rc == 0 && index < source->child_count; ++index) {
        bool has_child = false;

        rc = simplify_geometry(
            &source->children[index],
            max_distance,
            &children[child_count],
            &has_child,
            error,
            function_name
        );
        if (rc == 0 && has_child) {
            ++child_count;
        } else if (rc == 0) {
            distance_geometry_deinit(&children[child_count]);
        }
    }
    if (rc != 0) {
        for (uint32_t index = 0U; index < source->child_count; ++index) {
            distance_geometry_deinit(&children[index]);
        }
        free(children);
        return rc;
    }
    if (child_count == 0U) {
        free(children);
        *out_has_geometry = false;
        return 0;
    }
    *out_geometry = (struct spatial_distance_geometry){
        .type = source->type,
        .children = children,
        .child_count = child_count,
    };
    *out_has_geometry = true;
    return 0;
}

static int simplify_points(
    const struct spatial_point *points,
    uint32_t point_count,
    double max_distance,
    struct spatial_point **out_points,
    uint32_t *out_point_count,
    struct mylite_spatial_error *error
) {
    bool *keep = NULL;
    struct spatial_point *simplified = NULL;
    uint32_t output_count = 0U;
    uint32_t offset = 0U;
    int rc = 0;

    if (out_points == NULL || out_point_count == NULL) {
        return set_invalid_gis_data_error(error, "st_simplify");
    }
    *out_points = NULL;
    *out_point_count = 0U;
    if (point_count <= 2U) {
        return simplify_point_copy(points, point_count, out_points, out_point_count, error);
    }
    if (points == NULL || (size_t)point_count > SIZE_MAX / sizeof(*keep)) {
        return set_nomem_error(error);
    }
    keep = calloc(point_count, sizeof(*keep));
    if (keep == NULL) {
        return set_nomem_error(error);
    }
    rc = simplify_keep_mask(points, point_count, max_distance, keep, error);
    if (rc == 0) {
        output_count = simplify_kept_point_count(keep, point_count);
        if (output_count < 2U) {
            output_count = 2U;
            keep[0] = true;
            keep[point_count - 1U] = true;
        }
        if ((size_t)output_count > SIZE_MAX / sizeof(*simplified)) {
            rc = set_nomem_error(error);
        }
    }
    if (rc == 0) {
        simplified = calloc(output_count, sizeof(*simplified));
        if (simplified == NULL) {
            free(keep);
            return set_nomem_error(error);
        }
    }
    for (uint32_t index = 0U; rc == 0 && index < point_count; ++index) {
        if (keep[index]) {
            simplified[offset] = points[index];
            ++offset;
        }
    }
    free(keep);
    if (rc != 0) {
        free(simplified);
        return rc;
    }
    *out_points = simplified;
    *out_point_count = offset;
    return 0;
}

static int simplify_ring_points(
    const struct spatial_point *points,
    uint32_t point_count,
    double max_distance,
    struct spatial_point **out_points,
    uint32_t *out_point_count,
    bool *out_has_ring,
    struct mylite_spatial_error *error
) {
    struct spatial_point *simplified = NULL;
    uint32_t simplified_count = 0U;
    int rc = 0;

    if (out_points == NULL || out_point_count == NULL || out_has_ring == NULL) {
        return set_invalid_gis_data_error(error, "st_simplify");
    }
    *out_points = NULL;
    *out_point_count = 0U;
    *out_has_ring = false;
    if (points == NULL || point_count < 4U) {
        return 0;
    }
    rc = simplify_points(points, point_count, max_distance, &simplified, &simplified_count, error);
    if (rc != 0) {
        free(simplified);
        return rc;
    }
    if (simplified_count < 4U ||
        !spatial_points_are_equal(&simplified[0], &simplified[simplified_count - 1U]) ||
        !simplify_ring_is_valid(simplified, simplified_count)) {
        free(simplified);
        return 0;
    }
    simplify_ring_canonicalize(simplified, simplified_count);
    *out_points = simplified;
    *out_point_count = simplified_count;
    *out_has_ring = true;
    return 0;
}

static int simplify_point_copy(
    const struct spatial_point *points,
    uint32_t point_count,
    struct spatial_point **out_points,
    uint32_t *out_point_count,
    struct mylite_spatial_error *error
) {
    struct spatial_point *copy = NULL;

    if (point_count != 0U && points == NULL) {
        return set_invalid_gis_data_error(error, "st_simplify");
    }
    if ((size_t)point_count > SIZE_MAX / sizeof(*copy)) {
        return set_nomem_error(error);
    }
    copy = calloc(point_count == 0U ? 1U : point_count, sizeof(*copy));
    if (copy == NULL) {
        return set_nomem_error(error);
    }
    if (point_count != 0U) {
        memcpy(copy, points, (size_t)point_count * sizeof(*copy));
    }
    *out_points = copy;
    *out_point_count = point_count;
    return 0;
}

static int simplify_keep_mask(
    const struct spatial_point *points,
    uint32_t point_count, // NOLINT(bugprone-easily-swappable-parameters)
    double max_distance,
    bool *keep,
    struct mylite_spatial_error *error
) {
    struct spatial_simplify_range *ranges = NULL;
    uint32_t range_count = 0U;
    int rc = 0;

    if (points == NULL || keep == NULL || point_count < 2U) {
        return set_invalid_gis_data_error(error, "st_simplify");
    }
    if ((size_t)point_count > SIZE_MAX / sizeof(*ranges)) {
        return set_nomem_error(error);
    }
    ranges = calloc(point_count, sizeof(*ranges));
    if (ranges == NULL) {
        return set_nomem_error(error);
    }
    keep[0] = true;
    keep[point_count - 1U] = true;
    if (!simplify_range_push(
            ranges,
            point_count,
            &range_count,
            (struct spatial_simplify_range){.first = 0U, .last = point_count - 1U}
        )) {
        rc = set_nomem_error(error);
    }
    while (rc == 0 && range_count != 0U) {
        struct spatial_simplify_range range = ranges[range_count - 1U];
        struct spatial_segment segment = {
            .start = points[range.first],
            .end = points[range.last],
        };
        double farthest_distance = -1.0;
        uint32_t farthest_index = range.first;

        --range_count;
        if (range.last <= range.first + 1U) {
            continue;
        }
        for (uint32_t index = range.first + 1U; index < range.last; ++index) {
            double distance = distance_point_to_segment(&points[index], &segment);

            if (distance > farthest_distance) {
                farthest_distance = distance;
                farthest_index = index;
            }
        }
        if (farthest_distance > max_distance) {
            keep[farthest_index] = true;
            if (!simplify_range_push(
                    ranges,
                    point_count,
                    &range_count,
                    (struct spatial_simplify_range){
                        .first = range.first,
                        .last = farthest_index,
                    }
                ) ||
                !simplify_range_push(
                    ranges,
                    point_count,
                    &range_count,
                    (struct spatial_simplify_range){
                        .first = farthest_index,
                        .last = range.last,
                    }
                )) {
                rc = set_nomem_error(error);
            }
        }
    }
    free(ranges);
    return rc;
}

static bool simplify_range_push(
    struct spatial_simplify_range *ranges,
    uint32_t capacity,
    uint32_t *io_count,
    struct spatial_simplify_range range
) {
    if (ranges == NULL || io_count == NULL || *io_count >= capacity) {
        return false;
    }
    ranges[*io_count] = range;
    ++(*io_count);
    return true;
}

static uint32_t simplify_kept_point_count(const bool *keep, uint32_t point_count) {
    uint32_t kept_count = 0U;

    if (keep == NULL) {
        return 0U;
    }
    for (uint32_t index = 0U; index < point_count; ++index) {
        if (keep[index]) {
            ++kept_count;
        }
    }
    return kept_count;
}

static void simplify_ring_canonicalize(struct spatial_point *points, uint32_t point_count) {
    struct spatial_point *rotated = NULL;
    uint32_t unique_count = 0U;
    uint32_t start = 0U;

    if (points == NULL || point_count < 2U) {
        return;
    }
    unique_count = point_count - 1U;
    for (uint32_t index = 1U; index < unique_count; ++index) {
        if (points[index].coordinate_x > points[start].coordinate_x ||
            (points[index].coordinate_x == points[start].coordinate_x &&
             points[index].coordinate_y > points[start].coordinate_y)) {
            start = index;
        }
    }
    if (start == 0U) {
        return;
    }
    rotated = calloc(point_count, sizeof(*rotated));
    if (rotated == NULL) {
        return;
    }
    for (uint32_t index = 0U; index < unique_count; ++index) {
        rotated[index] = points[(start + index) % unique_count];
    }
    rotated[unique_count] = rotated[0];
    memcpy(points, rotated, (size_t)point_count * sizeof(*points));
    free(rotated);
}

static bool simplify_ring_is_valid(const struct spatial_point *points, uint32_t point_count) {
    struct spatial_distance_ring ring = {
        .points = (struct spatial_point *)points,
        .point_count = point_count,
    };

    return points != NULL && spatial_ring_has_noncollinear_points(&ring);
}

static int make_internal_geometry_from_distance_geometry(
    uint32_t srid,
    const struct spatial_distance_geometry *geometry,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    struct mylite_spatial_error *error
) {
    struct spatial_buffer buffer = {0};
    int rc = append_internal_prefix(&buffer, srid);

    if (rc == 0) {
        rc = append_distance_geometry_wkb(&buffer, geometry);
    }
    if (rc != 0) {
        spatial_buffer_deinit(&buffer);
        return set_nomem_error(error);
    }
    *out_bytes = buffer.bytes;
    *out_byte_count = buffer.size;
    return 0;
}

static int append_distance_geometry_wkb(
    struct spatial_buffer *buffer,
    const struct spatial_distance_geometry *geometry
) {
    int rc = 0;

    if (buffer == NULL || geometry == NULL) {
        return -1;
    }
    rc = spatial_buffer_append_byte(buffer, 1U);
    if (rc == 0) {
        rc = spatial_buffer_append_u32_le(buffer, (uint32_t)geometry->type);
    }
    switch (geometry->type) {
    case MYLITE_SPATIAL_GEOMETRY_POINT:
        if (rc == 0) {
            rc = spatial_buffer_append_double_le(buffer, geometry->point.coordinate_x);
        }
        if (rc == 0) {
            rc = spatial_buffer_append_double_le(buffer, geometry->point.coordinate_y);
        }
        return rc;
    case MYLITE_SPATIAL_GEOMETRY_LINESTRING:
        if (rc == 0) {
            rc = spatial_buffer_append_u32_le(buffer, geometry->point_count);
        }
        if (rc == 0) {
            rc = append_distance_geometry_points(buffer, geometry->points, geometry->point_count);
        }
        return rc;
    case MYLITE_SPATIAL_GEOMETRY_POLYGON:
        if (rc == 0) {
            rc = spatial_buffer_append_u32_le(buffer, geometry->ring_count);
        }
        for (uint32_t index = 0U; rc == 0 && index < geometry->ring_count; ++index) {
            rc = spatial_buffer_append_u32_le(buffer, geometry->rings[index].point_count);
            if (rc == 0) {
                rc = append_distance_geometry_points(
                    buffer,
                    geometry->rings[index].points,
                    geometry->rings[index].point_count
                );
            }
        }
        return rc;
    case MYLITE_SPATIAL_GEOMETRY_MULTIPOINT:
    case MYLITE_SPATIAL_GEOMETRY_MULTILINESTRING:
    case MYLITE_SPATIAL_GEOMETRY_MULTIPOLYGON:
    case MYLITE_SPATIAL_GEOMETRY_GEOMETRYCOLLECTION:
        if (rc == 0) {
            rc = spatial_buffer_append_u32_le(buffer, geometry->child_count);
        }
        for (uint32_t index = 0U; rc == 0 && index < geometry->child_count; ++index) {
            rc = append_distance_geometry_wkb(buffer, &geometry->children[index]);
        }
        return rc;
    default:
        break;
    }
    return -1;
}

static int append_distance_geometry_points(
    struct spatial_buffer *buffer,
    const struct spatial_point *points,
    uint32_t point_count
) {
    int rc = 0;

    if (point_count != 0U && points == NULL) {
        return -1;
    }
    for (uint32_t index = 0U; rc == 0 && index < point_count; ++index) {
        rc = spatial_buffer_append_double_le(buffer, points[index].coordinate_x);
        if (rc == 0) {
            rc = spatial_buffer_append_double_le(buffer, points[index].coordinate_y);
        }
    }
    return rc;
}

static int argument_simplify_distance(
    const struct mylite_spatial_argument *argument,
    double *out_value,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    char *text = NULL;
    char *end = NULL;
    double value = 0.0;

    if (argument == NULL || out_value == NULL) {
        return set_invalid_gis_data_error(error, function_name);
    }
    if (argument->has_numeric) {
        if (!isfinite(argument->numeric)) {
            return set_wrong_arguments_error(error, function_name);
        }
        *out_value = argument->numeric;
        return 0;
    }
    if (argument->byte_count == SIZE_MAX) {
        return set_nomem_error(error);
    }
    text = (char *)malloc(argument->byte_count + 1U);
    if (text == NULL) {
        return set_nomem_error(error);
    }
    if (argument->byte_count != 0U) {
        memcpy(text, argument->bytes, argument->byte_count);
    }
    text[argument->byte_count] = '\0';
    errno = 0;
    value = strtod(text, &end);
    if (end == text) {
        value = 0.0;
    } else if (errno == ERANGE || !isfinite(value)) {
        free(text);
        return set_wrong_arguments_error(error, function_name);
    }
    free(text);
    *out_value = value;
    return 0;
}

static int distance_geometry_read(
    struct spatial_wkb_cursor *cursor,
    struct spatial_distance_geometry *out_geometry,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    bool little_endian = false;
    enum mylite_spatial_geometry_type type = MYLITE_SPATIAL_GEOMETRY_NONE;
    uint32_t count = 0U;
    int rc = 0;

    if (out_geometry == NULL) {
        return set_invalid_gis_data_error(error, function_name);
    }
    *out_geometry = (struct spatial_distance_geometry){0};
    rc = cursor_read_header(cursor, &little_endian, &type, error, function_name);
    if (rc != 0) {
        return rc;
    }
    out_geometry->type = type;
    switch (type) {
    case MYLITE_SPATIAL_GEOMETRY_POINT:
        rc = cursor_read_double(
            cursor,
            little_endian,
            &out_geometry->point.coordinate_x,
            error,
            function_name
        );
        if (rc == 0) {
            rc = cursor_read_double(
                cursor,
                little_endian,
                &out_geometry->point.coordinate_y,
                error,
                function_name
            );
        }
        break;
    case MYLITE_SPATIAL_GEOMETRY_LINESTRING:
        rc = cursor_read_u32(cursor, little_endian, &count, error, function_name);
        if (rc == 0) {
            out_geometry->point_count = count;
            rc = distance_geometry_read_points(
                cursor,
                little_endian,
                count,
                &out_geometry->points,
                error,
                function_name
            );
        }
        break;
    case MYLITE_SPATIAL_GEOMETRY_POLYGON:
        rc = cursor_read_u32(cursor, little_endian, &count, error, function_name);
        if (rc == 0) {
            out_geometry->ring_count = count;
            if (count > 0U) {
                if ((size_t)count > SIZE_MAX / sizeof(*out_geometry->rings)) {
                    return set_nomem_error(error);
                }
                out_geometry->rings = calloc((size_t)count, sizeof(*out_geometry->rings));
                if (out_geometry->rings == NULL) {
                    return set_nomem_error(error);
                }
            }
        }
        for (uint32_t index = 0U; rc == 0 && index < count; ++index) {
            uint32_t point_count = 0U;

            rc = cursor_read_u32(cursor, little_endian, &point_count, error, function_name);
            if (rc == 0) {
                out_geometry->rings[index].point_count = point_count;
                rc = distance_geometry_read_points(
                    cursor,
                    little_endian,
                    point_count,
                    &out_geometry->rings[index].points,
                    error,
                    function_name
                );
            }
        }
        break;
    case MYLITE_SPATIAL_GEOMETRY_MULTIPOINT:
    case MYLITE_SPATIAL_GEOMETRY_MULTILINESTRING:
    case MYLITE_SPATIAL_GEOMETRY_MULTIPOLYGON:
    case MYLITE_SPATIAL_GEOMETRY_GEOMETRYCOLLECTION:
        rc = distance_geometry_read_children(
            cursor,
            little_endian,
            type,
            out_geometry,
            error,
            function_name
        );
        break;
    default:
        rc = set_invalid_gis_data_error(error, function_name);
        break;
    }
    if (rc != 0) {
        distance_geometry_deinit(out_geometry);
    }
    return rc;
}

static int distance_geometry_read_points(
    struct spatial_wkb_cursor *cursor,
    bool little_endian,
    uint32_t point_count,
    struct spatial_point **out_points,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    struct spatial_point *points = NULL;
    int rc = 0;

    if (out_points == NULL) {
        return set_invalid_gis_data_error(error, function_name);
    }
    *out_points = NULL;
    if (point_count == 0U) {
        return 0;
    }
    if ((size_t)point_count > SIZE_MAX / sizeof(*points)) {
        return set_nomem_error(error);
    }
    points = calloc((size_t)point_count, sizeof(*points));
    if (points == NULL) {
        return set_nomem_error(error);
    }
    for (uint32_t index = 0U; rc == 0 && index < point_count; ++index) {
        rc = cursor_read_double(
            cursor,
            little_endian,
            &points[index].coordinate_x,
            error,
            function_name
        );
        if (rc == 0) {
            rc = cursor_read_double(
                cursor,
                little_endian,
                &points[index].coordinate_y,
                error,
                function_name
            );
        }
    }
    if (rc != 0) {
        free(points);
        return rc;
    }
    *out_points = points;
    return 0;
}

static int distance_geometry_read_children(
    struct spatial_wkb_cursor *cursor,
    bool little_endian,
    enum mylite_spatial_geometry_type type,
    struct spatial_distance_geometry *out_geometry,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    enum mylite_spatial_geometry_type expected_type = collection_expected_nested_type(type);
    uint32_t count = 0U;
    int rc = cursor_read_u32(cursor, little_endian, &count, error, function_name);

    if (rc != 0) {
        return rc;
    }
    out_geometry->child_count = count;
    if (count == 0U) {
        return 0;
    }
    if ((size_t)count > SIZE_MAX / sizeof(*out_geometry->children)) {
        return set_nomem_error(error);
    }
    out_geometry->children = calloc((size_t)count, sizeof(*out_geometry->children));
    if (out_geometry->children == NULL) {
        return set_nomem_error(error);
    }
    for (uint32_t index = 0U; rc == 0 && index < count; ++index) {
        rc = distance_geometry_read(cursor, &out_geometry->children[index], error, function_name);
        if (rc == 0 && expected_type != MYLITE_SPATIAL_GEOMETRY_NONE &&
            out_geometry->children[index].type != expected_type) {
            rc = set_invalid_gis_data_error(error, function_name);
        }
    }
    return rc;
}

static void distance_geometry_deinit(struct spatial_distance_geometry *geometry) {
    if (geometry == NULL) {
        return;
    }
    free(geometry->points);
    for (uint32_t index = 0U; index < geometry->ring_count; ++index) {
        free(geometry->rings[index].points);
    }
    free(geometry->rings);
    for (uint32_t index = 0U; index < geometry->child_count; ++index) {
        distance_geometry_deinit(&geometry->children[index]);
    }
    free(geometry->children);
    *geometry = (struct spatial_distance_geometry){0};
}

static bool simplicity_geometry_is_simple(const struct spatial_distance_geometry *geometry) {
    if (geometry == NULL) {
        return false;
    }
    switch (geometry->type) {
    case MYLITE_SPATIAL_GEOMETRY_POINT:
        return validity_point_is_valid(&geometry->point);
    case MYLITE_SPATIAL_GEOMETRY_LINESTRING:
        return simplicity_line_is_simple(geometry->points, geometry->point_count);
    case MYLITE_SPATIAL_GEOMETRY_POLYGON:
    case MYLITE_SPATIAL_GEOMETRY_MULTIPOLYGON:
        return true;
    case MYLITE_SPATIAL_GEOMETRY_MULTIPOINT:
        return simplicity_multipoint_is_simple(geometry);
    case MYLITE_SPATIAL_GEOMETRY_MULTILINESTRING:
        return simplicity_multiline_is_simple(geometry);
    case MYLITE_SPATIAL_GEOMETRY_GEOMETRYCOLLECTION:
        return simplicity_collection_is_simple(geometry);
    default:
        break;
    }
    return false;
}

static bool simplicity_line_is_simple(const struct spatial_point *points, uint32_t point_count) {
    bool closed = false;
    uint32_t segment_count = 0U;

    if (!validity_line_is_valid(points, point_count)) {
        return false;
    }
    for (uint32_t index = 1U; index < point_count; ++index) {
        if (spatial_points_are_equal(&points[index - 1U], &points[index])) {
            return false;
        }
    }
    closed = spatial_points_are_equal(&points[0], &points[point_count - 1U]);
    segment_count = point_count - 1U;
    for (uint32_t left_index = 0U; left_index < segment_count; ++left_index) {
        struct spatial_segment left = {.start = points[left_index], .end = points[left_index + 1U]};

        for (uint32_t right_index = left_index + 1U; right_index < segment_count; ++right_index) {
            bool adjacent = right_index == left_index + 1U ||
                            (closed && left_index == 0U && right_index == segment_count - 1U);
            struct spatial_segment right = {
                .start = points[right_index],
                .end = points[right_index + 1U]
            };

            if (validity_segment_intersection_is_invalid(&left, &right, adjacent)) {
                return false;
            }
        }
    }
    return true;
}

static bool simplicity_multipoint_is_simple(const struct spatial_distance_geometry *geometry) {
    for (uint32_t index = 0U; index < geometry->child_count; ++index) {
        const struct spatial_distance_geometry *child = &geometry->children[index];

        if (child->type != MYLITE_SPATIAL_GEOMETRY_POINT ||
            !validity_point_is_valid(&child->point)) {
            return false;
        }
        for (uint32_t previous = 0U; previous < index; ++previous) {
            if (spatial_points_are_equal(&geometry->children[previous].point, &child->point)) {
                return false;
            }
        }
    }
    return true;
}

static bool simplicity_multiline_is_simple(const struct spatial_distance_geometry *geometry) {
    for (uint32_t index = 0U; index < geometry->child_count; ++index) {
        const struct spatial_distance_geometry *child = &geometry->children[index];

        if (child->type != MYLITE_SPATIAL_GEOMETRY_LINESTRING ||
            !simplicity_line_is_simple(child->points, child->point_count)) {
            return false;
        }
        for (uint32_t previous = 0U; previous < index; ++previous) {
            if (simplicity_line_pair_intersects_invalidly(&geometry->children[previous], child)) {
                return false;
            }
        }
    }
    return true;
}

static bool simplicity_collection_is_simple(const struct spatial_distance_geometry *geometry) {
    for (uint32_t index = 0U; index < geometry->child_count; ++index) {
        if (!simplicity_geometry_is_simple(&geometry->children[index])) {
            return false;
        }
        for (uint32_t previous = 0U; previous < index; ++previous) {
            if (simplicity_child_pair_intersects_invalidly(
                    &geometry->children[previous],
                    &geometry->children[index]
                )) {
                return false;
            }
        }
    }
    return true;
}

static bool simplicity_child_pair_intersects_invalidly(
    const struct spatial_distance_geometry *left,
    const struct spatial_distance_geometry *right
) {
    if (distance_geometry_is_collection(left)) {
        for (uint32_t index = 0U; index < left->child_count; ++index) {
            if (simplicity_child_pair_intersects_invalidly(&left->children[index], right)) {
                return true;
            }
        }
        return false;
    }
    if (distance_geometry_is_collection(right)) {
        for (uint32_t index = 0U; index < right->child_count; ++index) {
            if (simplicity_child_pair_intersects_invalidly(left, &right->children[index])) {
                return true;
            }
        }
        return false;
    }
    return simplicity_geometries_intersect_invalidly(left, right);
}

static bool simplicity_geometries_intersect_invalidly(
    const struct spatial_distance_geometry *left,
    const struct spatial_distance_geometry *right
) {
    switch (left->type) {
    case MYLITE_SPATIAL_GEOMETRY_POINT:
        if (right->type == MYLITE_SPATIAL_GEOMETRY_POINT) {
            return spatial_points_are_equal(&left->point, &right->point);
        }
        if (right->type == MYLITE_SPATIAL_GEOMETRY_LINESTRING) {
            return simplicity_point_line_intersection_invalid(&left->point, right);
        }
        if (right->type == MYLITE_SPATIAL_GEOMETRY_POLYGON) {
            return simplicity_point_polygon_intersection_invalid(&left->point, right);
        }
        break;
    case MYLITE_SPATIAL_GEOMETRY_LINESTRING:
        if (right->type == MYLITE_SPATIAL_GEOMETRY_POINT) {
            return simplicity_point_line_intersection_invalid(&right->point, left);
        }
        if (right->type == MYLITE_SPATIAL_GEOMETRY_LINESTRING) {
            return simplicity_line_pair_intersects_invalidly(left, right);
        }
        if (right->type == MYLITE_SPATIAL_GEOMETRY_POLYGON) {
            return simplicity_line_polygon_intersection_invalid(left, right);
        }
        break;
    case MYLITE_SPATIAL_GEOMETRY_POLYGON:
        if (right->type == MYLITE_SPATIAL_GEOMETRY_POINT) {
            return simplicity_point_polygon_intersection_invalid(&right->point, left);
        }
        if (right->type == MYLITE_SPATIAL_GEOMETRY_LINESTRING) {
            return simplicity_line_polygon_intersection_invalid(right, left);
        }
        if (right->type == MYLITE_SPATIAL_GEOMETRY_POLYGON) {
            return simplicity_polygon_polygon_intersection_invalid(left, right);
        }
        break;
    default:
        break;
    }
    return false;
}

static bool simplicity_point_line_intersection_invalid(
    const struct spatial_point *point,
    const struct spatial_distance_geometry *line
) {
    if (!simplicity_point_is_on_line(point, line)) {
        return false;
    }
    return !simplicity_point_is_line_boundary(point, line);
}

static bool simplicity_point_polygon_intersection_invalid(
    const struct spatial_point *point,
    const struct spatial_distance_geometry *polygon
) {
    return validity_polygon_contains_point_interior(polygon, point);
}

static bool simplicity_line_pair_intersects_invalidly(
    const struct spatial_distance_geometry *left,
    const struct spatial_distance_geometry *right
) {
    if (!line_has_segment(left) || !line_has_segment(right)) {
        return false;
    }
    for (uint32_t left_index = 0U; left_index + 1U < left->point_count; ++left_index) {
        struct spatial_segment left_segment = line_segment(left, left_index);

        for (uint32_t right_index = 0U; right_index + 1U < right->point_count; ++right_index) {
            struct spatial_segment right_segment = line_segment(right, right_index);

            if (!segments_intersect(&left_segment, &right_segment)) {
                continue;
            }
            if (validity_segment_intersection_is_invalid(&left_segment, &right_segment, true)) {
                return true;
            }
            if (!simplicity_line_segments_share_boundary_point(
                    left,
                    &left_segment,
                    right,
                    &right_segment
                )) {
                return true;
            }
        }
    }
    return false;
}

static bool simplicity_line_polygon_intersection_invalid(
    const struct spatial_distance_geometry *line,
    const struct spatial_distance_geometry *polygon
) {
    if (!line_has_segment(line)) {
        return false;
    }
    for (uint32_t index = 0U; index < line->point_count; ++index) {
        if (validity_polygon_contains_point_interior(polygon, &line->points[index])) {
            return true;
        }
    }
    for (uint32_t line_index = 0U; line_index + 1U < line->point_count; ++line_index) {
        struct spatial_segment segment = line_segment(line, line_index);

        if (simplicity_segment_midpoint_is_polygon_interior(&segment, polygon)) {
            return true;
        }
    }
    return false;
}

static bool simplicity_polygon_polygon_intersection_invalid(
    const struct spatial_distance_geometry *left,
    const struct spatial_distance_geometry *right
) {
    if (left->ring_count > 0U && left->rings[0].point_count > 0U &&
        validity_polygon_contains_point_interior(right, &left->rings[0].points[0])) {
        return true;
    }
    if (right->ring_count > 0U && right->rings[0].point_count > 0U &&
        validity_polygon_contains_point_interior(left, &right->rings[0].points[0])) {
        return true;
    }
    return false;
}

static bool simplicity_segment_midpoint_is_polygon_interior(
    const struct spatial_segment *segment,
    const struct spatial_distance_geometry *polygon
) {
    struct spatial_point midpoint = {
        .coordinate_x =
            (segment->start.coordinate_x + segment->end.coordinate_x) / spatial_midpoint_divisor,
        .coordinate_y =
            (segment->start.coordinate_y + segment->end.coordinate_y) / spatial_midpoint_divisor,
    };

    return validity_polygon_contains_point_interior(polygon, &midpoint);
}

static bool simplicity_line_segments_share_boundary_point(
    const struct spatial_distance_geometry *left_line,
    const struct spatial_segment *left_segment,
    const struct spatial_distance_geometry *right_line,
    const struct spatial_segment *right_segment
) {
    const struct spatial_point *left_points[spatial_segment_endpoint_count] = {
        &left_segment->start,
        &left_segment->end,
    };
    const struct spatial_point *right_points[spatial_segment_endpoint_count] = {
        &right_segment->start,
        &right_segment->end,
    };

    for (size_t left_index = 0U; left_index < spatial_segment_endpoint_count; ++left_index) {
        for (size_t right_index = 0U; right_index < spatial_segment_endpoint_count; ++right_index) {
            if (spatial_points_are_equal(left_points[left_index], right_points[right_index]) &&
                simplicity_point_is_line_boundary(left_points[left_index], left_line) &&
                simplicity_point_is_line_boundary(right_points[right_index], right_line)) {
                return true;
            }
        }
    }
    return false;
}

static bool simplicity_point_is_line_boundary(
    const struct spatial_point *point,
    const struct spatial_distance_geometry *line
) {
    if (simplicity_line_is_closed(line) || point == NULL || line == NULL ||
        line->point_count < 2U) {
        return false;
    }
    return spatial_points_are_equal(point, &line->points[0]) ||
           spatial_points_are_equal(point, &line->points[line->point_count - 1U]);
}

static bool simplicity_line_is_closed(const struct spatial_distance_geometry *line) {
    return line != NULL && line->point_count > 1U &&
           spatial_points_are_equal(&line->points[0], &line->points[line->point_count - 1U]);
}

static bool simplicity_point_is_on_line(
    const struct spatial_point *point,
    const struct spatial_distance_geometry *line
) {
    if (point == NULL || !line_has_segment(line)) {
        return false;
    }
    for (uint32_t index = 0U; index + 1U < line->point_count; ++index) {
        struct spatial_segment segment = line_segment(line, index);

        if (point_on_segment(point, &segment)) {
            return true;
        }
    }
    return false;
}

static bool validity_geometry_is_valid(const struct spatial_distance_geometry *geometry) {
    if (geometry == NULL) {
        return false;
    }
    switch (geometry->type) {
    case MYLITE_SPATIAL_GEOMETRY_POINT:
        return validity_point_is_valid(&geometry->point);
    case MYLITE_SPATIAL_GEOMETRY_LINESTRING:
        return validity_line_is_valid(geometry->points, geometry->point_count);
    case MYLITE_SPATIAL_GEOMETRY_POLYGON:
        return validity_polygon_is_valid(geometry);
    case MYLITE_SPATIAL_GEOMETRY_MULTIPOINT:
    case MYLITE_SPATIAL_GEOMETRY_MULTILINESTRING:
    case MYLITE_SPATIAL_GEOMETRY_GEOMETRYCOLLECTION:
        for (uint32_t index = 0U; index < geometry->child_count; ++index) {
            if (!validity_geometry_is_valid(&geometry->children[index])) {
                return false;
            }
        }
        return true;
    case MYLITE_SPATIAL_GEOMETRY_MULTIPOLYGON:
        for (uint32_t index = 0U; index < geometry->child_count; ++index) {
            if (!validity_polygon_is_valid(&geometry->children[index])) {
                return false;
            }
            for (uint32_t previous = 0U; previous < index; ++previous) {
                if (validity_polygons_intersect_invalidly(
                        &geometry->children[previous],
                        &geometry->children[index]
                    )) {
                    return false;
                }
            }
        }
        return true;
    default:
        break;
    }
    return false;
}

static bool validity_point_is_valid(const struct spatial_point *point) {
    return point != NULL && isfinite(point->coordinate_x) && isfinite(point->coordinate_y);
}

static bool validity_line_is_valid(const struct spatial_point *points, uint32_t point_count) {
    bool has_distinct_point = false;

    if (points == NULL || point_count < 2U) {
        return false;
    }
    for (uint32_t index = 0U; index < point_count; ++index) {
        if (!validity_point_is_valid(&points[index])) {
            return false;
        }
        if (index > 0U && !spatial_points_are_equal(&points[0], &points[index])) {
            has_distinct_point = true;
        }
    }
    return has_distinct_point;
}

static bool validity_polygon_is_valid(const struct spatial_distance_geometry *polygon) {
    if (polygon == NULL || polygon->type != MYLITE_SPATIAL_GEOMETRY_POLYGON ||
        polygon->ring_count == 0U) {
        return false;
    }
    for (uint32_t index = 0U; index < polygon->ring_count; ++index) {
        if (!validity_ring_is_valid(&polygon->rings[index])) {
            return false;
        }
    }
    return validity_polygon_holes_are_valid(polygon);
}

static bool validity_ring_is_valid(const struct spatial_distance_ring *ring) {
    if (ring == NULL || ring->points == NULL || ring->point_count < 4U ||
        !spatial_points_are_equal(&ring->points[0], &ring->points[ring->point_count - 1U]) ||
        !spatial_ring_has_noncollinear_points(ring)) {
        return false;
    }
    for (uint32_t index = 0U; index < ring->point_count; ++index) {
        if (!validity_point_is_valid(&ring->points[index])) {
            return false;
        }
        if (index > 0U &&
            spatial_points_are_equal(&ring->points[index - 1U], &ring->points[index])) {
            return false;
        }
    }
    return validity_ring_is_simple(ring);
}

static bool validity_ring_is_simple(const struct spatial_distance_ring *ring) {
    uint32_t segment_count = ring->point_count - 1U;

    for (uint32_t left_index = 0U; left_index < segment_count; ++left_index) {
        for (uint32_t right_index = left_index + 1U; right_index < segment_count; ++right_index) {
            if (right_index == left_index + 1U ||
                (left_index == 0U && right_index == segment_count - 1U)) {
                continue;
            }
            if (validity_ring_segments_intersect_invalidly(ring, left_index, right_index)) {
                return false;
            }
        }
    }
    return true;
}

static bool validity_ring_segments_intersect_invalidly(
    const struct spatial_distance_ring *ring,
    uint32_t left_index,
    uint32_t right_index
) {
    struct spatial_segment left = ring_segment(ring, left_index);
    struct spatial_segment right = ring_segment(ring, right_index);

    return segments_intersect(&left, &right);
}

static bool validity_polygon_holes_are_valid(const struct spatial_distance_geometry *polygon) {
    const struct spatial_distance_ring *exterior = &polygon->rings[0];

    for (uint32_t hole_index = 1U; hole_index < polygon->ring_count; ++hole_index) {
        const struct spatial_distance_ring *hole = &polygon->rings[hole_index];

        if (validity_rings_intersect_invalidly(exterior, hole)) {
            return false;
        }
        for (uint32_t point_index = 0U; point_index + 1U < hole->point_count; ++point_index) {
            if (!validity_ring_point_is_inside_or_on_boundary(
                    exterior,
                    &hole->points[point_index]
                )) {
                return false;
            }
        }
        for (uint32_t previous_index = 1U; previous_index < hole_index; ++previous_index) {
            const struct spatial_distance_ring *previous = &polygon->rings[previous_index];

            if (validity_rings_intersect_invalidly(previous, hole)) {
                return false;
            }
            for (uint32_t point_index = 0U; point_index + 1U < hole->point_count; ++point_index) {
                if (validity_ring_contains_point_interior(previous, &hole->points[point_index])) {
                    return false;
                }
            }
            for (uint32_t point_index = 0U; point_index + 1U < previous->point_count;
                 ++point_index) {
                if (validity_ring_contains_point_interior(hole, &previous->points[point_index])) {
                    return false;
                }
            }
        }
    }
    return true;
}

static bool validity_ring_point_is_inside_or_on_boundary(
    const struct spatial_distance_ring *ring,
    const struct spatial_point *point
) {
    enum spatial_point_ring_relation relation = point_ring_relation(ring, point);

    return relation == SPATIAL_POINT_RING_INSIDE || relation == SPATIAL_POINT_RING_BOUNDARY;
}

static bool validity_ring_contains_point_interior(
    const struct spatial_distance_ring *ring,
    const struct spatial_point *point
) {
    return point_ring_relation(ring, point) == SPATIAL_POINT_RING_INSIDE;
}

static bool validity_rings_intersect_invalidly(
    const struct spatial_distance_ring *left,
    const struct spatial_distance_ring *right
) {
    for (uint32_t left_index = 0U; left_index + 1U < left->point_count; ++left_index) {
        struct spatial_segment left_segment = ring_segment(left, left_index);

        for (uint32_t right_index = 0U; right_index + 1U < right->point_count; ++right_index) {
            struct spatial_segment right_segment = ring_segment(right, right_index);

            if (validity_segment_intersection_is_invalid(&left_segment, &right_segment, true)) {
                return true;
            }
        }
    }
    return false;
}

static bool validity_polygons_intersect_invalidly(
    const struct spatial_distance_geometry *left,
    const struct spatial_distance_geometry *right
) {
    for (uint32_t left_ring = 0U; left_ring < left->ring_count; ++left_ring) {
        for (uint32_t right_ring = 0U; right_ring < right->ring_count; ++right_ring) {
            if (validity_rings_intersect_invalidly(
                    &left->rings[left_ring],
                    &right->rings[right_ring]
                )) {
                return true;
            }
        }
    }
    for (uint32_t index = 0U; index + 1U < right->rings[0].point_count; ++index) {
        if (validity_polygon_contains_point_interior(left, &right->rings[0].points[index])) {
            return true;
        }
    }
    for (uint32_t index = 0U; index + 1U < left->rings[0].point_count; ++index) {
        if (validity_polygon_contains_point_interior(right, &left->rings[0].points[index])) {
            return true;
        }
    }
    return false;
}

static bool validity_polygon_contains_point_interior(
    const struct spatial_distance_geometry *polygon,
    const struct spatial_point *point
) {
    if (point_ring_relation(&polygon->rings[0], point) != SPATIAL_POINT_RING_INSIDE) {
        return false;
    }
    for (uint32_t index = 1U; index < polygon->ring_count; ++index) {
        enum spatial_point_ring_relation relation =
            point_ring_relation(&polygon->rings[index], point);

        if (relation == SPATIAL_POINT_RING_INSIDE || relation == SPATIAL_POINT_RING_BOUNDARY) {
            return false;
        }
    }
    return true;
}

static bool validity_segment_intersection_is_invalid(
    const struct spatial_segment *left, // NOLINT(bugprone-easily-swappable-parameters)
    const struct spatial_segment *right,
    bool allow_endpoint_touch
) {
    bool shares_endpoint = false;

    if (!segments_intersect(left, right)) {
        return false;
    }
    if (!allow_endpoint_touch || spatial_segments_are_same_undirected(left, right) ||
        spatial_point_is_on_segment_interior(&left->start, right) ||
        spatial_point_is_on_segment_interior(&left->end, right) ||
        spatial_point_is_on_segment_interior(&right->start, left) ||
        spatial_point_is_on_segment_interior(&right->end, left)) {
        return true;
    }
    shares_endpoint = spatial_point_is_segment_endpoint(&left->start, right) ||
                      spatial_point_is_segment_endpoint(&left->end, right) ||
                      spatial_point_is_segment_endpoint(&right->start, left) ||
                      spatial_point_is_segment_endpoint(&right->end, left);
    return !shares_endpoint;
}

static bool spatial_segments_are_same_undirected(
    const struct spatial_segment *left,
    const struct spatial_segment *right
) {
    return (spatial_points_are_equal(&left->start, &right->start) &&
            spatial_points_are_equal(&left->end, &right->end)) ||
           (spatial_points_are_equal(&left->start, &right->end) &&
            spatial_points_are_equal(&left->end, &right->start));
}

static bool spatial_point_is_segment_endpoint(
    const struct spatial_point *point,
    const struct spatial_segment *segment
) {
    return spatial_points_are_equal(point, &segment->start) ||
           spatial_points_are_equal(point, &segment->end);
}

static bool spatial_point_is_on_segment_interior(
    const struct spatial_point *point,
    const struct spatial_segment *segment
) {
    return point_on_segment(point, segment) && !spatial_point_is_segment_endpoint(point, segment);
}

static int distance_between_geometries(
    const struct spatial_distance_geometry *left,
    const struct spatial_distance_geometry *right,
    double *out_distance,
    bool *out_has_distance
) {
    double distance = 0.0;

    if (out_distance == NULL || out_has_distance == NULL || left == NULL || right == NULL) {
        return -1;
    }
    *out_distance = 0.0;
    *out_has_distance = false;
    if (distance_geometry_is_empty(left) || distance_geometry_is_empty(right)) {
        return 0;
    }
    if (distance_geometry_is_collection(left)) {
        for (uint32_t index = 0U; index < left->child_count; ++index) {
            bool child_has_distance = false;
            double child_distance = 0.0;

            if (distance_between_geometries(
                    &left->children[index],
                    right,
                    &child_distance,
                    &child_has_distance
                ) != 0) {
                return -1;
            }
            if (child_has_distance) {
                distance_consider(child_distance, out_distance, out_has_distance);
            }
        }
        return 0;
    }
    if (distance_geometry_is_collection(right)) {
        for (uint32_t index = 0U; index < right->child_count; ++index) {
            bool child_has_distance = false;
            double child_distance = 0.0;

            if (distance_between_geometries(
                    left,
                    &right->children[index],
                    &child_distance,
                    &child_has_distance
                ) != 0) {
                return -1;
            }
            if (child_has_distance) {
                distance_consider(child_distance, out_distance, out_has_distance);
            }
        }
        return 0;
    }
    if (distance_between_simple_geometries(left, right, &distance) != 0) {
        return -1;
    }
    distance_consider(distance, out_distance, out_has_distance);
    return 0;
}

static int distance_between_simple_geometries(
    const struct spatial_distance_geometry *left,
    const struct spatial_distance_geometry *right,
    double *out_distance
) {
    if (left == NULL || right == NULL || out_distance == NULL) {
        return -1;
    }
    switch (left->type) {
    case MYLITE_SPATIAL_GEOMETRY_POINT:
        *out_distance = distance_point_to_geometry(&left->point, right);
        return 0;
    case MYLITE_SPATIAL_GEOMETRY_LINESTRING:
        *out_distance = distance_line_to_geometry(left, right);
        return 0;
    case MYLITE_SPATIAL_GEOMETRY_POLYGON:
        *out_distance = distance_polygon_to_geometry(left, right);
        return 0;
    default:
        break;
    }
    return -1;
}

static double distance_point_to_geometry(
    const struct spatial_point *point,
    const struct spatial_distance_geometry *geometry
) {
    switch (geometry->type) {
    case MYLITE_SPATIAL_GEOMETRY_POINT:
        return distance_point_to_point(point, &geometry->point);
    case MYLITE_SPATIAL_GEOMETRY_LINESTRING:
        return distance_point_to_line(point, geometry);
    case MYLITE_SPATIAL_GEOMETRY_POLYGON:
        return distance_point_to_polygon(point, geometry);
    default:
        break;
    }
    return DBL_MAX;
}

static double distance_line_to_geometry(
    const struct spatial_distance_geometry *line,
    const struct spatial_distance_geometry *geometry
) {
    switch (geometry->type) {
    case MYLITE_SPATIAL_GEOMETRY_POINT:
        return distance_point_to_line(&geometry->point, line);
    case MYLITE_SPATIAL_GEOMETRY_LINESTRING:
        return distance_line_to_line(line, geometry);
    case MYLITE_SPATIAL_GEOMETRY_POLYGON:
        return distance_line_to_polygon(line, geometry);
    default:
        break;
    }
    return DBL_MAX;
}

static double distance_polygon_to_geometry(
    const struct spatial_distance_geometry *polygon,
    const struct spatial_distance_geometry *geometry
) {
    switch (geometry->type) {
    case MYLITE_SPATIAL_GEOMETRY_POINT:
        return distance_point_to_polygon(&geometry->point, polygon);
    case MYLITE_SPATIAL_GEOMETRY_LINESTRING:
        return distance_line_to_polygon(geometry, polygon);
    case MYLITE_SPATIAL_GEOMETRY_POLYGON:
        return distance_polygon_to_polygon(polygon, geometry);
    default:
        break;
    }
    return DBL_MAX;
}

static double distance_point_to_line(
    const struct spatial_point *point,
    const struct spatial_distance_geometry *line
) {
    bool has_distance = false;
    double distance = 0.0;

    if (line->point_count == 1U) {
        return distance_point_to_point(point, &line->points[0]);
    }
    for (uint32_t index = 0U; index + 1U < line->point_count; ++index) {
        struct spatial_segment segment = line_segment(line, index);

        distance_consider(distance_point_to_segment(point, &segment), &distance, &has_distance);
    }
    return has_distance ? distance : DBL_MAX;
}

static double distance_point_to_polygon(
    const struct spatial_point *point,
    const struct spatial_distance_geometry *polygon
) {
    bool has_distance = false;
    double distance = 0.0;

    if (polygon_contains_point_surface(polygon, point)) {
        return 0.0;
    }
    for (uint32_t index = 0U; index < polygon->ring_count; ++index) {
        distance_consider(
            distance_point_to_ring(point, &polygon->rings[index]),
            &distance,
            &has_distance
        );
    }
    return has_distance ? distance : DBL_MAX;
}

static double distance_line_to_line(
    const struct spatial_distance_geometry *left,
    const struct spatial_distance_geometry *right
) {
    bool has_distance = false;
    double distance = 0.0;

    if (!line_has_segment(left) || !line_has_segment(right)) {
        return DBL_MAX;
    }
    for (uint32_t left_index = 0U; left_index + 1U < left->point_count; ++left_index) {
        struct spatial_segment left_segment = line_segment(left, left_index);

        for (uint32_t right_index = 0U; right_index + 1U < right->point_count; ++right_index) {
            struct spatial_segment right_segment = line_segment(right, right_index);
            double candidate = distance_segment_to_segment(&left_segment, &right_segment);

            if (double_near_zero(candidate)) {
                return 0.0;
            }
            distance_consider(candidate, &distance, &has_distance);
        }
    }
    return has_distance ? distance : DBL_MAX;
}

static double distance_line_to_polygon(
    const struct spatial_distance_geometry *line,
    const struct spatial_distance_geometry *polygon
) {
    bool has_distance = false;
    double distance = 0.0;

    for (uint32_t index = 0U; index < line->point_count; ++index) {
        if (polygon_contains_point_surface(polygon, &line->points[index])) {
            return 0.0;
        }
    }
    if (line_intersects_polygon(line, polygon)) {
        return 0.0;
    }
    for (uint32_t index = 0U; index < polygon->ring_count; ++index) {
        distance_consider(
            distance_line_to_ring(line, &polygon->rings[index]),
            &distance,
            &has_distance
        );
    }
    return has_distance ? distance : DBL_MAX;
}

static double distance_polygon_to_polygon(
    const struct spatial_distance_geometry *left,
    const struct spatial_distance_geometry *right
) {
    bool has_distance = false;
    double distance = 0.0;

    if (polygon_rings_intersect(left, right)) {
        return 0.0;
    }
    if (left->ring_count > 0U && left->rings[0].point_count > 0U &&
        polygon_contains_point_surface(right, &left->rings[0].points[0])) {
        return 0.0;
    }
    if (right->ring_count > 0U && right->rings[0].point_count > 0U &&
        polygon_contains_point_surface(left, &right->rings[0].points[0])) {
        return 0.0;
    }
    for (uint32_t left_index = 0U; left_index < left->ring_count; ++left_index) {
        for (uint32_t right_index = 0U; right_index < right->ring_count; ++right_index) {
            distance_consider(
                distance_ring_to_ring(&left->rings[left_index], &right->rings[right_index]),
                &distance,
                &has_distance
            );
        }
    }
    return has_distance ? distance : DBL_MAX;
}

static bool relation_geometry_contains(
    const struct spatial_distance_geometry *container,
    const struct spatial_distance_geometry *content
) {
    if (container == NULL || content == NULL || distance_geometry_is_empty(container) ||
        distance_geometry_is_empty(content)) {
        return false;
    }
    if (distance_geometry_is_collection(content)) {
        for (uint32_t index = 0U; index < content->child_count; ++index) {
            if (!relation_geometry_contains(container, &content->children[index])) {
                return false;
            }
        }
        return true;
    }
    if (distance_geometry_is_collection(container)) {
        for (uint32_t index = 0U; index < container->child_count; ++index) {
            if (relation_geometry_contains(&container->children[index], content)) {
                return true;
            }
        }
        return false;
    }
    return relation_simple_geometry_contains(container, content);
}

static int relation_geometry_dimension(const struct spatial_distance_geometry *geometry) {
    int dimension = -1;

    if (geometry == NULL || distance_geometry_is_empty(geometry)) {
        return -1;
    }
    switch (geometry->type) {
    case MYLITE_SPATIAL_GEOMETRY_POINT:
    case MYLITE_SPATIAL_GEOMETRY_MULTIPOINT:
        return 0;
    case MYLITE_SPATIAL_GEOMETRY_LINESTRING:
    case MYLITE_SPATIAL_GEOMETRY_MULTILINESTRING:
        return 1;
    case MYLITE_SPATIAL_GEOMETRY_POLYGON:
    case MYLITE_SPATIAL_GEOMETRY_MULTIPOLYGON:
        return 2;
    case MYLITE_SPATIAL_GEOMETRY_GEOMETRYCOLLECTION:
        for (uint32_t index = 0U; index < geometry->child_count; ++index) {
            int child_dimension = relation_geometry_dimension(&geometry->children[index]);

            if (child_dimension > dimension) {
                dimension = child_dimension;
            }
        }
        return dimension;
    default:
        break;
    }
    return -1;
}

static bool relation_geometry_interiors_intersect(
    const struct spatial_distance_geometry *left,
    const struct spatial_distance_geometry *right
) {
    if (left == NULL || right == NULL || distance_geometry_is_empty(left) ||
        distance_geometry_is_empty(right)) {
        return false;
    }
    if (distance_geometry_is_collection(left)) {
        for (uint32_t index = 0U; index < left->child_count; ++index) {
            if (relation_geometry_interiors_intersect(&left->children[index], right)) {
                return true;
            }
        }
        return false;
    }
    if (distance_geometry_is_collection(right)) {
        for (uint32_t index = 0U; index < right->child_count; ++index) {
            if (relation_geometry_interiors_intersect(left, &right->children[index])) {
                return true;
            }
        }
        return false;
    }
    return relation_simple_geometry_interiors_intersect(left, right);
}

static bool relation_geometry_crosses(
    const struct spatial_distance_geometry *left,
    const struct spatial_distance_geometry *right,
    int left_dimension,
    int right_dimension
) {
    if (left_dimension == 1 && right_dimension == 1) {
        return relation_lines_cross(left, right);
    }
    return relation_geometry_interiors_intersect(left, right) &&
           !relation_geometry_contains(right, left);
}

static bool relation_lines_cross(
    const struct spatial_distance_geometry *left,
    const struct spatial_distance_geometry *right
) {
    if (left == NULL || right == NULL) {
        return false;
    }
    if (distance_geometry_is_collection(left)) {
        for (uint32_t index = 0U; index < left->child_count; ++index) {
            if (relation_lines_cross(&left->children[index], right)) {
                return true;
            }
        }
        return false;
    }
    if (distance_geometry_is_collection(right)) {
        for (uint32_t index = 0U; index < right->child_count; ++index) {
            if (relation_lines_cross(left, &right->children[index])) {
                return true;
            }
        }
        return false;
    }
    if (left->type != MYLITE_SPATIAL_GEOMETRY_LINESTRING ||
        right->type != MYLITE_SPATIAL_GEOMETRY_LINESTRING) {
        return false;
    }
    return relation_line_segments_cross_at_point(left, right);
}

static bool relation_line_segments_cross_at_point(
    const struct spatial_distance_geometry *left,
    const struct spatial_distance_geometry *right
) {
    if (!line_has_segment(left) || !line_has_segment(right)) {
        return false;
    }
    for (uint32_t left_index = 0U; left_index + 1U < left->point_count; ++left_index) {
        struct spatial_segment left_segment = line_segment(left, left_index);

        for (uint32_t right_index = 0U; right_index + 1U < right->point_count; ++right_index) {
            struct spatial_segment right_segment = line_segment(right, right_index);

            if (!relation_segments_overlap_collinearly(&left_segment, &right_segment) &&
                relation_line_segments_have_interior_intersection(
                    left,
                    &left_segment,
                    right,
                    &right_segment
                )) {
                return true;
            }
        }
    }
    return false;
}

static bool relation_geometry_overlaps(
    const struct spatial_distance_geometry *left,
    const struct spatial_distance_geometry *right,
    int dimension
) {
    if (relation_geometry_contains(left, right) || relation_geometry_contains(right, left)) {
        return false;
    }
    if (dimension == 0) {
        return relation_point_sets_overlap(left, right);
    }
    if (dimension == 1) {
        return relation_lines_overlap(left, right);
    }
    if (dimension == 2) {
        return relation_polygons_overlap(left, right);
    }
    return false;
}

static bool relation_point_sets_overlap(
    const struct spatial_distance_geometry *left,
    const struct spatial_distance_geometry *right
) {
    return relation_point_set_common_count(left, right) > 0U;
}

static size_t relation_point_set_common_count(
    const struct spatial_distance_geometry *left,
    const struct spatial_distance_geometry *right
) {
    size_t common_count = 0U;

    if (left == NULL || right == NULL) {
        return 0U;
    }
    if (left->type == MYLITE_SPATIAL_GEOMETRY_POINT) {
        return relation_geometry_has_point(right, &left->point) ? 1U : 0U;
    }
    if (!distance_geometry_is_collection(left)) {
        return 0U;
    }
    for (uint32_t index = 0U; index < left->child_count; ++index) {
        common_count += relation_point_set_common_count(&left->children[index], right);
    }
    return common_count;
}

static bool relation_geometry_has_point(
    const struct spatial_distance_geometry *geometry,
    const struct spatial_point *point
) {
    if (geometry == NULL || point == NULL) {
        return false;
    }
    if (geometry->type == MYLITE_SPATIAL_GEOMETRY_POINT) {
        return spatial_points_are_equal(&geometry->point, point);
    }
    if (!distance_geometry_is_collection(geometry)) {
        return false;
    }
    for (uint32_t index = 0U; index < geometry->child_count; ++index) {
        if (relation_geometry_has_point(&geometry->children[index], point)) {
            return true;
        }
    }
    return false;
}

static bool relation_lines_overlap(
    const struct spatial_distance_geometry *left,
    const struct spatial_distance_geometry *right
) {
    if (left == NULL || right == NULL) {
        return false;
    }
    if (distance_geometry_is_collection(left)) {
        for (uint32_t index = 0U; index < left->child_count; ++index) {
            if (relation_lines_overlap(&left->children[index], right)) {
                return true;
            }
        }
        return false;
    }
    if (distance_geometry_is_collection(right)) {
        for (uint32_t index = 0U; index < right->child_count; ++index) {
            if (relation_lines_overlap(left, &right->children[index])) {
                return true;
            }
        }
        return false;
    }
    if (left->type != MYLITE_SPATIAL_GEOMETRY_LINESTRING ||
        right->type != MYLITE_SPATIAL_GEOMETRY_LINESTRING) {
        return false;
    }
    return relation_line_segments_overlap_with_length(left, right);
}

static bool relation_line_segments_overlap_with_length(
    const struct spatial_distance_geometry *left,
    const struct spatial_distance_geometry *right
) {
    if (!line_has_segment(left) || !line_has_segment(right)) {
        return false;
    }
    for (uint32_t left_index = 0U; left_index + 1U < left->point_count; ++left_index) {
        struct spatial_segment left_segment = line_segment(left, left_index);

        for (uint32_t right_index = 0U; right_index + 1U < right->point_count; ++right_index) {
            struct spatial_segment right_segment = line_segment(right, right_index);

            if (relation_segments_overlap_collinearly(&left_segment, &right_segment) &&
                relation_collinear_segments_overlap_with_length(&left_segment, &right_segment)) {
                return true;
            }
        }
    }
    return false;
}

static bool relation_polygons_overlap(
    const struct spatial_distance_geometry *left,
    const struct spatial_distance_geometry *right
) {
    if (left == NULL || right == NULL) {
        return false;
    }
    if (distance_geometry_is_collection(left)) {
        for (uint32_t index = 0U; index < left->child_count; ++index) {
            if (relation_polygons_overlap(&left->children[index], right)) {
                return true;
            }
        }
        return false;
    }
    if (distance_geometry_is_collection(right)) {
        for (uint32_t index = 0U; index < right->child_count; ++index) {
            if (relation_polygons_overlap(left, &right->children[index])) {
                return true;
            }
        }
        return false;
    }
    if (left->type != MYLITE_SPATIAL_GEOMETRY_POLYGON ||
        right->type != MYLITE_SPATIAL_GEOMETRY_POLYGON) {
        return false;
    }
    return relation_polygons_have_interior_intersection(left, right);
}

static bool relation_simple_geometry_interiors_intersect(
    const struct spatial_distance_geometry *left,
    const struct spatial_distance_geometry *right
) {
    switch (left->type) {
    case MYLITE_SPATIAL_GEOMETRY_POINT:
        if (right->type == MYLITE_SPATIAL_GEOMETRY_POINT) {
            return spatial_points_are_equal(&left->point, &right->point);
        }
        if (right->type == MYLITE_SPATIAL_GEOMETRY_LINESTRING) {
            return relation_point_is_on_line_interior(&left->point, right);
        }
        if (right->type == MYLITE_SPATIAL_GEOMETRY_POLYGON) {
            return validity_polygon_contains_point_interior(right, &left->point);
        }
        break;
    case MYLITE_SPATIAL_GEOMETRY_LINESTRING:
        if (right->type == MYLITE_SPATIAL_GEOMETRY_POINT) {
            return relation_point_is_on_line_interior(&right->point, left);
        }
        if (right->type == MYLITE_SPATIAL_GEOMETRY_LINESTRING) {
            return relation_lines_have_interior_intersection(left, right);
        }
        if (right->type == MYLITE_SPATIAL_GEOMETRY_POLYGON) {
            return relation_line_polygon_interiors_intersect(left, right);
        }
        break;
    case MYLITE_SPATIAL_GEOMETRY_POLYGON:
        if (right->type == MYLITE_SPATIAL_GEOMETRY_POINT) {
            return validity_polygon_contains_point_interior(left, &right->point);
        }
        if (right->type == MYLITE_SPATIAL_GEOMETRY_LINESTRING) {
            return relation_line_polygon_interiors_intersect(right, left);
        }
        if (right->type == MYLITE_SPATIAL_GEOMETRY_POLYGON) {
            return relation_polygons_have_interior_intersection(left, right);
        }
        break;
    default:
        break;
    }
    return false;
}

static bool relation_lines_have_interior_intersection(
    const struct spatial_distance_geometry *left,
    const struct spatial_distance_geometry *right
) {
    if (!line_has_segment(left) || !line_has_segment(right)) {
        return false;
    }
    for (uint32_t left_index = 0U; left_index + 1U < left->point_count; ++left_index) {
        struct spatial_segment left_segment = line_segment(left, left_index);

        for (uint32_t right_index = 0U; right_index + 1U < right->point_count; ++right_index) {
            struct spatial_segment right_segment = line_segment(right, right_index);

            if (relation_line_segments_have_interior_intersection(
                    left,
                    &left_segment,
                    right,
                    &right_segment
                )) {
                return true;
            }
        }
    }
    return false;
}

static bool relation_line_segments_have_interior_intersection(
    const struct spatial_distance_geometry *left_line,
    const struct spatial_segment *left_segment,
    const struct spatial_distance_geometry *right_line,
    const struct spatial_segment *right_segment
) {
    bool endpoint_intersection = false;

    if (!segments_intersect(left_segment, right_segment)) {
        return false;
    }
    if (relation_segments_overlap_collinearly(left_segment, right_segment)) {
        return relation_collinear_segments_overlap_with_length(left_segment, right_segment);
    }
    if (relation_segment_intersection_endpoint_has_line_interiors(
            left_line,
            left_segment,
            right_line,
            right_segment
        )) {
        return true;
    }
    endpoint_intersection = point_on_segment(&left_segment->start, right_segment) ||
                            point_on_segment(&left_segment->end, right_segment) ||
                            point_on_segment(&right_segment->start, left_segment) ||
                            point_on_segment(&right_segment->end, left_segment);
    return !endpoint_intersection;
}

static bool relation_collinear_segments_overlap_with_length(
    const struct spatial_segment *left,
    const struct spatial_segment *right
) {
    double left_delta_x = fabs(left->end.coordinate_x - left->start.coordinate_x);
    double left_delta_y = fabs(left->end.coordinate_y - left->start.coordinate_y);
    bool use_x = left_delta_x >= left_delta_y;
    double left_min = use_x ? fmin(left->start.coordinate_x, left->end.coordinate_x)
                            : fmin(left->start.coordinate_y, left->end.coordinate_y);
    double left_max = use_x ? fmax(left->start.coordinate_x, left->end.coordinate_x)
                            : fmax(left->start.coordinate_y, left->end.coordinate_y);
    double right_min = use_x ? fmin(right->start.coordinate_x, right->end.coordinate_x)
                             : fmin(right->start.coordinate_y, right->end.coordinate_y);
    double right_max = use_x ? fmax(right->start.coordinate_x, right->end.coordinate_x)
                             : fmax(right->start.coordinate_y, right->end.coordinate_y);
    double overlap_min = fmax(left_min, right_min);
    double overlap_max = fmin(left_max, right_max);

    return overlap_max - overlap_min > spatial_distance_epsilon;
}

static bool relation_segment_intersection_endpoint_has_line_interiors(
    const struct spatial_distance_geometry *left_line,
    const struct spatial_segment *left_segment,
    const struct spatial_distance_geometry *right_line,
    const struct spatial_segment *right_segment
) {
    const struct spatial_point *left_points[spatial_segment_endpoint_count] = {
        &left_segment->start,
        &left_segment->end,
    };
    const struct spatial_point *right_points[spatial_segment_endpoint_count] = {
        &right_segment->start,
        &right_segment->end,
    };

    for (size_t index = 0U; index < spatial_segment_endpoint_count; ++index) {
        if (point_on_segment(left_points[index], right_segment) &&
            relation_point_is_on_line_interior(left_points[index], left_line) &&
            relation_point_is_on_line_interior(left_points[index], right_line)) {
            return true;
        }
        if (point_on_segment(right_points[index], left_segment) &&
            relation_point_is_on_line_interior(right_points[index], left_line) &&
            relation_point_is_on_line_interior(right_points[index], right_line)) {
            return true;
        }
    }
    return false;
}

static bool relation_line_polygon_interiors_intersect(
    const struct spatial_distance_geometry *line,
    const struct spatial_distance_geometry *polygon
) {
    if (!line_has_segment(line)) {
        return false;
    }
    for (uint32_t index = 0U; index < line->point_count; ++index) {
        if (validity_polygon_contains_point_interior(polygon, &line->points[index])) {
            return true;
        }
    }
    for (uint32_t index = 0U; index + 1U < line->point_count; ++index) {
        struct spatial_segment segment = line_segment(line, index);
        struct spatial_point midpoint = segment_midpoint(&segment);

        if (validity_polygon_contains_point_interior(polygon, &midpoint)) {
            return true;
        }
    }
    return false;
}

static bool relation_polygons_have_interior_intersection(
    const struct spatial_distance_geometry *left,
    const struct spatial_distance_geometry *right
) {
    if (left->ring_count == 0U || right->ring_count == 0U) {
        return false;
    }
    if (relation_polygon_surface_contains_polygon(left, right, NULL) &&
        relation_polygon_surface_contains_polygon(right, left, NULL)) {
        return true;
    }
    for (uint32_t index = 0U; index + 1U < right->rings[0].point_count; ++index) {
        if (validity_polygon_contains_point_interior(left, &right->rings[0].points[index])) {
            return true;
        }
    }
    for (uint32_t index = 0U; index + 1U < left->rings[0].point_count; ++index) {
        if (validity_polygon_contains_point_interior(right, &left->rings[0].points[index])) {
            return true;
        }
    }
    return relation_polygon_boundaries_cross(left, right);
}

static bool relation_polygon_boundaries_cross(
    const struct spatial_distance_geometry *left,
    const struct spatial_distance_geometry *right
) {
    for (uint32_t left_ring_index = 0U; left_ring_index < left->ring_count; ++left_ring_index) {
        const struct spatial_distance_ring *left_ring = &left->rings[left_ring_index];

        if (!ring_has_segment(left_ring)) {
            continue;
        }
        for (uint32_t right_ring_index = 0U; right_ring_index < right->ring_count;
             ++right_ring_index) {
            const struct spatial_distance_ring *right_ring = &right->rings[right_ring_index];

            if (!ring_has_segment(right_ring)) {
                continue;
            }
            for (uint32_t left_index = 0U; left_index + 1U < left_ring->point_count; ++left_index) {
                struct spatial_segment left_segment = ring_segment(left_ring, left_index);

                for (uint32_t right_index = 0U; right_index + 1U < right_ring->point_count;
                     ++right_index) {
                    struct spatial_segment right_segment = ring_segment(right_ring, right_index);
                    bool endpoint_intersection = false;

                    if (!segments_intersect(&left_segment, &right_segment) ||
                        relation_segments_overlap_collinearly(&left_segment, &right_segment)) {
                        continue;
                    }
                    endpoint_intersection = point_on_segment(&left_segment.start, &right_segment) ||
                                            point_on_segment(&left_segment.end, &right_segment) ||
                                            point_on_segment(&right_segment.start, &left_segment) ||
                                            point_on_segment(&right_segment.end, &left_segment);
                    if (!endpoint_intersection) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

static bool relation_simple_geometry_contains(
    const struct spatial_distance_geometry *container,
    const struct spatial_distance_geometry *content
) {
    switch (container->type) {
    case MYLITE_SPATIAL_GEOMETRY_POINT:
        return content->type == MYLITE_SPATIAL_GEOMETRY_POINT &&
               spatial_points_are_equal(&container->point, &content->point);
    case MYLITE_SPATIAL_GEOMETRY_LINESTRING:
        if (content->type == MYLITE_SPATIAL_GEOMETRY_POINT) {
            return relation_line_contains_point(container, &content->point);
        }
        if (content->type == MYLITE_SPATIAL_GEOMETRY_LINESTRING) {
            return relation_line_contains_line(container, content);
        }
        return false;
    case MYLITE_SPATIAL_GEOMETRY_POLYGON:
        if (content->type == MYLITE_SPATIAL_GEOMETRY_POINT) {
            return validity_polygon_contains_point_interior(container, &content->point);
        }
        if (content->type == MYLITE_SPATIAL_GEOMETRY_LINESTRING) {
            return relation_polygon_contains_line(container, content);
        }
        if (content->type == MYLITE_SPATIAL_GEOMETRY_POLYGON) {
            return relation_polygon_contains_polygon(container, content);
        }
        return false;
    default:
        break;
    }
    return false;
}

static bool relation_line_contains_point(
    const struct spatial_distance_geometry *line,
    const struct spatial_point *point
) {
    return relation_point_is_on_line_interior(point, line);
}

static bool relation_point_is_on_line_interior(
    const struct spatial_point *point,
    const struct spatial_distance_geometry *line
) {
    return simplicity_point_is_on_line(point, line) &&
           !simplicity_point_is_line_boundary(point, line);
}

static bool relation_line_contains_line(
    const struct spatial_distance_geometry *container,
    const struct spatial_distance_geometry *content
) {
    bool has_interior_point = false;

    if (!line_has_segment(container) || !line_has_segment(content)) {
        return false;
    }
    for (uint32_t index = 0U; index < content->point_count; ++index) {
        if (!simplicity_point_is_on_line(&content->points[index], container)) {
            return false;
        }
        if (relation_point_is_on_line_interior(&content->points[index], container)) {
            has_interior_point = true;
        }
    }
    for (uint32_t index = 0U; index + 1U < content->point_count; ++index) {
        struct spatial_segment segment = line_segment(content, index);
        struct spatial_point midpoint = segment_midpoint(&segment);

        if (!simplicity_point_is_on_line(&midpoint, container)) {
            return false;
        }
        if (relation_point_is_on_line_interior(&midpoint, container)) {
            has_interior_point = true;
        }
    }
    return has_interior_point;
}

static bool relation_polygon_contains_line(
    const struct spatial_distance_geometry *polygon,
    const struct spatial_distance_geometry *line
) {
    bool has_interior_point = false;

    if (!line_has_segment(line)) {
        return false;
    }
    for (uint32_t index = 0U; index < line->point_count; ++index) {
        if (!relation_polygon_contains_point_sample(
                polygon,
                &line->points[index],
                &has_interior_point
            )) {
            return false;
        }
    }
    for (uint32_t index = 0U; index + 1U < line->point_count; ++index) {
        struct spatial_segment segment = line_segment(line, index);
        struct spatial_point midpoint = segment_midpoint(&segment);

        if (!relation_polygon_contains_point_sample(polygon, &midpoint, &has_interior_point)) {
            return false;
        }
        if (relation_segment_crosses_polygon_boundary(polygon, &segment)) {
            return false;
        }
    }
    return has_interior_point;
}

static bool relation_polygon_contains_polygon(
    const struct spatial_distance_geometry *container,
    const struct spatial_distance_geometry *content
) {
    bool has_interior_sample = false;

    if (!relation_polygon_surface_contains_polygon(container, content, &has_interior_sample)) {
        return false;
    }
    if (has_interior_sample) {
        return true;
    }
    return relation_polygon_surface_contains_container(content, container);
}

static bool relation_polygon_surface_contains_container(
    const struct spatial_distance_geometry *content, // NOLINT(bugprone-easily-swappable-parameters)
    const struct spatial_distance_geometry *container
) {
    const struct spatial_distance_geometry *surface_container = content;
    const struct spatial_distance_geometry *surface_content = container;

    return relation_polygon_surface_contains_polygon(surface_container, surface_content, NULL);
}

static bool relation_polygon_surface_contains_polygon(
    const struct spatial_distance_geometry *container,
    const struct spatial_distance_geometry *content,
    bool *out_has_interior_sample
) {
    bool has_interior_sample = false;

    if (container == NULL || content == NULL || container->ring_count == 0U ||
        content->ring_count == 0U) {
        return false;
    }
    for (uint32_t index = 0U; index < content->ring_count; ++index) {
        if (!relation_polygon_contains_ring_samples(
                container,
                &content->rings[index],
                &has_interior_sample
            )) {
            return false;
        }
    }
    if (out_has_interior_sample != NULL) {
        *out_has_interior_sample = has_interior_sample;
    }
    return true;
}

static bool relation_polygon_contains_ring_samples(
    const struct spatial_distance_geometry *container,
    const struct spatial_distance_ring *ring,
    bool *io_has_interior_sample
) {
    if (!ring_has_segment(ring)) {
        return false;
    }
    for (uint32_t index = 0U; index < ring->point_count; ++index) {
        if (!relation_polygon_contains_point_sample(
                container,
                &ring->points[index],
                io_has_interior_sample
            )) {
            return false;
        }
    }
    for (uint32_t index = 0U; index + 1U < ring->point_count; ++index) {
        struct spatial_segment segment = ring_segment(ring, index);
        struct spatial_point midpoint = segment_midpoint(&segment);

        if (!relation_polygon_contains_point_sample(container, &midpoint, io_has_interior_sample)) {
            return false;
        }
        if (relation_segment_crosses_polygon_boundary(container, &segment)) {
            return false;
        }
    }
    return true;
}

static bool relation_polygon_contains_point_sample(
    const struct spatial_distance_geometry *container,
    const struct spatial_point *point,
    bool *io_has_interior_sample
) {
    if (!polygon_contains_point_surface(container, point)) {
        return false;
    }
    if (io_has_interior_sample != NULL &&
        validity_polygon_contains_point_interior(container, point)) {
        *io_has_interior_sample = true;
    }
    return true;
}

static bool relation_segment_crosses_polygon_boundary(
    const struct spatial_distance_geometry *polygon,
    const struct spatial_segment *segment
) {
    if (polygon == NULL || segment == NULL) {
        return false;
    }
    for (uint32_t ring_index = 0U; ring_index < polygon->ring_count; ++ring_index) {
        const struct spatial_distance_ring *ring = &polygon->rings[ring_index];

        if (!ring_has_segment(ring)) {
            continue;
        }
        for (uint32_t point_index = 0U; point_index + 1U < ring->point_count; ++point_index) {
            struct spatial_segment boundary = ring_segment(ring, point_index);
            bool intersects_at_content_endpoint = point_on_segment(&segment->start, &boundary) ||
                                                  point_on_segment(&segment->end, &boundary);

            if (!segments_intersect(segment, &boundary)) {
                continue;
            }
            if (relation_segments_overlap_collinearly(segment, &boundary)) {
                continue;
            }
            if (spatial_point_is_on_segment_interior(&boundary.start, segment) ||
                spatial_point_is_on_segment_interior(&boundary.end, segment)) {
                return true;
            }
            if (!intersects_at_content_endpoint) {
                return true;
            }
        }
    }
    return false;
}

static bool relation_segments_overlap_collinearly(
    const struct spatial_segment *left, // NOLINT(bugprone-easily-swappable-parameters)
    const struct spatial_segment *right
) {
    if (left == NULL || right == NULL ||
        !double_near_zero(point_cross_product(&left->start, &left->end, &right->start)) ||
        !double_near_zero(point_cross_product(&left->start, &left->end, &right->end))) {
        return false;
    }
    return point_on_segment(&left->start, right) || point_on_segment(&left->end, right) ||
           point_on_segment(&right->start, left) || point_on_segment(&right->end, left);
}

static struct spatial_point segment_midpoint(const struct spatial_segment *segment) {
    return (struct spatial_point){
        .coordinate_x =
            (segment->start.coordinate_x + segment->end.coordinate_x) / spatial_midpoint_divisor,
        .coordinate_y =
            (segment->start.coordinate_y + segment->end.coordinate_y) / spatial_midpoint_divisor,
    };
}

static bool distance_geometry_is_collection(const struct spatial_distance_geometry *geometry) {
    return geometry != NULL && (geometry->type == MYLITE_SPATIAL_GEOMETRY_MULTIPOINT ||
                                geometry->type == MYLITE_SPATIAL_GEOMETRY_MULTILINESTRING ||
                                geometry->type == MYLITE_SPATIAL_GEOMETRY_MULTIPOLYGON ||
                                geometry->type == MYLITE_SPATIAL_GEOMETRY_GEOMETRYCOLLECTION);
}

static bool distance_geometry_is_empty(const struct spatial_distance_geometry *geometry) {
    return distance_geometry_is_collection(geometry) && geometry->child_count == 0U;
}

static bool polygon_contains_point_surface(
    const struct spatial_distance_geometry *polygon,
    const struct spatial_point *point
) {
    enum spatial_point_ring_relation relation = SPATIAL_POINT_RING_OUTSIDE;

    if (polygon == NULL || point == NULL || polygon->ring_count == 0U) {
        return false;
    }
    relation = point_ring_relation(&polygon->rings[0], point);
    if (relation == SPATIAL_POINT_RING_BOUNDARY) {
        return true;
    }
    if (relation != SPATIAL_POINT_RING_INSIDE) {
        return false;
    }
    for (uint32_t index = 1U; index < polygon->ring_count; ++index) {
        relation = point_ring_relation(&polygon->rings[index], point);
        if (relation == SPATIAL_POINT_RING_BOUNDARY) {
            return true;
        }
        if (relation == SPATIAL_POINT_RING_INSIDE) {
            return false;
        }
    }
    return true;
}

static enum spatial_point_ring_relation point_ring_relation(
    const struct spatial_distance_ring *ring,
    const struct spatial_point *point
) {
    bool inside = false;

    if (ring == NULL || point == NULL || ring->point_count < 3U) {
        return SPATIAL_POINT_RING_OUTSIDE;
    }
    for (uint32_t index = 0U, previous = ring->point_count - 1U; index < ring->point_count;
         previous = index++) {
        const struct spatial_point *current_point = &ring->points[index];
        const struct spatial_point *previous_point = &ring->points[previous];
        const struct spatial_segment segment = {.start = *previous_point, .end = *current_point};

        if (point_on_segment(point, &segment)) {
            return SPATIAL_POINT_RING_BOUNDARY;
        }
        if ((current_point->coordinate_y > point->coordinate_y) !=
            (previous_point->coordinate_y > point->coordinate_y)) {
            double crossing_x = ((previous_point->coordinate_x - current_point->coordinate_x) *
                                 (point->coordinate_y - current_point->coordinate_y) /
                                 (previous_point->coordinate_y - current_point->coordinate_y)) +
                                current_point->coordinate_x;

            if (point->coordinate_x < crossing_x) {
                inside = !inside;
            }
        }
    }
    return inside ? SPATIAL_POINT_RING_INSIDE : SPATIAL_POINT_RING_OUTSIDE;
}

static bool line_intersects_polygon(
    const struct spatial_distance_geometry *line, // NOLINT(bugprone-easily-swappable-parameters)
    const struct spatial_distance_geometry *polygon
) {
    if (!line_has_segment(line)) {
        return false;
    }
    for (uint32_t line_index = 0U; line_index + 1U < line->point_count; ++line_index) {
        struct spatial_segment line_segment_value = line_segment(line, line_index);

        for (uint32_t ring_index = 0U; ring_index < polygon->ring_count; ++ring_index) {
            const struct spatial_distance_ring *ring = &polygon->rings[ring_index];

            for (uint32_t point_index = 0U; point_index + 1U < ring->point_count; ++point_index) {
                struct spatial_segment ring_segment_value = ring_segment(ring, point_index);

                if (segments_intersect(&line_segment_value, &ring_segment_value)) {
                    return true;
                }
            }
        }
    }
    return false;
}

static bool polygon_rings_intersect(
    const struct spatial_distance_geometry *left,
    const struct spatial_distance_geometry *right
) {
    for (uint32_t left_ring_index = 0U; left_ring_index < left->ring_count; ++left_ring_index) {
        const struct spatial_distance_ring *left_ring = &left->rings[left_ring_index];

        if (!ring_has_segment(left_ring)) {
            continue;
        }
        for (uint32_t right_ring_index = 0U; right_ring_index < right->ring_count;
             ++right_ring_index) {
            const struct spatial_distance_ring *right_ring = &right->rings[right_ring_index];

            if (!ring_has_segment(right_ring)) {
                continue;
            }
            for (uint32_t left_index = 0U; left_index + 1U < left_ring->point_count; ++left_index) {
                struct spatial_segment left_segment = ring_segment(left_ring, left_index);

                for (uint32_t right_index = 0U; right_index + 1U < right_ring->point_count;
                     ++right_index) {
                    struct spatial_segment right_segment = ring_segment(right_ring, right_index);

                    if (segments_intersect(&left_segment, &right_segment)) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

static double distance_line_to_ring(
    const struct spatial_distance_geometry *line,
    const struct spatial_distance_ring *ring
) {
    bool has_distance = false;
    double distance = 0.0;

    if (!line_has_segment(line) || !ring_has_segment(ring)) {
        return DBL_MAX;
    }
    for (uint32_t line_index = 0U; line_index + 1U < line->point_count; ++line_index) {
        struct spatial_segment line_segment_value = line_segment(line, line_index);

        for (uint32_t ring_index = 0U; ring_index + 1U < ring->point_count; ++ring_index) {
            struct spatial_segment ring_segment_value = ring_segment(ring, ring_index);
            double candidate =
                distance_segment_to_segment(&line_segment_value, &ring_segment_value);

            if (double_near_zero(candidate)) {
                return 0.0;
            }
            distance_consider(candidate, &distance, &has_distance);
        }
    }
    return has_distance ? distance : DBL_MAX;
}

static double distance_ring_to_ring(
    const struct spatial_distance_ring *left,
    const struct spatial_distance_ring *right
) {
    bool has_distance = false;
    double distance = 0.0;

    if (!ring_has_segment(left) || !ring_has_segment(right)) {
        return DBL_MAX;
    }
    for (uint32_t left_index = 0U; left_index + 1U < left->point_count; ++left_index) {
        struct spatial_segment left_segment = ring_segment(left, left_index);

        for (uint32_t right_index = 0U; right_index + 1U < right->point_count; ++right_index) {
            struct spatial_segment right_segment = ring_segment(right, right_index);
            double candidate = distance_segment_to_segment(&left_segment, &right_segment);

            if (double_near_zero(candidate)) {
                return 0.0;
            }
            distance_consider(candidate, &distance, &has_distance);
        }
    }
    return has_distance ? distance : DBL_MAX;
}

static double distance_point_to_ring(
    const struct spatial_point *point,
    const struct spatial_distance_ring *ring
) {
    bool has_distance = false;
    double distance = 0.0;

    if (!ring_has_segment(ring)) {
        return DBL_MAX;
    }
    for (uint32_t index = 0U; index + 1U < ring->point_count; ++index) {
        struct spatial_segment segment = ring_segment(ring, index);

        distance_consider(distance_point_to_segment(point, &segment), &distance, &has_distance);
    }
    return has_distance ? distance : DBL_MAX;
}

static bool ring_has_segment(const struct spatial_distance_ring *ring) {
    return ring != NULL && ring->points != NULL && ring->point_count > 1U;
}

static bool line_has_segment(const struct spatial_distance_geometry *line) {
    return line != NULL && line->points != NULL && line->point_count > 1U;
}

static struct spatial_segment ring_segment(
    const struct spatial_distance_ring *ring,
    uint32_t index
) {
    return (struct spatial_segment){.start = ring->points[index], .end = ring->points[index + 1U]};
}

static struct spatial_segment line_segment(
    const struct spatial_distance_geometry *line,
    uint32_t index
) {
    return (struct spatial_segment){.start = line->points[index], .end = line->points[index + 1U]};
}

static double distance_point_to_segment(
    const struct spatial_point *point,
    const struct spatial_segment *segment
) {
    double delta_x = segment->end.coordinate_x - segment->start.coordinate_x;
    double delta_y = segment->end.coordinate_y - segment->start.coordinate_y;
    double length_squared = (delta_x * delta_x) + (delta_y * delta_y);
    double position = 0.0;
    struct spatial_point projected = segment->start;

    if (double_near_zero(length_squared)) {
        return distance_point_to_point(point, &segment->start);
    }
    position = (((point->coordinate_x - segment->start.coordinate_x) * delta_x) +
                ((point->coordinate_y - segment->start.coordinate_y) * delta_y)) /
               length_squared;
    if (position <= 0.0) {
        return distance_point_to_point(point, &segment->start);
    }
    if (position >= 1.0) {
        return distance_point_to_point(point, &segment->end);
    }
    projected.coordinate_x = segment->start.coordinate_x + (position * delta_x);
    projected.coordinate_y = segment->start.coordinate_y + (position * delta_y);
    return distance_point_to_point(point, &projected);
}

static double distance_segment_to_segment(
    const struct spatial_segment *left,
    const struct spatial_segment *right
) {
    double distance = 0.0;
    bool has_distance = false;

    if (segments_intersect(left, right)) {
        return 0.0;
    }
    distance_consider(distance_point_to_segment(&left->start, right), &distance, &has_distance);
    distance_consider(distance_point_to_segment(&left->end, right), &distance, &has_distance);
    distance_consider(distance_point_to_segment(&right->start, left), &distance, &has_distance);
    distance_consider(distance_point_to_segment(&right->end, left), &distance, &has_distance);
    return has_distance ? distance : DBL_MAX;
}

static double distance_point_to_point(
    const struct spatial_point *left,
    const struct spatial_point *right
) {
    double delta_x = left->coordinate_x - right->coordinate_x;
    double delta_y = left->coordinate_y - right->coordinate_y;

    return sqrt((delta_x * delta_x) + (delta_y * delta_y));
}

static bool segments_intersect(
    const struct spatial_segment *left,
    const struct spatial_segment *right
) {
    double left_start = point_cross_product(&left->start, &left->end, &right->start);
    double left_end = point_cross_product(&left->start, &left->end, &right->end);
    double right_start = point_cross_product(&right->start, &right->end, &left->start);
    double right_end = point_cross_product(&right->start, &right->end, &left->end);

    if (point_on_segment(&right->start, left) || point_on_segment(&right->end, left) ||
        point_on_segment(&left->start, right) || point_on_segment(&left->end, right)) {
        return true;
    }
    return ((left_start > spatial_distance_epsilon && left_end < -spatial_distance_epsilon) ||
            (left_start < -spatial_distance_epsilon && left_end > spatial_distance_epsilon)) &&
           ((right_start > spatial_distance_epsilon && right_end < -spatial_distance_epsilon) ||
            (right_start < -spatial_distance_epsilon && right_end > spatial_distance_epsilon));
}

static bool point_on_segment(
    const struct spatial_point *point,
    const struct spatial_segment *segment
) {
    if (!double_near_zero(point_cross_product(&segment->start, &segment->end, point))) {
        return false;
    }
    return point->coordinate_x >= fmin(segment->start.coordinate_x, segment->end.coordinate_x) -
                                      spatial_distance_epsilon &&
           point->coordinate_x <= fmax(segment->start.coordinate_x, segment->end.coordinate_x) +
                                      spatial_distance_epsilon &&
           point->coordinate_y >= fmin(segment->start.coordinate_y, segment->end.coordinate_y) -
                                      spatial_distance_epsilon &&
           point->coordinate_y <= fmax(segment->start.coordinate_y, segment->end.coordinate_y) +
                                      spatial_distance_epsilon;
}

static double point_cross_product(
    const struct spatial_point *origin,
    const struct spatial_point *left,
    const struct spatial_point *right
) {
    return ((left->coordinate_x - origin->coordinate_x) *
            (right->coordinate_y - origin->coordinate_y)) -
           ((left->coordinate_y - origin->coordinate_y) *
            (right->coordinate_x - origin->coordinate_x));
}

static bool double_near_zero(double value) {
    return fabs(value) <= spatial_distance_epsilon;
}

static void distance_consider(double candidate, double *io_distance, bool *io_has_distance) {
    if (!isfinite(candidate) || candidate == DBL_MAX || io_distance == NULL ||
        io_has_distance == NULL) {
        return;
    }
    if (double_near_zero(candidate)) {
        *io_distance = 0.0;
        *io_has_distance = true;
        return;
    }
    if (!*io_has_distance || candidate < *io_distance) {
        *io_distance = candidate;
        *io_has_distance = true;
    }
}

static int wkb_swap_xy_at(
    struct spatial_wkb_cursor *cursor,
    struct spatial_buffer *out_wkb,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    bool little_endian = false;
    enum mylite_spatial_geometry_type type = MYLITE_SPATIAL_GEOMETRY_NONE;
    uint32_t count = 0U;
    int rc = cursor_read_header(cursor, &little_endian, &type, error, function_name);

    if (rc == 0) {
        rc = spatial_buffer_append_byte(out_wkb, 1U);
    }
    if (rc == 0) {
        rc = spatial_buffer_append_u32_le(out_wkb, (uint32_t)type);
    }
    if (rc != 0) {
        return rc;
    }
    switch (type) {
    case MYLITE_SPATIAL_GEOMETRY_POINT: {
        double coordinate_x = 0.0;
        double coordinate_y = 0.0;

        rc = cursor_read_double(cursor, little_endian, &coordinate_x, error, function_name);
        if (rc == 0) {
            rc = cursor_read_double(cursor, little_endian, &coordinate_y, error, function_name);
        }
        if (rc == 0) {
            rc = spatial_buffer_append_double_le(out_wkb, coordinate_y);
        }
        if (rc == 0) {
            rc = spatial_buffer_append_double_le(out_wkb, coordinate_x);
        }
        return rc;
    }
    case MYLITE_SPATIAL_GEOMETRY_LINESTRING:
        rc = cursor_read_u32(cursor, little_endian, &count, error, function_name);
        if (rc == 0) {
            rc = spatial_buffer_append_u32_le(out_wkb, count);
        }
        for (uint32_t index = 0U; rc == 0 && index < count; ++index) {
            double coordinate_x = 0.0;
            double coordinate_y = 0.0;

            rc = cursor_read_double(cursor, little_endian, &coordinate_x, error, function_name);
            if (rc == 0) {
                rc = cursor_read_double(cursor, little_endian, &coordinate_y, error, function_name);
            }
            if (rc == 0) {
                rc = spatial_buffer_append_double_le(out_wkb, coordinate_y);
            }
            if (rc == 0) {
                rc = spatial_buffer_append_double_le(out_wkb, coordinate_x);
            }
        }
        return rc;
    case MYLITE_SPATIAL_GEOMETRY_POLYGON:
        rc = cursor_read_u32(cursor, little_endian, &count, error, function_name);
        if (rc == 0) {
            rc = spatial_buffer_append_u32_le(out_wkb, count);
        }
        for (uint32_t ring_index = 0U; rc == 0 && ring_index < count; ++ring_index) {
            uint32_t point_count = 0U;

            rc = cursor_read_u32(cursor, little_endian, &point_count, error, function_name);
            if (rc == 0) {
                rc = spatial_buffer_append_u32_le(out_wkb, point_count);
            }
            for (uint32_t point_index = 0U; rc == 0 && point_index < point_count; ++point_index) {
                double coordinate_x = 0.0;
                double coordinate_y = 0.0;

                rc = cursor_read_double(cursor, little_endian, &coordinate_x, error, function_name);
                if (rc == 0) {
                    rc = cursor_read_double(
                        cursor,
                        little_endian,
                        &coordinate_y,
                        error,
                        function_name
                    );
                }
                if (rc == 0) {
                    rc = spatial_buffer_append_double_le(out_wkb, coordinate_y);
                }
                if (rc == 0) {
                    rc = spatial_buffer_append_double_le(out_wkb, coordinate_x);
                }
            }
        }
        return rc;
    case MYLITE_SPATIAL_GEOMETRY_MULTIPOINT:
    case MYLITE_SPATIAL_GEOMETRY_MULTILINESTRING:
    case MYLITE_SPATIAL_GEOMETRY_MULTIPOLYGON:
    case MYLITE_SPATIAL_GEOMETRY_GEOMETRYCOLLECTION:
        rc = cursor_read_u32(cursor, little_endian, &count, error, function_name);
        if (rc == 0) {
            rc = spatial_buffer_append_u32_le(out_wkb, count);
        }
        for (uint32_t index = 0U; rc == 0 && index < count; ++index) {
            rc = wkb_swap_xy_at(cursor, out_wkb, error, function_name);
        }
        return rc;
    default:
        break;
    }
    return set_invalid_gis_data_error(error, function_name);
}

static int read_single_geometry_argument(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct spatial_geometry_view *out_geometry,
    bool *out_is_null,
    struct mylite_spatial_error *error
) {
    int rc = validate_argument_count(kind, argument_count, 1U, 1U, error);

    if (rc != 0) {
        return rc;
    }
    if (out_geometry == NULL || out_is_null == NULL) {
        return set_invalid_gis_data_error(error, mylite_spatial_function_name(kind));
    }
    *out_geometry = (struct spatial_geometry_view){0};
    *out_is_null = arguments[0].is_null;
    if (*out_is_null) {
        return 0;
    }
    rc = validate_internal_geometry(
        arguments[0].bytes,
        arguments[0].byte_count,
        &out_geometry->type,
        &out_geometry->srid,
        error,
        mylite_spatial_function_name(kind)
    );
    if (rc != 0) {
        return rc;
    }
    out_geometry->wkb = (const unsigned char *)arguments[0].bytes + spatial_internal_srid_size;
    out_geometry->wkb_size = arguments[0].byte_count - spatial_internal_srid_size;
    return 0;
}

static int read_two_geometry_arguments(
    enum mylite_spatial_function_kind kind,
    const struct mylite_spatial_argument *arguments,
    size_t argument_count,
    struct spatial_geometry_view *out_left,
    struct spatial_geometry_view *out_right,
    bool *out_is_null,
    struct mylite_spatial_error *error
) {
    bool left_is_null = false;
    bool right_is_null = false;
    int rc = validate_argument_count(kind, argument_count, 2U, 2U, error);

    if (rc != 0) {
        return rc;
    }
    rc = read_single_geometry_argument(kind, arguments, 1U, out_left, &left_is_null, error);
    if (rc == 0) {
        rc = read_single_geometry_argument(
            kind,
            arguments + 1U,
            1U,
            out_right,
            &right_is_null,
            error
        );
    }
    if (rc != 0) {
        return rc;
    }
    if (out_is_null != NULL) {
        *out_is_null = left_is_null || right_is_null;
    }
    return 0;
}

static bool geometry_type_is_empty_collection(
    const struct spatial_geometry_view *geometry,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    struct spatial_wkb_cursor cursor = {0};
    bool little_endian = false;
    enum mylite_spatial_geometry_type type = MYLITE_SPATIAL_GEOMETRY_NONE;
    uint32_t count = 0U;

    if (geometry == NULL || geometry->type != MYLITE_SPATIAL_GEOMETRY_GEOMETRYCOLLECTION) {
        return false;
    }
    cursor = (struct spatial_wkb_cursor){.bytes = geometry->wkb, .size = geometry->wkb_size};
    if (cursor_read_header(&cursor, &little_endian, &type, error, function_name) != 0 ||
        cursor_read_u32(&cursor, little_endian, &count, error, function_name) != 0) {
        return false;
    }
    return count == 0U;
}

static int wkb_dimension_at(
    struct spatial_wkb_cursor *cursor,
    int *out_dimension,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    bool little_endian = false;
    enum mylite_spatial_geometry_type type = MYLITE_SPATIAL_GEOMETRY_NONE;
    uint32_t count = 0U;
    int dimension = -1;
    int rc = cursor_read_header(cursor, &little_endian, &type, error, function_name);

    if (rc != 0) {
        return rc;
    }
    switch (type) {
    case MYLITE_SPATIAL_GEOMETRY_POINT:
        rc = skip_wkb_points(cursor, 1U, error, function_name);
        dimension = 0;
        break;
    case MYLITE_SPATIAL_GEOMETRY_LINESTRING:
        rc = cursor_read_u32(cursor, little_endian, &count, error, function_name);
        if (rc == 0) {
            rc = skip_wkb_points(cursor, count, error, function_name);
        }
        dimension = 1;
        break;
    case MYLITE_SPATIAL_GEOMETRY_POLYGON:
        rc = cursor_read_u32(cursor, little_endian, &count, error, function_name);
        for (uint32_t index = 0U; rc == 0 && index < count; ++index) {
            uint32_t point_count = 0U;

            rc = cursor_read_u32(cursor, little_endian, &point_count, error, function_name);
            if (rc == 0) {
                rc = skip_wkb_points(cursor, point_count, error, function_name);
            }
        }
        dimension = 2;
        break;
    case MYLITE_SPATIAL_GEOMETRY_MULTIPOINT:
    case MYLITE_SPATIAL_GEOMETRY_MULTILINESTRING:
    case MYLITE_SPATIAL_GEOMETRY_MULTIPOLYGON:
    case MYLITE_SPATIAL_GEOMETRY_GEOMETRYCOLLECTION:
        rc = cursor_read_u32(cursor, little_endian, &count, error, function_name);
        for (uint32_t index = 0U; rc == 0 && index < count; ++index) {
            int nested_dimension = -1;

            rc = wkb_dimension_at(cursor, &nested_dimension, error, function_name);
            if (rc == 0 && nested_dimension > dimension) {
                dimension = nested_dimension;
            }
        }
        break;
    default:
        rc = set_invalid_gis_data_error(error, function_name);
        break;
    }
    if (rc == 0 && out_dimension != NULL) {
        *out_dimension = dimension;
    }
    return rc;
}

static int wkb_bounds_at(
    struct spatial_wkb_cursor *cursor,
    struct spatial_box *box,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    bool little_endian = false;
    enum mylite_spatial_geometry_type type = MYLITE_SPATIAL_GEOMETRY_NONE;
    uint32_t count = 0U;
    int rc = cursor_read_header(cursor, &little_endian, &type, error, function_name);

    if (rc != 0) {
        return rc;
    }
    switch (type) {
    case MYLITE_SPATIAL_GEOMETRY_POINT:
        for (uint32_t index = 0U; rc == 0 && index < 1U; ++index) {
            double coordinate_x = 0.0;
            double coordinate_y = 0.0;

            rc = cursor_read_double(cursor, little_endian, &coordinate_x, error, function_name);
            if (rc == 0) {
                rc = cursor_read_double(cursor, little_endian, &coordinate_y, error, function_name);
            }
            if (rc == 0) {
                spatial_box_include_point(box, coordinate_x, coordinate_y);
            }
        }
        break;
    case MYLITE_SPATIAL_GEOMETRY_LINESTRING:
        rc = cursor_read_u32(cursor, little_endian, &count, error, function_name);
        for (uint32_t index = 0U; rc == 0 && index < count; ++index) {
            double coordinate_x = 0.0;
            double coordinate_y = 0.0;

            rc = cursor_read_double(cursor, little_endian, &coordinate_x, error, function_name);
            if (rc == 0) {
                rc = cursor_read_double(cursor, little_endian, &coordinate_y, error, function_name);
            }
            if (rc == 0) {
                spatial_box_include_point(box, coordinate_x, coordinate_y);
            }
        }
        break;
    case MYLITE_SPATIAL_GEOMETRY_POLYGON:
        rc = cursor_read_u32(cursor, little_endian, &count, error, function_name);
        for (uint32_t ring_index = 0U; rc == 0 && ring_index < count; ++ring_index) {
            uint32_t point_count = 0U;

            rc = cursor_read_u32(cursor, little_endian, &point_count, error, function_name);
            for (uint32_t point_index = 0U; rc == 0 && point_index < point_count; ++point_index) {
                double coordinate_x = 0.0;
                double coordinate_y = 0.0;

                rc = cursor_read_double(cursor, little_endian, &coordinate_x, error, function_name);
                if (rc == 0) {
                    rc = cursor_read_double(
                        cursor,
                        little_endian,
                        &coordinate_y,
                        error,
                        function_name
                    );
                }
                if (rc == 0) {
                    spatial_box_include_point(box, coordinate_x, coordinate_y);
                }
            }
        }
        break;
    case MYLITE_SPATIAL_GEOMETRY_MULTIPOINT:
    case MYLITE_SPATIAL_GEOMETRY_MULTILINESTRING:
    case MYLITE_SPATIAL_GEOMETRY_MULTIPOLYGON:
    case MYLITE_SPATIAL_GEOMETRY_GEOMETRYCOLLECTION:
        rc = cursor_read_u32(cursor, little_endian, &count, error, function_name);
        for (uint32_t index = 0U; rc == 0 && index < count; ++index) {
            rc = wkb_bounds_at(cursor, box, error, function_name);
        }
        break;
    default:
        rc = set_invalid_gis_data_error(error, function_name);
        break;
    }
    return rc;
}

static int line_point_from_wkb(
    const struct spatial_geometry_view *geometry,
    uint32_t point_index,
    struct spatial_point *out_point,
    bool *out_found,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    struct spatial_wkb_cursor cursor = {0};
    bool little_endian = false;
    enum mylite_spatial_geometry_type type = MYLITE_SPATIAL_GEOMETRY_NONE;
    uint32_t point_count = 0U;
    int rc = 0;

    if (out_found == NULL || out_point == NULL || geometry == NULL) {
        return set_invalid_gis_data_error(error, function_name);
    }
    *out_found = false;
    cursor = (struct spatial_wkb_cursor){.bytes = geometry->wkb, .size = geometry->wkb_size};
    rc = cursor_read_header(&cursor, &little_endian, &type, error, function_name);
    if (rc == 0) {
        rc = cursor_read_u32(&cursor, little_endian, &point_count, error, function_name);
    }
    if (rc != 0) {
        return rc;
    }
    if (point_index == UINT32_MAX) {
        point_index = point_count;
    }
    if (point_index == 0U || point_index > point_count) {
        return 0;
    }
    for (uint32_t index = 1U; rc == 0 && index <= point_count; ++index) {
        struct spatial_point point = {0};

        rc = cursor_read_double(&cursor, little_endian, &point.coordinate_x, error, function_name);
        if (rc == 0) {
            rc = cursor_read_double(
                &cursor,
                little_endian,
                &point.coordinate_y,
                error,
                function_name
            );
        }
        if (rc == 0 && index == point_index) {
            *out_point = point;
            *out_found = true;
            return 0;
        }
    }
    return rc;
}

static int line_points_from_wkb(
    const struct spatial_geometry_view *geometry,
    struct spatial_point **out_points,
    uint32_t *out_point_count,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    struct spatial_wkb_cursor cursor = {0};
    struct spatial_point *points = NULL;
    bool little_endian = false;
    enum mylite_spatial_geometry_type type = MYLITE_SPATIAL_GEOMETRY_NONE;
    uint32_t point_count = 0U;
    int rc = 0;

    if (geometry == NULL || out_points == NULL || out_point_count == NULL) {
        return set_invalid_gis_data_error(error, function_name);
    }
    *out_points = NULL;
    *out_point_count = 0U;
    cursor = (struct spatial_wkb_cursor){.bytes = geometry->wkb, .size = geometry->wkb_size};
    rc = cursor_read_header(&cursor, &little_endian, &type, error, function_name);
    if (rc == 0) {
        rc = cursor_read_u32(&cursor, little_endian, &point_count, error, function_name);
    }
    if (rc != 0) {
        return rc;
    }
    if (point_count == 0U) {
        return 0;
    }
    if ((size_t)point_count > SIZE_MAX / sizeof(*points)) {
        return set_nomem_error(error);
    }
    points = calloc((size_t)point_count, sizeof(*points));
    if (points == NULL) {
        return set_nomem_error(error);
    }
    for (uint32_t index = 0U; rc == 0 && index < point_count; ++index) {
        rc = cursor_read_double(
            &cursor,
            little_endian,
            &points[index].coordinate_x,
            error,
            function_name
        );
        if (rc == 0) {
            rc = cursor_read_double(
                &cursor,
                little_endian,
                &points[index].coordinate_y,
                error,
                function_name
            );
        }
    }
    if (rc != 0) {
        free(points);
        return rc;
    }
    *out_points = points;
    *out_point_count = point_count;
    return 0;
}

static double line_points_length(const struct spatial_point *points, uint32_t point_count) {
    double length = 0.0;

    if (points == NULL || point_count < 2U) {
        return 0.0;
    }
    for (uint32_t index = 0U; index + 1U < point_count; ++index) {
        length += distance_point_to_point(&points[index], &points[index + 1U]);
    }
    return length;
}

static struct spatial_point line_point_at_distance(
    const struct spatial_point *points,
    uint32_t point_count,
    double target_distance
) {
    double traversed = 0.0;

    if (points == NULL || point_count == 0U) {
        return (struct spatial_point){0};
    }
    if (target_distance <= 0.0 || point_count == 1U) {
        return points[0];
    }
    for (uint32_t index = 0U; index + 1U < point_count; ++index) {
        double segment_length = distance_point_to_point(&points[index], &points[index + 1U]);

        if (double_near_zero(segment_length)) {
            continue;
        }
        if (traversed + segment_length >= target_distance) {
            double ratio = (target_distance - traversed) / segment_length;

            if (ratio <= 0.0) {
                return points[index];
            }
            if (ratio >= 1.0) {
                return points[index + 1U];
            }
            return (struct spatial_point){
                .coordinate_x =
                    points[index].coordinate_x +
                    ((points[index + 1U].coordinate_x - points[index].coordinate_x) * ratio),
                .coordinate_y =
                    points[index].coordinate_y +
                    ((points[index + 1U].coordinate_y - points[index].coordinate_y) * ratio),
            };
        }
        traversed += segment_length;
    }
    return points[point_count - 1U];
}

static int interpolated_line_points(
    const struct spatial_point *line_points,
    uint32_t line_point_count, // NOLINT(bugprone-easily-swappable-parameters)
    double fraction,
    struct spatial_point **out_points,
    uint32_t *out_point_count,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    struct spatial_point *points = NULL;
    uint32_t point_count = 1U;
    double total_length = line_points_length(line_points, line_point_count);

    if (out_points == NULL || out_point_count == NULL) {
        return set_invalid_gis_data_error(error, function_name);
    }
    *out_points = NULL;
    *out_point_count = 0U;
    if (fraction > 0.0 && !double_near_zero(total_length)) {
        double raw_point_count = floor((1.0 + spatial_distance_epsilon) / fraction);

        if (!isfinite(raw_point_count) || raw_point_count < 1.0 ||
            raw_point_count > (double)UINT32_MAX) {
            return set_distance_range_error(error, function_name);
        }
        point_count = (uint32_t)raw_point_count;
    }
    if ((size_t)point_count > SIZE_MAX / sizeof(*points)) {
        return set_nomem_error(error);
    }
    points = calloc((size_t)point_count, sizeof(*points));
    if (points == NULL) {
        return set_nomem_error(error);
    }
    if (fraction == 0.0 || double_near_zero(total_length)) {
        points[0] = line_point_at_distance(line_points, line_point_count, 0.0);
    } else {
        for (uint32_t index = 0U; index < point_count; ++index) {
            double current_fraction = fraction * (double)(index + 1U);

            if (current_fraction > 1.0 && current_fraction <= 1.0 + spatial_distance_epsilon) {
                current_fraction = 1.0;
            }
            points[index] = line_point_at_distance(
                line_points,
                line_point_count,
                total_length * current_fraction
            );
        }
    }
    *out_points = points;
    *out_point_count = point_count;
    return 0;
}

static int polygon_ring_from_wkb(
    const struct spatial_geometry_view *geometry,
    uint32_t ring_index,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    bool *out_found,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    struct spatial_wkb_cursor cursor = {0};
    bool little_endian = false;
    enum mylite_spatial_geometry_type type = MYLITE_SPATIAL_GEOMETRY_NONE;
    uint32_t ring_count = 0U;
    int rc = 0;

    if (geometry == NULL || out_bytes == NULL || out_byte_count == NULL || out_found == NULL) {
        return set_invalid_gis_data_error(error, function_name);
    }
    *out_bytes = NULL;
    *out_byte_count = 0U;
    *out_found = false;
    cursor = (struct spatial_wkb_cursor){.bytes = geometry->wkb, .size = geometry->wkb_size};
    rc = cursor_read_header(&cursor, &little_endian, &type, error, function_name);
    if (rc == 0) {
        rc = cursor_read_u32(&cursor, little_endian, &ring_count, error, function_name);
    }
    for (uint32_t current_ring = 0U; rc == 0 && current_ring < ring_count; ++current_ring) {
        uint32_t point_count = 0U;

        rc = cursor_read_u32(&cursor, little_endian, &point_count, error, function_name);
        if (rc != 0) {
            break;
        }
        if (current_ring == ring_index) {
            struct spatial_point *points = (struct spatial_point *)
                calloc(point_count == 0U ? 1U : point_count, sizeof(*points));

            if (points == NULL) {
                return set_nomem_error(error);
            }
            for (uint32_t point_index = 0U; rc == 0 && point_index < point_count; ++point_index) {
                rc = cursor_read_double(
                    &cursor,
                    little_endian,
                    &points[point_index].coordinate_x,
                    error,
                    function_name
                );
                if (rc == 0) {
                    rc = cursor_read_double(
                        &cursor,
                        little_endian,
                        &points[point_index].coordinate_y,
                        error,
                        function_name
                    );
                }
            }
            if (rc == 0) {
                rc = make_linestring_internal_geometry(
                    points,
                    point_count,
                    out_bytes,
                    out_byte_count,
                    error
                );
            }
            free(points);
            if (rc == 0) {
                *out_found = true;
            }
            return rc;
        }
        rc = skip_wkb_points(&cursor, point_count, error, function_name);
    }
    return rc;
}

static void spatial_box_include_point(
    struct spatial_box *box,
    double coordinate_x,
    double coordinate_y
) {
    if (box == NULL) {
        return;
    }
    if (!box->has_value) {
        *box = (struct spatial_box){
            .min_x = coordinate_x,
            .min_y = coordinate_y,
            .max_x = coordinate_x,
            .max_y = coordinate_y,
            .has_value = true,
        };
        return;
    }
    if (coordinate_x < box->min_x) {
        box->min_x = coordinate_x;
    }
    if (coordinate_x > box->max_x) {
        box->max_x = coordinate_x;
    }
    if (coordinate_y < box->min_y) {
        box->min_y = coordinate_y;
    }
    if (coordinate_y > box->max_y) {
        box->max_y = coordinate_y;
    }
}

static bool spatial_box_equals(const struct spatial_box *left, const struct spatial_box *right) {
    return left != NULL && right != NULL && left->has_value && right->has_value &&
           left->min_x == right->min_x && left->min_y == right->min_y &&
           left->max_x == right->max_x && left->max_y == right->max_y;
}

static bool spatial_box_covers(const struct spatial_box *left, const struct spatial_box *right) {
    return left != NULL && right != NULL && left->has_value && right->has_value &&
           left->min_x <= right->min_x && left->min_y <= right->min_y &&
           left->max_x >= right->max_x && left->max_y >= right->max_y;
}

static bool spatial_box_intersects(
    const struct spatial_box *left,
    const struct spatial_box *right
) {
    return left != NULL && right != NULL && left->has_value && right->has_value &&
           left->min_x <= right->max_x && left->max_x >= right->min_x &&
           left->min_y <= right->max_y && left->max_y >= right->min_y;
}

static bool spatial_box_interiors_intersect(
    const struct spatial_box *left,
    const struct spatial_box *right
) {
    double min_x = 0.0;
    double min_y = 0.0;
    double max_x = 0.0;
    double max_y = 0.0;

    if (left == NULL || right == NULL || !left->has_value || !right->has_value) {
        return false;
    }
    min_x = left->min_x > right->min_x ? left->min_x : right->min_x;
    min_y = left->min_y > right->min_y ? left->min_y : right->min_y;
    max_x = left->max_x < right->max_x ? left->max_x : right->max_x;
    max_y = left->max_y < right->max_y ? left->max_y : right->max_y;
    return min_x < max_x && min_y < max_y;
}

static int geohash_encode(
    double longitude, // NOLINT(bugprone-easily-swappable-parameters): geohash inputs.
    double latitude,
    uint32_t max_length,
    unsigned char **out_text,
    size_t *out_text_length,
    struct mylite_spatial_error *error
) {
    double longitude_min = spatial_geohash_longitude_min;
    double longitude_max = spatial_geohash_longitude_max;
    double latitude_min = spatial_geohash_latitude_min;
    double latitude_max = spatial_geohash_latitude_max;
    bool use_longitude = true;
    unsigned char *text = NULL;

    if (out_text == NULL || out_text_length == NULL || max_length == 0U) {
        return set_nomem_error(error);
    }
    text = (unsigned char *)malloc((size_t)max_length);
    if (text == NULL) {
        return set_nomem_error(error);
    }
    for (uint32_t index = 0U; index < max_length; ++index) {
        int value = 0;

        for (int bit = spatial_geohash_high_bit; bit > 0; bit >>= 1U) {
            if (use_longitude) {
                double midpoint =
                    (longitude_min + longitude_max) / spatial_geohash_interval_midpoint_divisor;

                if (longitude >= midpoint) {
                    value |= bit;
                    longitude_min = midpoint;
                } else {
                    longitude_max = midpoint;
                }
            } else {
                double midpoint =
                    (latitude_min + latitude_max) / spatial_geohash_interval_midpoint_divisor;

                if (latitude >= midpoint) {
                    value |= bit;
                    latitude_min = midpoint;
                } else {
                    latitude_max = midpoint;
                }
            }
            use_longitude = !use_longitude;
        }
        text[index] = (unsigned char)spatial_geohash_alphabet[value];
    }
    *out_text = text;
    *out_text_length = max_length;
    return 0;
}

static int geohash_decode(
    const struct mylite_spatial_argument *argument,
    double *out_longitude,
    double *out_latitude,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    const unsigned char *text = NULL;
    size_t text_length = 0U;
    double longitude_min = spatial_geohash_longitude_min;
    double longitude_max = spatial_geohash_longitude_max;
    double latitude_min = spatial_geohash_latitude_min;
    double latitude_max = spatial_geohash_latitude_max;
    bool use_longitude = true;

    if (argument == NULL || out_longitude == NULL || out_latitude == NULL) {
        return set_invalid_gis_data_error(error, function_name);
    }
    text = (const unsigned char *)argument->bytes;
    text_length = argument->byte_count;
    if (text == NULL || text_length == 0U) {
        return set_invalid_geohash_error(error, argument, function_name);
    }
    if (text_length > spatial_geohash_decode_max_length) {
        text_length = spatial_geohash_decode_max_length;
    }
    for (size_t index = 0U; index < text_length; ++index) {
        int value = geohash_character_value(text[index]);

        if (value < 0) {
            return set_invalid_geohash_error(error, argument, function_name);
        }
        for (int bit = spatial_geohash_high_bit; bit > 0; bit >>= 1U) {
            if (use_longitude) {
                double midpoint =
                    (longitude_min + longitude_max) / spatial_geohash_interval_midpoint_divisor;

                if ((value & bit) != 0) {
                    longitude_min = midpoint;
                } else {
                    longitude_max = midpoint;
                }
            } else {
                double midpoint =
                    (latitude_min + latitude_max) / spatial_geohash_interval_midpoint_divisor;

                if ((value & bit) != 0) {
                    latitude_min = midpoint;
                } else {
                    latitude_max = midpoint;
                }
            }
            use_longitude = !use_longitude;
        }
    }
    *out_longitude = geohash_round_coordinate(
        (longitude_min + longitude_max) / spatial_geohash_interval_midpoint_divisor,
        longitude_min,
        longitude_max
    );
    *out_latitude = geohash_round_coordinate(
        (latitude_min + latitude_max) / spatial_geohash_interval_midpoint_divisor,
        latitude_min,
        latitude_max
    );
    return 0;
}

static double geohash_round_coordinate(
    double value, // NOLINT(bugprone-easily-swappable-parameters): interval rounding.
    double minimum,
    double maximum
) {
    double width = maximum - minimum;
    double nearest_integer = nearbyint(value);
    int decimal_count = 0;
    double scale = 1.0;

    if (width <= spatial_geohash_integer_snap_width && nearest_integer >= minimum &&
        nearest_integer <= maximum) {
        return nearest_integer;
    }
    if (width > 0.0 && width < 1.0) {
        decimal_count = (int)ceil(-log10(width));
        if (decimal_count > spatial_geohash_max_round_decimals) {
            decimal_count = spatial_geohash_max_round_decimals;
        }
    }
    for (int index = 0; index < decimal_count; ++index) {
        scale *= spatial_decimal_base;
    }
    return nearbyint(value * scale) / scale;
}

static int geohash_character_value(unsigned char byte) {
    unsigned char lower = (unsigned char)tolower(byte);

    for (int index = 0; index < (int)(sizeof(spatial_geohash_alphabet) - 1U); ++index) {
        if ((unsigned char)spatial_geohash_alphabet[index] == lower) {
            return index;
        }
    }
    return -1;
}

static int append_internal_prefix(struct spatial_buffer *buffer, uint32_t srid) {
    return spatial_buffer_append_u32_le(buffer, srid);
}

static int spatial_buffer_append(struct spatial_buffer *buffer, const void *bytes, size_t size) {
    if (size == 0U) {
        return 0;
    }
    if (buffer == NULL || bytes == NULL || size > SIZE_MAX - buffer->size ||
        spatial_buffer_reserve(buffer, buffer->size + size) != 0) {
        return -1;
    }
    memcpy(buffer->bytes + buffer->size, bytes, size);
    buffer->size += size;
    return 0;
}

static int spatial_buffer_append_byte(struct spatial_buffer *buffer, unsigned char byte) {
    return spatial_buffer_append(buffer, &byte, 1U);
}

static int spatial_buffer_append_u32_le(struct spatial_buffer *buffer, uint32_t value) {
    unsigned char bytes[sizeof(uint32_t)];

    bytes[0] = (unsigned char)(value & spatial_u32_byte_mask);
    bytes[1] = (unsigned char)((value >> spatial_u32_second_byte_shift) & spatial_u32_byte_mask);
    bytes[2] = (unsigned char)((value >> spatial_u32_third_byte_shift) & spatial_u32_byte_mask);
    bytes[3] = (unsigned char)((value >> spatial_u32_fourth_byte_shift) & spatial_u32_byte_mask);
    return spatial_buffer_append(buffer, bytes, sizeof(bytes));
}

static int spatial_buffer_append_double_le(struct spatial_buffer *buffer, double value) {
    uint64_t bits = 0U;
    unsigned char bytes[sizeof(uint64_t)];

    memcpy(&bits, &value, sizeof(bits));
    for (size_t index = 0U; index < sizeof(bytes); ++index) {
        bytes[index] =
            (unsigned char)((bits >> (spatial_byte_bit_count * index)) & spatial_u32_byte_mask);
    }
    return spatial_buffer_append(buffer, bytes, sizeof(bytes));
}

static int spatial_buffer_reserve(struct spatial_buffer *buffer, size_t required) {
    size_t capacity = 0U;
    unsigned char *bytes = NULL;

    if (buffer == NULL) {
        return -1;
    }
    if (required <= buffer->capacity) {
        return 0;
    }
    capacity = buffer->capacity == 0U ? spatial_buffer_initial_capacity : buffer->capacity;
    while (capacity < required) {
        if (capacity > SIZE_MAX / 2U) {
            return -1;
        }
        capacity *= 2U;
    }
    bytes = (unsigned char *)realloc(buffer->bytes, capacity);
    if (bytes == NULL) {
        return -1;
    }
    buffer->bytes = bytes;
    buffer->capacity = capacity;
    return 0;
}

static void spatial_buffer_deinit(struct spatial_buffer *buffer) {
    if (buffer == NULL) {
        return;
    }
    free(buffer->bytes);
    *buffer = (struct spatial_buffer){0};
}

static uint32_t read_u32_endian(const unsigned char *bytes, bool little_endian) {
    if (little_endian) {
        return ((uint32_t)bytes[0]) | ((uint32_t)bytes[1] << spatial_u32_second_byte_shift) |
               ((uint32_t)bytes[2] << spatial_u32_third_byte_shift) |
               ((uint32_t)bytes[3] << spatial_u32_fourth_byte_shift);
    }
    return ((uint32_t)bytes[3]) | ((uint32_t)bytes[2] << spatial_u32_second_byte_shift) |
           ((uint32_t)bytes[1] << spatial_u32_third_byte_shift) |
           ((uint32_t)bytes[0] << spatial_u32_fourth_byte_shift);
}

static double read_double_endian(const unsigned char *bytes, bool little_endian) {
    uint64_t bits = 0U;
    double value = 0.0;

    for (size_t index = 0U; index < sizeof(bits); ++index) {
        size_t source_index = little_endian ? index : sizeof(bits) - index - 1U;

        bits |= ((uint64_t)bytes[source_index]) << (spatial_byte_bit_count * index);
    }
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static int validate_internal_geometry(
    const void *bytes,
    size_t byte_count,
    enum mylite_spatial_geometry_type *out_type,
    uint32_t *out_srid,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    const unsigned char *geometry = (const unsigned char *)bytes;
    enum mylite_spatial_geometry_type type = MYLITE_SPATIAL_GEOMETRY_NONE;
    uint32_t srid = 0U;

    if (geometry == NULL || byte_count <= spatial_internal_srid_size) {
        return set_invalid_gis_data_error(error, function_name);
    }
    srid = read_u32_endian(geometry, true);
    if (validate_wkb(
            geometry + spatial_internal_srid_size,
            byte_count - spatial_internal_srid_size,
            &type,
            error,
            function_name
        ) != 0) {
        return -1;
    }
    if (out_type != NULL) {
        *out_type = type;
    }
    if (out_srid != NULL) {
        *out_srid = srid;
    }
    return 0;
}

static int validate_wkb(
    const unsigned char *wkb,
    size_t wkb_size,
    enum mylite_spatial_geometry_type *out_type,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    struct spatial_wkb_cursor cursor = {.bytes = wkb, .size = wkb_size, .offset = 0U};
    enum mylite_spatial_geometry_type type = MYLITE_SPATIAL_GEOMETRY_NONE;

    if (wkb == NULL || wkb_size < spatial_wkb_header_size) {
        return set_invalid_gis_data_error(error, function_name);
    }
    if (validate_wkb_at(&cursor, &type, error, function_name) != 0 || cursor.offset != wkb_size) {
        if (error != NULL && error->code != 0) {
            return -1;
        }
        return set_invalid_gis_data_error(error, function_name);
    }
    if (out_type != NULL) {
        *out_type = type;
    }
    return 0;
}

static int validate_wkb_at(
    struct spatial_wkb_cursor *cursor,
    enum mylite_spatial_geometry_type *out_type,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    bool little_endian = false;
    enum mylite_spatial_geometry_type type = MYLITE_SPATIAL_GEOMETRY_NONE;
    uint32_t count = 0U;
    int rc = cursor_read_header(cursor, &little_endian, &type, error, function_name);

    if (rc != 0) {
        return rc;
    }
    switch (type) {
    case MYLITE_SPATIAL_GEOMETRY_POINT:
        rc = skip_wkb_points(cursor, 1U, error, function_name);
        break;
    case MYLITE_SPATIAL_GEOMETRY_LINESTRING:
        rc = cursor_read_u32(cursor, little_endian, &count, error, function_name);
        if (rc == 0 && count < 2U) {
            rc = set_invalid_gis_data_error(error, function_name);
        }
        if (rc == 0) {
            rc = skip_wkb_points(cursor, count, error, function_name);
        }
        break;
    case MYLITE_SPATIAL_GEOMETRY_POLYGON:
        rc = cursor_read_u32(cursor, little_endian, &count, error, function_name);
        for (uint32_t index = 0U; rc == 0 && index < count; ++index) {
            uint32_t point_count = 0U;

            rc = cursor_read_u32(cursor, little_endian, &point_count, error, function_name);
            if (rc == 0) {
                rc = skip_wkb_points(cursor, point_count, error, function_name);
            }
        }
        break;
    case MYLITE_SPATIAL_GEOMETRY_MULTIPOINT:
    case MYLITE_SPATIAL_GEOMETRY_MULTILINESTRING:
    case MYLITE_SPATIAL_GEOMETRY_MULTIPOLYGON:
    case MYLITE_SPATIAL_GEOMETRY_GEOMETRYCOLLECTION:
        rc = cursor_read_u32(cursor, little_endian, &count, error, function_name);
        for (uint32_t index = 0U; rc == 0 && index < count; ++index) {
            enum mylite_spatial_geometry_type nested_type = MYLITE_SPATIAL_GEOMETRY_NONE;
            enum mylite_spatial_geometry_type expected_nested_type =
                collection_expected_nested_type(type);

            rc = validate_wkb_at(cursor, &nested_type, error, function_name);
            if (rc == 0 && expected_nested_type != MYLITE_SPATIAL_GEOMETRY_NONE &&
                nested_type != expected_nested_type) {
                rc = set_invalid_gis_data_error(error, function_name);
            }
        }
        break;
    default:
        rc = set_invalid_gis_data_error(error, function_name);
        break;
    }
    if (rc == 0 && out_type != NULL) {
        *out_type = type;
    }
    return rc;
}

static enum mylite_spatial_geometry_type collection_expected_nested_type(
    enum mylite_spatial_geometry_type type
) {
    switch (type) {
    case MYLITE_SPATIAL_GEOMETRY_MULTIPOINT:
        return MYLITE_SPATIAL_GEOMETRY_POINT;
    case MYLITE_SPATIAL_GEOMETRY_MULTILINESTRING:
        return MYLITE_SPATIAL_GEOMETRY_LINESTRING;
    case MYLITE_SPATIAL_GEOMETRY_MULTIPOLYGON:
        return MYLITE_SPATIAL_GEOMETRY_POLYGON;
    default:
        break;
    }
    return MYLITE_SPATIAL_GEOMETRY_NONE;
}

static int skip_wkb_points(
    struct spatial_wkb_cursor *cursor,
    uint32_t point_count,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    if ((size_t)point_count > (SIZE_MAX / spatial_coordinate_size)) {
        return set_invalid_gis_data_error(error, function_name);
    }
    return cursor_skip(cursor, (size_t)point_count * spatial_coordinate_size, error, function_name);
}

static int cursor_read_header(
    struct spatial_wkb_cursor *cursor,
    bool *out_little_endian,
    enum mylite_spatial_geometry_type *out_type,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    unsigned char order = 0U;
    uint32_t type = 0U;

    if (cursor == NULL || cursor->bytes == NULL || out_little_endian == NULL || out_type == NULL ||
        cursor->offset > cursor->size || spatial_wkb_header_size > cursor->size - cursor->offset) {
        return set_invalid_gis_data_error(error, function_name);
    }
    order = cursor->bytes[cursor->offset];
    if (order != 0U && order != 1U) {
        return set_invalid_gis_data_error(error, function_name);
    }
    *out_little_endian = order == 1U;
    type = read_u32_endian(cursor->bytes + cursor->offset + 1U, *out_little_endian);
    if (type < (uint32_t)MYLITE_SPATIAL_GEOMETRY_POINT ||
        type > (uint32_t)MYLITE_SPATIAL_GEOMETRY_GEOMETRYCOLLECTION) {
        return set_invalid_gis_data_error(error, function_name);
    }
    cursor->offset += spatial_wkb_header_size;
    *out_type = (enum mylite_spatial_geometry_type)type;
    return 0;
}

static int cursor_read_u32(
    struct spatial_wkb_cursor *cursor,
    bool little_endian,
    uint32_t *out_value,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    if (cursor == NULL || cursor->bytes == NULL || out_value == NULL ||
        cursor->offset > cursor->size || sizeof(uint32_t) > cursor->size - cursor->offset) {
        return set_invalid_gis_data_error(error, function_name);
    }
    *out_value = read_u32_endian(cursor->bytes + cursor->offset, little_endian);
    cursor->offset += sizeof(uint32_t);
    return 0;
}

static int cursor_read_double(
    struct spatial_wkb_cursor *cursor,
    bool little_endian,
    double *out_value,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    if (cursor == NULL || cursor->bytes == NULL || out_value == NULL ||
        cursor->offset > cursor->size || sizeof(double) > cursor->size - cursor->offset) {
        return set_invalid_gis_data_error(error, function_name);
    }
    *out_value = read_double_endian(cursor->bytes + cursor->offset, little_endian);
    cursor->offset += sizeof(double);
    return 0;
}

static int cursor_skip(
    struct spatial_wkb_cursor *cursor,
    size_t size,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    if (cursor == NULL || cursor->offset > cursor->size || size > cursor->size - cursor->offset) {
        return set_invalid_gis_data_error(error, function_name);
    }
    cursor->offset += size;
    return 0;
}

static const char *geometry_type_name(enum mylite_spatial_geometry_type type) {
    switch (type) {
    case MYLITE_SPATIAL_GEOMETRY_POINT:
        return "POINT";
    case MYLITE_SPATIAL_GEOMETRY_LINESTRING:
        return "LINESTRING";
    case MYLITE_SPATIAL_GEOMETRY_POLYGON:
        return "POLYGON";
    case MYLITE_SPATIAL_GEOMETRY_MULTIPOINT:
        return "MULTIPOINT";
    case MYLITE_SPATIAL_GEOMETRY_MULTILINESTRING:
        return "MULTILINESTRING";
    case MYLITE_SPATIAL_GEOMETRY_MULTIPOLYGON:
        return "MULTIPOLYGON";
    case MYLITE_SPATIAL_GEOMETRY_GEOMETRYCOLLECTION:
        return "GEOMETRYCOLLECTION";
    default:
        break;
    }
    return "UNKNOWN";
}

static const char *cartesian_srs_not_implemented_geometry_type_name(
    enum mylite_spatial_geometry_type type
) {
    if (type == MYLITE_SPATIAL_GEOMETRY_GEOMETRYCOLLECTION) {
        return "GEOMCOLLECTION";
    }
    return geometry_type_name(type);
}

static int geometry_point_coordinates(
    const unsigned char *wkb,
    size_t wkb_size,
    double *out_x,
    double *out_y,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    struct spatial_wkb_cursor cursor = {.bytes = wkb, .size = wkb_size, .offset = 0U};
    bool little_endian = false;
    enum mylite_spatial_geometry_type type = MYLITE_SPATIAL_GEOMETRY_NONE;
    int rc = cursor_read_header(&cursor, &little_endian, &type, error, function_name);

    if (rc != 0 || type != MYLITE_SPATIAL_GEOMETRY_POINT) {
        return rc != 0 ? rc : set_invalid_gis_data_error(error, function_name);
    }
    rc = cursor_read_double(&cursor, little_endian, out_x, error, function_name);
    if (rc == 0) {
        rc = cursor_read_double(&cursor, little_endian, out_y, error, function_name);
    }
    return rc;
}

static int append_wkb_as_wkt(
    struct spatial_buffer *buffer,
    struct spatial_wkb_cursor *cursor,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    bool little_endian = false;
    enum mylite_spatial_geometry_type type = MYLITE_SPATIAL_GEOMETRY_NONE;
    int rc = cursor_read_header(cursor, &little_endian, &type, error, function_name);

    if (rc != 0) {
        return rc;
    }
    switch (type) {
    case MYLITE_SPATIAL_GEOMETRY_POINT:
        rc = append_cstring(buffer, "POINT(");
        if (rc == 0) {
            rc = append_wkb_point_body_as_wkt(buffer, cursor, little_endian, error, function_name);
        }
        if (rc == 0) {
            rc = spatial_buffer_append_byte(buffer, ')');
        }
        return rc;
    case MYLITE_SPATIAL_GEOMETRY_LINESTRING:
        rc = append_cstring(buffer, "LINESTRING(");
        if (rc == 0) {
            rc = append_wkb_line_body_as_wkt(buffer, cursor, little_endian, error, function_name);
        }
        if (rc == 0) {
            rc = spatial_buffer_append_byte(buffer, ')');
        }
        return rc;
    case MYLITE_SPATIAL_GEOMETRY_POLYGON:
        rc = append_cstring(buffer, "POLYGON(");
        if (rc == 0) {
            rc =
                append_wkb_polygon_body_as_wkt(buffer, cursor, little_endian, error, function_name);
        }
        if (rc == 0) {
            rc = spatial_buffer_append_byte(buffer, ')');
        }
        return rc;
    case MYLITE_SPATIAL_GEOMETRY_MULTIPOINT:
        rc = append_cstring(buffer, "MULTIPOINT(");
        break;
    case MYLITE_SPATIAL_GEOMETRY_MULTILINESTRING:
        rc = append_cstring(buffer, "MULTILINESTRING(");
        break;
    case MYLITE_SPATIAL_GEOMETRY_MULTIPOLYGON:
        rc = append_cstring(buffer, "MULTIPOLYGON(");
        break;
    case MYLITE_SPATIAL_GEOMETRY_GEOMETRYCOLLECTION:
        return append_wkb_collection_body_as_wkt(
            buffer,
            cursor,
            little_endian,
            type,
            error,
            function_name
        );
    default:
        return set_invalid_gis_data_error(error, function_name);
    }
    if (rc == 0) {
        rc = append_wkb_collection_body_as_wkt(
            buffer,
            cursor,
            little_endian,
            type,
            error,
            function_name
        );
    }
    if (rc == 0) {
        rc = spatial_buffer_append_byte(buffer, ')');
    }
    return rc;
}

static int append_wkb_point_body_as_wkt(
    struct spatial_buffer *buffer,
    struct spatial_wkb_cursor *cursor,
    bool little_endian,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    return append_point_coordinates_as_wkt(buffer, cursor, little_endian, error, function_name);
}

static int append_wkb_line_body_as_wkt(
    struct spatial_buffer *buffer,
    struct spatial_wkb_cursor *cursor,
    bool little_endian,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    uint32_t point_count = 0U;
    int rc = cursor_read_u32(cursor, little_endian, &point_count, error, function_name);

    for (uint32_t index = 0U; rc == 0 && index < point_count; ++index) {
        if (index != 0U) {
            rc = spatial_buffer_append_byte(buffer, ',');
        }
        if (rc == 0) {
            rc = append_point_coordinates_as_wkt(
                buffer,
                cursor,
                little_endian,
                error,
                function_name
            );
        }
    }
    return rc;
}

static int append_wkb_polygon_body_as_wkt(
    struct spatial_buffer *buffer,
    struct spatial_wkb_cursor *cursor,
    bool little_endian,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    uint32_t ring_count = 0U;
    int rc = cursor_read_u32(cursor, little_endian, &ring_count, error, function_name);

    for (uint32_t ring_index = 0U; rc == 0 && ring_index < ring_count; ++ring_index) {
        if (ring_index != 0U) {
            rc = spatial_buffer_append_byte(buffer, ',');
        }
        if (rc == 0) {
            rc = spatial_buffer_append_byte(buffer, '(');
        }
        if (rc == 0) {
            rc = append_wkb_line_body_as_wkt(buffer, cursor, little_endian, error, function_name);
        }
        if (rc == 0) {
            rc = spatial_buffer_append_byte(buffer, ')');
        }
    }
    return rc;
}

static int append_wkb_collection_body_as_wkt(
    struct spatial_buffer *buffer,
    struct spatial_wkb_cursor *cursor,
    bool little_endian,
    enum mylite_spatial_geometry_type type,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    uint32_t count = 0U;
    int rc = cursor_read_u32(cursor, little_endian, &count, error, function_name);

    if (rc != 0) {
        return rc;
    }
    if (type == MYLITE_SPATIAL_GEOMETRY_GEOMETRYCOLLECTION) {
        if (count == 0U) {
            return append_cstring(buffer, "GEOMETRYCOLLECTION EMPTY");
        }
        rc = append_cstring(buffer, "GEOMETRYCOLLECTION(");
    }
    for (uint32_t index = 0U; rc == 0 && index < count; ++index) {
        if (index != 0U) {
            rc = spatial_buffer_append_byte(buffer, ',');
        }
        if (rc != 0) {
            break;
        }
        if (type == MYLITE_SPATIAL_GEOMETRY_MULTIPOINT) {
            bool nested_little = false;
            enum mylite_spatial_geometry_type nested_type = MYLITE_SPATIAL_GEOMETRY_NONE;

            rc = cursor_read_header(cursor, &nested_little, &nested_type, error, function_name);
            if (rc == 0 && nested_type != MYLITE_SPATIAL_GEOMETRY_POINT) {
                rc = set_invalid_gis_data_error(error, function_name);
            }
            if (rc == 0) {
                rc = spatial_buffer_append_byte(buffer, '(');
            }
            if (rc == 0) {
                rc = append_wkb_point_body_as_wkt(
                    buffer,
                    cursor,
                    nested_little,
                    error,
                    function_name
                );
            }
            if (rc == 0) {
                rc = spatial_buffer_append_byte(buffer, ')');
            }
        } else if (type == MYLITE_SPATIAL_GEOMETRY_MULTILINESTRING) {
            bool nested_little = false;
            enum mylite_spatial_geometry_type nested_type = MYLITE_SPATIAL_GEOMETRY_NONE;

            rc = cursor_read_header(cursor, &nested_little, &nested_type, error, function_name);
            if (rc == 0 && nested_type != MYLITE_SPATIAL_GEOMETRY_LINESTRING) {
                rc = set_invalid_gis_data_error(error, function_name);
            }
            if (rc == 0) {
                rc = spatial_buffer_append_byte(buffer, '(');
            }
            if (rc == 0) {
                rc = append_wkb_line_body_as_wkt(
                    buffer,
                    cursor,
                    nested_little,
                    error,
                    function_name
                );
            }
            if (rc == 0) {
                rc = spatial_buffer_append_byte(buffer, ')');
            }
        } else if (type == MYLITE_SPATIAL_GEOMETRY_MULTIPOLYGON) {
            bool nested_little = false;
            enum mylite_spatial_geometry_type nested_type = MYLITE_SPATIAL_GEOMETRY_NONE;

            rc = cursor_read_header(cursor, &nested_little, &nested_type, error, function_name);
            if (rc == 0 && nested_type != MYLITE_SPATIAL_GEOMETRY_POLYGON) {
                rc = set_invalid_gis_data_error(error, function_name);
            }
            if (rc == 0) {
                rc = spatial_buffer_append_byte(buffer, '(');
            }
            if (rc == 0) {
                rc = append_wkb_polygon_body_as_wkt(
                    buffer,
                    cursor,
                    nested_little,
                    error,
                    function_name
                );
            }
            if (rc == 0) {
                rc = spatial_buffer_append_byte(buffer, ')');
            }
        } else {
            rc = append_wkb_as_wkt(buffer, cursor, error, function_name);
        }
    }
    if (rc == 0 && type == MYLITE_SPATIAL_GEOMETRY_GEOMETRYCOLLECTION) {
        rc = spatial_buffer_append_byte(buffer, ')');
    }
    return rc;
}

static int append_point_coordinates_as_wkt(
    struct spatial_buffer *buffer,
    struct spatial_wkb_cursor *cursor,
    bool little_endian,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    double coordinate_x = 0.0;
    double coordinate_y = 0.0;
    int rc = cursor_read_double(cursor, little_endian, &coordinate_x, error, function_name);

    if (rc == 0) {
        rc = cursor_read_double(cursor, little_endian, &coordinate_y, error, function_name);
    }
    if (rc == 0) {
        rc = append_double_as_text(buffer, coordinate_x);
    }
    if (rc == 0) {
        rc = spatial_buffer_append_byte(buffer, ' ');
    }
    if (rc == 0) {
        rc = append_double_as_text(buffer, coordinate_y);
    }
    return rc;
}

static int append_double_as_text(struct spatial_buffer *buffer, double value) {
    char text[spatial_double_text_capacity];
    int written = 0;

    if (value == 0.0) {
        value = 0.0;
    }
    written = snprintf(text, sizeof(text), "%.15g", value);
    if (written < 0 || (size_t)written >= sizeof(text)) {
        return -1;
    }
    return spatial_buffer_append(buffer, text, (size_t)written);
}

static int append_geojson_geometry(
    struct spatial_buffer *buffer,
    struct spatial_wkb_cursor *cursor,
    uint32_t srid,
    uint32_t max_dec_digits,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    bool little_endian = false;
    enum mylite_spatial_geometry_type type = MYLITE_SPATIAL_GEOMETRY_NONE;
    uint32_t count = 0U;
    int rc = cursor_read_header(cursor, &little_endian, &type, error, function_name);

    if (rc != 0) {
        return rc;
    }
    if (type == MYLITE_SPATIAL_GEOMETRY_GEOMETRYCOLLECTION) {
        rc = append_cstring(buffer, "\"type\": \"GeometryCollection\", \"geometries\": [");
        if (rc == 0) {
            rc = cursor_read_u32(cursor, little_endian, &count, error, function_name);
        }
        for (uint32_t index = 0U; rc == 0 && index < count; ++index) {
            if (index > 0U) {
                rc = append_cstring(buffer, ", ");
            }
            if (rc == 0) {
                rc = spatial_buffer_append_byte(buffer, '{');
            }
            if (rc == 0) {
                rc = append_geojson_geometry(
                    buffer,
                    cursor,
                    srid,
                    max_dec_digits,
                    error,
                    function_name
                );
            }
            if (rc == 0) {
                rc = spatial_buffer_append_byte(buffer, '}');
            }
        }
        if (rc == 0) {
            rc = spatial_buffer_append_byte(buffer, ']');
        }
        return rc;
    }

    rc = append_cstring(buffer, "\"type\": \"");
    if (rc == 0) {
        rc = append_cstring(buffer, geojson_geometry_type_name(type));
    }
    if (rc == 0) {
        rc = append_cstring(buffer, "\", \"coordinates\": ");
    }
    if (rc == 0) {
        rc = append_geojson_coordinates(
            buffer,
            cursor,
            type,
            little_endian,
            srid,
            max_dec_digits,
            error,
            function_name
        );
    }
    return rc;
}

static int append_geojson_coordinates(
    struct spatial_buffer *buffer,
    struct spatial_wkb_cursor *cursor,
    enum mylite_spatial_geometry_type type,
    bool little_endian,
    uint32_t srid,
    uint32_t max_dec_digits,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    switch (type) {
    case MYLITE_SPATIAL_GEOMETRY_POINT:
        return append_geojson_point_coordinates(
            buffer,
            cursor,
            little_endian,
            srid,
            max_dec_digits,
            error,
            function_name
        );
    case MYLITE_SPATIAL_GEOMETRY_LINESTRING:
        return append_geojson_line_coordinates(
            buffer,
            cursor,
            little_endian,
            srid,
            max_dec_digits,
            error,
            function_name
        );
    case MYLITE_SPATIAL_GEOMETRY_POLYGON:
        return append_geojson_polygon_coordinates(
            buffer,
            cursor,
            little_endian,
            srid,
            max_dec_digits,
            error,
            function_name
        );
    case MYLITE_SPATIAL_GEOMETRY_MULTIPOINT:
    case MYLITE_SPATIAL_GEOMETRY_MULTILINESTRING:
    case MYLITE_SPATIAL_GEOMETRY_MULTIPOLYGON:
        return append_geojson_collection_coordinates(
            buffer,
            cursor,
            type,
            little_endian,
            srid,
            max_dec_digits,
            error,
            function_name
        );
    case MYLITE_SPATIAL_GEOMETRY_GEOMETRYCOLLECTION:
    case MYLITE_SPATIAL_GEOMETRY_NONE:
        break;
    }
    return set_invalid_gis_data_error(error, function_name);
}

static int append_geojson_point_coordinates(
    struct spatial_buffer *buffer,
    struct spatial_wkb_cursor *cursor,
    bool little_endian,
    uint32_t srid, // NOLINT(bugprone-easily-swappable-parameters): GeoJSON axis options.
    uint32_t max_dec_digits,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    double coordinate_x = 0.0;
    double coordinate_y = 0.0;
    double output_x = 0.0;
    double output_y = 0.0;
    int rc = cursor_read_double(cursor, little_endian, &coordinate_x, error, function_name);

    if (rc == 0) {
        rc = cursor_read_double(cursor, little_endian, &coordinate_y, error, function_name);
    }
    output_x = srid == spatial_srid_wgs84 ? coordinate_y : coordinate_x;
    output_y = srid == spatial_srid_wgs84 ? coordinate_x : coordinate_y;
    if (rc == 0) {
        rc = spatial_buffer_append_byte(buffer, '[');
    }
    if (rc == 0) {
        rc = append_geojson_number(buffer, output_x, max_dec_digits);
    }
    if (rc == 0) {
        rc = append_cstring(buffer, ", ");
    }
    if (rc == 0) {
        rc = append_geojson_number(buffer, output_y, max_dec_digits);
    }
    if (rc == 0) {
        rc = spatial_buffer_append_byte(buffer, ']');
    }
    return rc;
}

static int append_geojson_line_coordinates(
    struct spatial_buffer *buffer,
    struct spatial_wkb_cursor *cursor,
    bool little_endian,
    uint32_t srid,
    uint32_t max_dec_digits,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    uint32_t count = 0U;
    int rc = cursor_read_u32(cursor, little_endian, &count, error, function_name);

    if (rc == 0) {
        rc = spatial_buffer_append_byte(buffer, '[');
    }
    for (uint32_t index = 0U; rc == 0 && index < count; ++index) {
        if (index > 0U) {
            rc = append_cstring(buffer, ", ");
        }
        if (rc == 0) {
            rc = append_geojson_point_coordinates(
                buffer,
                cursor,
                little_endian,
                srid,
                max_dec_digits,
                error,
                function_name
            );
        }
    }
    if (rc == 0) {
        rc = spatial_buffer_append_byte(buffer, ']');
    }
    return rc;
}

static int append_geojson_polygon_coordinates(
    struct spatial_buffer *buffer,
    struct spatial_wkb_cursor *cursor,
    bool little_endian,
    uint32_t srid,
    uint32_t max_dec_digits,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    uint32_t ring_count = 0U;
    int rc = cursor_read_u32(cursor, little_endian, &ring_count, error, function_name);

    if (rc == 0) {
        rc = spatial_buffer_append_byte(buffer, '[');
    }
    for (uint32_t ring_index = 0U; rc == 0 && ring_index < ring_count; ++ring_index) {
        if (ring_index > 0U) {
            rc = append_cstring(buffer, ", ");
        }
        if (rc == 0) {
            rc = append_geojson_line_coordinates(
                buffer,
                cursor,
                little_endian,
                srid,
                max_dec_digits,
                error,
                function_name
            );
        }
    }
    if (rc == 0) {
        rc = spatial_buffer_append_byte(buffer, ']');
    }
    return rc;
}

static int append_geojson_collection_coordinates(
    struct spatial_buffer *buffer,
    struct spatial_wkb_cursor *cursor,
    enum mylite_spatial_geometry_type type,
    bool little_endian,
    uint32_t srid,
    uint32_t max_dec_digits,
    struct mylite_spatial_error *error,
    const char *function_name
) {
    enum mylite_spatial_geometry_type expected_type = collection_expected_nested_type(type);
    uint32_t count = 0U;
    int rc = cursor_read_u32(cursor, little_endian, &count, error, function_name);

    if (rc == 0) {
        rc = spatial_buffer_append_byte(buffer, '[');
    }
    for (uint32_t index = 0U; rc == 0 && index < count; ++index) {
        bool nested_little_endian = false;
        enum mylite_spatial_geometry_type nested_type = MYLITE_SPATIAL_GEOMETRY_NONE;

        if (index > 0U) {
            rc = append_cstring(buffer, ", ");
        }
        if (rc == 0) {
            rc = cursor_read_header(
                cursor,
                &nested_little_endian,
                &nested_type,
                error,
                function_name
            );
        }
        if (rc == 0 && nested_type != expected_type) {
            rc = set_invalid_gis_data_error(error, function_name);
        }
        if (rc == 0) {
            rc = append_geojson_coordinates(
                buffer,
                cursor,
                nested_type,
                nested_little_endian,
                srid,
                max_dec_digits,
                error,
                function_name
            );
        }
    }
    if (rc == 0) {
        rc = spatial_buffer_append_byte(buffer, ']');
    }
    return rc;
}

static int append_geojson_number(
    struct spatial_buffer *buffer,
    double value,
    uint32_t max_dec_digits
) {
    char text[spatial_geojson_double_text_capacity];
    char *decimal = NULL;
    char *exponent = NULL;
    size_t length = 0U;
    int written = 0;

    if (value == 0.0) {
        value = 0.0;
    }
    if (max_dec_digits > spatial_geojson_default_max_dec_digits) {
        max_dec_digits = spatial_geojson_default_max_dec_digits;
    }
    written = snprintf(text, sizeof(text), "%.*f", (int)max_dec_digits, value);
    if (written < 0 || (size_t)written >= sizeof(text)) {
        return -1;
    }
    decimal = strchr(text, '.');
    exponent = strchr(text, 'e');
    if (exponent == NULL) {
        exponent = strchr(text, 'E');
    }
    if (decimal != NULL && exponent == NULL) {
        char *last = text + strlen(text) - 1U;

        while (last > decimal + 1 && *last == '0') {
            *last = '\0';
            --last;
        }
    } else if (decimal == NULL && exponent == NULL) {
        length = strlen(text);
        if (length + 2U >= sizeof(text)) {
            return -1;
        }
        text[length] = '.';
        text[length + 1U] = '0';
        text[length + 2U] = '\0';
    }
    return spatial_buffer_append(buffer, text, strlen(text));
}

static int append_geojson_bbox(
    struct spatial_buffer *buffer,
    const struct spatial_box *box,
    uint32_t srid, // NOLINT(bugprone-easily-swappable-parameters): GeoJSON axis options.
    uint32_t max_dec_digits
) {
    double values[4] = {0.0, 0.0, 0.0, 0.0};
    int rc = append_cstring(buffer, "\"bbox\": ");

    if (box == NULL || !box->has_value) {
        if (rc == 0) {
            rc = append_cstring(buffer, "[]");
        }
        return rc;
    }
    if (srid == spatial_srid_wgs84) {
        values[0] = box->min_y;
        values[1] = box->min_x;
        values[2] = box->max_y;
        values[3] = box->max_x;
    } else {
        values[0] = box->min_x;
        values[1] = box->min_y;
        values[2] = box->max_x;
        values[3] = box->max_y;
    }
    if (rc == 0) {
        rc = spatial_buffer_append_byte(buffer, '[');
    }
    for (size_t index = 0U; rc == 0 && index < 4U; ++index) {
        if (index > 0U) {
            rc = append_cstring(buffer, ", ");
        }
        if (rc == 0) {
            rc = append_geojson_number(buffer, values[index], max_dec_digits);
        }
    }
    if (rc == 0) {
        rc = spatial_buffer_append_byte(buffer, ']');
    }
    return rc;
}

static int append_geojson_geometry_as_wkt(
    const struct json_value *value,
    struct geojson_parse_context *context,
    struct spatial_buffer *out_wkt,
    bool *out_is_null
) {
    const struct json_value *type = geojson_member_value(value, "type");
    const struct json_value *coordinates = geojson_member_value(value, "coordinates");
    const struct json_value *geometries = geojson_member_value(value, "geometries");
    int rc = 0;

    if (out_is_null != NULL) {
        *out_is_null = value == NULL || value->kind == JSON_VALUE_NULL;
    }
    if (value == NULL || value->kind == JSON_VALUE_NULL) {
        return 0;
    }
    if (value->kind != JSON_VALUE_OBJECT || type == NULL || type->kind != JSON_VALUE_STRING) {
        return set_invalid_geojson_data_error(context->error, context->function_name);
    }
    if (geojson_string_equals(type, "Feature")) {
        return append_geojson_feature_as_wkt(value, context, out_wkt, out_is_null);
    }
    if (geojson_string_equals(type, "FeatureCollection")) {
        if (out_is_null != NULL) {
            *out_is_null = false;
        }
        return append_geojson_feature_collection_as_wkt(value, context, out_wkt);
    }
    if (geojson_string_equals(type, "Point")) {
        if (coordinates == NULL) {
            return set_invalid_geojson_missing_member_error(
                context->error,
                "coordinates",
                context->function_name
            );
        }
        rc = append_cstring(out_wkt, "POINT(");
        if (rc == 0) {
            rc = append_geojson_coordinate_as_wkt(coordinates, context, out_wkt);
        }
        if (rc == 0) {
            rc = spatial_buffer_append_byte(out_wkt, ')');
        }
        return rc;
    }
    if (geojson_string_equals(type, "LineString")) {
        if (coordinates == NULL) {
            return set_invalid_geojson_missing_member_error(
                context->error,
                "coordinates",
                context->function_name
            );
        }
        rc = append_cstring(out_wkt, "LINESTRING(");
        if (rc == 0) {
            rc = append_geojson_coordinate_list_as_wkt(coordinates, context, out_wkt);
        }
        if (rc == 0) {
            rc = spatial_buffer_append_byte(out_wkt, ')');
        }
        return rc;
    }
    if (geojson_string_equals(type, "Polygon")) {
        if (coordinates == NULL) {
            return set_invalid_geojson_missing_member_error(
                context->error,
                "coordinates",
                context->function_name
            );
        }
        rc = append_cstring(out_wkt, "POLYGON(");
        if (rc == 0) {
            rc = append_geojson_ring_list_as_wkt(coordinates, context, out_wkt);
        }
        if (rc == 0) {
            rc = spatial_buffer_append_byte(out_wkt, ')');
        }
        return rc;
    }
    if (geojson_string_equals(type, "MultiPoint")) {
        if (coordinates == NULL || coordinates->kind != JSON_VALUE_ARRAY) {
            return coordinates == NULL
                       ? set_invalid_geojson_missing_member_error(
                             context->error,
                             "coordinates",
                             context->function_name
                         )
                       : set_invalid_geojson_data_error(context->error, context->function_name);
        }
        rc = append_cstring(out_wkt, "MULTIPOINT(");
        for (size_t index = 0U; rc == 0 && index < coordinates->payload.array.count; ++index) {
            if (index > 0U) {
                rc = spatial_buffer_append_byte(out_wkt, ',');
            }
            if (rc == 0) {
                rc = spatial_buffer_append_byte(out_wkt, '(');
            }
            if (rc == 0) {
                rc = append_geojson_coordinate_as_wkt(
                    &coordinates->payload.array.values[index],
                    context,
                    out_wkt
                );
            }
            if (rc == 0) {
                rc = spatial_buffer_append_byte(out_wkt, ')');
            }
        }
        if (rc == 0) {
            rc = spatial_buffer_append_byte(out_wkt, ')');
        }
        return rc;
    }
    if (geojson_string_equals(type, "MultiLineString")) {
        if (coordinates == NULL || coordinates->kind != JSON_VALUE_ARRAY) {
            return coordinates == NULL
                       ? set_invalid_geojson_missing_member_error(
                             context->error,
                             "coordinates",
                             context->function_name
                         )
                       : set_invalid_geojson_data_error(context->error, context->function_name);
        }
        rc = append_cstring(out_wkt, "MULTILINESTRING(");
        for (size_t index = 0U; rc == 0 && index < coordinates->payload.array.count; ++index) {
            if (index > 0U) {
                rc = spatial_buffer_append_byte(out_wkt, ',');
            }
            if (rc == 0) {
                rc = spatial_buffer_append_byte(out_wkt, '(');
            }
            if (rc == 0) {
                rc = append_geojson_coordinate_list_as_wkt(
                    &coordinates->payload.array.values[index],
                    context,
                    out_wkt
                );
            }
            if (rc == 0) {
                rc = spatial_buffer_append_byte(out_wkt, ')');
            }
        }
        if (rc == 0) {
            rc = spatial_buffer_append_byte(out_wkt, ')');
        }
        return rc;
    }
    if (geojson_string_equals(type, "MultiPolygon")) {
        if (coordinates == NULL) {
            return set_invalid_geojson_missing_member_error(
                context->error,
                "coordinates",
                context->function_name
            );
        }
        rc = append_cstring(out_wkt, "MULTIPOLYGON(");
        if (rc == 0) {
            rc = append_geojson_polygon_list_as_wkt(coordinates, context, out_wkt);
        }
        if (rc == 0) {
            rc = spatial_buffer_append_byte(out_wkt, ')');
        }
        return rc;
    }
    if (geojson_string_equals(type, "GeometryCollection")) {
        if (geometries == NULL || geometries->kind != JSON_VALUE_ARRAY) {
            return geometries == NULL
                       ? set_invalid_geojson_missing_member_error(
                             context->error,
                             "geometries",
                             context->function_name
                         )
                       : set_invalid_geojson_data_error(context->error, context->function_name);
        }
        rc = append_cstring(out_wkt, "GEOMETRYCOLLECTION(");
        for (size_t index = 0U; rc == 0 && index < geometries->payload.array.count; ++index) {
            bool is_null = false;

            if (index > 0U) {
                rc = spatial_buffer_append_byte(out_wkt, ',');
            }
            if (rc == 0) {
                rc = append_geojson_geometry_as_wkt(
                    &geometries->payload.array.values[index],
                    context,
                    out_wkt,
                    &is_null
                );
            }
            if (rc == 0 && is_null) {
                rc = set_invalid_geojson_data_error(context->error, context->function_name);
            }
        }
        if (rc == 0) {
            rc = spatial_buffer_append_byte(out_wkt, ')');
        }
        return rc;
    }
    return set_invalid_geojson_data_error(context->error, context->function_name);
}

static int append_geojson_feature_as_wkt(
    const struct json_value *value,
    struct geojson_parse_context *context,
    struct spatial_buffer *out_wkt,
    bool *out_is_null
) {
    const struct json_value *geometry = geojson_member_value(value, "geometry");
    const struct json_value *properties = geojson_member_value(value, "properties");

    if (properties == NULL) {
        return set_invalid_geojson_missing_member_error(
            context->error,
            "properties",
            context->function_name
        );
    }
    if (geometry == NULL) {
        return set_invalid_geojson_missing_member_error(
            context->error,
            "geometry",
            context->function_name
        );
    }
    return append_geojson_geometry_as_wkt(geometry, context, out_wkt, out_is_null);
}

static int append_geojson_feature_collection_as_wkt(
    const struct json_value *value,
    struct geojson_parse_context *context,
    struct spatial_buffer *out_wkt
) {
    const struct json_value *features = geojson_member_value(value, "features");
    int rc = 0;

    if (features == NULL || features->kind != JSON_VALUE_ARRAY) {
        return features == NULL
                   ? set_invalid_geojson_missing_member_error(
                         context->error,
                         "features",
                         context->function_name
                     )
                   : set_invalid_geojson_data_error(context->error, context->function_name);
    }
    rc = append_cstring(out_wkt, "GEOMETRYCOLLECTION(");
    for (size_t index = 0U; rc == 0 && index < features->payload.array.count; ++index) {
        bool is_null = false;

        if (index > 0U) {
            rc = spatial_buffer_append_byte(out_wkt, ',');
        }
        if (rc == 0) {
            rc = append_geojson_geometry_as_wkt(
                &features->payload.array.values[index],
                context,
                out_wkt,
                &is_null
            );
        }
        if (rc == 0 && is_null) {
            rc = set_invalid_geojson_data_error(context->error, context->function_name);
        }
    }
    if (rc == 0) {
        rc = spatial_buffer_append_byte(out_wkt, ')');
    }
    return rc;
}

static int append_geojson_coordinate_as_wkt(
    const struct json_value *value,
    struct geojson_parse_context *context,
    struct spatial_buffer *out_wkt
) {
    double longitude = 0.0;
    double latitude = 0.0;
    double coordinate_x = 0.0;
    double coordinate_y = 0.0;
    int rc = 0;

    if (value == NULL || value->kind != JSON_VALUE_ARRAY || value->payload.array.count < 2U) {
        return set_invalid_geojson_data_error(context->error, context->function_name);
    }
    if (value->payload.array.count > 2U && !context->strip_extra_dimensions) {
        return set_unsupported_geojson_dimensions_error(
            context->error,
            value->payload.array.count,
            context->function_name
        );
    }
    rc = geojson_coordinate_number(&value->payload.array.values[0], &longitude, context);
    if (rc == 0) {
        rc = geojson_coordinate_number(&value->payload.array.values[1], &latitude, context);
    }
    if (rc != 0) {
        return rc;
    }
    if (context->srid == spatial_srid_wgs84) {
        if (longitude <= spatial_geohash_longitude_min ||
            longitude > spatial_geohash_longitude_max) {
            return set_geojson_longitude_error(context->error, longitude, context->function_name);
        }
        if (latitude < spatial_geohash_latitude_min || latitude > spatial_geohash_latitude_max) {
            return set_geojson_latitude_error(context->error, latitude, context->function_name);
        }
        coordinate_x = latitude;
        coordinate_y = longitude;
    } else {
        coordinate_x = longitude;
        coordinate_y = latitude;
    }
    rc = append_double_as_text(out_wkt, coordinate_x);
    if (rc == 0) {
        rc = spatial_buffer_append_byte(out_wkt, ' ');
    }
    if (rc == 0) {
        rc = append_double_as_text(out_wkt, coordinate_y);
    }
    return rc;
}

static int append_geojson_coordinate_list_as_wkt(
    const struct json_value *value,
    struct geojson_parse_context *context,
    struct spatial_buffer *out_wkt
) {
    int rc = 0;

    if (value == NULL || value->kind != JSON_VALUE_ARRAY) {
        return set_invalid_geojson_data_error(context->error, context->function_name);
    }
    for (size_t index = 0U; rc == 0 && index < value->payload.array.count; ++index) {
        if (index > 0U) {
            rc = spatial_buffer_append_byte(out_wkt, ',');
        }
        if (rc == 0) {
            rc = append_geojson_coordinate_as_wkt(
                &value->payload.array.values[index],
                context,
                out_wkt
            );
        }
    }
    return rc;
}

static int append_geojson_ring_list_as_wkt(
    const struct json_value *value,
    struct geojson_parse_context *context,
    struct spatial_buffer *out_wkt
) {
    int rc = 0;

    if (value == NULL || value->kind != JSON_VALUE_ARRAY) {
        return set_invalid_geojson_data_error(context->error, context->function_name);
    }
    for (size_t index = 0U; rc == 0 && index < value->payload.array.count; ++index) {
        if (index > 0U) {
            rc = spatial_buffer_append_byte(out_wkt, ',');
        }
        if (rc == 0) {
            rc = spatial_buffer_append_byte(out_wkt, '(');
        }
        if (rc == 0) {
            rc = append_geojson_coordinate_list_as_wkt(
                &value->payload.array.values[index],
                context,
                out_wkt
            );
        }
        if (rc == 0) {
            rc = spatial_buffer_append_byte(out_wkt, ')');
        }
    }
    return rc;
}

static int append_geojson_polygon_list_as_wkt(
    const struct json_value *value,
    struct geojson_parse_context *context,
    struct spatial_buffer *out_wkt
) {
    int rc = 0;

    if (value == NULL || value->kind != JSON_VALUE_ARRAY) {
        return set_invalid_geojson_data_error(context->error, context->function_name);
    }
    for (size_t index = 0U; rc == 0 && index < value->payload.array.count; ++index) {
        if (index > 0U) {
            rc = spatial_buffer_append_byte(out_wkt, ',');
        }
        if (rc == 0) {
            rc = spatial_buffer_append_byte(out_wkt, '(');
        }
        if (rc == 0) {
            rc = append_geojson_ring_list_as_wkt(
                &value->payload.array.values[index],
                context,
                out_wkt
            );
        }
        if (rc == 0) {
            rc = spatial_buffer_append_byte(out_wkt, ')');
        }
    }
    return rc;
}

static int geojson_coordinate_number(
    const struct json_value *value,
    double *out_value,
    struct geojson_parse_context *context
) {
    char *end = NULL;
    double parsed = 0.0;

    if (value == NULL || out_value == NULL || value->kind != JSON_VALUE_NUMBER ||
        value->payload.text.text == NULL) {
        return set_invalid_geojson_data_error(context->error, context->function_name);
    }
    errno = 0;
    parsed = strtod(value->payload.text.text, &end);
    if (end != value->payload.text.text + value->payload.text.length || errno == ERANGE ||
        !isfinite(parsed)) {
        return set_invalid_geojson_data_error(context->error, context->function_name);
    }
    *out_value = parsed;
    return 0;
}

static const char *geojson_geometry_type_name(enum mylite_spatial_geometry_type type) {
    switch (type) {
    case MYLITE_SPATIAL_GEOMETRY_POINT:
        return "Point";
    case MYLITE_SPATIAL_GEOMETRY_LINESTRING:
        return "LineString";
    case MYLITE_SPATIAL_GEOMETRY_POLYGON:
        return "Polygon";
    case MYLITE_SPATIAL_GEOMETRY_MULTIPOINT:
        return "MultiPoint";
    case MYLITE_SPATIAL_GEOMETRY_MULTILINESTRING:
        return "MultiLineString";
    case MYLITE_SPATIAL_GEOMETRY_MULTIPOLYGON:
        return "MultiPolygon";
    case MYLITE_SPATIAL_GEOMETRY_GEOMETRYCOLLECTION:
        return "GeometryCollection";
    case MYLITE_SPATIAL_GEOMETRY_NONE:
        break;
    }
    return "Geometry";
}

static const struct json_value *geojson_member_value(
    const struct json_value *value,
    const char *member
) {
    if (value == NULL || value->kind != JSON_VALUE_OBJECT || member == NULL) {
        return NULL;
    }
    for (size_t index = 0U; index < value->payload.object.count; ++index) {
        const struct json_member *candidate = &value->payload.object.members[index];

        if (geojson_member_name_equals(candidate->key, candidate->key_length, member)) {
            return candidate->value;
        }
    }
    return NULL;
}

static bool geojson_member_name_equals(const char *left, size_t left_length, const char *right) {
    if (left == NULL || right == NULL || strlen(right) != left_length) {
        return false;
    }
    for (size_t index = 0U; index < left_length; ++index) {
        if (tolower((unsigned char)left[index]) != tolower((unsigned char)right[index])) {
            return false;
        }
    }
    return true;
}

static bool geojson_string_equals(const struct json_value *value, const char *expected) {
    if (value == NULL || value->kind != JSON_VALUE_STRING || expected == NULL) {
        return false;
    }
    return value->payload.text.length == strlen(expected) &&
           memcmp(value->payload.text.text, expected, value->payload.text.length) == 0;
}

static int append_cstring(struct spatial_buffer *buffer, const char *text) {
    return spatial_buffer_append(buffer, text, strlen(text));
}

static int parse_wkt_to_internal(
    const char *text,
    size_t text_size,
    const char *function_name,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    enum mylite_spatial_geometry_type *out_type,
    struct mylite_spatial_error *error
) {
    char *copy = NULL;
    struct spatial_wkt_parser parser = {0};
    struct spatial_buffer wkb = {0};
    struct spatial_buffer internal = {0};
    enum mylite_spatial_geometry_type type = MYLITE_SPATIAL_GEOMETRY_NONE;
    int rc = 0;

    if (text == NULL || out_bytes == NULL || out_byte_count == NULL || out_type == NULL) {
        return set_invalid_gis_data_error(error, function_name);
    }
    if (text_size == SIZE_MAX) {
        return set_nomem_error(error);
    }
    copy = (char *)malloc(text_size + 1U);
    if (copy == NULL) {
        return set_nomem_error(error);
    }
    if (text_size != 0U) {
        memcpy(copy, text, text_size);
    }
    copy[text_size] = '\0';
    parser = (struct spatial_wkt_parser){
        .text = copy,
        .size = text_size,
        .offset = 0U,
        .function_name = function_name,
        .error = error,
    };
    rc = parse_wkt_geometry(&parser, &wkb, &type);
    wkt_skip_space(&parser);
    if (rc == 0 && !wkt_at_end(&parser)) {
        rc = set_invalid_gis_data_error(error, function_name);
    }
    if (rc == 0) {
        rc = append_internal_prefix(&internal, 0U);
    }
    if (rc == 0) {
        rc = spatial_buffer_append(&internal, wkb.bytes, wkb.size);
    }
    free(copy);
    spatial_buffer_deinit(&wkb);
    if (rc != 0) {
        spatial_buffer_deinit(&internal);
        return rc;
    }
    *out_bytes = internal.bytes;
    *out_byte_count = internal.size;
    *out_type = type;
    return 0;
}

static int parse_wkt_geometry(
    struct spatial_wkt_parser *parser,
    struct spatial_buffer *out_wkb,
    enum mylite_spatial_geometry_type *out_type
) {
    enum mylite_spatial_geometry_type type = MYLITE_SPATIAL_GEOMETRY_NONE;
    int rc = 0;

    if (!wkt_match_type(parser, &type)) {
        return set_invalid_gis_data_error(parser->error, parser->function_name);
    }
    switch (type) {
    case MYLITE_SPATIAL_GEOMETRY_POINT:
        rc = parse_wkt_point(parser, out_wkb);
        break;
    case MYLITE_SPATIAL_GEOMETRY_LINESTRING:
        rc = parse_wkt_linestring(parser, out_wkb);
        break;
    case MYLITE_SPATIAL_GEOMETRY_POLYGON:
        rc = parse_wkt_polygon(parser, out_wkb);
        break;
    case MYLITE_SPATIAL_GEOMETRY_MULTIPOINT:
        rc = parse_wkt_multipoint(parser, out_wkb);
        break;
    case MYLITE_SPATIAL_GEOMETRY_MULTILINESTRING:
        rc = parse_wkt_multilinestring(parser, out_wkb);
        break;
    case MYLITE_SPATIAL_GEOMETRY_MULTIPOLYGON:
        rc = parse_wkt_multipolygon(parser, out_wkb);
        break;
    case MYLITE_SPATIAL_GEOMETRY_GEOMETRYCOLLECTION:
        rc = parse_wkt_geometrycollection(parser, out_wkb);
        break;
    default:
        rc = set_invalid_gis_data_error(parser->error, parser->function_name);
        break;
    }
    if (rc == 0) {
        *out_type = type;
    }
    return rc;
}

static int parse_wkt_point(struct spatial_wkt_parser *parser, struct spatial_buffer *out_wkb) {
    double coordinate_x = 0.0;
    double coordinate_y = 0.0;
    int rc = spatial_buffer_append_byte(out_wkb, 1U);

    if (rc == 0) {
        rc = spatial_buffer_append_u32_le(out_wkb, (uint32_t)MYLITE_SPATIAL_GEOMETRY_POINT);
    }
    if (rc == 0) {
        rc = wkt_expect_byte(parser, '(');
    }
    if (rc == 0) {
        rc = parse_wkt_coordinate(parser, &coordinate_x, &coordinate_y);
    }
    if (rc == 0) {
        rc = wkt_expect_byte(parser, ')');
    }
    if (rc == 0) {
        rc = spatial_buffer_append_double_le(out_wkb, coordinate_x);
    }
    if (rc == 0) {
        rc = spatial_buffer_append_double_le(out_wkb, coordinate_y);
    }
    return rc;
}

static int parse_wkt_linestring(struct spatial_wkt_parser *parser, struct spatial_buffer *out_wkb) {
    struct spatial_buffer points = {0};
    uint32_t point_count = 0U;
    int rc = wkt_expect_byte(parser, '(');

    if (rc == 0) {
        rc = parse_wkt_coordinate_list(parser, &points, &point_count);
    }
    if (rc == 0) {
        rc = wkt_expect_byte(parser, ')');
    }
    if (rc == 0 && point_count < 2U) {
        rc = set_invalid_gis_data_error(parser->error, parser->function_name);
    }
    if (rc == 0) {
        rc = spatial_buffer_append_byte(out_wkb, 1U);
    }
    if (rc == 0) {
        rc = spatial_buffer_append_u32_le(out_wkb, (uint32_t)MYLITE_SPATIAL_GEOMETRY_LINESTRING);
    }
    if (rc == 0) {
        rc = spatial_buffer_append_u32_le(out_wkb, point_count);
    }
    if (rc == 0) {
        rc = spatial_buffer_append(out_wkb, points.bytes, points.size);
    }
    spatial_buffer_deinit(&points);
    return rc;
}

static int parse_wkt_polygon(struct spatial_wkt_parser *parser, struct spatial_buffer *out_wkb) {
    struct spatial_buffer rings = {0};
    uint32_t ring_count = 0U;
    int rc = wkt_expect_byte(parser, '(');

    if (rc == 0) {
        rc = parse_wkt_ring_list(parser, &rings, &ring_count);
    }
    if (rc == 0) {
        rc = wkt_expect_byte(parser, ')');
    }
    if (rc == 0) {
        rc = spatial_buffer_append_byte(out_wkb, 1U);
    }
    if (rc == 0) {
        rc = spatial_buffer_append_u32_le(out_wkb, (uint32_t)MYLITE_SPATIAL_GEOMETRY_POLYGON);
    }
    if (rc == 0) {
        rc = spatial_buffer_append_u32_le(out_wkb, ring_count);
    }
    if (rc == 0) {
        rc = spatial_buffer_append(out_wkb, rings.bytes, rings.size);
    }
    spatial_buffer_deinit(&rings);
    return rc;
}

static int parse_wkt_multipoint(struct spatial_wkt_parser *parser, struct spatial_buffer *out_wkb) {
    struct spatial_buffer points = {0};
    uint32_t point_count = 0U;
    int rc = wkt_expect_byte(parser, '(');

    while (rc == 0) {
        struct spatial_buffer point = {0};
        double coordinate_x = 0.0;
        double coordinate_y = 0.0;
        bool has_nested_parens = wkt_consume_byte(parser, '(');

        rc = parse_wkt_coordinate(parser, &coordinate_x, &coordinate_y);
        if (rc == 0 && has_nested_parens) {
            rc = wkt_expect_byte(parser, ')');
        }
        if (rc == 0) {
            rc = spatial_buffer_append_byte(&point, 1U);
        }
        if (rc == 0) {
            rc = spatial_buffer_append_u32_le(&point, (uint32_t)MYLITE_SPATIAL_GEOMETRY_POINT);
        }
        if (rc == 0) {
            rc = spatial_buffer_append_double_le(&point, coordinate_x);
        }
        if (rc == 0) {
            rc = spatial_buffer_append_double_le(&point, coordinate_y);
        }
        if (rc == 0) {
            rc = spatial_buffer_append(&points, point.bytes, point.size);
        }
        spatial_buffer_deinit(&point);
        if (rc != 0) {
            break;
        }
        ++point_count;
        if (!wkt_consume_byte(parser, ',')) {
            break;
        }
    }
    if (rc == 0) {
        rc = wkt_expect_byte(parser, ')');
    }
    if (rc == 0) {
        rc = spatial_buffer_append_byte(out_wkb, 1U);
    }
    if (rc == 0) {
        rc = spatial_buffer_append_u32_le(out_wkb, (uint32_t)MYLITE_SPATIAL_GEOMETRY_MULTIPOINT);
    }
    if (rc == 0) {
        rc = spatial_buffer_append_u32_le(out_wkb, point_count);
    }
    if (rc == 0) {
        rc = spatial_buffer_append(out_wkb, points.bytes, points.size);
    }
    spatial_buffer_deinit(&points);
    return rc;
}

static int parse_wkt_multilinestring(
    struct spatial_wkt_parser *parser,
    struct spatial_buffer *out_wkb
) {
    struct spatial_buffer lines = {0};
    uint32_t line_count = 0U;
    int rc = wkt_expect_byte(parser, '(');

    while (rc == 0) {
        struct spatial_buffer line = {0};

        rc = parse_wkt_linestring(parser, &line);
        if (rc == 0) {
            rc = spatial_buffer_append(&lines, line.bytes, line.size);
        }
        spatial_buffer_deinit(&line);
        if (rc != 0) {
            break;
        }
        ++line_count;
        if (!wkt_consume_byte(parser, ',')) {
            break;
        }
    }
    if (rc == 0) {
        rc = wkt_expect_byte(parser, ')');
    }
    if (rc == 0) {
        rc = spatial_buffer_append_byte(out_wkb, 1U);
    }
    if (rc == 0) {
        rc = spatial_buffer_append_u32_le(
            out_wkb,
            (uint32_t)MYLITE_SPATIAL_GEOMETRY_MULTILINESTRING
        );
    }
    if (rc == 0) {
        rc = spatial_buffer_append_u32_le(out_wkb, line_count);
    }
    if (rc == 0) {
        rc = spatial_buffer_append(out_wkb, lines.bytes, lines.size);
    }
    spatial_buffer_deinit(&lines);
    return rc;
}

static int parse_wkt_multipolygon(
    struct spatial_wkt_parser *parser,
    struct spatial_buffer *out_wkb
) {
    struct spatial_buffer polygons = {0};
    uint32_t polygon_count = 0U;
    int rc = wkt_expect_byte(parser, '(');

    while (rc == 0) {
        struct spatial_buffer polygon = {0};

        rc = parse_wkt_polygon(parser, &polygon);
        if (rc == 0) {
            rc = spatial_buffer_append(&polygons, polygon.bytes, polygon.size);
        }
        spatial_buffer_deinit(&polygon);
        if (rc != 0) {
            break;
        }
        ++polygon_count;
        if (!wkt_consume_byte(parser, ',')) {
            break;
        }
    }
    if (rc == 0) {
        rc = wkt_expect_byte(parser, ')');
    }
    if (rc == 0) {
        rc = spatial_buffer_append_byte(out_wkb, 1U);
    }
    if (rc == 0) {
        rc = spatial_buffer_append_u32_le(out_wkb, (uint32_t)MYLITE_SPATIAL_GEOMETRY_MULTIPOLYGON);
    }
    if (rc == 0) {
        rc = spatial_buffer_append_u32_le(out_wkb, polygon_count);
    }
    if (rc == 0) {
        rc = spatial_buffer_append(out_wkb, polygons.bytes, polygons.size);
    }
    spatial_buffer_deinit(&polygons);
    return rc;
}

static int parse_wkt_geometrycollection(
    struct spatial_wkt_parser *parser,
    struct spatial_buffer *out_wkb
) {
    struct spatial_buffer geometries = {0};
    uint32_t geometry_count = 0U;
    int rc = 0;

    wkt_skip_space(parser);
    if (wkt_match_keyword(parser, "EMPTY")) {
        rc = spatial_buffer_append_byte(out_wkb, 1U);
        if (rc == 0) {
            rc = spatial_buffer_append_u32_le(
                out_wkb,
                (uint32_t)MYLITE_SPATIAL_GEOMETRY_GEOMETRYCOLLECTION
            );
        }
        if (rc == 0) {
            rc = spatial_buffer_append_u32_le(out_wkb, 0U);
        }
        return rc;
    }
    rc = wkt_expect_byte(parser, '(');
    if (rc == 0 && wkt_consume_byte(parser, ')')) {
        rc = spatial_buffer_append_byte(out_wkb, 1U);
        if (rc == 0) {
            rc = spatial_buffer_append_u32_le(
                out_wkb,
                (uint32_t)MYLITE_SPATIAL_GEOMETRY_GEOMETRYCOLLECTION
            );
        }
        if (rc == 0) {
            rc = spatial_buffer_append_u32_le(out_wkb, 0U);
        }
        return rc;
    }
    while (rc == 0) {
        struct spatial_buffer geometry = {0};
        enum mylite_spatial_geometry_type type = MYLITE_SPATIAL_GEOMETRY_NONE;

        rc = parse_wkt_geometry(parser, &geometry, &type);
        (void)type;
        if (rc == 0) {
            rc = spatial_buffer_append(&geometries, geometry.bytes, geometry.size);
        }
        spatial_buffer_deinit(&geometry);
        if (rc != 0) {
            break;
        }
        ++geometry_count;
        if (!wkt_consume_byte(parser, ',')) {
            break;
        }
    }
    if (rc == 0) {
        rc = wkt_expect_byte(parser, ')');
    }
    if (rc == 0) {
        rc = spatial_buffer_append_byte(out_wkb, 1U);
    }
    if (rc == 0) {
        rc = spatial_buffer_append_u32_le(
            out_wkb,
            (uint32_t)MYLITE_SPATIAL_GEOMETRY_GEOMETRYCOLLECTION
        );
    }
    if (rc == 0) {
        rc = spatial_buffer_append_u32_le(out_wkb, geometry_count);
    }
    if (rc == 0) {
        rc = spatial_buffer_append(out_wkb, geometries.bytes, geometries.size);
    }
    spatial_buffer_deinit(&geometries);
    return rc;
}

static int parse_wkt_coordinate(struct spatial_wkt_parser *parser, double *out_x, double *out_y) {
    char *end = NULL;

    wkt_skip_space(parser);
    errno = 0;
    *out_x = strtod(parser->text + parser->offset, &end);
    if (end == parser->text + parser->offset || errno == ERANGE || !isfinite(*out_x)) {
        return set_invalid_gis_data_error(parser->error, parser->function_name);
    }
    parser->offset = (size_t)(end - parser->text);
    wkt_skip_space(parser);
    errno = 0;
    *out_y = strtod(parser->text + parser->offset, &end);
    if (end == parser->text + parser->offset || errno == ERANGE || !isfinite(*out_y)) {
        return set_invalid_gis_data_error(parser->error, parser->function_name);
    }
    parser->offset = (size_t)(end - parser->text);
    return 0;
}

static int parse_wkt_coordinate_list(
    struct spatial_wkt_parser *parser,
    struct spatial_buffer *out_points,
    uint32_t *out_point_count
) {
    int rc = 0;

    while (rc == 0) {
        double coordinate_x = 0.0;
        double coordinate_y = 0.0;

        rc = parse_wkt_coordinate(parser, &coordinate_x, &coordinate_y);
        if (rc == 0) {
            rc = spatial_buffer_append_double_le(out_points, coordinate_x);
        }
        if (rc == 0) {
            rc = spatial_buffer_append_double_le(out_points, coordinate_y);
        }
        if (rc != 0) {
            break;
        }
        ++(*out_point_count);
        if (!wkt_consume_byte(parser, ',')) {
            break;
        }
    }
    return rc;
}

static int parse_wkt_ring_list(
    struct spatial_wkt_parser *parser,
    struct spatial_buffer *out_rings,
    uint32_t *out_ring_count
) {
    int rc = 0;

    while (rc == 0) {
        struct spatial_buffer points = {0};
        uint32_t point_count = 0U;

        rc = wkt_expect_byte(parser, '(');
        if (rc == 0) {
            rc = parse_wkt_coordinate_list(parser, &points, &point_count);
        }
        if (rc == 0) {
            rc = wkt_expect_byte(parser, ')');
        }
        if (rc == 0) {
            rc = spatial_buffer_append_u32_le(out_rings, point_count);
        }
        if (rc == 0) {
            rc = spatial_buffer_append(out_rings, points.bytes, points.size);
        }
        spatial_buffer_deinit(&points);
        if (rc != 0) {
            break;
        }
        ++(*out_ring_count);
        if (!wkt_consume_byte(parser, ',')) {
            break;
        }
    }
    return rc;
}

static bool wkt_match_type(
    struct spatial_wkt_parser *parser,
    enum mylite_spatial_geometry_type *out_type
) {
    static const struct spatial_type_name types[] = {
        {"GEOMETRYCOLLECTION", MYLITE_SPATIAL_GEOMETRY_GEOMETRYCOLLECTION},
        {"GEOMCOLLECTION", MYLITE_SPATIAL_GEOMETRY_GEOMETRYCOLLECTION},
        {"MULTILINESTRING", MYLITE_SPATIAL_GEOMETRY_MULTILINESTRING},
        {"MULTIPOLYGON", MYLITE_SPATIAL_GEOMETRY_MULTIPOLYGON},
        {"MULTIPOINT", MYLITE_SPATIAL_GEOMETRY_MULTIPOINT},
        {"LINESTRING", MYLITE_SPATIAL_GEOMETRY_LINESTRING},
        {"POLYGON", MYLITE_SPATIAL_GEOMETRY_POLYGON},
        {"POINT", MYLITE_SPATIAL_GEOMETRY_POINT},
    };

    for (size_t index = 0U; index < sizeof(types) / sizeof(types[0]); ++index) {
        if (wkt_match_keyword(parser, types[index].text)) {
            *out_type = types[index].type;
            return true;
        }
    }
    return false;
}

static bool wkt_match_keyword(struct spatial_wkt_parser *parser, const char *keyword) {
    size_t length = strlen(keyword);

    wkt_skip_space(parser);
    if (parser->offset + length > parser->size) {
        return false;
    }
    for (size_t index = 0U; index < length; ++index) {
        if (toupper((unsigned char)parser->text[parser->offset + index]) !=
            toupper((unsigned char)keyword[index])) {
            return false;
        }
    }
    if (parser->offset + length < parser->size &&
        (isalnum((unsigned char)parser->text[parser->offset + length]) ||
         parser->text[parser->offset + length] == '_')) {
        return false;
    }
    parser->offset += length;
    return true;
}

static int wkt_expect_byte(struct spatial_wkt_parser *parser, char expected) {
    if (!wkt_consume_byte(parser, expected)) {
        return set_invalid_gis_data_error(parser->error, parser->function_name);
    }
    return 0;
}

static bool wkt_consume_byte(struct spatial_wkt_parser *parser, char expected) {
    wkt_skip_space(parser);
    if (parser->offset >= parser->size || parser->text[parser->offset] != expected) {
        return false;
    }
    ++parser->offset;
    return true;
}

static void wkt_skip_space(struct spatial_wkt_parser *parser) {
    while (parser->offset < parser->size && isspace((unsigned char)parser->text[parser->offset])) {
        ++parser->offset;
    }
}

static bool wkt_at_end(struct spatial_wkt_parser *parser) {
    return parser->offset >= parser->size;
}
