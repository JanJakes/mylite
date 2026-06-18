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
    mysql_error_native_function_argument_count = 1582,
    mysql_error_unknown_column = 1054,
    mysql_error_parse = 1064,
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

static int test_no_source_dual_and_do_string_searches(void);
static int test_table_backed_string_searches_and_reopen(void);
static int test_no_source_dual_and_do_find_in_set(void);
static int test_table_backed_find_in_set_predicates_and_dml(void);
static int test_no_source_dual_and_do_strcmp(void);
static int test_table_backed_strcmp_and_reopen(void);
static int test_string_search_diagnostics(void);
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

    failures += test_no_source_dual_and_do_string_searches();
    failures += test_table_backed_string_searches_and_reopen();
    failures += test_no_source_dual_and_do_find_in_set();
    failures += test_table_backed_find_in_set_predicates_and_dml();
    failures += test_no_source_dual_and_do_strcmp();
    failures += test_table_backed_strcmp_and_reopen();
    failures += test_string_search_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_no_source_dual_and_do_string_searches(void) {
    static const char *const columns_no_source[] = {
        "loc",    "instr",  "pos",       "missing",  "loc5", "empty1", "empty2",
        "empty4", "empty5", "zero",      "negative", "n1",   "n2",     "n3",
        "n4",     "n5",     "case_fold", "num",      "bool",
    };
    static const char *const values_no_source[] = {
        "4", "4",  "4",  "0",  "7",  "1",  "2", "4", "0", "0",
        "0", NULL, NULL, NULL, NULL, NULL, "1", "2", "1",
    };
    static const char *const columns_dual[] = {"a", "b"};
    static const char *const values_dual[] = {"1", "1"};
    static const char *const columns_scalar_integer_arguments[] = {"loc_abs", "empty_len"};
    static const char *const values_scalar_integer_arguments[] = {"2", "4"};
    static const char *const columns_row_status[] = {"ROW_COUNT()", "@@warning_count"};
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
            .sql = "SELECT LOCATE('bar','foobarbar') AS loc, "
                   "INSTR('foobarbar','bar') AS instr, "
                   "POSITION('bar' IN 'foobarbar') AS pos, LOCATE('xbar','foobar') AS missing, "
                   "LOCATE('bar','foobarbar',5) AS loc5, LOCATE('', 'abc') AS empty1, "
                   "LOCATE('', 'abc', 2) AS empty2, LOCATE('', 'abc', 4) AS empty4, "
                   "LOCATE('', 'abc', 5) AS empty5, LOCATE('a','abc',0) AS zero, "
                   "LOCATE('a','abc',-1) AS negative, LOCATE(NULL,'abc') AS n1, "
                   "LOCATE('a',NULL) AS n2, LOCATE('a','abc',NULL) AS n3, "
                   "INSTR(NULL,'a') AS n4, POSITION('a' IN NULL) AS n5, "
                   "LOCATE('A','abc') AS case_fold, LOCATE(23, 12345) AS num, "
                   "LOCATE(TRUE, 12345) AS bool",
            .columns = columns_no_source,
            .column_count = sizeof(columns_no_source) / sizeof(columns_no_source[0]),
            .values = values_no_source,
            .row_count = 1U,
            .context = "no-source string search values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT LOCATE ('a','abc') AS a, INSTR ('abc','a') AS b FROM DUAL",
            .columns = columns_dual,
            .column_count = sizeof(columns_dual) / sizeof(columns_dual[0]),
            .values = values_dual,
            .row_count = 1U,
            .context = "dual string search whitespace",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT LOCATE('b', 'abc', ABS(-2)) AS loc_abs, "
                   "LOCATE('', 'abc', LENGTH('abcd')) AS empty_len",
            .columns = columns_scalar_integer_arguments,
            .column_count = sizeof(columns_scalar_integer_arguments) /
                            sizeof(columns_scalar_integer_arguments[0]),
            .values = values_scalar_integer_arguments,
            .row_count = 1U,
            .context = "no-source string search integer function arguments",
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
            .context = "row count after string search select",
        }
    );

    failures += execute_ok(database, "DO LOCATE('a','abc'), INSTR('abc','a')", &result);
    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, "string search do columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "string search do rows");
        failures +=
            expect_int64(mylite_result_affected_rows(result), 0, "string search do affected");
        failures +=
            expect_size(mylite_result_warning_count(result), 0U, "string search do warnings");
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
            .context = "row count after string search do",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_string_searches_and_reopen(void) {
    static const char *const columns_table[] = {
        "id",
        "loc",
        "instr",
        "pos",
        "char_a",
        "num_2",
        "dec_minus",
        "folded",
    };
    static const char *const values_table[] = {
        "1", "4",  "4",  "4", "1", "2",  "1",  "4",  "2",  "3", "3", "3",
        "0", NULL, NULL, "3", "3", NULL, NULL, NULL, NULL, "0", "0", NULL,
    };
    static const char *const columns_limited[] = {"id", "loc"};
    static const char *const values_limited[] = {"3", NULL, "2", "3"};
    static const char *const columns_integer_expression_search[] = {"id", "from_expr", "num_expr"};
    static const char *const values_integer_expression_search[] = {
        "1",
        "4",
        "2",
        "2",
        "0",
        NULL,
        "3",
        NULL,
        "0",
    };
    static const char *const columns_type_coverage[] = {
        "text_pos",
        "year_pos",
        "date_pos",
        "time_pos",
        "datetime_pos",
        "timestamp_pos",
    };
    static const char *const values_type_coverage[] = {"4", "1", "6", "4", "18", "18"};
    static const char *const columns_labels[] = {
        "LOCATE('bar',s)",
        "b",
        "POSITION('bar' IN s)",
    };
    static const char *const values_labels[] = {"4", "4", "4"};
    static const char *const columns_predicate_truth[] = {"id"};
    static const char *const values_predicate_truth[] = {"1", "2"};
    static const char *const columns_predicate_comparison[] = {"id"};
    static const char *const values_predicate_comparison[] = {"2", "1"};
    static const char *const columns_predicate_null[] = {"id"};
    static const char *const values_predicate_null[] = {"3"};
    static const char *const columns_predicate_not_null[] = {"id"};
    static const char *const values_predicate_not_null[] = {"1", "2"};
    static const char *const columns_predicate_between[] = {"id"};
    static const char *const values_predicate_between[] = {"1", "2"};
    static const char *const columns_predicate_not_between[] = {"id"};
    static const char *const values_predicate_not_between[] = {"2"};
    static const char *const columns_order_expression[] = {"id", "pos"};
    static const char *const values_order_expression[] = {"1", "4", "2", "3", "3", NULL};
    static const char *const columns_reopen[] = {"id", "loc", "instr"};
    static const char *const values_reopen[] = {"1", "4", "4", "2", "3", "3"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    mylite_file_preamble_init(expected_preamble);
    failures += open_app_database(&database, "table", path, sizeof(path));
    failures += execute_ok(
        database,
        "CREATE TABLE t(id INT, s VARCHAR(30), c CHAR(10), n INT, d DECIMAL(6,2), "
        "tx TEXT, y YEAR, da DATE, ti TIME, dt DATETIME, ts TIMESTAMP)",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES "
        "(1, 'foobarbar', 'abc', 12345, -12.30, 'alpha beta', 2024, '2024-01-02', "
        "'03:04:05', '2024-01-02 03:04:05', '2024-01-02 03:04:06'), "
        "(2, 'xxbar', 'xyz', NULL, NULL, 'needle', 2000, '2000-02-03', '04:05:06', "
        "'2000-02-03 04:05:06', '2000-02-03 04:05:07'), "
        "(3, NULL, NULL, 10000, 10.50, NULL, NULL, NULL, NULL, NULL, NULL)",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, LOCATE('bar', s) AS loc, INSTR(s, 'bar') AS instr, "
                   "POSITION('bar' IN s) AS pos, LOCATE('a', c) AS char_a, "
                   "LOCATE('2', n) AS num_2, LOCATE('-', d) AS dec_minus, "
                   "LOCATE('BAR', s) AS folded FROM t ORDER BY id",
            .columns = columns_table,
            .column_count = sizeof(columns_table) / sizeof(columns_table[0]),
            .values = values_table,
            .row_count = 3U,
            .context = "table string search values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, LOCATE('bar', s) AS loc FROM t WHERE id >= 1 "
                   "ORDER BY id DESC LIMIT 2",
            .columns = columns_limited,
            .column_count = sizeof(columns_limited) / sizeof(columns_limited[0]),
            .values = values_limited,
            .row_count = 2U,
            .context = "table string search row envelope",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, LOCATE('bar', s, id + 3) AS from_expr, "
                   "LOCATE('2', n, ABS(id)) AS num_expr FROM t ORDER BY id",
            .columns = columns_integer_expression_search,
            .column_count = sizeof(columns_integer_expression_search) /
                            sizeof(columns_integer_expression_search[0]),
            .values = values_integer_expression_search,
            .row_count = 3U,
            .context = "table string search integer expression arguments",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT LOCATE('ha', tx) AS text_pos, LOCATE('20', y) AS year_pos, "
                   "LOCATE('01', da) AS date_pos, LOCATE('04', ti) AS time_pos, "
                   "LOCATE('05', dt) AS datetime_pos, LOCATE('06', ts) AS timestamp_pos "
                   "FROM t WHERE id = 1",
            .columns = columns_type_coverage,
            .column_count = sizeof(columns_type_coverage) / sizeof(columns_type_coverage[0]),
            .values = values_type_coverage,
            .row_count = 1U,
            .context = "table string search descriptor type coverage",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT LOCATE('bar',s), INSTR(s,'bar') AS b, "
                   "POSITION('bar' IN s) FROM t WHERE id = 1",
            .columns = columns_labels,
            .column_count = sizeof(columns_labels) / sizeof(columns_labels[0]),
            .values = values_labels,
            .row_count = 1U,
            .context = "string search labels",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE LOCATE('bar', s) ORDER BY id",
            .columns = columns_predicate_truth,
            .column_count = sizeof(columns_predicate_truth) / sizeof(columns_predicate_truth[0]),
            .values = values_predicate_truth,
            .row_count = 2U,
            .context = "string search predicate truth",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE LOCATE('bar', s) > 0 "
                   "ORDER BY INSTR(s, 'bar'), id",
            .columns = columns_predicate_comparison,
            .column_count =
                sizeof(columns_predicate_comparison) / sizeof(columns_predicate_comparison[0]),
            .values = values_predicate_comparison,
            .row_count = 2U,
            .context = "string search predicate comparison and order",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE INSTR(s, 'bar') IS NULL ORDER BY id",
            .columns = columns_predicate_null,
            .column_count = sizeof(columns_predicate_null) / sizeof(columns_predicate_null[0]),
            .values = values_predicate_null,
            .row_count = 1U,
            .context = "string search predicate null test",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE INSTR(s, 'bar') IS NOT NULL ORDER BY id",
            .columns = columns_predicate_not_null,
            .column_count =
                sizeof(columns_predicate_not_null) / sizeof(columns_predicate_not_null[0]),
            .values = values_predicate_not_null,
            .row_count = 2U,
            .context = "string search predicate not null test",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE POSITION('bar' IN s) BETWEEN 3 AND 4 ORDER BY id",
            .columns = columns_predicate_between,
            .column_count =
                sizeof(columns_predicate_between) / sizeof(columns_predicate_between[0]),
            .values = values_predicate_between,
            .row_count = 2U,
            .context = "string search predicate between",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE POSITION('bar' IN s) NOT BETWEEN 4 AND 4 "
                   "ORDER BY id",
            .columns = columns_predicate_not_between,
            .column_count =
                sizeof(columns_predicate_not_between) / sizeof(columns_predicate_not_between[0]),
            .values = values_predicate_not_between,
            .row_count = 1U,
            .context = "string search predicate not between",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, POSITION('bar' IN s) AS pos FROM t "
                   "ORDER BY POSITION('bar' IN s) DESC, id",
            .columns = columns_order_expression,
            .column_count = sizeof(columns_order_expression) / sizeof(columns_order_expression[0]),
            .values = values_order_expression,
            .row_count = 3U,
            .context = "string search order expression",
        }
    );
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(actual_preamble),
        "string search preamble unchanged"
    );

    mylite_close(database);
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen string search file");
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, LOCATE('bar', s) AS loc, INSTR(s, 'bar') AS instr "
                   "FROM t WHERE id <= 2 ORDER BY id",
            .columns = columns_reopen,
            .column_count = sizeof(columns_reopen) / sizeof(columns_reopen[0]),
            .values = values_reopen,
            .row_count = 2U,
            .context = "string search reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_no_source_dual_and_do_find_in_set(void) {
    static const char *const columns_no_source[] = {
        "hit",    "missing",   "n1",     "n2",        "n3",           "empty0",   "empty1",
        "empty2", "empty3",    "empty4", "empty5",    "comma_search", "folded",   "space1",
        "space2", "duplicate", "num",    "bool_true", "bool_false",   "negative",
    };
    static const char *const values_no_source[] = {
        "2", "0", NULL, NULL, NULL, "0", "0", "2", "1", "2",
        "0", "0", "1",  "0",  "2",  "2", "2", "2", "1", "2",
    };
    static const char *const columns_dual[] = {"spaced", "alias"};
    static const char *const values_dual[] = {"2", "2"};
    static const char *const columns_nested[] = {"concat_args", "numeric_arg"};
    static const char *const values_nested[] = {"2", "2"};
    static const char *const columns_row_status[] = {"ROW_COUNT()", "@@warning_count"};
    static const char *const values_after_select[] = {"-1", "0"};
    static const char *const values_after_do[] = {"0", "0"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += open_app_database(&database, "find-no-source", path, sizeof(path));
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT FIND_IN_SET('b','a,b,c') AS hit, "
                   "FIND_IN_SET('x','a,b,c') AS missing, FIND_IN_SET(NULL,'a,b') AS n1, "
                   "FIND_IN_SET('a',NULL) AS n2, FIND_IN_SET(NULL,NULL) AS n3, "
                   "FIND_IN_SET('','') AS empty0, FIND_IN_SET('','a') AS empty1, "
                   "FIND_IN_SET('', 'a,') AS empty2, FIND_IN_SET('', ',a') AS empty3, "
                   "FIND_IN_SET('', 'a,,b') AS empty4, FIND_IN_SET('a','') AS empty5, "
                   "FIND_IN_SET('a,b','a,b') AS comma_search, "
                   "FIND_IN_SET('abc','ABC,def') AS folded, "
                   "FIND_IN_SET('b','a, b,c') AS space1, "
                   "FIND_IN_SET(' b','a, b,c') AS space2, "
                   "FIND_IN_SET('b','a,b,b') AS duplicate, "
                   "FIND_IN_SET(2,'1,2,3') AS num, FIND_IN_SET(TRUE,'0,1,2') AS bool_true, "
                   "FIND_IN_SET(FALSE,'0,1') AS bool_false, "
                   "FIND_IN_SET(-1,'0,-1') AS negative",
            .columns = columns_no_source,
            .column_count = sizeof(columns_no_source) / sizeof(columns_no_source[0]),
            .values = values_no_source,
            .row_count = 1U,
            .context = "no-source find_in_set values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT FIND_IN_SET ('b','a,b') AS spaced, "
                   "FIND_IN_SET('green','red,green') AS alias FROM DUAL",
            .columns = columns_dual,
            .column_count = sizeof(columns_dual) / sizeof(columns_dual[0]),
            .values = values_dual,
            .row_count = 1U,
            .context = "dual find_in_set",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT FIND_IN_SET(CONCAT('b'), CONCAT('a,','b')) AS concat_args, "
                   "FIND_IN_SET(LOCATE('b','abc'), '1,2,3') AS numeric_arg",
            .columns = columns_nested,
            .column_count = sizeof(columns_nested) / sizeof(columns_nested[0]),
            .values = values_nested,
            .row_count = 1U,
            .context = "nested find_in_set arguments",
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
            .context = "row count after find_in_set select",
        }
    );

    failures += execute_ok(database, "DO FIND_IN_SET('x', 'a,x'), FIND_IN_SET(NULL, 'a')", &result);
    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, "find_in_set do columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "find_in_set do rows");
        failures += expect_int64(mylite_result_affected_rows(result), 0, "find_in_set do affected");
        failures += expect_size(mylite_result_warning_count(result), 0U, "find_in_set do warnings");
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
            .context = "row count after find_in_set do",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_find_in_set_predicates_and_dml(void) {
    static const char *const columns_table[] = {
        "id",
        "i_pos",
        "d_pos",
        "v_pos",
        "tx_pos",
        "year_pos",
        "date_pos",
        "time_pos",
        "datetime_pos",
        "timestamp_pos",
        "enum_pos",
    };
    static const char *const values_table[] = {
        "1", "1",  "1",  "2",  "2",  "1",  "1",  "1",  "1",  "1",  "1",
        "2", NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    };
    static const char *const columns_id[] = {"id"};
    static const char *const values_red[] = {"1", "5"};
    static const char *const values_zero[] = {"2", "3"};
    static const char *const values_null[] = {"4"};
    static const char *const values_not_null[] = {"1", "2", "3", "5"};
    static const char *const columns_update[] = {"id", "note"};
    static const char *const values_after_update[] = {
        "1",
        "hit",
        "2",
        "start",
        "3",
        "hit",
        "4",
        "start",
        "5",
        "start",
    };
    static const char *const values_after_delete[] = {"1", "hit", "4", "start", "5", "start"};
    static const char *const values_reopen[] = {"1", "hit", "4", "start", "5", "start"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    mylite_file_preamble_init(expected_preamble);
    failures += open_app_database(&database, "find-table", path, sizeof(path));
    failures += execute_ok(
        database,
        "CREATE TABLE typed("
        "id INT, i INT, d DECIMAL(6,2), v VARCHAR(40), tx TEXT, y YEAR, da DATE, ti TIME, "
        "dt DATETIME, ts TIMESTAMP, e ENUM('alpha','beta'), s SET('red','green','blue'))",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO typed VALUES "
        "(1, 123, -12.30, 'red,green', 'alpha,beta', 2024, '2024-01-02', "
        "'03:04:05', '2024-01-02 03:04:05', '2024-01-02 03:04:06', 'alpha', "
        "'red,blue'), "
        "(2, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL)",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, FIND_IN_SET('123', i) AS i_pos, "
                   "FIND_IN_SET('-12.30', d) AS d_pos, FIND_IN_SET('green', v) AS v_pos, "
                   "FIND_IN_SET('beta', tx) AS tx_pos, FIND_IN_SET('2024', y) AS year_pos, "
                   "FIND_IN_SET('2024-01-02', da) AS date_pos, "
                   "FIND_IN_SET('03:04:05', ti) AS time_pos, "
                   "FIND_IN_SET('2024-01-02 03:04:05', dt) AS datetime_pos, "
                   "FIND_IN_SET('2024-01-02 03:04:06', ts) AS timestamp_pos, "
                   "FIND_IN_SET('alpha', e) AS enum_pos "
                   "FROM typed ORDER BY id",
            .columns = columns_table,
            .column_count = sizeof(columns_table) / sizeof(columns_table[0]),
            .values = values_table,
            .row_count = 2U,
            .context = "table find_in_set descriptor types",
        }
    );
    failures += execute_error(
        database,
        "SELECT FIND_IN_SET('blue', s) FROM typed",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "FIND_IN_SET() does not support SET columns",
        }
    );

    failures += execute_ok(database, "CREATE TABLE p(id INT, tags VARCHAR(40))", NULL);
    failures += execute_ok(
        database,
        "INSERT INTO p VALUES (1, 'red,green'), (2, 'blue'), (3, ''), (4, NULL), (5, 'Red')",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM p WHERE FIND_IN_SET('red', tags) ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_red,
            .row_count = 2U,
            .context = "find_in_set truth predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM p WHERE FIND_IN_SET('red', tags) > 0 ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_red,
            .row_count = 2U,
            .context = "find_in_set comparison predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM p WHERE FIND_IN_SET('red', tags) = 0 ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_zero,
            .row_count = 2U,
            .context = "find_in_set zero predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM p WHERE FIND_IN_SET('red', tags) IS NULL ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_null,
            .row_count = 1U,
            .context = "find_in_set is null predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM p WHERE FIND_IN_SET('red', tags) IS NOT NULL ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_not_null,
            .row_count = 4U,
            .context = "find_in_set is not null predicate",
        }
    );

    failures +=
        execute_ok(database, "CREATE TABLE dml(id INT, tags VARCHAR(40), note VARCHAR(10))", NULL);
    failures += execute_ok(
        database,
        "INSERT INTO dml VALUES "
        "(1, 'red', 'start'), (2, 'blue', 'start'), (3, 'red,blue', 'start'), "
        "(4, NULL, 'start'), (5, '', 'start')",
        NULL
    );
    failures +=
        execute_ok(database, "UPDATE dml SET note = 'hit' WHERE FIND_IN_SET('red', tags)", &result);
    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, "find update columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "find update rows");
        failures += expect_int64(mylite_result_affected_rows(result), 2, "find update affected");
        failures += expect_size(mylite_result_warning_count(result), 0U, "find update warnings");
    }
    mylite_result_free(result);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, note FROM dml ORDER BY id",
            .columns = columns_update,
            .column_count = sizeof(columns_update) / sizeof(columns_update[0]),
            .values = values_after_update,
            .row_count = sizeof(values_after_update) / sizeof(values_after_update[0]) /
                         (sizeof(columns_update) / sizeof(columns_update[0])),
            .context = "find_in_set update rows",
        }
    );
    failures +=
        execute_ok(database, "DELETE FROM dml WHERE FIND_IN_SET('blue', tags) > 0", &result);
    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, "find delete columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "find delete rows");
        failures += expect_int64(mylite_result_affected_rows(result), 2, "find delete affected");
        failures += expect_size(mylite_result_warning_count(result), 0U, "find delete warnings");
    }
    mylite_result_free(result);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, note FROM dml ORDER BY id",
            .columns = columns_update,
            .column_count = sizeof(columns_update) / sizeof(columns_update[0]),
            .values = values_after_delete,
            .row_count = sizeof(values_after_delete) / sizeof(values_after_delete[0]) /
                         (sizeof(columns_update) / sizeof(columns_update[0])),
            .context = "find_in_set delete rows",
        }
    );
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(actual_preamble),
        "find_in_set preamble unchanged"
    );

    mylite_close(database);
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen find_in_set file");
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, note FROM dml ORDER BY id",
            .columns = columns_update,
            .column_count = sizeof(columns_update) / sizeof(columns_update[0]),
            .values = values_reopen,
            .row_count = sizeof(values_reopen) / sizeof(values_reopen[0]) /
                         (sizeof(columns_update) / sizeof(columns_update[0])),
            .context = "find_in_set reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_no_source_dual_and_do_strcmp(void) {
    static const char *const columns_no_source[] = {
        "lt",
        "gt",
        "eq",
        "n1",
        "n2",
        "case_fold",
        "empty0",
        "empty_lt",
        "empty_gt",
        "trail_gt",
        "trail_lt",
        "num_lt",
        "num_gt",
        "bool_t",
        "bool_f",
        "negative",
        "warn",
    };
    static const char *const values_no_source[] = {
        "-1",
        "1",
        "0",
        NULL,
        NULL,
        "0",
        "0",
        "-1",
        "1",
        "1",
        "-1",
        "-1",
        "1",
        "0",
        "0",
        "-1",
        "0",
    };
    static const char *const columns_dual[] = {"c", "STRCMP('b','a')"};
    static const char *const values_dual[] = {"1", "1"};
    static const char *const columns_nested[] = {"concat_args", "numeric_arg"};
    static const char *const values_nested[] = {"0", "0"};
    static const char *const columns_row_status[] = {"ROW_COUNT()", "@@warning_count"};
    static const char *const values_after_select[] = {"-1", "0"};
    static const char *const values_after_do[] = {"0", "0"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += open_app_database(&database, "strcmp-no-source", path, sizeof(path));
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT STRCMP('text','text2') AS lt, STRCMP('text2','text') AS gt, "
                   "STRCMP('text','text') AS eq, STRCMP(NULL,'a') AS n1, "
                   "STRCMP('a',NULL) AS n2, STRCMP('abc','ABC') AS case_fold, "
                   "STRCMP('', '') AS empty0, STRCMP('', 'a') AS empty_lt, "
                   "STRCMP('a', '') AS empty_gt, STRCMP('a ', 'a') AS trail_gt, "
                   "STRCMP('a', 'a ') AS trail_lt, STRCMP(10, '2') AS num_lt, "
                   "STRCMP('9', 10) AS num_gt, STRCMP(TRUE, '1') AS bool_t, "
                   "STRCMP(FALSE, '0') AS bool_f, STRCMP(-1, '-2') AS negative, "
                   "@@warning_count AS warn",
            .columns = columns_no_source,
            .column_count = sizeof(columns_no_source) / sizeof(columns_no_source[0]),
            .values = values_no_source,
            .row_count = 1U,
            .context = "no-source strcmp values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT STRCMP ('b','a') AS c, STRCMP('b','a') FROM DUAL",
            .columns = columns_dual,
            .column_count = sizeof(columns_dual) / sizeof(columns_dual[0]),
            .values = values_dual,
            .row_count = 1U,
            .context = "dual strcmp whitespace",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT STRCMP(CONCAT('abc'), 'ABC') AS concat_args, "
                   "STRCMP(LOCATE('b','abc'), '2') AS numeric_arg",
            .columns = columns_nested,
            .column_count = sizeof(columns_nested) / sizeof(columns_nested[0]),
            .values = values_nested,
            .row_count = 1U,
            .context = "nested strcmp arguments",
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
            .context = "row count after strcmp select",
        }
    );

    failures += execute_ok(database, "DO STRCMP('x','x'), STRCMP(NULL,'a')", &result);
    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, "strcmp do columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "strcmp do rows");
        failures += expect_int64(mylite_result_affected_rows(result), 0, "strcmp do affected");
        failures += expect_size(mylite_result_warning_count(result), 0U, "strcmp do warnings");
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
            .context = "row count after strcmp do",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_strcmp_and_reopen(void) {
    static const char *const columns_table[] = {
        "id",
        "v_cmp",
        "text_cmp",
        "int_cmp",
        "year_cmp",
        "date_cmp",
    };
    static const char *const values_table[] = {
        "1",
        "0",
        "-1",
        "-1",
        "0",
        "0",
        "2",
        "0",
        "0",
        "0",
        "-1",
        "-1",
        "3",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
    };
    static const char *const columns_limited[] = {"id", "cmp"};
    static const char *const values_limited[] = {"3", NULL, "2", "0"};
    static const char *const columns_type_coverage[] = {
        "dec_cmp",
        "datetime_cmp",
        "timestamp_cmp",
    };
    static const char *const values_type_coverage[] = {"0", "0", "0"};
    static const char *const columns_reopen[] = {"id", "cmp"};
    static const char *const values_reopen[] = {"1", "0", "2", "0"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    mylite_file_preamble_init(expected_preamble);
    failures += open_app_database(&database, "strcmp-table", path, sizeof(path));
    failures += execute_ok(
        database,
        "CREATE TABLE s("
        "id INT, v VARCHAR(10), t TEXT, n INT, y YEAR, d DATE, dec_col DECIMAL(6,2), "
        "ti TIME, dt DATETIME, ts TIMESTAMP)",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO s VALUES "
        "(1, 'abc', 'alpha', 10, 2024, '2024-01-02', -12.30, '03:04:05', "
        "'2024-01-02 03:04:05', '2024-01-02 03:04:06'), "
        "(2, 'ABC', 'Beta', 2, 2000, '2000-02-03', NULL, NULL, NULL, NULL), "
        "(3, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL)",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, STRCMP(v, 'abc') AS v_cmp, "
                   "STRCMP(t, 'beta') AS text_cmp, STRCMP(n, '2') AS int_cmp, "
                   "STRCMP(y, '2024') AS year_cmp, STRCMP(d, '2024-01-02') AS date_cmp "
                   "FROM s ORDER BY id",
            .columns = columns_table,
            .column_count = sizeof(columns_table) / sizeof(columns_table[0]),
            .values = values_table,
            .row_count = 3U,
            .context = "table strcmp descriptor values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, STRCMP(v, 'abc') AS cmp FROM s WHERE id >= 1 "
                   "ORDER BY id DESC LIMIT 2",
            .columns = columns_limited,
            .column_count = sizeof(columns_limited) / sizeof(columns_limited[0]),
            .values = values_limited,
            .row_count = 2U,
            .context = "table strcmp row envelope",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT STRCMP(dec_col, '-12.30') AS dec_cmp, "
                   "STRCMP(dt, '2024-01-02 03:04:05') AS datetime_cmp, "
                   "STRCMP(ts, '2024-01-02 03:04:06') AS timestamp_cmp "
                   "FROM s WHERE id = 1",
            .columns = columns_type_coverage,
            .column_count = sizeof(columns_type_coverage) / sizeof(columns_type_coverage[0]),
            .values = values_type_coverage,
            .row_count = 1U,
            .context = "table strcmp descriptor type coverage",
        }
    );
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(actual_preamble),
        "strcmp preamble unchanged"
    );

    mylite_close(database);
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen strcmp file");
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, STRCMP(v, 'abc') AS cmp FROM s WHERE id <= 2 ORDER BY id",
            .columns = columns_reopen,
            .column_count = sizeof(columns_reopen) / sizeof(columns_reopen[0]),
            .values = values_reopen,
            .row_count = 2U,
            .context = "strcmp reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_string_search_diagnostics(void) {
    static const char *const columns_locate_position[] = {"loc"};
    static const char *const values_locate_position[] = {"1"};
    static const char *const columns_nested[] = {"find_nested", "strcmp_nested"};
    static const char *const values_nested[] = {"1", "0"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += execute_ok(
        database,
        "CREATE TABLE t(id INT, v VARCHAR(20), f FLOAT, b VARBINARY(10), ti TIME)",
        NULL
    );
    failures +=
        execute_ok(database, "INSERT INTO t VALUES (1, 'abc', 1.5, X'61', '03:04:05')", NULL);
    failures +=
        execute_ok(database, "INSERT INTO t VALUES (2, '\xC3\xA9', 1.5, X'62', NULL)", NULL);
    failures += execute_error(
        database,
        "SELECT LOCATE('a')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'LOCATE'",
        }
    );
    failures += execute_error(
        database,
        "SELECT LOCATE('a','abc',1,2)",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'LOCATE'",
        }
    );
    failures += execute_error(
        database,
        "SELECT INSTR('abc')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'INSTR'",
        }
    );
    failures += execute_error(
        database,
        "SELECT INSTR('abc','a','x')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'INSTR'",
        }
    );
    failures += execute_error(
        database,
        "SELECT POSITION ('a' IN 'abc')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near '('",
        }
    );
    failures += execute_error(
        database,
        "SELECT LOCATE('a', missing) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT LOCATE('a', f) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "string search functions do not support approximate numeric columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT LOCATE('a', b) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "string search functions do not support binary columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT LOCATE('a', v, id) AS loc FROM t WHERE id = 1",
            .columns = columns_locate_position,
            .column_count = sizeof(columns_locate_position) / sizeof(columns_locate_position[0]),
            .values = values_locate_position,
            .row_count = 1U,
            .context = "LOCATE integer column position",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT FIND_IN_SET(CONCAT(v), CONCAT(v, ',x')) AS find_nested, "
                   "STRCMP(CONCAT(v), 'ABC') AS strcmp_nested FROM t WHERE id = 1",
            .columns = columns_nested,
            .column_count = sizeof(columns_nested) / sizeof(columns_nested[0]),
            .values = values_nested,
            .row_count = 1U,
            .context = "nested search compare arguments on ASCII row",
        }
    );
    failures += execute_error(
        database,
        "SELECT LOCATE(LOCATE('a', v), v) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "string search functions support only ASCII text values",
        }
    );
    failures += execute_error(
        database,
        "SELECT LOCATE('\xC3\xA9', '\xC3\xA9')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "string search functions support only ASCII text values",
        }
    );
    failures += execute_error(
        database,
        "SELECT LOCATE('x', v) FROM t WHERE id = 2",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "string search functions support only ASCII text values",
        }
    );
    failures += execute_error(
        database,
        "UPDATE t SET v = LOCATE('a', v)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT LOCATE(X'61', 'abc')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "string search functions support only string",
        }
    );
    failures += execute_error(
        database,
        "SELECT LOCATE(?, 'abc')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT FIND_IN_SET()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part =
                "Incorrect parameter count in the call to native function 'FIND_IN_SET'",
        }
    );
    failures += execute_error(
        database,
        "SELECT FIND_IN_SET('a')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part =
                "Incorrect parameter count in the call to native function 'FIND_IN_SET'",
        }
    );
    failures += execute_error(
        database,
        "SELECT FIND_IN_SET('a','a','extra')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part =
                "Incorrect parameter count in the call to native function 'FIND_IN_SET'",
        }
    );
    failures += execute_error(
        database,
        "SELECT FIND_IN_SET('a', missing) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM t WHERE FIND_IN_SET('a', missing)",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'where clause'",
        }
    );
    failures += execute_error(
        database,
        "SELECT FIND_IN_SET('a', f) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "FIND_IN_SET() does not support approximate columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT FIND_IN_SET('a', b) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "FIND_IN_SET() does not support binary columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT FIND_IN_SET(LOCATE('a', v), v) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "string search functions support only ASCII text values",
        }
    );
    failures += execute_error(
        database,
        "SELECT FIND_IN_SET('\xC3\xA9', '\xC3\xA9')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "string search functions support only ASCII text values",
        }
    );
    failures += execute_error(
        database,
        "SELECT FIND_IN_SET('a', v) FROM t WHERE id = 2",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "string search functions support only ASCII text values",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM t WHERE FIND_IN_SET('a', v) = '1'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM t ORDER BY FIND_IN_SET('a', v)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "UPDATE t SET v = FIND_IN_SET('a', v)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT FIND_IN_SET(X'61', 'abc')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "FIND_IN_SET() supports only string",
        }
    );
    failures += execute_error(
        database,
        "SELECT FIND_IN_SET(?, 'abc')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT STRCMP()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'STRCMP'",
        }
    );
    failures += execute_error(
        database,
        "SELECT STRCMP('a')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'STRCMP'",
        }
    );
    failures += execute_error(
        database,
        "SELECT STRCMP('a','b','c')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'STRCMP'",
        }
    );
    failures += execute_error(
        database,
        "SELECT STRCMP('a', missing) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT STRCMP('a', f) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "STRCMP() does not support approximate columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT STRCMP('a', b) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "STRCMP() does not support binary columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT STRCMP('a', ti) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "STRCMP() does not support TIME columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT STRCMP(LOCATE('a', v), v) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "string search functions support only ASCII text values",
        }
    );
    failures += execute_error(
        database,
        "SELECT STRCMP('\xC3\xA9', '\xC3\xA9')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "string search functions support only ASCII text values",
        }
    );
    failures += execute_error(
        database,
        "SELECT STRCMP('a', v) FROM t WHERE id = 2",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "string search functions support only ASCII text values",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM t WHERE STRCMP('a', v) = 0",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM t ORDER BY STRCMP('a', v)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "UPDATE t SET v = STRCMP('a', v)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT STRCMP(X'61', 'abc')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "STRCMP() supports only string",
        }
    );
    failures += execute_error(
        database,
        "SELECT STRCMP(?, 'abc')",
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
        "/tmp/mylite-string-search-functions-%s-%d.mylite",
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
        fprintf(stderr, "%s: bytes differ\n", context);
        return 1;
    }
    return 0;
}
