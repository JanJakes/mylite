#include "mylite_select_aggregate.h"

#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "mylite_expression.h"
#include "mylite_select.h"
#include "mylite_select_eval.h"
#include "mylite_select_eval_expression.h"
#include "mylite_select_rowset.h"
#include "mylite_select_rowset_distinct.h"
#include "mylite_span.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { mylite_group_concat_warning_size = 64U };

static int update_table_select_count_distinct_state(
    mylite_stmt *stmt,
    struct mylite_select_aggregate_state *state,
    const struct mylite_select_aggregate_binding *binding,
    const struct mylite_table_select_row *row,
    const struct mylite_select_eval_callbacks *callbacks
);

static int update_table_select_group_concat_state(
    mylite_stmt *stmt,
    struct mylite_select_aggregate_state *state,
    const struct mylite_select_aggregate_binding *binding,
    const struct mylite_table_select_row *row,
    const struct mylite_select_eval_callbacks *callbacks
);

static int finalize_table_select_group_concat_state(
    mylite_stmt *stmt,
    const struct mylite_select_aggregate_state *state,
    const struct mylite_select_aggregate_binding *binding,
    struct mylite_expression_value *out_value
);

static int evaluate_table_select_count_distinct_tuple(
    mylite_stmt *stmt,
    const struct mylite_select_aggregate_binding *binding,
    const struct mylite_table_select_row *row,
    const struct mylite_select_eval_callbacks *callbacks,
    struct mylite_count_distinct_tuple *out_tuple,
    bool *out_has_null
);

static int evaluate_table_select_group_concat_item(
    mylite_stmt *stmt,
    const struct mylite_select_aggregate_binding *binding,
    const struct mylite_table_select_row *row,
    const struct mylite_select_eval_callbacks *callbacks,
    struct mylite_group_concat_item *out_item,
    bool *out_skip
);

static int evaluate_table_select_group_concat_arguments(
    mylite_stmt *stmt,
    const struct mylite_select_aggregate_binding *binding,
    const struct mylite_table_select_row *row,
    const struct mylite_select_eval_callbacks *callbacks,
    struct mylite_group_concat_item *item,
    bool *out_has_null
);

static int evaluate_table_select_group_concat_order_values(
    mylite_stmt *stmt,
    const struct mylite_select_aggregate_binding *binding,
    const struct mylite_table_select_row *row,
    const struct mylite_select_eval_callbacks *callbacks,
    struct mylite_group_concat_item *item
);

static int evaluate_table_select_group_concat_order_item(
    mylite_stmt *stmt,
    const struct mylite_select_aggregate_binding *binding,
    const struct mylite_table_select_row *row,
    const struct mylite_select_eval_callbacks *callbacks,
    const struct mylite_sql_ast_node *order_item,
    const struct mylite_group_concat_item *item,
    struct mylite_expression_value *out_value
);

static int make_group_concat_item_text(mylite_stmt *stmt, struct mylite_group_concat_item *item);

static bool count_distinct_tuple_exists(
    const struct mylite_select_aggregate_state *state,
    const struct mylite_count_distinct_tuple *tuple,
    const struct mylite_select_aggregate_binding *binding
);

static bool group_concat_item_exists(
    const struct mylite_select_aggregate_state *state,
    const struct mylite_group_concat_item *item
);

static bool count_distinct_tuples_equal(
    const struct mylite_count_distinct_tuple *left,
    const struct mylite_count_distinct_tuple *right,
    const struct mylite_select_aggregate_binding *binding
);

static bool group_concat_items_equal(
    const struct mylite_group_concat_item *left,
    const struct mylite_group_concat_item *right
);

static int append_count_distinct_tuple(
    struct mylite_select_aggregate_state *state,
    struct mylite_count_distinct_tuple *tuple
);

static int append_group_concat_item(
    struct mylite_select_aggregate_state *state,
    struct mylite_group_concat_item *item
);

static int append_group_concat_piece(
    mylite_stmt *stmt,
    char **result,
    size_t *result_length,
    const char *piece,
    size_t piece_length,
    size_t row_number,
    bool *truncated
);

static int append_group_concat_truncation_warning(mylite_stmt *stmt, size_t row_number);

static int sorted_group_concat_item_indexes(
    mylite_stmt *stmt,
    const struct mylite_select_aggregate_state *state,
    const struct mylite_select_aggregate_binding *binding,
    size_t **out_indexes
);

