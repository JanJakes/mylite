#include <mylite/mylite.h>

#include "mylite_json_internal.h"

#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

struct json_parsed_value {
    struct json_value value;
    bool opens_container;
};

struct json_parse_frame {
    struct json_value *container;
};

struct json_parse_stack {
    struct json_parse_frame frames[json_max_nesting_depth];
    size_t count;
};

struct json_number_integer_part {
    size_t start;
    size_t end;
    bool is_negative;
};

static int parse_next_value(struct json_parser *parser, struct json_parsed_value *out_value);
static int parse_next_object_member(struct json_parser *parser, struct json_parse_stack *stack);
static int parse_next_array_value(struct json_parser *parser, struct json_parse_stack *stack);
static int finish_completed_value(struct json_parser *parser, struct json_parse_stack *stack);
static void close_completed_container(struct json_parse_stack *stack);
static struct json_parse_frame *parse_stack_top(struct json_parse_stack *stack);
static int parse_stack_push(
    struct json_parser *parser,
    struct json_parse_stack *stack,
    struct json_value *container
);
static int parse_string_value(struct json_parser *parser, struct json_value *out_value);
static int append_string_byte(
    struct json_parser *parser,
    struct json_writer *string,
    unsigned char byte
);
static int append_string_escape(
    struct json_parser *parser,
    struct json_writer *string,
    size_t escape_position
);
static int parse_hex_digit(struct json_parser *parser, size_t position, unsigned int *out_digit);
static int append_ascii_codepoint(
    struct json_parser *parser,
    size_t position,
    struct json_writer *string,
    unsigned int codepoint
);
static int parse_number(struct json_parser *parser, struct json_value *out_value);
static int parse_number_integer_part(
    struct json_parser *parser,
    size_t start,
    struct json_number_integer_part *out_part
);
static int parse_number_fraction_part(struct json_parser *parser, bool *out_has_fraction);
static int parse_number_exponent_part(struct json_parser *parser, bool *out_has_exponent);
static int parse_integer_number(
    struct json_parser *parser,
    size_t start,
    bool is_negative,
    size_t integer_start,
    size_t integer_end,
    struct json_value *out_value
);
static int parse_floating_number(
    struct json_parser *parser,
    size_t start,
    struct json_value *out_value
);
static bool integer_number_is_in_signed_range(
    const char *digits,
    size_t digit_count,
    bool is_negative
);
static int copy_number_text(
    const char *text,
    size_t length,
    bool negative_zero,
    enum json_number_kind number_kind,
    struct json_value *out_value
);
static int parse_literal(
    struct json_parser *parser,
    const char *literal,
    enum json_value_kind kind,
    bool boolean,
    struct json_value *out_value
);

int mylite_json_internal_parse_document(struct json_parser *parser, struct json_value *out_value) {
    struct json_parse_stack stack = {0};
    struct json_parsed_value parsed = {0};
    int rc = MYLITE_OK;

    mylite_json_internal_skip_whitespace(parser);
    rc = parse_next_value(parser, &parsed);
    if (rc != MYLITE_OK) {
        return rc;
    }
    *out_value = parsed.value;
    parsed.value = (struct json_value){0};

    if (parsed.opens_container) {
        rc = parse_stack_push(parser, &stack, out_value);
    } else {
        rc = finish_completed_value(parser, &stack);
    }
    while (rc == MYLITE_OK && stack.count > 0U) {
        struct json_parse_frame *frame = parse_stack_top(&stack);

        if (frame->container->kind == JSON_VALUE_OBJECT) {
            rc = parse_next_object_member(parser, &stack);
        } else {
            rc = parse_next_array_value(parser, &stack);
        }
    }

    return rc;
}

