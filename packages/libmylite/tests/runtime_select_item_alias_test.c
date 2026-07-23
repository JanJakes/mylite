#include <mylite/mylite.h>

#include "runtime_test_support.h"

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
    seed_sql_capacity = 512,
    long_alias_sql_capacity = 1024,
    path_suffix_capacity = 16,
    mysql_error_parse = 1064,
    mysql_error_column_ambiguous = 1052,
    mysql_error_unknown_column = 1054,
    mysql_error_identifier_too_long = 1059,
    mysql_error_table_does_not_exist = 1146,
    alias_64_length = 64,
    alias_65_length = 65,
    alias_256_length = 256,
    alias_257_length = 257,
    alias_text_capacity = alias_257_length + 1,
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

struct seed_numbers_request {
    const char *schema;
    const char *rows;
};

struct formatted_alias_query {
    const char *sql_format;
    const char *alias;
    const char *value;
    const char *context;
};

struct formatted_alias_error {
    const char *sql_format;
    const char *alias;
    struct expected_sql_error expected;
};

static int test_select_alias_values_reopen_rename_and_drop(void);
static int test_select_alias_length_boundaries(void);
static int test_select_alias_diagnostics(void);
static int test_independent_alias_handles(void);
static int seed_numbers(mylite_db *database, struct seed_numbers_request request);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_formatted_alias_query(mylite_db *database, struct formatted_alias_query query);
static int expect_formatted_alias_error(mylite_db *database, struct formatted_alias_error expected);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int make_alias_text(char *destination, size_t destination_size, size_t length);
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

    failures += test_select_alias_values_reopen_rename_and_drop();
    failures += test_select_alias_length_boundaries();
    failures += test_select_alias_diagnostics();
    failures += test_independent_alias_handles();

    return failures == 0 ? 0 : 1;
}

