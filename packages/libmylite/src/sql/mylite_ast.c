#include "mylite_ast.h"

#include <stdlib.h>

void mylite_sql_ast_init(struct mylite_sql_ast *ast) {
    if (ast == NULL) {
        return;
    }

    ast->first_allocated = NULL;
}

void mylite_sql_ast_deinit(struct mylite_sql_ast *ast) {
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

struct mylite_sql_ast_node *mylite_sql_ast_new_node(
    struct mylite_sql_ast *ast,
    enum mylite_sql_ast_node_kind kind,
    struct mylite_sql_source_span span
) {
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

void mylite_sql_ast_node_append_child(
    struct mylite_sql_ast_node *parent,
    struct mylite_sql_ast_node *child
) {
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

void mylite_sql_ast_node_set_span(
    struct mylite_sql_ast_node *node,
    struct mylite_sql_source_span span
) {
    if (node == NULL) {
        return;
    }

    node->span = span;
}

void mylite_sql_ast_node_set_select_modifier(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_select_modifier modifier
) {
    if (node == NULL) {
        return;
    }

    node->payload.select.modifier = modifier;
}

void mylite_sql_ast_node_set_select_options(
    struct mylite_sql_ast_node *node,
    unsigned int options
) {
    if (node == NULL) {
        return;
    }

    node->payload.select.options = options;
}

void mylite_sql_ast_node_set_select_calc_found_rows(
    struct mylite_sql_ast_node *node,
    int calc_found_rows
) {
    if (node == NULL) {
        return;
    }

    node->payload.select.calc_found_rows = calc_found_rows != 0;
}

void mylite_sql_ast_node_set_select_locking_clause(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_select_locking_clause locking_clause
) {
    if (node == NULL) {
        return;
    }

    node->payload.select.locking_clause = locking_clause;
}

void mylite_sql_ast_node_set_literal_kind(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_literal_kind literal_kind
) {
    if (node == NULL) {
        return;
    }

    node->payload.literal.kind = literal_kind;
}

void mylite_sql_ast_node_set_operator(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_operator operator_kind
) {
    if (node == NULL) {
        return;
    }

    node->payload.expression.operator_kind = operator_kind;
}

void mylite_sql_ast_node_set_integer_type(
    struct mylite_sql_ast_node *node,
    struct mylite_sql_ast_integer_type_payload payload
) {
    if (node == NULL) {
        return;
    }

    node->payload.integer_type = payload;
}

void mylite_sql_ast_node_set_varchar_type(
    struct mylite_sql_ast_node *node,
    struct mylite_sql_ast_varchar_type_payload payload
) {
    if (node == NULL) {
        return;
    }

    node->payload.varchar_type = payload;
}

void mylite_sql_ast_node_set_nullability(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_nullability nullability
) {
    if (node == NULL) {
        return;
    }

    node->payload.nullability.kind = nullability;
}

void mylite_sql_ast_node_set_order_direction(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_order_direction direction
) {
    if (node == NULL) {
        return;
    }

    node->payload.order_direction.kind = direction;
}

void mylite_sql_ast_node_set_column_visibility(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_column_visibility visibility
) {
    if (node == NULL) {
        return;
    }

    node->payload.column_visibility.kind = visibility;
}

size_t mylite_sql_ast_node_child_count(const struct mylite_sql_ast_node *node) {
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

enum mylite_sql_ast_select_modifier mylite_sql_ast_node_select_modifier(
    const struct mylite_sql_ast_node *node
) {
    if (node == NULL || node->kind != MYLITE_SQL_AST_SELECT_STATEMENT) {
        return MYLITE_SQL_AST_SELECT_MODIFIER_DEFAULT;
    }

    return node->payload.select.modifier;
}

unsigned int mylite_sql_ast_node_select_options(const struct mylite_sql_ast_node *node) {
    if (node == NULL || node->kind != MYLITE_SQL_AST_SELECT_STATEMENT) {
        return 0U;
    }

    return node->payload.select.options;
}

int mylite_sql_ast_node_select_calc_found_rows(const struct mylite_sql_ast_node *node) {
    if (node == NULL || node->kind != MYLITE_SQL_AST_SELECT_STATEMENT) {
        return 0;
    }

    return node->payload.select.calc_found_rows;
}

enum mylite_sql_ast_select_locking_clause mylite_sql_ast_node_select_locking_clause(
    const struct mylite_sql_ast_node *node
) {
    if (node == NULL || node->kind != MYLITE_SQL_AST_SELECT_STATEMENT) {
        return MYLITE_SQL_AST_SELECT_LOCKING_CLAUSE_NONE;
    }

    return node->payload.select.locking_clause;
}

enum mylite_sql_ast_literal_kind mylite_sql_ast_node_literal_kind(
    const struct mylite_sql_ast_node *node
) {
    if (node == NULL || node->kind != MYLITE_SQL_AST_LITERAL) {
        return MYLITE_SQL_AST_LITERAL_NONE;
    }

    return node->payload.literal.kind;
}

enum mylite_sql_ast_operator mylite_sql_ast_node_operator(const struct mylite_sql_ast_node *node) {
    if (node == NULL ||
        (node->kind != MYLITE_SQL_AST_UNARY_EXPRESSION &&
         node->kind != MYLITE_SQL_AST_BINARY_EXPRESSION &&
         node->kind != MYLITE_SQL_AST_COMPARISON_PREDICATE &&
         node->kind != MYLITE_SQL_AST_IS_NULL_PREDICATE &&
         node->kind != MYLITE_SQL_AST_IS_BOOLEAN_PREDICATE &&
         node->kind != MYLITE_SQL_AST_AND_PREDICATE && node->kind != MYLITE_SQL_AST_OR_PREDICATE &&
         node->kind != MYLITE_SQL_AST_XOR_PREDICATE &&
         node->kind != MYLITE_SQL_AST_NOT_PREDICATE)) {
        return MYLITE_SQL_AST_OPERATOR_NONE;
    }

    return node->payload.expression.operator_kind;
}

enum mylite_sql_ast_integer_type mylite_sql_ast_node_integer_type(
    const struct mylite_sql_ast_node *node
) {
    if (node == NULL || node->kind != MYLITE_SQL_AST_INTEGER_TYPE) {
        return MYLITE_SQL_AST_INTEGER_TYPE_NONE;
    }

    return node->payload.integer_type.kind;
}

int mylite_sql_ast_node_integer_type_is_unsigned(const struct mylite_sql_ast_node *node) {
    if (node == NULL || node->kind != MYLITE_SQL_AST_INTEGER_TYPE) {
        return 0;
    }

    return node->payload.integer_type.is_unsigned;
}

int mylite_sql_ast_node_integer_type_has_display_width(const struct mylite_sql_ast_node *node) {
    if (node == NULL || node->kind != MYLITE_SQL_AST_INTEGER_TYPE) {
        return 0;
    }

    return node->payload.integer_type.has_display_width;
}

int mylite_sql_ast_node_integer_type_is_bool_alias(const struct mylite_sql_ast_node *node) {
    if (node == NULL || node->kind != MYLITE_SQL_AST_INTEGER_TYPE) {
        return 0;
    }

    return node->payload.integer_type.is_bool_alias;
}

struct mylite_sql_source_span mylite_sql_ast_node_integer_type_display_width_span(
    const struct mylite_sql_ast_node *node
) {
    if (node == NULL || node->kind != MYLITE_SQL_AST_INTEGER_TYPE) {
        return (struct mylite_sql_source_span){0};
    }

    return node->payload.integer_type.display_width_span;
}

struct mylite_sql_source_span mylite_sql_ast_node_varchar_type_length_span(
    const struct mylite_sql_ast_node *node
) {
    if (node == NULL || node->kind != MYLITE_SQL_AST_VARCHAR_TYPE) {
        return (struct mylite_sql_source_span){0};
    }

    return node->payload.varchar_type.length_span;
}

enum mylite_sql_ast_nullability mylite_sql_ast_node_nullability(
    const struct mylite_sql_ast_node *node
) {
    if (node == NULL || node->kind != MYLITE_SQL_AST_NULLABILITY) {
        return MYLITE_SQL_AST_NULLABILITY_UNSPECIFIED;
    }

    return node->payload.nullability.kind;
}

enum mylite_sql_ast_order_direction mylite_sql_ast_node_order_direction(
    const struct mylite_sql_ast_node *node
) {
    if (node == NULL || node->kind != MYLITE_SQL_AST_ORDER_DIRECTION) {
        return MYLITE_SQL_AST_ORDER_DIRECTION_DEFAULT;
    }

    return node->payload.order_direction.kind;
}

enum mylite_sql_ast_column_visibility mylite_sql_ast_node_column_visibility(
    const struct mylite_sql_ast_node *node
) {
    if (node == NULL || node->kind != MYLITE_SQL_AST_ALTER_TABLE_COLUMN_VISIBILITY_STATEMENT) {
        return MYLITE_SQL_AST_COLUMN_VISIBILITY_VISIBLE;
    }

    return node->payload.column_visibility.kind;
}

const char *mylite_sql_ast_node_kind_name(enum mylite_sql_ast_node_kind kind) {
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
    case MYLITE_SQL_AST_CREATE_TABLE_STATEMENT:
        return "create_table_statement";
    case MYLITE_SQL_AST_CREATE_TABLE_LIKE_STATEMENT:
        return "create_table_like_statement";
    case MYLITE_SQL_AST_CREATE_TABLE_SELECT_STATEMENT:
        return "create_table_select_statement";
    case MYLITE_SQL_AST_DROP_TABLE_STATEMENT:
        return "drop_table_statement";
    case MYLITE_SQL_AST_SHOW_TABLES_STATEMENT:
        return "show_tables_statement";
    case MYLITE_SQL_AST_COLUMN_DEFINITION_LIST:
        return "column_definition_list";
    case MYLITE_SQL_AST_COLUMN_DEFINITION:
        return "column_definition";
    case MYLITE_SQL_AST_INTEGER_TYPE:
        return "integer_type";
    case MYLITE_SQL_AST_VARCHAR_TYPE:
        return "varchar_type";
    case MYLITE_SQL_AST_PRIMARY_KEY_DEFINITION:
        return "primary_key_definition";
    case MYLITE_SQL_AST_PRIMARY_KEY_PART_LIST:
        return "primary_key_part_list";
    case MYLITE_SQL_AST_INLINE_PRIMARY_KEY:
        return "inline_primary_key";
    case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_LIST:
        return "column_attribute_list";
    case MYLITE_SQL_AST_COLUMN_AUTO_INCREMENT:
        return "column_auto_increment";
    case MYLITE_SQL_AST_TABLE_AUTO_INCREMENT_OPTION:
        return "table_auto_increment_option";
    case MYLITE_SQL_AST_NULLABILITY:
        return "nullability";
    case MYLITE_SQL_AST_COLUMN_DEFAULT_NULL:
        return "column_default_null";
    case MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE:
        return "column_default_value";
    case MYLITE_SQL_AST_DML_DEFAULT_VALUE:
        return "dml_default_value";
    case MYLITE_SQL_AST_IF_FUNCTION:
        return "if_function";
    case MYLITE_SQL_AST_IFNULL_FUNCTION:
        return "ifnull_function";
    case MYLITE_SQL_AST_IFNULL_ARGUMENT_COUNT_ERROR:
        return "ifnull_argument_count_error";
    case MYLITE_SQL_AST_COALESCE_FUNCTION:
        return "coalesce_function";
    case MYLITE_SQL_AST_NULLIF_FUNCTION:
        return "nullif_function";
    case MYLITE_SQL_AST_NULLIF_ARGUMENT_COUNT_ERROR:
        return "nullif_argument_count_error";
    case MYLITE_SQL_AST_ISNULL_FUNCTION:
        return "isnull_function";
    case MYLITE_SQL_AST_ISNULL_ARGUMENT_COUNT_ERROR:
        return "isnull_argument_count_error";
    case MYLITE_SQL_AST_MOD_FUNCTION:
        return "mod_function";
    case MYLITE_SQL_AST_BIT_COUNT_FUNCTION:
        return "bit_count_function";
    case MYLITE_SQL_AST_BIT_COUNT_ARGUMENT_COUNT_ERROR:
        return "bit_count_argument_count_error";
    case MYLITE_SQL_AST_ABS_FUNCTION:
        return "abs_function";
    case MYLITE_SQL_AST_ABS_ARGUMENT_COUNT_ERROR:
        return "abs_argument_count_error";
    case MYLITE_SQL_AST_SIGN_FUNCTION:
        return "sign_function";
    case MYLITE_SQL_AST_SIGN_ARGUMENT_COUNT_ERROR:
        return "sign_argument_count_error";
    case MYLITE_SQL_AST_CEIL_FUNCTION:
        return "ceil_function";
    case MYLITE_SQL_AST_CEIL_ARGUMENT_COUNT_ERROR:
        return "ceil_argument_count_error";
    case MYLITE_SQL_AST_CEILING_FUNCTION:
        return "ceiling_function";
    case MYLITE_SQL_AST_CEILING_ARGUMENT_COUNT_ERROR:
        return "ceiling_argument_count_error";
    case MYLITE_SQL_AST_FLOOR_FUNCTION:
        return "floor_function";
    case MYLITE_SQL_AST_FLOOR_ARGUMENT_COUNT_ERROR:
        return "floor_argument_count_error";
    case MYLITE_SQL_AST_ROUND_FUNCTION:
        return "round_function";
    case MYLITE_SQL_AST_ROUND_ARGUMENT_COUNT_ERROR:
        return "round_argument_count_error";
    case MYLITE_SQL_AST_BIN_FUNCTION:
        return "bin_function";
    case MYLITE_SQL_AST_BIN_ARGUMENT_COUNT_ERROR:
        return "bin_argument_count_error";
    case MYLITE_SQL_AST_OCT_FUNCTION:
        return "oct_function";
    case MYLITE_SQL_AST_OCT_ARGUMENT_COUNT_ERROR:
        return "oct_argument_count_error";
    case MYLITE_SQL_AST_CONV_FUNCTION:
        return "conv_function";
    case MYLITE_SQL_AST_CONV_ARGUMENT_COUNT_ERROR:
        return "conv_argument_count_error";
    case MYLITE_SQL_AST_PI_FUNCTION:
        return "pi_function";
    case MYLITE_SQL_AST_PI_ARGUMENT_COUNT_ERROR:
        return "pi_argument_count_error";
    case MYLITE_SQL_AST_SQRT_FUNCTION:
        return "sqrt_function";
    case MYLITE_SQL_AST_SQRT_ARGUMENT_COUNT_ERROR:
        return "sqrt_argument_count_error";
    case MYLITE_SQL_AST_DEGREES_FUNCTION:
        return "degrees_function";
    case MYLITE_SQL_AST_DEGREES_ARGUMENT_COUNT_ERROR:
        return "degrees_argument_count_error";
    case MYLITE_SQL_AST_RADIANS_FUNCTION:
        return "radians_function";
    case MYLITE_SQL_AST_RADIANS_ARGUMENT_COUNT_ERROR:
        return "radians_argument_count_error";
    case MYLITE_SQL_AST_ACOS_FUNCTION:
        return "acos_function";
    case MYLITE_SQL_AST_ACOS_ARGUMENT_COUNT_ERROR:
        return "acos_argument_count_error";
    case MYLITE_SQL_AST_ASIN_FUNCTION:
        return "asin_function";
    case MYLITE_SQL_AST_ASIN_ARGUMENT_COUNT_ERROR:
        return "asin_argument_count_error";
    case MYLITE_SQL_AST_ATAN_FUNCTION:
        return "atan_function";
    case MYLITE_SQL_AST_ATAN_ARGUMENT_COUNT_ERROR:
        return "atan_argument_count_error";
    case MYLITE_SQL_AST_ATAN2_FUNCTION:
        return "atan2_function";
    case MYLITE_SQL_AST_ATAN2_ARGUMENT_COUNT_ERROR:
        return "atan2_argument_count_error";
    case MYLITE_SQL_AST_SEARCHED_CASE_EXPRESSION:
        return "searched_case_expression";
    case MYLITE_SQL_AST_SIMPLE_CASE_EXPRESSION:
        return "simple_case_expression";
    case MYLITE_SQL_AST_CASE_WHEN_LIST:
        return "case_when_list";
    case MYLITE_SQL_AST_CASE_WHEN_CLAUSE:
        return "case_when_clause";
    case MYLITE_SQL_AST_CASE_ELSE_CLAUSE:
        return "case_else_clause";
    case MYLITE_SQL_AST_DO_EXPRESSION_LIST:
        return "do_expression_list";
    case MYLITE_SQL_AST_DO_STATEMENT:
        return "do_statement";
    case MYLITE_SQL_AST_SET_SYSTEM_VARIABLE_STATEMENT:
        return "set_system_variable_statement";
    case MYLITE_SQL_AST_SET_SYSTEM_VARIABLE_TARGET:
        return "set_system_variable_target";
    case MYLITE_SQL_AST_SET_DEFAULT_VALUE:
        return "set_default_value";
    case MYLITE_SQL_AST_SHOW_VARIABLES_STATEMENT:
        return "show_variables_statement";
    case MYLITE_SQL_AST_RENAME_TABLE_STATEMENT:
        return "rename_table_statement";
    case MYLITE_SQL_AST_INSERT_STATEMENT:
        return "insert_statement";
    case MYLITE_SQL_AST_INSERT_SELECT_STATEMENT:
        return "insert_select_statement";
    case MYLITE_SQL_AST_REPLACE_SELECT_STATEMENT:
        return "replace_select_statement";
    case MYLITE_SQL_AST_REPLACE_LOW_PRIORITY_MODIFIER:
        return "replace_low_priority_modifier";
    case MYLITE_SQL_AST_REPLACE_DELAYED_MODIFIER:
        return "replace_delayed_modifier";
    case MYLITE_SQL_AST_INSERT_LOW_PRIORITY_MODIFIER:
        return "insert_low_priority_modifier";
    case MYLITE_SQL_AST_INSERT_HIGH_PRIORITY_MODIFIER:
        return "insert_high_priority_modifier";
    case MYLITE_SQL_AST_INSERT_DELAYED_MODIFIER:
        return "insert_delayed_modifier";
    case MYLITE_SQL_AST_INSERT_IGNORE_MODIFIER:
        return "insert_ignore_modifier";
    case MYLITE_SQL_AST_REPLACE_VALUES_STATEMENT:
        return "replace_values_statement";
    case MYLITE_SQL_AST_REPLACE_SET_STATEMENT:
        return "replace_set_statement";
    case MYLITE_SQL_AST_SET_NAMES_STATEMENT:
        return "set_names_statement";
    case MYLITE_SQL_AST_SET_CHARACTER_SET_STATEMENT:
        return "set_character_set_statement";
    case MYLITE_SQL_AST_SET_CHARACTER_SET_DEFAULT_TARGET:
        return "set_character_set_default_target";
    case MYLITE_SQL_AST_ALTER_TABLE_DEFAULT_CHARSET_COLLATION_STATEMENT:
        return "alter_table_default_charset_collation_statement";
    case MYLITE_SQL_AST_ALTER_TABLE_ORDER_BY_STATEMENT:
        return "alter_table_order_by_statement";
    case MYLITE_SQL_AST_ORDER_BY_ITEM_LIST:
        return "order_by_item_list";
    case MYLITE_SQL_AST_ORDER_BY_ITEM:
        return "order_by_item";
    case MYLITE_SQL_AST_ALTER_TABLE_FORCE_STATEMENT:
        return "alter_table_force_statement";
    case MYLITE_SQL_AST_IDENTIFIER_LIST:
        return "identifier_list";
    case MYLITE_SQL_AST_INSERT_ROW_LIST:
        return "insert_row_list";
    case MYLITE_SQL_AST_INSERT_ROW:
        return "insert_row";
    case MYLITE_SQL_AST_FROM_TABLE:
        return "from_table";
    case MYLITE_SQL_AST_WHERE_CLAUSE:
        return "where_clause";
    case MYLITE_SQL_AST_COMPARISON_PREDICATE:
        return "comparison_predicate";
    case MYLITE_SQL_AST_IS_NULL_PREDICATE:
        return "is_null_predicate";
    case MYLITE_SQL_AST_AND_PREDICATE:
        return "and_predicate";
    case MYLITE_SQL_AST_OR_PREDICATE:
        return "or_predicate";
    case MYLITE_SQL_AST_XOR_PREDICATE:
        return "xor_predicate";
    case MYLITE_SQL_AST_NOT_PREDICATE:
        return "not_predicate";
    case MYLITE_SQL_AST_BETWEEN_PREDICATE:
        return "between_predicate";
    case MYLITE_SQL_AST_IN_PREDICATE:
        return "in_predicate";
    case MYLITE_SQL_AST_PREDICATE_VALUE_LIST:
        return "predicate_value_list";
    case MYLITE_SQL_AST_IS_BOOLEAN_PREDICATE:
        return "is_boolean_predicate";
    case MYLITE_SQL_AST_ORDER_BY_CLAUSE:
        return "order_by_clause";
    case MYLITE_SQL_AST_ORDER_DIRECTION:
        return "order_direction";
    case MYLITE_SQL_AST_LIMIT_CLAUSE:
        return "limit_clause";
    case MYLITE_SQL_AST_DELETE_STATEMENT:
        return "delete_statement";
    case MYLITE_SQL_AST_UPDATE_STATEMENT:
        return "update_statement";
    case MYLITE_SQL_AST_UPDATE_ASSIGNMENT_LIST:
        return "update_assignment_list";
    case MYLITE_SQL_AST_UPDATE_ASSIGNMENT:
        return "update_assignment";
    case MYLITE_SQL_AST_TRUNCATE_TABLE_STATEMENT:
        return "truncate_table_statement";
    case MYLITE_SQL_AST_CREATE_SCHEMA_STATEMENT:
        return "create_schema_statement";
    case MYLITE_SQL_AST_DROP_SCHEMA_STATEMENT:
        return "drop_schema_statement";
    case MYLITE_SQL_AST_SHOW_DATABASES_STATEMENT:
        return "show_databases_statement";
    case MYLITE_SQL_AST_DATABASE_FUNCTION:
        return "database_function";
    case MYLITE_SQL_AST_SCHEMA_FUNCTION:
        return "schema_function";
    case MYLITE_SQL_AST_USER_FUNCTION:
        return "user_function";
    case MYLITE_SQL_AST_CURRENT_USER_FUNCTION:
        return "current_user_function";
    case MYLITE_SQL_AST_VERSION_FUNCTION:
        return "version_function";
    case MYLITE_SQL_AST_VERSION_ARGUMENT_COUNT_ERROR:
        return "version_argument_count_error";
    case MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST:
        return "function_argument_list";
    case MYLITE_SQL_AST_ROW_COUNT_FUNCTION:
        return "row_count_function";
    case MYLITE_SQL_AST_FOUND_ROWS_FUNCTION:
        return "found_rows_function";
    case MYLITE_SQL_AST_FOUND_ROWS_ARGUMENT_COUNT_ERROR:
        return "found_rows_argument_count_error";
    case MYLITE_SQL_AST_LAST_INSERT_ID_FUNCTION:
        return "last_insert_id_function";
    case MYLITE_SQL_AST_MIN_AGGREGATE_FUNCTION:
        return "min_aggregate_function";
    case MYLITE_SQL_AST_MAX_AGGREGATE_FUNCTION:
        return "max_aggregate_function";
    case MYLITE_SQL_AST_SUM_AGGREGATE_FUNCTION:
        return "sum_aggregate_function";
    case MYLITE_SQL_AST_AVG_AGGREGATE_FUNCTION:
        return "avg_aggregate_function";
    case MYLITE_SQL_AST_BIT_AND_AGGREGATE_FUNCTION:
        return "bit_and_aggregate_function";
    case MYLITE_SQL_AST_BIT_OR_AGGREGATE_FUNCTION:
        return "bit_or_aggregate_function";
    case MYLITE_SQL_AST_BIT_XOR_AGGREGATE_FUNCTION:
        return "bit_xor_aggregate_function";
    case MYLITE_SQL_AST_GROUP_BY_CLAUSE:
        return "group_by_clause";
    case MYLITE_SQL_AST_HAVING_CLAUSE:
        return "having_clause";
    case MYLITE_SQL_AST_COUNT_COLUMN_FUNCTION:
        return "count_column_function";
    case MYLITE_SQL_AST_COUNT_LITERAL_FUNCTION:
        return "count_literal_function";
    case MYLITE_SQL_AST_COUNT_DISTINCT_COLUMN_FUNCTION:
        return "count_distinct_column_function";
    case MYLITE_SQL_AST_SESSION_USER_FUNCTION:
        return "session_user_function";
    case MYLITE_SQL_AST_SYSTEM_USER_FUNCTION:
        return "system_user_function";
    case MYLITE_SQL_AST_CONNECTION_ID_FUNCTION:
        return "connection_id_function";
    case MYLITE_SQL_AST_CONNECTION_ID_ARGUMENT_COUNT_ERROR:
        return "connection_id_argument_count_error";
    case MYLITE_SQL_AST_COUNT_STAR_FUNCTION:
        return "count_star_function";
    case MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT:
        return "show_columns_statement";
    case MYLITE_SQL_AST_SHOW_CREATE_TABLE_STATEMENT:
        return "show_create_table_statement";
    case MYLITE_SQL_AST_TABLE_ENGINE_OPTION:
        return "table_engine_option";
    case MYLITE_SQL_AST_SHOW_ENGINES_STATEMENT:
        return "show_engines_statement";
    case MYLITE_SQL_AST_TABLE_OPTION_LIST:
        return "table_option_list";
    case MYLITE_SQL_AST_TABLE_CHARSET_OPTION:
        return "table_charset_option";
    case MYLITE_SQL_AST_TABLE_COLLATION_OPTION:
        return "table_collation_option";
    case MYLITE_SQL_AST_INSERT_SET_STATEMENT:
        return "insert_set_statement";
    case MYLITE_SQL_AST_INSERT_ASSIGNMENT_LIST:
        return "insert_assignment_list";
    case MYLITE_SQL_AST_INSERT_ASSIGNMENT:
        return "insert_assignment";
    case MYLITE_SQL_AST_ALTER_TABLE_RENAME_STATEMENT:
        return "alter_table_rename_statement";
    case MYLITE_SQL_AST_SHOW_INDEX_STATEMENT:
        return "show_index_statement";
    case MYLITE_SQL_AST_SHOW_CREATE_DATABASE_STATEMENT:
        return "show_create_database_statement";
    case MYLITE_SQL_AST_SHOW_TABLE_STATUS_STATEMENT:
        return "show_table_status_statement";
    case MYLITE_SQL_AST_SHOW_CHARACTER_SET_STATEMENT:
        return "show_character_set_statement";
    case MYLITE_SQL_AST_SHOW_COLLATION_STATEMENT:
        return "show_collation_statement";
    case MYLITE_SQL_AST_SHOW_TRIGGERS_STATEMENT:
        return "show_triggers_statement";
    case MYLITE_SQL_AST_SHOW_EVENTS_STATEMENT:
        return "show_events_statement";
    case MYLITE_SQL_AST_SHOW_OPEN_TABLES_STATEMENT:
        return "show_open_tables_statement";
    case MYLITE_SQL_AST_SHOW_PROCEDURE_STATUS_STATEMENT:
        return "show_procedure_status_statement";
    case MYLITE_SQL_AST_SHOW_FUNCTION_STATUS_STATEMENT:
        return "show_function_status_statement";
    case MYLITE_SQL_AST_SHOW_PROCESSLIST_STATEMENT:
        return "show_processlist_statement";
    case MYLITE_SQL_AST_SHOW_FULL_PROCESSLIST_STATEMENT:
        return "show_full_processlist_statement";
    case MYLITE_SQL_AST_SHOW_WARNINGS_STATEMENT:
        return "show_warnings_statement";
    case MYLITE_SQL_AST_SHOW_COUNT_WARNINGS_STATEMENT:
        return "show_count_warnings_statement";
    case MYLITE_SQL_AST_SHOW_ERRORS_STATEMENT:
        return "show_errors_statement";
    case MYLITE_SQL_AST_SHOW_COUNT_ERRORS_STATEMENT:
        return "show_count_errors_statement";
    case MYLITE_SQL_AST_SYSTEM_VARIABLE:
        return "system_variable";
    case MYLITE_SQL_AST_CURRENT_ROLE_FUNCTION:
        return "current_role_function";
    case MYLITE_SQL_AST_CURRENT_ROLE_ARGUMENT_COUNT_ERROR:
        return "current_role_argument_count_error";
    case MYLITE_SQL_AST_ALTER_TABLE_ADD_COLUMN_STATEMENT:
        return "alter_table_add_column_statement";
    case MYLITE_SQL_AST_ALTER_TABLE_DROP_COLUMN_STATEMENT:
        return "alter_table_drop_column_statement";
    case MYLITE_SQL_AST_ALTER_TABLE_RENAME_COLUMN_STATEMENT:
        return "alter_table_rename_column_statement";
    case MYLITE_SQL_AST_ALTER_TABLE_MODIFY_COLUMN_STATEMENT:
        return "alter_table_modify_column_statement";
    case MYLITE_SQL_AST_ALTER_TABLE_CHANGE_COLUMN_STATEMENT:
        return "alter_table_change_column_statement";
    case MYLITE_SQL_AST_ALTER_TABLE_SET_DEFAULT_STATEMENT:
        return "alter_table_set_default_statement";
    case MYLITE_SQL_AST_ALTER_TABLE_DROP_DEFAULT_STATEMENT:
        return "alter_table_drop_default_statement";
    case MYLITE_SQL_AST_ALTER_TABLE_COLUMN_VISIBILITY_STATEMENT:
        return "alter_table_column_visibility_statement";
    case MYLITE_SQL_AST_CREATE_IF_NOT_EXISTS_CLAUSE:
        return "create_if_not_exists_clause";
    case MYLITE_SQL_AST_DROP_IF_EXISTS_CLAUSE:
        return "drop_if_exists_clause";
    case MYLITE_SQL_AST_CREATE_SCHEMA_IF_NOT_EXISTS_CLAUSE:
        return "create_schema_if_not_exists_clause";
    case MYLITE_SQL_AST_DROP_SCHEMA_IF_EXISTS_CLAUSE:
        return "drop_schema_if_exists_clause";
    case MYLITE_SQL_AST_TABLE_NAME_LIST:
        return "table_name_list";
    case MYLITE_SQL_AST_RENAME_TABLE_PAIR:
        return "rename_table_pair";
    case MYLITE_SQL_AST_RENAME_TABLE_PAIR_LIST:
        return "rename_table_pair_list";
    }

    return "unknown";
}

const char *mylite_sql_ast_literal_kind_name(enum mylite_sql_ast_literal_kind kind) {
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

const char *mylite_sql_ast_operator_name(enum mylite_sql_ast_operator operator_kind) {
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
    case MYLITE_SQL_AST_OPERATOR_IS_NULL:
        return "is_null";
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL:
        return "is_not_null";
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_AND:
        return "logical_and";
    case MYLITE_SQL_AST_OPERATOR_DEPRECATED_LOGICAL_AND:
        return "deprecated_logical_and";
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_OR:
        return "logical_or";
    case MYLITE_SQL_AST_OPERATOR_DEPRECATED_LOGICAL_OR:
        return "deprecated_logical_or";
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT:
        return "logical_not";
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR:
        return "logical_xor";
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
    case MYLITE_SQL_AST_OPERATOR_MODULO:
        return "modulo";
    case MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE:
        return "integer_divide";
    case MYLITE_SQL_AST_OPERATOR_BITWISE_NOT:
        return "bitwise_not";
    case MYLITE_SQL_AST_OPERATOR_BITWISE_XOR:
        return "bitwise_xor";
    case MYLITE_SQL_AST_OPERATOR_LEFT_SHIFT:
        return "left_shift";
    case MYLITE_SQL_AST_OPERATOR_RIGHT_SHIFT:
        return "right_shift";
    case MYLITE_SQL_AST_OPERATOR_BITWISE_AND:
        return "bitwise_and";
    case MYLITE_SQL_AST_OPERATOR_BITWISE_OR:
        return "bitwise_or";
    }

    return "unknown";
}

const char *mylite_sql_ast_integer_type_name(enum mylite_sql_ast_integer_type integer_type) {
    switch (integer_type) {
    case MYLITE_SQL_AST_INTEGER_TYPE_NONE:
        return "none";
    case MYLITE_SQL_AST_INTEGER_TYPE_INT:
        return "int";
    case MYLITE_SQL_AST_INTEGER_TYPE_BIGINT:
        return "bigint";
    case MYLITE_SQL_AST_INTEGER_TYPE_TINYINT:
        return "tinyint";
    case MYLITE_SQL_AST_INTEGER_TYPE_SMALLINT:
        return "smallint";
    case MYLITE_SQL_AST_INTEGER_TYPE_MEDIUMINT:
        return "mediumint";
    }

    return "unknown";
}

const char *mylite_sql_ast_nullability_name(enum mylite_sql_ast_nullability nullability) {
    switch (nullability) {
    case MYLITE_SQL_AST_NULLABILITY_UNSPECIFIED:
        return "unspecified";
    case MYLITE_SQL_AST_NULLABILITY_NULL:
        return "null";
    case MYLITE_SQL_AST_NULLABILITY_NOT_NULL:
        return "not_null";
    }

    return "unknown";
}

const char *mylite_sql_ast_order_direction_name(enum mylite_sql_ast_order_direction direction) {
    switch (direction) {
    case MYLITE_SQL_AST_ORDER_DIRECTION_DEFAULT:
        return "default";
    case MYLITE_SQL_AST_ORDER_DIRECTION_ASC:
        return "asc";
    case MYLITE_SQL_AST_ORDER_DIRECTION_DESC:
        return "desc";
    }

    return "unknown";
}

const char *mylite_sql_ast_column_visibility_name(
    enum mylite_sql_ast_column_visibility visibility
) {
    switch (visibility) {
    case MYLITE_SQL_AST_COLUMN_VISIBILITY_VISIBLE:
        return "visible";
    case MYLITE_SQL_AST_COLUMN_VISIBILITY_INVISIBLE:
        return "invisible";
    }

    return "unknown";
}
