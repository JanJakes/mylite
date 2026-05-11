#include <mylite/mylite.h>

#include "runtime/mylite_catalog.h"
#include "runtime/mylite_connection.h"
#include "sqlite3.h"
#include "storage/mylite_file_format.h"

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
    mysql_error_no_database_selected = 1046,
    mysql_error_unknown_database = 1049,
    mysql_error_unknown_column = 1054,
    mysql_error_incorrect_database_name = 1102,
    mysql_error_incorrect_table_name = 1103,
    mysql_error_table_does_not_exist = 1146,
    mysql_error_incorrect_column_name = 1166,
    mysql_error_field_no_default = 1364,
    mysql_error_must_have_visible_column = 4028,
    show_columns_column_count = 6,
    visibility_catalog_generation_mutation_count = 7,
    numbers_descriptor_column_count = 7,
    visible_after_delete_row_count = 5,
    short_sql_capacity = 128,
    create_numbers_sql_capacity = 512,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_statement {
    int64_t affected_rows;
    size_t warning_count;
};

struct expected_query {
    const char *sql;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    const char *context;
};

struct expected_contains_query {
    const char *sql;
    const char *needle;
    const char *context;
};

static int test_visibility_success_dml_metadata_and_persistence(void);
static int test_visibility_diagnostics(void);
static int test_catalog_v3_visibility_migration(void);
static int test_independent_visibility_handles(void);
static int seed_schema(mylite_db *database, const char *name);
static int create_numbers_table(mylite_db *database, const char *table_name);
static int make_catalog_look_like_v3(sqlite3 *sqlite);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int expect_statement_result(
    mylite_db *database,
    const char *sql,
    struct expected_statement expected
);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_single_value_contains(mylite_db *database, struct expected_contains_query query);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int execute_sql(sqlite3 *connection, const char *sql);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_uint64(uint64_t actual, uint64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_true(int condition, const char *context);
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

    failures += test_visibility_success_dml_metadata_and_persistence();
    failures += test_visibility_diagnostics();
    failures += test_catalog_v3_visibility_migration();
    failures += test_independent_visibility_handles();

    return failures == 0 ? 0 : 1;
}

