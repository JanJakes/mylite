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
    spatial_validity_row_count = 5,
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

static int test_scalar_spatial_validity_functions(void);
static int test_table_backed_spatial_validity_functions(void);
static int test_spatial_validity_diagnostics(void);
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

    failures += test_scalar_spatial_validity_functions();
    failures += test_table_backed_spatial_validity_functions();
    failures += test_spatial_validity_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_scalar_spatial_validity_functions(void) {
    static const char *const values[] = {
        NULL, NULL,
        "1",  "POINT(1 2)",
        "1",  "GEOMETRYCOLLECTION EMPTY",
        "1",  "LINESTRING(0 0,1 1)",
        "0",  NULL,
        "0",  NULL,
        "1",  "LINESTRING(0 0,0 0,1 1)",
        "1",  "LINESTRING(0 0,1 1,0 0)",
        "1",  "MULTIPOINT((0 0),(0 0))",
        "1",  "POLYGON((0 0,4 0,4 4,0 4,0 0))",
        "0",  NULL,
        "0",  NULL,
        "1",  "POLYGON((0 0,4 0,4 4,0 4,0 0),(1 1,2 1,2 2,1 1))",
        "0",  NULL,
        "0",  NULL,
        "0",  NULL,
        "1",  "GEOMETRYCOLLECTION(POINT(1 2),LINESTRING(0 0,1 1))",
        "0",  NULL,
    };
    mylite_db *database = NULL;
    int failures =
        mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open scalar database");

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ST_IsValid(NULL), ST_AsText(ST_Validate(NULL)), "
                   "ST_IsValid(Point(1,2)), ST_AsText(ST_Validate(Point(1,2))), "
                   "ST_IsValid(ST_GeomFromText('GEOMETRYCOLLECTION EMPTY')), "
                   "ST_AsText(ST_Validate(ST_GeomFromText('GEOMETRYCOLLECTION EMPTY'))), "
                   "ST_IsValid(ST_GeomFromText('LINESTRING(0 0,1 1)')), "
                   "ST_AsText(ST_Validate(ST_GeomFromText('LINESTRING(0 0,1 1)'))), "
                   "ST_IsValid(ST_GeomFromText('LINESTRING(0 0,0 0)')), "
                   "ST_AsText(ST_Validate(ST_GeomFromText('LINESTRING(0 0,0 0)'))), "
                   "ST_IsValid(ST_GeomFromText('LINESTRING(0 0,-0.00 0,0.0 0)')), "
                   "ST_AsText(ST_Validate(ST_GeomFromText('LINESTRING(0 0,-0.00 0,0.0 0)'))), "
                   "ST_IsValid(ST_GeomFromText('LINESTRING(0 0,0 0,1 1)')), "
                   "ST_AsText(ST_Validate(ST_GeomFromText('LINESTRING(0 0,0 0,1 1)'))), "
                   "ST_IsValid(ST_GeomFromText('LINESTRING(0 0,1 1,0 0)')), "
                   "ST_AsText(ST_Validate(ST_GeomFromText('LINESTRING(0 0,1 1,0 0)'))), "
                   "ST_IsValid(ST_GeomFromText('MULTIPOINT((0 0),(0 0))')), "
                   "ST_AsText(ST_Validate(ST_GeomFromText('MULTIPOINT((0 0),(0 0))'))), "
                   "ST_IsValid(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))')), "
                   "ST_AsText(ST_Validate(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))'))), "
                   "ST_IsValid(ST_GeomFromText('POLYGON((0 0,0 0,0 0,0 0,0 0))')), "
                   "ST_AsText(ST_Validate(ST_GeomFromText('POLYGON((0 0,0 0,0 0,0 0,0 0))'))), "
                   "ST_IsValid(ST_GeomFromText('POLYGON((0 0,2 2,2 0,0 2,0 0))')), "
                   "ST_AsText(ST_Validate(ST_GeomFromText('POLYGON((0 0,2 2,2 0,0 2,0 0))'))), "
                   "ST_IsValid(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0),"
                   "(1 1,2 1,2 2,1 1))')), "
                   "ST_AsText(ST_Validate(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0),"
                   "(1 1,2 1,2 2,1 1))'))), "
                   "ST_IsValid(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0),"
                   "(5 5,6 5,6 6,5 5))')), "
                   "ST_AsText(ST_Validate(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0),"
                   "(5 5,6 5,6 6,5 5))'))), "
                   "ST_IsValid(ST_GeomFromText('MULTILINESTRING((0 0,1 1),(2 2,2 2))')), "
                   "ST_AsText(ST_Validate(ST_GeomFromText('MULTILINESTRING((0 0,1 1),"
                   "(2 2,2 2))'))), "
                   "ST_IsValid(ST_GeomFromText('MULTIPOLYGON(((0 0,2 0,2 2,0 2,0 0)),"
                   "((1 1,3 1,3 3,1 3,1 1)))')), "
                   "ST_AsText(ST_Validate(ST_GeomFromText('MULTIPOLYGON(((0 0,2 0,2 2,0 2,0 0)),"
                   "((1 1,3 1,3 3,1 3,1 1)))'))), "
                   "ST_IsValid(ST_GeomFromText('GEOMETRYCOLLECTION(POINT(1 2),"
                   "LINESTRING(0 0,1 1))')), "
                   "ST_AsText(ST_Validate(ST_GeomFromText('GEOMETRYCOLLECTION(POINT(1 2),"
                   "LINESTRING(0 0,1 1))'))), "
                   "ST_IsValid(ST_GeomFromText('GEOMETRYCOLLECTION(POINT(1 2),"
                   "LINESTRING(0 0,-0.00 0,0.0 0))')), "
                   "ST_AsText(ST_Validate(ST_GeomFromText('GEOMETRYCOLLECTION(POINT(1 2),"
                   "LINESTRING(0 0,-0.00 0,0.0 0))')))",
            .column_count = sizeof(values) / sizeof(values[0]),
            .values = values,
            .row_count = 1U,
            .context = "scalar spatial validity functions",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_table_backed_spatial_validity_functions(void) {
    static const char *const projection_values[] = {
        "1",
        "1",
        "POINT(1 2)",
        "2",
        "0",
        NULL,
        "3",
        "0",
        NULL,
        "4",
        "1",
        "POLYGON((0 0,4 0,4 4,0 4,0 0))",
        "5",
        NULL,
        NULL,
    };
    static const char *const update_values[] = {
        "1",
        "POINT(1 2)",
        "2",
        NULL,
        "3",
        NULL,
        "4",
        "POLYGON((0 0,4 0,4 4,0 4,0 0))",
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
    failures += execute_ok(database, "CREATE DATABASE spatial_validity", NULL);
    failures += execute_ok(database, "USE spatial_validity", NULL);
    failures += execute_ok(
        database,
        "CREATE TABLE spatial_values(id INT PRIMARY KEY, g GEOMETRY, txt VARCHAR(120))",
        NULL
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO spatial_values VALUES "
        "(1, Point(1,2), NULL), "
        "(2, ST_GeomFromText('LINESTRING(0 0,0 0)'), NULL), "
        "(3, ST_GeomFromText('POLYGON((0 0,2 2,2 0,0 2,0 0))'), NULL), "
        "(4, ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))'), NULL), "
        "(5, NULL, NULL)",
        (struct expected_dml_result){
            .affected_rows = spatial_validity_row_count,
            .warning_count = 0U,
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, ST_IsValid(g), ST_AsText(ST_Validate(g)) "
                   "FROM spatial_values ORDER BY id",
            .column_count = 3U,
            .values = projection_values,
            .row_count = spatial_validity_row_count,
            .context = "row-backed spatial validity projection",
        }
    );
    failures += expect_dml_ok(
        database,
        "UPDATE spatial_values SET txt = ST_AsText(ST_Validate(g))",
        (struct expected_dml_result){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, txt FROM spatial_values ORDER BY id",
            .column_count = 2U,
            .values = update_values,
            .row_count = spatial_validity_row_count,
            .context = "row-backed spatial validity update",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_spatial_validity_diagnostics(void) {
    mylite_db *database = NULL;
    int failures = mylite_test_expect_int(
        mylite_open_memory(&database),
        MYLITE_OK,
        "open diagnostics database"
    );

    failures += execute_error(
        database,
        "SELECT ST_IsValid()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_parameter_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'st_isvalid'",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_AsText(ST_Validate(X'010203'))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_gis_data,
            .sqlstate = "22023",
            .message_part = "Invalid GIS data provided to function st_validate",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_IsValid(ST_GeomFromText('LINESTRING(0 0)'))",
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
