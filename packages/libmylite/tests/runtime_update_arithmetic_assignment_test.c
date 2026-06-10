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
    create_table_sql_capacity = 1024,
    bounds_column_count = 6,
    mysql_error_bad_null = 1048,
    mysql_error_unknown_column = 1054,
    mysql_error_duplicate = 1062,
    mysql_error_parse = 1064,
    mysql_error_data_out_of_range = 1264,
    mysql_error_bigint_out_of_range = 1690,
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

struct expected_update_result {
    int64_t affected_rows;
    size_t warning_count;
};

static int test_arithmetic_success_persistence_and_limits(void);
static int test_constant_arithmetic_assignments(void);
static int test_arithmetic_errors(void);
static int test_independent_arithmetic_handles(void);
static int open_app_database(const char *path, mylite_db **out_database);
static int create_numbers_table(mylite_db *database, const char *table_name);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_update_ok(mylite_db *database, const char *sql, int64_t affected_rows);
static int expect_update_with_warning_count(
    mylite_db *database,
    const char *sql,
    struct expected_update_result expected
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

    failures += test_arithmetic_success_persistence_and_limits();
    failures += test_constant_arithmetic_assignments();
    failures += test_arithmetic_errors();
    failures += test_independent_arithmetic_handles();

    return failures == 0 ? 0 : 1;
}

