#include "mylite_ast.h"

#include <stdlib.h>

void mylite_sql_ast_init(struct mylite_sql_ast *ast)
{
    if (ast == NULL) {
        return;
    }

    ast->first_allocated = NULL;
}

void mylite_sql_ast_deinit(struct mylite_sql_ast *ast)
{
    struct mylite_sql_ast_node *node = NULL;

    if (ast == NULL) {
        return;
    }

    node = ast->first_allocated;
    while (node != NULL) {
        struct mylite_sql_ast_node *next = node->next_allocated;
        free(node);
        node = next;
    }
    ast->first_allocated = NULL;
}

struct mylite_sql_ast_node *mylite_sql_ast_new_node(struct mylite_sql_ast *ast,
                                                    enum mylite_sql_ast_node_kind kind,
                                                    struct mylite_sql_source_span span)
{
    struct mylite_sql_ast_node *node = NULL;

    if (ast == NULL) {
        return NULL;
    }

    node = calloc(1U, sizeof(*node));
    if (node == NULL) {
        return NULL;
    }

    node->kind = kind;
    node->span = span;
    node->next_allocated = ast->first_allocated;
    ast->first_allocated = node;
    return node;
}

void mylite_sql_ast_node_append_child(struct mylite_sql_ast_node *parent,
                                      struct mylite_sql_ast_node *child)
{
    if (parent == NULL || child == NULL) {
        return;
    }

    child->next_sibling = NULL;
    if (parent->last_child == NULL) {
        parent->first_child = child;
    } else {
        parent->last_child->next_sibling = child;
    }
    parent->last_child = child;
}

void mylite_sql_ast_node_set_span(struct mylite_sql_ast_node *node,
                                  struct mylite_sql_source_span span)
{
    if (node == NULL) {
        return;
    }

    node->span = span;
}

void mylite_sql_ast_node_set_literal_kind(struct mylite_sql_ast_node *node,
                                          enum mylite_sql_ast_literal_kind literal_kind)
{
    if (node == NULL) {
        return;
    }

    node->literal_kind = literal_kind;
}

void mylite_sql_ast_node_set_delete_form(struct mylite_sql_ast_node *node,
                                         enum mylite_sql_ast_delete_form delete_form)
{
    if (node == NULL) {
        return;
    }

    node->delete_form = delete_form;
}

void mylite_sql_ast_node_set_operator(struct mylite_sql_ast_node *node,
                                      enum mylite_sql_ast_operator operator_kind)
{
    if (node == NULL) {
        return;
    }

    node->operator_kind = operator_kind;
}

void mylite_sql_ast_node_set_schema_option(struct mylite_sql_ast_node *node,
                                           enum mylite_sql_ast_schema_option schema_option)
{
    if (node == NULL) {
        return;
    }

    node->schema_option = schema_option;
}

void mylite_sql_ast_node_set_column_type(struct mylite_sql_ast_node *node,
                                         enum mylite_sql_ast_column_type column_type)
{
    if (node == NULL) {
        return;
    }

    node->column_type = column_type;
}

void mylite_sql_ast_node_set_column_type_signed(struct mylite_sql_ast_node *node)
{
    if (node == NULL) {
        return;
    }

    node->column_type_signed = true;
}

void mylite_sql_ast_node_set_column_type_unsigned(struct mylite_sql_ast_node *node)
{
    if (node == NULL) {
        return;
    }

    node->column_type_unsigned = true;
}

void mylite_sql_ast_node_set_column_display_width(struct mylite_sql_ast_node *node,
                                                  unsigned int display_width)
{
    if (node == NULL) {
        return;
    }

    node->has_column_display_width = true;
    node->column_display_width = display_width;
}

void mylite_sql_ast_node_set_column_length(struct mylite_sql_ast_node *node, uint64_t length)
{
    if (node == NULL) {
        return;
    }

    node->has_column_length = true;
    node->column_length = length;
}

void mylite_sql_ast_node_set_column_precision(struct mylite_sql_ast_node *node, uint64_t precision)
{
    if (node == NULL) {
        return;
    }

    node->has_column_precision = true;
    node->column_precision = precision;
}

void mylite_sql_ast_node_set_column_scale(struct mylite_sql_ast_node *node, uint64_t scale)
{
    if (node == NULL) {
        return;
    }

    node->has_column_scale = true;
    node->column_scale = scale;
}

void mylite_sql_ast_node_set_column_character_set(struct mylite_sql_ast_node *node,
                                                  struct mylite_sql_source_span span)
{
    if (node == NULL) {
        return;
    }

    node->has_column_character_set = true;
    node->column_character_set = span;
}

void mylite_sql_ast_node_set_column_collation(struct mylite_sql_ast_node *node,
                                              struct mylite_sql_source_span span)
{
    if (node == NULL) {
        return;
    }

    node->has_column_collation = true;
    node->column_collation = span;
}

void mylite_sql_ast_node_set_column_binary_attribute(struct mylite_sql_ast_node *node)
{
    if (node == NULL) {
        return;
    }

    node->column_binary_attribute = true;
}

void mylite_sql_ast_node_set_column_byte_attribute(struct mylite_sql_ast_node *node)
{
    if (node == NULL) {
        return;
    }

    node->column_byte_attribute = true;
}

void mylite_sql_ast_node_set_column_zerofill_attribute(struct mylite_sql_ast_node *node)
{
    if (node == NULL) {
        return;
    }

    node->column_zerofill_attribute = true;
}

void mylite_sql_ast_node_set_column_national_attribute(struct mylite_sql_ast_node *node)
{
    if (node == NULL) {
        return;
    }

    node->column_national_attribute = true;
}

void mylite_sql_ast_node_set_column_attribute(struct mylite_sql_ast_node *node,
                                              enum mylite_sql_ast_column_attribute column_attribute)
{
    if (node == NULL) {
        return;
    }

    node->column_attribute = column_attribute;
}

void mylite_sql_ast_node_set_column_format(struct mylite_sql_ast_node *node,
                                           enum mylite_sql_ast_column_format column_format)
{
    if (node == NULL) {
        return;
    }

    node->column_format = column_format;
}

void mylite_sql_ast_node_set_column_storage(struct mylite_sql_ast_node *node,
                                            enum mylite_sql_ast_column_storage column_storage)
{
    if (node == NULL) {
        return;
    }

    node->column_storage = column_storage;
}

void mylite_sql_ast_node_set_key_part_order(struct mylite_sql_ast_node *node,
                                            enum mylite_sql_ast_key_part_order order)
{
    if (node == NULL) {
        return;
    }

    node->key_part_order = order;
}

void mylite_sql_ast_node_set_limit_bound_value(struct mylite_sql_ast_node *node, uint64_t value)
{
    if (node == NULL) {
        return;
    }

    node->has_limit_bound_value = true;
    node->limit_bound_value = value;
}

