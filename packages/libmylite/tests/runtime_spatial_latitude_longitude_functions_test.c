#include <mylite/mylite.h>

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
    latitude_longitude_projection_column_count = 5,
    mysql_error_native_function_parameter_count = 1582,
    mysql_error_invalid_gis_data = 3037,
    mysql_error_unexpected_geometry_type = 3516,
    mysql_error_srs_not_found = 3548,
    mysql_error_srs_not_geographic = 3726,
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

static int test_scalar_spatial_latitude_longitude_functions(void);
static int test_table_backed_spatial_latitude_longitude_functions(void);
static int test_spatial_latitude_longitude_metadata(void);
static int test_spatial_latitude_longitude_diagnostics(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query expected);
static int open_app_database(
    mylite_db **out_database,
    const char *name,
    char *path,
    size_t path_size
);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int expect_int(int actual, int expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);

int main(void) {
    int failures = 0;

    failures += test_scalar_spatial_latitude_longitude_functions();
    failures += test_table_backed_spatial_latitude_longitude_functions();
    failures += test_spatial_latitude_longitude_metadata();
    failures += test_spatial_latitude_longitude_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_scalar_spatial_latitude_longitude_functions(void) {
    static const char *const values[] = {"45", "90", "-90", "180", NULL, NULL};
    mylite_db *database = NULL;
    int failures = expect_int(mylite_open_memory(&database), MYLITE_OK, "open scalar database");

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ST_Latitude(ST_GeomFromGeoJSON('{\"type\":\"Point\","
                   "\"coordinates\":[90,45]}')), "
                   "ST_Longitude(ST_GeomFromGeoJSON('{\"type\":\"Point\","
                   "\"coordinates\":[90,45]}')), "
                   "ST_Latitude(ST_GeomFromGeoJSON('{\"type\":\"Point\","
                   "\"coordinates\":[180,-90]}')), "
                   "ST_Longitude(ST_GeomFromGeoJSON('{\"type\":\"Point\","
                   "\"coordinates\":[180,-90]}')), "
                   "ST_Latitude(NULL), ST_Longitude(NULL)",
            .column_count = sizeof(values) / sizeof(values[0]),
            .values = values,
            .row_count = 1U,
            .context = "scalar latitude longitude getters",
        }
    );
    mylite_close(database);
    return failures;
}

static int test_table_backed_spatial_latitude_longitude_functions(void) {
    static const char *const values[] = {
        "1",
        "45",
        "90",
        "POINT(45 90)",
        "4326",
        "2",
        "-20",
        "120",
        "POINT(-20 120)",
        "4326",
        "3",
        NULL,
        NULL,
        NULL,
        NULL,
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = open_app_database(&database, "table", path, sizeof(path));

    failures += execute_ok(database, "CREATE TABLE points(id INT PRIMARY KEY, p GEOMETRY)", NULL);
    failures += execute_ok(
        database,
        "INSERT INTO points VALUES "
        "(1, ST_GeomFromGeoJSON('{\"type\":\"Point\",\"coordinates\":[90,45]}')), "
        "(2, ST_GeomFromGeoJSON('{\"type\":\"Point\",\"coordinates\":[120,-20]}')), "
        "(3, NULL)",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, ST_Latitude(p), ST_Longitude(p), ST_AsText(p), ST_SRID(p) "
                   "FROM points ORDER BY id",
            .column_count = latitude_longitude_projection_column_count,
            .values = values,
            .row_count = 3U,
            .context = "table-backed latitude longitude getters",
        }
    );
    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_spatial_latitude_longitude_metadata(void) {
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = expect_int(mylite_open_memory(&database), MYLITE_OK, "open metadata database");

    failures += execute_ok(
        database,
        "SELECT ST_Latitude(ST_GeomFromGeoJSON('{\"type\":\"Point\",\"coordinates\":[90,45]}')), "
        "ST_Longitude(ST_GeomFromGeoJSON('{\"type\":\"Point\",\"coordinates\":[90,45]}'))",
        &result
    );
    if (failures == 0) {
        failures += expect_int(
            (int)mylite_result_column_type(result, 0U),
            MYLITE_RESULT_COLUMN_TYPE_DOUBLE,
            "ST_Latitude metadata type"
        );
        failures += expect_int(
            (int)mylite_result_column_type(result, 1U),
            MYLITE_RESULT_COLUMN_TYPE_DOUBLE,
            "ST_Longitude metadata type"
        );
    }
    mylite_result_free(result);
    mylite_close(database);
    return failures;
}

static int test_spatial_latitude_longitude_diagnostics(void) {
    mylite_db *database = NULL;
    int failures =
        expect_int(mylite_open_memory(&database), MYLITE_OK, "open diagnostics database");

    failures += execute_error(
        database,
        "SELECT ST_Latitude(Point(1,2))",
        (struct expected_sql_error){
            .code = mysql_error_srs_not_geographic,
            .sqlstate = "22S00",
            .message_part =
                "Function st_latitude is only defined for geographic spatial reference systems",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_Longitude(Point(1,2))",
        (struct expected_sql_error){
            .code = mysql_error_srs_not_geographic,
            .sqlstate = "22S00",
            .message_part =
                "Function st_longitude is only defined for geographic spatial reference systems",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_Latitude(ST_GeomFromText('LINESTRING(0 0,1 1)'))",
        (struct expected_sql_error){
            .code = mysql_error_unexpected_geometry_type,
            .sqlstate = "22S01",
            .message_part =
                "POINT value is a geometry of unexpected type LINESTRING in st_latitude",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_Longitude(ST_GeomFromText('LINESTRING(0 0,1 1)'))",
        (struct expected_sql_error){
            .code = mysql_error_unexpected_geometry_type,
            .sqlstate = "22S01",
            .message_part =
                "POINT value is a geometry of unexpected type LINESTRING in st_longitude",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_Latitude(X'00')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_gis_data,
            .sqlstate = "22023",
            .message_part = "Invalid GIS data provided to function st_latitude",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_Longitude(X'00')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_gis_data,
            .sqlstate = "22023",
            .message_part = "Invalid GIS data provided to function st_longitude",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_Latitude()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_parameter_count,
            .sqlstate = "42000",
            .message_part =
                "Incorrect parameter count in the call to native function 'st_latitude'",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_Longitude()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_parameter_count,
            .sqlstate = "42000",
            .message_part =
                "Incorrect parameter count in the call to native function 'st_longitude'",
        }
    );
    failures += execute_error(
        database,
        "SELECT ST_Latitude(ST_GeomFromGeoJSON('{\"type\":\"Point\","
        "\"coordinates\":[90,45]}',1,999999))",
        (struct expected_sql_error){
            .code = mysql_error_srs_not_found,
            .sqlstate = "SR001",
            .message_part = "There's no spatial reference system with SRID 999999.",
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

static int open_app_database(
    mylite_db **out_database,
    const char *name,
    char *path,
    size_t path_size
) {
    int failures = 0;

    if (make_test_path(path, path_size, name) != 0) {
        return 1;
    }
    remove_related_files(path);
    failures += expect_int(mylite_open(path, out_database), MYLITE_OK, name);
    if (failures == 0) {
        failures += execute_ok(*out_database, "CREATE DATABASE app", NULL);
    }
    if (failures == 0) {
        failures += execute_ok(*out_database, "USE app", NULL);
    }
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
        "/tmp/mylite_spatial_latitude_longitude_%s_%d.mylite",
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
