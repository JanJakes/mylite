#include "mylite_test_support.h"

#include <mylite/mylite.h>

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
    statistics_field_count = 6,
    metadata_index_row_count = 7,
    mysql_error_duplicate_key = 1062,
    mysql_error_key_too_long = 1071,
    mysql_error_storage_engine_cant_index_column = 1167,
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

struct expected_dml_result {
    int64_t affected_rows;
    size_t warning_count;
};

static int test_full_binary_index_metadata_dml_and_persistence(void);
static int test_full_binary_index_diagnostics(void);
static int test_full_binary_index_independent_handles(void);
static int create_schema(mylite_db *database);
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

    failures += test_full_binary_index_metadata_dml_and_persistence();
    failures += test_full_binary_index_diagnostics();
    failures += test_full_binary_index_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_full_binary_index_metadata_dml_and_persistence(void) {
    static const char *const show_create_rows[] = {
        "bin_full",
        "CREATE TABLE `bin_full` (\n"
        "  `id` int NOT NULL,\n"
        "  `b` binary(4) DEFAULT NULL,\n"
        "  `vb` varbinary(8) DEFAULT NULL,\n"
        "  PRIMARY KEY (`id`),\n"
        "  UNIQUE KEY `u_b` (`b`),\n"
        "  UNIQUE KEY `u_vb` (`vb`),\n"
        "  KEY `k_vb` (`vb`),\n"
        "  KEY `k_mix` (`b`,`vb`),\n"
        "  KEY `k_vb_desc` (`vb` DESC)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const index_rows[] = {
        "bin_full", "0", "PRIMARY",   "1",   "id",  "A",        "0", NULL,    NULL,  "",
        "BTREE",    "",  "",          "YES", NULL,  "bin_full", "0", "u_b",   "1",   "b",
        "A",        "0", NULL,        NULL,  "YES", "BTREE",    "",  "",      "YES", NULL,
        "bin_full", "0", "u_vb",      "1",   "vb",  "A",        "0", NULL,    NULL,  "YES",
        "BTREE",    "",  "",          "YES", NULL,  "bin_full", "1", "k_vb",  "1",   "vb",
        "A",        "0", NULL,        NULL,  "YES", "BTREE",    "",  "",      "YES", NULL,
        "bin_full", "1", "k_mix",     "1",   "b",   "A",        "0", NULL,    NULL,  "YES",
        "BTREE",    "",  "",          "YES", NULL,  "bin_full", "1", "k_mix", "2",   "vb",
        "A",        "0", NULL,        NULL,  "YES", "BTREE",    "",  "",      "YES", NULL,
        "bin_full", "1", "k_vb_desc", "1",   "vb",  "D",        "0", NULL,    NULL,  "YES",
        "BTREE",    "",  "",          "YES", NULL,
    };
    static const char *const statistics_rows[] = {
        "k_mix", "1",    "1",       "b",    NULL, "YES", "k_mix", "1",         "2",   "vb", NULL,
        "YES",   "k_vb", "1",       "1",    "vb", NULL,  "YES",   "k_vb_desc", "1",   "1",  "vb",
        NULL,    "YES",  "PRIMARY", "0",    "1",  "id",  NULL,    "",          "u_b", "0",  "1",
        "b",     NULL,   "YES",     "u_vb", "0",  "1",   "vb",    NULL,        "YES",
    };
    static const char *const initial_rows[] = {
        "1",
        "41000000",
        "41",
        "2",
        "61000000",
        "4100",
        "3",
        NULL,
        NULL,
    };
    static const char *const dml_status_rows[] = {"0", "2"};
    static const char *const dml_count_rows[] = {"4"};
    static const char *const odku_status_rows[] = {"2", "1", "0"};
    static const char *const odku_rows[] = {"616263", "2"};
    static const char *const replace_update_status_rows[] = {"1", "0"};
    static const char *const replace_update_count_rows[] = {"2"};
    static const char *const replace_update_rows[] = {"4200"};
    static const char *const null_rows[] = {"2"};
    static const char *const clone_index_count_rows[] = {"7"};
    static const char *const added_index_count_rows[] = {"2"};
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
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open binary full file");
    failures += create_schema(database);
    failures += expect_statement_ok(
        database,
        "CREATE TABLE bin_full ("
        "id INT PRIMARY KEY, b BINARY(4), vb VARBINARY(8), "
        "UNIQUE KEY u_b (b), KEY k_vb (vb), KEY k_mix (b, vb))"
    );
    failures += expect_statement_ok(database, "ALTER TABLE bin_full ADD KEY k_vb_desc (vb DESC)");
    failures += expect_statement_ok(database, "CREATE UNIQUE INDEX u_vb ON bin_full (vb)");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE bin_full",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "full binary SHOW CREATE TABLE",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM bin_full",
            .values = index_rows,
            .column_count = show_index_field_count,
            .row_count = metadata_index_row_count,
            .context = "full binary SHOW INDEX",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME, SUB_PART, "
                   "NULLABLE FROM INFORMATION_SCHEMA.STATISTICS WHERE TABLE_SCHEMA = 'app' "
                   "AND TABLE_NAME = 'bin_full' ORDER BY INDEX_NAME",
            .values = statistics_rows,
            .column_count = statistics_field_count,
            .row_count = metadata_index_row_count,
            .context = "full binary INFORMATION_SCHEMA.STATISTICS",
        }
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO bin_full VALUES (1, X'4100', X'41'), "
        "(2, X'61', X'4100'), (3, NULL, NULL)",
        3
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, HEX(b), HEX(vb) FROM bin_full ORDER BY id",
            .values = initial_rows,
            .column_count = 3U,
            .row_count = 3U,
            .context = "full binary stored values",
        }
    );
    failures += expect_dml_ok(database, "INSERT INTO bin_full VALUES (4, X'6262', X'62')", 1);
    failures += expect_dml_result(
        database,
        "INSERT IGNORE INTO bin_full VALUES (5, X'6262', X'63'), (6, X'64', X'62')",
        (struct expected_dml_result){.affected_rows = 0, .warning_count = 2U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .values = dml_status_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "full binary DML status",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM bin_full",
            .values = dml_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "full binary DML count",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE odku_binary (vb VARBINARY(8), n INT, UNIQUE KEY u_vb (vb))"
    );
    failures += expect_dml_ok(database, "INSERT INTO odku_binary VALUES (X'616263', 1)", 1);
    failures += expect_dml_result(
        database,
        "INSERT INTO odku_binary VALUES (X'616263', 2) ON DUPLICATE KEY UPDATE n = VALUES(n)",
        (struct expected_dml_result){.affected_rows = 2, .warning_count = 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count, @@error_count",
            .values = odku_status_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "full binary ODKU status",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT HEX(vb), n FROM odku_binary",
            .values = odku_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "full binary ODKU state",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE replace_update_binary ("
        "id INT PRIMARY KEY, vb VARBINARY(4), UNIQUE KEY u_vb (vb))"
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO replace_update_binary VALUES (1, X'41'), (2, X'42')",
        2
    );
    failures += expect_dml_ok(database, "REPLACE INTO replace_update_binary VALUES (3, X'41')", 2);
    failures +=
        expect_dml_ok(database, "UPDATE replace_update_binary SET vb = X'4200' WHERE id = 2", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .values = replace_update_status_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "full binary replace and update status",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM replace_update_binary",
            .values = replace_update_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "full binary replace and update count",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT HEX(vb) FROM replace_update_binary WHERE id = 2",
            .values = replace_update_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "full binary replace and update state",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE null_binary (b BINARY(4), vb VARBINARY(4), UNIQUE KEY u_mix (b, vb))"
    );
    failures +=
        expect_dml_ok(database, "INSERT INTO null_binary VALUES (NULL, X'41'), (NULL, X'41')", 2);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM null_binary WHERE b IS NULL",
            .values = null_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "full binary unique NULL values",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE bin_clone LIKE bin_full");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'bin_clone'",
            .values = clone_index_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "full binary indexes clone with CREATE TABLE LIKE",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE added_binary_indexes (b BINARY(2), v VARBINARY(2))"
    );
    failures +=
        expect_statement_ok(database, "ALTER TABLE added_binary_indexes ADD UNIQUE KEY u_b (b)");
    failures += expect_statement_ok(database, "CREATE INDEX k_v ON added_binary_indexes (v)");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'added_binary_indexes'",
            .values = added_index_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "alter-added unique and standalone nonunique full binary indexes",
        }
    );
    mylite_close(database);
    database = NULL;

    failures += mylite_test_expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble unchanged"
    );
    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "reopen binary full file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, HEX(b), HEX(vb) FROM bin_full WHERE id IN (1, 2, 4) ORDER BY id",
            .values = (const char *const[]
            ){"1", "41000000", "41", "2", "61000000", "4100", "4", "62620000", "62"},
            .column_count = 3U,
            .row_count = 3U,
            .context = "full binary rows persist after reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_full_binary_index_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "open binary full diagnostics"
    );
    failures += create_schema(database);
    failures +=
        expect_statement_ok(database, "CREATE TABLE dup_vb (v VARBINARY(4), UNIQUE KEY u_vb (v))");
    failures += expect_dml_ok(database, "INSERT INTO dup_vb VALUES (X'0001AA')", 1);
    failures += execute_error(
        database,
        "INSERT INTO dup_vb VALUES (X'0001AA')",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '\\x00\\x01\\xAA' for key 'dup_vb.u_vb'",
        }
    );
    failures +=
        expect_statement_ok(database, "CREATE TABLE dup_b (b BINARY(4), UNIQUE KEY u_b (b))");
    failures += expect_dml_ok(database, "INSERT INTO dup_b VALUES (X'41004200')", 1);
    failures += execute_error(
        database,
        "INSERT INTO dup_b VALUES (X'41004200')",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry 'A\\x00B' for key 'dup_b.u_b'",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE update_dup (id INT PRIMARY KEY, vb VARBINARY(4), UNIQUE KEY u_vb (vb))"
    );
    failures +=
        expect_dml_ok(database, "INSERT INTO update_dup VALUES (1, X'41'), (2, X'4200')", 2);
    failures += execute_error(
        database,
        "UPDATE update_dup SET vb = X'4200' WHERE id = 1",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry 'B\\x00' for key 'update_dup.u_vb'",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE alter_existing (vb VARBINARY(4))");
    failures +=
        expect_dml_ok(database, "INSERT INTO alter_existing VALUES (X'6162'), (X'6162')", 2);
    failures += execute_error(
        database,
        "ALTER TABLE alter_existing ADD UNIQUE KEY u_vb (vb)",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry 'ab' for key 'alter_existing.u_vb'",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE create_existing (vb VARBINARY(4))");
    failures +=
        expect_dml_ok(database, "INSERT INTO create_existing VALUES (X'6162'), (X'6162')", 2);
    failures += execute_error(
        database,
        "CREATE UNIQUE INDEX u_vb ON create_existing (vb)",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry 'ab' for key 'create_existing.u_vb'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE blob_bad (b BLOB, KEY k (b))",
        (struct expected_sql_error){
            .code = mysql_error_blob_key_without_length,
            .sqlstate = "42000",
            .message_part = "BLOB/TEXT column 'b' used in key specification without a key length",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_vb (v VARBINARY(3073), KEY k (v))",
        (struct expected_sql_error){
            .code = mysql_error_key_too_long,
            .sqlstate = "42000",
            .message_part = "Specified key was too long; max key length is 3072 bytes",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_zero (v VARBINARY(0), KEY k (v))",
        (struct expected_sql_error){
            .code = mysql_error_storage_engine_cant_index_column,
            .sqlstate = "42000",
            .message_part = "The used storage engine can't index column 'v'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_mix (a VARBINARY(2000), b VARBINARY(1073), KEY k (a, b))",
        (struct expected_sql_error){
            .code = mysql_error_key_too_long,
            .sqlstate = "42000",
            .message_part = "Specified key was too long; max key length is 3072 bytes",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_full_binary_index_independent_handles(void) {
    static const char *const first_rows[] = {"01"};
    static const char *const second_rows[] = {"02"};
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
        "open first binary full"
    );
    failures += mylite_test_expect_int(
        mylite_open(second_path, &second),
        MYLITE_OK,
        "open second binary full"
    );
    failures += create_schema(first);
    failures += create_schema(second);
    failures += expect_statement_ok(first, "CREATE TABLE t (v VARBINARY(1), UNIQUE KEY u_v (v))");
    failures += expect_statement_ok(second, "CREATE TABLE t (v VARBINARY(1), UNIQUE KEY u_v (v))");
    failures += expect_dml_ok(first, "INSERT INTO t VALUES (X'01')", 1);
    failures += expect_dml_ok(second, "INSERT INTO t VALUES (X'02')", 1);
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT HEX(v) FROM t",
            .values = first_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "first full binary state",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT HEX(v) FROM t",
            .values = second_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "second full binary state",
        }
    );

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);
    return failures;
}

static int create_schema(mylite_db *database) {
    int failures = expect_statement_ok(database, "CREATE DATABASE app");

    failures += expect_statement_ok(database, "USE app");
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s failed: %s\n", sql, mylite_errmsg(database));
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
    mylite_result_free(result);
    return failures;
}

static int expect_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += mylite_test_expect_size(mylite_result_column_count(result), 0U, sql);
    failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, sql);
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows) {
    return expect_dml_result(
        database,
        sql,
        (struct expected_dml_result){.affected_rows = affected_rows, .warning_count = 0U}
    );
}

