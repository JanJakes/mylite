#include <mylite/mylite.h>

#include "runtime/mylite_catalog.h"
#include "runtime/mylite_connection.h"
#include "storage/mylite_file_format.h"

#include <stdbool.h>
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
    sql_capacity = 512,
    unsigned_int_default = 6,
    hidden_default = 9,
    mysql_error_parse = 1064,
    mysql_error_no_database_selected = 1046,
    mysql_error_unknown_database = 1049,
    mysql_error_table_exists = 1050,
    mysql_error_unknown_column = 1054,
    mysql_error_duplicate_column = 1060,
    mysql_error_incorrect_table_name = 1103,
    mysql_error_unknown = 1105,
    mysql_error_table_does_not_exist = 1146,
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

struct expected_statement {
    const char *sql;
    int64_t affected_rows;
    size_t warning_count;
};

struct expected_column_descriptor {
    const char *schema_name;
    const char *table_name;
    const char *name;
    const char *logical_type;
    bool is_nullable;
    bool is_visible;
    enum mylite_catalog_column_default_kind default_kind;
    int64_t default_integer;
};

static int test_create_table_select_success_persistence_and_preamble(void);
static int test_create_table_select_diagnostics_and_noop_ordering(void);
static int test_create_table_select_after_rename_and_drop(void);
static int test_independent_create_table_select_handles(void);
static int seed_schema(mylite_db *database, const char *name);
static int create_source_table(mylite_db *database, const char *table_name);
static int seed_source_rows(mylite_db *database, const char *table_name);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_statement_ok(mylite_db *database, struct expected_statement expected);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int expect_column_matches(mylite_db *database, struct expected_column_descriptor expected);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_uint64(uint64_t actual, uint64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_bool(bool actual, bool expected, const char *context);
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

    failures += test_create_table_select_success_persistence_and_preamble();
    failures += test_create_table_select_diagnostics_and_noop_ordering();
    failures += test_create_table_select_after_rename_and_drop();
    failures += test_independent_create_table_select_handles();

    return failures == 0 ? 0 : 1;
}