static int test_visibility_success_dml_metadata_and_persistence(void) {
    static const char *const ids_after_invisible[] = {"1", "2", "3"};
    static const char *const explicit_values[] = {"1", "10", "20", "2", "11", NULL};
    static const char *const inserted_defaults[] = {"4", "7", NULL, "9", "4", "5", "0"};
    static const char *const visible_id_v[] =
        {"1", "10", "2", "11", "3", "12", "5", "77", "6", "78"};
    static const char *const show_v_invisible[] = {"v", "int", "YES", "", "7", "INVISIBLE"};
    static const char *const show_v_visible[] = {"v", "int", "YES", "", "7", ""};
    static const char *const show_modify_visible[] = {"modify_col", "int", "YES", "", "7", ""};
    static const char *const show_changed_visible[] = {"changed_col", "int", "YES", "", "8", ""};
    static const char *const reopened_row[] = {"5", "77", NULL};
    static const char *const renamed_star[] = {"1", "2", "3", "5", "6"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    const struct mylite_catalog *catalog = NULL;
    const struct mylite_session_state *session = NULL;
    uint64_t catalog_generation_before = 0U;
    uint64_t sqlite_generation_before = 0U;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "success") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open success file");
    failures += seed_schema(database, "app");
    failures += execute_statement_ok(database, "USE app");
    failures += create_numbers_table(database, "numbers");
    failures += execute_statement_ok(database, "ALTER TABLE numbers ALTER id SET VISIBLE");

    catalog = mylite_connection_catalog_for_test(database);
    session = mylite_connection_session_state(database);
    if (catalog != NULL) {
        catalog_generation_before = catalog->generation;
    }
    if (session != NULL) {
        sqlite_generation_before = session->sqlite_schema_generation;
    }

    failures += execute_statement_ok(database, "ALTER TABLE numbers ALTER COLUMN v SET INVISIBLE");
    failures += execute_statement_ok(database, "ALTER TABLE numbers ALTER n SET INVISIBLE");
    failures += execute_statement_ok(database, "ALTER TABLE numbers ALTER nn SET INVISIBLE");
    failures += execute_statement_ok(database, "ALTER TABLE numbers ALTER u SET INVISIBLE");
    failures += execute_statement_ok(database, "ALTER TABLE numbers ALTER bu SET INVISIBLE");
    failures += execute_statement_ok(database, "ALTER TABLE numbers ALTER b SET INVISIBLE");
    failures += execute_statement_ok(database, "ALTER TABLE numbers ALTER b SET INVISIBLE");

    catalog = mylite_connection_catalog_for_test(database);
    session = mylite_connection_session_state(database);
    if (catalog != NULL) {
        failures += expect_uint64(
            catalog->generation,
            catalog_generation_before + visibility_catalog_generation_mutation_count,
            "visibility changes bump catalog generation"
        );
    }
    if (session != NULL) {
        failures += expect_uint64(
            session->sqlite_schema_generation,
            sqlite_generation_before,
            "visibility changes preserve SQLite schema generation"
        );
    }

    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM numbers ORDER BY id",
            .values = ids_after_invisible,
            .column_count = 1U,
            .row_count = 3U,
            .context = "SELECT star omits invisible columns",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, n FROM numbers ORDER BY id LIMIT 2",
            .values = explicit_values,
            .column_count = 3U,
            .row_count = 2U,
            .context = "explicit SELECT reads invisible columns",
        }
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO numbers VALUES (4)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, n, nn, u, bu, b FROM numbers WHERE id = 4",
            .values = inserted_defaults,
            .column_count = numbers_descriptor_column_count,
            .row_count = 1U,
            .context = "implicit insert maps visible columns and defaults invisible columns",
        }
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO numbers (id, v) VALUES (5, 77)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO numbers SET id = 6, v = 78, n = 88",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "UPDATE numbers SET v = 90 WHERE n IS NULL ORDER BY v LIMIT 1",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "DELETE FROM numbers WHERE v = 90 ORDER BY v LIMIT 1",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM numbers LIKE 'v'",
            .values = show_v_invisible,
            .column_count = show_columns_column_count,
            .row_count = 1U,
            .context = "SHOW COLUMNS marks invisible column",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "EXPLAIN numbers",
            .values = NULL,
            .column_count = show_columns_column_count,
            .row_count = numbers_descriptor_column_count,
            .context = "EXPLAIN marks invisible column",
        }
    );
    failures += expect_single_value_contains(
        database,
        (struct expected_contains_query){
            .sql = "SHOW CREATE TABLE numbers",
            .needle = "`v` int DEFAULT '7' /*!80023 INVISIBLE */",
            .context = "SHOW CREATE renders invisible defaulted column",
        }
    );
    failures += expect_single_value_contains(
        database,
        (struct expected_contains_query){
            .sql = "SHOW CREATE TABLE numbers",
            .needle = "`n` int DEFAULT NULL /*!80023 INVISIBLE */",
            .context = "SHOW CREATE renders invisible nullable column",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers DROP COLUMN id",
        (struct expected_sql_error){
            mysql_error_must_have_visible_column,
            "HY000",
            "A table must have at least one visible column.",
        }
    );

    failures += execute_statement_ok(database, "ALTER TABLE numbers ALTER v SET VISIBLE");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM numbers LIKE 'v'",
            .values = show_v_visible,
            .column_count = show_columns_column_count,
            .row_count = 1U,
            .context = "SHOW COLUMNS clears invisible extra",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM numbers ORDER BY id",
            .values = visible_id_v,
            .column_count = 2U,
            .row_count = visible_after_delete_row_count,
            .context = "SELECT star includes restored visible column",
        }
    );
    failures += execute_statement_ok(
        database,
        "CREATE TABLE redefine_visibility ("
        "id INT, modify_col INT DEFAULT 7, change_col INT DEFAULT 8)"
    );
    failures += execute_statement_ok(
        database,
        "ALTER TABLE redefine_visibility ALTER modify_col SET INVISIBLE"
    );
    failures += execute_statement_ok(
        database,
        "ALTER TABLE redefine_visibility ALTER change_col SET INVISIBLE"
    );
    failures += execute_statement_ok(
        database,
        "ALTER TABLE redefine_visibility MODIFY modify_col INT DEFAULT 7"
    );
    failures += execute_statement_ok(
        database,
        "ALTER TABLE redefine_visibility CHANGE change_col changed_col INT DEFAULT 8"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM redefine_visibility LIKE 'modify_col'",
            .values = show_modify_visible,
            .column_count = show_columns_column_count,
            .row_count = 1U,
            .context = "MODIFY makes omitted-visibility definition visible",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM redefine_visibility LIKE 'changed_col'",
            .values = show_changed_visible,
            .column_count = show_columns_column_count,
            .row_count = 1U,
            .context = "CHANGE makes omitted-visibility definition visible",
        }
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen success file");
    failures += execute_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, n FROM numbers WHERE id = 5",
            .values = reopened_row,
            .column_count = 3U,
            .row_count = 1U,
            .context = "reopened invisible metadata and rows persist",
        }
    );
    failures += execute_statement_ok(database, "ALTER TABLE numbers RENAME TO renamed_numbers");
    failures += execute_statement_ok(database, "ALTER TABLE renamed_numbers ALTER v SET INVISIBLE");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM renamed_numbers ORDER BY id",
            .values = renamed_star,
            .column_count = 1U,
            .row_count = visible_after_delete_row_count,
            .context = "renamed table preserves visibility metadata",
        }
    );
    failures += execute_statement_ok(database, "DROP TABLE renamed_numbers");
    failures += execute_error(
        database,
        "ALTER TABLE renamed_numbers ALTER v SET VISIBLE",
        (struct expected_sql_error){
            mysql_error_table_does_not_exist,
            "42S02",
            "doesn't exist",
        }
    );

    mylite_close(database);
    database = NULL;

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "visibility changes preserve MyLite preamble"
    );

    remove_related_files(path);

    return failures;
}

