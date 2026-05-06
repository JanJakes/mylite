#include "mylite_dml.h"

#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "mylite_runtime.h"
#include "mylite_select.h"
#include "mylite_span.h"
#include "mylite_system_variables.h"
#include "mylite_user_variables.h"
#include "sql/mylite_ast.h"
#include "sql/mylite_expression.h"

#include <stdlib.h>

static int resolve_update_default_column(
    void *user_data,
    const struct mylite_sql_ast_node *function_call,
    const struct mylite_insert_table_column **out_column,
    struct mylite_insert_table *loaded_table
);

static int set_unknown_default_column_error(
    mylite_db *database,
    const struct mylite_sql_ast_node *identifier
);

int mylite_dml_promote_expression_warnings(mylite_db *database, size_t warning_start) {
    const struct mylite_expression_warning *warning = NULL;
    int status = MYLITE_OK;

    if (database == NULL || warning_start >= database->warnings.count) {
        return MYLITE_OK;
    }

    warning = &database->warnings.items[warning_start];
    status = mylite_diagnostics_set_error_message(database, warning->message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

int mylite_dml_set_expression_condition_error(mylite_db *database, size_t warning_start) {
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

int mylite_dml_resolve_update_expression_identifier(
    void *user_data,
    const struct mylite_sql_ast_node *identifier,
    struct mylite_expression_value *out_value
) {
    struct mylite_update_expression_context *context = user_data;
    size_t column_index = 0U;
    int status = MYLITE_OK;

    if (context == NULL || context->table == NULL || context->row == NULL) {
        return -1;
    }
    if (mylite_system_variable_identifier_is_system_variable(identifier)) {
        return mylite_system_variable_eval_identifier(context->database, identifier, out_value);
    }
    if (mylite_user_variable_identifier_is_user_variable(identifier)) {
        return mylite_user_variable_eval_identifier(context->database, identifier, out_value);
    }

    status = mylite_select_resolve_column_reference(context->table, identifier, &column_index);
    if (status != MYLITE_OK || column_index == context->table->column_count ||
        column_index >= context->row->value_count) {
        return -1;
    }
    return mylite_expression_value_copy(&context->row->values[column_index], out_value);
}

int mylite_dml_evaluate_session_function(
    void *user_data,
    const struct mylite_sql_ast_node *function_call,
    const struct mylite_expression_eval_context *expression_context,
    struct mylite_expression_warnings *warnings,
    struct mylite_expression_value *out_value
) {
    struct mylite_update_expression_context *context = user_data;
    const struct mylite_dml_expression_callbacks *callbacks =
        context == NULL ? NULL : context->callbacks;

    if (callbacks == NULL || callbacks->eval_session_function == NULL) {
        return -1;
    }
    return callbacks->eval_session_function(
        callbacks->user_data,
        context == NULL ? NULL : context->table,
        function_call,
        expression_context,
        warnings,
        out_value
    );
}

int mylite_dml_evaluate_subquery(
    void *user_data,
    const struct mylite_sql_ast_node *subquery,
    struct mylite_expression_warnings *warnings,
    struct mylite_expression_value *out_value
) {
    struct mylite_update_expression_context *context = user_data;
    const struct mylite_dml_expression_callbacks *callbacks =
        context == NULL ? NULL : context->callbacks;

    if (callbacks == NULL || callbacks->eval_subquery == NULL) {
        return -1;
    }
    return callbacks->eval_subquery(callbacks->user_data, subquery, warnings, out_value);
}

int mylite_dml_evaluate_default_function(
    void *user_data,
    const struct mylite_sql_ast_node *function_call,
    struct mylite_expression_value *out_value
) {
    const struct mylite_insert_table_column *column = NULL;
    struct mylite_insert_table loaded_table = {0};
    struct mylite_update_expression_context *context = user_data;
    int status = resolve_update_default_column(user_data, function_call, &column, &loaded_table);

    if (status == MYLITE_OK) {
        status = mylite_dml_resolve_default_function_value(context->database, column, out_value);
    }
    mylite_dml_insert_table_deinit(&loaded_table);
    return status;
}

static int resolve_update_default_column(
    void *user_data,
    const struct mylite_sql_ast_node *function_call,
    const struct mylite_insert_table_column **out_column,
    struct mylite_insert_table *loaded_table
) {
    struct mylite_update_expression_context *context = user_data;
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(function_call, 1U);
    const struct mylite_sql_ast_node *identifier =
        arguments == NULL ? NULL : mylite_ast_child_at(arguments, 0U);
    const struct mylite_insert_table *write_table = NULL;
    size_t column_index = 0U;
    int status = MYLITE_OK;

    *out_column = NULL;
    if (context == NULL || context->database == NULL || context->table == NULL ||
        identifier == NULL || loaded_table == NULL) {
        return -1;
    }

    status = mylite_select_resolve_column_reference(context->table, identifier, &column_index);
    if (status != MYLITE_OK) {
        return status;
    }
    if (column_index == context->table->column_count) {
        return set_unknown_default_column_error(context->database, identifier);
    }

    write_table = context->write_table;
    if (write_table == NULL) {
        status = mylite_dml_load_write_table(
            context->database,
            context->table->schema_name,
            context->table->table_name,
            0U,
            loaded_table
        );
        if (status != MYLITE_OK) {
            return status;
        }
        write_table = loaded_table;
    }
    if (column_index >= write_table->column_count) {
        return -1;
    }

    *out_column = &write_table->columns[column_index];
    return MYLITE_OK;
}

static int set_unknown_default_column_error(
    mylite_db *database,
    const struct mylite_sql_ast_node *identifier
) {
    char *reference = mylite_select_copy_reference_name(identifier);
    int status = MYLITE_OK;

    if (reference == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = mylite_diagnostics_set_error_message_parts(
        database,
        "Unknown column '",
        reference,
        "' in 'field list'"
    );
    free(reference);
    if (status == MYLITE_OK) {
        status = mylite_diagnostics_append_error(
            database,
            MYLITE_MYSQL_ER_BAD_FIELD_ERROR,
            mylite_error_message(database)
        );
    }
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}
