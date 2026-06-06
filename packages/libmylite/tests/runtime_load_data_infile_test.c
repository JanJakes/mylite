#include <mylite/mylite.h>

#include "runtime/mylite_catalog.h"
#include "runtime/mylite_connection.h"
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
    sql_capacity = 4096,
    loaded_column_count = 5,
    mysql_error_cant_get_stat = 13,
    mysql_error_no_database_selected = 1046,
    mysql_error_unknown_database = 1049,
    mysql_error_unknown_column = 1054,
    mysql_error_table_does_not_exist = 1146,
    mysql_error_load_data_row_missing = 1261,
    mysql_error_load_data_row_truncated = 1262,
    mysql_error_load_data_null_to_not_null = 1263,
    mysql_error_truncated_wrong_value = 1366,
    mysql_error_load_data_local_disabled = 3948,
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

struct expected_load_result {
    int64_t affected_rows;
    size_t warning_count;
};

struct text_file {
    const char *path;
    const char *contents;
};

struct load_data_sql {
    const char *file_path;
    const char *tail;
};

static int test_load_data_success_persistence_and_metadata(void);
static int test_load_data_diagnostics_and_nonstrict_adjustment(void);
static int test_load_data_independent_handles(void);
static int seed_schema(mylite_db *database, const char *name);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_load_ok(
    mylite_db *database,
    const char *sql,
    struct expected_load_result expected
);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int make_test_path(char *path, size_t path_size, const char *name, const char *suffix);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int write_text_file(struct text_file file);
static int build_load_sql(char *sql, size_t sql_size, struct load_data_sql load_sql);
static int escape_sql_string(char *output, size_t output_size, const char *input);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
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

    failures += test_load_data_success_persistence_and_metadata();
    failures += test_load_data_diagnostics_and_nonstrict_adjustment();
    failures += test_load_data_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_load_data_success_persistence_and_metadata(void) {
    static const char *const loaded_rows[] = {
        "1",
        "-2147483648",
        "alpha",
        NULL,
        "9",
        "2",
        "2147483647",
        "empty",
        "20",
        "10",
        "3",
        "0",
        "tab\tinside",
        NULL,
        "11",
    };
    static const char *const partial_rows[] = {"10", "first", "5", "11", "second", "5"};
    static const char *const persisted_rows[] = {"1", "alpha", "2", "empty", "3", "tab\tinside"};
    char database_path[test_path_capacity];
    char import_path[test_path_capacity];
    char partial_path[test_path_capacity];
    char sql[sql_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    const struct mylite_catalog *catalog = NULL;
    const struct mylite_session_state *session = NULL;
    uint64_t catalog_generation_before_load = 0U;
    uint64_t sqlite_generation_before_load = 0U;
    int failures = 0;

    if (make_test_path(database_path, sizeof(database_path), "success", ".mylite") != 0 ||
        make_test_path(import_path, sizeof(import_path), "success_rows", ".tsv") != 0 ||
        make_test_path(partial_path, sizeof(partial_path), "partial_rows", ".tsv") != 0) {
        return 1;
    }
    remove_related_files(database_path);
    (void)remove(import_path);
    (void)remove(partial_path);
    mylite_file_preamble_init(expected_preamble);

    failures += write_text_file((struct text_file){
        .path = import_path,
        .contents = "1\t-2147483648\talpha\t\\N\t9\n"
                    "2\t2147483647\tempty\t20\t10\n"
                    "3\t0\ttab\\tinside\t\\N\t11\n",
    });
    failures += write_text_file((struct text_file){
        .path = partial_path,
        .contents = "id\tbody\n10\tfirst\n11\tsecond\n",
    });

    failures += expect_int(mylite_open(database_path, &database), MYLITE_OK, "open success file");
    failures += seed_schema(database, "app");
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(
        database,
        "CREATE TABLE imported ("
        "id INT NOT NULL, i INT, body VARCHAR(20), n INT NULL, nn INT NOT NULL)",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "CREATE TABLE partial (id INT NOT NULL, body VARCHAR(20), nn INT NOT NULL DEFAULT 5)",
        &result
    );
    mylite_result_free(result);
    result = NULL;

    catalog = mylite_connection_catalog_for_test(database);
    session = mylite_connection_session_state(database);
    if (catalog != NULL) {
        catalog_generation_before_load = catalog->generation;
    }
    if (session != NULL) {
        sqlite_generation_before_load = session->sqlite_schema_generation;
    }

    failures += build_load_sql(
        sql,
        sizeof(sql),
        (struct load_data_sql){.file_path = import_path, .tail = "INTO TABLE imported"}
    );
    failures += expect_load_ok(
        database,
        sql,
        (struct expected_load_result){.affected_rows = 3, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, i, body, n, nn FROM imported ORDER BY id",
            .values = loaded_rows,
            .column_count = loaded_column_count,
            .row_count = 3U,
            .context = "loaded rows",
        }
    );

    catalog = mylite_connection_catalog_for_test(database);
    session = mylite_connection_session_state(database);
    if (catalog != NULL) {
        failures += expect_uint64(
            catalog->generation,
            catalog_generation_before_load,
            "load data leaves catalog generation"
        );
    }
    if (session != NULL) {
        failures += expect_uint64(
            session->sqlite_schema_generation,
            sqlite_generation_before_load,
            "load data leaves SQLite schema generation"
        );
    }

    failures += build_load_sql(
        sql,
        sizeof(sql),
        (struct load_data_sql){
            .file_path = partial_path,
            .tail = "INTO TABLE app.partial IGNORE 1 LINES (id, body)",
        }
    );
    failures += expect_load_ok(
        database,
        sql,
        (struct expected_load_result){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, body, nn FROM partial ORDER BY id",
            .values = partial_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "partial load rows",
        }
    );

    mylite_close(database);
    database = NULL;

    failures += read_file_at(database_path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "load data preserves preamble"
    );

    failures += expect_int(mylite_open(database_path, &database), MYLITE_OK, "reopen success file");
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, body FROM imported ORDER BY id",
            .values = persisted_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "loaded rows persist",
        }
    );

    mylite_close(database);
    remove_related_files(database_path);
    (void)remove(import_path);
    (void)remove(partial_path);

    return failures;
}

