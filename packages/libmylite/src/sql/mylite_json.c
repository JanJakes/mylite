#include "mylite_json.h"

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
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
    json_literal_false_length = 5U,
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
    json_path_decimal_base = 10U,
    json_number_buffer_length = 64U,
};

struct json_parser {
    const char *text;
    size_t length;
    size_t offset;
    struct mylite_json_error *error;
};

struct json_value;

struct json_dom_member {
    char *key;
    size_t key_length;
    struct json_value *value;
};

struct json_value {
    enum mylite_json_type type;
    char *text;
    size_t text_length;
    struct json_value *items;
    size_t item_count;
    struct json_dom_member *members;
    size_t member_count;
};

enum json_path_leg_kind {
    JSON_PATH_LEG_MEMBER = 0,
    JSON_PATH_LEG_MEMBER_WILDCARD = 1,
    JSON_PATH_LEG_ARRAY_INDEX = 2,
    JSON_PATH_LEG_ARRAY_WILDCARD = 3,
    JSON_PATH_LEG_ARRAY_RANGE = 4,
    JSON_PATH_LEG_RECURSIVE = 5,
};

struct json_path_array_position {
    bool from_last;
    size_t offset;
};

struct json_path_leg {
    enum json_path_leg_kind kind;
    char *member;
    size_t member_length;
    struct json_path_array_position index;
    struct json_path_array_position range_start;
    struct json_path_array_position range_end;
};

struct json_path {
    struct json_path_leg *legs;
    size_t leg_count;
    bool can_match_multiple;
};

struct json_value_ref_list {
    const struct json_value **items;
    size_t count;
};

static bool parse_value(struct json_parser *parser, enum mylite_json_type *out_type);

static int parse_document(
    const char *text,
    size_t length,
    struct json_value *out_value,
    struct mylite_json_error *out_error
);

static int parse_dom_value(struct json_parser *parser, struct json_value *out_value);

static int parse_dom_object(struct json_parser *parser, struct json_value *out_value);

static int parse_dom_array(struct json_parser *parser, struct json_value *out_value);

static int parse_dom_number(struct json_parser *parser, struct json_value *out_value);

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

static int json_value_to_text(const struct json_value *value, char **out_text, size_t *out_length);

static int append_json_value(char **text, size_t *length, const struct json_value *value);

static int append_json_array_value(char **text, size_t *length, const struct json_value *value);

static int append_json_object_value(char **text, size_t *length, const struct json_value *value);

static int parse_json_path(
    const char *path_text,
    size_t path_length,
    bool allow_multiple_matches,
    struct json_path *out_path,
    struct mylite_json_error *out_error
);

static int parse_json_path_member(
    struct json_parser *parser,
    bool allow_multiple_matches,
    struct json_path *path,
    struct mylite_json_error *out_error
);

static int parse_json_path_array(
    struct json_parser *parser,
    bool allow_multiple_matches,
    struct json_path *path,
    struct mylite_json_error *out_error
);

static bool parse_json_path_array_position(
    struct json_parser *parser,
    struct json_path_array_position *out_position
);

static int append_json_path_leg(struct json_path *path, struct json_path_leg leg);

static int eval_json_path(
    const struct json_value *root,
    const struct json_path *path,
    struct json_value_ref_list *out_matches
);

static int eval_json_path_from(
    const struct json_value *value,
    const struct json_path *path,
    size_t leg_index,
    struct json_value_ref_list *matches
);

static int eval_json_path_recursive(
    const struct json_value *value,
    const struct json_path *path,
    size_t next_leg_index,
    struct json_value_ref_list *matches
);

static int eval_json_path_leg(
    const struct json_value *value,
    const struct json_path_leg *leg,
    struct json_value_ref_list *matches
);

static int eval_json_path_member(
    const struct json_value *value,
    const struct json_path_leg *leg,
    struct json_value_ref_list *matches
);

static int eval_json_path_array_index(
    const struct json_value *value,
    const struct json_path_leg *leg,
    struct json_value_ref_list *matches
);

static int eval_json_path_array_range(
    const struct json_value *value,
    const struct json_path_leg *leg,
    struct json_value_ref_list *matches
);

static bool json_path_array_position_value(
    struct json_path_array_position position,
    size_t item_count,
    size_t *out_index
);

static int json_value_ref_list_append(
    struct json_value_ref_list *list,
    const struct json_value *value
);

static uint64_t json_value_length(const struct json_value *value);

static int json_object_append_member(
    struct json_value *object,
    char *key,
    size_t key_length,
    struct json_value *value
);

static int json_object_member_compare(const void *left, const void *right);
// NOLINTBEGIN(bugprone-easily-swappable-parameters)
static int normalize_json_number_text(
    const char *text,
    size_t length,
    enum mylite_json_type type,
    char **out_text,
    size_t *out_length
);
// NOLINTEND(bugprone-easily-swappable-parameters)
static int format_json_double_text(double value, char **out_text, size_t *out_length);

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

static void set_path_error(struct mylite_json_error *error, const char *message, size_t position);

static void json_value_deinit(struct json_value *value);

static void json_path_deinit(struct json_path *path);

static void json_value_ref_list_deinit(struct json_value_ref_list *list);

static bool is_hex_digit(char byte);

static uint32_t hex_digit_value(char byte);

bool mylite_json_validate(
    const char *text,
    size_t length,
    enum mylite_json_type *out_type,
    struct mylite_json_error *out_error
) {
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
        set_error(
            &parser,
            "The document root must not be followed by other values.",
            parser.offset
        );
        return false;
    }
    if (out_type != NULL) {
        *out_type = type;
    }
    return true;
}

