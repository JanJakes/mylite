#include "mylite_select_group_validate.h"

#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "mylite_select.h"
#include "mylite_select_resolve.h"
#include "mylite_select_subquery.h"
#include "mylite_span.h"

#include <stdlib.h>

static int validate_select_grouping_clause_expression(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    enum mylite_select_grouping_reference_policy reference_policy);
static bool select_expression_contains_aggregate(const struct mylite_sql_ast_node *expression);
static bool select_output_is_group_invariant(const struct mylite_select_plan *plan,
                                             size_t output_index);
static bool
select_expression_is_group_invariant(const struct mylite_select_plan *plan,
                                     const struct mylite_sql_ast_node *expression,
                                     enum mylite_select_grouping_reference_policy reference_policy);
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
static bool select_column_reference_is_grouped(const struct mylite_select_plan *plan,
                                               const struct mylite_sql_ast_node *identifier);
static bool select_output_is_grouped_by_key(const struct mylite_select_plan *plan,
                                            size_t output_index);
static bool select_expression_is_grouped(const struct mylite_select_plan *plan,
                                         const struct mylite_sql_ast_node *expression);
static bool select_group_key_matches_column_output(const struct mylite_select_plan *plan,
                                                   const struct mylite_select_group_key *group_key,
                                                   size_t output_index);
static int set_select_only_full_group_by_error(mylite_db *database, const char *expression_text,
                                               bool implicit_group);

int mylite_select_validate_grouping(mylite_db *database, const struct mylite_select_plan *plan)
{
    bool aggregate_query = (plan->has_aggregate || plan->has_group_by || plan->has_having) != 0;
    bool implicit_group = true;

    if (plan->has_group_by) {
        implicit_group = false;
    }

    if (!aggregate_query) {
        return MYLITE_OK;
    }

    for (size_t index = 0U; index < plan->output_count; ++index) {
        const struct mylite_select_output_column *output = &plan->outputs[index];

        if (!select_output_is_group_invariant(plan, index)) {
            return set_select_only_full_group_by_error(database, output->label, implicit_group);
        }
    }
    if (plan->having_expression != NULL) {
        int status = validate_select_grouping_clause_expression(
            database, plan, plan->having_expression, MYLITE_SELECT_GROUPING_REFERENCE_HAVING);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    for (size_t index = 0U; index < plan->order_key_count; ++index) {
        const struct mylite_select_order_key *order_key = &plan->order_keys[index];

        if (order_key->kind == MYLITE_SELECT_ORDER_KEY_OUTPUT) {
            if (!select_output_is_group_invariant(plan, order_key->output_index)) {
                const char *label = order_key->output_index < plan->output_count
                                        ? plan->outputs[order_key->output_index].label
                                        : "";

                return set_select_only_full_group_by_error(database, label, implicit_group);
            }
            continue;
        }

        {
            int status = validate_select_grouping_clause_expression(
                database, plan, order_key->expression, MYLITE_SELECT_GROUPING_REFERENCE_ORDER);

            if (status != MYLITE_OK) {
                return status;
            }
        }
    }
    return MYLITE_OK;
}

bool mylite_select_output_contains_aggregate(const struct mylite_select_plan *plan,
                                             size_t output_index)
{
    if (output_index >= plan->output_count) {
        return false;
    }
    if (plan->outputs[output_index].kind != MYLITE_SELECT_OUTPUT_EXPRESSION) {
        return false;
    }
    return select_expression_contains_aggregate(plan->outputs[output_index].expression);
}

static int validate_select_grouping_clause_expression(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    enum mylite_select_grouping_reference_policy reference_policy)
{
    char *expression_text = NULL;
    bool implicit_group = true;
    int status = MYLITE_OK;

    if (plan->has_group_by) {
        implicit_group = false;
    }

    if (select_expression_is_group_invariant(plan, expression, reference_policy)) {
        return MYLITE_OK;
    }

    expression_text = expression == NULL
                          ? NULL
                          : mylite_copy_span_text(expression->span.text, expression->span.length);
    if (expression != NULL && expression_text == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = set_select_only_full_group_by_error(database, expression_text, implicit_group);
    free(expression_text);
    return status;
}

// NOLINTNEXTLINE(misc-no-recursion)
static bool select_expression_contains_aggregate(const struct mylite_sql_ast_node *expression)
{
    if (expression == NULL) {
        return false;
    }
    if (expression->kind == MYLITE_SQL_AST_AGGREGATE_CALL) {
        return true;
    }
    if (expression->kind == MYLITE_SQL_AST_QUANTIFIED_COMPARISON) {
        return select_expression_contains_aggregate(mylite_ast_child_at(expression, 0U));
    }
    for (const struct mylite_sql_ast_node *child = expression->first_child; child != NULL;
         child = child->next_sibling) {
        if (select_expression_contains_aggregate(child)) {
            return true;
        }
    }
    return false;
}

// NOLINTNEXTLINE(misc-no-recursion)
static bool select_output_is_group_invariant(const struct mylite_select_plan *plan,
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
        return mylite_select_column_index_is_grouped(plan, output->column_index);
    }
    return select_expression_is_group_invariant(plan, output->expression,
                                                MYLITE_SELECT_GROUPING_REFERENCE_SELECT);
}

static bool select_expression_is_group_invariant( // NOLINT(misc-no-recursion)
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
        return select_identifier_is_group_invariant(plan, expression, reference_policy);
    case MYLITE_SQL_AST_CAST_EXPRESSION:
        return select_expression_is_group_invariant(plan, mylite_ast_child_at(expression, 0U),
                                                    reference_policy);
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
            return select_expression_is_group_invariant(plan, mylite_ast_child_at(expression, 0U),
                                                        reference_policy);
        }
        return select_expression_children_are_group_invariant(plan, expression->first_child,
                                                              reference_policy);
    case MYLITE_SQL_AST_SUBQUERY_EXPRESSION:
    case MYLITE_SQL_AST_EXISTS_EXPRESSION:
        return true;
    case MYLITE_SQL_AST_QUANTIFIED_COMPARISON:
        return select_expression_is_group_invariant(plan, mylite_ast_child_at(expression, 0U),
                                                    reference_policy);
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
        if (!select_expression_is_group_invariant(plan, child, reference_policy)) {
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
        return select_column_reference_is_grouped(plan, identifier);
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

    if (select_column_reference_is_grouped(plan, identifier)) {
        return true;
    }
    if (identifier == NULL || identifier->kind != MYLITE_SQL_AST_IDENTIFIER) {
        return false;
    }

    output_matches = mylite_select_output_label_span_count(plan, identifier->span, &output_index);
    if (output_matches != 1U) {
        return false;
    }
    return select_output_is_group_invariant(plan, output_index);
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
            return select_output_is_group_invariant(plan, output_index);
        }
        if (output_matches > 1U) {
            return false;
        }
    }
    return select_column_reference_is_grouped(plan, identifier);
}

