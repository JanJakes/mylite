#include "mylite_test_support.h"

#include <mylite/mylite.h>

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
    all_outer_row_count = 5,
    mysql_error_no_database_selected = 1046,
    mysql_error_not_supported_yet = 1235,
    mysql_error_operand_should_contain_one_column = 1241,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_query {
    const char *sql;
    const char *const *columns;
    size_t column_count;
    const char *const *values;
    size_t row_count;
    const char *context;
};

static int test_quantified_subquery_values(void);
static int test_quantified_subquery_diagnostics(void);
static int open_app_database(
    mylite_db **out_database,
    const char *name,
    char *path,
    size_t path_size
);
static int seed_quantified_subquery_tables(mylite_db *database);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query expected);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_quantified_subquery_values();
    failures += test_quantified_subquery_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_quantified_subquery_values(void) {
    static const char *const id_column[] = {"id"};
    static const char *const ids_2_3[] = {"2", "3"};
    static const char *const ids_3_5[] = {"3", "5"};
    static const char *const id_1[] = {"1"};
    static const char *const ids_1_5[] = {"1", "5"};
    static const char *const all_ids[] = {"1", "2", "3", "4", "5"};
    static const char *const ids_1_2_4[] = {"1", "2", "4"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "values", path, sizeof(path));
    failures += seed_quantified_subquery_tables(database);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM outer_t WHERE v = ANY (SELECT v FROM inner_t) ORDER BY id",
            .columns = id_column,
            .column_count = 1U,
            .values = ids_2_3,
            .row_count = 2U,
            .context = "equals any",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM outer_t WHERE v = SOME (SELECT v FROM inner_t) ORDER BY id",
            .columns = id_column,
            .column_count = 1U,
            .values = ids_2_3,
            .row_count = 2U,
            .context = "equals some",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM outer_t WHERE v > ANY (SELECT v FROM inner_t) ORDER BY id",
            .columns = id_column,
            .column_count = 1U,
            .values = ids_3_5,
            .row_count = 2U,
            .context = "greater than any",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM outer_t WHERE v < ALL "
                   "(SELECT v FROM inner_t WHERE v IS NOT NULL) ORDER BY id",
            .columns = id_column,
            .column_count = 1U,
            .values = id_1,
            .row_count = 1U,
            .context = "less than all",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM outer_t WHERE v <> ALL "
                   "(SELECT v FROM inner_t WHERE v IS NOT NULL) ORDER BY id",
            .columns = id_column,
            .column_count = 1U,
            .values = ids_1_5,
            .row_count = 2U,
            .context = "not equal all",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM outer_t WHERE v = ALL (SELECT v FROM empty_t) ORDER BY id",
            .columns = id_column,
            .column_count = 1U,
            .values = all_ids,
            .row_count = all_outer_row_count,
            .context = "all over empty subquery",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM outer_t WHERE v = ANY (SELECT v FROM empty_t) ORDER BY id",
            .columns = id_column,
            .column_count = 1U,
            .values = NULL,
            .row_count = 0U,
            .context = "any over empty subquery",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM outer_t WHERE s = ANY (SELECT s FROM inner_t) ORDER BY id",
            .columns = id_column,
            .column_count = 1U,
            .values = ids_2_3,
            .row_count = 2U,
            .context = "string any uses default collation",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM outer_t WHERE s <> ALL "
                   "(SELECT s FROM inner_t WHERE s IS NOT NULL) ORDER BY id",
            .columns = id_column,
            .column_count = 1U,
            .values = ids_1_5,
            .row_count = 2U,
            .context = "string not equal all uses default collation",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM outer_t WHERE "
                   "v = ANY (SELECT v FROM inner_t WHERE v IS NULL) IS UNKNOWN "
                   "ORDER BY id",
            .columns = id_column,
            .column_count = 1U,
            .values = all_ids,
            .row_count = all_outer_row_count,
            .context = "any unknown suffix",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM outer_t WHERE NOT (v = ANY (SELECT v FROM inner_t)) "
                   "ORDER BY id",
            .columns = id_column,
            .column_count = 1U,
            .values = NULL,
            .row_count = 0U,
            .context = "not any preserves unknown",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM outer_t WHERE v <= ALL (SELECT v FROM inner_t) ORDER BY id",
            .columns = id_column,
            .column_count = 1U,
            .values = NULL,
            .row_count = 0U,
            .context = "all with inner null filters direct where",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM outer_t WHERE "
                   "v <= ALL (SELECT v FROM inner_t) IS UNKNOWN "
                   "ORDER BY id",
            .columns = id_column,
            .column_count = 1U,
            .values = ids_1_2_4,
            .row_count = 3U,
            .context = "all false takes precedence over unknown",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM outer_t WHERE "
                   "v = ANY (SELECT v FROM inner_t) IS NOT UNKNOWN "
                   "ORDER BY id",
            .columns = id_column,
            .column_count = 1U,
            .values = ids_2_3,
            .row_count = 2U,
            .context = "any not unknown suffix",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM outer_t WHERE v >= ALL "
                   "(SELECT v FROM inner_t WHERE v IS NOT NULL) ORDER BY id",
            .columns = id_column,
            .column_count = 1U,
            .values = ids_3_5,
            .row_count = 2U,
            .context = "greater equal all",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT o.id FROM outer_t AS o WHERE o.v = ANY "
                   "(SELECT i.v FROM inner_t AS i WHERE i.v = o.v) ORDER BY o.id",
            .columns = id_column,
            .column_count = 1U,
            .values = ids_2_3,
            .row_count = 2U,
            .context = "correlated integer any",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_quantified_subquery_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "no-schema") != 0) {
        return 1;
    }
    remove_related_files(path);
    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open no selected schema");
    failures += execute_error(
        database,
        "SELECT id FROM outer_t WHERE v = ANY (SELECT v FROM inner_t)",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    mylite_close(database);
    remove_related_files(path);

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += seed_quantified_subquery_tables(database);
    failures += execute_error(
        database,
        "SELECT id FROM outer_t WHERE v = ANY (SELECT v, s FROM inner_t)",
        (struct expected_sql_error){
            .code = mysql_error_operand_should_contain_one_column,
            .sqlstate = "21000",
            .message_part = "Operand should contain 1 column(s)",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM outer_t WHERE v = ANY (SELECT v FROM inner_t LIMIT 1)",
        (struct expected_sql_error){
            .code = mysql_error_not_supported_yet,
            .sqlstate = "42000",
            .message_part = "LIMIT & IN/ALL/ANY/SOME subquery",
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

static int seed_quantified_subquery_tables(mylite_db *database) {
    int failures = 0;

    failures += execute_ok(
        database,
        "CREATE TABLE outer_t (id INT PRIMARY KEY, v INT NULL, s VARCHAR(20) NULL)",
        NULL
    );
    failures += execute_ok(database, "CREATE TABLE inner_t (v INT NULL, s VARCHAR(20) NULL)", NULL);
    failures += execute_ok(database, "CREATE TABLE empty_t (v INT NULL, s VARCHAR(20) NULL)", NULL);
    failures += execute_ok(
        database,
        "INSERT INTO outer_t VALUES "
        "(1, 1, 'ann'), "
        "(2, 2, 'BOB'), "
        "(3, 3, 'cat'), "
        "(4, NULL, NULL), "
        "(5, 5, 'eve')",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO inner_t VALUES (2, 'bob'), (3, 'CAT'), (NULL, NULL)",
        NULL
    );
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: expected success, got %d: %s\n", sql, rc, mylite_errmsg(database));
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

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, expected.sql, &result);

    if (failures != 0) {
        return failures;
    }
    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        expected.column_count,
        expected.context
    );
    failures += mylite_test_expect_size(
        mylite_result_row_count(result),
        expected.row_count,
        expected.context
    );
    failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, expected.context);

    for (size_t column = 0U; column < expected.column_count; ++column) {
        failures += mylite_test_expect_text(
            mylite_result_column_name(result, column),
            expected.columns[column],
            expected.context
        );
    }
    for (size_t row = 0U; row < expected.row_count; ++row) {
        for (size_t column = 0U; column < expected.column_count; ++column) {
            size_t value_index = (row * expected.column_count) + column;

            failures += expect_result_value(
                result,
                row,
                column,
                expected.values[value_index],
                expected.context
            );
        }
    }

    mylite_result_free(result);
    return failures;
}

static void remove_related_files(const char *path) {
    remove_with_suffix(path, "");
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related_path[test_path_capacity + path_suffix_capacity];
    int written = snprintf(related_path, sizeof(related_path), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related_path)) {
        return;
    }
    (void)remove(related_path);
}

static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
) {
    const char *actual = mylite_result_value_text(result, row, column);

    return mylite_test_expect_text(actual, expected, context);
}
