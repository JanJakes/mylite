#include "mylite_dml.h"

#include "mylite_span.h"

#include <stdlib.h>
#include <string.h>

static int copy_insert_table_name(const struct mylite_sql_ast_node *table_name,
                                  struct mylite_insert_values_plan *plan);
static int copy_insert_column_list(const struct mylite_sql_ast_node *columns,
                                   struct mylite_insert_values_plan *plan);
static int add_insert_column(struct mylite_insert_values_plan *plan, char *column_name);
static int copy_insert_rows(const struct mylite_sql_ast_node *rows,
                            struct mylite_insert_values_plan *plan);
static int copy_insert_row(const struct mylite_sql_ast_node *row,
                           struct mylite_insert_values_plan *plan);
static int add_insert_row(struct mylite_insert_values_plan *plan, struct mylite_insert_row row);
static int copy_insert_value(const struct mylite_sql_ast_node *value_node,
                             struct mylite_insert_value *out_value);
static int copy_insert_simple_value(const struct mylite_sql_ast_node *value_node,
                                    struct mylite_insert_value *out_value);
static int copy_insert_values_function(const struct mylite_sql_ast_node *value_node,
                                       struct mylite_insert_value *out_value);
static int copy_insert_column_reference(const struct mylite_sql_ast_node *identifier,
                                        struct mylite_insert_column_reference *out_reference);
static int copy_insert_column_reference_parts(const struct mylite_sql_ast_node *identifier,
                                              char **parts, size_t *part_count);
static int copy_insert_literal_value(const struct mylite_sql_ast_node *literal,
                                     struct mylite_insert_value *out_value);
static int copy_insert_unary_value(const struct mylite_sql_ast_node *expression,
                                   struct mylite_insert_value *out_value);
static int copy_insert_binary_value(const struct mylite_sql_ast_node *expression,
                                    struct mylite_insert_value *out_value);
static int copy_insert_set_assignments(const struct mylite_sql_ast_node *assignments,
                                       struct mylite_insert_set_plan *plan);
static int copy_insert_set_assignment(const struct mylite_sql_ast_node *assignment,
                                      struct mylite_insert_set_plan *plan);
static int add_insert_set_assignment(struct mylite_insert_set_plan *plan,
                                     struct mylite_insert_set_assignment assignment);
static int copy_insert_row_alias(const struct mylite_sql_ast_node *row_alias,
                                 struct mylite_insert_values_plan *plan);
static int add_insert_alias_column(struct mylite_insert_values_plan *plan, char *column_name);
static int copy_insert_duplicate_update_clause(const struct mylite_sql_ast_node *clause,
                                               struct mylite_insert_duplicate_update_plan *plan);
static int copy_insert_update_assignments(const struct mylite_sql_ast_node *assignments,
                                          struct mylite_insert_duplicate_update_plan *plan);
static int copy_insert_update_assignment(const struct mylite_sql_ast_node *assignment,
                                         struct mylite_insert_duplicate_update_plan *plan);
static int add_insert_update_assignment(struct mylite_insert_duplicate_update_plan *plan,
                                        struct mylite_insert_update_assignment assignment);

int mylite_dml_copy_insert_values_statement(const struct mylite_sql_ast_node *statement,
                                            struct mylite_insert_values_plan *values_plan,
                                            struct mylite_insert_duplicate_update_plan *update_plan)
{
    const struct mylite_sql_ast_node *table_name = mylite_ast_child_at(statement, 0U);
    const struct mylite_sql_ast_node *second_child = mylite_ast_child_at(statement, 1U);
    const struct mylite_sql_ast_node *columns = NULL;
    const struct mylite_sql_ast_node *rows = NULL;
    const struct mylite_sql_ast_node *row_alias = NULL;
    const struct mylite_sql_ast_node *duplicate_update = NULL;
    int status = MYLITE_OK;

    if (statement == NULL || values_plan == NULL || update_plan == NULL) {
        return MYLITE_MISUSE;
    }

