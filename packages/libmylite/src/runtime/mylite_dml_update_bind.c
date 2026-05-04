#include "mylite_dml.h"

#include "mylite_diagnostics.h"
#include "mylite_expression_validation.h"
#include "mylite_select.h"
#include "mylite_select_types.h"
#include "mylite_span.h"
#include "sql/mylite_expression.h"

#include <stdlib.h>
#include <string.h>

static int copy_dml_target_name(mylite_db *database, const char *source, char **out_name);
static int reject_deferred_update_clauses(mylite_db *database,
                                          const struct mylite_update_plan *plan);
static int bind_update_assignment_values(mylite_db *database,
                                         const struct mylite_select_table *table,
                                         struct mylite_update_bound_assignment *assignments,
                                         size_t assignment_count);
static int bind_update_assignment_expression(mylite_db *database,
                                             const struct mylite_select_table *table,
                                             const struct mylite_sql_ast_node *expression);
static int bind_update_where_clause(mylite_db *database, const struct mylite_update_plan *plan,
                                    const struct mylite_select_table *table);
static int bind_update_predicate_expression(mylite_db *database,
                                            const struct mylite_select_table *table,
                                            const struct mylite_sql_ast_node *expression,
                                            const char *clause_context);
static int bind_update_function_call(mylite_db *database, const struct mylite_select_table *table,
                                     const struct mylite_sql_ast_node *expression,
                                     const char *clause_context);
static int bind_update_order_expression(mylite_db *database,
                                        const struct mylite_select_table *table,
                                        const struct mylite_sql_ast_node *expression);
static size_t update_column_reference_index(const struct mylite_select_table *table,
                                            const struct mylite_update_column_reference *reference);
static bool
update_column_reference_qualifiers_match(const struct mylite_select_table *table,
                                         const struct mylite_update_column_reference *reference);
static size_t update_select_column_index(const struct mylite_select_table *table,
                                         const char *column_name);
static char *
copy_update_column_reference_name(const struct mylite_update_column_reference *reference);
static int set_update_unknown_field_error(mylite_db *database, const char *column_name);

int mylite_dml_copy_update_target_to_select_table(mylite_db *database,
                                                  const struct mylite_update_plan *plan,
                                                  struct mylite_select_table *table)
{
    const struct mylite_update_target *target = NULL;
    int status = MYLITE_OK;

    if (database == NULL || plan == NULL || table == NULL) {
        return MYLITE_MISUSE;
    }