void mylite_sql_ast_node_set_index_algorithm(struct mylite_sql_ast_node *node,
                                             enum mylite_sql_ast_index_algorithm algorithm)
{
    if (node == NULL) {
        return;
    }

    node->index_algorithm = algorithm;
}

void mylite_sql_ast_node_set_index_option(struct mylite_sql_ast_node *node,
                                          enum mylite_sql_ast_index_option option)
{
    if (node == NULL) {
        return;
    }

    node->index_option = option;
}

void mylite_sql_ast_node_set_index_class(struct mylite_sql_ast_node *node,
                                         enum mylite_sql_ast_index_class index_class)
{
    if (node == NULL) {
        return;
    }

    node->index_class = index_class;
}

void mylite_sql_ast_node_set_ddl_table_option(struct mylite_sql_ast_node *node,
                                              enum mylite_sql_ast_ddl_table_option option)
{
    if (node == NULL) {
        return;
    }

    node->ddl_table_option = option;
}

void mylite_sql_ast_node_set_table_option(struct mylite_sql_ast_node *node,
                                          enum mylite_sql_ast_table_option option)
{
    if (node == NULL) {
        return;
    }

    node->table_option = option;
}

void mylite_sql_ast_node_set_alter_table_action(struct mylite_sql_ast_node *node,
                                                enum mylite_sql_ast_alter_table_action action,
                                                bool column_keyword)
{
    if (node == NULL) {
        return;
    }

    node->alter_table_action = action;
    node->alter_table_action_column_keyword = column_keyword;
}

void mylite_sql_ast_node_set_alter_table_column_position(
    struct mylite_sql_ast_node *node, enum mylite_sql_ast_alter_table_column_position position)
{
    if (node == NULL) {
        return;
    }

    node->alter_table_column_position = position;
}

void mylite_sql_ast_node_set_alter_table_index_spelling(
    struct mylite_sql_ast_node *node, enum mylite_sql_ast_alter_table_index_spelling spelling)
{
    if (node == NULL) {
        return;
    }

    node->alter_table_index_spelling = spelling;
}

void mylite_sql_ast_node_set_alter_table_constraint_spelling(
    struct mylite_sql_ast_node *node, enum mylite_sql_ast_alter_table_constraint_spelling spelling)
{
    if (node == NULL) {
        return;
    }

    node->alter_table_constraint_spelling = spelling;
}

void mylite_sql_ast_node_set_constraint_enforcement(
    struct mylite_sql_ast_node *node, enum mylite_sql_ast_constraint_enforcement enforcement)
{
    if (node == NULL) {
        return;
    }

    node->constraint_enforcement = enforcement;
}

void mylite_sql_ast_node_set_reference_option(struct mylite_sql_ast_node *node,
                                              enum mylite_sql_ast_reference_option option)
{
    if (node == NULL) {
        return;
    }

    node->reference_option = option;
}

void mylite_sql_ast_node_set_reference_action(struct mylite_sql_ast_node *node,
                                              enum mylite_sql_ast_reference_action action)
{
    if (node == NULL) {
        return;
    }

    node->reference_action = action;
}

void mylite_sql_ast_node_set_reference_match(struct mylite_sql_ast_node *node,
                                             enum mylite_sql_ast_reference_match match)
{
    if (node == NULL) {
        return;
    }

    node->reference_match = match;
}

void mylite_sql_ast_node_set_drop_table_temporary(struct mylite_sql_ast_node *node)
{
    if (node == NULL) {
        return;
    }

    node->drop_table_temporary = true;
}

void mylite_sql_ast_node_set_drop_table_restrict(struct mylite_sql_ast_node *node)
{
    if (node == NULL) {
        return;
    }

    node->drop_table_restrict = true;
}

void mylite_sql_ast_node_set_drop_table_cascade(struct mylite_sql_ast_node *node)
{
    if (node == NULL) {
        return;
    }

    node->drop_table_cascade = true;
}

void mylite_sql_ast_node_set_insert_ignore(struct mylite_sql_ast_node *node)
{
    if (node == NULL) {
        return;
    }

    node->insert_ignore = true;
}

void mylite_sql_ast_node_set_replace_low_priority(struct mylite_sql_ast_node *node)
{
    if (node == NULL) {
        return;
    }

    node->replace_low_priority = true;
}

void mylite_sql_ast_node_set_replace_delayed(struct mylite_sql_ast_node *node)
{
    if (node == NULL) {
        return;
    }

    node->replace_delayed = true;
}

void mylite_sql_ast_node_set_transaction_access_mode(
    struct mylite_sql_ast_node *node, enum mylite_sql_ast_transaction_access_mode access_mode)
{
    if (node == NULL) {
        return;
    }

    node->transaction_access_mode = access_mode;
}

void mylite_sql_ast_node_set_transaction_consistent_snapshot(struct mylite_sql_ast_node *node)
{
    if (node == NULL) {
        return;
    }

    node->transaction_consistent_snapshot = true;
}

void mylite_sql_ast_node_set_transaction_completion(struct mylite_sql_ast_node *node,
                                                    enum mylite_sql_ast_transaction_chain chain,
                                                    enum mylite_sql_ast_transaction_release release)
{
    if (node == NULL) {
        return;
    }

    node->transaction_chain = chain;
    node->transaction_release = release;
}

void mylite_sql_ast_node_set_case_expression_simple(struct mylite_sql_ast_node *node)
{
    if (node == NULL) {
        return;
    }

    node->case_expression_simple = true;
}

void mylite_sql_ast_node_set_aggregate(struct mylite_sql_ast_node *node,
                                       enum mylite_sql_ast_aggregate_kind aggregate_kind,
                                       enum mylite_sql_ast_aggregate_argument aggregate_argument)
{
    if (node == NULL) {
        return;
    }

    node->aggregate_kind = aggregate_kind;
    node->aggregate_argument = aggregate_argument;
}

void mylite_sql_ast_node_set_join_type(struct mylite_sql_ast_node *node,
                                       enum mylite_sql_ast_join_type join_type)
{
    if (node == NULL) {
        return;
    }

    node->join_type = join_type;
}

void mylite_sql_ast_node_set_join_condition_type(
    struct mylite_sql_ast_node *node, enum mylite_sql_ast_join_condition_type condition_type)
{
    if (node == NULL) {
        return;
    }

    node->join_condition_type = condition_type;
}

void mylite_sql_ast_node_set_select_duplicate_mode(
    struct mylite_sql_ast_node *node, enum mylite_sql_ast_select_duplicate_mode mode,
    bool explicit_mode, bool conflict, size_t modifier_count,
    struct mylite_sql_ast_select_duplicate_mode_spans spans)
{
    if (node == NULL) {
        return;
    }

    node->select_duplicate_mode = mode;
    node->select_duplicate_mode_explicit = explicit_mode;
    node->select_duplicate_mode_conflict = conflict;
    node->select_duplicate_modifier_count = modifier_count;
    node->select_duplicate_first_span = spans.first;
    node->select_duplicate_last_span = spans.last;
    node->select_duplicate_conflict_span = spans.conflict;
}

