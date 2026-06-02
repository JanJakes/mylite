#include <mylite/mylite.h>

#include "mylite_json_internal.h"

#include <stdlib.h>

static int parse_path_member_leg(
    struct json_parser *parser,
    const struct json_value **inout_value,
    bool *inout_matched
);
static int parse_path_quoted_member_leg(
    struct json_parser *parser,
    const struct json_value **inout_value,
    bool *inout_matched
);
static int parse_path_identifier_member_leg(
    struct json_parser *parser,
    const struct json_value **inout_value,
    bool *inout_matched
);
static int parse_path_array_leg(
    struct json_parser *parser,
    const struct json_value **inout_value,
    bool *inout_matched
);
static int parse_path_array_index(struct json_parser *parser, size_t *out_index);
static int parse_json_set_path_member_leg(struct json_parser *parser, struct json_set_path *path);
static int parse_json_set_path_quoted_member_leg(
    struct json_parser *parser,
    struct json_set_path *path
);
static int parse_json_set_path_identifier_member_leg(
    struct json_parser *parser,
    struct json_set_path *path
);
static int parse_json_set_path_array_leg(struct json_parser *parser, struct json_set_path *path);
static int parse_json_set_path_array_index(struct json_parser *parser, size_t *out_index);
static int json_set_path_append_member(
    struct json_set_path *path,
    const char *member,
    size_t member_length
);
static int json_set_path_append_array_index(struct json_set_path *path, size_t index);
static int json_set_path_reserve(struct json_set_path *path, size_t required_capacity);
static bool path_identifier_start_byte(char byte);
static bool path_identifier_byte(char byte);
static bool path_text_is_ascii(const char *text, size_t text_length);

