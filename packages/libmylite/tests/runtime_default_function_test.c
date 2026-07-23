#include "mylite_test_support.h"

#include <mylite/mylite.h>

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
    path_suffix_capacity = 16,
    mysql_error_unknown_column = 1054,
    mysql_error_bad_null = 1048,
    mysql_error_no_default = 1364,
    mysql_error_parse = 1064,
    mysql_error_default_val_generated = 3773,
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
    size_t warning_count;
    const char *context;
};

static int test_default_function_select_dml_and_persistence(void);
static int test_default_function_diagnostics(void);
static int open_app_database(
    mylite_db **out_database,
    const char *name,
    char *path,
    size_t path_size
);
static int seed_default_table(mylite_db *database);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_statement_ok(
    mylite_db *database,
    const char *sql,
    struct expected_statement expected,
    const char *context
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

    failures += test_default_function_select_dml_and_persistence();
    failures += test_default_function_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_default_function_select_dml_and_persistence(void) {
    static const char *const defaults_row[] = {
        "7",
        "8",
        NULL,
        "abc",
        "",
        "2001-02-03",
        "2001-02-03 04:05:06",
        NULL,
        "0000-00-00 00:00:00",
        NULL,
        "0000-00-00 00:00:00",
        "8",
        "8",
    };
    static const char *const alias_default[] = {"8"};
    static const char *const inserted_values_row[] = {"7", "8", NULL, "9", "abc"};
    static const char *const inserted_set_row[] = {"7", "8", NULL, "10", "abc"};
    static const char *const replaced_values_row[] = {"7", "8", NULL, "11", "abc"};
    static const char *const replaced_set_row[] = {"7", "8", NULL, "12", "abc"};
    static const char *const updated_row[] = {"1", "7", "abc", NULL, NULL};
    static const char *const duplicate_row[] = {"1", "8", "abc"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (open_app_database(&database, "success", path, sizeof(path)) != 0) {
        return 1;
    }
    mylite_file_preamble_init(expected_preamble);
    failures += seed_default_table(database);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT DEFAULT(id), DEFAULT(n), DEFAULT(nul), DEFAULT(s), DEFAULT(e), "
                   "DEFAULT(d), DEFAULT(dt), DEFAULT(ct), DEFAULT(ctn), DEFAULT(ts), "
                   "DEFAULT(tsn), DEFAULT(t.n), DEFAULT(app.t.n) FROM t",
            .values = defaults_row,
            .column_count = sizeof(defaults_row) / sizeof(defaults_row[0]),
            .row_count = 1U,
            .warning_count = 0U,
            .context = "select default values",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT DEFAULT(q.n) FROM t AS q",
            .values = alias_default,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "select aliased default",
        }
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO t(id, n, nul, nn, s) "
        "VALUES(DEFAULT(id), DEFAULT(n), DEFAULT(nul), 9, DEFAULT(s))",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U},
        "insert values default function"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, n, nul, nn, s FROM t WHERE nn = 9",
            .values = inserted_values_row,
            .column_count = sizeof(inserted_values_row) / sizeof(inserted_values_row[0]),
            .row_count = 1U,
            .warning_count = 0U,
            .context = "insert values default function row",
        }
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO t SET id = DEFAULT(id), n = DEFAULT(n), nul = DEFAULT(nul), "
        "nn = 10, s = DEFAULT(s)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U},
        "insert set default function"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, n, nul, nn, s FROM t WHERE nn = 10",
            .values = inserted_set_row,
            .column_count = sizeof(inserted_set_row) / sizeof(inserted_set_row[0]),
            .row_count = 1U,
            .warning_count = 0U,
            .context = "insert set default function row",
        }
    );
    failures += expect_statement_ok(
        database,
        "REPLACE INTO t(id, n, nul, nn, s) "
        "VALUES(DEFAULT(id), DEFAULT(n), DEFAULT(nul), 11, DEFAULT(s))",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U},
        "replace values default function"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, n, nul, nn, s FROM t WHERE nn = 11",
            .values = replaced_values_row,
            .column_count = sizeof(replaced_values_row) / sizeof(replaced_values_row[0]),
            .row_count = 1U,
            .warning_count = 0U,
            .context = "replace values default function row",
        }
    );
    failures += expect_statement_ok(
        database,
        "REPLACE INTO t SET id = DEFAULT(id), n = DEFAULT(n), nul = DEFAULT(nul), "
        "nn = 12, s = DEFAULT(s)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U},
        "replace set default function"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, n, nul, nn, s FROM t WHERE nn = 12",
            .values = replaced_set_row,
            .column_count = sizeof(replaced_set_row) / sizeof(replaced_set_row[0]),
            .row_count = 1U,
            .warning_count = 0U,
            .context = "replace set default function row",
        }
    );
    failures += expect_statement_ok(
        database,
        "UPDATE t SET n = DEFAULT(id), s = DEFAULT(s), ct = DEFAULT(ct), ts = DEFAULT(ts) "
        "WHERE id = 1",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U},
        "update default function"
    );
    failures += expect_statement_ok(
        database,
        "UPDATE t SET s = DEFAULT(n) WHERE id = 999",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "update default conversion skipped for no match"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, n, s, ct, ts FROM t WHERE id = 1",
            .values = updated_row,
            .column_count = sizeof(updated_row) / sizeof(updated_row[0]),
            .row_count = 1U,
            .warning_count = 0U,
            .context = "updated default function row",
        }
    );
    failures += expect_statement_ok(
        database,
        "UPDATE t SET nn = DEFAULT(nul) WHERE id = 999",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "update null default skipped for no match"
    );
    failures += execute_error(
        database,
        "UPDATE t SET nn = DEFAULT(nul) WHERE id = 1",
        (struct expected_sql_error){
            .code = mysql_error_bad_null,
            .sqlstate = "23000",
            .message_part = "Column 'nn' cannot be null",
        }
    );
    failures += execute_ok(
        database,
        "CREATE TABLE p(id INT DEFAULT 1 PRIMARY KEY, n INT DEFAULT 8, s VARCHAR(10) DEFAULT "
        "'abc')",
        NULL
    );
    failures += execute_ok(database, "INSERT INTO p VALUES(1, 5, 'x')", NULL);
    failures += expect_statement_ok(
        database,
        "INSERT INTO p(id, n, s) VALUES(1, 9, 'y') "
        "ON DUPLICATE KEY UPDATE n = DEFAULT(n), s = DEFAULT(s)",
        (struct expected_statement){.affected_rows = 2, .warning_count = 0U},
        "duplicate update default function"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, n, s FROM p",
            .values = duplicate_row,
            .column_count = sizeof(duplicate_row) / sizeof(duplicate_row[0]),
            .row_count = 1U,
            .warning_count = 0U,
            .context = "duplicate update default function row",
        }
    );
    mylite_close(database);
    database = NULL;

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "reopen default file");
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, n, s, ct, ts FROM t WHERE id = 1",
            .values = updated_row,
            .column_count = sizeof(updated_row) / sizeof(updated_row[0]),
            .row_count = 1U,
            .warning_count = 0U,
            .context = "reopened updated default row",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, n, s FROM p",
            .values = duplicate_row,
            .column_count = 3U,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "reopened duplicate default row",
        }
    );
    mylite_close(database);
    database = NULL;
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "default function preamble"
    );

    remove_related_files(path);
    return failures;
}

