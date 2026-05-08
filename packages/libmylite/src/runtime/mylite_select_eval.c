#include "mylite_select_eval.h"

#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_expression.h"
#include "mylite_runtime.h"
#include "mylite_select.h"
#include "mylite_select_eval_expression.h"
#include "mylite_select_rowset.h"
#include "mylite_span.h"

#include <stdio.h>
#include <stdlib.h>

static char *select_output_value_to_text(
    const mylite_stmt *stmt,
    size_t output_index,
    const struct mylite_expression_value *value
);

static const struct mylite_field_descriptor *select_output_descriptor(
    const mylite_stmt *stmt,
    size_t output_index
);

static char *select_decimal_output_value_to_text(
    const struct mylite_expression_value *value,
    unsigned int decimals
);

static bool select_output_value_as_double(
    const struct mylite_expression_value *value,
    double *out_number
);

static char *format_decimal_output_value(double value, unsigned int decimals);

int mylite_select_eval_group_key(
    mylite_stmt *stmt,
    const struct mylite_table_select_row *row,
    const struct mylite_select_group_key *group_key,
    const struct mylite_select_eval_callbacks *callbacks,
    struct mylite_expression_value *out_value
) {
    if (group_key->kind == MYLITE_SELECT_GROUP_KEY_OUTPUT) {
        return mylite_select_eval_cached_output_value(
            stmt,
            row,
            group_key->output_index,
            callbacks,
            out_value
        );
    }

    struct mylite_table_select_eval_context user_context = {0};
    struct mylite_expression_eval_context context = {0};
    int status = 0;

    mylite_select_eval_context_init(&user_context, stmt, row, callbacks, false, false);
    mylite_select_eval_expression_context_init(&context, &user_context);
    status = mylite_expression_eval_with_context(
        group_key->expression,
        &context,
        &stmt->database->warnings,
        out_value
    );
    if (status != 0) {
        return mylite_select_eval_map_expression_status(stmt, status, callbacks);
    }
    return MYLITE_OK;
}

int mylite_select_eval_having(
    mylite_stmt *stmt,
    const struct mylite_table_select_row *row,
    const struct mylite_select_eval_callbacks *callbacks,
    bool *out_matches
) {
    struct mylite_expression_value value = {0};
    int truth = -1;
    int status = 0;

    *out_matches = true;
    if (stmt->select_plan.having_expression == NULL) {
        return MYLITE_OK;
    }

    struct mylite_table_select_eval_context user_context = {0};
    struct mylite_expression_eval_context context = {0};

    mylite_select_eval_context_init(&user_context, stmt, row, callbacks, false, true);
    mylite_select_eval_expression_context_init(&context, &user_context);
    status = mylite_expression_eval_with_context(
        stmt->select_plan.having_expression,
        &context,
        &stmt->database->warnings,
        &value
    );
    if (status == 0) {
        status = mylite_expression_value_truth(&value, &stmt->database->warnings, &truth);
    }
    mylite_expression_value_deinit(&value);
    if (status != 0) {
        return mylite_select_eval_map_expression_status(stmt, status, callbacks);
    }

    *out_matches = truth == 1;
    return MYLITE_OK;
}

int mylite_select_eval_aggregate_argument(
    mylite_stmt *stmt,
    const struct mylite_select_aggregate_binding *binding,
    const struct mylite_table_select_row *row,
    const struct mylite_select_eval_callbacks *callbacks,
    struct mylite_expression_value *out_value
) {
    struct mylite_table_select_eval_context user_context = {0};
    struct mylite_expression_eval_context context = {0};
    int status = 0;

    mylite_select_eval_context_init(&user_context, stmt, row, callbacks, false, false);
    mylite_select_eval_expression_context_init(&context, &user_context);
    status = mylite_expression_eval_with_context(
        binding->argument,
        &context,
        &stmt->database->warnings,
        out_value
    );
    if (status != 0) {
        return mylite_select_eval_map_expression_status(stmt, status, callbacks);
    }
    return MYLITE_OK;
}

