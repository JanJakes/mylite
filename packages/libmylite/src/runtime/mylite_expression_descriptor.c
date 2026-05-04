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