static int expect_dml_result(
    mylite_db *database,
    const char *sql,
    struct expected_dml_result expected
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += mylite_test_expect_size(mylite_result_column_count(result), 0U, sql);
    failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, sql);
    failures +=
        mylite_test_expect_int64(mylite_result_affected_rows(result), expected.affected_rows, sql);
    failures +=
        mylite_test_expect_size(mylite_result_warning_count(result), expected.warning_count, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    size_t value_count = query.column_count * query.row_count;
    int failures = execute_ok(database, query.sql, &result);

    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        query.column_count,
        query.context
    );
    failures +=
        mylite_test_expect_size(mylite_result_row_count(result), query.row_count, query.context);
    for (size_t value_index = 0U; value_index < value_count; ++value_index) {
        size_t row = value_index / query.column_count;
        size_t column = value_index % query.column_count;

        failures +=
            expect_result_value(result, row, column, query.values[value_index], query.context);
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

    if (expected == NULL) {
        return mylite_test_expect_int(actual == NULL, 1, context);
    }
    return mylite_test_expect_text(actual, expected, context);
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
    size_t bytes_read = 0U;

    if (file == NULL) {
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fclose(file);
        return 1;
    }
    bytes_read = fread(buffer, 1U, size, file);
    fclose(file);
    return bytes_read == size ? 0 : 1;
}

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (size == 0U && expected != NULL) {
        return 0;
    }
    if (actual != NULL && expected != NULL && memcmp(actual, expected, size) == 0) {
        return 0;
    }
    fprintf(stderr, "%s: byte sequence mismatch\n", context);
    return 1;
}
