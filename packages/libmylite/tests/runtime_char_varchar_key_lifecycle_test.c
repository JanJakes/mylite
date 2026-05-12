#include <mylite/mylite.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    show_columns_field_count = 6,
    show_index_field_count = 15,
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

struct expected_dml_result {
    int64_t affected_rows;
    size_t warning_count;
};

static int test_create_time_string_key_metadata_and_dml(void);
static int test_string_key_duplicate_semantics(void);
static int test_nullable_unique_and_alter_add_primary_key(void);
static int test_non_ascii_string_key_values_are_rejected(void);
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
static int expect_alter_primary_key_ok(mylite_db *database, const char *sql);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

int main(void) {
    int failures = 0;

    failures += test_create_time_string_key_metadata_and_dml();
    failures += test_string_key_duplicate_semantics();
    failures += test_nullable_unique_and_alter_add_primary_key();
    failures += test_non_ascii_string_key_values_are_rejected();

    return failures == 0 ? 0 : 1;
}

static int test_create_time_string_key_metadata_and_dml(void) {
    static const char *const show_columns_rows[] = {
        "c",
        "char(3)",
        "NO",
        "PRI",
        NULL,
        "",
        "v",
        "varchar(3)",
        "YES",
        "UNI",
        NULL,
        "",
        "n",
        "varchar(3)",
        "NO",
        "UNI",
        "x",
        "",
    };
    static const char *const show_index_rows[] = {
        "pk_inline", "0",     "PRIMARY", "1",         "c",     "A",  "0",         NULL,    NULL,
        "",          "BTREE", "",        "",          "YES",   NULL, "pk_inline", "0",     "n",
        "1",         "n",     "A",       "0",         NULL,    NULL, "",          "BTREE", "",
        "",          "YES",   NULL,      "pk_inline", "0",     "v",  "1",         "v",     "A",
        "0",         NULL,    NULL,      "YES",       "BTREE", "",   "",          "YES",   NULL,
    };
    static const char *const show_create_rows[] = {
        "pk_inline",
        "CREATE TABLE `pk_inline` (\n"
        "  `c` char(3) NOT NULL,\n"
        "  `v` varchar(3) DEFAULT NULL,\n"
        "  `n` varchar(3) NOT NULL DEFAULT 'x',\n"
        "  PRIMARY KEY (`c`),\n"
        "  UNIQUE KEY `n` (`n`),\n"
        "  UNIQUE KEY `v` (`v`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const table_level_rows[] = {"a", "b", "c", "d"};
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        expect_int(mylite_open(":memory:", &database), MYLITE_OK, "open create-time database");
    failures += create_schema(database);
    failures += expect_statement_ok(
        database,
        "CREATE TABLE pk_inline ("
        "c CHAR(3) PRIMARY KEY, v VARCHAR(3) UNIQUE, n VARCHAR(3) UNIQUE NOT NULL DEFAULT 'x')"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM pk_inline",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = 3U,
            .context = "string key SHOW COLUMNS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM pk_inline",
            .values = show_index_rows,
            .column_count = show_index_field_count,
            .row_count = 3U,
            .context = "string key SHOW INDEX",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE pk_inline",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "string key SHOW CREATE",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE table_level (id INT, c CHAR(3), v VARCHAR(3), "
        "PRIMARY KEY (c), UNIQUE KEY u_v (v))"
    );
    failures +=
        expect_dml_ok(database, "INSERT INTO table_level VALUES (1, 'a', 'b'), (2, 'c', 'd')", 2);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT c, v FROM table_level ORDER BY id",
            .values = table_level_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "table-level string keys",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_string_key_duplicate_semantics(void) {
    static const char *const varchar_space_rows[] = {"1", "a", "2", "a "};
    static const char *const update_unique_rows[] = {"1", "abc", "2", "def"};
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        expect_int(mylite_open(":memory:", &database), MYLITE_OK, "open duplicate database");
    failures += create_schema(database);

    failures += expect_statement_ok(database, "CREATE TABLE pkdup (c CHAR(3) PRIMARY KEY)");
    failures += expect_dml_ok(database, "INSERT INTO pkdup VALUES ('abc')", 1);
    failures += execute_error(
        database,
        "INSERT INTO pkdup VALUES ('ABC')",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry 'ABC' for key 'pkdup.PRIMARY'",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE unique_case (v VARCHAR(3) UNIQUE)");
    failures += expect_dml_ok(database, "INSERT INTO unique_case VALUES ('abc')", 1);
    failures += execute_error(
        database,
        "INSERT INTO unique_case VALUES ('ABC')",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry 'ABC' for key 'unique_case.v'",
        }
    );
    failures += expect_dml_result(
        database,
        "INSERT IGNORE INTO unique_case VALUES ('ABC')",
        (struct expected_dml_result){.affected_rows = 0, .warning_count = 1U}
    );

    failures +=
        expect_statement_ok(database, "CREATE TABLE update_unique (id INT, v VARCHAR(3) UNIQUE)");
    failures +=
        expect_dml_ok(database, "INSERT INTO update_unique VALUES (1, 'abc'), (2, 'def')", 2);
    failures += execute_error(
        database,
        "UPDATE update_unique SET v = 'ABC' WHERE id = 2",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry 'ABC' for key 'update_unique.v'",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM update_unique ORDER BY id",
            .values = update_unique_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "string unique update duplicate leaves rows unchanged",
        }
    );

    failures +=
        expect_statement_ok(database, "CREATE TABLE varchar_spaces (id INT, v VARCHAR(5) UNIQUE)");
    failures += expect_dml_ok(database, "INSERT INTO varchar_spaces VALUES (1, 'a'), (2, 'a ')", 2);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM varchar_spaces ORDER BY id",
            .values = varchar_space_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "VARCHAR trailing-space distinct keys",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE char_spaces (c CHAR(5) UNIQUE)");
    failures += expect_dml_ok(database, "INSERT INTO char_spaces VALUES ('a')", 1);
    failures += execute_error(
        database,
        "INSERT INTO char_spaces VALUES ('a ')",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry 'a' for key 'char_spaces.c'",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_nullable_unique_and_alter_add_primary_key(void) {
    static const char *const nullable_rows[] = {"1", NULL, "2", NULL, "3", "a"};
    static const char *const add_pk_show_create_rows[] = {
        "add_pk_v_space",
        "CREATE TABLE `add_pk_v_space` (\n"
        "  `v` varchar(10) NOT NULL,\n"
        "  PRIMARY KEY (`v`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open(":memory:", &database), MYLITE_OK, "open alter database");
    failures += create_schema(database);

    failures += expect_statement_ok(
        database,
        "CREATE TABLE nullable_unique (id INT, v VARCHAR(5), UNIQUE KEY u_v (v))"
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO nullable_unique VALUES (1, NULL), (2, NULL), (3, 'a')",
        3
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM nullable_unique ORDER BY id",
            .values = nullable_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "nullable unique string key",
        }
    );

    failures +=
        expect_statement_ok(database, "CREATE TABLE add_pk_v_space (v VARCHAR(10) NOT NULL)");
    failures += expect_dml_ok(database, "INSERT INTO add_pk_v_space VALUES ('a'), ('a ')", 2);
    failures +=
        expect_alter_primary_key_ok(database, "ALTER TABLE add_pk_v_space ADD PRIMARY KEY (v)");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE add_pk_v_space",
            .values = add_pk_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "ALTER ADD VARCHAR primary key trailing-space distinct",
        }
    );

    failures +=
        expect_statement_ok(database, "CREATE TABLE add_pk_v_case (v VARCHAR(10) NOT NULL)");
    failures += expect_dml_ok(database, "INSERT INTO add_pk_v_case VALUES ('a'), ('A')", 2);
    failures += execute_error(
        database,
        "ALTER TABLE add_pk_v_case ADD PRIMARY KEY (v)",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry 'a' for key 'add_pk_v_case.PRIMARY'",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_non_ascii_string_key_values_are_rejected(void) {
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        expect_int(mylite_open(":memory:", &database), MYLITE_OK, "open non-ascii database");
    failures += create_schema(database);
    failures += expect_statement_ok(database, "CREATE TABLE non_ascii (v VARCHAR(5) UNIQUE)");
    failures += execute_error(
        database,
        "INSERT INTO non_ascii VALUES ('\xC3\xA9')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "non-ASCII string key values are not supported",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE non_ascii_update (id INT, v VARCHAR(5) UNIQUE)"
    );
    failures += expect_dml_ok(database, "INSERT INTO non_ascii_update VALUES (1, 'abc')", 1);
    failures += execute_error(
        database,
        "UPDATE non_ascii_update SET v = '\xC3\xA9' WHERE id = 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "non-ASCII string key values are not supported",
        }
    );

    mylite_close(database);
    return failures;
}

static int create_schema(mylite_db *database) {
    int failures = 0;

    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected success, got %d %s %s\n",
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
        fprintf(stderr, "%s: expected error, got success\n", sql);
        mylite_result_free(result);
        return 1;
    }
    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, sql);
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

    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, sql);
        failures += expect_size(mylite_result_row_count(result), 0U, sql);
        failures += expect_int64(mylite_result_affected_rows(result), expected.affected_rows, sql);
        failures += expect_size(mylite_result_warning_count(result), expected.warning_count, sql);
    }
    mylite_result_free(result);
    return failures;
}

static int expect_alter_primary_key_ok(mylite_db *database, const char *sql) {
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
            actual != NULL ? actual : "<NULL>"
        );
        return 1;
    }
    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected [%s] to contain [%s]\n",
            context,
            actual != NULL ? actual : "<NULL>",
            needle
        );
        return 1;
    }
    return 0;
}
