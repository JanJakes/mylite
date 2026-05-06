#include "mylite_table_ddl_create_column_copy.h"

#include "mylite_span.h"
#include "mylite_table_ddl.h"

#include <stdlib.h>
#include <string.h>

static int copy_create_table_column_type(
    const struct mylite_sql_ast_node *type_node,
    struct mylite_create_table_column_type *type
);

static int copy_create_table_column_attributes(
    const struct mylite_sql_ast_node *attributes,
    struct mylite_create_table_column *column
);

static char *copy_expression_text(const struct mylite_sql_ast_node *node);

int mylite_table_ddl_copy_create_table_column(
    const struct mylite_sql_ast_node *column_node,
    struct mylite_create_table_plan *plan
) {
    struct mylite_create_table_column *columns = NULL;
    struct mylite_create_table_column column = {
        .nullable = true,
        .visible = true,
    };
    int status = MYLITE_OK;

    column.name = mylite_copy_identifier_span(mylite_ast_child_at(column_node, 0U));
    if (column.name == NULL) {
        return MYLITE_NOMEM;
    }
    status = copy_create_table_column_type(mylite_ast_child_at(column_node, 1U), &column.type);
    if (status == MYLITE_OK) {
        status = copy_create_table_column_attributes(mylite_ast_child_at(column_node, 2U), &column);
    }
    if (status != MYLITE_OK) {
        mylite_table_ddl_create_table_column_deinit(&column);
        return status;
    }

    columns = realloc(plan->columns, (plan->column_count + 1U) * sizeof(*plan->columns));
    if (columns == NULL) {
        mylite_table_ddl_create_table_column_deinit(&column);
        return MYLITE_NOMEM;
    }

    plan->columns = columns;
    plan->columns[plan->column_count++] = column;
    return MYLITE_OK;
}

static int copy_create_table_column_type(
    const struct mylite_sql_ast_node *type_node,
    struct mylite_create_table_column_type *type
) {
    if (type_node == NULL || type_node->kind != MYLITE_SQL_AST_COLUMN_TYPE) {
        return MYLITE_UNSUPPORTED;
    }

    *type = (struct mylite_create_table_column_type){
        .ast_type = type_node->column_type,
        .attributes = {
            .display_width = type_node->column_display_width,
            .length = type_node->column_length,
            .precision = type_node->column_precision,
            .scale = type_node->column_scale,
            .has_display_width = type_node->has_column_display_width,
            .has_signed = type_node->column_type_signed,
            .has_unsigned = type_node->column_type_unsigned,
            .has_length = type_node->has_column_length,
            .has_precision = type_node->has_column_precision,
            .has_scale = type_node->has_column_scale,
            .has_binary_attribute = type_node->column_binary_attribute,
            .has_byte_attribute = type_node->column_byte_attribute,
            .has_zerofill_attribute = type_node->column_zerofill_attribute,
            .is_national = type_node->column_national_attribute,
        },
    };
    if (type_node->has_column_character_set) {
        type->character_set = mylite_copy_span_text(
            type_node->column_character_set.text,
            type_node->column_character_set.length
        );
        if (type->character_set == NULL) {
            return MYLITE_NOMEM;
        }
        type->attributes.has_character_set = true;
        type->attributes.character_set = type->character_set;
        type->attributes.character_set_length = strlen(type->character_set);
    }
    if (type_node->has_column_collation) {
        type->collation = mylite_copy_span_text(
            type_node->column_collation.text,
            type_node->column_collation.length
        );
        if (type->collation == NULL) {
            return MYLITE_NOMEM;
        }
        type->attributes.has_collation = true;
        type->attributes.collation = type->collation;
        type->attributes.collation_length = strlen(type->collation);
    }
    return MYLITE_OK;
}

static int copy_create_table_column_attributes(
    const struct mylite_sql_ast_node *attributes,
    struct mylite_create_table_column *column
) {
    const struct mylite_sql_ast_node *attribute = NULL;

    for (attribute = attributes == NULL ? NULL : attributes->first_child; attribute != NULL;
         attribute = attribute->next_sibling) {
        char *copy = NULL;

        switch (attribute->column_attribute) {
        case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_NULL:
            column->nullable = true;
            break;
        case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_NOT_NULL:
            column->nullable = false;
            break;
        case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_DEFAULT:
            copy = copy_expression_text(mylite_ast_child_at(attribute, 0U));
            if (copy == NULL && mylite_ast_child_at(attribute, 0U) != NULL &&
                mylite_ast_child_at(attribute, 0U)->literal_kind != MYLITE_SQL_AST_LITERAL_NULL) {
                return MYLITE_NOMEM;
            }
            free(column->default_text);
            column->default_text = copy;
            if (mylite_ast_child_at(attribute, 0U) != NULL &&
                (mylite_ast_child_at(attribute, 0U)->kind == MYLITE_SQL_AST_CURRENT_TIMESTAMP ||
                 mylite_ast_child_at(attribute, 0U)->kind ==
                     MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION)) {
                column->has_generated_default = true;
            }
            break;
        case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_ON_UPDATE:
            column->has_on_update_current_timestamp = true;
            break;
        case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_COMMENT:
            copy = mylite_copy_string_literal_span(mylite_ast_child_at(attribute, 0U));
            if (copy == NULL) {
                return MYLITE_NOMEM;
            }
            free(column->comment);
            column->comment = copy;
            break;
        case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_VISIBLE:
            column->visible = true;
            break;
        case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_INVISIBLE:
            column->visible = false;
            break;
        case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_AUTO_INCREMENT:
            column->auto_increment = true;
            break;
        case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_PRIMARY_KEY:
            column->primary_key = true;
            column->nullable = false;
            break;
        case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_UNIQUE_KEY:
            column->unique_key = true;
            break;
        case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_REFERENCES:
            /* MySQL accepts inline REFERENCES in CREATE TABLE but does not create FK metadata. */
            break;
        case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_GENERATED:
            copy = copy_expression_text(mylite_ast_child_at(attribute, 0U));
            if (copy == NULL) {
                return MYLITE_NOMEM;
            }
            free(column->generation_expression);
            column->generation_expression = copy;
            column->generated_column_storage = attribute->generated_column_storage;
            break;
        case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_COLUMN_FORMAT:
        case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_STORAGE:
        case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_NONE:
            break;
        }
    }
    return MYLITE_OK;
}

static char *copy_expression_text(const struct mylite_sql_ast_node *node) {
    if (node == NULL) {
        return NULL;
    }
    if (node->kind == MYLITE_SQL_AST_LITERAL &&
        node->literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
        return mylite_copy_string_literal_span(node);
    }
    if (node->kind == MYLITE_SQL_AST_LITERAL && node->literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        return NULL;
    }
    if (node->kind == MYLITE_SQL_AST_CURRENT_TIMESTAMP) {
        return mylite_copy_span_text("CURRENT_TIMESTAMP", strlen("CURRENT_TIMESTAMP"));
    }
    return mylite_copy_span_text(node->span.text, node->span.length);
}