static int test_visibility_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics file");
    failures += seed_schema(database, "app");
    failures += execute_error(
        database,
        "ALTER TABLE numbers ALTER v SET INVISIBLE",
        (struct expected_sql_error){
            mysql_error_no_database_selected,
            "3D000",
            "No database selected",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE missing_schema.numbers ALTER v SET INVISIBLE",
        (struct expected_sql_error){mysql_error_unknown_database, "42000", "Unknown database"}
    );
    failures += execute_statement_ok(database, "USE app");
    failures += create_numbers_table(database, "numbers");
    failures += execute_error(
        database,
        "ALTER TABLE missing_numbers ALTER v SET INVISIBLE",
        (struct expected_sql_error){
            mysql_error_table_does_not_exist,
            "42S02",
            "doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE _mylite_private ALTER v SET INVISIBLE",
        (struct expected_sql_error){
            mysql_error_incorrect_table_name,
            "42000",
            "Incorrect table name",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE _mylite_private.numbers ALTER v SET INVISIBLE",
        (struct expected_sql_error){
            mysql_error_incorrect_database_name,
            "42000",
            "Incorrect database name",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers ALTER missing SET INVISIBLE",
        (struct expected_sql_error){mysql_error_unknown_column, "42S22", "Unknown column"}
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers ALTER _mylite_private SET INVISIBLE",
        (struct expected_sql_error){
            mysql_error_incorrect_column_name,
            "42000",
            "Incorrect column name",
        }
    );
    failures += execute_statement_ok(database, "CREATE TABLE single_visible (id INT)");
    failures += execute_error(
        database,
        "ALTER TABLE single_visible ALTER id SET INVISIBLE",
        (struct expected_sql_error){
            mysql_error_must_have_visible_column,
            "HY000",
            "A table must have at least one visible column.",
        }
    );
    failures +=
        execute_statement_ok(database, "CREATE TABLE no_default (id INT, hidden INT NOT NULL)");
    failures += execute_statement_ok(database, "ALTER TABLE no_default ALTER hidden SET INVISIBLE");
    failures += execute_error(
        database,
        "INSERT INTO no_default VALUES (1)",
        (struct expected_sql_error){
            mysql_error_field_no_default,
            "HY000",
            "Field 'hidden' doesn't have a default value",
        }
    );
    failures += execute_statement_ok(
        database,
        "CREATE TABLE drop_default_visibility (id INT, hidden INT DEFAULT 1)"
    );
    failures += execute_statement_ok(
        database,
        "ALTER TABLE drop_default_visibility ALTER hidden DROP DEFAULT"
    );
    failures += execute_statement_ok(
        database,
        "ALTER TABLE drop_default_visibility ALTER hidden SET INVISIBLE"
    );
    failures += execute_error(
        database,
        "INSERT INTO drop_default_visibility VALUES (1)",
        (struct expected_sql_error){
            mysql_error_field_no_default,
            "HY000",
            "Field 'hidden' doesn't have a default value",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_catalog_v3_visibility_migration(void) {
    static const char *const show_v_visible[] = {"v", "int", "YES", "", NULL, ""};
    static const char *const show_v_invisible[] = {"v", "int", "YES", "", NULL, "INVISIBLE"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    sqlite3 *sqlite = NULL;
    const struct mylite_catalog *catalog = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "migration_v3") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open v3 migration source");
    failures += seed_schema(database, "app");
    failures += execute_statement_ok(database, "USE app");
    failures += execute_statement_ok(database, "CREATE TABLE migrated (id INT, v INT)");
    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite != NULL) {
        failures += make_catalog_look_like_v3(sqlite);
    }
    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open migrated v3 catalog");
    catalog = mylite_connection_catalog_for_test(database);
    if (catalog != NULL) {
        failures += expect_uint64(
            catalog->schema_version,
            MYLITE_CATALOG_SCHEMA_VERSION,
            "migrated catalog version"
        );
    }
    failures += execute_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM migrated LIKE 'v'",
            .values = show_v_visible,
            .column_count = show_columns_column_count,
            .row_count = 1U,
            .context = "v3 migration makes existing columns visible",
        }
    );
    failures += execute_statement_ok(database, "ALTER TABLE migrated ALTER v SET INVISIBLE");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM migrated LIKE 'v'",
            .values = show_v_invisible,
            .column_count = show_columns_column_count,
            .row_count = 1U,
            .context = "migrated catalog accepts visibility mutation",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_independent_visibility_handles(void) {
    static const char *const first_star[] = {"1"};
    static const char *const second_star[] = {"1", "2"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "independent_first") != 0 ||
        make_test_path(second_path, sizeof(second_path), "independent_second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures +=
        expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first visibility file");
    failures +=
        expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second visibility file");
    failures += seed_schema(first, "app");
    failures += seed_schema(second, "app");
    failures += execute_statement_ok(first, "USE app");
    failures += execute_statement_ok(second, "USE app");
    failures += execute_statement_ok(first, "CREATE TABLE t (id INT, v INT)");
    failures += execute_statement_ok(second, "CREATE TABLE t (id INT, v INT)");
    failures += expect_statement_result(
        first,
        "INSERT INTO t VALUES (1, 2)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_result(
        second,
        "INSERT INTO t VALUES (1, 2)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += execute_statement_ok(first, "ALTER TABLE t ALTER v SET INVISIBLE");
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT * FROM t",
            .values = first_star,
            .column_count = 1U,
            .row_count = 1U,
            .context = "first file has invisible column",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT * FROM t",
            .values = second_star,
            .column_count = 2U,
            .row_count = 1U,
            .context = "second file keeps independent visible column",
        }
    );

    mylite_close(second);
    mylite_close(first);
    remove_related_files(second_path);
    remove_related_files(first_path);

    return failures;
}

static int seed_schema(mylite_db *database, const char *name) {
    char sql[short_sql_capacity];
    int written = snprintf(sql, sizeof(sql), "CREATE DATABASE %s", name);

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        return 1;
    }

    return expect_statement_result(
        database,
        sql,
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
}

static int create_numbers_table(mylite_db *database, const char *table_name) {
    char sql[create_numbers_sql_capacity];
    int failures = 0;
    int written = snprintf(
        sql,
        sizeof(sql),
        "CREATE TABLE %s ("
        "id INT NOT NULL, "
        "v INT DEFAULT 7, "
        "n INT DEFAULT NULL, "
        "nn INT NOT NULL DEFAULT 9, "
        "u INT UNSIGNED DEFAULT 4, "
        "bu BIGINT UNSIGNED DEFAULT 5, "
        "b BOOL DEFAULT FALSE"
        ")",
        table_name
    );

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        return 1;
    }
    failures += execute_statement_ok(database, sql);

    written = snprintf(
        sql,
        sizeof(sql),
        "INSERT INTO %s (id, v, n, nn, u, bu, b) VALUES "
        "(1, 10, 20, 30, 40, 50, TRUE), "
        "(2, 11, NULL, 31, 41, 51, FALSE), "
        "(3, 12, 22, 32, 42, 52, TRUE)",
        table_name
    );
    if (written < 0 || (size_t)written >= sizeof(sql)) {
        return failures + 1;
    }
    failures += expect_statement_result(
        database,
        sql,
        (struct expected_statement){.affected_rows = 3, .warning_count = 0U}
    );

    return failures;
}

static int make_catalog_look_like_v3(sqlite3 *sqlite) {
    int failures = 0;

    failures += execute_sql(sqlite, "DROP TABLE _mylite_catalog_index_columns");
    failures += execute_sql(sqlite, "DROP TABLE _mylite_catalog_indexes");
    failures +=
        execute_sql(sqlite, "ALTER TABLE _mylite_catalog_tables DROP COLUMN auto_increment_next");
    failures +=
        execute_sql(sqlite, "ALTER TABLE _mylite_catalog_columns DROP COLUMN is_auto_increment");
    failures += execute_sql(sqlite, "ALTER TABLE _mylite_catalog_columns DROP COLUMN is_visible");
    failures += execute_sql(
        sqlite,
        "UPDATE _mylite_catalog_state "
        "SET schema_version = 3, minimum_reader_schema_version = 3"
    );

    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "execute '%s' failed: rc=%d err=%d state=%s message=%s\n",
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

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    failures += expect_int(rc, MYLITE_ERROR, sql);
    failures += expect_true(result == NULL, "failed execute leaves result null");
    failures += expect_int(mylite_errcode(database), expected.code, "error code");
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, "SQLSTATE");
    failures += expect_contains(mylite_errmsg(database), expected.message_part, "error message");
    mylite_result_free(result);

    return failures;
}

static int execute_statement_ok(mylite_db *database, const char *sql) {
    return expect_statement_result(
        database,
        sql,
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
}

static int expect_statement_result(
    mylite_db *database,
    const char *sql,
    struct expected_statement expected
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, sql);
    failures += expect_size(mylite_result_row_count(result), 0U, sql);
    failures += expect_int64(mylite_result_affected_rows(result), expected.affected_rows, sql);
    failures += expect_size(mylite_result_warning_count(result), expected.warning_count, sql);
    mylite_result_free(result);

    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    failures += expect_size(mylite_result_column_count(result), query.column_count, query.context);
    failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
    if (query.values != NULL) {
        for (size_t row = 0U; row < query.row_count; ++row) {
            for (size_t column = 0U; column < query.column_count; ++column) {
                size_t value_index = (row * query.column_count) + column;

                failures += expect_result_value(
                    result,
                    row,
                    column,
                    query.values[value_index],
                    query.context
                );
            }
        }
    }
    failures += expect_int64(mylite_result_affected_rows(result), 0, query.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, query.context);
    mylite_result_free(result);

    return failures;
}

static int expect_single_value_contains(mylite_db *database, struct expected_contains_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    failures += expect_size(mylite_result_column_count(result), 2U, query.context);
    failures += expect_size(mylite_result_row_count(result), 1U, query.context);
    failures +=
        expect_contains(mylite_result_value_text(result, 0U, 1U), query.needle, query.context);
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
        return expect_true(actual == NULL, context);
    }

    return expect_text(actual, expected, context);
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    const char *directory = getenv("TMPDIR");
    int written = 0;

    if (directory == NULL || directory[0] == '\0') {
        directory = getenv("TEMP");
    }
    if (directory == NULL || directory[0] == '\0') {
        directory = ".";
    }

    written = snprintf(
        path,
        path_size,
        "%s/mylite_alter_column_visibility_%d_%s.mylite",
        directory,
        current_process_id(),
        name
    );
    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "test path is too long for %s\n", name);
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
    remove(path);
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related[test_path_capacity];
    int written = snprintf(related, sizeof(related), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(related)) {
        remove(related);
    }
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        fprintf(stderr, "failed to open %s\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fclose(file);
        fprintf(stderr, "failed to seek %s\n", path);
        return 1;
    }
    if (fread(buffer, 1U, size, file) != size) {
        fclose(file);
        fprintf(stderr, "failed to read %s\n", path);
        return 1;
    }
    fclose(file);

    return 0;
}

static int execute_sql(sqlite3 *connection, const char *sql) {
    int rc = sqlite3_exec(connection, sql, NULL, NULL, NULL);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "sqlite exec failed for '%s': %d\n", sql, rc);
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

static int expect_uint64(uint64_t actual, uint64_t expected, const char *context) {
    if (actual != expected) {
        fprintf(
            stderr,
            "%s: expected %llu, got %llu\n",
            context,
            (unsigned long long)expected,
            (unsigned long long)actual
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

static int expect_true(int condition, const char *context) {
    if (!condition) {
        fprintf(stderr, "%s: expected true\n", context);
        return 1;
    }

    return 0;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected '%s', got '%s'\n",
            context,
            expected,
            actual == NULL ? "(null)" : actual
        );
        return 1;
    }

    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected '%s' to contain '%s'\n",
            context,
            actual == NULL ? "(null)" : actual,
            needle
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
        fprintf(stderr, "%s: byte mismatch\n", context);
        return 1;
    }

    return 0;
}
