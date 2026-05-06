#include "mylite_select_group_invariant.h"

#include "mylite_select.h"
#include "mylite_select_resolve.h"
#include "mylite_select_subquery.h"
#include "mylite_span.h"
#include "mylite_system_variables.h"
#include "mylite_user_variables.h"

#include <stdlib.h>

static bool select_expression_children_are_group_invariant(
    const struct mylite_select_plan *plan, const struct mylite_sql_ast_node *first_child,
    enum mylite_select_grouping_reference_policy reference_policy);
static bool
select_identifier_is_group_invariant(const struct mylite_select_plan *plan,
                                     const struct mylite_sql_ast_node *identifier,
                                     enum mylite_select_grouping_reference_policy reference_policy);
static bool
select_having_identifier_is_group_invariant(const struct mylite_select_plan *plan,
                                            const struct mylite_sql_ast_node *identifier);
static bool
select_order_identifier_is_group_invariant(const struct mylite_select_plan *plan,
                                           const struct mylite_sql_ast_node *identifier);
static bool
select_column_reference_is_group_invariant(const struct mylite_select_plan *plan,
                                           const struct mylite_sql_ast_node *identifier);
static bool select_resolve_column_reference(const struct mylite_select_plan *plan,
                                            const struct mylite_sql_ast_node *identifier,
                                            size_t *out_column_index);
static bool select_column_index_is_functionally_dependent(const struct mylite_select_plan *plan,
                                                          size_t column_index);
static bool select_table_is_functionally_determined(const struct mylite_select_plan *plan,
                                                    const struct mylite_select_table *table);
static const struct mylite_select_table *
select_table_for_column_index(const struct mylite_select_plan *plan, size_t column_index);
static bool select_output_is_grouped_by_key(const struct mylite_select_plan *plan,
                                            size_t output_index);
static bool select_expression_is_grouped(const struct mylite_select_plan *plan,
                                         const struct mylite_sql_ast_node *expression);
static bool select_group_key_matches_column_output(const struct mylite_select_plan *plan,
                                                   const struct mylite_select_group_key *group_key,
                                                   size_t output_index);

// NOLINTNEXTLINE(misc-no-recursion)
bool mylite_select_expression_contains_aggregate(const struct mylite_sql_ast_node *expression)
{
    if (expression == NULL) {
        return false;
    }
    if (expression->kind == MYLITE_SQL_AST_AGGREGATE_CALL) {
        return true;
    }
    if (expression->kind == MYLITE_SQL_AST_QUANTIFIED_COMPARISON) {
        return mylite_select_expression_contains_aggregate(mylite_ast_child_at(expression, 0U));
    }
    for (const struct mylite_sql_ast_node *child = expression->first_child; child != NULL;
         child = child->next_sibling) {
        if (mylite_select_expression_contains_aggregate(child)) {
            return true;
        }
    }
    return false;
}

// NOLINTNEXTLINE(misc-no-recursion)
bool mylite_select_output_is_group_invariant(const struct mylite_select_plan *plan,
                                             size_t output_index)
{
    const struct mylite_select_output_column *output = NULL;

    if (output_index >= plan->output_count) {
        return false;
    }
    if (select_output_is_grouped_by_key(plan, output_index)) {
        return true;
    }
    output = &plan->outputs[output_index];
    if (output->kind == MYLITE_SELECT_OUTPUT_COLUMN) {
        if (mylite_select_column_index_is_grouped(plan, output->column_index)) {
            return true;
        }
        return select_column_index_is_functionally_dependent(plan, output->column_index);
    }
    return mylite_select_expression_is_group_invariant(plan, output->expression,
                                                       MYLITE_SELECT_GROUPING_REFERENCE_SELECT);
}

