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
    statistics_field_count = 18,
    metadata_index_part_row_count = 9,
    metadata_physical_index_count = 6,
    mysql_error_parse = 1064,
    mysql_error_duplicate_column = 1060,
    mysql_error_duplicate_key = 1062,
    mysql_error_incorrect_prefix_key = 1089,
    mysql_error_blob_key_without_length = 1170,
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

static int test_descending_key_part_metadata_dml_and_persistence(void);
static int test_alter_table_add_descending_primary_key(void);
static int test_descending_key_part_diagnostics(void);
static int test_descending_key_part_independent_handles(void);
static int create_metadata_schema(mylite_db *database);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_ddl_ok(mylite_db *database, const char *sql);
static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_physical_index_count(
    mylite_db *database,
    int expected_count,
    const char *context
);
static int expect_physical_desc_index_count(
    mylite_db *database,
    int expected_count,
    const char *context
);
static int expect_sqlite_index_count(
    mylite_db *database,
    const char *sql,
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

    failures += test_descending_key_part_metadata_dml_and_persistence();
    failures += test_alter_table_add_descending_primary_key();
    failures += test_descending_key_part_diagnostics();
    failures += test_descending_key_part_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_descending_key_part_metadata_dml_and_persistence(void) {
    static const char *const show_create_rows[] = {
        "t",
        "CREATE TABLE `t` (\n"
        "  `id` int NOT NULL,\n"
        "  `a` int DEFAULT NULL,\n"
        "  `b` int DEFAULT NULL,\n"
        "  `v` varchar(20) DEFAULT NULL,\n"
        "  PRIMARY KEY (`id` DESC),\n"
        "  UNIQUE KEY `u_b` (`b` DESC),\n"
        "  KEY `k_mix` (`a`,`b` DESC),\n"
        "  KEY `k_v` (`v`(5) DESC),\n"
        "  KEY `k_created` (`a` DESC,`b`),\n"
        "  KEY `k_alt` (`v`(3),`a` DESC)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const clone_show_create_rows[] = {
        "clone",
        "CREATE TABLE `clone` (\n"
        "  `id` int NOT NULL,\n"
        "  `a` int DEFAULT NULL,\n"
        "  `b` int DEFAULT NULL,\n"
        "  `v` varchar(20) DEFAULT NULL,\n"
        "  PRIMARY KEY (`id` DESC),\n"
        "  UNIQUE KEY `u_b` (`b` DESC),\n"
        "  KEY `k_mix` (`a`,`b` DESC),\n"
        "  KEY `k_v` (`v`(5) DESC),\n"
        "  KEY `k_created` (`a` DESC,`b`),\n"
        "  KEY `k_alt` (`v`(3),`a` DESC)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const show_index_rows[] = {
        "t", "0", "PRIMARY",   "1", "id", "D", "0", NULL, NULL, "",    "BTREE", "", "", "YES", NULL,
        "t", "0", "u_b",       "1", "b",  "D", "0", NULL, NULL, "YES", "BTREE", "", "", "YES", NULL,
        "t", "1", "k_mix",     "1", "a",  "A", "0", NULL, NULL, "YES", "BTREE", "", "", "YES", NULL,
        "t", "1", "k_mix",     "2", "b",  "D", "0", NULL, NULL, "YES", "BTREE", "", "", "YES", NULL,
        "t", "1", "k_v",       "1", "v",  "D", "0", "5",  NULL, "YES", "BTREE", "", "", "YES", NULL,
        "t", "1", "k_created", "1", "a",  "D", "0", NULL, NULL, "YES", "BTREE", "", "", "YES", NULL,
        "t", "1", "k_created", "2", "b",  "A", "0", NULL, NULL, "YES", "BTREE", "", "", "YES", NULL,
        "t", "1", "k_alt",     "1", "v",  "A", "0", "3",  NULL, "YES", "BTREE", "", "", "YES", NULL,
        "t", "1", "k_alt",     "2", "a",  "D", "0", NULL, NULL, "YES", "BTREE", "", "", "YES", NULL,
    };
    static const char *const statistics_rows[] = {
        "def", "app",   "t", "1", "app", "k_alt",     "1",   "v",     "A", "0", "3",   NULL,
        "YES", "BTREE", "",  "",  "YES", NULL,        "def", "app",   "t", "1", "app", "k_alt",
        "2",   "a",     "D", "0", NULL,  NULL,        "YES", "BTREE", "",  "",  "YES", NULL,
        "def", "app",   "t", "1", "app", "k_created", "1",   "a",     "D", "0", NULL,  NULL,
        "YES", "BTREE", "",  "",  "YES", NULL,        "def", "app",   "t", "1", "app", "k_created",
        "2",   "b",     "A", "0", NULL,  NULL,        "YES", "BTREE", "",  "",  "YES", NULL,
        "def", "app",   "t", "1", "app", "k_mix",     "1",   "a",     "A", "0", NULL,  NULL,
        "YES", "BTREE", "",  "",  "YES", NULL,        "def", "app",   "t", "1", "app", "k_mix",
        "2",   "b",     "D", "0", NULL,  NULL,        "YES", "BTREE", "",  "",  "YES", NULL,
        "def", "app",   "t", "1", "app", "k_v",       "1",   "v",     "D", "0", "5",   NULL,
        "YES", "BTREE", "",  "",  "YES", NULL,        "def", "app",   "t", "0", "app", "PRIMARY",
        "1",   "id",    "D", "0", NULL,  NULL,        "",    "BTREE", "",  "",  "YES", NULL,
        "def", "app",   "t", "0", "app", "u_b",       "1",   "b",     "D", "0", NULL,  NULL,
        "YES", "BTREE", "",  "",  "YES", NULL,
    };
    static const char *const selected_rows[] = {"1", "1", "10", "abc", "2", "2", "20", "def"};
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

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open metadata db");
    failures += create_metadata_schema(database);
    failures += expect_physical_index_count(
        database,
        metadata_physical_index_count,
        "descending physical index count"
    );
    failures += expect_physical_desc_index_count(
        database,
        metadata_physical_index_count,
        "descending physical index DDL count"
    );
    failures += expect_dml_ok(database, "INSERT INTO t VALUES (1,1,10,'abc'),(2,2,20,'def')", 2);
    failures += execute_error(
        database,
        "INSERT INTO t VALUES (3,3,20,'dup')",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '20' for key 't.u_b'",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE t",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "descending SHOW CREATE TABLE",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM t",
            .values = show_index_rows,
            .column_count = show_index_field_count,
            .row_count = metadata_index_part_row_count,
            .context = "descending SHOW INDEX",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM INFORMATION_SCHEMA.STATISTICS WHERE TABLE_SCHEMA = 'app' "
                   "AND TABLE_NAME = 't' ORDER BY INDEX_NAME",
            .values = statistics_rows,
            .column_count = statistics_field_count,
            .row_count = metadata_index_part_row_count,
            .context = "descending INFORMATION_SCHEMA.STATISTICS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, a, b, v FROM t ORDER BY id",
            .values = selected_rows,
            .column_count = 4U,
            .row_count = 2U,
            .context = "descending index DML rows",
        }
    );
    failures += expect_ddl_ok(database, "CREATE TABLE clone LIKE t");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE clone",
            .values = clone_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "descending CREATE TABLE LIKE metadata",
        }
    );
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after descending index metadata"
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen metadata db");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE t",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "reopened descending SHOW CREATE TABLE",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_alter_table_add_descending_primary_key(void) {
    static const char *const show_create_rows[] = {
        "add_pk",
        "CREATE TABLE `add_pk` (\n"
        "  `a` int NOT NULL,\n"
        "  `b` int NOT NULL,\n"
        "  PRIMARY KEY (`a` DESC,`b`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const show_index_rows[] = {
        "add_pk", "0", "PRIMARY", "1", "a", "D", "0", NULL, NULL, "", "BTREE", "", "", "YES", NULL,
        "add_pk", "0", "PRIMARY", "2", "b", "A", "0", NULL, NULL, "", "BTREE", "", "", "YES", NULL,
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "alter_primary") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open alter primary db");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures +=
        expect_statement_ok(database, "CREATE TABLE add_pk (a INT NOT NULL, b INT NOT NULL)");
    failures += expect_ddl_ok(database, "ALTER TABLE add_pk ADD PRIMARY KEY (a DESC, b ASC)");
    failures += expect_physical_index_count(database, 1, "descending alter primary physical index");
    failures +=
        expect_physical_desc_index_count(database, 1, "descending alter primary physical DDL");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE add_pk",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "descending ALTER ADD PRIMARY KEY SHOW CREATE",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM add_pk",
            .values = show_index_rows,
            .column_count = show_index_field_count,
            .row_count = 2U,
            .context = "descending ALTER ADD PRIMARY KEY SHOW INDEX",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_descending_key_part_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics db");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += execute_error(
        database,
        "CREATE TABLE duplicate_part (a INT, KEY k (a DESC, a ASC))",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_column,
            .sqlstate = "42S21",
            .message_part = "Duplicate column name 'a'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_int_prefix (id INT, KEY k (id(3) DESC))",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_prefix_key,
            .sqlstate = "HY000",
            .message_part = "Incorrect prefix key",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_text_desc (body TEXT, KEY k (body DESC))",
        (struct expected_sql_error){
            .code = mysql_error_blob_key_without_length,
            .sqlstate = "42000",
            .message_part =
                "BLOB/TEXT column 'body' used in key specification without a key length",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE qualified_part (a INT, KEY k (qualified_part.a DESC))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near '.'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE qualified_primary_part (a INT, PRIMARY KEY (qualified_primary_part.a DESC))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near '.'",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_descending_key_part_independent_handles(void) {
    static const char *const first_show_index_rows[] = {
        "t",
        "1",
        "k",
        "1",
        "id",
        "D",
        "0",
        NULL,
        NULL,
        "YES",
        "BTREE",
        "",
        "",
        "YES",
        NULL,
    };
    static const char *const second_show_index_rows[] = {
        "t",
        "1",
        "k",
        "1",
        "id",
        "A",
        "0",
        NULL,
        NULL,
        "YES",
        "BTREE",
        "",
        "",
        "YES",
        NULL,
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

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first db");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second db");
    failures += expect_statement_ok(first, "CREATE DATABASE app");
    failures += expect_statement_ok(first, "USE app");
    failures += expect_statement_ok(second, "CREATE DATABASE app");
    failures += expect_statement_ok(second, "USE app");
    failures += expect_ddl_ok(first, "CREATE TABLE t (id INT, KEY k (id DESC))");
    failures += expect_ddl_ok(second, "CREATE TABLE t (id INT, KEY k (id ASC))");
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SHOW INDEX FROM t",
            .values = first_show_index_rows,
            .column_count = show_index_field_count,
            .row_count = 1U,
            .context = "first descending handle metadata",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SHOW INDEX FROM t",
            .values = second_show_index_rows,
            .column_count = show_index_field_count,
            .row_count = 1U,
            .context = "second ascending handle metadata",
        }
    );

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);
    return failures;
}