void mylite_sql_ast_node_set_select_calc_found_rows(struct mylite_sql_ast_node *node)
{
    if (node == NULL) {
        return;
    }

    node->select_calc_found_rows = true;
}

void mylite_sql_ast_node_set_set_duplicate_mode(struct mylite_sql_ast_node *node,
                                                enum mylite_sql_ast_set_duplicate_mode mode)
{
    if (node == NULL) {
        return;
    }

    node->set_duplicate_mode = mode;
}

void mylite_sql_ast_node_set_subquery_quantifier(struct mylite_sql_ast_node *node,
                                                 enum mylite_sql_ast_subquery_quantifier quantifier)
{
    if (node == NULL) {
        return;
    }

    node->subquery_quantifier = quantifier;
}

void mylite_sql_ast_node_set_trim_spec(struct mylite_sql_ast_node *node,
                                       enum mylite_sql_ast_trim_direction direction)
{
    if (node == NULL) {
        return;
    }

    node->trim_spec = true;
    node->trim_direction = direction;
}

void mylite_sql_ast_node_set_interval_spec(struct mylite_sql_ast_node *node,
                                           enum mylite_sql_ast_interval_unit unit)
{
    if (node == NULL) {
        return;
    }

    node->interval_spec = true;
    node->interval_unit = unit;
}

void mylite_sql_ast_node_set_show_tables_extended(struct mylite_sql_ast_node *node)
{
    if (node == NULL) {
        return;
    }

    node->show_tables_extended = true;
}

void mylite_sql_ast_node_set_show_tables_full(struct mylite_sql_ast_node *node)
{
    if (node == NULL) {
        return;
    }

    node->show_tables_full = true;
}

void mylite_sql_ast_node_set_show_columns_extended(struct mylite_sql_ast_node *node)
{
    if (node == NULL) {
        return;
    }

    node->show_columns_extended = true;
}

void mylite_sql_ast_node_set_show_columns_full(struct mylite_sql_ast_node *node)
{
    if (node == NULL) {
        return;
    }

    node->show_columns_full = true;
}

void mylite_sql_ast_node_set_show_index_extended(struct mylite_sql_ast_node *node)
{
    if (node == NULL) {
        return;
    }

    node->show_index_extended = true;
}

void mylite_sql_ast_node_set_show_create_schema_if_not_exists(struct mylite_sql_ast_node *node)
{
    if (node == NULL) {
        return;
    }

    node->show_create_schema_if_not_exists = true;
}

void mylite_sql_ast_node_set_show_diagnostics_kind(struct mylite_sql_ast_node *node,
                                                   enum mylite_sql_ast_show_diagnostics_kind kind)
{
    if (node == NULL) {
        return;
    }

    node->show_diagnostics_kind = kind;
}

void mylite_sql_ast_node_set_show_variables_scope(struct mylite_sql_ast_node *node,
                                                  enum mylite_sql_ast_show_variables_scope scope)
{
    if (node == NULL) {
        return;
    }

    node->show_variables_scope = scope;
}

void mylite_sql_ast_node_set_show_status_scope(struct mylite_sql_ast_node *node,
                                               enum mylite_sql_ast_show_status_scope scope)
{
    if (node == NULL) {
        return;
    }

    node->show_status_scope = scope;
}

void mylite_sql_ast_node_set_system_variable_scope(
    struct mylite_sql_ast_node *node, enum mylite_sql_ast_set_system_variable_scope scope)
{
    if (node == NULL) {
        return;
    }

    node->set_system_variable_scope = scope;
}

const struct mylite_sql_ast_node *
mylite_sql_ast_unwrap_parenthesized_expression(const struct mylite_sql_ast_node *expression)
{
    while (expression != NULL && expression->kind == MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION) {
        expression = expression->first_child;
    }
    return expression;
}

size_t mylite_sql_ast_node_child_count(const struct mylite_sql_ast_node *node)
{
    const struct mylite_sql_ast_node *child = NULL;
    size_t count = 0U;

    if (node == NULL) {
        return 0U;
    }

    child = node->first_child;
    while (child != NULL) {
        ++count;
        child = child->next_sibling;
    }
    return count;
}

