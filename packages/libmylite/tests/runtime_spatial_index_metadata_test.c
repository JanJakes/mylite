#include <mylite/mylite.h>

#include "runtime/mylite_connection.h"
#include "runtime/mylite_diagnostics.h"
#include "sqlite3.h"
#include "storage/mylite_file_format.h"

#include <stdint.h>
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
    test_path_suffix_capacity = 16,
    spatial_column_count = 8,
    show_columns_field_count = 6,
    show_index_field_count = 15,
    information_schema_columns_field_count = 7,
    information_schema_statistics_field_count = 10,
    mysql_error_duplicate_key_name = 1061,
    mysql_error_parse = 1064,
    mysql_error_invalid_default = 1067,
    mysql_error_too_many_key_parts = 1070,
    mysql_error_key_column_does_not_exist = 1072,
    mysql_error_incorrect_prefix_key = 1089,
    mysql_error_blob_text_cant_have_default = 1101,
    mysql_error_wrong_usage = 1221,
    mysql_error_spatial_must_be_not_null = 1252,
    mysql_error_no_default = 1364,
    mysql_error_spatial_index_non_geometric = 1687,
    mysql_error_spatial_column_cannot_be_null = 3673,
    mysql_error_spatial_unique = 3728,
    mysql_error_spatial_index_type_not_supported = 3729,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_query {
    const char *sql;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    const char *context;
};

