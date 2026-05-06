#include "mylite_json.h"

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum json_parser_constant {
    json_control_byte_limit = 0x20U,
    json_hex_nibble_shift = 4U,
    json_hex_nibble_mask = 0x0FU,
    json_hex_alpha_offset = 10U,
    json_high_surrogate_min = 0xD800U,
    json_high_surrogate_max = 0xDBFFU,
    json_low_surrogate_min = 0xDC00U,
    json_low_surrogate_max = 0xDFFFU,
    json_surrogate_base = 0x10000U,
    json_surrogate_shift = 10U,
    json_utf8_one_byte_max = 0x7FU,
    json_utf8_two_byte_max = 0x7FFU,
    json_utf8_three_byte_max = 0xFFFFU,
    json_utf8_four_byte_max = 0x10FFFFU,
    json_utf8_continuation_prefix = 0x80U,
    json_utf8_two_byte_prefix = 0xC0U,
    json_utf8_three_byte_prefix = 0xE0U,
    json_utf8_four_byte_prefix = 0xF0U,
    json_utf8_payload_mask = 0x3FU,
    json_utf8_shift_6 = 6U,
    json_utf8_shift_12 = 12U,
    json_utf8_shift_18 = 18U,
};

struct json_parser {
    const char *text;
    size_t length;
    size_t offset;
    struct mylite_json_error *error;
};

static bool parse_value(struct json_parser *parser, enum mylite_json_type *out_type);
static bool parse_object(struct json_parser *parser);
static bool parse_array(struct json_parser *parser);
static bool parse_string(struct json_parser *parser, char **out_text, size_t *out_length);
static bool parse_escape(struct json_parser *parser, char **out_text, size_t *out_length);
static bool parse_unicode_escape(struct json_parser *parser, uint32_t *out_codepoint);
static bool parse_hex_quad(struct json_parser *parser, uint32_t *out_codepoint);
static bool parse_number(struct json_parser *parser, enum mylite_json_type *out_type);
static bool parse_digits(struct json_parser *parser);
static bool parse_literal(struct json_parser *parser, const char *literal);
static bool append_utf8(char **text, size_t *length, uint32_t codepoint);
static bool append_quoted_byte(char **text, size_t *length, unsigned char byte);
static bool append_control_escape(char **text, size_t *length, unsigned char byte);
static bool append_optional_byte(char **text, size_t *length, char byte);
static bool append_optional_utf8(char **text, size_t *length, uint32_t codepoint);
static bool append_byte(char **text, size_t *length, char byte);
static bool append_bytes(char **text, size_t *length, const char *addition, size_t addition_length);
static char *copy_text(const char *text, size_t length);
static void skip_whitespace(struct json_parser *parser);
static bool consume_byte(struct json_parser *parser, char expected);
static bool peek_byte(const struct json_parser *parser, char *out_byte);
static bool at_end(const struct json_parser *parser);
static void set_error(struct json_parser *parser, const char *message, size_t position);
static bool is_hex_digit(char byte);
static uint32_t hex_digit_value(char byte);

bool mylite_json_validate(const char *text, size_t length, enum mylite_json_type *out_type,
                          struct mylite_json_error *out_error)
{
    struct json_parser parser = {.text = text, .length = length, .error = out_error};
    enum mylite_json_type type = MYLITE_JSON_TYPE_INVALID;

    if (out_error != NULL) {
        *out_error = (struct mylite_json_error){0};
    }
    skip_whitespace(&parser);
    if (!parse_value(&parser, &type)) {
        return false;
    }
    skip_whitespace(&parser);
    if (!at_end(&parser)) {
        set_error(&parser, "The document root must not be followed by other values.",
                  parser.offset);
        return false;
    }
    if (out_type != NULL) {
        *out_type = type;
    }
    return true;
}

const char *mylite_json_type_name(enum mylite_json_type type)
{
    switch (type) {
    case MYLITE_JSON_TYPE_NULL:
        return "NULL";
    case MYLITE_JSON_TYPE_BOOLEAN:
        return "BOOLEAN";
    case MYLITE_JSON_TYPE_INTEGER:
        return "INTEGER";
    case MYLITE_JSON_TYPE_DOUBLE:
        return "DOUBLE";
    case MYLITE_JSON_TYPE_STRING:
        return "STRING";
    case MYLITE_JSON_TYPE_ARRAY:
        return "ARRAY";
    case MYLITE_JSON_TYPE_OBJECT:
        return "OBJECT";
    case MYLITE_JSON_TYPE_INVALID:
        return NULL;
    }
    return NULL;
}

