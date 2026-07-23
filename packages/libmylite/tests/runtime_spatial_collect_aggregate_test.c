#include "mylite_test_support.h"

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
    mysql_error_gis_different_srids = 4034,
    grouped_collection_row_count = 5,
    test_path_capacity = 1024,
    related_file_suffix_capacity = 16,
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

static int test_spatial_collect_results(void);
static int test_spatial_collect_diagnostics(void);
static int open_app_database(
    mylite_db **out_database,
    const char *name,
    char *path,
    size_t path_size
);
static int seed_spatial_collect_table(mylite_db *database);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_discard(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static void remove_related_files(const char *path);

int main(void) {
    int failures = 0;

    failures += test_spatial_collect_results();
    failures += test_spatial_collect_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_spatial_collect_results(void) {
    static const char *const point_values[] = {
        "MULTIPOINT((0 0),(1 1))",
        "MULTIPOINT",
        "0",
    };
    static const char *const distinct_values[] = {"MULTIPOINT((0 0),(1 1))"};
    static const char *const grouped_distinct_values[] = {
        "8",
        "MULTIPOINT((0 0),(1 1))",
    };
    static const char *const collection_input_values[] = {
        "9",
        "GEOMETRYCOLLECTION(MULTIPOINT((0 0),(1 1)))",
        "GEOMCOLLECTION",
        "10",
        "GEOMETRYCOLLECTION(GEOMETRYCOLLECTION(POINT(2 2),LINESTRING(0 0,1 1)))",
        "GEOMCOLLECTION",
    };
    static const char *const grouped_values[] = {
        "1",
        "MULTIPOINT((0 0),(1 1))",
        "MULTIPOINT",
        "2",
        "MULTILINESTRING((0 0,1 1),(2 2,3 3))",
        "MULTILINESTRING",
        "3",
        "MULTIPOLYGON(((0 0,1 0,1 1,0 0)),((2 2,3 2,3 3,2 2)))",
        "MULTIPOLYGON",
        "4",
        "GEOMETRYCOLLECTION(POINT(0 0),LINESTRING(0 0,1 1))",
        "GEOMCOLLECTION",
        "5",
        NULL,
        NULL,
    };
    static const char *const srid_values[] = {"MULTIPOINT((0 0))", "4326"};
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    char path[test_path_capacity];
    int failures = 0;

    failures += open_app_database(&database, "results", path, sizeof(path));
    failures += seed_spatial_collect_table(database);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ST_AsText(ST_Collect(g)), ST_GeometryType(ST_Collect(g)), "
                   "ST_SRID(ST_Collect(g)) FROM shapes WHERE grp = 1",
            .column_count = 3U,
            .values = point_values,
            .row_count = 1U,
            .context = "point collection wrappers",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ST_AsText(ST_Collect(DISTINCT g)) FROM shapes WHERE grp = 8",
            .column_count = 1U,
            .values = distinct_values,
            .row_count = 1U,
            .context = "distinct point collection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT grp, ST_AsText(ST_Collect(DISTINCT g)) "
                   "FROM shapes WHERE grp = 8 GROUP BY grp",
            .column_count = 2U,
            .values = grouped_distinct_values,
            .row_count = 1U,
            .context = "grouped distinct point collection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT grp, ST_AsText(ST_Collect(g)), ST_GeometryType(ST_Collect(g)) "
                   "FROM shapes WHERE grp IN (1,2,3,4,5) GROUP BY grp ORDER BY grp",
            .column_count = 3U,
            .values = grouped_values,
            .row_count = grouped_collection_row_count,
            .context = "grouped collection wrappers",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT grp, ST_AsText(ST_Collect(g)), ST_GeometryType(ST_Collect(g)) "
                   "FROM shapes WHERE grp IN (9,10) GROUP BY grp ORDER BY grp",
            .column_count = 3U,
            .values = collection_input_values,
            .row_count = 2U,
            .context = "multi and collection input wrappers",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ST_AsText(ST_Collect(g)), ST_SRID(ST_Collect(g)) "
                   "FROM shapes WHERE grp = 6",
            .column_count = 2U,
            .values = srid_values,
            .row_count = 1U,
            .context = "srid-preserving collection",
        }
    );

    failures += execute_ok(database, "SELECT ST_Collect(g) FROM shapes WHERE grp = 1", &result);
    if (result != NULL) {
        failures += mylite_test_expect_size(
            mylite_result_column_count(result),
            1U,
            "direct collect columns"
        );
        failures += mylite_test_expect_int(
            mylite_result_column_type(result, 0U),
            MYLITE_RESULT_COLUMN_TYPE_GEOMETRY,
            "direct collect column type"
        );
        failures +=
            mylite_test_expect_size(mylite_result_row_count(result), 1U, "direct collect rows");
        failures += mylite_test_expect_int(
            mylite_result_value_bytes(result, 0U, 0U) != NULL,
            1,
            "direct collect bytes"
        );
        mylite_result_free(result);
    }

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_spatial_collect_diagnostics(void) {
    mylite_db *database = NULL;
    char path[test_path_capacity];
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += seed_spatial_collect_table(database);
    failures += execute_error(
        database,
        "SELECT ST_AsText(ST_Collect(g)) FROM shapes WHERE grp = 7",
        (struct expected_sql_error){
            .code = mysql_error_gis_different_srids,
            .sqlstate = "22S05",
            .message_part = "different SRIDs: 4326 and 0",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int open_app_database(
    mylite_db **out_database,
    const char *name,
    char *path,
    size_t path_size
) {
    int failures = 0;

    if (mylite_test_make_path(path, path_size, name) != 0) {
        return 1;
    }
    remove_related_files(path);
    failures += mylite_test_expect_int(mylite_open(path, out_database), MYLITE_OK, name);
    if (failures == 0) {
        failures += execute_discard(*out_database, "CREATE DATABASE app");
    }
    if (failures == 0) {
        failures += execute_discard(*out_database, "USE app");
    }
    return failures;
}

static int seed_spatial_collect_table(mylite_db *database) {
    int failures = 0;

    failures += execute_discard(database, "CREATE TABLE shapes (grp INT, id INT, g GEOMETRY)");
    failures += execute_discard(
        database,
        "INSERT INTO shapes VALUES "
        "(1,1,Point(0,0)),"
        "(1,2,Point(1,1)),"
        "(1,3,NULL),"
        "(2,1,ST_GeomFromText('LINESTRING(0 0,1 1)')),"
        "(2,2,ST_GeomFromText('LINESTRING(2 2,3 3)')),"
        "(3,1,ST_GeomFromText('POLYGON((0 0,1 0,1 1,0 0))')),"
        "(3,2,ST_GeomFromText('POLYGON((2 2,3 2,3 3,2 2))')),"
        "(4,1,Point(0,0)),"
        "(4,2,ST_GeomFromText('LINESTRING(0 0,1 1)')),"
        "(5,1,NULL),"
        "(6,1,ST_GeomFromGeoJSON('{\"type\":\"Point\",\"coordinates\":[0,0]}')),"
        "(7,1,ST_GeomFromGeoJSON('{\"type\":\"Point\",\"coordinates\":[0,0]}')),"
        "(7,2,Point(1,1)),"
        "(8,1,Point(0,0)),"
        "(8,2,Point(0,0)),"
        "(8,3,Point(1,1)),"
        "(9,1,ST_GeomFromText('MULTIPOINT(0 0,1 1)')),"
        "(10,1,ST_GeomFromText('GEOMETRYCOLLECTION(POINT(2 2),LINESTRING(0 0,1 1))'))"
    );
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected success, got %d (%d %s %s)\n",
            sql,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        return 1;
    }
    return 0;
}

static int execute_discard(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    mylite_result_free(result);
    return failures;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

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

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, expected.sql, &result);

    if (failures != 0 || result == NULL) {
        mylite_result_free(result);
        return failures + 1;
    }
    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        expected.column_count,
        expected.sql
    );
    failures +=
        mylite_test_expect_size(mylite_result_row_count(result), expected.row_count, expected.sql);
    for (size_t row = 0U; row < expected.row_count; ++row) {
        for (size_t column = 0U; column < expected.column_count; ++column) {
            failures += expect_result_value(
                result,
                row,
                column,
                expected.values[(row * expected.column_count) + column],
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
    const char *actual = mylite_result_value_text(result, row, column);

    if (expected == NULL) {
        if (actual != NULL) {
            fprintf(stderr, "%s: expected NULL, got \"%s\"\n", context, actual);
            return 1;
        }
        return 0;
    }
    if (actual == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected \"%s\", got \"%s\"\n",
            context,
            expected,
            actual == NULL ? "NULL" : actual
        );
        return 1;
    }
    return 0;
}

static void remove_related_files(const char *path) {
    char suffix_path[test_path_capacity + related_file_suffix_capacity];
    static const char *const suffixes[] = {"", "-wal", "-shm"};

    for (size_t i = 0U; i < sizeof(suffixes) / sizeof(suffixes[0]); ++i) {
        int written = snprintf(suffix_path, sizeof(suffix_path), "%s%s", path, suffixes[i]);

        if (written >= 0 && (size_t)written < sizeof(suffix_path)) {
            remove(suffix_path);
        }
    }
}
