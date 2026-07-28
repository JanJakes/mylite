#include "parser_test_support.h"

#include "mylite_test_support.h"

#include "sql/mylite_parser_driver.h"
#include "sql/mylite_parser_placeholders.h"
#include "sql/mylite_parser_resources.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

_Static_assert(MYLITE_SQL_PARSER_RETRY_KIND_COUNT == 6, "retry strategy count may not grow");
_Static_assert(mylite_sql_parser_retry_callback_limit == 6, "retry callback ceiling may not grow");

struct expected_retry_strategy {
    enum mylite_sql_parser_retry_kind kind;
    unsigned int category_mask;
};

struct expected_retry_metrics {
    const char *sql;
    size_t callback_count;
    const char *context;
};

static int test_retry_inventory(void);
static int test_invalid_retry_kind_preserves_ast(void);
static int test_primary_grammar_replacements(void);
static int test_retained_retry_order(void);
static int expect_primary_parse(const char *sql, const char *context);
static int expect_retry_metrics(struct expected_retry_metrics expected);

int main(void) {
    int failures = 0;

    failures += test_retry_inventory();
    failures += test_invalid_retry_kind_preserves_ast();
    failures += test_primary_grammar_replacements();
    failures += test_retained_retry_order();

    return failures == 0 ? 0 : 1;
}

static int test_retry_inventory(void) {
    static const struct expected_retry_strategy strategies[] = {
        {
            .kind = MYLITE_SQL_PARSER_RETRY_ROW_CONSTRUCTOR_PREDICATE,
            .category_mask = MYLITE_SQL_PARSER_RETRY_CATEGORY_GRAMMAR_TRANSFORM,
        },
        {
            .kind = MYLITE_SQL_PARSER_RETRY_SELECT_RESULT_OPTION_REORDER,
            .category_mask = MYLITE_SQL_PARSER_RETRY_CATEGORY_TOKEN_TRANSFORM,
        },
        {
            .kind = MYLITE_SQL_PARSER_RETRY_PARENTHESIZED_ROW_CONSTRUCTOR,
            .category_mask = MYLITE_SQL_PARSER_RETRY_CATEGORY_TOKEN_TRANSFORM,
        },
        {
            .kind = MYLITE_SQL_PARSER_RETRY_ROW_ARITHMETIC_PREDICATE,
            .category_mask = MYLITE_SQL_PARSER_RETRY_CATEGORY_GRAMMAR_TRANSFORM,
        },
        {
            .kind = MYLITE_SQL_PARSER_RETRY_REPEATED_SELECT_LOCKING,
            .category_mask = MYLITE_SQL_PARSER_RETRY_CATEGORY_GRAMMAR_TRANSFORM,
        },
        {
            .kind = MYLITE_SQL_PARSER_RETRY_PLACEHOLDER,
            .category_mask = MYLITE_SQL_PARSER_RETRY_CATEGORY_GRAMMAR_TRANSFORM |
                             MYLITE_SQL_PARSER_RETRY_CATEGORY_UTILITY_PLACEHOLDER |
                             MYLITE_SQL_PARSER_RETRY_CATEGORY_UNSUPPORTED_FALLBACK,
        },
    };
    int failures = 0;

    failures += parser_test_expect_true(
        sizeof(strategies) / sizeof(strategies[0]) == (size_t)MYLITE_SQL_PARSER_RETRY_KIND_COUNT,
        "retry inventory count"
    );
    for (size_t index = 0U; index < sizeof(strategies) / sizeof(strategies[0]); ++index) {
        failures += parser_test_expect_true(
            strategies[index].kind == (enum mylite_sql_parser_retry_kind)index,
            "retry inventory order"
        );
        failures += parser_test_expect_true(
            mylite_sql_parser_retry_category_mask(strategies[index].kind) ==
                strategies[index].category_mask,
            "retry inventory category"
        );
    }
    failures += parser_test_expect_true(
        mylite_sql_parser_retry_category_mask(MYLITE_SQL_PARSER_RETRY_KIND_COUNT) == 0U,
        "invalid retry category"
    );
    return failures;
}