static int test_select_alias_values_reopen_rename_and_drop(void) {
    static const char *const columns_xy[] = {"x", "y"};
    static const char *const columns_customer[] = {"Customer identity"};
    static const char *const column_x[] = {"x"};
    static const char *const column_x_upper[] = {"X"};
    static const char *const column_id[] = {"id"};
    static const char *const column_selected[] = {"selected"};
    static const char *const column_c[] = {"c"};
    static const char *const column_cn[] = {"cn"};
    static const char *const column_cd[] = {"cd"};
    static const char *const column_mn[] = {"mn"};
    static const char *const column_mx[] = {"mx"};
    static const char *const columns_scalar[] = {"d", "u", "cu", "wc"};
    static const char *const column_persisted[] = {"persisted"};
    static const char *const column_after_rename[] = {"after_rename"};
    static const char *const values_xy[] = {"10", "5", NULL, "6", "20", "7"};
    static const char *const value_10[] = {"10"};
    static const char *const value_20[] = {"20"};
    static const char *const values_distinct[] = {NULL, "10", "20"};
    static const char *const values_desc[] = {"20", "10", NULL};
    static const char *const values_alias_shadow[] = {NULL, "10", "20"};
    static const char *const value_null[] = {NULL};
    static const char *const value_3[] = {"3"};
    static const char *const value_2[] = {"2"};
    static const char *const values_scalar[] = {"app", "root@%", "root@%", "0"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "values") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open values file");
    failures += seed_numbers(
        database,
        (struct seed_numbers_request){
            .schema = "app",
            .rows = "(1, 10, 5), (2, NULL, 6), (3, 20, 7)",
        }
    );

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT n AS x, nn y FROM numbers ORDER BY id",
            .columns = columns_xy,
            .column_count = 2U,
            .values = values_xy,
            .row_count = 3U,
            .context = "identifier and bare aliases",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT n AS `Customer identity` FROM numbers ORDER BY id LIMIT 1",
            .columns = columns_customer,
            .column_count = 1U,
            .values = value_10,
            .row_count = 1U,
            .context = "quoted identifier alias",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT n AS 'Customer identity' FROM numbers "
                   "ORDER BY `Customer identity` DESC LIMIT 1",
            .columns = columns_customer,
            .column_count = 1U,
            .values = value_20,
            .row_count = 1U,
            .context = "string literal alias ordered by identifier quoting",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT n 'Customer identity' FROM numbers ORDER BY id LIMIT 1",
            .columns = columns_customer,
            .column_count = 1U,
            .values = value_10,
            .row_count = 1U,
            .context = "bare string literal alias",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DISTINCT n AS x FROM numbers ORDER BY x",
            .columns = column_x,
            .column_count = 1U,
            .values = values_distinct,
            .row_count = 3U,
            .context = "distinct alias",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DISTINCTROW n x FROM numbers ORDER BY x",
            .columns = column_x,
            .column_count = 1U,
            .values = values_distinct,
            .row_count = 3U,
            .context = "distinctrow alias",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT n AS X FROM numbers ORDER BY x DESC",
            .columns = column_x_upper,
            .column_count = 1U,
            .values = values_desc,
            .row_count = 3U,
            .context = "case-insensitive order alias",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT n AS id FROM numbers ORDER BY id",
            .columns = column_id,
            .column_count = 1U,
            .values = values_alias_shadow,
            .row_count = 3U,
            .context = "order alias shadows descriptor column",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT n AS id FROM numbers WHERE id = 2",
            .columns = column_id,
            .column_count = 1U,
            .values = value_null,
            .row_count = 1U,
            .context = "where ignores alias",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT nums.n AS selected FROM numbers AS nums "
                   "WHERE nums.n IS NOT NULL ORDER BY selected DESC LIMIT 1",
            .columns = column_selected,
            .column_count = 1U,
            .values = value_20,
            .row_count = 1U,
            .context = "source-qualified projection with alias order",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) AS c FROM numbers",
            .columns = column_c,
            .column_count = 1U,
            .values = value_3,
            .row_count = 1U,
            .context = "count star alias",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(n) cn FROM numbers",
            .columns = column_cn,
            .column_count = 1U,
            .values = value_2,
            .row_count = 1U,
            .context = "count column alias",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(DISTINCT n) AS cd FROM numbers",
            .columns = column_cd,
            .column_count = 1U,
            .values = value_2,
            .row_count = 1U,
            .context = "count distinct alias",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT MIN(n) AS mn FROM numbers",
            .columns = column_mn,
            .column_count = 1U,
            .values = value_10,
            .row_count = 1U,
            .context = "min alias",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT MAX(n) mx FROM numbers",
            .columns = column_mx,
            .column_count = 1U,
            .values = value_20,
            .row_count = 1U,
            .context = "max alias",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DATABASE() AS d, USER() u, CURRENT_USER AS cu, @@warning_count AS wc",
            .columns = columns_scalar,
            .column_count = 4U,
            .values = values_scalar,
            .row_count = 1U,
            .context = "scalar aliases",
        }
    );

    mylite_close(database);
    database = NULL;
    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "reopen values file");
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT n AS persisted FROM numbers ORDER BY id LIMIT 1",
            .columns = column_persisted,
            .column_count = 1U,
            .values = value_10,
            .row_count = 1U,
            .context = "alias after reopen",
        }
    );
    failures += execute_ok(database, "RENAME TABLE numbers TO renamed_numbers", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT n AS after_rename FROM renamed_numbers ORDER BY id LIMIT 1",
            .columns = column_after_rename,
            .column_count = 1U,
            .values = value_10,
            .row_count = 1U,
            .context = "alias after rename",
        }
    );
    failures += execute_ok(database, "DROP TABLE renamed_numbers", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_error(
        database,
        "SELECT n AS missing_after_drop FROM renamed_numbers",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.renamed_numbers' doesn't exist",
        }
    );

    mylite_close(database);
    database = NULL;
    failures += mylite_test_expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read file preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble unchanged"
    );
    remove_related_files(path);

    if (database != NULL) {
        mylite_close(database);
    }

    return failures;
}

