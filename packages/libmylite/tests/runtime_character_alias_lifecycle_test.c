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
    related_file_suffix_capacity = 8,
    show_columns_field_count = 6,
    show_columns_row_count = 6,
    inserted_row_field_count = 6,
    information_schema_field_count = 9,
    information_schema_alias_row_count = 5,
    clone_column_row_count = 8,
    mysql_error_duplicate_key = 1062,
    mysql_error_parse = 1064,
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
    int64_t affected_rows;
    size_t warning_count;
};

static int test_character_alias_success_persistence_and_introspection(void);
static int test_character_alias_diagnostics(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_statement_result(
    mylite_db *database,
    const char *sql,
    struct expected_statement expected
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
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text_or_null(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_character_alias_success_persistence_and_introspection();
    failures += test_character_alias_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_character_alias_success_persistence_and_introspection(void) {
    static const char *const show_columns_rows[] = {
        "id", "int",        "NO",  "", NULL, "", "c",  "char(1)",    "YES", "", "q",  "",
        "c0", "char(0)",    "YES", "", NULL, "", "c2", "char(2)",    "NO",  "", NULL, "",
        "v",  "varchar(3)", "YES", "", "xy", "", "cv", "varchar(4)", "YES", "", NULL, "",
    };
    static const char *const show_create_rows[] = {
        "aliases",
        "CREATE TABLE `aliases` (\n"
        "  `id` int NOT NULL,\n"
        "  `c` char(1) DEFAULT 'q',\n"
        "  `c0` char(0) DEFAULT NULL,\n"
        "  `c2` char(2) NOT NULL,\n"
        "  `v` varchar(3) DEFAULT 'xy',\n"
        "  `cv` varchar(4) DEFAULT NULL\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const information_schema_rows[] = {
        "c",  "char",    "char(1)",    "1", "4",  "utf8mb4", "utf8mb4_0900_ai_ci", "YES", "q",
        "c0", "char",    "char(0)",    "0", "0",  "utf8mb4", "utf8mb4_0900_ai_ci", "YES", NULL,
        "c2", "char",    "char(2)",    "2", "8",  "utf8mb4", "utf8mb4_0900_ai_ci", "NO",  NULL,
        "v",  "varchar", "varchar(3)", "3", "12", "utf8mb4", "utf8mb4_0900_ai_ci", "YES", "xy",
        "cv", "varchar", "varchar(4)", "4", "16", "utf8mb4", "utf8mb4_0900_ai_ci", "YES", NULL,
    };
    static const char *const inserted_rows[] = {
        "1",
        "x",
        "",
        "ab",
        "a  ",
        "b   ",
    };
    static const char *const updated_char_rows[] = {"1", "z", "a  ", "cc"};
    static const char *const extra_column[] = {"extra", "char(2)", "NO", "", "x", ""};
    static const char *const vv_column[] = {"vv", "varchar(2)", "YES", "", "y", ""};
    static const char *const clone_columns[] = {
        "id",    "int",        "NO",  NULL, "c",  "char(1)",    "YES", "q",
        "c0",    "char(0)",    "YES", NULL, "c2", "char(2)",    "NO",  NULL,
        "v",     "varchar(3)", "YES", "xy", "cv", "varchar(4)", "YES", NULL,
        "extra", "char(2)",    "NO",  "x",  "vv", "varchar(2)", "YES", "y",
    };
    static const char *const key_show_create_rows[] = {
        "key_aliases",
        "CREATE TABLE `key_aliases` (\n"
        "  `c` char(3) NOT NULL,\n"
        "  `v` varchar(3) DEFAULT NULL,\n"
        "  `k` varchar(3) DEFAULT NULL,\n"
        "  PRIMARY KEY (`c`),\n"
        "  UNIQUE KEY `v` (`v`),\n"
        "  KEY `k_k` (`k`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
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

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open character alias file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE aliases ("
        "id INT NOT NULL, "
        "c CHARACTER DEFAULT 'q', "
        "c0 CHARACTER(0), "
        "c2 CHARACTER(2) NOT NULL, "
        "v CHARACTER VARYING(3) DEFAULT 'xy', "
        "cv CHAR VARYING(4))"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM aliases",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = show_columns_row_count,
            .context = "character aliases SHOW COLUMNS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE aliases",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "character aliases SHOW CREATE",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, DATA_TYPE, COLUMN_TYPE, CHARACTER_MAXIMUM_LENGTH, "
                   "CHARACTER_OCTET_LENGTH, CHARACTER_SET_NAME, COLLATION_NAME, IS_NULLABLE, "
                   "COLUMN_DEFAULT FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'aliases' "
                   "AND COLUMN_NAME <> 'id' ORDER BY ORDINAL_POSITION",
            .values = information_schema_rows,
            .column_count = information_schema_field_count,
            .row_count = information_schema_alias_row_count,
            .context = "character aliases information schema",
        }
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO aliases VALUES (1, 'x ', '', 'ab  ', 'a  ', 'b   ')",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, c, c0, c2, v, cv FROM aliases",
            .values = inserted_rows,
            .column_count = inserted_row_field_count,
            .row_count = 1U,
            .context = "character aliases row semantics",
        }
    );
    failures += expect_statement_result(
        database,
        "UPDATE aliases SET c2 = 'z ' WHERE id = 1",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "UPDATE aliases SET c2 = 'z ' WHERE id = 1",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "UPDATE aliases SET cv = 'cc' WHERE id = 1",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, c2, v, cv FROM aliases",
            .values = updated_char_rows,
            .column_count = 4U,
            .row_count = 1U,
            .context = "character aliases updated row",
        }
    );
    failures += expect_statement_ok(
        database,
        "ALTER TABLE aliases ADD COLUMN extra CHARACTER(2) NOT NULL DEFAULT 'x'"
    );
    failures += expect_statement_ok(
        database,
        "ALTER TABLE aliases ADD COLUMN vv CHARACTER VARYING(2) DEFAULT 'y'"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM aliases LIKE 'extra'",
            .values = extra_column,
            .column_count = show_columns_field_count,
            .row_count = 1U,
            .context = "added character alias column",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM aliases LIKE 'vv'",
            .values = vv_column,
            .column_count = show_columns_field_count,
            .row_count = 1U,
            .context = "added character varying alias column",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE clone LIKE aliases");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, COLUMN_TYPE, IS_NULLABLE, COLUMN_DEFAULT "
                   "FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'clone' "
                   "ORDER BY ORDINAL_POSITION",
            .values = clone_columns,
            .column_count = 4U,
            .row_count = clone_column_row_count,
            .context = "character aliases clone metadata",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE key_aliases ("
        "c CHARACTER(3) PRIMARY KEY, "
        "v CHARACTER VARYING(3) UNIQUE, "
        "k CHAR VARYING(3), "
        "KEY k_k (k))"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE key_aliases",
            .values = key_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "character alias key SHOW CREATE",
        }
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO key_aliases VALUES ('a', 'b', 'c')",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += execute_error(
        database,
        "INSERT INTO key_aliases VALUES ('a', 'd', 'e')",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry 'a'",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO key_aliases VALUES ('d', 'b', 'f')",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry 'b'",
        }
    );

    mylite_close(database);
    database = NULL;

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "character alias lifecycle preserves preamble"
    );

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen character alias file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, c2, v, cv FROM aliases",
            .values = updated_char_rows,
            .column_count = 4U,
            .row_count = 1U,
            .context = "character alias rows persist after reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_character_alias_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "open character alias diagnostics file"
    );
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");

    failures += execute_error(
        database,
        "CREATE TABLE bad_varying (v CHARACTER VARYING)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_char_varying (v CHAR VARYING)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_empty (v CHARACTER())",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_negative (v CHARACTER VARYING(-1))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_char_length (v CHARACTER(256))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "CHAR supports only lengths 0 through 255",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_varchar_length (v CHARACTER VARYING(256))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "VARCHAR supports only lengths 0 through 255",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_character_byte (v CHARACTER BYTE)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_char_byte (v CHAR BYTE)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_character_set (v CHARACTER SET utf8mb4)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_national (v NATIONAL CHARACTER VARYING(2))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
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
        (void)fprintf(
            stderr,
            "%s: expected success, got rc=%d err=%d state=%s message=%s\n",
            sql,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }

    *out_result = result;
    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc == MYLITE_OK) {
        (void)fprintf(stderr, "%s: expected error, got success\n", sql);
        mylite_result_free(result);
        return 1;
    }
    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text_or_null(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

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

    if (result != NULL) {
        failures += expect_size(mylite_result_column_count(result), 0U, sql);
        failures += expect_size(mylite_result_row_count(result), 0U, sql);
        failures += expect_int64(mylite_result_affected_rows(result), expected.affected_rows, sql);
        failures += expect_size(mylite_result_warning_count(result), expected.warning_count, sql);
    }
    mylite_result_free(result);
    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    if (result != NULL) {
        failures +=
            expect_size(mylite_result_column_count(result), query.column_count, query.context);
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
        failures += expect_int64(mylite_result_affected_rows(result), 0, query.context);
        failures += expect_size(mylite_result_warning_count(result), 0U, query.context);
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
    return expect_text_or_null(mylite_result_value_text(result, row, column), expected, context);
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_character_alias_lifecycle_%ld_%s.mylite",
        (long)current_process_id(),
        name
    );

    if (written < 0 || (size_t)written >= path_size) {
        (void)fprintf(stderr, "failed to build test path\n");
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
    char buffer[test_path_capacity + related_file_suffix_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(buffer)) {
        (void)remove(buffer);
    }
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t bytes_read = 0U;

    if (file == NULL) {
        (void)fprintf(stderr, "%s: failed to open file\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        (void)fprintf(stderr, "%s: failed to seek file\n", path);
        fclose(file);
        return 1;
    }
    bytes_read = fread(buffer, 1U, size, file);
    if (bytes_read != size) {
        (void)fprintf(stderr, "%s: expected %zu bytes, got %zu\n", path, size, bytes_read);
        fclose(file);
        return 1;
    }
    fclose(file);
    return 0;
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    (void)fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
    return 1;
}

static int expect_int64(int64_t actual, int64_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    (void)fprintf(
        stderr,
        "%s: expected %lld, got %lld\n",
        context,
        (long long)expected,
        (long long)actual
    );
    return 1;
}

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    (void)fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
    return 1;
}

static int expect_text_or_null(const char *actual, const char *expected, const char *context) {
    if (actual == NULL && expected == NULL) {
        return 0;
    }
    if (actual != NULL && expected != NULL && strcmp(actual, expected) == 0) {
        return 0;
    }
    (void)fprintf(
        stderr,
        "%s: expected [%s], got [%s]\n",
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
    (void)fprintf(
        stderr,
        "%s: expected [%s] to contain [%s]\n",
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
    (void)fprintf(stderr, "%s: bytes differ\n", context);
    return 1;
}