static int test_invalid_retry_kind_preserves_ast(void) {
    static const char sql[] = "SELECT 1";
    struct mylite_sql_parse_config config = {
        .input = sql,
        .length = sizeof(sql) - 1U,
    };
    struct mylite_sql_parse_result result = {0};
    struct mylite_sql_parser_retry_context retry_context = {0};
    struct mylite_sql_ast_node *original_root = NULL;
    enum mylite_sql_parse_status status = mylite_sql_parser_parse_with_lemon(config, &result);
    bool handled = true;
    int failures = 0;

    failures += parser_test_expect_true(status == MYLITE_SQL_PARSE_OK, "invalid retry setup parse");
    original_root = result.root;
    if (status == MYLITE_SQL_PARSE_OK) {
        status = mylite_sql_parser_retry_context_init(config, &retry_context);
    }
    failures +=
        parser_test_expect_true(status == MYLITE_SQL_PARSE_OK, "invalid retry setup tokenization");
    if (status == MYLITE_SQL_PARSE_OK) {
        status = mylite_sql_parser_try_retry(
            MYLITE_SQL_PARSER_RETRY_KIND_COUNT,
            config,
            &result,
            &retry_context,
            &handled
        );
    }
    failures +=
        parser_test_expect_true(status == MYLITE_SQL_PARSE_MISUSE, "invalid retry kind status");
    failures += parser_test_expect_true(!handled, "invalid retry kind remains unhandled");
    failures += parser_test_expect_true(
        result.root == original_root && result.status == MYLITE_SQL_PARSE_OK,
        "invalid retry kind preserves parse result"
    );
    failures += parser_test_expect_true(
        mylite_sql_ast_spans_are_within_source(&result.ast, sql, sizeof(sql) - 1U),
        "invalid retry kind preserves AST spans"
    );

    mylite_sql_parser_retry_context_deinit(&retry_context);
    mylite_sql_parse_result_deinit(&result);
    return failures;
}

static int test_primary_grammar_replacements(void) {
    static const char tableless_sql[] = "SELECT 1 LIMIT 1";
    struct mylite_sql_parse_result result = {0};
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *option = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(tableless_sql, MYLITE_SQL_PARSE_OK, &result);
    failures += parser_test_expect_true(
        result.retry_tokenization_count == 0U && result.retry_callback_count == 0U &&
            result.retry_handled_count == 0U,
        "tableless limit primary metrics"
    );
    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_SELECT_STATEMENT, "tableless select AST");
    option = parser_test_first_child_kind(statement, MYLITE_SQL_AST_LIMIT_CLAUSE);
    failures += parser_test_expect_node(option, MYLITE_SQL_AST_LIMIT_CLAUSE, "tableless limit AST");
    failures +=
        parser_test_expect_span_text(parser_test_child_at(option, 0U), "1", "tableless row count");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE INDEX prefix_type TYPE BTREE ON type_table(type)",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_true(
        result.retry_tokenization_count == 0U && result.retry_callback_count == 0U &&
            result.retry_handled_count == 0U,
        "prefix TYPE primary metrics"
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_CREATE_INDEX_STATEMENT,
        "prefix TYPE create index AST"
    );
    option = parser_test_child_at(statement, 1U);
    failures +=
        parser_test_expect_node(option, MYLITE_SQL_AST_INDEX_TYPE_OPTION, "prefix TYPE option AST");
    failures +=
        parser_test_expect_span_text(parser_test_child_at(option, 0U), "BTREE", "prefix TYPE name");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE INDEX suffix_type ON type_table(type) TYPE HASH",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_true(
        result.retry_tokenization_count == 0U && result.retry_callback_count == 0U &&
            result.retry_handled_count == 0U,
        "suffix TYPE primary metrics"
    );
    statement = parser_test_child_at(result.root, 0U);
    option = parser_test_first_child_kind(statement, MYLITE_SQL_AST_INDEX_OPTION_LIST);
    failures += parser_test_expect_node(
        option,
        MYLITE_SQL_AST_INDEX_OPTION_LIST,
        "suffix TYPE option list"
    );
    option = parser_test_child_at(option, 0U);
    failures +=
        parser_test_expect_node(option, MYLITE_SQL_AST_INDEX_TYPE_OPTION, "suffix TYPE option AST");
    failures +=
        parser_test_expect_span_text(parser_test_child_at(option, 0U), "HASH", "suffix TYPE name");
    mylite_sql_parse_result_deinit(&result);

    failures += expect_primary_parse("CREATE TABLE type(type INT)", "TYPE DDL identifiers");
    failures += expect_primary_parse("SELECT type FROM type", "TYPE query identifiers");
    failures += expect_primary_parse("SELECT 1 LIMIT 1, 2", "tableless comma limit");
    failures += expect_primary_parse("SELECT 1 LIMIT 2 OFFSET 1", "tableless offset limit");
    return failures;
}

