#include "mylite_expression_descriptor_scalar.h"

#include "mylite_expression_descriptor.h"
#include "mylite_function_names.h"
#include "mylite_metadata_constants.h"
#include "mylite_span.h"
#include "sql/mylite_ast.h"
#include "sql/mylite_digest.h"
#include "sql/mylite_expression.h"

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

static bool infer_session_function_descriptor(mylite_db *database,
                                              const struct mylite_sql_ast_node *name,
                                              struct mylite_field_descriptor *out_descriptor);
static bool infer_inet_function_descriptor(mylite_db *database,
                                           const struct mylite_sql_ast_node *name,
                                           struct mylite_field_descriptor *out_descriptor);
static uint64_t hash_function_result_chars(const struct mylite_sql_ast_node *expression,
                                           const struct mylite_expression_value *value);
static uint64_t sha2_function_result_chars(const struct mylite_sql_ast_node *expression,
                                           const struct mylite_expression_value *value);
static bool sha2_literal_hash_length(const struct mylite_sql_ast_node *argument,
                                     uint64_t *out_bits);
static uint64_t sha2_result_chars_from_bits(uint64_t bits);

bool mylite_expression_descriptor_function_result_nullable(
    bool arguments_nullable, const struct mylite_expression_value *value)
{
    if (value != NULL) {
        return value->kind == MYLITE_EXPRESSION_VALUE_NULL;
    }
    return arguments_nullable;
}

bool mylite_expression_descriptor_infer_text_function(
    mylite_db *database, const struct mylite_sql_ast_node *name,
    const struct mylite_expression_value *value, bool result_nullable,
    struct mylite_field_descriptor *out_descriptor)
{
    uint64_t length = mylite_mysql_text_length;

    if (!mylite_function_name_has_text_result(name)) {
        return false;
    }
    if (value != NULL && value->kind == MYLITE_EXPRESSION_VALUE_TEXT) {
        length = mylite_expression_descriptor_string_length(database, value, NULL);
    }

    *out_descriptor = (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_VAR_STRING,
        .flags = 0U,
        .length = length,
        .decimals = mylite_mysql_not_fixed_decimals,
        .charset_id = mylite_expression_descriptor_connection_charset_id(database),
        .nullable = result_nullable,
    };
    mylite_field_descriptor_set_nullable(out_descriptor, result_nullable);
    return true;
}

bool mylite_expression_descriptor_infer_base_conversion_function(
    mylite_db *database, const struct mylite_sql_ast_node *name,
    struct mylite_field_descriptor *out_descriptor)
{
    uint64_t max_bytes_per_character =
        mylite_expression_descriptor_connection_character_max_length(database);
    uint64_t length =
        max_bytes_per_character > UINT64_MAX / mylite_mysql_base_conversion_result_chars
            ? mylite_mysql_long_text_length
            : mylite_mysql_base_conversion_result_chars * max_bytes_per_character;

    if (!mylite_function_name_has_base_conversion_result(name)) {
        return false;
    }
    *out_descriptor = (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_VAR_STRING,
        .flags = 0U,
        .length = length,
        .decimals = mylite_mysql_not_fixed_decimals,
        .charset_id = mylite_expression_descriptor_connection_charset_id(database),
        .nullable = true,
    };
    mylite_field_descriptor_set_nullable(out_descriptor, true);
    return true;
}

bool mylite_expression_descriptor_infer_hash_function(
    mylite_db *database, const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_value *value, struct mylite_field_descriptor *out_descriptor)
{
    const struct mylite_sql_ast_node *name = mylite_ast_child_at(expression, 0U);
    uint64_t max_bytes_per_character =
        mylite_expression_descriptor_connection_character_max_length(database);
    uint64_t result_chars = 0U;
    uint64_t length = 0U;
    unsigned int charset_id = mylite_expression_descriptor_connection_charset_id(database);

    if (!mylite_function_name_is_hash(name)) {
        return false;
    }

    result_chars = hash_function_result_chars(expression, value);
    length = max_bytes_per_character > UINT64_MAX / result_chars
                 ? mylite_mysql_long_text_length
                 : result_chars * max_bytes_per_character;
    *out_descriptor = (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_VAR_STRING,
        .flags = charset_id == mylite_mysql_binary_charset_id ? MYLITE_FIELD_FLAG_BINARY : 0U,
        .length = length,
        .decimals = mylite_mysql_not_fixed_decimals,
        .charset_id = charset_id,
        .nullable = true,
    };
    mylite_field_descriptor_set_nullable(out_descriptor, true);
    return true;
}