static int test_create_table_select_success_persistence_and_preamble(void) {
    static const char *const copied_rows[] = {
        "2",
        NULL,
        "200",
        "2000",
        "3",
        "30",
        "300",
        "3000",
    };
    static const char *const hidden_rows[] = {"70"};
    static const char *const row_count_two[] = {"2"};
    static const char *const empty_count[] = {"0"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    const struct mylite_catalog *catalog = NULL;
    const struct mylite_session_state *session = NULL;
    uint64_t catalog_generation_before = 0U;
    uint64_t sqlite_generation_before = 0U;
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "success") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open success file");
    failures += seed_schema(database, "app");
    failures += seed_schema(database, "other");
    failures += create_source_table(database, "app.src");
    failures += seed_source_rows(database, "app.src");

    catalog = mylite_connection_catalog_for_test(database);
    session = mylite_connection_session_state(database);
    if (catalog != NULL) {
        catalog_generation_before = catalog->generation;
    }
    if (session != NULL) {
        sqlite_generation_before = session->sqlite_schema_generation;
    }

    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "CREATE TABLE other.copy AS "
                   "SELECT id, n AS nullable_alias, b, iu "
                   "FROM app.src WHERE id >= 1 ORDER BY id DESC LIMIT 2",
            .affected_rows = 2,
            .warning_count = 0U,
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT()",
            .values = row_count_two,
            .column_count = 1U,
            .row_count = 1U,
            .context = "ctas row count function",
        }
    );
    catalog = mylite_connection_catalog_for_test(database);
    session = mylite_connection_session_state(database);
    if (catalog != NULL) {
        failures += expect_uint64(
            catalog->generation,
            catalog_generation_before + 1U,
            "ctas increments catalog generation"
        );
    }
    if (session != NULL) {
        failures += expect_uint64(
            session->sqlite_schema_generation,
            sqlite_generation_before + 1U,
            "ctas increments SQLite schema generation"
        );
    }
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, nullable_alias, b, iu FROM other.copy ORDER BY id",
            .values = copied_rows,
            .column_count = 4U,
            .row_count = 2U,
            .context = "ctas copied rows",
        }
    );
    failures += expect_column_matches(
        database,
        (struct expected_column_descriptor){
            .schema_name = "other",
            .table_name = "copy",
            .name = "nullable_alias",
            .logical_type = "INT",
            .is_nullable = true,
            .is_visible = true,
            .default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_NONE,
            .default_integer = 0,
        }
    );
    failures += expect_column_matches(
        database,
        (struct expected_column_descriptor){
            .schema_name = "other",
            .table_name = "copy",
            .name = "iu",
            .logical_type = "INT UNSIGNED",
            .is_nullable = false,
            .is_visible = true,
            .default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER,
            .default_integer = unsigned_int_default,
        }
    );

    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "CREATE TABLE other.hidden_copy AS SELECT hidden FROM app.src WHERE id = 1",
            .affected_rows = 1,
            .warning_count = 0U,
        }
    );
    failures += expect_column_matches(
        database,
        (struct expected_column_descriptor){
            .schema_name = "other",
            .table_name = "hidden_copy",
            .name = "hidden",
            .logical_type = "INT",
            .is_nullable = true,
            .is_visible = true,
            .default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER,
            .default_integer = hidden_default,
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM other.hidden_copy",
            .values = hidden_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "ctas explicit invisible source becomes visible",
        }
    );

    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "CREATE TABLE other.empty_copy AS SELECT id, n FROM app.src WHERE id = 999",
            .affected_rows = 0,
            .warning_count = 0U,
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM other.empty_copy",
            .values = empty_count,
            .column_count = 1U,
            .row_count = 1U,
            .context = "ctas zero-row source still creates table",
        }
    );

    mylite_close(database);
    database = NULL;

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "ctas preserves preamble"
    );

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen success file");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, nullable_alias, b, iu FROM other.copy ORDER BY id",
            .values = copied_rows,
            .column_count = 4U,
            .row_count = 2U,
            .context = "persisted ctas rows",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_create_table_select_diagnostics_and_noop_ordering(void) {
    static const char *const existing_count[] = {"0"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics file");
    failures += seed_schema(database, "app");
    failures += create_source_table(database, "app.src");
    failures += seed_source_rows(database, "app.src");
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "CREATE TABLE app.existing(id INT)",
            .affected_rows = 0,
            .warning_count = 0U,
        }
    );

    failures += execute_error(
        database,
        "CREATE TABLE dst AS SELECT id FROM app.src",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE app.dst AS SELECT id FROM src",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE nosuch_target.dst AS SELECT id FROM nosuch_source.src",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'nosuch_source'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE nosuch_target.dst AS SELECT id FROM app.missing_source",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing_source' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE nosuch_target.dst AS SELECT id FROM app.src",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'nosuch_target'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE app.existing AS SELECT id, id FROM app.src",
        (struct expected_sql_error){
            .code = mysql_error_table_exists,
            .sqlstate = "42S01",
            .message_part = "Table 'existing' already exists",
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "CREATE TABLE IF NOT EXISTS app.existing AS SELECT id, id FROM app.src",
            .affected_rows = 0,
            .warning_count = 1U,
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM app.existing",
            .values = existing_count,
            .column_count = 1U,
            .row_count = 1U,
            .context = "ctas if-not-exists skips row copy",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE IF NOT EXISTS app.existing AS SELECT id FROM app.missing_source",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing_source' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE app.duplicate_output AS SELECT id, id FROM app.src",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_column,
            .sqlstate = "42S21",
            .message_part = "Duplicate column name 'id'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE app.bad_projection AS SELECT missing FROM app.src",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE app.bad_where AS SELECT id FROM app.src WHERE missing = 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'where clause'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE app.bad_order AS SELECT id FROM app.src ORDER BY missing",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'order clause'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE app._mylite_bad AS SELECT id FROM app.src",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name '_mylite_bad'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE app.bad_expr AS SELECT id + 1 FROM app.src",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT supports only descriptor table columns",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE app.bad_literal AS SELECT '1' FROM app.src",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT supports only descriptor table columns",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE app.bad_definitions(id INT) AS SELECT id FROM app.src",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "CREATE TEMPORARY TABLE app.bad_temp AS SELECT id FROM app.src",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE app.bad_join AS SELECT src.id FROM app.src JOIN app.src other",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "CREATE TABLE ... SELECT does not support joined SELECT",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_create_table_select_after_rename_and_drop(void) {
    static const char *const copied_rows[] = {"1", "10"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "rename_drop") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open rename/drop file");
    failures += seed_schema(database, "app");
    failures += create_source_table(database, "app.src");
    failures += seed_source_rows(database, "app.src");
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "RENAME TABLE app.src TO app.renamed_src",
            .affected_rows = 0,
            .warning_count = 0U,
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "CREATE TABLE app.copy_after_rename AS "
                   "SELECT id, n FROM app.renamed_src WHERE id = 1",
            .affected_rows = 1,
            .warning_count = 0U,
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, n FROM app.copy_after_rename",
            .values = copied_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "ctas after source rename",
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "DROP TABLE app.renamed_src",
            .affected_rows = 0,
            .warning_count = 0U,
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE app.copy_after_drop AS SELECT id FROM app.renamed_src",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.renamed_src' doesn't exist",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_independent_create_table_select_handles(void) {
    static const char *const first_values[] = {"1", "10"};
    static const char *const second_values[] = {"2", "20"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "first") != 0 ||
        make_test_path(second_path, sizeof(second_path), "second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first file");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second file");
    failures += seed_schema(first, "app");
    failures += seed_schema(second, "app");
    failures += create_source_table(first, "app.src");
    failures += create_source_table(second, "app.src");
    failures += expect_statement_ok(
        first,
        (struct expected_statement){
            .sql = "INSERT INTO app.src(id, n, b, iu, hidden) VALUES (1, 10, 100, 1000, 70)",
            .affected_rows = 1,
            .warning_count = 0U,
        }
    );
    failures += expect_statement_ok(
        second,
        (struct expected_statement){
            .sql = "INSERT INTO app.src(id, n, b, iu, hidden) VALUES (2, 20, 200, 2000, 80)",
            .affected_rows = 1,
            .warning_count = 0U,
        }
    );
    failures += expect_statement_ok(
        first,
        (struct expected_statement){
            .sql = "CREATE TABLE app.copy AS SELECT id, n FROM app.src",
            .affected_rows = 1,
            .warning_count = 0U,
        }
    );
    failures += expect_statement_ok(
        second,
        (struct expected_statement){
            .sql = "CREATE TABLE app.copy AS SELECT id, n FROM app.src",
            .affected_rows = 1,
            .warning_count = 0U,
        }
    );
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT id, n FROM app.copy",
            .values = first_values,
            .column_count = 2U,
            .row_count = 1U,
            .context = "first independent ctas rows",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT id, n FROM app.copy",
            .values = second_values,
            .column_count = 2U,
            .row_count = 1U,
            .context = "second independent ctas rows",
        }
    );

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);

    return failures;
}

