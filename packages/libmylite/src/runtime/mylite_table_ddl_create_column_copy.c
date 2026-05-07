#include "mylite_table_ddl_create_column_copy.h"

#include "mylite_span.h"
#include "mylite_statement_ast.h"
#include "mylite_table_ddl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const size_t generated_check_constraint_suffix_digits = 20U;
static const size_t binary_expression_text_overhead = 5U;

static int copy_create_table_column_type(
    const struct mylite_sql_ast_node *type_node,
    struct mylite_create_table_column_type *type
);

static int copy_create_table_column_attributes(
    const struct mylite_sql_ast_node *attributes,
    struct mylite_create_table_column *column,
    struct mylite_create_table_plan *plan
);

static int copy_create_table_column_check_attribute(
    const struct mylite_sql_ast_node *attribute,
    struct mylite_create_table_plan *plan
);

static char *copy_generated_expression_text(const struct mylite_sql_ast_node *expression);
static char *copy_generated_binary_expression_text(const struct mylite_sql_ast_node *expression);
static char *copy_generated_operand_text(const struct mylite_sql_ast_node *expression);
static char *copy_generated_parenthesized_expression_text(
    const struct mylite_sql_ast_node *expression
);
static char *copy_generated_fallback_expression_text(const struct mylite_sql_ast_node *expression);
static const char *generated_operator_symbol(enum mylite_sql_ast_operator operator_kind);
static char *copy_check_constraint_name(
    const struct mylite_create_table_plan *plan,
    const struct mylite_sql_ast_node *constraint_name
);
static char *copy_generated_check_constraint_name(const struct mylite_create_table_plan *plan);
static size_t generated_check_constraint_count(const struct mylite_create_table_plan *plan);
static char *copy_check_binary_expression_text(const struct mylite_sql_ast_node *expression);
static char *copy_check_operand_text(const struct mylite_sql_ast_node *expression);
static char *copy_check_identifier_text(const struct mylite_sql_ast_node *expression);
static char *copy_check_parenthesized_expression_text(const struct mylite_sql_ast_node *expression);
static char *copy_check_fallback_expression_text(const struct mylite_sql_ast_node *expression);
static const char *check_operator_symbol(enum mylite_sql_ast_operator operator_kind);

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
        status = copy_create_table_column_attributes(
            mylite_ast_child_at(column_node, 2U),
            &column,
            plan
        );
    }
    if (status != MYLITE_OK) {
        mylite_table_ddl_create_table_column_deinit(&column);
        return status;
    }
    if (!column.has_default && (column.nullable || column.auto_increment)) {
        column.has_default = true;
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

int mylite_table_ddl_add_create_table_check(
    struct mylite_create_table_plan *plan,
    const struct mylite_create_table_check_ast *input
) {
    struct mylite_create_table_check check = {
        .enforced = input->enforcement != MYLITE_SQL_AST_CONSTRAINT_ENFORCEMENT_NOT_ENFORCED,
        .generated_name = input->constraint_name == NULL,
    };
    struct mylite_create_table_check *checks = NULL;
    int status = MYLITE_OK;

    check.name = copy_check_constraint_name(plan, input->constraint_name);
    if (check.name == NULL) {
        return MYLITE_NOMEM;
    }
    check.clause = mylite_table_ddl_copy_check_clause_text(input->expression);
    if (check.clause == NULL) {
        mylite_table_ddl_create_table_check_deinit(&check);
        return MYLITE_NOMEM;
    }
    status = mylite_table_ddl_copy_check_expression_ast(input->expression, &check);
    if (status != MYLITE_OK) {
        mylite_table_ddl_create_table_check_deinit(&check);
        return status;
    }

    checks = realloc(plan->checks, (plan->check_count + 1U) * sizeof(*plan->checks));
    if (checks == NULL) {
        mylite_table_ddl_create_table_check_deinit(&check);
        return MYLITE_NOMEM;
    }

    plan->checks = checks;
    plan->checks[plan->check_count++] = check;
    return MYLITE_OK;
}

int mylite_table_ddl_copy_check_expression_ast(
    const struct mylite_sql_ast_node *expression,
    struct mylite_create_table_check *check
) {
    struct mylite_sql_ast_node *expression_copy = NULL;
    int status = MYLITE_OK;

    if (expression == NULL || expression->span.text == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    check->expression_sql = mylite_copy_span_text(expression->span.text, expression->span.length);
    if (check->expression_sql == NULL) {
        return MYLITE_NOMEM;
    }
    status = mylite_statement_ast_clone_subtree(
        &check->expression_ast,
        expression,
        expression->span.text,
        check->expression_sql,
        expression->span.length,
        &expression_copy
    );
    if (status != MYLITE_OK) {
        return status;
    }
    check->expression = expression_copy;
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
    struct mylite_create_table_column *column,
    struct mylite_create_table_plan *plan
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
            copy = mylite_copy_column_default_text(mylite_ast_child_at(attribute, 0U));
            if (copy == NULL && mylite_ast_child_at(attribute, 0U) != NULL &&
                mylite_ast_child_at(attribute, 0U)->literal_kind != MYLITE_SQL_AST_LITERAL_NULL) {
                return MYLITE_NOMEM;
            }
            free(column->default_text);
            column->default_text = copy;
            column->has_default = true;
            if (mylite_column_default_node_is_generated(mylite_ast_child_at(attribute, 0U))) {
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
        case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_CHECK:
            return copy_create_table_column_check_attribute(attribute, plan);
        case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_GENERATED:
            copy = copy_generated_expression_text(mylite_ast_child_at(attribute, 0U));
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

static int copy_create_table_column_check_attribute(
    const struct mylite_sql_ast_node *attribute,
    struct mylite_create_table_plan *plan
) {
    const struct mylite_sql_ast_node *first_child = mylite_ast_child_at(attribute, 0U);
    const struct mylite_sql_ast_node *constraint_name =
        first_child != NULL && first_child->kind == MYLITE_SQL_AST_IDENTIFIER ? first_child : NULL;
    const struct mylite_sql_ast_node *expression =
        constraint_name == NULL ? first_child : constraint_name->next_sibling;
    const struct mylite_create_table_check_ast input = {
        .constraint_name = constraint_name,
        .expression = expression,
        .enforcement = attribute->constraint_enforcement,
    };

    return mylite_table_ddl_add_create_table_check(plan, &input);
}

static char *copy_generated_expression_text(const struct mylite_sql_ast_node *expression) {
    if (expression == NULL) {
        return NULL;
    }
    if (expression->kind == MYLITE_SQL_AST_BINARY_EXPRESSION) {
        return copy_generated_binary_expression_text(expression);
    }
    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        return copy_check_identifier_text(expression);
    }
    if (expression->kind == MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION) {
        return copy_generated_parenthesized_expression_text(expression);
    }
    if (expression->kind == MYLITE_SQL_AST_CURRENT_TIMESTAMP) {
        return mylite_copy_span_text("CURRENT_TIMESTAMP", strlen("CURRENT_TIMESTAMP"));
    }
    return copy_generated_fallback_expression_text(expression);
}

static char *copy_generated_binary_expression_text(const struct mylite_sql_ast_node *expression) {
    const struct mylite_sql_ast_node *left_expression = mylite_ast_child_at(expression, 0U);
    const struct mylite_sql_ast_node *right_expression = mylite_ast_child_at(expression, 1U);
    const char *operator_text = generated_operator_symbol(expression->operator_kind);
    char *left_text = NULL;
    char *right_text = NULL;
    char *text = NULL;
    size_t length = 0U;

    if (operator_text == NULL || left_expression == NULL || right_expression == NULL) {
        return copy_generated_fallback_expression_text(expression);
    }

    left_text = copy_generated_operand_text(left_expression);
    right_text = copy_generated_operand_text(right_expression);
    if (left_text == NULL || right_text == NULL) {
        free(left_text);
        free(right_text);
        return NULL;
    }

    length = strlen(left_text) + strlen(operator_text) + strlen(right_text) +
             binary_expression_text_overhead;
    text = malloc(length + 1U);
    if (text != NULL) {
        (void)snprintf(text, length + 1U, "(%s %s %s)", left_text, operator_text, right_text);
    }
    free(left_text);
    free(right_text);
    return text;
}

static char *copy_generated_operand_text(const struct mylite_sql_ast_node *expression) {
    if (expression == NULL) {
        return NULL;
    }
    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        return copy_check_identifier_text(expression);
    }
    if (expression->kind == MYLITE_SQL_AST_BINARY_EXPRESSION) {
        return copy_generated_binary_expression_text(expression);
    }
    if (expression->kind == MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION) {
        return copy_generated_parenthesized_expression_text(expression);
    }
    return copy_generated_fallback_expression_text(expression);
}

static char *copy_generated_parenthesized_expression_text(
    const struct mylite_sql_ast_node *expression
) {
    const struct mylite_sql_ast_node *inner_expression = mylite_ast_child_at(expression, 0U);
    char *inner_text = inner_expression == NULL
                           ? copy_generated_fallback_expression_text(expression)
                           : copy_generated_expression_text(inner_expression);
    char *text = NULL;
    size_t length = 0U;

    if (inner_text == NULL) {
        return NULL;
    }
    length = strlen(inner_text) + 2U;
    text = malloc(length + 1U);
    if (text != NULL) {
        (void)snprintf(text, length + 1U, "(%s)", inner_text);
    }
    free(inner_text);
    return text;
}

static char *copy_generated_fallback_expression_text(const struct mylite_sql_ast_node *expression) {
    return mylite_copy_span_text(expression->span.text, expression->span.length);
}

static const char *generated_operator_symbol(enum mylite_sql_ast_operator operator_kind) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_ADD:
        return "+";
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
        return "-";
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
        return "*";
    case MYLITE_SQL_AST_OPERATOR_DIVIDE:
        return "/";
    case MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE:
        return "div";
    case MYLITE_SQL_AST_OPERATOR_MODULO:
        return "mod";
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
        return "=";
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
        return "<>";
    case MYLITE_SQL_AST_OPERATOR_LESS:
        return "<";
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
        return "<=";
    case MYLITE_SQL_AST_OPERATOR_GREATER:
        return ">";
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
        return ">=";
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_AND:
        return "and";
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_OR:
        return "or";
    case MYLITE_SQL_AST_OPERATOR_NONE:
    case MYLITE_SQL_AST_OPERATOR_POSITIVE:
    case MYLITE_SQL_AST_OPERATOR_NEGATIVE:
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_NOT:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_AND:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_XOR:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_OR:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_LEFT:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_RIGHT:
    case MYLITE_SQL_AST_OPERATOR_IS_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_BETWEEN:
    case MYLITE_SQL_AST_OPERATOR_NOT_BETWEEN:
    case MYLITE_SQL_AST_OPERATOR_LIKE:
    case MYLITE_SQL_AST_OPERATOR_NOT_LIKE:
    case MYLITE_SQL_AST_OPERATOR_IN:
    case MYLITE_SQL_AST_OPERATOR_NOT_IN:
    case MYLITE_SQL_AST_OPERATOR_REGEXP:
    case MYLITE_SQL_AST_OPERATOR_NOT_REGEXP:
    case MYLITE_SQL_AST_OPERATOR_JSON_EXTRACT:
    case MYLITE_SQL_AST_OPERATOR_JSON_UNQUOTE_EXTRACT:
    case MYLITE_SQL_AST_OPERATOR_BINARY_CAST:
    case MYLITE_SQL_AST_OPERATOR_COLLATE:
        return NULL;
    }
    return NULL;
}