static int compare_group_concat_items(
    const struct mylite_group_concat_item *left,
    const struct mylite_group_concat_item *right,
    const struct mylite_select_aggregate_binding *binding
);

static bool group_concat_order_expression_is_argument_ordinal(
    const struct mylite_sql_ast_node *expression,
    size_t argument_count,
    size_t *out_index
);

static const struct mylite_sql_ast_node *group_concat_argument_list(
    const struct mylite_select_aggregate_binding *binding
);

static const struct mylite_sql_ast_node *group_concat_order_by_clause(
    const struct mylite_select_aggregate_binding *binding
);

static const struct mylite_sql_ast_node *group_concat_separator_literal(
    const struct mylite_select_aggregate_binding *binding
);

static size_t group_concat_order_value_count(const struct mylite_select_aggregate_binding *binding);

static size_t group_concat_text_length(
    const struct mylite_expression_value *value,
    const char *text
);

static bool group_concat_is_distinct(const struct mylite_select_aggregate_binding *binding);

static void count_distinct_tuple_deinit(struct mylite_count_distinct_tuple *tuple);

static void group_concat_item_deinit(struct mylite_group_concat_item *item);

int mylite_select_update_aggregate_state(
    mylite_stmt *stmt,
    struct mylite_select_aggregate_state *state,
    const struct mylite_select_aggregate_binding *binding,
    const struct mylite_table_select_row *row,
    const struct mylite_select_eval_callbacks *callbacks
) {
    struct mylite_expression_value value = {0};
    int status = MYLITE_OK;

    if (state->count != UINT64_MAX) {
        ++state->count;
    }
    if (binding->argument_kind == MYLITE_SQL_AST_AGGREGATE_ARGUMENT_STAR) {
        return MYLITE_OK;
    }
    if (binding->kind == MYLITE_SQL_AST_AGGREGATE_GROUP_CONCAT) {
        return update_table_select_group_concat_state(stmt, state, binding, row, callbacks);
    }
    if (binding->argument_kind == MYLITE_SQL_AST_AGGREGATE_ARGUMENT_DISTINCT_EXPRESSION_LIST) {
        return update_table_select_count_distinct_state(stmt, state, binding, row, callbacks);
    }

    status = mylite_select_eval_aggregate_argument(stmt, binding, row, callbacks, &value);
    if (status != MYLITE_OK) {
        return status;
    }
    if (value.kind == MYLITE_EXPRESSION_VALUE_NULL) {
        mylite_expression_value_deinit(&value);
        return MYLITE_OK;
    }
    if (state->non_null_count != UINT64_MAX) {
        ++state->non_null_count;
    }

    switch (binding->kind) {
    case MYLITE_SQL_AST_AGGREGATE_SUM:
    case MYLITE_SQL_AST_AGGREGATE_AVG: {
        struct mylite_aggregate_numeric_value numeric = {0};

        status =
            mylite_select_aggregate_value_to_double(&stmt->database->warnings, &value, &numeric);
        if (status == MYLITE_OK) {
            state->sum += numeric.value;
            if (state->non_null_count == 1U) {
                state->integral_sum = numeric.integral;
                state->unsigned_sum = numeric.unsigned_value;
            } else {
                state->integral_sum = (state->integral_sum && numeric.integral) != 0;
                state->unsigned_sum = (state->unsigned_sum && numeric.unsigned_value) != 0;
            }
        }
        break;
    }
    case MYLITE_SQL_AST_AGGREGATE_MIN:
    case MYLITE_SQL_AST_AGGREGATE_MAX:
        if (!state->has_value ||
            (binding->kind == MYLITE_SQL_AST_AGGREGATE_MIN &&
             mylite_select_compare_values(&value, &state->value) < 0) ||
            (binding->kind == MYLITE_SQL_AST_AGGREGATE_MAX &&
             mylite_select_compare_values(&value, &state->value) > 0)) {
            mylite_expression_value_deinit(&state->value);
            status =
                mylite_expression_value_copy(&value, &state->value) == 0 ? MYLITE_OK : MYLITE_NOMEM;
            state->has_value = status == MYLITE_OK;
        }
        break;
    case MYLITE_SQL_AST_AGGREGATE_COUNT:
    case MYLITE_SQL_AST_AGGREGATE_NONE:
    case MYLITE_SQL_AST_AGGREGATE_GROUP_CONCAT:
        break;
    }

    mylite_expression_value_deinit(&value);
    if (status == MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
    }
    return status;
}

