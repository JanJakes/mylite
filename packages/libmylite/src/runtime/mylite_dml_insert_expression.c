#include "mylite_dml.h"

#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_dml_insert_bound_value.h"
#include "mylite_dml_insert_column_reference.h"
#include "mylite_dml_insert_copy_value.h"
#include "mylite_dml_insert_default.h"
#include "mylite_dml_insert_diagnostics.h"
#include "mylite_span.h"
#include "mylite_system_variables.h"
#include "mylite_user_variables.h"
#include "sql/mylite_ast.h"
#include "sql/mylite_expression.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct mylite_insert_expression_context {
    mylite_db *database;
    const char *schema_name;
    const struct mylite_insert_values_plan *plan;
    const struct mylite_insert_table *table;
    const struct mylite_insert_bound_value *values;
    const struct mylite_dml_expression_callbacks *callbacks;
};

static int evaluate_insert_expression(
    mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_eval_context *context,
    struct mylite_expression_value *out_value
);

static int resolve_insert_expression_bound_value(
    mylite_db *database,
    const struct mylite_insert_values_plan *plan,
    const struct mylite_insert_table *table,
    const struct mylite_insert_table_column *column,
    const struct mylite_expression_value *value,
    uint64_t statement_row_count,
    struct mylite_insert_execution_state *state,
    struct mylite_insert_bound_value *out_value
);

static int resolve_insert_expression_null_value(
    mylite_db *database,
    const struct mylite_insert_values_plan *plan,
    const struct mylite_insert_table *table,
    const struct mylite_insert_table_column *column,
    uint64_t statement_row_count,
    struct mylite_insert_execution_state *state,
    struct mylite_insert_bound_value *out_value
);

static int resolve_insert_expression_uint64_value(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    const struct mylite_expression_value *value,
    uint64_t statement_row_count,
    struct mylite_insert_execution_state *state,
    bool ignore,
    struct mylite_insert_bound_value *out_value
);

static int resolve_insert_expression_text_value(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    const struct mylite_expression_value *value,
    uint64_t statement_row_count,
    struct mylite_insert_execution_state *state,
    bool ignore,
    struct mylite_insert_bound_value *out_value
);

static int resolve_insert_expression_identifier(
    void *user_data,
    const struct mylite_sql_ast_node *identifier,
    struct mylite_expression_value *out_value
);

static int evaluate_insert_expression_session_function(
    void *user_data,
    const struct mylite_sql_ast_node *function_call,
    const struct mylite_expression_eval_context *expression_context,
    struct mylite_expression_warnings *warnings,
    struct mylite_expression_value *out_value
);

static int evaluate_insert_expression_default_function(
    void *user_data,
    const struct mylite_sql_ast_node *function_call,
    struct mylite_expression_value *out_value
);

static int copy_insert_expression_column_value(
    mylite_db *database,
    const struct mylite_insert_expression_context *context,
    const struct mylite_sql_ast_node *identifier,
    struct mylite_expression_value *out_value
);

static int resolve_default_function_column(
    const struct mylite_insert_expression_context *context,
    const struct mylite_sql_ast_node *function_call,
    const struct mylite_insert_table_column **out_column
);

static int set_insert_unknown_column_error(
    mylite_db *database,
    const struct mylite_insert_column_reference *reference
);

static size_t insert_table_column_pointer_index(
    const struct mylite_insert_table *table,
    const struct mylite_insert_table_column *column
);

static void insert_column_reference_deinit(struct mylite_insert_column_reference *reference);