const char *mylite_sql_ast_node_kind_name(enum mylite_sql_ast_node_kind kind)
{
    switch (kind) {
    case MYLITE_SQL_AST_SCRIPT:
        return "script";
    case MYLITE_SQL_AST_SELECT_STATEMENT:
        return "select_statement";
    case MYLITE_SQL_AST_USE_STATEMENT:
        return "use_statement";
    case MYLITE_SQL_AST_SELECT_LIST:
        return "select_list";
    case MYLITE_SQL_AST_SELECT_ITEM:
        return "select_item";
    case MYLITE_SQL_AST_FROM_DUAL:
        return "from_dual";
    case MYLITE_SQL_AST_FROM_TABLE:
        return "from_table";
    case MYLITE_SQL_AST_FROM_TABLE_REFERENCES:
        return "from_table_references";
    case MYLITE_SQL_AST_TABLE_REFERENCE_LIST:
        return "table_reference_list";
    case MYLITE_SQL_AST_JOIN_EXPRESSION:
        return "join_expression";
    case MYLITE_SQL_AST_JOIN_CONDITION:
        return "join_condition";
    case MYLITE_SQL_AST_USING_COLUMN_LIST:
        return "using_column_list";
    case MYLITE_SQL_AST_USING_COLUMN:
        return "using_column";
    case MYLITE_SQL_AST_IDENTIFIER:
        return "identifier";
    case MYLITE_SQL_AST_QUALIFIED_IDENTIFIER:
        return "qualified_identifier";
    case MYLITE_SQL_AST_WILDCARD:
        return "wildcard";
    case MYLITE_SQL_AST_LITERAL:
        return "literal";
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
        return "unary_expression";
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
        return "binary_expression";
    case MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION:
        return "parenthesized_expression";
    case MYLITE_SQL_AST_CREATE_SCHEMA_STATEMENT:
        return "create_schema_statement";
    case MYLITE_SQL_AST_ALTER_SCHEMA_STATEMENT:
        return "alter_schema_statement";
    case MYLITE_SQL_AST_DROP_SCHEMA_STATEMENT:
        return "drop_schema_statement";
    case MYLITE_SQL_AST_SHOW_SCHEMAS_STATEMENT:
        return "show_schemas_statement";
    case MYLITE_SQL_AST_IF_EXISTS:
        return "if_exists";
    case MYLITE_SQL_AST_IF_NOT_EXISTS:
        return "if_not_exists";
    case MYLITE_SQL_AST_SCHEMA_OPTION_LIST:
        return "schema_option_list";
    case MYLITE_SQL_AST_SCHEMA_OPTION:
        return "schema_option";
    case MYLITE_SQL_AST_SET_NAMES_STATEMENT:
        return "set_names_statement";
    case MYLITE_SQL_AST_SET_CHARACTER_SET_STATEMENT:
        return "set_character_set_statement";
    case MYLITE_SQL_AST_SET_SYSTEM_VARIABLE_STATEMENT:
        return "set_system_variable_statement";
    case MYLITE_SQL_AST_DEFAULT:
        return "default";
    case MYLITE_SQL_AST_CREATE_TABLE_STATEMENT:
        return "create_table_statement";
    case MYLITE_SQL_AST_COLUMN_DEFINITION_LIST:
        return "column_definition_list";
    case MYLITE_SQL_AST_COLUMN_DEFINITION:
        return "column_definition";
    case MYLITE_SQL_AST_COLUMN_TYPE:
        return "column_type";
    case MYLITE_SQL_AST_COLUMN_TYPE_ATTRIBUTE_LIST:
        return "column_type_attribute_list";
    case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_LIST:
        return "column_attribute_list";
    case MYLITE_SQL_AST_COLUMN_ATTRIBUTE:
        return "column_attribute";
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP:
        return "current_timestamp";
    case MYLITE_SQL_AST_PRIMARY_KEY_CONSTRAINT:
        return "primary_key_constraint";
    case MYLITE_SQL_AST_KEY_PART_LIST:
        return "key_part_list";
    case MYLITE_SQL_AST_KEY_PART:
        return "key_part";
    case MYLITE_SQL_AST_INDEX_TYPE:
        return "index_type";
    case MYLITE_SQL_AST_INDEX_OPTION_LIST:
        return "index_option_list";
    case MYLITE_SQL_AST_INDEX_OPTION:
        return "index_option";
    case MYLITE_SQL_AST_SECONDARY_INDEX:
        return "secondary_index";
    case MYLITE_SQL_AST_UNIQUE_INDEX:
        return "unique_index";
    case MYLITE_SQL_AST_TABLE_OPTION_LIST:
        return "table_option_list";
    case MYLITE_SQL_AST_TABLE_OPTION:
        return "table_option";
    case MYLITE_SQL_AST_DROP_TABLE_STATEMENT:
        return "drop_table_statement";
    case MYLITE_SQL_AST_TABLE_NAME_LIST:
        return "table_name_list";
    case MYLITE_SQL_AST_INSERT_VALUES_STATEMENT:
        return "insert_values_statement";
    case MYLITE_SQL_AST_INSERT_COLUMN_LIST:
        return "insert_column_list";
    case MYLITE_SQL_AST_INSERT_ROW_LIST:
        return "insert_row_list";
    case MYLITE_SQL_AST_INSERT_ROW:
        return "insert_row";
    case MYLITE_SQL_AST_INSERT_VALUE_LIST:
        return "insert_value_list";
    case MYLITE_SQL_AST_INSERT_SET_STATEMENT:
        return "insert_set_statement";
    case MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT_LIST:
        return "insert_set_assignment_list";
    case MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT:
        return "insert_set_assignment";
    case MYLITE_SQL_AST_TERNARY_EXPRESSION:
        return "ternary_expression";
    case MYLITE_SQL_AST_EXPRESSION_LIST:
        return "expression_list";
    case MYLITE_SQL_AST_WHERE_CLAUSE:
        return "where_clause";
    case MYLITE_SQL_AST_ORDER_BY_CLAUSE:
        return "order_by_clause";
    case MYLITE_SQL_AST_ORDER_ITEM_LIST:
        return "order_item_list";
    case MYLITE_SQL_AST_ORDER_ITEM:
        return "order_item";
    case MYLITE_SQL_AST_LIMIT_CLAUSE:
        return "limit_clause";
    case MYLITE_SQL_AST_LIMIT_BOUND:
        return "limit_bound";
    case MYLITE_SQL_AST_UPDATE_STATEMENT:
        return "update_statement";
    case MYLITE_SQL_AST_UPDATE_TARGET:
        return "update_target";
    case MYLITE_SQL_AST_UPDATE_ASSIGNMENT_LIST:
        return "update_assignment_list";
    case MYLITE_SQL_AST_UPDATE_ASSIGNMENT:
        return "update_assignment";
    case MYLITE_SQL_AST_UPDATE_LIMIT_CLAUSE:
        return "update_limit_clause";
    case MYLITE_SQL_AST_DELETE_STATEMENT:
        return "delete_statement";
    case MYLITE_SQL_AST_DELETE_TARGET:
        return "delete_target";
    case MYLITE_SQL_AST_DELETE_LIMIT_CLAUSE:
        return "delete_limit_clause";
    case MYLITE_SQL_AST_START_TRANSACTION_STATEMENT:
        return "start_transaction_statement";
    case MYLITE_SQL_AST_BEGIN_TRANSACTION_STATEMENT:
        return "begin_transaction_statement";
    case MYLITE_SQL_AST_TRANSACTION_CHARACTERISTIC_LIST:
        return "transaction_characteristic_list";
    case MYLITE_SQL_AST_TRANSACTION_CHARACTERISTIC:
        return "transaction_characteristic";
    case MYLITE_SQL_AST_COMMIT_STATEMENT:
        return "commit_statement";
    case MYLITE_SQL_AST_ROLLBACK_STATEMENT:
        return "rollback_statement";
    case MYLITE_SQL_AST_TRANSACTION_COMPLETION:
        return "transaction_completion";
    case MYLITE_SQL_AST_SAVEPOINT_STATEMENT:
        return "savepoint_statement";
    case MYLITE_SQL_AST_ROLLBACK_TO_SAVEPOINT_STATEMENT:
        return "rollback_to_savepoint_statement";
    case MYLITE_SQL_AST_RELEASE_SAVEPOINT_STATEMENT:
        return "release_savepoint_statement";
    case MYLITE_SQL_AST_FUNCTION_CALL:
        return "function_call";
    case MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST:
        return "function_argument_list";
    case MYLITE_SQL_AST_CASE_EXPRESSION:
        return "case_expression";
    case MYLITE_SQL_AST_CASE_WHEN_LIST:
        return "case_when_list";
    case MYLITE_SQL_AST_CASE_WHEN:
        return "case_when";
    case MYLITE_SQL_AST_CAST_EXPRESSION:
        return "cast_expression";
    case MYLITE_SQL_AST_GROUP_BY_CLAUSE:
        return "group_by_clause";
    case MYLITE_SQL_AST_GROUP_ITEM_LIST:
        return "group_item_list";
    case MYLITE_SQL_AST_GROUP_ITEM:
        return "group_item";
    case MYLITE_SQL_AST_HAVING_CLAUSE:
        return "having_clause";
    case MYLITE_SQL_AST_AGGREGATE_CALL:
        return "aggregate_call";
    case MYLITE_SQL_AST_SUBQUERY_EXPRESSION:
        return "subquery_expression";
    case MYLITE_SQL_AST_EXISTS_EXPRESSION:
        return "exists_expression";
    case MYLITE_SQL_AST_QUANTIFIED_COMPARISON:
        return "quantified_comparison";
    case MYLITE_SQL_AST_ROW_CONSTRUCTOR:
        return "row_constructor";
    case MYLITE_SQL_AST_QUERY_EXPRESSION:
        return "query_expression";
    case MYLITE_SQL_AST_UNION_EXPRESSION:
        return "union_expression";
    case MYLITE_SQL_AST_QUERY_PRIMARY:
        return "query_primary";
    case MYLITE_SQL_AST_INSERT_DUPLICATE_UPDATE_CLAUSE:
        return "insert_duplicate_update_clause";
    case MYLITE_SQL_AST_INSERT_UPDATE_ASSIGNMENT_LIST:
        return "insert_update_assignment_list";
    case MYLITE_SQL_AST_INSERT_UPDATE_ASSIGNMENT:
        return "insert_update_assignment";
    case MYLITE_SQL_AST_INSERT_ROW_ALIAS:
        return "insert_row_alias";
    case MYLITE_SQL_AST_INSERT_ALIAS_COLUMN_LIST:
        return "insert_alias_column_list";
    case MYLITE_SQL_AST_REPLACE_VALUES_STATEMENT:
        return "replace_values_statement";
    case MYLITE_SQL_AST_REPLACE_SET_STATEMENT:
        return "replace_set_statement";
    case MYLITE_SQL_AST_CREATE_INDEX_STATEMENT:
        return "create_index_statement";
    case MYLITE_SQL_AST_DROP_INDEX_STATEMENT:
        return "drop_index_statement";
    case MYLITE_SQL_AST_DDL_TABLE_OPTION_LIST:
        return "ddl_table_option_list";
    case MYLITE_SQL_AST_DDL_TABLE_OPTION:
        return "ddl_table_option";
    case MYLITE_SQL_AST_ALTER_TABLE_STATEMENT:
        return "alter_table_statement";
    case MYLITE_SQL_AST_ALTER_TABLE_ITEM_LIST:
        return "alter_table_item_list";
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION:
        return "alter_table_action";
    case MYLITE_SQL_AST_ALTER_TABLE_COLUMN_POSITION:
        return "alter_table_column_position";
    case MYLITE_SQL_AST_RENAME_TABLE_STATEMENT:
        return "rename_table_statement";
    case MYLITE_SQL_AST_RENAME_TABLE_PAIR_LIST:
        return "rename_table_pair_list";
    case MYLITE_SQL_AST_RENAME_TABLE_PAIR:
        return "rename_table_pair";
    case MYLITE_SQL_AST_TRUNCATE_TABLE_STATEMENT:
        return "truncate_table_statement";
    case MYLITE_SQL_AST_SHOW_TABLES_STATEMENT:
        return "show_tables_statement";
    case MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT:
        return "show_columns_statement";
    case MYLITE_SQL_AST_SHOW_INDEX_STATEMENT:
        return "show_index_statement";
    case MYLITE_SQL_AST_DESCRIBE_TABLE_STATEMENT:
        return "describe_table_statement";
    case MYLITE_SQL_AST_SHOW_CREATE_TABLE_STATEMENT:
        return "show_create_table_statement";
    case MYLITE_SQL_AST_SHOW_DIAGNOSTICS_STATEMENT:
        return "show_diagnostics_statement";
    case MYLITE_SQL_AST_SHOW_DIAGNOSTICS_COUNT_STATEMENT:
        return "show_diagnostics_count_statement";
    case MYLITE_SQL_AST_SHOW_VARIABLES_STATEMENT:
        return "show_variables_statement";
    case MYLITE_SQL_AST_SHOW_STATUS_STATEMENT:
        return "show_status_statement";
    case MYLITE_SQL_AST_SHOW_CHARACTER_SET_STATEMENT:
        return "show_character_set_statement";
    case MYLITE_SQL_AST_SHOW_COLLATION_STATEMENT:
        return "show_collation_statement";
    case MYLITE_SQL_AST_SHOW_ENGINES_STATEMENT:
        return "show_engines_statement";
    case MYLITE_SQL_AST_SHOW_CREATE_SCHEMA_STATEMENT:
        return "show_create_schema_statement";
    case MYLITE_SQL_AST_SHOW_TABLE_STATUS_STATEMENT:
        return "show_table_status_statement";
    case MYLITE_SQL_AST_DELETE_TARGET_LIST:
        return "delete_target_list";
    case MYLITE_SQL_AST_DELETE_TARGET_NAME:
        return "delete_target_name";
    }

    return "unknown";
}