bool mylite_select_expression_is_group_invariant( // NOLINT(misc-no-recursion)
    const struct mylite_select_plan *plan, const struct mylite_sql_ast_node *expression,
    enum mylite_select_grouping_reference_policy reference_policy)
{
    if (expression == NULL) {
        return false;
    }
    if (select_expression_is_grouped(plan, expression)) {
        return true;
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_LITERAL:
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP:
    case MYLITE_SQL_AST_AGGREGATE_CALL:
        return true;
    case MYLITE_SQL_AST_IDENTIFIER:
    case MYLITE_SQL_AST_QUALIFIED_IDENTIFIER:
        if (mylite_system_variable_identifier_is_system_variable(expression)) {
            return true;
        }
        if (mylite_user_variable_identifier_is_user_variable(expression)) {
            return true;
        }
        return select_identifier_is_group_invariant(plan, expression, reference_policy);
    case MYLITE_SQL_AST_CAST_EXPRESSION:
        return mylite_select_expression_is_group_invariant(
            plan, mylite_ast_child_at(expression, 0U), reference_policy);
    case MYLITE_SQL_AST_FUNCTION_CALL: {
        const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);

        return select_expression_children_are_group_invariant(
            plan, arguments == NULL ? NULL : arguments->first_child, reference_policy);
    }
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
    case MYLITE_SQL_AST_TERNARY_EXPRESSION:
    case MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION:
    case MYLITE_SQL_AST_EXPRESSION_LIST:
    case MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST:
    case MYLITE_SQL_AST_CASE_EXPRESSION:
    case MYLITE_SQL_AST_CASE_WHEN_LIST:
    case MYLITE_SQL_AST_CASE_WHEN:
        return select_expression_children_are_group_invariant(plan, expression->first_child,
                                                              reference_policy);
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
        if (mylite_select_subquery_binary_expression_is_in(expression)) {
            return mylite_select_expression_is_group_invariant(
                plan, mylite_ast_child_at(expression, 0U), reference_policy);
        }
        return select_expression_children_are_group_invariant(plan, expression->first_child,
                                                              reference_policy);
    case MYLITE_SQL_AST_SUBQUERY_EXPRESSION:
    case MYLITE_SQL_AST_EXISTS_EXPRESSION:
        return true;
    case MYLITE_SQL_AST_QUANTIFIED_COMPARISON:
        return mylite_select_expression_is_group_invariant(
            plan, mylite_ast_child_at(expression, 0U), reference_policy);
    case MYLITE_SQL_AST_CREATE_INDEX_STATEMENT:
    case MYLITE_SQL_AST_DROP_INDEX_STATEMENT:
    case MYLITE_SQL_AST_DDL_TABLE_OPTION_LIST:
    case MYLITE_SQL_AST_DDL_TABLE_OPTION:
    case MYLITE_SQL_AST_ALTER_TABLE_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_ITEM_LIST:
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION:
    case MYLITE_SQL_AST_ALTER_TABLE_COLUMN_POSITION:
    case MYLITE_SQL_AST_RENAME_TABLE_STATEMENT:
    case MYLITE_SQL_AST_TRUNCATE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_SHOW_VARIABLES_STATEMENT:
    case MYLITE_SQL_AST_SHOW_STATUS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_ENGINES_STATEMENT:
    case MYLITE_SQL_AST_SHOW_CHARACTER_SET_STATEMENT:
    case MYLITE_SQL_AST_SHOW_COLLATION_STATEMENT:
    case MYLITE_SQL_AST_SHOW_TABLES_STATEMENT:
    case MYLITE_SQL_AST_SHOW_TABLE_STATUS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_INDEX_STATEMENT:
    case MYLITE_SQL_AST_SHOW_CREATE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_SHOW_CREATE_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_SHOW_DIAGNOSTICS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_DIAGNOSTICS_COUNT_STATEMENT:
    case MYLITE_SQL_AST_DESCRIBE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_RENAME_TABLE_PAIR_LIST:
    case MYLITE_SQL_AST_RENAME_TABLE_PAIR:
    case MYLITE_SQL_AST_PLACEHOLDER_STATEMENT:
    case MYLITE_SQL_AST_ROW_CONSTRUCTOR:
    case MYLITE_SQL_AST_QUERY_EXPRESSION:
    case MYLITE_SQL_AST_UNION_EXPRESSION:
    case MYLITE_SQL_AST_QUERY_PRIMARY:
    case MYLITE_SQL_AST_INSERT_DUPLICATE_UPDATE_CLAUSE:
    case MYLITE_SQL_AST_INSERT_UPDATE_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_INSERT_UPDATE_ASSIGNMENT:
    case MYLITE_SQL_AST_INSERT_ROW_ALIAS:
    case MYLITE_SQL_AST_INSERT_ALIAS_COLUMN_LIST:
        return select_expression_children_are_group_invariant(plan, expression->first_child,
                                                              reference_policy);
    case MYLITE_SQL_AST_GROUP_BY_CLAUSE:
    case MYLITE_SQL_AST_GROUP_ITEM_LIST:
    case MYLITE_SQL_AST_GROUP_ITEM:
    case MYLITE_SQL_AST_HAVING_CLAUSE:
    case MYLITE_SQL_AST_SCRIPT:
    case MYLITE_SQL_AST_SELECT_STATEMENT:
    case MYLITE_SQL_AST_USE_STATEMENT:
    case MYLITE_SQL_AST_SELECT_LIST:
    case MYLITE_SQL_AST_SELECT_ITEM:
    case MYLITE_SQL_AST_FROM_DUAL:
    case MYLITE_SQL_AST_FROM_TABLE:
    case MYLITE_SQL_AST_FROM_TABLE_REFERENCES:
    case MYLITE_SQL_AST_TABLE_REFERENCE_LIST:
    case MYLITE_SQL_AST_JOIN_EXPRESSION:
    case MYLITE_SQL_AST_JOIN_CONDITION:
    case MYLITE_SQL_AST_USING_COLUMN_LIST:
    case MYLITE_SQL_AST_USING_COLUMN:
    case MYLITE_SQL_AST_WILDCARD:
    case MYLITE_SQL_AST_CREATE_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_ALTER_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_DROP_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_SHOW_SCHEMAS_STATEMENT:
    case MYLITE_SQL_AST_IF_EXISTS:
    case MYLITE_SQL_AST_IF_NOT_EXISTS:
    case MYLITE_SQL_AST_SCHEMA_OPTION_LIST:
    case MYLITE_SQL_AST_SCHEMA_OPTION:
    case MYLITE_SQL_AST_SET_NAMES_STATEMENT:
    case MYLITE_SQL_AST_SET_CHARACTER_SET_STATEMENT:
    case MYLITE_SQL_AST_SET_SYSTEM_VARIABLE_STATEMENT:
    case MYLITE_SQL_AST_SET_USER_VARIABLE_STATEMENT:
    case MYLITE_SQL_AST_USER_VARIABLE_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_USER_VARIABLE_ASSIGNMENT:
    case MYLITE_SQL_AST_PREPARE_STATEMENT:
    case MYLITE_SQL_AST_EXECUTE_STATEMENT:
    case MYLITE_SQL_AST_EXECUTE_USING_LIST:
    case MYLITE_SQL_AST_DEALLOCATE_PREPARE_STATEMENT:
    case MYLITE_SQL_AST_DEFAULT:
    case MYLITE_SQL_AST_CREATE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_COLUMN_DEFINITION_LIST:
    case MYLITE_SQL_AST_COLUMN_DEFINITION:
    case MYLITE_SQL_AST_COLUMN_TYPE:
    case MYLITE_SQL_AST_COLUMN_TYPE_ATTRIBUTE_LIST:
    case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_LIST:
    case MYLITE_SQL_AST_COLUMN_ATTRIBUTE:
    case MYLITE_SQL_AST_PRIMARY_KEY_CONSTRAINT:
    case MYLITE_SQL_AST_KEY_PART_LIST:
    case MYLITE_SQL_AST_KEY_PART:
    case MYLITE_SQL_AST_INDEX_TYPE:
    case MYLITE_SQL_AST_INDEX_OPTION_LIST:
    case MYLITE_SQL_AST_INDEX_OPTION:
    case MYLITE_SQL_AST_SECONDARY_INDEX:
    case MYLITE_SQL_AST_UNIQUE_INDEX:
    case MYLITE_SQL_AST_TABLE_OPTION_LIST:
    case MYLITE_SQL_AST_TABLE_OPTION:
    case MYLITE_SQL_AST_DROP_TABLE_STATEMENT:
    case MYLITE_SQL_AST_TABLE_NAME_LIST:
    case MYLITE_SQL_AST_INSERT_VALUES_STATEMENT:
    case MYLITE_SQL_AST_INSERT_COLUMN_LIST:
    case MYLITE_SQL_AST_INSERT_ROW_LIST:
    case MYLITE_SQL_AST_INSERT_ROW:
    case MYLITE_SQL_AST_INSERT_VALUE_LIST:
    case MYLITE_SQL_AST_INSERT_SET_STATEMENT:
    case MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT:
    case MYLITE_SQL_AST_REPLACE_VALUES_STATEMENT:
    case MYLITE_SQL_AST_REPLACE_SET_STATEMENT:
    case MYLITE_SQL_AST_WHERE_CLAUSE:
    case MYLITE_SQL_AST_ORDER_BY_CLAUSE:
    case MYLITE_SQL_AST_ORDER_ITEM_LIST:
    case MYLITE_SQL_AST_ORDER_ITEM:
    case MYLITE_SQL_AST_LIMIT_CLAUSE:
    case MYLITE_SQL_AST_LIMIT_BOUND:
    case MYLITE_SQL_AST_UPDATE_STATEMENT:
    case MYLITE_SQL_AST_UPDATE_TARGET:
    case MYLITE_SQL_AST_UPDATE_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_UPDATE_ASSIGNMENT:
    case MYLITE_SQL_AST_UPDATE_LIMIT_CLAUSE:
    case MYLITE_SQL_AST_DELETE_STATEMENT:
    case MYLITE_SQL_AST_DELETE_TARGET:
    case MYLITE_SQL_AST_DELETE_LIMIT_CLAUSE:
    case MYLITE_SQL_AST_DELETE_TARGET_LIST:
    case MYLITE_SQL_AST_DELETE_TARGET_NAME:
    case MYLITE_SQL_AST_START_TRANSACTION_STATEMENT:
    case MYLITE_SQL_AST_BEGIN_TRANSACTION_STATEMENT:
    case MYLITE_SQL_AST_TRANSACTION_CHARACTERISTIC_LIST:
    case MYLITE_SQL_AST_TRANSACTION_CHARACTERISTIC:
    case MYLITE_SQL_AST_COMMIT_STATEMENT:
    case MYLITE_SQL_AST_ROLLBACK_STATEMENT:
    case MYLITE_SQL_AST_TRANSACTION_COMPLETION:
    case MYLITE_SQL_AST_SAVEPOINT_STATEMENT:
    case MYLITE_SQL_AST_ROLLBACK_TO_SAVEPOINT_STATEMENT:
    case MYLITE_SQL_AST_RELEASE_SAVEPOINT_STATEMENT:
        return false;
    }

    return false;
}

