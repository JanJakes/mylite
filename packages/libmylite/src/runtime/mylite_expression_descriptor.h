#ifndef MYLITE_RUNTIME_MYLITE_EXPRESSION_DESCRIPTOR_H
#define MYLITE_RUNTIME_MYLITE_EXPRESSION_DESCRIPTOR_H

#include <mylite/mylite.h>

#include "mylite_expression.h"
#include "mylite_field_descriptor.h"
#include "sql/mylite_ast.h"

#include <stdbool.h>
#include <stdint.h>

struct mylite_collation;

bool mylite_expression_descriptor_has_text_result(const struct mylite_field_descriptor *descriptor);
bool mylite_expression_descriptor_has_numeric_result(
    const struct mylite_field_descriptor *descriptor
);
bool mylite_expression_descriptor_has_decimal_result(
    const struct mylite_field_descriptor *descriptor
);
bool mylite_expression_descriptor_has_double_result(
    const struct mylite_field_descriptor *descriptor
);
void mylite_expression_descriptor_merge_union_operand(
    const mylite_db *database,
    struct mylite_field_descriptor *descriptor,
    const struct mylite_field_descriptor *operand
);
struct mylite_field_descriptor mylite_expression_descriptor_from_value(
    const struct mylite_expression_value *value
);
bool mylite_expression_descriptor_operator_forces_not_null(
    enum mylite_sql_ast_operator operator_kind
);
struct mylite_field_descriptor mylite_expression_descriptor_null(void);
struct mylite_field_descriptor mylite_expression_descriptor_boolean(bool nullable);
struct mylite_field_descriptor mylite_expression_descriptor_unsigned_longlong(bool nullable);
struct mylite_field_descriptor mylite_expression_descriptor_signed_longlong(bool nullable);
struct mylite_field_descriptor mylite_expression_descriptor_decimal(bool nullable);
bool mylite_expression_descriptor_is_nullable(const struct mylite_field_descriptor *descriptor);
void mylite_expression_descriptor_set_scalar_subquery_nullable(
    struct mylite_field_descriptor *descriptor
);
struct mylite_field_descriptor mylite_expression_descriptor_defaults(void);
unsigned int mylite_expression_descriptor_connection_collation_id(const mylite_db *database);
unsigned int mylite_expression_descriptor_connection_charset_id(const mylite_db *database);
const struct mylite_collation *mylite_expression_descriptor_collation_lookup_id(
    unsigned int collation_id
);
unsigned int mylite_expression_descriptor_literal_decimal_scale(
    const struct mylite_sql_ast_node *expression
);
uint64_t mylite_expression_descriptor_literal_integer_length(
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_value *value
);
uint64_t mylite_expression_descriptor_string_length(
    const mylite_db *database,
    const struct mylite_expression_value *value,
    const struct mylite_sql_ast_node *expression
);
uint64_t mylite_expression_descriptor_connection_character_max_length(const mylite_db *database);
uint64_t mylite_expression_descriptor_utf8_display_character_count(const char *text);
uint64_t mylite_expression_descriptor_max_u64(uint64_t left, uint64_t right);

#endif
