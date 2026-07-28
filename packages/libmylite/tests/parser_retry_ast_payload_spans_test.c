#include "parser_test_support.h"
#include "sql/mylite_parser_driver.h"
#include "sql/mylite_parser_placeholders.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum {
    interval_root_length = 7,
    interval_payload_offset = 8,
    node_search_stack_capacity = 64,
};

static int test_partition_retry_payload_spans_and_snapshot(void);
static int test_payload_validation_and_atomic_rebase(void);
static int test_snapshot_rejects_payload_outside_root(void);
static int test_nonpayload_prefix_retries_remain_valid(void);
static int test_tableless_prefix_helper_remains_valid(void);
static int expect_snapshot_payload_inventory(const struct mylite_sql_ast_snapshot *snapshot);
static int expect_payload_span(
    struct mylite_sql_source_span span,
    const char *expected,
    const char *source,
    size_t source_length,
    const char *context
);
static int expect_zero_span(struct mylite_sql_source_span span, const char *context);
static const struct mylite_sql_ast_node *snapshot_node_kind_at(
    const struct mylite_sql_ast_snapshot *snapshot,
    enum mylite_sql_ast_node_kind kind,
    size_t occurrence
);
static struct mylite_sql_ast_node *find_node_kind(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_node_kind kind
);

int main(void) {
    int failures = 0;

    failures += test_partition_retry_payload_spans_and_snapshot();
    failures += test_payload_validation_and_atomic_rebase();
    failures += test_snapshot_rejects_payload_outside_root();
    failures += test_nonpayload_prefix_retries_remain_valid();
    failures += test_tableless_prefix_helper_remains_valid();

    return failures == 0 ? 0 : 1;
}

static int test_partition_retry_payload_spans_and_snapshot(void) {
    static const char sql_text[] =
        "CREATE TABLE payload_types ("
        "id INT NOT NULL, i INT(11), v VARCHAR(12), c CHAR(3), t TEXT(12), "
        "b BINARY(4), vb VARBINARY(5), bits BIT(6), y YEAR(4), "
        "d DECIMAL(10,2), f FLOAT(9,3), plain_date DATE, "
        "dt DATETIME(4), ts TIMESTAMP(5), tm TIME(6), "
        "PRIMARY KEY(id)) PARTITION BY HASH(id) PARTITIONS 2";
    struct mylite_sql_parse_result result = {0};
    struct mylite_sql_ast_snapshot snapshot;
    struct mylite_sql_ast_node *statement = NULL;
    char *sql = (char *)malloc(sizeof(sql_text));
    int failures = 0;

    failures += parser_test_expect_true(sql != NULL, "allocate retry payload SQL");
    if (sql == NULL) {
        return failures;
    }
    memcpy(sql, sql_text, sizeof(sql_text));
    failures += parser_test_parse_sql(sql, MYLITE_SQL_PARSE_OK, &result);
    failures += parser_test_expect_true(
        result.retry_handled_count == 1U,
        "partition payload statement uses one retry"
    );
    failures += parser_test_expect_true(
        mylite_sql_ast_spans_are_within_source(&result.ast, sql, sizeof(sql_text) - 1U),
        "partition retry validates primary and payload spans"
    );
    statement = result.root == NULL ? NULL : result.root->first_child;
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        "partition payload create statement"
    );

    mylite_sql_ast_snapshot_init(&snapshot);
    if (statement != NULL) {
        failures += parser_test_expect_true(
            mylite_sql_ast_snapshot_clone_subtree(&snapshot, statement),
            "clone partition retry payload snapshot"
        );
    }
    mylite_sql_parse_result_deinit(&result);
    memset(sql, 'x', sizeof(sql_text) - 1U);
    free(sql);

    failures += expect_snapshot_payload_inventory(&snapshot);
    mylite_sql_ast_snapshot_deinit(&snapshot);
    return failures;
}