static char *copy_check_constraint_name(
    const struct mylite_create_table_plan *plan,
    const struct mylite_sql_ast_node *constraint_name
) {
    if (constraint_name != NULL) {
        return mylite_copy_identifier_span(constraint_name);
    }
    return copy_generated_check_constraint_name(plan);
}

static char *copy_generated_check_constraint_name(const struct mylite_create_table_plan *plan) {
    size_t table_name_length = strlen(plan->table_name);
    size_t length = table_name_length + strlen("_chk_") + generated_check_constraint_suffix_digits;
    char *name = malloc(length + 1U);

    if (name == NULL) {
        return NULL;
    }
    (void)snprintf(
        name,
        length + 1U,
        "%s_chk_%zu",
        plan->table_name,
        generated_check_constraint_count(plan) + 1U
    );
    return name;
}

static size_t generated_check_constraint_count(const struct mylite_create_table_plan *plan) {
    size_t count = 0U;

    for (size_t index = 0U; index < plan->check_count; ++index) {
        if (plan->checks[index].generated_name) {
            ++count;
        }
    }
    return count;
}

char *mylite_table_ddl_copy_check_clause_text(const struct mylite_sql_ast_node *expression) {
    if (expression == NULL) {
        return NULL;
    }
    if (expression->kind == MYLITE_SQL_AST_BINARY_EXPRESSION) {
        return copy_check_binary_expression_text(expression);
    }
    return copy_check_parenthesized_expression_text(expression);
}

