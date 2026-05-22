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
    show_index_field_count = 15,
    mysql_error_parse = 1064,
    mysql_error_row_is_referenced = 1451,
    mysql_error_no_referenced_row = 1452,
    mysql_error_no_database_selected = 1046,
    mysql_error_unknown_database = 1049,
    mysql_error_duplicate_key = 1062,
    mysql_error_incorrect_table_name = 1103,
    mysql_error_table_does_not_exist = 1146,
    mysql_error_key_does_not_exist = 1176,
    mysql_error_algorithm_not_supported = 1845,
    mysql_error_primary_key_index_invisible = 3522,
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

static int test_index_visibility_success_metadata_and_persistence(void);
static int test_index_visibility_rename_and_foreign_key_enforcement(void);
static int test_index_visibility_diagnostics(void);
static int test_catalog_v22_index_visibility_migration(void);
static int test_independent_visibility_handles(void);
static int seed_visibility_schema(mylite_db *database);
static int make_catalog_look_like_v22(sqlite3 *sqlite);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_statement_result(
    mylite_db *database,
    const char *sql,
    struct expected_statement expected
);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_single_value_contains(mylite_db *database, struct expected_contains_query query);
static int expect_single_value_not_contains(
    mylite_db *database,
    struct expected_contains_query query
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
static int execute_sql(sqlite3 *connection, const char *sql);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_uint64(uint64_t actual, uint64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_true(int condition, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);
static int expect_not_contains(const char *actual, const char *needle, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_index_visibility_success_metadata_and_persistence();
    failures += test_index_visibility_rename_and_foreign_key_enforcement();
    failures += test_index_visibility_diagnostics();
    failures += test_catalog_v22_index_visibility_migration();
    failures += test_independent_visibility_handles();

    return failures == 0 ? 0 : 1;
}

static int test_index_visibility_success_metadata_and_persistence(void) {
    static const char *const show_index_rows[] = {
        "t",     "0", "PRIMARY", "1",   "id",  "A",        "0", NULL,      NULL, "",
        "BTREE", "",  "",        "YES", NULL,  "t",        "0", "u_a",     "1",  "a",
        "A",     "0", NULL,      NULL,  "",    "BTREE",    "",  "",        "NO", NULL,
        "t",     "1", "k_b",     "1",   "b",   "A",        "0", NULL,      NULL, "YES",
        "BTREE", "",  "",        "NO",  NULL,  "t",        "1", "ft_body", "1",  "body",
        NULL,    "0", NULL,      NULL,  "YES", "FULLTEXT", "",  "",        "NO", NULL,
    };
    static const char *const show_index_rows_after_visible[] = {
        "t",     "0", "PRIMARY", "1",   "id",  "A",        "0", NULL,      NULL, "",
        "BTREE", "",  "",        "YES", NULL,  "t",        "0", "u_a",     "1",  "a",
        "A",     "0", NULL,      NULL,  "",    "BTREE",    "",  "",        "NO", NULL,
        "t",     "1", "k_b",     "1",   "b",   "A",        "0", NULL,      NULL, "YES",
        "BTREE", "",  "",        "YES", NULL,  "t",        "1", "ft_body", "1",  "body",
        NULL,    "0", NULL,      NULL,  "YES", "FULLTEXT", "",  "",        "NO", NULL,
    };
    static const char *const k_b_visible[] = {"NO"};
    static const char *const k_b_revisible[] = {"YES"};
    static const char *const ft_visible[] = {"NO"};
    static const char *const reopened_visible[] = {"u_a", "NO", "k_b", "NO", "ft_body", "NO"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    const struct mylite_session_state *session = NULL;
    uint64_t sqlite_schema_generation = 0U;
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "success") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open success file");
    failures += seed_visibility_schema(database);
    session = mylite_connection_session_state(database);
    sqlite_schema_generation = session == NULL ? 0U : session->sqlite_schema_generation;

    failures += expect_statement_ok(database, "ALTER TABLE t ALTER INDEX k_b INVISIBLE");
    failures += expect_statement_result(
        database,
        "ALTER TABLE t ALTER INDEX u_a INVISIBLE, ALGORITHM=COPY",
        (struct expected_statement){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_statement_ok(database, "ALTER TABLE t ALTER INDEX ft_body INVISIBLE");
    failures += expect_statement_ok(database, "ALTER TABLE t ALTER INDEX k_b INVISIBLE");
    session = mylite_connection_session_state(database);
    failures += expect_uint64(
        session == NULL ? 0U : session->sqlite_schema_generation,
        sqlite_schema_generation,
        "index visibility preserves SQLite schema generation"
    );

    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM t",
            .values = show_index_rows,
            .column_count = show_index_field_count,
            .row_count = 4U,
            .context = "SHOW INDEX after invisible indexes",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT IS_VISIBLE FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 't' AND INDEX_NAME = 'k_b'",
            .values = k_b_visible,
            .column_count = 1U,
            .row_count = 1U,
            .context = "INFORMATION_SCHEMA.STATISTICS secondary visibility",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT IS_VISIBLE FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 't' AND INDEX_NAME = 'ft_body'",
            .values = ft_visible,
            .column_count = 1U,
            .row_count = 1U,
            .context = "INFORMATION_SCHEMA.STATISTICS fulltext visibility",
        }
    );
    failures += expect_single_value_contains(
        database,
        (struct expected_contains_query){
            .sql = "SHOW CREATE TABLE t",
            .needle = "UNIQUE KEY `u_a` (`a`) /*!80000 INVISIBLE */",
            .context = "SHOW CREATE renders invisible unique index",
        }
    );
    failures += expect_single_value_contains(
        database,
        (struct expected_contains_query){
            .sql = "SHOW CREATE TABLE t",
            .needle = "KEY `k_b` (`b`) /*!80000 INVISIBLE */",
            .context = "SHOW CREATE renders invisible secondary index",
        }
    );
    failures += expect_single_value_contains(
        database,
        (struct expected_contains_query){
            .sql = "SHOW CREATE TABLE t",
            .needle = "FULLTEXT KEY `ft_body` (`body`) /*!80000 INVISIBLE */",
            .context = "SHOW CREATE renders invisible fulltext index",
        }
    );
    failures += expect_statement_ok(database, "ALTER TABLE t ALTER INDEX k_b VISIBLE");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM t",
            .values = show_index_rows_after_visible,
            .column_count = show_index_field_count,
            .row_count = 4U,
            .context = "SHOW INDEX after secondary index visible",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT IS_VISIBLE FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 't' AND INDEX_NAME = 'k_b'",
            .values = k_b_revisible,
            .column_count = 1U,
            .row_count = 1U,
            .context = "INFORMATION_SCHEMA.STATISTICS secondary visible again",
        }
    );
    failures += expect_single_value_not_contains(
        database,
        (struct expected_contains_query){
            .sql = "SHOW CREATE TABLE t",
            .needle = "KEY `k_b` (`b`) /*!80000 INVISIBLE */",
            .context = "SHOW CREATE omits invisible comment after VISIBLE",
        }
    );
    failures += expect_statement_ok(database, "ALTER TABLE t ALTER INDEX k_b INVISIBLE");
    failures += execute_error(
        database,
        "INSERT INTO t VALUES (3, 10, 30, 'dup')",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '10' for key 't.u_a'",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE clone LIKE t");
    failures += expect_single_value_contains(
        database,
        (struct expected_contains_query){
            .sql = "SHOW CREATE TABLE clone",
            .needle = "KEY `k_b` (`b`) /*!80000 INVISIBLE */",
            .context = "CREATE TABLE LIKE preserves invisible secondary index",
        }
    );

    mylite_close(database);
    database = NULL;
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures +=
        expect_bytes(actual_preamble, expected_preamble, sizeof(expected_preamble), "preamble");

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen success file");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT INDEX_NAME, IS_VISIBLE FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 't' AND INDEX_NAME <> 'PRIMARY'",
            .values = reopened_visible,
            .column_count = 2U,
            .row_count = 3U,
            .context = "index visibility persists after reopen",
        }
    );
    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_index_visibility_rename_and_foreign_key_enforcement(void) {
    static const char *const renamed_visible[] = {"renamed_v", "NO"};
    static const char *const foreign_index_visible[] = {"fk_pid", "NO", "u_code", "NO"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "rename_fk") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open rename/fk file");
    failures += expect_statement_result(
        database,
        "CREATE DATABASE app",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE rename_t (id INT PRIMARY KEY, v INT, KEY k_v(v))"
    );
    failures += expect_statement_ok(database, "ALTER TABLE rename_t ALTER INDEX k_v INVISIBLE");
    failures += expect_statement_ok(database, "ALTER TABLE rename_t RENAME INDEX k_v TO renamed_v");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT INDEX_NAME, IS_VISIBLE FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'rename_t' "
                   "AND INDEX_NAME <> 'PRIMARY'",
            .values = renamed_visible,
            .column_count = 2U,
            .row_count = 1U,
            .context = "renamed index keeps invisible metadata",
        }
    );
    failures += expect_single_value_contains(
        database,
        (struct expected_contains_query){
            .sql = "SHOW CREATE TABLE rename_t",
            .needle = "KEY `renamed_v` (`v`) /*!80000 INVISIBLE */",
            .context = "SHOW CREATE renders renamed invisible index",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE parent (id INT PRIMARY KEY, code INT NOT NULL, UNIQUE KEY u_code(code))"
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE child ("
        "id INT PRIMARY KEY, pid INT, code INT, KEY fk_pid(pid), "
        "CONSTRAINT fk_pid_parent FOREIGN KEY(pid) REFERENCES parent(id), "
        "CONSTRAINT fk_code_parent FOREIGN KEY(code) REFERENCES parent(code))"
    );
    failures += expect_statement_ok(database, "ALTER TABLE child ALTER INDEX fk_pid INVISIBLE");
    failures += expect_statement_ok(database, "ALTER TABLE parent ALTER INDEX u_code INVISIBLE");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT INDEX_NAME, IS_VISIBLE FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND "
                   "((TABLE_NAME = 'parent' AND INDEX_NAME = 'u_code') OR "
                   "(TABLE_NAME = 'child' AND INDEX_NAME = 'fk_pid')) "
                   "ORDER BY INDEX_NAME",
            .values = foreign_index_visible,
            .column_count = 2U,
            .row_count = 2U,
            .context = "foreign key indexes are invisible",
        }
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO parent VALUES (1, 10)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO child VALUES (1, 1, 10)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += execute_error(
        database,
        "INSERT INTO child VALUES (2, 999, 10)",
        (struct expected_sql_error){
            .code = mysql_error_no_referenced_row,
            .sqlstate = "23000",
            .message_part = "child row",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO child VALUES (3, 1, 999)",
        (struct expected_sql_error){
            .code = mysql_error_no_referenced_row,
            .sqlstate = "23000",
            .message_part = "child row",
        }
    );
    failures += execute_error(
        database,
        "DELETE FROM parent WHERE id = 1",
        (struct expected_sql_error){
            .code = mysql_error_row_is_referenced,
            .sqlstate = "23000",
            .message_part = "parent row",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_index_visibility_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics file");
    failures += execute_error(
        database,
        "ALTER TABLE t ALTER INDEX k_b INVISIBLE",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += seed_visibility_schema(database);
    failures += execute_error(
        database,
        "ALTER TABLE missing_schema.t ALTER INDEX k_b INVISIBLE",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'missing_schema'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE missing_table ALTER INDEX k_b INVISIBLE",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing_table' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE t ALTER INDEX missing_idx INVISIBLE",
        (struct expected_sql_error){
            .code = mysql_error_key_does_not_exist,
            .sqlstate = "42000",
            .message_part = "Key 'missing_idx' doesn't exist in table 't'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE t ALTER INDEX `PRIMARY` INVISIBLE",
        (struct expected_sql_error){
            .code = mysql_error_primary_key_index_invisible,
            .sqlstate = "HY000",
            .message_part = "A primary key index cannot be invisible",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE t ALTER INDEX `PRIMARY` VISIBLE",
        (struct expected_sql_error){
            .code = mysql_error_primary_key_index_invisible,
            .sqlstate = "HY000",
            .message_part = "A primary key index cannot be invisible",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE t ALTER INDEX PRIMARY INVISIBLE",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE t ALTER KEY k_b INVISIBLE",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE _mylite_internal ALTER INDEX k_b INVISIBLE",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name '_mylite_internal'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE t ALTER INDEX k_b INVISIBLE, ALGORITHM=INSTANT",
        (struct expected_sql_error){
            .code = mysql_error_algorithm_not_supported,
            .sqlstate = "0A000",
            .message_part = "ALGORITHM=INSTANT is not supported for this operation",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_catalog_v22_index_visibility_migration(void) {
    static const char *const visible_rows[] = {"YES"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    sqlite3 *sqlite = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "migration") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open migration file");
    failures += seed_visibility_schema(database);
    sqlite = mylite_connection_sqlite_for_test(database);
    failures += make_catalog_look_like_v22(sqlite);
    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen migration file");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT IS_VISIBLE FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 't' AND INDEX_NAME = 'k_b'",
            .values = visible_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "v22 migration defaults existing indexes visible",
        }
    );
    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_independent_visibility_handles(void) {
    static const char *const first_rows[] = {"NO"};
    static const char *const second_rows[] = {"YES"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "independent_a") != 0 ||
        make_test_path(second_path, sizeof(second_path), "independent_b") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first file");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second file");
    failures += seed_visibility_schema(first);
    failures += seed_visibility_schema(second);
    failures += expect_statement_ok(first, "ALTER TABLE t ALTER INDEX k_b INVISIBLE");
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT IS_VISIBLE FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 't' AND INDEX_NAME = 'k_b'",
            .values = first_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "first handle invisible index",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT IS_VISIBLE FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 't' AND INDEX_NAME = 'k_b'",
            .values = second_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "second handle keeps visible index",
        }
    );

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);
    return failures;
}

static int seed_visibility_schema(mylite_db *database) {
    int failures = 0;

    failures += expect_statement_result(
        database,
        "CREATE DATABASE app",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE t ("
        "id INT NOT NULL, a INT NOT NULL, b INT, body TEXT, "
        "PRIMARY KEY(id), UNIQUE KEY u_a(a), KEY k_b(b), FULLTEXT KEY ft_body(body))"
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO t VALUES (1, 10, 100, 'alpha'), (2, 20, NULL, 'beta')",
        (struct expected_statement){.affected_rows = 2, .warning_count = 0U}
    );

    return failures;
}

static int make_catalog_look_like_v22(sqlite3 *sqlite) {
    int failures = 0;

    failures += execute_sql(
        sqlite,
        "ALTER TABLE _mylite_catalog_indexes DROP COLUMN show_create_explicit_btree"
    );
    failures += execute_sql(sqlite, "ALTER TABLE _mylite_catalog_indexes DROP COLUMN comment");
    failures += execute_sql(sqlite, "ALTER TABLE _mylite_catalog_indexes DROP COLUMN is_visible");
    failures += execute_sql(sqlite, "ALTER TABLE _mylite_catalog_columns DROP COLUMN comment");
    failures += execute_sql(sqlite, "ALTER TABLE _mylite_catalog_tables DROP COLUMN comment");
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
        "SET schema_version = 22, minimum_reader_schema_version = 22"
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

static int expect_statement_ok(mylite_db *database, const char *sql) {
    return expect_statement_result(
        database,
        sql,
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
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

static int expect_single_value_not_contains(
    mylite_db *database,
    struct expected_contains_query query
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    failures += expect_size(mylite_result_column_count(result), 2U, query.context);
    failures += expect_size(mylite_result_row_count(result), 1U, query.context);
    failures +=
        expect_not_contains(mylite_result_value_text(result, 0U, 1U), query.needle, query.context);
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
        "%s/mylite_alter_index_visibility_%d_%s.mylite",
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

static int expect_not_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || strstr(actual, needle) != NULL) {
        fprintf(
            stderr,
            "%s: expected '%s' not to contain '%s'\n",
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
