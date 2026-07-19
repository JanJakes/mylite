#include "parser_test_support.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    deep_snapshot_node_count = 4096,
    parsed_snapshot_minimum_node_count = 5,
    decimal_source_length = 13,
    decimal_precision_offset = 8,
    decimal_scale_offset = 11,
};

static int test_parsed_expression_snapshot_owns_source(void);
static int test_payload_spans_are_rebased(void);
static int test_deep_snapshot_is_iterative(void);
static int expect_true(int condition, const char *label);

int main(void) {
    int failures = 0;

    failures += test_parsed_expression_snapshot_owns_source();
    failures += test_payload_spans_are_rebased();
    failures += test_deep_snapshot_is_iterative();

    return failures == 0 ? 0 : 1;
}

static int test_parsed_expression_snapshot_owns_source(void) {
    static const char sql_text[] =
        "UPDATE t SET value = IF(1, CONCAT('left', 'right'), CURRENT_TIMESTAMP(3));";
    struct mylite_sql_ast_snapshot snapshot;
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *assignment = NULL;
    const struct mylite_sql_ast_node *value = NULL;
    const struct mylite_sql_ast_node *snapshot_root = NULL;
    char *sql = (char *)malloc(sizeof(sql_text));
    bool found_concat = false;
    int failures = 0;

    failures += expect_true(sql != NULL, "allocate parsed snapshot SQL");
    if (sql == NULL) {
        return failures;
    }
    memcpy(sql, sql_text, sizeof(sql_text));
    failures += parser_test_parse_sql(sql, MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    assignment = parser_test_child_at(parser_test_child_at(statement, 1U), 0U);
    value = parser_test_child_at(assignment, 1U);
    failures += parser_test_expect_node(value, MYLITE_SQL_AST_IF_FUNCTION, "snapshot source IF");

    mylite_sql_ast_snapshot_init(&snapshot);
    failures += expect_true(
        mylite_sql_ast_snapshot_clone_subtree(&snapshot, value),
        "clone parsed expression snapshot"
    );
    failures += expect_true(
        snapshot.node_count > (size_t)parsed_snapshot_minimum_node_count,
        "parsed snapshot node count"
    );
    failures += expect_true(snapshot.source != sql, "parsed snapshot source is independent");

    mylite_sql_parse_result_deinit(&result);
    memset(sql, 'x', sizeof(sql_text) - 1U);
    free(sql);

    snapshot_root = mylite_sql_ast_snapshot_root(&snapshot);
    failures +=
        parser_test_expect_node(snapshot_root, MYLITE_SQL_AST_IF_FUNCTION, "snapshot root IF");
    failures += parser_test_expect_span_text(
        snapshot_root,
        "IF(1, CONCAT('left', 'right'), CURRENT_TIMESTAMP(3))",
        "snapshot root source"
    );
    for (size_t index = 0U; index < snapshot.node_count; ++index) {
        const struct mylite_sql_source_span span = snapshot.nodes[index].span;

        found_concat = found_concat || snapshot.nodes[index].kind == MYLITE_SQL_AST_CONCAT_FUNCTION;
        failures += expect_true(
            span.text == snapshot.source + span.offset,
            "snapshot node span points into owned source"
        );
    }
    failures += expect_true(found_concat, "snapshot preserves nested CONCAT");

    mylite_sql_ast_snapshot_deinit(&snapshot);
    return failures;
}

static int test_payload_spans_are_rebased(void) {
    char source[] = "DECIMAL(10,2)";
    struct mylite_sql_ast_node node = {
        .kind = MYLITE_SQL_AST_DECIMAL_TYPE,
        .span =
            {
                .text = source,
                .length = decimal_source_length,
                .offset = 0U,
                .source_length = decimal_source_length,
            },
        .payload.decimal_type =
            {
                .has_precision = 1,
                .has_scale = 1,
                .precision_span =
                    {
                        .text = source + decimal_precision_offset,
                        .length = 2U,
                        .offset = decimal_precision_offset,
                        .source_length = decimal_source_length,
                    },
                .scale_span =
                    {
                        .text = source + decimal_scale_offset,
                        .length = 1U,
                        .offset = decimal_scale_offset,
                        .source_length = decimal_source_length,
                    },
            },
    };
    struct mylite_sql_ast_snapshot snapshot;
    const struct mylite_sql_ast_node *snapshot_root = NULL;
    int failures = 0;

    mylite_sql_ast_snapshot_init(&snapshot);
    failures += expect_true(
        mylite_sql_ast_snapshot_clone_subtree(&snapshot, &node),
        "clone payload-span snapshot"
    );
    memset(source, 'x', sizeof(source) - 1U);
    snapshot_root = mylite_sql_ast_snapshot_root(&snapshot);
    failures += expect_true(
        snapshot_root->payload.decimal_type.precision_span.text ==
            snapshot.source + decimal_precision_offset,
        "snapshot precision span is rebased"
    );
    failures += expect_true(
        snapshot_root->payload.decimal_type.scale_span.text ==
            snapshot.source + decimal_scale_offset,
        "snapshot scale span is rebased"
    );
    failures += expect_true(
        memcmp(snapshot_root->payload.decimal_type.precision_span.text, "10", 2U) == 0,
        "snapshot precision text"
    );
    failures += expect_true(
        snapshot_root->payload.decimal_type.scale_span.text[0] == '2',
        "snapshot scale text"
    );

    mylite_sql_ast_snapshot_deinit(&snapshot);
    return failures;
}

static int test_deep_snapshot_is_iterative(void) {
    static const char source[] = "1";
    struct mylite_sql_ast_node *nodes =
        (struct mylite_sql_ast_node *)calloc(deep_snapshot_node_count, sizeof(*nodes));
    struct mylite_sql_ast_snapshot snapshot;
    const struct mylite_sql_ast_node *current = NULL;
    size_t visited = 0U;
    int failures = 0;

    failures += expect_true(nodes != NULL, "allocate deep snapshot source nodes");
    if (nodes == NULL) {
        return failures;
    }
    for (size_t index = 0U; index < deep_snapshot_node_count; ++index) {
        nodes[index].kind = MYLITE_SQL_AST_LITERAL;
        nodes[index].span = (struct mylite_sql_source_span){
            .text = source,
            .length = 1U,
            .offset = 0U,
            .source_length = 1U,
        };
        nodes[index].payload.literal.kind = MYLITE_SQL_AST_LITERAL_INTEGER;
        if (index + 1U < deep_snapshot_node_count) {
            nodes[index].first_child = &nodes[index + 1U];
            nodes[index].last_child = &nodes[index + 1U];
        }
    }

    mylite_sql_ast_snapshot_init(&snapshot);
    failures += expect_true(
        mylite_sql_ast_snapshot_clone_subtree(&snapshot, &nodes[0]),
        "clone deep iterative snapshot"
    );
    current = mylite_sql_ast_snapshot_root(&snapshot);
    while (current != NULL) {
        ++visited;
        current = current->first_child;
    }
    failures +=
        expect_true(visited == deep_snapshot_node_count, "deep snapshot preserves every child");

    mylite_sql_ast_snapshot_deinit(&snapshot);
    free(nodes);
    return failures;
}

static int expect_true(int condition, const char *label) {
    if (condition) {
        return 0;
    }
    fprintf(stderr, "FAIL: %s\n", label);
    return 1;
}