const char *mylite_json_type_name(enum mylite_json_type type) {
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

int mylite_json_quote_string(const char *text, size_t length, char **out_text, size_t *out_length) {
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

int mylite_json_unquote_string(
    const char *text,
    size_t length,
    char **out_text,
    size_t *out_length,
    struct mylite_json_error *out_error
) {
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

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
int mylite_json_extract(
    const char *document,
    size_t document_length,
    const char *const *paths,
    const size_t *path_lengths,
    size_t path_count,
    char **out_json,
    size_t *out_json_length,
    bool *out_found,
    struct mylite_json_error *out_error
) {
    struct json_value root = {0};
    struct json_value_ref_list matches = {0};
    bool autowrap = path_count > 1U;
    int status = MYLITE_JSON_STATUS_OK;

    if (out_json != NULL) {
        *out_json = NULL;
    }
    if (out_json_length != NULL) {
        *out_json_length = 0U;
    }
    if (out_found != NULL) {
        *out_found = false;
    }
    if (out_json == NULL || out_json_length == NULL) {
        return MYLITE_JSON_STATUS_NOMEM;
    }
    status = parse_document(document, document_length, &root, out_error);
    if (status != MYLITE_JSON_STATUS_OK) {
        return status;
    }
    for (size_t index = 0U; index < path_count; ++index) {
        struct json_path path = {0};

        status = parse_json_path(paths[index], path_lengths[index], true, &path, out_error);
        if (status == MYLITE_JSON_STATUS_OK) {
            if (path.can_match_multiple) {
                autowrap = true;
            }
            status = eval_json_path(&root, &path, &matches);
        }
        json_path_deinit(&path);
        if (status != MYLITE_JSON_STATUS_OK) {
            goto cleanup;
        }
    }
    if (matches.count == 0U) {
        status = MYLITE_JSON_STATUS_OK;
        goto cleanup;
    }
    if (autowrap) {
        struct json_value wrapper = {
            .type = MYLITE_JSON_TYPE_ARRAY,
            .items = NULL,
            .item_count = matches.count,
        };

        wrapper.items = calloc(matches.count, sizeof(*wrapper.items));
        if (wrapper.items == NULL) {
            status = MYLITE_JSON_STATUS_NOMEM;
            goto cleanup;
        }
        for (size_t index = 0U; index < matches.count; ++index) {
            wrapper.items[index] = *matches.items[index];
        }
        status = json_value_to_text(&wrapper, out_json, out_json_length);
        free(wrapper.items);
    } else {
        status = json_value_to_text(matches.items[0], out_json, out_json_length);
    }
    if (status == MYLITE_JSON_STATUS_OK && out_found != NULL) {
        *out_found = true;
    }

cleanup:
    json_value_ref_list_deinit(&matches);
    json_value_deinit(&root);
    return status;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
int mylite_json_contains_path(
    const char *document,
    size_t document_length,
    const char *const *paths,
    const size_t *path_lengths,
    size_t path_count,
    bool require_all,
    bool *out_contains,
    struct mylite_json_error *out_error
) {
    struct json_value root = {0};
    int status = parse_document(document, document_length, &root, out_error);

    if (out_contains != NULL) {
        *out_contains = false;
    }
    if (status != MYLITE_JSON_STATUS_OK) {
        return status;
    }
    for (size_t index = 0U; index < path_count; ++index) {
        struct json_path path = {0};
        struct json_value_ref_list matches = {0};
        bool exists = false;

        status = parse_json_path(paths[index], path_lengths[index], true, &path, out_error);
        if (status == MYLITE_JSON_STATUS_OK) {
            status = eval_json_path(&root, &path, &matches);
        }
        exists = matches.count != 0U;
        json_value_ref_list_deinit(&matches);
        json_path_deinit(&path);
        if (status != MYLITE_JSON_STATUS_OK) {
            goto cleanup;
        }
        if (!require_all && exists) {
            if (out_contains != NULL) {
                *out_contains = true;
            }
            goto cleanup;
        }
        if (require_all && !exists) {
            if (out_contains != NULL) {
                *out_contains = false;
            }
            goto cleanup;
        }
    }
    if (out_contains != NULL) {
        bool contains = require_all;

        if (path_count == 0U) {
            contains = true;
        }
        *out_contains = contains;
    }

cleanup:
    json_value_deinit(&root);
    return status;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
int mylite_json_keys(
    const char *document,
    size_t document_length,
    const char *path,
    size_t path_length,
    bool has_path,
    char **out_json,
    size_t *out_json_length,
    bool *out_found,
    struct mylite_json_error *out_error
) {
    struct json_value root = {0};
    struct json_value_ref_list matches = {0};
    const struct json_value *object = NULL;
    char *json = NULL;
    size_t json_length = 0U;
    int status = MYLITE_JSON_STATUS_OK;

    if (out_json != NULL) {
        *out_json = NULL;
    }
    if (out_json_length != NULL) {
        *out_json_length = 0U;
    }
    if (out_found != NULL) {
        *out_found = false;
    }
    if (out_json == NULL || out_json_length == NULL) {
        return MYLITE_JSON_STATUS_NOMEM;
    }
    status = parse_document(document, document_length, &root, out_error);
    if (status != MYLITE_JSON_STATUS_OK) {
        return status;
    }
    object = &root;
    if (has_path) {
        struct json_path parsed_path = {0};

        status = parse_json_path(path, path_length, false, &parsed_path, out_error);
        if (status == MYLITE_JSON_STATUS_OK) {
            status = eval_json_path(&root, &parsed_path, &matches);
        }
        json_path_deinit(&parsed_path);
        if (status != MYLITE_JSON_STATUS_OK) {
            goto cleanup;
        }
        object = matches.count == 1U ? matches.items[0] : NULL;
    }
    if (object == NULL || object->type != MYLITE_JSON_TYPE_OBJECT) {
        goto cleanup;
    }
    if (!append_byte(&json, &json_length, '[')) {
        status = MYLITE_JSON_STATUS_NOMEM;
    }
    for (size_t index = 0U; status == MYLITE_JSON_STATUS_OK && index < object->member_count;
         ++index) {
        char *quoted = NULL;
        size_t quoted_length = 0U;

        if (index != 0U && !append_bytes(&json, &json_length, ", ", 2U)) {
            status = MYLITE_JSON_STATUS_NOMEM;
            break;
        }
        status = mylite_json_quote_string(
                     object->members[index].key,
                     object->members[index].key_length,
                     &quoted,
                     &quoted_length
                 ) == 0
                     ? MYLITE_JSON_STATUS_OK
                     : MYLITE_JSON_STATUS_NOMEM;
        if (status == MYLITE_JSON_STATUS_OK &&
            !append_bytes(&json, &json_length, quoted, quoted_length)) {
            status = MYLITE_JSON_STATUS_NOMEM;
        }
        free(quoted);
    }
    if (status == MYLITE_JSON_STATUS_OK && !append_byte(&json, &json_length, ']')) {
        status = MYLITE_JSON_STATUS_NOMEM;
    }
    if (status == MYLITE_JSON_STATUS_OK) {
        *out_json = json;
        *out_json_length = json_length;
        json = NULL;
        if (out_found != NULL) {
            *out_found = true;
        }
    }

cleanup:
    free(json);
    json_value_ref_list_deinit(&matches);
    json_value_deinit(&root);
    return status;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
int mylite_json_length(
    const char *document,
    size_t document_length,
    const char *path,
    size_t path_length,
    bool has_path,
    uint64_t *out_length,
    bool *out_found,
    struct mylite_json_error *out_error
) {
    struct json_value root = {0};
    struct json_value_ref_list matches = {0};
    const struct json_value *value = NULL;
    int status = MYLITE_JSON_STATUS_OK;

    if (out_length != NULL) {
        *out_length = 0U;
    }
    if (out_found != NULL) {
        *out_found = false;
    }
    if (out_length == NULL) {
        return MYLITE_JSON_STATUS_NOMEM;
    }
    status = parse_document(document, document_length, &root, out_error);
    if (status != MYLITE_JSON_STATUS_OK) {
        return status;
    }
    value = &root;
    if (has_path) {
        struct json_path parsed_path = {0};

        status = parse_json_path(path, path_length, true, &parsed_path, out_error);
        if (status == MYLITE_JSON_STATUS_OK) {
            status = eval_json_path(&root, &parsed_path, &matches);
        }
        if (status == MYLITE_JSON_STATUS_OK && matches.count == 0U) {
            json_path_deinit(&parsed_path);
            goto cleanup;
        }
        if (status == MYLITE_JSON_STATUS_OK && parsed_path.can_match_multiple) {
            if (out_length != NULL) {
                *out_length = matches.count;
            }
            if (out_found != NULL) {
                *out_found = true;
            }
            json_path_deinit(&parsed_path);
            goto cleanup;
        }
        value = status == MYLITE_JSON_STATUS_OK ? matches.items[0] : NULL;
        json_path_deinit(&parsed_path);
        if (status != MYLITE_JSON_STATUS_OK) {
            goto cleanup;
        }
    }
    *out_length = json_value_length(value);
    if (out_found != NULL) {
        *out_found = true;
    }

cleanup:
    json_value_ref_list_deinit(&matches);
    json_value_deinit(&root);
    return status;
}

static int parse_document(
    const char *text,
    size_t length,
    struct json_value *out_value,
    struct mylite_json_error *out_error
) {
    struct json_parser parser = {.text = text, .length = length, .error = out_error};
    int status = MYLITE_JSON_STATUS_OK;

    if (out_error != NULL) {
        *out_error = (struct mylite_json_error){0};
    }
    skip_whitespace(&parser);
    status = parse_dom_value(&parser, out_value);
    if (status != MYLITE_JSON_STATUS_OK) {
        json_value_deinit(out_value);
        return status;
    }
    skip_whitespace(&parser);
    if (!at_end(&parser)) {
        set_error(
            &parser,
            "The document root must not be followed by other values.",
            parser.offset
        );
        json_value_deinit(out_value);
        return MYLITE_JSON_STATUS_INVALID_DOCUMENT;
    }
    return MYLITE_JSON_STATUS_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int parse_dom_value(struct json_parser *parser, struct json_value *out_value) {
    char byte = '\0';

    skip_whitespace(parser);
    if (!peek_byte(parser, &byte)) {
        set_error(parser, "Invalid value.", parser->offset);
        return MYLITE_JSON_STATUS_INVALID_DOCUMENT;
    }
    if (byte == '{') {
        return parse_dom_object(parser, out_value);
    }
    if (byte == '[') {
        return parse_dom_array(parser, out_value);
    }
    if (byte == '"') {
        out_value->type = MYLITE_JSON_TYPE_STRING;
        if (!parse_string(parser, &out_value->text, &out_value->text_length)) {
            return MYLITE_JSON_STATUS_INVALID_DOCUMENT;
        }
        return MYLITE_JSON_STATUS_OK;
    }
    if (byte == '-' || isdigit((unsigned char)byte)) {
        return parse_dom_number(parser, out_value);
    }
    if (parse_literal(parser, "true")) {
        out_value->type = MYLITE_JSON_TYPE_BOOLEAN;
        out_value->text = copy_text("true", 4U);
        out_value->text_length = 4U;
        return out_value->text == NULL ? MYLITE_JSON_STATUS_NOMEM : MYLITE_JSON_STATUS_OK;
    }
    if (parse_literal(parser, "false")) {
        out_value->type = MYLITE_JSON_TYPE_BOOLEAN;
        out_value->text = copy_text("false", json_literal_false_length);
        out_value->text_length = json_literal_false_length;
        return out_value->text == NULL ? MYLITE_JSON_STATUS_NOMEM : MYLITE_JSON_STATUS_OK;
    }
    if (parse_literal(parser, "null")) {
        out_value->type = MYLITE_JSON_TYPE_NULL;
        return MYLITE_JSON_STATUS_OK;
    }
    set_error(parser, "Invalid value.", parser->offset);
    return MYLITE_JSON_STATUS_INVALID_DOCUMENT;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int parse_dom_object(struct json_parser *parser, struct json_value *out_value) {
    int status = MYLITE_JSON_STATUS_OK;

    out_value->type = MYLITE_JSON_TYPE_OBJECT;
    if (!consume_byte(parser, '{')) {
        return MYLITE_JSON_STATUS_INVALID_DOCUMENT;
    }
    skip_whitespace(parser);
    if (consume_byte(parser, '}')) {
        return MYLITE_JSON_STATUS_OK;
    }
    for (;;) {
        char *key = NULL;
        size_t key_length = 0U;
        struct json_value *value = NULL;

        if (!parse_string(parser, &key, &key_length)) {
            return MYLITE_JSON_STATUS_INVALID_DOCUMENT;
        }
        skip_whitespace(parser);
        if (!consume_byte(parser, ':')) {
            set_error(parser, "Missing a colon after a name of object member.", parser->offset);
            free(key);
            return MYLITE_JSON_STATUS_INVALID_DOCUMENT;
        }
        value = calloc(1U, sizeof(*value));
        if (value == NULL) {
            free(key);
            return MYLITE_JSON_STATUS_NOMEM;
        }
        status = parse_dom_value(parser, value);
        if (status != MYLITE_JSON_STATUS_OK) {
            free(key);
            free(value);
            return status;
        }
        status = json_object_append_member(out_value, key, key_length, value);
        if (status != MYLITE_JSON_STATUS_OK) {
            free(key);
            json_value_deinit(value);
            free(value);
            return status;
        }
        skip_whitespace(parser);
        if (consume_byte(parser, '}')) {
            qsort(
                out_value->members,
                out_value->member_count,
                sizeof(*out_value->members),
                json_object_member_compare
            );
            return MYLITE_JSON_STATUS_OK;
        }
        if (!consume_byte(parser, ',')) {
            set_error(parser, "Missing a comma or '}' after an object member.", parser->offset);
            return MYLITE_JSON_STATUS_INVALID_DOCUMENT;
        }
        skip_whitespace(parser);
    }
}

// NOLINTNEXTLINE(misc-no-recursion)
static int parse_dom_array(struct json_parser *parser, struct json_value *out_value) {
    int status = MYLITE_JSON_STATUS_OK;

    out_value->type = MYLITE_JSON_TYPE_ARRAY;
    if (!consume_byte(parser, '[')) {
        return MYLITE_JSON_STATUS_INVALID_DOCUMENT;
    }
    skip_whitespace(parser);
    if (consume_byte(parser, ']')) {
        return MYLITE_JSON_STATUS_OK;
    }
    for (;;) {
        struct json_value *items = NULL;

        items = realloc(out_value->items, (out_value->item_count + 1U) * sizeof(*items));
        if (items == NULL) {
            return MYLITE_JSON_STATUS_NOMEM;
        }
        out_value->items = items;
        out_value->items[out_value->item_count] = (struct json_value){0};
        status = parse_dom_value(parser, &out_value->items[out_value->item_count]);
        if (status != MYLITE_JSON_STATUS_OK) {
            return status;
        }
        ++out_value->item_count;
        skip_whitespace(parser);
        if (consume_byte(parser, ']')) {
            return MYLITE_JSON_STATUS_OK;
        }
        if (!consume_byte(parser, ',')) {
            set_error(parser, "Missing a comma or ']' after an array element.", parser->offset);
            return MYLITE_JSON_STATUS_INVALID_DOCUMENT;
        }
        skip_whitespace(parser);
    }
}

static int parse_dom_number(struct json_parser *parser, struct json_value *out_value) {
    enum mylite_json_type type = MYLITE_JSON_TYPE_INVALID;
    size_t start = parser->offset;

    if (!parse_number(parser, &type)) {
        return MYLITE_JSON_STATUS_INVALID_DOCUMENT;
    }
    out_value->type = type;
    return normalize_json_number_text(
        parser->text + start,
        parser->offset - start,
        type,
        &out_value->text,
        &out_value->text_length
    );
}

static int json_value_to_text(const struct json_value *value, char **out_text, size_t *out_length) {
    char *text = NULL;
    size_t length = 0U;
    int status = append_json_value(&text, &length, value);

    if (status == MYLITE_JSON_STATUS_OK) {
        *out_text = text;
        *out_length = length;
        return MYLITE_JSON_STATUS_OK;
    }
    free(text);
    return status;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int append_json_value(char **text, size_t *length, const struct json_value *value) {
    char *quoted = NULL;
    size_t quoted_length = 0U;
    int status = MYLITE_JSON_STATUS_OK;

    if (value == NULL) {
        return MYLITE_JSON_STATUS_NOMEM;
    }
    switch (value->type) {
    case MYLITE_JSON_TYPE_NULL:
        if (!append_bytes(text, length, "null", 4U)) {
            return MYLITE_JSON_STATUS_NOMEM;
        }
        return MYLITE_JSON_STATUS_OK;
    case MYLITE_JSON_TYPE_BOOLEAN:
    case MYLITE_JSON_TYPE_INTEGER:
    case MYLITE_JSON_TYPE_DOUBLE:
        if (!append_bytes(text, length, value->text, value->text_length)) {
            return MYLITE_JSON_STATUS_NOMEM;
        }
        return MYLITE_JSON_STATUS_OK;
    case MYLITE_JSON_TYPE_STRING:
        status =
            mylite_json_quote_string(value->text, value->text_length, &quoted, &quoted_length) == 0
                ? MYLITE_JSON_STATUS_OK
                : MYLITE_JSON_STATUS_NOMEM;
        if (status == MYLITE_JSON_STATUS_OK && !append_bytes(text, length, quoted, quoted_length)) {
            status = MYLITE_JSON_STATUS_NOMEM;
        }
        free(quoted);
        return status;
    case MYLITE_JSON_TYPE_ARRAY:
        return append_json_array_value(text, length, value);
    case MYLITE_JSON_TYPE_OBJECT:
        return append_json_object_value(text, length, value);
    case MYLITE_JSON_TYPE_INVALID:
        return MYLITE_JSON_STATUS_NOMEM;
    }
    return MYLITE_JSON_STATUS_NOMEM;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int append_json_array_value(char **text, size_t *length, const struct json_value *value) {
    if (!append_byte(text, length, '[')) {
        return MYLITE_JSON_STATUS_NOMEM;
    }
    for (size_t index = 0U; index < value->item_count; ++index) {
        int status = MYLITE_JSON_STATUS_OK;

        if (index != 0U && !append_bytes(text, length, ", ", 2U)) {
            return MYLITE_JSON_STATUS_NOMEM;
        }
        status = append_json_value(text, length, &value->items[index]);
        if (status != MYLITE_JSON_STATUS_OK) {
            return status;
        }
    }
    if (!append_byte(text, length, ']')) {
        return MYLITE_JSON_STATUS_NOMEM;
    }
    return MYLITE_JSON_STATUS_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int append_json_object_value(char **text, size_t *length, const struct json_value *value) {
    if (!append_byte(text, length, '{')) {
        return MYLITE_JSON_STATUS_NOMEM;
    }
    for (size_t index = 0U; index < value->member_count; ++index) {
        char *quoted = NULL;
        size_t quoted_length = 0U;
        int status = MYLITE_JSON_STATUS_OK;

        if (index != 0U && !append_bytes(text, length, ", ", 2U)) {
            return MYLITE_JSON_STATUS_NOMEM;
        }
        status = mylite_json_quote_string(
                     value->members[index].key,
                     value->members[index].key_length,
                     &quoted,
                     &quoted_length
                 ) == 0
                     ? MYLITE_JSON_STATUS_OK
                     : MYLITE_JSON_STATUS_NOMEM;
        if (status == MYLITE_JSON_STATUS_OK && !append_bytes(text, length, quoted, quoted_length)) {
            status = MYLITE_JSON_STATUS_NOMEM;
        }
        if (status == MYLITE_JSON_STATUS_OK && !append_bytes(text, length, ": ", 2U)) {
            status = MYLITE_JSON_STATUS_NOMEM;
        }
        if (status == MYLITE_JSON_STATUS_OK) {
            status = append_json_value(text, length, value->members[index].value);
        }
        free(quoted);
        if (status != MYLITE_JSON_STATUS_OK) {
            return status;
        }
    }
    if (!append_byte(text, length, '}')) {
        return MYLITE_JSON_STATUS_NOMEM;
    }
    return MYLITE_JSON_STATUS_OK;
}

// Recursive descent keeps the JSON grammar small and mirrors nested document structure.
// NOLINTNEXTLINE(misc-no-recursion)
static bool parse_value(struct json_parser *parser, enum mylite_json_type *out_type) {
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
static bool parse_object(struct json_parser *parser) {
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
static bool parse_array(struct json_parser *parser) {
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

static bool parse_string(struct json_parser *parser, char **out_text, size_t *out_length) {
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
            if (!parse_escape(
                    parser,
                    out_text == NULL ? NULL : &result,
                    out_length == NULL ? NULL : &result_length
                )) {
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

static bool parse_escape(struct json_parser *parser, char **out_text, size_t *out_length) {
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

static bool parse_unicode_escape(struct json_parser *parser, uint32_t *out_codepoint) {
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

static bool parse_hex_quad(struct json_parser *parser, uint32_t *out_codepoint) {
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

static bool parse_number(struct json_parser *parser, enum mylite_json_type *out_type) {
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

static bool parse_digits(struct json_parser *parser) {
    size_t start = parser->offset;

    while (!at_end(parser) && isdigit((unsigned char)parser->text[parser->offset])) {
        ++parser->offset;
    }
    return parser->offset > start;
}

static bool parse_literal(struct json_parser *parser, const char *literal) {
    size_t length = strlen(literal);

    if (parser->offset + length > parser->length ||
        memcmp(parser->text + parser->offset, literal, length) != 0) {
        return false;
    }
    parser->offset += length;
    return true;
}

static int parse_json_path(
    const char *path_text,
    size_t path_length,
    bool allow_multiple_matches,
    struct json_path *out_path,
    struct mylite_json_error *out_error
) {
    struct json_parser parser = {.text = path_text, .length = path_length};
    int status = MYLITE_JSON_STATUS_OK;

    if (out_error != NULL) {
        *out_error = (struct mylite_json_error){0};
    }
    if (!consume_byte(&parser, '$')) {
        set_path_error(out_error, "Invalid JSON path expression.", 1U);
        return MYLITE_JSON_STATUS_INVALID_PATH;
    }
    while (!at_end(&parser)) {
        if (parser.text[parser.offset] == '.') {
            status = parse_json_path_member(&parser, allow_multiple_matches, out_path, out_error);
        } else if (parser.text[parser.offset] == '[') {
            status = parse_json_path_array(&parser, allow_multiple_matches, out_path, out_error);
        } else if (
            parser.offset + 1U < parser.length && parser.text[parser.offset] == '*' &&
            parser.text[parser.offset + 1U] == '*'
        ) {
            if (!allow_multiple_matches) {
                set_path_error(
                    out_error,
                    "Path expression may not contain recursive wildcard.",
                    parser.offset + 1U
                );
                status = MYLITE_JSON_STATUS_PATH_WILDCARD_NOT_ALLOWED;
                json_path_deinit(out_path);
                return status;
            }
            status = append_json_path_leg(
                out_path,
                (struct json_path_leg){.kind = JSON_PATH_LEG_RECURSIVE}
            );
            if (status == MYLITE_JSON_STATUS_OK) {
                out_path->can_match_multiple = true;
                parser.offset += 2U;
                if (at_end(&parser)) {
                    set_path_error(out_error, "Invalid JSON path expression.", parser.offset);
                    status = MYLITE_JSON_STATUS_INVALID_PATH;
                }
            }
        } else {
            set_path_error(out_error, "Invalid JSON path expression.", parser.offset + 1U);
            status = MYLITE_JSON_STATUS_INVALID_PATH;
        }
        if (status != MYLITE_JSON_STATUS_OK) {
            json_path_deinit(out_path);
            return status;
        }
    }
    return MYLITE_JSON_STATUS_OK;
}

static int parse_json_path_member(
    struct json_parser *parser,
    bool allow_multiple_matches,
    struct json_path *path,
    struct mylite_json_error *out_error
) {
    size_t member_start = 0U;
    char *member = NULL;
    size_t member_length = 0U;

    ++parser->offset;
    if (at_end(parser)) {
        set_path_error(out_error, "Invalid JSON path expression.", parser->offset);
        return MYLITE_JSON_STATUS_INVALID_PATH;
    }
    if (consume_byte(parser, '*')) {
        if (!allow_multiple_matches) {
            set_path_error(out_error, "Path expression may not contain wildcard.", parser->offset);
            return MYLITE_JSON_STATUS_PATH_WILDCARD_NOT_ALLOWED;
        }
        path->can_match_multiple = true;
        return append_json_path_leg(
            path,
            (struct json_path_leg){.kind = JSON_PATH_LEG_MEMBER_WILDCARD}
        );
    }
    if (parser->text[parser->offset] == '"') {
        if (!parse_string(parser, &member, &member_length)) {
            set_path_error(out_error, "Invalid JSON path expression.", parser->offset + 1U);
            return MYLITE_JSON_STATUS_INVALID_PATH;
        }
    } else {
        member_start = parser->offset;
        while (!at_end(parser) && parser->text[parser->offset] != '.' &&
               parser->text[parser->offset] != '[') {
            if (parser->offset + 1U < parser->length && parser->text[parser->offset] == '*' &&
                parser->text[parser->offset + 1U] == '*') {
                break;
            }
            ++parser->offset;
        }
        if (parser->offset == member_start) {
            set_path_error(out_error, "Invalid JSON path expression.", parser->offset + 1U);
            return MYLITE_JSON_STATUS_INVALID_PATH;
        }
        member = copy_text(parser->text + member_start, parser->offset - member_start);
        member_length = parser->offset - member_start;
        if (member == NULL) {
            return MYLITE_JSON_STATUS_NOMEM;
        }
    }
    return append_json_path_leg(
        path,
        (
            struct json_path_leg
        ){.kind = JSON_PATH_LEG_MEMBER, .member = member, .member_length = member_length}
    );
}

static int parse_json_path_array(
    struct json_parser *parser,
    bool allow_multiple_matches,
    struct json_path *path,
    struct mylite_json_error *out_error
) {
    struct json_path_array_position start = {.from_last = false, .offset = 0U};
    struct json_path_array_position end = {.from_last = false, .offset = 0U};

    ++parser->offset;
    skip_whitespace(parser);
    if (consume_byte(parser, '*')) {
        skip_whitespace(parser);
        if (!consume_byte(parser, ']')) {
            set_path_error(out_error, "Invalid JSON path expression.", parser->offset + 1U);
            return MYLITE_JSON_STATUS_INVALID_PATH;
        }
        if (!allow_multiple_matches) {
            set_path_error(out_error, "Path expression may not contain wildcard.", parser->offset);
            return MYLITE_JSON_STATUS_PATH_WILDCARD_NOT_ALLOWED;
        }
        path->can_match_multiple = true;
        return append_json_path_leg(
            path,
            (struct json_path_leg){.kind = JSON_PATH_LEG_ARRAY_WILDCARD}
        );
    }
    if (!parse_json_path_array_position(parser, &start)) {
        set_path_error(out_error, "Invalid JSON path expression.", parser->offset + 1U);
        return MYLITE_JSON_STATUS_INVALID_PATH;
    }
    skip_whitespace(parser);
    if (parser->offset + 2U <= parser->length && parser->text[parser->offset] == 't' &&
        parser->text[parser->offset + 1U] == 'o') {
        parser->offset += 2U;
        skip_whitespace(parser);
        if (!parse_json_path_array_position(parser, &end)) {
            set_path_error(out_error, "Invalid JSON path expression.", parser->offset + 1U);
            return MYLITE_JSON_STATUS_INVALID_PATH;
        }
        skip_whitespace(parser);
        if (!consume_byte(parser, ']')) {
            set_path_error(out_error, "Invalid JSON path expression.", parser->offset + 1U);
            return MYLITE_JSON_STATUS_INVALID_PATH;
        }
        if (!allow_multiple_matches) {
            set_path_error(out_error, "Path expression may not contain range.", parser->offset);
            return MYLITE_JSON_STATUS_PATH_WILDCARD_NOT_ALLOWED;
        }
        path->can_match_multiple = true;
        return append_json_path_leg(
            path,
            (
                struct json_path_leg
            ){.kind = JSON_PATH_LEG_ARRAY_RANGE, .range_start = start, .range_end = end}
        );
    }
    if (!consume_byte(parser, ']')) {
        set_path_error(out_error, "Invalid JSON path expression.", parser->offset + 1U);
        return MYLITE_JSON_STATUS_INVALID_PATH;
    }
    return append_json_path_leg(
        path,
        (struct json_path_leg){.kind = JSON_PATH_LEG_ARRAY_INDEX, .index = start}
    );
}

static bool parse_json_path_array_position(
    struct json_parser *parser,
    struct json_path_array_position *out_position
) {
    size_t value = 0U;
    bool from_last = false;
    bool saw_digit = false;

    if (parser->offset + 4U <= parser->length &&
        memcmp(parser->text + parser->offset, "last", 4U) == 0) {
        from_last = true;
        parser->offset += 4U;
        skip_whitespace(parser);
        if (consume_byte(parser, '-')) {
            skip_whitespace(parser);
            while (!at_end(parser) && isdigit((unsigned char)parser->text[parser->offset])) {
                saw_digit = true;
                value =
                    (value * json_path_decimal_base) + (size_t)(parser->text[parser->offset] - '0');
                ++parser->offset;
            }
            if (!saw_digit) {
                return false;
            }
        }
    } else {
        while (!at_end(parser) && isdigit((unsigned char)parser->text[parser->offset])) {
            saw_digit = true;
            value = (value * json_path_decimal_base) + (size_t)(parser->text[parser->offset] - '0');
            ++parser->offset;
        }
        if (!saw_digit) {
            return false;
        }
    }
    *out_position = (struct json_path_array_position){.from_last = from_last, .offset = value};
    return true;
}

static int append_json_path_leg(struct json_path *path, struct json_path_leg leg) {
    struct json_path_leg *updated = realloc(path->legs, (path->leg_count + 1U) * sizeof(*updated));

    if (updated == NULL) {
        free(leg.member);
        return MYLITE_JSON_STATUS_NOMEM;
    }
    path->legs = updated;
    path->legs[path->leg_count++] = leg;
    return MYLITE_JSON_STATUS_OK;
}

static int eval_json_path(
    const struct json_value *root,
    const struct json_path *path,
    struct json_value_ref_list *out_matches
) {
    return eval_json_path_from(root, path, 0U, out_matches);
}

// NOLINTNEXTLINE(misc-no-recursion)
static int eval_json_path_from(
    const struct json_value *value,
    const struct json_path *path,
    size_t leg_index,
    struct json_value_ref_list *matches
) {
    struct json_value_ref_list next = {0};
    int status = MYLITE_JSON_STATUS_OK;

    if (leg_index == path->leg_count) {
        return json_value_ref_list_append(matches, value);
    }
    if (path->legs[leg_index].kind == JSON_PATH_LEG_RECURSIVE) {
        return eval_json_path_recursive(value, path, leg_index + 1U, matches);
    }
    status = eval_json_path_leg(value, &path->legs[leg_index], &next);
    for (size_t index = 0U; status == MYLITE_JSON_STATUS_OK && index < next.count; ++index) {
        status = eval_json_path_from(next.items[index], path, leg_index + 1U, matches);
    }
    json_value_ref_list_deinit(&next);
    return status;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int eval_json_path_recursive(
    const struct json_value *value,
    const struct json_path *path,
    size_t next_leg_index,
    struct json_value_ref_list *matches
) {
    int status = eval_json_path_from(value, path, next_leg_index, matches);

    if (status != MYLITE_JSON_STATUS_OK) {
        return status;
    }
    if (value->type == MYLITE_JSON_TYPE_OBJECT) {
        for (size_t index = 0U; index < value->member_count; ++index) {
            status = eval_json_path_recursive(
                value->members[index].value,
                path,
                next_leg_index,
                matches
            );
            if (status != MYLITE_JSON_STATUS_OK) {
                return status;
            }
        }
    } else if (value->type == MYLITE_JSON_TYPE_ARRAY) {
        for (size_t index = 0U; index < value->item_count; ++index) {
            status = eval_json_path_recursive(&value->items[index], path, next_leg_index, matches);
            if (status != MYLITE_JSON_STATUS_OK) {
                return status;
            }
        }
    }
    return MYLITE_JSON_STATUS_OK;
}

static int eval_json_path_leg(
    const struct json_value *value,
    const struct json_path_leg *leg,
    struct json_value_ref_list *matches
) {
    switch (leg->kind) {
    case JSON_PATH_LEG_MEMBER:
    case JSON_PATH_LEG_MEMBER_WILDCARD:
        return eval_json_path_member(value, leg, matches);
    case JSON_PATH_LEG_ARRAY_INDEX:
        return eval_json_path_array_index(value, leg, matches);
    case JSON_PATH_LEG_ARRAY_WILDCARD:
        if (value->type != MYLITE_JSON_TYPE_ARRAY) {
            return MYLITE_JSON_STATUS_OK;
        }
        for (size_t index = 0U; index < value->item_count; ++index) {
            int status = json_value_ref_list_append(matches, &value->items[index]);

            if (status != MYLITE_JSON_STATUS_OK) {
                return status;
            }
        }
        return MYLITE_JSON_STATUS_OK;
    case JSON_PATH_LEG_ARRAY_RANGE:
        return eval_json_path_array_range(value, leg, matches);
    case JSON_PATH_LEG_RECURSIVE:
        return MYLITE_JSON_STATUS_OK;
    }
    return MYLITE_JSON_STATUS_OK;
}

static int eval_json_path_member(
    const struct json_value *value,
    const struct json_path_leg *leg,
    struct json_value_ref_list *matches
) {
    if (value->type != MYLITE_JSON_TYPE_OBJECT) {
        return MYLITE_JSON_STATUS_OK;
    }
    for (size_t index = 0U; index < value->member_count; ++index) {
        bool selected = leg->kind == JSON_PATH_LEG_MEMBER_WILDCARD;

        if (!selected && value->members[index].key_length == leg->member_length) {
            selected = memcmp(value->members[index].key, leg->member, leg->member_length) == 0;
        }

        if (selected) {
            int status = json_value_ref_list_append(matches, value->members[index].value);

            if (status != MYLITE_JSON_STATUS_OK) {
                return status;
            }
        }
    }
    return MYLITE_JSON_STATUS_OK;
}

static int eval_json_path_array_index(
    const struct json_value *value,
    const struct json_path_leg *leg,
    struct json_value_ref_list *matches
) {
    size_t index = 0U;

    if (value->type != MYLITE_JSON_TYPE_ARRAY ||
        !json_path_array_position_value(leg->index, value->item_count, &index)) {
        return MYLITE_JSON_STATUS_OK;
    }
    return json_value_ref_list_append(matches, &value->items[index]);
}

static int eval_json_path_array_range(
    const struct json_value *value,
    const struct json_path_leg *leg,
    struct json_value_ref_list *matches
) {
    size_t start = 0U;
    size_t end = 0U;

    if (value->type != MYLITE_JSON_TYPE_ARRAY ||
        !json_path_array_position_value(leg->range_start, value->item_count, &start) ||
        !json_path_array_position_value(leg->range_end, value->item_count, &end) || start > end) {
        return MYLITE_JSON_STATUS_OK;
    }
    for (size_t index = start; index <= end; ++index) {
        int status = json_value_ref_list_append(matches, &value->items[index]);

        if (status != MYLITE_JSON_STATUS_OK) {
            return status;
        }
        if (index == SIZE_MAX) {
            break;
        }
    }
    return MYLITE_JSON_STATUS_OK;
}

static bool json_path_array_position_value(
    struct json_path_array_position position,
    size_t item_count,
    size_t *out_index
) {
    if (item_count == 0U) {
        return false;
    }
    if (position.from_last) {
        if (position.offset >= item_count) {
            return false;
        }
        *out_index = item_count - 1U - position.offset;
        return true;
    }
    if (position.offset >= item_count) {
        return false;
    }
    *out_index = position.offset;
    return true;
}

static int json_value_ref_list_append(
    struct json_value_ref_list *list,
    const struct json_value *value
) {
    const struct json_value **updated = (const struct json_value **)
        realloc((void *)list->items, (list->count + 1U) * sizeof(*updated));

    if (updated == NULL) {
        return MYLITE_JSON_STATUS_NOMEM;
    }
    list->items = updated;
    list->items[list->count++] = value;
    return MYLITE_JSON_STATUS_OK;
}

static uint64_t json_value_length(const struct json_value *value) {
    if (value == NULL) {
        return 0U;
    }
    if (value->type == MYLITE_JSON_TYPE_ARRAY) {
        return value->item_count;
    }
    if (value->type == MYLITE_JSON_TYPE_OBJECT) {
        return value->member_count;
    }
    return 1U;
}

static int json_object_append_member(
    struct json_value *object,
    char *key,
    size_t key_length,
    struct json_value *value
) {
    struct json_dom_member *updated = NULL;

    for (size_t index = 0U; index < object->member_count; ++index) {
        if (object->members[index].key_length == key_length &&
            memcmp(object->members[index].key, key, key_length) == 0) {
            free(key);
            json_value_deinit(object->members[index].value);
            free(object->members[index].value);
            object->members[index].value = value;
            return MYLITE_JSON_STATUS_OK;
        }
    }
    updated = realloc(object->members, (object->member_count + 1U) * sizeof(*updated));
    if (updated == NULL) {
        return MYLITE_JSON_STATUS_NOMEM;
    }
    object->members = updated;
    object->members[object->member_count++] = (struct json_dom_member){
        .key = key,
        .key_length = key_length,
        .value = value,
    };
    return MYLITE_JSON_STATUS_OK;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
static int json_object_member_compare(const void *left, const void *right) {
    const struct json_dom_member *left_member = left;
    const struct json_dom_member *right_member = right;
    size_t min_length = left_member->key_length < right_member->key_length
                            ? left_member->key_length
                            : right_member->key_length;
    int comparison = 0;

    if (left_member->key_length != right_member->key_length) {
        return left_member->key_length < right_member->key_length ? -1 : 1;
    }
    comparison = memcmp(left_member->key, right_member->key, min_length);
    if (comparison < 0) {
        return -1;
    }
    if (comparison > 0) {
        return 1;
    }
    return 0;
}

// NOLINTBEGIN(bugprone-easily-swappable-parameters)
static int normalize_json_number_text(
    const char *text,
    size_t length,
    enum mylite_json_type type,
    char **out_text,
    size_t *out_length
)
// NOLINTEND(bugprone-easily-swappable-parameters)
{
    char *copy = NULL;
    int status = MYLITE_JSON_STATUS_OK;

    if (type == MYLITE_JSON_TYPE_INTEGER) {
        if (length == 2U && text[0] == '-' && text[1] == '0') {
            *out_text = copy_text("0", 1U);
            *out_length = 1U;
            return *out_text == NULL ? MYLITE_JSON_STATUS_NOMEM : MYLITE_JSON_STATUS_OK;
        }
        *out_text = copy_text(text, length);
        *out_length = length;
        return *out_text == NULL ? MYLITE_JSON_STATUS_NOMEM : MYLITE_JSON_STATUS_OK;
    }
    copy = copy_text(text, length);
    if (copy == NULL) {
        return MYLITE_JSON_STATUS_NOMEM;
    }
    status = format_json_double_text(strtod(copy, NULL), out_text, out_length);
    free(copy);
    return status;
}

static int format_json_double_text(double value, char **out_text, size_t *out_length) {
    char buffer[json_number_buffer_length];
    int length = snprintf(buffer, sizeof(buffer), "%.16g", value);
    bool has_decimal_marker = false;

    if (length <= 0 || (size_t)length >= sizeof(buffer)) {
        return MYLITE_JSON_STATUS_NOMEM;
    }
    for (int index = 0; index < length; ++index) {
        if (buffer[index] == '.' || buffer[index] == 'e' || buffer[index] == 'E') {
            has_decimal_marker = true;
            break;
        }
    }
    if (!has_decimal_marker) {
        if ((size_t)length + 2U >= sizeof(buffer)) {
            return MYLITE_JSON_STATUS_NOMEM;
        }
        buffer[length++] = '.';
        buffer[length++] = '0';
        buffer[length] = '\0';
    }
    *out_text = copy_text(buffer, (size_t)length);
    *out_length = (size_t)length;
    return *out_text == NULL ? MYLITE_JSON_STATUS_NOMEM : MYLITE_JSON_STATUS_OK;
}

static bool append_utf8(char **text, size_t *length, uint32_t codepoint) {
    if (codepoint <= json_utf8_one_byte_max) {
        return append_byte(text, length, (char)codepoint);
    }
    if (codepoint <= json_utf8_two_byte_max) {
        char bytes[] = {
            (char)(json_utf8_two_byte_prefix | (codepoint >> json_utf8_shift_6)),
            (char)(json_utf8_continuation_prefix | (codepoint & json_utf8_payload_mask))
        };

        return append_bytes(text, length, bytes, sizeof(bytes));
    }
    if (codepoint <= json_utf8_three_byte_max) {
        char bytes[] = {
            (char)(json_utf8_three_byte_prefix | (codepoint >> json_utf8_shift_12)),
            (char)(json_utf8_continuation_prefix |
                   ((codepoint >> json_utf8_shift_6) & json_utf8_payload_mask)),
            (char)(json_utf8_continuation_prefix | (codepoint & json_utf8_payload_mask))
        };

        return append_bytes(text, length, bytes, sizeof(bytes));
    }
    if (codepoint <= json_utf8_four_byte_max) {
        char bytes[] = {
            (char)(json_utf8_four_byte_prefix | (codepoint >> json_utf8_shift_18)),
            (char)(json_utf8_continuation_prefix |
                   ((codepoint >> json_utf8_shift_12) & json_utf8_payload_mask)),
            (char)(json_utf8_continuation_prefix |
                   ((codepoint >> json_utf8_shift_6) & json_utf8_payload_mask)),
            (char)(json_utf8_continuation_prefix | (codepoint & json_utf8_payload_mask))
        };

        return append_bytes(text, length, bytes, sizeof(bytes));
    }
    return false;
}

static bool append_quoted_byte(char **text, size_t *length, unsigned char byte) {
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

static bool append_control_escape(char **text, size_t *length, unsigned char byte) {
    static const char hex[] = "0123456789abcdef";
    char escape[] =
        {'\\', 'u', '0', '0', hex[byte >> json_hex_nibble_shift], hex[byte & json_hex_nibble_mask]};

    return append_bytes(text, length, escape, sizeof(escape));
}

static bool append_optional_byte(char **text, size_t *length, char byte) {
    if (text == NULL) {
        return true;
    }
    return append_byte(text, length, byte);
}

static bool append_optional_utf8(char **text, size_t *length, uint32_t codepoint) {
    if (text == NULL) {
        return true;
    }
    return append_utf8(text, length, codepoint);
}

static bool append_byte(char **text, size_t *length, char byte) {
    return append_bytes(text, length, &byte, 1U);
}

static bool append_bytes(
    char **text,
    size_t *length,
    const char *addition,
    size_t addition_length
) {
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

static char *copy_text(const char *text, size_t length) {
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

static void skip_whitespace(struct json_parser *parser) {
    while (!at_end(parser) && isspace((unsigned char)parser->text[parser->offset])) {
        ++parser->offset;
    }
}

static bool consume_byte(struct json_parser *parser, char expected) {
    if (at_end(parser) || parser->text[parser->offset] != expected) {
        return false;
    }
    ++parser->offset;
    return true;
}

static bool peek_byte(const struct json_parser *parser, char *out_byte) {
    if (at_end(parser)) {
        return false;
    }
    *out_byte = parser->text[parser->offset];
    return true;
}

static bool at_end(const struct json_parser *parser) {
    if (parser == NULL || parser->text == NULL) {
        return true;
    }
    return parser->offset >= parser->length;
}

static void set_error(struct json_parser *parser, const char *message, size_t position) {
    if (parser != NULL && parser->error != NULL && parser->error->message == NULL) {
        *parser->error = (struct mylite_json_error){.message = message, .position = position};
    }
}

static void set_path_error(struct mylite_json_error *error, const char *message, size_t position) {
    if (error != NULL && error->message == NULL) {
        *error = (struct mylite_json_error){.message = message, .position = position};
    }
}

// NOLINTNEXTLINE(misc-no-recursion)
static void json_value_deinit(struct json_value *value) {
    if (value == NULL) {
        return;
    }
    free(value->text);
    for (size_t index = 0U; index < value->item_count; ++index) {
        json_value_deinit(&value->items[index]);
    }
    free(value->items);
    for (size_t index = 0U; index < value->member_count; ++index) {
        free(value->members[index].key);
        json_value_deinit(value->members[index].value);
        free(value->members[index].value);
    }
    free(value->members);
    *value = (struct json_value){0};
}

static void json_path_deinit(struct json_path *path) {
    if (path == NULL) {
        return;
    }
    for (size_t index = 0U; index < path->leg_count; ++index) {
        free(path->legs[index].member);
    }
    free(path->legs);
    *path = (struct json_path){0};
}

static void json_value_ref_list_deinit(struct json_value_ref_list *list) {
    if (list == NULL) {
        return;
    }
    free((void *)list->items);
    *list = (struct json_value_ref_list){0};
}

static bool is_hex_digit(char byte) {
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

static uint32_t hex_digit_value(char byte) {
    if (byte >= '0' && byte <= '9') {
        return (uint32_t)(byte - '0');
    }
    if (byte >= 'a' && byte <= 'f') {
        return (uint32_t)(byte - 'a' + json_hex_alpha_offset);
    }
    return (uint32_t)(byte - 'A' + json_hex_alpha_offset);
}