    status = copy_insert_table_name(table_name, values_plan);
    values_plan->ignore = statement->insert_ignore;
    if (second_child != NULL && second_child->kind == MYLITE_SQL_AST_INSERT_COLUMN_LIST) {
        columns = second_child;
        rows = mylite_ast_child_at(statement, 2U);
    } else {
        rows = second_child;
    }
    row_alias = mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_INSERT_ROW_ALIAS);
    duplicate_update =
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_INSERT_DUPLICATE_UPDATE_CLAUSE);

    if (status == MYLITE_OK) {
        status = copy_insert_column_list(columns, values_plan);
    }
    if (status == MYLITE_OK) {
        status = copy_insert_rows(rows, values_plan);
    }
    if (status == MYLITE_OK) {
        status = copy_insert_row_alias(row_alias, values_plan);
    }
    if (status == MYLITE_OK) {
        status = copy_insert_duplicate_update_clause(duplicate_update, update_plan);
    }
    return status;
}

int mylite_dml_copy_insert_set_statement(const struct mylite_sql_ast_node *statement,
                                         struct mylite_insert_values_plan *values_plan,
                                         struct mylite_insert_set_plan *set_plan,
                                         struct mylite_insert_duplicate_update_plan *update_plan)
{
    const struct mylite_sql_ast_node *table_name = mylite_ast_child_at(statement, 0U);
    const struct mylite_sql_ast_node *assignments = mylite_ast_child_at(statement, 1U);
    const struct mylite_sql_ast_node *row_alias =
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_INSERT_ROW_ALIAS);
    const struct mylite_sql_ast_node *duplicate_update =
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_INSERT_DUPLICATE_UPDATE_CLAUSE);
    int status = MYLITE_OK;

    if (statement == NULL || values_plan == NULL || set_plan == NULL || update_plan == NULL) {
        return MYLITE_MISUSE;
    }

    status = copy_insert_table_name(table_name, values_plan);
    values_plan->ignore = statement->insert_ignore;
    if (status == MYLITE_OK) {
        status = copy_insert_set_assignments(assignments, set_plan);
    }
    if (status == MYLITE_OK) {
        status = copy_insert_row_alias(row_alias, values_plan);
    }
    if (status == MYLITE_OK) {
        status = copy_insert_duplicate_update_clause(duplicate_update, update_plan);
    }
    return status;
}

int mylite_dml_copy_replace_values_statement(const struct mylite_sql_ast_node *statement,
                                             struct mylite_insert_values_plan *values_plan)
{
    const struct mylite_sql_ast_node *table_name = mylite_ast_child_at(statement, 0U);
    const struct mylite_sql_ast_node *second_child = mylite_ast_child_at(statement, 1U);
    const struct mylite_sql_ast_node *columns = NULL;
    const struct mylite_sql_ast_node *rows = NULL;
    int status = MYLITE_OK;

    if (statement == NULL || values_plan == NULL) {
        return MYLITE_MISUSE;
    }

    status = copy_insert_table_name(table_name, values_plan);
    values_plan->replace_low_priority = statement->replace_low_priority;
    values_plan->replace_delayed = statement->replace_delayed;
    if (second_child != NULL && second_child->kind == MYLITE_SQL_AST_INSERT_COLUMN_LIST) {
        columns = second_child;
        rows = mylite_ast_child_at(statement, 2U);
    } else {
        rows = second_child;
    }

    if (status == MYLITE_OK) {
        status = copy_insert_column_list(columns, values_plan);
    }
    if (status == MYLITE_OK) {
        status = copy_insert_rows(rows, values_plan);
    }
    return status;
}

int mylite_dml_copy_replace_set_statement(const struct mylite_sql_ast_node *statement,
                                          struct mylite_insert_values_plan *values_plan,
                                          struct mylite_insert_set_plan *set_plan)
{
    const struct mylite_sql_ast_node *table_name = mylite_ast_child_at(statement, 0U);
    const struct mylite_sql_ast_node *assignments = mylite_ast_child_at(statement, 1U);
    int status = MYLITE_OK;

    if (statement == NULL || values_plan == NULL || set_plan == NULL) {
        return MYLITE_MISUSE;
    }

