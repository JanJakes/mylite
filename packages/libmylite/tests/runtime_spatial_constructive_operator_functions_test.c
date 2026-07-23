#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdio.h>
#include <string.h>

enum {
    mysql_error_wrong_arguments = 1210,
    mysql_error_native_function_parameter_count = 1582,
    mysql_error_parameter_exceeds_max_points = 3134,
    mysql_error_gis_different_srids = 3033,
    mysql_error_invalid_gis_data = 3037,
    mysql_error_not_implemented_for_cartesian_srs = 3704,
    mysql_error_transform_source_srs_not_supported = 3741,
    mysql_error_transform_target_srs_not_supported = 3742,
    strategy_column_count = 9,
    mysql_buffer_strategy_byte_count = 12,
    mysql_buffer_strategy_hex_size = (mysql_buffer_strategy_byte_count * 2) + 1,
    hex_low_nibble_mask = 0x0F,
    scalar_constructive_column_count = 22,
    row_constructive_row_count = 3,
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

static int test_scalar_spatial_constructive_operator_functions(void);
static int test_table_backed_spatial_constructive_operator_functions(void);
static int test_spatial_constructive_operator_diagnostics(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_strategy_query(mylite_db *database);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int expect_result_hex(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static void bytes_to_hex(const void *bytes, size_t byte_count, char *out_hex, size_t out_hex_size);

int main(void) {
    int failures = 0;

    failures += test_scalar_spatial_constructive_operator_functions();
    failures += test_table_backed_spatial_constructive_operator_functions();
    failures += test_spatial_constructive_operator_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_scalar_spatial_constructive_operator_functions(void) {
    static const char *const values[] = {
        "POINT(0 0)",
        "POINT(0 0)",
        "POINT(0 0)",
        NULL,
        "4326",
        "POINT(1 1)",
        "GEOMETRYCOLLECTION EMPTY",
        "POINT(1 1)",
        "GEOMETRYCOLLECTION EMPTY",
        "POINT(1 1)",
        "MULTIPOINT((1 1),(2 2))",
        "MULTIPOINT((1 1),(2 2))",
        "GEOMETRYCOLLECTION EMPTY",
        "MULTIPOINT((1 1),(3 3))",
        "POINT(2 2)",
        "MULTIPOINT((1 1),(2 2),(3 3),(4 4))",
        "MULTIPOINT((1 1),(3 3),(4 4))",
        "MULTIPOINT((1 1),(2 2))",
        "MULTIPOINT((3 3),(1 1),(2 2))",
        NULL,
        "POINT(1 1)",
        "4326",
    };
    mylite_db *database = NULL;
    int failures =
        mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open scalar database");

    failures += expect_strategy_query(database);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT "
                   "ST_AsText(ST_Buffer(Point(0,0), 0)), "
                   "ST_AsText(ST_Buffer(Point(0,0), 'abc')), "
                   "ST_AsText(ST_Buffer(Point(0,0), 0, ST_Buffer_Strategy('point_square'))), "
                   "ST_AsText(ST_Buffer(Point(0,0), 0, NULL)), "
                   "ST_SRID(ST_Buffer(ST_PointFromGeoHash('mh2n0p0581',4326), 0)), "
                   "ST_AsText(ST_Difference(Point(1,1), Point(2,2))), "
                   "ST_AsText(ST_Difference(Point(1,1), Point(1,1))), "
                   "ST_AsText(ST_Intersection(Point(1,1), Point(1,1))), "
                   "ST_AsText(ST_Intersection(Point(1,1), Point(2,2))), "
                   "ST_AsText(ST_Union(Point(1,1), Point(1,1))), "
                   "ST_AsText(ST_Union(Point(1,1), Point(2,2))), "
                   "ST_AsText(ST_SymDifference(Point(1,1), Point(2,2))), "
                   "ST_AsText(ST_SymDifference(Point(1,1), Point(1,1))), "
                   "ST_AsText(ST_Difference("
                   "ST_GeomFromText('MULTIPOINT((1 1),(2 2),(2 2),(3 3))'), "
                   "ST_GeomFromText('MULTIPOINT((2 2),(4 4))'))), "
                   "ST_AsText(ST_Intersection("
                   "ST_GeomFromText('MULTIPOINT((1 1),(2 2),(2 2),(3 3))'), "
                   "ST_GeomFromText('MULTIPOINT((2 2),(4 4))'))), "
                   "ST_AsText(ST_Union("
                   "ST_GeomFromText('MULTIPOINT((1 1),(2 2),(2 2),(3 3))'), "
                   "ST_GeomFromText('MULTIPOINT((2 2),(4 4))'))), "
                   "ST_AsText(ST_SymDifference("
                   "ST_GeomFromText('MULTIPOINT((1 1),(2 2),(2 2),(3 3))'), "
                   "ST_GeomFromText('MULTIPOINT((2 2),(4 4))'))), "
                   "ST_AsText(ST_Union(Point(1,1), "
                   "ST_GeomFromText('MULTIPOINT((1 1),(2 2))'))), "
                   "ST_AsText(ST_SymDifference(Point(3,3), "
                   "ST_GeomFromText('MULTIPOINT((1 1),(2 2))'))), "
                   "ST_AsText(ST_Union(NULL, Point(1,1))), "
                   "ST_AsText(ST_Transform(Point(1,1), '0abc')), "
                   "ST_SRID(ST_Transform(ST_PointFromGeoHash('mh2n0p0581',4326), 4326))",
            .column_count = scalar_constructive_column_count,
            .values = values,
            .row_count = 1U,
            .context = "scalar spatial constructive operator functions",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_table_backed_spatial_constructive_operator_functions(void) {
    static const char *const projection_values[] = {
        "1",
        "MULTIPOINT((1 1),(2 2))",
        "GEOMETRYCOLLECTION EMPTY",
        "2",
        "MULTIPOINT((1 1),(2 2),(3 3),(4 4))",
        "POINT(2 2)",
        "3",
        NULL,
        NULL,
    };
    static const char *const update_values[] = {
        "1",
        "MULTIPOINT((1 1),(2 2))",
        "2",
        "MULTIPOINT((1 1),(3 3),(4 4))",
        "3",
        NULL,
    };
    mylite_db *database = NULL;
    int failures = mylite_test_expect_int(
        mylite_test_open_temporary(&database),
        MYLITE_OK,
        "open row database"
    );

    failures += execute_ok(database, "CREATE DATABASE spatial_constructive", NULL);
    failures += execute_ok(database, "USE spatial_constructive", NULL);
    failures += execute_ok(
        database,
        "CREATE TABLE spatial_values("
        "id INT PRIMARY KEY, left_g GEOMETRY, right_g GEOMETRY, out_g GEOMETRY)",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO spatial_values(id, left_g, right_g) VALUES "
        "(1, Point(1,1), Point(2,2)), "
        "(2, ST_GeomFromText('MULTIPOINT((1 1),(2 2),(2 2),(3 3))'), "
        "ST_GeomFromText('MULTIPOINT((2 2),(4 4))')), "
        "(3, NULL, Point(1,1))",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, ST_AsText(ST_Union(left_g, right_g)), "
                   "ST_AsText(ST_Intersection(left_g, right_g)) "
                   "FROM spatial_values ORDER BY id",
            .column_count = 3U,
            .values = projection_values,
            .row_count = row_constructive_row_count,
            .context = "row-backed point set projection",
        }
    );
    failures += execute_ok(
        database,
        "UPDATE spatial_values SET out_g = ST_SymDifference(left_g, right_g)",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, ST_AsText(out_g) FROM spatial_values ORDER BY id",
            .column_count = 2U,
            .values = update_values,
            .row_count = row_constructive_row_count,
            .context = "row-backed point set assignment",
        }
    );

    mylite_close(database);
    return failures;
}

static int expect_strategy_query(mylite_db *database) {
    static const char *const expected_hex[] = {
        "060000000000000000000000",
        "020000000000000000000000",
        "050000003333333333330740",
        "030000000000000000004040",
        "040000000000000000004040",
        "010000000000000000004040",
        "060000000000000000000000",
        NULL,
        NULL,
    };
    mylite_result *result = NULL;
    int failures = execute_ok(
        database,
        "SELECT ST_Buffer_Strategy('point_square'), "
        "ST_Buffer_Strategy('end_flat'), "
        "ST_Buffer_Strategy('point_circle', 2.9), "
        "ST_Buffer_Strategy('join_round', 32), "
        "ST_Buffer_Strategy('join_miter', 32), "
        "ST_Buffer_Strategy('end_round', 32), "
        "ST_Buffer_Strategy('POINT_SQUARE'), "
        "ST_Buffer_Strategy(NULL), "
        "ST_Buffer_Strategy('point_circle', NULL)",
        &result
    );

    if (result == NULL) {
        return failures + 1;
    }
    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        strategy_column_count,
        "strategy"
    );
    failures += mylite_test_expect_size(mylite_result_row_count(result), 1U, "strategy");
    if (mylite_result_column_count(result) == strategy_column_count &&
        mylite_result_row_count(result) == 1U) {
        for (size_t column = 0U; column < strategy_column_count; ++column) {
            failures += expect_result_hex(
                result,
                0U,
                column,
                expected_hex[column],
                "buffer strategy bytes"
            );
        }
    }
    mylite_result_free(result);
    return failures;
}

static int test_spatial_constructive_operator_diagnostics(void) {
    mylite_db *database = NULL;
    int failures = mylite_test_expect_int(
        mylite_open_memory(&database),
        MYLITE_OK,
        "open diagnostics database"
    );

    failures += execute_error(
        database,
        "SELECT ST_Buffer_Strategy()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_parameter_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_Buffer_Strategy('point_circle', 1, 2)",
        (struct expected_sql_error){
            .code = mysql_error_native_function_parameter_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_Buffer_Strategy('bad', NULL)",
        (struct expected_sql_error){
            .code = mysql_error_wrong_arguments,
            .sqlstate = "HY000",
            .message_part = "Incorrect arguments to st_buffer_strategy",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_Buffer_Strategy('point_square', NULL)",
        (struct expected_sql_error){
            .code = mysql_error_wrong_arguments,
            .sqlstate = "HY000",
            .message_part = "Incorrect arguments to st_buffer_strategy",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_Buffer_Strategy('point_circle')",
        (struct expected_sql_error){
            .code = mysql_error_wrong_arguments,
            .sqlstate = "HY000",
            .message_part = "Incorrect arguments to st_buffer_strategy",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_Buffer_Strategy('point_circle', 65537)",
        (struct expected_sql_error){
            .code = mysql_error_parameter_exceeds_max_points,
            .sqlstate = "HY000",
            .message_part = "Parameter points_per_circle exceeds",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_Buffer(Point(0,0), 1)",
        (struct expected_sql_error){
            .code = mysql_error_not_implemented_for_cartesian_srs,
            .sqlstate = "22S00",
            .message_part = "st_buffer(POINT, POINT) has not been implemented",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_Difference(ST_GeomFromText('LINESTRING(0 0,1 1)'), "
        "ST_GeomFromText('LINESTRING(1 0,0 1)'))",
        (struct expected_sql_error){
            .code = mysql_error_not_implemented_for_cartesian_srs,
            .sqlstate = "22S00",
            .message_part = "st_difference(LINESTRING, LINESTRING) has not been implemented",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_Union(ST_PointFromGeoHash('mh2n0p0581',4326), Point(1,1))",
        (struct expected_sql_error){
            .code = mysql_error_gis_different_srids,
            .sqlstate = "HY000",
            .message_part = "different srids: 4326 and 0",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_Union(X'010203', Point(1,1))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_gis_data,
            .sqlstate = "22023",
            .message_part = "Invalid GIS data provided to function st_union",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_Transform(Point(1,1), 4326)",
        (struct expected_sql_error){
            .code = mysql_error_transform_source_srs_not_supported,
            .sqlstate = "22S00",
            .message_part = "Transformation from SRID 0 is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_Transform(ST_PointFromGeoHash('mh2n0p0581',4326), 0)",
        (struct expected_sql_error){
            .code = mysql_error_transform_target_srs_not_supported,
            .sqlstate = "22S00",
            .message_part = "Transformation to SRID 0 is not supported",
        }
    );

    mylite_close(database);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *local_result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &local_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected success, got rc=%d sqlstate=%s message=%s\n",
            sql,
            rc,
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        return 1;
    }
    if (out_result != NULL) {
        *out_result = local_result;
    } else {
        mylite_result_free(local_result);
    }
    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    mylite_result_free(result);
    if (rc == MYLITE_OK) {
        fprintf(stderr, "%s: expected error, got success\n", sql);
        return 1;
    }
    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, sql);
    failures += mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += mylite_test_expect_contains(mylite_errmsg(database), expected.message_part, sql);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, expected.sql, &result);
    size_t value_count = expected.column_count * expected.row_count;

    if (result == NULL) {
        return failures + 1;
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
    if (mylite_result_column_count(result) == expected.column_count &&
        mylite_result_row_count(result) == expected.row_count) {
        for (size_t index = 0U; index < value_count; ++index) {
            failures += expect_result_value(
                result,
                index / expected.column_count,
                index % expected.column_count,
                expected.values[index],
                expected.context
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
    const char *context
) {
    return mylite_test_expect_text(
        mylite_result_value_text(result, row, column),
        expected,
        context
    );
}

static int expect_result_hex(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
) {
    char actual_hex[mysql_buffer_strategy_hex_size];
    const void *bytes = mylite_result_value_bytes(result, row, column);
    size_t byte_count = mylite_result_value_size(result, row, column);

    if (expected == NULL) {
        return mylite_test_expect_text(
            mylite_result_value_text(result, row, column),
            NULL,
            context
        );
    }
    bytes_to_hex(bytes, byte_count, actual_hex, sizeof(actual_hex));
    return mylite_test_expect_text(actual_hex, expected, context);
}

static void bytes_to_hex(const void *bytes, size_t byte_count, char *out_hex, size_t out_hex_size) {
    static const char hex_digits[] = "0123456789ABCDEF";
    const unsigned char *input = (const unsigned char *)bytes;
    size_t output_index = 0U;

    if (out_hex == NULL || out_hex_size == 0U) {
        return;
    }
    if (bytes == NULL || byte_count > (out_hex_size - 1U) / 2U) {
        out_hex[0] = '\0';
        return;
    }
    for (size_t index = 0U; index < byte_count; ++index) {
        out_hex[output_index++] = hex_digits[input[index] >> 4U];
        out_hex[output_index++] = hex_digits[input[index] & hex_low_nibble_mask];
    }
    out_hex[output_index] = '\0';
}
