#include <mylite/mylite.h>

#include "runtime/mylite_connection.h"
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
    show_columns_field_count = 6,
    defaults_column_count = 6,
    escaped_alter_column_count = 3,
    reopened_column_count = 5,
    information_schema_column_count = 7,
    mysql_error_parse = 1064,
    mysql_error_invalid_default = 1067,
    mysql_error_blob_text_cant_have_default = 1101,
    mysql_error_bad_null = 1048,
    mysql_error_data_too_long = 1406,
    mysql_error_no_default = 1364,
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

struct expected_dml_result {
    int64_t affected_rows;
    size_t warning_count;
};

static int test_string_defaults_success_persistence_and_introspection(void);
static int test_character_expression_defaults(void);
static int test_catalog_v28_character_expression_migration(void);
static int test_catalog_v33_scalar_expression_migration(void);
static int test_string_defaults_diagnostics(void);
static int test_string_defaults_independent_handles(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows);
static int expect_dml_result(
    mylite_db *database,
    const char *sql,
    struct expected_dml_result expected
);
static int expect_query_values(mylite_db *database, struct expected_query query);
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

    failures += test_string_defaults_success_persistence_and_introspection();
    failures += test_character_expression_defaults();
    failures += test_catalog_v28_character_expression_migration();
    failures += test_catalog_v33_scalar_expression_migration();
    failures += test_string_defaults_diagnostics();
    failures += test_string_defaults_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_string_defaults_success_persistence_and_introspection(void) {
    static const char *const show_columns_rows[] = {
        "id",  "int",        "NO",  "", NULL, "", "vc",  "varchar(3)", "YES", "", "ab", "",
        "vnn", "varchar(3)", "NO",  "", "",   "", "v0",  "varchar(0)", "YES", "", "",   "",
        "c",   "char(3)",    "YES", "", "xy", "", "cnn", "char(3)",    "NO",  "", "q",  "",
    };
    static const char *const show_create_rows[] = {
        "defaults",
        "CREATE TABLE `defaults` (\n"
        "  `id` int NOT NULL,\n"
        "  `vc` varchar(3) DEFAULT 'ab',\n"
        "  `vnn` varchar(3) NOT NULL DEFAULT '',\n"
        "  `v0` varchar(0) DEFAULT '',\n"
        "  `c` char(3) DEFAULT 'xy',\n"
        "  `cnn` char(3) NOT NULL DEFAULT 'q'\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const information_schema_rows[] = {
        "id",  "int",     NULL, NULL, NULL, "NO",  "int",
        "vc",  "varchar", "3",  "12", "ab", "YES", "varchar(3)",
        "vnn", "varchar", "3",  "12", "",   "NO",  "varchar(3)",
        "v0",  "varchar", "0",  "0",  "",   "YES", "varchar(0)",
        "c",   "char",    "3",  "12", "xy", "YES", "char(3)",
        "cnn", "char",    "3",  "12", "q",  "NO",  "char(3)",
    };
    static const char *const default_rows[] = {
        "1",
        "ab",
        "",
        "",
        "xy",
        "q",
        "2",
        "ab",
        "",
        "",
        "xy",
        "q",
    };
    static const char *const update_replace_rows[] = {"1", "ab", "", "3", "ab", ""};
    static const char *const added_rows[] = {
        "1",
        "hey",
        "2",
        "hey",
        "3",
        "hey",
    };
    static const char *const added_escaped_rows[] = {
        "1",
        "a'b",
        "x\ny",
        "2",
        "a'b",
        "x\ny",
        "3",
        "a'b",
        "x\ny",
    };
    static const char *const set_default_row[] = {"vc", "varchar(3)", "YES", "", "zz", ""};
    static const char *const inserted_set_default_row[] = {"4", "zz"};
    static const char *const dropped_default_row[] = {"vc", "varchar(3)", "YES", "", NULL, ""};
    static const char *const clone_show_create_rows[] = {
        "like_defaults",
        "CREATE TABLE `like_defaults` (\n"
        "  `id` int NOT NULL,\n"
        "  `vc` varchar(3),\n"
        "  `vnn` varchar(3) NOT NULL DEFAULT '',\n"
        "  `v0` varchar(0) DEFAULT '',\n"
        "  `c` char(3) DEFAULT 'xy',\n"
        "  `cnn` char(3) NOT NULL DEFAULT 'q',\n"
        "  `added` varchar(3) NOT NULL DEFAULT 'hey',\n"
        "  `added_quote` varchar(10) NOT NULL DEFAULT 'a''b',\n"
        "  `added_newline` varchar(10) NOT NULL DEFAULT 'x\\ny'\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
        "ctas_defaults",
        "CREATE TABLE `ctas_defaults` (\n"
        "  `vc` varchar(3),\n"
        "  `vnn` varchar(3) NOT NULL DEFAULT '',\n"
        "  `c` char(3) DEFAULT 'xy'\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const escape_show_create_rows[] = {
        "esc",
        "CREATE TABLE `esc` (\n"
        "  `v` varchar(10) DEFAULT 'a''b',\n"
        "  `w` varchar(10) DEFAULT 'x\\ny'\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const escape_value_rows[] = {"a'b", "x\ny"};
    static const char *const reopened_rows[] = {"4", "zz", "hey", "a'b", "x\ny"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "success") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open success file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE defaults (id INT NOT NULL, vc VARCHAR(3) DEFAULT 'ab', "
        "vnn VARCHAR(3) NOT NULL DEFAULT '', v0 VARCHAR(0) DEFAULT '', "
        "c CHAR(3) DEFAULT 'xy ', cnn CHAR(3) NOT NULL DEFAULT 'q')"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM defaults",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = defaults_column_count,
            .context = "string defaults SHOW COLUMNS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "DESCRIBE defaults",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = defaults_column_count,
            .context = "string defaults DESCRIBE",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "EXPLAIN defaults",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = defaults_column_count,
            .context = "string defaults EXPLAIN table",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE defaults",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "string defaults SHOW CREATE TABLE",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, "
                   "CHARACTER_OCTET_LENGTH, COLUMN_DEFAULT, IS_NULLABLE, COLUMN_TYPE "
                   "FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA='app' "
                   "AND TABLE_NAME='defaults' ORDER BY ORDINAL_POSITION",
            .values = information_schema_rows,
            .column_count = information_schema_column_count,
            .row_count = defaults_column_count,
            .context = "string defaults information schema",
        }
    );

    failures += expect_dml_ok(database, "INSERT INTO defaults (id) VALUES (1)", 1);
    failures += expect_dml_ok(
        database,
        "INSERT INTO defaults VALUES (2, DEFAULT, DEFAULT, DEFAULT, DEFAULT, DEFAULT)",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, vc, vnn, v0, c, cnn FROM defaults ORDER BY id",
            .values = default_rows,
            .column_count = defaults_column_count,
            .row_count = 2U,
            .context = "string defaults DML materialization",
        }
    );
    failures += expect_dml_ok(database, "UPDATE defaults SET vnn='zz' WHERE id=1", 1);
    failures += expect_dml_ok(database, "UPDATE defaults SET vnn=DEFAULT WHERE id=1", 1);
    failures += expect_dml_ok(database, "REPLACE INTO defaults (id) VALUES (3)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, vc, vnn FROM defaults WHERE id IN (1, 3) ORDER BY id",
            .values = update_replace_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "string defaults UPDATE and REPLACE",
        }
    );

    failures += expect_statement_ok(
        database,
        "ALTER TABLE defaults ADD COLUMN added VARCHAR(3) NOT NULL DEFAULT 'hey'"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, added FROM defaults ORDER BY id",
            .values = added_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "string defaults ALTER ADD",
        }
    );
    failures += expect_statement_ok(
        database,
        "ALTER TABLE defaults ADD COLUMN added_quote VARCHAR(10) NOT NULL DEFAULT 'a''b'"
    );
    failures += expect_statement_ok(
        database,
        "ALTER TABLE defaults ADD COLUMN added_newline VARCHAR(10) NOT NULL DEFAULT 'x\\ny'"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, added_quote, added_newline FROM defaults ORDER BY id",
            .values = added_escaped_rows,
            .column_count = escaped_alter_column_count,
            .row_count = 3U,
            .context = "string defaults ALTER ADD escaped backfill",
        }
    );
    failures +=
        expect_statement_ok(database, "ALTER TABLE defaults ALTER COLUMN vc SET DEFAULT 'zz'");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM defaults LIKE 'vc'",
            .values = set_default_row,
            .column_count = show_columns_field_count,
            .row_count = 1U,
            .context = "string defaults ALTER SET",
        }
    );
    failures += expect_dml_ok(database, "INSERT INTO defaults (id) VALUES (4)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, vc FROM defaults WHERE id=4",
            .values = inserted_set_default_row,
            .column_count = 2U,
            .row_count = 1U,
            .context = "string defaults ALTER SET materialization",
        }
    );
    failures += expect_statement_ok(database, "ALTER TABLE defaults ALTER COLUMN vc DROP DEFAULT");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM defaults LIKE 'vc'",
            .values = dropped_default_row,
            .column_count = show_columns_field_count,
            .row_count = 1U,
            .context = "string defaults ALTER DROP",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO defaults (id) VALUES (5)",
        (struct expected_sql_error){
            .code = mysql_error_no_default,
            .sqlstate = "HY000",
            .message_part = "Field 'vc' doesn't have a default value",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE like_defaults LIKE defaults");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE ctas_defaults AS SELECT vc, vnn, c FROM defaults"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE like_defaults",
            .values = clone_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "string defaults CREATE LIKE",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE ctas_defaults",
            .values = &clone_show_create_rows[2],
            .column_count = 2U,
            .row_count = 1U,
            .context = "string defaults CTAS",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE esc (v VARCHAR(10) DEFAULT 'a''b', w VARCHAR(10) DEFAULT 'x\\ny')"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE esc",
            .values = escape_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "string defaults escaped SHOW CREATE",
        }
    );
    failures += expect_dml_ok(database, "INSERT INTO esc VALUES(DEFAULT, DEFAULT)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT v, w FROM esc",
            .values = escape_value_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "string defaults escaped materialization",
        }
    );

    mylite_close(database);
    database = NULL;
    failures += expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble preserved"
    );

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen string defaults file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, vc, added, added_quote, added_newline FROM defaults WHERE id=4",
            .values = reopened_rows,
            .column_count = reopened_column_count,
            .row_count = 1U,
            .context = "string defaults reopen",
        }
    );
    failures += expect_statement_ok(database, "RENAME TABLE defaults TO renamed_defaults");
    failures += expect_statement_ok(database, "DROP TABLE renamed_defaults");

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_character_expression_defaults(void) {
    enum {
        character_expression_column_count = 6,
        character_expression_metadata_column_count = 3,
    };

    static const char *const show_columns_rows[] = {
        "id", "int",        "NO",  "", NULL,           "",
        "c",  "char(4)",    "YES", "", "_utf8mb4'ab'", "DEFAULT_GENERATED",
        "vc", "varchar(4)", "YES", "", "_utf8mb4'xy'", "DEFAULT_GENERATED",
        "ce", "char(4)",    "YES", "", "_utf8mb4''",   "DEFAULT_GENERATED",
        "v0", "varchar(0)", "YES", "", "_utf8mb4''",   "DEFAULT_GENERATED",
        "vn", "varchar(4)", "YES", "", "NULL",         "DEFAULT_GENERATED",
    };
    static const char *const show_create_rows[] = {
        "character_expr",
        "CREATE TABLE `character_expr` (\n"
        "  `id` int NOT NULL,\n"
        "  `c` char(4) DEFAULT (_utf8mb4'ab'),\n"
        "  `vc` varchar(4) DEFAULT (_utf8mb4'xy'),\n"
        "  `ce` char(4) DEFAULT (_utf8mb4''),\n"
        "  `v0` varchar(0) DEFAULT (_utf8mb4''),\n"
        "  `vn` varchar(4) DEFAULT (NULL)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const information_schema_rows[] = {
        "id",
        NULL,
        "",
        "c",
        "_utf8mb4'ab'",
        "DEFAULT_GENERATED",
        "vc",
        "_utf8mb4'xy'",
        "DEFAULT_GENERATED",
        "ce",
        "_utf8mb4''",
        "DEFAULT_GENERATED",
        "v0",
        "_utf8mb4''",
        "DEFAULT_GENERATED",
        "vn",
        "NULL",
        "DEFAULT_GENERATED",
    };
    static const char *const default_rows[] = {
        "1",
        "ab",
        "xy",
        "",
        "",
        NULL,
        "2",
        "ab",
        "xy",
        "",
        "",
        NULL,
    };
    static const char *const added_rows[] = {"1", "add"};
    static const char *const altered_rows[] = {
        "1",
        "base",
        "add",
        "2",
        "mod",
        "chg",
    };
    static const char *const alter_set_rows[] = {"v", "varchar(5)", "YES", "", "set", ""};
    static const char *const alter_set_show_create_rows[] = {
        "alter_set_character",
        "CREATE TABLE `alter_set_character` (\n"
        "  `v` varchar(5) DEFAULT 'set'\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const function_generated_rows[] = {"ab"};
    static const char *const like_show_create_rows[] = {
        "character_like",
        "CREATE TABLE `character_like` (\n"
        "  `id` int NOT NULL,\n"
        "  `c` char(4) DEFAULT (_utf8mb4'ab'),\n"
        "  `vc` varchar(4) DEFAULT (_utf8mb4'xy'),\n"
        "  `ce` char(4) DEFAULT (_utf8mb4''),\n"
        "  `v0` varchar(0) DEFAULT (_utf8mb4''),\n"
        "  `vn` varchar(4) DEFAULT (NULL)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const reopened_rows[] = {"3", "ab", "xy"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "character-expression") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open character defaults");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE character_expr (id INT NOT NULL, c CHAR(4) DEFAULT ('ab'), "
        "vc VARCHAR(4) DEFAULT ('xy'), ce CHAR(4) DEFAULT (''), "
        "v0 VARCHAR(0) DEFAULT (''), vn VARCHAR(4) DEFAULT (NULL))"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM character_expr",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = character_expression_column_count,
            .context = "character expression defaults SHOW COLUMNS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE character_expr",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "character expression defaults SHOW CREATE",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, COLUMN_DEFAULT, EXTRA FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA='app' AND TABLE_NAME='character_expr' "
                   "ORDER BY ORDINAL_POSITION",
            .values = information_schema_rows,
            .column_count = character_expression_metadata_column_count,
            .row_count = character_expression_column_count,
            .context = "character expression defaults information schema",
        }
    );

    failures += expect_dml_ok(database, "INSERT INTO character_expr (id) VALUES (1)", 1);
    failures += expect_dml_ok(
        database,
        "INSERT INTO character_expr VALUES (2, DEFAULT, DEFAULT, DEFAULT, DEFAULT, DEFAULT)",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, c, vc, ce, v0, vn FROM character_expr ORDER BY id",
            .values = default_rows,
            .column_count = character_expression_column_count,
            .row_count = 2U,
            .context = "character expression defaults materialization",
        }
    );
    failures += expect_dml_ok(database, "UPDATE character_expr SET c='ab', vc='xy'", 0);
    failures += expect_dml_ok(database, "UPDATE character_expr SET c=DEFAULT, vc=DEFAULT", 0);
    failures += expect_dml_ok(database, "REPLACE INTO character_expr (id) VALUES (3)", 1);

    failures += expect_statement_ok(database, "CREATE TABLE character_like LIKE character_expr");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE character_like",
            .values = like_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "character expression defaults CREATE LIKE",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE alter_character (id INT, v VARCHAR(5) DEFAULT ('base'))"
    );
    failures += expect_dml_ok(database, "INSERT INTO alter_character (id) VALUES (1)", 1);
    failures += expect_statement_ok(
        database,
        "ALTER TABLE alter_character ADD COLUMN added VARCHAR(5) DEFAULT ('add')"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, added FROM alter_character ORDER BY id",
            .values = added_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "character expression defaults ALTER ADD",
        }
    );
    failures += expect_statement_ok(
        database,
        "ALTER TABLE alter_character MODIFY COLUMN v VARCHAR(5) DEFAULT ('mod')"
    );
    failures += expect_statement_ok(
        database,
        "ALTER TABLE alter_character CHANGE COLUMN added changed VARCHAR(5) DEFAULT ('chg')"
    );
    failures += expect_dml_ok(database, "INSERT INTO alter_character (id) VALUES (2)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, changed FROM alter_character ORDER BY id",
            .values = altered_rows,
            .column_count = character_expression_metadata_column_count,
            .row_count = 2U,
            .context = "character expression defaults ALTER column definitions",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE alter_set_character (v VARCHAR(5))");
    failures += expect_statement_ok(
        database,
        "ALTER TABLE alter_set_character ALTER COLUMN v SET DEFAULT ('set')"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM alter_set_character",
            .values = alter_set_rows,
            .column_count = show_columns_field_count,
            .row_count = 1U,
            .context = "character expression defaults ALTER SET ordinary metadata",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE alter_set_character",
            .values = alter_set_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "character expression defaults ALTER SET ordinary show create",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE bad_generated_null (v VARCHAR(3) NOT NULL DEFAULT (NULL))"
    );
    failures += execute_error(
        database,
        "INSERT INTO bad_generated_null () VALUES ()",
        (struct expected_sql_error){
            .code = mysql_error_bad_null,
            .sqlstate = "23000",
            .message_part = "cannot be null",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE generated_overlength (v VARCHAR(3) DEFAULT ('abcd'), c CHAR(3) DEFAULT "
        "('abcd'))"
    );
    failures += execute_error(
        database,
        "INSERT INTO generated_overlength () VALUES ()",
        (struct expected_sql_error){
            .code = mysql_error_data_too_long,
            .sqlstate = "22001",
            .message_part = "Data too long for column 'v'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_numeric_generated (v VARCHAR(3) DEFAULT (1 + 2))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'v'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_hex_generated (v VARCHAR(3) DEFAULT (0x41))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'v'",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE function_generated (v VARCHAR(3) DEFAULT (CONCAT('a','b')))"
    );
    failures += expect_dml_ok(database, "INSERT INTO function_generated () VALUES ()", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT v FROM function_generated",
            .values = function_generated_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "character expression defaults CONCAT materialization",
        }
    );

    mylite_close(database);
    database = NULL;
    failures += expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read character expression preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "character expression preamble preserved"
    );

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen character defaults");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, c, vc FROM character_expr WHERE id=3",
            .values = reopened_rows,
            .column_count = character_expression_metadata_column_count,
            .row_count = 1U,
            .context = "character expression defaults reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_catalog_v28_character_expression_migration(void) {
    static const char *const generated_rows[] = {
        "v",
        "varchar(3)",
        "YES",
        "",
        "_utf8mb4'ok'",
        "DEFAULT_GENERATED",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    sqlite3 *sqlite = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "character-expression-migration") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open v28 migration file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(database, "CREATE TABLE existing_defaults (id INT)");
    sqlite = mylite_connection_sqlite_for_test(database);
    failures +=
        execute_sql(sqlite, "ALTER TABLE _mylite_catalog_tables DROP COLUMN row_format_option");
    failures +=
        execute_sql(sqlite, "ALTER TABLE _mylite_catalog_tables DROP COLUMN key_block_size");
    failures += execute_sql(sqlite, "ALTER TABLE _mylite_catalog_tables DROP COLUMN pack_keys");
    failures += execute_sql(sqlite, "ALTER TABLE _mylite_catalog_tables DROP COLUMN checksum");
    failures +=
        execute_sql(sqlite, "ALTER TABLE _mylite_catalog_tables DROP COLUMN stats_persistent");
    failures +=
        execute_sql(sqlite, "ALTER TABLE _mylite_catalog_tables DROP COLUMN stats_auto_recalc");
    failures +=
        execute_sql(sqlite, "ALTER TABLE _mylite_catalog_tables DROP COLUMN stats_sample_pages");
    failures += execute_sql(
        sqlite,
        "UPDATE _mylite_catalog_state "
        "SET schema_version = 28, minimum_reader_schema_version = 28"
    );
    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen v28 migration file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE generated_after_migration (v VARCHAR(3) DEFAULT ('ok'))"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM generated_after_migration",
            .values = generated_rows,
            .column_count = show_columns_field_count,
            .row_count = 1U,
            .context = "v28 migration admits generated character defaults",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_catalog_v33_scalar_expression_migration(void) {
    static const char *const generated_rows[] = {
        "v",
        "varchar(3)",
        "YES",
        "",
        "concat(_utf8mb4\\'a\\',_utf8mb4\\'b\\')",
        "DEFAULT_GENERATED",
    };
    static const char downgrade_catalog_columns_sql[] =
        "BEGIN IMMEDIATE;"
        "ALTER TABLE _mylite_catalog_columns RENAME TO _mylite_catalog_columns_v33_test;"
        "CREATE TABLE _mylite_catalog_columns ("
        "column_id INTEGER PRIMARY KEY,"
        "table_id INTEGER NOT NULL,"
        "ordinal_position INTEGER NOT NULL CHECK(ordinal_position > 0),"
        "name TEXT NOT NULL,"
        "logical_type TEXT NOT NULL,"
        "physical_type TEXT NOT NULL,"
        "is_nullable INTEGER NOT NULL CHECK(is_nullable IN (0, 1)),"
        "is_visible INTEGER NOT NULL CHECK(is_visible IN (0, 1)),"
        "is_auto_increment INTEGER NOT NULL CHECK(is_auto_increment IN (0, 1)),"
        "default_kind INTEGER NOT NULL CHECK(default_kind IN "
        "(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11)),"
        "default_integer INTEGER,"
        "default_text TEXT,"
        "on_update_current_timestamp INTEGER NOT NULL "
        "CHECK(on_update_current_timestamp IN (0, 1)),"
        "character_set_name TEXT NOT NULL,"
        "collation_name TEXT NOT NULL,"
        "comment TEXT NOT NULL,"
        "is_generated INTEGER NOT NULL CHECK(is_generated IN (0, 1)),"
        "generated_kind INTEGER NOT NULL CHECK(generated_kind IN (0, 1, 2)),"
        "generation_expression TEXT NOT NULL,"
        "sqlite_generation_expression TEXT NOT NULL,"
        "descriptor_version INTEGER NOT NULL,"
        "created_catalog_generation INTEGER NOT NULL,"
        "updated_catalog_generation INTEGER NOT NULL,"
        "UNIQUE(table_id, ordinal_position),"
        "UNIQUE(table_id, name)"
        ");"
        "INSERT INTO _mylite_catalog_columns "
        "(column_id, table_id, ordinal_position, name, logical_type, physical_type, "
        "is_nullable, is_visible, is_auto_increment, default_kind, default_integer, "
        "default_text, on_update_current_timestamp, character_set_name, collation_name, "
        "comment, is_generated, generated_kind, generation_expression, "
        "sqlite_generation_expression, descriptor_version, created_catalog_generation, "
        "updated_catalog_generation) "
        "SELECT column_id, table_id, ordinal_position, name, logical_type, physical_type, "
        "is_nullable, is_visible, is_auto_increment, default_kind, default_integer, "
        "default_text, on_update_current_timestamp, character_set_name, collation_name, "
        "comment, is_generated, generated_kind, generation_expression, "
        "sqlite_generation_expression, descriptor_version, created_catalog_generation, "
        "updated_catalog_generation FROM _mylite_catalog_columns_v33_test;"
        "DROP TABLE _mylite_catalog_columns_v33_test;"
        "UPDATE _mylite_catalog_state "
        "SET schema_version = 33, minimum_reader_schema_version = 33;"
        "COMMIT;";
    char path[test_path_capacity];
    mylite_db *database = NULL;
    sqlite3 *sqlite = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "scalar-expression-migration") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open v33 migration file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(database, "CREATE TABLE existing_defaults (id INT)");
    sqlite = mylite_connection_sqlite_for_test(database);
    failures += execute_sql(sqlite, downgrade_catalog_columns_sql);
    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen v33 migration file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE generated_after_migration (v VARCHAR(3) DEFAULT (CONCAT('a','b')))"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM generated_after_migration",
            .values = generated_rows,
            .column_count = show_columns_field_count,
            .row_count = 1U,
            .context = "v33 migration admits scalar expression defaults",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_string_defaults_diagnostics(void) {
    static const char *const numeric_character_show_columns_rows[] = {
        "v", "varchar(3)", "YES", "", "0", "",
        "c", "char(3)",    "YES", "", "1", "",
        "f", "varchar(5)", "YES", "", "0", "",
    };
    static const char *const numeric_character_show_create_rows[] = {
        "numeric_character_defaults",
        "CREATE TABLE `numeric_character_defaults` (\n"
        "  `v` varchar(3) DEFAULT '0',\n"
        "  `c` char(3) DEFAULT '1',\n"
        "  `f` varchar(5) DEFAULT '0'\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const numeric_character_default_rows[] = {"0", "1", "0"};
    mylite_db *database = NULL;
    int failures = expect_int(mylite_open(":memory:", &database), MYLITE_OK, "open diagnostics");

    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(database, "CREATE TABLE diag (vc VARCHAR(3), c CHAR(3))");
    failures += execute_error(
        database,
        "CREATE TABLE bad_null (v VARCHAR(3) NOT NULL DEFAULT NULL)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'v'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_over (v VARCHAR(3) DEFAULT 'abcd')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'v'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_v0 (v VARCHAR(0) DEFAULT 'x')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'v'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_c0 (c CHAR(0) DEFAULT 'x')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'c'",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE numeric_character_defaults (v VARCHAR(3) DEFAULT 0, "
        "c CHAR(3) DEFAULT TRUE, f VARCHAR(5) DEFAULT FALSE)"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM numeric_character_defaults",
            .values = numeric_character_show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = 3U,
            .context = "numeric character defaults SHOW COLUMNS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE numeric_character_defaults",
            .values = numeric_character_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "numeric character defaults SHOW CREATE",
        }
    );
    failures += expect_dml_ok(database, "INSERT INTO numeric_character_defaults () VALUES ()", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT v, c, f FROM numeric_character_defaults",
            .values = numeric_character_default_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "numeric character defaults materialization",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_text (t TEXT DEFAULT 'abc')",
        (struct expected_sql_error){
            .code = mysql_error_blob_text_cant_have_default,
            .sqlstate = "42000",
            .message_part = "can't have a default value",
        }
    );
    failures +=
        expect_statement_ok(database, "CREATE TABLE generated_ok (v VARCHAR(3) DEFAULT ('a'))");
    failures += execute_error(
        database,
        "ALTER TABLE diag ALTER COLUMN vc SET DEFAULT 'abcd'",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'vc'",
        }
    );
    failures += expect_statement_ok(database, "ALTER TABLE diag ALTER COLUMN c SET DEFAULT 1");

    mylite_close(database);
    return failures;
}

static int test_string_defaults_independent_handles(void) {
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    static const char *const first_expected[] = {"aa"};
    static const char *const second_expected[] = {"bb"};
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "first") != 0 ||
        make_test_path(second_path, sizeof(second_path), "second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first handle");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second handle");
    failures += expect_statement_ok(first, "CREATE DATABASE app");
    failures += expect_statement_ok(second, "CREATE DATABASE app");
    failures += expect_statement_ok(first, "USE app");
    failures += expect_statement_ok(second, "USE app");
    failures += expect_statement_ok(first, "CREATE TABLE t (id INT, v VARCHAR(3) DEFAULT 'aa')");
    failures += expect_statement_ok(second, "CREATE TABLE t (id INT, v VARCHAR(3) DEFAULT 'bb')");
    failures += expect_dml_ok(first, "INSERT INTO t (id) VALUES (1)", 1);
    failures += expect_dml_ok(second, "INSERT INTO t (id) VALUES (1)", 1);
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT v FROM t WHERE id = 1",
            .values = first_expected,
            .column_count = 1U,
            .row_count = 1U,
            .context = "first independent string default",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT v FROM t WHERE id = 1",
            .values = second_expected,
            .column_count = 1U,
            .row_count = 1U,
            .context = "second independent string default",
        }
    );

    mylite_close(second);
    mylite_close(first);
    remove_related_files(second_path);
    remove_related_files(first_path);
    return failures;
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

static int expect_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, "statement column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "statement row count");
    failures += expect_size(mylite_result_warning_count(result), 0U, "statement warning count");
    mylite_result_free(result);

    return failures;
}

static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows) {
    return expect_dml_result(
        database,
        sql,
        (struct expected_dml_result){
            .affected_rows = affected_rows,
            .warning_count = 0U,
        }
    );
}

static int expect_dml_result(
    mylite_db *database,
    const char *sql,
    struct expected_dml_result expected
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, "DML column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "DML row count");
    failures +=
        expect_int64(mylite_result_affected_rows(result), expected.affected_rows, "DML affected");
    failures +=
        expect_size(mylite_result_warning_count(result), expected.warning_count, "DML warnings");
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
        "%s/mylite_string_defaults_%d_%s.mylite",
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

static int execute_sql(sqlite3 *connection, const char *sql) {
    char *message = NULL;
    int rc = sqlite3_exec(connection, sql, NULL, NULL, &message);

    if (rc != SQLITE_OK) {
        fprintf(
            stderr,
            "sqlite execute failed: rc=%d sql=%s message=%s\n",
            rc,
            sql,
            message == NULL ? "(null)" : message
        );
        sqlite3_free(message);
        return 1;
    }

    sqlite3_free(message);
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