static int test_arithmetic_success_persistence_and_limits(void) {
    static const char *const after_increment[] = {
        "1",
        "2",
        "2",
        NULL,
        "3",
        "4",
        "4",
        "1",
    };
    static const char *const after_decrement[] = {"1", "0"};
    static const char *const unsigned_increment[] = {"1", "2"};
    static const char *const order_asc[] = {
        "1",
        "1",
        "2",
        NULL,
        "3",
        "3",
        "4",
        "5",
    };
    static const char *const order_desc[] = {
        "1",
        "1",
        "2",
        NULL,
        "3",
        "-2",
        "4",
        "0",
    };
    static const char *const tie_sum[] = {"62"};
    static const char *const bounds_values[] = {
        "1",
        "2147483647",
        "4294967295",
        "7",
        "9223372036854775807",
        "9223372036854775807",
        "2",
        "-2147483648",
        "8",
        "0",
        "-9223372036854775808",
        "0",
    };
    static const char *const all_null_value[] = {NULL};
    static const char *const row_expression_values[] = {
        "1",
        "5",
        "99",
        NULL,
        "2",
        "4",
        "2",
        "9",
        "3",
        "6",
        "3",
        "9",
        "4",
        "0",
        "4",
        "9",
    };
    static const char *const renamed_value[] = {"3"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "success") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += open_app_database(path, &database);

    failures += create_numbers_table(database, "numbers");
    failures += expect_update_ok(database, "UPDATE numbers SET i = i + 1 ORDER BY id", 3);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, i FROM numbers ORDER BY id",
            .values = after_increment,
            .column_count = 2U,
            .row_count = 4U,
            .context = "full-table arithmetic increment",
        }
    );
    failures += expect_update_ok(database, "UPDATE numbers SET i = i + 0", 0);
    failures += expect_update_ok(database, "UPDATE numbers SET nn = nn - 1 WHERE id = 1", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, nn FROM numbers WHERE id = 1",
            .values = after_decrement,
            .column_count = 2U,
            .row_count = 1U,
            .context = "filtered arithmetic decrement",
        }
    );
    failures += expect_update_ok(database, "UPDATE numbers SET iu = iu + 1 WHERE id = 1", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, iu FROM numbers WHERE id = 1",
            .values = unsigned_increment,
            .column_count = 2U,
            .row_count = 1U,
            .context = "unsigned arithmetic increment",
        }
    );
    failures += expect_update_ok(
        database,
        "UPDATE numbers SET i = i + 999999999999999999999 WHERE id = 999",
        0
    );
    failures +=
        expect_update_ok(database, "UPDATE numbers SET i = i + 999999999999999999999 LIMIT 0", 0);

    failures += create_numbers_table(database, "row_expr");
    failures += expect_update_ok(database, "UPDATE row_expr SET i = nn * nn WHERE id = 2", 1);
    failures +=
        expect_update_ok(database, "UPDATE row_expr SET i = nn + id, n = nn * id WHERE id = 3", 1);
    failures +=
        expect_update_ok(database, "UPDATE row_expr SET i = i + 4, nn = 99 WHERE id = 1", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, i, nn, n FROM row_expr ORDER BY id",
            .values = row_expression_values,
            .column_count = 4U,
            .row_count = 4U,
            .context = "row-scalar arithmetic update assignments",
        }
    );

    failures += create_numbers_table(database, "ordered_asc");
    failures +=
        expect_update_ok(database, "UPDATE ordered_asc SET i = i + 5 ORDER BY tie LIMIT 2", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, i FROM ordered_asc ORDER BY id",
            .values = order_asc,
            .column_count = 2U,
            .row_count = 4U,
            .context = "ordered limited arithmetic asc null placement",
        }
    );

    failures += create_numbers_table(database, "ordered_desc");
    failures += expect_update_ok(
        database,
        "UPDATE ordered_desc SET i = i - 5 ORDER BY tie DESC LIMIT 1",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, i FROM ordered_desc ORDER BY id",
            .values = order_desc,
            .column_count = 2U,
            .row_count = 4U,
            .context = "ordered limited arithmetic desc null placement",
        }
    );

    failures += execute_ok(database, "CREATE TABLE ties (id INT, i INT, tie INT)", &result);
    mylite_result_free(result);
    result = NULL;
    failures +=
        execute_ok(database, "INSERT INTO ties VALUES (1, 10, 1), (2, 20, 1), (3, 30, 1)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_update_ok(database, "UPDATE ties SET i = i + 1 ORDER BY tie LIMIT 2", 2);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT SUM(i) FROM ties",
            .values = tie_sum,
            .column_count = 1U,
            .row_count = 1U,
            .context = "ordered limited tie count",
        }
    );

    failures += execute_ok(
        database,
        "CREATE TABLE bounds (id INT, i INT, iu INT UNSIGNED, integeru INTEGER UNSIGNED, "
        "b BIGINT, bu BIGINT UNSIGNED)",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "INSERT INTO bounds VALUES "
        "(1, 2147483646, 4294967294, 7, 9223372036854775806, 9223372036854775806), "
        "(2, -2147483647, 8, 1, -9223372036854775807, 0)",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_update_ok(database, "UPDATE bounds SET i = i + 1 WHERE id = 1", 1);
    failures += expect_update_ok(database, "UPDATE bounds SET i = i - 1 WHERE id = 2", 1);
    failures += expect_update_ok(database, "UPDATE bounds SET iu = iu + 1 WHERE id = 1", 1);
    failures +=
        expect_update_ok(database, "UPDATE bounds SET integeru = integeru - 1 WHERE id = 2", 1);
    failures += expect_update_ok(database, "UPDATE bounds SET b = b + 1 WHERE id = 1", 1);
    failures += expect_update_ok(database, "UPDATE bounds SET b = b - 1 WHERE id = 2", 1);
    failures += expect_update_ok(database, "UPDATE bounds SET bu = bu + 1 WHERE id = 1", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, i, iu, integeru, b, bu FROM bounds ORDER BY id",
            .values = bounds_values,
            .column_count = bounds_column_count,
            .row_count = 2U,
            .context = "arithmetic assignment integer boundaries",
        }
    );

    failures += execute_ok(database, "CREATE TABLE all_nulls (id INT, n INT NULL)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "INSERT INTO all_nulls VALUES (1, NULL)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_update_ok(
        database,
        "UPDATE all_nulls SET n = n + 999999999999999999999 WHERE id = 1",
        0
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT n FROM all_nulls WHERE id = 1",
            .values = all_null_value,
            .column_count = 1U,
            .row_count = 1U,
            .context = "all-null oversized arithmetic skip",
        }
    );

    failures += execute_ok(database, "ALTER TABLE numbers RENAME TO renamed_numbers", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_update_ok(database, "UPDATE renamed_numbers SET i = i + 1 WHERE id = 1", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT i FROM renamed_numbers WHERE id = 1",
            .values = renamed_value,
            .column_count = 1U,
            .row_count = 1U,
            .context = "arithmetic assignment after rename",
        }
    );

    failures += expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read preamble after arithmetic update"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "arithmetic update preserves preamble"
    );

    mylite_close(database);
    database = NULL;
    failures +=
        expect_int(mylite_open(path, &database), MYLITE_OK, "reopen arithmetic update file");
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT i FROM renamed_numbers WHERE id = 1",
            .values = renamed_value,
            .column_count = 1U,
            .row_count = 1U,
            .context = "persisted arithmetic assignment",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_constant_arithmetic_assignments(void) {
    static const size_t integer_family_column_count = 5U;
    static const char *const core_values[] = {"1", "3", "2", "14", "3", "20"};
    static const char *const boolean_values[] = {"1", "2", "2", "1"};
    static const char *const null_value[] = {NULL};
    static const char *const family_values[] = {"11", "12", "13", "14", "15"};
    static const char *const ordered_values[] = {
        "1",
        "30",
        "2",
        "30",
        "3",
        "20",
    };
    static const char *const key_values[] = {
        "2",
        "20",
        "2",
        "2",
        "3",
        "21",
        "11",
        "1",
        "4",
        "40",
        "12",
        "4",
    };
    static const char *const persisted_value[] = {"30"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "constant") != 0) {
        return 1;
    }
    remove_related_files(path);
    failures += open_app_database(path, &database);

    failures += execute_ok(
        database,
        "CREATE TABLE constants ("
        "id INT PRIMARY KEY, "
        "i INT NULL, "
        "n INT NULL, "
        "nn INT NOT NULL, "
        "u INT UNSIGNED NULL, "
        "integeru INTEGER UNSIGNED NULL, "
        "b BIGINT NULL, "
        "bu BIGINT UNSIGNED NULL)",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "INSERT INTO constants VALUES "
        "(1, 0, 5, 7, 0, 0, 0, 0), "
        "(2, 10, 6, 8, 10, 10, 10, 10), "
        "(3, NULL, NULL, 9, NULL, NULL, NULL, NULL)",
        &result
    );
    mylite_result_free(result);
    result = NULL;

    failures += expect_update_ok(database, "UPDATE constants SET i = 1 + 2 WHERE id = 1", 1);
    failures += expect_update_ok(database, "UPDATE constants SET i = 2 + 3 * 4 WHERE id = 2", 1);
    failures += expect_update_ok(database, "UPDATE constants SET i = (2 + 3) * 4 WHERE id = 3", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, i FROM constants ORDER BY id",
            .values = core_values,
            .column_count = 2U,
            .row_count = 3U,
            .context = "constant arithmetic precedence",
        }
    );

    failures += expect_update_ok(database, "UPDATE constants SET i = TRUE + 1 WHERE id = 1", 1);
    failures += expect_update_ok(database, "UPDATE constants SET i = -1 + +2 WHERE id = 2", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, i FROM constants WHERE id IN (1, 2) ORDER BY id",
            .values = boolean_values,
            .column_count = 2U,
            .row_count = 2U,
            .context = "constant arithmetic boolean and unary operands",
        }
    );

    failures += expect_update_ok(database, "UPDATE constants SET n = NULL + 1 WHERE id = 1", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT n FROM constants WHERE id = 1",
            .values = null_value,
            .column_count = 1U,
            .row_count = 1U,
            .context = "constant arithmetic null result",
        }
    );

    failures += expect_update_ok(database, "UPDATE constants SET i = 10 + 1 WHERE id = 1", 1);
    failures += expect_update_ok(database, "UPDATE constants SET u = 10 + 2 WHERE id = 1", 1);
    failures +=
        expect_update_ok(database, "UPDATE constants SET integeru = 10 + 3 WHERE id = 1", 1);
    failures += expect_update_ok(database, "UPDATE constants SET b = 10 + 4 WHERE id = 1", 1);
    failures += expect_update_ok(database, "UPDATE constants SET bu = 10 + 5 WHERE id = 1", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT i, u, integeru, b, bu FROM constants WHERE id = 1",
            .values = family_values,
            .column_count = integer_family_column_count,
            .row_count = 1U,
            .context = "constant arithmetic integer families",
        }
    );

    failures +=
        expect_update_ok(database, "UPDATE constants SET i = 20 + 10 ORDER BY id LIMIT 2", 2);
    failures += expect_update_ok(database, "UPDATE constants SET i = 30 WHERE id IN (1, 2)", 0);
    failures += expect_update_ok(database, "UPDATE constants SET i = 77 + 1 LIMIT 0", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, i FROM constants ORDER BY id",
            .values = ordered_values,
            .column_count = 2U,
            .row_count = 3U,
            .context = "constant arithmetic order limit and no-op",
        }
    );

    failures += execute_ok(
        database,
        "CREATE TABLE key_constants ("
        "id INT PRIMARY KEY, "
        "u INT UNIQUE, "
        "a INT AUTO_INCREMENT UNIQUE, "
        "v INT)",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "INSERT INTO key_constants(id, u, a, v) VALUES (1, 10, NULL, 1), (2, 20, NULL, 2)",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_update_ok(database, "UPDATE key_constants SET id = 2 + 1 WHERE id = 1", 1);
    failures += expect_update_ok(database, "UPDATE key_constants SET u = 10 + 11 WHERE id = 3", 1);
    failures += expect_update_ok(database, "UPDATE key_constants SET a = 10 + 1 WHERE id = 3", 1);
    failures +=
        execute_ok(database, "INSERT INTO key_constants(id, u, v) VALUES (4, 40, 4)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, u, a, v FROM key_constants ORDER BY id",
            .values = key_values,
            .column_count = 4U,
            .row_count = 3U,
            .context = "constant arithmetic key and auto-increment targets",
        }
    );

    mylite_close(database);
    database = NULL;
    failures +=
        expect_int(mylite_open(path, &database), MYLITE_OK, "reopen constant arithmetic file");
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT i FROM constants WHERE id = 1",
            .values = persisted_value,
            .column_count = 1U,
            .row_count = 1U,
            .context = "persisted constant arithmetic assignment",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_arithmetic_errors(void) {
    static const char *const nonstrict_null_values[] = {"1", "0", "2", "0", "3", "0"};
    static const char *const overflow_original[] = {"1", "2147483647", "0", "9223372036854775807"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "errors") != 0) {
        return 1;
    }
    remove_related_files(path);
    failures += open_app_database(path, &database);

    failures += execute_ok(
        database,
        "CREATE TABLE expressions (id INT, i INT, nn INT, s VARCHAR(20), u INT UNIQUE)",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "INSERT INTO expressions VALUES (1, 1, 2, 'x', 5)", &result);
    mylite_result_free(result);
    result = NULL;

    failures +=
        execute_ok(database, "CREATE TABLE not_null_expr (id INT, nn INT NOT NULL)", &result);
    mylite_result_free(result);
    result = NULL;
    failures +=
        execute_ok(database, "INSERT INTO not_null_expr VALUES (1, 7), (2, 8), (3, 0)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "SET sql_mode = ''", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_update_with_warning_count(
        database,
        "UPDATE not_null_expr SET nn = NULL + 1",
        (struct expected_update_result){.affected_rows = 2, .warning_count = 3U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, nn FROM not_null_expr ORDER BY id",
            .values = nonstrict_null_values,
            .column_count = 2U,
            .row_count = 3U,
            .context = "constant arithmetic non-strict null adjustment warnings",
        }
    );
    failures += execute_ok(database, "SET sql_mode = 'STRICT_TRANS_TABLES'", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_error(
        database,
        "UPDATE not_null_expr SET nn = NULL + 1 WHERE id = 1",
        (struct expected_sql_error){
            .code = mysql_error_bad_null,
            .sqlstate = "23000",
            .message_part = "Column 'nn' cannot be null",
        }
    );

    failures += execute_error(
        database,
        "UPDATE expressions SET i = missing + 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += expect_update_ok(database, "UPDATE expressions SET i = nn + 1", 1);
    failures += execute_error(
        database,
        "UPDATE expressions SET s = s + 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "UPDATE arithmetic assignment supports only integer columns",
        }
    );
    failures += execute_error(
        database,
        "UPDATE expressions SET u = u + 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "UPDATE arithmetic assignment supports only same-column",
        }
    );
    failures += execute_error(
        database,
        "UPDATE expressions SET i = i + -1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "UPDATE expressions SET i = 1 / 2",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "UPDATE expressions SET i = 1 DIV 2",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "UPDATE expressions SET i = 1 % 2",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "UPDATE expressions SET i = 1 + other",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "UPDATE expressions SET s = 1 + 2",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "UPDATE constant arithmetic assignment supports only integer columns",
        }
    );
    failures += execute_error(
        database,
        "UPDATE expressions SET s = NULL + 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "UPDATE constant arithmetic assignment supports only integer columns",
        }
    );
    failures += execute_error(
        database,
        "UPDATE expressions SET i = 1 + 2, nn = 3 + 4",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "UPDATE multiple assignments support only",
        }
    );
    failures += execute_error(
        database,
        "UPDATE expressions SET i = i + 1.5",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "UPDATE expressions SET i = expressions.i + 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );

    failures +=
        execute_ok(database, "CREATE TABLE pk_numbers (id INT PRIMARY KEY, i INT)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "INSERT INTO pk_numbers VALUES (1, 10)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_error(
        database,
        "UPDATE pk_numbers SET id = id + 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "UPDATE arithmetic assignment supports only same-column",
        }
    );

    failures += execute_ok(
        database,
        "CREATE TABLE key_conflicts (id INT PRIMARY KEY, u INT UNIQUE)",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "INSERT INTO key_conflicts VALUES (1, 10), (2, 20)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_error(
        database,
        "UPDATE key_conflicts SET id = 1 + 1 WHERE id = 1",
        (struct expected_sql_error){
            .code = mysql_error_duplicate,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '2'",
        }
    );
    failures += execute_error(
        database,
        "UPDATE key_conflicts SET u = 10 + 10 WHERE id = 1",
        (struct expected_sql_error){
            .code = mysql_error_duplicate,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '20'",
        }
    );

    failures += execute_ok(
        database,
        "CREATE TABLE overflows (id INT, i INT, iu INT UNSIGNED, b BIGINT, bu BIGINT UNSIGNED)",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "INSERT INTO overflows VALUES (1, 2147483647, 0, 9223372036854775807, "
        "9223372036854775807)",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_error(
        database,
        "UPDATE overflows SET i = i + 1 WHERE id = 1",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'i' at row 1",
        }
    );
    failures += execute_error(
        database,
        "UPDATE overflows SET iu = iu - 1 WHERE id = 1",
        (struct expected_sql_error){
            .code = mysql_error_bigint_out_of_range,
            .sqlstate = "22003",
            .message_part = "BIGINT UNSIGNED value is out of range",
        }
    );
    failures += execute_error(
        database,
        "UPDATE overflows SET b = b + 1 WHERE id = 1",
        (struct expected_sql_error){
            .code = mysql_error_bigint_out_of_range,
            .sqlstate = "22003",
            .message_part = "BIGINT value is out of range",
        }
    );
    failures += execute_error(
        database,
        "UPDATE overflows SET bu = bu + 1 WHERE id = 1",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'bu' at row 1",
        }
    );
    failures += execute_error(
        database,
        "UPDATE overflows SET i = 2147483647 + 1 WHERE id = 1",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'i' at row 1",
        }
    );
    failures += execute_error(
        database,
        "UPDATE overflows SET iu = -1 + 0 WHERE id = 1",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'iu' at row 1",
        }
    );
    failures += execute_error(
        database,
        "UPDATE overflows SET bu = 9223372036854775807 + 1 WHERE id = 1",
        (struct expected_sql_error){
            .code = mysql_error_bigint_out_of_range,
            .sqlstate = "22003",
            .message_part = "BIGINT value is out of range",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, i, iu, b FROM overflows",
            .values = overflow_original,
            .column_count = 4U,
            .row_count = 1U,
            .context = "overflow errors preserve row values",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_independent_arithmetic_handles(void) {
    static const char *const first_expected[] = {"6"};
    static const char *const second_expected[] = {"1"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "independent_first") != 0 ||
        make_test_path(second_path, sizeof(second_path), "independent_second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += open_app_database(first_path, &first);
    failures += open_app_database(second_path, &second);
    failures += create_numbers_table(first, "numbers");
    failures += create_numbers_table(second, "numbers");
    failures += expect_update_ok(first, "UPDATE numbers SET i = i + 5 WHERE id = 1", 1);
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT i FROM numbers WHERE id = 1",
            .values = first_expected,
            .column_count = 1U,
            .row_count = 1U,
            .context = "first independent arithmetic update",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT i FROM numbers WHERE id = 1",
            .values = second_expected,
            .column_count = 1U,
            .row_count = 1U,
            .context = "second independent arithmetic unchanged",
        }
    );

    mylite_result_free(result);
    mylite_close(second);
    mylite_close(first);
    remove_related_files(second_path);
    remove_related_files(first_path);

    return failures;
}

static int open_app_database(const char *path, mylite_db **out_database) {
    mylite_result *result = NULL;
    int failures = expect_int(mylite_open(path, out_database), MYLITE_OK, "open app database");

    failures += execute_ok(*out_database, "CREATE DATABASE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(*out_database, "USE app", &result);
    mylite_result_free(result);

    return failures;
}

static int create_numbers_table(mylite_db *database, const char *table_name) {
    char sql[create_table_sql_capacity];
    mylite_result *result = NULL;
    int written = snprintf(
        sql,
        sizeof(sql),
        "CREATE TABLE %s ("
        "id INT NOT NULL, "
        "i INT, "
        "nn INT NOT NULL, "
        "iu INT UNSIGNED, "
        "integeru INTEGER UNSIGNED, "
        "b BIGINT, "
        "bu BIGINT UNSIGNED, "
        "n INT NULL, "
        "tie INT NULL)",
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
        "(1, 1, 1, 1, 7, 1, 1, NULL, 2), "
        "(2, NULL, 2, 0, 8, 6, 6, 9, NULL), "
        "(3, 3, 3, 4, 9, -7, 7, NULL, 3), "
        "(4, 0, 4, 8, 10, 8, 8, 9, 1)",
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
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

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
    failures += expect_true(result == NULL, "failed execute leaves result null");
    failures += expect_int(mylite_errcode(database), expected.code, "error code");
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, "SQLSTATE");
    failures += expect_contains(mylite_errmsg(database), expected.message_part, "error message");
    mylite_result_free(result);

    return failures;
}

static int expect_update_ok(mylite_db *database, const char *sql, int64_t affected_rows) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, "update column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "update row count");
    failures += expect_int64(mylite_result_affected_rows(result), affected_rows, "update affected");
    failures += expect_size(mylite_result_warning_count(result), 0U, "update warning count");
    mylite_result_free(result);

    return failures;
}

static int expect_update_with_warning_count(
    mylite_db *database,
    const char *sql,
    struct expected_update_result expected
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, "update column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "update row count");
    failures += expect_int64(
        mylite_result_affected_rows(result),
        expected.affected_rows,
        "update affected"
    );
    failures += expect_size(
        mylite_result_warning_count(result),
        expected.warning_count,
        "update warning count"
    );
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
        "%s/mylite_update_arithmetic_assignment_%d_%s.mylite",
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
    remove_with_suffix(path, "-journal");
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

    if (file == NULL) {
        fprintf(stderr, "failed to open %s for reading\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "failed to seek %s\n", path);
        (void)fclose(file);
        return 1;
    }
    if (fread(buffer, 1U, size, file) != size) {
        fprintf(stderr, "failed to read %zu bytes from %s\n", size, path);
        (void)fclose(file);
        return 1;
    }
    if (fclose(file) != 0) {
        fprintf(stderr, "failed to close %s\n", path);
        return 1;
    }

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
            "%s: expected text '%s', got '%s'\n",
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

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: byte comparison failed\n", context);
        return 1;
    }

    return 0;
}
