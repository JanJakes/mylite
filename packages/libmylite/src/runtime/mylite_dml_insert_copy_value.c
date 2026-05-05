#include "mylite_dml_insert_copy_value.h"

#include "mylite_span.h"

#include <stdlib.h>
#include <string.h>

static int copy_insert_simple_value(const struct mylite_sql_ast_node *value_node,
                                    struct mylite_insert_value *out_value);
static int copy_insert_values_function(const struct mylite_sql_ast_node *value_node,
                                       struct mylite_insert_value *out_value);
static int copy_insert_column_reference_parts(const struct mylite_sql_ast_node *identifier,
                                              char **parts, size_t *part_count);
static int copy_insert_literal_value(const struct mylite_sql_ast_node *literal,
                                     struct mylite_insert_value *out_value);
static int copy_insert_unary_value(const struct mylite_sql_ast_node *expression,
                                   struct mylite_insert_value *out_value);
static int copy_insert_binary_value(const struct mylite_sql_ast_node *expression,
                                    struct mylite_insert_value *out_value);

int mylite_dml_copy_insert_value(const struct mylite_sql_ast_node *value_node,
                                 struct mylite_insert_value *out_value)
{
    while (value_node != NULL && value_node->kind == MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION) {
        value_node = mylite_ast_child_at(value_node, 0U);
    }
    if (value_node == NULL) {
        return MYLITE_UNSUPPORTED;
    }

    if (value_node->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return copy_insert_unary_value(value_node, out_value);
    }
    if (value_node->kind == MYLITE_SQL_AST_BINARY_EXPRESSION) {
        return copy_insert_binary_value(value_node, out_value);
    }
    return copy_insert_simple_value(value_node, out_value);
}

int mylite_dml_copy_insert_column_reference(const struct mylite_sql_ast_node *identifier,
                                            struct mylite_insert_column_reference *out_reference)
{
    char *parts[3] = {0};
    size_t part_count = 0U;
    int status = copy_insert_column_reference_parts(identifier, parts, &part_count);

    if (status != MYLITE_OK) {
        for (size_t index = 0U; index < part_count; ++index) {
            free(parts[index]);
        }
        return status;
    }

    if (part_count == 1U) {
        out_reference->column_name = parts[0];
        return MYLITE_OK;
    }
    if (part_count == 2U) {
        out_reference->table_name = parts[0];
        out_reference->column_name = parts[1];
        return MYLITE_OK;
    }
    if (part_count == 3U) {
        out_reference->schema_name = parts[0];
        out_reference->table_name = parts[1];
        out_reference->column_name = parts[2];
        return MYLITE_OK;
    }

    return MYLITE_UNSUPPORTED;
}

static int copy_insert_simple_value(const struct mylite_sql_ast_node *value_node,
                                    struct mylite_insert_value *out_value)
{
    while (value_node != NULL && value_node->kind == MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION) {
        value_node = mylite_ast_child_at(value_node, 0U);
    }
    if (value_node == NULL) {
        return MYLITE_UNSUPPORTED;
    }

    if (value_node->kind == MYLITE_SQL_AST_DEFAULT) {
        *out_value = (struct mylite_insert_value){.kind = MYLITE_INSERT_VALUE_DEFAULT};
        return MYLITE_OK;
    }
    if (value_node->kind == MYLITE_SQL_AST_LITERAL) {
        return copy_insert_literal_value(value_node, out_value);
    }
    if (value_node->kind == MYLITE_SQL_AST_CURRENT_TIMESTAMP) {
        *out_value = (struct mylite_insert_value){.kind = MYLITE_INSERT_VALUE_CURRENT_TIMESTAMP};
        return MYLITE_OK;
    }
    if (value_node->kind == MYLITE_SQL_AST_IDENTIFIER ||
        value_node->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        out_value->kind = MYLITE_INSERT_VALUE_COLUMN_REFERENCE;
        return mylite_dml_copy_insert_column_reference(value_node, &out_value->column_reference);
    }
    if (value_node->kind == MYLITE_SQL_AST_FUNCTION_CALL) {
        return copy_insert_values_function(value_node, out_value);
    }