int mylite_dml_resolve_insert_expression_bound_value(
    mylite_db *database,
    const char *schema_name,
    const struct mylite_insert_values_plan *plan,
    const struct mylite_insert_table *table,
    const struct mylite_insert_bound_value *values,
    const struct mylite_insert_table_column *column,
    const struct mylite_sql_ast_node *expression,
    uint64_t statement_row_count,
    struct mylite_insert_execution_state *state,
    const struct mylite_dml_expression_callbacks *callbacks,
    struct mylite_insert_bound_value *out_value
) {
    struct mylite_insert_expression_context user_context = {
        .database = database,
        .schema_name = schema_name,
        .plan = plan,
        .table = table,
        .values = values,
        .callbacks = callbacks,
    };
    struct mylite_expression_eval_context context = {
        .user_data = &user_context,
        .real_as_float = mylite_connection_sql_mode_has_real_as_float(database),
        .resolve_identifier = resolve_insert_expression_identifier,
        .eval_session_function = evaluate_insert_expression_session_function,
        .eval_default_function = evaluate_insert_expression_default_function,
    };
    struct mylite_expression_value value = {0};
    int status = MYLITE_OK;

    if (database == NULL || plan == NULL || table == NULL || column == NULL || expression == NULL ||
        out_value == NULL) {
        return MYLITE_MISUSE;
    }

    status = evaluate_insert_expression(database, expression, &context, &value);
    if (status == MYLITE_OK) {
        status = resolve_insert_expression_bound_value(
            database,
            plan,
            table,
            column,
            &value,
            statement_row_count,
            state,
            out_value
        );
    }

    mylite_expression_value_deinit(&value);
    return status;
}

static int evaluate_insert_expression(
    mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_eval_context *context,
    struct mylite_expression_value *out_value
) {
    size_t warning_start = database->warnings.count;
    int eval_status =
        mylite_expression_eval_with_context(expression, context, &database->warnings, out_value);
    int status = MYLITE_OK;

    if (eval_status == 0) {
        status = mylite_dml_promote_expression_warnings(database, warning_start);
    } else if (eval_status == MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        status = MYLITE_NOMEM;
    } else if (eval_status == MYLITE_EXEC_ERROR) {
        status = MYLITE_EXEC_ERROR;
    } else {
        status = mylite_dml_set_expression_condition_error(database, warning_start);
        if (status == MYLITE_OK) {
            status = mylite_dml_insert_set_unsupported_expression_error(database);
        }
    }
    return status;
}

static int resolve_insert_expression_bound_value(
    mylite_db *database,
    const struct mylite_insert_values_plan *plan,
    const struct mylite_insert_table *table,
    const struct mylite_insert_table_column *column,
    const struct mylite_expression_value *value,
    uint64_t statement_row_count,
    struct mylite_insert_execution_state *state,
    struct mylite_insert_bound_value *out_value
) {
    bool ignore = (plan != NULL && plan->ignore) != 0;

    switch (value->kind) {
    case MYLITE_EXPRESSION_VALUE_NULL:
        return resolve_insert_expression_null_value(
            database,
            plan,
            table,
            column,
            statement_row_count,
            state,
            out_value
        );
    case MYLITE_EXPRESSION_VALUE_INT64:
        if (value->int64_value == 0 &&
            mylite_dml_insert_auto_increment_zero_generates(database, column)) {
            return mylite_dml_allocate_insert_auto_increment(
                database,
                statement_row_count,
                state,
                out_value
            );
        }
        *out_value = (struct mylite_insert_bound_value){
            .kind = MYLITE_INSERT_BOUND_INTEGER,
            .integer_value = value->int64_value,
        };
        return MYLITE_OK;
    case MYLITE_EXPRESSION_VALUE_UINT64:
        return resolve_insert_expression_uint64_value(
            database,
            column,
            value,
            statement_row_count,
            state,
            ignore,
            out_value
        );
    case MYLITE_EXPRESSION_VALUE_REAL:
        if (value->real_value == 0.0 &&
            mylite_dml_insert_auto_increment_zero_generates(database, column)) {
            return mylite_dml_allocate_insert_auto_increment(
                database,
                statement_row_count,
                state,
                out_value
            );
        }
        if (column->auto_increment) {
            return mylite_dml_insert_set_unsupported_expression_error(database);
        }
        *out_value = (struct mylite_insert_bound_value){
            .kind = MYLITE_INSERT_BOUND_REAL,
            .real_value = value->real_value,
        };
        return MYLITE_OK;
    case MYLITE_EXPRESSION_VALUE_TEXT:
        return resolve_insert_expression_text_value(
            database,
            column,
            value,
            statement_row_count,
            state,
            ignore,
            out_value
        );
    }
    return mylite_dml_insert_set_unsupported_expression_error(database);
}

