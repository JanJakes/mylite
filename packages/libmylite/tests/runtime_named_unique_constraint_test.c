#include "mylite_test_support.h"

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
    detail_capacity = 256,
    related_path_extra_capacity = 16,
    show_index_field_count = 15,
    mysql_error_duplicate_key = 1062,
    mysql_error_duplicate_key_name = 1061,
    mysql_error_key_column_missing = 1072,
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

static int test_named_unique_constraint_metadata_dml_drop_and_persistence(void);
static int test_named_unique_constraint_variants_prefixes_and_diagnostics(void);
static int test_named_unique_constraint_independent_handles(void);
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
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_named_unique_constraint_metadata_dml_drop_and_persistence();
    failures += test_named_unique_constraint_variants_prefixes_and_diagnostics();
    failures += test_named_unique_constraint_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_named_unique_constraint_metadata_dml_drop_and_persistence(void) {
    static const char *const show_create_rows[] = {
        "named_unique",
        "CREATE TABLE `named_unique` (\n"
        "  `id` int DEFAULT NULL,\n"
        "  `name` int DEFAULT NULL,\n"
        "  UNIQUE KEY `c_name` (`name`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const show_index_rows[] = {
        "named_unique",
        "0",
        "c_name",
        "1",
        "name",
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
    static const char *const constraint_rows[] = {"c_name", "UNIQUE", "YES"};
    static const char *const statistics_rows[] = {"c_name", "0", "1", "name"};
    static const char *const key_column_rows[] = {"c_name", "name", "1"};
    static const char *const nullable_rows[] = {"1", "10", "2", NULL, "3", NULL};
    static const char *const dropped_show_create_rows[] = {
        "named_unique",
        "CREATE TABLE `named_unique` (\n"
        "  `id` int DEFAULT NULL,\n"
        "  `name` int DEFAULT NULL\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const zero_rows[] = {"0"};
    static const char *const persisted_rows[] = {"1", "10", "2", NULL, "3", NULL, "4", "10"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "metadata") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open named unique file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE named_unique (id INT, name INT, CONSTRAINT c_name UNIQUE (name))"
    );
    failures += expect_physical_index_count(database, 1, "physical named unique index");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE named_unique",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "SHOW CREATE named UNIQUE constraint",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM named_unique",
            .values = show_index_rows,
            .column_count = show_index_field_count,
            .row_count = 1U,
            .context = "SHOW INDEX named UNIQUE constraint",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED "
                   "FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'named_unique'",
            .values = constraint_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "I_S TABLE_CONSTRAINTS named UNIQUE constraint",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME "
                   "FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'named_unique'",
            .values = statistics_rows,
            .column_count = 4U,
            .row_count = 1U,
            .context = "I_S STATISTICS named UNIQUE constraint",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION "
                   "FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'named_unique'",
            .values = key_column_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "I_S KEY_COLUMN_USAGE named UNIQUE constraint",
        }
    );

    failures +=
        expect_dml_ok(database, "INSERT INTO named_unique VALUES (1,10),(2,20),(3,NULL)", 3);
    failures += expect_dml_ok(database, "UPDATE named_unique SET name = NULL WHERE id = 2", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, name FROM named_unique ORDER BY id",
            .values = nullable_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "named UNIQUE permits duplicate NULL values",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO named_unique VALUES (4,10)",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '10' for key 'named_unique.c_name'",
        }
    );
    failures += expect_statement_ok(database, "ALTER TABLE named_unique DROP INDEX c_name");
    failures += expect_physical_index_count(database, 0, "physical named unique index after drop");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE named_unique",
            .values = dropped_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "SHOW CREATE after dropping named UNIQUE constraint",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'named_unique'",
            .values = zero_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "I_S constraints after named UNIQUE drop",
        }
    );
    failures += expect_dml_ok(database, "INSERT INTO named_unique VALUES (4,10)", 1);
    failures += expect_bytes(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)) == 0 ? actual_preamble
                                                                              : NULL,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after named UNIQUE lifecycle"
    );

    mylite_close(database);
    database = NULL;

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "reopen named unique file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, name FROM named_unique ORDER BY id",
            .values = persisted_rows,
            .column_count = 2U,
            .row_count = 4U,
            .context = "reopened rows after named UNIQUE drop",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'named_unique'",
            .values = zero_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "reopened named UNIQUE metadata after drop",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_named_unique_constraint_variants_prefixes_and_diagnostics(void) {
    static const char *const no_symbol_rows[] = {
        "no_symbol",
        "CREATE TABLE `no_symbol` (\n"
        "  `a` int DEFAULT NULL,\n"
        "  UNIQUE KEY `a` (`a`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const key_form_rows[] = {
        "key_form",
        "CREATE TABLE `key_form` (\n"
        "  `a` int DEFAULT NULL,\n"
        "  UNIQUE KEY `c_key` (`a`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const index_form_rows[] = {
        "index_form",
        "CREATE TABLE `index_form` (\n"
        "  `a` int DEFAULT NULL,\n"
        "  UNIQUE KEY `c_idx` (`a`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const explicit_name_rows[] = {
        "explicit_name",
        "CREATE TABLE `explicit_name` (\n"
        "  `a` int DEFAULT NULL,\n"
        "  UNIQUE KEY `visible` (`a`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const explicit_no_keyword_rows[] = {
        "explicit_no_keyword",
        "CREATE TABLE `explicit_no_keyword` (\n"
        "  `a` int DEFAULT NULL,\n"
        "  UNIQUE KEY `visible` (`a`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const prefix_show_create_rows[] = {
        "prefix_named",
        "CREATE TABLE `prefix_named` (\n"
        "  `a` varchar(20) DEFAULT NULL,\n"
        "  `body` text,\n"
        "  UNIQUE KEY `c_pref` (`a`(3),`body`(2) DESC)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const prefix_index_rows[] = {
        "prefix_named",
        "0",
        "c_pref",
        "1",
        "a",
        "A",
        "0",
        "3",
        NULL,
        "YES",
        "BTREE",
        "",
        "",
        "YES",
        NULL,
        "prefix_named",
        "0",
        "c_pref",
        "2",
        "body",
        "D",
        "0",
        "2",
        NULL,
        "YES",
        "BTREE",
        "",
        "",
        "YES",
        NULL,
    };
    static const char *const clone_index_rows[] = {
        "clone_prefix",
        "0",
        "c_pref",
        "1",
        "a",
        "A",
        "0",
        "3",
        NULL,
        "YES",
        "BTREE",
        "",
        "",
        "YES",
        NULL,
        "clone_prefix",
        "0",
        "c_pref",
        "2",
        "body",
        "D",
        "0",
        "2",
        NULL,
        "YES",
        "BTREE",
        "",
        "",
        "YES",
        NULL,
    };
    static const char *const same_name_count[] = {"2"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "variants") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open variant file db");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures +=
        expect_statement_ok(database, "CREATE TABLE no_symbol (a INT, CONSTRAINT UNIQUE (a))");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE key_form (a INT, CONSTRAINT c_key UNIQUE KEY (a))"
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE index_form (a INT, CONSTRAINT c_idx UNIQUE INDEX (a))"
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE explicit_name (a INT, CONSTRAINT ignored UNIQUE KEY visible (a))"
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE explicit_no_keyword (a INT, CONSTRAINT ignored UNIQUE visible (a))"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE no_symbol",
            .values = no_symbol_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "CONSTRAINT UNIQUE generated name",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE key_form",
            .values = key_form_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "CONSTRAINT name UNIQUE KEY",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE index_form",
            .values = index_form_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "CONSTRAINT name UNIQUE INDEX",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE explicit_name",
            .values = explicit_name_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "CONSTRAINT name UNIQUE KEY explicit index name",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE explicit_no_keyword",
            .values = explicit_no_keyword_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "CONSTRAINT name UNIQUE explicit no-keyword index name",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE prefix_named (a VARCHAR(20), body TEXT, "
        "CONSTRAINT c_pref UNIQUE KEY (a(3), body(2) DESC))"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE prefix_named",
            .values = prefix_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "named UNIQUE prefix SHOW CREATE",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM prefix_named",
            .values = prefix_index_rows,
            .column_count = show_index_field_count,
            .row_count = 2U,
            .context = "named UNIQUE prefix SHOW INDEX",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE clone_prefix LIKE prefix_named");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM clone_prefix",
            .values = clone_index_rows,
            .column_count = show_index_field_count,
            .row_count = 2U,
            .context = "named UNIQUE prefix cloned by CREATE TABLE LIKE",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE same_one (a INT, CONSTRAINT reused UNIQUE (a))"
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE same_two (a INT, CONSTRAINT reused UNIQUE (a))"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "
                   "WHERE TABLE_SCHEMA = 'app' AND CONSTRAINT_NAME = 'reused'",
            .values = same_name_count,
            .column_count = 1U,
            .row_count = 1U,
            .context = "same named UNIQUE constraint on different tables",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE duplicate_same_table ("
        "a INT, b INT, CONSTRAINT same UNIQUE (a), UNIQUE KEY same (b))",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key_name,
            .sqlstate = "42000",
            .message_part = "Duplicate key name 'same'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE missing_named (a INT, CONSTRAINT c_missing UNIQUE (missing))",
        (struct expected_sql_error){
            .code = mysql_error_key_column_missing,
            .sqlstate = "42000",
            .message_part = "Key column 'missing' doesn't exist in table",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_named_unique_constraint_independent_handles(void) {
    static const char *const first_rows[] = {"1"};
    static const char *const second_rows[] = {"2"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (mylite_test_make_path(first_path, sizeof(first_path), "first") != 0 ||
        mylite_test_make_path(second_path, sizeof(second_path), "second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += mylite_test_expect_int(
        mylite_open(first_path, &first),
        MYLITE_OK,
        "open first named unique db"
    );
    failures += mylite_test_expect_int(
        mylite_open(second_path, &second),
        MYLITE_OK,
        "open second named unique db"
    );
    failures += expect_statement_ok(first, "CREATE DATABASE app");
    failures += expect_statement_ok(second, "CREATE DATABASE app");
    failures += expect_statement_ok(first, "USE app");
    failures += expect_statement_ok(second, "USE app");
    failures += expect_statement_ok(first, "CREATE TABLE t (v INT, CONSTRAINT c UNIQUE (v))");
    failures += expect_statement_ok(second, "CREATE TABLE t (v INT, CONSTRAINT c UNIQUE (v))");
    failures += expect_dml_ok(first, "INSERT INTO t VALUES (1)", 1);
    failures += expect_dml_ok(second, "INSERT INTO t VALUES (2)", 1);
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT v FROM t",
            .values = first_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "first named UNIQUE handle rows",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT v FROM t",
            .values = second_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "second named UNIQUE handle rows",
        }
    );

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);
    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s failed with %d/%s/%s\n",
            sql,
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

    if (rc == MYLITE_OK) {
        fprintf(stderr, "%s unexpectedly succeeded\n", sql);
        mylite_result_free(result);
        return 1;
    }

    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, "error code");
    failures += mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, "SQLSTATE");
    failures += mylite_test_expect_contains(
        mylite_errmsg(database),
        expected.message_part,
        "error message"
    );
    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        0U,
        "failed statement column count"
    );
    failures +=
        mylite_test_expect_size(mylite_result_row_count(result), 0U, "failed statement row count");
    mylite_result_free(result);
    return failures;
}