    *out_value = (struct mylite_insert_value){.kind = MYLITE_INSERT_VALUE_UNSUPPORTED};
    return MYLITE_OK;
}

static int copy_insert_values_function(const struct mylite_sql_ast_node *value_node,
                                       struct mylite_insert_value *out_value)
{
    const struct mylite_sql_ast_node *name = mylite_ast_child_at(value_node, 0U);
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(value_node, 1U);
    const struct mylite_sql_ast_node *argument = NULL;

    if (name == NULL || name->kind != MYLITE_SQL_AST_IDENTIFIER ||
        !mylite_span_equal_ci(name->span, "VALUES") || arguments == NULL ||
        arguments->kind != MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST ||
        arguments->first_child == NULL || arguments->first_child->next_sibling != NULL) {
        *out_value = (struct mylite_insert_value){.kind = MYLITE_INSERT_VALUE_UNSUPPORTED};
        return MYLITE_OK;
    }

    argument = arguments->first_child;
    if (argument->kind != MYLITE_SQL_AST_IDENTIFIER) {
        *out_value = (struct mylite_insert_value){.kind = MYLITE_INSERT_VALUE_UNSUPPORTED};
        return MYLITE_OK;
    }

    out_value->kind = MYLITE_INSERT_VALUE_VALUES_FUNCTION;
    out_value->values_function_count = 1U;
    return mylite_dml_copy_insert_column_reference(argument, &out_value->column_reference);
}

static int copy_insert_column_reference_parts(const struct mylite_sql_ast_node *identifier,
                                              char **parts, size_t *part_count)
{
    const struct mylite_sql_ast_node *segments[3] = {0};
    const struct mylite_sql_ast_node *current = identifier;
    size_t segment_count = 0U;

    *part_count = 0U;
    while (current != NULL && current->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        if (segment_count >= 3U) {
            return MYLITE_UNSUPPORTED;
        }
        segments[segment_count++] = mylite_ast_child_at(current, 1U);
        current = mylite_ast_child_at(current, 0U);
    }
    if (current == NULL || current->kind != MYLITE_SQL_AST_IDENTIFIER || segment_count >= 3U) {
        return MYLITE_UNSUPPORTED;
    }
    segments[segment_count++] = current;

    for (size_t index = 0U; index < segment_count; ++index) {
        const struct mylite_sql_ast_node *segment = segments[segment_count - index - 1U];

        if (segment == NULL || segment->kind != MYLITE_SQL_AST_IDENTIFIER) {
            return MYLITE_UNSUPPORTED;
        }
        parts[index] = mylite_copy_identifier_span(segment);
        if (parts[index] == NULL) {
            for (size_t previous = 0U; previous < index; ++previous) {
                free(parts[previous]);
                parts[previous] = NULL;
            }
            *part_count = 0U;
            return MYLITE_NOMEM;
        }
        *part_count += 1U;
    }
    return MYLITE_OK;
}