bool mylite_expression_descriptor_infer_session_or_inet_function(
    mylite_db *database, const struct mylite_sql_ast_node *name,
    struct mylite_field_descriptor *out_descriptor)
{
    if (infer_session_function_descriptor(database, name, out_descriptor)) {
        return true;
    }
    return infer_inet_function_descriptor(database, name, out_descriptor);
}

bool mylite_expression_descriptor_infer_strcmp_function(
    const struct mylite_sql_ast_node *name, bool result_nullable,
    struct mylite_field_descriptor *out_descriptor)
{
    if (!mylite_function_name_is_strcmp(name)) {
        return false;
    }

    *out_descriptor = mylite_expression_descriptor_signed_longlong(result_nullable);
    out_descriptor->length = 2U;
    return true;
}

bool mylite_expression_descriptor_infer_uuid_function(
    mylite_db *database, const struct mylite_sql_ast_node *name,
    struct mylite_field_descriptor *out_descriptor)
{
    if (mylite_function_name_is_is_uuid(name)) {
        *out_descriptor = mylite_expression_descriptor_signed_longlong(true);
        out_descriptor->length = 1U;
        return true;
    }
    if (mylite_function_name_is_uuid_to_bin(name)) {
        *out_descriptor = (struct mylite_field_descriptor){
            .type = MYLITE_FIELD_TYPE_VAR_STRING,
            .flags = MYLITE_FIELD_FLAG_BINARY,
            .length = mylite_mysql_uuid_binary_result_bytes,
            .decimals = mylite_mysql_not_fixed_decimals,
            .charset_id = mylite_mysql_binary_charset_id,
            .nullable = true,
        };
        mylite_field_descriptor_set_nullable(out_descriptor, true);
        return true;
    }
    if (mylite_function_name_is_uuid(name) || mylite_function_name_is_bin_to_uuid(name)) {
        uint64_t max_bytes_per_character =
            mylite_expression_descriptor_connection_character_max_length(database);
        uint64_t length = max_bytes_per_character > UINT64_MAX / mylite_mysql_uuid_text_result_chars
                              ? mylite_mysql_long_text_length
                              : mylite_mysql_uuid_text_result_chars * max_bytes_per_character;

        *out_descriptor = (struct mylite_field_descriptor){
            .type = MYLITE_FIELD_TYPE_VAR_STRING,
            .flags = 0U,
            .length = length,
            .decimals = mylite_mysql_not_fixed_decimals,
            .charset_id = mylite_expression_descriptor_connection_charset_id(database),
            .nullable = true,
        };
        mylite_field_descriptor_set_nullable(out_descriptor, true);
        return true;
    }
    return false;
}

bool mylite_expression_descriptor_infer_list_index_function(
    const struct mylite_sql_ast_node *name, bool nullable,
    struct mylite_field_descriptor *out_descriptor)
{
    if (mylite_function_name_is_field(name)) {
        *out_descriptor = mylite_expression_descriptor_signed_longlong(false);
        out_descriptor->length = mylite_mysql_list_index_function_display_length;
        return true;
    }
    if (mylite_function_name_is_find_in_set(name)) {
        *out_descriptor = mylite_expression_descriptor_signed_longlong(nullable);
        out_descriptor->length = mylite_mysql_list_index_function_display_length;
        return true;
    }
    return false;
}