const char *mylite_sql_ast_schema_option_name(enum mylite_sql_ast_schema_option schema_option)
{
    switch (schema_option) {
    case MYLITE_SQL_AST_SCHEMA_OPTION_NONE:
        return "none";
    case MYLITE_SQL_AST_SCHEMA_OPTION_CHARACTER_SET:
        return "character_set";
    case MYLITE_SQL_AST_SCHEMA_OPTION_COLLATE:
        return "collate";
    case MYLITE_SQL_AST_SCHEMA_OPTION_ENCRYPTION:
        return "encryption";
    case MYLITE_SQL_AST_SCHEMA_OPTION_READ_ONLY:
        return "read_only";
    }

    return "unknown";
}

const char *mylite_sql_ast_column_type_name(enum mylite_sql_ast_column_type column_type)
{
    switch (column_type) {
    case MYLITE_SQL_AST_COLUMN_TYPE_NONE:
        return "none";
    case MYLITE_SQL_AST_COLUMN_TYPE_TINYINT:
        return "tinyint";
    case MYLITE_SQL_AST_COLUMN_TYPE_SMALLINT:
        return "smallint";
    case MYLITE_SQL_AST_COLUMN_TYPE_MEDIUMINT:
        return "mediumint";
    case MYLITE_SQL_AST_COLUMN_TYPE_INT:
        return "int";
    case MYLITE_SQL_AST_COLUMN_TYPE_BIGINT:
        return "bigint";
    case MYLITE_SQL_AST_COLUMN_TYPE_BOOL:
        return "bool";
    case MYLITE_SQL_AST_COLUMN_TYPE_BOOLEAN:
        return "boolean";
    case MYLITE_SQL_AST_COLUMN_TYPE_CHAR:
        return "char";
    case MYLITE_SQL_AST_COLUMN_TYPE_VARCHAR:
        return "varchar";
    case MYLITE_SQL_AST_COLUMN_TYPE_TINYTEXT:
        return "tinytext";
    case MYLITE_SQL_AST_COLUMN_TYPE_TEXT:
        return "text";
    case MYLITE_SQL_AST_COLUMN_TYPE_MEDIUMTEXT:
        return "mediumtext";
    case MYLITE_SQL_AST_COLUMN_TYPE_LONGTEXT:
        return "longtext";
    case MYLITE_SQL_AST_COLUMN_TYPE_BINARY:
        return "binary";
    case MYLITE_SQL_AST_COLUMN_TYPE_VARBINARY:
        return "varbinary";
    case MYLITE_SQL_AST_COLUMN_TYPE_TINYBLOB:
        return "tinyblob";
    case MYLITE_SQL_AST_COLUMN_TYPE_BLOB:
        return "blob";
    case MYLITE_SQL_AST_COLUMN_TYPE_MEDIUMBLOB:
        return "mediumblob";
    case MYLITE_SQL_AST_COLUMN_TYPE_LONGBLOB:
        return "longblob";
    case MYLITE_SQL_AST_COLUMN_TYPE_DECIMAL:
        return "decimal";
    case MYLITE_SQL_AST_COLUMN_TYPE_FLOAT:
        return "float";
    case MYLITE_SQL_AST_COLUMN_TYPE_DOUBLE:
        return "double";
    case MYLITE_SQL_AST_COLUMN_TYPE_DATE:
        return "date";
    case MYLITE_SQL_AST_COLUMN_TYPE_TIME:
        return "time";
    case MYLITE_SQL_AST_COLUMN_TYPE_DATETIME:
        return "datetime";
    case MYLITE_SQL_AST_COLUMN_TYPE_TIMESTAMP:
        return "timestamp";
    case MYLITE_SQL_AST_COLUMN_TYPE_YEAR:
        return "year";
    }

    return "unknown";
}

