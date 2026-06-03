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

static int test_no_source_dual_and_do_string_slices(void);
static int test_table_backed_string_slices_and_reopen(void);
static int test_string_slice_diagnostics(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query expected);
static int open_app_database(
    mylite_db **out_database,
    const char *name,
    char *path,
    size_t path_size
);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
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
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
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

    failures += test_no_source_dual_and_do_string_slices();
    failures += test_table_backed_string_slices_and_reopen();
    failures += test_string_slice_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_no_source_dual_and_do_string_slices(void) {
    static const char *const columns_no_source[] = {
        "l5",    "r3",    "l0",    "r0",        "ln",    "rn",        "lp", "rp",
        "lbig",  "rbig",  "lnull", "lnull_len", "rnull", "rnull_len", "le", "rface",
        "lboth", "rboth", "li",    "rnint",     "ldb",   "rmode",     "lt", "rf",
    };
    static const char *const values_no_source[] = {
        "fooba",
        "bar",
        "",
        "",
        "",
        "",
        "ab",
        "bc",
        "abc",
        "abc",
        NULL,
        NULL,
        NULL,
        NULL,
        "\xC3\xA9",
        "\xF0\x9F\x99\x82",
        "\xC3\xA9\xF0\x9F\x99\x82",
        "\xC3\xA9\xF0\x9F\x99\x82",
        "12",
        "345",
        "ap",
        "SUBSTITUTION",
        "1",
        "0",
    };
    static const char *const columns_dual[] = {"a", "b"};
    static const char *const values_dual[] = {"a", "c"};
    static const char *const columns_substring[] = {
        "s2", "sf",  "m2", "s23",  "sf23", "mneg", "sn",  "sfar",  "sz",   "sl0",   "sln",
        "se", "sem", "si", "sint", "st",   "sfal", "sdb", "smode", "snul", "spnul", "slnul",
    };
    static const char *const values_substring[] = {
        "bcdef",
        "bcdef",
        "bcdef",
        "bcd",
        "bcd",
        "cd",
        "def",
        "",
        "",
        "",
        "",
        "\xC3\xA9\xF0\x9F\x99\x82",
        "\360\237\231\202a",
        "234",
        "12345",
        "1",
        "0",
        "ap",
        "SUBSTITUTION",
        NULL,
        NULL,
        NULL,
    };
    static const char *const columns_substring_index[] = {
        "p2",    "n2",     "missp",  "missn", "zero",    "emptyv", "nullv",
        "nulld", "nullc",  "casea",  "caseA", "utf8",    "dash",   "over1",
        "over2", "overn1", "overn2", "truth", "signedc", "num",    "falsec",
    };
    static const char *const values_substring_index[] = {
        "www.mysql", "mysql.com", "abc", "abc", "",   "",
        NULL,        NULL,        NULL,  "A",   "Aa", "\xC3\xA9/\xF0\x9F\x99\x82",
        "c",         "",          "aa",  "",    "aa", "a",
        "c",         "1",         "",
    };
    static const char *const columns_row_status[] = {"ROW_COUNT()", "@@warning_count"};
    static const char *const values_after_select[] = {"-1", "0"};
    static const char *const values_after_do[] = {"0", "0"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += open_app_database(&database, "no-source", path, sizeof(path));
    failures += execute_ok(database, "SET SESSION sql_mode = 'NO_ENGINE_SUBSTITUTION'", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT LEFT('foobarbar', 5) AS l5, RIGHT('foobarbar', 3) AS r3, "
                   "LEFT('abc', 0) AS l0, RIGHT('abc', 0) AS r0, LEFT('abc', -1) AS ln, "
                   "RIGHT('abc', -1) AS rn, LEFT('abc', +2) AS lp, RIGHT('abc', +2) AS rp, "
                   "LEFT('abc', 9) AS lbig, RIGHT('abc', 9) AS rbig, "
                   "LEFT(NULL, 1) AS lnull, LEFT('abc', NULL) AS lnull_len, "
                   "RIGHT(NULL, 1) AS rnull, RIGHT('abc', NULL) AS rnull_len, "
                   "LEFT('\xC3\xA9\xF0\x9F\x99\x82', 1) AS le, "
                   "RIGHT('\xC3\xA9\xF0\x9F\x99\x82', 1) AS rface, "
                   "LEFT('\xC3\xA9\xF0\x9F\x99\x82', 2) AS lboth, "
                   "RIGHT('\xC3\xA9\xF0\x9F\x99\x82', 2) AS rboth, "
                   "LEFT(12345, 2) AS li, RIGHT(-12345, 3) AS rnint, "
                   "LEFT(DATABASE(), 2) AS ldb, RIGHT(@@sql_mode, 12) AS rmode, "
                   "LEFT(TRUE, 1) AS lt, RIGHT(FALSE, 1) AS rf",
            .columns = columns_no_source,
            .column_count = sizeof(columns_no_source) / sizeof(columns_no_source[0]),
            .values = values_no_source,
            .row_count = 1U,
            .context = "no-source string slice values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT LEFT ('abc', 1) AS a, RIGHT ('abc', 1) AS b FROM DUAL",
            .columns = columns_dual,
            .column_count = sizeof(columns_dual) / sizeof(columns_dual[0]),
            .values = values_dual,
            .row_count = 1U,
            .context = "dual string slice whitespace",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT SUBSTRING('abcdef', 2) AS s2, SUBSTR('abcdef' FROM 2) AS sf, "
                   "MID('abcdef', 2) AS m2, SUBSTRING('abcdef', 2, 3) AS s23, "
                   "SUBSTR('abcdef' FROM 2 FOR 3) AS sf23, "
                   "MID('abcdef' FROM -4 FOR 2) AS mneg, SUBSTRING('abcdef', -3) AS sn, "
                   "SUBSTRING('abc', -5) AS sfar, SUBSTRING('abc', 0) AS sz, "
                   "SUBSTRING('abc', 1, 0) AS sl0, SUBSTRING('abc', 1, -1) AS sln, "
                   "SUBSTRING('\xC3\xA9\360\237\231\202abc', 1, 2) AS se, "
                   "SUBSTRING('\xC3\xA9\360\237\231\202abc', -4, 2) AS sem, "
                   "SUBSTRING(12345, 2, 3) AS si, SUBSTRING(-12345, 2) AS sint, "
                   "SUBSTRING(TRUE, 1) AS st, SUBSTRING(FALSE, 1) AS sfal, "
                   "SUBSTRING(DATABASE(), 1, 2) AS sdb, SUBSTRING(@@sql_mode, -12) AS smode, "
                   "SUBSTRING(NULL, 1) AS snul, SUBSTRING('abc', NULL) AS spnul, "
                   "SUBSTRING('abc', 1, NULL) AS slnul",
            .columns = columns_substring,
            .column_count = sizeof(columns_substring) / sizeof(columns_substring[0]),
            .values = values_substring,
            .row_count = 1U,
            .context = "no-source substring values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT SUBSTRING_INDEX('www.mysql.com', '.', 2) AS p2, "
                   "SUBSTRING_INDEX('www.mysql.com', '.', -2) AS n2, "
                   "SUBSTRING_INDEX('abc', '.', 1) AS missp, "
                   "SUBSTRING_INDEX('abc', '.', -1) AS missn, "
                   "SUBSTRING_INDEX('abc', '.', 0) AS zero, "
                   "SUBSTRING_INDEX('abc', '', 1) AS emptyv, "
                   "SUBSTRING_INDEX(NULL, '.', 1) AS nullv, "
                   "SUBSTRING_INDEX('abc', NULL, 1) AS nulld, "
                   "SUBSTRING_INDEX('abc', '.', NULL) AS nullc, "
                   "SUBSTRING_INDEX('AaA', 'a', 1) AS casea, "
                   "SUBSTRING_INDEX('AaA', 'A', 2) AS caseA, "
                   "SUBSTRING_INDEX('\xC3\xA9/\xF0\x9F\x99\x82/x', '/', 2) AS utf8, "
                   "SUBSTRING_INDEX('a--b--c', '--', -1) AS dash, "
                   "SUBSTRING_INDEX('aaaa', 'aa', 1) AS over1, "
                   "SUBSTRING_INDEX('aaaa', 'aa', 2) AS over2, "
                   "SUBSTRING_INDEX('aaaa', 'aa', -1) AS overn1, "
                   "SUBSTRING_INDEX('aaaa', 'aa', -2) AS overn2, "
                   "SUBSTRING_INDEX('abc', 'b', TRUE) AS truth, "
                   "SUBSTRING_INDEX('abc', 'b', -1) AS signedc, "
                   "SUBSTRING_INDEX(12345, '2', 1) AS num, "
                   "SUBSTRING_INDEX('abc', 'b', FALSE) AS falsec",
            .columns = columns_substring_index,
            .column_count = sizeof(columns_substring_index) / sizeof(columns_substring_index[0]),
            .values = values_substring_index,
            .row_count = 1U,
            .context = "no-source substring_index values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .columns = columns_row_status,
            .column_count = sizeof(columns_row_status) / sizeof(columns_row_status[0]),
            .values = values_after_select,
            .row_count = 1U,
            .context = "row count after string slice select",
        }
    );

    failures += execute_ok(
        database,
        "DO LEFT('abc', 1), RIGHT(NULL, 1), LEFT(TRUE, 1), "
        "SUBSTRING('abc', 1), SUBSTR(NULL, 1), MID('abc', 1, 1), "
        "SUBSTRING_INDEX('abc', 'b', 1), SUBSTRING_INDEX(NULL, '.', 1)",
        &result
    );
    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, "string slice do columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "string slice do rows");
        failures +=
            expect_int64(mylite_result_affected_rows(result), 0, "string slice do affected");
        failures +=
            expect_size(mylite_result_warning_count(result), 0U, "string slice do warnings");
    }
    mylite_result_free(result);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .columns = columns_row_status,
            .column_count = sizeof(columns_row_status) / sizeof(columns_row_status[0]),
            .values = values_after_do,
            .row_count = 1U,
            .context = "row count after string slice do",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_string_slices_and_reopen(void) {
    static const char *const columns_table[] = {
        "id",
        "lv",
        "rv",
        "lc",
        "rc",
        "lt",
        "rt",
        "li",
        "rd",
        "ly",
        "ldt",
    };
    static const char *const values_table[] = {
        "1",
        "ab",
        "c",
        "a",
        "a",
        "hel",
        "lo",
        "12",
        "30",
        "2024",
        "2024-01-02",
        "2",
        "\xC3\xA9\xF0\x9F\x99\x82",
        "\xF0\x9F\x99\x82",
        "\xC3\xA9",
        "\xC3\xA9",
        "",
        "",
        "-7",
        "50",
        "1970",
        NULL,
        "3",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
    };
    static const char *const columns_substring_table[] = {
        "id",
        "sv",
        "st",
        "mc",
        "tail",
        "si",
        "sd",
        "sy",
        "sdt",
    };
    static const char *const values_substring_table[] = {
        "1",
        "bc",
        "el",
        "a",
        "ello",
        "23",
        "30",
        "2024",
        "2024-01-02",
        "2",
        "\xF0\x9F\x99\x82",
        "",
        "\xC3\xA9",
        "",
        "7",
        "50",
        "1970",
        NULL,
        "3",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
    };
    static const char *const columns_limited[] = {"id", "rv"};
    static const char *const values_limited[] = {"3", NULL, "2", "\xF0\x9F\x99\x82"};
    static const char *const columns_substring_limited[] = {"id", "s"};
    static const char *const values_substring_limited[] = {"3", NULL, "2", ""};
    static const char *const columns_branches[] = {"id", "l0", "rn", "lnull_len", "rnull_len"};
    static const char *const values_branches[] = {
        "1",
        "",
        "",
        NULL,
        NULL,
        "3",
        NULL,
        NULL,
        NULL,
        NULL,
    };
    static const char *const columns_substring_branches[] = {
        "id",
        "sz",
        "sl0",
        "sln",
        "sfar",
        "spnul",
        "slnul",
    };
    static const char *const values_substring_branches[] = {
        "1",
        "",
        "",
        "",
        "",
        NULL,
        NULL,
        "3",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
    };
    static const char *const columns_case_position[] = {"id", "suffix"};
    static const char *const values_case_position[] = {
        "1",
        "llo",
        "2",
        "",
        "3",
        NULL,
    };
    static const char *const columns_concat_case_slice[] = {"id", "label"};
    static const char *const values_concat_case_slice[] = {
        "1",
        "xllo",
        "2",
        "y",
        "3",
        NULL,
    };
    static const char *const columns_labels[] = {"LEFT(v, 2)", "r"};
    static const char *const values_labels[] = {"ab", "c"};
    static const char *const columns_substring_labels[] = {"SUBSTRING(v, 2, 3)", "s"};
    static const char *const values_substring_labels[] = {"bc", "el"};
    static const char *const columns_reopen[] = {"lv", "rv"};
    static const char *const values_reopen[] = {"ab", "c"};
    static const char *const columns_substring_reopen[] = {"sv", "st"};
    static const char *const values_substring_reopen[] = {"bc", "el"};
    static const char *const columns_substring_index_table[] = {
        "id",
        "s",
        "dot",
        "sc",
        "si",
        "sd",
        "sy",
        "sdt",
        "tail",
    };
    static const char *const values_substring_index_table[] = {
        "1",
        "www.mysql",
        "mysql.com",
        "ab",
        "1",
        "12",
        "",
        "2024-01",
        "c",
        "2",
        "AaA",
        "AaA",
        "AA",
        "-",
        "-4",
        "1970",
        NULL,
        "",
        "3",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "4",
        "\xC3\xA9/\xF0\x9F\x99\x82",
        "\xC3\xA9/\xF0\x9F\x99\x82/x",
        "\xC3\xA9/\xF0\x9F\x99\x82",
        NULL,
        NULL,
        NULL,
        NULL,
        "right",
    };
    static const char *const columns_substring_index_limited[] = {"id", "s"};
    static const char *const values_substring_index_limited[] = {
        "4",
        "\xC3\xA9/\xF0\x9F\x99\x82",
        "3",
        NULL,
        "2",
        "AaA",
    };
    static const char *const columns_substring_index_labels[] = {
        "SUBSTRING_INDEX(v, '.', 1)",
        "s",
    };
    static const char *const values_substring_index_labels[] = {"www", "mysql.com"};
    static const char *const columns_substring_index_qualified[] = {"s"};
    static const char *const values_substring_index_qualified[] = {"www.mysql"};
    static const char *const columns_id[] = {"id"};
    static const char *const substring_case_predicate_rows[] = {"2"};
    static const char *const substring_null_safe_rows[] = {"3"};
    static const char *const substring_is_not_null_rows[] = {"1", "2", "4"};
    static const char *const substring_not_equal_rows[] = {"1"};
    static const char *const columns_substring_index_reopen[] = {"s", "tail"};
    static const char *const values_substring_index_reopen[] = {"www.mysql", "c"};
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
        "id INT, v VARCHAR(20), c CHAR(5), txt TEXT, i INT, d DECIMAL(6,2), "
        "y YEAR, dt DATETIME, b VARBINARY(4), f DOUBLE, hidden INT"
        ")",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO t(id, v, c, txt, i, d, y, dt, b, f, hidden) VALUES "
        "(1, 'abc', 'a  ', 'hello', 12345, 12.30, 2024, '2024-01-02 13:29:17', "
        "X'4142', 1.5, 77), "
        "(2, '\xC3\xA9\xF0\x9F\x99\x82', '\xC3\xA9', '', -7, -4.50, 70, NULL, "
        "X'c389', -2.5, 5), "
        "(3, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL)",
        NULL
    );
    failures += execute_ok(database, "ALTER TABLE t ALTER hidden SET INVISIBLE", NULL);
    failures += execute_ok(
        database,
        "CREATE TABLE si("
        "id INT, v VARCHAR(40), delim VARCHAR(8), c CHAR(6), txt TEXT, i INT, "
        "d DECIMAL(8,2), y YEAR, dt DATETIME"
        ")",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO si VALUES "
        "(1, 'www.mysql.com', '.', 'ab.cd', 'a/b/c', 12345, 12.30, 2024, "
        "'2024-01-02 13:29:17'), "
        "(2, 'AaA', 'a', 'AA', '', -22, -4.50, 70, NULL), "
        "(3, NULL, '.', NULL, NULL, NULL, NULL, NULL, NULL), "
        "(4, '\xC3\xA9/\xF0\x9F\x99\x82/x', '/', '\xC3\xA9/\xF0\x9F\x99\x82', "
        "'left/right', NULL, NULL, NULL, NULL)",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, LEFT(v, 2) AS lv, RIGHT(v, 1) AS rv, "
                   "LEFT(c, 2) AS lc, RIGHT(c, 1) AS rc, LEFT(txt, 3) AS lt, "
                   "RIGHT(txt, 2) AS rt, LEFT(i, 2) AS li, RIGHT(d, 2) AS rd, "
                   "LEFT(y, 4) AS ly, LEFT(dt, 10) AS ldt FROM t ORDER BY id",
            .columns = columns_table,
            .column_count = sizeof(columns_table) / sizeof(columns_table[0]),
            .values = values_table,
            .row_count = 3U,
            .context = "table string slice values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, SUBSTRING(v, 2, 3) AS sv, "
                   "SUBSTR(txt FROM -4 FOR 2) AS st, MID(c, 1, 2) AS mc, "
                   "SUBSTRING(txt, 2) AS tail, SUBSTRING(i, 2, 2) AS si, "
                   "SUBSTR(d, -2) AS sd, MID(y, 1, 4) AS sy, "
                   "SUBSTRING(dt, 1, 10) AS sdt FROM t ORDER BY id",
            .columns = columns_substring_table,
            .column_count = sizeof(columns_substring_table) / sizeof(columns_substring_table[0]),
            .values = values_substring_table,
            .row_count = 3U,
            .context = "table substring values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, RIGHT(v, 1) AS rv FROM t WHERE id >= 1 ORDER BY id DESC LIMIT 2",
            .columns = columns_limited,
            .column_count = sizeof(columns_limited) / sizeof(columns_limited[0]),
            .values = values_limited,
            .row_count = 2U,
            .context = "table string slice row envelope",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, SUBSTR(v FROM -4 FOR 2) AS s FROM t "
                   "WHERE id >= 1 ORDER BY id DESC LIMIT 2",
            .columns = columns_substring_limited,
            .column_count =
                sizeof(columns_substring_limited) / sizeof(columns_substring_limited[0]),
            .values = values_substring_limited,
            .row_count = 2U,
            .context = "table substring row envelope",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, LEFT(v, 0) AS l0, RIGHT(v, -1) AS rn, "
                   "LEFT(v, NULL) AS lnull_len, RIGHT(v, NULL) AS rnull_len "
                   "FROM t WHERE id IN (1, 3) ORDER BY id",
            .columns = columns_branches,
            .column_count = sizeof(columns_branches) / sizeof(columns_branches[0]),
            .values = values_branches,
            .row_count = 2U,
            .context = "table string slice null and nonpositive branches",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, SUBSTRING(v, 0) AS sz, SUBSTRING(v, 1, 0) AS sl0, "
                   "SUBSTRING(v, 1, -1) AS sln, SUBSTRING(v, -4) AS sfar, "
                   "SUBSTRING(v, NULL) AS spnul, SUBSTRING(v, 1, NULL) AS slnul "
                   "FROM t WHERE id IN (1, 3) ORDER BY id",
            .columns = columns_substring_branches,
            .column_count =
                sizeof(columns_substring_branches) / sizeof(columns_substring_branches[0]),
            .values = values_substring_branches,
            .row_count = 2U,
            .context = "table substring null and nonpositive branches",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, SUBSTRING(txt, "
                   "CHAR_LENGTH(CASE WHEN v LIKE 'a%' THEN 'he' ELSE '' END) + 1) AS suffix "
                   "FROM t ORDER BY id",
            .columns = columns_case_position,
            .column_count = sizeof(columns_case_position) / sizeof(columns_case_position[0]),
            .values = values_case_position,
            .row_count = 3U,
            .context = "substring char_length searched case position",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, CONCAT(CASE WHEN v LIKE 'a%' THEN 'x' ELSE 'y' END, "
                   "SUBSTRING(txt, CHAR_LENGTH(CASE WHEN v LIKE 'a%' THEN 'he' ELSE '' END) + 1)) "
                   "AS label FROM t ORDER BY id",
            .columns = columns_concat_case_slice,
            .column_count =
                sizeof(columns_concat_case_slice) / sizeof(columns_concat_case_slice[0]),
            .values = values_concat_case_slice,
            .row_count = 3U,
            .context = "concat searched case substring argument",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT LEFT(v, 2), RIGHT(v, 1) AS r FROM t WHERE id = 1",
            .columns = columns_labels,
            .column_count = sizeof(columns_labels) / sizeof(columns_labels[0]),
            .values = values_labels,
            .row_count = 1U,
            .context = "string slice labels",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT SUBSTRING(v, 2, 3), SUBSTR(txt FROM -4 FOR 2) AS s "
                   "FROM t WHERE id = 1",
            .columns = columns_substring_labels,
            .column_count = sizeof(columns_substring_labels) / sizeof(columns_substring_labels[0]),
            .values = values_substring_labels,
            .row_count = 1U,
            .context = "substring labels",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, SUBSTRING_INDEX(v, delim, 2) AS s, "
                   "SUBSTRING_INDEX(v, '.', -2) AS dot, "
                   "SUBSTRING_INDEX(c, '.', 1) AS sc, "
                   "SUBSTRING_INDEX(i, '2', 1) AS si, "
                   "SUBSTRING_INDEX(d, '.', 1) AS sd, "
                   "SUBSTRING_INDEX(y, '2', 1) AS sy, "
                   "SUBSTRING_INDEX(dt, '-', 2) AS sdt, "
                   "SUBSTRING_INDEX(txt, '/', -1) AS tail FROM si ORDER BY id",
            .columns = columns_substring_index_table,
            .column_count =
                sizeof(columns_substring_index_table) / sizeof(columns_substring_index_table[0]),
            .values = values_substring_index_table,
            .row_count = 4U,
            .context = "table substring_index values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, SUBSTRING_INDEX(v, delim, 2) AS s FROM si "
                   "WHERE id >= 2 ORDER BY id DESC LIMIT 3",
            .columns = columns_substring_index_limited,
            .column_count = sizeof(columns_substring_index_limited) /
                            sizeof(columns_substring_index_limited[0]),
            .values = values_substring_index_limited,
            .row_count = 3U,
            .context = "table substring_index row envelope",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT SUBSTRING_INDEX(v, '.', 1), SUBSTRING_INDEX(v, '.', -2) AS s "
                   "FROM si WHERE id = 1",
            .columns = columns_substring_index_labels,
            .column_count =
                sizeof(columns_substring_index_labels) / sizeof(columns_substring_index_labels[0]),
            .values = values_substring_index_labels,
            .row_count = 1U,
            .context = "substring_index labels",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT SUBSTRING_INDEX(s.v, s.delim, 2) AS s FROM si AS s WHERE s.id = 1",
            .columns = columns_substring_index_qualified,
            .column_count = sizeof(columns_substring_index_qualified) /
                            sizeof(columns_substring_index_qualified[0]),
            .values = values_substring_index_qualified,
            .row_count = 1U,
            .context = "substring_index qualified descriptor arguments",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM si WHERE SUBSTRING(v, 1, 1) = 'a'",
            .columns = columns_id,
            .column_count = 1U,
            .values = substring_case_predicate_rows,
            .row_count = 1U,
            .context = "substring case-insensitive predicate rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM si WHERE SUBSTR(v, 1) != SUBSTRING(v, 1) ORDER BY id",
            .columns = columns_id,
            .column_count = 1U,
            .values = NULL,
            .row_count = 0U,
            .context = "substring compared to substring synonym predicate rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM si WHERE SUBSTR(v FROM 1 FOR 1) <=> NULL",
            .columns = columns_id,
            .column_count = 1U,
            .values = substring_null_safe_rows,
            .row_count = 1U,
            .context = "substring null-safe predicate rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM si WHERE MID(v, 1, 1) IS NOT NULL ORDER BY id",
            .columns = columns_id,
            .column_count = 1U,
            .values = substring_is_not_null_rows,
            .row_count = 3U,
            .context = "substring is not null predicate rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM si WHERE SUBSTRING(v, 1, 1) <> 'a' AND id < 4 ORDER BY id",
            .columns = columns_id,
            .column_count = 1U,
            .values = substring_not_equal_rows,
            .row_count = 1U,
            .context = "substring not equal predicate rows",
        }
    );
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(actual_preamble),
        "string slice preamble unchanged"
    );

    mylite_close(database);
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen string slice file");
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT LEFT(v, 2) AS lv, RIGHT(v, 1) AS rv FROM t WHERE id = 1",
            .columns = columns_reopen,
            .column_count = sizeof(columns_reopen) / sizeof(columns_reopen[0]),
            .values = values_reopen,
            .row_count = 1U,
            .context = "string slice reopen",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT SUBSTRING(v, 2, 3) AS sv, SUBSTR(txt FROM -4 FOR 2) AS st "
                   "FROM t WHERE id = 1",
            .columns = columns_substring_reopen,
            .column_count = sizeof(columns_substring_reopen) / sizeof(columns_substring_reopen[0]),
            .values = values_substring_reopen,
            .row_count = 1U,
            .context = "substring reopen",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT SUBSTRING_INDEX(v, delim, 2) AS s, "
                   "SUBSTRING_INDEX(txt, '/', -1) AS tail FROM si WHERE id = 1",
            .columns = columns_substring_index_reopen,
            .column_count =
                sizeof(columns_substring_index_reopen) / sizeof(columns_substring_index_reopen[0]),
            .values = values_substring_index_reopen,
            .row_count = 1U,
            .context = "substring_index reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_string_slice_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += execute_ok(
        database,
        "CREATE TABLE t(id INT, v VARCHAR(20), b VARBINARY(4), f DOUBLE)",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES (1, 'abc', X'6162', 1.5), "
        "(2, '\xC3\xA9', X'c389', -2.5)",
        NULL
    );
    failures += execute_error(
        database,
        "SELECT LEFT()",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT RIGHT('a', 1, 2)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT MID('a', 1, 2, 3)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT SUBSTRING_INDEX()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part =
                "Incorrect parameter count in the call to native function 'SUBSTRING_INDEX'",
        }
    );
    failures += execute_error(
        database,
        "SELECT SUBSTRING_INDEX('a')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part =
                "Incorrect parameter count in the call to native function 'SUBSTRING_INDEX'",
        }
    );
    failures += execute_error(
        database,
        "SELECT SUBSTRING_INDEX('a', 'b')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part =
                "Incorrect parameter count in the call to native function 'SUBSTRING_INDEX'",
        }
    );
    failures += execute_error(
        database,
        "SELECT SUBSTRING_INDEX('a', 'b', 1, 2)",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part =
                "Incorrect parameter count in the call to native function 'SUBSTRING_INDEX'",
        }
    );
    failures += execute_error(
        database,
        "SELECT SUBSTRING ('abc', 1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT LEFT(missing, 1)",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT LEFT(missing, 1) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT LEFT('abc', '2')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "string slice functions support only integer, boolean, and NULL length literals",
        }
    );
    failures += execute_error(
        database,
        "SELECT SUBSTRING('abc', '2')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "string slice functions support only integer, boolean, and NULL position literals",
        }
    );
    failures += execute_error(
        database,
        "SELECT SUBSTRING('abc', 1, '2')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "string slice functions support only integer, boolean, and NULL length literals",
        }
    );
    failures += execute_error(
        database,
        "SELECT SUBSTRING('abc', 9223372036854775808)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "string slice function position literals must fit the signed 64-bit range",
        }
    );
    failures += execute_error(
        database,
        "SELECT SUBSTRING_INDEX('abc', 'b', '2')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SUBSTRING_INDEX() count supports only integer, boolean, and NULL",
        }
    );
    failures += execute_error(
        database,
        "SELECT SUBSTRING_INDEX('abc', 'b', 9223372036854775808)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SUBSTRING_INDEX() count literals must fit the signed 64-bit range",
        }
    );
    failures += execute_error(
        database,
        "SELECT LEFT(CAST('ABC' AS BINARY), 1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "string slice functions support only string, integer, boolean, NULL",
        }
    );
    failures += execute_error(
        database,
        "SELECT SUBSTRING(CAST('ABC' AS BINARY), 1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "string slice functions support only string, integer, boolean, NULL",
        }
    );
    failures += execute_error(
        database,
        "SELECT SUBSTRING_INDEX(CAST('ABC' AS BINARY), '.', 1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SUBSTRING_INDEX() supports only string, integer, boolean, NULL",
        }
    );
    failures += execute_error(
        database,
        "SELECT SUBSTRING_INDEX(X'61', '.', 1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SUBSTRING_INDEX() supports only string, integer, boolean, NULL",
        }
    );
    failures += execute_error(
        database,
        "SELECT LEFT(b, 1) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "string slice functions do not support binary columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT SUBSTR(b, 1) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "string slice functions do not support binary columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT SUBSTRING_INDEX(b, '.', 1) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SUBSTRING_INDEX() does not support binary columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT LEFT(f, 1) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "string slice functions do not support approximate numeric columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT SUBSTRING_INDEX(f, '.', 1) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SUBSTRING_INDEX() does not support approximate columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT SUBSTRING_INDEX(missing, '.', 1) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT SUBSTRING_INDEX(v, '.', id) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SUBSTRING_INDEX() count supports only integer, boolean, and NULL",
        }
    );
    failures += execute_error(
        database,
        "SELECT SUBSTRING_INDEX(LEFT(v, 1), '.', 1) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SUBSTRING_INDEX() supports only string, integer, boolean, NULL",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM t WHERE SUBSTRING(missing, 1, 1) = 'a'",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'where clause'",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM t WHERE SUBSTRING(f, 1, 1) = 'a'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "string slice functions do not support approximate numeric columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM t WHERE SUBSTRING(v, '1') = 'a'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "string slice functions support only integer, boolean, and NULL position literals",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM t WHERE SUBSTRING(v, 1, 1) = 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "WHERE string predicates support only string literals",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM t WHERE SUBSTRING_INDEX(v, '.', 1) = 'a'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near '('",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM t ORDER BY SUBSTRING_INDEX(v, '.', 1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near '('",
        }
    );
    failures += execute_error(
        database,
        "UPDATE t SET v = SUBSTRING_INDEX(v, '.', 1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near 'SUBSTRING_INDEX'",
        }
    );
    failures += execute_error(
        database,
        "SELECT MID(f, 1) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "string slice functions do not support approximate numeric columns",
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
    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, sql);
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

    if (make_test_path(path, path_size, name) != 0) {
        return 1;
    }
    remove_related_files(path);
    failures += expect_int(mylite_open(path, out_database), MYLITE_OK, name);
    if (failures == 0) {
        failures += execute_ok(*out_database, "CREATE DATABASE app", NULL);
    }
    if (failures == 0) {
        failures += execute_ok(*out_database, "USE app", NULL);
    }
    return failures;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite-string-slice-functions-%s-%d.mylite",
        name,
        current_process_id()
    );

    return written < 0 || (size_t)written >= path_size ? 1 : 0;
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
    char related_path[test_path_capacity + path_suffix_capacity];
    int written = snprintf(related_path, sizeof(related_path), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related_path)) {
        return;
    }
    (void)remove(related_path);
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    int failures = 0;

    if (file == NULL) {
        fprintf(stderr, "%s: failed to open file\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "%s: failed to seek file\n", path);
        failures = 1;
    } else if (fread(buffer, 1U, size, file) != size) {
        fprintf(stderr, "%s: failed to read file\n", path);
        failures = 1;
    }
    if (fclose(file) != 0) {
        fprintf(stderr, "%s: failed to close file\n", path);
        failures = 1;
    }
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, expected.sql, &result);

    if (failures != 0) {
        return failures;
    }
    failures +=
        expect_size(mylite_result_column_count(result), expected.column_count, expected.context);
    failures += expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, expected.context);

    for (size_t column = 0U; column < expected.column_count; ++column) {
        failures += expect_text(
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

static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
) {
    const char *actual = mylite_result_value_text(result, row, column);

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
    if (actual == NULL || expected == NULL) {
        if (actual != expected) {
            fprintf(
                stderr,
                "%s: expected %s, got %s\n",
                context,
                expected == NULL ? "(null)" : expected,
                actual == NULL ? "(null)" : actual
            );
            return 1;
        }
        return 0;
    }
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "%s: expected %s, got %s\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || needle == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected message containing %s, got %s\n",
            context,
            needle == NULL ? "(null)" : needle,
            actual == NULL ? "(null)" : actual
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
        fprintf(stderr, "%s: byte mismatch\n", context);
        return 1;
    }
    return 0;
}