static int resolve_insert_expression_null_value(
    mylite_db *database,
    const struct mylite_insert_values_plan *plan,
    const struct mylite_insert_table *table,
    const struct mylite_insert_table_column *column,
    uint64_t statement_row_count,
    struct mylite_insert_execution_state *state,
    struct mylite_insert_bound_value *out_value
) {
    if (column->auto_increment) {
        return mylite_dml_allocate_insert_auto_increment(
            database,
            statement_row_count,
            state,
            out_value
        );
    }
    if (!column->nullable) {
        if (!plan->ignore) {
            return mylite_dml_set_not_null_column_error(database, column->name);
        }

        size_t column_index = insert_table_column_pointer_index(table, column);
        int status =
            state == NULL || column_index == table->column_count
                ? mylite_dml_insert_append_null_warning(database, column->name)
                : mylite_dml_insert_append_null_warning_once(database, column, state, column_index);

        if (status != MYLITE_OK) {
            return status;
        }
        return mylite_dml_resolve_insert_implicit_expression_default(database, column, out_value);
    }
    *out_value = (struct mylite_insert_bound_value){.kind = MYLITE_INSERT_BOUND_NULL};
    return MYLITE_OK;
}

static int resolve_insert_expression_uint64_value(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    const struct mylite_expression_value *value,
    uint64_t statement_row_count,
    struct mylite_insert_execution_state *state,
    bool ignore,
    struct mylite_insert_bound_value *out_value
) {
    if (value->uint64_value <= (uint64_t)INT64_MAX) {
        int64_t integer_value = (int64_t)value->uint64_value;

        if (integer_value == 0 &&
            mylite_dml_insert_auto_increment_zero_generates(database, column)) {
            return mylite_dml_allocate_insert_auto_increment(
                database,
                statement_row_count,
                state,
                out_value
            );
        }
        *out_value = (struct mylite_insert_bound_value){
            .kind = MYLITE_INSERT_BOUND_INTEGER,
            .integer_value = integer_value,
        };
        return MYLITE_OK;
    }
    return resolve_insert_expression_text_value(
        database,
        column,
        value,
        statement_row_count,
        state,
        ignore,
        out_value
    );
}