static int parse_next_value(struct json_parser *parser, struct json_parsed_value *out_value) {
    char byte = mylite_json_internal_parser_peek(parser);

    *out_value = (struct json_parsed_value){0};
    if (byte == '{') {
        mylite_json_internal_parser_match(parser, '{');
        out_value->value.kind = JSON_VALUE_OBJECT;
        mylite_json_internal_skip_whitespace(parser);
        if (!mylite_json_internal_parser_match(parser, '}')) {
            out_value->opens_container = true;
        }
        return MYLITE_OK;
    }
    if (byte == '[') {
        mylite_json_internal_parser_match(parser, '[');
        out_value->value.kind = JSON_VALUE_ARRAY;
        mylite_json_internal_skip_whitespace(parser);
        if (!mylite_json_internal_parser_match(parser, ']')) {
            out_value->opens_container = true;
        }
        return MYLITE_OK;
    }
    if (byte == '"') {
        return parse_string_value(parser, &out_value->value);
    }
    if (byte == '-' || (byte >= '0' && byte <= '9')) {
        return parse_number(parser, &out_value->value);
    }
    if (byte == 'n') {
        return parse_literal(parser, "null", JSON_VALUE_NULL, false, &out_value->value);
    }
    if (byte == 't') {
        return parse_literal(parser, "true", JSON_VALUE_BOOL, true, &out_value->value);
    }
    if (byte == 'f') {
        return parse_literal(parser, "false", JSON_VALUE_BOOL, false, &out_value->value);
    }

    return mylite_json_internal_parser_invalid(parser, parser->position);
}

static int parse_next_object_member(struct json_parser *parser, struct json_parse_stack *stack) {
    struct json_parse_frame *frame = parse_stack_top(stack);
    struct json_object *object = &frame->container->payload.object;
    struct json_parsed_value parsed = {0};
    struct json_value *stored_value = NULL;
    char *key = NULL;
    size_t key_length = 0U;
    int rc = MYLITE_OK;

    if (mylite_json_internal_parser_peek(parser) != '"') {
        return mylite_json_internal_parser_invalid_with_detail(
            parser,
            parser->position,
            MYLITE_JSON_ERROR_MISSING_OBJECT_MEMBER_NAME
        );
    }
    rc = mylite_json_internal_parse_string(parser, &key, &key_length);
    if (rc == MYLITE_OK) {
        mylite_json_internal_skip_whitespace(parser);
        if (!mylite_json_internal_parser_match(parser, ':')) {
            rc = mylite_json_internal_parser_invalid(parser, parser->position);
        }
    }
    if (rc == MYLITE_OK) {
        mylite_json_internal_skip_whitespace(parser);
        rc = parse_next_value(parser, &parsed);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_json_internal_object_append_member(
            object,
            key,
            key_length,
            &parsed.value,
            &stored_value
        );
        key = NULL;
    }
    if (rc == MYLITE_OK) {
        if (parsed.opens_container) {
            rc = parse_stack_push(parser, stack, stored_value);
        } else {
            rc = finish_completed_value(parser, stack);
        }
    }

    free(key);
    if (rc != MYLITE_OK) {
        mylite_json_internal_value_deinit(&parsed.value);
    }
    return rc;
}

static int parse_next_array_value(struct json_parser *parser, struct json_parse_stack *stack) {
    struct json_parse_frame *frame = parse_stack_top(stack);
    struct json_array *array = &frame->container->payload.array;
    struct json_parsed_value parsed = {0};
    struct json_value *stored_value = NULL;
    int rc = parse_next_value(parser, &parsed);

    if (rc == MYLITE_OK) {
        rc = mylite_json_internal_array_append_value(array, &parsed.value, &stored_value);
    }
    if (rc == MYLITE_OK) {
        if (parsed.opens_container) {
            rc = parse_stack_push(parser, stack, stored_value);
        } else {
            rc = finish_completed_value(parser, stack);
        }
    }
    if (rc != MYLITE_OK) {
        mylite_json_internal_value_deinit(&parsed.value);
    }
    return rc;
}

static int finish_completed_value(struct json_parser *parser, struct json_parse_stack *stack) {
    while (true) {
        struct json_parse_frame *frame = NULL;

        mylite_json_internal_skip_whitespace(parser);
        frame = parse_stack_top(stack);
        if (frame == NULL) {
            if (mylite_json_internal_parser_at_end(parser)) {
                return MYLITE_OK;
            }
            return mylite_json_internal_parser_invalid(parser, parser->position);
        }
        if (frame->container->kind == JSON_VALUE_OBJECT &&
            mylite_json_internal_parser_match(parser, '}')) {
            close_completed_container(stack);
            continue;
        }
        if (frame->container->kind == JSON_VALUE_ARRAY &&
            mylite_json_internal_parser_match(parser, ']')) {
            close_completed_container(stack);
            continue;
        }
        if (!mylite_json_internal_parser_match(parser, ',')) {
            return mylite_json_internal_parser_invalid(parser, parser->position);
        }
        mylite_json_internal_skip_whitespace(parser);
        return MYLITE_OK;
    }
}

