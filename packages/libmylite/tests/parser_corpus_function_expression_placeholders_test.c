#include "parser_test_support.h"

#include <stddef.h>

struct expected_statement {
    const char *sql;
    enum mylite_sql_ast_node_kind kind;
};

static int test_function_expression_placeholders(void);
static int expect_statement_kind(struct expected_statement expected);
static int expect_syntax_error(const char *sql);
static int parse_ok(const char *sql);

int main(void) {
    int failures = 0;

    failures += test_function_expression_placeholders();

    return failures == 0 ? 0 : 1;
}

static int test_function_expression_placeholders(void) {
    static const struct expected_statement placeholders[] = {
        {.sql = "SELECT HEX(WEIGHT_STRING('a' AS CHAR(1)))",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT COUNT(DISTINCT a) FROM t1 GROUP BY b "
                "HAVING COUNT(DISTINCT a) > 1",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT latin1_f FROM t1 ORDER BY latin1_f, HEX(latin1_f)",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT b FROM t1 GROUP BY CAST(b AS BINARY) LIKE ''",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT * FROM t1 WHERE word = CAST(0xDF AS CHAR)",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT * FROM JSON_TABLE('[]', '$[*]' COLUMNS (p NCHAR PATH '$.a')) AS jt",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "INSERT INTO t1 VALUES (DATE_FORMAT('2004-02-02','%M'))",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "UPDATE t1 SET a = DATE_ADD(NULL, INTERVAL 1 DAY)",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT mysqltest.f1()", .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
    };
    int failures = 0;

    failures += parse_ok("SELECT ABS(-1)");
    failures += parse_ok("SELECT DATE_ADD('2024-01-01', INTERVAL 1 DAY)");
    failures +=
        parse_ok("SELECT GROUP_CONCAT(name ORDER BY name SEPARATOR ',') FROM t1 GROUP BY grp");
    failures += expect_syntax_error("SELECT f(1,,2)");
    failures += expect_syntax_error("SELECT ABS(1) +");
    failures += expect_syntax_error("SELECT ABS(1) FROM");
    failures += expect_syntax_error("SELECT LOCATE(?, 'abc')");
    failures += expect_syntax_error("SELECT ROW_NUMBER()");
    failures += expect_syntax_error("VALUES ROW(1) WHERE TRUE");
    failures += expect_syntax_error("DELETE FROM t2 WHERE fld3 = 'd%' ORDER BY");
    failures += parse_ok("DELETE FROM t2 WHERE fld3 = 'd%' ORDER BY RAND()");
    failures += parse_ok("SELECT * FROM t1 WHERE a = IF(b < 10, _ucs2 0x0061, _ucs2 0x0062)");

    for (size_t index = 0U; index < sizeof(placeholders) / sizeof(placeholders[0]); ++index) {
        failures += expect_statement_kind(placeholders[index]);
    }
    return failures;
}

static int expect_statement_kind(struct expected_statement expected) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(expected.sql, MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(statement, expected.kind, expected.sql);
    failures += parser_test_expect_child_count(statement, 0U, expected.sql);
    mylite_sql_parse_result_deinit(&result);
    return failures;
}

static int expect_syntax_error(const char *sql) {
    struct mylite_sql_parse_result result;
    int failures = parser_test_parse_sql(sql, MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);

    mylite_sql_parse_result_deinit(&result);
    return failures;
}

static int parse_ok(const char *sql) {
    struct mylite_sql_parse_result result;
    int failures = parser_test_parse_sql(sql, MYLITE_SQL_PARSE_OK, &result);

    if (failures != 0) {
        (void)parser_test_expect_true(0, sql);
    }
    mylite_sql_parse_result_deinit(&result);
    return failures;
}
