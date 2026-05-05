#include "mylite_select_distinct_validate.h"

#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "mylite_select.h"
#include "mylite_select_order_resolve.h"
#include "mylite_select_resolve.h"
#include "mylite_span.h"
#include "sqlite3.h"

#include <stdlib.h>

static int validate_select_distinct_order_key(mylite_db *database,
                                              const struct mylite_select_plan *plan,
                                              const struct mylite_select_order_key *order_key,
                                              size_t order_position);
static int validate_select_distinct_order_expression(mylite_db *database,
                                                     const struct mylite_select_plan *plan,
                                                     const struct mylite_sql_ast_node *expression,
                                                     size_t order_position);
static int validate_select_distinct_order_expression_node(
    mylite_db *database, const struct mylite_select_plan *plan,
    struct mylite_select_distinct_order_validation_stack *stack,
    const struct mylite_sql_ast_node *expression, size_t order_position, bool alias_first);
static int push_select_distinct_order_expression_child(
    mylite_db *database, struct mylite_select_distinct_order_validation_stack *stack,
    const struct mylite_sql_ast_node *expression, bool alias_first);
static int push_select_distinct_order_expression_children(
    mylite_db *database, struct mylite_select_distinct_order_validation_stack *stack,
    const struct mylite_sql_ast_node *expression, bool alias_first);
static bool
pop_select_distinct_order_expression(struct mylite_select_distinct_order_validation_stack *stack,
                                     const struct mylite_sql_ast_node **out_expression,
                                     bool *out_alias_first);
static void select_distinct_order_validation_stack_deinit(
    struct mylite_select_distinct_order_validation_stack *stack);
static int validate_select_distinct_order_identifier(mylite_db *database,
                                                     const struct mylite_select_plan *plan,
                                                     const struct mylite_sql_ast_node *identifier,
                                                     size_t order_position, bool alias_first);
static int validate_select_distinct_order_identifier_column_first(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *identifier, size_t order_position, bool *out_resolved);
static bool
select_distinct_order_expression_matches_output(const struct mylite_select_plan *plan,
                                                const struct mylite_sql_ast_node *expression);
static bool select_distinct_column_index_is_output(const struct mylite_select_plan *plan,
                                                   size_t column_index);
static int set_select_distinct_order_column_error(
    mylite_db *database, const struct mylite_select_plan *plan,
    struct mylite_select_distinct_order_column_error_context context);

