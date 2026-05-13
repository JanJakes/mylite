#include <mylite/mylite.h>

#include "runtime/mylite_catalog.h"
#include "runtime/mylite_connection.h"
#include "storage/mylite_file_format.h"

#include <inttypes.h>
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
    show_create_sql_capacity = 256,
    show_create_column_count = 2,
    mysql_error_parse = 1064,
    mysql_error_no_database_selected = 1046,
    mysql_error_unknown_database = 1049,
    mysql_error_incorrect_database_name = 1102,
    mysql_error_incorrect_table_name = 1103,
    mysql_error_unknown_character_set = 1115,
    mysql_error_table_does_not_exist = 1146,
    mysql_error_collation_not_valid_for_character_set = 1253,
    mysql_error_unknown_collation = 1273,
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

struct alter_form {
    const char *sql;
    const char *context;
};

struct show_create_expectation {
    const char *show_sql;
    const char *table_name;
    const char *expected_collation;
    const char *context;
};

static int test_alter_table_default_charset_success_persistence_and_preamble(void);
static int test_alter_table_default_charset_diagnostics(void);
static int test_independent_alter_table_default_charset_handles(void);
static int expect_alter_ok(mylite_db *database, struct alter_form form);
static int expect_show_create_single_int(
    mylite_db *database,
    struct show_create_expectation expected
);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int execute_error_with_length(
    mylite_db *database,
    const char *sql,
    size_t sql_size,
    struct expected_sql_error expected,
    const char *context
);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_uint64(uint64_t actual, uint64_t expected, const char *context);
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

    failures += test_alter_table_default_charset_success_persistence_and_preamble();
    failures += test_alter_table_default_charset_diagnostics();
    failures += test_independent_alter_table_default_charset_handles();

    return failures == 0 ? 0 : 1;
}