bool mylite_expression_descriptor_infer_code_search_function(
    const struct mylite_sql_ast_node *name, bool nullable,
    struct mylite_field_descriptor *out_descriptor)
{
    if (mylite_function_name_is_ascii(name)) {
        *out_descriptor = mylite_expression_descriptor_signed_longlong(nullable);
        out_descriptor->length = mylite_mysql_ascii_function_display_length;
        return true;
    }
    if (mylite_function_name_is_ord(name)) {
        *out_descriptor = mylite_expression_descriptor_signed_longlong(nullable);
        out_descriptor->length = mylite_mysql_ord_function_display_length;
        return true;
    }
    if (mylite_function_name_has_search_result(name)) {
        *out_descriptor = mylite_expression_descriptor_signed_longlong(nullable);
        out_descriptor->length = mylite_mysql_search_function_display_length;
        return true;
    }
    return false;
}

static uint64_t hash_function_result_chars(const struct mylite_sql_ast_node *expression,
                                           const struct mylite_expression_value *value)
{
    const struct mylite_sql_ast_node *name = mylite_ast_child_at(expression, 0U);

    if (mylite_function_name_is_md5(name)) {
        return MYLITE_DIGEST_MD5_HEX_LENGTH;
    }
    if (mylite_function_name_is_sha1(name)) {
        return MYLITE_DIGEST_SHA1_HEX_LENGTH;
    }
    return sha2_function_result_chars(expression, value);
}

static uint64_t sha2_function_result_chars(const struct mylite_sql_ast_node *expression,
                                           const struct mylite_expression_value *value)
{
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    const struct mylite_sql_ast_node *length_argument = mylite_ast_child_at(arguments, 1U);
    uint64_t bits = 0U;

    if (value != NULL && value->kind == MYLITE_EXPRESSION_VALUE_TEXT) {
        return value->text_length;
    }
    if (sha2_literal_hash_length(length_argument, &bits)) {
        return sha2_result_chars_from_bits(bits);
    }
    if (length_argument != NULL && length_argument->kind == MYLITE_SQL_AST_LITERAL) {
        return MYLITE_DIGEST_SHA2_256_HEX_LENGTH;
    }
    return MYLITE_DIGEST_SHA2_512_HEX_LENGTH;
}

static bool sha2_literal_hash_length(const struct mylite_sql_ast_node *argument, uint64_t *out_bits)
{
    char *text = NULL;
    char *end = NULL;
    uint64_t bits = 0U;

    if (argument == NULL || argument->kind != MYLITE_SQL_AST_LITERAL || out_bits == NULL) {
        return false;
    }
    if (argument->literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        *out_bits = UINT64_MAX;
        return true;
    }
    if (argument->literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        return false;
    }
    text = mylite_copy_span_text(argument->span.text, argument->span.length);
    if (text == NULL) {
        return false;
    }
    errno = 0;
    bits = strtoull(text, &end, 10);
    if (errno != 0 || end == text || (end != NULL && *end != '\0')) {
        free(text);
        return false;
    }
    free(text);
    *out_bits = bits;
    return true;
}

static uint64_t sha2_result_chars_from_bits(uint64_t bits)
{
    switch (bits) {
    case 224U:
        return MYLITE_DIGEST_SHA2_224_HEX_LENGTH;
    case 0U:
    case 256U:
        return MYLITE_DIGEST_SHA2_256_HEX_LENGTH;
    case 384U:
        return MYLITE_DIGEST_SHA2_384_HEX_LENGTH;
    case 512U:
        return MYLITE_DIGEST_SHA2_512_HEX_LENGTH;
    default:
        return MYLITE_DIGEST_SHA2_256_HEX_LENGTH;
    }
}

