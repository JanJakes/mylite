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
    spatial_simplify_row_count = 4,
    mysql_error_wrong_arguments = 1210,
    mysql_error_native_function_parameter_count = 1582,
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

static int test_scalar_spatial_simplify_function(void);
static int test_table_backed_spatial_simplify_function(void);
static int test_spatial_simplify_diagnostics(void);
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

    failures += test_scalar_spatial_simplify_function();
    failures += test_table_backed_spatial_simplify_function();
    failures += test_spatial_simplify_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_scalar_spatial_simplify_function(void) {
    static const char *const values[] = {
        "LINESTRING(0 0,0 1,1 1,2 3,3 3)",
        "LINESTRING(0 0,3 3)",
        "POINT(1 2)",
        "MULTIPOINT((0 0),(1 1),(2 2))",
        "LINESTRING(0 0,3 0)",
        "LINESTRING(0 0,1 0.1,3 0)",
        "POLYGON((2 2,0 2,0 0,2 0,2 2))",
        NULL,
        "MULTILINESTRING((0 0,2 0),(0 0,0 2))",
        "MULTIPOLYGON(((2 2,0 2,0 0,2 0,2 2)))",
        "POLYGON((5 5,0 5,0 0,5 0,5 5))",
        "GEOMETRYCOLLECTION(POINT(1 2),LINESTRING(0 0,2 0))",
        "GEOMETRYCOLLECTION(LINESTRING(0 0,2 0))",
        NULL,
        NULL,
    };
    mylite_db *database = NULL;
    int failures =
        expect_int(mylite_open(":memory:", &database), MYLITE_OK, "open scalar database");

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT "
                   "ST_AsText(ST_Simplify(ST_GeomFromText('LINESTRING(0 0,0 1,1 1,"
                   "1 2,2 2,2 3,3 3)'), '0.5')), "
                   "ST_AsText(ST_Simplify(ST_GeomFromText('LINESTRING(0 0,0 1,1 1,"
                   "1 2,2 2,2 3,3 3)'), '1.0')), "
                   "ST_AsText(ST_Simplify(Point(1,2), '1')), "
                   "ST_AsText(ST_Simplify(ST_GeomFromText('MULTIPOINT((0 0),(1 1),(2 2))'), '1')), "
                   "ST_AsText(ST_Simplify(ST_GeomFromText('LINESTRING(0 0,1 0.1,2 0,3 0)'), "
                   "'0.2')), "
                   "ST_AsText(ST_Simplify(ST_GeomFromText('LINESTRING(0 0,1 0.1,2 0,3 0)'), "
                   "'0.05')), "
                   "ST_AsText(ST_Simplify(ST_GeomFromText('POLYGON((0 0,1 0.1,2 0,2 2,"
                   "0 2,0 0))'), '0.2')), "
                   "ST_AsText(ST_Simplify(ST_GeomFromText('POLYGON((0 0,0.1 0,0.1 0.1,"
                   "0 0.1,0 0))'), '1')), "
                   "ST_AsText(ST_Simplify(ST_GeomFromText('MULTILINESTRING((0 0,1 0.1,2 0),"
                   "(0 0,0 1,0 2))'), '0.2')), "
                   "ST_AsText(ST_Simplify(ST_GeomFromText('MULTIPOLYGON(((0 0,1 0.1,2 0,"
                   "2 2,0 2,0 0)))'), '0.2')), "
                   "ST_AsText(ST_Simplify(ST_GeomFromText('POLYGON((0 0,5 0,5 5,0 5,"
                   "0 0),(1 1,1.1 1,1.1 1.1,1 1.1,1 1))'), '1')), "
                   "ST_AsText(ST_Simplify(ST_GeomFromText('GEOMETRYCOLLECTION(POINT(1 2),"
                   "LINESTRING(0 0,1 0.1,2 0))'), '0.2')), "
                   "ST_AsText(ST_Simplify(ST_GeomFromText('GEOMETRYCOLLECTION(POLYGON((0 0,"
                   "0.1 0,0.1 0.1,0 0.1,0 0)),LINESTRING(0 0,1 0.1,2 0))'), '1')), "
                   "ST_AsText(ST_Simplify(ST_GeomFromText('GEOMETRYCOLLECTION EMPTY'), '1')), "
                   "ST_AsText(ST_Simplify(NULL, 0))",
            .column_count = sizeof(values) / sizeof(values[0]),
            .values = values,
            .row_count = 1U,
            .context = "scalar spatial simplify function",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_table_backed_spatial_simplify_function(void) {
    static const char *const projection_values[] = {
        "1",
        "LINESTRING(0 0,3 0)",
        "2",
        "POLYGON((2 2,0 2,0 0,2 0,2 2))",
        "3",
        "GEOMETRYCOLLECTION(POINT(1 2),LINESTRING(0 0,2 0))",
        "4",
        NULL,
    };
    static const char *const update_values[] = {
        "1",
        "LINESTRING(0 0,3 0)",
        "2",
        "POLYGON((2 2,0 2,0 0,2 0,2 2))",
        "3",
        "GEOMETRYCOLLECTION(POINT(1 2),LINESTRING(0 0,2 0))",
        "4",
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
    failures += execute_ok(database, "CREATE DATABASE spatial_simplify", NULL);
    failures += execute_ok(database, "USE spatial_simplify", NULL);
    failures += execute_ok(
        database,
        "CREATE TABLE spatial_values("
        "id INT PRIMARY KEY, g GEOMETRY, simplified GEOMETRY)",
        NULL
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO spatial_values(id, g) VALUES "
        "(1, ST_GeomFromText('LINESTRING(0 0,1 0.1,2 0,3 0)')), "
        "(2, ST_GeomFromText('POLYGON((0 0,1 0.1,2 0,2 2,0 2,0 0))')), "
        "(3, ST_GeomFromText('GEOMETRYCOLLECTION(POINT(1 2),LINESTRING(0 0,1 0.1,2 0))')), "
        "(4, ST_GeomFromText('POLYGON((0 0,0.1 0,0.1 0.1,0 0.1,0 0))'))",
        (struct expected_dml_result){
            .affected_rows = spatial_simplify_row_count,
            .warning_count = 0U,
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, ST_AsText(ST_Simplify(g, '0.2')) FROM spatial_values ORDER BY id",
            .column_count = 2U,
            .values = projection_values,
            .row_count = spatial_simplify_row_count,
            .context = "row-backed spatial simplify projection",
        }
    );
    failures += expect_dml_ok(
        database,
        "UPDATE spatial_values SET simplified = ST_Simplify(g, '0.2')",
        (struct expected_dml_result){
            .affected_rows = spatial_simplify_row_count - 1,
            .warning_count = 0U,
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, ST_AsText(simplified) FROM spatial_values ORDER BY id",
            .column_count = 2U,
            .values = update_values,
            .row_count = spatial_simplify_row_count,
            .context = "row-backed spatial simplify update",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_spatial_simplify_diagnostics(void) {
    mylite_db *database = NULL;
    int failures =
        expect_int(mylite_open(":memory:", &database), MYLITE_OK, "open diagnostics database");

    failures += execute_error(
        database,
        "SELECT ST_Simplify()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_parameter_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'st_simplify'",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_Simplify(Point(1,2), 0)",
        (struct expected_sql_error){
            .code = mysql_error_wrong_arguments,
            .sqlstate = "HY000",
            .message_part = "Incorrect arguments to st_simplify",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_Simplify(Point(1,2), 'abc')",
        (struct expected_sql_error){
            .code = mysql_error_wrong_arguments,
            .sqlstate = "HY000",
            .message_part = "Incorrect arguments to st_simplify",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_Simplify(X'010203', 1)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_gis_data,
            .sqlstate = "22023",
            .message_part = "Invalid GIS data provided to function st_simplify",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_AsText(ST_Simplify(ST_PointFromGeoHash('mh2n0p0581',4326), '1'))",
        (struct expected_sql_error){
            .code = mysql_error_not_implemented_for_geographic_srs,
            .sqlstate = "22S00",
            .message_part = "st_simplify(POINT, ...) has not been implemented for geographic "
                            "spatial reference systems",
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
                "%s: expected NULL at %zu,%zu, got [%s]\n",
                context,
                row,
                column,
                actual
            );
            return 1;
        }
        return 0;
    }
    return expect_text(actual, expected, context);
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
            "%s: expected [%s], got [%s]\n",
            context,
            expected == NULL ? "<null>" : expected,
            actual == NULL ? "<null>" : actual
        );
        return 1;
    }
    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || needle == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected [%s] to contain [%s]\n",
            context,
            actual == NULL ? "<null>" : actual,
            needle == NULL ? "<null>" : needle
        );
        return 1;
    }
    return 0;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_spatial_simplify_%ld_%s.mylite",
        (long)current_process_id(),
        name
    );

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "failed to format test database path\n");
        return 1;
    }
    return 0;
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
    char buffer[test_path_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(buffer)) {
        return;
    }
    (void)remove(buffer);
}