int mylite_json_quote_string(const char *text, size_t length, char **out_text, size_t *out_length)
{
    char *result = NULL;
    size_t result_length = 0U;

    if (!append_byte(&result, &result_length, '"')) {
        return -1;
    }
    for (size_t index = 0U; index < length; ++index) {
        unsigned char byte = 0U;

        if (text != NULL) {
            byte = (unsigned char)text[index];
        }
        if (!append_quoted_byte(&result, &result_length, byte)) {
            goto error;
        }
    }
    if (!append_byte(&result, &result_length, '"')) {
        goto error;
    }
    *out_text = result;
    *out_length = result_length;
    return 0;

error:
    free(result);
    return -1;
}

int mylite_json_unquote_string(const char *text, size_t length, char **out_text, size_t *out_length,
                               struct mylite_json_error *out_error)
{
    struct json_parser parser = {.text = text, .length = length, .error = out_error};
    char *result = NULL;
    size_t result_length = 0U;

    if (out_error != NULL) {
        *out_error = (struct mylite_json_error){0};
    }
    if (length < 2U || text == NULL || text[0] != '"' || text[length - 1U] != '"') {
        result = copy_text(text, length);
        if (result == NULL) {
            return -1;
        }
        *out_text = result;
        *out_length = length;
        return 0;
    }
    if (!parse_string(&parser, &result, &result_length) || !at_end(&parser)) {
        free(result);
        return 1;
    }
    *out_text = result;
    *out_length = result_length;
    return 0;
}

// Recursive descent keeps the JSON grammar small and mirrors nested document structure.
// NOLINTNEXTLINE(misc-no-recursion)
static bool parse_value(struct json_parser *parser, enum mylite_json_type *out_type)
{
    char byte = '\0';

    skip_whitespace(parser);
    if (!peek_byte(parser, &byte)) {
        set_error(parser, "Invalid value.", parser->offset);
        return false;
    }
    if (byte == '{') {
        if (!parse_object(parser)) {
            return false;
        }
        *out_type = MYLITE_JSON_TYPE_OBJECT;
        return true;
    }
    if (byte == '[') {
        if (!parse_array(parser)) {
            return false;
        }
        *out_type = MYLITE_JSON_TYPE_ARRAY;
        return true;
    }
    if (byte == '"') {
        if (!parse_string(parser, NULL, NULL)) {
            return false;
        }
        *out_type = MYLITE_JSON_TYPE_STRING;
        return true;
    }
    if (byte == '-' || isdigit((unsigned char)byte)) {
        return parse_number(parser, out_type);
    }
    if (parse_literal(parser, "true") || parse_literal(parser, "false")) {
        *out_type = MYLITE_JSON_TYPE_BOOLEAN;
        return true;
    }
    if (parse_literal(parser, "null")) {
        *out_type = MYLITE_JSON_TYPE_NULL;
        return true;
    }
    set_error(parser, "Invalid value.", parser->offset);
    return false;
}

// NOLINTNEXTLINE(misc-no-recursion)
static bool parse_object(struct json_parser *parser)
{
    if (!consume_byte(parser, '{')) {
        return false;
    }
    skip_whitespace(parser);
    if (consume_byte(parser, '}')) {
        return true;
    }
    for (;;) {
        enum mylite_json_type ignored_type = MYLITE_JSON_TYPE_INVALID;

        if (!parse_string(parser, NULL, NULL)) {
            return false;
        }
        skip_whitespace(parser);
        if (!consume_byte(parser, ':')) {
            set_error(parser, "Missing a colon after a name of object member.", parser->offset);
            return false;
        }
        if (!parse_value(parser, &ignored_type)) {
            return false;
        }
        skip_whitespace(parser);
        if (consume_byte(parser, '}')) {
            return true;
        }
        if (!consume_byte(parser, ',')) {
            set_error(parser, "Missing a comma or '}' after an object member.", parser->offset);
            return false;
        }
        skip_whitespace(parser);
    }
}