static char *copy_check_binary_expression_text(const struct mylite_sql_ast_node *expression) {
    const struct mylite_sql_ast_node *left_expression = mylite_ast_child_at(expression, 0U);
    const struct mylite_sql_ast_node *right_expression = mylite_ast_child_at(expression, 1U);
    const char *operator_text = check_operator_symbol(expression->operator_kind);
    char *left_text = NULL;
    char *right_text = NULL;
    char *text = NULL;
    size_t length = 0U;

    if (operator_text == NULL || left_expression == NULL || right_expression == NULL) {
        return copy_check_parenthesized_expression_text(expression);
    }

    left_text = copy_check_operand_text(left_expression);
    right_text = copy_check_operand_text(right_expression);
    if (left_text == NULL || right_text == NULL) {
        free(left_text);
        free(right_text);
        return NULL;
    }

    length = strlen(left_text) + strlen(operator_text) + strlen(right_text) +
             binary_expression_text_overhead;
    text = malloc(length + 1U);
    if (text != NULL) {
        (void)snprintf(text, length + 1U, "(%s %s %s)", left_text, operator_text, right_text);
    }
    free(left_text);
    free(right_text);
    return text;
}

static char *copy_check_operand_text(const struct mylite_sql_ast_node *expression) {
    if (expression == NULL) {
        return NULL;
    }
    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        return copy_check_identifier_text(expression);
    }
    if (expression->kind == MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION) {
        return copy_check_parenthesized_expression_text(expression);
    }
    return copy_check_fallback_expression_text(expression);
}

