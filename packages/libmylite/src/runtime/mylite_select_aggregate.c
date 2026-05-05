#include "mylite_select_aggregate.h"

#include "mylite_diagnostics.h"
#include "mylite_expression.h"
#include "mylite_select.h"
#include "mylite_select_eval.h"
#include "mylite_select_rowset.h"
#include "mylite_select_rowset_distinct.h"

#include <stdint.h>
#include <stdlib.h>

static int
update_table_select_count_distinct_state(mylite_stmt *stmt,
                                         struct mylite_select_aggregate_state *state,
                                         const struct mylite_select_aggregate_binding *binding,
                                         const struct mylite_table_select_row *row,
                                         const struct mylite_select_eval_callbacks *callbacks);
static int evaluate_table_select_count_distinct_tuple(
    mylite_stmt *stmt, const struct mylite_select_aggregate_binding *binding,
    const struct mylite_table_select_row *row, const struct mylite_select_eval_callbacks *callbacks,
    struct mylite_count_distinct_tuple *out_tuple, bool *out_has_null);
static bool count_distinct_tuple_exists(const struct mylite_select_aggregate_state *state,
                                        const struct mylite_count_distinct_tuple *tuple,
                                        const struct mylite_select_aggregate_binding *binding);
static bool count_distinct_tuples_equal(const struct mylite_count_distinct_tuple *left,
                                        const struct mylite_count_distinct_tuple *right,
                                        const struct mylite_select_aggregate_binding *binding);
static int append_count_distinct_tuple(struct mylite_select_aggregate_state *state,
                                       struct mylite_count_distinct_tuple *tuple);
static void count_distinct_tuple_deinit(struct mylite_count_distinct_tuple *tuple);

int mylite_select_update_aggregate_state(mylite_stmt *stmt,
                                         struct mylite_select_aggregate_state *state,
                                         const struct mylite_select_aggregate_binding *binding,
                                         const struct mylite_table_select_row *row,
                                         const struct mylite_select_eval_callbacks *callbacks)
{
    struct mylite_expression_value value = {0};
    int status = MYLITE_OK;

    if (state->count != UINT64_MAX) {
        ++state->count;
    }
    if (binding->argument_kind == MYLITE_SQL_AST_AGGREGATE_ARGUMENT_STAR) {
        return MYLITE_OK;
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
        break;
    }

    mylite_expression_value_deinit(&value);
    if (status == MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
    }
    return status;
}

int mylite_select_finalize_aggregate_state(mylite_stmt *stmt,
                                           const struct mylite_select_aggregate_state *state,
                                           const struct mylite_select_aggregate_binding *binding,
                                           struct mylite_expression_value *out_value)
{
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
        return mylite_select_aggregate_format_double(state->sum / (double)state->non_null_count,
                                                     out_value);
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
    case MYLITE_SQL_AST_AGGREGATE_NONE:
        break;
    }

    *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
    return MYLITE_OK;
}

void mylite_select_aggregate_state_deinit(struct mylite_select_aggregate_state *state)
{
    if (state == NULL) {
        return;
    }

    mylite_expression_value_deinit(&state->value);
    for (size_t index = 0U; index < state->distinct_tuple_count; ++index) {
        count_distinct_tuple_deinit(&state->distinct_tuples[index]);
    }
    free(state->distinct_tuples);
    *state = (struct mylite_select_aggregate_state){0};
}

static int update_table_select_count_distinct_state(
    mylite_stmt *stmt, struct mylite_select_aggregate_state *state,
    const struct mylite_select_aggregate_binding *binding,
    const struct mylite_table_select_row *row, const struct mylite_select_eval_callbacks *callbacks)
{
    struct mylite_count_distinct_tuple tuple = {0};
    bool has_null = false;
    int status = evaluate_table_select_count_distinct_tuple(stmt, binding, row, callbacks, &tuple,
                                                            &has_null);

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

static int evaluate_table_select_count_distinct_tuple(
    mylite_stmt *stmt, const struct mylite_select_aggregate_binding *binding,
    const struct mylite_table_select_row *row, const struct mylite_select_eval_callbacks *callbacks,
    struct mylite_count_distinct_tuple *out_tuple, bool *out_has_null)
{
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
         argument != NULL; argument = argument->next_sibling) {
        struct mylite_select_aggregate_binding argument_binding = {
            .argument = argument,
            .argument_kind = MYLITE_SQL_AST_AGGREGATE_ARGUMENT_EXPRESSION,
        };
        int status = mylite_select_eval_aggregate_argument(stmt, &argument_binding, row, callbacks,
                                                           &out_tuple->values[index]);

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

static bool count_distinct_tuple_exists(const struct mylite_select_aggregate_state *state,
                                        const struct mylite_count_distinct_tuple *tuple,
                                        const struct mylite_select_aggregate_binding *binding)
{
    for (size_t index = 0U; index < state->distinct_tuple_count; ++index) {
        if (count_distinct_tuples_equal(&state->distinct_tuples[index], tuple, binding)) {
            return true;
        }
    }
    return false;
}

static bool count_distinct_tuples_equal(const struct mylite_count_distinct_tuple *left,
                                        const struct mylite_count_distinct_tuple *right,
                                        const struct mylite_select_aggregate_binding *binding)
{
    if (left->value_count != right->value_count ||
        left->value_count != binding->argument_descriptor_count) {
        return false;
    }

    for (size_t index = 0U; index < left->value_count; ++index) {
        if (mylite_select_compare_distinct_values(&left->values[index], &right->values[index],
                                                  &binding->argument_descriptors[index]) != 0) {
            return false;
        }
    }
    return true;
}

static int append_count_distinct_tuple(struct mylite_select_aggregate_state *state,
                                       struct mylite_count_distinct_tuple *tuple)
{
    struct mylite_count_distinct_tuple *tuples =
        realloc(state->distinct_tuples,
                (state->distinct_tuple_count + 1U) * sizeof(*state->distinct_tuples));

    if (tuples == NULL) {
        return MYLITE_NOMEM;
    }

    state->distinct_tuples = tuples;
    state->distinct_tuples[state->distinct_tuple_count++] = *tuple;
    *tuple = (struct mylite_count_distinct_tuple){0};
    return MYLITE_OK;
}

static void count_distinct_tuple_deinit(struct mylite_count_distinct_tuple *tuple)
{
    if (tuple == NULL) {
        return;
    }

    for (size_t index = 0U; index < tuple->value_count; ++index) {
        mylite_expression_value_deinit(&tuple->values[index]);
    }
    free(tuple->values);
    *tuple = (struct mylite_count_distinct_tuple){0};
}
