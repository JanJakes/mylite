#include <mylite/mylite.h>

#include "mylite_json_internal.h"

#include <string.h>

enum json_validate_state {
    JSON_VALIDATE_VALUE,
    JSON_VALIDATE_OBJECT_KEY_OR_END,
    JSON_VALIDATE_OBJECT_KEY_REQUIRED,
    JSON_VALIDATE_OBJECT_COLON,
    JSON_VALIDATE_OBJECT_COMMA_OR_END,
    JSON_VALIDATE_ARRAY_VALUE_OR_END,
    JSON_VALIDATE_ARRAY_VALUE_REQUIRED,
    JSON_VALIDATE_ARRAY_COMMA_OR_END,
};

struct json_validate_stack {
    enum json_validate_state states[json_max_nesting_depth + 2U];
    size_t count;
};

static bool validate_state(
    struct json_parser *parser,
    struct json_validate_stack *stack,
    enum json_validate_state state
);
static bool validate_value_state(struct json_parser *parser, struct json_validate_stack *stack);
static bool validate_object_key_or_end_state(struct json_parser *parser);
static bool validate_object_key_required_state(
    struct json_parser *parser,
    struct json_validate_stack *stack
);
static bool validate_object_colon_state(
    struct json_parser *parser,
    struct json_validate_stack *stack
);
static bool validate_object_comma_or_end_state(
    struct json_parser *parser,
    struct json_validate_stack *stack
);
static bool validate_array_value_or_end_state(
    struct json_parser *parser,
    struct json_validate_stack *stack
);
static bool validate_array_value_required_state(struct json_validate_stack *stack);
static bool validate_array_comma_or_end_state(
    struct json_parser *parser,
    struct json_validate_stack *stack
);
static bool validate_container_depth_available(const struct json_validate_stack *stack);
static bool validate_stack_push(struct json_validate_stack *stack, enum json_validate_state state);
static enum json_validate_state validate_stack_pop(struct json_validate_stack *stack);
static bool validate_string(struct json_parser *parser);
static bool validate_string_escape(struct json_parser *parser);
static bool validate_number(struct json_parser *parser);
static bool validate_integer_digits(struct json_parser *parser);
static bool validate_fraction_digits(struct json_parser *parser);
static bool validate_exponent_digits(struct json_parser *parser);
static bool validate_literal_token(struct json_parser *parser, const char *literal, size_t length);

bool mylite_json_internal_validate_document(struct json_parser *parser) {
    struct json_validate_stack stack = {0};

    if (!validate_stack_push(&stack, JSON_VALIDATE_VALUE)) {
        return false;
    }
    while (stack.count > 0U) {
        enum json_validate_state state = validate_stack_pop(&stack);

        if (!validate_state(parser, &stack, state)) {
            return false;
        }
    }
    mylite_json_internal_skip_whitespace(parser);
    return mylite_json_internal_parser_at_end(parser);
}

static bool validate_state(
    struct json_parser *parser,
    struct json_validate_stack *stack,
    enum json_validate_state state
) {
    switch (state) {
    case JSON_VALIDATE_VALUE:
        return validate_value_state(parser, stack);
    case JSON_VALIDATE_OBJECT_KEY_OR_END:
        if (validate_object_key_or_end_state(parser)) {
            return true;
        }
        return validate_object_key_required_state(parser, stack);
    case JSON_VALIDATE_OBJECT_KEY_REQUIRED:
        return validate_object_key_required_state(parser, stack);
    case JSON_VALIDATE_OBJECT_COLON:
        return validate_object_colon_state(parser, stack);
    case JSON_VALIDATE_OBJECT_COMMA_OR_END:
        return validate_object_comma_or_end_state(parser, stack);
    case JSON_VALIDATE_ARRAY_VALUE_OR_END:
        return validate_array_value_or_end_state(parser, stack);
    case JSON_VALIDATE_ARRAY_VALUE_REQUIRED:
        return validate_array_value_required_state(stack);
    case JSON_VALIDATE_ARRAY_COMMA_OR_END:
        return validate_array_comma_or_end_state(parser, stack);
    }
    return false;
}

