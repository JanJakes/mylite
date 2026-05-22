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

void mylite_sql_ast_node_set_show_tables_full(struct mylite_sql_ast_node *node, int is_full) {
    if (node == NULL) {
        return;
    }

    node->payload.show_tables.is_full = is_full != 0;
}

void mylite_sql_ast_node_set_union_modifier(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_union_modifier modifier
) {
    if (node == NULL) {
        return;
    }

    node->payload.union_term.modifier = modifier;
}

void mylite_sql_ast_node_set_join_kind(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_join_kind join_kind
) {
    if (node == NULL) {
        return;
    }

    node->payload.join.kind = join_kind;
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

void mylite_sql_ast_node_set_char_type(
    struct mylite_sql_ast_node *node,
    struct mylite_sql_ast_char_type_payload payload
) {
    if (node == NULL) {
        return;
    }

    node->payload.char_type = payload;
}

void mylite_sql_ast_node_set_text_type(
    struct mylite_sql_ast_node *node,
    struct mylite_sql_ast_text_type_payload payload
) {
    if (node == NULL) {
        return;
    }

    node->payload.text_type = payload;
}

void mylite_sql_ast_node_set_binary_string_type(
    struct mylite_sql_ast_node *node,
    struct mylite_sql_ast_binary_string_type_payload payload
) {
    if (node == NULL) {
        return;
    }

    node->payload.binary_string_type = payload;
}

void mylite_sql_ast_node_set_spatial_type(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_spatial_type spatial_type
) {
    if (node == NULL) {
        return;
    }

    node->payload.spatial_type.kind = spatial_type;
}

void mylite_sql_ast_node_set_bit_type(
    struct mylite_sql_ast_node *node,
    struct mylite_sql_ast_bit_type_payload payload
) {
    if (node == NULL) {
        return;
    }

    node->payload.bit_type = payload;
}

void mylite_sql_ast_node_set_year_type(
    struct mylite_sql_ast_node *node,
    struct mylite_sql_ast_year_type_payload payload
) {
    if (node == NULL) {
        return;
    }

    node->payload.year_type = payload;
}

void mylite_sql_ast_node_set_decimal_type(
    struct mylite_sql_ast_node *node,
    struct mylite_sql_ast_decimal_type_payload payload
) {
    if (node == NULL) {
        return;
    }

    node->payload.decimal_type = payload;
}

void mylite_sql_ast_node_set_approximate_type(
    struct mylite_sql_ast_node *node,
    struct mylite_sql_ast_approximate_type_payload payload
) {
    if (node == NULL) {
        return;
    }

    node->payload.approximate_type = payload;
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

    if (node->kind == MYLITE_SQL_AST_ALTER_TABLE_INDEX_VISIBILITY_STATEMENT) {
        node->payload.alter_table_options.visibility = visibility;
        return;
    }

    node->payload.column_visibility.kind = visibility;
}

void mylite_sql_ast_node_set_alter_table_options(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_alter_algorithm algorithm,
    enum mylite_sql_ast_alter_lock lock
) {
    if (node == NULL) {
        return;
    }

    node->payload.alter_table_options.algorithm = algorithm;
    node->payload.alter_table_options.lock = lock;
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

int mylite_sql_ast_node_show_tables_is_full(const struct mylite_sql_ast_node *node) {
    if (node == NULL || node->kind != MYLITE_SQL_AST_SHOW_TABLES_STATEMENT) {
        return 0;
    }

    return node->payload.show_tables.is_full;
}

enum mylite_sql_ast_union_modifier mylite_sql_ast_node_union_modifier(
    const struct mylite_sql_ast_node *node
) {
    if (node == NULL || node->kind != MYLITE_SQL_AST_UNION_TERM) {
        return MYLITE_SQL_AST_UNION_MODIFIER_DISTINCT;
    }

    return node->payload.union_term.modifier;
}

enum mylite_sql_ast_join_kind mylite_sql_ast_node_join_kind(
    const struct mylite_sql_ast_node *node
) {
    if (node == NULL || node->kind != MYLITE_SQL_AST_FROM_JOIN) {
        return MYLITE_SQL_AST_JOIN_KIND_INNER;
    }
    if (node->payload.join.kind == 0) {
        return MYLITE_SQL_AST_JOIN_KIND_INNER;
    }

    return node->payload.join.kind;
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

int mylite_sql_ast_node_integer_type_is_serial_alias(const struct mylite_sql_ast_node *node) {
    if (node == NULL || node->kind != MYLITE_SQL_AST_INTEGER_TYPE) {
        return 0;
    }

    return node->payload.integer_type.is_serial_alias;
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

int mylite_sql_ast_node_varchar_type_is_national(const struct mylite_sql_ast_node *node) {
    if (node == NULL || node->kind != MYLITE_SQL_AST_VARCHAR_TYPE) {
        return 0;
    }

    return node->payload.varchar_type.is_national;
}

int mylite_sql_ast_node_char_type_has_explicit_length(const struct mylite_sql_ast_node *node) {
    if (node == NULL || node->kind != MYLITE_SQL_AST_CHAR_TYPE) {
        return 0;
    }

    return node->payload.char_type.has_explicit_length;
}

int mylite_sql_ast_node_char_type_is_national(const struct mylite_sql_ast_node *node) {
    if (node == NULL || node->kind != MYLITE_SQL_AST_CHAR_TYPE) {
        return 0;
    }

    return node->payload.char_type.is_national;
}

struct mylite_sql_source_span mylite_sql_ast_node_char_type_length_span(
    const struct mylite_sql_ast_node *node
) {
    if (node == NULL || node->kind != MYLITE_SQL_AST_CHAR_TYPE) {
        return (struct mylite_sql_source_span){0};
    }

    return node->payload.char_type.length_span;
}

enum mylite_sql_ast_text_type mylite_sql_ast_node_text_type(
    const struct mylite_sql_ast_node *node
) {
    if (node == NULL || node->kind != MYLITE_SQL_AST_TEXT_TYPE) {
        return MYLITE_SQL_AST_TEXT_TYPE_NONE;
    }

    return node->payload.text_type.kind;
}

int mylite_sql_ast_node_text_type_has_length(const struct mylite_sql_ast_node *node) {
    if (node == NULL || node->kind != MYLITE_SQL_AST_TEXT_TYPE) {
        return 0;
    }

    return node->payload.text_type.has_length;
}

struct mylite_sql_source_span mylite_sql_ast_node_text_type_length_span(
    const struct mylite_sql_ast_node *node
) {
    if (node == NULL || node->kind != MYLITE_SQL_AST_TEXT_TYPE) {
        return (struct mylite_sql_source_span){0};
    }

    return node->payload.text_type.length_span;
}

enum mylite_sql_ast_binary_string_type mylite_sql_ast_node_binary_string_type(
    const struct mylite_sql_ast_node *node
) {
    if (node == NULL || node->kind != MYLITE_SQL_AST_BINARY_STRING_TYPE) {
        return MYLITE_SQL_AST_BINARY_STRING_TYPE_NONE;
    }

    return node->payload.binary_string_type.kind;
}

enum mylite_sql_ast_spatial_type mylite_sql_ast_node_spatial_type(
    const struct mylite_sql_ast_node *node
) {
    if (node == NULL || node->kind != MYLITE_SQL_AST_SPATIAL_TYPE) {
        return MYLITE_SQL_AST_SPATIAL_TYPE_NONE;
    }

    return node->payload.spatial_type.kind;
}

int mylite_sql_ast_node_binary_string_type_has_length(const struct mylite_sql_ast_node *node) {
    if (node == NULL || node->kind != MYLITE_SQL_AST_BINARY_STRING_TYPE) {
        return 0;
    }

    return node->payload.binary_string_type.has_length;
}

struct mylite_sql_source_span mylite_sql_ast_node_binary_string_type_length_span(
    const struct mylite_sql_ast_node *node
) {
    if (node == NULL || node->kind != MYLITE_SQL_AST_BINARY_STRING_TYPE) {
        return (struct mylite_sql_source_span){0};
    }

    return node->payload.binary_string_type.length_span;
}

int mylite_sql_ast_node_bit_type_has_length(const struct mylite_sql_ast_node *node) {
    if (node == NULL || node->kind != MYLITE_SQL_AST_BIT_TYPE) {
        return 0;
    }

    return node->payload.bit_type.has_length;
}

struct mylite_sql_source_span mylite_sql_ast_node_bit_type_length_span(
    const struct mylite_sql_ast_node *node
) {
    if (node == NULL || node->kind != MYLITE_SQL_AST_BIT_TYPE) {
        return (struct mylite_sql_source_span){0};
    }

    return node->payload.bit_type.length_span;
}

int mylite_sql_ast_node_year_type_has_width(const struct mylite_sql_ast_node *node) {
    if (node == NULL || node->kind != MYLITE_SQL_AST_YEAR_TYPE) {
        return 0;
    }

    return node->payload.year_type.has_width;
}

struct mylite_sql_source_span mylite_sql_ast_node_year_type_width_span(
    const struct mylite_sql_ast_node *node
) {
    if (node == NULL || node->kind != MYLITE_SQL_AST_YEAR_TYPE) {
        return (struct mylite_sql_source_span){0};
    }

    return node->payload.year_type.width_span;
}

enum mylite_sql_ast_decimal_type mylite_sql_ast_node_decimal_type(
    const struct mylite_sql_ast_node *node
) {
    if (node == NULL || node->kind != MYLITE_SQL_AST_DECIMAL_TYPE) {
        return MYLITE_SQL_AST_DECIMAL_TYPE_DECIMAL;
    }

    return node->payload.decimal_type.kind;
}

int mylite_sql_ast_node_decimal_type_has_precision(const struct mylite_sql_ast_node *node) {
    if (node == NULL || node->kind != MYLITE_SQL_AST_DECIMAL_TYPE) {
        return 0;
    }

    return node->payload.decimal_type.has_precision;
}

int mylite_sql_ast_node_decimal_type_has_scale(const struct mylite_sql_ast_node *node) {
    if (node == NULL || node->kind != MYLITE_SQL_AST_DECIMAL_TYPE) {
        return 0;
    }

    return node->payload.decimal_type.has_scale;
}

int mylite_sql_ast_node_decimal_type_is_unsigned(const struct mylite_sql_ast_node *node) {
    if (node == NULL || node->kind != MYLITE_SQL_AST_DECIMAL_TYPE) {
        return 0;
    }

    return node->payload.decimal_type.is_unsigned;
}

struct mylite_sql_source_span mylite_sql_ast_node_decimal_type_precision_span(
    const struct mylite_sql_ast_node *node
) {
    if (node == NULL || node->kind != MYLITE_SQL_AST_DECIMAL_TYPE) {
        return (struct mylite_sql_source_span){0};
    }

    return node->payload.decimal_type.precision_span;
}

struct mylite_sql_source_span mylite_sql_ast_node_decimal_type_scale_span(
    const struct mylite_sql_ast_node *node
) {
    if (node == NULL || node->kind != MYLITE_SQL_AST_DECIMAL_TYPE) {
        return (struct mylite_sql_source_span){0};
    }

    return node->payload.decimal_type.scale_span;
}

enum mylite_sql_ast_approximate_type mylite_sql_ast_node_approximate_type(
    const struct mylite_sql_ast_node *node
) {
    if (node == NULL || node->kind != MYLITE_SQL_AST_APPROXIMATE_TYPE) {
        return MYLITE_SQL_AST_APPROXIMATE_TYPE_FLOAT;
    }

    return node->payload.approximate_type.kind;
}

int mylite_sql_ast_node_approximate_type_has_precision(const struct mylite_sql_ast_node *node) {
    if (node == NULL || node->kind != MYLITE_SQL_AST_APPROXIMATE_TYPE) {
        return 0;
    }

    return node->payload.approximate_type.has_precision;
}

int mylite_sql_ast_node_approximate_type_is_unsigned(const struct mylite_sql_ast_node *node) {
    if (node == NULL || node->kind != MYLITE_SQL_AST_APPROXIMATE_TYPE) {
        return 0;
    }

    return node->payload.approximate_type.is_unsigned;
}

struct mylite_sql_source_span mylite_sql_ast_node_approximate_type_precision_span(
    const struct mylite_sql_ast_node *node
) {
    if (node == NULL || node->kind != MYLITE_SQL_AST_APPROXIMATE_TYPE) {
        return (struct mylite_sql_source_span){0};
    }

    return node->payload.approximate_type.precision_span;
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
    if (node == NULL) {
        return MYLITE_SQL_AST_COLUMN_VISIBILITY_VISIBLE;
    }
    if (node->kind == MYLITE_SQL_AST_ALTER_TABLE_INDEX_VISIBILITY_STATEMENT) {
        return node->payload.alter_table_options.visibility;
    }
    if (node->kind != MYLITE_SQL_AST_ALTER_TABLE_COLUMN_VISIBILITY_STATEMENT &&
        node->kind != MYLITE_SQL_AST_INDEX_VISIBILITY_OPTION) {
        return MYLITE_SQL_AST_COLUMN_VISIBILITY_VISIBLE;
    }

    return node->payload.column_visibility.kind;
}

enum mylite_sql_ast_alter_algorithm mylite_sql_ast_node_alter_algorithm(
    const struct mylite_sql_ast_node *node
) {
    if (node == NULL) {
        return MYLITE_SQL_AST_ALTER_ALGORITHM_UNSPECIFIED;
    }

    return node->payload.alter_table_options.algorithm;
}

enum mylite_sql_ast_alter_lock mylite_sql_ast_node_alter_lock(
    const struct mylite_sql_ast_node *node
) {
    if (node == NULL) {
        return MYLITE_SQL_AST_ALTER_LOCK_UNSPECIFIED;
    }

    return node->payload.alter_table_options.lock;
}

const char *mylite_sql_ast_node_kind_name(enum mylite_sql_ast_node_kind kind) {
    switch (kind) {
    case MYLITE_SQL_AST_SCRIPT:
        return "script";
    case MYLITE_SQL_AST_SELECT_STATEMENT:
        return "select_statement";
    case MYLITE_SQL_AST_COMPOUND_SELECT_STATEMENT:
        return "compound_select_statement";
    case MYLITE_SQL_AST_UNION_TERM_LIST:
        return "union_term_list";
    case MYLITE_SQL_AST_UNION_TERM:
        return "union_term";
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
    case MYLITE_SQL_AST_QUALIFIED_WILDCARD:
        return "qualified_wildcard";
    case MYLITE_SQL_AST_LITERAL:
        return "literal";
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
        return "unary_expression";
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
        return "binary_expression";
    case MYLITE_SQL_AST_CAST_BINARY_EXPRESSION:
        return "cast_binary_expression";
    case MYLITE_SQL_AST_CAST_CHAR_EXPRESSION:
        return "cast_char_expression";
    case MYLITE_SQL_AST_CAST_SIGNED_EXPRESSION:
        return "cast_signed_expression";
    case MYLITE_SQL_AST_CAST_UNSIGNED_EXPRESSION:
        return "cast_unsigned_expression";
    case MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION:
        return "convert_using_binary_expression";
    case MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION:
        return "convert_binary_type_expression";
    case MYLITE_SQL_AST_CONVERT_CHAR_TYPE_EXPRESSION:
        return "convert_char_type_expression";
    case MYLITE_SQL_AST_CONVERT_SIGNED_TYPE_EXPRESSION:
        return "convert_signed_type_expression";
    case MYLITE_SQL_AST_CONVERT_UNSIGNED_TYPE_EXPRESSION:
        return "convert_unsigned_type_expression";
    case MYLITE_SQL_AST_CONVERT_USING_CHARSET_EXPRESSION:
        return "convert_using_charset_expression";
    case MYLITE_SQL_AST_CHARSET_FUNCTION:
        return "charset_function";
    case MYLITE_SQL_AST_COLLATION_FUNCTION:
        return "collation_function";
    case MYLITE_SQL_AST_COERCIBILITY_FUNCTION:
        return "coercibility_function";
    case MYLITE_SQL_AST_COLUMN_POSITION_FIRST:
        return "column_position_first";
    case MYLITE_SQL_AST_COLUMN_POSITION_AFTER:
        return "column_position_after";
    case MYLITE_SQL_AST_DATE_ADD_FUNCTION:
        return "date_add_function";
    case MYLITE_SQL_AST_DATE_SUB_FUNCTION:
        return "date_sub_function";
    case MYLITE_SQL_AST_ADDDATE_FUNCTION:
        return "adddate_function";
    case MYLITE_SQL_AST_SUBDATE_FUNCTION:
        return "subdate_function";
    case MYLITE_SQL_AST_ADDTIME_FUNCTION:
        return "addtime_function";
    case MYLITE_SQL_AST_ADDTIME_ARGUMENT_COUNT_ERROR:
        return "addtime_argument_count_error";
    case MYLITE_SQL_AST_SUBTIME_FUNCTION:
        return "subtime_function";
    case MYLITE_SQL_AST_SUBTIME_ARGUMENT_COUNT_ERROR:
        return "subtime_argument_count_error";
    case MYLITE_SQL_AST_DATE_FORMAT_FUNCTION:
        return "date_format_function";
    case MYLITE_SQL_AST_DATE_FORMAT_ARGUMENT_COUNT_ERROR:
        return "date_format_argument_count_error";
    case MYLITE_SQL_AST_DATEDIFF_FUNCTION:
        return "datediff_function";
    case MYLITE_SQL_AST_DATEDIFF_ARGUMENT_COUNT_ERROR:
        return "datediff_argument_count_error";
    case MYLITE_SQL_AST_UNIX_TIMESTAMP_FUNCTION:
        return "unix_timestamp_function";
    case MYLITE_SQL_AST_UNIX_TIMESTAMP_ARGUMENT_COUNT_ERROR:
        return "unix_timestamp_argument_count_error";
    case MYLITE_SQL_AST_DATE_FUNCTION:
        return "date_function";
    case MYLITE_SQL_AST_TIME_FUNCTION:
        return "time_function";
    case MYLITE_SQL_AST_TIME_TO_SEC_FUNCTION:
        return "time_to_sec_function";
    case MYLITE_SQL_AST_TIME_TO_SEC_ARGUMENT_COUNT_ERROR:
        return "time_to_sec_argument_count_error";
    case MYLITE_SQL_AST_SEC_TO_TIME_FUNCTION:
        return "sec_to_time_function";
    case MYLITE_SQL_AST_SEC_TO_TIME_ARGUMENT_COUNT_ERROR:
        return "sec_to_time_argument_count_error";
    case MYLITE_SQL_AST_FROM_UNIXTIME_FUNCTION:
        return "from_unixtime_function";
    case MYLITE_SQL_AST_FROM_UNIXTIME_ARGUMENT_COUNT_ERROR:
        return "from_unixtime_argument_count_error";
    case MYLITE_SQL_AST_EXTRACT_FUNCTION:
        return "extract_function";
    case MYLITE_SQL_AST_QUARTER_FUNCTION:
        return "quarter_function";
    case MYLITE_SQL_AST_YEAR_FUNCTION:
        return "year_function";
    case MYLITE_SQL_AST_MONTH_FUNCTION:
        return "month_function";
    case MYLITE_SQL_AST_DAY_FUNCTION:
        return "day_function";
    case MYLITE_SQL_AST_DAYOFMONTH_FUNCTION:
        return "dayofmonth_function";
    case MYLITE_SQL_AST_DAYOFMONTH_ARGUMENT_COUNT_ERROR:
        return "dayofmonth_argument_count_error";
    case MYLITE_SQL_AST_DAYOFWEEK_FUNCTION:
        return "dayofweek_function";
    case MYLITE_SQL_AST_DAYOFWEEK_ARGUMENT_COUNT_ERROR:
        return "dayofweek_argument_count_error";
    case MYLITE_SQL_AST_DAYOFYEAR_FUNCTION:
        return "dayofyear_function";
    case MYLITE_SQL_AST_DAYOFYEAR_ARGUMENT_COUNT_ERROR:
        return "dayofyear_argument_count_error";
    case MYLITE_SQL_AST_LAST_DAY_FUNCTION:
        return "last_day_function";
    case MYLITE_SQL_AST_LAST_DAY_ARGUMENT_COUNT_ERROR:
        return "last_day_argument_count_error";
    case MYLITE_SQL_AST_INDEX_OPTION_LIST:
        return "index_option_list";
    case MYLITE_SQL_AST_INDEX_TYPE_OPTION:
        return "index_type_option";
    case MYLITE_SQL_AST_INDEX_COMMENT_OPTION:
        return "index_comment_option";
    case MYLITE_SQL_AST_INDEX_VISIBILITY_OPTION:
        return "index_visibility_option";
    case MYLITE_SQL_AST_COLUMN_COMMENT_ATTRIBUTE:
        return "column_comment_attribute";
    case MYLITE_SQL_AST_HOUR_FUNCTION:
        return "hour_function";
    case MYLITE_SQL_AST_MINUTE_FUNCTION:
        return "minute_function";
    case MYLITE_SQL_AST_SECOND_FUNCTION:
        return "second_function";
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP_VALUE:
        return "current_timestamp_value";
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP_ARGUMENT_COUNT_ERROR:
        return "current_timestamp_argument_count_error";
    case MYLITE_SQL_AST_CURRENT_DATE_VALUE:
        return "current_date_value";
    case MYLITE_SQL_AST_CURRENT_TIME_VALUE:
        return "current_time_value";
    case MYLITE_SQL_AST_UTC_DATE_VALUE:
        return "utc_date_value";
    case MYLITE_SQL_AST_UTC_TIME_VALUE:
        return "utc_time_value";
    case MYLITE_SQL_AST_UTC_TIMESTAMP_VALUE:
        return "utc_timestamp_value";
    case MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION:
        return "parenthesized_expression";
    case MYLITE_SQL_AST_CREATE_TABLE_STATEMENT:
        return "create_table_statement";
    case MYLITE_SQL_AST_CREATE_TABLE_LIKE_STATEMENT:
        return "create_table_like_statement";
    case MYLITE_SQL_AST_CREATE_TEMPORARY_TABLE_LIKE_STATEMENT:
        return "create_temporary_table_like_statement";
    case MYLITE_SQL_AST_CREATE_TABLE_SELECT_STATEMENT:
        return "create_table_select_statement";
    case MYLITE_SQL_AST_CREATE_TEMPORARY_TABLE_SELECT_STATEMENT:
        return "create_temporary_table_select_statement";
    case MYLITE_SQL_AST_CREATE_TEMPORARY_TABLE_STATEMENT:
        return "create_temporary_table_statement";
    case MYLITE_SQL_AST_DROP_TABLE_STATEMENT:
        return "drop_table_statement";
    case MYLITE_SQL_AST_DROP_TEMPORARY_TABLE_STATEMENT:
        return "drop_temporary_table_statement";
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
    case MYLITE_SQL_AST_CHAR_TYPE:
        return "char_type";
    case MYLITE_SQL_AST_TEXT_TYPE:
        return "text_type";
    case MYLITE_SQL_AST_JSON_TYPE:
        return "json_type";
    case MYLITE_SQL_AST_SPATIAL_TYPE:
        return "spatial_type";
    case MYLITE_SQL_AST_ENUM_TYPE:
        return "enum_type";
    case MYLITE_SQL_AST_ENUM_LABEL_LIST:
        return "enum_label_list";
    case MYLITE_SQL_AST_SET_TYPE:
        return "set_type";
    case MYLITE_SQL_AST_SET_MEMBER_LIST:
        return "set_member_list";
    case MYLITE_SQL_AST_BINARY_STRING_TYPE:
        return "binary_string_type";
    case MYLITE_SQL_AST_BIT_TYPE:
        return "bit_type";
    case MYLITE_SQL_AST_YEAR_TYPE:
        return "year_type";
    case MYLITE_SQL_AST_DECIMAL_TYPE:
        return "decimal_type";
    case MYLITE_SQL_AST_APPROXIMATE_TYPE:
        return "approximate_type";
    case MYLITE_SQL_AST_DATE_TYPE:
        return "date_type";
    case MYLITE_SQL_AST_DATETIME_TYPE:
        return "datetime_type";
    case MYLITE_SQL_AST_TIMESTAMP_TYPE:
        return "timestamp_type";
    case MYLITE_SQL_AST_TIME_TYPE:
        return "time_type";
    case MYLITE_SQL_AST_PRIMARY_KEY_DEFINITION:
        return "primary_key_definition";
    case MYLITE_SQL_AST_PRIMARY_KEY_PART_LIST:
        return "primary_key_part_list";
    case MYLITE_SQL_AST_SECONDARY_INDEX_DEFINITION:
        return "secondary_index_definition";
    case MYLITE_SQL_AST_SECONDARY_INDEX_PART_LIST:
        return "secondary_index_part_list";
    case MYLITE_SQL_AST_SECONDARY_INDEX_PART:
        return "secondary_index_part";
    case MYLITE_SQL_AST_FOREIGN_KEY_DEFINITION:
        return "foreign_key_definition";
    case MYLITE_SQL_AST_FOREIGN_KEY_PART_LIST:
        return "foreign_key_part_list";
    case MYLITE_SQL_AST_FOREIGN_KEY_INDEX_NAME:
        return "foreign_key_index_name";
    case MYLITE_SQL_AST_FOREIGN_KEY_ACTION_LIST:
        return "foreign_key_action_list";
    case MYLITE_SQL_AST_FOREIGN_KEY_ON_DELETE_CASCADE:
        return "foreign_key_on_delete_cascade";
    case MYLITE_SQL_AST_FOREIGN_KEY_ON_DELETE_RESTRICT:
        return "foreign_key_on_delete_restrict";
    case MYLITE_SQL_AST_FOREIGN_KEY_ON_DELETE_NO_ACTION:
        return "foreign_key_on_delete_no_action";
    case MYLITE_SQL_AST_FOREIGN_KEY_ON_DELETE_SET_NULL:
        return "foreign_key_on_delete_set_null";
    case MYLITE_SQL_AST_FOREIGN_KEY_ON_UPDATE_CASCADE:
        return "foreign_key_on_update_cascade";
    case MYLITE_SQL_AST_FOREIGN_KEY_ON_UPDATE_RESTRICT:
        return "foreign_key_on_update_restrict";
    case MYLITE_SQL_AST_FOREIGN_KEY_ON_UPDATE_NO_ACTION:
        return "foreign_key_on_update_no_action";
    case MYLITE_SQL_AST_FOREIGN_KEY_ON_UPDATE_SET_NULL:
        return "foreign_key_on_update_set_null";
    case MYLITE_SQL_AST_CHECK_CONSTRAINT_DEFINITION:
        return "check_constraint_definition";
    case MYLITE_SQL_AST_CHECK_ENFORCEMENT_ENFORCED:
        return "check_enforcement_enforced";
    case MYLITE_SQL_AST_CHECK_ENFORCEMENT_NOT_ENFORCED:
        return "check_enforcement_not_enforced";
    case MYLITE_SQL_AST_ALTER_TABLE_ADD_CHECK_STATEMENT:
        return "alter_table_add_check_statement";
    case MYLITE_SQL_AST_ALTER_TABLE_DROP_CHECK_STATEMENT:
        return "alter_table_drop_check_statement";
    case MYLITE_SQL_AST_ALTER_TABLE_ALTER_CHECK_STATEMENT:
        return "alter_table_alter_check_statement";
    case MYLITE_SQL_AST_ALTER_TABLE_DROP_CONSTRAINT_STATEMENT:
        return "alter_table_drop_constraint_statement";
    case MYLITE_SQL_AST_UNIQUE_INDEX_DEFINITION:
        return "unique_index_definition";
    case MYLITE_SQL_AST_FULLTEXT_INDEX_DEFINITION:
        return "fulltext_index_definition";
    case MYLITE_SQL_AST_SPATIAL_INDEX_DEFINITION:
        return "spatial_index_definition";
    case MYLITE_SQL_AST_INLINE_UNIQUE_KEY:
        return "inline_unique_key";
    case MYLITE_SQL_AST_INLINE_PRIMARY_KEY:
        return "inline_primary_key";
    case MYLITE_SQL_AST_ALTER_TABLE_ADD_PRIMARY_KEY_STATEMENT:
        return "alter_table_add_primary_key_statement";
    case MYLITE_SQL_AST_ALTER_TABLE_DROP_PRIMARY_KEY_STATEMENT:
        return "alter_table_drop_primary_key_statement";
    case MYLITE_SQL_AST_ALTER_TABLE_ADD_INDEX_STATEMENT:
        return "alter_table_add_index_statement";
    case MYLITE_SQL_AST_ALTER_TABLE_ADD_FOREIGN_KEY_STATEMENT:
        return "alter_table_add_foreign_key_statement";
    case MYLITE_SQL_AST_ALTER_TABLE_DROP_FOREIGN_KEY_STATEMENT:
        return "alter_table_drop_foreign_key_statement";
    case MYLITE_SQL_AST_ALTER_TABLE_DROP_INDEX_STATEMENT:
        return "alter_table_drop_index_statement";
    case MYLITE_SQL_AST_ALTER_TABLE_RENAME_INDEX_STATEMENT:
        return "alter_table_rename_index_statement";
    case MYLITE_SQL_AST_CREATE_INDEX_STATEMENT:
        return "create_index_statement";
    case MYLITE_SQL_AST_CREATE_UNIQUE_INDEX_STATEMENT:
        return "create_unique_index_statement";
    case MYLITE_SQL_AST_CREATE_FULLTEXT_INDEX_STATEMENT:
        return "create_fulltext_index_statement";
    case MYLITE_SQL_AST_CREATE_SPATIAL_INDEX_STATEMENT:
        return "create_spatial_index_statement";
    case MYLITE_SQL_AST_DROP_INDEX_STATEMENT:
        return "drop_index_statement";
    case MYLITE_SQL_AST_ALTER_TABLE_AUTO_INCREMENT_STATEMENT:
        return "alter_table_auto_increment_statement";
    case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_LIST:
        return "column_attribute_list";
    case MYLITE_SQL_AST_COLUMN_AUTO_INCREMENT:
        return "column_auto_increment";
    case MYLITE_SQL_AST_COLUMN_ON_UPDATE_CURRENT_TIMESTAMP:
        return "column_on_update_current_timestamp";
    case MYLITE_SQL_AST_COLUMN_CHARSET_ATTRIBUTE:
        return "column_charset_attribute";
    case MYLITE_SQL_AST_COLUMN_COLLATION_ATTRIBUTE:
        return "column_collation_attribute";
    case MYLITE_SQL_AST_GENERATED_COLUMN_CLAUSE:
        return "generated_column_clause";
    case MYLITE_SQL_AST_GENERATED_COLUMN_VIRTUAL:
        return "generated_column_virtual";
    case MYLITE_SQL_AST_GENERATED_COLUMN_STORED:
        return "generated_column_stored";
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
    case MYLITE_SQL_AST_CRC32_FUNCTION:
        return "crc32_function";
    case MYLITE_SQL_AST_CRC32_ARGUMENT_COUNT_ERROR:
        return "crc32_argument_count_error";
    case MYLITE_SQL_AST_HEX_FUNCTION:
        return "hex_function";
    case MYLITE_SQL_AST_HEX_ARGUMENT_COUNT_ERROR:
        return "hex_argument_count_error";
    case MYLITE_SQL_AST_UNHEX_FUNCTION:
        return "unhex_function";
    case MYLITE_SQL_AST_UNHEX_ARGUMENT_COUNT_ERROR:
        return "unhex_argument_count_error";
    case MYLITE_SQL_AST_DEFAULT_FUNCTION:
        return "default_function";
    case MYLITE_SQL_AST_ASCII_FUNCTION:
        return "ascii_function";
    case MYLITE_SQL_AST_ORD_FUNCTION:
        return "ord_function";
    case MYLITE_SQL_AST_ORD_ARGUMENT_COUNT_ERROR:
        return "ord_argument_count_error";
    case MYLITE_SQL_AST_FORMAT_FUNCTION:
        return "format_function";
    case MYLITE_SQL_AST_FORMAT_LOCALE_UNSUPPORTED:
        return "format_locale_unsupported";
    case MYLITE_SQL_AST_FORMAT_ARGUMENT_COUNT_ERROR:
        return "format_argument_count_error";
    case MYLITE_SQL_AST_TRUNCATE_FUNCTION:
        return "truncate_function";
    case MYLITE_SQL_AST_TRUNCATE_ARGUMENT_COUNT_ERROR:
        return "truncate_argument_count_error";
    case MYLITE_SQL_AST_LENGTH_FUNCTION:
        return "length_function";
    case MYLITE_SQL_AST_LENGTH_ARGUMENT_COUNT_ERROR:
        return "length_argument_count_error";
    case MYLITE_SQL_AST_OCTET_LENGTH_FUNCTION:
        return "octet_length_function";
    case MYLITE_SQL_AST_OCTET_LENGTH_ARGUMENT_COUNT_ERROR:
        return "octet_length_argument_count_error";
    case MYLITE_SQL_AST_BIT_LENGTH_FUNCTION:
        return "bit_length_function";
    case MYLITE_SQL_AST_BIT_LENGTH_ARGUMENT_COUNT_ERROR:
        return "bit_length_argument_count_error";
    case MYLITE_SQL_AST_CHAR_LENGTH_FUNCTION:
        return "char_length_function";
    case MYLITE_SQL_AST_CHAR_LENGTH_ARGUMENT_COUNT_ERROR:
        return "char_length_argument_count_error";
    case MYLITE_SQL_AST_CHARACTER_LENGTH_FUNCTION:
        return "character_length_function";
    case MYLITE_SQL_AST_CHARACTER_LENGTH_ARGUMENT_COUNT_ERROR:
        return "character_length_argument_count_error";
    case MYLITE_SQL_AST_LOWER_FUNCTION:
        return "lower_function";
    case MYLITE_SQL_AST_LOWER_ARGUMENT_COUNT_ERROR:
        return "lower_argument_count_error";
    case MYLITE_SQL_AST_LCASE_FUNCTION:
        return "lcase_function";
    case MYLITE_SQL_AST_LCASE_ARGUMENT_COUNT_ERROR:
        return "lcase_argument_count_error";
    case MYLITE_SQL_AST_UPPER_FUNCTION:
        return "upper_function";
    case MYLITE_SQL_AST_UPPER_ARGUMENT_COUNT_ERROR:
        return "upper_argument_count_error";
    case MYLITE_SQL_AST_UCASE_FUNCTION:
        return "ucase_function";
    case MYLITE_SQL_AST_UCASE_ARGUMENT_COUNT_ERROR:
        return "ucase_argument_count_error";
    case MYLITE_SQL_AST_LTRIM_FUNCTION:
        return "ltrim_function";
    case MYLITE_SQL_AST_LTRIM_ARGUMENT_COUNT_ERROR:
        return "ltrim_argument_count_error";
    case MYLITE_SQL_AST_RTRIM_FUNCTION:
        return "rtrim_function";
    case MYLITE_SQL_AST_RTRIM_ARGUMENT_COUNT_ERROR:
        return "rtrim_argument_count_error";
    case MYLITE_SQL_AST_TRIM_FUNCTION:
        return "trim_function";
    case MYLITE_SQL_AST_TRIM_LEADING_FUNCTION:
        return "trim_leading_function";
    case MYLITE_SQL_AST_TRIM_TRAILING_FUNCTION:
        return "trim_trailing_function";
    case MYLITE_SQL_AST_LEFT_FUNCTION:
        return "left_function";
    case MYLITE_SQL_AST_RIGHT_FUNCTION:
        return "right_function";
    case MYLITE_SQL_AST_LPAD_FUNCTION:
        return "lpad_function";
    case MYLITE_SQL_AST_LPAD_ARGUMENT_COUNT_ERROR:
        return "lpad_argument_count_error";
    case MYLITE_SQL_AST_RPAD_FUNCTION:
        return "rpad_function";
    case MYLITE_SQL_AST_RPAD_ARGUMENT_COUNT_ERROR:
        return "rpad_argument_count_error";
    case MYLITE_SQL_AST_REPEAT_FUNCTION:
        return "repeat_function";
    case MYLITE_SQL_AST_SPACE_FUNCTION:
        return "space_function";
    case MYLITE_SQL_AST_SPACE_ARGUMENT_COUNT_ERROR:
        return "space_argument_count_error";
    case MYLITE_SQL_AST_SUBSTRING_FUNCTION:
        return "substring_function";
    case MYLITE_SQL_AST_SUBSTR_FUNCTION:
        return "substr_function";
    case MYLITE_SQL_AST_MID_FUNCTION:
        return "mid_function";
    case MYLITE_SQL_AST_SUBSTRING_INDEX_FUNCTION:
        return "substring_index_function";
    case MYLITE_SQL_AST_SUBSTRING_INDEX_ARGUMENT_COUNT_ERROR:
        return "substring_index_argument_count_error";
    case MYLITE_SQL_AST_LOCATE_FUNCTION:
        return "locate_function";
    case MYLITE_SQL_AST_LOCATE_ARGUMENT_COUNT_ERROR:
        return "locate_argument_count_error";
    case MYLITE_SQL_AST_INSTR_FUNCTION:
        return "instr_function";
    case MYLITE_SQL_AST_INSTR_ARGUMENT_COUNT_ERROR:
        return "instr_argument_count_error";
    case MYLITE_SQL_AST_POSITION_FUNCTION:
        return "position_function";
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
    case MYLITE_SQL_AST_CONCAT_FUNCTION:
        return "concat_function";
    case MYLITE_SQL_AST_CONCAT_ARGUMENT_COUNT_ERROR:
        return "concat_argument_count_error";
    case MYLITE_SQL_AST_CONCAT_WS_FUNCTION:
        return "concat_ws_function";
    case MYLITE_SQL_AST_CONCAT_WS_ARGUMENT_COUNT_ERROR:
        return "concat_ws_argument_count_error";
    case MYLITE_SQL_AST_REPLACE_FUNCTION:
        return "replace_function";
    case MYLITE_SQL_AST_REVERSE_FUNCTION:
        return "reverse_function";
    case MYLITE_SQL_AST_CHAR_FUNCTION:
        return "char_function";
    case MYLITE_SQL_AST_JSON_VALID_FUNCTION:
        return "json_valid_function";
    case MYLITE_SQL_AST_JSON_VALID_ARGUMENT_COUNT_ERROR:
        return "json_valid_argument_count_error";
    case MYLITE_SQL_AST_REGEXP_LIKE_FUNCTION:
        return "regexp_like_function";
    case MYLITE_SQL_AST_REGEXP_LIKE_ARGUMENT_COUNT_ERROR:
        return "regexp_like_argument_count_error";
    case MYLITE_SQL_AST_JSON_EXTRACT_FUNCTION:
        return "json_extract_function";
    case MYLITE_SQL_AST_JSON_EXTRACT_ARGUMENT_COUNT_ERROR:
        return "json_extract_argument_count_error";
    case MYLITE_SQL_AST_JSON_UNQUOTE_FUNCTION:
        return "json_unquote_function";
    case MYLITE_SQL_AST_JSON_UNQUOTE_ARGUMENT_COUNT_ERROR:
        return "json_unquote_argument_count_error";
    case MYLITE_SQL_AST_JSON_ARRAY_FUNCTION:
        return "json_array_function";
    case MYLITE_SQL_AST_JSON_OBJECT_FUNCTION:
        return "json_object_function";
    case MYLITE_SQL_AST_JSON_CONTAINS_FUNCTION:
        return "json_contains_function";
    case MYLITE_SQL_AST_JSON_CONTAINS_ARGUMENT_COUNT_ERROR:
        return "json_contains_argument_count_error";
    case MYLITE_SQL_AST_JSON_CONTAINS_PATH_FUNCTION:
        return "json_contains_path_function";
    case MYLITE_SQL_AST_JSON_CONTAINS_PATH_ARGUMENT_COUNT_ERROR:
        return "json_contains_path_argument_count_error";
    case MYLITE_SQL_AST_JSON_TYPE_FUNCTION:
        return "json_type_function";
    case MYLITE_SQL_AST_JSON_TYPE_ARGUMENT_COUNT_ERROR:
        return "json_type_argument_count_error";
    case MYLITE_SQL_AST_JSON_LENGTH_FUNCTION:
        return "json_length_function";
    case MYLITE_SQL_AST_JSON_LENGTH_ARGUMENT_COUNT_ERROR:
        return "json_length_argument_count_error";
    case MYLITE_SQL_AST_FIELD_FUNCTION:
        return "field_function";
    case MYLITE_SQL_AST_FIELD_ARGUMENT_COUNT_ERROR:
        return "field_argument_count_error";
    case MYLITE_SQL_AST_GREATEST_FUNCTION:
        return "greatest_function";
    case MYLITE_SQL_AST_GREATEST_ARGUMENT_COUNT_ERROR:
        return "greatest_argument_count_error";
    case MYLITE_SQL_AST_LEAST_FUNCTION:
        return "least_function";
    case MYLITE_SQL_AST_LEAST_ARGUMENT_COUNT_ERROR:
        return "least_argument_count_error";
    case MYLITE_SQL_AST_FIND_IN_SET_FUNCTION:
        return "find_in_set_function";
    case MYLITE_SQL_AST_FIND_IN_SET_ARGUMENT_COUNT_ERROR:
        return "find_in_set_argument_count_error";
    case MYLITE_SQL_AST_STRCMP_FUNCTION:
        return "strcmp_function";
    case MYLITE_SQL_AST_STRCMP_ARGUMENT_COUNT_ERROR:
        return "strcmp_argument_count_error";
    case MYLITE_SQL_AST_QUOTE_FUNCTION:
        return "quote_function";
    case MYLITE_SQL_AST_QUOTE_ARGUMENT_COUNT_ERROR:
        return "quote_argument_count_error";
    case MYLITE_SQL_AST_SCALAR_SUBQUERY:
        return "scalar_subquery";
    case MYLITE_SQL_AST_PI_FUNCTION:
        return "pi_function";
    case MYLITE_SQL_AST_PI_ARGUMENT_COUNT_ERROR:
        return "pi_argument_count_error";
    case MYLITE_SQL_AST_RAND_FUNCTION:
        return "rand_function";
    case MYLITE_SQL_AST_RAND_SEED_FUNCTION:
        return "rand_seed_function";
    case MYLITE_SQL_AST_RAND_ARGUMENT_COUNT_ERROR:
        return "rand_argument_count_error";
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
    case MYLITE_SQL_AST_SIN_FUNCTION:
        return "sin_function";
    case MYLITE_SQL_AST_SIN_ARGUMENT_COUNT_ERROR:
        return "sin_argument_count_error";
    case MYLITE_SQL_AST_COS_FUNCTION:
        return "cos_function";
    case MYLITE_SQL_AST_COS_ARGUMENT_COUNT_ERROR:
        return "cos_argument_count_error";
    case MYLITE_SQL_AST_TAN_FUNCTION:
        return "tan_function";
    case MYLITE_SQL_AST_TAN_ARGUMENT_COUNT_ERROR:
        return "tan_argument_count_error";
    case MYLITE_SQL_AST_COT_FUNCTION:
        return "cot_function";
    case MYLITE_SQL_AST_COT_ARGUMENT_COUNT_ERROR:
        return "cot_argument_count_error";
    case MYLITE_SQL_AST_EXP_FUNCTION:
        return "exp_function";
    case MYLITE_SQL_AST_EXP_ARGUMENT_COUNT_ERROR:
        return "exp_argument_count_error";
    case MYLITE_SQL_AST_LN_FUNCTION:
        return "ln_function";
    case MYLITE_SQL_AST_LN_ARGUMENT_COUNT_ERROR:
        return "ln_argument_count_error";
    case MYLITE_SQL_AST_LOG_FUNCTION:
        return "log_function";
    case MYLITE_SQL_AST_LOG10_FUNCTION:
        return "log10_function";
    case MYLITE_SQL_AST_LOG10_ARGUMENT_COUNT_ERROR:
        return "log10_argument_count_error";
    case MYLITE_SQL_AST_LOG2_FUNCTION:
        return "log2_function";
    case MYLITE_SQL_AST_LOG2_ARGUMENT_COUNT_ERROR:
        return "log2_argument_count_error";
    case MYLITE_SQL_AST_POW_FUNCTION:
        return "pow_function";
    case MYLITE_SQL_AST_POW_ARGUMENT_COUNT_ERROR:
        return "pow_argument_count_error";
    case MYLITE_SQL_AST_POWER_FUNCTION:
        return "power_function";
    case MYLITE_SQL_AST_POWER_ARGUMENT_COUNT_ERROR:
        return "power_argument_count_error";
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
    case MYLITE_SQL_AST_SET_STATEMENT:
        return "set_statement";
    case MYLITE_SQL_AST_SET_ASSIGNMENT_LIST:
        return "set_assignment_list";
    case MYLITE_SQL_AST_SET_ASSIGNMENT:
        return "set_assignment";
    case MYLITE_SQL_AST_PREPARE_STATEMENT:
        return "prepare_statement";
    case MYLITE_SQL_AST_EXECUTE_STATEMENT:
        return "execute_statement";
    case MYLITE_SQL_AST_DEALLOCATE_PREPARE_STATEMENT:
        return "deallocate_prepare_statement";
    case MYLITE_SQL_AST_EXECUTE_USING_LIST:
        return "execute_using_list";
    case MYLITE_SQL_AST_SET_SYSTEM_VARIABLE_STATEMENT:
        return "set_system_variable_statement";
    case MYLITE_SQL_AST_SET_SYSTEM_VARIABLE_TARGET:
        return "set_system_variable_target";
    case MYLITE_SQL_AST_SET_DEFAULT_VALUE:
        return "set_default_value";
    case MYLITE_SQL_AST_SHOW_VARIABLES_STATEMENT:
        return "show_variables_statement";
    case MYLITE_SQL_AST_SHOW_STATUS_STATEMENT:
        return "show_status_statement";
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
    case MYLITE_SQL_AST_INSERT_DUPLICATE_UPDATE_CLAUSE:
        return "insert_duplicate_update_clause";
    case MYLITE_SQL_AST_INSERT_DUPLICATE_ASSIGNMENT_LIST:
        return "insert_duplicate_assignment_list";
    case MYLITE_SQL_AST_INSERT_DUPLICATE_ASSIGNMENT:
        return "insert_duplicate_assignment";
    case MYLITE_SQL_AST_INSERT_VALUES_REFERENCE:
        return "insert_values_reference";
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
    case MYLITE_SQL_AST_ALTER_TABLE_CONVERT_CHARACTER_SET_STATEMENT:
        return "alter_table_convert_character_set_statement";
    case MYLITE_SQL_AST_ALTER_TABLE_COMMENT_STATEMENT:
        return "alter_table_comment_statement";
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
    case MYLITE_SQL_AST_FROM_JOIN:
        return "from_join";
    case MYLITE_SQL_AST_INDEX_HINT_LIST:
        return "index_hint_list";
    case MYLITE_SQL_AST_USE_INDEX_HINT:
        return "use_index_hint";
    case MYLITE_SQL_AST_FORCE_INDEX_HINT:
        return "force_index_hint";
    case MYLITE_SQL_AST_IGNORE_INDEX_HINT:
        return "ignore_index_hint";
    case MYLITE_SQL_AST_INDEX_HINT_FOR_JOIN:
        return "index_hint_for_join";
    case MYLITE_SQL_AST_INDEX_HINT_FOR_ORDER_BY:
        return "index_hint_for_order_by";
    case MYLITE_SQL_AST_INDEX_HINT_FOR_GROUP_BY:
        return "index_hint_for_group_by";
    case MYLITE_SQL_AST_ALTER_TABLE_INDEX_VISIBILITY_STATEMENT:
        return "alter_table_index_visibility_statement";
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
    case MYLITE_SQL_AST_EXISTS_PREDICATE:
        return "exists_predicate";
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
    case MYLITE_SQL_AST_JOINED_DELETE_STATEMENT:
        return "joined_delete_statement";
    case MYLITE_SQL_AST_UPDATE_STATEMENT:
        return "update_statement";
    case MYLITE_SQL_AST_JOINED_UPDATE_STATEMENT:
        return "joined_update_statement";
    case MYLITE_SQL_AST_START_TRANSACTION_STATEMENT:
        return "start_transaction_statement";
    case MYLITE_SQL_AST_SET_TRANSACTION_STATEMENT:
        return "set_transaction_statement";
    case MYLITE_SQL_AST_TRANSACTION_CHARACTERISTIC_LIST:
        return "transaction_characteristic_list";
    case MYLITE_SQL_AST_TRANSACTION_ISOLATION_REPEATABLE_READ:
        return "transaction_isolation_repeatable_read";
    case MYLITE_SQL_AST_TRANSACTION_ISOLATION_READ_COMMITTED:
        return "transaction_isolation_read_committed";
    case MYLITE_SQL_AST_TRANSACTION_ISOLATION_READ_UNCOMMITTED:
        return "transaction_isolation_read_uncommitted";
    case MYLITE_SQL_AST_TRANSACTION_ISOLATION_SERIALIZABLE:
        return "transaction_isolation_serializable";
    case MYLITE_SQL_AST_TRANSACTION_ACCESS_READ_WRITE:
        return "transaction_access_read_write";
    case MYLITE_SQL_AST_TRANSACTION_ACCESS_READ_ONLY:
        return "transaction_access_read_only";
    case MYLITE_SQL_AST_TRANSACTION_CONSISTENT_SNAPSHOT:
        return "transaction_consistent_snapshot";
    case MYLITE_SQL_AST_COMMIT_STATEMENT:
        return "commit_statement";
    case MYLITE_SQL_AST_ROLLBACK_STATEMENT:
        return "rollback_statement";
    case MYLITE_SQL_AST_SAVEPOINT_STATEMENT:
        return "savepoint_statement";
    case MYLITE_SQL_AST_ROLLBACK_TO_SAVEPOINT_STATEMENT:
        return "rollback_to_savepoint_statement";
    case MYLITE_SQL_AST_RELEASE_SAVEPOINT_STATEMENT:
        return "release_savepoint_statement";
    case MYLITE_SQL_AST_ANALYZE_TABLE_STATEMENT:
        return "analyze_table_statement";
    case MYLITE_SQL_AST_CHECK_TABLE_STATEMENT:
        return "check_table_statement";
    case MYLITE_SQL_AST_OPTIMIZE_TABLE_STATEMENT:
        return "optimize_table_statement";
    case MYLITE_SQL_AST_REPAIR_TABLE_STATEMENT:
        return "repair_table_statement";
    case MYLITE_SQL_AST_UPDATE_ASSIGNMENT_LIST:
        return "update_assignment_list";
    case MYLITE_SQL_AST_UPDATE_ASSIGNMENT:
        return "update_assignment";
    case MYLITE_SQL_AST_TRUNCATE_TABLE_STATEMENT:
        return "truncate_table_statement";
    case MYLITE_SQL_AST_CREATE_SCHEMA_STATEMENT:
        return "create_schema_statement";
    case MYLITE_SQL_AST_ALTER_SCHEMA_DEFAULT_CHARSET_COLLATION_STATEMENT:
        return "alter_schema_default_charset_collation_statement";
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
    case MYLITE_SQL_AST_LAST_INSERT_ID_SET_FUNCTION:
        return "last_insert_id_set_function";
    case MYLITE_SQL_AST_LAST_INSERT_ID_ARGUMENT_COUNT_ERROR:
        return "last_insert_id_argument_count_error";
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
    case MYLITE_SQL_AST_GROUP_CONCAT_AGGREGATE_FUNCTION:
        return "group_concat_aggregate_function";
    case MYLITE_SQL_AST_LOCK_TABLES_STATEMENT:
        return "lock_tables_statement";
    case MYLITE_SQL_AST_UNLOCK_TABLES_STATEMENT:
        return "unlock_tables_statement";
    case MYLITE_SQL_AST_LOCK_TABLE_TARGET_LIST:
        return "lock_table_target_list";
    case MYLITE_SQL_AST_LOCK_TABLE_TARGET:
        return "lock_table_target";
    case MYLITE_SQL_AST_LOCK_TABLE_READ_LOCK:
        return "lock_table_read_lock";
    case MYLITE_SQL_AST_LOCK_TABLE_READ_LOCAL_LOCK:
        return "lock_table_read_local_lock";
    case MYLITE_SQL_AST_LOCK_TABLE_WRITE_LOCK:
        return "lock_table_write_lock";
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
    case MYLITE_SQL_AST_SHOW_FULL_COLUMNS_STATEMENT:
        return "show_full_columns_statement";
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
    case MYLITE_SQL_AST_TABLE_COMMENT_OPTION:
        return "table_comment_option";
    case MYLITE_SQL_AST_TABLE_ROW_FORMAT_OPTION:
        return "table_row_format_option";
    case MYLITE_SQL_AST_TABLE_KEY_BLOCK_SIZE_OPTION:
        return "table_key_block_size_option";
    case MYLITE_SQL_AST_TABLE_PACK_KEYS_OPTION:
        return "table_pack_keys_option";
    case MYLITE_SQL_AST_TABLE_CHECKSUM_OPTION:
        return "table_checksum_option";
    case MYLITE_SQL_AST_TABLE_STATS_PERSISTENT_OPTION:
        return "table_stats_persistent_option";
    case MYLITE_SQL_AST_TABLE_STATS_AUTO_RECALC_OPTION:
        return "table_stats_auto_recalc_option";
    case MYLITE_SQL_AST_TABLE_STATS_SAMPLE_PAGES_OPTION:
        return "table_stats_sample_pages_option";
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
    case MYLITE_SQL_AST_SHOW_GRANTS_STATEMENT:
        return "show_grants_statement";
    case MYLITE_SQL_AST_SHOW_WARNINGS_STATEMENT:
        return "show_warnings_statement";
    case MYLITE_SQL_AST_SHOW_COUNT_WARNINGS_STATEMENT:
        return "show_count_warnings_statement";
    case MYLITE_SQL_AST_SHOW_ERRORS_STATEMENT:
        return "show_errors_statement";
    case MYLITE_SQL_AST_SHOW_COUNT_ERRORS_STATEMENT:
        return "show_count_errors_statement";
    case MYLITE_SQL_AST_USER_VARIABLE:
        return "user_variable";
    case MYLITE_SQL_AST_SYSTEM_VARIABLE:
        return "system_variable";
    case MYLITE_SQL_AST_CURRENT_ROLE_FUNCTION:
        return "current_role_function";
    case MYLITE_SQL_AST_CURRENT_ROLE_ARGUMENT_COUNT_ERROR:
        return "current_role_argument_count_error";
    case MYLITE_SQL_AST_ALTER_TABLE_ADD_COLUMN_STATEMENT:
        return "alter_table_add_column_statement";
    case MYLITE_SQL_AST_ALTER_TABLE_MULTI_ACTION_STATEMENT:
        return "alter_table_multi_action_statement";
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_LIST:
        return "alter_table_action_list";
    case MYLITE_SQL_AST_ALTER_TABLE_DISABLE_KEYS_STATEMENT:
        return "alter_table_disable_keys_statement";
    case MYLITE_SQL_AST_ALTER_TABLE_ENABLE_KEYS_STATEMENT:
        return "alter_table_enable_keys_statement";
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
    case MYLITE_SQL_AST_OPERATOR_CONCAT:
        return "concat";
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
    case MYLITE_SQL_AST_OPERATOR_LIKE:
        return "like";
    case MYLITE_SQL_AST_OPERATOR_REGEXP:
        return "regexp";
    case MYLITE_SQL_AST_OPERATOR_RLIKE:
        return "rlike";
    case MYLITE_SQL_AST_OPERATOR_JSON_EXTRACT:
        return "json_extract";
    case MYLITE_SQL_AST_OPERATOR_JSON_UNQUOTE_EXTRACT:
        return "json_unquote_extract";
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

const char *mylite_sql_ast_text_type_name(enum mylite_sql_ast_text_type text_type) {
    switch (text_type) {
    case MYLITE_SQL_AST_TEXT_TYPE_NONE:
        return "none";
    case MYLITE_SQL_AST_TEXT_TYPE_TINYTEXT:
        return "tinytext";
    case MYLITE_SQL_AST_TEXT_TYPE_TEXT:
        return "text";
    case MYLITE_SQL_AST_TEXT_TYPE_MEDIUMTEXT:
        return "mediumtext";
    case MYLITE_SQL_AST_TEXT_TYPE_LONGTEXT:
        return "longtext";
    }

    return "unknown";
}

const char *mylite_sql_ast_binary_string_type_name(
    enum mylite_sql_ast_binary_string_type binary_string_type
) {
    switch (binary_string_type) {
    case MYLITE_SQL_AST_BINARY_STRING_TYPE_NONE:
        return "none";
    case MYLITE_SQL_AST_BINARY_STRING_TYPE_BINARY:
        return "binary";
    case MYLITE_SQL_AST_BINARY_STRING_TYPE_VARBINARY:
        return "varbinary";
    case MYLITE_SQL_AST_BINARY_STRING_TYPE_TINYBLOB:
        return "tinyblob";
    case MYLITE_SQL_AST_BINARY_STRING_TYPE_BLOB:
        return "blob";
    case MYLITE_SQL_AST_BINARY_STRING_TYPE_MEDIUMBLOB:
        return "mediumblob";
    case MYLITE_SQL_AST_BINARY_STRING_TYPE_LONGBLOB:
        return "longblob";
    }

    return "unknown";
}

const char *mylite_sql_ast_spatial_type_name(enum mylite_sql_ast_spatial_type spatial_type) {
    switch (spatial_type) {
    case MYLITE_SQL_AST_SPATIAL_TYPE_NONE:
        return "none";
    case MYLITE_SQL_AST_SPATIAL_TYPE_GEOMETRY:
        return "geometry";
    case MYLITE_SQL_AST_SPATIAL_TYPE_POINT:
        return "point";
    case MYLITE_SQL_AST_SPATIAL_TYPE_LINESTRING:
        return "linestring";
    case MYLITE_SQL_AST_SPATIAL_TYPE_POLYGON:
        return "polygon";
    case MYLITE_SQL_AST_SPATIAL_TYPE_MULTIPOINT:
        return "multipoint";
    case MYLITE_SQL_AST_SPATIAL_TYPE_MULTILINESTRING:
        return "multilinestring";
    case MYLITE_SQL_AST_SPATIAL_TYPE_MULTIPOLYGON:
        return "multipolygon";
    case MYLITE_SQL_AST_SPATIAL_TYPE_GEOMETRYCOLLECTION:
        return "geomcollection";
    }

    return "unknown";
}

const char *mylite_sql_ast_decimal_type_name(enum mylite_sql_ast_decimal_type decimal_type) {
    switch (decimal_type) {
    case MYLITE_SQL_AST_DECIMAL_TYPE_DECIMAL:
        return "decimal";
    case MYLITE_SQL_AST_DECIMAL_TYPE_DEC:
        return "dec";
    case MYLITE_SQL_AST_DECIMAL_TYPE_NUMERIC:
        return "numeric";
    case MYLITE_SQL_AST_DECIMAL_TYPE_FIXED:
        return "fixed";
    }

    return "unknown";
}

const char *mylite_sql_ast_approximate_type_name(
    enum mylite_sql_ast_approximate_type approximate_type
) {
    switch (approximate_type) {
    case MYLITE_SQL_AST_APPROXIMATE_TYPE_FLOAT:
        return "float";
    case MYLITE_SQL_AST_APPROXIMATE_TYPE_FLOAT4:
        return "float4";
    case MYLITE_SQL_AST_APPROXIMATE_TYPE_FLOAT8:
        return "float8";
    case MYLITE_SQL_AST_APPROXIMATE_TYPE_DOUBLE:
        return "double";
    case MYLITE_SQL_AST_APPROXIMATE_TYPE_REAL:
        return "real";
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