static int resolve_insert_expression_text_value(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    const struct mylite_expression_value *value,
    uint64_t statement_row_count,
    struct mylite_insert_execution_state *state,
    bool ignore,
    struct mylite_insert_bound_value *out_value
) {
    char *text = mylite_expression_value_to_text(value);
    size_t text_length = value->kind == MYLITE_EXPRESSION_VALUE_TEXT && value->text_value != NULL
                             ? value->text_length
                             : 0U;
    int status = MYLITE_OK;

    if (text == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    if (value->kind != MYLITE_EXPRESSION_VALUE_TEXT) {
        text_length = strlen(text);
    }
    status = value->kind == MYLITE_EXPRESSION_VALUE_TEXT
                 ? mylite_dml_resolve_insert_quoted_text_value(
                       database,
                       column,
                       text,
                       text_length,
                       statement_row_count,
                       state,
                       ignore,
                       out_value
                   )
                 : mylite_dml_resolve_insert_text_value(
                       database,
                       column,
                       text,
                       text_length,
                       statement_row_count,
                       state,
                       ignore,
                       out_value
                   );
    free(text);
    return status;
}

static int resolve_insert_expression_identifier(
    void *user_data,
    const struct mylite_sql_ast_node *identifier,
    struct mylite_expression_value *out_value
) {
    struct mylite_insert_expression_context *context = user_data;

    if (context == NULL) {
        return -1;
    }
    if (mylite_system_variable_identifier_is_system_variable(identifier)) {
        return mylite_system_variable_eval_identifier(context->database, identifier, out_value);
    }
    if (mylite_user_variable_identifier_is_user_variable(identifier)) {
        return mylite_user_variable_eval_identifier(context->database, identifier, out_value);
    }
    if (context->values == NULL) {
        return -1;
    }
    return copy_insert_expression_column_value(context->database, context, identifier, out_value);
}

static int evaluate_insert_expression_session_function(
    void *user_data,
    const struct mylite_sql_ast_node *function_call,
    const struct mylite_expression_eval_context *expression_context,
    struct mylite_expression_warnings *warnings,
    struct mylite_expression_value *out_value
) {
    struct mylite_insert_expression_context *context = user_data;

    if (context == NULL || context->callbacks == NULL ||
        context->callbacks->eval_session_function == NULL) {
        return -1;
    }
    return context->callbacks->eval_session_function(
        context->callbacks->user_data,
        NULL,
        function_call,
        expression_context,
        warnings,
        out_value
    );
}

static int evaluate_insert_expression_default_function(
    void *user_data,
    const struct mylite_sql_ast_node *function_call,
    struct mylite_expression_value *out_value
) {
    struct mylite_insert_expression_context *context = user_data;
    const struct mylite_insert_table_column *column = NULL;
    int status = resolve_default_function_column(context, function_call, &column);

    if (status != MYLITE_OK) {
        return status;
    }
    return mylite_dml_resolve_default_function_value(context->database, column, out_value);
}

static int copy_insert_expression_column_value(
    mylite_db *database,
    const struct mylite_insert_expression_context *context,
    const struct mylite_sql_ast_node *identifier,
    struct mylite_expression_value *out_value
) {
    struct mylite_insert_column_reference reference = {0};
    size_t column_index = 0U;
    int status = mylite_dml_copy_insert_column_reference(identifier, &reference);

    if (status != MYLITE_OK) {
        insert_column_reference_deinit(&reference);
        return status;
    }

    column_index = mylite_dml_insert_table_column_reference_index(
        context->table,
        context->schema_name,
        context->plan->table_name,
        &reference
    );
    if (column_index == context->table->column_count) {
        status = set_insert_unknown_column_error(database, &reference);
    } else {
        status = mylite_dml_copy_insert_bound_value_to_expression(
            &context->values[column_index],
            out_value
        );
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
        }
    }

    insert_column_reference_deinit(&reference);
    return status;
}

static int resolve_default_function_column(
    const struct mylite_insert_expression_context *context,
    const struct mylite_sql_ast_node *function_call,
    const struct mylite_insert_table_column **out_column
) {
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(function_call, 1U);
    const struct mylite_sql_ast_node *identifier =
        arguments == NULL ? NULL : mylite_ast_child_at(arguments, 0U);
    struct mylite_insert_column_reference reference = {0};
    size_t column_index = 0U;
    int status = MYLITE_OK;

    *out_column = NULL;
    if (context == NULL || context->table == NULL || identifier == NULL) {
        return -1;
    }

    status = mylite_dml_copy_insert_column_reference(identifier, &reference);
    if (status != MYLITE_OK) {
        insert_column_reference_deinit(&reference);
        return status;
    }
    column_index = mylite_dml_insert_table_column_reference_index(
        context->table,
        context->schema_name,
        context->plan->table_name,
        &reference
    );
    if (column_index == context->table->column_count) {
        status = set_insert_unknown_column_error(context->database, &reference);
    } else {
        *out_column = &context->table->columns[column_index];
    }

    insert_column_reference_deinit(&reference);
    return status;
}

static int set_insert_unknown_column_error(
    mylite_db *database,
    const struct mylite_insert_column_reference *reference
) {
    int status = mylite_diagnostics_set_error_message_parts(
        database,
        "Unknown column '",
        reference == NULL || reference->column_name == NULL ? "" : reference->column_name,
        "' in 'field list'"
    );

    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static size_t insert_table_column_pointer_index(
    const struct mylite_insert_table *table,
    const struct mylite_insert_table_column *column
) {
    for (size_t index = 0U; index < table->column_count; ++index) {
        if (&table->columns[index] == column) {
            return index;
        }
    }
    return table->column_count;
}

static void insert_column_reference_deinit(struct mylite_insert_column_reference *reference) {
    if (reference == NULL) {
        return;
    }
    free(reference->schema_name);
    free(reference->table_name);
    free(reference->column_name);
    *reference = (struct mylite_insert_column_reference){0};
}