// NOLINTNEXTLINE(misc-no-recursion)
static bool select_expression_children_are_group_invariant(
    const struct mylite_select_plan *plan, const struct mylite_sql_ast_node *first_child,
    enum mylite_select_grouping_reference_policy reference_policy)
{
    for (const struct mylite_sql_ast_node *child = first_child; child != NULL;
         child = child->next_sibling) {
        if (!mylite_select_expression_is_group_invariant(plan, child, reference_policy)) {
            return false;
        }
    }
    return true;
}

static bool select_identifier_is_group_invariant( // NOLINT(misc-no-recursion)
    const struct mylite_select_plan *plan, const struct mylite_sql_ast_node *identifier,
    enum mylite_select_grouping_reference_policy reference_policy)
{
    switch (reference_policy) {
    case MYLITE_SELECT_GROUPING_REFERENCE_SELECT:
        return select_column_reference_is_group_invariant(plan, identifier);
    case MYLITE_SELECT_GROUPING_REFERENCE_HAVING:
        return select_having_identifier_is_group_invariant(plan, identifier);
    case MYLITE_SELECT_GROUPING_REFERENCE_ORDER:
        return select_order_identifier_is_group_invariant(plan, identifier);
    }
    return false;
}

static bool select_having_identifier_is_group_invariant( // NOLINT(misc-no-recursion)
    const struct mylite_select_plan *plan, const struct mylite_sql_ast_node *identifier)
{
    size_t output_index = 0U;
    size_t output_matches = 0U;

    if (select_column_reference_is_group_invariant(plan, identifier)) {
        return true;
    }
    if (identifier == NULL || identifier->kind != MYLITE_SQL_AST_IDENTIFIER) {
        return false;
    }

    output_matches = mylite_select_output_label_span_count(plan, identifier->span, &output_index);
    if (output_matches != 1U) {
        return false;
    }
    return mylite_select_output_is_group_invariant(plan, output_index);
}

