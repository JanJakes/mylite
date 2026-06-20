#include <mylite/mylite.h>

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
    test_path_suffix_capacity = 16,
    sqlite_sql_capacity = 2048,
    mysql_error_parse = 1064,
    mysql_error_no_database_selected = 1046,
    mysql_error_table_does_not_exist = 1146,
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

static int test_statistical_values_and_grouping(void);
static int test_statistical_diagnostics(void);
static int seed_database(mylite_db *database);
static int create_numbers_table(mylite_db *database, const char *table_name);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query query);
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
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

int main(void) {
    int failures = 0;

    failures += test_statistical_values_and_grouping();
    failures += test_statistical_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_statistical_values_and_grouping(void) {
    static const char *const grouped_columns[] = {
        "g",
        "STDDEV_POP(n)",
        "STDDEV_SAMP(n)",
        "VAR_POP(n)",
        "VAR_SAMP(n)",
    };
    static const char *const grouped_values[] = {
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "1",
        "0",
        NULL,
        "0",
        NULL,
        "2",
        "5",
        "7.0710678118654755",
        "25",
        "50",
    };
    static const char *const grouped_alias_columns[] = {"g", "s"};
    static const char *const grouped_var_alias_columns[] = {"g", "v"};
    static const char *const grouped_stddev_pop_alias_order_values[] = {
        NULL,
        NULL,
        "1",
        "0",
        "2",
        "5",
    };
    static const char *const grouped_var_pop_alias_desc_limit_values[] = {
        "2",
        "25",
        "1",
        "0",
    };
    static const char *const grouped_stddev_samp_alias_desc_limit_values[] = {
        "2",
        "7.0710678118654755",
    };
    static const char *const grouped_var_samp_alias_desc_limit_values[] = {
        "2",
        "50",
    };
    static const char *const grouped_std_alias_desc_limit_values[] = {
        "2",
        "5",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "values") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open values file");
    failures += seed_database(database);
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += create_numbers_table(database, "numbers");
    failures += create_numbers_table(database, "all_null_numbers");
    failures += execute_ok(database, "TRUNCATE TABLE all_null_numbers", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "INSERT INTO all_null_numbers(id, g, n, nn, s) VALUES (1, NULL, NULL, 5, NULL), "
        "(2, NULL, NULL, 7, NULL)",
        &result
    );
    mylite_result_free(result);
    result = NULL;

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT STD(n) FROM numbers",
            .columns = (const char *const[]){"STD(n)"},
            .column_count = 1U,
            .values = (const char *const[]){"8.16496580927726"},
            .row_count = 1U,
            .context = "std alias",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT STDDEV(n) FROM numbers",
            .columns = (const char *const[]){"STDDEV(n)"},
            .column_count = 1U,
            .values = (const char *const[]){"8.16496580927726"},
            .row_count = 1U,
            .context = "stddev alias",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT STDDEV_POP(n) FROM numbers",
            .columns = (const char *const[]){"STDDEV_POP(n)"},
            .column_count = 1U,
            .values = (const char *const[]){"8.16496580927726"},
            .row_count = 1U,
            .context = "stddev population",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT STDDEV_SAMP(n) FROM numbers",
            .columns = (const char *const[]){"STDDEV_SAMP(n)"},
            .column_count = 1U,
            .values = (const char *const[]){"10"},
            .row_count = 1U,
            .context = "stddev sample",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT VAR_POP(n) FROM numbers",
            .columns = (const char *const[]){"VAR_POP(n)"},
            .column_count = 1U,
            .values = (const char *const[]){"66.66666666666667"},
            .row_count = 1U,
            .context = "variance population",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT VAR_SAMP(n) FROM numbers",
            .columns = (const char *const[]){"VAR_SAMP(n)"},
            .column_count = 1U,
            .values = (const char *const[]){"100"},
            .row_count = 1U,
            .context = "variance sample",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT VARIANCE(n) FROM numbers",
            .columns = (const char *const[]){"VARIANCE(n)"},
            .column_count = 1U,
            .values = (const char *const[]){"66.66666666666667"},
            .row_count = 1U,
            .context = "variance alias",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT STDDEV_POP(n) FROM all_null_numbers",
            .columns = (const char *const[]){"STDDEV_POP(n)"},
            .column_count = 1U,
            .values = (const char *const[]){NULL},
            .row_count = 1U,
            .context = "all null statistical values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT STDDEV_POP(n) FROM numbers WHERE id = 2",
            .columns = (const char *const[]){"STDDEV_POP(n)"},
            .column_count = 1U,
            .values = (const char *const[]){"0"},
            .row_count = 1U,
            .context = "single population stddev",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT STDDEV_SAMP(n) FROM numbers WHERE id = 2",
            .columns = (const char *const[]){"STDDEV_SAMP(n)"},
            .column_count = 1U,
            .values = (const char *const[]){NULL},
            .row_count = 1U,
            .context = "single sample stddev",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT VAR_POP(n) FROM numbers WHERE id = 2",
            .columns = (const char *const[]){"VAR_POP(n)"},
            .column_count = 1U,
            .values = (const char *const[]){"0"},
            .row_count = 1U,
            .context = "single population variance",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT VAR_SAMP(n) FROM numbers WHERE id = 2",
            .columns = (const char *const[]){"VAR_SAMP(n)"},
            .column_count = 1U,
            .values = (const char *const[]){NULL},
            .row_count = 1U,
            .context = "single sample variance",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT g, STDDEV_POP(n), STDDEV_SAMP(n), VAR_POP(n), VAR_SAMP(n) "
                   "FROM numbers GROUP BY g ORDER BY g",
            .columns = grouped_columns,
            .column_count = sizeof(grouped_columns) / sizeof(grouped_columns[0]),
            .values = grouped_values,
            .row_count = 3U,
            .context = "grouped statistical values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT g, STDDEV_POP(n) AS s FROM numbers GROUP BY g ORDER BY s",
            .columns = grouped_alias_columns,
            .column_count = sizeof(grouped_alias_columns) / sizeof(grouped_alias_columns[0]),
            .values = grouped_stddev_pop_alias_order_values,
            .row_count = 3U,
            .context = "grouped statistical stddev_pop aggregate alias order",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT g, STDDEV_POP(n) AS s FROM numbers GROUP BY g "
                   "ORDER BY STDDEV_POP(n)",
            .columns = grouped_alias_columns,
            .column_count = sizeof(grouped_alias_columns) / sizeof(grouped_alias_columns[0]),
            .values = grouped_stddev_pop_alias_order_values,
            .row_count = 3U,
            .context = "grouped statistical stddev_pop aggregate expression order",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT g, VAR_POP(n) AS v FROM numbers GROUP BY g ORDER BY v DESC LIMIT 2",
            .columns = grouped_var_alias_columns,
            .column_count =
                sizeof(grouped_var_alias_columns) / sizeof(grouped_var_alias_columns[0]),
            .values = grouped_var_pop_alias_desc_limit_values,
            .row_count = 2U,
            .context = "grouped statistical var_pop aggregate alias descending limit",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT g, VAR_POP(n) AS v FROM numbers GROUP BY g "
                   "ORDER BY VAR_POP(n) DESC LIMIT 2",
            .columns = grouped_var_alias_columns,
            .column_count =
                sizeof(grouped_var_alias_columns) / sizeof(grouped_var_alias_columns[0]),
            .values = grouped_var_pop_alias_desc_limit_values,
            .row_count = 2U,
            .context = "grouped statistical var_pop aggregate expression descending limit",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT g, STDDEV_SAMP(n) AS s FROM numbers GROUP BY g ORDER BY s DESC LIMIT 1",
            .columns = grouped_alias_columns,
            .column_count = sizeof(grouped_alias_columns) / sizeof(grouped_alias_columns[0]),
            .values = grouped_stddev_samp_alias_desc_limit_values,
            .row_count = 1U,
            .context = "grouped statistical stddev_samp aggregate alias descending limit",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT g, STDDEV_SAMP(n) AS s FROM numbers GROUP BY g "
                   "ORDER BY STDDEV_SAMP(n) DESC LIMIT 1",
            .columns = grouped_alias_columns,
            .column_count = sizeof(grouped_alias_columns) / sizeof(grouped_alias_columns[0]),
            .values = grouped_stddev_samp_alias_desc_limit_values,
            .row_count = 1U,
            .context = "grouped statistical stddev_samp aggregate expression descending limit",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT g, VAR_SAMP(n) AS v FROM numbers GROUP BY g ORDER BY v DESC LIMIT 1",
            .columns = grouped_var_alias_columns,
            .column_count =
                sizeof(grouped_var_alias_columns) / sizeof(grouped_var_alias_columns[0]),
            .values = grouped_var_samp_alias_desc_limit_values,
            .row_count = 1U,
            .context = "grouped statistical var_samp aggregate alias descending limit",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT g, VAR_SAMP(n) AS v FROM numbers GROUP BY g "
                   "ORDER BY VAR_SAMP(n) DESC LIMIT 1",
            .columns = grouped_var_alias_columns,
            .column_count =
                sizeof(grouped_var_alias_columns) / sizeof(grouped_var_alias_columns[0]),
            .values = grouped_var_samp_alias_desc_limit_values,
            .row_count = 1U,
            .context = "grouped statistical var_samp aggregate expression descending limit",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT g, STD(n) AS s FROM numbers GROUP BY g ORDER BY s DESC LIMIT 1",
            .columns = grouped_alias_columns,
            .column_count = sizeof(grouped_alias_columns) / sizeof(grouped_alias_columns[0]),
            .values = grouped_std_alias_desc_limit_values,
            .row_count = 1U,
            .context = "grouped statistical std aggregate alias descending limit",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT g, STD(n) AS s FROM numbers GROUP BY g ORDER BY STD(n) DESC LIMIT 1",
            .columns = grouped_alias_columns,
            .column_count = sizeof(grouped_alias_columns) / sizeof(grouped_alias_columns[0]),
            .values = grouped_std_alias_desc_limit_values,
            .row_count = 1U,
            .context = "grouped statistical std aggregate expression descending limit",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT g, STDDEV(n) AS s FROM numbers GROUP BY g "
                   "ORDER BY STDDEV(n) DESC LIMIT 1",
            .columns = grouped_alias_columns,
            .column_count = sizeof(grouped_alias_columns) / sizeof(grouped_alias_columns[0]),
            .values = grouped_std_alias_desc_limit_values,
            .row_count = 1U,
            .context = "grouped statistical stddev aggregate expression descending limit",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT g, VARIANCE(n) AS v FROM numbers GROUP BY g ORDER BY v DESC LIMIT 1",
            .columns = grouped_var_alias_columns,
            .column_count =
                sizeof(grouped_var_alias_columns) / sizeof(grouped_var_alias_columns[0]),
            .values = grouped_var_pop_alias_desc_limit_values,
            .row_count = 1U,
            .context = "grouped statistical variance aggregate alias descending limit",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT g, VARIANCE(n) AS v FROM numbers GROUP BY g "
                   "ORDER BY VARIANCE(n) DESC LIMIT 1",
            .columns = grouped_var_alias_columns,
            .column_count =
                sizeof(grouped_var_alias_columns) / sizeof(grouped_var_alias_columns[0]),
            .values = grouped_var_pop_alias_desc_limit_values,
            .row_count = 1U,
            .context = "grouped statistical variance aggregate expression descending limit",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT g, STDDEV_POP(n + 1) AS s FROM numbers GROUP BY g ORDER BY s",
            .columns = grouped_alias_columns,
            .column_count = sizeof(grouped_alias_columns) / sizeof(grouped_alias_columns[0]),
            .values = grouped_stddev_pop_alias_order_values,
            .row_count = 3U,
            .context = "grouped statistical row-scalar aggregate alias order",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT STDDEV_POP(n + 1) FROM numbers",
            .columns = (const char *const[]){"STDDEV_POP(n + 1)"},
            .column_count = 1U,
            .values = (const char *const[]){"8.16496580927726"},
            .row_count = 1U,
            .context = "row scalar stddev argument",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT VAR_POP(n + 1) FROM numbers",
            .columns = (const char *const[]){"VAR_POP(n + 1)"},
            .column_count = 1U,
            .values = (const char *const[]){"66.66666666666667"},
            .row_count = 1U,
            .context = "row scalar variance argument",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT STDDEV_POP(1)",
            .columns = (const char *const[]){"STDDEV_POP(1)"},
            .column_count = 1U,
            .values = (const char *const[]){"0"},
            .row_count = 1U,
            .context = "tableless population stddev",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT STDDEV_SAMP(1)",
            .columns = (const char *const[]){"STDDEV_SAMP(1)"},
            .column_count = 1U,
            .values = (const char *const[]){NULL},
            .row_count = 1U,
            .context = "tableless sample stddev",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT VAR_POP(1)",
            .columns = (const char *const[]){"VAR_POP(1)"},
            .column_count = 1U,
            .values = (const char *const[]){"0"},
            .row_count = 1U,
            .context = "tableless population variance",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT VAR_SAMP(1)",
            .columns = (const char *const[]){"VAR_SAMP(1)"},
            .column_count = 1U,
            .values = (const char *const[]){NULL},
            .row_count = 1U,
            .context = "tableless sample variance",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT STD(1)",
            .columns = (const char *const[]){"STD(1)"},
            .column_count = 1U,
            .values = (const char *const[]){"0"},
            .row_count = 1U,
            .context = "tableless std alias",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT VARIANCE(1)",
            .columns = (const char *const[]){"VARIANCE(1)"},
            .column_count = 1U,
            .values = (const char *const[]){"0"},
            .row_count = 1U,
            .context = "tableless variance alias",
        }
    );

    failures += execute_ok(database, "RENAME TABLE numbers TO stat_numbers", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT VAR_POP(n) FROM stat_numbers",
            .columns = (const char *const[]){"VAR_POP(n)"},
            .column_count = 1U,
            .values = (const char *const[]){"66.66666666666667"},
            .row_count = 1U,
            .context = "renamed statistical source",
        }
    );
    failures += execute_ok(database, "TRUNCATE TABLE stat_numbers", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT VAR_POP(n) FROM stat_numbers",
            .columns = (const char *const[]){"VAR_POP(n)"},
            .column_count = 1U,
            .values = (const char *const[]){NULL},
            .row_count = 1U,
            .context = "truncated statistical source",
        }
    );
    failures += execute_ok(database, "DROP TABLE stat_numbers", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_error(
        database,
        "SELECT VAR_POP(n) FROM stat_numbers",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.stat_numbers' doesn't exist",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_statistical_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics file");
    failures += execute_error(
        database,
        "SELECT STDDEV_POP(n) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += seed_database(database);
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += create_numbers_table(database, "numbers");

    failures += execute_error(
        database,
        "SELECT STDDEV_POP(n) FROM missing",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "SELECT STDDEV_POP(s) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "statistical aggregate supports only integer descriptor columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT STDDEV_POP(n) OVER () FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "aggregate window functions are not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT STDDEV_POP(DISTINCT n) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int seed_database(mylite_db *database) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, "CREATE DATABASE app", &result);

    mylite_result_free(result);
    return failures;
}

static int create_numbers_table(mylite_db *database, const char *table_name) {
    char sql[sqlite_sql_capacity];
    mylite_result *result = NULL;
    int written = snprintf(
        sql,
        sizeof(sql),
        "CREATE TABLE %s(id INT NOT NULL, g INT NULL, n INT NULL, nn INT NOT NULL, "
        "s VARCHAR(20) NULL)",
        table_name
    );
    int failures = 0;

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "create table SQL is too long for %s\n", table_name);
        return 1;
    }
    failures += execute_ok(database, sql, &result);
    mylite_result_free(result);
    result = NULL;

    written = snprintf(
        sql,
        sizeof(sql),
        "INSERT INTO %s VALUES "
        "(1, NULL, NULL, 5, '1'), "
        "(2, 1, 10, 7, '2'), "
        "(3, 2, 20, 8, 'x'), "
        "(4, 2, 30, 9, NULL)",
        table_name
    );
    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "insert SQL is too long for %s\n", table_name);
        return failures + 1;
    }
    failures += execute_ok(database, sql, &result);
    mylite_result_free(result);

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
        *out_result = NULL;
        return 1;
    }

    *out_result = result;
    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    if (rc == MYLITE_OK) {
        fprintf(stderr, "execute '%s': expected error, got OK\n", sql);
        mylite_result_free(result);
        return 1;
    }
    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    if (result == NULL) {
        return failures + 1;
    }
    failures += expect_size(mylite_result_column_count(result), query.column_count, query.context);
    failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
    for (size_t column = 0U; column < query.column_count; ++column) {
        failures += expect_text(
            mylite_result_column_name(result, column),
            query.columns[column],
            query.sql
        );
    }
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
                "%s row %zu col %zu: expected NULL, got '%s'\n",
                context,
                row,
                column,
                actual
            );
            return 1;
        }
        return 0;
    }
    if (actual == NULL) {
        fprintf(
            stderr,
            "%s row %zu col %zu: expected '%s', got NULL\n",
            context,
            row,
            column,
            expected
        );
        return 1;
    }
    return expect_text(actual, expected, context);
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_statistical_aggregates_%d_%s.mylite",
        current_process_id(),
        name
    );

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "test path is too long\n");
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
    remove_with_suffix(path, "-shm");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-journal");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related[test_path_capacity + test_path_suffix_capacity];
    int written = snprintf(related, sizeof(related), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related)) {
        return;
    }
    (void)remove(related);
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
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected '%s', got '%s'\n",
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
            "%s: expected '%s' to contain '%s'\n",
            context,
            actual == NULL ? "(null)" : actual,
            needle == NULL ? "(null)" : needle
        );
        return 1;
    }
    return 0;
}
