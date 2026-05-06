#include "mylite_select_order_resolve.h"

#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "mylite_select.h"
#include "mylite_select_resolve.h"
#include "mylite_span.h"

#include <stdlib.h>

int mylite_select_set_unknown_order_column_error(mylite_db *database, const char *column_name) {
    int status = mylite_diagnostics_set_error_message_parts(
        database,
        "Unknown column '",
        column_name,
        "' in 'order clause'"
    );

    if (status == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_append_error(
        database,
        MYLITE_MYSQL_ER_BAD_FIELD_ERROR,
        mylite_error_message(database)
    );
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

int mylite_select_set_ambiguous_order_column_error(mylite_db *database, const char *column_name) {
    int status = mylite_diagnostics_set_error_message_parts(
        database,
        "Column '",
        column_name,
        "' in order clause is ambiguous"
    );

    if (status == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_append_error(
        database,
        MYLITE_MYSQL_ER_NON_UNIQ_ERROR,
        mylite_error_message(database)
    );
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

int mylite_select_resolve_order_reference(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    enum mylite_select_order_key_kind *out_kind,
    size_t *out_index
) {
    char *parts[3] = {0};
    size_t part_count = 0U;
    int status = mylite_copy_identifier_parts(expression, parts, &part_count);

    *out_kind = MYLITE_SELECT_ORDER_KEY_EXPRESSION;
    *out_index = 0U;
    if (status != MYLITE_OK) {
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
        }
        return status;
    }

    if (part_count == 1U) {
        size_t output_index = 0U;
        size_t output_matches = mylite_select_output_label_count(plan, parts[0], &output_index);

        if (output_matches > 1U) {
            status = mylite_select_set_ambiguous_order_column_error(database, parts[0]);
            goto cleanup;
        }
        if (output_matches == 1U) {
            *out_kind = MYLITE_SELECT_ORDER_KEY_OUTPUT;
            *out_index = output_index;
            goto cleanup;
        }
    }

    status = mylite_select_resolve_plan_column_parts(
        database,
        plan,
        parts,
        part_count,
        "order clause",
        out_index
    );
    if (status == MYLITE_OK) {
        *out_kind = MYLITE_SELECT_ORDER_KEY_EXPRESSION;
    }

cleanup:
    for (size_t index = 0U; index < part_count && index < 3U; ++index) {
        free(parts[index]);
    }
    return status;
}