static int test_retained_retry_order(void) {
    static const struct expected_retry_metrics expectations[] = {
        {
            .sql = "SELECT * FROM t WHERE (a,b) = (1,2)",
            .callback_count = 1U,
            .context = "row constructor predicate retry",
        },
        {
            .sql = "SELECT SQL_BIG_RESULT DISTINCT 1",
            .callback_count = 2U,
            .context = "result option reorder retry",
        },
        {
            .sql = "SELECT (1, 2)",
            .callback_count = 3U,
            .context = "parenthesized row constructor retry",
        },
        {
            .sql = "SELECT 1 WHERE (1 + 1) > 1",
            .callback_count = 4U,
            .context = "row arithmetic retry",
        },
        {
            .sql = "SELECT 1 FOR UPDATE FOR SHARE",
            .callback_count = 5U,
            .context = "repeated locking retry",
        },
        {
            .sql = "SHOW EXTENDED COLUMNS FROM t",
            .callback_count = 6U,
            .context = "residual placeholder retry",
        },
    };
    int failures = 0;

    for (size_t index = 0U; index < sizeof(expectations) / sizeof(expectations[0]); ++index) {
        failures += expect_retry_metrics(expectations[index]);
    }
    failures += expect_retry_metrics((struct expected_retry_metrics){
        .sql = "SELECT FROM DUAL",
        .callback_count = 6U,
        .context = "unhandled syntax retry ceiling",
    });
    return failures;
}

static int expect_primary_parse(const char *sql, const char *context) {
    struct mylite_sql_parse_result result = {0};
    int failures = parser_test_parse_sql(sql, MYLITE_SQL_PARSE_OK, &result);

    failures += parser_test_expect_true(
        result.retry_tokenization_count == 0U && result.retry_callback_count == 0U &&
            result.retry_handled_count == 0U,
        context
    );
    mylite_sql_parse_result_deinit(&result);
    return failures;
}

static int expect_retry_metrics(struct expected_retry_metrics expected) {
    struct mylite_sql_parse_result result = {0};
    enum mylite_sql_parse_status expected_status =
        expected.callback_count == 6U && strcmp(expected.sql, "SELECT FROM DUAL") == 0
            ? MYLITE_SQL_PARSE_SYNTAX_ERROR
            : MYLITE_SQL_PARSE_OK;
    int failures = parser_test_parse_sql(expected.sql, expected_status, &result);

    failures += mylite_test_expect_size(result.retry_tokenization_count, 1U, expected.context);
    failures += mylite_test_expect_size(
        result.retry_callback_count,
        expected.callback_count,
        expected.context
    );
    failures += mylite_test_expect_size(
        result.retry_handled_count,
        expected_status == MYLITE_SQL_PARSE_OK ? 1U : 0U,
        expected.context
    );
    mylite_sql_parse_result_deinit(&result);
    return failures;
}