static int test_payload_validation_and_atomic_rebase(void) {
    static const char sql[] = "CREATE TABLE t (v VARCHAR(12), d DECIMAL(10,2), dt DATETIME(4))";
    const size_t source_length = sizeof(sql) - 1U;
    const size_t rebased_length = source_length + 64U;
    struct mylite_sql_parse_result result = {0};
    struct mylite_sql_ast_node *statement = NULL;
    struct mylite_sql_ast_node *varchar_type = NULL;
    struct mylite_sql_ast_node *decimal_type = NULL;
    struct mylite_sql_source_span saved_varchar_span;
    struct mylite_sql_source_span saved_scale_span;
    int failures = parser_test_parse_sql(sql, MYLITE_SQL_PARSE_OK, &result);

    failures += parser_test_expect_true(
        result.retry_handled_count == 0U,
        "primary grammar payload control has no retry"
    );
    statement = result.root == NULL ? NULL : result.root->first_child;
    varchar_type = find_node_kind(result.root, MYLITE_SQL_AST_VARCHAR_TYPE);
    decimal_type = find_node_kind(result.root, MYLITE_SQL_AST_DECIMAL_TYPE);
    failures += parser_test_expect_true(
        statement != NULL && varchar_type != NULL && decimal_type != NULL,
        "find payload validation nodes"
    );
    if (statement == NULL || varchar_type == NULL || decimal_type == NULL) {
        mylite_sql_parse_result_deinit(&result);
        return failures;
    }

    saved_varchar_span = varchar_type->payload.varchar_type.length_span;
    varchar_type->payload.varchar_type.length_span.text = sql;
    failures += parser_test_expect_true(
        !mylite_sql_ast_spans_are_within_source(&result.ast, sql, source_length),
        "payload validator rejects wrong pointer"
    );
    varchar_type->payload.varchar_type.length_span = saved_varchar_span;
    varchar_type->payload.varchar_type.length_span.source_length = source_length - 1U;
    failures += parser_test_expect_true(
        !mylite_sql_ast_spans_are_within_source(&result.ast, sql, source_length),
        "payload validator rejects wrong source length"
    );
    varchar_type->payload.varchar_type.length_span = saved_varchar_span;
    varchar_type->payload.varchar_type.length_span.offset = SIZE_MAX;
    failures += parser_test_expect_true(
        !mylite_sql_ast_spans_are_within_source(&result.ast, sql, source_length),
        "payload validator rejects invalid offset"
    );
    varchar_type->payload.varchar_type.length_span = saved_varchar_span;
    varchar_type->payload.varchar_type.length_span.length = SIZE_MAX;
    failures += parser_test_expect_true(
        !mylite_sql_ast_spans_are_within_source(&result.ast, sql, source_length),
        "payload validator rejects invalid length"
    );
    varchar_type->payload.varchar_type.length_span = saved_varchar_span;

    saved_scale_span = decimal_type->payload.decimal_type.scale_span;
    decimal_type->payload.decimal_type.scale_span.text = sql;
    failures += parser_test_expect_true(
        !mylite_sql_ast_rebase_source_length(&result.ast, sql, source_length, rebased_length),
        "payload rebase rejects later invalid span"
    );
    failures += parser_test_expect_true(
        statement->span.source_length == source_length &&
            varchar_type->payload.varchar_type.length_span.source_length == source_length,
        "failed payload rebase is atomic"
    );
    decimal_type->payload.decimal_type.scale_span = saved_scale_span;

    failures += parser_test_expect_true(
        mylite_sql_ast_rebase_source_length(&result.ast, sql, source_length, rebased_length),
        "payload rebase succeeds for valid AST"
    );
    failures += parser_test_expect_true(
        statement->span.source_length == rebased_length &&
            varchar_type->payload.varchar_type.length_span.source_length == rebased_length &&
            decimal_type->payload.decimal_type.precision_span.source_length == rebased_length &&
            decimal_type->payload.decimal_type.scale_span.source_length == rebased_length,
        "payload rebase updates primary and embedded spans"
    );
    failures += parser_test_expect_true(
        mylite_sql_ast_spans_are_within_source(&result.ast, sql, rebased_length),
        "rebased payload AST validates against extended source"
    );

    mylite_sql_parse_result_deinit(&result);
    return failures;
}

