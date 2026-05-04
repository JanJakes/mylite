#include "mylite_dml.h"

#include "mylite_diagnostics.h"
#include "mylite_runtime.h"
#include "mylite_select.h"
#include "sql/mylite_expression.h"

int mylite_dml_promote_expression_warnings(mylite_db *database, size_t warning_start)
{
    const struct mylite_expression_warning *warning = NULL;
    int status = MYLITE_OK;

    if (database == NULL || warning_start >= database->warnings.count) {
        return MYLITE_OK;
    }

    warning = &database->warnings.items[warning_start];
    status = mylite_diagnostics_set_error_message(database, warning->message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

int mylite_dml_set_expression_condition_error(mylite_db *database, size_t warning_start)
{
    if (database == NULL || warning_start >= database->warnings.count) {
        return MYLITE_OK;
    }

    for (size_t index = warning_start; index < database->warnings.count; ++index) {
        const struct mylite_expression_warning *condition = &database->warnings.items[index];

        if (condition->level == MYLITE_EXPRESSION_WARNING_LEVEL_NOTE) {
            continue;
        }

        int status = mylite_diagnostics_set_error_message(database, condition->message);

        return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
    }
    return MYLITE_OK;
}

int mylite_dml_resolve_update_expression_identifier(void *user_data,
                                                    const struct mylite_sql_ast_node *identifier,
                                                    struct mylite_expression_value *out_value)
{
    struct mylite_update_expression_context *context = user_data;
    size_t column_index = 0U;
    int status = MYLITE_OK;

    if (context == NULL || context->table == NULL || context->row == NULL) {
        return -1;
    }

    status = mylite_select_resolve_column_reference(context->table, identifier, &column_index);
    if (status != MYLITE_OK || column_index == context->table->column_count ||
        column_index >= context->row->value_count) {
        return -1;
    }
    return mylite_expression_value_copy(&context->row->values[column_index], out_value);
}