    status = copy_insert_table_name(table_name, values_plan);
    values_plan->replace_low_priority = statement->replace_low_priority;
    values_plan->replace_delayed = statement->replace_delayed;
    if (status == MYLITE_OK) {
        status = copy_insert_set_assignments(assignments, set_plan);
    }
    return status;
}

static int copy_insert_table_name(const struct mylite_sql_ast_node *table_name,
                                  struct mylite_insert_values_plan *plan)
{
    if (table_name == NULL) {
        return MYLITE_NOMEM;
    }
    if (table_name->kind == MYLITE_SQL_AST_IDENTIFIER) {
        plan->table_name = mylite_copy_identifier_span(table_name);
        return plan->table_name == NULL ? MYLITE_NOMEM : MYLITE_OK;
    }
    if (table_name->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER &&
        mylite_ast_child_at(table_name, 0U) != NULL &&
        mylite_ast_child_at(table_name, 1U) != NULL &&
        mylite_ast_child_at(table_name, 0U)->kind == MYLITE_SQL_AST_IDENTIFIER &&
        mylite_ast_child_at(table_name, 1U)->kind == MYLITE_SQL_AST_IDENTIFIER) {
        plan->schema_name = mylite_copy_identifier_span(mylite_ast_child_at(table_name, 0U));
        plan->table_name = mylite_copy_identifier_span(mylite_ast_child_at(table_name, 1U));
        if (plan->schema_name == NULL || plan->table_name == NULL) {
            return MYLITE_NOMEM;
        }
        return MYLITE_OK;
    }
    return MYLITE_UNSUPPORTED;
}

static int copy_insert_column_list(const struct mylite_sql_ast_node *columns,
                                   struct mylite_insert_values_plan *plan)
{
    if (columns == NULL) {
        plan->has_column_list = false;
        return MYLITE_OK;
    }

    plan->has_column_list = true;
    for (const struct mylite_sql_ast_node *column = columns->first_child; column != NULL;
         column = column->next_sibling) {
        char *column_name = mylite_copy_identifier_span(column);
        int status = MYLITE_OK;

        if (column_name == NULL) {
            return MYLITE_NOMEM;
        }
        status = add_insert_column(plan, column_name);
        if (status != MYLITE_OK) {
            free(column_name);
            return status;
        }
    }
    return MYLITE_OK;
}

static int add_insert_column(struct mylite_insert_values_plan *plan, char *column_name)
{
    char **columns =
        (char **)realloc((void *)plan->columns, (plan->column_count + 1U) * sizeof(*plan->columns));

    if (columns == NULL) {
        return MYLITE_NOMEM;
    }

    plan->columns = columns;
    plan->columns[plan->column_count++] = column_name;
    return MYLITE_OK;
}

static int copy_insert_rows(const struct mylite_sql_ast_node *rows,
                            struct mylite_insert_values_plan *plan)
{
    if (rows == NULL || rows->kind != MYLITE_SQL_AST_INSERT_ROW_LIST) {
        return MYLITE_UNSUPPORTED;
    }

    for (const struct mylite_sql_ast_node *row = rows->first_child; row != NULL;
         row = row->next_sibling) {
        int status = copy_insert_row(row, plan);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return plan->row_count == 0U ? MYLITE_UNSUPPORTED : MYLITE_OK;
}

static int copy_insert_row(const struct mylite_sql_ast_node *row,
                           struct mylite_insert_values_plan *plan)
{
    struct mylite_insert_row insert_row = {0};
    int status = MYLITE_OK;

    if (row == NULL || row->kind != MYLITE_SQL_AST_INSERT_ROW) {
        return MYLITE_UNSUPPORTED;
    }