static bool select_column_reference_is_grouped(const struct mylite_select_plan *plan,
                                               const struct mylite_sql_ast_node *identifier)
{
    size_t column_index = plan->table.column_count;

    if (identifier == NULL || (identifier->kind != MYLITE_SQL_AST_IDENTIFIER &&
                               identifier->kind != MYLITE_SQL_AST_QUALIFIED_IDENTIFIER)) {
        return false;
    }
    if (mylite_select_resolve_column_reference(&plan->table, identifier, &column_index) !=
            MYLITE_OK ||
        column_index == plan->table.column_count) {
        return false;
    }
    return mylite_select_column_index_is_grouped(plan, column_index);
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
    size_t group_column_index = plan->table.column_count;
    const struct mylite_select_output_column *output = &plan->outputs[output_index];

    if (output->kind != MYLITE_SELECT_OUTPUT_COLUMN ||
        group_key->kind != MYLITE_SELECT_GROUP_KEY_EXPRESSION || group_key->expression == NULL) {
        return false;
    }
    if (group_key->expression->kind != MYLITE_SQL_AST_IDENTIFIER &&
        group_key->expression->kind != MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        return false;
    }
    if (mylite_select_resolve_column_reference(&plan->table, group_key->expression,
                                               &group_column_index) != MYLITE_OK) {
        return false;
    }
    return group_column_index == output->column_index;
}

static int set_select_only_full_group_by_error(mylite_db *database, const char *expression_text,
                                               bool implicit_group)
{
    int status = MYLITE_OK;

    if (implicit_group) {
        status = mylite_diagnostics_set_error_message_parts(
            database,
            "In aggregated query without GROUP BY, expression contains nonaggregated "
            "column '",
            expression_text == NULL ? "" : expression_text,
            "'; this is incompatible with sql_mode=only_full_group_by");
        if (status == MYLITE_NOMEM) {
            return MYLITE_NOMEM;
        }
        status = mylite_diagnostics_append_error(
            database, MYLITE_MYSQL_ER_MIX_OF_GROUP_FUNC_AND_FIELDS, mylite_error_message(database));
    } else {
        status = mylite_diagnostics_set_error_message_parts(
            database, "Expression contains nonaggregated column '",
            expression_text == NULL ? "" : expression_text,
            "' which is not functionally dependent on GROUP BY; this is incompatible with "
            "sql_mode=only_full_group_by");
        if (status == MYLITE_NOMEM) {
            return MYLITE_NOMEM;
        }
        status = mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_WRONG_FIELD_WITH_GROUP,
                                                 mylite_error_message(database));
    }
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}