const char *
mylite_sql_ast_column_attribute_name(enum mylite_sql_ast_column_attribute column_attribute)
{
    switch (column_attribute) {
    case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_NONE:
        return "none";
    case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_NULL:
        return "null";
    case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_NOT_NULL:
        return "not_null";
    case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_DEFAULT:
        return "default";
    case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_ON_UPDATE:
        return "on_update";
    case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_COMMENT:
        return "comment";
    case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_VISIBLE:
        return "visible";
    case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_INVISIBLE:
        return "invisible";
    case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_COLUMN_FORMAT:
        return "column_format";
    case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_STORAGE:
        return "storage";
    case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_AUTO_INCREMENT:
        return "auto_increment";
    case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_PRIMARY_KEY:
        return "primary_key";
    case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_UNIQUE_KEY:
        return "unique_key";
    }

    return "unknown";
}

const char *mylite_sql_ast_column_format_name(enum mylite_sql_ast_column_format column_format)
{
    switch (column_format) {
    case MYLITE_SQL_AST_COLUMN_FORMAT_NONE:
        return "none";
    case MYLITE_SQL_AST_COLUMN_FORMAT_DEFAULT:
        return "default";
    case MYLITE_SQL_AST_COLUMN_FORMAT_FIXED:
        return "fixed";
    case MYLITE_SQL_AST_COLUMN_FORMAT_DYNAMIC:
        return "dynamic";
    }

    return "unknown";
}

const char *mylite_sql_ast_column_storage_name(enum mylite_sql_ast_column_storage column_storage)
{
    switch (column_storage) {
    case MYLITE_SQL_AST_COLUMN_STORAGE_NONE:
        return "none";
    case MYLITE_SQL_AST_COLUMN_STORAGE_DEFAULT:
        return "default";
    case MYLITE_SQL_AST_COLUMN_STORAGE_DISK:
        return "disk";
    case MYLITE_SQL_AST_COLUMN_STORAGE_MEMORY:
        return "memory";
    }

    return "unknown";
}

const char *mylite_sql_ast_key_part_order_name(enum mylite_sql_ast_key_part_order order)
{
    switch (order) {
    case MYLITE_SQL_AST_KEY_PART_ORDER_NONE:
        return "none";
    case MYLITE_SQL_AST_KEY_PART_ORDER_ASC:
        return "asc";
    case MYLITE_SQL_AST_KEY_PART_ORDER_DESC:
        return "desc";
    }

    return "unknown";
}

const char *mylite_sql_ast_index_algorithm_name(enum mylite_sql_ast_index_algorithm algorithm)
{
    switch (algorithm) {
    case MYLITE_SQL_AST_INDEX_ALGORITHM_NONE:
        return "none";
    case MYLITE_SQL_AST_INDEX_ALGORITHM_BTREE:
        return "btree";
    case MYLITE_SQL_AST_INDEX_ALGORITHM_HASH:
        return "hash";
    }

    return "unknown";
}

const char *mylite_sql_ast_index_option_name(enum mylite_sql_ast_index_option option)
{
    switch (option) {
    case MYLITE_SQL_AST_INDEX_OPTION_NONE:
        return "none";
    case MYLITE_SQL_AST_INDEX_OPTION_USING:
        return "using";
    case MYLITE_SQL_AST_INDEX_OPTION_KEY_BLOCK_SIZE:
        return "key_block_size";
    case MYLITE_SQL_AST_INDEX_OPTION_COMMENT:
        return "comment";
    case MYLITE_SQL_AST_INDEX_OPTION_VISIBLE:
        return "visible";
    case MYLITE_SQL_AST_INDEX_OPTION_INVISIBLE:
        return "invisible";
    case MYLITE_SQL_AST_INDEX_OPTION_ENGINE_ATTRIBUTE:
        return "engine_attribute";
    case MYLITE_SQL_AST_INDEX_OPTION_SECONDARY_ENGINE_ATTRIBUTE:
        return "secondary_engine_attribute";
    case MYLITE_SQL_AST_INDEX_OPTION_WITH_PARSER:
        return "with_parser";
    }

    return "unknown";
}

const char *mylite_sql_ast_index_class_name(enum mylite_sql_ast_index_class index_class)
{
    switch (index_class) {
    case MYLITE_SQL_AST_INDEX_CLASS_ORDINARY:
        return "ordinary";
    case MYLITE_SQL_AST_INDEX_CLASS_UNIQUE:
        return "unique";
    case MYLITE_SQL_AST_INDEX_CLASS_FULLTEXT:
        return "fulltext";
    case MYLITE_SQL_AST_INDEX_CLASS_SPATIAL:
        return "spatial";
    }

    return "unknown";
}

const char *mylite_sql_ast_ddl_table_option_name(enum mylite_sql_ast_ddl_table_option option)
{
    switch (option) {
    case MYLITE_SQL_AST_DDL_TABLE_OPTION_NONE:
        return "none";
    case MYLITE_SQL_AST_DDL_TABLE_OPTION_ALGORITHM:
        return "algorithm";
    case MYLITE_SQL_AST_DDL_TABLE_OPTION_LOCK:
        return "lock";
    }

    return "unknown";
}