static int expect_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures +=
        mylite_test_expect_size(mylite_result_column_count(result), 0U, "statement column count");
    failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, "statement row count");
    failures +=
        mylite_test_expect_size(mylite_result_warning_count(result), 0U, "statement warning count");
    mylite_result_free(result);
    return failures;
}

static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += mylite_test_expect_size(mylite_result_column_count(result), 0U, "DML column count");
    failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, "DML row count");
    failures += mylite_test_expect_int64(
        mylite_result_affected_rows(result),
        affected_rows,
        "DML affected rows"
    );
    failures +=
        mylite_test_expect_size(mylite_result_warning_count(result), 0U, "DML warning count");
    mylite_result_free(result);
    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        query.column_count,
        query.context
    );
    failures +=
        mylite_test_expect_size(mylite_result_row_count(result), query.row_count, query.context);
    for (size_t row = 0; row < query.row_count; ++row) {
        for (size_t column = 0; column < query.column_count; ++column) {
            size_t index = (row * query.column_count) + column;
            char detail[detail_capacity];

            snprintf(detail, sizeof(detail), "%s row %zu column %zu", query.context, row, column);
            failures += mylite_test_expect_text(
                mylite_result_value_text(result, row, column),
                query.values[index],
                detail
            );
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
    static const char *const sql = "SELECT COUNT(*) FROM sqlite_schema WHERE type = 'index' AND "
                                   "name LIKE '_mylite_user_index_%'";
    sqlite3 *sqlite = mylite_connection_sqlite_for_test(database);
    sqlite3_stmt *statement = NULL;
    int failures = 0;

    if (sqlite == NULL) {
        fprintf(stderr, "%s: missing sqlite connection\n", context);
        return 1;
    }
    if (sqlite3_prepare_v2(sqlite, sql, -1, &statement, NULL) != SQLITE_OK) {
        fprintf(stderr, "%s: prepare failed: %s\n", context, sqlite3_errmsg(sqlite));
        return 1;
    }
    if (sqlite3_step(statement) == SQLITE_ROW) {
        failures +=
            mylite_test_expect_int(sqlite3_column_int(statement, 0), expected_count, context);
    } else {
        fprintf(stderr, "%s: index count query returned no row\n", context);
        failures += 1;
    }
    sqlite3_finalize(statement);
    return failures;
}

static void remove_related_files(const char *path) {
    remove_with_suffix(path, "");
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related[test_path_capacity + related_path_extra_capacity];
    int written = snprintf(related, sizeof(related), "%s%s", path, suffix);
    if (written >= 0 && (size_t)written < sizeof(related)) {
        remove(related);
    }
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    int result = 0;

    if (file == NULL) {
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0 || fread(buffer, 1U, size, file) != size) {
        result = 1;
    }
    fclose(file);
    return result;
}

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (actual == NULL || expected == NULL || memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: byte comparison failed\n", context);
        return 1;
    }
    return 0;
}
