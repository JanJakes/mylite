#include "mylite_expression_descriptor.h"

#include "mylite_charset.h"
#include "mylite_metadata_constants.h"
#include "mylite_runtime.h"

#include <ctype.h>
#include <stddef.h>

static const unsigned int mylite_utf8_continuation_mask = 0xC0U;
static const unsigned int mylite_utf8_continuation_marker = 0x80U;

bool mylite_expression_descriptor_has_text_result(const struct mylite_field_descriptor *descriptor)
{
    if (descriptor == NULL) {
        return false;
    }
    if (descriptor->type == MYLITE_FIELD_TYPE_VAR_STRING) {
        return true;
    }
    if ((descriptor->flags & MYLITE_FIELD_FLAG_NUM) != 0U) {
        return false;
    }
    if (descriptor->charset_id != mylite_mysql_binary_charset_id) {
        return true;
    }
    return false;
}

bool mylite_expression_descriptor_has_numeric_result(
    const struct mylite_field_descriptor *descriptor)
{
    if (descriptor == NULL || descriptor->type == MYLITE_FIELD_TYPE_NULL) {
        return false;
    }
    return (descriptor->flags & MYLITE_FIELD_FLAG_NUM) != 0U;
}

bool mylite_expression_descriptor_has_decimal_result(
    const struct mylite_field_descriptor *descriptor)
{
    if (descriptor == NULL) {
        return false;
    }
    if (descriptor->type == MYLITE_FIELD_TYPE_NEWDECIMAL) {
        return true;
    }
    return false;
}

bool mylite_expression_descriptor_has_double_result(
    const struct mylite_field_descriptor *descriptor)
{
    if (descriptor == NULL) {
        return false;
    }
    if (descriptor->type == MYLITE_FIELD_TYPE_DOUBLE) {
        return true;
    }
    return false;
}

struct mylite_field_descriptor
mylite_expression_descriptor_from_value(const struct mylite_expression_value *value)
{
    if (value == NULL) {
        return mylite_expression_descriptor_defaults();
    }

    switch (value->kind) {
    case MYLITE_EXPRESSION_VALUE_NULL:
        return mylite_expression_descriptor_null();
    case MYLITE_EXPRESSION_VALUE_INT64:
        return mylite_expression_descriptor_signed_longlong(false);
    case MYLITE_EXPRESSION_VALUE_UINT64:
        return mylite_expression_descriptor_unsigned_longlong(false);
    case MYLITE_EXPRESSION_VALUE_REAL:
        return (struct mylite_field_descriptor){
            .type = MYLITE_FIELD_TYPE_DOUBLE,
            .flags = MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
            .length = mylite_mysql_double_display_length,
            .decimals = mylite_mysql_not_fixed_decimals,
            .charset_id = mylite_mysql_binary_charset_id,
            .nullable = false,
        };
    case MYLITE_EXPRESSION_VALUE_TEXT:
        return (struct mylite_field_descriptor){
            .type = MYLITE_FIELD_TYPE_VAR_STRING,
            .flags = MYLITE_FIELD_FLAG_NOT_NULL,
            .length = value->text_value == NULL ? 0U : value->text_length,
            .decimals = mylite_mysql_not_fixed_decimals,
            .charset_id = mylite_mysql_utf8mb4_0900_ai_ci_charset_id,
            .nullable = false,
        };
    }
    return mylite_expression_descriptor_defaults();
}

bool mylite_expression_descriptor_operator_forces_not_null(
    enum mylite_sql_ast_operator operator_kind)
{
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_IS_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN:
        return true;
    case MYLITE_SQL_AST_OPERATOR_NONE:
    case MYLITE_SQL_AST_OPERATOR_POSITIVE:
    case MYLITE_SQL_AST_OPERATOR_NEGATIVE:
    case MYLITE_SQL_AST_OPERATOR_ADD:
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
    case MYLITE_SQL_AST_OPERATOR_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LESS:
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_GREATER:
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_AND:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_OR:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_NOT:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_AND:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_XOR:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_OR:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_LEFT:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_RIGHT:
    case MYLITE_SQL_AST_OPERATOR_BETWEEN:
    case MYLITE_SQL_AST_OPERATOR_NOT_BETWEEN:
    case MYLITE_SQL_AST_OPERATOR_LIKE:
    case MYLITE_SQL_AST_OPERATOR_NOT_LIKE:
    case MYLITE_SQL_AST_OPERATOR_IN:
    case MYLITE_SQL_AST_OPERATOR_NOT_IN:
    case MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_MODULO:
        return false;
    }
    return false;
}

struct mylite_field_descriptor mylite_expression_descriptor_null(void)
{
    return (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_NULL,
        .flags = MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = true,
    };
}

struct mylite_field_descriptor mylite_expression_descriptor_boolean(bool nullable)
{
    struct mylite_field_descriptor descriptor = {
        .type = MYLITE_FIELD_TYPE_LONGLONG,
        .flags = MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
        .length = 1U,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = nullable,
    };

    mylite_field_descriptor_set_nullable(&descriptor, nullable);
    return descriptor;
}

struct mylite_field_descriptor mylite_expression_descriptor_unsigned_longlong(bool nullable)
{
    struct mylite_field_descriptor descriptor =
        mylite_expression_descriptor_signed_longlong(nullable);

