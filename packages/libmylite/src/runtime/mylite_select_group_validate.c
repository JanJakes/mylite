#include "mylite_select_group_validate.h"

#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "mylite_select_group_invariant.h"
#include "mylite_span.h"

#include <stdlib.h>

static int validate_select_grouping_clause_expression(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    enum mylite_select_grouping_reference_policy reference_policy
);

static int set_select_only_full_group_by_error(
    mylite_db *database,
    const char *expression_text,
    bool implicit_group
);

int mylite_select_validate_grouping(mylite_db *database, const struct mylite_select_plan *plan) {
    bool aggregate_query = (plan->has_aggregate || plan->has_group_by) != 0;
    bool implicit_group = true;

    if (plan->has_group_by) {
        implicit_group = false;
    }

    if (!aggregate_query) {
        return MYLITE_OK;
    }
    if (!mylite_connection_sql_mode_has_only_full_group_by(database)) {
        return MYLITE_OK;
    }

    for (size_t index = 0U; index < plan->output_count; ++index) {
        const struct mylite_select_output_column *output = &plan->outputs[index];

        if (!mylite_select_output_is_group_invariant(plan, index)) {
            return set_select_only_full_group_by_error(database, output->label, implicit_group);
        }
    }
    if (plan->having_expression != NULL) {
        int status = validate_select_grouping_clause_expression(
            database,
            plan,
            plan->having_expression,
            MYLITE_SELECT_GROUPING_REFERENCE_HAVING
        );

        if (status != MYLITE_OK) {
            return status;
        }
    }
    for (size_t index = 0U; index < plan->order_key_count; ++index) {
        const struct mylite_select_order_key *order_key = &plan->order_keys[index];

        if (order_key->kind == MYLITE_SELECT_ORDER_KEY_OUTPUT) {
            if (!mylite_select_output_is_group_invariant(plan, order_key->output_index)) {
                const char *label = order_key->output_index < plan->output_count
                                        ? plan->outputs[order_key->output_index].label
                                        : "";

                return set_select_only_full_group_by_error(database, label, implicit_group);
            }
            continue;
        }

        {
            int status = validate_select_grouping_clause_expression(
                database,
                plan,
                order_key->expression,
                MYLITE_SELECT_GROUPING_REFERENCE_ORDER
            );

            if (status != MYLITE_OK) {
                return status;
            }
        }
    }
    return MYLITE_OK;
}

bool mylite_select_output_contains_aggregate(
    const struct mylite_select_plan *plan,
    size_t output_index
) {
    if (output_index >= plan->output_count) {
        return false;
    }
    if (plan->outputs[output_index].kind != MYLITE_SELECT_OUTPUT_EXPRESSION) {
        return false;
    }
    return mylite_select_expression_contains_aggregate(plan->outputs[output_index].expression);
}

static int validate_select_grouping_clause_expression(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    enum mylite_select_grouping_reference_policy reference_policy
) {
    char *expression_text = NULL;
    bool implicit_group = true;
    int status = MYLITE_OK;

    if (plan->has_group_by) {
        implicit_group = false;
    }

    if (mylite_select_expression_is_group_invariant(plan, expression, reference_policy)) {
        return MYLITE_OK;
    }

    expression_text = expression == NULL
                          ? NULL
                          : mylite_copy_span_text(expression->span.text, expression->span.length);
    if (expression != NULL && expression_text == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = set_select_only_full_group_by_error(database, expression_text, implicit_group);
    free(expression_text);
    return status;
}

static int set_select_only_full_group_by_error(
    mylite_db *database,
    const char *expression_text,
    bool implicit_group
) {
    int status = MYLITE_OK;

    if (implicit_group) {
        status = mylite_diagnostics_set_error_message_parts(
            database,
            "In aggregated query without GROUP BY, expression contains nonaggregated "
            "column '",
            expression_text == NULL ? "" : expression_text,
            "'; this is incompatible with sql_mode=only_full_group_by"
        );
        if (status == MYLITE_NOMEM) {
            return MYLITE_NOMEM;
        }
        status = mylite_diagnostics_append_error(
            database,
            MYLITE_MYSQL_ER_MIX_OF_GROUP_FUNC_AND_FIELDS,
            mylite_error_message(database)
        );
    } else {
        status = mylite_diagnostics_set_error_message_parts(
            database,
            "Expression contains nonaggregated column '",
            expression_text == NULL ? "" : expression_text,
            "' which is not functionally dependent on GROUP BY; this is incompatible with "
            "sql_mode=only_full_group_by"
        );
        if (status == MYLITE_NOMEM) {
            return MYLITE_NOMEM;
        }
        status = mylite_diagnostics_append_error(
            database,
            MYLITE_MYSQL_ER_WRONG_FIELD_WITH_GROUP,
            mylite_error_message(database)
        );
    }
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}