static int create_metadata_schema(mylite_db *database) {
    int failures = 0;

    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_ddl_ok(
        database,
        "CREATE TABLE t ("
        "id INT NOT NULL, a INT, b INT, v VARCHAR(20), "
        "PRIMARY KEY (id DESC), KEY k_mix (a ASC, b DESC), "
        "KEY k_v (v(5) DESC), UNIQUE KEY u_b (b DESC))"
    );
    failures += expect_ddl_ok(database, "CREATE INDEX k_created ON t (a DESC, b ASC)");
    failures += expect_ddl_ok(database, "ALTER TABLE t ADD KEY k_alt (v(3) ASC, a DESC)");

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

static int expect_ddl_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, sql);
        failures += expect_size(mylite_result_row_count(result), 0U, sql);
        failures += expect_int64(mylite_result_affected_rows(result), 0, sql);
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
    return expect_sqlite_index_count(
        database,
        "SELECT count(*) FROM sqlite_schema "
        "WHERE type = 'index' AND name GLOB '_mylite_user_index_*'",
        expected_count,
        context
    );
}

static int expect_physical_desc_index_count(
    mylite_db *database,
    int expected_count,
    const char *context
) {
    return expect_sqlite_index_count(
        database,
        "SELECT count(*) FROM sqlite_schema "
        "WHERE type = 'index' AND name GLOB '_mylite_user_index_*' AND sql LIKE '% DESC%'",
        expected_count,
        context
    );
}

static int expect_sqlite_index_count(
    mylite_db *database,
    const char *sql,
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
    rc = sqlite3_prepare_v2(connection, sql, sqlite_use_nul_terminated_string, &statement, NULL);
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
        "/tmp/mylite_descending_index_key_parts_%d_%s.mylite",
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
    if (actual == NULL || memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: bytes differ\n", context);
        return 1;
    }
    return 0;
}