static bool infer_session_function_descriptor(mylite_db *database,
                                              const struct mylite_sql_ast_node *name,
                                              struct mylite_field_descriptor *out_descriptor)
{
    if (name == NULL) {
        return false;
    }
    if (mylite_function_name_is_charset(name) || mylite_function_name_is_collation(name)) {
        uint64_t max_bytes_per_character =
            mylite_expression_descriptor_connection_character_max_length(database);
        uint64_t length =
            max_bytes_per_character >
                    UINT64_MAX / mylite_mysql_charset_collation_function_display_chars
                ? mylite_mysql_long_text_length
                : mylite_mysql_charset_collation_function_display_chars * max_bytes_per_character;

        *out_descriptor = (struct mylite_field_descriptor){
            .type = MYLITE_FIELD_TYPE_VAR_STRING,
            .flags = 0U,
            .length = length,
            .decimals = mylite_mysql_not_fixed_decimals,
            .charset_id = mylite_expression_descriptor_connection_collation_id(database),
            .nullable = true,
        };
        return true;
    }
    if (mylite_function_name_is_coercibility(name)) {
        *out_descriptor = (struct mylite_field_descriptor){
            .type = MYLITE_FIELD_TYPE_LONGLONG,
            .flags = MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
            .length = mylite_mysql_coercibility_function_display_length,
            .charset_id = mylite_mysql_binary_charset_id,
            .nullable = false,
        };
        return true;
    }
    if (mylite_span_equal_ci(name->span, "DATABASE") ||
        mylite_span_equal_ci(name->span, "SCHEMA")) {
        *out_descriptor = (struct mylite_field_descriptor){
            .type = MYLITE_FIELD_TYPE_VAR_STRING,
            .flags = 0U,
            .length = mylite_mysql_schema_function_display_length,
            .decimals = mylite_mysql_not_fixed_decimals,
            .charset_id = mylite_expression_descriptor_connection_charset_id(database),
            .nullable = true,
        };
        return true;
    }
    if (mylite_span_equal_ci(name->span, "VERSION")) {
        *out_descriptor = (struct mylite_field_descriptor){
            .type = MYLITE_FIELD_TYPE_VAR_STRING,
            .flags = MYLITE_FIELD_FLAG_NOT_NULL,
            .length = mylite_mysql_version_function_display_length,
            .decimals = mylite_mysql_not_fixed_decimals,
            .charset_id = mylite_expression_descriptor_connection_charset_id(database),
            .nullable = false,
        };
        return true;
    }
    if (mylite_span_equal_ci(name->span, "USER") ||
        mylite_span_equal_ci(name->span, "SESSION_USER") ||
        mylite_span_equal_ci(name->span, "SYSTEM_USER") ||
        mylite_span_equal_ci(name->span, "CURRENT_USER")) {
        uint64_t max_bytes_per_character =
            mylite_expression_descriptor_connection_character_max_length(database);
        uint64_t length =
            max_bytes_per_character > UINT64_MAX / mylite_mysql_identity_function_display_chars
                ? mylite_mysql_long_text_length
                : mylite_mysql_identity_function_display_chars * max_bytes_per_character;

        *out_descriptor = (struct mylite_field_descriptor){
            .type = MYLITE_FIELD_TYPE_VAR_STRING,
            .flags = 0U,
            .length = length,
            .decimals = mylite_mysql_not_fixed_decimals,
            .charset_id = mylite_expression_descriptor_connection_charset_id(database),
            .nullable = true,
        };
        return true;
    }
    if (mylite_span_equal_ci(name->span, "CURRENT_ROLE")) {
        uint64_t max_bytes_per_character =
            mylite_expression_descriptor_connection_character_max_length(database);
        uint64_t length =
            max_bytes_per_character > UINT64_MAX / mylite_mysql_current_role_function_display_chars
                ? mylite_mysql_long_text_length
                : mylite_mysql_current_role_function_display_chars * max_bytes_per_character;

        *out_descriptor = (struct mylite_field_descriptor){
            .type = MYLITE_FIELD_TYPE_BLOB,
            .flags = 0U,
            .length = length,
            .decimals = mylite_mysql_not_fixed_decimals,
            .charset_id = mylite_expression_descriptor_connection_charset_id(database),
            .nullable = true,
        };
        return true;
    }
    if (mylite_span_equal_ci(name->span, "LAST_INSERT_ID")) {
        *out_descriptor = (struct mylite_field_descriptor){
            .type = MYLITE_FIELD_TYPE_LONGLONG,
            .flags = MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_UNSIGNED |
                     MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
            .length = mylite_mysql_session_integer_function_display_length,
            .charset_id = mylite_mysql_binary_charset_id,
            .nullable = false,
        };
        return true;
    }
    if (mylite_span_equal_ci(name->span, "CONNECTION_ID")) {
        *out_descriptor = (struct mylite_field_descriptor){
            .type = MYLITE_FIELD_TYPE_LONGLONG,
            .flags = MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_UNSIGNED |
                     MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
            .length = mylite_mysql_session_integer_function_display_length,
            .charset_id = mylite_mysql_binary_charset_id,
            .nullable = false,
        };
        return true;
    }
    if (mylite_function_name_is_uuid_short(name)) {
        *out_descriptor = (struct mylite_field_descriptor){
            .type = MYLITE_FIELD_TYPE_LONGLONG,
            .flags = MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_UNSIGNED |
                     MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
            .length = mylite_mysql_session_integer_function_display_length,
            .charset_id = mylite_mysql_binary_charset_id,
            .nullable = false,
        };
        return true;
    }
    if (mylite_span_equal_ci(name->span, "GET_LOCK") ||
        mylite_span_equal_ci(name->span, "IS_FREE_LOCK") ||
        mylite_span_equal_ci(name->span, "RELEASE_LOCK")) {
        *out_descriptor = mylite_expression_descriptor_signed_longlong(true);
        out_descriptor->length = 1U;
        return true;
    }
    if (mylite_span_equal_ci(name->span, "IS_USED_LOCK")) {
        *out_descriptor = mylite_expression_descriptor_unsigned_longlong(true);
        out_descriptor->length = mylite_mysql_session_integer_function_display_length;
        return true;
    }
    if (mylite_span_equal_ci(name->span, "RELEASE_ALL_LOCKS")) {
        *out_descriptor = (struct mylite_field_descriptor){
            .type = MYLITE_FIELD_TYPE_LONGLONG,
            .flags = MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_UNSIGNED |
                     MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
            .length = mylite_mysql_session_integer_function_display_length,
            .charset_id = mylite_mysql_binary_charset_id,
            .nullable = false,
        };
        return true;
    }
    if (mylite_span_equal_ci(name->span, "ROW_COUNT")) {
        *out_descriptor = (struct mylite_field_descriptor){
            .type = MYLITE_FIELD_TYPE_LONGLONG,
            .flags = MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
            .length = mylite_mysql_session_integer_function_display_length,
            .charset_id = mylite_mysql_binary_charset_id,
            .nullable = false,
        };
        return true;
    }
    if (mylite_span_equal_ci(name->span, "FOUND_ROWS")) {
        *out_descriptor = (struct mylite_field_descriptor){
            .type = MYLITE_FIELD_TYPE_LONGLONG,
            .flags = MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_UNSIGNED |
                     MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
            .length = mylite_mysql_session_integer_function_display_length,
            .charset_id = mylite_mysql_binary_charset_id,
            .nullable = false,
        };
        return true;
    }
    return false;
}