static int seed_schema(mylite_db *database, const char *name) {
    struct mylite_catalog_schema_descriptor schema = {0};

    return expect_int(
        mylite_catalog_create_schema(database, name, &schema),
        MYLITE_OK,
        "seed schema"
    );
}

static int create_source_table(mylite_db *database, const char *table_name) {
    char sql[sql_capacity];
    int written = snprintf(
        sql,
        sizeof(sql),
        "CREATE TABLE %s ("
        "id INT NOT NULL DEFAULT 7, "
        "n INTEGER NULL DEFAULT NULL, "
        "b BIGINT NOT NULL, "
        "iu INT UNSIGNED NOT NULL DEFAULT 6, "
        "hidden INT NULL DEFAULT 9)",
        table_name
    );
    int failures = 0;

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "create source table SQL is too long for %s\n", table_name);
        return 1;
    }
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = sql,
            .affected_rows = 0,
            .warning_count = 0U,
        }
    );

    written = snprintf(sql, sizeof(sql), "ALTER TABLE %s ALTER hidden SET INVISIBLE", table_name);
    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "alter source table SQL is too long for %s\n", table_name);
        return failures + 1;
    }
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = sql,
            .affected_rows = 0,
            .warning_count = 0U,
        }
    );

    return failures;
}

static int seed_source_rows(mylite_db *database, const char *table_name) {
    char sql[sql_capacity];
    int written = snprintf(
        sql,
        sizeof(sql),
        "INSERT INTO %s(id, n, b, iu, hidden) VALUES "
        "(1, 10, 100, 1000, 70), "
        "(2, NULL, 200, 2000, 80), "
        "(3, 30, 300, 3000, 90)",
        table_name
    );

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "insert source rows SQL is too long for %s\n", table_name);
        return 1;
    }

    return expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = sql,
            .affected_rows = 3,
            .warning_count = 0U,
        }
    );
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "execute '%s': expected OK, got %d / %d %s %s\n",
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

    if (rc != MYLITE_ERROR) {
        fprintf(stderr, "execute '%s': expected MYLITE_ERROR, got %d\n", sql, rc);
        failures += 1;
    }
    failures += expect_true(result == NULL, "failed execute leaves result null");
    failures += expect_int(mylite_errcode(database), expected.code, "error code");
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, "SQLSTATE");
    failures += expect_contains(mylite_errmsg(database), expected.message_part, "error message");
    mylite_result_free(result);

    return failures;
}

