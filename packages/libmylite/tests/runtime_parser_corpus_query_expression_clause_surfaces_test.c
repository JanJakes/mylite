#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdio.h>
#include <string.h>

enum {
    mysql_error_parse = 1064,
    mysql_error_operand_should_contain_one_column = 1241,
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

static int test_query_expression_clause_surfaces(void);
static int execute_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query_values(mylite_db *database, struct expected_query query);

int main(void) {
    return test_query_expression_clause_surfaces() == 0 ? 0 : 1;
}

static int test_query_expression_clause_surfaces(void) {
    static const char *const simple_predicate_rows[] = {"1"};
    static const char *const arithmetic_predicate_count_rows[] = {"2"};
    static const char *const nested_arithmetic_predicate_count_rows[] = {"1"};
    static const char *const function_order_key_rows[] = {"3", "1"};
    static const char *const values_order_key_rows[] = {"1", "2"};
    static const char *const subquery_arithmetic_predicate_rows[] = {"1", "3"};
    static const char *const comparison_result_known_rows[] = {"2"};
    static const char *const comparison_result_unknown_rows[] = {"1"};
    static const char *const comparison_result_never_unknown_rows[] = {"3"};
    static const char *const comparison_result_dml_rows[] = {"1"};
    static const char *const comparison_result_delete_rows[] = {"3"};
    static const char *const literal_left_decimal_count_rows[] = {"1"};
    static const char *const literal_left_datetime_le_count_rows[] = {"2"};
    static const char *const literal_left_datetime_lt_count_rows[] = {"1"};
    static const char *const literal_left_datetime_ge_count_rows[] = {"1"};
    static const char *const literal_left_datetime_gt_count_rows[] = {"1"};
    static const char *const literal_left_decimal_ne_count_rows[] = {"1"};
    static const char *const literal_left_dotted_date_count_rows[] = {"1"};
    static const char *const literal_left_between_datetime_rows[] = {
        "2001-04-10 12:34:56",
        "2001-03-01 00:00:00",
    };
    static const char *const literal_left_not_between_datetime_count_rows[] = {"0"};
    static const char *const literal_left_between_descriptor_rows[] = {
        "2001-03-01 00:00:00",
        "2001-03-20",
    };
    static const char *const literal_left_in_count_rows[] = {"4"};
    static const char *const literal_left_not_in_count_rows[] = {"0"};
    static const char *const row_constructor_equal_count_rows[] = {"1"};
    static const char *const row_constructor_null_safe_count_rows[] = {"1"};
    static const char *const row_constructor_not_equal_count_rows[] = {"2"};
    static const char *const row_constructor_null_not_equal_count_rows[] = {"1"};
    static const char *const row_constructor_null_safe_null_count_rows[] = {"0"};
    static const char *const row_constructor_parenthesized_not_equal_count_rows[] = {"1"};
    static const char *const row_constructor_order_strict_count_rows[] = {"1"};
    static const char *const row_constructor_order_inclusive_count_rows[] = {"2"};
    static const char *const row_constructor_null_equal_count_rows[] = {"0"};
    static const char *const row_constructor_dml_rows[] = {"2"};
    static const char *const row_constructor_tuple_in_count_rows[] = {"1"};
    static const char *const row_constructor_tuple_in_all_count_rows[] = {"2"};
    static const char *const row_constructor_tuple_not_in_count_rows[] = {"1"};
    static const char *const row_constructor_tuple_null_not_in_none_count_rows[] = {"0"};
    static const char *const row_constructor_dml_in_rows[] = {"2"};
    mylite_db *database = NULL;
    int failures = 0;

    failures += mylite_test_expect_int(
        mylite_test_open_temporary(&database),
        MYLITE_OK,
        "open transient database"
    );
    if (failures != 0) {
        return failures;
    }

    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    failures += execute_ok(database, "CREATE TABLE t1 (a INT, b INT, c VARCHAR(20))");
    failures += execute_ok(database, "CREATE TABLE t_tuple (a INT, b INT, c VARCHAR(20))");
    failures += execute_ok(database, "CREATE TABLE t_dml (a INT, b INT, c VARCHAR(20))");
    failures += execute_ok(database, "CREATE TABLE t_dml_in (a INT, b INT, c VARCHAR(20))");
    failures += execute_ok(database, "CREATE TABLE t2 (a INT, b INT)");
    failures += execute_ok(database, "CREATE TABLE t (u INT)");
    failures += execute_ok(
        database,
        "CREATE TABLE t_dates (f1 DATE, f2 DATETIME, f3 DATE, a DATETIME, "
        "value DECIMAL(30,0))"
    );
    failures += execute_ok(database, "CREATE TABLE v1 (f1 DATE)");
    failures += execute_ok(database, "INSERT INTO t1 VALUES (1, 2, 'x'), (3, 4, 'y')");
    failures +=
        execute_ok(database, "INSERT INTO t_tuple VALUES (1, 2, 'x'), (1, NULL, 'n'), (3, 4, 'y')");
    failures +=
        execute_ok(database, "INSERT INTO t_dml VALUES (1, 2, 'x'), (3, 4, 'y'), (5, 6, 'z')");
    failures +=
        execute_ok(database, "INSERT INTO t_dml_in VALUES (1, 2, 'x'), (3, 4, 'y'), (5, 6, 'z')");
    failures += execute_ok(database, "INSERT INTO t2 VALUES (1, 20), (3, 40)");
    failures += execute_ok(database, "INSERT INTO t VALUES (256), (257), (NULL)");
    failures += execute_ok(
        database,
        "INSERT INTO t_dates VALUES "
        "('2001-01-01','2001-04-10 12:34:56','2001-05-01',"
        "'2010-02-01 09:31:02',100000000000000000000002),"
        "('2001-01-01','2001-03-01 00:00:00','2001-03-20',"
        "'2010-02-02 00:00:00',5)"
    );
    failures += execute_ok(database, "INSERT INTO v1 VALUES ('2005-02-02')");

    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT a FROM t1 WHERE a = 1",
            .values = simple_predicate_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "existing simple predicate support",
        }
    );

    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t1 WHERE a + 1 > 1",
            .values = arithmetic_predicate_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "row arithmetic predicate support",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t1 WHERE (a + 1) > 1",
            .values = arithmetic_predicate_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "parenthesized row arithmetic predicate support",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t1 WHERE ((a + 1) * 2) > 4",
            .values = nested_arithmetic_predicate_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "nested row arithmetic predicate support",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t1 WHERE ((a + 1) * 2) > 4 AND "
                   "((b + 1) * 2) > 8",
            .values = nested_arithmetic_predicate_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "multiple nested row arithmetic predicate support",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t1 WHERE a = b - 1",
            .values = arithmetic_predicate_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "row arithmetic comparison value support",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t1 WHERE a = ABS(b - 1)",
            .values = arithmetic_predicate_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "numeric function arithmetic comparison value support",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t1 WHERE a BETWEEN b - 1 AND b + 1",
            .values = arithmetic_predicate_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "row arithmetic between value support",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t1 WHERE a IN (b - 1, 0)",
            .values = arithmetic_predicate_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "row arithmetic in-list value support",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT a FROM t1 ORDER BY ABS(b - 5)",
            .values = function_order_key_rows,
            .column_count = 1U,
            .row_count = 2U,
            .context = "numeric function order key",
        }
    );
    failures += execute_error(
        database,
        "SELECT a, COUNT(*) FROM t1 GROUP BY a + 0 "
        "HAVING COUNT(*) >= 1 AND a > 0 ORDER BY a + 0",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT a FROM t1 WHERE (a,b) = (1,2)",
            .values = simple_predicate_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "parenthesized row tuple equality predicate support",
        }
    );
    failures += execute_error(
        database,
        "SELECT * FROM t1 JOIN t2 ON ROW(1,2)=ROW(t1.a,t2.b)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t WHERE u=256 IS NOT NULL",
            .values = comparison_result_known_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "comparison-result IS NOT NULL predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t WHERE u=256 IS UNKNOWN",
            .values = comparison_result_unknown_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "comparison-result IS UNKNOWN predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t WHERE u=256 IS NULL",
            .values = comparison_result_unknown_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "comparison-result IS NULL predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t WHERE u=256 IS NOT UNKNOWN",
            .values = comparison_result_known_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "comparison-result IS NOT UNKNOWN predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t WHERE u > 256 IS UNKNOWN",
            .values = comparison_result_unknown_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "range comparison-result IS UNKNOWN predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t WHERE u <=> NULL IS NOT UNKNOWN",
            .values = comparison_result_never_unknown_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "null-safe comparison-result IS NOT UNKNOWN predicate",
        }
    );
    failures += execute_ok(database, "UPDATE t SET u = 300 WHERE u=256 IS UNKNOWN");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t WHERE u = 300",
            .values = comparison_result_dml_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "comparison-result IS UNKNOWN update predicate",
        }
    );
    failures += execute_ok(database, "INSERT INTO t VALUES (NULL)");
    failures += execute_ok(database, "DELETE FROM t WHERE u=256 IS UNKNOWN");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t",
            .values = comparison_result_delete_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "comparison-result IS UNKNOWN delete predicate",
        }
    );
    failures += execute_error(
        database,
        "SELECT * FROM t1 WHERE f1->\"$.id\"= 5",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT * FROM t1 WHERE f1->>\"$.name\" = \"James\"",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT {fn CONCAT(a1,a2)} FROM t1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "UPDATE t3 SET a4={d '1789-07-14'} WHERE a1=0",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT a, COUNT(*) FROM t1 GROUP BY a HAVING a = b - 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT t1.a FROM t1 JOIN t2 ON t1.a = t2.b - 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT * FROM t1 LEFT JOIN t2 ON t1.a = t2.a WHERE t1.a BETWEEN t2.b AND t1.b",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT * FROM t1 LEFT JOIN t2 ON t1.a = t2.a WHERE t1.a IN(t2.a, t2.b)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t1 WHERE ROW(1,2,'x')=ROW(a,b,c)",
            .values = row_constructor_equal_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "row constructor equality predicate support",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t1 WHERE ROW(1,2,'x')<=>ROW(a,b,c)",
            .values = row_constructor_null_safe_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "row constructor null-safe equality predicate support",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t1 WHERE ROW(1,2,'z')<>ROW(a,b,c)",
            .values = row_constructor_not_equal_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "row constructor inequality predicate support",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t1 WHERE ROW(1,2,'z')!=ROW(a,b,c)",
            .values = row_constructor_not_equal_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "row constructor bang inequality predicate support",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t1 WHERE ROW(1,2,NULL)<>ROW(a,b,c)",
            .values = row_constructor_null_not_equal_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "row constructor NULL inequality predicate support",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t1 WHERE ROW(1,2,NULL)<=>ROW(a,b,c)",
            .values = row_constructor_null_safe_null_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "row constructor null-safe NULL predicate support",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t1 WHERE (a,b) <> (1,2)",
            .values = row_constructor_parenthesized_not_equal_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "parenthesized row tuple inequality predicate support",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t1 WHERE ROW(a,b) > ROW(1,2)",
            .values = row_constructor_order_strict_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "row constructor greater predicate support",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t1 WHERE ROW(a,b) >= ROW(1,2)",
            .values = row_constructor_order_inclusive_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "row constructor greater-equal predicate support",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t1 WHERE ROW(a,b) < ROW(3,4)",
            .values = row_constructor_order_strict_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "row constructor less predicate support",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t1 WHERE (a,b) <= (3,4)",
            .values = row_constructor_order_inclusive_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "parenthesized row tuple less-equal predicate support",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t1 WHERE ROW(1,3) > ROW(a,b)",
            .values = row_constructor_order_strict_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "literal-left row constructor order predicate support",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t_tuple WHERE ROW(a,b) <=> ROW(1,NULL)",
            .values = row_constructor_null_safe_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "row constructor NULL null-safe comparison support",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t_tuple WHERE ROW(a,b) = ROW(1,NULL)",
            .values = row_constructor_null_equal_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "row constructor NULL equality predicate support",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t_tuple WHERE ROW(a,b) <> ROW(1,NULL)",
            .values = row_constructor_order_strict_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "row constructor NULL inequality predicate support",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t_tuple WHERE ROW(a,b) > ROW(1,2)",
            .values = row_constructor_order_strict_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "row constructor NULL order predicate support",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t_tuple WHERE ROW(a,b) <= ROW(1,NULL)",
            .values = row_constructor_null_equal_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "row constructor NULL inclusive order predicate support",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t1 WHERE (a,b) IN ((1,2),(9,9))",
            .values = row_constructor_tuple_in_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "parenthesized row tuple IN predicate support",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t1 WHERE ROW(a,b) IN (ROW(1,2), ROW(3,4))",
            .values = row_constructor_tuple_in_all_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "row constructor IN predicate support",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t1 WHERE (a,b) NOT IN ((1,2),(9,9))",
            .values = row_constructor_tuple_not_in_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "parenthesized row tuple NOT IN predicate support",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t1 WHERE ROW(1,2) IN (ROW(a,b))",
            .values = row_constructor_tuple_in_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "literal-left row constructor IN predicate support",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t_tuple WHERE ROW(a,b) IN (ROW(1,NULL), ROW(3,4))",
            .values = row_constructor_tuple_in_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "row constructor NULL IN predicate support",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t_tuple WHERE ROW(a,b) NOT IN (ROW(1,NULL), ROW(9,9))",
            .values = row_constructor_tuple_not_in_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "row constructor NULL NOT IN predicate support",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t_tuple WHERE (a,b) NOT IN ((1,NULL),(3,4))",
            .values = row_constructor_tuple_null_not_in_none_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "parenthesized row tuple NULL NOT IN filtering support",
        }
    );
    failures +=
        execute_ok(database, "UPDATE t_dml_in SET c = 'tuple-in' WHERE (a,b) IN ((1,2),(5,6))");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t_dml_in WHERE c = 'tuple-in'",
            .values = row_constructor_dml_in_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "row constructor IN UPDATE predicate support",
        }
    );
    failures +=
        execute_ok(database, "DELETE FROM t_dml_in WHERE ROW(a,b) NOT IN (ROW(3,4), ROW(5,6))");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t_dml_in",
            .values = row_constructor_dml_in_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "row constructor NOT IN DELETE predicate support",
        }
    );
    failures += execute_ok(database, "UPDATE t_dml SET c = 'hit' WHERE (a,b) >= (3,4)");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t_dml WHERE c = 'hit'",
            .values = row_constructor_dml_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "row constructor UPDATE predicate support",
        }
    );
    failures += execute_ok(database, "DELETE FROM t_dml WHERE ROW(a,b) < ROW(3,4)");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t_dml",
            .values = row_constructor_dml_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "row constructor DELETE predicate support",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(*) FROM t1 WHERE ROW(1,2)=ROW(a,b,c)",
        (struct expected_sql_error){
            .code = mysql_error_operand_should_contain_one_column,
            .sqlstate = "21000",
            .message_part = "Operand should contain 2 column(s)",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(*) FROM t1 WHERE (a,b) IN ((1,2,3))",
        (struct expected_sql_error){
            .code = mysql_error_operand_should_contain_one_column,
            .sqlstate = "21000",
            .message_part = "Operand should contain 2 column(s)",
        }
    );
    failures += execute_error(
        database,
        "SELECT x FROM t GROUP BY x, MATCH(x) AGAINST ('abc') "
        "HAVING MATCH(x) AGAINST ('abc')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "VALUES ROW(1),ROW(2) ORDER BY '1' DESC",
            .values = values_order_key_rows,
            .column_count = 1U,
            .row_count = 2U,
            .context = "values string order key support",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT a FROM t1 WHERE a IN (SELECT a FROM t2 WHERE b + 1 > 20) ORDER BY a",
            .values = subquery_arithmetic_predicate_rows,
            .column_count = 1U,
            .row_count = 2U,
            .context = "subquery row arithmetic predicate support",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT a FROM t1 WHERE a IN (SELECT a FROM t2 WHERE (b + 1) > 20) ORDER BY a",
            .values = subquery_arithmetic_predicate_rows,
            .column_count = 1U,
            .row_count = 2U,
            .context = "subquery parenthesized row arithmetic predicate support",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT a FROM t1 WHERE a IN (SELECT a FROM t2 WHERE ((b + 1) * 2) > 40) "
                   "ORDER BY a",
            .values = subquery_arithmetic_predicate_rows,
            .column_count = 1U,
            .row_count = 2U,
            .context = "subquery nested row arithmetic predicate support",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT f2 FROM t_dates "
                   "WHERE '2001-04-10 12:34:56' BETWEEN f2 AND '01-05-01'",
            .values = literal_left_between_datetime_rows,
            .column_count = 1U,
            .row_count = 2U,
            .context = "literal-left string BETWEEN datetime and string bound",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t_dates "
                   "WHERE '2001-04-10 12:34:56' NOT BETWEEN f2 AND '01-05-01'",
            .values = literal_left_not_between_datetime_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "literal-left string NOT BETWEEN datetime and string bound",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT f2, f3 FROM t_dates WHERE '01-03-10' BETWEEN f2 AND f3",
            .values = literal_left_between_descriptor_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "literal-left string BETWEEN descriptor bounds",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t_dates,t2 WHERE '01-01-01' IN (f1, '01-02-03')",
            .values = literal_left_in_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "literal-left string IN descriptor list",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t_dates,t2 "
                   "WHERE '01-01-01' NOT IN (f1, '01-02-03')",
            .values = literal_left_not_in_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "literal-left string NOT IN descriptor list",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t_dates WHERE '100000000000000000000002' = value",
            .values = literal_left_decimal_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "literal-left string decimal equality predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t_dates WHERE '2010-02-01 09:31:02.0' <= a",
            .values = literal_left_datetime_le_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "literal-left string datetime less-or-equal predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t_dates WHERE '2010-02-01 09:31:02.0' < a",
            .values = literal_left_datetime_lt_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "literal-left string datetime less-than predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t_dates WHERE '2010-02-01 09:31:02.0' >= a",
            .values = literal_left_datetime_ge_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "literal-left string datetime greater-or-equal predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t_dates WHERE '2010-02-02 00:00:00.0' > a",
            .values = literal_left_datetime_gt_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "literal-left string datetime greater-than predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t_dates WHERE '5' <> value",
            .values = literal_left_decimal_ne_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "literal-left string decimal not-equal predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM v1 WHERE '2005.02.02'=f1",
            .values = literal_left_dotted_date_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "literal-left dotted date equality predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM v1 WHERE '2005.02.02'<=>f1",
            .values = literal_left_dotted_date_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "literal-left dotted date null-safe equality predicate",
        }
    );
    failures += execute_error(
        database,
        "SELECT 1 FROM t1 GROUP BY @b := @a, @b",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "select `foo` ()",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT DISTINCT t2.col_int_key FROM t1 LEFT JOIN t2 "
        "ON t1.col_varchar_10 = t2.col_varchar_10_key "
        "WHERE t2.pk ORDER BY t2.col_int_key",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "WITH qn AS (SELECT 1) SELECT * FROM qn",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "WITH RECURSIVE qn(n) AS (SELECT 1 UNION ALL SELECT n + 1 FROM qn WHERE n < 3) "
        "SELECT * FROM qn",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "WITH cte AS (SELECT 1) (SELECT * FROM cte) LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "WITH ids AS (SELECT id FROM t1) UPDATE t1 SET b = 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "WITH doomed AS (SELECT id FROM t1) DELETE FROM t1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "UPDATE t1, t2 SET t1.b = t1.b + 1 WHERE t1.a = t2.a",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "DELETE FROM t1 WHERE a = a + sleep(0) ORDER BY a LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );

    mylite_close(database);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: expected success, got %d: %s\n", sql, rc, mylite_errmsg(database));
        mylite_result_free(result);
        return 1;
    }
    mylite_result_free(result);
    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    failures += mylite_test_expect_int(rc, MYLITE_ERROR, sql);
    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, "error code");
    failures +=
        mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, "error sqlstate");
    failures += mylite_test_expect_contains(
        mylite_errmsg(database),
        expected.message_part,
        "error message"
    );
    if (result != NULL) {
        failures += mylite_test_expect_size(
            mylite_result_column_count(result),
            0U,
            "failed result columns"
        );
        failures +=
            mylite_test_expect_size(mylite_result_row_count(result), 0U, "failed result rows");
    }
    mylite_result_free(result);
    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, query.sql, strlen(query.sql), &result);
    int failures = 0;

    failures += mylite_test_expect_int(rc, MYLITE_OK, query.context);
    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: %s\n", query.sql, mylite_errmsg(database));
        mylite_result_free(result);
        return failures;
    }
    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        query.column_count,
        query.context
    );
    failures +=
        mylite_test_expect_size(mylite_result_row_count(result), query.row_count, query.context);
    if (failures == 0 && query.values != NULL) {
        for (size_t row = 0U; row < query.row_count; ++row) {
            for (size_t column = 0U; column < query.column_count; ++column) {
                failures += mylite_test_expect_text(
                    mylite_result_value_text(result, row, column),
                    query.values[(row * query.column_count) + column],
                    query.context
                );
            }
        }
    }
    mylite_result_free(result);
    return failures;
}