// NOLINTNEXTLINE(misc-no-recursion)
static bool select_order_identifier_is_group_invariant(const struct mylite_select_plan *plan,
                                                       const struct mylite_sql_ast_node *identifier)
{
    size_t output_index = 0U;
    size_t output_matches = 0U;

    if (identifier != NULL && identifier->kind == MYLITE_SQL_AST_IDENTIFIER) {
        output_matches =
            mylite_select_output_label_span_count(plan, identifier->span, &output_index);
        if (output_matches == 1U) {
            return mylite_select_output_is_group_invariant(plan, output_index);
        }
        if (output_matches > 1U) {
            return false;
        }
    }
    return select_column_reference_is_group_invariant(plan, identifier);
}

static bool select_column_reference_is_group_invariant(const struct mylite_select_plan *plan,
                                                       const struct mylite_sql_ast_node *identifier)
{
    size_t column_index = mylite_select_plan_column_count(plan);

    if (!select_resolve_column_reference(plan, identifier, &column_index)) {
        return false;
    }
    if (mylite_select_column_index_is_grouped(plan, column_index)) {
        return true;
    }
    return select_column_index_is_functionally_dependent(plan, column_index);
}

static bool select_resolve_column_reference(const struct mylite_select_plan *plan,
                                            const struct mylite_sql_ast_node *identifier,
                                            size_t *out_column_index)
{
    char *parts[3] = {0};
    size_t part_count = 0U;
    size_t match_count = 0U;
    bool resolved = false;

    *out_column_index = mylite_select_plan_column_count(plan);
    if (identifier == NULL || (identifier->kind != MYLITE_SQL_AST_IDENTIFIER &&
                               identifier->kind != MYLITE_SQL_AST_QUALIFIED_IDENTIFIER)) {
        return false;
    }
    if (mylite_copy_identifier_parts(identifier, parts, &part_count) != MYLITE_OK) {
        return false;
    }

    match_count = mylite_select_count_plan_column_parts_matches(
        plan, parts, part_count, 0U, mylite_select_plan_table_count(plan), out_column_index);
    resolved = match_count == 1U;
    for (size_t index = 0U; index < part_count && index < 3U; ++index) {
        free(parts[index]);
    }
    return resolved;
}

