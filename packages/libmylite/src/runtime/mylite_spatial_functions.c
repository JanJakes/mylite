#include "mylite_spatial_functions.h"

#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_spatial.h"
#include "mylite_sqlite_bootstrap.h"
#include "mylite_sqlite_registration.h"

#include <mylite/mylite.h>

#include <limits.h>
#include <stddef.h>
#include <stdlib.h>

struct spatial_sqlite_function_descriptor {
    const char *name;
    enum mylite_spatial_function_kind kind;
};

static void spatial_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static int spatial_sqlite_arguments(
    int argc,
    sqlite3_value **argv,
    struct mylite_spatial_argument **out_arguments
);
static void spatial_sqlite_set_error(
    sqlite3_context *context,
    const struct mylite_spatial_error *error
);
static void spatial_sqlite_set_result(
    sqlite3_context *context,
    const struct mylite_spatial_result *result
);

static const struct spatial_sqlite_function_descriptor spatial_sqlite_function_descriptors[] = {
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

int mylite_sqlite_register_spatial_functions(sqlite3 *sqlite) {
    int rc = MYLITE_OK;

    for (size_t index = 0U; index < sizeof(spatial_sqlite_function_descriptors) /
                                        sizeof(spatial_sqlite_function_descriptors[0]);
         ++index) {
        const struct mylite_sqlite_function_registration registration = {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = spatial_sqlite_function_descriptors[index].name,
            .argument_count = -1,
            .text_representation = SQLITE_UTF8,
            .application_data = (void *)&spatial_sqlite_function_descriptors[index],
            .scalar_callback = spatial_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        };

        rc = mylite_sqlite_register_functions(sqlite, &registration, 1U);
        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    return MYLITE_OK;
}

static void spatial_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    const struct spatial_sqlite_function_descriptor *descriptor = sqlite3_user_data(context);
    struct mylite_spatial_argument *arguments = NULL;
    struct mylite_spatial_result result = {0};
    struct mylite_spatial_error error = {0};

    if (descriptor == NULL) {
        sqlite3_result_error(context, "MyLite spatial function is not registered", -1);
        return;
    }
    if (spatial_sqlite_arguments(argc, argv, &arguments) != MYLITE_OK) {
        sqlite3_result_error_nomem(context);
        return;
    }
    if (mylite_spatial_evaluate(descriptor->kind, arguments, (size_t)argc, &result, &error) != 0) {
        spatial_sqlite_set_error(context, &error);
    } else {
        spatial_sqlite_set_result(context, &result);
    }

    mylite_spatial_result_deinit(&result);
    free(arguments);
}

static int spatial_sqlite_arguments(
    int argc,
    sqlite3_value **argv,
    struct mylite_spatial_argument **out_arguments
) {
    struct mylite_spatial_argument *arguments = NULL;

    if (argc < 0 || out_arguments == NULL) {
        return MYLITE_MISUSE;
    }
    *out_arguments = NULL;
    if (argc == 0) {
        return MYLITE_OK;
    }
    arguments = (struct mylite_spatial_argument *)calloc((size_t)argc, sizeof(*arguments));
    if (arguments == NULL) {
        return MYLITE_NOMEM;
    }
    for (int index = 0; index < argc; ++index) {
        int type = sqlite3_value_type(argv[index]);

        if (type == SQLITE_NULL) {
            arguments[index].is_null = true;
            continue;
        }
        arguments[index].bytes = sqlite3_value_blob(argv[index]);
        arguments[index].byte_count = (size_t)sqlite3_value_bytes(argv[index]);
        if (type == SQLITE_INTEGER || type == SQLITE_FLOAT) {
            arguments[index].numeric = sqlite3_value_double(argv[index]);
            arguments[index].has_numeric = true;
        }
    }
    *out_arguments = arguments;
    return MYLITE_OK;
}

static void spatial_sqlite_set_error(
    sqlite3_context *context,
    const struct mylite_spatial_error *error
) {
    struct mylite_db *database = mylite_sqlite_bootstrap_owner_from_context(context);
    const char *message = error == NULL || error->message[0] == '\0'
                              ? "MyLite spatial function failed"
                              : error->message;

    if (error != NULL && error->is_nomem) {
        sqlite3_result_error_nomem(context);
        return;
    }
    if (database != NULL && error != NULL && error->code != 0) {
        mylite_diagnostics_set_error(
            mylite_connection_diagnostics(database),
            error->code,
            error->sqlstate == NULL ? "HY000" : error->sqlstate,
            message
        );
    }
    sqlite3_result_error(context, message, -1);
}

static void spatial_sqlite_set_result(
    sqlite3_context *context,
    const struct mylite_spatial_result *result
) {
    if (result == NULL || result->kind == MYLITE_SPATIAL_RESULT_NULL) {
        sqlite3_result_null(context);
        return;
    }
    switch (result->kind) {
    case MYLITE_SPATIAL_RESULT_GEOMETRY:
    case MYLITE_SPATIAL_RESULT_BLOB:
        if (result->byte_count > (size_t)INT_MAX) {
            sqlite3_result_error(context, "MyLite spatial result too large", -1);
            return;
        }
        sqlite3_result_blob(context, result->bytes, (int)result->byte_count, SQLITE_TRANSIENT);
        return;
    case MYLITE_SPATIAL_RESULT_TEXT:
        if (result->byte_count > (size_t)INT_MAX) {
            sqlite3_result_error(context, "MyLite spatial result too large", -1);
            return;
        }
        sqlite3_result_text(context, result->bytes, (int)result->byte_count, SQLITE_TRANSIENT);
        return;
    case MYLITE_SPATIAL_RESULT_INTEGER:
        sqlite3_result_int64(context, (sqlite3_int64)result->integer);
        return;
    case MYLITE_SPATIAL_RESULT_DOUBLE:
        sqlite3_result_double(context, result->real);
        return;
    case MYLITE_SPATIAL_RESULT_NULL:
        break;
    }
    sqlite3_result_null(context);
}