static int test_default_function_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (open_app_database(&database, "diagnostics", path, sizeof(path)) != 0) {
        return 1;
    }
    failures += seed_default_table(database);
    failures += execute_error(
        database,
        "SELECT DEFAULT(n)",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'n' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT DEFAULT(other.t.n) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'other.t.n' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT DEFAULT(ex) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_default_val_generated,
            .sqlstate = "HY000",
            .message_part = "DEFAULT function cannot be used with default value expressions",
        }
    );
    failures += execute_error(
        database,
        "SELECT DEFAULT(nn) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_no_default,
            .sqlstate = "HY000",
            .message_part = "Field 'nn' doesn't have a default value",
        }
    );
    failures += execute_error(
        database,
        "UPDATE t SET n = DEFAULT(nn) WHERE id = 999",
        (struct expected_sql_error){
            .code = mysql_error_no_default,
            .sqlstate = "HY000",
            .message_part = "Field 'nn' doesn't have a default value",
        }
    );
    failures += execute_error(
        database,
        "SELECT DEFAULT(cd) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_default_val_generated,
            .sqlstate = "HY000",
            .message_part = "DEFAULT function cannot be used with default value expressions",
        }
    );
    failures += execute_error(
        database,
        "SELECT DEFAULT(cti) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_default_val_generated,
            .sqlstate = "HY000",
            .message_part = "DEFAULT function cannot be used with default value expressions",
        }
    );
    failures += execute_error(
        database,
        "UPDATE t SET s = DEFAULT(n) WHERE id = 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "DEFAULT() assignment does not support implicit descriptor conversion",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int open_app_database(
    mylite_db **out_database,
    const char *name,
    char *path,
    size_t path_size
) {
    int failures = 0;

    if (mylite_test_make_path(path, path_size, name) != 0) {
        return 1;
    }
    remove_related_files(path);
    failures += mylite_test_expect_int(mylite_open(path, out_database), MYLITE_OK, name);
    if (failures == 0) {
        failures += execute_ok(*out_database, "CREATE DATABASE app", NULL);
    }
    if (failures == 0) {
        failures += execute_ok(*out_database, "USE app", NULL);
    }
    return failures;
}