    for (const struct mylite_sql_ast_node *value = row->first_child; value != NULL;
         value = value->next_sibling) {
        struct mylite_insert_value *values = NULL;
        struct mylite_insert_value insert_value = {0};

        status = copy_insert_value(value, &insert_value);
        if (status != MYLITE_OK) {
            mylite_dml_insert_value_deinit(&insert_value);
            mylite_dml_insert_row_deinit(&insert_row);
            return status;
        }

        values =
            realloc(insert_row.values, (insert_row.value_count + 1U) * sizeof(*insert_row.values));
        if (values == NULL) {
            mylite_dml_insert_value_deinit(&insert_value);
            mylite_dml_insert_row_deinit(&insert_row);
            return MYLITE_NOMEM;
        }
        insert_row.values = values;
        insert_row.values[insert_row.value_count++] = insert_value;
    }

    status = add_insert_row(plan, insert_row);
    if (status != MYLITE_OK) {
        mylite_dml_insert_row_deinit(&insert_row);
    }
    return status;
}

static int add_insert_row(struct mylite_insert_values_plan *plan, struct mylite_insert_row row)
{
    struct mylite_insert_row *rows =
        realloc(plan->rows, (plan->row_count + 1U) * sizeof(*plan->rows));

    if (rows == NULL) {
        return MYLITE_NOMEM;
    }

    plan->rows = rows;
    plan->rows[plan->row_count++] = row;
    return MYLITE_OK;
}

static int copy_insert_value(const struct mylite_sql_ast_node *value_node,
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
        return copy_insert_column_reference(value_node, &out_value->column_reference);
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
    return copy_insert_column_reference(argument, &out_value->column_reference);
}

static int copy_insert_column_reference(const struct mylite_sql_ast_node *identifier,
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

static int copy_insert_set_assignments(const struct mylite_sql_ast_node *assignments,
                                       struct mylite_insert_set_plan *plan)
{
    if (assignments == NULL || assignments->kind != MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT_LIST) {
        return MYLITE_UNSUPPORTED;
    }

    for (const struct mylite_sql_ast_node *assignment = assignments->first_child;
         assignment != NULL; assignment = assignment->next_sibling) {
        int status = copy_insert_set_assignment(assignment, plan);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return plan->assignment_count == 0U ? MYLITE_UNSUPPORTED : MYLITE_OK;
}

static int copy_insert_set_assignment(const struct mylite_sql_ast_node *assignment,
                                      struct mylite_insert_set_plan *plan)
{
    struct mylite_insert_set_assignment insert_assignment = {0};
    int status = MYLITE_OK;

    if (assignment == NULL || assignment->kind != MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT) {
        return MYLITE_UNSUPPORTED;
    }

    status = copy_insert_column_reference(mylite_ast_child_at(assignment, 0U),
                                          &insert_assignment.target);
    if (status == MYLITE_OK) {
        status = copy_insert_value(mylite_ast_child_at(assignment, 1U), &insert_assignment.value);
    }
    if (status == MYLITE_OK) {
        status = add_insert_set_assignment(plan, insert_assignment);
    }
    if (status != MYLITE_OK) {
        mylite_dml_insert_set_assignment_deinit(&insert_assignment);
    }
    return status;
}

static int add_insert_set_assignment(struct mylite_insert_set_plan *plan,
                                     struct mylite_insert_set_assignment assignment)
{
    struct mylite_insert_set_assignment *assignments =
        realloc(plan->assignments, (plan->assignment_count + 1U) * sizeof(*plan->assignments));

    if (assignments == NULL) {
        return MYLITE_NOMEM;
    }

    plan->assignments = assignments;
    plan->assignments[plan->assignment_count++] = assignment;
    return MYLITE_OK;
}

static int copy_insert_row_alias(const struct mylite_sql_ast_node *row_alias,
                                 struct mylite_insert_values_plan *plan)
{
    const struct mylite_sql_ast_node *alias = mylite_ast_child_at(row_alias, 0U);
    const struct mylite_sql_ast_node *columns = mylite_ast_child_at(row_alias, 1U);

    if (row_alias == NULL) {
        return MYLITE_OK;
    }
    if (row_alias->kind != MYLITE_SQL_AST_INSERT_ROW_ALIAS || alias == NULL ||
        alias->kind != MYLITE_SQL_AST_IDENTIFIER) {
        return MYLITE_UNSUPPORTED;
    }

    plan->row_alias = mylite_copy_identifier_span(alias);
    if (plan->row_alias == NULL) {
        return MYLITE_NOMEM;
    }

    if (columns == NULL) {
        return MYLITE_OK;
    }
    if (columns->kind != MYLITE_SQL_AST_INSERT_ALIAS_COLUMN_LIST) {
        return MYLITE_UNSUPPORTED;
    }
    for (const struct mylite_sql_ast_node *column = columns->first_child; column != NULL;
         column = column->next_sibling) {
        char *column_name = mylite_copy_identifier_span(column);
        int status = MYLITE_OK;

        if (column_name == NULL) {
            return MYLITE_NOMEM;
        }
        status = add_insert_alias_column(plan, column_name);
        if (status != MYLITE_OK) {
            free(column_name);
            return status;
        }
    }
    return MYLITE_OK;
}

static int add_insert_alias_column(struct mylite_insert_values_plan *plan, char *column_name)
{
    char **columns =
        (char **)realloc((void *)plan->alias_columns,
                         (plan->alias_column_count + 1U) * sizeof(*plan->alias_columns));

    if (columns == NULL) {
        return MYLITE_NOMEM;
    }

    plan->alias_columns = columns;
    plan->alias_columns[plan->alias_column_count++] = column_name;
    return MYLITE_OK;
}

static int copy_insert_duplicate_update_clause(const struct mylite_sql_ast_node *clause,
                                               struct mylite_insert_duplicate_update_plan *plan)
{
    if (clause == NULL) {
        return MYLITE_OK;
    }
    if (clause->kind != MYLITE_SQL_AST_INSERT_DUPLICATE_UPDATE_CLAUSE) {
        return MYLITE_UNSUPPORTED;
    }

    plan->has_clause = true;
    return copy_insert_update_assignments(mylite_ast_child_at(clause, 0U), plan);
}

static int copy_insert_update_assignments(const struct mylite_sql_ast_node *assignments,
                                          struct mylite_insert_duplicate_update_plan *plan)
{
    if (assignments == NULL || assignments->kind != MYLITE_SQL_AST_INSERT_UPDATE_ASSIGNMENT_LIST) {
        return MYLITE_UNSUPPORTED;
    }

    for (const struct mylite_sql_ast_node *assignment = assignments->first_child;
         assignment != NULL; assignment = assignment->next_sibling) {
        int status = copy_insert_update_assignment(assignment, plan);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return plan->assignment_count == 0U ? MYLITE_UNSUPPORTED : MYLITE_OK;
}

static int copy_insert_update_assignment(const struct mylite_sql_ast_node *assignment,
                                         struct mylite_insert_duplicate_update_plan *plan)
{
    struct mylite_insert_update_assignment insert_assignment = {0};
    int status = MYLITE_OK;

    if (assignment == NULL || assignment->kind != MYLITE_SQL_AST_INSERT_UPDATE_ASSIGNMENT) {
        return MYLITE_UNSUPPORTED;
    }

    status = copy_insert_column_reference(mylite_ast_child_at(assignment, 0U),
                                          &insert_assignment.target);
    if (status == MYLITE_OK) {
        status = copy_insert_value(mylite_ast_child_at(assignment, 1U), &insert_assignment.value);
    }
    if (status == MYLITE_OK) {
        status = add_insert_update_assignment(plan, insert_assignment);
    }
    if (status != MYLITE_OK) {
        mylite_dml_insert_update_assignment_deinit(&insert_assignment);
    }
    return status;
}

static int add_insert_update_assignment(struct mylite_insert_duplicate_update_plan *plan,
                                        struct mylite_insert_update_assignment assignment)
{
    struct mylite_insert_update_assignment *assignments =
        realloc(plan->assignments, (plan->assignment_count + 1U) * sizeof(*plan->assignments));

    if (assignments == NULL) {
        return MYLITE_NOMEM;
    }

    plan->assignments = assignments;
    plan->assignments[plan->assignment_count++] = assignment;
    return MYLITE_OK;
}
