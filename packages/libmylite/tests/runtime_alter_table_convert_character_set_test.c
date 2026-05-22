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
    show_create_column_count = 2,
    converted_column_metadata_row_count = 6,
    preserved_target_row_column_count = 5,
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

struct expected_statement {
    const char *sql;
    const char *context;
};

static int test_convert_success_metadata_persistence_and_preamble(void);
static int test_convert_diagnostics(void);
static int test_independent_convert_handles(void);
static int expect_convert_ok(mylite_db *database, struct expected_statement statement);
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

    failures += test_convert_success_metadata_persistence_and_preamble();
    failures += test_convert_diagnostics();
    failures += test_independent_convert_handles();

    return failures == 0 ? 0 : 1;
}

static int test_convert_success_metadata_persistence_and_preamble(void) {
    static const char *const show_target_values[] = {
        "target",
        "CREATE TABLE `target` (\n"
        "  `id` int DEFAULT NULL,\n"
        "  `inherited` varchar(10) COLLATE utf8mb4_unicode_ci DEFAULT NULL,\n"
        "  `ch` char(3) COLLATE utf8mb4_unicode_ci DEFAULT NULL,\n"
        "  `body` text COLLATE utf8mb4_unicode_ci,\n"
        "  `explicit_col` varchar(10) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci "
        "DEFAULT NULL,\n"
        "  `binary_col` varbinary(10) DEFAULT NULL\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci",
    };
    static const char *const column_values[] = {
        "id",
        NULL,
        NULL,
        "inherited",
        "utf8mb4",
        "utf8mb4_unicode_ci",
        "ch",
        "utf8mb4",
        "utf8mb4_unicode_ci",
        "body",
        "utf8mb4",
        "utf8mb4_unicode_ci",
        "explicit_col",
        "utf8mb4",
        "utf8mb4_unicode_ci",
        "binary_col",
        NULL,
        NULL,
    };
    static const char *const table_collation_value[] = {"utf8mb4_unicode_ci"};
    static const char *const row_values[] = {"1", "abc", "xy", "text", "def"};
    static const char *const status_values[] = {"0", "0", "0"};
    static const char *const numeric_show_values[] = {
        "numeric_only",
        "CREATE TABLE `numeric_only` (\n"
        "  `id` int DEFAULT NULL\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const no_selected_show_values[] = {
        "no_selected_default",
        "CREATE TABLE `no_selected_default` (\n"
        "  `v` varchar(10) DEFAULT NULL\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const selected_other_show_values[] = {
        "selected_other_default",
        "CREATE TABLE `selected_other_default` (\n"
        "  `v` varchar(10) COLLATE utf8mb4_general_ci DEFAULT NULL\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci",
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    const struct mylite_catalog *catalog = NULL;
    const struct mylite_session_state *session = NULL;
    struct mylite_catalog_schema_descriptor app_schema = {0};
    struct mylite_catalog_table_descriptor before_table = {0};
    struct mylite_catalog_table_descriptor after_table = {0};
    struct mylite_catalog_column_descriptor inherited_column = {0};
    struct mylite_catalog_column_descriptor explicit_before = {0};
    struct mylite_catalog_column_descriptor explicit_after = {0};
    uint64_t catalog_generation_before = 0U;
    uint64_t sqlite_generation_before = 0U;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "success") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open success file");
    failures += execute_statement_ok(
        database,
        "CREATE DATABASE app DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci"
    );
    failures += execute_statement_ok(
        database,
        "CREATE DATABASE other DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_bin"
    );
    failures += execute_statement_ok(database, "USE app");
    failures += execute_statement_ok(
        database,
        "CREATE TABLE target ("
        "id INT, "
        "inherited VARCHAR(10), "
        "ch CHAR(3), "
        "body TEXT, "
        "explicit_col VARCHAR(10) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin, "
        "binary_col VARBINARY(10)"
        ") DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci"
    );
    failures += execute_statement_ok(
        database,
        "INSERT INTO target(id, inherited, ch, body, explicit_col) "
        "VALUES (1, 'abc', 'xy', 'text', 'def')"
    );
    failures += execute_statement_ok(
        database,
        "CREATE TABLE numeric_only(id INT) DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin"
    );
    failures += execute_statement_ok(
        database,
        "CREATE TABLE other.no_selected_default(v VARCHAR(10)) "
        "DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin"
    );
    failures += execute_statement_ok(
        database,
        "CREATE TABLE other.selected_other_default(v VARCHAR(10)) "
        "DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin"
    );

    failures += expect_int(
        mylite_catalog_read_schema_by_name(database, "app", &app_schema),
        MYLITE_OK,
        "read app schema"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, app_schema.schema_id, "target", &before_table),
        MYLITE_OK,
        "read target before convert"
    );
    failures += expect_int(
        mylite_catalog_read_column_by_name(
            database,
            before_table.table_id,
            "explicit_col",
            &explicit_before
        ),
        MYLITE_OK,
        "read explicit column before convert"
    );
    catalog = mylite_connection_catalog_for_test(database);
    session = mylite_connection_session_state(database);
    if (catalog != NULL) {
        catalog_generation_before = catalog->generation;
    }
    if (session != NULL) {
        sqlite_generation_before = session->sqlite_schema_generation;
    }

    failures += expect_convert_ok(
        database,
        (struct expected_statement){
            .sql = "ALTER TABLE target CONVERT TO CHARACTER SET utf8mb4 "
                   "COLLATE utf8mb4_unicode_ci",
            .context = "convert target",
        }
    );
    failures += expect_convert_ok(
        database,
        (struct expected_statement){
            .sql = "ALTER TABLE numeric_only CONVERT TO CHARSET `utf8mb4`",
            .context = "convert numeric-only table",
        }
    );
    failures += expect_convert_ok(
        database,
        (struct expected_statement){
            .sql = "ALTER TABLE other.selected_other_default CONVERT TO CHARACTER SET DEFAULT",
            .context = "convert schema-qualified table using selected database default",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count, @@error_count, ROW_COUNT()",
            .values = status_values,
            .column_count = 3U,
            .row_count = 1U,
            .context = "convert status variables",
        }
    );

    failures += expect_int(
        mylite_catalog_read_table_by_name(database, app_schema.schema_id, "target", &after_table),
        MYLITE_OK,
        "read target after convert"
    );
    failures += expect_int64(after_table.table_id, before_table.table_id, "table id unchanged");
    failures += expect_text(
        after_table.physical_name,
        before_table.physical_name,
        "physical name unchanged"
    );
    failures += expect_text(after_table.default_charset, "utf8mb4", "target table charset");
    failures +=
        expect_text(after_table.default_collation, "utf8mb4_unicode_ci", "target collation");
    failures += expect_uint64(
        after_table.descriptor_version,
        before_table.descriptor_version + 1U,
        "convert bumps table descriptor version"
    );
    failures += expect_uint64(
        after_table.updated_catalog_generation,
        catalog_generation_before + 1U,
        "convert updates table generation"
    );
    catalog = mylite_connection_catalog_for_test(database);
    if (catalog != NULL) {
        failures += expect_uint64(
            catalog->generation,
            catalog_generation_before + 3U,
            "three convert mutations update catalog generation"
        );
    }
    session = mylite_connection_session_state(database);
    if (session != NULL) {
        failures += expect_uint64(
            session->sqlite_schema_generation,
            sqlite_generation_before,
            "convert preserves SQLite schema generation"
        );
    }
    failures += expect_int(
        mylite_catalog_read_column_by_name(
            database,
            after_table.table_id,
            "inherited",
            &inherited_column
        ),
        MYLITE_OK,
        "read inherited column after convert"
    );
    failures +=
        expect_text(inherited_column.character_set_name, "", "inherited charset remains implicit");
    failures +=
        expect_text(inherited_column.collation_name, "", "inherited collation remains implicit");
    failures += expect_int(
        mylite_catalog_read_column_by_name(
            database,
            after_table.table_id,
            "explicit_col",
            &explicit_after
        ),
        MYLITE_OK,
        "read explicit column after convert"
    );
    failures += expect_text(explicit_after.character_set_name, "utf8mb4", "explicit charset");
    failures +=
        expect_text(explicit_after.collation_name, "utf8mb4_unicode_ci", "explicit collation");
    failures += expect_uint64(
        explicit_after.descriptor_version,
        explicit_before.descriptor_version + 1U,
        "explicit column descriptor version"
    );

    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE target",
            .values = show_target_values,
            .column_count = show_create_column_count,
            .row_count = 1U,
            .context = "show create converted target",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, CHARACTER_SET_NAME, COLLATION_NAME "
                   "FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA='app' AND TABLE_NAME='target' "
                   "ORDER BY ORDINAL_POSITION",
            .values = column_values,
            .column_count = 3U,
            .row_count = converted_column_metadata_row_count,
            .context = "converted column metadata",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_COLLATION FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA='app' AND TABLE_NAME='target'",
            .values = table_collation_value,
            .column_count = 1U,
            .row_count = 1U,
            .context = "converted table metadata",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, inherited, ch, body, explicit_col FROM target",
            .values = row_values,
            .column_count = preserved_target_row_column_count,
            .row_count = 1U,
            .context = "convert preserves rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE numeric_only",
            .values = numeric_show_values,
            .column_count = show_create_column_count,
            .row_count = 1U,
            .context = "numeric-only convert show create",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE other.selected_other_default",
            .values = selected_other_show_values,
            .column_count = show_create_column_count,
            .row_count = 1U,
            .context = "selected database default convert show create",
        }
    );

    failures += execute_statement_ok(database, "ALTER TABLE target RENAME TO converted_target");
    failures += execute_error(
        database,
        "ALTER TABLE target CONVERT TO CHARACTER SET utf8mb4",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.target' doesn't exist",
        }
    );
    failures += expect_convert_ok(
        database,
        (struct expected_statement){
            .sql = "ALTER TABLE converted_target CONVERT TO CHARACTER SET utf8mb4",
            .context = "convert renamed table",
        }
    );
    failures += execute_statement_ok(database, "DROP TABLE converted_target");
    failures += execute_error(
        database,
        "ALTER TABLE converted_target CONVERT TO CHARACTER SET utf8mb4",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.converted_target' doesn't exist",
        }
    );

    mylite_close(database);
    database = NULL;

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "convert preserves preamble"
    );

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen success file");
    failures += expect_convert_ok(
        database,
        (struct expected_statement){
            .sql = "ALTER TABLE other.no_selected_default CONVERT TO CHARACTER SET DEFAULT",
            .context = "convert schema-qualified table using server fallback default",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE other.no_selected_default",
            .values = no_selected_show_values,
            .column_count = show_create_column_count,
            .row_count = 1U,
            .context = "no selected database default convert show create",
        }
    );
    failures += execute_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE numeric_only",
            .values = numeric_show_values,
            .column_count = show_create_column_count,
            .row_count = 1U,
            .context = "reopened numeric-only convert show create",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_convert_diagnostics(void) {
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
        "ALTER TABLE target CONVERT TO CHARACTER SET utf8mb4",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_statement_ok(database, "USE app");
    failures += execute_statement_ok(database, "CREATE TABLE target(v VARCHAR(10))");
    failures += execute_statement_ok(
        database,
        "CREATE TABLE ascii_source(v VARCHAR(10)) DEFAULT CHARSET=ascii"
    );
    failures +=
        execute_statement_ok(database, "CREATE TABLE national_source(v NATIONAL VARCHAR(10))");
    failures += execute_statement_ok(database, "CREATE TABLE enum_source(v ENUM('a', 'b'))");
    failures += execute_statement_ok(database, "CREATE TABLE set_source(v SET('a', 'b'))");

    failures += execute_error(
        database,
        "ALTER TABLE missing_schema.target CONVERT TO CHARACTER SET utf8mb4",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'missing_schema'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE missing CONVERT TO CHARACTER SET utf8mb4",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE _mylite_reserved.target CONVERT TO CHARACTER SET utf8mb4",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_database_name,
            .sqlstate = "42000",
            .message_part = "Incorrect database name '_mylite_reserved'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE _mylite_reserved CONVERT TO CHARACTER SET utf8mb4",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name '_mylite_reserved'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE target CONVERT TO CHARACTER SET nosuch_charset",
        (struct expected_sql_error){
            .code = mysql_error_unknown_character_set,
            .sqlstate = "42000",
            .message_part = "Unknown character set: 'nosuch_charset'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE target CONVERT TO CHARACTER SET utf8mb4 COLLATE nosuch_collation",
        (struct expected_sql_error){
            .code = mysql_error_unknown_collation,
            .sqlstate = "HY000",
            .message_part = "Unknown collation: 'nosuch_collation'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE target CONVERT TO CHARACTER SET utf8mb4 COLLATE ascii_bin",
        (struct expected_sql_error){
            .code = mysql_error_collation_not_valid_for_character_set,
            .sqlstate = "42000",
            .message_part = "COLLATION 'ascii_bin' is not valid for CHARACTER SET 'utf8mb4'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE target CONVERT TO CHARACTER SET ascii",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "ALTER TABLE CONVERT TO CHARACTER SET supports only utf8mb4 target character set",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE target CONVERT TO CHARACTER SET binary",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "ALTER TABLE CONVERT TO CHARACTER SET supports only utf8mb4 target character set",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE ascii_source CONVERT TO CHARACTER SET utf8mb4",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "ALTER TABLE CONVERT TO CHARACTER SET does not yet support "
                            "cross-character-set conversion",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE national_source CONVERT TO CHARACTER SET utf8mb4",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "ALTER TABLE CONVERT TO CHARACTER SET does not yet support national "
                            "character columns",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE enum_source CONVERT TO CHARACTER SET utf8mb4",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "ALTER TABLE CONVERT TO CHARACTER SET does not yet support ENUM or SET columns",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE set_source CONVERT TO CHARACTER SET utf8mb4",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "ALTER TABLE CONVERT TO CHARACTER SET does not yet support ENUM or SET columns",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE target CONVERT TO CHARACTER SET=utf8mb4",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near '='",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE target CONVERT TO CHARACTER SET utf8mb4 COLLATE DEFAULT",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near 'DEFAULT'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE target CONVERT TO CHARACTER SET DEFAULT COLLATE utf8mb4_bin",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near 'COLLATE'",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_independent_convert_handles(void) {
    static const char *const first_show_values[] = {
        "target",
        "CREATE TABLE `target` (\n"
        "  `v` varchar(10) COLLATE utf8mb4_unicode_ci DEFAULT NULL\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci",
    };
    static const char *const second_show_values[] = {
        "target",
        "CREATE TABLE `target` (\n"
        "  `v` varchar(10) DEFAULT NULL\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
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
    failures += execute_statement_ok(first, "CREATE DATABASE app");
    failures += execute_statement_ok(first, "USE app");
    failures += execute_statement_ok(first, "CREATE TABLE target(v VARCHAR(10))");
    failures += execute_statement_ok(second, "CREATE DATABASE app");
    failures += execute_statement_ok(second, "USE app");
    failures += execute_statement_ok(second, "CREATE TABLE target(v VARCHAR(10))");
    failures += expect_convert_ok(
        first,
        (struct expected_statement){
            .sql = "ALTER TABLE target CONVERT TO CHARACTER SET utf8mb4 "
                   "COLLATE utf8mb4_unicode_ci",
            .context = "first handle convert",
        }
    );
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE target",
            .values = first_show_values,
            .column_count = show_create_column_count,
            .row_count = 1U,
            .context = "first handle show create",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE target",
            .values = second_show_values,
            .column_count = show_create_column_count,
            .row_count = 1U,
            .context = "second handle show create",
        }
    );

    mylite_close(second);
    mylite_close(first);
    remove_related_files(second_path);
    remove_related_files(first_path);
    return failures;
}

static int expect_convert_ok(mylite_db *database, struct expected_statement statement) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, statement.sql, &result);

    if (result == NULL) {
        return failures + 1;
    }

    failures += expect_size(mylite_result_column_count(result), 0U, statement.context);
    failures += expect_size(mylite_result_row_count(result), 0U, statement.context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, statement.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, statement.context);

    mylite_result_free(result);
    return failures;
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
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    if (rc == MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected error %d/%s, got success\n",
            sql,
            expected.code,
            expected.sqlstate
        );
        mylite_result_free(result);
        return 1;
    }

    if (mylite_errcode(database) != expected.code ||
        strcmp(mylite_sqlstate(database), expected.sqlstate) != 0) {
        fprintf(
            stderr,
            "%s: actual diagnostic %d/%s: %s\n",
            sql,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
    }
    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);
    return failures;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_alter_table_convert_charset_%s_%d.mylite",
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