static int test_spatial_metadata_surface(void);
static int test_spatial_added_and_implicit_index_forms(void);
static int test_spatial_index_type_options(void);
static int test_spatial_create_table_like_metadata(void);
static int test_spatial_null_dml_and_result_metadata(void);
static int test_spatial_diagnostics(void);
static int test_spatial_file_persistence(void);
static int test_spatial_independent_handles(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_statement_ok_with_warning_count(
    mylite_db *database,
    const char *sql,
    size_t warning_count
);
static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_spatial_result_metadata(mylite_db *database, const char *sql);
static int expect_physical_index_count(
    mylite_db *database,
    int expected_count,
    const char *context
);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_spatial_metadata_surface();
    failures += test_spatial_added_and_implicit_index_forms();
    failures += test_spatial_index_type_options();
    failures += test_spatial_create_table_like_metadata();
    failures += test_spatial_null_dml_and_result_metadata();
    failures += test_spatial_diagnostics();
    failures += test_spatial_file_persistence();
    failures += test_spatial_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_spatial_metadata_surface(void) {
    static const char show_create[] =
        "CREATE TABLE `spatial_meta` (\n"
        "  `g` geometry DEFAULT NULL,\n"
        "  `p` point NOT NULL,\n"
        "  `l` linestring DEFAULT NULL,\n"
        "  `poly` polygon DEFAULT NULL,\n"
        "  `mp` multipoint DEFAULT NULL,\n"
        "  `ml` multilinestring DEFAULT NULL,\n"
        "  `mpoly` multipolygon DEFAULT NULL,\n"
        "  `gc` geomcollection DEFAULT NULL,\n"
        "  SPATIAL KEY `sp` (`p`) COMMENT 'geo' /*!80000 INVISIBLE */\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci";
    static const char *const show_create_rows[] = {"spatial_meta", show_create};
    static const char *const show_columns_rows[] = {
        "g",     "geometry",        "YES", "",    NULL, "",
        "p",     "point",           "NO",  "MUL", NULL, "",
        "l",     "linestring",      "YES", "",    NULL, "",
        "poly",  "polygon",         "YES", "",    NULL, "",
        "mp",    "multipoint",      "YES", "",    NULL, "",
        "ml",    "multilinestring", "YES", "",    NULL, "",
        "mpoly", "multipolygon",    "YES", "",    NULL, "",
        "gc",    "geomcollection",  "YES", "",    NULL, "",
    };
    static const char *const show_index_rows[] = {
        "spatial_meta",
        "1",
        "sp",
        "1",
        "p",
        "A",
        "0",
        "32",
        NULL,
        "",
        "SPATIAL",
        "",
        "geo",
        "NO",
        NULL,
    };
    static const char *const information_schema_columns_rows[] = {
        "g",     "geometry",        "geometry",        "YES", NULL, "",    NULL,
        "p",     "point",           "point",           "NO",  NULL, "MUL", NULL,
        "l",     "linestring",      "linestring",      "YES", NULL, "",    NULL,
        "poly",  "polygon",         "polygon",         "YES", NULL, "",    NULL,
        "mp",    "multipoint",      "multipoint",      "YES", NULL, "",    NULL,
        "ml",    "multilinestring", "multilinestring", "YES", NULL, "",    NULL,
        "mpoly", "multipolygon",    "multipolygon",    "YES", NULL, "",    NULL,
        "gc",    "geomcollection",  "geomcollection",  "YES", NULL, "",    NULL,
    };
    static const char *const information_schema_statistics_rows[] = {
        "sp",
        "1",
        "1",
        "p",
        "A",
        "32",
        "",
        "SPATIAL",
        "geo",
        "NO",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        expect_int(mylite_open(":memory:", &database), MYLITE_OK, "open transient database");
    if (failures != 0) {
        return failures;
    }

    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok_with_warning_count(
        database,
        "CREATE TABLE spatial_meta ("
        "g GEOMETRY, p POINT NOT NULL, l LINESTRING, poly POLYGON, mp MULTIPOINT, "
        "ml MULTILINESTRING, mpoly MULTIPOLYGON, gc GEOMETRYCOLLECTION, "
        "SPATIAL KEY sp (p) COMMENT 'geo' INVISIBLE)",
        1U
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE spatial_meta",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "spatial SHOW CREATE TABLE",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM spatial_meta",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = spatial_column_count,
            .context = "spatial SHOW COLUMNS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM spatial_meta",
            .values = show_index_rows,
            .column_count = show_index_field_count,
            .row_count = 1U,
            .context = "spatial SHOW INDEX",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, DATA_TYPE, COLUMN_TYPE, IS_NULLABLE, COLUMN_DEFAULT, "
                   "COLUMN_KEY, SRS_ID FROM information_schema.columns "
                   "WHERE table_schema = DATABASE() AND table_name = 'spatial_meta' "
                   "ORDER BY ORDINAL_POSITION",
            .values = information_schema_columns_rows,
            .column_count = information_schema_columns_field_count,
            .row_count = spatial_column_count,
            .context = "spatial INFORMATION_SCHEMA.COLUMNS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME, COLLATION, "
                   "SUB_PART, NULLABLE, INDEX_TYPE, INDEX_COMMENT, IS_VISIBLE "
                   "FROM information_schema.statistics WHERE table_schema = DATABASE() "
                   "AND table_name = 'spatial_meta' ORDER BY INDEX_NAME",
            .values = information_schema_statistics_rows,
            .column_count = information_schema_statistics_field_count,
            .row_count = 1U,
            .context = "spatial INFORMATION_SCHEMA.STATISTICS",
        }
    );
    failures += expect_physical_index_count(database, 0, "spatial catalog-only index");

    mylite_close(database);
    return failures;
}

static int test_spatial_added_and_implicit_index_forms(void) {
    static const char *const explicit_rows[] = {
        "sg",
        "SPATIAL",
        "sp",
        "SPATIAL",
    };
    static const char *const implicit_rows[] = {"kg", "SPATIAL"};
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        expect_int(mylite_open(":memory:", &database), MYLITE_OK, "open transient database");
    if (failures != 0) {
        return failures;
    }

    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE alter_spatial (g GEOMETRY NOT NULL, p POINT NOT NULL)"
    );
    failures += expect_statement_ok_with_warning_count(
        database,
        "ALTER TABLE alter_spatial ADD SPATIAL INDEX sg (g)",
        1U
    );
    failures += expect_statement_ok_with_warning_count(
        database,
        "CREATE SPATIAL INDEX sp ON alter_spatial (p)",
        1U
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT INDEX_NAME, INDEX_TYPE FROM information_schema.statistics "
                   "WHERE table_schema = DATABASE() AND table_name = 'alter_spatial' "
                   "ORDER BY INDEX_NAME",
            .values = explicit_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "explicit spatial index forms",
        }
    );

    failures += expect_statement_ok_with_warning_count(
        database,
        "CREATE TABLE implicit_spatial (g GEOMETRY NOT NULL, KEY kg (g))",
        1U
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT INDEX_NAME, INDEX_TYPE FROM information_schema.statistics "
                   "WHERE table_schema = DATABASE() AND table_name = 'implicit_spatial'",
            .values = implicit_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "implicit spatial index form",
        }
    );
    failures +=
        expect_statement_ok(database, "ALTER TABLE implicit_spatial RENAME INDEX kg TO renamed");
    failures += expect_statement_ok(database, "ALTER TABLE implicit_spatial DROP INDEX renamed");
    failures += expect_physical_index_count(database, 0, "spatial add/drop catalog-only indexes");

    mylite_close(database);
    return failures;
}

