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
    statistics_field_count = 18,
    indexed_table_column_row_count = 7,
    indexed_table_index_row_count = 6,
    indexed_table_physical_index_count = 6,
    filtered_result_column_count = 6,
    mysql_error_parse = 1064,
    mysql_error_duplicate_key_name = 1061,
    mysql_error_key_column_missing = 1072,
    mysql_error_table_does_not_exist = 1146,
    mysql_error_blob_key_without_length = 1170,
    mysql_error_incorrect_index_name = 1280,
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

static int test_secondary_index_metadata_dml_and_persistence(void);
static int test_secondary_index_diagnostics(void);
static int test_secondary_index_independent_handles(void);
static int create_secondary_index_schema(mylite_db *database);
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

    failures += test_secondary_index_metadata_dml_and_persistence();
    failures += test_secondary_index_diagnostics();
    failures += test_secondary_index_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_secondary_index_metadata_dml_and_persistence(void) {
    static const char *const show_columns_rows[] = {
        "id",   "int",  "NO",  "PRI",    NULL,           "",    "v",   "int",     "YES",
        "MUL",  NULL,   "",    "amount", "decimal(5,2)", "YES", "MUL", NULL,      "",
        "d",    "date", "YES", "MUL",    NULL,           "",    "c",   "char(3)", "YES",
        "MUL",  NULL,   "",    "name",   "varchar(20)",  "YES", "MUL", NULL,      "",
        "body", "text", "YES", "",       NULL,           "",
    };
    static const char *const show_index_rows[] = {
        "idx_t", "0", "PRIMARY",  "1",   "id",     "A",     "0", NULL,     NULL,  "",
        "BTREE", "",  "",         "YES", NULL,     "idx_t", "1", "k_v",    "1",   "v",
        "A",     "0", NULL,       NULL,  "YES",    "BTREE", "",  "",       "YES", NULL,
        "idx_t", "1", "k_amount", "1",   "amount", "A",     "0", NULL,     NULL,  "YES",
        "BTREE", "",  "",         "YES", NULL,     "idx_t", "1", "k_date", "1",   "d",
        "A",     "0", NULL,       NULL,  "YES",    "BTREE", "",  "",       "YES", NULL,
        "idx_t", "1", "k_char",   "1",   "c",      "A",     "0", NULL,     NULL,  "YES",
        "BTREE", "",  "",         "YES", NULL,     "idx_t", "1", "k_name", "1",   "name",
        "A",     "0", NULL,       NULL,  "YES",    "BTREE", "",  "",       "YES", NULL,
    };
    static const char *const renamed_show_index_rows[] = {
        "renamed_idx", "0", "PRIMARY",  "1",   "id",
        "A",           "0", NULL,       NULL,  "",
        "BTREE",       "",  "",         "YES", NULL,
        "renamed_idx", "1", "k_v",      "1",   "v",
        "A",           "0", NULL,       NULL,  "YES",
        "BTREE",       "",  "",         "YES", NULL,
        "renamed_idx", "1", "k_amount", "1",   "amount",
        "A",           "0", NULL,       NULL,  "YES",
        "BTREE",       "",  "",         "YES", NULL,
        "renamed_idx", "1", "k_date",   "1",   "d",
        "A",           "0", NULL,       NULL,  "YES",
        "BTREE",       "",  "",         "YES", NULL,
        "renamed_idx", "1", "k_char",   "1",   "c",
        "A",           "0", NULL,       NULL,  "YES",
        "BTREE",       "",  "",         "YES", NULL,
        "renamed_idx", "1", "k_name",   "1",   "name",
        "A",           "0", NULL,       NULL,  "YES",
        "BTREE",       "",  "",         "YES", NULL,
    };
    static const char *const clone_show_index_rows[] = {
        "clone", "0", "PRIMARY",  "1",   "id",     "A",     "0", NULL,     NULL,  "",
        "BTREE", "",  "",         "YES", NULL,     "clone", "1", "k_v",    "1",   "v",
        "A",     "0", NULL,       NULL,  "YES",    "BTREE", "",  "",       "YES", NULL,
        "clone", "1", "k_amount", "1",   "amount", "A",     "0", NULL,     NULL,  "YES",
        "BTREE", "",  "",         "YES", NULL,     "clone", "1", "k_date", "1",   "d",
        "A",     "0", NULL,       NULL,  "YES",    "BTREE", "",  "",       "YES", NULL,
        "clone", "1", "k_char",   "1",   "c",      "A",     "0", NULL,     NULL,  "YES",
        "BTREE", "",  "",         "YES", NULL,     "clone", "1", "k_name", "1",   "name",
        "A",     "0", NULL,       NULL,  "YES",    "BTREE", "",  "",       "YES", NULL,
    };
    static const char *const statistics_rows[] = {
        "def", "app", "idx_t", "1",   "app",   "k_amount", "1", "amount", "A",
        "0",   NULL,  NULL,    "YES", "BTREE", "",         "",  "YES",    NULL,
        "def", "app", "idx_t", "1",   "app",   "k_char",   "1", "c",      "A",
        "0",   NULL,  NULL,    "YES", "BTREE", "",         "",  "YES",    NULL,
        "def", "app", "idx_t", "1",   "app",   "k_date",   "1", "d",      "A",
        "0",   NULL,  NULL,    "YES", "BTREE", "",         "",  "YES",    NULL,
        "def", "app", "idx_t", "1",   "app",   "k_name",   "1", "name",   "A",
        "0",   NULL,  NULL,    "YES", "BTREE", "",         "",  "YES",    NULL,
        "def", "app", "idx_t", "1",   "app",   "k_v",      "1", "v",      "A",
        "0",   NULL,  NULL,    "YES", "BTREE", "",         "",  "YES",    NULL,
        "def", "app", "idx_t", "0",   "app",   "PRIMARY",  "1", "id",     "A",
        "0",   NULL,  NULL,    "",    "BTREE", "",         "",  "YES",    NULL,
    };
    static const char *const show_create_rows[] = {
        "idx_t",
        "CREATE TABLE `idx_t` (\n"
        "  `id` int NOT NULL,\n"
        "  `v` int DEFAULT NULL,\n"
        "  `amount` decimal(5,2) DEFAULT NULL,\n"
        "  `d` date DEFAULT NULL,\n"
        "  `c` char(3) DEFAULT NULL,\n"
        "  `name` varchar(20) DEFAULT NULL,\n"
        "  `body` text,\n"
        "  PRIMARY KEY (`id`),\n"
        "  KEY `k_v` (`v`),\n"
        "  KEY `k_amount` (`amount`),\n"
        "  KEY `k_date` (`d`),\n"
        "  KEY `k_char` (`c`),\n"
        "  KEY `k_name` (`name`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const unnamed_show_create_rows[] = {
        "unnamed_idx",
        "CREATE TABLE `unnamed_idx` (\n"
        "  `id` int DEFAULT NULL,\n"
        "  `v` int DEFAULT NULL,\n"
        "  KEY `v` (`v`),\n"
        "  KEY `v_2` (`v`),\n"
        "  KEY `id` (`id`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const filtered_rows[] = {"1", "10", "12.30", "2024-01-02", "abc", "jan"};
    static const char *const empty_show_index_rows[] = {0};
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

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open secondary index file");
    failures += create_secondary_index_schema(database);
    failures += expect_physical_index_count(
        database,
        indexed_table_physical_index_count,
        "secondary index physical SQLite objects"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM idx_t",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = indexed_table_column_row_count,
            .context = "secondary index SHOW COLUMNS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM idx_t",
            .values = show_index_rows,
            .column_count = show_index_field_count,
            .row_count = indexed_table_index_row_count,
            .context = "secondary index SHOW INDEX",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE idx_t",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "secondary index SHOW CREATE TABLE",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM INFORMATION_SCHEMA.STATISTICS WHERE TABLE_SCHEMA = 'app' "
                   "AND TABLE_NAME = 'idx_t' ORDER BY INDEX_NAME",
            .values = statistics_rows,
            .column_count = statistics_field_count,
            .row_count = indexed_table_index_row_count,
            .context = "secondary index INFORMATION_SCHEMA.STATISTICS",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE unnamed_idx (id INT, v INT, KEY (v), INDEX (v), KEY (id))"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE unnamed_idx",
            .values = unnamed_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "unnamed secondary index names",
        }
    );

    failures += expect_dml_ok(
        database,
        "INSERT INTO idx_t (id, v, amount, d, c, name, body) VALUES "
        "(1, 10, 12.30, '2024-01-02', 'abc', 'jan', 'body'), "
        "(2, NULL, NULL, NULL, NULL, NULL, NULL)",
        2
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, amount, d, c, name FROM idx_t WHERE v = 10",
            .values = filtered_rows,
            .column_count = filtered_result_column_count,
            .row_count = 1U,
            .context = "secondary index table DML remains readable",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE clone LIKE idx_t");
    failures += expect_statement_ok(database, "CREATE TABLE copied AS SELECT id, v FROM idx_t");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM copied",
            .values = empty_show_index_rows,
            .column_count = show_index_field_count,
            .row_count = 0U,
            .context = "CREATE TABLE SELECT omits secondary indexes",
        }
    );
    failures += expect_statement_ok(database, "RENAME TABLE idx_t TO renamed_idx");
    failures += execute_error(
        database,
        "SHOW INDEX FROM idx_t",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.idx_t' doesn't exist",
        }
    );
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after secondary index lifecycle"
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen secondary index file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM renamed_idx",
            .values = renamed_show_index_rows,
            .column_count = show_index_field_count,
            .row_count = indexed_table_index_row_count,
            .context = "renamed secondary indexes persist",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM clone",
            .values = clone_show_index_rows,
            .column_count = show_index_field_count,
            .row_count = indexed_table_index_row_count,
            .context = "CREATE TABLE LIKE clones secondary indexes",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_secondary_index_diagnostics(void) {
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open(":memory:", &database), MYLITE_OK, "open diagnostics db");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");

    failures += execute_error(
        database,
        "CREATE TABLE unknown_key (id INT, KEY k (missing))",
        (struct expected_sql_error){
            .code = mysql_error_key_column_missing,
            .sqlstate = "42000",
            .message_part = "Key column 'missing' doesn't exist in table",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE duplicate_key_name (id INT, v INT, KEY k (id), INDEX k (v))",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key_name,
            .sqlstate = "42000",
            .message_part = "Duplicate key name 'k'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE primary_key_name (id INT, KEY `PRIMARY` (id))",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_index_name,
            .sqlstate = "42000",
            .message_part = "Incorrect index name 'PRIMARY'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE text_key (id INT, body TEXT, KEY k_body (body))",
        (struct expected_sql_error){
            .code = mysql_error_blob_key_without_length,
            .sqlstate = "42000",
            .message_part =
                "BLOB/TEXT column 'body' used in key specification without a key length",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE composite_key (id INT, v INT, KEY k (id, v))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "Secondary indexes support exactly one key column",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE unique_key (id INT, UNIQUE KEY k (id))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE key_prefix (id INT, KEY k (id(4)))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures +=
        expect_statement_ok(database, "CREATE TABLE indexed_rebuild (id INT, v INT, KEY k (v))");
    failures += execute_error(
        database,
        "ALTER TABLE indexed_rebuild ORDER BY id",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "ALTER TABLE ORDER BY does not yet support secondary-index tables",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE indexed_rebuild FORCE",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "ALTER TABLE FORCE does not yet support secondary-index tables",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE indexed_rebuild MODIFY id BIGINT",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "ALTER TABLE MODIFY COLUMN does not yet support secondary-index tables",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE indexed_rebuild CHANGE id changed BIGINT",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "ALTER TABLE CHANGE COLUMN does not yet support secondary-index tables",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_secondary_index_independent_handles(void) {
    static const char *const first_values[] = {"10"};
    static const char *const second_values[] = {"20"};
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

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first handle");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second handle");
    failures += expect_statement_ok(first, "CREATE DATABASE app");
    failures += expect_statement_ok(second, "CREATE DATABASE app");
    failures += expect_statement_ok(first, "USE app");
    failures += expect_statement_ok(second, "USE app");
    failures += expect_statement_ok(first, "CREATE TABLE t (id INT, v INT, KEY k_v (v))");
    failures += expect_statement_ok(second, "CREATE TABLE t (id INT, v INT, KEY k_v (v))");
    failures += expect_dml_ok(first, "INSERT INTO t VALUES (1, 10)", 1);
    failures += expect_dml_ok(second, "INSERT INTO t VALUES (1, 20)", 1);
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT v FROM t WHERE id = 1",
            .values = first_values,
            .column_count = 1U,
            .row_count = 1U,
            .context = "first independent secondary-index table state",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT v FROM t WHERE id = 1",
            .values = second_values,
            .column_count = 1U,
            .row_count = 1U,
            .context = "second independent secondary-index table state",
        }
    );

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);
    return failures;
}

static int create_secondary_index_schema(mylite_db *database) {
    int failures = 0;

    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE idx_t ("
        "id INT NOT NULL, v INT, amount DECIMAL(5,2), d DATE, c CHAR(3), "
        "name VARCHAR(20), body TEXT, PRIMARY KEY (id), KEY k_v (v), "
        "INDEX k_amount (amount), KEY k_date (d), KEY k_char (c), KEY k_name (name))"
    );

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
        "/tmp/mylite_secondary_index_%d_%s.mylite",
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