static bool infer_inet_function_descriptor(mylite_db *database,
                                           const struct mylite_sql_ast_node *name,
                                           struct mylite_field_descriptor *out_descriptor)
{
    if (mylite_function_name_is_inet_aton(name)) {
        *out_descriptor = mylite_expression_descriptor_unsigned_longlong(true);
        out_descriptor->length = mylite_mysql_signed_longlong_display_length;
        return true;
    }
    if (mylite_function_name_is_inet_ntoa(name)) {
        uint64_t max_bytes_per_character =
            mylite_expression_descriptor_connection_character_max_length(database);
        uint64_t length = max_bytes_per_character > UINT64_MAX / mylite_mysql_inet_ntoa_result_chars
                              ? mylite_mysql_long_text_length
                              : mylite_mysql_inet_ntoa_result_chars * max_bytes_per_character;

        *out_descriptor = (struct mylite_field_descriptor){
            .type = MYLITE_FIELD_TYPE_VAR_STRING,
            .flags = 0U,
            .length = length,
            .decimals = mylite_mysql_not_fixed_decimals,
            .charset_id = mylite_expression_descriptor_connection_charset_id(database),
            .nullable = true,
        };
        mylite_field_descriptor_set_nullable(out_descriptor, true);
        return true;
    }
    return false;
}