static bool validate_value_state(struct json_parser *parser, struct json_validate_stack *stack) {
    char byte = '\0';

    mylite_json_internal_skip_whitespace(parser);
    byte = mylite_json_internal_parser_peek(parser);
    switch (byte) {
    case '{':
        if (!validate_container_depth_available(stack) ||
            !mylite_json_internal_parser_match(parser, '{')) {
            return false;
        }
        return validate_stack_push(stack, JSON_VALIDATE_OBJECT_KEY_OR_END);
    case '[':
        if (!validate_container_depth_available(stack) ||
            !mylite_json_internal_parser_match(parser, '[')) {
            return false;
        }
        return validate_stack_push(stack, JSON_VALIDATE_ARRAY_VALUE_OR_END);
    case '"':
        return validate_string(parser);
    case 't':
        return validate_literal_token(parser, "true", json_true_literal_length);
    case 'f':
        return validate_literal_token(parser, "false", json_false_literal_length);
    case 'n':
        return validate_literal_token(parser, "null", json_null_literal_length);
    case '-':
        return validate_number(parser);
    default:
        break;
    }
    if (byte >= '0' && byte <= '9') {
        return validate_number(parser);
    }
    return false;
}

static bool validate_object_key_or_end_state(struct json_parser *parser) {
    mylite_json_internal_skip_whitespace(parser);
    return mylite_json_internal_parser_match(parser, '}');
}

static bool validate_object_key_required_state(
    struct json_parser *parser,
    struct json_validate_stack *stack
) {
    mylite_json_internal_skip_whitespace(parser);
    if (!validate_string(parser)) {
        return false;
    }
    return validate_stack_push(stack, JSON_VALIDATE_OBJECT_COLON);
}

static bool validate_object_colon_state(
    struct json_parser *parser,
    struct json_validate_stack *stack
) {
    mylite_json_internal_skip_whitespace(parser);
    if (!mylite_json_internal_parser_match(parser, ':')) {
        return false;
    }
    if (!validate_stack_push(stack, JSON_VALIDATE_OBJECT_COMMA_OR_END)) {
        return false;
    }
    return validate_stack_push(stack, JSON_VALIDATE_VALUE);
}

static bool validate_object_comma_or_end_state(
    struct json_parser *parser,
    struct json_validate_stack *stack
) {
    mylite_json_internal_skip_whitespace(parser);
    if (mylite_json_internal_parser_match(parser, '}')) {
        return true;
    }
    if (!mylite_json_internal_parser_match(parser, ',')) {
        return false;
    }
    return validate_stack_push(stack, JSON_VALIDATE_OBJECT_KEY_REQUIRED);
}

static bool validate_array_value_or_end_state(
    struct json_parser *parser,
    struct json_validate_stack *stack
) {
    mylite_json_internal_skip_whitespace(parser);
    if (mylite_json_internal_parser_match(parser, ']')) {
        return true;
    }
    return validate_array_value_required_state(stack);
}

static bool validate_array_value_required_state(struct json_validate_stack *stack) {
    if (!validate_stack_push(stack, JSON_VALIDATE_ARRAY_COMMA_OR_END)) {
        return false;
    }
    return validate_stack_push(stack, JSON_VALIDATE_VALUE);
}

static bool validate_array_comma_or_end_state(
    struct json_parser *parser,
    struct json_validate_stack *stack
) {
    mylite_json_internal_skip_whitespace(parser);
    if (mylite_json_internal_parser_match(parser, ']')) {
        return true;
    }
    if (!mylite_json_internal_parser_match(parser, ',')) {
        return false;
    }
    return validate_stack_push(stack, JSON_VALIDATE_ARRAY_VALUE_REQUIRED);
}

static bool validate_container_depth_available(const struct json_validate_stack *stack) {
    return stack->count < json_max_nesting_depth;
}

static bool validate_stack_push(struct json_validate_stack *stack, enum json_validate_state state) {
    if (stack->count >= sizeof(stack->states) / sizeof(stack->states[0])) {
        return false;
    }
    stack->states[stack->count] = state;
    ++stack->count;
    return true;
}

