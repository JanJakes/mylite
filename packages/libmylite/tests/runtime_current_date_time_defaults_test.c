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
    show_columns_column_count = 6,
    show_full_columns_column_count = 9,
    information_schema_column_count = 6,
    mysql_error_parse = 1064,
    mysql_error_invalid_default = 1067,
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

static int test_current_date_time_default_metadata_and_dml(void);
static int test_current_date_time_default_alter_like_persistence_and_file_safety(void);
static int test_current_date_time_default_diagnostics(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows);
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

    failures += test_current_date_time_default_metadata_and_dml();
    failures += test_current_date_time_default_alter_like_persistence_and_file_safety();
    failures += test_current_date_time_default_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_current_date_time_default_metadata_and_dml(void) {
    static const char *const show_create_values[] = {
        "generated_temporals",
        "CREATE TABLE `generated_temporals` (\n"
        "  `id` int DEFAULT NULL,\n"
        "  `d1` date DEFAULT (curdate()),\n"
        "  `d2` date DEFAULT (curdate()),\n"
        "  `d3` date DEFAULT (curdate()),\n"
        "  `tm1` time DEFAULT (curtime()),\n"
        "  `tm2` time DEFAULT (curtime()),\n"
        "  `tm3` time DEFAULT (curtime())\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const show_d1_values[] = {
        "d1",
        "date",
        "YES",
        "",
        "curdate()",
        "DEFAULT_GENERATED",
    };
    static const char *const show_full_tm1_values[] = {
        "tm1",
        "time",
        NULL,
        "YES",
        "",
        "curtime()",
        "DEFAULT_GENERATED",
        "select,insert,update,references",
        "",
    };
    static const char *const information_schema_d1_values[] = {
        "d1",
        "date",
        "curdate()",
        "YES",
        "DEFAULT_GENERATED",
        NULL,
    };
    static const char *const information_schema_tm1_values[] = {
        "tm1",
        "time",
        "curtime()",
        "YES",
        "DEFAULT_GENERATED",
        "0",
    };
    static const char *const after_insert_values[] = {"1", "2023-11-14", "22:13:20"};
    static const char *const after_values_default[] = {"2", "2023-11-14", "22:14:20"};
    static const char *const after_insert_set[] = {"3", "2023-11-14", "22:15:20"};
    static const char *const after_replace[] = {"4", "2023-11-14", "22:16:20"};
    static const char *const update_counts[] = {"1", "0"};
    static const char *const update_noop_counts[] = {"0", "0"};
    static const char *const after_update_values[] = {"1", "2023-11-14", "22:17:20"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "metadata") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open metadata file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE generated_temporals ("
        "id INT, "
        "d1 DATE DEFAULT (CURDATE()), "
        "d2 DATE DEFAULT (CURRENT_DATE), "
        "d3 DATE DEFAULT (CURRENT_DATE()), "
        "tm1 TIME DEFAULT (CURTIME()), "
        "tm2 TIME DEFAULT (CURRENT_TIME), "
        "tm3 TIME DEFAULT (CURRENT_TIME()))"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE generated_temporals",
            .values = show_create_values,
            .column_count = 2U,
            .row_count = 1U,
            .context = "show create generated temporals",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM generated_temporals LIKE 'd1'",
            .values = show_d1_values,
            .column_count = show_columns_column_count,
            .row_count = 1U,
            .context = "show columns generated date default",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW FULL COLUMNS FROM generated_temporals LIKE 'tm1'",
            .values = show_full_tm1_values,
            .column_count = show_full_columns_column_count,
            .row_count = 1U,
            .context = "show full columns generated time default",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, DATA_TYPE, COLUMN_DEFAULT, IS_NULLABLE, EXTRA, "
                   "DATETIME_PRECISION FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'generated_temporals' "
                   "AND COLUMN_NAME = 'd1'",
            .values = information_schema_d1_values,
            .column_count = information_schema_column_count,
            .row_count = 1U,
            .context = "information schema generated date default",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, DATA_TYPE, COLUMN_DEFAULT, IS_NULLABLE, EXTRA, "
                   "DATETIME_PRECISION FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'generated_temporals' "
                   "AND COLUMN_NAME = 'tm1'",
            .values = information_schema_tm1_values,
            .column_count = information_schema_column_count,
            .row_count = 1U,
            .context = "information schema generated time default",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE dml_defaults (id INT, d DATE DEFAULT (CURDATE()), "
        "tm TIME DEFAULT (CURTIME()))"
    );
    failures += expect_statement_ok(database, "SET timestamp = 1700000000");
    failures += expect_dml_ok(database, "INSERT INTO dml_defaults(id) VALUES (1)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, d, tm FROM dml_defaults WHERE id = 1",
            .values = after_insert_values,
            .column_count = 3U,
            .row_count = 1U,
            .context = "omitted generated date time defaults",
        }
    );
    failures += expect_statement_ok(database, "SET timestamp = 1700000060");
    failures += expect_dml_ok(database, "INSERT INTO dml_defaults VALUES (2, DEFAULT, DEFAULT)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, d, tm FROM dml_defaults WHERE id = 2",
            .values = after_values_default,
            .column_count = 3U,
            .row_count = 1U,
            .context = "explicit generated date time defaults",
        }
    );
    failures += expect_statement_ok(database, "SET timestamp = 1700000120");
    failures += expect_dml_ok(
        database,
        "INSERT INTO dml_defaults SET id = 3, d = DEFAULT, tm = DEFAULT",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, d, tm FROM dml_defaults WHERE id = 3",
            .values = after_insert_set,
            .column_count = 3U,
            .row_count = 1U,
            .context = "insert set generated date time defaults",
        }
    );
    failures += expect_statement_ok(database, "SET timestamp = 1700000180");
    failures +=
        expect_dml_ok(database, "REPLACE INTO dml_defaults VALUES (4, DEFAULT, DEFAULT)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, d, tm FROM dml_defaults WHERE id = 4",
            .values = after_replace,
            .column_count = 3U,
            .row_count = 1U,
            .context = "replace generated date time defaults",
        }
    );
    failures += expect_statement_ok(database, "SET timestamp = 1700000240");
    failures += expect_dml_ok(
        database,
        "UPDATE dml_defaults SET d = DEFAULT, tm = DEFAULT WHERE id = 1",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .values = update_counts,
            .column_count = 2U,
            .row_count = 1U,
            .context = "update generated defaults counts",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, d, tm FROM dml_defaults WHERE id = 1",
            .values = after_update_values,
            .column_count = 3U,
            .row_count = 1U,
            .context = "update generated defaults values",
        }
    );
    failures += expect_dml_ok(
        database,
        "UPDATE dml_defaults SET d = DEFAULT, tm = DEFAULT WHERE id = 1",
        0
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .values = update_noop_counts,
            .column_count = 2U,
            .row_count = 1U,
            .context = "no-op generated defaults counts",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_current_date_time_default_alter_like_persistence_and_file_safety(void) {
    static const char *const backfilled_rows[] = {
        "1",
        "2023-11-14",
        "22:13:20",
        "2",
        "2023-11-14",
        "22:13:20",
    };
    static const char *const after_alter_default_row[] = {"3", "2023-11-14", "22:14:20"};
    static const char *const clone_show_create[] = {
        "clone",
        "CREATE TABLE `clone` (\n"
        "  `id` int DEFAULT NULL,\n"
        "  `d` date NOT NULL DEFAULT (curdate()),\n"
        "  `tm` time NOT NULL DEFAULT (curtime())\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const clone_row[] = {"10", "2023-11-14", "22:15:20"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "alter") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open alter file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(database, "CREATE TABLE altered (id INT)");
    failures += expect_dml_ok(database, "INSERT INTO altered VALUES (1), (2)", 2);
    failures += expect_statement_ok(database, "SET timestamp = 1700000000");
    failures += expect_statement_ok(
        database,
        "ALTER TABLE altered ADD COLUMN d DATE NOT NULL DEFAULT (CURDATE())"
    );
    failures += expect_statement_ok(
        database,
        "ALTER TABLE altered ADD COLUMN tm TIME NOT NULL DEFAULT (CURTIME())"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM altered ORDER BY id",
            .values = backfilled_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "alter add generated defaults backfill",
        }
    );
    failures += expect_statement_ok(database, "SET timestamp = 1700000060");
    failures += expect_statement_ok(
        database,
        "ALTER TABLE altered ALTER COLUMN d SET DEFAULT (CURRENT_DATE)"
    );
    failures += expect_statement_ok(
        database,
        "ALTER TABLE altered ALTER COLUMN tm SET DEFAULT (CURRENT_TIME)"
    );
    failures += expect_dml_ok(database, "INSERT INTO altered(id) VALUES (3)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM altered WHERE id = 3",
            .values = after_alter_default_row,
            .column_count = 3U,
            .row_count = 1U,
            .context = "alter set generated defaults future insert",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE clone LIKE altered");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE clone",
            .values = clone_show_create,
            .column_count = 2U,
            .row_count = 1U,
            .context = "create table like generated defaults",
        }
    );
    failures += expect_statement_ok(database, "SET timestamp = 1700000120");
    failures += expect_dml_ok(database, "INSERT INTO clone(id) VALUES (10)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM clone",
            .values = clone_row,
            .column_count = 3U,
            .row_count = 1U,
            .context = "clone generated defaults materialize",
        }
    );
    failures += expect_statement_ok(database, "RENAME TABLE altered TO renamed");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM renamed WHERE id = 3",
            .values = after_alter_default_row,
            .column_count = 3U,
            .row_count = 1U,
            .context = "renamed generated defaults table",
        }
    );
    failures += expect_statement_ok(database, "DROP TABLE clone");
    failures +=
        expect_int(read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)), 0, "preamble");
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "current date time defaults leave preamble"
    );

    mylite_close(database);
    database = NULL;
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen alter file");
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM renamed WHERE id = 3",
            .values = after_alter_default_row,
            .column_count = 3U,
            .row_count = 1U,
            .context = "generated defaults persist after reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_current_date_time_default_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += execute_error(
        database,
        "CREATE TABLE bad_date_syntax (d DATE DEFAULT CURRENT_DATE)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_time_syntax (tm TIME DEFAULT CURRENT_TIME)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_int (i INT DEFAULT (CURDATE()))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'i'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_date (d DATE DEFAULT (CURTIME()))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'd'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_time (tm TIME DEFAULT (CURDATE()))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'tm'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_date_now (d DATE DEFAULT (NOW()))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'd'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_time_fsp (tm TIME DEFAULT (CURRENT_TIME(0)))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

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
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, "dml column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "dml row count");
    failures += expect_int64(mylite_result_affected_rows(result), affected_rows, "dml affected");
    failures += expect_size(mylite_result_warning_count(result), 0U, "dml warning count");
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
        "%s/mylite_current_date_time_defaults_%d_%s.mylite",
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
    size_t read_count = 0U;

    if (file == NULL) {
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fclose(file);
        return 1;
    }
    read_count = fread(buffer, 1U, size, file);
    if (fclose(file) != 0) {
        return 1;
    }
    return read_count == size ? 0 : 1;
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
            "%s: expected [%s], got [%s]\n",
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
            "%s: expected [%s] to contain [%s]\n",
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
    if (actual == NULL || expected == NULL || memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: bytes differ\n", context);
        return 1;
    }
    return 0;
}