int mylite_select_finalize_aggregate_state(
    mylite_stmt *stmt,
    const struct mylite_select_aggregate_state *state,
    const struct mylite_select_aggregate_binding *binding,
    struct mylite_expression_value *out_value
) {
    switch (binding->kind) {
    case MYLITE_SQL_AST_AGGREGATE_COUNT:
        *out_value = (struct mylite_expression_value){
            .kind = MYLITE_EXPRESSION_VALUE_INT64,
            .int64_value = binding->argument_kind == MYLITE_SQL_AST_AGGREGATE_ARGUMENT_STAR
                               ? (int64_t)state->count
                               : (int64_t)state->non_null_count,
        };
        return MYLITE_OK;
    case MYLITE_SQL_AST_AGGREGATE_SUM:
        if (state->non_null_count == 0U) {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
            return MYLITE_OK;
        }
        if (state->integral_sum) {
            if (state->unsigned_sum) {
                *out_value = (struct mylite_expression_value){
                    .kind = MYLITE_EXPRESSION_VALUE_UINT64,
                    .uint64_value = (uint64_t)state->sum,
                };
            } else {
                *out_value = (struct mylite_expression_value){
                    .kind = MYLITE_EXPRESSION_VALUE_INT64,
                    .int64_value = (int64_t)state->sum,
                };
            }
            return MYLITE_OK;
        }
        return mylite_select_aggregate_format_double(state->sum, out_value);
    case MYLITE_SQL_AST_AGGREGATE_AVG:
        if (state->non_null_count == 0U) {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
            return MYLITE_OK;
        }
        if (state->integral_sum) {
            *out_value = (struct mylite_expression_value){
                .kind = MYLITE_EXPRESSION_VALUE_REAL,
                .real_value = state->sum / (double)state->non_null_count,
            };
            return MYLITE_OK;
        }
        return mylite_select_aggregate_format_double(
            state->sum / (double)state->non_null_count,
            out_value
        );
    case MYLITE_SQL_AST_AGGREGATE_MIN:
    case MYLITE_SQL_AST_AGGREGATE_MAX:
        if (!state->has_value) {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
            return MYLITE_OK;
        }
        if (mylite_expression_value_copy(&state->value, out_value) != 0) {
            (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
            return MYLITE_NOMEM;
        }
        return MYLITE_OK;
    case MYLITE_SQL_AST_AGGREGATE_GROUP_CONCAT:
        return finalize_table_select_group_concat_state(stmt, state, binding, out_value);
    case MYLITE_SQL_AST_AGGREGATE_NONE:
        break;
    }

    *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
    return MYLITE_OK;
}

void mylite_select_aggregate_state_deinit(struct mylite_select_aggregate_state *state) {
    if (state == NULL) {
        return;
    }

    mylite_expression_value_deinit(&state->value);
    for (size_t index = 0U; index < state->distinct_tuple_count; ++index) {
        count_distinct_tuple_deinit(&state->distinct_tuples[index]);
    }
    for (size_t index = 0U; index < state->group_concat_item_count; ++index) {
        group_concat_item_deinit(&state->group_concat_items[index]);
    }
    free(state->distinct_tuples);
    free(state->group_concat_items);
    *state = (struct mylite_select_aggregate_state){0};
}

static int update_table_select_count_distinct_state(
    mylite_stmt *stmt,
    struct mylite_select_aggregate_state *state,
    const struct mylite_select_aggregate_binding *binding,
    const struct mylite_table_select_row *row,
    const struct mylite_select_eval_callbacks *callbacks
) {
    struct mylite_count_distinct_tuple tuple = {0};
    bool has_null = false;
    int status = evaluate_table_select_count_distinct_tuple(
        stmt,
        binding,
        row,
        callbacks,
        &tuple,
        &has_null
    );

    if (status != MYLITE_OK) {
        count_distinct_tuple_deinit(&tuple);
        return status;
    }
    if (has_null || count_distinct_tuple_exists(state, &tuple, binding)) {
        count_distinct_tuple_deinit(&tuple);
        return MYLITE_OK;
    }

    status = append_count_distinct_tuple(state, &tuple);
    if (status != MYLITE_OK) {
        count_distinct_tuple_deinit(&tuple);
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        }
        return status;
    }
    if (state->non_null_count != UINT64_MAX) {
        ++state->non_null_count;
    }
    return MYLITE_OK;
}