static int test_spatial_index_type_options(void) {
    static const char rtree_leading_show_create[] =
        "CREATE TABLE `rtree_leading` (\n"
        "  `g` geometry NOT NULL,\n"
        "  SPATIAL KEY `kg` (`g`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci";
    static const char rtree_trailing_show_create[] =
        "CREATE TABLE `rtree_trailing` (\n"
        "  `g` geometry NOT NULL,\n"
        "  SPATIAL KEY `kg` (`g`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci";
    static const char *const rtree_leading_rows[] = {
        "rtree_leading",
        rtree_leading_show_create,
    };
    static const char *const rtree_trailing_rows[] = {
        "rtree_trailing",
        rtree_trailing_show_create,
    };
    static const char *const rtree_added_rows[] = {
        "kg",
        "SPATIAL",
        "g",
        "kp",
        "SPATIAL",
        "p",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        expect_int(mylite_open(":memory:", &database), MYLITE_OK, "open transient database");
    if (failures != 0) {
        return failures;
    }

    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok_with_warning_count(
        database,
        "CREATE TABLE rtree_leading (g GEOMETRY NOT NULL, KEY kg USING RTREE (g))",
        1U
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE rtree_leading",
            .values = rtree_leading_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "leading RTREE spatial SHOW CREATE TABLE",
        }
    );
    failures += expect_statement_ok_with_warning_count(
        database,
        "CREATE TABLE rtree_trailing (g GEOMETRY NOT NULL, KEY kg (g) USING RTREE)",
        1U
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE rtree_trailing",
            .values = rtree_trailing_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "trailing RTREE spatial SHOW CREATE TABLE",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE rtree_added (g GEOMETRY NOT NULL, p POINT NOT NULL)"
    );
    failures += expect_statement_ok_with_warning_count(
        database,
        "ALTER TABLE rtree_added ADD INDEX kg USING RTREE (g)",
        1U
    );
    failures += expect_statement_ok_with_warning_count(
        database,
        "CREATE INDEX kp ON rtree_added (p) USING RTREE",
        1U
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT INDEX_NAME, INDEX_TYPE, COLUMN_NAME "
                   "FROM information_schema.statistics WHERE table_schema = DATABASE() "
                   "AND table_name = 'rtree_added' ORDER BY INDEX_NAME",
            .values = rtree_added_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "RTREE spatial metadata",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE ordinary_btree_spatial (g GEOMETRY NOT NULL, KEY kg USING BTREE (g))",
        (struct expected_sql_error){
            .code = mysql_error_spatial_index_type_not_supported,
            .sqlstate = "HY000",
            .message_part = "The index type BTREE is not supported for spatial indexes.",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE ordinary_hash_spatial (g GEOMETRY NOT NULL, KEY kg (g) USING HASH)",
        (struct expected_sql_error){
            .code = mysql_error_spatial_index_type_not_supported,
            .sqlstate = "HY000",
            .message_part = "The index type HASH is not supported for spatial indexes.",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE ordinary_rtree_nonspatial (id INT NOT NULL, KEY kid USING RTREE (id))",
        (struct expected_sql_error){
            .code = mysql_error_spatial_index_non_geometric,
            .sqlstate = "42000",
            .message_part = "A SPATIAL index may only contain a geometrical type column",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE unique_rtree_spatial (g GEOMETRY NOT NULL, UNIQUE KEY ug USING RTREE (g))",
        (struct expected_sql_error){
            .code = mysql_error_spatial_unique,
            .sqlstate = "HY000",
            .message_part = "Spatial indexes can't be primary or unique indexes.",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE explicit_spatial_rtree (g GEOMETRY NOT NULL, SPATIAL KEY sg (g) "
        "USING RTREE)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures +=
        expect_statement_ok(database, "CREATE TABLE standalone_rtree (g GEOMETRY NOT NULL)");
    failures += execute_error(
        database,
        "ALTER TABLE standalone_rtree ADD INDEX bad_btree USING BTREE (g)",
        (struct expected_sql_error){
            .code = mysql_error_spatial_index_type_not_supported,
            .sqlstate = "HY000",
            .message_part = "The index type BTREE is not supported for spatial indexes.",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE standalone_rtree ADD INDEX bad_hash (g) USING HASH",
        (struct expected_sql_error){
            .code = mysql_error_spatial_index_type_not_supported,
            .sqlstate = "HY000",
            .message_part = "The index type HASH is not supported for spatial indexes.",
        }
    );
    failures += execute_error(
        database,
        "CREATE INDEX bad_create_btree USING BTREE ON standalone_rtree (g)",
        (struct expected_sql_error){
            .code = mysql_error_spatial_index_type_not_supported,
            .sqlstate = "HY000",
            .message_part = "The index type BTREE is not supported for spatial indexes.",
        }
    );
    failures += execute_error(
        database,
        "CREATE INDEX bad_create_hash ON standalone_rtree (g) USING HASH",
        (struct expected_sql_error){
            .code = mysql_error_spatial_index_type_not_supported,
            .sqlstate = "HY000",
            .message_part = "The index type HASH is not supported for spatial indexes.",
        }
    );
    failures +=
        expect_statement_ok(database, "CREATE TABLE standalone_nonspatial (id INT NOT NULL)");
    failures += execute_error(
        database,
        "ALTER TABLE standalone_nonspatial ADD INDEX bad_rtree USING RTREE (id)",
        (struct expected_sql_error){
            .code = mysql_error_spatial_index_non_geometric,
            .sqlstate = "42000",
            .message_part = "A SPATIAL index may only contain a geometrical type column",
        }
    );
    failures += execute_error(
        database,
        "CREATE INDEX bad_create_rtree USING RTREE ON standalone_nonspatial (id)",
        (struct expected_sql_error){
            .code = mysql_error_spatial_index_non_geometric,
            .sqlstate = "42000",
            .message_part = "A SPATIAL index may only contain a geometrical type column",
        }
    );
    failures += execute_error(
        database,
        "CREATE SPATIAL INDEX sg ON standalone_rtree (g) USING RTREE",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures += expect_physical_index_count(database, 0, "RTREE spatial catalog-only indexes");

    mylite_close(database);
    return failures;
}

static int test_spatial_create_table_like_metadata(void) {
    static const char show_create[] =
        "CREATE TABLE `spatial_like_clone` (\n"
        "  `g` geometry DEFAULT NULL,\n"
        "  `p` point NOT NULL,\n"
        "  SPATIAL KEY `sp` (`p`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci";
    static const char *const show_create_rows[] = {"spatial_like_clone", show_create};
    static const char *const index_rows[] = {"spatial_like_clone", "sp", "p", "SPATIAL"};
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        expect_int(mylite_open(":memory:", &database), MYLITE_OK, "open transient database");
    if (failures != 0) {
        return failures;
    }

    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok_with_warning_count(
        database,
        "CREATE TABLE spatial_like_source (g GEOMETRY, p POINT NOT NULL, SPATIAL KEY sp (p))",
        1U
    );
    failures +=
        expect_statement_ok(database, "CREATE TABLE spatial_like_clone LIKE spatial_like_source");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE spatial_like_clone",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "spatial CREATE TABLE LIKE SHOW CREATE TABLE",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, INDEX_NAME, COLUMN_NAME, INDEX_TYPE "
                   "FROM information_schema.statistics "
                   "WHERE table_schema = DATABASE() AND table_name = 'spatial_like_clone'",
            .values = index_rows,
            .column_count = 4U,
            .row_count = 1U,
            .context = "spatial CREATE TABLE LIKE index metadata",
        }
    );
    failures +=
        expect_physical_index_count(database, 0, "spatial CREATE TABLE LIKE catalog-only indexes");

    mylite_close(database);
    return failures;
}

static int test_spatial_null_dml_and_result_metadata(void) {
    static const char *const rows[] = {"1", NULL, "2", NULL, "3", NULL};
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        expect_int(mylite_open(":memory:", &database), MYLITE_OK, "open transient database");
    if (failures != 0) {
        return failures;
    }

    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(database, "CREATE TABLE nullable_spatial (id INT, g GEOMETRY)");
    failures += expect_dml_ok(database, "INSERT INTO nullable_spatial (id) VALUES (1)", 1);
    failures += expect_dml_ok(database, "INSERT INTO nullable_spatial VALUES (2, DEFAULT)", 1);
    failures += expect_dml_ok(database, "INSERT INTO nullable_spatial VALUES (3, NULL)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, g FROM nullable_spatial ORDER BY id",
            .values = rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "nullable spatial null DML",
        }
    );
    failures += expect_spatial_result_metadata(
        database,
        "SELECT g FROM nullable_spatial ORDER BY id LIMIT 1"
    );
    failures += execute_error(
        database,
        "INSERT INTO nullable_spatial VALUES (4, 'POINT(1 2)')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "Spatial DML supports only NULL or DEFAULT values",
        }
    );
    failures += execute_error(
        database,
        "UPDATE nullable_spatial SET g = 'POINT(1 2)' WHERE id = 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "Spatial DML supports only NULL or DEFAULT values",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_spatial_diagnostics(void) {
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        expect_int(mylite_open(":memory:", &database), MYLITE_OK, "open transient database");
    if (failures != 0) {
        return failures;
    }

    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += execute_error(
        database,
        "CREATE TABLE spatial_nullable_index (g GEOMETRY, SPATIAL KEY sg (g))",
        (struct expected_sql_error){
            .code = mysql_error_spatial_must_be_not_null,
            .sqlstate = "42000",
            .message_part = "All parts of a SPATIAL index must be NOT NULL",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE spatial_nonspatial (id INT NOT NULL)");
    failures += execute_error(
        database,
        "CREATE SPATIAL INDEX sid ON spatial_nonspatial (id)",
        (struct expected_sql_error){
            .code = mysql_error_spatial_index_non_geometric,
            .sqlstate = "42000",
            .message_part = "A SPATIAL index may only contain a geometrical type column",
        }
    );
    failures += execute_error(
        database,
        "CREATE SPATIAL INDEX smissing ON spatial_nonspatial (missing)",
        (struct expected_sql_error){
            .code = mysql_error_key_column_does_not_exist,
            .sqlstate = "42000",
            .message_part = "Key column 'missing' doesn't exist in table",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE spatial_multi (g GEOMETRY NOT NULL, p POINT NOT NULL, SPATIAL KEY sm (g, p))",
        (struct expected_sql_error){
            .code = mysql_error_too_many_key_parts,
            .sqlstate = "42000",
            .message_part = "Too many key parts specified",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE spatial_prefix (g GEOMETRY NOT NULL, SPATIAL KEY sprefix (g(4)))",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_prefix_key,
            .sqlstate = "HY000",
            .message_part = "Incorrect prefix key",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE spatial_order (g GEOMETRY NOT NULL, SPATIAL KEY sorder (g DESC))",
        (struct expected_sql_error){
            .code = mysql_error_wrong_usage,
            .sqlstate = "HY000",
            .message_part = "Incorrect usage of spatial/fulltext/hash index",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE unique_spatial (g GEOMETRY NOT NULL, UNIQUE KEY ug (g))",
        (struct expected_sql_error){
            .code = mysql_error_spatial_unique,
            .sqlstate = "HY000",
            .message_part = "Spatial indexes can't be primary or unique indexes.",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE inline_unique_spatial (g GEOMETRY UNIQUE)",
        (struct expected_sql_error){
            .code = mysql_error_spatial_unique,
            .sqlstate = "HY000",
            .message_part = "Spatial indexes can't be primary or unique indexes.",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE primary_spatial (g GEOMETRY NOT NULL, PRIMARY KEY (g))",
        (struct expected_sql_error){
            .code = mysql_error_spatial_unique,
            .sqlstate = "HY000",
            .message_part = "Spatial indexes can't be primary or unique indexes.",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE inline_primary_spatial (g GEOMETRY PRIMARY KEY)",
        (struct expected_sql_error){
            .code = mysql_error_spatial_unique,
            .sqlstate = "HY000",
            .message_part = "Spatial indexes can't be primary or unique indexes.",
        }
    );
    failures += expect_statement_ok_with_warning_count(
        database,
        "CREATE TABLE spatial_duplicate (g GEOMETRY NOT NULL, SPATIAL KEY sg (g))",
        1U
    );
    failures += execute_error(
        database,
        "ALTER TABLE spatial_duplicate ADD SPATIAL INDEX sg (g)",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key_name,
            .sqlstate = "42000",
            .message_part = "Duplicate key name 'sg'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TEMPORARY TABLE tmp_spatial (g GEOMETRY NOT NULL, SPATIAL KEY sg (g))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "Temporary SPATIAL indexes are not yet supported",
        }
    );
    failures += execute_error(
        database,
        "CREATE TEMPORARY TABLE tmp_implicit_spatial (g GEOMETRY NOT NULL, KEY kg (g))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "Temporary SPATIAL indexes are not yet supported",
        }
    );
    failures += execute_error(
        database,
        "CREATE TEMPORARY TABLE tmp_spatial_column (g GEOMETRY)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "Temporary SPATIAL columns are not yet supported",
        }
    );
    failures += expect_statement_ok_with_warning_count(
        database,
        "CREATE TABLE tmp_like_source (g GEOMETRY NOT NULL, SPATIAL KEY sg (g))",
        1U
    );
    failures += execute_error(
        database,
        "CREATE TEMPORARY TABLE tmp_like_clone LIKE tmp_like_source",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "Temporary SPATIAL indexes are not yet supported",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE tmp_like_column_source (g GEOMETRY)");
    failures += execute_error(
        database,
        "CREATE TEMPORARY TABLE tmp_like_column_clone LIKE tmp_like_column_source",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "Temporary SPATIAL columns are not yet supported",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE spatial_alter_scope (id INT)");
    failures += execute_error(
        database,
        "ALTER TABLE spatial_alter_scope ADD COLUMN g GEOMETRY",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "ALTER TABLE ADD COLUMN does not yet support spatial columns",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE spatial_alter_scope MODIFY COLUMN id GEOMETRY",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "ALTER TABLE MODIFY COLUMN supports only compatible baseline integer, character, "
                "text, binary string, and temporal column replacements",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE spatial_alter_scope CHANGE COLUMN id g GEOMETRY",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "ALTER TABLE CHANGE COLUMN supports only compatible baseline integer, character, "
                "text, binary string, and temporal column replacements",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE spatial_default (g GEOMETRY DEFAULT 'x')",
        (struct expected_sql_error){
            .code = mysql_error_blob_text_cant_have_default,
            .sqlstate = "42000",
            .message_part = "BLOB, TEXT, GEOMETRY or JSON column 'g' can't have a default value",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE spatial_not_null_default (g GEOMETRY NOT NULL DEFAULT NULL)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'g'",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE spatial_missing_default (id INT, g GEOMETRY NOT NULL)"
    );
    failures += execute_error(
        database,
        "INSERT INTO spatial_missing_default (id) VALUES (1)",
        (struct expected_sql_error){
            .code = mysql_error_no_default,
            .sqlstate = "HY000",
            .message_part = "Field 'g' doesn't have a default value",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE spatial_not_null_values (id INT, g GEOMETRY NOT NULL)"
    );
    failures += execute_error(
        database,
        "INSERT INTO spatial_not_null_values VALUES (1, NULL)",
        (struct expected_sql_error){
            .code = mysql_error_spatial_column_cannot_be_null,
            .sqlstate = "23000",
            .message_part = "Column 'g' cannot be null",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_spatial_file_persistence(void) {
    static const char *const rows[] = {"1", NULL, "2", NULL};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = make_test_path(path, sizeof(path), "runtime_spatial_index_metadata");

    if (failures != 0) {
        return failures;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open file");
    if (failures == 0) {
        failures += expect_statement_ok(database, "CREATE DATABASE app");
        failures += expect_statement_ok(database, "USE app");
    }
    if (failures == 0) {
        failures += expect_statement_ok_with_warning_count(
            database,
            "CREATE TABLE persisted_index (id INT, p POINT NOT NULL, SPATIAL KEY sp (p))",
            1U
        );
        failures +=
            expect_statement_ok(database, "CREATE TABLE persisted_rows (id INT, g GEOMETRY)");
        failures += expect_dml_ok(database, "INSERT INTO persisted_rows (id) VALUES (1)", 1);
    }
    mylite_close(database);
    database = NULL;

    if (failures == 0) {
        failures += read_file_at(path, 0L, preamble, sizeof(preamble));
        failures += expect_bytes(
            preamble,
            expected_preamble,
            sizeof(expected_preamble),
            "spatial file preamble"
        );
        failures += expect_int(
            mylite_file_preamble_validate(preamble),
            1,
            "spatial file preamble validation"
        );
    }
    if (failures == 0) {
        failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen file");
    }
    if (failures == 0) {
        failures += expect_statement_ok(database, "USE app");
    }
    if (failures == 0) {
        failures += expect_dml_ok(database, "INSERT INTO persisted_rows (id) VALUES (2)", 1);
        failures += expect_query_values(
            database,
            (struct expected_query){
                .sql = "SELECT id, g FROM persisted_rows ORDER BY id",
                .values = rows,
                .column_count = 2U,
                .row_count = 2U,
                .context = "spatial file persistence",
            }
        );
    }

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_spatial_independent_handles(void) {
    static const char *const first_rows[] = {"1"};
    static const char *const second_rows[] = {"2"};
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    failures += expect_int(mylite_open(":memory:", &first), MYLITE_OK, "open first");
    failures += expect_int(mylite_open(":memory:", &second), MYLITE_OK, "open second");
    if (failures == 0) {
        failures += expect_statement_ok(first, "CREATE DATABASE app");
        failures += expect_statement_ok(first, "USE app");
        failures += expect_statement_ok(second, "CREATE DATABASE app");
        failures += expect_statement_ok(second, "USE app");
        failures += expect_statement_ok(first, "CREATE TABLE t (id INT, g GEOMETRY)");
        failures += expect_statement_ok(second, "CREATE TABLE t (id INT, g GEOMETRY)");
        failures += expect_dml_ok(first, "INSERT INTO t (id) VALUES (1)", 1);
        failures += expect_dml_ok(second, "INSERT INTO t (id) VALUES (2)", 1);
        failures += expect_query_values(
            first,
            (struct expected_query){
                .sql = "SELECT id FROM t",
                .values = first_rows,
                .column_count = 1U,
                .row_count = 1U,
                .context = "first spatial handle",
            }
        );
        failures += expect_query_values(
            second,
            (struct expected_query){
                .sql = "SELECT id FROM t",
                .values = second_rows,
                .column_count = 1U,
                .row_count = 1U,
                .context = "second spatial handle",
            }
        );
    }

    mylite_close(first);
    mylite_close(second);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        const struct mylite_diagnostics *diagnostics = mylite_connection_diagnostics(database);

        fprintf(
            stderr,
            "expected success for [%s], got rc=%d err=%d state=%s message=%s\n",
            sql,
            rc,
            mylite_diagnostics_errcode(diagnostics),
            mylite_diagnostics_sqlstate(diagnostics),
            mylite_diagnostics_errmsg(diagnostics)
        );
        return 1;
    }

    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    const struct mylite_diagnostics *diagnostics = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc == MYLITE_OK) {
        fprintf(stderr, "expected error for [%s]\n", sql);
        mylite_result_free(result);
        return 1;
    }
    diagnostics = mylite_connection_diagnostics(database);
    failures += expect_int(mylite_diagnostics_errcode(diagnostics), expected.code, sql);
    failures += expect_text(mylite_diagnostics_sqlstate(diagnostics), expected.sqlstate, sql);
    failures += expect_contains(mylite_diagnostics_errmsg(diagnostics), expected.message_part, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, sql);
        failures += expect_size(mylite_result_row_count(result), 0U, sql);
        failures += expect_size(mylite_result_warning_count(result), 0U, sql);
    }
    mylite_result_free(result);
    return failures;
}

static int expect_statement_ok_with_warning_count(
    mylite_db *database,
    const char *sql,
    size_t warning_count
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, sql);
        failures += expect_size(mylite_result_row_count(result), 0U, sql);
        failures += expect_int64(mylite_result_affected_rows(result), 0, sql);
        failures += expect_size(mylite_result_warning_count(result), warning_count, sql);
    }
    mylite_result_free(result);
    return failures;
}

static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, sql);
        failures += expect_size(mylite_result_row_count(result), 0U, sql);
        failures += expect_int64(mylite_result_affected_rows(result), affected_rows, sql);
        failures += expect_size(mylite_result_warning_count(result), 0U, sql);
    }
    mylite_result_free(result);
    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    if (failures == 0) {
        failures +=
            expect_size(mylite_result_column_count(result), query.column_count, query.context);
        failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
    }
    for (size_t row = 0U; failures == 0 && row < query.row_count; ++row) {
        for (size_t column = 0U; column < query.column_count; ++column) {
            size_t value_index = (row * query.column_count) + column;
            const char *actual = mylite_result_value_text(result, row, column);
            const char *expected = query.values[value_index];

            if (expected == NULL) {
                if (actual != NULL) {
                    fprintf(stderr, "%s: expected NULL, got [%s]\n", query.context, actual);
                    ++failures;
                }
            } else {
                failures += expect_text(actual, expected, query.context);
            }
        }
    }

    mylite_result_free(result);
    return failures;
}

static int expect_spatial_result_metadata(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 1U, sql);
        failures += expect_size(mylite_result_row_count(result), 1U, sql);
        failures += expect_int(
            (int)mylite_result_column_type(result, 0U),
            MYLITE_RESULT_COLUMN_TYPE_GEOMETRY,
            sql
        );
        failures += expect_int(
            (int)(mylite_result_column_flags(result, 0U) & MYLITE_RESULT_COLUMN_FLAG_BLOB),
            MYLITE_RESULT_COLUMN_FLAG_BLOB,
            sql
        );
        if (mylite_result_value_text(result, 0U, 0U) != NULL) {
            fprintf(stderr, "%s: expected NULL spatial value\n", sql);
            ++failures;
        }
    }

    mylite_result_free(result);
    return failures;
}

static int expect_physical_index_count(
    mylite_db *database,
    int expected_count,
    const char *context
) {
    sqlite3 *connection = mylite_connection_sqlite_for_test(database);
    sqlite3_stmt *statement = NULL;
    int actual_count = 0;
    int rc = SQLITE_OK;

    if (connection == NULL) {
        fprintf(stderr, "%s: missing SQLite test connection\n", context);
        return 1;
    }

    rc = sqlite3_prepare_v2(
        connection,
        "SELECT count(*) FROM sqlite_schema "
        "WHERE type = 'index' AND name GLOB '_mylite_user_index_*'",
        -1,
        &statement,
        NULL
    );
    if (rc != SQLITE_OK) {
        fprintf(stderr, "%s: prepare physical index query failed: %d\n", context, rc);
        return 1;
    }

    rc = sqlite3_step(statement);
    if (rc == SQLITE_ROW) {
        actual_count = sqlite3_column_int(statement, 0);
        rc = SQLITE_OK;
    }
    if (statement != NULL && sqlite3_finalize(statement) != SQLITE_OK && rc == SQLITE_OK) {
        rc = SQLITE_ERROR;
    }
    if (rc != SQLITE_OK) {
        fprintf(stderr, "%s: physical index query failed: %d\n", context, rc);
        return 1;
    }

    return expect_int(actual_count, expected_count, context);
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(path, path_size, "/tmp/mylite_%s_%d.mylite", name, current_process_id());

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "failed to build test path\n");
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
    char buffer[test_path_capacity + test_path_suffix_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(buffer)) {
        (void)remove(buffer);
    }
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t read_size = 0U;

    if (file == NULL) {
        fprintf(stderr, "failed to open %s\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "failed to seek %s\n", path);
        (void)fclose(file);
        return 1;
    }
    read_size = fread(buffer, 1U, size, file);
    if (fclose(file) != 0 || read_size != size) {
        fprintf(stderr, "failed to read %s\n", path);
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
    if ((actual == NULL && expected != NULL) || (actual != NULL && expected == NULL) ||
        (actual != NULL && expected != NULL && strcmp(actual, expected) != 0)) {
        fprintf(
            stderr,
            "%s: expected [%s], got [%s]\n",
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
            "%s: expected [%s] to contain [%s]\n",
            context,
            actual == NULL ? "NULL" : actual,
            needle == NULL ? "NULL" : needle
        );
        return 1;
    }
    return 0;
}

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: bytes did not match\n", context);
        return 1;
    }
    return 0;
}