static char *copy_check_identifier_text(const struct mylite_sql_ast_node *expression) {
    const struct mylite_sql_ast_node *identifier_node = expression;
    char *identifier = NULL;
    char *text = NULL;
    size_t length = 0U;

    while (identifier_node != NULL &&
           identifier_node->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        identifier_node = mylite_ast_child_at(identifier_node, 1U);
    }
    identifier = mylite_copy_identifier_span(identifier_node);
    if (identifier == NULL) {
        return NULL;
    }
    length = strlen(identifier) + 2U;
    text = malloc(length + 1U);
    if (text != NULL) {
        (void)snprintf(text, length + 1U, "`%s`", identifier);
    }
    free(identifier);
    return text;
}

static char *copy_check_parenthesized_expression_text(
    const struct mylite_sql_ast_node *expression
) {
    char *inner_text = copy_check_fallback_expression_text(expression);
    char *text = NULL;
    size_t length = 0U;

    if (inner_text == NULL) {
        return NULL;
    }
    length = strlen(inner_text) + 2U;
    text = malloc(length + 1U);
    if (text != NULL) {
        (void)snprintf(text, length + 1U, "(%s)", inner_text);
    }
    free(inner_text);
    return text;
}

static char *copy_check_fallback_expression_text(const struct mylite_sql_ast_node *expression) {
    return mylite_copy_span_text(expression->span.text, expression->span.length);
}

static const char *check_operator_symbol(enum mylite_sql_ast_operator operator_kind) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
        return "=";
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
        return "<>";
    case MYLITE_SQL_AST_OPERATOR_LESS:
        return "<";
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
        return "<=";
    case MYLITE_SQL_AST_OPERATOR_GREATER:
        return ">";
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
        return ">=";
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_AND:
        return "and";
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_OR:
        return "or";
    case MYLITE_SQL_AST_OPERATOR_NONE:
    case MYLITE_SQL_AST_OPERATOR_POSITIVE:
    case MYLITE_SQL_AST_OPERATOR_NEGATIVE:
    case MYLITE_SQL_AST_OPERATOR_ADD:
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
    case MYLITE_SQL_AST_OPERATOR_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_NOT:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_AND:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_XOR:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_OR:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_LEFT:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_RIGHT:
    case MYLITE_SQL_AST_OPERATOR_IS_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_BETWEEN:
    case MYLITE_SQL_AST_OPERATOR_NOT_BETWEEN:
    case MYLITE_SQL_AST_OPERATOR_LIKE:
    case MYLITE_SQL_AST_OPERATOR_NOT_LIKE:
    case MYLITE_SQL_AST_OPERATOR_IN:
    case MYLITE_SQL_AST_OPERATOR_NOT_IN:
    case MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_MODULO:
    case MYLITE_SQL_AST_OPERATOR_REGEXP:
    case MYLITE_SQL_AST_OPERATOR_NOT_REGEXP:
    case MYLITE_SQL_AST_OPERATOR_JSON_EXTRACT:
    case MYLITE_SQL_AST_OPERATOR_JSON_UNQUOTE_EXTRACT:
    case MYLITE_SQL_AST_OPERATOR_BINARY_CAST:
    case MYLITE_SQL_AST_OPERATOR_COLLATE:
        return NULL;
    }
    return NULL;
}