static int copy_insert_literal_value(const struct mylite_sql_ast_node *literal,
                                     struct mylite_insert_value *out_value)
{
    if (literal->literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER) {
        out_value->kind = MYLITE_INSERT_VALUE_INTEGER;
        out_value->text = mylite_copy_span_text(literal->span.text, literal->span.length);
        return out_value->text == NULL ? MYLITE_NOMEM : MYLITE_OK;
    }
    if (literal->literal_kind == MYLITE_SQL_AST_LITERAL_DECIMAL ||
        literal->literal_kind == MYLITE_SQL_AST_LITERAL_FLOAT) {
        out_value->kind = MYLITE_INSERT_VALUE_REAL;
        out_value->text = mylite_copy_span_text(literal->span.text, literal->span.length);
        return out_value->text == NULL ? MYLITE_NOMEM : MYLITE_OK;
    }
    if (literal->literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
        out_value->kind = MYLITE_INSERT_VALUE_TEXT;
        out_value->text = mylite_copy_string_literal_span(literal);
        return out_value->text == NULL ? MYLITE_NOMEM : MYLITE_OK;
    }
    if (literal->literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        out_value->kind = MYLITE_INSERT_VALUE_INTEGER;
        out_value->text = mylite_copy_span_text("1", 1U);
        return out_value->text == NULL ? MYLITE_NOMEM : MYLITE_OK;
    }
    if (literal->literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        out_value->kind = MYLITE_INSERT_VALUE_INTEGER;
        out_value->text = mylite_copy_span_text("0", 1U);
        return out_value->text == NULL ? MYLITE_NOMEM : MYLITE_OK;
    }
    if (literal->literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        out_value->kind = MYLITE_INSERT_VALUE_NULL;
        return MYLITE_OK;
    }

    *out_value = (struct mylite_insert_value){.kind = MYLITE_INSERT_VALUE_UNSUPPORTED};
    return MYLITE_OK;
}

static int copy_insert_unary_value(const struct mylite_sql_ast_node *expression,
                                   struct mylite_insert_value *out_value)
{
    const struct mylite_sql_ast_node *operand = mylite_ast_child_at(expression, 0U);
    const char *sign = expression->operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE ? "-" : "+";
    size_t sign_length = strlen(sign);

    if (operand == NULL || operand->kind != MYLITE_SQL_AST_LITERAL ||
        (operand->literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER &&
         operand->literal_kind != MYLITE_SQL_AST_LITERAL_DECIMAL &&
         operand->literal_kind != MYLITE_SQL_AST_LITERAL_FLOAT)) {
        int status = MYLITE_OK;

        out_value->left = calloc(1U, sizeof(*out_value->left));
        if (out_value->left == NULL) {
            return MYLITE_NOMEM;
        }
        out_value->kind = MYLITE_INSERT_VALUE_UNARY_EXPRESSION;
        out_value->operator_kind = expression->operator_kind;
        status = copy_insert_simple_value(operand, out_value->left);
        if (status == MYLITE_OK) {
            out_value->values_function_count = out_value->left->values_function_count;
        }
        return status;
    }

    out_value->kind = operand->literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER
                          ? MYLITE_INSERT_VALUE_INTEGER
                          : MYLITE_INSERT_VALUE_REAL;
    out_value->text = malloc(sign_length + operand->span.length + 1U);
    if (out_value->text != NULL) {
        memcpy(out_value->text, sign, sign_length);
        memcpy(out_value->text + sign_length, operand->span.text, operand->span.length);
        out_value->text[sign_length + operand->span.length] = '\0';
    }
    return out_value->text == NULL ? MYLITE_NOMEM : MYLITE_OK;
}

static int copy_insert_binary_value(const struct mylite_sql_ast_node *expression,
                                    struct mylite_insert_value *out_value)
{
    int status = MYLITE_OK;

    out_value->left = calloc(1U, sizeof(*out_value->left));
    out_value->right = calloc(1U, sizeof(*out_value->right));
    if (out_value->left == NULL || out_value->right == NULL) {
        return MYLITE_NOMEM;
    }

    out_value->kind = MYLITE_INSERT_VALUE_BINARY_EXPRESSION;
    out_value->operator_kind = expression->operator_kind;

    status = copy_insert_simple_value(mylite_ast_child_at(expression, 0U), out_value->left);
    if (status != MYLITE_OK) {
        return status;
    }
    status = copy_insert_simple_value(mylite_ast_child_at(expression, 1U), out_value->right);
    if (status == MYLITE_OK) {
        out_value->values_function_count =
            out_value->left->values_function_count + out_value->right->values_function_count;
    }
    return status;
}