static int update_table_select_group_concat_state(
    mylite_stmt *stmt,
    struct mylite_select_aggregate_state *state,
    const struct mylite_select_aggregate_binding *binding,
    const struct mylite_table_select_row *row,
    const struct mylite_select_eval_callbacks *callbacks
) {
    struct mylite_group_concat_item item = {0};
    bool skip = false;
    int status =
        evaluate_table_select_group_concat_item(stmt, binding, row, callbacks, &item, &skip);

    if (status != MYLITE_OK) {
        group_concat_item_deinit(&item);
        return status;
    }
    if (skip || (group_concat_is_distinct(binding) && group_concat_item_exists(state, &item))) {
        group_concat_item_deinit(&item);
        return MYLITE_OK;
    }

    status = append_group_concat_item(state, &item);
    if (status != MYLITE_OK) {
        group_concat_item_deinit(&item);
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        }
        return status;
    }
    if (state->non_null_count != UINT64_MAX) {
        ++state->non_null_count;
    }
    return MYLITE_OK;
}

static int finalize_table_select_group_concat_state(
    mylite_stmt *stmt,
    const struct mylite_select_aggregate_state *state,
    const struct mylite_select_aggregate_binding *binding,
    struct mylite_expression_value *out_value
) {
    const struct mylite_sql_ast_node *separator_literal = group_concat_separator_literal(binding);
    char *separator = NULL;
    const char *separator_text = ",";
    size_t separator_length = 1U;
    size_t *indexes = NULL;
    char *result = NULL;
    size_t result_length = 0U;
    bool truncated = false;
    int status = MYLITE_OK;

    if (state->group_concat_item_count == 0U) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        return MYLITE_OK;
    }

    if (separator_literal != NULL) {
        separator = mylite_copy_string_literal_span(separator_literal);
        if (separator == NULL) {
            (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
            return MYLITE_NOMEM;
        }
        separator_text = separator;
        separator_length = strlen(separator);
    }

    status = sorted_group_concat_item_indexes(stmt, state, binding, &indexes);
    for (size_t position = 0U; status == MYLITE_OK && position < state->group_concat_item_count;
         ++position) {
        size_t item_index = indexes == NULL ? position : indexes[position];
        const struct mylite_group_concat_item *item = &state->group_concat_items[item_index];
        size_t row_number = position + 1U;

        if (position > 0U) {
            status = append_group_concat_piece(
                stmt,
                &result,
                &result_length,
                separator_text,
                separator_length,
                row_number,
                &truncated
            );
        }
        if (status == MYLITE_OK && !truncated) {
            status = append_group_concat_piece(
                stmt,
                &result,
                &result_length,
                item->text,
                item->text_length,
                row_number,
                &truncated
            );
        }
        if (truncated) {
            break;
        }
    }

    free(separator);
    free(indexes);
    if (status != MYLITE_OK) {
        free(result);
        return status;
    }
    if (result == NULL) {
        result = mylite_copy_span_text("", 0U);
        if (result == NULL) {
            (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
            return MYLITE_NOMEM;
        }
    }

    *out_value = (struct mylite_expression_value){
        .kind = MYLITE_EXPRESSION_VALUE_TEXT,
        .text_value = result,
        .text_length = result_length,
    };
    return MYLITE_OK;
}

static int evaluate_table_select_count_distinct_tuple(
    mylite_stmt *stmt,
    const struct mylite_select_aggregate_binding *binding,
    const struct mylite_table_select_row *row,
    const struct mylite_select_eval_callbacks *callbacks,
    struct mylite_count_distinct_tuple *out_tuple,
    bool *out_has_null
) {
    size_t value_count = mylite_sql_ast_node_child_count(binding->argument);

    *out_tuple = (struct mylite_count_distinct_tuple){0};
    *out_has_null = false;
    if (value_count == 0U) {
        return MYLITE_UNSUPPORTED;
    }

    out_tuple->values = calloc(value_count, sizeof(*out_tuple->values));
    if (out_tuple->values == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    out_tuple->value_count = value_count;

    size_t index = 0U;
    for (const struct mylite_sql_ast_node *argument = binding->argument->first_child;
         argument != NULL;
         argument = argument->next_sibling) {
        struct mylite_select_aggregate_binding argument_binding = {
            .argument = argument,
            .argument_kind = MYLITE_SQL_AST_AGGREGATE_ARGUMENT_EXPRESSION,
        };
        int status = mylite_select_eval_aggregate_argument(
            stmt,
            &argument_binding,
            row,
            callbacks,
            &out_tuple->values[index]
        );

        if (status != MYLITE_OK) {
            return status;
        }
        if (out_tuple->values[index].kind == MYLITE_EXPRESSION_VALUE_NULL) {
            *out_has_null = true;
        }
        ++index;
    }
    return MYLITE_OK;
}

static int evaluate_table_select_group_concat_item(
    mylite_stmt *stmt,
    const struct mylite_select_aggregate_binding *binding,
    const struct mylite_table_select_row *row,
    const struct mylite_select_eval_callbacks *callbacks,
    struct mylite_group_concat_item *out_item,
    bool *out_skip
) {
    bool has_null = false;
    int status = MYLITE_OK;

    *out_item = (struct mylite_group_concat_item){0};
    *out_skip = false;

    status = evaluate_table_select_group_concat_arguments(
        stmt,
        binding,
        row,
        callbacks,
        out_item,
        &has_null
    );
    if (status != MYLITE_OK) {
        return status;
    }
    if (has_null) {
        *out_skip = true;
        return MYLITE_OK;
    }

    status = make_group_concat_item_text(stmt, out_item);
    if (status == MYLITE_OK) {
        status = evaluate_table_select_group_concat_order_values(
            stmt,
            binding,
            row,
            callbacks,
            out_item
        );
    }
    return status;
}

static int evaluate_table_select_group_concat_arguments(
    mylite_stmt *stmt,
    const struct mylite_select_aggregate_binding *binding,
    const struct mylite_table_select_row *row,
    const struct mylite_select_eval_callbacks *callbacks,
    struct mylite_group_concat_item *item,
    bool *out_has_null
) {
    const struct mylite_sql_ast_node *arguments = group_concat_argument_list(binding);
    size_t argument_count = mylite_sql_ast_node_child_count(arguments);

    *out_has_null = false;
    if (argument_count == 0U) {
        return MYLITE_UNSUPPORTED;
    }

    item->arguments = calloc(argument_count, sizeof(*item->arguments));
    if (item->arguments == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    item->argument_count = argument_count;

    size_t index = 0U;
    for (const struct mylite_sql_ast_node *argument = arguments->first_child; argument != NULL;
         argument = argument->next_sibling) {
        struct mylite_select_aggregate_binding argument_binding = {
            .argument = argument,
            .argument_kind = MYLITE_SQL_AST_AGGREGATE_ARGUMENT_EXPRESSION,
        };
        int status = mylite_select_eval_aggregate_argument(
            stmt,
            &argument_binding,
            row,
            callbacks,
            &item->arguments[index]
        );

        if (status != MYLITE_OK) {
            return status;
        }
        if (item->arguments[index].kind == MYLITE_EXPRESSION_VALUE_NULL) {
            *out_has_null = true;
        }
        ++index;
    }
    return MYLITE_OK;
}

static int evaluate_table_select_group_concat_order_values(
    mylite_stmt *stmt,
    const struct mylite_select_aggregate_binding *binding,
    const struct mylite_table_select_row *row,
    const struct mylite_select_eval_callbacks *callbacks,
    struct mylite_group_concat_item *item
) {
    const struct mylite_sql_ast_node *order_by = group_concat_order_by_clause(binding);
    const struct mylite_sql_ast_node *order_items =
        order_by == NULL ? NULL : mylite_ast_child_at(order_by, 0U);
    size_t order_value_count = group_concat_order_value_count(binding);

    if (order_value_count == 0U) {
        return MYLITE_OK;
    }

    item->order_values = calloc(order_value_count, sizeof(*item->order_values));
    if (item->order_values == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    item->order_value_count = order_value_count;

    size_t index = 0U;
    for (const struct mylite_sql_ast_node *order_item =
             order_items == NULL ? NULL : order_items->first_child;
         order_item != NULL;
         order_item = order_item->next_sibling) {
        int status = evaluate_table_select_group_concat_order_item(
            stmt,
            binding,
            row,
            callbacks,
            order_item,
            item,
            &item->order_values[index]
        );

        if (status != MYLITE_OK) {
            return status;
        }
        ++index;
    }
    return MYLITE_OK;
}

static int evaluate_table_select_group_concat_order_item(
    mylite_stmt *stmt,
    const struct mylite_select_aggregate_binding *binding,
    const struct mylite_table_select_row *row,
    const struct mylite_select_eval_callbacks *callbacks,
    const struct mylite_sql_ast_node *order_item,
    const struct mylite_group_concat_item *item,
    struct mylite_expression_value *out_value
) {
    const struct mylite_sql_ast_node *expression = mylite_ast_child_at(order_item, 0U);
    size_t argument_index = 0U;

    (void)binding;
    if (group_concat_order_expression_is_argument_ordinal(
            expression,
            item->argument_count,
            &argument_index
        )) {
        if (mylite_expression_value_copy(&item->arguments[argument_index], out_value) != 0) {
            (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
            return MYLITE_NOMEM;
        }
        return MYLITE_OK;
    }

    struct mylite_table_select_eval_context user_context = {0};
    struct mylite_expression_eval_context context = {0};
    int status = 0;

    mylite_select_eval_context_init(&user_context, stmt, row, callbacks, false, false);
    mylite_select_eval_expression_context_init(&context, &user_context);
    status = mylite_expression_eval_with_context(
        expression,
        &context,
        &stmt->database->warnings,
        out_value
    );
    if (status != 0) {
        return mylite_select_eval_map_expression_status(stmt, status, callbacks);
    }
    return MYLITE_OK;
}

static int make_group_concat_item_text(mylite_stmt *stmt, struct mylite_group_concat_item *item) {
    char **texts = calloc(item->argument_count, sizeof(*texts));
    size_t *lengths = calloc(item->argument_count, sizeof(*lengths));
    char *text = NULL;
    size_t text_length = 0U;
    size_t offset = 0U;
    int status = MYLITE_OK;

    if ((texts == NULL || lengths == NULL) && item->argument_count != 0U) {
        status = MYLITE_NOMEM;
        goto cleanup;
    }

    for (size_t index = 0U; index < item->argument_count; ++index) {
        texts[index] = mylite_expression_value_to_text(&item->arguments[index]);
        if (texts[index] == NULL) {
            status = MYLITE_NOMEM;
            goto cleanup;
        }
        lengths[index] = group_concat_text_length(&item->arguments[index], texts[index]);
        if (SIZE_MAX - text_length < lengths[index]) {
            status = MYLITE_NOMEM;
            goto cleanup;
        }
        text_length += lengths[index];
    }

    text = malloc(text_length + 1U);
    if (text == NULL) {
        status = MYLITE_NOMEM;
        goto cleanup;
    }
    for (size_t index = 0U; index < item->argument_count; ++index) {
        if (lengths[index] > 0U) {
            memcpy(text + offset, texts[index], lengths[index]);
            offset += lengths[index];
        }
    }
    text[text_length] = '\0';

    item->text = text;
    item->text_length = text_length;
    text = NULL;

cleanup:
    if (status == MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
    }
    free(text);
    if (texts != NULL) {
        for (size_t index = 0U; index < item->argument_count; ++index) {
            free(texts[index]);
        }
    }
    free(texts);
    free(lengths);
    return status;
}

static bool count_distinct_tuple_exists(
    const struct mylite_select_aggregate_state *state,
    const struct mylite_count_distinct_tuple *tuple,
    const struct mylite_select_aggregate_binding *binding
) {
    for (size_t index = 0U; index < state->distinct_tuple_count; ++index) {
        if (count_distinct_tuples_equal(&state->distinct_tuples[index], tuple, binding)) {
            return true;
        }
    }
    return false;
}

static bool group_concat_item_exists(
    const struct mylite_select_aggregate_state *state,
    const struct mylite_group_concat_item *item
) {
    for (size_t index = 0U; index < state->group_concat_item_count; ++index) {
        if (group_concat_items_equal(&state->group_concat_items[index], item)) {
            return true;
        }
    }
    return false;
}

static bool count_distinct_tuples_equal(
    const struct mylite_count_distinct_tuple *left,
    const struct mylite_count_distinct_tuple *right,
    const struct mylite_select_aggregate_binding *binding
) {
    if (left->value_count != right->value_count ||
        left->value_count != binding->argument_descriptor_count) {
        return false;
    }

    for (size_t index = 0U; index < left->value_count; ++index) {
        if (mylite_select_compare_distinct_values(
                &left->values[index],
                &right->values[index],
                &binding->argument_descriptors[index]
            ) != 0) {
            return false;
        }
    }
    return true;
}

static bool group_concat_items_equal(
    const struct mylite_group_concat_item *left,
    const struct mylite_group_concat_item *right
) {
    if (left->argument_count != right->argument_count) {
        return false;
    }
    for (size_t index = 0U; index < left->argument_count; ++index) {
        if (mylite_select_compare_values(&left->arguments[index], &right->arguments[index]) != 0) {
            return false;
        }
    }
    return true;
}

static int append_count_distinct_tuple(
    struct mylite_select_aggregate_state *state,
    struct mylite_count_distinct_tuple *tuple
) {
    struct mylite_count_distinct_tuple *tuples = realloc(
        state->distinct_tuples,
        (state->distinct_tuple_count + 1U) * sizeof(*state->distinct_tuples)
    );

    if (tuples == NULL) {
        return MYLITE_NOMEM;
    }

    state->distinct_tuples = tuples;
    state->distinct_tuples[state->distinct_tuple_count++] = *tuple;
    *tuple = (struct mylite_count_distinct_tuple){0};
    return MYLITE_OK;
}

static int append_group_concat_item(
    struct mylite_select_aggregate_state *state,
    struct mylite_group_concat_item *item
) {
    struct mylite_group_concat_item *items = realloc(
        state->group_concat_items,
        (state->group_concat_item_count + 1U) * sizeof(*state->group_concat_items)
    );

    if (items == NULL) {
        return MYLITE_NOMEM;
    }

    state->group_concat_items = items;
    state->group_concat_items[state->group_concat_item_count++] = *item;
    *item = (struct mylite_group_concat_item){0};
    return MYLITE_OK;
}

static int append_group_concat_piece(
    mylite_stmt *stmt,
    char **result,
    size_t *result_length,
    const char *piece,
    size_t piece_length,
    size_t row_number,
    bool *truncated
) {
    size_t remaining = 0U;
    size_t max_len = mylite_connection_group_concat_max_len_size(stmt->database);
    size_t append_length = piece_length;
    char *buffer = NULL;

    if (*truncated || piece_length == 0U) {
        return MYLITE_OK;
    }
    if (*result_length >= max_len) {
        *truncated = true;
        return append_group_concat_truncation_warning(stmt, row_number);
    }

    remaining = max_len - *result_length;
    if (append_length > remaining) {
        append_length = remaining;
        *truncated = true;
    }

    buffer = realloc(*result, *result_length + append_length + 1U);
    if (buffer == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    *result = buffer;
    if (append_length > 0U) {
        memcpy(*result + *result_length, piece, append_length);
        *result_length += append_length;
    }
    (*result)[*result_length] = '\0';

    if (*truncated) {
        return append_group_concat_truncation_warning(stmt, row_number);
    }
    return MYLITE_OK;
}

static int append_group_concat_truncation_warning(mylite_stmt *stmt, size_t row_number) {
    char message[mylite_group_concat_warning_size] = {0};
    int length =
        snprintf(message, sizeof(message), "Row %zu was cut by GROUP_CONCAT()", row_number);

    if (length < 0 || (size_t)length >= sizeof(message)) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    if (mylite_diagnostics_append_warning(
            stmt->database,
            MYLITE_MYSQL_ER_CUT_VALUE_GROUP_CONCAT,
            message
        ) != MYLITE_OK) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    return MYLITE_OK;
}

static int sorted_group_concat_item_indexes(
    mylite_stmt *stmt,
    const struct mylite_select_aggregate_state *state,
    const struct mylite_select_aggregate_binding *binding,
    size_t **out_indexes
) {
    size_t *indexes = NULL;

    *out_indexes = NULL;
    if (group_concat_order_value_count(binding) == 0U) {
        return MYLITE_OK;
    }

    indexes = calloc(state->group_concat_item_count, sizeof(*indexes));
    if (indexes == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    for (size_t index = 0U; index < state->group_concat_item_count; ++index) {
        indexes[index] = index;
    }

    for (size_t index = 1U; index < state->group_concat_item_count; ++index) {
        size_t item_index = indexes[index];
        size_t cursor = index;

        while (cursor > 0U && compare_group_concat_items(
                                  &state->group_concat_items[indexes[cursor - 1U]],
                                  &state->group_concat_items[item_index],
                                  binding
                              ) > 0) {
            indexes[cursor] = indexes[cursor - 1U];
            --cursor;
        }
        indexes[cursor] = item_index;
    }

    *out_indexes = indexes;
    return MYLITE_OK;
}

static int compare_group_concat_items(
    const struct mylite_group_concat_item *left,
    const struct mylite_group_concat_item *right,
    const struct mylite_select_aggregate_binding *binding
) {
    const struct mylite_sql_ast_node *order_by = group_concat_order_by_clause(binding);
    const struct mylite_sql_ast_node *items =
        order_by == NULL ? NULL : mylite_ast_child_at(order_by, 0U);
    const struct mylite_sql_ast_node *order_item = items == NULL ? NULL : items->first_child;
    size_t index = 0U;

    while (order_item != NULL && index < left->order_value_count &&
           index < right->order_value_count) {
        int comparison =
            mylite_select_compare_values(&left->order_values[index], &right->order_values[index]);

        if (comparison != 0) {
            return order_item->key_part_order == MYLITE_SQL_AST_KEY_PART_ORDER_DESC ? -comparison
                                                                                    : comparison;
        }
        order_item = order_item->next_sibling;
        ++index;
    }
    return 0;
}

static bool group_concat_order_expression_is_argument_ordinal(
    const struct mylite_sql_ast_node *expression,
    size_t argument_count,
    size_t *out_index
) {
    uint64_t ordinal = 0U;

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_LITERAL ||
        expression->literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER ||
        !mylite_select_parse_uint64_span(expression->span, &ordinal) || ordinal == 0U ||
        ordinal > argument_count) {
        return false;
    }
    *out_index = (size_t)(ordinal - 1U);
    return true;
}

static const struct mylite_sql_ast_node *group_concat_argument_list(
    const struct mylite_select_aggregate_binding *binding
) {
    return binding == NULL || binding->kind != MYLITE_SQL_AST_AGGREGATE_GROUP_CONCAT
               ? NULL
               : binding->argument;
}

static const struct mylite_sql_ast_node *group_concat_order_by_clause(
    const struct mylite_select_aggregate_binding *binding
) {
    const struct mylite_sql_ast_node *call =
        binding == NULL || binding->kind != MYLITE_SQL_AST_AGGREGATE_GROUP_CONCAT ? NULL
                                                                                  : binding->call;

    return mylite_ast_find_child_kind(call, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
}

static const struct mylite_sql_ast_node *group_concat_separator_literal(
    const struct mylite_select_aggregate_binding *binding
) {
    const struct mylite_sql_ast_node *call =
        binding == NULL || binding->kind != MYLITE_SQL_AST_AGGREGATE_GROUP_CONCAT ? NULL
                                                                                  : binding->call;
    const struct mylite_sql_ast_node *argument = binding == NULL ? NULL : binding->argument;

    for (const struct mylite_sql_ast_node *child = call == NULL ? NULL : call->first_child;
         child != NULL;
         child = child->next_sibling) {
        if (child != argument && child->kind == MYLITE_SQL_AST_LITERAL &&
            child->literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
            return child;
        }
    }
    return NULL;
}

static size_t group_concat_order_value_count(
    const struct mylite_select_aggregate_binding *binding
) {
    const struct mylite_sql_ast_node *order_by = group_concat_order_by_clause(binding);
    const struct mylite_sql_ast_node *items =
        order_by == NULL ? NULL : mylite_ast_child_at(order_by, 0U);

    return mylite_sql_ast_node_child_count(items);
}

static size_t group_concat_text_length(
    const struct mylite_expression_value *value,
    const char *text
) {
    if (value != NULL && value->kind == MYLITE_EXPRESSION_VALUE_TEXT) {
        return value->text_length;
    }
    return text == NULL ? 0U : strlen(text);
}

static bool group_concat_is_distinct(const struct mylite_select_aggregate_binding *binding) {
    return binding != NULL &&
           binding->argument_kind == MYLITE_SQL_AST_AGGREGATE_ARGUMENT_DISTINCT_EXPRESSION_LIST;
}

static void count_distinct_tuple_deinit(struct mylite_count_distinct_tuple *tuple) {
    if (tuple == NULL) {
        return;
    }

    for (size_t index = 0U; index < tuple->value_count; ++index) {
        mylite_expression_value_deinit(&tuple->values[index]);
    }
    free(tuple->values);
    *tuple = (struct mylite_count_distinct_tuple){0};
}

static void group_concat_item_deinit(struct mylite_group_concat_item *item) {
    if (item == NULL) {
        return;
    }

    for (size_t index = 0U; index < item->argument_count; ++index) {
        mylite_expression_value_deinit(&item->arguments[index]);
    }
    for (size_t index = 0U; index < item->order_value_count; ++index) {
        mylite_expression_value_deinit(&item->order_values[index]);
    }
    free(item->arguments);
    free(item->order_values);
    free(item->text);
    *item = (struct mylite_group_concat_item){0};
}
