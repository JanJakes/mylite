#include "parser_test_support.h"

static int test_select_into_user_variable_positions(void);
static int test_select_into_user_variable_duplicate_rejection(void);
static int expect_select_into_list(
    const char *sql,
    size_t expected_variable_count,
    const char *const *expected_spans,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_select_into_user_variable_positions();
    failures += test_select_into_user_variable_duplicate_rejection();

    return failures == 0 ? 0 : 1;
}

static int test_select_into_user_variable_positions(void) {
    static const char *const scalar_spans[] = {"@a"};
    static const char *const table_spans[] = {"@id", "@name"};
    static const char *const trailing_spans[] = {"@first"};
    static const char *const locking_spans[] = {"@locked"};
    int failures = 0;

    failures +=
        expect_select_into_list("SELECT 1 INTO @a;", 1U, scalar_spans, "scalar select into");
    failures += expect_select_into_list(
        "SELECT id, name INTO @id, @name FROM t WHERE id = 1;",
        2U,
        table_spans,
        "pre-FROM table select into"
    );
    failures += expect_select_into_list(
        "SELECT id FROM t ORDER BY id LIMIT 1 INTO @first;",
        1U,
        trailing_spans,
        "trailing table select into"
    );
    failures += expect_select_into_list(
        "SELECT id FROM t ORDER BY id LIMIT 1 FOR UPDATE INTO @locked;",
        1U,
        locking_spans,
        "post-locking select into"
    );

    return failures;
}

static int test_select_into_user_variable_duplicate_rejection(void) {
    struct mylite_sql_parse_result result;
    int failures = parser_test_parse_sql(
        "SELECT id INTO @id FROM t INTO @again;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );

    mylite_sql_parse_result_deinit(&result);
    return failures;
}

static int expect_select_into_list(
    const char *sql,
    size_t expected_variable_count,
    const char *const *expected_spans,
    const char *context
) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *into_list = NULL;
    int failures = parser_test_parse_sql(sql, MYLITE_SQL_PARSE_OK, &result);

    statement = parser_test_child_at(result.root, 0U);
    into_list = parser_test_first_child_kind(statement, MYLITE_SQL_AST_SELECT_INTO_LIST);

    failures += parser_test_expect_node(statement, MYLITE_SQL_AST_SELECT_STATEMENT, context);
    failures +=
        parser_test_expect_node(into_list, MYLITE_SQL_AST_SELECT_INTO_LIST, "select into list");
    failures +=
        parser_test_expect_child_count(into_list, expected_variable_count, "select into count");
    for (size_t index = 0U; index < expected_variable_count; ++index) {
        failures += parser_test_expect_node(
            parser_test_child_at(into_list, index),
            MYLITE_SQL_AST_USER_VARIABLE,
            "select into variable"
        );
        failures += parser_test_expect_span_text(
            parser_test_child_at(into_list, index),
            expected_spans[index],
            "select into variable span"
        );
    }

    mylite_sql_parse_result_deinit(&result);
    return failures;
}