static int expect_statement_ok(mylite_db *database, struct expected_statement expected) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, expected.sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, "statement column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "statement row count");
    failures += expect_int64(
        mylite_result_affected_rows(result),
        expected.affected_rows,
        "statement affected rows"
    );
    failures += expect_size(
        mylite_result_warning_count(result),
        expected.warning_count,
        "statement warning count"
    );
    mylite_result_free(result);

    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    failures += expect_size(mylite_result_column_count(result), query.column_count, query.context);
    failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
    for (size_t row = 0U; row < query.row_count; ++row) {
        for (size_t column = 0U; column < query.column_count; ++column) {
            size_t value_index = (row * query.column_count) + column;

            failures +=
                expect_result_value(result, row, column, query.values[value_index], query.context);
        }
    }
    failures += expect_int64(mylite_result_affected_rows(result), 0, query.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, query.context);
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

static int expect_column_matches(mylite_db *database, struct expected_column_descriptor expected) {
    struct mylite_catalog_schema_descriptor schema = {0};
    struct mylite_catalog_table_descriptor table = {0};
    struct mylite_catalog_column_descriptor column = {0};
    int failures = 0;

    failures += expect_int(
        mylite_catalog_read_schema_by_name(database, expected.schema_name, &schema),
        MYLITE_OK,
        "read descriptor schema"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, expected.table_name, &table),
        MYLITE_OK,
        "read descriptor table"
    );
    failures += expect_int(
        mylite_catalog_read_column_by_name(database, table.table_id, expected.name, &column),
        MYLITE_OK,
        "read descriptor column"
    );
    failures += expect_text(column.name, expected.name, "descriptor column name");
    failures += expect_text(column.logical_type, expected.logical_type, "descriptor logical type");
    failures += expect_text(column.physical_type, "INTEGER", "descriptor physical type");
    failures += expect_bool(column.is_nullable, expected.is_nullable, "descriptor nullability");
    failures += expect_bool(column.is_visible, expected.is_visible, "descriptor visibility");
    failures +=
        expect_int((int)column.default_kind, (int)expected.default_kind, "descriptor default kind");
    failures +=
        expect_int64(column.default_integer, expected.default_integer, "descriptor default value");

    return failures;
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
        "%s/mylite_create_table_select_lifecycle_%d_%s.mylite",
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
    remove_with_suffix(path, "");
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related_path[test_path_capacity];
    int written = snprintf(related_path, sizeof(related_path), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related_path)) {
        return;
    }

    (void)remove(related_path);
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        fprintf(stderr, "failed to open %s for reading\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "failed to seek %s\n", path);
        (void)fclose(file);
        return 1;
    }
    if (fread(buffer, 1U, size, file) != size) {
        fprintf(stderr, "failed to read %zu bytes from %s\n", size, path);
        (void)fclose(file);
        return 1;
    }
    if (fclose(file) != 0) {
        fprintf(stderr, "failed to close %s\n", path);
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

static int expect_bool(bool actual, bool expected, const char *context) {
    if (actual != expected) {
        const char *actual_text = "false";
        const char *expected_text = "false";

        if (actual) {
            actual_text = "true";
        }
        if (expected) {
            expected_text = "true";
        }
        fprintf(stderr, "%s: expected %s, got %s\n", context, expected_text, actual_text);
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
    if (actual == NULL && expected == NULL) {
        return 0;
    }
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected text [%s], got [%s]\n",
            context,
            expected == NULL ? "(null)" : expected,
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
            "%s: expected [%s] to contain [%s]\n",
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
        fprintf(stderr, "%s: bytes differ\n", context);
        return 1;
    }

    return 0;
}