static enum json_validate_state validate_stack_pop(struct json_validate_stack *stack) {
    --stack->count;
    return stack->states[stack->count];
}

static bool validate_string(struct json_parser *parser) {
    if (!mylite_json_internal_parser_match(parser, '"')) {
        return false;
    }
    while (!mylite_json_internal_parser_at_end(parser)) {
        unsigned char byte = (unsigned char)mylite_json_internal_parser_peek(parser);

        ++parser->position;
        if (byte == '"') {
            return true;
        }
        if (byte == '\\') {
            if (!validate_string_escape(parser)) {
                return false;
            }
            continue;
        }
        if (byte < json_control_byte_limit) {
            return false;
        }
    }
    return false;
}

static bool validate_string_escape(struct json_parser *parser) {
    char byte = '\0';

    if (mylite_json_internal_parser_at_end(parser)) {
        return false;
    }
    byte = mylite_json_internal_parser_peek(parser);
    ++parser->position;
    switch (byte) {
    case '"':
    case '\\':
    case '/':
    case 'b':
    case 'f':
    case 'n':
    case 'r':
    case 't':
        return true;
    case 'u':
        for (size_t digit_index = 0U; digit_index < json_unicode_escape_digit_count;
             ++digit_index) {
            if (mylite_json_internal_parser_at_end(parser) ||
                !mylite_json_internal_is_hex_digit(mylite_json_internal_parser_peek(parser))) {
                return false;
            }
            ++parser->position;
        }
        return true;
    default:
        break;
    }
    return false;
}

static bool validate_number(struct json_parser *parser) {
    if (mylite_json_internal_parser_match(parser, '-')) {
        if (mylite_json_internal_parser_at_end(parser)) {
            return false;
        }
    }
    if (!validate_integer_digits(parser)) {
        return false;
    }
    if (mylite_json_internal_parser_peek(parser) == '.' && !validate_fraction_digits(parser)) {
        return false;
    }
    if ((mylite_json_internal_parser_peek(parser) == 'e' ||
         mylite_json_internal_parser_peek(parser) == 'E') &&
        !validate_exponent_digits(parser)) {
        return false;
    }
    return true;
}

static bool validate_integer_digits(struct json_parser *parser) {
    char byte = mylite_json_internal_parser_peek(parser);

    if (byte == '0') {
        ++parser->position;
        return true;
    }
    if (byte < '1' || byte > '9') {
        return false;
    }
    do {
        ++parser->position;
        byte = mylite_json_internal_parser_peek(parser);
    } while (mylite_json_internal_is_decimal_digit(byte));
    return true;
}

static bool validate_fraction_digits(struct json_parser *parser) {
    if (!mylite_json_internal_parser_match(parser, '.')) {
        return false;
    }
    if (!mylite_json_internal_is_decimal_digit(mylite_json_internal_parser_peek(parser))) {
        return false;
    }
    do {
        ++parser->position;
    } while (mylite_json_internal_is_decimal_digit(mylite_json_internal_parser_peek(parser)));
    return true;
}

static bool validate_exponent_digits(struct json_parser *parser) {
    if (mylite_json_internal_parser_peek(parser) != 'e' &&
        mylite_json_internal_parser_peek(parser) != 'E') {
        return false;
    }
    ++parser->position;
    if (mylite_json_internal_parser_peek(parser) == '+' ||
        mylite_json_internal_parser_peek(parser) == '-') {
        ++parser->position;
    }
    if (!mylite_json_internal_is_decimal_digit(mylite_json_internal_parser_peek(parser))) {
        return false;
    }
    do {
        ++parser->position;
    } while (mylite_json_internal_is_decimal_digit(mylite_json_internal_parser_peek(parser)));
    return true;
}

static bool validate_literal_token(struct json_parser *parser, const char *literal, size_t length) {
    if (literal == NULL || parser->position > parser->length ||
        length > parser->length - parser->position) {
        return false;
    }
    if (memcmp(&parser->text[parser->position], literal, length) != 0) {
        return false;
    }
    parser->position += length;
    return true;
}