static int test_alter_table_default_charset_success_persistence_and_preamble(void) {
    static const struct alter_form forms[] = {
        {"ALTER TABLE target DEFAULT CHARSET=utf8mb4", "default charset"},
        {"ALTER TABLE target DEFAULT CHARACTER SET utf8mb4", "default character set"},
        {
            "ALTER TABLE target DEFAULT CHARACTER SET=utf8mb4",
            "default character set equal",
        },
        {"ALTER TABLE target CHARACTER SET=utf8mb4", "character set equal"},
        {"ALTER TABLE target CHARSET utf8mb4", "charset space"},
        {"ALTER TABLE target COLLATE=utf8mb4_0900_ai_ci", "collate only"},
        {"ALTER TABLE target DEFAULT COLLATE utf8mb4_0900_ai_ci", "default collate"},
        {
            "ALTER TABLE target DEFAULT CHARSET='utf8mb4' "
            "COLLATE=\"utf8mb4_0900_ai_ci\"",
            "string option names",
        },
        {
            "ALTER TABLE target DEFAULT CHARSET=`utf8mb4` "
            "COLLATE=`utf8mb4_0900_ai_ci`",
            "quoted option names",
        },
        {
            "ALTER TABLE target DEFAULT CHARSET=UTF8MB4 COLLATE=UTF8MB4_0900_AI_CI",
            "uppercase option names",
        },
        {
            "ALTER TABLE target DEFAULT CHARSET=utf8mb4 CHARSET=utf8mb4",
            "duplicate fixed charset",
        },
    };
    static const char *const rows[] = {"1", "2"};
    static const char *const status_values[] = {"0", "0", "0"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    const struct mylite_catalog *catalog = NULL;
    const struct mylite_session_state *session = NULL;
    struct mylite_catalog_schema_descriptor schema = {0};
    struct mylite_catalog_table_descriptor before_table = {0};
    struct mylite_catalog_table_descriptor after_table = {0};
    uint64_t catalog_generation_before = 0U;
    uint64_t sqlite_generation_before = 0U;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "success") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open success file");
    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");
    failures += execute_statement_ok(database, "CREATE TABLE target (id INT)");
    failures += execute_statement_ok(database, "CREATE TABLE qualified_target (id INT)");
    failures += execute_statement_ok(database, "CREATE TABLE rename_target (id INT)");
    failures += execute_statement_ok(database, "CREATE TABLE drop_target (id INT)");
    failures += execute_statement_ok(database, "INSERT INTO target VALUES (1), (2)");

    failures += expect_int(
        mylite_catalog_read_schema_by_name(database, "app", &schema),
        MYLITE_OK,
        "read app schema"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "target", &before_table),
        MYLITE_OK,
        "read target table before alter"
    );
    catalog = mylite_connection_catalog_for_test(database);
    session = mylite_connection_session_state(database);
    if (catalog != NULL) {
        catalog_generation_before = catalog->generation;
    }
    if (session != NULL) {
        sqlite_generation_before = session->sqlite_schema_generation;
    }

    for (size_t form_index = 0U; form_index < sizeof(forms) / sizeof(forms[0]); ++form_index) {
        failures += expect_alter_ok(database, forms[form_index]);
    }
    failures += expect_alter_ok(
        database,
        (struct alter_form){
            .sql = "ALTER TABLE app.qualified_target DEFAULT CHARSET=utf8mb4",
            .context = "schema-qualified alter",
        }
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "target", &after_table),
        MYLITE_OK,
        "read target table after alter"
    );
    failures += expect_int64(after_table.table_id, before_table.table_id, "table id unchanged");
    failures += expect_text(
        after_table.physical_name,
        before_table.physical_name,
        "physical name unchanged"
    );
    failures += expect_uint64(
        after_table.descriptor_version,
        before_table.descriptor_version,
        "alter preserves table descriptor version"
    );
    failures += expect_uint64(
        after_table.updated_catalog_generation,
        before_table.updated_catalog_generation,
        "alter preserves table updated generation"
    );
    catalog = mylite_connection_catalog_for_test(database);
    session = mylite_connection_session_state(database);
    if (catalog != NULL) {
        failures += expect_uint64(
            catalog->generation,
            catalog_generation_before,
            "alter preserves catalog generation"
        );
    }
    if (session != NULL) {
        failures += expect_uint64(
            session->sqlite_schema_generation,
            sqlite_generation_before,
            "alter preserves SQLite schema generation"
        );
    }
    before_table = after_table;
    catalog = mylite_connection_catalog_for_test(database);
    if (catalog != NULL) {
        catalog_generation_before = catalog->generation;
    }
    failures += expect_alter_ok(
        database,
        (struct alter_form){
            .sql = "ALTER TABLE target COLLATE=utf8mb4_unicode_ci",
            .context = "legacy unicode collation",
        }
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "target", &after_table),
        MYLITE_OK,
        "read target table after collation change"
    );
    failures += expect_text(after_table.default_charset, "utf8mb4", "alter stores table charset");
    failures += expect_text(
        after_table.default_collation,
        "utf8mb4_unicode_ci",
        "alter stores table collation"
    );
    failures += expect_uint64(
        after_table.descriptor_version,
        before_table.descriptor_version + 1U,
        "alter bumps table descriptor version"
    );
    failures += expect_uint64(
        after_table.updated_catalog_generation,
        catalog_generation_before + 1U,
        "alter updates table generation"
    );
    catalog = mylite_connection_catalog_for_test(database);
    if (catalog != NULL) {
        failures += expect_uint64(
            catalog->generation,
            catalog_generation_before + 1U,
            "alter updates catalog generation"
        );
    }
    session = mylite_connection_session_state(database);
    if (session != NULL) {
        failures += expect_uint64(
            session->sqlite_schema_generation,
            sqlite_generation_before,
            "collation alter preserves SQLite schema generation"
        );
    }
    before_table = after_table;
    catalog = mylite_connection_catalog_for_test(database);
    if (catalog != NULL) {
        catalog_generation_before = catalog->generation;
    }
    failures += expect_alter_ok(
        database,
        (struct alter_form){
            .sql = "ALTER TABLE target DEFAULT CHARSET=utf8mb4",
            .context = "charset-only alter resets default collation",
        }
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "target", &after_table),
        MYLITE_OK,
        "read target table after charset-only reset"
    );
    failures +=
        expect_text(after_table.default_charset, "utf8mb4", "charset-only alter keeps utf8mb4");
    failures += expect_text(
        after_table.default_collation,
        "utf8mb4_0900_ai_ci",
        "charset-only alter resets table collation"
    );
    failures += expect_uint64(
        after_table.descriptor_version,
        before_table.descriptor_version + 1U,
        "charset-only alter reset bumps descriptor version"
    );
    failures += expect_uint64(
        after_table.updated_catalog_generation,
        catalog_generation_before + 1U,
        "charset-only alter reset updates table generation"
    );
    catalog = mylite_connection_catalog_for_test(database);
    if (catalog != NULL) {
        failures += expect_uint64(
            catalog->generation,
            catalog_generation_before + 1U,
            "charset-only alter reset updates catalog generation"
        );
    }
    session = mylite_connection_session_state(database);
    if (session != NULL) {
        failures += expect_uint64(
            session->sqlite_schema_generation,
            sqlite_generation_before,
            "charset-only alter reset preserves SQLite schema generation"
        );
    }
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count, @@error_count, ROW_COUNT()",
            .values = status_values,
            .column_count = 3U,
            .row_count = 1U,
            .context = "alter status variables",
        }
    );
    failures +=
        execute_statement_ok(database, "ALTER TABLE rename_target RENAME TO renamed_target");
    failures += execute_error(
        database,
        "ALTER TABLE rename_target DEFAULT CHARSET=utf8mb4",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.rename_target' doesn't exist",
        }
    );
    failures += expect_alter_ok(
        database,
        (struct alter_form){
            .sql = "ALTER TABLE renamed_target DEFAULT CHARSET=utf8mb4",
            .context = "alter renamed table",
        }
    );
    failures += execute_statement_ok(database, "DROP TABLE drop_target");
    failures += execute_error(
        database,
        "ALTER TABLE drop_target DEFAULT CHARSET=utf8mb4",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.drop_target' doesn't exist",
        }
    );

    failures += expect_show_create_single_int(
        database,
        (struct show_create_expectation){
            .show_sql = "SHOW CREATE TABLE target",
            .table_name = "target",
            .expected_collation = "utf8mb4_0900_ai_ci",
            .context = "show create",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM target ORDER BY id",
            .values = rows,
            .column_count = 1U,
            .row_count = 2U,
            .context = "alter preserves rows",
        }
    );

    mylite_close(database);
    database = NULL;

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "alter preserves preamble"
    );

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen success file");
    failures += execute_statement_ok(database, "USE app");
    failures += expect_show_create_single_int(
        database,
        (struct show_create_expectation){
            .show_sql = "SHOW CREATE TABLE target",
            .table_name = "target",
            .expected_collation = "utf8mb4_0900_ai_ci",
            .context = "reopened show create",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM target ORDER BY id",
            .values = rows,
            .column_count = 1U,
            .row_count = 2U,
            .context = "reopened rows",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_alter_table_default_charset_diagnostics(void) {
    static const char raw_nul_charset_sql[] = "ALTER TABLE target DEFAULT CHARSET='utf8"
                                              "\0"
                                              "mb4'";
    static const char raw_nul_collation_sql[] = "ALTER TABLE target COLLATE=`utf8mb4"
                                                "\0"
                                                "_0900_ai_ci`";
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics file");
    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_error(
        database,
        "ALTER TABLE target DEFAULT CHARSET=utf8mb4",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_statement_ok(database, "USE app");
    failures += execute_statement_ok(database, "CREATE TABLE target (id INT)");

    failures += execute_error(
        database,
        "ALTER TABLE missing_schema.target DEFAULT CHARSET=utf8mb4",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'missing_schema'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE missing DEFAULT CHARSET=utf8mb4",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE _mylite_reserved.target DEFAULT CHARSET=utf8mb4",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_database_name,
            .sqlstate = "42000",
            .message_part = "Incorrect database name '_mylite_reserved'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE _mylite_reserved DEFAULT CHARSET=utf8mb4",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name '_mylite_reserved'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE target DEFAULT CHARSET=nosuch_charset",
        (struct expected_sql_error){
            .code = mysql_error_unknown_character_set,
            .sqlstate = "42000",
            .message_part = "Unknown character set: 'nosuch_charset'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE target DEFAULT CHARSET=latin1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_character_set,
            .sqlstate = "42000",
            .message_part = "Unknown character set: 'latin1'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE target DEFAULT CHARSET=latin1 COLLATE=utf8mb4_unicode_ci",
        (struct expected_sql_error){
            .code = mysql_error_collation_not_valid_for_character_set,
            .sqlstate = "42000",
            .message_part =
                "COLLATION 'utf8mb4_unicode_ci' is not valid for CHARACTER SET 'latin1'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE target DEFAULT CHARSET=utf8mb4 COLLATE=latin1_swedish_ci",
        (struct expected_sql_error){
            .code = mysql_error_collation_not_valid_for_character_set,
            .sqlstate = "42000",
            .message_part =
                "COLLATION 'latin1_swedish_ci' is not valid for CHARACTER SET 'utf8mb4'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE target COLLATE=nosuch_collation",
        (struct expected_sql_error){
            .code = mysql_error_unknown_collation,
            .sqlstate = "HY000",
            .message_part = "Unknown collation: 'nosuch_collation'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE target DEFAULT CHARSET=utf8mb4 CHARSET=latin1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_character_set,
            .sqlstate = "42000",
            .message_part = "Unknown character set: 'latin1'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE target DEFAULT CHARSET='utf8\\0mb4'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "table character set names do not support NUL bytes",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE target COLLATE='utf8mb4\\0_0900_ai_ci'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "table collation names do not support NUL bytes",
        }
    );
    failures += execute_error_with_length(
        database,
        raw_nul_charset_sql,
        sizeof(raw_nul_charset_sql) - 1U,
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "table character set names do not support NUL bytes",
        },
        "raw NUL charset"
    );
    failures += execute_error_with_length(
        database,
        raw_nul_collation_sql,
        sizeof(raw_nul_collation_sql) - 1U,
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "table collation names do not support NUL bytes",
        },
        "raw NUL collation"
    );
    failures += execute_error(
        database,
        "ALTER TABLE target ENGINE=InnoDB",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_independent_alter_table_default_charset_handles(void) {
    static const char *const first_rows[] = {"1"};
    static const char *const second_rows[] = {"2"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "independent-first") != 0 ||
        make_test_path(second_path, sizeof(second_path), "independent-second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first file");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second file");
    failures += execute_statement_ok(first, "CREATE DATABASE app");
    failures += execute_statement_ok(first, "USE app");
    failures += execute_statement_ok(first, "CREATE TABLE target (id INT)");
    failures += execute_statement_ok(first, "INSERT INTO target VALUES (1)");
    failures += execute_statement_ok(second, "CREATE DATABASE app");
    failures += execute_statement_ok(second, "USE app");
    failures += execute_statement_ok(second, "CREATE TABLE target (id INT)");
    failures += execute_statement_ok(second, "INSERT INTO target VALUES (2)");
    failures += expect_alter_ok(
        first,
        (struct alter_form){
            .sql = "ALTER TABLE target DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin",
            .context = "first handle alter",
        }
    );
    failures += expect_show_create_single_int(
        first,
        (struct show_create_expectation){
            .show_sql = "SHOW CREATE TABLE target",
            .table_name = "target",
            .expected_collation = "utf8mb4_bin",
            .context = "first handle show create",
        }
    );
    failures += expect_show_create_single_int(
        second,
        (struct show_create_expectation){
            .show_sql = "SHOW CREATE TABLE target",
            .table_name = "target",
            .context = "second handle show create",
        }
    );
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT id FROM target",
            .values = first_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "first handle rows",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT id FROM target",
            .values = second_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "second handle rows",
        }
    );

    mylite_close(second);
    mylite_close(first);
    remove_related_files(second_path);
    remove_related_files(first_path);
    return failures;
}

static int expect_alter_ok(mylite_db *database, struct alter_form form) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, form.sql, &result);

    if (result == NULL) {
        return failures + 1;
    }

    failures += expect_size(mylite_result_column_count(result), 0U, form.context);
    failures += expect_size(mylite_result_row_count(result), 0U, form.context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, form.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, form.context);

    mylite_result_free(result);
    return failures;
}

static int expect_show_create_single_int(
    mylite_db *database,
    struct show_create_expectation expected
) {
    char create_sql[show_create_sql_capacity];
    const char *collation =
        expected.expected_collation == NULL ? "utf8mb4_0900_ai_ci" : expected.expected_collation;
    int written = snprintf(
        create_sql,
        sizeof(create_sql),
        "CREATE TABLE `%s` (\n"
        "  `id` int DEFAULT NULL\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=%s",
        expected.table_name,
        collation
    );
    const char *const values[show_create_column_count] = {expected.table_name, create_sql};

    if (written < 0 || (size_t)written >= sizeof(create_sql)) {
        fprintf(stderr, "%s: failed to build expected SHOW CREATE TABLE text\n", expected.context);
        return 1;
    }

    return expect_query_values(
        database,
        (struct expected_query){
            .sql = expected.show_sql,
            .values = values,
            .column_count = show_create_column_count,
            .row_count = 1U,
            .context = expected.context,
        }
    );
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    if (result == NULL) {
        return failures + 1;
    }

    failures += expect_size(mylite_result_column_count(result), query.column_count, query.context);
    failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
    for (size_t row = 0U; row < query.row_count; ++row) {
        for (size_t column = 0U; column < query.column_count; ++column) {
            failures += expect_result_value(
                result,
                row,
                column,
                query.values[(row * query.column_count) + column],
                query.context
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

    if (actual == NULL && expected == NULL) {
        return 0;
    }
    if (actual != NULL && expected != NULL && strcmp(actual, expected) == 0) {
        return 0;
    }

    fprintf(
        stderr,
        "%s: expected value [%s] at %zu/%zu, got [%s]\n",
        context,
        expected == NULL ? "NULL" : expected,
        row,
        column,
        actual == NULL ? "NULL" : actual
    );
    return 1;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected success, got %d/%s: %s\n",
            sql,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        return 1;
    }
    if (out_result == NULL || *out_result == NULL) {
        fprintf(stderr, "%s: expected result object\n", sql);
        return 1;
    }

    return 0;
}

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    mylite_result_free(result);
    return failures;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    return execute_error_with_length(database, sql, strlen(sql), expected, sql);
}

static int execute_error_with_length(
    mylite_db *database,
    const char *sql,
    size_t sql_size,
    struct expected_sql_error expected,
    const char *context
) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, sql_size, &result);
    int failures = 0;

    if (rc == MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected error %d/%s, got success\n",
            context,
            expected.code,
            expected.sqlstate
        );
        mylite_result_free(result);
        return 1;
    }

    failures += expect_int(mylite_errcode(database), expected.code, context);
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, context);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, context);
    mylite_result_free(result);

    return failures;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_alter_table_default_charset_%s_%d.mylite",
        name,
        current_process_id()
    );

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "failed to build test path for %s\n", name);
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
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related_path[test_path_capacity];
    int written = snprintf(related_path, sizeof(related_path), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(related_path)) {
        remove(related_path);
    }
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    int failures = 0;

    if (file == NULL) {
        fprintf(stderr, "%s: failed to open file\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "%s: failed to seek to %ld\n", path, offset);
        failures += 1;
    } else if (fread(buffer, 1U, size, file) != size) {
        fprintf(stderr, "%s: failed to read %zu bytes\n", path, size);
        failures += 1;
    }
    fclose(file);
    return failures;
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
    return 1;
}

static int expect_int64(int64_t actual, int64_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(stderr, "%s: expected %" PRId64 ", got %" PRId64 "\n", context, expected, actual);
    return 1;
}

static int expect_uint64(uint64_t actual, uint64_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(stderr, "%s: expected %" PRIu64 ", got %" PRIu64 "\n", context, expected, actual);
    return 1;
}

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
    return 1;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual != NULL && expected != NULL && strcmp(actual, expected) == 0) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected text [%s], got [%s]\n",
        context,
        expected == NULL ? "NULL" : expected,
        actual == NULL ? "NULL" : actual
    );
    return 1;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual != NULL && needle != NULL && strstr(actual, needle) != NULL) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected text [%s] to contain [%s]\n",
        context,
        actual == NULL ? "NULL" : actual,
        needle == NULL ? "NULL" : needle
    );
    return 1;
}

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (memcmp(actual, expected, size) == 0) {
        return 0;
    }
    fprintf(stderr, "%s: byte buffer differs\n", context);
    return 1;
}
