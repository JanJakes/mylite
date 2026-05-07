#include "mylite_expression_collation_leaf.h"

#include "mylite_diagnostics.h"
#include "mylite_expression_collation.h"
#include "mylite_expression_descriptor.h"
#include "mylite_expression_validation.h"
#include "mylite_metadata_constants.h"
#include "mylite_select.h"
#include "mylite_span.h"
#include "mylite_system_variables.h"
#include "mylite_user_variables.h"

#include <stdlib.h>

static int infer_table_identifier_descriptor(
    const struct mylite_select_table *table,
    const struct mylite_sql_ast_node *expression,
    struct mylite_field_descriptor *out_descriptor
);

int mylite_expression_infer_literal_collation_info(
    mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_collation_callbacks *callbacks,
    struct mylite_charset_collation_info *out_info
) {
    switch (expression->literal_kind) {
    case MYLITE_SQL_AST_LITERAL_NULL:
        *out_info = mylite_expression_binary_collation_info(mylite_mysql_coercibility_ignorable);
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_STRING:
        *out_info = mylite_expression_connection_collation_info(
            database,
            mylite_mysql_coercibility_coercible
        );
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_NATIONAL_STRING:
        *out_info =
            mylite_expression_utf8mb3_general_collation_info(mylite_mysql_coercibility_coercible);
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_INTEGER:
    case MYLITE_SQL_AST_LITERAL_DECIMAL:
    case MYLITE_SQL_AST_LITERAL_FLOAT:
    case MYLITE_SQL_AST_LITERAL_TRUE:
    case MYLITE_SQL_AST_LITERAL_FALSE:
        *out_info = mylite_expression_binary_collation_info(mylite_mysql_coercibility_numeric);
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_DATE:
    case MYLITE_SQL_AST_LITERAL_TIME:
    case MYLITE_SQL_AST_LITERAL_TIMESTAMP:
    case MYLITE_SQL_AST_LITERAL_HEX:
    case MYLITE_SQL_AST_LITERAL_BIT:
    case MYLITE_SQL_AST_LITERAL_NONE:
        return mylite_expression_infer_descriptor_collation_info(
            database,
            NULL,
            expression,
            mylite_mysql_coercibility_coercible,
            callbacks,
            out_info
        );
    }
    return MYLITE_UNSUPPORTED;
}

int mylite_expression_infer_identifier_collation_info(
    mylite_db *database,
    const struct mylite_expression_collation_context *context,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_collation_callbacks *callbacks,
    struct mylite_charset_collation_info *out_info
) {
    struct mylite_field_descriptor descriptor = mylite_expression_descriptor_defaults();
    int status = MYLITE_UNSUPPORTED;

    if (mylite_system_variable_identifier_is_system_variable(expression)) {
        status = mylite_system_variable_infer_identifier(database, expression, &descriptor);
    } else if (mylite_user_variable_identifier_is_user_variable(expression)) {
        status = mylite_user_variable_infer_identifier(database, expression, &descriptor);
    } else if (context != NULL && context->table != NULL) {
        status = infer_table_identifier_descriptor(context->table, expression, &descriptor);
    } else if (
        context != NULL && context->plan != NULL && callbacks != NULL &&
        callbacks->infer_expression_descriptor != NULL
    ) {
        status =
            callbacks
                ->infer_expression_descriptor(database, context->plan, expression, &descriptor);
    }
    if (status != MYLITE_OK) {
        return status;
    }
    *out_info = mylite_expression_descriptor_collation_info(
        &descriptor,
        mylite_mysql_coercibility_implicit
    );
    return MYLITE_OK;
}

int mylite_expression_infer_cast_collation_info(
    mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_collation_callbacks *callbacks,
    struct mylite_charset_collation_info *out_info
) {
    const struct mylite_sql_ast_node *target = mylite_ast_child_at(expression, 1U);

    if (target == NULL || target->kind != MYLITE_SQL_AST_COLUMN_TYPE) {
        return MYLITE_UNSUPPORTED;
    }
    if (target->column_type == MYLITE_SQL_AST_COLUMN_TYPE_BINARY) {
        *out_info = mylite_expression_binary_collation_info(mylite_mysql_coercibility_implicit);
        return MYLITE_OK;
    }
    if (target->column_type == MYLITE_SQL_AST_COLUMN_TYPE_CHAR) {
        char *charset_name = NULL;

        if (!target->has_column_character_set) {
            *out_info = mylite_expression_connection_collation_info(
                database,
                mylite_mysql_coercibility_implicit
            );
            return MYLITE_OK;
        }
        charset_name = mylite_copy_unquoted_span_text(target->column_character_set);
        if (charset_name == NULL) {
            return MYLITE_NOMEM;
        }
        if (!mylite_expression_char_function_charset_name_is_supported(charset_name)) {
            int status = mylite_diagnostics_set_unknown_charset_error(database, charset_name);

            free(charset_name);
            return status;
        }
        *out_info = mylite_expression_charset_collation_info(charset_name);
        out_info->coercibility = mylite_mysql_coercibility_implicit;
        free(charset_name);
        return MYLITE_OK;
    }
    return mylite_expression_infer_descriptor_collation_info(
        database,
        NULL,
        expression,
        mylite_mysql_coercibility_coercible,
        callbacks,
        out_info
    );
}

static int infer_table_identifier_descriptor(
    const struct mylite_select_table *table,
    const struct mylite_sql_ast_node *expression,
    struct mylite_field_descriptor *out_descriptor
) {
    size_t column_index = table == NULL ? 0U : table->column_count;
    int status = table == NULL
                     ? MYLITE_UNSUPPORTED
                     : mylite_select_resolve_column_reference(table, expression, &column_index);

    if (status != MYLITE_OK || table == NULL || column_index >= table->column_count) {
        *out_descriptor = mylite_expression_descriptor_defaults();
        return status == MYLITE_OK ? MYLITE_UNSUPPORTED : status;
    }
    *out_descriptor = table->columns[column_index].descriptor;
    return MYLITE_OK;
}
