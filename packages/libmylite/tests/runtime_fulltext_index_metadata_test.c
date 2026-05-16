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
    show_columns_field_count = 6,
    show_index_field_count = 15,
    statistics_field_count = 6,
    fulltext_table_column_count = 6,
    fulltext_index_row_count = 6,
    mysql_error_parse = 1064,
    mysql_error_duplicate_key_name = 1061,
    mysql_error_key_column_missing = 1072,
    mysql_error_storage_engine_cant_index_column = 1167,
    mysql_error_incorrect_index_name = 1280,
    mysql_error_fulltext_column = 1283,
    mysql_error_key_part_length_cannot_be_zero = 1391,
    mysql_error_wrong_usage = 1221,
    mysql_error_temporary_fulltext_index = 1796,
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

struct sqlite_exec_request {
    const char *sql;
    const char *context;
};

static int test_fulltext_metadata_persistence_and_operations(void);
static int test_fulltext_mixed_index_show_create_order(void);
static int test_fulltext_catalog_v18_migration(void);
static int test_fulltext_diagnostics(void);
static int test_fulltext_independent_handles(void);
static int create_fulltext_schema(mylite_db *database);
static int make_catalog_look_like_v18(sqlite3 *connection);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_physical_index_count(
    mylite_db *database,
    int expected_count,
    const char *context
);
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
static int execute_sqlite(sqlite3 *connection, struct sqlite_exec_request request);
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

    failures += test_fulltext_metadata_persistence_and_operations();
    failures += test_fulltext_mixed_index_show_create_order();
    failures += test_fulltext_catalog_v18_migration();
    failures += test_fulltext_diagnostics();
    failures += test_fulltext_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_fulltext_metadata_persistence_and_operations(void) {
    static const char *const show_columns_rows[] = {
        "id",   "int",      "YES", "",          NULL,       "",    "title", "varchar(191)", "YES",
        "MUL",  NULL,       "",    "body",      "text",     "YES", "MUL",   NULL,           "",
        "tiny", "tinytext", "YES", "MUL",       NULL,       "",    "med",   "mediumtext",   "YES",
        "",     NULL,       "",    "long_body", "longtext", "YES", "",      NULL,           "",
    };
    static const char *const columns_rows[] = {
        "id",
        "",
        "title",
        "MUL",
        "body",
        "MUL",
        "tiny",
        "MUL",
        "med",
        "",
        "long_body",
        "",
    };
    static const char *const show_index_rows[] = {
        "ft",  "1",         "ft_title_body",
        "1",   "title",     NULL,
        "0",   NULL,        NULL,
        "YES", "FULLTEXT",  "",
        "",    "YES",       NULL,
        "ft",  "1",         "ft_title_body",
        "2",   "body",      NULL,
        "0",   NULL,        NULL,
        "YES", "FULLTEXT",  "",
        "",    "YES",       NULL,
        "ft",  "1",         "ft_body_prefix",
        "1",   "body",      NULL,
        "0",   NULL,        NULL,
        "YES", "FULLTEXT",  "",
        "",    "YES",       NULL,
        "ft",  "1",         "ft_texts",
        "1",   "tiny",      NULL,
        "0",   NULL,        NULL,
        "YES", "FULLTEXT",  "",
        "",    "YES",       NULL,
        "ft",  "1",         "ft_texts",
        "2",   "med",       NULL,
        "0",   NULL,        NULL,
        "YES", "FULLTEXT",  "",
        "",    "YES",       NULL,
        "ft",  "1",         "ft_texts",
        "3",   "long_body", NULL,
        "0",   NULL,        NULL,
        "YES", "FULLTEXT",  "",
        "",    "YES",       NULL,
    };
    static const char *const clone_show_index_rows[] = {
        "ft_clone", "1",         "ft_title_body",
        "1",        "title",     NULL,
        "0",        NULL,        NULL,
        "YES",      "FULLTEXT",  "",
        "",         "YES",       NULL,
        "ft_clone", "1",         "ft_title_body",
        "2",        "body",      NULL,
        "0",        NULL,        NULL,
        "YES",      "FULLTEXT",  "",
        "",         "YES",       NULL,
        "ft_clone", "1",         "ft_body_prefix",
        "1",        "body",      NULL,
        "0",        NULL,        NULL,
        "YES",      "FULLTEXT",  "",
        "",         "YES",       NULL,
        "ft_clone", "1",         "ft_texts",
        "1",        "tiny",      NULL,
        "0",        NULL,        NULL,
        "YES",      "FULLTEXT",  "",
        "",         "YES",       NULL,
        "ft_clone", "1",         "ft_texts",
        "2",        "med",       NULL,
        "0",        NULL,        NULL,
        "YES",      "FULLTEXT",  "",
        "",         "YES",       NULL,
        "ft_clone", "1",         "ft_texts",
        "3",        "long_body", NULL,
        "0",        NULL,        NULL,
        "YES",      "FULLTEXT",  "",
        "",         "YES",       NULL,
    };
    static const char *const renamed_show_create_rows[] = {
        "ft",
        "CREATE TABLE `ft` (\n"
        "  `id` int DEFAULT NULL,\n"
        "  `title` varchar(191) DEFAULT NULL,\n"
        "  `body` text,\n"
        "  `tiny` tinytext,\n"
        "  `med` mediumtext,\n"
        "  `long_body` longtext,\n"
        "  FULLTEXT KEY `ft_title_body` (`title`,`body`),\n"
        "  FULLTEXT KEY `ft_body_text` (`body`),\n"
        "  FULLTEXT KEY `ft_texts` (`tiny`,`med`,`long_body`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const dropped_show_create_rows[] = {
        "ft",
        "CREATE TABLE `ft` (\n"
        "  `id` int DEFAULT NULL,\n"
        "  `title` varchar(191) DEFAULT NULL,\n"
        "  `body` text,\n"
        "  `tiny` tinytext,\n"
        "  `med` mediumtext,\n"
        "  `long_body` longtext,\n"
        "  FULLTEXT KEY `ft_title_body` (`title`,`body`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const dropped_show_index_rows[] = {
        "ft",  "1",        "ft_title_body",
        "1",   "title",    NULL,
        "0",   NULL,       NULL,
        "YES", "FULLTEXT", "",
        "",    "YES",      NULL,
        "ft",  "1",        "ft_title_body",
        "2",   "body",     NULL,
        "0",   NULL,       NULL,
        "YES", "FULLTEXT", "",
        "",    "YES",      NULL,
    };
    static const char *const show_create_rows[] = {
        "ft",
        "CREATE TABLE `ft` (\n"
        "  `id` int DEFAULT NULL,\n"
        "  `title` varchar(191) DEFAULT NULL,\n"
        "  `body` text,\n"
        "  `tiny` tinytext,\n"
        "  `med` mediumtext,\n"
        "  `long_body` longtext,\n"
        "  FULLTEXT KEY `ft_title_body` (`title`,`body`),\n"
        "  FULLTEXT KEY `ft_body_prefix` (`body`),\n"
        "  FULLTEXT KEY `ft_texts` (`tiny`,`med`,`long_body`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const generated_show_create_rows[] = {
        "generated_names",
        "CREATE TABLE `generated_names` (\n"
        "  `body` text,\n"
        "  FULLTEXT KEY `body` (`body`),\n"
        "  FULLTEXT KEY `body_2` (`body`),\n"
        "  FULLTEXT KEY `body_3` (`body`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const statistics_rows[] = {
        "ft_body_prefix", "1", "body",      NULL, NULL, "FULLTEXT",
        "ft_texts",       "1", "tiny",      NULL, NULL, "FULLTEXT",
        "ft_texts",       "2", "med",       NULL, NULL, "FULLTEXT",
        "ft_texts",       "3", "long_body", NULL, NULL, "FULLTEXT",
        "ft_title_body",  "1", "title",     NULL, NULL, "FULLTEXT",
        "ft_title_body",  "2", "body",      NULL, NULL, "FULLTEXT",
    };
    static const char *const constraint_count_rows[] = {"0"};
    static const char *const row_values[] = {"1", "alpha", "payload", "tiny", "medium", "long"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "metadata") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open fulltext file");
    failures += create_fulltext_schema(database);
    failures += expect_physical_index_count(database, 0, "fulltext creates no SQLite index");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM ft",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = fulltext_table_column_count,
            .context = "fulltext SHOW COLUMNS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, COLUMN_KEY FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'ft' "
                   "ORDER BY ORDINAL_POSITION",
            .values = columns_rows,
            .column_count = 2U,
            .row_count = fulltext_table_column_count,
            .context = "fulltext INFORMATION_SCHEMA.COLUMNS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM ft",
            .values = show_index_rows,
            .column_count = show_index_field_count,
            .row_count = fulltext_index_row_count,
            .context = "fulltext SHOW INDEX",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT INDEX_NAME, SEQ_IN_INDEX, COLUMN_NAME, COLLATION, "
                   "SUB_PART, INDEX_TYPE FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'ft' "
                   "ORDER BY INDEX_NAME",
            .values = statistics_rows,
            .column_count = statistics_field_count,
            .row_count = fulltext_index_row_count,
            .context = "fulltext INFORMATION_SCHEMA.STATISTICS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'ft'",
            .values = constraint_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "fulltext table constraints",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE ft",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "fulltext SHOW CREATE TABLE",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE generated_names (body TEXT, FULLTEXT (body), FULLTEXT (body), "
        "FULLTEXT (body))"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE generated_names",
            .values = generated_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "generated fulltext names",
        }
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO ft VALUES (1, 'alpha', 'payload', 'tiny', 'medium', 'long')",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, title, body, tiny, med, long_body FROM ft",
            .values = row_values,
            .column_count = fulltext_table_column_count,
            .row_count = 1U,
            .context = "fulltext table rows remain readable",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE ft_clone LIKE ft");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM ft_clone",
            .values = clone_show_index_rows,
            .column_count = show_index_field_count,
            .row_count = fulltext_index_row_count,
            .context = "fulltext clone SHOW INDEX",
        }
    );
    failures +=
        expect_statement_ok(database, "ALTER TABLE ft RENAME INDEX ft_body_prefix TO ft_body_text");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE ft",
            .values = renamed_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "renamed fulltext SHOW CREATE TABLE",
        }
    );
    failures += expect_statement_ok(database, "ALTER TABLE ft DROP INDEX ft_body_text");
    failures += expect_statement_ok(database, "DROP INDEX ft_texts ON ft");
    failures +=
        expect_physical_index_count(database, 0, "dropped fulltext creates no SQLite index");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE ft",
            .values = dropped_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "dropped fulltext SHOW CREATE TABLE",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM ft",
            .values = dropped_show_index_rows,
            .column_count = show_index_field_count,
            .row_count = 2U,
            .context = "dropped fulltext SHOW INDEX",
        }
    );
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after fulltext operations"
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen fulltext file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE ft",
            .values = dropped_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "reopened fulltext SHOW CREATE TABLE",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, title, body, tiny, med, long_body FROM ft",
            .values = row_values,
            .column_count = fulltext_table_column_count,
            .row_count = 1U,
            .context = "reopened fulltext rows",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_fulltext_mixed_index_show_create_order(void) {
    static const char *const show_create_rows[] = {
        "mixed_order",
        "CREATE TABLE `mixed_order` (\n"
        "  `body` text,\n"
        "  `k` int DEFAULT NULL,\n"
        "  KEY `k_body` (`k`),\n"
        "  FULLTEXT KEY `ft_body` (`body`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open(":memory:", &database), MYLITE_OK, "open mixed order db");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE mixed_order (body TEXT, k INT, FULLTEXT KEY ft_body (body), KEY k_body (k))"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE mixed_order",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "mixed ordinary and fulltext SHOW CREATE TABLE order",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_fulltext_catalog_v18_migration(void) {
    static const char *const legacy_show_create_rows[] = {
        "legacy",
        "CREATE TABLE `legacy` (\n"
        "  `id` int DEFAULT NULL,\n"
        "  KEY `k_id` (`id`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    sqlite3 *sqlite = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "migration_v18") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open v18 migration source");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(database, "CREATE TABLE legacy (id INT, KEY k_id (id))");
    if (failures == 0) {
        sqlite = mylite_connection_sqlite_for_test(database);
        failures += make_catalog_look_like_v18(sqlite);
    }
    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open migrated v18 catalog");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE legacy",
            .values = legacy_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "v18 migration preserves secondary indexes",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE migrated_fulltext (body TEXT, FULLTEXT KEY ft_body (body))"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_fulltext_diagnostics(void) {
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open(":memory:", &database), MYLITE_OK, "open diagnostics db");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE good_char_varchar (c CHAR(1), v VARCHAR(1), "
        "FULLTEXT KEY ft_c (c), FULLTEXT KEY ft_v (v))"
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_int (id INT, FULLTEXT KEY ft_id (id))",
        (struct expected_sql_error){
            .code = mysql_error_fulltext_column,
            .sqlstate = "HY000",
            .message_part = "Column 'id' cannot be part of FULLTEXT index",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_blob (payload BLOB, FULLTEXT KEY ft_payload (payload))",
        (struct expected_sql_error){
            .code = mysql_error_fulltext_column,
            .sqlstate = "HY000",
            .message_part = "Column 'payload' cannot be part of FULLTEXT index",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_binary (payload BINARY(4), FULLTEXT KEY ft_payload (payload))",
        (struct expected_sql_error){
            .code = mysql_error_fulltext_column,
            .sqlstate = "HY000",
            .message_part = "Column 'payload' cannot be part of FULLTEXT index",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_char_zero (c CHAR(0), FULLTEXT KEY ft_c (c))",
        (struct expected_sql_error){
            .code = mysql_error_storage_engine_cant_index_column,
            .sqlstate = "42000",
            .message_part = "The used storage engine can't index column 'c'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_varchar_zero (v VARCHAR(0), FULLTEXT KEY ft_v (v))",
        (struct expected_sql_error){
            .code = mysql_error_storage_engine_cant_index_column,
            .sqlstate = "42000",
            .message_part = "The used storage engine can't index column 'v'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_order_asc (body TEXT, FULLTEXT KEY ft_body (body ASC))",
        (struct expected_sql_error){
            .code = mysql_error_wrong_usage,
            .sqlstate = "HY000",
            .message_part =
                "Incorrect usage of spatial/fulltext/hash index and explicit index order",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_order_desc (body TEXT, FULLTEXT KEY ft_body (body DESC))",
        (struct expected_sql_error){
            .code = mysql_error_wrong_usage,
            .sqlstate = "HY000",
            .message_part =
                "Incorrect usage of spatial/fulltext/hash index and explicit index order",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_zero (body TEXT, FULLTEXT KEY ft_body (body(0)))",
        (struct expected_sql_error){
            .code = mysql_error_key_part_length_cannot_be_zero,
            .sqlstate = "HY000",
            .message_part = "Key part 'body' length cannot be 0",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_missing (body TEXT, FULLTEXT KEY ft_missing (missing))",
        (struct expected_sql_error){
            .code = mysql_error_key_column_missing,
            .sqlstate = "42000",
            .message_part = "Key column 'missing' doesn't exist in table",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_duplicate (body TEXT, FULLTEXT KEY ft_body (body), "
        "FULLTEXT KEY ft_body (body))",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key_name,
            .sqlstate = "42000",
            .message_part = "Duplicate key name 'ft_body'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_primary_name (body TEXT, FULLTEXT KEY `PRIMARY` (body))",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_index_name,
            .sqlstate = "42000",
            .message_part = "Incorrect index name 'PRIMARY'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_constraint (body TEXT, CONSTRAINT c FULLTEXT KEY ft_body (body))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "CREATE FULLTEXT INDEX ft_body ON bad_standalone (body)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "CREATE TEMPORARY TABLE bad_temp (body TEXT, FULLTEXT KEY ft_body (body))",
        (struct expected_sql_error){
            .code = mysql_error_temporary_fulltext_index,
            .sqlstate = "HY000",
            .message_part = "Cannot create FULLTEXT index on temporary InnoDB table",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_fulltext_independent_handles(void) {
    static const char *const empty_show_index_rows[] = {0};
    static const char *const show_index_rows[] = {
        "ft",  "1",         "ft_title_body",
        "1",   "title",     NULL,
        "0",   NULL,        NULL,
        "YES", "FULLTEXT",  "",
        "",    "YES",       NULL,
        "ft",  "1",         "ft_title_body",
        "2",   "body",      NULL,
        "0",   NULL,        NULL,
        "YES", "FULLTEXT",  "",
        "",    "YES",       NULL,
        "ft",  "1",         "ft_body_prefix",
        "1",   "body",      NULL,
        "0",   NULL,        NULL,
        "YES", "FULLTEXT",  "",
        "",    "YES",       NULL,
        "ft",  "1",         "ft_texts",
        "1",   "tiny",      NULL,
        "0",   NULL,        NULL,
        "YES", "FULLTEXT",  "",
        "",    "YES",       NULL,
        "ft",  "1",         "ft_texts",
        "2",   "med",       NULL,
        "0",   NULL,        NULL,
        "YES", "FULLTEXT",  "",
        "",    "YES",       NULL,
        "ft",  "1",         "ft_texts",
        "3",   "long_body", NULL,
        "0",   NULL,        NULL,
        "YES", "FULLTEXT",  "",
        "",    "YES",       NULL,
    };
    char left_path[test_path_capacity];
    char right_path[test_path_capacity];
    mylite_db *left = NULL;
    mylite_db *right = NULL;
    int failures = 0;

    if (make_test_path(left_path, sizeof(left_path), "left") != 0 ||
        make_test_path(right_path, sizeof(right_path), "right") != 0) {
        return 1;
    }
    remove_related_files(left_path);
    remove_related_files(right_path);

    failures += expect_int(mylite_open(left_path, &left), MYLITE_OK, "open left fulltext file");
    failures += expect_int(mylite_open(right_path, &right), MYLITE_OK, "open right fulltext file");
    failures += create_fulltext_schema(left);
    failures += create_fulltext_schema(right);
    failures += expect_statement_ok(left, "ALTER TABLE ft DROP INDEX ft_title_body");
    failures += expect_statement_ok(left, "ALTER TABLE ft DROP INDEX ft_body_prefix");
    failures += expect_statement_ok(left, "ALTER TABLE ft DROP INDEX ft_texts");
    failures += expect_query_values(
        left,
        (struct expected_query){
            .sql = "SHOW INDEX FROM ft",
            .values = empty_show_index_rows,
            .column_count = show_index_field_count,
            .row_count = 0U,
            .context = "left fulltext indexes dropped",
        }
    );
    failures += expect_query_values(
        right,
        (struct expected_query){
            .sql = "SHOW INDEX FROM ft",
            .values = show_index_rows,
            .column_count = show_index_field_count,
            .row_count = fulltext_index_row_count,
            .context = "right fulltext indexes remain",
        }
    );

    mylite_close(left);
    mylite_close(right);
    remove_related_files(left_path);
    remove_related_files(right_path);
    return failures;
}

static int create_fulltext_schema(mylite_db *database) {
    int failures = 0;

    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE ft ("
        "id INT, title VARCHAR(191), body TEXT, tiny TINYTEXT, med MEDIUMTEXT, "
        "long_body LONGTEXT, FULLTEXT KEY ft_title_body (title, body), "
        "FULLTEXT INDEX ft_body_prefix (body(10)), FULLTEXT ft_texts (tiny, med, long_body))"
    );

    return failures;
}

static int make_catalog_look_like_v18(sqlite3 *connection) {
    static const char *const sql =
        "BEGIN IMMEDIATE;"
        "ALTER TABLE _mylite_catalog_indexes RENAME TO _mylite_catalog_indexes_v19;"
        "CREATE TABLE _mylite_catalog_indexes ("
        "index_id INTEGER PRIMARY KEY,"
        "table_id INTEGER NOT NULL,"
        "name TEXT NOT NULL,"
        "kind INTEGER NOT NULL CHECK(kind IN (1, 2)),"
        "is_unique INTEGER NOT NULL CHECK(is_unique IN (0, 1)),"
        "physical_name TEXT NOT NULL UNIQUE,"
        "descriptor_version INTEGER NOT NULL,"
        "created_catalog_generation INTEGER NOT NULL,"
        "updated_catalog_generation INTEGER NOT NULL,"
        "UNIQUE(table_id, name)"
        ");"
        "INSERT INTO _mylite_catalog_indexes "
        "(index_id, table_id, name, kind, is_unique, physical_name, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation) "
        "SELECT index_id, table_id, name, kind, is_unique, physical_name, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_indexes_v19;"
        "DROP TABLE _mylite_catalog_indexes_v19;"
        "UPDATE _mylite_catalog_state "
        "SET schema_version = 18, minimum_reader_schema_version = 18;"
        "COMMIT;";

    return execute_sqlite(
        connection,
        (struct sqlite_exec_request){
            .sql = sql,
            .context = "make catalog look like v18",
        }
    );
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

            failures +=
                expect_result_value(result, row, column, query.values[value_index], query.context);
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
    enum { sqlite_use_nul_terminated_string = -1 };

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
        sqlite_use_nul_terminated_string,
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
                "%s: expected NULL at row %zu column %zu, got [%s]\n",
                context,
                row,
                column,
                actual
            );
            return 1;
        }
        return 0;
    }

    return expect_text(actual, expected, context);
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_fulltext_index_%d_%s.mylite",
        current_process_id(),
        name
    );

    if (written < 0 || (size_t)written >= path_size) {
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
    char buffer[test_path_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(buffer)) {
        (void)remove(buffer);
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
        return 1;
    }
    if (fread(buffer, 1U, size, file) != size) {
        fclose(file);
        return 1;
    }
    fclose(file);
    return 0;
}

static int execute_sqlite(sqlite3 *connection, struct sqlite_exec_request request) {
    char *message = NULL;
    int rc = sqlite3_exec(connection, request.sql, NULL, NULL, &message);

    if (rc != SQLITE_OK) {
        fprintf(
            stderr,
            "%s: sqlite rc=%d message=%s\n",
            request.context,
            rc,
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

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected [%s], got [%s]\n",
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
            "%s: expected message containing [%s], got [%s]\n",
            context,
            needle,
            actual == NULL ? "(null)" : actual
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