    target = &plan->target;
    if (target->schema_name != NULL) {
        status = copy_dml_target_name(database, target->schema_name, &table->schema_name);
        if (status != MYLITE_OK) {
            return status;
        }
    }
    status = copy_dml_target_name(database, target->table_name, &table->table_name);
    if (status != MYLITE_OK) {
        return status;
    }
    if (target->alias != NULL) {
        status = copy_dml_target_name(database, target->alias, &table->alias);
        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

int mylite_dml_copy_delete_target_to_select_table(mylite_db *database,
                                                  const struct mylite_delete_plan *plan,
                                                  struct mylite_select_table *table)
{
    const struct mylite_delete_target *target = NULL;
    int status = MYLITE_OK;

    if (database == NULL || plan == NULL || table == NULL) {
        return MYLITE_MISUSE;
    }

    target = &plan->target;
    if (target->schema_name != NULL) {
        status = copy_dml_target_name(database, target->schema_name, &table->schema_name);
        if (status != MYLITE_OK) {
            return status;
        }
    }
    status = copy_dml_target_name(database, target->table_name, &table->table_name);
    if (status != MYLITE_OK) {
        return status;
    }
    if (target->alias != NULL) {
        status = copy_dml_target_name(database, target->alias, &table->alias);
        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

int mylite_dml_bind_update_subset(mylite_db *database, const struct mylite_update_plan *plan,
                                  const struct mylite_select_table *table,
                                  struct mylite_update_bound_assignment **out_assignments)
{
    struct mylite_update_bound_assignment *assignments = NULL;
    size_t assignment_count = 0U;
    int status = MYLITE_OK;

    if (database == NULL || plan == NULL || table == NULL || out_assignments == NULL) {
        return MYLITE_MISUSE;
    }

    status = reject_deferred_update_clauses(database, plan);
    *out_assignments = NULL;
    if (status != MYLITE_OK) {
        return status;
    }

    assignment_count = plan->assignment_count;
    if (assignment_count == 0U) {
        return mylite_dml_set_update_unsupported_assignment_error(database);
    }

    assignments = calloc(assignment_count, sizeof(*assignments));
    if (assignments == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = mylite_dml_bind_update_assignment_targets(database, plan, table, assignments,
                                                       assignment_count);
    if (status == MYLITE_OK) {
        status = bind_update_assignment_values(database, table, assignments, assignment_count);
    }
    if (status == MYLITE_OK) {
        status = bind_update_where_clause(database, plan, table);
    }
    if (status != MYLITE_OK) {
        free(assignments);
        return status;
    }

    *out_assignments = assignments;
    return MYLITE_OK;
}

int mylite_dml_bind_update_order_by_clause(mylite_db *database,
                                           const struct mylite_update_plan *plan,
                                           const struct mylite_select_table *table,
                                           struct mylite_update_order_plan *order_plan)
{
    const struct mylite_sql_ast_node *items = NULL;

    if (database == NULL || plan == NULL || table == NULL || order_plan == NULL) {
        return MYLITE_MISUSE;
    }

    items = mylite_ast_child_at(plan->order_by_clause, 0U);
    if (plan->order_by_clause == NULL) {
        return MYLITE_OK;
    }
    if (plan->order_by_clause->kind != MYLITE_SQL_AST_ORDER_BY_CLAUSE || items == NULL ||
        items->kind != MYLITE_SQL_AST_ORDER_ITEM_LIST) {
        return mylite_dml_set_update_unsupported_clause_error(database);
    }

    for (const struct mylite_sql_ast_node *item = items->first_child; item != NULL;
         item = item->next_sibling) {
        const struct mylite_sql_ast_node *expression = mylite_ast_child_at(item, 0U);
        struct mylite_select_order_key order_key = {
            .kind = MYLITE_SELECT_ORDER_KEY_EXPRESSION,
            .direction = MYLITE_SQL_AST_KEY_PART_ORDER_ASC,
            .expression = expression,
        };
        int status = MYLITE_OK;

        if (item->kind != MYLITE_SQL_AST_ORDER_ITEM || expression == NULL) {
            return mylite_dml_set_update_unsupported_clause_error(database);
        }
        if (item->key_part_order == MYLITE_SQL_AST_KEY_PART_ORDER_DESC) {
            order_key.direction = MYLITE_SQL_AST_KEY_PART_ORDER_DESC;
        }
        status = bind_update_order_expression(database, table, expression);
        if (status == MYLITE_OK) {
            status = mylite_dml_add_update_order_key(order_plan, &order_key);
            if (status == MYLITE_NOMEM) {
                (void)mylite_diagnostics_set_error_message(database, "out of memory");
            }
        }
        if (status != MYLITE_OK) {
            return status;
        }
    }
    return order_plan->order_key_count == 0U
               ? mylite_dml_set_update_unsupported_clause_error(database)
               : MYLITE_OK;
}

int mylite_dml_bind_update_assignment_targets(mylite_db *database,
                                              const struct mylite_update_plan *plan,
                                              const struct mylite_select_table *table,
                                              struct mylite_update_bound_assignment *assignments,
                                              size_t assignment_count)
{
    if (database == NULL || plan == NULL || table == NULL) {
        return MYLITE_MISUSE;
    }
    if (assignment_count != plan->assignment_count) {
        return MYLITE_MISUSE;
    }
    if (assignment_count != 0U && (plan->assignments == NULL || assignments == NULL)) {
        return MYLITE_MISUSE;
    }

    for (size_t index = 0U; index < assignment_count; ++index) {
        const struct mylite_update_assignment *assignment = &plan->assignments[index];
        size_t column_index = update_column_reference_index(table, &assignment->target);

        if (column_index == table->column_count) {
            char *reference = copy_update_column_reference_name(&assignment->target);
            int status = MYLITE_OK;

            if (reference == NULL) {
                (void)mylite_diagnostics_set_error_message(database, "out of memory");
                return MYLITE_NOMEM;
            }
            status = set_update_unknown_field_error(database, reference);
            free(reference);
            return status;
        }
        assignments[index] = (struct mylite_update_bound_assignment){
            .column_index = column_index,
            .value = assignment->value,
        };
    }
    return MYLITE_OK;
}

static int reject_deferred_update_clauses(mylite_db *database,
                                          const struct mylite_update_plan *plan)
{
    const struct mylite_sql_ast_node *limit = plan->limit_clause;

    if (limit == NULL) {
        return MYLITE_OK;
    }
    if (limit->kind != MYLITE_SQL_AST_UPDATE_LIMIT_CLAUSE ||
        mylite_ast_child_at(limit, 0U) == NULL ||
        mylite_ast_child_at(limit, 0U)->kind != MYLITE_SQL_AST_LIMIT_BOUND ||
        !mylite_ast_child_at(limit, 0U)->has_limit_bound_value) {
        return mylite_dml_set_update_unsupported_clause_error(database);
    }
    return MYLITE_OK;
}

static int bind_update_assignment_values(mylite_db *database,
                                         const struct mylite_select_table *table,
                                         struct mylite_update_bound_assignment *assignments,
                                         size_t assignment_count)
{
    for (size_t index = 0U; index < assignment_count; ++index) {
        int status = bind_update_assignment_expression(database, table, assignments[index].value);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int bind_update_assignment_expression(mylite_db *database,
                                             const struct mylite_select_table *table,
                                             const struct mylite_sql_ast_node *expression)
{
    if (expression != NULL && expression->kind == MYLITE_SQL_AST_DEFAULT) {
        return MYLITE_OK;
    }
    return bind_update_predicate_expression(database, table, expression, "field list");
}

static int bind_update_where_clause(mylite_db *database, const struct mylite_update_plan *plan,
                                    const struct mylite_select_table *table)
{
    const struct mylite_sql_ast_node *predicate = mylite_ast_child_at(plan->where_clause, 0U);

    if (plan->where_clause == NULL) {
        return MYLITE_OK;
    }
    if (plan->where_clause->kind != MYLITE_SQL_AST_WHERE_CLAUSE || predicate == NULL) {
        return mylite_dml_set_update_unsupported_clause_error(database);
    }
    return bind_update_predicate_expression(database, table, predicate, "where clause");
}

// NOLINTNEXTLINE(misc-no-recursion)
static int bind_update_predicate_expression(mylite_db *database,
                                            const struct mylite_select_table *table,
                                            const struct mylite_sql_ast_node *expression,
                                            const char *clause_context)
{
    if (expression == NULL) {
        return mylite_dml_set_update_unsupported_clause_error(database);
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_LITERAL:
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP:
        return MYLITE_OK;
    case MYLITE_SQL_AST_IDENTIFIER:
    case MYLITE_SQL_AST_QUALIFIED_IDENTIFIER: {
        size_t column_index = table->column_count;
        int status = mylite_select_resolve_column_reference(table, expression, &column_index);

        if (status != MYLITE_OK) {
            return status;
        }
        if (column_index == table->column_count) {
            char *reference = mylite_select_copy_reference_name(expression);

            if (reference == NULL) {
                (void)mylite_diagnostics_set_error_message(database, "out of memory");
                return MYLITE_NOMEM;
            }
            status =
                mylite_dml_set_update_unknown_column_error(database, reference, clause_context);
            free(reference);
            return status;
        }
        return MYLITE_OK;
    }
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_TERNARY_EXPRESSION:
    case MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION:
    case MYLITE_SQL_AST_EXPRESSION_LIST:
    case MYLITE_SQL_AST_CASE_EXPRESSION:
    case MYLITE_SQL_AST_CASE_WHEN_LIST:
    case MYLITE_SQL_AST_CASE_WHEN:
        for (const struct mylite_sql_ast_node *child = expression->first_child; child != NULL;
             child = child->next_sibling) {
            int status = bind_update_predicate_expression(database, table, child, clause_context);

            if (status != MYLITE_OK) {
                return status;
            }
        }
        return MYLITE_OK;
    case MYLITE_SQL_AST_SUBQUERY_EXPRESSION:
    case MYLITE_SQL_AST_EXISTS_EXPRESSION:
    case MYLITE_SQL_AST_QUANTIFIED_COMPARISON:
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
        return mylite_dml_set_update_unsupported_clause_error(database);
    case MYLITE_SQL_AST_CAST_EXPRESSION: {
        int status = mylite_expression_validate_cast_target_charset(database, expression);

        if (status != MYLITE_OK) {
            return status;
        }
        return bind_update_predicate_expression(
            database, table, mylite_ast_child_at(expression, 0U), clause_context);
    }
    case MYLITE_SQL_AST_FUNCTION_CALL:
        return bind_update_function_call(database, table, expression, clause_context);
    case MYLITE_SQL_AST_AGGREGATE_CALL:
    case MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST:
    case MYLITE_SQL_AST_DEFAULT:
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
    case MYLITE_SQL_AST_GROUP_BY_CLAUSE:
    case MYLITE_SQL_AST_GROUP_ITEM_LIST:
    case MYLITE_SQL_AST_GROUP_ITEM:
    case MYLITE_SQL_AST_HAVING_CLAUSE:
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
        return mylite_dml_set_update_unsupported_expression_error(database, clause_context);
    }

    return mylite_dml_set_update_unsupported_expression_error(database, clause_context);
}

// NOLINTNEXTLINE(misc-no-recursion)
static int bind_update_function_call(mylite_db *database, const struct mylite_select_table *table,
                                     const struct mylite_sql_ast_node *expression,
                                     const char *clause_context)
{
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    int status = MYLITE_OK;

    if (!mylite_expression_is_supported_function_call(expression)) {
        return mylite_dml_set_update_unsupported_expression_error(database, clause_context);
    }
    status = mylite_expression_validate_char_function_charset(database, expression);
    if (status != MYLITE_OK) {
        return status;
    }
    for (const struct mylite_sql_ast_node *child = arguments == NULL ? NULL
                                                                     : arguments->first_child;
         child != NULL; child = child->next_sibling) {
        status = bind_update_predicate_expression(database, table, child, clause_context);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int bind_update_order_expression(mylite_db *database,
                                        const struct mylite_select_table *table,
                                        const struct mylite_sql_ast_node *expression)
{
    return bind_update_predicate_expression(database, table, expression, "order clause");
}

static int copy_dml_target_name(mylite_db *database, const char *source, char **out_name)
{
    *out_name = mylite_copy_nonempty_cstring(source);
    if (*out_name == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    return MYLITE_OK;
}

static size_t update_column_reference_index(const struct mylite_select_table *table,
                                            const struct mylite_update_column_reference *reference)
{
    if (!update_column_reference_qualifiers_match(table, reference)) {
        return table->column_count;
    }
    return update_select_column_index(table, reference->column_name);
}

static bool
update_column_reference_qualifiers_match(const struct mylite_select_table *table,
                                         const struct mylite_update_column_reference *reference)
{
    if (reference->schema_name != NULL) {
        if (table->alias != NULL || reference->table_name == NULL) {
            return false;
        }
        if (strcmp(reference->schema_name, table->schema_name) != 0) {
            return false;
        }
        if (strcmp(reference->table_name, table->table_name) != 0) {
            return false;
        }
        return true;
    }
    if (reference->table_name != NULL) {
        const char *visible_table = table->alias == NULL ? table->table_name : table->alias;

        if (strcmp(reference->table_name, visible_table) != 0) {
            return false;
        }
        return true;
    }
    return true;
}

static size_t update_select_column_index(const struct mylite_select_table *table,
                                         const char *column_name)
{
    for (size_t index = 0U; index < table->column_count; ++index) {
        if (mylite_ascii_case_equal(table->columns[index].name, column_name)) {
            return index;
        }
    }
    return table->column_count;
}

static char *
copy_update_column_reference_name(const struct mylite_update_column_reference *reference)
{
    sqlite3_str *text = sqlite3_str_new(NULL);

    if (text == NULL) {
        return NULL;
    }
    if (reference->schema_name != NULL) {
        sqlite3_str_appendf(text, "%s.", reference->schema_name);
    }
    if (reference->table_name != NULL) {
        sqlite3_str_appendf(text, "%s.", reference->table_name);
    }
    sqlite3_str_append(text, reference->column_name == NULL ? "" : reference->column_name,
                       reference->column_name == NULL ? 0 : (int)strlen(reference->column_name));
    return sqlite3_str_finish(text);
}

static int set_update_unknown_field_error(mylite_db *database, const char *column_name)
{
    return mylite_dml_set_update_unknown_column_error(database, column_name, "field list");
}