static int test_select_alias_length_boundaries(void) {
    static const char *const value_20[] = {"20"};
    char alias64[alias_text_capacity];
    char alias65[alias_text_capacity];
    char alias256[alias_text_capacity];
    char alias257[alias_text_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += mylite_test_expect_int(
        make_alias_text(alias64, sizeof(alias64), alias_64_length),
        0,
        "alias64"
    );
    failures += mylite_test_expect_int(
        make_alias_text(alias65, sizeof(alias65), alias_65_length),
        0,
        "alias65"
    );
    failures += mylite_test_expect_int(
        make_alias_text(alias256, sizeof(alias256), alias_256_length),
        0,
        "alias256"
    );
    failures += mylite_test_expect_int(
        make_alias_text(alias257, sizeof(alias257), alias_257_length),
        0,
        "alias257"
    );
    if (failures != 0) {
        return failures;
    }

    failures +=
        mylite_test_expect_int(mylite_test_open_temporary(&database), MYLITE_OK, "open length db");
    failures += seed_numbers(
        database,
        (struct seed_numbers_request){
            .schema = "app",
            .rows = "(1, 10, 5), (2, NULL, 6), (3, 20, 7)",
        }
    );

    failures += expect_formatted_alias_query(
        database,
        (struct formatted_alias_query){
            .sql_format = "SELECT n AS %s FROM numbers ORDER BY %s DESC LIMIT 1",
            .alias = alias64,
            .value = value_20[0],
            .context = "64-character identifier alias",
        }
    );
    failures += expect_formatted_alias_query(
        database,
        (struct formatted_alias_query){
            .sql_format = "SELECT n AS %s FROM numbers ORDER BY %s DESC LIMIT 1",
            .alias = alias65,
            .value = value_20[0],
            .context = "65-character identifier alias",
        }
    );
    failures += expect_formatted_alias_query(
        database,
        (struct formatted_alias_query){
            .sql_format = "SELECT n AS `%s` FROM numbers ORDER BY `%s` DESC LIMIT 1",
            .alias = alias256,
            .value = value_20[0],
            .context = "256-character quoted identifier alias",
        }
    );
    failures += expect_formatted_alias_query(
        database,
        (struct formatted_alias_query){
            .sql_format = "SELECT n AS '%s' FROM numbers ORDER BY `%s` DESC LIMIT 1",
            .alias = alias256,
            .value = value_20[0],
            .context = "256-character string alias",
        }
    );
    failures += execute_ok(database, "SELECT ROW_COUNT()", &result);
    mylite_result_free(result);
    result = NULL;

    failures += expect_formatted_alias_error(
        database,
        (struct formatted_alias_error){
            .sql_format = "SELECT n AS %s FROM numbers",
            .alias = alias257,
            .expected =
                (struct expected_sql_error){
                    .code = mysql_error_identifier_too_long,
                    .sqlstate = "42000",
                    .message_part = "alias identifier is too long",
                },
        }
    );
    failures += expect_formatted_alias_error(
        database,
        (struct formatted_alias_error){
            .sql_format = "SELECT n AS `%s` FROM numbers",
            .alias = alias257,
            .expected =
                (struct expected_sql_error){
                    .code = mysql_error_identifier_too_long,
                    .sqlstate = "42000",
                    .message_part = "alias identifier is too long",
                },
        }
    );
    failures += expect_formatted_alias_error(
        database,
        (struct formatted_alias_error){
            .sql_format = "SELECT n AS '%s' FROM numbers",
            .alias = alias257,
            .expected =
                (struct expected_sql_error){
                    .code = mysql_error_identifier_too_long,
                    .sqlstate = "42000",
                    .message_part = "alias identifier is too long",
                },
        }
    );

    mylite_close(database);

    return failures;
}

static int test_select_alias_diagnostics(void) {
    static const char *const column_x[] = {"x"};
    static const char *const string_order_values[] = {"10", NULL, "20"};
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += mylite_test_expect_int(
        mylite_test_open_temporary(&database),
        MYLITE_OK,
        "open diagnostics db"
    );
    failures += seed_numbers(
        database,
        (struct seed_numbers_request){
            .schema = "app",
            .rows = "(1, 10, 5), (2, NULL, 6), (3, 20, 7)",
        }
    );

    failures += execute_error(
        database,
        "SELECT n AS x FROM numbers WHERE x = 10",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'x' in 'where clause'",
        }
    );
    failures += execute_error(
        database,
        "SELECT id AS x, n AS x FROM numbers ORDER BY x",
        (struct expected_sql_error){
            .code = mysql_error_column_ambiguous,
            .sqlstate = "23000",
            .message_part = "Column 'x' in order clause is ambiguous",
        }
    );
    failures += execute_error(
        database,
        "SELECT n AS x FROM numbers ORDER BY numbers.x",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'numbers.x' in 'order clause'",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT n AS 'x' FROM numbers ORDER BY 'x'",
            .columns = column_x,
            .column_count = 1U,
            .values = string_order_values,
            .row_count = 3U,
            .context = "string order key is constant not alias",
        }
    );
    failures += execute_error(
        database,
        "SELECT * AS x FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );

    failures += execute_ok(database, "DROP TABLE numbers", &result);
    mylite_result_free(result);
    result = NULL;
    mylite_close(database);

    return failures;
}