static int test_snapshot_rejects_payload_outside_root(void) {
    static const char source[] = "VARCHAR 12";
    struct mylite_sql_ast_node type = {
        .kind = MYLITE_SQL_AST_VARCHAR_TYPE,
        .span =
            {
                .text = source,
                .length = interval_root_length,
                .offset = 0U,
                .source_length = sizeof(source) - 1U,
            },
        .payload.varchar_type =
            {
                .length_span =
                    {
                        .text = source + interval_payload_offset,
                        .length = 2U,
                        .offset = interval_payload_offset,
                        .source_length = sizeof(source) - 1U,
                    },
            },
    };
    struct mylite_sql_ast_node root = {
        .kind = MYLITE_SQL_AST_COLUMN_DEFINITION,
        .span =
            {
                .text = source,
                .length = interval_root_length,
                .offset = 0U,
                .source_length = sizeof(source) - 1U,
            },
        .first_child = &type,
        .last_child = &type,
    };
    struct mylite_sql_ast_snapshot snapshot;
    int failures = 0;

    mylite_sql_ast_snapshot_init(&snapshot);
    failures += parser_test_expect_true(
        !mylite_sql_ast_snapshot_clone_subtree(&snapshot, &root),
        "snapshot rejects payload outside root interval"
    );
    failures += parser_test_expect_true(
        snapshot.source == NULL && snapshot.nodes == NULL && snapshot.node_count == 0U,
        "failed payload snapshot cleans partial ownership"
    );
    mylite_sql_ast_snapshot_deinit(&snapshot);
    return failures;
}

static int test_nonpayload_prefix_retries_remain_valid(void) {
    static const char *const statements[] = {
        "SELECT 1 FOR UPDATE FOR SHARE",
        "ALTER TABLE t RENAME TO u, ALGORITHM=INPLACE",
    };
    int failures = 0;

    for (size_t index = 0U; index < sizeof(statements) / sizeof(statements[0]); ++index) {
        struct mylite_sql_parse_result result = {0};
        const size_t source_length = strlen(statements[index]);

        failures += parser_test_parse_sql(statements[index], MYLITE_SQL_PARSE_OK, &result);
        failures += parser_test_expect_true(
            result.retry_handled_count == 1U,
            "nonpayload prefix statement uses one retry"
        );
        failures += parser_test_expect_true(
            mylite_sql_ast_spans_are_within_source(&result.ast, statements[index], source_length),
            "nonpayload prefix retry spans remain valid"
        );
        mylite_sql_parse_result_deinit(&result);
    }
    return failures;
}

static int test_tableless_prefix_helper_remains_valid(void) {
    static const char sql[] = "SELECT 1 LIMIT 1";
    struct mylite_sql_parse_config config = {
        .input = sql,
        .length = sizeof(sql) - 1U,
    };
    struct mylite_sql_parse_result result = {0};
    struct mylite_sql_parser_retry_context retry_context = {0};
    enum mylite_sql_parse_status status = mylite_sql_parser_parse_with_lemon(config, &result);
    bool handled = false;
    int failures = 0;

    if (status == MYLITE_SQL_PARSE_OK) {
        status = mylite_sql_parser_retry_context_init(config, &retry_context);
    }
    if (status == MYLITE_SQL_PARSE_OK) {
        status = mylite_sql_parser_try_parse_tableless_select_limit_statement(
            config,
            &result,
            &retry_context,
            &handled
        );
    }
    failures += parser_test_expect_true(
        status == MYLITE_SQL_PARSE_OK && handled,
        "tableless limit prefix helper handles statement"
    );
    failures += parser_test_expect_true(
        mylite_sql_ast_spans_are_within_source(&result.ast, sql, sizeof(sql) - 1U),
        "tableless limit prefix helper spans remain valid"
    );

    mylite_sql_parser_retry_context_deinit(&retry_context);
    mylite_sql_parse_result_deinit(&result);
    return failures;
}

