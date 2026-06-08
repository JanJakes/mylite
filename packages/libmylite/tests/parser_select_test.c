#include "parser_test_support.h"

static int test_select_where_predicates(void);
static int test_select_order_limit_clauses(void);
static int test_select_group_by_clause(void);
static int test_select_distinct_clause(void);
static int test_select_sql_calc_found_rows_clause(void);
static int test_select_noop_modifier_clause(void);
static int test_select_locking_clause(void);
static int test_select_all_clause(void);
static int test_select_union_clause(void);
static int test_select_with_information_schema_union_clause(void);
static int test_table_statement(void);
static int test_values_statement(void);
static int test_select_table_alias_clause(void);
static int test_select_inner_join_clause(void);
static int test_select_item_alias_clause(void);

int main(void) {
    int failures = 0;

    failures += test_select_where_predicates();
    failures += test_select_order_limit_clauses();
    failures += test_select_group_by_clause();
    failures += test_select_distinct_clause();
    failures += test_select_sql_calc_found_rows_clause();
    failures += test_select_noop_modifier_clause();
    failures += test_select_locking_clause();
    failures += test_select_all_clause();
    failures += test_select_union_clause();
    failures += test_select_with_information_schema_union_clause();
    failures += test_table_statement();
    failures += test_values_statement();
    failures += test_select_table_alias_clause();
    failures += test_select_inner_join_clause();
    failures += test_select_item_alias_clause();

    return failures == 0 ? 0 : 1;
}

