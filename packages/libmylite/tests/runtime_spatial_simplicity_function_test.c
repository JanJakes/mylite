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
    spatial_simplicity_case_count = 31,
    spatial_simplicity_row_count = 5,
    mysql_error_native_function_parameter_count = 1582,
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

static int test_scalar_spatial_simplicity_function(void);
static int test_table_backed_spatial_simplicity_function(void);
static int test_spatial_simplicity_diagnostics(void);
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

    failures += test_scalar_spatial_simplicity_function();
    failures += test_table_backed_spatial_simplicity_function();
    failures += test_spatial_simplicity_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_scalar_spatial_simplicity_function(void) {
    static const char *const values[] = {
        "NULL",
        NULL,
        "point",
        "1",
        "emptygc",
        "1",
        "line_simple",
        "1",
        "line_closed_triangle",
        "1",
        "line_repeat_mid",
        "0",
        "line_self_cross",
        "0",
        "line_duplicate_consecutive",
        "0",
        "line_collinear_backtrack",
        "0",
        "multipoint_unique",
        "1",
        "multipoint_dup",
        "0",
        "multiline_disjoint",
        "1",
        "multiline_endpoint_touch",
        "1",
        "multiline_endpoint_to_interior",
        "0",
        "multiline_interior_cross",
        "0",
        "multiline_interior_overlap",
        "0",
        "multiline_same_reversed",
        "0",
        "multiline_closed_touch_closed",
        "0",
        "polygon_valid",
        "1",
        "polygon_invalid_self",
        "1",
        "multipolygon_overlap",
        "1",
        "collection_simple",
        "1",
        "collection_nonsimple_member",
        "0",
        "collection_crossing_lines",
        "0",
        "collection_duplicate_points",
        "0",
        "collection_line_point_endpoint",
        "1",
        "collection_line_point_interior",
        "0",
        "collection_polygon_point_boundary",
        "1",
        "collection_polygon_point_interior",
        "0",
        "collection_polygon_line_outside",
        "1",
        "collection_polygon_line_cross",
        "0",
    };
    mylite_db *database = NULL;
    int failures =
        mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open scalar database");

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 'NULL', ST_IsSimple(NULL) UNION ALL "
                   "SELECT 'point', ST_IsSimple(Point(1,2)) UNION ALL "
                   "SELECT 'emptygc', ST_IsSimple(ST_GeomFromText('GEOMETRYCOLLECTION EMPTY')) "
                   "UNION ALL "
                   "SELECT 'line_simple', ST_IsSimple(ST_GeomFromText('LINESTRING(0 0,1 1)')) "
                   "UNION ALL "
                   "SELECT 'line_closed_triangle', "
                   "ST_IsSimple(ST_GeomFromText('LINESTRING(0 0,1 0,0 1,0 0)')) UNION ALL "
                   "SELECT 'line_repeat_mid', "
                   "ST_IsSimple(ST_GeomFromText('LINESTRING(0 0,1 1,0 0)')) UNION ALL "
                   "SELECT 'line_self_cross', "
                   "ST_IsSimple(ST_GeomFromText('LINESTRING(0 0,2 2,0 2,2 0)')) UNION ALL "
                   "SELECT 'line_duplicate_consecutive', "
                   "ST_IsSimple(ST_GeomFromText('LINESTRING(0 0,0 0,1 1)')) UNION ALL "
                   "SELECT 'line_collinear_backtrack', "
                   "ST_IsSimple(ST_GeomFromText('LINESTRING(0 0,2 0,1 0)')) UNION ALL "
                   "SELECT 'multipoint_unique', "
                   "ST_IsSimple(ST_GeomFromText('MULTIPOINT((0 0),(1 1))')) UNION ALL "
                   "SELECT 'multipoint_dup', "
                   "ST_IsSimple(ST_GeomFromText('MULTIPOINT((0 0),(0 0))')) UNION ALL "
                   "SELECT 'multiline_disjoint', "
                   "ST_IsSimple(ST_GeomFromText('MULTILINESTRING((0 0,1 1),(2 2,3 3))')) "
                   "UNION ALL "
                   "SELECT 'multiline_endpoint_touch', "
                   "ST_IsSimple(ST_GeomFromText('MULTILINESTRING((0 0,1 1),(1 1,2 2))')) "
                   "UNION ALL "
                   "SELECT 'multiline_endpoint_to_interior', "
                   "ST_IsSimple(ST_GeomFromText('MULTILINESTRING((0 0,2 0),(1 0,1 1))')) "
                   "UNION ALL "
                   "SELECT 'multiline_interior_cross', "
                   "ST_IsSimple(ST_GeomFromText('MULTILINESTRING((0 0,2 2),(0 2,2 0))')) "
                   "UNION ALL "
                   "SELECT 'multiline_interior_overlap', "
                   "ST_IsSimple(ST_GeomFromText('MULTILINESTRING((0 0,2 0),(1 0,3 0))')) "
                   "UNION ALL "
                   "SELECT 'multiline_same_reversed', "
                   "ST_IsSimple(ST_GeomFromText('MULTILINESTRING((0 0,1 1),(1 1,0 0))')) "
                   "UNION ALL "
                   "SELECT 'multiline_closed_touch_closed', "
                   "ST_IsSimple(ST_GeomFromText('MULTILINESTRING((0 0,1 0,0 1,0 0),"
                   "(0 0,-1 0,0 -1,0 0))')) UNION ALL "
                   "SELECT 'polygon_valid', "
                   "ST_IsSimple(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))')) "
                   "UNION ALL "
                   "SELECT 'polygon_invalid_self', "
                   "ST_IsSimple(ST_GeomFromText('POLYGON((0 0,2 2,2 0,0 2,0 0))')) "
                   "UNION ALL "
                   "SELECT 'multipolygon_overlap', "
                   "ST_IsSimple(ST_GeomFromText('MULTIPOLYGON(((0 0,2 0,2 2,0 2,0 0)),"
                   "((1 1,3 1,3 3,1 3,1 1)))')) UNION ALL "
                   "SELECT 'collection_simple', "
                   "ST_IsSimple(ST_GeomFromText('GEOMETRYCOLLECTION(POINT(1 2),"
                   "LINESTRING(0 0,1 1))')) UNION ALL "
                   "SELECT 'collection_nonsimple_member', "
                   "ST_IsSimple(ST_GeomFromText('GEOMETRYCOLLECTION(POINT(1 2),"
                   "MULTIPOINT((0 0),(0 0)))')) UNION ALL "
                   "SELECT 'collection_crossing_lines', "
                   "ST_IsSimple(ST_GeomFromText('GEOMETRYCOLLECTION(LINESTRING(0 0,2 2),"
                   "LINESTRING(0 2,2 0))')) UNION ALL "
                   "SELECT 'collection_duplicate_points', "
                   "ST_IsSimple(ST_GeomFromText('GEOMETRYCOLLECTION(POINT(0 0),"
                   "POINT(0 0))')) UNION ALL "
                   "SELECT 'collection_line_point_endpoint', "
                   "ST_IsSimple(ST_GeomFromText('GEOMETRYCOLLECTION(LINESTRING(0 0,2 0),"
                   "POINT(0 0))')) UNION ALL "
                   "SELECT 'collection_line_point_interior', "
                   "ST_IsSimple(ST_GeomFromText('GEOMETRYCOLLECTION(LINESTRING(0 0,2 0),"
                   "POINT(1 0))')) UNION ALL "
                   "SELECT 'collection_polygon_point_boundary', "
                   "ST_IsSimple(ST_GeomFromText('GEOMETRYCOLLECTION("
                   "POLYGON((0 0,4 0,4 4,0 4,0 0)),POINT(0 0))')) UNION ALL "
                   "SELECT 'collection_polygon_point_interior', "
                   "ST_IsSimple(ST_GeomFromText('GEOMETRYCOLLECTION("
                   "POLYGON((0 0,4 0,4 4,0 4,0 0)),POINT(1 1))')) UNION ALL "
                   "SELECT 'collection_polygon_line_outside', "
                   "ST_IsSimple(ST_GeomFromText('GEOMETRYCOLLECTION("
                   "POLYGON((0 0,4 0,4 4,0 4,0 0)),LINESTRING(5 5,6 6))')) UNION ALL "
                   "SELECT 'collection_polygon_line_cross', "
                   "ST_IsSimple(ST_GeomFromText('GEOMETRYCOLLECTION("
                   "POLYGON((0 0,4 0,4 4,0 4,0 0)),LINESTRING(-1 2,5 2))'))",
            .column_count = 2U,
            .values = values,
            .row_count = spatial_simplicity_case_count,
            .context = "scalar spatial simplicity function",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_table_backed_spatial_simplicity_function(void) {
    static const char *const projection_values[] = {
        "1",
        "1",
        "2",
        "0",
        "3",
        "0",
        "4",
        "1",
        "5",
        NULL,
    };
    static const char *const update_values[] = {
        "1",
        "1",
        "2",
        "0",
        "3",
        "0",
        "4",
        "1",
        "5",
        NULL,
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
    failures += execute_ok(database, "CREATE DATABASE spatial_simplicity", NULL);
    failures += execute_ok(database, "USE spatial_simplicity", NULL);
    failures += execute_ok(
        database,
        "CREATE TABLE spatial_values(id INT PRIMARY KEY, g GEOMETRY, txt VARCHAR(16))",
        NULL
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO spatial_values VALUES "
        "(1, Point(1,2), NULL), "
        "(2, ST_GeomFromText('LINESTRING(0 0,2 2,0 2,2 0)'), NULL), "
        "(3, ST_GeomFromText('MULTIPOINT((0 0),(0 0))'), NULL), "
        "(4, ST_GeomFromText('POLYGON((0 0,2 2,2 0,0 2,0 0))'), NULL), "
        "(5, NULL, NULL)",
        (struct expected_dml_result){
            .affected_rows = spatial_simplicity_row_count,
            .warning_count = 0U,
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, ST_IsSimple(g) FROM spatial_values ORDER BY id",
            .column_count = 2U,
            .values = projection_values,
            .row_count = spatial_simplicity_row_count,
            .context = "row-backed spatial simplicity projection",
        }
    );
    failures += expect_dml_ok(
        database,
        "UPDATE spatial_values SET txt = ST_IsSimple(g)",
        (struct expected_dml_result){.affected_rows = 4, .warning_count = 0U}
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, txt FROM spatial_values ORDER BY id",
            .column_count = 2U,
            .values = update_values,
            .row_count = spatial_simplicity_row_count,
            .context = "row-backed spatial simplicity update",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_spatial_simplicity_diagnostics(void) {
    mylite_db *database = NULL;
    int failures = mylite_test_expect_int(
        mylite_open_memory(&database),
        MYLITE_OK,
        "open diagnostics database"
    );

    failures += execute_error(
        database,
        "SELECT ST_IsSimple()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_parameter_count,
            .sqlstate = "42000",
            .message_part =
                "Incorrect parameter count in the call to native function 'st_issimple'",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_IsSimple(X'010203')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_gis_data,
            .sqlstate = "22023",
            .message_part = "Invalid GIS data provided to function st_issimple",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_IsSimple(ST_GeomFromText('LINESTRING(0 0)'))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_gis_data,
            .sqlstate = "22023",
            .message_part = "Invalid GIS data provided to function st_geomfromtext",
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