    descriptor.flags |= MYLITE_FIELD_FLAG_UNSIGNED;
    descriptor.length = mylite_mysql_unsigned_longlong_display_length;
    return descriptor;
}

struct mylite_field_descriptor mylite_expression_descriptor_signed_longlong(bool nullable)
{
    struct mylite_field_descriptor descriptor = {
        .type = MYLITE_FIELD_TYPE_LONGLONG,
        .flags = MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
        .length = 2U,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = nullable,
    };

    mylite_field_descriptor_set_nullable(&descriptor, nullable);
    return descriptor;
}

struct mylite_field_descriptor mylite_expression_descriptor_decimal(bool nullable)
{
    struct mylite_field_descriptor descriptor = {
        .type = MYLITE_FIELD_TYPE_NEWDECIMAL,
        .flags = MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
        .length = mylite_mysql_decimal_divide_display_length,
        .decimals = mylite_mysql_decimal_divide_scale,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = nullable,
    };

    mylite_field_descriptor_set_nullable(&descriptor, nullable);
    return descriptor;
}

bool mylite_expression_descriptor_is_nullable(const struct mylite_field_descriptor *descriptor)
{
    if (descriptor == NULL) {
        return true;
    }
    return descriptor->nullable;
}

void mylite_expression_descriptor_set_scalar_subquery_nullable(
    struct mylite_field_descriptor *descriptor)
{
    mylite_field_descriptor_set_nullable(descriptor, true);
}

struct mylite_field_descriptor mylite_expression_descriptor_defaults(void)
{
    return (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_NULL,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = true,
    };
}

unsigned int mylite_expression_descriptor_connection_collation_id(const mylite_db *database)
{
    const struct mylite_collation *collation =
        database == NULL ? NULL : mylite_collation_lookup(database->collation_connection);

    if (collation == NULL) {
        return mylite_expression_descriptor_connection_charset_id(database);
    }
    return (unsigned int)collation->id;
}

unsigned int mylite_expression_descriptor_connection_charset_id(const mylite_db *database)
{
    const struct mylite_charset *charset =
        database == NULL ? NULL : mylite_charset_lookup(database->character_set_results);
    const struct mylite_collation *collation =
        charset == NULL ? NULL : mylite_collation_lookup(charset->default_collation);

    if (collation == NULL) {
        return mylite_mysql_binary_charset_id;
    }
    return (unsigned int)collation->id;
}

const struct mylite_collation *
mylite_expression_descriptor_collation_lookup_id(unsigned int collation_id)
{
    for (size_t index = 0U; index < mylite_collation_count(); ++index) {
        const struct mylite_collation *collation = mylite_collation_at(index);

        if (collation != NULL && collation->id >= 0 &&
            (unsigned int)collation->id == collation_id) {
            return collation;
        }
    }
    return NULL;
}

unsigned int
mylite_expression_descriptor_literal_decimal_scale(const struct mylite_sql_ast_node *expression)
{
    const char *start = expression == NULL ? NULL : expression->span.text;
    size_t length = expression == NULL ? 0U : expression->span.length;

    for (size_t index = 0U; index < length; ++index) {
        if (start[index] == '.') {
            size_t scale = 0U;

            ++index;
            while (index < length && isdigit((unsigned char)start[index])) {
                ++scale;
                ++index;
            }
            return (unsigned int)scale;
        }
    }
    return 0U;
}

uint64_t
mylite_expression_descriptor_literal_integer_length(const struct mylite_sql_ast_node *expression,
                                                    const struct mylite_expression_value *value)
{
    uint64_t length = expression == NULL ? 0U : expression->span.length;

    if (value != NULL && value->kind == MYLITE_EXPRESSION_VALUE_UINT64) {
        return length;
    }
    if (length == 0U) {
        return 2U;
    }
    if (expression->span.text[0] != '-') {
        ++length;
    }
    return mylite_expression_descriptor_max_u64(length, 2U);
}

uint64_t mylite_expression_descriptor_string_length(const mylite_db *database,
                                                    const struct mylite_expression_value *value,
                                                    const struct mylite_sql_ast_node *expression)
{
    uint64_t byte_length = 0U;
    uint64_t max_bytes_per_character =
        mylite_expression_descriptor_connection_character_max_length(database);

    if (value != NULL && value->kind == MYLITE_EXPRESSION_VALUE_TEXT && value->text_value != NULL) {
        byte_length = value->text_length;
    } else if (expression != NULL && expression->span.length >= 2U) {
        byte_length = expression->span.length - 2U;
    }
    return byte_length * max_bytes_per_character;
}

uint64_t mylite_expression_descriptor_connection_character_max_length(const mylite_db *database)
{
    const struct mylite_charset *charset =
        database == NULL ? NULL : mylite_charset_lookup(database->character_set_results);

    return charset == NULL ? 1U : (uint64_t)charset->max_length;
}

uint64_t mylite_expression_descriptor_utf8_display_character_count(const char *text)
{
    uint64_t count = 0U;

    if (text == NULL) {
        return 0U;
    }
    for (const unsigned char *cursor = (const unsigned char *)text; *cursor != '\0'; ++cursor) {
        if ((*cursor & mylite_utf8_continuation_mask) != mylite_utf8_continuation_marker) {
            ++count;
        }
    }
    return count;
}

uint64_t mylite_expression_descriptor_max_u64(uint64_t left, uint64_t right)
{
    return left > right ? left : right;
}