static bool select_column_index_is_functionally_dependent(const struct mylite_select_plan *plan,
                                                          size_t column_index)
{
    const struct mylite_select_table *table = select_table_for_column_index(plan, column_index);

    return select_table_is_functionally_determined(plan, table);
}

static bool select_table_is_functionally_determined(const struct mylite_select_plan *plan,
                                                    const struct mylite_select_table *table)
{
    if (table == NULL) {
        return false;
    }
    for (size_t key_index = 0U; key_index < table->unique_not_null_key_count; ++key_index) {
        const struct mylite_select_column_sequence *key = &table->unique_not_null_keys[key_index];
        bool all_key_columns_grouped = key->column_count != 0U;

        for (size_t part = 0U; all_key_columns_grouped && part < key->column_count; ++part) {
            all_key_columns_grouped =
                mylite_select_column_index_is_grouped(plan, key->column_indexes[part]);
        }
        if (all_key_columns_grouped) {
            return true;
        }
    }
    return false;
}

static const struct mylite_select_table *
select_table_for_column_index(const struct mylite_select_plan *plan, size_t column_index)
{
    size_t table_count = mylite_select_plan_table_count(plan);

    for (size_t table_index = 0U; table_index < table_count; ++table_index) {
        const struct mylite_select_table *table = mylite_select_plan_table_const(plan, table_index);

        if (table != NULL && column_index >= table->first_column_index &&
            column_index < table->first_column_index + table->column_count) {
            return table;
        }
    }
    return NULL;
}

static bool select_output_is_grouped_by_key(const struct mylite_select_plan *plan,
                                            size_t output_index)
{
    if (output_index >= plan->output_count) {
        return false;
    }
    for (size_t index = 0U; index < plan->group_key_count; ++index) {
        const struct mylite_select_group_key *group_key = &plan->group_keys[index];

        if (group_key->kind == MYLITE_SELECT_GROUP_KEY_OUTPUT &&
            group_key->output_index == output_index) {
            return true;
        }
        if (select_group_key_matches_column_output(plan, group_key, output_index)) {
            return true;
        }
    }
    return false;
}

static bool select_expression_is_grouped(const struct mylite_select_plan *plan,
                                         const struct mylite_sql_ast_node *expression)
{
    for (size_t index = 0U; index < plan->group_key_count; ++index) {
        const struct mylite_select_group_key *group_key = &plan->group_keys[index];

        if (group_key->kind == MYLITE_SELECT_GROUP_KEY_EXPRESSION &&
            group_key->expression != NULL &&
            mylite_source_span_equal_ci(group_key->expression->span, expression->span)) {
            return true;
        }
    }
    return false;
}

static bool select_group_key_matches_column_output(const struct mylite_select_plan *plan,
                                                   const struct mylite_select_group_key *group_key,
                                                   size_t output_index)
{
    size_t group_column_index = mylite_select_plan_column_count(plan);
    const struct mylite_select_output_column *output = &plan->outputs[output_index];

    if (output->kind != MYLITE_SELECT_OUTPUT_COLUMN ||
        group_key->kind != MYLITE_SELECT_GROUP_KEY_EXPRESSION || group_key->expression == NULL) {
        return false;
    }
    if (group_key->expression->kind != MYLITE_SQL_AST_IDENTIFIER &&
        group_key->expression->kind != MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        return false;
    }
    if (!select_resolve_column_reference(plan, group_key->expression, &group_column_index)) {
        return false;
    }
    return group_column_index == output->column_index;
}
