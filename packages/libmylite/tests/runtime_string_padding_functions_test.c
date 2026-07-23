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
    mysql_error_parse = 1064,
    mysql_error_native_function_argument_count = 1582,
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

static int test_no_source_dual_and_do_padding(void);
static int test_table_backed_padding_and_reopen(void);
static int test_independent_file_backed_padding_handles(void);
static int test_padding_diagnostics(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query expected);
static int open_app_database(
    mylite_db **out_database,
    const char *name,
    char *path,
    size_t path_size
);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_no_source_dual_and_do_padding();
    failures += test_table_backed_padding_and_reopen();
    failures += test_independent_file_backed_padding_handles();
    failures += test_padding_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_no_source_dual_and_do_padding(void) {
    static const char *const columns_no_source[] = {
        "lp",     "rp",       "lt",        "rt",         "rep",         "sp",        "l0",
        "r0",     "rep0",     "repn",      "sp0",        "spn",         "ln",        "rn",
        "nlv",    "nll",      "nlp",       "nr",         "nrep",        "nsp",       "emptyp",
        "emptyr", "fitl",     "fitr",      "multi_l",    "multi_r",     "multi_rep", "num_l",
        "num_r",  "true_rep", "false_rep", "true_space", "false_space", "true_l",    "false_r",
    };
    static const char *const values_no_source[] = {
        "??hi",
        "hi???",
        "h",
        "h",
        "MySQLMySQLMySQL",
        "   ",
        "",
        "",
        "",
        "",
        "",
        "",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "",
        "",
        "hi",
        "hi",
        "\xF0\x9F\x99\x82\xF0\x9F\x99\x82\xC3\xA9",
        "\xC3\xA9\xF0\x9F\x99\x82\xF0\x9F\x99\x82",
        "\xC3\xA9\xF0\x9F\x99\x82\xC3\xA9\xF0\x9F\x99\x82",
        "00123",
        "-7xx",
        "111",
        "00",
        " ",
        "",
        "x",
        "",
    };
    static const char *const columns_dual[] = {"a", "b", "c", "d"};
    static const char *const values_dual[] = {"??hi", "hi??", "xx", "  "};
    static const char *const columns_scalar_integer_arguments[] = {
        "lp_abs",
        "rp_len",
        "rep_abs",
        "sp_len",
    };
    static const char *const values_scalar_integer_arguments[] = {
        "00hi",
        "hix",
        "abab",
        "  ",
    };
    static const char *const columns_status[] = {"ROW_COUNT()", "@@warning_count"};
    static const char *const values_after_select[] = {"-1", "0"};
    static const char *const values_after_do[] = {"0", "0"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += open_app_database(&database, "no-source", path, sizeof(path));
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT LPAD('hi',4,'?\?') AS lp, RPAD('hi',5,'?') AS rp, "
                   "LPAD('hi',1,'?\?') AS lt, RPAD('hi',1,'?') AS rt, "
                   "REPEAT('MySQL',3) AS rep, SPACE(3) AS sp, LPAD('hi',0,'?') AS l0, "
                   "RPAD('hi',0,'?') AS r0, REPEAT('x',0) AS rep0, REPEAT('x',-1) AS repn, "
                   "SPACE(0) AS sp0, SPACE(-1) AS spn, LPAD('hi',-1,'?') AS ln, "
                   "RPAD('hi',-1,'?') AS rn, LPAD(NULL,4,'?') AS nlv, "
                   "LPAD('hi',NULL,'?') AS nll, LPAD('hi',4,NULL) AS nlp, "
                   "RPAD(NULL,4,'?') AS nr, REPEAT(NULL,2) AS nrep, SPACE(NULL) AS nsp, "
                   "LPAD('hi',4,'') AS emptyp, RPAD('hi',4,'') AS emptyr, "
                   "LPAD('hi',2,'') AS fitl, RPAD('hi',2,'') AS fitr, "
                   "LPAD('\xC3\xA9',3,'\xF0\x9F\x99\x82') AS multi_l, "
                   "RPAD('\xC3\xA9',3,'\xF0\x9F\x99\x82') AS multi_r, "
                   "REPEAT('\xC3\xA9\xF0\x9F\x99\x82',2) AS multi_rep, "
                   "LPAD(123,5,'0') AS num_l, RPAD(-7,4,'x') AS num_r, "
                   "REPEAT(TRUE,3) AS true_rep, REPEAT(FALSE,2) AS false_rep, "
                   "SPACE(TRUE) AS true_space, SPACE(FALSE) AS false_space, "
                   "LPAD('x', TRUE, '?') AS true_l, RPAD('x', FALSE, '?') AS false_r",
            .columns = columns_no_source,
            .column_count = sizeof(columns_no_source) / sizeof(columns_no_source[0]),
            .values = values_no_source,
            .row_count = 1U,
            .context = "no-source padding values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT LPAD ('hi',4,'?') AS a, RPAD ('hi',4,'?') AS b, "
                   "REPEAT ('x',2) AS c, SPACE (2) AS d FROM DUAL",
            .columns = columns_dual,
            .column_count = sizeof(columns_dual) / sizeof(columns_dual[0]),
            .values = values_dual,
            .row_count = 1U,
            .context = "dual padding whitespace",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT LPAD('hi', ABS(-4), '0') AS lp_abs, "
                   "RPAD('hi', LENGTH('abc'), 'x') AS rp_len, "
                   "REPEAT('ab', ABS(-2)) AS rep_abs, "
                   "SPACE(LENGTH('xy')) AS sp_len",
            .columns = columns_scalar_integer_arguments,
            .column_count = sizeof(columns_scalar_integer_arguments) /
                            sizeof(columns_scalar_integer_arguments[0]),
            .values = values_scalar_integer_arguments,
            .row_count = 1U,
            .context = "no-source padding integer function arguments",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .columns = columns_status,
            .column_count = sizeof(columns_status) / sizeof(columns_status[0]),
            .values = values_after_select,
            .row_count = 1U,
            .context = "row count after padding select",
        }
    );

    failures += execute_ok(
        database,
        "DO LPAD('abc', 5, '0'), RPAD(NULL, 5, '0'), REPEAT('x', 2), SPACE(2)",
        &result
    );
    if (failures == 0) {
        failures +=
            mylite_test_expect_size(mylite_result_column_count(result), 0U, "padding do columns");
        failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, "padding do rows");
        failures +=
            mylite_test_expect_int64(mylite_result_affected_rows(result), 0, "padding do affected");
        failures +=
            mylite_test_expect_size(mylite_result_warning_count(result), 0U, "padding do warnings");
    }
    mylite_result_free(result);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .columns = columns_status,
            .column_count = sizeof(columns_status) / sizeof(columns_status[0]),
            .values = values_after_do,
            .row_count = 1U,
            .context = "row count after padding do",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_padding_and_reopen(void) {
    static const char *const columns_table[] = {
        "id",
        "lpv",
        "rpv",
        "repv",
        "sp",
        "lpc",
        "rpc",
        "reptxt",
        "lpi",
        "rpd",
        "lpy",
        "rpdt",
    };
    static const char *const values_table[] = {
        "1",
        "00abc",
        "abc00",
        "abcabc",
        "  ",
        "000a",
        "a000",
        "hihi",
        "00123",
        "12.30x",
        "2024",
        "2024-01-02 ",
        "2",
        "000\xC3\xA9\xF0\x9F\x99\x82",
        "\xC3\xA9\xF0\x9F\x99\x82\060\060\060",
        "\xC3\xA9\xF0\x9F\x99\x82\xC3\xA9\xF0\x9F\x99\x82",
        "  ",
        "000\xC3\xA9",
        "\xC3\xA9\060\060\060",
        "",
        "000-7",
        "-4.50x",
        "1970",
        NULL,
        "3",
        NULL,
        NULL,
        NULL,
        "  ",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
    };
    static const char *const columns_limited[] = {"id", "padded"};
    static const char *const values_limited[] = {
        "3",
        NULL,
        "2",
        "0\xC3\xA9\xF0\x9F\x99\x82",
    };
    static const char *const columns_integer_expression_padding[] = {
        "id",
        "lp",
        "rp",
        "rep",
        "sp",
    };
    static const char *const values_integer_expression_padding[] = {
        "1",
        "00abc",
        "abc",
        "abc",
        " ",
        "2",
        "0000\xC3\xA9\xF0\x9F\x99\x82",
        "\xC3\xA9\xF0\x9F\x99\x82xx",
        "\xC3\xA9\xF0\x9F\x99\x82\xC3\xA9\xF0\x9F\x99\x82",
        "  ",
        "3",
        NULL,
        NULL,
        NULL,
        "   ",
    };
    static const char *const columns_count[] = {"COUNT(*)"};
    static const char *const values_count_one[] = {"1"};
    static const char *const columns_id[] = {"id"};
    static const char *const values_repeat_null_rows[] = {"3"};
    static const char *const values_space_rows[] = {"2"};
    static const char *const columns_padding_order[] = {"id", "padded"};
    static const char *const values_padding_order[] = {
        "3",
        NULL,
        "2",
        "000\xC3\xA9\xF0\x9F\x99\x82",
        "1",
        "00abc",
    };
    static const char *const columns_reopen[] = {"id", "padded"};
    static const char *const values_reopen[] = {
        "1",
        "00abc",
        "2",
        "000\xC3\xA9\xF0\x9F\x99\x82",
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    mylite_file_preamble_init(expected_preamble);
    failures += open_app_database(&database, "table", path, sizeof(path));
    failures += execute_ok(
        database,
        "CREATE TABLE t("
        "id INT, v VARCHAR(20), c CHAR(5), txt TEXT, i INT, d DECIMAL(6,2), y YEAR, "
        "dt DATETIME, b VARBINARY(8), bitcol BIT(4), f DOUBLE"
        ")",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES "
        "(1, 'abc', 'a  ', 'hi', 123, 12.30, 2024, '2024-01-02 13:29:17', X'616263', b'1010', "
        "1.25), "
        "(2, '\xC3\xA9\xF0\x9F\x99\x82', '\xC3\xA9', '', -7, -4.50, 70, NULL, X'00', b'1', -2.5), "
        "(3, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL)",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, LPAD(v, 5, '0') AS lpv, RPAD(v, 5, '0') AS rpv, "
                   "REPEAT(v, 2) AS repv, SPACE(2) AS sp, LPAD(c, 4, '0') AS lpc, "
                   "RPAD(c, 4, '0') AS rpc, REPEAT(txt, 2) AS reptxt, "
                   "LPAD(i, 5, '0') AS lpi, RPAD(d, 6, 'x') AS rpd, "
                   "LPAD(y, 4, '0') AS lpy, RPAD(dt, 11, ' ') AS rpdt FROM t ORDER BY id",
            .columns = columns_table,
            .column_count = sizeof(columns_table) / sizeof(columns_table[0]),
            .values = values_table,
            .row_count = 3U,
            .context = "table padding values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, LPAD(v, 3, '0') AS padded FROM t WHERE id >= 1 "
                   "ORDER BY id DESC LIMIT 2",
            .columns = columns_limited,
            .column_count = sizeof(columns_limited) / sizeof(columns_limited[0]),
            .values = values_limited,
            .row_count = 2U,
            .context = "table padding row envelope",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, LPAD(v, id + 4, '0') AS lp, "
                   "RPAD(v, ABS(id + 2), 'x') AS rp, REPEAT(v, id) AS rep, "
                   "SPACE(id) AS sp FROM t ORDER BY id",
            .columns = columns_integer_expression_padding,
            .column_count = sizeof(columns_integer_expression_padding) /
                            sizeof(columns_integer_expression_padding[0]),
            .values = values_integer_expression_padding,
            .row_count = 3U,
            .context = "table padding integer expression arguments",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t WHERE LPAD(v, 5, '0') = '00abc'",
            .columns = columns_count,
            .column_count = sizeof(columns_count) / sizeof(columns_count[0]),
            .values = values_count_one,
            .row_count = 1U,
            .context = "lpad predicate count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t WHERE RPAD(v, 5, '0') BETWEEN 'abc00' AND 'abc00'",
            .columns = columns_count,
            .column_count = sizeof(columns_count) / sizeof(columns_count[0]),
            .values = values_count_one,
            .row_count = 1U,
            .context = "rpad between predicate count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE REPEAT(v, 2) IS NULL ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_repeat_null_rows,
            .row_count = 1U,
            .context = "repeat is null predicate rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE SPACE(id) = '  ' ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_space_rows,
            .row_count = 1U,
            .context = "space predicate rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, LPAD(v, 5, '0') AS padded FROM t "
                   "ORDER BY LPAD(v, 5, '0'), id",
            .columns = columns_padding_order,
            .column_count = sizeof(columns_padding_order) / sizeof(columns_padding_order[0]),
            .values = values_padding_order,
            .row_count = 3U,
            .context = "padding order expression rows",
        }
    );
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "padding preamble"
    );

    mylite_close(database);
    database = NULL;
    failures += mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "reopen padding");
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, LPAD(v, 5, '0') AS padded FROM t WHERE id <= 2 ORDER BY id",
            .columns = columns_reopen,
            .column_count = sizeof(columns_reopen) / sizeof(columns_reopen[0]),
            .values = values_reopen,
            .row_count = 2U,
            .context = "reopen padding values",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_independent_file_backed_padding_handles(void) {
    static const char *const columns[] = {"padded"};
    static const char *const values_one[] = {"00a"};
    static const char *const values_two[] = {"0bb"};
    char path_one[test_path_capacity];
    char path_two[test_path_capacity];
    mylite_db *one = NULL;
    mylite_db *two = NULL;
    int failures = 0;

    failures += open_app_database(&one, "independent-one", path_one, sizeof(path_one));
    failures += open_app_database(&two, "independent-two", path_two, sizeof(path_two));
    failures += execute_ok(one, "CREATE TABLE t(v VARCHAR(8))", NULL);
    failures += execute_ok(two, "CREATE TABLE t(v VARCHAR(8))", NULL);
    failures += execute_ok(one, "INSERT INTO t VALUES ('a')", NULL);
    failures += execute_ok(two, "INSERT INTO t VALUES ('bb')", NULL);
    failures += expect_query(
        one,
        (struct expected_query){
            .sql = "SELECT LPAD(v, 3, '0') AS padded FROM t",
            .columns = columns,
            .column_count = sizeof(columns) / sizeof(columns[0]),
            .values = values_one,
            .row_count = 1U,
            .context = "independent padding handle one",
        }
    );
    failures += expect_query(
        two,
        (struct expected_query){
            .sql = "SELECT LPAD(v, 3, '0') AS padded FROM t",
            .columns = columns,
            .column_count = sizeof(columns) / sizeof(columns[0]),
            .values = values_two,
            .row_count = 1U,
            .context = "independent padding handle two",
        }
    );

    mylite_close(two);
    mylite_close(one);
    remove_related_files(path_two);
    remove_related_files(path_one);
    return failures;
}