// NOLINTNEXTLINE(misc-no-recursion)
static bool parse_array(struct json_parser *parser)
{
    if (!consume_byte(parser, '[')) {
        return false;
    }
    skip_whitespace(parser);
    if (consume_byte(parser, ']')) {
        return true;
    }
    for (;;) {
        enum mylite_json_type ignored_type = MYLITE_JSON_TYPE_INVALID;

        if (!parse_value(parser, &ignored_type)) {
            return false;
        }
        skip_whitespace(parser);
        if (consume_byte(parser, ']')) {
            return true;
        }
        if (!consume_byte(parser, ',')) {
            set_error(parser, "Missing a comma or ']' after an array element.", parser->offset);
            return false;
        }
        skip_whitespace(parser);
    }
}

static bool parse_string(struct json_parser *parser, char **out_text, size_t *out_length)
{
    char *result = NULL;
    size_t result_length = 0U;

    if (!consume_byte(parser, '"')) {
        set_error(parser, "Invalid value.", parser->offset);
        return false;
    }
    while (!at_end(parser)) {
        unsigned char byte = (unsigned char)parser->text[parser->offset++];

        if (byte == '"') {
            if (out_text != NULL) {
                *out_text = result;
                *out_length = result_length;
            } else {
                free(result);
            }
            return true;
        }
        if (byte == '\\') {
            --parser->offset;
            if (!parse_escape(parser, out_text == NULL ? NULL : &result,
                              out_length == NULL ? NULL : &result_length)) {
                free(result);
                return false;
            }
            continue;
        }
        if (byte < json_control_byte_limit) {
            set_error(parser, "Invalid encoding in string.", parser->offset - 1U);
            free(result);
            return false;
        }
        if (out_text != NULL && !append_byte(&result, &result_length, (char)byte)) {
            free(result);
            return false;
        }
    }
    set_error(parser, "Missing a closing quotation mark in string.", parser->offset);
    free(result);
    return false;
}

static bool parse_escape(struct json_parser *parser, char **out_text, size_t *out_length)
{
    size_t escape_position = parser->offset;
    char escaped = '\0';

    if (!consume_byte(parser, '\\') || !peek_byte(parser, &escaped)) {
        set_error(parser, "Invalid escape character in string.", escape_position);
        return false;
    }
    ++parser->offset;
    switch (escaped) {
    case '"':
    case '\\':
    case '/':
        return append_optional_byte(out_text, out_length, escaped);
    case 'b':
        return append_optional_byte(out_text, out_length, '\b');
    case 'f':
        return append_optional_byte(out_text, out_length, '\f');
    case 'n':
        return append_optional_byte(out_text, out_length, '\n');
    case 'r':
        return append_optional_byte(out_text, out_length, '\r');
    case 't':
        return append_optional_byte(out_text, out_length, '\t');
    case 'u': {
        uint32_t codepoint = 0U;

        if (!parse_unicode_escape(parser, &codepoint)) {
            set_error(parser, "Invalid escape character in string.", escape_position);
            return false;
        }
        return append_optional_utf8(out_text, out_length, codepoint);
    }
    default:
        set_error(parser, "Invalid escape character in string.", escape_position);
        return false;
    }
}

static bool parse_unicode_escape(struct json_parser *parser, uint32_t *out_codepoint)
{
    uint32_t codepoint = 0U;

    if (!parse_hex_quad(parser, &codepoint)) {
        return false;
    }
    if (codepoint >= json_high_surrogate_min && codepoint <= json_high_surrogate_max) {
        uint32_t low = 0U;

        if (parser->offset + 2U > parser->length || parser->text[parser->offset] != '\\' ||
            parser->text[parser->offset + 1U] != 'u') {
            return false;
        }
        parser->offset += 2U;
        if (!parse_hex_quad(parser, &low) || low < json_low_surrogate_min ||
            low > json_low_surrogate_max) {
            return false;
        }
        *out_codepoint = json_surrogate_base +
                         ((codepoint - json_high_surrogate_min) << json_surrogate_shift) +
                         (low - json_low_surrogate_min);
        return true;
    }
    if (codepoint >= json_low_surrogate_min && codepoint <= json_low_surrogate_max) {
        return false;
    }
    *out_codepoint = codepoint;
    return true;
}