static int test_select_where_predicates(void) {
    static const char *const in_predicate_values[] = {"-2", "+1", "0x41", "NULL", "TRUE", "FALSE"};

    static const struct {
        const char *sql;
        enum mylite_sql_ast_operator expected_operator;
        enum mylite_sql_ast_node_kind expected_kind;
    } cases[] = {
        {
            "SELECT id FROM simple_lifecycle WHERE id = 1;",
            MYLITE_SQL_AST_OPERATOR_EQUAL,
            MYLITE_SQL_AST_COMPARISON_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id <=> 1;",
            MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL,
            MYLITE_SQL_AST_COMPARISON_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id <> 1;",
            MYLITE_SQL_AST_OPERATOR_NOT_EQUAL,
            MYLITE_SQL_AST_COMPARISON_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id != 1;",
            MYLITE_SQL_AST_OPERATOR_NOT_EQUAL,
            MYLITE_SQL_AST_COMPARISON_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id < -1;",
            MYLITE_SQL_AST_OPERATOR_LESS,
            MYLITE_SQL_AST_COMPARISON_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id <= +1;",
            MYLITE_SQL_AST_OPERATOR_LESS_EQUAL,
            MYLITE_SQL_AST_COMPARISON_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id > 1;",
            MYLITE_SQL_AST_OPERATOR_GREATER,
            MYLITE_SQL_AST_COMPARISON_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id >= 1;",
            MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL,
            MYLITE_SQL_AST_COMPARISON_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id LIKE '1%';",
            MYLITE_SQL_AST_OPERATOR_LIKE,
            MYLITE_SQL_AST_COMPARISON_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id LIKE BINARY '1%';",
            MYLITE_SQL_AST_OPERATOR_LIKE_BINARY,
            MYLITE_SQL_AST_COMPARISON_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id LIKE BINARY NULL;",
            MYLITE_SQL_AST_OPERATOR_LIKE_BINARY,
            MYLITE_SQL_AST_COMPARISON_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id REGEXP '^1$';",
            MYLITE_SQL_AST_OPERATOR_REGEXP,
            MYLITE_SQL_AST_COMPARISON_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id RLIKE '^1$';",
            MYLITE_SQL_AST_OPERATOR_RLIKE,
            MYLITE_SQL_AST_COMPARISON_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id = TRUE;",
            MYLITE_SQL_AST_OPERATOR_EQUAL,
            MYLITE_SQL_AST_COMPARISON_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id <=> false;",
            MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL,
            MYLITE_SQL_AST_COMPARISON_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id <=> NULL;",
            MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL,
            MYLITE_SQL_AST_COMPARISON_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id = NULL;",
            MYLITE_SQL_AST_OPERATOR_EQUAL,
            MYLITE_SQL_AST_COMPARISON_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id = 0x41;",
            MYLITE_SQL_AST_OPERATOR_EQUAL,
            MYLITE_SQL_AST_COMPARISON_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id IS NULL;",
            MYLITE_SQL_AST_OPERATOR_IS_NULL,
            MYLITE_SQL_AST_IS_NULL_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id IS NOT NULL;",
            MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL,
            MYLITE_SQL_AST_IS_NULL_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id IS TRUE;",
            MYLITE_SQL_AST_OPERATOR_IS_TRUE,
            MYLITE_SQL_AST_IS_BOOLEAN_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id IS NOT TRUE;",
            MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE,
            MYLITE_SQL_AST_IS_BOOLEAN_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id IS FALSE;",
            MYLITE_SQL_AST_OPERATOR_IS_FALSE,
            MYLITE_SQL_AST_IS_BOOLEAN_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id IS NOT FALSE;",
            MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE,
            MYLITE_SQL_AST_IS_BOOLEAN_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id IS UNKNOWN;",
            MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN,
            MYLITE_SQL_AST_IS_BOOLEAN_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id IS NOT UNKNOWN;",
            MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN,
            MYLITE_SQL_AST_IS_BOOLEAN_PREDICATE,
        },
    };
    struct mylite_sql_parse_result result;
    int failures = 0;

    for (size_t index = 0U; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        const struct mylite_sql_ast_node *statement = NULL;
        const struct mylite_sql_ast_node *where_clause = NULL;
        const struct mylite_sql_ast_node *predicate = NULL;

        failures += parser_test_parse_sql(cases[index].sql, MYLITE_SQL_PARSE_OK, &result);
        statement = parser_test_child_at(result.root, 0U);
        where_clause = parser_test_child_at(statement, 2U);
        predicate = parser_test_child_at(where_clause, 0U);
        failures +=
            parser_test_expect_node(where_clause, MYLITE_SQL_AST_WHERE_CLAUSE, "where clause");
        failures +=
            parser_test_expect_node(predicate, cases[index].expected_kind, "where predicate");
        failures += parser_test_expect_operator(
            predicate,
            cases[index].expected_operator,
            "where operator"
        );
        failures += parser_test_expect_span_text(
            parser_test_child_at(predicate, 0U),
            "id",
            "where predicate column"
        );
        mylite_sql_parse_result_deinit(&result);
    }

    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle WHERE TRUE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_LITERAL_TRUE,
        "scalar literal truth predicate"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle WHERE 0x1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_LITERAL_HEX,
        "hex scalar literal truth predicate"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle WHERE -1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_UNARY_EXPRESSION,
        "signed scalar literal truth predicate"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle WHERE 1 = id;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_COMPARISON_PREDICATE,
        "literal-left column predicate"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                0U
            ),
            0U
        ),
        "1",
        "literal-left column predicate literal"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                0U
            ),
            1U
        ),
        "id",
        "literal-left column predicate column"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle WHERE 0x1 = id;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_LITERAL_HEX,
        "hex literal-left column predicate"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle WHERE NULL <=> id;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL,
        "literal-left null-safe predicate"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle WHERE 1 = 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_COMPARISON_PREDICATE,
        "scalar literal comparison predicate"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle WHERE NULL IS UNKNOWN;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN,
        "scalar literal unknown predicate"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'app';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_COMPARISON_PREDICATE,
        "information schema string predicate"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                0U
            ),
            1U
        ),
        "'app'",
        "information schema string predicate value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = DATABASE();",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_COMPARISON_PREDICATE,
        "information schema database predicate"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                0U
            ),
            1U
        ),
        MYLITE_SQL_AST_DATABASE_FUNCTION,
        "information schema database predicate value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle WHERE id BETWEEN -2 AND 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_BETWEEN_PREDICATE,
        "between predicate"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                0U
            ),
            0U
        ),
        "id",
        "between predicate column"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                0U
            ),
            1U
        ),
        "-2",
        "between lower bound"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                0U
            ),
            2U
        ),
        "1",
        "between upper bound"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle WHERE id BETWEEN 0x1 AND 0x41;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                0U
            ),
            1U
        ),
        MYLITE_SQL_AST_LITERAL_HEX,
        "hex between lower bound"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                0U
            ),
            2U
        ),
        MYLITE_SQL_AST_LITERAL_HEX,
        "hex between upper bound"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle WHERE id NOT BETWEEN -2 AND 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_NOT_PREDICATE,
        "not between predicate"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_BETWEEN_PREDICATE,
        "not between child"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle WHERE NOT id BETWEEN -2 AND 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_NOT_PREDICATE,
        "prefix not between predicate"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_BETWEEN_PREDICATE,
        "prefix not between child"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle WHERE id BETWEEN -2 AND 1 AND nn = 5 OR id = 3;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_OR_PREDICATE,
        "between and or precedence root"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_AND_PREDICATE,
        "between binds before later and"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(
                    parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                    0U
                ),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_BETWEEN_PREDICATE,
        "between precedence child"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle WHERE id IN (-2, +1, 0x41, NULL, TRUE, FALSE);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_IN_PREDICATE,
        "in predicate"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                0U
            ),
            0U
        ),
        "id",
        "in predicate column"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                0U
            ),
            1U
        ),
        MYLITE_SQL_AST_PREDICATE_VALUE_LIST,
        "in predicate list"
    );
    failures += parser_test_expect_child_count(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                0U
            ),
            1U
        ),
        sizeof(in_predicate_values) / sizeof(in_predicate_values[0]),
        "in predicate list count"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(
                    parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                    0U
                ),
                1U
            ),
            0U
        ),
        "-2",
        "in predicate first value"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(
                    parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                    0U
                ),
                1U
            ),
            2U
        ),
        "0x41",
        "in predicate hex value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle WHERE id NOT IN (-2, 1, NULL);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_NOT_PREDICATE,
        "not in predicate"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_IN_PREDICATE,
        "not in child"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle WHERE id NOT LIKE '1%';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_NOT_PREDICATE,
        "not like predicate"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_COMPARISON_PREDICATE,
        "not like child"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_OPERATOR_LIKE,
        "not like child operator"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle WHERE id NOT LIKE BINARY '1%';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_NOT_PREDICATE,
        "not like binary predicate"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_COMPARISON_PREDICATE,
        "not like binary child"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_OPERATOR_LIKE_BINARY,
        "not like binary child operator"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle WHERE id NOT REGEXP '^1$';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_NOT_PREDICATE,
        "not regexp predicate"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_COMPARISON_PREDICATE,
        "not regexp child"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_OPERATOR_REGEXP,
        "not regexp child operator"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle WHERE id NOT RLIKE '^1$';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_NOT_PREDICATE,
        "not rlike predicate"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_COMPARISON_PREDICATE,
        "not rlike child"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_OPERATOR_RLIKE,
        "not rlike child operator"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle WHERE NOT id IN (-2, 1, NULL);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_NOT_PREDICATE,
        "prefix not in predicate"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_IN_PREDICATE,
        "prefix not in child"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle WHERE id IN (-2, 1) AND nn = 5 OR id = 3;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_OR_PREDICATE,
        "in and or precedence root"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_AND_PREDICATE,
        "in binds before later and"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(
                    parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                    0U
                ),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_IN_PREDICATE,
        "in precedence child"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle WHERE NOT id IS UNKNOWN;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_NOT_PREDICATE,
        "prefix not is unknown predicate"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_IS_BOOLEAN_PREDICATE,
        "prefix not is unknown child"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle WHERE id IS TRUE AND nn = 5 OR id = 3;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_OR_PREDICATE,
        "is boolean and or precedence root"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_AND_PREDICATE,
        "is boolean binds before later and"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(
                    parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                    0U
                ),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_IS_BOOLEAN_PREDICATE,
        "is boolean precedence child"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle WHERE id IN ();",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle WHERE id IN (nn);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle WHERE id IN ('1');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(
                    parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                    0U
                ),
                1U
            ),
            0U
        ),
        MYLITE_SQL_AST_LITERAL_STRING,
        "string IN value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle WHERE id LIKE DATABASE();",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle WHERE id LIKE '1%' ESCAPE '#';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle WHERE id REGEXP 1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle WHERE id REGEXP DATABASE();",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle WHERE id IS 1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle WHERE id IS TRUE IS TRUE;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle WHERE 1 IS TRUE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle WHERE id + 1 IS TRUE;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle WHERE id BETWEEN NULL AND 1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle WHERE id BETWEEN nn AND 1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT * FROM simple_lifecycle WHERE (id = +1);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized predicate"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT * FROM simple_lifecycle WHERE id = 1 AND nn IS NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_AND_PREDICATE,
        "and predicate"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_OPERATOR_LOGICAL_AND,
        "and predicate operator"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT * FROM simple_lifecycle WHERE (id = 1) && (nn IS NOT NULL);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_AND_PREDICATE,
        "deprecated and predicate"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_OPERATOR_DEPRECATED_LOGICAL_AND,
        "deprecated and predicate operator"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT * FROM simple_lifecycle WHERE id = 1 AND nn = 2 AND n IS NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_AND_PREDICATE,
        "chained and predicate"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_AND_PREDICATE,
        "left-associative chained and predicate"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT * FROM simple_lifecycle WHERE id = 1 OR nn = 2;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_OR_PREDICATE,
        "or predicate"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_OPERATOR_LOGICAL_OR,
        "or predicate operator"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT * FROM simple_lifecycle WHERE id = 1 || nn = 2;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_OR_PREDICATE,
        "deprecated or predicate"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_OPERATOR_DEPRECATED_LOGICAL_OR,
        "deprecated or predicate operator"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT * FROM simple_lifecycle WHERE id = 1 OR nn = 2 AND n IS NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_OR_PREDICATE,
        "or and precedence root"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                0U
            ),
            1U
        ),
        MYLITE_SQL_AST_AND_PREDICATE,
        "and binds tighter than or"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT * FROM simple_lifecycle WHERE id = 1 XOR nn = 2;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_XOR_PREDICATE,
        "xor predicate"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR,
        "xor predicate operator"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT * FROM simple_lifecycle WHERE id = 1 XOR nn = 2 XOR n IS NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_XOR_PREDICATE,
        "chained xor predicate"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_XOR_PREDICATE,
        "left-associative chained xor predicate"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT * FROM simple_lifecycle WHERE id = 1 XOR nn = 2 AND n IS NULL OR id = 3;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_OR_PREDICATE,
        "or above xor precedence root"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_XOR_PREDICATE,
        "xor binds tighter than or"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(
                    parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                    0U
                ),
                0U
            ),
            1U
        ),
        MYLITE_SQL_AST_AND_PREDICATE,
        "and binds tighter than xor"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT * FROM simple_lifecycle WHERE (id = 1 OR nn = 2) XOR n IS NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_XOR_PREDICATE,
        "parentheses override xor precedence"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized or xor left operand"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT * FROM simple_lifecycle WHERE NOT id = 1 XOR nn = 2;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_XOR_PREDICATE,
        "not xor precedence root"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_NOT_PREDICATE,
        "not binds tighter than xor"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT * FROM simple_lifecycle WHERE (id = 1 OR nn = 2) AND n IS NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_AND_PREDICATE,
        "parentheses override or precedence"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized or left operand"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT * FROM simple_lifecycle WHERE NOT id = 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_NOT_PREDICATE,
        "not predicate"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT,
        "not predicate operator"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT * FROM simple_lifecycle WHERE NOT NOT id = 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_NOT_PREDICATE,
        "outer repeated not predicate"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_NOT_PREDICATE,
        "inner repeated not predicate"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT * FROM simple_lifecycle WHERE NOT id = 1 AND nn = 2 OR n IS NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_OR_PREDICATE,
        "not and or precedence root"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_AND_PREDICATE,
        "not binds tighter than and"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(
                    parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                    0U
                ),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_NOT_PREDICATE,
        "not predicate under and"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT * FROM simple_lifecycle WHERE NOT (id = 1 OR nn = 2);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_NOT_PREDICATE,
        "not parenthesized predicate"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "not parenthesized child"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle WHERE simple_lifecycle.id = 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "qualified predicate column"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle WHERE id = TRUE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                0U
            ),
            1U
        ),
        MYLITE_SQL_AST_LITERAL_TRUE,
        "true predicate right operand"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM users WHERE EXISTS (SELECT 1 FROM orders);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_EXISTS_PREDICATE,
        "exists predicate"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 2U), 0U),
        "EXISTS (SELECT 1 FROM orders)",
        "exists predicate span"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_SELECT_STATEMENT,
        "exists inner select"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM users WHERE NOT EXISTS (SELECT * FROM empty_orders);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_NOT_PREDICATE,
        "not exists predicate"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_EXISTS_PREDICATE,
        "not exists child"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT u.id FROM users AS u WHERE EXISTS "
        "(SELECT 1 FROM orders AS o WHERE o.user_id = u.id);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(
                    parser_test_child_at(
                        parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                        0U
                    ),
                    0U
                ),
                2U
            ),
            0U
        ),
        MYLITE_SQL_AST_COMPARISON_PREDICATE,
        "exists correlated comparison"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(
                    parser_test_child_at(
                        parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                        0U
                    ),
                    0U
                ),
                2U
            ),
            0U
        ),
        MYLITE_SQL_AST_COMPARISON_PREDICATE,
        "exists correlated comparison node"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(
                    parser_test_child_at(
                        parser_test_child_at(
                            parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                            0U
                        ),
                        0U
                    ),
                    2U
                ),
                0U
            ),
            1U
        ),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "exists correlated right column"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT u.id FROM users AS u WHERE u.id IN "
        "(SELECT o.user_id FROM orders AS o WHERE o.user_id = u.id);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_IN_PREDICATE,
        "IN subquery predicate"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                0U
            ),
            1U
        ),
        MYLITE_SQL_AST_SELECT_STATEMENT,
        "IN subquery inner select"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT u.id FROM users AS u WHERE u.id NOT IN "
        "(SELECT o.user_id FROM orders AS o);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_NOT_PREDICATE,
        "NOT IN subquery predicate"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_IN_PREDICATE,
        "NOT IN subquery child"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT p.ID FROM posts AS p WHERE "
        "(SELECT post_status FROM posts WHERE ID = p.post_parent) IN ('publish');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_IN_PREDICATE,
        "scalar subquery IN predicate"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_SCALAR_SUBQUERY,
        "scalar subquery IN left operand"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                0U
            ),
            1U
        ),
        MYLITE_SQL_AST_PREDICATE_VALUE_LIST,
        "scalar subquery IN value list"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT p.ID FROM posts AS p WHERE "
        "(SELECT post_status FROM posts WHERE ID = p.post_parent) NOT IN ('trash');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_NOT_PREDICATE,
        "scalar subquery NOT IN predicate"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_IN_PREDICATE,
        "scalar subquery NOT IN child"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT EXISTS (SELECT 1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_select_order_limit_clauses(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *order_clause = NULL;
    const struct mylite_sql_ast_node *order_items = NULL;
    const struct mylite_sql_ast_node *order_item = NULL;
    const struct mylite_sql_ast_node *limit_clause = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT * FROM simple_lifecycle ORDER BY id;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    order_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    failures +=
        parser_test_expect_node(order_clause, MYLITE_SQL_AST_ORDER_BY_CLAUSE, "order clause");
    failures += parser_test_expect_span_text(
        parser_test_child_at(order_clause, 0U),
        "id",
        "default order key"
    );
    failures += parser_test_expect_true(
        parser_test_child_at(order_clause, 1U) == NULL,
        "default order has no direction"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle WHERE id = 1 ORDER BY nn ASC LIMIT 2 OFFSET 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    order_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    limit_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_LIMIT_CLAUSE);
    failures +=
        parser_test_expect_node(order_clause, MYLITE_SQL_AST_ORDER_BY_CLAUSE, "asc order clause");
    failures +=
        parser_test_expect_span_text(parser_test_child_at(order_clause, 0U), "nn", "asc order key");
    failures += parser_test_expect_order_direction(
        parser_test_child_at(order_clause, 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_ASC,
        "asc order direction"
    );
    failures +=
        parser_test_expect_node(limit_clause, MYLITE_SQL_AST_LIMIT_CLAUSE, "offset limit clause");
    failures += parser_test_expect_span_text(
        parser_test_child_at(limit_clause, 0U),
        "2",
        "offset limit row count"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(limit_clause, 1U),
        "1",
        "offset limit offset"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle ORDER BY simple_lifecycle.id DESC LIMIT 1, 2;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    order_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    limit_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_LIMIT_CLAUSE);
    failures += parser_test_expect_node(
        parser_test_child_at(order_clause, 0U),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "qualified order key"
    );
    failures += parser_test_expect_order_direction(
        parser_test_child_at(order_clause, 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_DESC,
        "desc order direction"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(limit_clause, 0U),
        "2",
        "comma limit row count"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(limit_clause, 1U),
        "1",
        "comma limit offset"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle ORDER BY n DESC, id ASC, nn LIMIT 3;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    order_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    order_items = parser_test_child_at(order_clause, 0U);
    failures += parser_test_expect_node(
        order_items,
        MYLITE_SQL_AST_ORDER_BY_ITEM_LIST,
        "multi-key order list"
    );
    order_item = parser_test_child_at(order_items, 0U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(order_item, 0U),
        "n",
        "first multi-key order key"
    );
    failures += parser_test_expect_order_direction(
        parser_test_child_at(order_item, 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_DESC,
        "first multi-key direction"
    );
    order_item = parser_test_child_at(order_items, 1U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(order_item, 0U),
        "id",
        "second multi-key order key"
    );
    failures += parser_test_expect_order_direction(
        parser_test_child_at(order_item, 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_ASC,
        "second multi-key direction"
    );
    order_item = parser_test_child_at(order_items, 2U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(order_item, 0U),
        "nn",
        "third multi-key order key"
    );
    failures += parser_test_expect_true(
        parser_test_child_at(order_item, 1U) == NULL,
        "third multi-key default direction"
    );
    limit_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_LIMIT_CLAUSE);
    failures += parser_test_expect_span_text(
        parser_test_child_at(limit_clause, 0U),
        "3",
        "multi-key limit row count"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle ORDER BY FIELD(name, 'b', 'a') DESC LIMIT 2;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    order_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    failures +=
        parser_test_expect_node(order_clause, MYLITE_SQL_AST_ORDER_BY_CLAUSE, "field order clause");
    failures += parser_test_expect_node(
        parser_test_child_at(order_clause, 0U),
        MYLITE_SQL_AST_FIELD_FUNCTION,
        "field order key"
    );
    failures += parser_test_expect_order_direction(
        parser_test_child_at(order_clause, 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_DESC,
        "field order direction"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle ORDER BY (FIELD(name, 'b', 'a'));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    order_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    failures += parser_test_expect_node(
        parser_test_child_at(order_clause, 0U),
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized field order key"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(order_clause, 0U), 0U),
        MYLITE_SQL_AST_FIELD_FUNCTION,
        "parenthesized field order child"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle ORDER BY title LIKE '%foo%' DESC, created DESC;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    order_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    order_items = parser_test_child_at(order_clause, 0U);
    order_item = parser_test_child_at(order_items, 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(order_item, 0U),
        MYLITE_SQL_AST_COMPARISON_PREDICATE,
        "like order key"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(order_item, 0U),
        MYLITE_SQL_AST_OPERATOR_LIKE,
        "like order operator"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(order_item, 0U), 0U),
        "title",
        "like order column"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(order_item, 0U), 1U),
        "'%foo%'",
        "like order pattern"
    );
    failures += parser_test_expect_order_direction(
        parser_test_child_at(order_item, 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_DESC,
        "like order direction"
    );
    order_item = parser_test_child_at(order_items, 1U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(order_item, 0U),
        "created",
        "like order tiebreaker"
    );
    failures += parser_test_expect_order_direction(
        parser_test_child_at(order_item, 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_DESC,
        "like order tiebreaker direction"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle "
        "ORDER BY (CASE WHEN title LIKE '%foo%' THEN 1 ELSE 2 END), created DESC;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    order_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    order_items = parser_test_child_at(order_clause, 0U);
    order_item = parser_test_child_at(order_items, 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(order_item, 0U),
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "searched case order key"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(order_item, 0U), 0U),
        MYLITE_SQL_AST_SEARCHED_CASE_EXPRESSION,
        "searched case order child"
    );
    order_item = parser_test_child_at(order_items, 1U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(order_item, 0U),
        "created",
        "searched case order tiebreaker"
    );
    failures += parser_test_expect_order_direction(
        parser_test_child_at(order_item, 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_DESC,
        "searched case order tiebreaker direction"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle LIMIT 0;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    limit_clause = parser_test_first_child_kind(
        parser_test_child_at(result.root, 0U),
        MYLITE_SQL_AST_LIMIT_CLAUSE
    );
    failures +=
        parser_test_expect_node(limit_clause, MYLITE_SQL_AST_LIMIT_CLAUSE, "simple limit clause");
    failures += parser_test_expect_span_text(
        parser_test_child_at(limit_clause, 0U),
        "0",
        "simple limit row count"
    );
    failures += parser_test_expect_true(
        parser_test_child_at(limit_clause, 1U) == NULL,
        "simple limit has no offset"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_select_group_by_clause(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *group_clause = NULL;
    const struct mylite_sql_ast_node *having_clause = NULL;
    const struct mylite_sql_ast_node *order_clause = NULL;
    const struct mylite_sql_ast_node *limit_clause = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT g, COUNT(*) AS c FROM numbers WHERE id >= 1 GROUP BY g HAVING c > 1 "
        "ORDER BY g LIMIT 2;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(statement, 0U);
    group_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_GROUP_BY_CLAUSE);
    having_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_HAVING_CLAUSE);
    order_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    limit_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_LIMIT_CLAUSE);
    failures +=
        parser_test_expect_node(group_clause, MYLITE_SQL_AST_GROUP_BY_CLAUSE, "group clause");
    failures +=
        parser_test_expect_span_text(parser_test_child_at(group_clause, 0U), "g", "group key");
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 2U),
        MYLITE_SQL_AST_WHERE_CLAUSE,
        "group where"
    );
    failures +=
        parser_test_expect_node(having_clause, MYLITE_SQL_AST_HAVING_CLAUSE, "group having");
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(having_clause, 0U), 0U),
        "c",
        "having alias"
    );
    failures +=
        parser_test_expect_node(order_clause, MYLITE_SQL_AST_ORDER_BY_CLAUSE, "group order");
    failures += parser_test_expect_node(limit_clause, MYLITE_SQL_AST_LIMIT_CLAUSE, "group limit");
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 1U), 0U),
        MYLITE_SQL_AST_COUNT_STAR_FUNCTION,
        "group aggregate"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT DISTINCT meta_key FROM wp_postmeta WHERE meta_key NOT BETWEEN '_' AND '_z' "
        "HAVING meta_key NOT LIKE CONCAT('\\_', '%') ORDER BY meta_key LIMIT 30;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    having_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_HAVING_CLAUSE);
    failures += parser_test_expect_node(
        having_clause,
        MYLITE_SQL_AST_HAVING_CLAUSE,
        "select having without group"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(having_clause, 0U),
        MYLITE_SQL_AST_NOT_PREDICATE,
        "select having not like"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT t1.name FROM t1 JOIN t2 ON t2.id = t1.id HAVING name;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    having_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_HAVING_CLAUSE);
    failures += parser_test_expect_node(
        parser_test_child_at(having_clause, 0U),
        MYLITE_SQL_AST_IDENTIFIER,
        "bare having identifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT t.g AS k, SUM(t.n) AS s FROM app.numbers AS t GROUP BY t.g "
        "HAVING SUM(t.n) IS NOT NULL ORDER BY k DESC LIMIT 1 OFFSET 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    group_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_GROUP_BY_CLAUSE);
    having_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_HAVING_CLAUSE);
    order_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    limit_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_LIMIT_CLAUSE);
    failures += parser_test_expect_node(
        parser_test_child_at(group_clause, 0U),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "qualified group key"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(group_clause, 0U),
        "t.g",
        "qualified group span"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(having_clause, 0U), 0U),
        MYLITE_SQL_AST_SUM_AGGREGATE_FUNCTION,
        "having aggregate operand"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(order_clause, 0U),
        "k",
        "group order alias"
    );
    failures += parser_test_expect_order_direction(
        parser_test_child_at(order_clause, 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_DESC,
        "group order desc"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(limit_clause, 0U),
        "1",
        "group limit row count"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(limit_clause, 1U),
        "1",
        "group limit offset"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT g, COUNT(*) FROM numbers GROUP BY 1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT g, COUNT(*) FROM numbers GROUP BY g, n;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    group_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_GROUP_BY_CLAUSE);
    failures += parser_test_expect_span_text(
        parser_test_child_at(group_clause, 0U),
        "g",
        "first group key"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(group_clause, 1U),
        "n",
        "second group key"
    );
    failures += parser_test_expect_true(
        parser_test_child_at(group_clause, 2U) == NULL,
        "two-key group clause child count"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT t.g, t.n, COUNT(*) FROM app.numbers AS t GROUP BY t.g, t.n;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    group_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_GROUP_BY_CLAUSE);
    failures += parser_test_expect_node(
        parser_test_child_at(group_clause, 0U),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "first qualified group key"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(group_clause, 1U),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "second qualified group key"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(group_clause, 0U),
        "t.g",
        "first qualified group span"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(group_clause, 1U),
        "t.n",
        "second qualified group span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT g, COUNT(*) FROM numbers GROUP BY missing;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    group_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_GROUP_BY_CLAUSE);
    failures += parser_test_expect_span_text(
        parser_test_child_at(group_clause, 0U),
        "missing",
        "mismatched group span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT g, COUNT(*) FROM numbers GROUP BY g HAVING COUNT(*) > 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    having_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_HAVING_CLAUSE);
    failures +=
        parser_test_expect_node(having_clause, MYLITE_SQL_AST_HAVING_CLAUSE, "count having");
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(having_clause, 0U), 0U),
        MYLITE_SQL_AST_COUNT_STAR_FUNCTION,
        "having count star"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT g, COUNT(*) FROM numbers GROUP BY g HAVING COUNT(*) + 1 > 2;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_select_distinct_clause(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *order_clause = NULL;
    const struct mylite_sql_ast_node *limit_clause = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT DISTINCT n FROM simple_lifecycle WHERE n IS NOT NULL ORDER BY n DESC "
        "LIMIT 1 OFFSET 0;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(statement, 0U);
    order_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    limit_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_LIMIT_CLAUSE);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DISTINCT,
        "select distinct modifier"
    );
    failures += parser_test_expect_child_count(select_list, 1U, "select distinct item count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(select_list, 0U), 0U),
        "n",
        "select distinct column"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 1U),
        MYLITE_SQL_AST_FROM_TABLE,
        "distinct table"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 2U),
        MYLITE_SQL_AST_WHERE_CLAUSE,
        "distinct where"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(order_clause, 0U),
        "n",
        "distinct order key"
    );
    failures += parser_test_expect_order_direction(
        parser_test_child_at(order_clause, 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_DESC,
        "distinct desc direction"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(limit_clause, 0U),
        "1",
        "distinct limit row count"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(limit_clause, 1U),
        "0",
        "distinct limit offset"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT DISTINCT * FROM simple_lifecycle ORDER BY id LIMIT 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DISTINCT,
        "select distinct wildcard modifier"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 0U), 0U), 0U),
        MYLITE_SQL_AST_WILDCARD,
        "select distinct wildcard"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT DISTINCT n, nn FROM simple_lifecycle ORDER BY n;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DISTINCT,
        "select distinct multiple modifier"
    );
    failures += parser_test_expect_child_count(
        parser_test_child_at(statement, 0U),
        2U,
        "select distinct multiple items"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT DISTINCTROW n FROM simple_lifecycle WHERE n IS NOT NULL ORDER BY n DESC "
        "LIMIT 1 OFFSET 0;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(statement, 0U);
    order_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    limit_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_LIMIT_CLAUSE);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DISTINCT,
        "select distinctrow modifier"
    );
    failures += parser_test_expect_child_count(select_list, 1U, "select distinctrow item count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(select_list, 0U), 0U),
        "n",
        "select distinctrow column"
    );
    failures += parser_test_expect_order_direction(
        parser_test_child_at(order_clause, 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_DESC,
        "distinctrow desc direction"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(limit_clause, 0U),
        "1",
        "distinctrow limit row count"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT DISTINCTROW * FROM simple_lifecycle ORDER BY id LIMIT 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DISTINCT,
        "select distinctrow wildcard modifier"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 0U), 0U), 0U),
        MYLITE_SQL_AST_WILDCARD,
        "select distinctrow wildcard"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT DISTINCT n;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DISTINCT,
        "select distinct scalar parsed for runtime rejection"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT DISTINCT n FROM DUAL;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DISTINCT,
        "select distinct dual parsed for runtime rejection"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT DISTINCTROW n;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DISTINCT,
        "select distinctrow scalar parsed for runtime rejection"
    );
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("SELECT DISTINCTROW n FROM DUAL;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DISTINCT,
        "select distinctrow dual parsed for runtime rejection"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_select_sql_calc_found_rows_clause(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *order_clause = NULL;
    const struct mylite_sql_ast_node *limit_clause = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT SQL_CALC_FOUND_ROWS n FROM simple_lifecycle WHERE n IS NOT NULL "
        "ORDER BY n DESC LIMIT 1 OFFSET 0;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(statement, 0U);
    order_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    limit_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_LIMIT_CLAUSE);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DEFAULT,
        "sql calc default modifier"
    );
    failures += parser_test_expect_true(
        mylite_sql_ast_node_select_calc_found_rows(statement),
        "sql calc found rows flag"
    );
    failures += parser_test_expect_child_count(select_list, 1U, "sql calc item count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(select_list, 0U), 0U),
        "n",
        "sql calc column"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 1U),
        MYLITE_SQL_AST_FROM_TABLE,
        "sql calc table"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 2U),
        MYLITE_SQL_AST_WHERE_CLAUSE,
        "sql calc where"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(order_clause, 0U),
        "n",
        "sql calc order key"
    );
    failures += parser_test_expect_order_direction(
        parser_test_child_at(order_clause, 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_DESC,
        "sql calc desc direction"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(limit_clause, 0U),
        "1",
        "sql calc limit row count"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(limit_clause, 1U),
        "0",
        "sql calc limit offset"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT ALL SQL_CALC_FOUND_ROWS * FROM simple_lifecycle ORDER BY id LIMIT 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DEFAULT,
        "all sql calc default modifier"
    );
    failures += parser_test_expect_true(
        mylite_sql_ast_node_select_calc_found_rows(statement),
        "all sql calc found rows flag"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 0U), 0U), 0U),
        MYLITE_SQL_AST_WILDCARD,
        "all sql calc wildcard"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT SQL_CALC_FOUND_ROWS COUNT(*) FROM simple_lifecycle;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_select_calc_found_rows(statement),
        "sql calc aggregate parsed for runtime rejection"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT SQL_CALC_FOUND_ROWS 1;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_select_calc_found_rows(statement),
        "sql calc scalar parsed for runtime rejection"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "SELECT SQL_CALC_FOUND_ROWS n FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_select_calc_found_rows(statement),
        "sql calc dual parsed for runtime rejection"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "SELECT DISTINCT SQL_CALC_FOUND_ROWS n FROM simple_lifecycle;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DISTINCT,
        "distinct sql calc modifier parsed for runtime rejection"
    );
    failures += parser_test_expect_true(
        mylite_sql_ast_node_select_calc_found_rows(statement),
        "distinct sql calc flag parsed for runtime rejection"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "SELECT SQL_CALC_FOUND_ROWS DISTINCT n FROM simple_lifecycle;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "SELECT SQL_CALC_FOUND_ROWS ALL n FROM simple_lifecycle;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_select_noop_modifier_clause(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    unsigned int expected_options =
        MYLITE_SQL_AST_SELECT_OPTION_HIGH_PRIORITY | MYLITE_SQL_AST_SELECT_OPTION_STRAIGHT_JOIN |
        MYLITE_SQL_AST_SELECT_OPTION_SQL_SMALL_RESULT |
        MYLITE_SQL_AST_SELECT_OPTION_SQL_BIG_RESULT |
        MYLITE_SQL_AST_SELECT_OPTION_SQL_BUFFER_RESULT | MYLITE_SQL_AST_SELECT_OPTION_SQL_NO_CACHE;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT HIGH_PRIORITY STRAIGHT_JOIN SQL_SMALL_RESULT SQL_BIG_RESULT "
        "SQL_BUFFER_RESULT SQL_NO_CACHE 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_select_options(statement) == expected_options,
        "scalar noop select modifiers"
    );
    failures += parser_test_expect_true(
        !mylite_sql_ast_node_select_calc_found_rows(statement),
        "scalar noop select no sql calc flag"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT DISTINCT HIGH_PRIORITY STRAIGHT_JOIN SQL_SMALL_RESULT SQL_BIG_RESULT "
        "SQL_BUFFER_RESULT SQL_NO_CACHE SQL_CALC_FOUND_ROWS n FROM simple_lifecycle "
        "ORDER BY n DESC LIMIT 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DISTINCT,
        "table noop select distinct modifier"
    );
    failures += parser_test_expect_true(
        mylite_sql_ast_node_select_options(statement) == expected_options,
        "table noop select modifiers"
    );
    failures += parser_test_expect_true(
        mylite_sql_ast_node_select_calc_found_rows(statement),
        "table noop select sql calc flag"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT SQL_NO_CACHE SQL_BUFFER_RESULT id FROM simple_lifecycle;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT HIGH_PRIORITY HIGH_PRIORITY id FROM simple_lifecycle;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT SQL_CALC_FOUND_ROWS SQL_NO_CACHE id FROM simple_lifecycle;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_select_locking_clause(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *select_source = NULL;
    int failures = 0;

    failures += parser_test_parse_sql("SELECT 1 FOR UPDATE;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_select_locking_clause(statement) ==
            MYLITE_SQL_AST_SELECT_LOCKING_CLAUSE_FOR_UPDATE,
        "scalar select for update"
    );
    failures += parser_test_expect_span_text(
        statement,
        "SELECT 1 FOR UPDATE",
        "scalar select for update span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT 1 FROM DUAL FOR SHARE;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_select_locking_clause(statement) ==
            MYLITE_SQL_AST_SELECT_LOCKING_CLAUSE_FOR_SHARE,
        "dual select for share"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle WHERE id = 1 ORDER BY id LIMIT 1 FOR UPDATE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_select_locking_clause(statement) ==
            MYLITE_SQL_AST_SELECT_LOCKING_CLAUSE_FOR_UPDATE,
        "table select for update"
    );
    failures += parser_test_expect_node(
        parser_test_first_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE),
        MYLITE_SQL_AST_ORDER_BY_CLAUSE,
        "locking select order clause"
    );
    failures += parser_test_expect_node(
        parser_test_first_child_kind(statement, MYLITE_SQL_AST_LIMIT_CLAUSE),
        MYLITE_SQL_AST_LIMIT_CLAUSE,
        "locking select limit clause"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT * FROM simple_lifecycle LOCK IN SHARE MODE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_select_locking_clause(statement) ==
            MYLITE_SQL_AST_SELECT_LOCKING_CLAUSE_LOCK_IN_SHARE_MODE,
        "table select lock in share mode"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT n, COUNT(*) FROM simple_lifecycle GROUP BY n ORDER BY n FOR SHARE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_select_locking_clause(statement) ==
            MYLITE_SQL_AST_SELECT_LOCKING_CLAUSE_FOR_SHARE,
        "grouped select for share"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "INSERT INTO copy SELECT id FROM simple_lifecycle FOR UPDATE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    select_source = parser_test_child_at(statement, 2U);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_select_locking_clause(select_source) ==
            MYLITE_SQL_AST_SELECT_LOCKING_CLAUSE_FOR_UPDATE,
        "insert select for update"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "REPLACE INTO copy SELECT id FROM simple_lifecycle FOR SHARE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    select_source = parser_test_child_at(statement, 2U);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_select_locking_clause(select_source) ==
            MYLITE_SQL_AST_SELECT_LOCKING_CLAUSE_FOR_SHARE,
        "replace select for share"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE copy AS SELECT id FROM simple_lifecycle FOR UPDATE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    select_source = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_select_locking_clause(select_source) ==
            MYLITE_SQL_AST_SELECT_LOCKING_CLAUSE_FOR_UPDATE,
        "ctas select for update"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle FOR UPDATE NOWAIT;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_select_locking_clause(statement) ==
            MYLITE_SQL_AST_SELECT_LOCKING_CLAUSE_FOR_UPDATE,
        "select for update nowait"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle FOR SHARE SKIP LOCKED;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_select_locking_clause(statement) ==
            MYLITE_SQL_AST_SELECT_LOCKING_CLAUSE_FOR_SHARE,
        "select for share skip locked"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle FOR UPDATE SKIP LOCKED;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_select_locking_clause(statement) ==
            MYLITE_SQL_AST_SELECT_LOCKING_CLAUSE_FOR_UPDATE,
        "select for update skip locked"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle FOR SHARE NOWAIT;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_select_locking_clause(statement) ==
            MYLITE_SQL_AST_SELECT_LOCKING_CLAUSE_FOR_SHARE,
        "select for share nowait"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle FOR UPDATE OF simple_lifecycle;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle FOR UPDATE FOR SHARE;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle FOR UPDATE ORDER BY id;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_select_all_clause(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *order_clause = NULL;
    const struct mylite_sql_ast_node *limit_clause = NULL;
    int failures = 0;

    failures += parser_test_parse_sql("SELECT ALL 1;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DEFAULT,
        "select all scalar modifier"
    );
    failures += parser_test_expect_child_count(select_list, 1U, "select all scalar item count");
    failures += parser_test_expect_literal(
        parser_test_child_at(parser_test_child_at(select_list, 0U), 0U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "select all scalar literal"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT ALL 1 FROM DUAL;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DEFAULT,
        "select all dual modifier"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 1U),
        MYLITE_SQL_AST_FROM_DUAL,
        "select all dual"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT ALL n FROM simple_lifecycle WHERE n IS NOT NULL ORDER BY n DESC "
        "LIMIT 1 OFFSET 0;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(statement, 0U);
    order_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    limit_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_LIMIT_CLAUSE);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DEFAULT,
        "select all table modifier"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(select_list, 0U), 0U),
        "n",
        "select all column"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 1U),
        MYLITE_SQL_AST_FROM_TABLE,
        "select all table"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 2U),
        MYLITE_SQL_AST_WHERE_CLAUSE,
        "select all where"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(order_clause, 0U),
        "n",
        "select all order key"
    );
    failures += parser_test_expect_order_direction(
        parser_test_child_at(order_clause, 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_DESC,
        "select all desc direction"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(limit_clause, 0U),
        "1",
        "select all limit row count"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(limit_clause, 1U),
        "0",
        "select all limit offset"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT ALL * FROM simple_lifecycle ORDER BY id LIMIT 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DEFAULT,
        "select all wildcard modifier"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 0U), 0U), 0U),
        MYLITE_SQL_AST_WILDCARD,
        "select all wildcard"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT ALL *;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DEFAULT,
        "select all bare wildcard modifier"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 0U), 0U), 0U),
        MYLITE_SQL_AST_WILDCARD,
        "select all bare wildcard"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT ALL * FROM DUAL;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DEFAULT,
        "select all dual wildcard modifier"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 0U), 0U), 0U),
        MYLITE_SQL_AST_WILDCARD,
        "select all dual wildcard"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 1U),
        MYLITE_SQL_AST_FROM_DUAL,
        "select all dual wildcard"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT ALL COUNT(*) FROM simple_lifecycle;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DEFAULT,
        "select all aggregate modifier"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 0U), 0U),
        MYLITE_SQL_AST_COUNT_STAR_FUNCTION,
        "select all count star"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT ALL ALL 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "SELECT ALL DISTINCT n FROM simple_lifecycle;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "SELECT ALL DISTINCTROW n FROM simple_lifecycle;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "SELECT DISTINCT ALL n FROM simple_lifecycle;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_select_union_clause(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *terms = NULL;
    const struct mylite_sql_ast_node *first_term = NULL;
    const struct mylite_sql_ast_node *second_term = NULL;
    int failures = 0;

    failures += parser_test_parse_sql("SELECT 1 UNION SELECT 2;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    terms = parser_test_child_at(statement, 1U);
    first_term = parser_test_child_at(terms, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_COMPOUND_SELECT_STATEMENT,
        "union statement"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 0U),
        MYLITE_SQL_AST_SELECT_STATEMENT,
        "union left"
    );
    failures += parser_test_expect_node(terms, MYLITE_SQL_AST_UNION_TERM_LIST, "union terms");
    failures += parser_test_expect_child_count(terms, 1U, "union term count");
    failures += parser_test_expect_node(first_term, MYLITE_SQL_AST_UNION_TERM, "union term");
    failures += parser_test_expect_true(
        mylite_sql_ast_node_set_operator_kind(first_term) == MYLITE_SQL_AST_SET_OPERATOR_UNION,
        "union set operator"
    );
    failures += parser_test_expect_true(
        mylite_sql_ast_node_union_modifier(first_term) == MYLITE_SQL_AST_UNION_MODIFIER_DISTINCT,
        "default union modifier"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(first_term, 0U),
        MYLITE_SQL_AST_SELECT_STATEMENT,
        "union right"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM a UNION ALL SELECT id FROM b UNION DISTINCT SELECT id FROM c;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    terms = parser_test_child_at(statement, 1U);
    first_term = parser_test_child_at(terms, 0U);
    second_term = parser_test_child_at(terms, 1U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_COMPOUND_SELECT_STATEMENT,
        "union chain statement"
    );
    failures += parser_test_expect_child_count(terms, 2U, "union chain term count");
    failures += parser_test_expect_true(
        mylite_sql_ast_node_union_modifier(first_term) == MYLITE_SQL_AST_UNION_MODIFIER_ALL,
        "union all modifier"
    );
    failures += parser_test_expect_true(
        mylite_sql_ast_node_union_modifier(second_term) == MYLITE_SQL_AST_UNION_MODIFIER_DISTINCT,
        "union distinct modifier"
    );
    failures += parser_test_expect_true(
        mylite_sql_ast_node_set_operator_kind(first_term) == MYLITE_SQL_AST_SET_OPERATOR_UNION,
        "union all set operator"
    );
    failures += parser_test_expect_true(
        mylite_sql_ast_node_set_operator_kind(second_term) == MYLITE_SQL_AST_SET_OPERATOR_UNION,
        "union distinct set operator"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(first_term, 0U), 0U),
                0U
            ),
            0U
        ),
        "id",
        "union all branch select item"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(second_term, 0U), 0U),
                0U
            ),
            0U
        ),
        "id",
        "union distinct branch select item"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM a INTERSECT ALL SELECT id FROM b INTERSECT DISTINCT SELECT id FROM c;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    terms = parser_test_child_at(statement, 1U);
    first_term = parser_test_child_at(terms, 0U);
    second_term = parser_test_child_at(terms, 1U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_COMPOUND_SELECT_STATEMENT,
        "intersect chain statement"
    );
    failures += parser_test_expect_child_count(terms, 2U, "intersect chain term count");
    failures += parser_test_expect_true(
        mylite_sql_ast_node_set_operator_kind(first_term) == MYLITE_SQL_AST_SET_OPERATOR_INTERSECT,
        "intersect all set operator"
    );
    failures += parser_test_expect_true(
        mylite_sql_ast_node_union_modifier(first_term) == MYLITE_SQL_AST_UNION_MODIFIER_ALL,
        "intersect all modifier"
    );
    failures += parser_test_expect_true(
        mylite_sql_ast_node_set_operator_kind(second_term) == MYLITE_SQL_AST_SET_OPERATOR_INTERSECT,
        "intersect distinct set operator"
    );
    failures += parser_test_expect_true(
        mylite_sql_ast_node_union_modifier(second_term) == MYLITE_SQL_AST_UNION_MODIFIER_DISTINCT,
        "intersect distinct modifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM a EXCEPT SELECT id FROM b EXCEPT ALL SELECT id FROM c;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    terms = parser_test_child_at(statement, 1U);
    first_term = parser_test_child_at(terms, 0U);
    second_term = parser_test_child_at(terms, 1U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_COMPOUND_SELECT_STATEMENT,
        "except chain statement"
    );
    failures += parser_test_expect_child_count(terms, 2U, "except chain term count");
    failures += parser_test_expect_true(
        mylite_sql_ast_node_set_operator_kind(first_term) == MYLITE_SQL_AST_SET_OPERATOR_EXCEPT,
        "except default set operator"
    );
    failures += parser_test_expect_true(
        mylite_sql_ast_node_union_modifier(first_term) == MYLITE_SQL_AST_UNION_MODIFIER_DISTINCT,
        "except default modifier"
    );
    failures += parser_test_expect_true(
        mylite_sql_ast_node_set_operator_kind(second_term) == MYLITE_SQL_AST_SET_OPERATOR_EXCEPT,
        "except all set operator"
    );
    failures += parser_test_expect_true(
        mylite_sql_ast_node_union_modifier(second_term) == MYLITE_SQL_AST_UNION_MODIFIER_ALL,
        "except all modifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM a UNION SELECT id FROM b INTERSECT SELECT id FROM c;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    terms = parser_test_child_at(statement, 1U);
    first_term = parser_test_child_at(terms, 0U);
    second_term = parser_test_child_at(terms, 1U);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_set_operator_kind(first_term) == MYLITE_SQL_AST_SET_OPERATOR_UNION,
        "mixed union term operator"
    );
    failures += parser_test_expect_true(
        mylite_sql_ast_node_set_operator_kind(second_term) == MYLITE_SQL_AST_SET_OPERATOR_INTERSECT,
        "mixed intersect term operator"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM a ORDER BY id UNION SELECT id FROM b;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        parser_test_first_child_kind(
            parser_test_child_at(statement, 0U),
            MYLITE_SQL_AST_ORDER_BY_CLAUSE
        ),
        MYLITE_SQL_AST_ORDER_BY_CLAUSE,
        "union branch order parsed for runtime rejection"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM a ORDER BY id INTERSECT SELECT id FROM b;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        parser_test_first_child_kind(
            parser_test_child_at(statement, 0U),
            MYLITE_SQL_AST_ORDER_BY_CLAUSE
        ),
        MYLITE_SQL_AST_ORDER_BY_CLAUSE,
        "intersect branch order parsed for runtime rejection"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_select_with_information_schema_union_clause(void) {
    static const char sql[] =
        "WITH cols AS ("
        "SELECT COLUMN_NAME AS column_name FROM INFORMATION_SCHEMA.COLUMNS "
        "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'wp_users'), "
        "indexes AS ("
        "SELECT DISTINCT INDEX_NAME AS index_name FROM INFORMATION_SCHEMA.STATISTICS "
        "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'wp_users') "
        "SELECT CONCAT(column_name, ' (column)') AS name FROM cols "
        "UNION ALL "
        "SELECT CONCAT(index_name, ' (index)') AS name FROM indexes "
        "ORDER BY name";
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(sql, MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SELECT_STATEMENT,
        "information schema WITH union bridge statement"
    );
    failures += parser_test_expect_child_count(statement, 0U, "WITH bridge select child count");
    failures += parser_test_expect_span_text(statement, sql, "WITH bridge statement span");
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_table_statement(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *from_table = NULL;
    const struct mylite_sql_ast_node *order_clause = NULL;
    const struct mylite_sql_ast_node *order_items = NULL;
    const struct mylite_sql_ast_node *limit_clause = NULL;
    int failures = 0;

    failures += parser_test_parse_sql("TABLE simple_lifecycle;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(statement, 0U);
    from_table = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SELECT_STATEMENT,
        "table statement select"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 0U), 0U),
        MYLITE_SQL_AST_WILDCARD,
        "table statement wildcard"
    );
    failures +=
        parser_test_expect_node(from_table, MYLITE_SQL_AST_FROM_TABLE, "table statement source");
    failures += parser_test_expect_span_text(
        parser_test_child_at(from_table, 0U),
        "simple_lifecycle",
        "table name"
    );
    failures += parser_test_expect_true(
        parser_test_first_child_kind(statement, MYLITE_SQL_AST_WHERE_CLAUSE) == NULL,
        "table statement has no where"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "TABLE app.simple_lifecycle ORDER BY n DESC, id ASC LIMIT 2 OFFSET 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    from_table = parser_test_child_at(statement, 1U);
    order_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    order_items = parser_test_child_at(order_clause, 0U);
    limit_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_LIMIT_CLAUSE);
    failures += parser_test_expect_span_text(
        parser_test_child_at(from_table, 0U),
        "app.simple_lifecycle",
        "schema table"
    );
    failures +=
        parser_test_expect_node(order_items, MYLITE_SQL_AST_ORDER_BY_ITEM_LIST, "table order list");
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(order_items, 0U), 0U),
        "n",
        "first table key"
    );
    failures += parser_test_expect_order_direction(
        parser_test_child_at(parser_test_child_at(order_items, 0U), 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_DESC,
        "first table key direction"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(order_items, 1U), 0U),
        "id",
        "second table key"
    );
    failures += parser_test_expect_order_direction(
        parser_test_child_at(parser_test_child_at(order_items, 1U), 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_ASC,
        "second table key direction"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(limit_clause, 0U),
        "2",
        "table limit row count"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(limit_clause, 1U),
        "1",
        "table limit offset"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "TABLE simple_lifecycle ORDER BY simple_lifecycle.id LIMIT 1, 2;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    order_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    limit_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_LIMIT_CLAUSE);
    failures += parser_test_expect_node(
        parser_test_child_at(order_clause, 0U),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "qualified table order key"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(limit_clause, 0U),
        "2",
        "table comma limit row count"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(limit_clause, 1U),
        "1",
        "table comma limit offset"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("TABLE simple_lifecycle LIMIT 0;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    limit_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_LIMIT_CLAUSE);
    failures += parser_test_expect_span_text(
        parser_test_child_at(limit_clause, 0U),
        "0",
        "table zero limit"
    );
    failures += parser_test_expect_true(
        parser_test_child_at(limit_clause, 1U) == NULL,
        "table simple limit no offset"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "TABLE simple_lifecycle WHERE id = 1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "TABLE simple_lifecycle AS s;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "TABLE simple_lifecycle LIMIT +1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "TABLE simple_lifecycle ORDER BY 1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "TABLE simple_lifecycle ORDER BY FIELD(name, 'b', 'a');",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "TABLE simple_lifecycle UNION SELECT * FROM t;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_values_statement(void) {
    enum {
        values_first_row_child_count = 7U,
        values_false_child_index = 5U,
        values_default_child_index = 6U,
    };
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *rows = NULL;
    const struct mylite_sql_ast_node *first_row = NULL;
    const struct mylite_sql_ast_node *second_row = NULL;
    const struct mylite_sql_ast_node *order_clause = NULL;
    const struct mylite_sql_ast_node *order_items = NULL;
    const struct mylite_sql_ast_node *limit_clause = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "VALUES ROW(1, -2, NULL, 'a', TRUE, FALSE, DEFAULT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    rows = parser_test_child_at(statement, 0U);
    first_row = parser_test_child_at(rows, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_VALUES_STATEMENT, "values statement");
    failures += parser_test_expect_node(rows, MYLITE_SQL_AST_VALUES_ROW_LIST, "values row list");
    failures += parser_test_expect_node(first_row, MYLITE_SQL_AST_VALUES_ROW, "values first row");
    failures += parser_test_expect_child_count(
        first_row,
        values_first_row_child_count,
        "values row child count"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(first_row, 0U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "integer"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(first_row, 1U),
        MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        "negative integer"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(first_row, 2U),
        MYLITE_SQL_AST_LITERAL_NULL,
        "null"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(first_row, 3U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "string"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(first_row, 4U),
        MYLITE_SQL_AST_LITERAL_TRUE,
        "true"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(first_row, values_false_child_index),
        MYLITE_SQL_AST_LITERAL_FALSE,
        "false"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(first_row, values_default_child_index),
        MYLITE_SQL_AST_DML_DEFAULT_VALUE,
        "default value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "VALUES ROW(1), ROW(2) ORDER BY column_0 DESC, 1 ASC LIMIT 1, 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    rows = parser_test_child_at(statement, 0U);
    first_row = parser_test_child_at(rows, 0U);
    second_row = parser_test_child_at(rows, 1U);
    order_clause = parser_test_child_at(statement, 1U);
    order_items = parser_test_child_at(order_clause, 0U);
    limit_clause = parser_test_child_at(statement, 2U);
    failures += parser_test_expect_child_count(rows, 2U, "values row count");
    failures +=
        parser_test_expect_span_text(parser_test_child_at(first_row, 0U), "1", "first values row");
    failures += parser_test_expect_span_text(
        parser_test_child_at(second_row, 0U),
        "2",
        "second values row"
    );
    failures += parser_test_expect_node(
        order_items,
        MYLITE_SQL_AST_ORDER_BY_ITEM_LIST,
        "values order list"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(order_items, 0U), 0U),
        "column_0",
        "values order name"
    );
    failures += parser_test_expect_order_direction(
        parser_test_child_at(parser_test_child_at(order_items, 0U), 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_DESC,
        "values order desc"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(order_items, 1U), 0U),
        "1",
        "values order ordinal"
    );
    failures += parser_test_expect_order_direction(
        parser_test_child_at(parser_test_child_at(order_items, 1U), 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_ASC,
        "values order asc"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(limit_clause, 0U),
        "1",
        "values comma limit row count"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(limit_clause, 1U),
        "1",
        "values comma limit offset"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "VALUES ROW('a') ORDER BY `column_0` LIMIT 1 OFFSET 0;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    order_clause = parser_test_child_at(statement, 1U);
    limit_clause = parser_test_child_at(statement, 2U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(order_clause, 0U),
        "`column_0`",
        "quoted values order"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(limit_clause, 0U),
        "1",
        "values offset limit row count"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(limit_clause, 1U),
        "0",
        "values offset limit offset"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("VALUES ROW();", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    rows = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_child_count(
        parser_test_child_at(rows, 0U),
        0U,
        "empty values row parses"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("VALUES (1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("VALUES ROW(1) AS v;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("VALUES ROW(1) WHERE TRUE;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("VALUES ROW(1) LIMIT +1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("VALUES ROW(1 + 2);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("VALUES ROW(ABS(1));", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("VALUES ROW(?);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("VALUES ROW(1.5);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("VALUES ROW(0x1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("VALUES ROW(b'1');", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_select_table_alias_clause(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *from_table = NULL;
    const struct mylite_sql_ast_node *hint_list = NULL;
    const struct mylite_sql_ast_node *hint = NULL;
    const struct mylite_sql_ast_node *name_list = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *order_clause = NULL;
    const struct mylite_sql_ast_node *limit_clause = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT n FROM simple_lifecycle AS s WHERE n IS NOT NULL ORDER BY n DESC "
        "LIMIT 1 OFFSET 0;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    from_table = parser_test_child_at(statement, 1U);
    order_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    limit_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_LIMIT_CLAUSE);
    failures += parser_test_expect_node(from_table, MYLITE_SQL_AST_FROM_TABLE, "alias from table");
    failures += parser_test_expect_child_count(from_table, 2U, "alias from table child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(from_table, 0U),
        "simple_lifecycle",
        "alias table"
    );
    failures += parser_test_expect_span_text(parser_test_child_at(from_table, 1U), "s", "as alias");
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 2U),
        MYLITE_SQL_AST_WHERE_CLAUSE,
        "alias where"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(order_clause, 0U),
        "n",
        "alias order key"
    );
    failures += parser_test_expect_order_direction(
        parser_test_child_at(order_clause, 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_DESC,
        "alias desc direction"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(limit_clause, 0U),
        "1",
        "alias limit row count"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(limit_clause, 1U),
        "0",
        "alias limit offset"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT * FROM app.simple_lifecycle s ORDER BY id LIMIT 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    from_table = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 0U), 0U), 0U),
        MYLITE_SQL_AST_WILDCARD,
        "bare alias wildcard"
    );
    failures += parser_test_expect_child_count(from_table, 2U, "bare alias child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(from_table, 0U),
        "app.simple_lifecycle",
        "schema table"
    );
    failures +=
        parser_test_expect_span_text(parser_test_child_at(from_table, 1U), "s", "bare alias");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT ALL n FROM simple_lifecycle AS `select` ORDER BY n;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    from_table = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DEFAULT,
        "all alias modifier"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(from_table, 1U),
        "`select`",
        "quoted alias"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT DISTINCT n FROM simple_lifecycle s ORDER BY n;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    from_table = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DISTINCT,
        "distinct alias modifier"
    );
    failures +=
        parser_test_expect_span_text(parser_test_child_at(from_table, 1U), "s", "distinct alias");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT DISTINCTROW n FROM simple_lifecycle AS s ORDER BY n;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    from_table = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DISTINCT,
        "distinctrow alias modifier"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(from_table, 1U),
        "s",
        "distinctrow alias"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT COUNT(*) FROM simple_lifecycle AS s;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(statement, 0U);
    from_table = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 0U), 0U),
        MYLITE_SQL_AST_COUNT_STAR_FUNCTION,
        "count alias function"
    );
    failures +=
        parser_test_expect_span_text(parser_test_child_at(from_table, 1U), "s", "count alias");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT MIN(n) FROM simple_lifecycle s;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    from_table = parser_test_child_at(statement, 1U);
    failures +=
        parser_test_expect_span_text(parser_test_child_at(from_table, 1U), "s", "min alias");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle USE INDEX (k_id) WHERE id = 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    from_table = parser_test_child_at(statement, 1U);
    hint_list = parser_test_child_at(from_table, 1U);
    hint = parser_test_child_at(hint_list, 0U);
    name_list = parser_test_child_at(hint, 0U);
    failures += parser_test_expect_child_count(from_table, 2U, "hint without alias child count");
    failures += parser_test_expect_node(hint_list, MYLITE_SQL_AST_INDEX_HINT_LIST, "use hint list");
    failures += parser_test_expect_node(hint, MYLITE_SQL_AST_USE_INDEX_HINT, "use index hint");
    failures +=
        parser_test_expect_node(name_list, MYLITE_SQL_AST_IDENTIFIER_LIST, "use index names");
    failures +=
        parser_test_expect_span_text(parser_test_child_at(name_list, 0U), "k_id", "use index name");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle AS s FORCE KEY FOR ORDER BY (PRIMARY, k_n) "
        "ORDER BY s.id;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    from_table = parser_test_child_at(statement, 1U);
    hint_list = parser_test_child_at(from_table, 2U);
    hint = parser_test_child_at(hint_list, 0U);
    failures += parser_test_expect_child_count(from_table, 3U, "hint with alias child count");
    failures +=
        parser_test_expect_span_text(parser_test_child_at(from_table, 1U), "s", "hint alias");
    failures +=
        parser_test_expect_node(hint_list, MYLITE_SQL_AST_INDEX_HINT_LIST, "force hint list");
    failures += parser_test_expect_node(hint, MYLITE_SQL_AST_FORCE_INDEX_HINT, "force key hint");
    failures += parser_test_expect_node(
        parser_test_child_at(hint, 0U),
        MYLITE_SQL_AST_INDEX_HINT_FOR_ORDER_BY,
        "force order scope"
    );
    name_list = parser_test_child_at(hint, 1U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(name_list, 0U),
        "PRIMARY",
        "primary hint name"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(name_list, 1U),
        "k_n",
        "force second hint name"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle USE INDEX ();",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    from_table = parser_test_child_at(statement, 1U);
    hint = parser_test_child_at(parser_test_child_at(from_table, 1U), 0U);
    name_list = parser_test_child_at(hint, 0U);
    failures += parser_test_expect_child_count(name_list, 0U, "empty use index names");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle FORCE INDEX ();",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "SELECT id FROM simple_lifecycle IGNORE INDEX ();",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT n FROM simple_lifecycle AS;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "SELECT n FROM simple_lifecycle AS WHERE n = 1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_select_inner_join_clause(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *from_join = NULL;
    const struct mylite_sql_ast_node *nested_join = NULL;
    const struct mylite_sql_ast_node *left_source = NULL;
    const struct mylite_sql_ast_node *right_source = NULL;
    const struct mylite_sql_ast_node *third_source = NULL;
    const struct mylite_sql_ast_node *hint_list = NULL;
    const struct mylite_sql_ast_node *hint = NULL;
    const struct mylite_sql_ast_node *condition = NULL;
    const struct mylite_sql_ast_node *order_clause = NULL;
    const struct mylite_sql_ast_node *limit_clause = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT l.id, r.w FROM lefts AS l JOIN rights AS r ON l.k = r.k "
        "WHERE l.v = 100 ORDER BY r.w LIMIT 1 OFFSET 0;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    from_join = parser_test_child_at(statement, 1U);
    left_source = parser_test_child_at(from_join, 0U);
    right_source = parser_test_child_at(from_join, 1U);
    condition = parser_test_child_at(from_join, 2U);
    order_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    limit_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_LIMIT_CLAUSE);
    failures += parser_test_expect_node(from_join, MYLITE_SQL_AST_FROM_JOIN, "join from clause");
    failures += parser_test_expect_child_count(from_join, 3U, "join child count");
    failures += parser_test_expect_true(
        mylite_sql_ast_node_join_kind(from_join) == MYLITE_SQL_AST_JOIN_KIND_INNER,
        "join kind"
    );
    failures += parser_test_expect_node(left_source, MYLITE_SQL_AST_FROM_TABLE, "join left source");
    failures += parser_test_expect_span_text(
        parser_test_child_at(left_source, 0U),
        "lefts",
        "join left table"
    );
    failures +=
        parser_test_expect_span_text(parser_test_child_at(left_source, 1U), "l", "join left alias");
    failures +=
        parser_test_expect_node(right_source, MYLITE_SQL_AST_FROM_TABLE, "join right source");
    failures += parser_test_expect_span_text(
        parser_test_child_at(right_source, 0U),
        "rights",
        "join right table"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(right_source, 1U),
        "r",
        "join right alias"
    );
    failures += parser_test_expect_node(
        condition,
        MYLITE_SQL_AST_COMPARISON_PREDICATE,
        "join on condition"
    );
    failures += parser_test_expect_operator(
        condition,
        MYLITE_SQL_AST_OPERATOR_EQUAL,
        "join equality operator"
    );
    failures +=
        parser_test_expect_span_text(parser_test_child_at(condition, 0U), "l.k", "join left key");
    failures +=
        parser_test_expect_span_text(parser_test_child_at(condition, 1U), "r.k", "join right key");
    failures += parser_test_expect_span_text(
        parser_test_child_at(order_clause, 0U),
        "r.w",
        "join order key"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(limit_clause, 0U),
        "1",
        "join limit row count"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(limit_clause, 1U),
        "0",
        "join limit offset"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT l.id FROM lefts AS l USE INDEX (k_l) "
        "JOIN rights r IGNORE KEY FOR JOIN (k_r, PRIMARY) ON l.k = r.k;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    from_join = parser_test_child_at(statement, 1U);
    left_source = parser_test_child_at(from_join, 0U);
    right_source = parser_test_child_at(from_join, 1U);
    hint_list = parser_test_child_at(left_source, 2U);
    hint = parser_test_child_at(hint_list, 0U);
    failures +=
        parser_test_expect_node(hint_list, MYLITE_SQL_AST_INDEX_HINT_LIST, "join left hint list");
    failures += parser_test_expect_node(hint, MYLITE_SQL_AST_USE_INDEX_HINT, "join left use hint");
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(hint, 0U), 0U),
        "k_l",
        "join left hint name"
    );
    hint_list = parser_test_child_at(right_source, 2U);
    hint = parser_test_child_at(hint_list, 0U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(right_source, 1U),
        "r",
        "join right alias stable"
    );
    failures +=
        parser_test_expect_node(hint, MYLITE_SQL_AST_IGNORE_INDEX_HINT, "join right ignore hint");
    failures += parser_test_expect_node(
        parser_test_child_at(hint, 0U),
        MYLITE_SQL_AST_INDEX_HINT_FOR_JOIN,
        "join right hint scope"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(hint, 1U), 1U),
        "PRIMARY",
        "join primary hint"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT * FROM lefts CROSS JOIN rights;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    from_join = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 0U), 0U), 0U),
        MYLITE_SQL_AST_WILDCARD,
        "cross join wildcard"
    );
    failures +=
        parser_test_expect_node(from_join, MYLITE_SQL_AST_FROM_JOIN, "cross join from clause");
    failures += parser_test_expect_child_count(from_join, 2U, "cross join omits condition child");
    failures += parser_test_expect_true(
        mylite_sql_ast_node_join_kind(from_join) == MYLITE_SQL_AST_JOIN_KIND_INNER,
        "cross join kind"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(from_join, 0U), 0U),
        "lefts",
        "cross left"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(from_join, 1U), 0U),
        "rights",
        "cross right"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT l.id, r.w FROM lefts AS l, rights AS r WHERE l.k = r.k "
        "ORDER BY r.w LIMIT 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    from_join = parser_test_child_at(statement, 1U);
    left_source = parser_test_child_at(from_join, 0U);
    right_source = parser_test_child_at(from_join, 1U);
    order_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    limit_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_LIMIT_CLAUSE);
    failures +=
        parser_test_expect_node(from_join, MYLITE_SQL_AST_FROM_JOIN, "comma join from clause");
    failures += parser_test_expect_child_count(from_join, 2U, "comma join child count");
    failures += parser_test_expect_true(
        mylite_sql_ast_node_join_kind(from_join) == MYLITE_SQL_AST_JOIN_KIND_INNER,
        "comma join kind"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(left_source, 0U),
        "lefts",
        "comma left table"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(left_source, 1U),
        "l",
        "comma left alias"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(right_source, 0U),
        "rights",
        "comma right table"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(right_source, 1U),
        "r",
        "comma right alias"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(order_clause, 0U),
        "r.w",
        "comma order key"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(limit_clause, 0U),
        "1",
        "comma limit row count"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT * FROM lefts, rights;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    from_join = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 0U), 0U), 0U),
        MYLITE_SQL_AST_WILDCARD,
        "comma join wildcard"
    );
    failures += parser_test_expect_node(from_join, MYLITE_SQL_AST_FROM_JOIN, "comma wildcard join");
    failures += parser_test_expect_child_count(from_join, 2U, "comma wildcard child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT l.id FROM lefts l JOIN rights r ON l.k = r.k "
        "JOIN extras e ON r.id = e.id;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    from_join = parser_test_child_at(statement, 1U);
    nested_join = parser_test_child_at(from_join, 0U);
    third_source = parser_test_child_at(from_join, 1U);
    condition = parser_test_child_at(from_join, 2U);
    failures +=
        parser_test_expect_node(from_join, MYLITE_SQL_AST_FROM_JOIN, "multi-source join root");
    failures +=
        parser_test_expect_node(nested_join, MYLITE_SQL_AST_FROM_JOIN, "multi-source nested join");
    failures += parser_test_expect_child_count(from_join, 3U, "multi-source join child count");
    failures +=
        parser_test_expect_child_count(nested_join, 3U, "multi-source nested join child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(nested_join, 0U), 0U),
        "lefts",
        "multi-source first table"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(nested_join, 1U), 0U),
        "rights",
        "multi-source second table"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(third_source, 0U),
        "extras",
        "multi-source third table"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(condition, 0U),
        "r.id",
        "multi-source final left key"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(condition, 1U),
        "e.id",
        "multi-source final right key"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT l.id FROM lefts l INNER JOIN rights r ON l.k = r.k;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    from_join = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_node(from_join, MYLITE_SQL_AST_FROM_JOIN, "inner join");
    failures += parser_test_expect_true(
        mylite_sql_ast_node_join_kind(from_join) == MYLITE_SQL_AST_JOIN_KIND_INNER,
        "inner join kind"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT l.id FROM lefts l STRAIGHT_JOIN rights r ON l.k = r.k;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    from_join = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_node(from_join, MYLITE_SQL_AST_FROM_JOIN, "straight join");
    failures += parser_test_expect_true(
        mylite_sql_ast_node_join_kind(from_join) == MYLITE_SQL_AST_JOIN_KIND_INNER,
        "straight join kind"
    );
    failures += parser_test_expect_child_count(from_join, 3U, "straight join child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT l.id FROM lefts l STRAIGHT_JOIN rights r;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    from_join = parser_test_child_at(statement, 1U);
    failures +=
        parser_test_expect_node(from_join, MYLITE_SQL_AST_FROM_JOIN, "straight cartesian join");
    failures += parser_test_expect_true(
        mylite_sql_ast_node_join_kind(from_join) == MYLITE_SQL_AST_JOIN_KIND_INNER,
        "straight cartesian join kind"
    );
    failures +=
        parser_test_expect_child_count(from_join, 2U, "straight cartesian join child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT l.id FROM lefts l STRAIGHT_JOIN rights r ON l.k = r.k "
        "STRAIGHT_JOIN extras e ON r.id = e.id;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    from_join = parser_test_child_at(statement, 1U);
    nested_join = parser_test_child_at(from_join, 0U);
    failures += parser_test_expect_node(from_join, MYLITE_SQL_AST_FROM_JOIN, "straight chain root");
    failures +=
        parser_test_expect_node(nested_join, MYLITE_SQL_AST_FROM_JOIN, "straight chain nested");
    failures += parser_test_expect_true(
        mylite_sql_ast_node_join_kind(from_join) == MYLITE_SQL_AST_JOIN_KIND_INNER,
        "straight chain root kind"
    );
    failures += parser_test_expect_true(
        mylite_sql_ast_node_join_kind(nested_join) == MYLITE_SQL_AST_JOIN_KIND_INNER,
        "straight chain nested kind"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT l.id FROM lefts l LEFT JOIN rights r ON l.k = r.k;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    from_join = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_node(from_join, MYLITE_SQL_AST_FROM_JOIN, "left join");
    failures += parser_test_expect_true(
        mylite_sql_ast_node_join_kind(from_join) == MYLITE_SQL_AST_JOIN_KIND_LEFT_OUTER,
        "left join kind"
    );
    failures += parser_test_expect_child_count(from_join, 3U, "left join child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT l.id FROM lefts l LEFT JOIN rights r ON l.k = r.k "
        "LEFT JOIN extras e ON r.id = e.id;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    from_join = parser_test_child_at(statement, 1U);
    nested_join = parser_test_child_at(from_join, 0U);
    failures += parser_test_expect_node(from_join, MYLITE_SQL_AST_FROM_JOIN, "left chain root");
    failures += parser_test_expect_node(nested_join, MYLITE_SQL_AST_FROM_JOIN, "left chain nested");
    failures += parser_test_expect_true(
        mylite_sql_ast_node_join_kind(from_join) == MYLITE_SQL_AST_JOIN_KIND_LEFT_OUTER,
        "left chain root kind"
    );
    failures += parser_test_expect_true(
        mylite_sql_ast_node_join_kind(nested_join) == MYLITE_SQL_AST_JOIN_KIND_LEFT_OUTER,
        "left chain nested kind"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT l.id FROM lefts l LEFT OUTER JOIN rights r ON l.k = r.k;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    from_join = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_node(from_join, MYLITE_SQL_AST_FROM_JOIN, "left outer join");
    failures += parser_test_expect_true(
        mylite_sql_ast_node_join_kind(from_join) == MYLITE_SQL_AST_JOIN_KIND_LEFT_OUTER,
        "left outer join kind"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT l.id FROM lefts l RIGHT JOIN rights r ON l.k = r.k;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    from_join = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_node(from_join, MYLITE_SQL_AST_FROM_JOIN, "right join");
    failures += parser_test_expect_true(
        mylite_sql_ast_node_join_kind(from_join) == MYLITE_SQL_AST_JOIN_KIND_RIGHT_OUTER,
        "right join kind"
    );
    failures += parser_test_expect_child_count(from_join, 3U, "right join child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT l.id FROM lefts l RIGHT OUTER JOIN rights r ON l.k = r.k;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    from_join = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_node(from_join, MYLITE_SQL_AST_FROM_JOIN, "right outer join");
    failures += parser_test_expect_true(
        mylite_sql_ast_node_join_kind(from_join) == MYLITE_SQL_AST_JOIN_KIND_RIGHT_OUTER,
        "right outer join kind"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT l.id FROM lefts l, rights r, extras e;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    from_join = parser_test_child_at(statement, 1U);
    nested_join = parser_test_child_at(from_join, 0U);
    third_source = parser_test_child_at(from_join, 1U);
    failures +=
        parser_test_expect_node(from_join, MYLITE_SQL_AST_FROM_JOIN, "multi-source comma root");
    failures +=
        parser_test_expect_node(nested_join, MYLITE_SQL_AST_FROM_JOIN, "multi-source comma nested");
    failures += parser_test_expect_child_count(from_join, 2U, "multi-source comma child count");
    failures +=
        parser_test_expect_child_count(nested_join, 2U, "multi-source nested comma child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(nested_join, 0U), 0U),
        "lefts",
        "multi-source comma first table"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(nested_join, 1U), 0U),
        "rights",
        "multi-source comma second table"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(third_source, 0U),
        "extras",
        "multi-source comma third table"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "SELECT l.id FROM lefts l, rights r JOIN extras e ON r.id = e.id;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "SELECT id FROM t STRAIGHT_JOIN other USING (id);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_select_item_alias_clause(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_item = NULL;
    const struct mylite_sql_ast_node *second_item = NULL;
    const struct mylite_sql_ast_node *third_item = NULL;
    const struct mylite_sql_ast_node *order_clause = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT n AS x, nn y, nums.n AS `Customer identity`, n 'literal alias' "
        "FROM numbers AS nums ORDER BY x DESC LIMIT 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(statement, 0U);
    first_item = parser_test_child_at(select_list, 0U);
    second_item = parser_test_child_at(select_list, 1U);
    third_item = parser_test_child_at(select_list, 2U);
    order_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    failures += parser_test_expect_child_count(select_list, 4U, "select item alias count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(first_item, 0U),
        "n",
        "as alias expression"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(first_item, 1U),
        "x",
        "as alias identifier"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(second_item, 0U),
        "nn",
        "bare alias expression"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(second_item, 1U),
        "y",
        "bare alias identifier"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(third_item, 0U),
        "nums.n",
        "qualified alias expression"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(third_item, 1U),
        "`Customer identity`",
        "quoted identifier select alias"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(parser_test_child_at(select_list, 3U), 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "string select alias"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(order_clause, 0U),
        "x",
        "alias order key"
    );
    failures += parser_test_expect_order_direction(
        parser_test_child_at(order_clause, 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_DESC,
        "alias order direction"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT DISTINCT n AS x FROM numbers ORDER BY x;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DISTINCT,
        "distinct select item alias modifier"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(select_list, 0U), 1U),
        "x",
        "distinct select item alias"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT COUNT(*) AS c FROM numbers;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 0U), 0U),
        MYLITE_SQL_AST_COUNT_STAR_FUNCTION,
        "count select item alias expression"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(select_list, 0U), 1U),
        "c",
        "count alias"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT DATABASE() AS d, USER() u FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(select_list, 0U), 1U),
        "d",
        "database function alias"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(select_list, 1U), 1U),
        "u",
        "user function alias"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT 1, BOGUS(1) FROM bogus;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(statement, 0U);
    {
        const struct mylite_sql_ast_node *function =
            parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
        failures +=
            parser_test_expect_node(function, MYLITE_SQL_AST_GENERIC_FUNCTION, "generic function");
        failures += parser_test_expect_span_text(
            parser_test_child_at(function, 0U),
            "BOGUS",
            "generic function name"
        );
        failures += parser_test_expect_node(
            parser_test_child_at(function, 1U),
            MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST,
            "generic function arguments"
        );
    }
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT * AS x FROM numbers;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "SELECT n AS 'x' FROM numbers ORDER BY 'x';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}