static int test_padding_diagnostics(void) {
    static const char *const columns_expression_count[] = {"padded"};
    static const char *const values_expression_count[] = {"ab"};
    static const char *const columns_nested_value[] = {"padded"};
    static const char *const values_nested_value[] = {"00abc"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += execute_ok(
        database,
        "CREATE TABLE t(id INT, v VARCHAR(20), b VARBINARY(8), bitcol BIT(4), f DOUBLE)",
        NULL
    );
    failures +=
        execute_ok(database, "INSERT INTO t VALUES (1, 'abc', X'616263', b'1010', 1.25)", NULL);

    failures += execute_error(
        database,
        "SELECT LPAD()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'LPAD'",
        }
    );
    failures += execute_error(
        database,
        "SELECT RPAD('a', 1)",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'RPAD'",
        }
    );
    failures += execute_error(
        database,
        "SELECT SPACE(1, 2)",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'SPACE'",
        }
    );
    failures += execute_error(
        database,
        "SELECT REPEAT('a')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT LPAD(missing, 3, '0')",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT LPAD(missing, 3, '0') FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT LPAD(v, 1 + 1, '0') AS padded FROM t",
            .columns = columns_expression_count,
            .column_count = sizeof(columns_expression_count) / sizeof(columns_expression_count[0]),
            .values = values_expression_count,
            .row_count = 1U,
            .context = "LPAD arithmetic count argument",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT LPAD(CONCAT(v), 5, '0') AS padded FROM t",
            .columns = columns_nested_value,
            .column_count = sizeof(columns_nested_value) / sizeof(columns_nested_value[0]),
            .values = values_nested_value,
            .row_count = 1U,
            .context = "LPAD nested value argument",
        }
    );
    failures += execute_error(
        database,
        "SELECT LPAD(b, 5, '0') FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "string padding functions do not support binary columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT LPAD(bitcol, 5, '0') FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "string padding functions do not support binary columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT LPAD(f, 5, '0') FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "string padding functions do not support approximate numeric columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM t WHERE LPAD(v, 5, '0')",
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

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = NULL;
    size_t read_count = 0U;

    file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "%s: failed to open file\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fclose(file);
        fprintf(stderr, "%s: failed to seek file\n", path);
        return 1;
    }
    read_count = fread(buffer, 1U, size, file);
    fclose(file);
    if (read_count != size) {
        fprintf(stderr, "%s: expected %zu bytes, read %zu\n", path, size, read_count);
        return 1;
    }
    return 0;
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