static int expect_snapshot_payload_inventory(const struct mylite_sql_ast_snapshot *snapshot) {
    const struct mylite_sql_ast_node *root = mylite_sql_ast_snapshot_root(snapshot);
    const struct mylite_sql_ast_node *integer_type =
        snapshot_node_kind_at(snapshot, MYLITE_SQL_AST_INTEGER_TYPE, 1U);
    const struct mylite_sql_ast_node *varchar_type =
        snapshot_node_kind_at(snapshot, MYLITE_SQL_AST_VARCHAR_TYPE, 0U);
    const struct mylite_sql_ast_node *char_type =
        snapshot_node_kind_at(snapshot, MYLITE_SQL_AST_CHAR_TYPE, 0U);
    const struct mylite_sql_ast_node *text_type =
        snapshot_node_kind_at(snapshot, MYLITE_SQL_AST_TEXT_TYPE, 0U);
    const struct mylite_sql_ast_node *binary_type =
        snapshot_node_kind_at(snapshot, MYLITE_SQL_AST_BINARY_STRING_TYPE, 0U);
    const struct mylite_sql_ast_node *varbinary_type =
        snapshot_node_kind_at(snapshot, MYLITE_SQL_AST_BINARY_STRING_TYPE, 1U);
    const struct mylite_sql_ast_node *bit_type =
        snapshot_node_kind_at(snapshot, MYLITE_SQL_AST_BIT_TYPE, 0U);
    const struct mylite_sql_ast_node *year_type =
        snapshot_node_kind_at(snapshot, MYLITE_SQL_AST_YEAR_TYPE, 0U);
    const struct mylite_sql_ast_node *decimal_type =
        snapshot_node_kind_at(snapshot, MYLITE_SQL_AST_DECIMAL_TYPE, 0U);
    const struct mylite_sql_ast_node *approximate_type =
        snapshot_node_kind_at(snapshot, MYLITE_SQL_AST_APPROXIMATE_TYPE, 0U);
    const struct mylite_sql_ast_node *date_type =
        snapshot_node_kind_at(snapshot, MYLITE_SQL_AST_DATE_TYPE, 0U);
    const struct mylite_sql_ast_node *datetime_type =
        snapshot_node_kind_at(snapshot, MYLITE_SQL_AST_DATETIME_TYPE, 0U);
    const struct mylite_sql_ast_node *timestamp_type =
        snapshot_node_kind_at(snapshot, MYLITE_SQL_AST_TIMESTAMP_TYPE, 0U);
    const struct mylite_sql_ast_node *time_type =
        snapshot_node_kind_at(snapshot, MYLITE_SQL_AST_TIME_TYPE, 0U);
    const char *source = snapshot == NULL ? NULL : snapshot->source;
    const size_t source_length = root == NULL ? 0U : root->span.source_length;
    int failures = 0;

    failures += parser_test_expect_true(
        root != NULL && integer_type != NULL && varchar_type != NULL && char_type != NULL &&
            text_type != NULL && binary_type != NULL && varbinary_type != NULL &&
            bit_type != NULL && year_type != NULL && decimal_type != NULL &&
            approximate_type != NULL && date_type != NULL && datetime_type != NULL &&
            timestamp_type != NULL && time_type != NULL,
        "snapshot contains complete payload-span inventory"
    );
    if (root == NULL || integer_type == NULL || varchar_type == NULL || char_type == NULL ||
        text_type == NULL || binary_type == NULL || varbinary_type == NULL || bit_type == NULL ||
        year_type == NULL || decimal_type == NULL || approximate_type == NULL ||
        date_type == NULL || datetime_type == NULL || timestamp_type == NULL || time_type == NULL) {
        return failures;
    }

    failures += expect_payload_span(
        integer_type->payload.integer_type.display_width_span,
        "11",
        source,
        source_length,
        "integer width snapshot"
    );
    failures += expect_payload_span(
        varchar_type->payload.varchar_type.length_span,
        "12",
        source,
        source_length,
        "varchar length snapshot"
    );
    failures += expect_payload_span(
        char_type->payload.char_type.length_span,
        "3",
        source,
        source_length,
        "char length snapshot"
    );
    failures += expect_payload_span(
        text_type->payload.text_type.length_span,
        "12",
        source,
        source_length,
        "text length snapshot"
    );
    failures += expect_payload_span(
        binary_type->payload.binary_string_type.length_span,
        "4",
        source,
        source_length,
        "binary length snapshot"
    );
    failures += expect_payload_span(
        varbinary_type->payload.binary_string_type.length_span,
        "5",
        source,
        source_length,
        "varbinary length snapshot"
    );
    failures += expect_payload_span(
        bit_type->payload.bit_type.length_span,
        "6",
        source,
        source_length,
        "bit length snapshot"
    );
    failures += expect_payload_span(
        year_type->payload.year_type.width_span,
        "4",
        source,
        source_length,
        "year width snapshot"
    );
    failures += expect_payload_span(
        decimal_type->payload.decimal_type.precision_span,
        "10",
        source,
        source_length,
        "decimal precision snapshot"
    );
    failures += expect_payload_span(
        decimal_type->payload.decimal_type.scale_span,
        "2",
        source,
        source_length,
        "decimal scale snapshot"
    );
    failures += expect_payload_span(
        approximate_type->payload.approximate_type.precision_span,
        "9",
        source,
        source_length,
        "approximate precision snapshot"
    );
    failures += expect_payload_span(
        approximate_type->payload.approximate_type.scale_span,
        "3",
        source,
        source_length,
        "approximate scale snapshot"
    );
    failures += expect_zero_span(
        date_type->payload.temporal_fractional_precision.precision_span,
        "date optional precision snapshot"
    );
    failures += expect_payload_span(
        datetime_type->payload.temporal_fractional_precision.precision_span,
        "4",
        source,
        source_length,
        "datetime precision snapshot"
    );
    failures += expect_payload_span(
        timestamp_type->payload.temporal_fractional_precision.precision_span,
        "5",
        source,
        source_length,
        "timestamp precision snapshot"
    );
    failures += expect_payload_span(
        time_type->payload.temporal_fractional_precision.precision_span,
        "6",
        source,
        source_length,
        "time precision snapshot"
    );
    return failures;
}