static void close_completed_container(struct json_parse_stack *stack) {
    struct json_parse_frame *frame = parse_stack_top(stack);

    if (frame == NULL) {
        return;
    }
    if (frame->container->kind == JSON_VALUE_OBJECT) {
        mylite_json_internal_sort_object_members_by_mysql_display_order(
            &frame->container->payload.object
        );
    }
    --stack->count;
}

static struct json_parse_frame *parse_stack_top(struct json_parse_stack *stack) {
    if (stack->count == 0U) {
        return NULL;
    }
    return &stack->frames[stack->count - 1U];
}

static int parse_stack_push(
    struct json_parser *parser,
    struct json_parse_stack *stack,
    struct json_value *container
) {
    if (stack->count >= json_max_nesting_depth) {
        return mylite_json_internal_parser_unsupported(parser, parser->position);
    }
    stack->frames[stack->count] = (struct json_parse_frame){.container = container};
    ++stack->count;
    return MYLITE_OK;
}

static int parse_string_value(struct json_parser *parser, struct json_value *out_value) {
    char *text = NULL;
    size_t text_length = 0U;
    int rc = mylite_json_internal_parse_string(parser, &text, &text_length);

    if (rc != MYLITE_OK) {
        return rc;
    }

    out_value->kind = JSON_VALUE_STRING;
    out_value->payload.text = (struct json_text){
        .text = text,
        .length = text_length,
    };
    return MYLITE_OK;
}

int mylite_json_internal_parse_string(
    struct json_parser *parser,
    char **out_text,
    size_t *out_text_length
) {
    struct json_writer string = {0};
    int rc = MYLITE_OK;

    *out_text = NULL;
    *out_text_length = 0U;
    if (!mylite_json_internal_parser_match(parser, '"')) {
        return mylite_json_internal_parser_invalid(parser, parser->position);
    }

    while (rc == MYLITE_OK && !mylite_json_internal_parser_at_end(parser)) {
        size_t position = parser->position;
        unsigned char byte = (unsigned char)parser->text[parser->position++];

        if (byte == '"') {
            if (string.text == NULL) {
                rc = mylite_json_internal_writer_append_text(&string, "", 0U);
            }
            if (rc == MYLITE_OK) {
                *out_text_length = string.length;
                *out_text = mylite_json_internal_writer_take(&string);
                if (*out_text == NULL) {
                    rc = MYLITE_NOMEM;
                }
            }
            mylite_json_internal_writer_deinit(&string);
            return rc;
        }
        if (byte == '\\') {
            rc = append_string_escape(parser, &string, position);
        } else {
            rc = append_string_byte(parser, &string, byte);
        }
    }

    mylite_json_internal_writer_deinit(&string);
    return rc == MYLITE_OK ? mylite_json_internal_parser_invalid(parser, parser->position) : rc;
}

static int append_string_byte(
    struct json_parser *parser,
    struct json_writer *string,
    unsigned char byte
) {
    if (byte == '\0') {
        return mylite_json_internal_parser_unsupported(parser, parser->position - 1U);
    }
    if (byte < json_control_byte_limit) {
        return mylite_json_internal_parser_invalid(parser, parser->position - 1U);
    }
    return mylite_json_internal_writer_append_char(string, (char)byte);
}

static int append_string_escape(
    struct json_parser *parser,
    struct json_writer *string,
    size_t escape_position
) {
    unsigned char escaped = 0U;
    unsigned int codepoint = 0U;

    if (mylite_json_internal_parser_at_end(parser)) {
        return mylite_json_internal_parser_invalid(parser, escape_position);
    }
    escaped = (unsigned char)parser->text[parser->position++];
    switch (escaped) {
    case '"':
    case '\\':
    case '/':
        return mylite_json_internal_writer_append_char(string, (char)escaped);
    case 'b':
        return mylite_json_internal_writer_append_char(string, '\b');
    case 'f':
        return mylite_json_internal_writer_append_char(string, '\f');
    case 'n':
        return mylite_json_internal_writer_append_char(string, '\n');
    case 'r':
        return mylite_json_internal_writer_append_char(string, '\r');
    case 't':
        return mylite_json_internal_writer_append_char(string, '\t');
    case 'u':
        for (size_t index = 0U; index < json_unicode_escape_digit_count; ++index) {
            unsigned int digit = 0U;
            int rc = parse_hex_digit(parser, parser->position + index, &digit);

            if (rc != MYLITE_OK) {
                return rc;
            }
            codepoint = (codepoint << json_hex_nibble_bits) | digit;
        }
        parser->position += json_unicode_escape_digit_count;
        return append_ascii_codepoint(parser, escape_position, string, codepoint);
    default:
        return mylite_json_internal_parser_invalid(parser, parser->position - 1U);
    }
}