static bool parse_hex_quad(struct json_parser *parser, uint32_t *out_codepoint)
{
    uint32_t value = 0U;

    if (parser->offset + 4U > parser->length) {
        return false;
    }
    for (size_t index = 0U; index < 4U; ++index) {
        char byte = parser->text[parser->offset + index];

        if (!is_hex_digit(byte)) {
            return false;
        }
        value = (value << 4U) | hex_digit_value(byte);
    }
    parser->offset += 4U;
    *out_codepoint = value;
    return true;
}

static bool parse_number(struct json_parser *parser, enum mylite_json_type *out_type)
{
    bool is_double = false;

    if (consume_byte(parser, '-')) {
        if (at_end(parser)) {
            set_error(parser, "Invalid value.", parser->offset);
            return false;
        }
    }
    if (consume_byte(parser, '0')) {
        char next = '\0';

        if (peek_byte(parser, &next) && isdigit((unsigned char)next)) {
            set_error(parser, "Invalid value.", parser->offset);
            return false;
        }
    } else if (!parse_digits(parser)) {
        set_error(parser, "Invalid value.", parser->offset);
        return false;
    }
    if (consume_byte(parser, '.')) {
        is_double = true;
        if (!parse_digits(parser)) {
            set_error(parser, "Invalid value.", parser->offset);
            return false;
        }
    }
    if (!at_end(parser) &&
        (parser->text[parser->offset] == 'e' || parser->text[parser->offset] == 'E')) {
        is_double = true;
        ++parser->offset;
        if (!at_end(parser) &&
            (parser->text[parser->offset] == '+' || parser->text[parser->offset] == '-')) {
            ++parser->offset;
        }
        if (!parse_digits(parser)) {
            set_error(parser, "Invalid value.", parser->offset);
            return false;
        }
    }
    if (is_double) {
        *out_type = MYLITE_JSON_TYPE_DOUBLE;
    } else {
        *out_type = MYLITE_JSON_TYPE_INTEGER;
    }
    return true;
}

static bool parse_digits(struct json_parser *parser)
{
    size_t start = parser->offset;

    while (!at_end(parser) && isdigit((unsigned char)parser->text[parser->offset])) {
        ++parser->offset;
    }
    return parser->offset > start;
}

static bool parse_literal(struct json_parser *parser, const char *literal)
{
    size_t length = strlen(literal);

    if (parser->offset + length > parser->length ||
        memcmp(parser->text + parser->offset, literal, length) != 0) {
        return false;
    }
    parser->offset += length;
    return true;
}

static bool append_utf8(char **text, size_t *length, uint32_t codepoint)
{
    if (codepoint <= json_utf8_one_byte_max) {
        return append_byte(text, length, (char)codepoint);
    }
    if (codepoint <= json_utf8_two_byte_max) {
        char bytes[] = {
            (char)(json_utf8_two_byte_prefix | (codepoint >> json_utf8_shift_6)),
            (char)(json_utf8_continuation_prefix | (codepoint & json_utf8_payload_mask))};

        return append_bytes(text, length, bytes, sizeof(bytes));
    }
    if (codepoint <= json_utf8_three_byte_max) {
        char bytes[] = {
            (char)(json_utf8_three_byte_prefix | (codepoint >> json_utf8_shift_12)),
            (char)(json_utf8_continuation_prefix |
                   ((codepoint >> json_utf8_shift_6) & json_utf8_payload_mask)),
            (char)(json_utf8_continuation_prefix | (codepoint & json_utf8_payload_mask))};

        return append_bytes(text, length, bytes, sizeof(bytes));
    }
    if (codepoint <= json_utf8_four_byte_max) {
        char bytes[] = {
            (char)(json_utf8_four_byte_prefix | (codepoint >> json_utf8_shift_18)),
            (char)(json_utf8_continuation_prefix |
                   ((codepoint >> json_utf8_shift_12) & json_utf8_payload_mask)),
            (char)(json_utf8_continuation_prefix |
                   ((codepoint >> json_utf8_shift_6) & json_utf8_payload_mask)),
            (char)(json_utf8_continuation_prefix | (codepoint & json_utf8_payload_mask))};

        return append_bytes(text, length, bytes, sizeof(bytes));
    }
    return false;
}