const char *mylite_sql_ast_alter_table_action_name(enum mylite_sql_ast_alter_table_action action)
{
    switch (action) {
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_NONE:
        return "none";
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_ADD_COLUMN:
        return "add_column";
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_DROP_COLUMN:
        return "drop_column";
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_RENAME_COLUMN:
        return "rename_column";
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_CHANGE_COLUMN:
        return "change_column";
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_MODIFY_COLUMN:
        return "modify_column";
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_ADD_PRIMARY_KEY:
        return "add_primary_key";
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_DROP_PRIMARY_KEY:
        return "drop_primary_key";
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_ADD_UNIQUE_INDEX:
        return "add_unique_index";
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_ADD_SECONDARY_INDEX:
        return "add_secondary_index";
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_ADD_FULLTEXT_INDEX:
        return "add_fulltext_index";
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_ADD_SPATIAL_INDEX:
        return "add_spatial_index";
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_DROP_INDEX:
        return "drop_index";
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_RENAME_INDEX:
        return "rename_index";
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_ALTER_INDEX_VISIBILITY:
        return "alter_index_visibility";
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_ADD_CHECK:
        return "add_check";
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_DROP_CHECK_OR_CONSTRAINT:
        return "drop_check_or_constraint";
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_ALTER_CHECK_OR_CONSTRAINT:
        return "alter_check_or_constraint";
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_ADD_FOREIGN_KEY:
        return "add_foreign_key";
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_DROP_FOREIGN_KEY:
        return "drop_foreign_key";
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_RENAME_TABLE:
        return "rename_table";
    }

    return "unknown";
}

const char *mylite_sql_ast_alter_table_index_spelling_name(
    enum mylite_sql_ast_alter_table_index_spelling spelling)
{
    switch (spelling) {
    case MYLITE_SQL_AST_ALTER_TABLE_INDEX_SPELLING_NONE:
        return "none";
    case MYLITE_SQL_AST_ALTER_TABLE_INDEX_SPELLING_INDEX:
        return "index";
    case MYLITE_SQL_AST_ALTER_TABLE_INDEX_SPELLING_KEY:
        return "key";
    }

    return "unknown";
}

const char *mylite_sql_ast_alter_table_constraint_spelling_name(
    enum mylite_sql_ast_alter_table_constraint_spelling spelling)
{
    switch (spelling) {
    case MYLITE_SQL_AST_ALTER_TABLE_CONSTRAINT_SPELLING_NONE:
        return "none";
    case MYLITE_SQL_AST_ALTER_TABLE_CONSTRAINT_SPELLING_CHECK:
        return "check";
    case MYLITE_SQL_AST_ALTER_TABLE_CONSTRAINT_SPELLING_CONSTRAINT:
        return "constraint";
    }

    return "unknown";
}

const char *
mylite_sql_ast_constraint_enforcement_name(enum mylite_sql_ast_constraint_enforcement enforcement)
{
    switch (enforcement) {
    case MYLITE_SQL_AST_CONSTRAINT_ENFORCEMENT_DEFAULT:
        return "default";
    case MYLITE_SQL_AST_CONSTRAINT_ENFORCEMENT_ENFORCED:
        return "enforced";
    case MYLITE_SQL_AST_CONSTRAINT_ENFORCEMENT_NOT_ENFORCED:
        return "not_enforced";
    }

    return "unknown";
}

const char *mylite_sql_ast_reference_option_name(enum mylite_sql_ast_reference_option option)
{
    switch (option) {
    case MYLITE_SQL_AST_REFERENCE_OPTION_NONE:
        return "none";
    case MYLITE_SQL_AST_REFERENCE_OPTION_ON_DELETE:
        return "on_delete";
    case MYLITE_SQL_AST_REFERENCE_OPTION_ON_UPDATE:
        return "on_update";
    case MYLITE_SQL_AST_REFERENCE_OPTION_MATCH:
        return "match";
    }

    return "unknown";
}

const char *mylite_sql_ast_reference_action_name(enum mylite_sql_ast_reference_action action)
{
    switch (action) {
    case MYLITE_SQL_AST_REFERENCE_ACTION_NONE:
        return "none";
    case MYLITE_SQL_AST_REFERENCE_ACTION_RESTRICT:
        return "restrict";
    case MYLITE_SQL_AST_REFERENCE_ACTION_CASCADE:
        return "cascade";
    case MYLITE_SQL_AST_REFERENCE_ACTION_SET_NULL:
        return "set_null";
    case MYLITE_SQL_AST_REFERENCE_ACTION_NO_ACTION:
        return "no_action";
    case MYLITE_SQL_AST_REFERENCE_ACTION_SET_DEFAULT:
        return "set_default";
    }

    return "unknown";
}

const char *mylite_sql_ast_reference_match_name(enum mylite_sql_ast_reference_match match)
{
    switch (match) {
    case MYLITE_SQL_AST_REFERENCE_MATCH_NONE:
        return "none";
    case MYLITE_SQL_AST_REFERENCE_MATCH_SIMPLE:
        return "simple";
    case MYLITE_SQL_AST_REFERENCE_MATCH_FULL:
        return "full";
    case MYLITE_SQL_AST_REFERENCE_MATCH_PARTIAL:
        return "partial";
    }

    return "unknown";
}

const char *mylite_sql_ast_alter_table_column_position_name(
    enum mylite_sql_ast_alter_table_column_position position)
{
    switch (position) {
    case MYLITE_SQL_AST_ALTER_TABLE_COLUMN_POSITION_NONE:
        return "none";
    case MYLITE_SQL_AST_ALTER_TABLE_COLUMN_POSITION_FIRST:
        return "first";
    case MYLITE_SQL_AST_ALTER_TABLE_COLUMN_POSITION_AFTER:
        return "after";
    }

    return "unknown";
}

const char *mylite_sql_ast_aggregate_kind_name(enum mylite_sql_ast_aggregate_kind aggregate_kind)
{
    switch (aggregate_kind) {
    case MYLITE_SQL_AST_AGGREGATE_NONE:
        return "none";
    case MYLITE_SQL_AST_AGGREGATE_COUNT:
        return "count";
    case MYLITE_SQL_AST_AGGREGATE_SUM:
        return "sum";
    case MYLITE_SQL_AST_AGGREGATE_AVG:
        return "avg";
    case MYLITE_SQL_AST_AGGREGATE_MIN:
        return "min";
    case MYLITE_SQL_AST_AGGREGATE_MAX:
        return "max";
    case MYLITE_SQL_AST_AGGREGATE_GROUP_CONCAT:
        return "group_concat";
    }

    return "unknown";
}

const char *
mylite_sql_ast_aggregate_argument_name(enum mylite_sql_ast_aggregate_argument aggregate_argument)
{
    switch (aggregate_argument) {
    case MYLITE_SQL_AST_AGGREGATE_ARGUMENT_NONE:
        return "none";
    case MYLITE_SQL_AST_AGGREGATE_ARGUMENT_STAR:
        return "star";
    case MYLITE_SQL_AST_AGGREGATE_ARGUMENT_EXPRESSION:
        return "expression";
    case MYLITE_SQL_AST_AGGREGATE_ARGUMENT_DISTINCT_EXPRESSION_LIST:
        return "distinct expression list";
    case MYLITE_SQL_AST_AGGREGATE_ARGUMENT_EXPRESSION_LIST:
        return "expression list";
    }

    return "unknown";
}