int mylite_select_validate_distinct_order(mylite_db *database,
                                          const struct mylite_select_plan *plan)
{
    if (!mylite_select_duplicate_mode_is_distinct(plan->duplicate_mode)) {
        return MYLITE_OK;
    }
    for (size_t index = 0U; index < plan->order_key_count; ++index) {
        int status = validate_select_distinct_order_key(database, plan, &plan->order_keys[index],
                                                        index + 1U);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int validate_select_distinct_order_key(mylite_db *database,
                                              const struct mylite_select_plan *plan,
                                              const struct mylite_select_order_key *order_key,
                                              size_t order_position)
{
    if (order_key->kind == MYLITE_SELECT_ORDER_KEY_OUTPUT) {
        return MYLITE_OK;
    }
    return validate_select_distinct_order_expression(database, plan, order_key->expression,
                                                     order_position);
}

static int validate_select_distinct_order_expression(mylite_db *database,
                                                     const struct mylite_select_plan *plan,
                                                     const struct mylite_sql_ast_node *expression,
                                                     size_t order_position)
{
    struct mylite_select_distinct_order_validation_stack stack = {0};
    const struct mylite_sql_ast_node *current = NULL;
    bool alias_first = false;
    int status = push_select_distinct_order_expression_child(database, &stack, expression, true);

    while (status == MYLITE_OK &&
           pop_select_distinct_order_expression(&stack, &current, &alias_first)) {
        status = validate_select_distinct_order_expression_node(database, plan, &stack, current,
                                                                order_position, alias_first);
    }

    select_distinct_order_validation_stack_deinit(&stack);
    return status;
}

static int validate_select_distinct_order_expression_node(
    mylite_db *database, const struct mylite_select_plan *plan,
    struct mylite_select_distinct_order_validation_stack *stack,
    const struct mylite_sql_ast_node *expression, size_t order_position, bool alias_first)
{
    if (expression == NULL || select_distinct_order_expression_matches_output(plan, expression)) {
        return MYLITE_OK;
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_LITERAL:
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP:
        return MYLITE_OK;
    case MYLITE_SQL_AST_IDENTIFIER:
    case MYLITE_SQL_AST_QUALIFIED_IDENTIFIER:
        return validate_select_distinct_order_identifier(database, plan, expression, order_position,
                                                         alias_first);
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_QUANTIFIED_COMPARISON:
    case MYLITE_SQL_AST_TERNARY_EXPRESSION:
    case MYLITE_SQL_AST_EXPRESSION_LIST:
    case MYLITE_SQL_AST_ROW_CONSTRUCTOR:
    case MYLITE_SQL_AST_CASE_EXPRESSION:
    case MYLITE_SQL_AST_CASE_WHEN_LIST:
    case MYLITE_SQL_AST_CASE_WHEN:
        return push_select_distinct_order_expression_children(database, stack, expression, false);
    case MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION:
        return push_select_distinct_order_expression_child(
            database, stack, mylite_ast_child_at(expression, 0U), alias_first);
    case MYLITE_SQL_AST_CAST_EXPRESSION:
        return push_select_distinct_order_expression_child(
            database, stack, mylite_ast_child_at(expression, 0U), false);
    case MYLITE_SQL_AST_FUNCTION_CALL:
        return push_select_distinct_order_expression_children(
            database, stack, mylite_ast_child_at(expression, 1U), false);
    case MYLITE_SQL_AST_AGGREGATE_CALL:
        if (expression->aggregate_argument == MYLITE_SQL_AST_AGGREGATE_ARGUMENT_EXPRESSION) {
            return push_select_distinct_order_expression_child(
                database, stack, mylite_ast_child_at(expression, 1U), false);
        }
        return MYLITE_OK;
    default:
        return MYLITE_OK;
    }
}

static int push_select_distinct_order_expression_child(
    mylite_db *database, struct mylite_select_distinct_order_validation_stack *stack,
    const struct mylite_sql_ast_node *expression, bool alias_first)
{
    const size_t next_count = stack->count + 1U;
    struct mylite_select_distinct_order_validation_frame *frames = NULL;

    if (expression == NULL) {
        return MYLITE_OK;
    }
    if (next_count <= stack->capacity) {
        stack->frames[stack->count++] = (struct mylite_select_distinct_order_validation_frame){
            .expression = expression,
            .alias_first = alias_first,
        };
        return MYLITE_OK;
    }

    frames = realloc(stack->frames, next_count * sizeof(*stack->frames));
    if (frames == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    stack->frames = frames;
    stack->capacity = next_count;
    stack->frames[stack->count++] = (struct mylite_select_distinct_order_validation_frame){
        .expression = expression,
        .alias_first = alias_first,
    };
    return MYLITE_OK;
}

static int push_select_distinct_order_expression_children(
    mylite_db *database, struct mylite_select_distinct_order_validation_stack *stack,
    const struct mylite_sql_ast_node *expression, bool alias_first)
{
    struct mylite_select_distinct_order_validation_frame *children = NULL;
    const struct mylite_sql_ast_node *child = expression == NULL ? NULL : expression->first_child;
    size_t child_count = 0U;
    int status = MYLITE_OK;

    for (; child != NULL; child = child->next_sibling) {
        ++child_count;
    }
    if (child_count == 0U) {
        return MYLITE_OK;
    }

    children = calloc(child_count, sizeof(*children));
    if (children == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    child = expression->first_child;
    for (size_t index = 0U; index < child_count; ++index) {
        children[index].expression = child;
        child = child->next_sibling;
    }
    for (size_t index = child_count; status == MYLITE_OK && index > 0U; --index) {
        status = push_select_distinct_order_expression_child(
            database, stack, children[index - 1U].expression, alias_first);
    }
    free(children);
    return status;
}

static bool
pop_select_distinct_order_expression(struct mylite_select_distinct_order_validation_stack *stack,
                                     const struct mylite_sql_ast_node **out_expression,
                                     bool *out_alias_first)
{
    if (stack->count == 0U) {
        *out_expression = NULL;
        *out_alias_first = false;
        return false;
    }
    --stack->count;
    *out_expression = stack->frames[stack->count].expression;
    *out_alias_first = stack->frames[stack->count].alias_first;
    return true;
}

static void select_distinct_order_validation_stack_deinit(
    struct mylite_select_distinct_order_validation_stack *stack)
{
    free(stack->frames);
    *stack = (struct mylite_select_distinct_order_validation_stack){0};
}

static int validate_select_distinct_order_identifier(mylite_db *database,
                                                     const struct mylite_select_plan *plan,
                                                     const struct mylite_sql_ast_node *identifier,
                                                     size_t order_position, bool alias_first)
{
    enum mylite_select_order_key_kind kind = MYLITE_SELECT_ORDER_KEY_EXPRESSION;
    size_t index = 0U;
    bool resolved = false;
    int status = MYLITE_OK;

    if (!alias_first) {
        status = validate_select_distinct_order_identifier_column_first(database, plan, identifier,
                                                                        order_position, &resolved);
    }

    if (status != MYLITE_OK || resolved) {
        return status;
    }

    status = mylite_select_resolve_order_reference(database, plan, identifier, &kind, &index);

    if (status != MYLITE_OK) {
        return status;
    }
    if (kind == MYLITE_SELECT_ORDER_KEY_OUTPUT ||
        select_distinct_column_index_is_output(plan, index)) {
        return MYLITE_OK;
    }
    return set_select_distinct_order_column_error(
        database, plan,
        (struct mylite_select_distinct_order_column_error_context){
            .order_position = order_position,
            .column_index = index,
        });
}

static int validate_select_distinct_order_identifier_column_first(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *identifier, size_t order_position, bool *out_resolved)
{
    char *parts[3] = {0};
    size_t part_count = 0U;
    size_t column_index = mylite_select_plan_column_count(plan);
    int status = mylite_copy_identifier_parts(identifier, parts, &part_count);

    *out_resolved = false;
    if (status != MYLITE_OK) {
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
        }
        return status;
    }
    if (part_count == 1U) {
        size_t column_matches = mylite_select_count_plan_column_parts_matches(
            plan, parts, part_count, 0U, mylite_select_plan_table_count(plan), &column_index);

        if (column_matches > 1U) {
            status = mylite_select_set_ambiguous_order_column_error(database, parts[0]);
            *out_resolved = true;
            goto cleanup;
        }
        if (column_matches == 1U) {
            *out_resolved = true;
            if (select_distinct_column_index_is_output(plan, column_index)) {
                goto cleanup;
            }
            status = set_select_distinct_order_column_error(
                database, plan,
                (struct mylite_select_distinct_order_column_error_context){
                    .order_position = order_position,
                    .column_index = column_index,
                });
        }
    }

cleanup:
    for (size_t index = 0U; index < part_count && index < 3U; ++index) {
        free(parts[index]);
    }
    return status;
}

static bool
select_distinct_order_expression_matches_output(const struct mylite_select_plan *plan,
                                                const struct mylite_sql_ast_node *expression)
{
    const struct mylite_sql_ast_node *unwrapped =
        mylite_sql_ast_unwrap_parenthesized_expression(expression);

    if (unwrapped == NULL) {
        return false;
    }
    for (size_t index = 0U; index < plan->output_count; ++index) {
        const struct mylite_select_output_column *output = &plan->outputs[index];
        const struct mylite_sql_ast_node *output_expression =
            mylite_sql_ast_unwrap_parenthesized_expression(output->expression);

        if (output->kind == MYLITE_SELECT_OUTPUT_EXPRESSION && output_expression != NULL &&
            mylite_source_span_equal_ci(output_expression->span, unwrapped->span)) {
            return true;
        }
    }
    return false;
}

static bool select_distinct_column_index_is_output(const struct mylite_select_plan *plan,
                                                   size_t column_index)
{
    for (size_t index = 0U; index < plan->output_count; ++index) {
        const struct mylite_select_output_column *output = &plan->outputs[index];

        if (output->kind == MYLITE_SELECT_OUTPUT_COLUMN && output->column_index == column_index) {
            return true;
        }
    }
    return false;
}

static int set_select_distinct_order_column_error(
    mylite_db *database, const struct mylite_select_plan *plan,
    struct mylite_select_distinct_order_column_error_context context)
{
    const struct mylite_select_table *table = NULL;
    const struct mylite_select_column *column =
        mylite_select_plan_column_const(plan, context.column_index, &table);
    const char *schema_name = table == NULL || table->schema_name == NULL ? "" : table->schema_name;
    const char *table_name = table == NULL || table->table_name == NULL ? "" : table->table_name;
    const char *column_name = column == NULL || column->name == NULL ? "" : column->name;
    char *reference = sqlite3_mprintf("%q.%q.%q", schema_name, table_name, column_name);
    char *message = NULL;
    int status = MYLITE_OK;

    if (reference == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    message = sqlite3_mprintf(
        "Expression #%llu of ORDER BY clause is not in SELECT list, references column '%q' "
        "which is not in SELECT list; this is incompatible with DISTINCT",
        (unsigned long long)context.order_position, reference);
    sqlite3_free(reference);
    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_set_error_message(database, message);
    if (status == MYLITE_OK) {
        status = mylite_diagnostics_append_error(
            database, MYLITE_MYSQL_ER_FIELD_IN_ORDER_NOT_SELECT, message);
    }
    sqlite3_free(message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}