static int test_independent_alias_handles(void) {
    static const char *const column_x[] = {"x"};
    static const char *const first_values[] = {"10"};
    static const char *const second_values[] = {"30"};
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    failures +=
        mylite_test_expect_int(mylite_test_open_temporary(&first), MYLITE_OK, "open first handle");
    failures += mylite_test_expect_int(
        mylite_test_open_temporary(&second),
        MYLITE_OK,
        "open second handle"
    );
    failures += seed_numbers(
        first,
        (struct seed_numbers_request){
            .schema = "app",
            .rows = "(1, 10, 5)",
        }
    );
    failures += seed_numbers(
        second,
        (struct seed_numbers_request){
            .schema = "app",
            .rows = "(1, 30, 5)",
        }
    );
    failures += expect_query(
        first,
        (struct expected_query){
            .sql = "SELECT n AS x FROM numbers ORDER BY x",
            .columns = column_x,
            .column_count = 1U,
            .values = first_values,
            .row_count = 1U,
            .context = "first handle alias",
        }
    );
    failures += expect_query(
        second,
        (struct expected_query){
            .sql = "SELECT n AS x FROM numbers ORDER BY x",
            .columns = column_x,
            .column_count = 1U,
            .values = second_values,
            .row_count = 1U,
            .context = "second handle alias",
        }
    );

    mylite_close(first);
    mylite_close(second);

    return failures;
}

static int seed_numbers(mylite_db *database, struct seed_numbers_request request) {
    char sql[seed_sql_capacity];
    mylite_result *result = NULL;
    int failures = 0;
    int written = snprintf(sql, sizeof(sql), "CREATE DATABASE %s", request.schema);

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "seed sql buffer too small\n");
        return 1;
    }

    failures += execute_ok(database, sql, &result);
    mylite_result_free(result);
    result = NULL;

    written = snprintf(sql, sizeof(sql), "USE %s", request.schema);
    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "seed sql buffer too small\n");
        return 1;
    }
    failures += execute_ok(database, sql, &result);
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(
        database,
        "CREATE TABLE numbers (id INT NOT NULL, n INT NULL, nn INT NOT NULL)",
        &result
    );
    mylite_result_free(result);
    result = NULL;

    written = snprintf(sql, sizeof(sql), "INSERT INTO numbers VALUES %s", request.rows);
    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "seed sql buffer too small\n");
        return 1;
    }
    failures += execute_ok(database, sql, &result);
    mylite_result_free(result);

    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "execute '%s': rc=%d err=%d state=%s message=%s\n",
            sql,
            rc,
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

    if (rc != MYLITE_ERROR) {
        fprintf(stderr, "execute '%s': expected MYLITE_ERROR, got %d\n", sql, rc);
        failures += 1;
    }
    failures += mylite_test_expect_true(result == NULL, "failed execute leaves result null");
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

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, expected.sql, &result);

    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        expected.column_count,
        expected.context
    );
    for (size_t column = 0U; column < expected.column_count; ++column) {
        failures += mylite_test_expect_text(
            mylite_result_column_name(result, column),
            expected.columns[column],
            expected.context
        );
    }
    failures += mylite_test_expect_size(
        mylite_result_row_count(result),
        expected.row_count,
        expected.context
    );
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
    failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, expected.context);
    mylite_result_free(result);

    return failures;
}

static int expect_formatted_alias_query(mylite_db *database, struct formatted_alias_query query) {
    char sql[long_alias_sql_capacity];
    const char *const columns[] = {query.alias};
    const char *const values[] = {query.value};
    int written = snprintf(sql, sizeof(sql), query.sql_format, query.alias, query.alias);

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "%s: formatted alias SQL is too long\n", query.context);
        return 1;
    }

    return expect_query(
        database,
        (struct expected_query){
            .sql = sql,
            .columns = columns,
            .column_count = 1U,
            .values = values,
            .row_count = 1U,
            .context = query.context,
        }
    );
}

static int expect_formatted_alias_error(
    mylite_db *database,
    struct formatted_alias_error expected
) {
    char sql[long_alias_sql_capacity];
    int written = snprintf(sql, sizeof(sql), expected.sql_format, expected.alias);

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "formatted alias error SQL is too long\n");
        return 1;
    }

    return execute_error(database, sql, expected.expected);
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
            fprintf(stderr, "%s: expected NULL, got %s\n", context, actual);
            return 1;
        }
        return 0;
    }

    return mylite_test_expect_text(actual, expected, context);
}

static int make_alias_text(char *destination, size_t destination_size, size_t length) {
    if (destination == NULL || length >= destination_size) {
        return -1;
    }

    memset(destination, 'a', length);
    destination[length] = '\0';
    return 0;
}

static void remove_related_files(const char *path) {
    remove(path);
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char buffer[test_path_capacity + path_suffix_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(buffer)) {
        remove(buffer);
    }
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    int status = 0;

    if (file == NULL) {
        return -1;
    }
    if (fseek(file, offset, SEEK_SET) != 0 || fread(buffer, 1U, size, file) != size) {
        status = -1;
    }
    if (fclose(file) != 0) {
        status = -1;
    }
    return status;
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