int mylite_json_internal_extract_path_value(
    struct json_parser *parser,
    const struct json_value *root,
    const struct json_value **out_value,
    bool *out_matched
) {
    int rc = MYLITE_OK;

    if (out_value == NULL || out_matched == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = root;
    *out_matched = root != NULL;
    if (!mylite_json_internal_parser_match(parser, '$')) {
        return mylite_json_internal_parser_invalid(parser, parser->position);
    }

    while (rc == MYLITE_OK && !mylite_json_internal_parser_at_end(parser)) {
        char byte = mylite_json_internal_parser_peek(parser);

        if (byte == '.') {
            rc = parse_path_member_leg(parser, out_value, out_matched);
        } else if (byte == '[') {
            rc = parse_path_array_leg(parser, out_value, out_matched);
        } else if (byte == '*') {
            rc = mylite_json_internal_parser_unsupported(parser, parser->position);
        } else {
            rc = mylite_json_internal_parser_invalid(parser, parser->position);
        }
    }

    return rc;
}

static int parse_path_member_leg(
    struct json_parser *parser,
    const struct json_value **inout_value,
    bool *inout_matched
) {
    if (!mylite_json_internal_parser_match(parser, '.')) {
        return mylite_json_internal_parser_invalid(parser, parser->position);
    }
    if (mylite_json_internal_parser_peek(parser) == '"') {
        return parse_path_quoted_member_leg(parser, inout_value, inout_matched);
    }
    if (!path_identifier_start_byte(mylite_json_internal_parser_peek(parser))) {
        if (mylite_json_internal_parser_at_end(parser)) {
            return mylite_json_internal_parser_invalid(parser, parser->position);
        }
        if (mylite_json_internal_parser_peek(parser) == '*') {
            return mylite_json_internal_parser_unsupported(parser, parser->position);
        }
        return mylite_json_internal_parser_invalid(parser, parser->position);
    }
    return parse_path_identifier_member_leg(parser, inout_value, inout_matched);
}

static int parse_path_quoted_member_leg(
    struct json_parser *parser,
    const struct json_value **inout_value,
    bool *inout_matched
) {
    char *member = NULL;
    size_t member_length = 0U;
    size_t start = parser->position;
    int rc = mylite_json_internal_parse_string(parser, &member, &member_length);

    if (rc == MYLITE_OK && !path_text_is_ascii(member, member_length)) {
        rc = mylite_json_internal_parser_unsupported(parser, start);
    }
    if (rc == MYLITE_OK && *inout_matched) {
        const struct json_value *next =
            mylite_json_internal_object_member_value(*inout_value, member, member_length);

        if (next == NULL) {
            *inout_matched = false;
            *inout_value = NULL;
        } else {
            *inout_value = next;
        }
    }

    free(member);
    return rc;
}

static int parse_path_identifier_member_leg(
    struct json_parser *parser,
    const struct json_value **inout_value,
    bool *inout_matched
) {
    size_t start = parser->position;

    ++parser->position;
    while (path_identifier_byte(mylite_json_internal_parser_peek(parser))) {
        ++parser->position;
    }
    if (*inout_matched) {
        const char *member = &parser->text[start];
        size_t member_length = parser->position - start;
        const struct json_value *next =
            mylite_json_internal_object_member_value(*inout_value, member, member_length);

        if (next == NULL) {
            *inout_matched = false;
            *inout_value = NULL;
        } else {
            *inout_value = next;
        }
    }
    return MYLITE_OK;
}

static int parse_path_array_leg(
    struct json_parser *parser,
    const struct json_value **inout_value,
    bool *inout_matched
) {
    size_t index = 0U;
    int rc = MYLITE_OK;

    if (!mylite_json_internal_parser_match(parser, '[')) {
        return mylite_json_internal_parser_invalid(parser, parser->position);
    }
    rc = parse_path_array_index(parser, &index);
    if (rc == MYLITE_OK && !mylite_json_internal_parser_match(parser, ']')) {
        rc = mylite_json_internal_parser_invalid(parser, parser->position);
    }
    if (rc == MYLITE_OK && *inout_matched) {
        const struct json_value *next = mylite_json_internal_array_index_value(*inout_value, index);

        if (next == NULL) {
            *inout_matched = false;
            *inout_value = NULL;
        } else {
            *inout_value = next;
        }
    }
    return rc;
}

static int parse_path_array_index(struct json_parser *parser, size_t *out_index) {
    size_t start = parser->position;
    size_t value = 0U;

    if (mylite_json_internal_parser_peek(parser) == '*' ||
        mylite_json_internal_parser_peek(parser) == 'l' ||
        mylite_json_internal_parser_peek(parser) == 'L') {
        return mylite_json_internal_parser_unsupported(parser, parser->position);
    }
    if (!mylite_json_internal_is_decimal_digit(mylite_json_internal_parser_peek(parser))) {
        return mylite_json_internal_parser_invalid(parser, parser->position);
    }
    if (mylite_json_internal_parser_peek(parser) == '0') {
        ++parser->position;
        if (mylite_json_internal_is_decimal_digit(mylite_json_internal_parser_peek(parser))) {
            return mylite_json_internal_parser_invalid(parser, parser->position);
        }
        *out_index = 0U;
        return MYLITE_OK;
    }

    while (mylite_json_internal_is_decimal_digit(mylite_json_internal_parser_peek(parser))) {
        unsigned int digit = (unsigned int)(mylite_json_internal_parser_peek(parser) - '0');

        if (value > (SIZE_MAX - digit) / json_decimal_base) {
            return mylite_json_internal_parser_unsupported(parser, start);
        }
        value = (value * json_decimal_base) + digit;
        ++parser->position;
    }
    *out_index = value;
    return MYLITE_OK;
}

int mylite_json_internal_parse_set_path(
    struct json_parser *parser,
    struct json_set_path *out_path
) {
    int rc = MYLITE_OK;

    if (out_path == NULL) {
        return MYLITE_MISUSE;
    }
    *out_path = (struct json_set_path){0};
    if (!mylite_json_internal_parser_match(parser, '$')) {
        return mylite_json_internal_parser_invalid(parser, parser->position);
    }

    while (rc == MYLITE_OK && !mylite_json_internal_parser_at_end(parser)) {
        char byte = mylite_json_internal_parser_peek(parser);

        if (byte == '.') {
            rc = parse_json_set_path_member_leg(parser, out_path);
        } else if (byte == '[') {
            rc = parse_json_set_path_array_leg(parser, out_path);
        } else if (byte == '*') {
            rc = mylite_json_internal_parser_unsupported(parser, parser->position);
        } else {
            rc = mylite_json_internal_parser_invalid(parser, parser->position);
        }
    }
    if (rc != MYLITE_OK) {
        mylite_json_internal_set_path_deinit(out_path);
    }
    return rc;
}

static int parse_json_set_path_member_leg(struct json_parser *parser, struct json_set_path *path) {
    if (!mylite_json_internal_parser_match(parser, '.')) {
        return mylite_json_internal_parser_invalid(parser, parser->position);
    }
    if (mylite_json_internal_parser_peek(parser) == '"') {
        return parse_json_set_path_quoted_member_leg(parser, path);
    }
    if (!path_identifier_start_byte(mylite_json_internal_parser_peek(parser))) {
        if (mylite_json_internal_parser_at_end(parser)) {
            return mylite_json_internal_parser_invalid(parser, parser->position);
        }
        if (mylite_json_internal_parser_peek(parser) == '*') {
            return mylite_json_internal_parser_unsupported(parser, parser->position);
        }
        return mylite_json_internal_parser_invalid(parser, parser->position);
    }
    return parse_json_set_path_identifier_member_leg(parser, path);
}

static int parse_json_set_path_quoted_member_leg(
    struct json_parser *parser,
    struct json_set_path *path
) {
    char *member = NULL;
    size_t member_length = 0U;
    size_t start = parser->position;
    int rc = mylite_json_internal_parse_string(parser, &member, &member_length);

    if (rc == MYLITE_OK && !path_text_is_ascii(member, member_length)) {
        rc = mylite_json_internal_parser_unsupported(parser, start);
    }
    if (rc == MYLITE_OK) {
        rc = json_set_path_append_member(path, member, member_length);
        if (rc == MYLITE_OK) {
            member = NULL;
        }
    }

    free(member);
    return rc;
}

static int parse_json_set_path_identifier_member_leg(
    struct json_parser *parser,
    struct json_set_path *path
) {
    char *member = NULL;
    size_t start = parser->position;
    size_t member_length = 0U;
    int rc = MYLITE_OK;

    ++parser->position;
    while (path_identifier_byte(mylite_json_internal_parser_peek(parser))) {
        ++parser->position;
    }
    member_length = parser->position - start;
    rc = mylite_json_internal_copy_result_text(
        &parser->text[start],
        member_length,
        &member,
        &member_length
    );
    if (rc == MYLITE_OK) {
        rc = json_set_path_append_member(path, member, member_length);
        if (rc == MYLITE_OK) {
            member = NULL;
        }
    }

    free(member);
    return rc;
}

static int parse_json_set_path_array_leg(struct json_parser *parser, struct json_set_path *path) {
    size_t index = 0U;
    int rc = MYLITE_OK;

    if (!mylite_json_internal_parser_match(parser, '[')) {
        return mylite_json_internal_parser_invalid(parser, parser->position);
    }
    rc = parse_json_set_path_array_index(parser, &index);
    if (rc == MYLITE_OK && !mylite_json_internal_parser_match(parser, ']')) {
        rc = mylite_json_internal_parser_invalid(parser, parser->position);
    }
    if (rc == MYLITE_OK) {
        rc = json_set_path_append_array_index(path, index);
    }
    return rc;
}

static int parse_json_set_path_array_index(struct json_parser *parser, size_t *out_index) {
    size_t start = parser->position;
    size_t value = 0U;
    bool has_digit = false;

    if (mylite_json_internal_parser_peek(parser) == '*' ||
        mylite_json_internal_parser_peek(parser) == 'l' ||
        mylite_json_internal_parser_peek(parser) == 'L') {
        return mylite_json_internal_parser_unsupported(parser, parser->position);
    }
    while (mylite_json_internal_is_decimal_digit(mylite_json_internal_parser_peek(parser))) {
        unsigned int digit = (unsigned int)(mylite_json_internal_parser_peek(parser) - '0');

        has_digit = true;
        if (value > (SIZE_MAX - digit) / json_decimal_base) {
            return mylite_json_internal_parser_unsupported(parser, start);
        }
        value = (value * json_decimal_base) + digit;
        ++parser->position;
    }
    if (!has_digit) {
        return mylite_json_internal_parser_invalid(parser, parser->position);
    }
    *out_index = value;
    return MYLITE_OK;
}

static int json_set_path_append_member(
    struct json_set_path *path,
    const char *member,
    size_t member_length
) {
    int rc = json_set_path_reserve(path, path->count + 1U);

    if (rc != MYLITE_OK) {
        return rc;
    }
    path->legs[path->count] = (struct json_set_path_leg){
        .kind = JSON_SET_PATH_MEMBER,
        .member = member,
        .member_length = member_length,
        .index = 0U,
    };
    ++path->count;
    return MYLITE_OK;
}

static int json_set_path_append_array_index(struct json_set_path *path, size_t index) {
    int rc = json_set_path_reserve(path, path->count + 1U);

    if (rc != MYLITE_OK) {
        return rc;
    }
    path->legs[path->count] = (struct json_set_path_leg){
        .kind = JSON_SET_PATH_ARRAY,
        .member = NULL,
        .member_length = 0U,
        .index = index,
    };
    ++path->count;
    return MYLITE_OK;
}

static int json_set_path_reserve(struct json_set_path *path, size_t required_capacity) {
    struct json_set_path_leg *legs = NULL;
    size_t capacity = path->capacity;

    if (required_capacity <= capacity) {
        return MYLITE_OK;
    }
    if (capacity == 0U) {
        capacity = json_initial_container_capacity;
    }
    while (capacity < required_capacity) {
        if (capacity > SIZE_MAX / 2U) {
            return MYLITE_NOMEM;
        }
        capacity *= 2U;
    }
    if (capacity > SIZE_MAX / sizeof(*legs)) {
        return MYLITE_NOMEM;
    }

    legs = realloc(path->legs, capacity * sizeof(*legs));
    if (legs == NULL) {
        return MYLITE_NOMEM;
    }
    path->legs = legs;
    path->capacity = capacity;
    return MYLITE_OK;
}

void mylite_json_internal_set_path_deinit(struct json_set_path *path) {
    if (path == NULL) {
        return;
    }
    for (size_t index = 0U; index < path->count; ++index) {
        free((void *)path->legs[index].member);
    }
    free(path->legs);
    *path = (struct json_set_path){0};
}

static bool path_identifier_start_byte(char byte) {
    return ((byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z') || byte == '_' ||
            byte == '$') != 0;
}

static bool path_identifier_byte(char byte) {
    return (path_identifier_start_byte(byte) || (byte >= '0' && byte <= '9')) != 0;
}

static bool path_text_is_ascii(const char *text, size_t text_length) {
    for (size_t index = 0U; index < text_length; ++index) {
        unsigned char byte = (unsigned char)text[index];

        if (byte == '\0' || byte > json_ascii_byte_limit) {
            return false;
        }
    }
    return true;
}