static int parse_hex_digit(struct json_parser *parser, size_t position, unsigned int *out_digit) {
    unsigned char byte = 0U;

    if (position >= parser->length) {
        return mylite_json_internal_parser_invalid(parser, position);
    }
    byte = (unsigned char)parser->text[position];
    if (byte >= '0' && byte <= '9') {
        *out_digit = (unsigned int)(byte - '0');
        return MYLITE_OK;
    }
    if (byte >= 'a' && byte <= 'f') {
        *out_digit = (unsigned int)(byte - 'a') + json_decimal_base;
        return MYLITE_OK;
    }
    if (byte >= 'A' && byte <= 'F') {
        *out_digit = (unsigned int)(byte - 'A') + json_decimal_base;
        return MYLITE_OK;
    }
    return mylite_json_internal_parser_invalid(parser, position);
}

static int append_ascii_codepoint(
    struct json_parser *parser,
    size_t position,
    struct json_writer *string,
    unsigned int codepoint
) {
    if (codepoint == 0U || codepoint > json_ascii_byte_limit) {
        return mylite_json_internal_parser_unsupported(parser, position);
    }
    return mylite_json_internal_writer_append_char(string, (char)codepoint);
}

static int parse_number(struct json_parser *parser, struct json_value *out_value) {
    struct json_number_integer_part integer = {0};
    size_t start = parser->position;
    bool has_fraction = false;
    bool has_exponent = false;
    int rc = parse_number_integer_part(parser, start, &integer);

    if (rc == MYLITE_OK) {
        rc = parse_number_fraction_part(parser, &has_fraction);
    }
    if (rc == MYLITE_OK) {
        rc = parse_number_exponent_part(parser, &has_exponent);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (has_fraction || has_exponent) {
        return parse_floating_number(parser, start, out_value);
    }

    return parse_integer_number(
        parser,
        start,
        integer.is_negative,
        integer.start,
        integer.end,
        out_value
    );
}

static int parse_number_integer_part(
    struct json_parser *parser,
    size_t start,
    struct json_number_integer_part *out_part
) {
    out_part->is_negative = mylite_json_internal_parser_match(parser, '-');
    out_part->start = parser->position;
    if (mylite_json_internal_parser_at_end(parser)) {
        return mylite_json_internal_parser_invalid(parser, start);
    }
    if (mylite_json_internal_parser_peek(parser) == '0') {
        ++parser->position;
        if (!mylite_json_internal_parser_at_end(parser) &&
            mylite_json_internal_is_decimal_digit(mylite_json_internal_parser_peek(parser))) {
            return mylite_json_internal_parser_invalid(parser, parser->position);
        }
    } else if (mylite_json_internal_parser_peek(parser) >= '1' &&
               mylite_json_internal_parser_peek(parser) <= '9') {
        while (!mylite_json_internal_parser_at_end(parser) &&
               mylite_json_internal_is_decimal_digit(mylite_json_internal_parser_peek(parser))) {
            ++parser->position;
        }
    } else {
        return mylite_json_internal_parser_invalid(parser, parser->position);
    }
    out_part->end = parser->position;
    return MYLITE_OK;
}

static int parse_number_fraction_part(struct json_parser *parser, bool *out_has_fraction) {
    *out_has_fraction = false;
    if (mylite_json_internal_parser_at_end(parser) ||
        mylite_json_internal_parser_peek(parser) != '.') {
        return MYLITE_OK;
    }
    *out_has_fraction = true;
    ++parser->position;
    if (mylite_json_internal_parser_at_end(parser) ||
        !mylite_json_internal_is_decimal_digit(mylite_json_internal_parser_peek(parser))) {
        return mylite_json_internal_parser_invalid(parser, parser->position);
    }
    while (!mylite_json_internal_parser_at_end(parser) &&
           mylite_json_internal_is_decimal_digit(mylite_json_internal_parser_peek(parser))) {
        ++parser->position;
    }
    return MYLITE_OK;
}

static int parse_number_exponent_part(struct json_parser *parser, bool *out_has_exponent) {
    *out_has_exponent = false;
    if (mylite_json_internal_parser_at_end(parser) ||
        (mylite_json_internal_parser_peek(parser) != 'e' &&
         mylite_json_internal_parser_peek(parser) != 'E')) {
        return MYLITE_OK;
    }
    *out_has_exponent = true;
    ++parser->position;
    if (!mylite_json_internal_parser_at_end(parser) &&
        (mylite_json_internal_parser_peek(parser) == '+' ||
         mylite_json_internal_parser_peek(parser) == '-')) {
        ++parser->position;
    }
    if (mylite_json_internal_parser_at_end(parser) ||
        !mylite_json_internal_is_decimal_digit(mylite_json_internal_parser_peek(parser))) {
        return mylite_json_internal_parser_invalid(parser, parser->position);
    }
    while (!mylite_json_internal_parser_at_end(parser) &&
           mylite_json_internal_is_decimal_digit(mylite_json_internal_parser_peek(parser))) {
        ++parser->position;
    }
    return MYLITE_OK;
}

static int parse_integer_number(
    struct json_parser *parser,
    size_t start,
    bool is_negative,
    size_t integer_start,
    size_t integer_end,
    struct json_value *out_value
) {
    const char *digits = parser->text + integer_start;
    size_t digit_count = integer_end - integer_start;
    bool negative_zero = false;

    if (!integer_number_is_in_signed_range(digits, digit_count, is_negative)) {
        return mylite_json_internal_parser_unsupported(parser, start);
    }
    if (is_negative && digit_count == 1U && digits[0] == '0') {
        negative_zero = true;
    }

    return copy_number_text(
        parser->text + start,
        parser->position - start,
        negative_zero,
        JSON_NUMBER_INTEGER,
        out_value
    );
}

static int parse_floating_number(
    struct json_parser *parser,
    size_t start,
    struct json_value *out_value
) {
    char *text = NULL;
    char *end = NULL;
    double value = 0.0;
    size_t length = parser->position - start;
    int rc = MYLITE_OK;

    if (length == SIZE_MAX) {
        return MYLITE_NOMEM;
    }
    text = malloc(length + 1U);
    if (text == NULL) {
        return MYLITE_NOMEM;
    }
    memcpy(text, parser->text + start, length);
    text[length] = '\0';
    errno = 0;
    value = strtod(text, &end);
    if (end != text + length || errno == ERANGE || !isfinite(value)) {
        rc = mylite_json_internal_parser_unsupported(parser, start);
    } else {
        rc = copy_number_text(text, length, false, JSON_NUMBER_DOUBLE, out_value);
    }
    free(text);
    return rc;
}

static bool integer_number_is_in_signed_range(
    const char *digits,
    size_t digit_count,
    bool is_negative
) {
    const char *maximum = "9223372036854775807";

    if (is_negative) {
        maximum = "9223372036854775808";
    }

    if (digits == NULL || digit_count == 0U) {
        return false;
    }
    if (digit_count < json_signed_int64_digit_count) {
        return true;
    }
    if (digit_count > json_signed_int64_digit_count) {
        return false;
    }
    return memcmp(digits, maximum, json_signed_int64_digit_count) <= 0;
}

static int copy_number_text(
    const char *text,
    size_t length,
    bool negative_zero,
    enum json_number_kind number_kind,
    struct json_value *out_value
) {
    char *copy = NULL;

    if (negative_zero) {
        text = "0";
        length = 1U;
    }
    if (length == SIZE_MAX) {
        return MYLITE_NOMEM;
    }
    copy = malloc(length + 1U);
    if (copy == NULL) {
        return MYLITE_NOMEM;
    }
    memcpy(copy, text, length);
    copy[length] = '\0';

    out_value->kind = JSON_VALUE_NUMBER;
    out_value->number_kind = number_kind;
    out_value->payload.text = (struct json_text){
        .text = copy,
        .length = length,
    };
    return MYLITE_OK;
}

static int parse_literal(
    struct json_parser *parser,
    const char *literal,
    enum json_value_kind kind,
    bool boolean,
    struct json_value *out_value
) {
    size_t length = strlen(literal);

    if (parser->position > parser->length || length > parser->length - parser->position ||
        memcmp(parser->text + parser->position, literal, length) != 0) {
        return mylite_json_internal_parser_invalid(parser, parser->position);
    }
    parser->position += length;
    out_value->kind = kind;
    if (kind == JSON_VALUE_BOOL) {
        out_value->payload.boolean = boolean;
    }
    return MYLITE_OK;
}