const char *mylite_sql_ast_join_type_name(enum mylite_sql_ast_join_type join_type)
{
    switch (join_type) {
    case MYLITE_SQL_AST_JOIN_NONE:
        return "none";
    case MYLITE_SQL_AST_JOIN_INNER:
        return "inner";
    case MYLITE_SQL_AST_JOIN_CROSS:
        return "cross";
    case MYLITE_SQL_AST_JOIN_COMMA:
        return "comma";
    case MYLITE_SQL_AST_JOIN_LEFT:
        return "left";
    case MYLITE_SQL_AST_JOIN_RIGHT:
        return "right";
    }

    return "unknown";
}

const char *
mylite_sql_ast_join_condition_type_name(enum mylite_sql_ast_join_condition_type condition_type)
{
    switch (condition_type) {
    case MYLITE_SQL_AST_JOIN_CONDITION_NONE:
        return "none";
    case MYLITE_SQL_AST_JOIN_CONDITION_ON:
        return "on";
    case MYLITE_SQL_AST_JOIN_CONDITION_USING:
        return "using";
    }

    return "unknown";
}

const char *
mylite_sql_ast_select_duplicate_mode_name(enum mylite_sql_ast_select_duplicate_mode mode)
{
    switch (mode) {
    case MYLITE_SQL_AST_SELECT_DUPLICATES_IMPLICIT_ALL:
        return "implicit_all";
    case MYLITE_SQL_AST_SELECT_DUPLICATES_ALL:
        return "all";
    case MYLITE_SQL_AST_SELECT_DUPLICATES_DISTINCT:
        return "distinct";
    }

    return "unknown";
}

const char *mylite_sql_ast_set_duplicate_mode_name(enum mylite_sql_ast_set_duplicate_mode mode)
{
    switch (mode) {
    case MYLITE_SQL_AST_SET_DUPLICATES_DISTINCT:
        return "distinct";
    case MYLITE_SQL_AST_SET_DUPLICATES_ALL:
        return "all";
    }

    return "unknown";
}

const char *
mylite_sql_ast_subquery_quantifier_name(enum mylite_sql_ast_subquery_quantifier quantifier)
{
    switch (quantifier) {
    case MYLITE_SQL_AST_SUBQUERY_QUANTIFIER_NONE:
        return "none";
    case MYLITE_SQL_AST_SUBQUERY_QUANTIFIER_ANY:
        return "any";
    case MYLITE_SQL_AST_SUBQUERY_QUANTIFIER_SOME:
        return "some";
    case MYLITE_SQL_AST_SUBQUERY_QUANTIFIER_ALL:
        return "all";
    }

    return "unknown";
}

const char *mylite_sql_ast_literal_kind_name(enum mylite_sql_ast_literal_kind kind)
{
    switch (kind) {
    case MYLITE_SQL_AST_LITERAL_NONE:
        return "none";
    case MYLITE_SQL_AST_LITERAL_INTEGER:
        return "integer";
    case MYLITE_SQL_AST_LITERAL_DECIMAL:
        return "decimal";
    case MYLITE_SQL_AST_LITERAL_FLOAT:
        return "float";
    case MYLITE_SQL_AST_LITERAL_STRING:
        return "string";
    case MYLITE_SQL_AST_LITERAL_NATIONAL_STRING:
        return "national_string";
    case MYLITE_SQL_AST_LITERAL_HEX:
        return "hex";
    case MYLITE_SQL_AST_LITERAL_BIT:
        return "bit";
    case MYLITE_SQL_AST_LITERAL_TRUE:
        return "true";
    case MYLITE_SQL_AST_LITERAL_FALSE:
        return "false";
    case MYLITE_SQL_AST_LITERAL_NULL:
        return "null";
    }

    return "unknown";
}

const char *mylite_sql_ast_operator_name(enum mylite_sql_ast_operator operator_kind)
{
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_NONE:
        return "none";
    case MYLITE_SQL_AST_OPERATOR_POSITIVE:
        return "positive";
    case MYLITE_SQL_AST_OPERATOR_NEGATIVE:
        return "negative";
    case MYLITE_SQL_AST_OPERATOR_ADD:
        return "add";
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
        return "subtract";
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
        return "multiply";
    case MYLITE_SQL_AST_OPERATOR_DIVIDE:
        return "divide";
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
        return "equal";
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
        return "null_safe_equal";
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
        return "not_equal";
    case MYLITE_SQL_AST_OPERATOR_LESS:
        return "less";
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
        return "less_equal";
    case MYLITE_SQL_AST_OPERATOR_GREATER:
        return "greater";
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
        return "greater_equal";
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT:
        return "logical_not";
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_AND:
        return "logical_and";
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR:
        return "logical_xor";
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_OR:
        return "logical_or";
    case MYLITE_SQL_AST_OPERATOR_BITWISE_NOT:
        return "bitwise_not";
    case MYLITE_SQL_AST_OPERATOR_BITWISE_AND:
        return "bitwise_and";
    case MYLITE_SQL_AST_OPERATOR_BITWISE_XOR:
        return "bitwise_xor";
    case MYLITE_SQL_AST_OPERATOR_BITWISE_OR:
        return "bitwise_or";
    case MYLITE_SQL_AST_OPERATOR_SHIFT_LEFT:
        return "shift_left";
    case MYLITE_SQL_AST_OPERATOR_SHIFT_RIGHT:
        return "shift_right";
    case MYLITE_SQL_AST_OPERATOR_IS_NULL:
        return "is_null";
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL:
        return "is_not_null";
    case MYLITE_SQL_AST_OPERATOR_IS_TRUE:
        return "is_true";
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE:
        return "is_not_true";
    case MYLITE_SQL_AST_OPERATOR_IS_FALSE:
        return "is_false";
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE:
        return "is_not_false";
    case MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN:
        return "is_unknown";
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN:
        return "is_not_unknown";
    case MYLITE_SQL_AST_OPERATOR_BETWEEN:
        return "between";
    case MYLITE_SQL_AST_OPERATOR_NOT_BETWEEN:
        return "not_between";
    case MYLITE_SQL_AST_OPERATOR_LIKE:
        return "like";
    case MYLITE_SQL_AST_OPERATOR_NOT_LIKE:
        return "not_like";
    case MYLITE_SQL_AST_OPERATOR_REGEXP:
        return "regexp";
    case MYLITE_SQL_AST_OPERATOR_NOT_REGEXP:
        return "not_regexp";
    case MYLITE_SQL_AST_OPERATOR_JSON_EXTRACT:
        return "json_extract";
    case MYLITE_SQL_AST_OPERATOR_JSON_UNQUOTE_EXTRACT:
        return "json_unquote_extract";
    case MYLITE_SQL_AST_OPERATOR_IN:
        return "in";
    case MYLITE_SQL_AST_OPERATOR_NOT_IN:
        return "not_in";
    case MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE:
        return "integer_divide";
    case MYLITE_SQL_AST_OPERATOR_MODULO:
        return "modulo";
    }

    return "unknown";
}