int mylite_select_eval_order_values(
    mylite_stmt *stmt,
    struct mylite_table_select_row *row,
    const struct mylite_select_eval_callbacks *callbacks
) {
    size_t order_key_count = stmt->select_plan.order_key_count;

    row->order_values = calloc(order_key_count, sizeof(*row->order_values));
    if (row->order_values == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    row->order_value_count = order_key_count;

    for (size_t index = 0U; index < order_key_count; ++index) {
        const struct mylite_select_order_key *order_key = &stmt->select_plan.order_keys[index];
        int status = MYLITE_OK;

        if (order_key->kind == MYLITE_SELECT_ORDER_KEY_OUTPUT) {
            status = mylite_select_eval_cached_output_value(
                stmt,
                row,
                order_key->output_index,
                callbacks,
                &row->order_values[index]
            );
        } else {
            struct mylite_table_select_eval_context user_context = {0};
            struct mylite_expression_eval_context context = {0};
            int eval_status = 0;

            mylite_select_eval_context_init(&user_context, stmt, row, callbacks, true, false);
            mylite_select_eval_expression_context_init(&context, &user_context);
            eval_status = mylite_expression_eval_with_context(
                order_key->expression,
                &context,
                &stmt->database->warnings,
                &row->order_values[index]
            );
            if (eval_status != 0) {
                status = mylite_select_eval_map_expression_status(stmt, eval_status, callbacks);
            }
        }

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

int mylite_select_eval_materialize_output_values(
    mylite_stmt *stmt,
    struct mylite_table_select_row *row,
    const struct mylite_select_eval_callbacks *callbacks
) {
    if (row->output_value_count == stmt->select_plan.output_count) {
        return MYLITE_OK;
    }

    row->output_values = calloc(stmt->select_plan.output_count, sizeof(*row->output_values));
    if (row->output_values == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    row->output_value_count = stmt->select_plan.output_count;

    for (size_t index = 0U; index < stmt->select_plan.output_count; ++index) {
        int status = mylite_select_eval_output_value(
            stmt,
            row,
            index,
            callbacks,
            &row->output_values[index]
        );

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

int mylite_select_eval_set_current_row(
    mylite_stmt *stmt,
    const struct mylite_table_select_row *row,
    const struct mylite_select_eval_callbacks *callbacks
) {
    mylite_select_result_current_values_deinit(&stmt->select_result);

    stmt->select_result.current_values =
        calloc(stmt->select_plan.output_count, sizeof(*stmt->select_result.current_values));
    stmt->select_result.current_texts =
        (char **)calloc(stmt->select_plan.output_count, sizeof(*stmt->select_result.current_texts));
    if (stmt->select_result.current_values == NULL || stmt->select_result.current_texts == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    stmt->select_result.current_value_count = stmt->select_plan.output_count;

    for (size_t index = 0U; index < stmt->select_plan.output_count; ++index) {
        int status = mylite_select_eval_cached_output_value(
            stmt,
            row,
            index,
            callbacks,
            &stmt->select_result.current_values[index]
        );

        if (status != MYLITE_OK) {
            return status;
        }
        if (stmt->select_result.current_values[index].kind != MYLITE_EXPRESSION_VALUE_NULL) {
            stmt->select_result.current_texts[index] = select_output_value_to_text(
                stmt,
                index,
                &stmt->select_result.current_values[index]
            );
            if (stmt->select_result.current_texts[index] == NULL) {
                (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
                return MYLITE_NOMEM;
            }
        }
    }
    stmt->select_result.has_current_row = true;
    return MYLITE_OK;
}

int mylite_select_eval_constant_predicate(
    mylite_stmt *stmt,
    const struct mylite_select_eval_callbacks *callbacks
) {
    struct mylite_expression_value value = {0};
    struct mylite_expression_eval_context context = {
        .real_as_float = mylite_connection_sql_mode_has_real_as_float(stmt->database),
        .character_set_connection = mylite_connection_character_set_connection(stmt->database),
    };
    int truth = -1;
    int status = 0;

    if (stmt->select_constant_predicate_evaluated ||
        !mylite_expression_is_cacheable_no_table(stmt->select_predicate)) {
        return MYLITE_OK;
    }

    status = mylite_expression_eval_with_context(
        stmt->select_predicate,
        &context,
        &stmt->database->warnings,
        &value
    );
    if (status == 0) {
        status = mylite_expression_value_truth(&value, &stmt->database->warnings, &truth);
    }
    if (status != 0) {
        mylite_expression_value_deinit(&value);
        return callbacks->set_expression_eval_error(stmt);
    }

    stmt->select_constant_predicate_evaluated = true;
    stmt->select_constant_predicate_matches = truth == 1;
    mylite_expression_value_deinit(&value);
    return MYLITE_OK;
}

int mylite_select_eval_expression_predicate(
    mylite_stmt *stmt,
    const struct mylite_table_select_row *row,
    const struct mylite_sql_ast_node *predicate,
    const struct mylite_select_eval_callbacks *callbacks,
    bool *out_matches
) {
    struct mylite_table_select_eval_context user_context = {0};
    struct mylite_expression_eval_context context = {0};
    struct mylite_expression_value value = {0};
    int truth = 0;
    int status = 0;

    *out_matches = false;
    mylite_select_eval_context_init(&user_context, stmt, row, callbacks, false, false);
    mylite_select_eval_expression_context_init(&context, &user_context);
    status =
        mylite_expression_eval_with_context(predicate, &context, &stmt->database->warnings, &value);
    if (status == 0) {
        status = mylite_expression_value_truth(&value, &stmt->database->warnings, &truth);
    }
    mylite_expression_value_deinit(&value);
    if (status != 0) {
        return mylite_select_eval_map_expression_status(stmt, status, callbacks);
    }

    *out_matches = truth == 1;
    return MYLITE_OK;
}

int mylite_select_eval_row_predicate(
    mylite_stmt *stmt,
    const struct mylite_table_select_row *row,
    const struct mylite_select_eval_callbacks *callbacks,
    bool *out_matches
) {
    return mylite_select_eval_expression_predicate(
        stmt,
        row,
        stmt->select_predicate,
        callbacks,
        out_matches
    );
}

static char *select_output_value_to_text(
    const mylite_stmt *stmt,
    size_t output_index,
    const struct mylite_expression_value *value
) {
    const struct mylite_field_descriptor *descriptor = select_output_descriptor(stmt, output_index);

    if (descriptor != NULL && descriptor->type == MYLITE_FIELD_TYPE_NEWDECIMAL) {
        char *decimal_text = select_decimal_output_value_to_text(value, descriptor->decimals);

        if (decimal_text != NULL) {
            return decimal_text;
        }
    }
    return mylite_expression_value_to_text(value);
}

static const struct mylite_field_descriptor *select_output_descriptor(
    const mylite_stmt *stmt,
    size_t output_index
) {
    if (stmt == NULL || output_index >= stmt->result_metadata.column_count ||
        stmt->result_metadata.columns == NULL) {
        return NULL;
    }
    return &stmt->result_metadata.columns[output_index].descriptor;
}

static char *select_decimal_output_value_to_text(
    const struct mylite_expression_value *value,
    unsigned int decimals
) {
    double number = 0.0;

    if (value != NULL && value->kind == MYLITE_EXPRESSION_VALUE_TEXT) {
        return mylite_copy_span_text(
            value->text_value == NULL ? "" : value->text_value,
            value->text_value == NULL ? 0U : value->text_length
        );
    }
    if (!select_output_value_as_double(value, &number)) {
        return NULL;
    }
    return format_decimal_output_value(number, decimals);
}

static bool select_output_value_as_double(
    const struct mylite_expression_value *value,
    double *out_number
) {
    if (value == NULL || out_number == NULL || value->kind == MYLITE_EXPRESSION_VALUE_NULL) {
        return false;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_INT64) {
        *out_number = (double)value->int64_value;
        return true;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_UINT64) {
        *out_number = (double)value->uint64_value;
        return true;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_REAL) {
        *out_number = value->real_value;
        return true;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_TEXT) {
        char *copy = mylite_copy_span_text(
            value->text_value == NULL ? "" : value->text_value,
            value->text_value == NULL ? 0U : value->text_length
        );
        char *end = NULL;
        double number = 0.0;

        if (copy == NULL) {
            return false;
        }
        number = strtod(copy, &end);
        if (end == copy) {
            free(copy);
            return false;
        }
        free(copy);
        *out_number = number;
        return true;
    }
    return false;
}

static char *format_decimal_output_value(double value, unsigned int decimals) {
    int length = snprintf(NULL, 0U, "%.*f", (int)decimals, value);
    char *text = NULL;
    int written = 0;

    if (length < 0) {
        return NULL;
    }
    text = malloc((size_t)length + 1U);
    if (text == NULL) {
        return NULL;
    }
    written = snprintf(text, (size_t)length + 1U, "%.*f", (int)decimals, value);
    if (written != length) {
        free(text);
        return NULL;
    }
    return text;
}
