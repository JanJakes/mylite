#include "parser_test_support.h"

static int test_set_expression_values(void);
static int test_predicate_expression_values(void);
static int test_dml_expression_values(void);
static int test_match_against_forms(void);
static int test_rollup_marker(void);
static int test_fulltext_rollup_words_remain_identifiers(void);
static int parse_ok(const char *sql);

int main(void) {
    int failures = 0;

    failures += test_set_expression_values();
    failures += test_predicate_expression_values();
    failures += test_dml_expression_values();
    failures += test_match_against_forms();
    failures += test_rollup_marker();
    failures += test_fulltext_rollup_words_remain_identifiers();

    return failures == 0 ? 0 : 1;
}

static int test_set_expression_values(void) {
    static const char *const forms[] = {
        "SET @a = ST_AsText(ST_GeomFromText('POINT(1 1)'))",
        "SET @a := LOG10(0.0) + 1",
        "SET @a = CONVERT(@b USING utf8mb4)",
        "SET @a = (SELECT COUNT(*) FROM t1)",
        "SET TIMESTAMP = UNIX_TIMESTAMP('2019-03-11 12:00:00')",
    };
    int failures = 0;

    for (size_t index = 0U; index < sizeof(forms) / sizeof(forms[0]); ++index) {
        failures += parse_ok(forms[index]);
    }

    return failures;
}

static int test_predicate_expression_values(void) {
    static const char *const forms[] = {
        "SELECT * FROM t1 WHERE id = 1.1",
        "SELECT * FROM t1 WHERE id BETWEEN 1.1 AND 1.9",
        "SELECT * FROM t1 WHERE COALESCE(a, 0) IN (0.8, 0.9)",
    };
    int failures = 0;

    for (size_t index = 0U; index < sizeof(forms) / sizeof(forms[0]); ++index) {
        failures += parse_ok(forms[index]);
    }

    return failures;
}

static int test_dml_expression_values(void) {
    static const char *const forms[] = {
        "INSERT INTO t1 VALUES (ST_GeomFromText('POINT(10 10)'))",
        "INSERT INTO t1 VALUES ((ST_PointFromText('POINT(10 10)')))",
        "INSERT INTO t1 VALUES (~0, -1 | 0, ROW(1, 2))",
        "INSERT INTO t1 SET g = ST_GeomFromText('POINT(1 1)')",
        "REPLACE INTO t1 VALUES (POINT(1, 1), ST_SRID(ST_GeomFromText('POINT(0 0)')))",
        "UPDATE t1 SET g = ST_GeomFromText('POINT(1 1)') WHERE id = 1",
        "UPDATE t1 SET n = ~0 WHERE id = 1",
        "INSERT INTO t1 VALUES (1) ON DUPLICATE KEY UPDATE g = ST_PointFromText('POINT(0 0)')",
    };
    int failures = 0;

    for (size_t index = 0U; index < sizeof(forms) / sizeof(forms[0]); ++index) {
        failures += parse_ok(forms[index]);
    }

    return failures;
}

static int test_match_against_forms(void) {
    static const char *const forms[] = {
        "SELECT MATCH(title) AGAINST ('needle') FROM articles",
        "SELECT MATCH(title, body) AGAINST ('database' IN NATURAL LANGUAGE MODE) FROM articles",
        "SELECT MATCH(title) AGAINST ('+mysql -sqlite' IN BOOLEAN MODE) FROM articles",
        "SELECT MATCH(title) AGAINST ('mysql' WITH QUERY EXPANSION) FROM articles",
        ("SELECT MATCH(title) AGAINST ('mysql' IN NATURAL LANGUAGE MODE WITH QUERY EXPANSION) "
         "FROM articles"),
        "SELECT * FROM articles WHERE MATCH(title) AGAINST ('needle')",
        "SELECT * FROM articles WHERE MATCH(title) AGAINST ('needle') > 0",
    };
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *function = NULL;
    int failures = 0;

    for (size_t index = 0U; index < sizeof(forms) / sizeof(forms[0]); ++index) {
        failures += parse_ok(forms[index]);
    }

    failures += parser_test_parse_sql(forms[0], MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(statement, 0U);
    function = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        function,
        MYLITE_SQL_AST_GENERIC_FUNCTION,
        "MATCH AGAINST placeholder function"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_rollup_marker(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *group_clause = NULL;
    int failures = parser_test_parse_sql(
        "SELECT region, SUM(amount) FROM sales GROUP BY region WITH ROLLUP",
        MYLITE_SQL_PARSE_OK,
        &result
    );

    statement = parser_test_child_at(result.root, 0U);
    group_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_GROUP_BY_CLAUSE);
    failures += parser_test_expect_node(group_clause, MYLITE_SQL_AST_GROUP_BY_CLAUSE, "GROUP BY");
    failures += parser_test_expect_child_count(group_clause, 2U, "ROLLUP marker child count");
    failures += parser_test_expect_node(
        parser_test_child_at(group_clause, 1U),
        MYLITE_SQL_AST_GROUP_BY_ROLLUP_MODIFIER,
        "ROLLUP marker"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_fulltext_rollup_words_remain_identifiers(void) {
    static const char *const forms[] = {
        "CREATE TABLE keyword_ids (against INT, expansion INT, language INT, query INT, rollup "
        "INT)",
        "SELECT against, expansion, language, query, rollup FROM keyword_ids",
    };
    int failures = 0;

    for (size_t index = 0U; index < sizeof(forms) / sizeof(forms[0]); ++index) {
        failures += parse_ok(forms[index]);
    }

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