static int seed_default_table(mylite_db *database) {
    int failures = 0;

    failures += execute_ok(
        database,
        "CREATE TABLE t("
        "id INT NOT NULL DEFAULT 7, "
        "n INT NULL DEFAULT 8, "
        "nul INT NULL DEFAULT NULL, "
        "nn INT NOT NULL, "
        "s VARCHAR(10) DEFAULT 'abc', "
        "e VARCHAR(10) DEFAULT '', "
        "d DATE DEFAULT '2001-02-03', "
        "dt DATETIME DEFAULT '2001-02-03 04:05:06', "
        "ct DATETIME DEFAULT CURRENT_TIMESTAMP, "
        "ctn DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP, "
        "ts TIMESTAMP NULL DEFAULT CURRENT_TIMESTAMP, "
        "tsn TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, "
        "cd DATE DEFAULT (CURDATE()), "
        "cti TIME DEFAULT (CURTIME()), "
        "ex INT DEFAULT (1 + 2))",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO t(id, n, nul, nn, s, e, d, dt) "
        "VALUES(1, 10, NULL, 2, 'x', 'y', '2020-01-01', '2020-01-01 01:02:03')",
        NULL
    );
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected success, got %d %s %s\n",
            sql,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }
    if (out_result != NULL) {
        *out_result = result;
    } else {
        mylite_result_free(result);
    }
    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    if (rc == MYLITE_OK) {
        fprintf(stderr, "%s: expected error, got success\n", sql);
        mylite_result_free(result);
        return 1;
    }
    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, sql);
    failures += mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += mylite_test_expect_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_statement_ok(
    mylite_db *database,
    const char *sql,
    struct expected_statement expected,
    const char *context
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures == 0) {
        failures += mylite_test_expect_size(mylite_result_column_count(result), 0U, context);
        failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, context);
        failures += mylite_test_expect_int64(
            mylite_result_affected_rows(result),
            expected.affected_rows,
            context
        );
        failures += mylite_test_expect_size(
            mylite_result_warning_count(result),
            expected.warning_count,
            context
        );
    }
    mylite_result_free(result);
    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    if (failures == 0) {
        failures += mylite_test_expect_size(
            mylite_result_column_count(result),
            query.column_count,
            query.context
        );
        failures += mylite_test_expect_size(
            mylite_result_row_count(result),
            query.row_count,
            query.context
        );
        failures += mylite_test_expect_size(
            mylite_result_warning_count(result),
            query.warning_count,
            query.context
        );
    }
    for (size_t row_index = 0U; failures == 0 && row_index < query.row_count; ++row_index) {
        for (size_t column_index = 0U; column_index < query.column_count; ++column_index) {
            size_t offset = (row_index * query.column_count) + column_index;

            failures += expect_result_value(
                result,
                row_index,
                column_index,
                query.values[offset],
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
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: row %zu column %zu expected [%s], got [%s]\n",
            context,
            row,
            column,
            expected == NULL ? "NULL" : expected,
            actual == NULL ? "NULL" : actual
        );
        return 1;
    }
    return 0;
}

static void remove_related_files(const char *path) {
    remove(path);
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related[test_path_capacity + path_suffix_capacity];
    int written = snprintf(related, sizeof(related), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related)) {
        return;
    }
    remove(related);
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t bytes_read = 0U;

    if (file == NULL) {
        fprintf(stderr, "%s: failed to open file\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "%s: failed to seek file\n", path);
        fclose(file);
        return 1;
    }
    bytes_read = fread(buffer, 1U, size, file);
    fclose(file);
    if (bytes_read != size) {
        fprintf(stderr, "%s: expected %zu bytes, read %zu\n", path, size, bytes_read);
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
        fprintf(stderr, "%s: byte sequence mismatch\n", context);
        return 1;
    }
    return 0;
}