static bool append_quoted_byte(char **text, size_t *length, unsigned char byte)
{
    switch (byte) {
    case '"':
        return append_bytes(text, length, "\\\"", 2U);
    case '\\':
        return append_bytes(text, length, "\\\\", 2U);
    case '\b':
        return append_bytes(text, length, "\\b", 2U);
    case '\f':
        return append_bytes(text, length, "\\f", 2U);
    case '\n':
        return append_bytes(text, length, "\\n", 2U);
    case '\r':
        return append_bytes(text, length, "\\r", 2U);
    case '\t':
        return append_bytes(text, length, "\\t", 2U);
    default:
        if (byte < json_control_byte_limit) {
            return append_control_escape(text, length, byte);
        }
        return append_byte(text, length, (char)byte);
    }
}

static bool append_control_escape(char **text, size_t *length, unsigned char byte)
{
    static const char hex[] = "0123456789abcdef";
    char escape[] = {
        '\\', 'u', '0', '0', hex[byte >> json_hex_nibble_shift], hex[byte & json_hex_nibble_mask]};

    return append_bytes(text, length, escape, sizeof(escape));
}

static bool append_optional_byte(char **text, size_t *length, char byte)
{
    if (text == NULL) {
        return true;
    }
    return append_byte(text, length, byte);
}

static bool append_optional_utf8(char **text, size_t *length, uint32_t codepoint)
{
    if (text == NULL) {
        return true;
    }
    return append_utf8(text, length, codepoint);
}

static bool append_byte(char **text, size_t *length, char byte)
{
    return append_bytes(text, length, &byte, 1U);
}

static bool append_bytes(char **text, size_t *length, const char *addition, size_t addition_length)
{
    char *updated = NULL;

    if (text == NULL || length == NULL || addition_length > SIZE_MAX - *length - 1U) {
        return false;
    }
    updated = realloc(*text, *length + addition_length + 1U);
    if (updated == NULL) {
        return false;
    }
    if (addition_length != 0U) {
        memcpy(updated + *length, addition, addition_length);
    }
    *text = updated;
    *length += addition_length;
    (*text)[*length] = '\0';
    return true;
}

static char *copy_text(const char *text, size_t length)
{
    char *copy = malloc(length + 1U);

    if (copy == NULL) {
        return NULL;
    }
    if (length != 0U) {
        memcpy(copy, text == NULL ? "" : text, length);
    }
    copy[length] = '\0';
    return copy;
}

static void skip_whitespace(struct json_parser *parser)
{
    while (!at_end(parser) && isspace((unsigned char)parser->text[parser->offset])) {
        ++parser->offset;
    }
}

static bool consume_byte(struct json_parser *parser, char expected)
{
    if (at_end(parser) || parser->text[parser->offset] != expected) {
        return false;
    }
    ++parser->offset;
    return true;
}

static bool peek_byte(const struct json_parser *parser, char *out_byte)
{
    if (at_end(parser)) {
        return false;
    }
    *out_byte = parser->text[parser->offset];
    return true;
}

static bool at_end(const struct json_parser *parser)
{
    if (parser == NULL || parser->text == NULL) {
        return true;
    }
    return parser->offset >= parser->length;
}

static void set_error(struct json_parser *parser, const char *message, size_t position)
{
    if (parser != NULL && parser->error != NULL && parser->error->message == NULL) {
        *parser->error = (struct mylite_json_error){.message = message, .position = position};
    }
}

static bool is_hex_digit(char byte)
{
    if (byte >= '0' && byte <= '9') {
        return true;
    }
    if (byte >= 'a' && byte <= 'f') {
        return true;
    }
    if (byte >= 'A' && byte <= 'F') {
        return true;
    }
    return false;
}

static uint32_t hex_digit_value(char byte)
{
    if (byte >= '0' && byte <= '9') {
        return (uint32_t)(byte - '0');
    }
    if (byte >= 'a' && byte <= 'f') {
        return (uint32_t)(byte - 'a' + json_hex_alpha_offset);
    }
    return (uint32_t)(byte - 'A' + json_hex_alpha_offset);
}