static int expect_payload_span(
    struct mylite_sql_source_span span,
    const char *expected,
    const char *source,
    size_t source_length,
    const char *context
) {
    const size_t expected_length = expected == NULL ? 0U : strlen(expected);

    return parser_test_expect_true(
        expected != NULL && source != NULL && span.text == source + span.offset &&
            span.source_length == source_length && span.length == expected_length &&
            memcmp(span.text, expected, expected_length) == 0,
        context
    );
}

static int expect_zero_span(struct mylite_sql_source_span span, const char *context) {
    return parser_test_expect_true(
        span.text == NULL && span.length == 0U && span.offset == 0U && span.source_length == 0U,
        context
    );
}

static const struct mylite_sql_ast_node *snapshot_node_kind_at(
    const struct mylite_sql_ast_snapshot *snapshot,
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): compact test lookup.
    enum mylite_sql_ast_node_kind kind,
    size_t occurrence
) {
    if (snapshot == NULL || snapshot->nodes == NULL) {
        return NULL;
    }
    for (size_t index = 0U; index < snapshot->node_count; ++index) {
        if (snapshot->nodes[index].kind != kind) {
            continue;
        }
        if (occurrence == 0U) {
            return &snapshot->nodes[index];
        }
        --occurrence;
    }
    return NULL;
}

static struct mylite_sql_ast_node *find_node_kind(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_node_kind kind
) {
    struct mylite_sql_ast_node *stack[node_search_stack_capacity];
    size_t stack_size = 0U;

    if (node == NULL) {
        return NULL;
    }
    stack[stack_size++] = node;
    while (stack_size != 0U) {
        struct mylite_sql_ast_node *current = stack[--stack_size];

        if (current->kind == kind) {
            return current;
        }
        for (struct mylite_sql_ast_node *child = current->first_child; child != NULL;
             child = child->next_sibling) {
            if (stack_size >= (size_t)node_search_stack_capacity) {
                return NULL;
            }
            stack[stack_size++] = child;
        }
    }
    return NULL;
}