static int test_load_data_diagnostics_and_nonstrict_adjustment(void) {
    static const char *const nonstrict_missing_rows[] = {"1", NULL};
    static const char *const nonstrict_missing_default_rows[] = {"12", NULL, "0", "0", ""};
    static const char *const nonstrict_extra_rows[] = {"2", "3"};
    static const char *const nonstrict_invalid_rows[] = {"7", "0"};
    static const char *const nonstrict_empty_temporal_rows[] = {
        "0000",
        "0000-00-00",
        "00:00:00",
        "0000-00-00 00:00:00",
        "0000-00-00 00:00:00",
    };
    char database_path[test_path_capacity];
    char load_path[test_path_capacity];
    char missing_path[test_path_capacity];
    char sql[sql_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(database_path, sizeof(database_path), "diagnostics", ".mylite") != 0 ||
        make_test_path(load_path, sizeof(load_path), "diagnostics_rows", ".tsv") != 0 ||
        make_test_path(missing_path, sizeof(missing_path), "missing_rows", ".tsv") != 0) {
        return 1;
    }
    remove_related_files(database_path);
    (void)remove(load_path);
    (void)remove(missing_path);

    failures +=
        expect_int(mylite_open(database_path, &database), MYLITE_OK, "open diagnostics file");
    failures += seed_schema(database, "app");

    failures += build_load_sql(
        sql,
        sizeof(sql),
        (struct load_data_sql){.file_path = load_path, .tail = "INTO TABLE no_default"}
    );
    failures += execute_error(
        database,
        sql,
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );

    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "CREATE TABLE strict_fields (id INT, v INT)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "CREATE TABLE strict_nulls (id INT, v INT NOT NULL)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "CREATE TABLE missing_defaults ("
        "id INT, no_default INT NULL, explicit_default INT NULL DEFAULT 9, "
        "not_null_default INT NOT NULL DEFAULT 7, body VARCHAR(10) NOT NULL DEFAULT 'd')",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "CREATE TABLE empty_temporal ("
        "y YEAR, d DATE, tm TIME, dt DATETIME, ts TIMESTAMP NULL)",
        &result
    );
    mylite_result_free(result);
    result = NULL;

    failures += build_load_sql(
        sql,
        sizeof(sql),
        (struct load_data_sql){.file_path = missing_path, .tail = "INTO TABLE strict_fields"}
    );
    failures += execute_error(
        database,
        sql,
        (struct expected_sql_error){
            .code = mysql_error_cant_get_stat,
            .sqlstate = "HY000",
            .message_part = "Can't get stat",
        }
    );

    failures += write_text_file((struct text_file){.path = load_path, .contents = "1\n"});
    failures += build_load_sql(
        sql,
        sizeof(sql),
        (struct load_data_sql){.file_path = load_path,
                               .tail = "INTO TABLE missing_schema.strict_fields"}
    );
    failures += execute_error(
        database,
        sql,
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database",
        }
    );
    failures += build_load_sql(
        sql,
        sizeof(sql),
        (struct load_data_sql){.file_path = load_path, .tail = "INTO TABLE missing_table"}
    );
    failures += execute_error(
        database,
        sql,
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "doesn't exist",
        }
    );
    failures += build_load_sql(
        sql,
        sizeof(sql),
        (struct load_data_sql){.file_path = load_path,
                               .tail = "INTO TABLE strict_fields (missing_col)"}
    );
    failures += execute_error(
        database,
        sql,
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column",
        }
    );
    failures += build_load_sql(
        sql,
        sizeof(sql),
        (struct load_data_sql){.file_path = load_path, .tail = "INTO TABLE strict_fields"}
    );
    failures += execute_error(
        database,
        sql,
        (struct expected_sql_error){
            .code = mysql_error_load_data_row_missing,
            .sqlstate = "01000",
            .message_part = "doesn't contain data for all columns",
        }
    );

    failures += execute_ok(database, "SET sql_mode = ''", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_load_ok(
        database,
        sql,
        (struct expected_load_result){.affected_rows = 1, .warning_count = 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM strict_fields ORDER BY id",
            .values = nonstrict_missing_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "nonstrict missing row",
        }
    );

    failures += write_text_file((struct text_file){.path = load_path, .contents = "12\n"});
    failures += build_load_sql(
        sql,
        sizeof(sql),
        (struct load_data_sql){.file_path = load_path, .tail = "INTO TABLE missing_defaults"}
    );
    failures += expect_load_ok(
        database,
        sql,
        (struct expected_load_result){.affected_rows = 1, .warning_count = 4U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, no_default, explicit_default, not_null_default, body "
                   "FROM missing_defaults",
            .values = nonstrict_missing_default_rows,
            .column_count = loaded_column_count,
            .row_count = 1U,
            .context = "nonstrict missing defaults",
        }
    );

    failures += write_text_file((struct text_file){.path = load_path, .contents = "\t\t\t\t\n"});
    failures += build_load_sql(
        sql,
        sizeof(sql),
        (struct load_data_sql){.file_path = load_path, .tail = "INTO TABLE empty_temporal"}
    );
    failures += expect_load_ok(
        database,
        sql,
        (struct expected_load_result){.affected_rows = 1, .warning_count = 4U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT y, d, tm, dt, ts FROM empty_temporal",
            .values = nonstrict_empty_temporal_rows,
            .column_count = loaded_column_count,
            .row_count = 1U,
            .context = "nonstrict empty temporal fields",
        }
    );

    failures += execute_ok(database, "SET sql_mode = 'STRICT_TRANS_TABLES'", &result);
    mylite_result_free(result);
    result = NULL;
    failures += write_text_file((struct text_file){.path = load_path, .contents = "2\t3\t4\n"});
    failures += build_load_sql(
        sql,
        sizeof(sql),
        (struct load_data_sql){.file_path = load_path, .tail = "INTO TABLE strict_fields"}
    );
    failures += execute_error(
        database,
        sql,
        (struct expected_sql_error){
            .code = mysql_error_load_data_row_truncated,
            .sqlstate = "01000",
            .message_part = "was truncated",
        }
    );
    failures += execute_ok(database, "SET sql_mode = ''", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_load_ok(
        database,
        sql,
        (struct expected_load_result){.affected_rows = 1, .warning_count = 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM strict_fields WHERE id = 2",
            .values = nonstrict_extra_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "nonstrict extra row",
        }
    );

    failures += write_text_file((struct text_file){.path = load_path, .contents = "7\t\n"});
    failures += build_load_sql(
        sql,
        sizeof(sql),
        (struct load_data_sql){.file_path = load_path, .tail = "INTO TABLE strict_nulls"}
    );
    failures += expect_load_ok(
        database,
        sql,
        (struct expected_load_result){.affected_rows = 1, .warning_count = 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM strict_nulls WHERE id = 7",
            .values = nonstrict_invalid_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "nonstrict invalid integer",
        }
    );

    failures += execute_ok(database, "SET sql_mode = 'STRICT_TRANS_TABLES'", &result);
    mylite_result_free(result);
    result = NULL;
    failures += write_text_file((struct text_file){.path = load_path, .contents = "8\tbad\n"});
    failures += build_load_sql(
        sql,
        sizeof(sql),
        (struct load_data_sql){.file_path = load_path, .tail = "INTO TABLE strict_nulls"}
    );
    failures += execute_error(
        database,
        sql,
        (struct expected_sql_error){
            .code = mysql_error_truncated_wrong_value,
            .sqlstate = "HY000",
            .message_part = "Incorrect integer value",
        }
    );
    failures += write_text_file((struct text_file){.path = load_path, .contents = "9\t\\N\n"});
    failures += execute_error(
        database,
        sql,
        (struct expected_sql_error){
            .code = mysql_error_load_data_null_to_not_null,
            .sqlstate = "22004",
            .message_part = "NULL supplied to NOT NULL",
        }
    );

    failures += execute_error(
        database,
        "LOAD DATA LOCAL INFILE '/tmp/mylite-disabled-local.tsv' INTO TABLE strict_fields",
        (struct expected_sql_error){
            .code = mysql_error_load_data_local_disabled,
            .sqlstate = "42000",
            .message_part = "Loading local data is disabled",
        }
    );

    mylite_close(database);
    remove_related_files(database_path);
    (void)remove(load_path);
    (void)remove(missing_path);

    return failures;
}

static int test_load_data_independent_handles(void) {
    static const char *const first_rows[] = {"1", "one"};
    static const char *const second_rows[] = {"2", "two"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    char first_import[test_path_capacity];
    char second_import[test_path_capacity];
    char sql[sql_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "independent_first", ".mylite") != 0 ||
        make_test_path(second_path, sizeof(second_path), "independent_second", ".mylite") != 0 ||
        make_test_path(first_import, sizeof(first_import), "independent_first", ".tsv") != 0 ||
        make_test_path(second_import, sizeof(second_import), "independent_second", ".tsv") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);
    (void)remove(first_import);
    (void)remove(second_import);

    failures += write_text_file((struct text_file){.path = first_import, .contents = "1\tone\n"});
    failures += write_text_file((struct text_file){.path = second_import, .contents = "2\ttwo\n"});
    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first handle");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second handle");
    failures += seed_schema(first, "app");
    failures += seed_schema(second, "app");
    failures += execute_ok(first, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(second, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(first, "CREATE TABLE loaded (id INT, body VARCHAR(20))", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(second, "CREATE TABLE loaded (id INT, body VARCHAR(20))", &result);
    mylite_result_free(result);
    result = NULL;

    failures += build_load_sql(
        sql,
        sizeof(sql),
        (struct load_data_sql){.file_path = first_import, .tail = "INTO TABLE loaded"}
    );
    failures += expect_load_ok(
        first,
        sql,
        (struct expected_load_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += build_load_sql(
        sql,
        sizeof(sql),
        (struct load_data_sql){.file_path = second_import, .tail = "INTO TABLE loaded"}
    );
    failures += expect_load_ok(
        second,
        sql,
        (struct expected_load_result){.affected_rows = 1, .warning_count = 0U}
    );

    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT id, body FROM loaded",
            .values = first_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "first handle rows",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT id, body FROM loaded",
            .values = second_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "second handle rows",
        }
    );

    mylite_close(second);
    mylite_close(first);
    remove_related_files(first_path);
    remove_related_files(second_path);
    (void)remove(first_import);
    (void)remove(second_import);

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

static int expect_load_ok(
    mylite_db *database,
    const char *sql,
    struct expected_load_result expected
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, "load data column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "load data row count");
    failures += expect_int64(
        mylite_result_affected_rows(result),
        expected.affected_rows,
        "load data affected"
    );
    failures += expect_size(
        mylite_result_warning_count(result),
        expected.warning_count,
        "load data warnings"
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

static int make_test_path(char *path, size_t path_size, const char *name, const char *suffix) {
    const char *directory = getenv("TMPDIR");
    const char *separator = "/";
    int written = 0;

    if (directory == NULL || directory[0] == '\0') {
        directory = getenv("TEMP");
    }
    if (directory == NULL || directory[0] == '\0') {
        directory = ".";
    }
    if (directory[strlen(directory) - 1U] == '/') {
        separator = "";
    }

    written = snprintf(
        path,
        path_size,
        "%s%smylite_load_data_infile_%d_%s%s",
        directory,
        separator,
        current_process_id(),
        name,
        suffix
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

static int write_text_file(struct text_file file_data) {
    FILE *file = fopen(file_data.path, "wb");
    size_t text_length = strlen(file_data.contents);

    if (file == NULL) {
        fprintf(stderr, "failed to open %s for writing\n", file_data.path);
        return 1;
    }
    if (fwrite(file_data.contents, 1U, text_length, file) != text_length) {
        fprintf(stderr, "failed to write %s\n", file_data.path);
        (void)fclose(file);
        return 1;
    }
    if (fclose(file) != 0) {
        fprintf(stderr, "failed to close %s\n", file_data.path);
        return 1;
    }

    return 0;
}

static int build_load_sql(char *sql, size_t sql_size, struct load_data_sql load_sql) {
    char escaped_path[test_path_capacity * 2U];
    int written = 0;

    if (escape_sql_string(escaped_path, sizeof(escaped_path), load_sql.file_path) != 0) {
        fprintf(stderr, "LOAD DATA path is too long for %s\n", load_sql.file_path);
        return 1;
    }

    written = snprintf(sql, sql_size, "LOAD DATA INFILE '%s' %s", escaped_path, load_sql.tail);

    if (written < 0 || (size_t)written >= sql_size) {
        fprintf(stderr, "LOAD DATA SQL is too long for %s\n", load_sql.file_path);
        return 1;
    }

    return 0;
}

static int escape_sql_string(char *output, size_t output_size, const char *input) {
    size_t read_offset = 0U;
    size_t write_offset = 0U;

    while (input[read_offset] != '\0') {
        if (input[read_offset] == '\\' || input[read_offset] == '\'') {
            if (write_offset + 1U >= output_size) {
                return 1;
            }
            output[write_offset] = '\\';
            ++write_offset;
        }
        if (write_offset + 1U >= output_size) {
            return 1;
        }
        output[write_offset] = input[read_offset];
        ++write_offset;
        ++read_offset;
    }

    output[write_offset] = '\0';
    return 0;
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

static int expect_true(int condition, const char *context) {
    if (!condition) {
        fprintf(stderr, "%s: expected true\n", context);
        return 1;
    }

    return 0;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected text '%s', got '%s'\n",
            context,
            expected == NULL ? "(null)" : expected,
            actual == NULL ? "(null)" : actual
        );
        return 1;
    }

    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || needle == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected '%s' to contain '%s'\n",
            context,
            actual == NULL ? "(null)" : actual,
            needle == NULL ? "(null)" : needle
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
        fprintf(stderr, "%s: byte comparison failed\n", context);
        return 1;
    }

    return 0;
}
