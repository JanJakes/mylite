#include <mylite/mylite.h>

#include "mylite_json.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum json_value_kind {
    JSON_VALUE_NULL,
    JSON_VALUE_BOOL,
    JSON_VALUE_NUMBER,
    JSON_VALUE_STRING,
    JSON_VALUE_ARRAY,
    JSON_VALUE_OBJECT,
};

enum {
    json_initial_container_capacity = 4,
    json_writer_initial_capacity = 32,
    json_unicode_escape_digit_count = 4,
    json_signed_int64_digit_count = 19,
    json_max_nesting_depth = 100,
    json_true_literal_length = 4,
    json_false_literal_length = 5,
    json_null_literal_length = 4,
    json_member_separator_length = 2,
    json_unicode_escape_prefix_length = 4,
    json_hex_nibble_bits = 4,
    json_decimal_base = 10,
    json_control_byte_limit = 0x20,
    json_ascii_byte_limit = 0x7f,
    json_hex_low_nibble = 0x0f,
    json_sql_integer_buffer_length = 32,
    json_contains_stack_capacity = (json_max_nesting_depth * 2) + 2,
};

struct json_text {
    char *text;
    size_t length;
};

struct json_value;

struct json_array {
    struct json_value *values;
    size_t count;
    size_t capacity;
};

struct json_member {
    char *key;
    size_t key_length;
    struct json_value *value;
};

struct json_object {
    struct json_member *members;
    size_t count;
    size_t capacity;
};

struct json_value {
    enum json_value_kind kind;

    union {
        bool boolean;
        struct json_text text;
        struct json_array array;
        struct json_object object;
    } payload;
};

struct json_parser {
    const char *text;
    size_t length;
    size_t position;
    struct mylite_json_normalize_result result;
};

struct json_writer {
    char *text;
    size_t length;
    size_t capacity;
};

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

struct json_emit_frame {
    const struct json_value *container;
    size_t index;
};

struct json_emit_stack {
    struct json_emit_frame frames[json_max_nesting_depth];
    size_t count;
};

struct json_deinit_frame {
    struct json_value *value;
    size_t index;
    bool free_value;
};

struct json_deinit_stack {
    struct json_deinit_frame frames[json_max_nesting_depth + 1U];
    size_t count;
};

enum json_contains_frame_kind {
    JSON_CONTAINS_CHECK,
    JSON_CONTAINS_ARRAY_VALUE,
    JSON_CONTAINS_ARRAY_CANDIDATE,
    JSON_CONTAINS_OBJECT_CANDIDATE,
};

struct json_contains_frame {
    enum json_contains_frame_kind kind;
    const struct json_value *target;
    const struct json_value *candidate;
    size_t target_index;
    size_t candidate_index;
    bool waiting_child;
    bool child_result;
};

struct json_contains_stack {
    struct json_contains_frame frames[json_contains_stack_capacity];
    size_t count;
    bool done;
    bool result;
};

struct json_number_integer_part {
    size_t start;
    size_t end;
    bool is_negative;
};

static int parse_document(struct json_parser *parser, struct json_value *out_value);
static int json_value_from_sql_value(
    const struct mylite_json_sql_value *sql_value,
    struct json_value *out_value,
    struct mylite_json_normalize_result *out_result
);
static int json_object_key_from_sql_value(
    const struct mylite_json_sql_value *sql_value,
    char **out_key,
    size_t *out_key_length
);
static int copy_integer_text(int64_t value, char **out_text, size_t *out_text_length);
static int emit_constructed_json(
    struct json_value *value,
    char **out_text,
    size_t *out_text_length
);
static bool validate_document(struct json_parser *parser);
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
static int object_append_member(
    struct json_object *object,
    char *key,
    size_t key_length,
    struct json_value *value,
    struct json_value **out_stored_value
);
static int object_reserve_members(struct json_object *object, size_t required_capacity);
static void sort_object_members_by_mysql_display_order(struct json_object *object);
static bool object_member_is_after(const struct json_member *left, const struct json_member *right);
static int compare_object_member_keys(
    const struct json_member *left,
    const struct json_member *right
);
static int array_append_value(
    struct json_array *array,
    struct json_value *value,
    struct json_value **out_stored_value
);
static int array_reserve_values(struct json_array *array, size_t required_capacity);
static int parse_string_value(struct json_parser *parser, struct json_value *out_value);
static int parse_string(struct json_parser *parser, char **out_text, size_t *out_text_length);
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
static bool integer_number_is_in_signed_range(
    const char *digits,
    size_t digit_count,
    bool is_negative
);
static int copy_number_text(
    const char *text,
    size_t length,
    bool negative_zero,
    struct json_value *out_value
);
static int parse_literal(
    struct json_parser *parser,
    const char *literal,
    enum json_value_kind kind,
    bool boolean,
    struct json_value *out_value
);
static int extract_path_value(
    struct json_parser *parser,
    const struct json_value *root,
    const struct json_value **out_value,
    bool *out_matched
);
static bool json_value_contains(
    const struct json_value *target,
    const struct json_value *candidate
);
static bool json_contains_stack_push(
    struct json_contains_stack *stack,
    const struct json_value *target,
    const struct json_value *candidate
);
static bool json_contains_stack_complete(struct json_contains_stack *stack, bool result);
static bool json_contains_process_check_frame(
    struct json_contains_stack *stack,
    struct json_contains_frame *frame
);
static bool json_contains_process_array_value_frame(
    struct json_contains_stack *stack,
    struct json_contains_frame *frame
);
static bool json_contains_process_array_candidate_frame(
    struct json_contains_stack *stack,
    struct json_contains_frame *frame
);
static bool json_contains_process_object_candidate_frame(
    struct json_contains_stack *stack,
    struct json_contains_frame *frame
);
static bool json_scalar_values_equal(
    const struct json_value *target,
    const struct json_value *candidate
);
static const char *json_value_type_name(const struct json_value *value);
static int json_value_shallow_length(const struct json_value *value, int64_t *out_length);
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
static bool path_identifier_start_byte(char byte);
static bool path_identifier_byte(char byte);
static bool path_text_is_ascii(const char *text, size_t text_length);
static const struct json_value *object_member_value(
    const struct json_value *value,
    const char *member,
    size_t member_length
);
static const struct json_value *array_index_value(const struct json_value *value, size_t index);
static int emit_value(struct json_writer *writer, const struct json_value *value);
static int emit_value_start(
    struct json_writer *writer,
    const struct json_value *value,
    struct json_emit_stack *stack
);
static int emit_array_next_value(
    struct json_writer *writer,
    struct json_emit_stack *stack,
    const struct json_value **out_child
);
static int emit_object_next_member(
    struct json_writer *writer,
    struct json_emit_stack *stack,
    const struct json_value **out_child
);
static int emit_bool_value(struct json_writer *writer, bool boolean);
static int emit_stack_push(struct json_emit_stack *stack, const struct json_value *container);
static int emit_string(struct json_writer *writer, const char *text, size_t text_length);
static int writer_append_json_escape(struct json_writer *writer, unsigned char byte);
static int writer_append_ascii_hex_digit(struct json_writer *writer, unsigned char value);
static int writer_append_char(struct json_writer *writer, char byte);
static int writer_append_text(struct json_writer *writer, const char *text, size_t text_length);
static int writer_reserve(struct json_writer *writer, size_t required_capacity);
static char *writer_take(struct json_writer *writer);
static void writer_deinit(struct json_writer *writer);
static int copy_result_text(
    const char *text,
    size_t text_length,
    char **out_text,
    size_t *out_text_length
);
static void value_deinit(struct json_value *value);
static bool deinit_stack_push(
    struct json_deinit_stack *stack,
    struct json_value *value,
    bool free_value
);
static void deinit_stack_pop(struct json_deinit_stack *stack);
static void skip_whitespace(struct json_parser *parser);
static bool parser_at_end(const struct json_parser *parser);
static char parser_peek(const struct json_parser *parser);
static bool parser_match(struct json_parser *parser, char expected);
static bool is_decimal_digit(char byte);
static bool is_hex_digit(char byte);
static int parser_invalid(struct json_parser *parser, size_t position);
static int parser_unsupported(struct json_parser *parser, size_t position);

int mylite_json_normalize(
    const char *text,
    size_t text_length,
    char **out_text,
    size_t *out_text_length,
    struct mylite_json_normalize_result *out_result
) {
    struct json_parser parser = {
        .text = text,
        .length = text_length,
        .position = 0U,
        .result = {.status = MYLITE_JSON_NORMALIZE_OK, .position = 0U},
    };
    struct json_value value = {0};
    struct json_writer writer = {0};
    int rc = MYLITE_OK;

    if (out_text == NULL || out_text_length == NULL || out_result == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_text_length = 0U;
    *out_result = (struct mylite_json_normalize_result){
        .status = MYLITE_JSON_NORMALIZE_INVALID,
        .position = 0U,
    };
    if (text == NULL) {
        return MYLITE_ERROR;
    }

    rc = parse_document(&parser, &value);
    if (rc == MYLITE_OK) {
        rc = emit_value(&writer, &value);
    }
    if (rc == MYLITE_OK) {
        *out_text_length = writer.length;
        *out_text = writer_take(&writer);
        if (*out_text == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    *out_result = parser.result;
    value_deinit(&value);
    writer_deinit(&writer);
    return rc;
}

int mylite_json_validate(const char *text, size_t text_length, bool *out_is_valid) {
    struct json_parser parser = {
        .text = text,
        .length = text_length,
        .position = 0U,
        .result = {.status = MYLITE_JSON_NORMALIZE_OK, .position = 0U},
    };

    if (out_is_valid == NULL) {
        return MYLITE_MISUSE;
    }
    *out_is_valid = false;
    if (text == NULL) {
        return MYLITE_ERROR;
    }

    *out_is_valid = validate_document(&parser);
    return MYLITE_OK;
}

int mylite_json_type(
    const char *text,
    size_t text_length,
    const char **out_type,
    struct mylite_json_normalize_result *out_result
) {
    struct json_parser parser = {
        .text = text,
        .length = text_length,
        .position = 0U,
        .result = {.status = MYLITE_JSON_NORMALIZE_OK, .position = 0U},
    };
    struct json_value value = {0};
    int rc = MYLITE_OK;

    if (out_type == NULL || out_result == NULL) {
        return MYLITE_MISUSE;
    }
    *out_type = NULL;
    *out_result = (struct mylite_json_normalize_result){
        .status = MYLITE_JSON_NORMALIZE_INVALID,
        .position = 0U,
    };
    if (text == NULL) {
        return MYLITE_ERROR;
    }

    rc = parse_document(&parser, &value);
    *out_result = parser.result;
    if (rc == MYLITE_OK) {
        *out_type = json_value_type_name(&value);
    }

    value_deinit(&value);
    return rc;
}

int mylite_json_length(
    const char *text,
    size_t text_length,
    const char *path,
    size_t path_length,
    bool has_path,
    int64_t *out_length,
    bool *out_is_null,
    struct mylite_json_normalize_result *out_result
) {
    struct json_parser document_parser = {
        .text = text,
        .length = text_length,
        .position = 0U,
        .result = {.status = MYLITE_JSON_NORMALIZE_OK, .position = 0U},
    };
    struct json_parser path_parser = {
        .text = path,
        .length = path_length,
        .position = 0U,
        .result = {.status = MYLITE_JSON_NORMALIZE_OK, .position = 0U},
    };
    struct json_value document = {0};
    const struct json_value *measured_value = NULL;
    bool matched = false;
    int rc = MYLITE_OK;

    if (out_length == NULL || out_is_null == NULL || out_result == NULL) {
        return MYLITE_MISUSE;
    }
    *out_length = 0;
    *out_is_null = false;
    *out_result = (struct mylite_json_normalize_result){
        .status = MYLITE_JSON_NORMALIZE_INVALID,
        .position = 0U,
    };
    if (text == NULL || (has_path && path == NULL)) {
        return MYLITE_ERROR;
    }

    rc = parse_document(&document_parser, &document);
    *out_result = document_parser.result;
    if (rc == MYLITE_OK && has_path) {
        rc = extract_path_value(&path_parser, &document, &measured_value, &matched);
        *out_result = path_parser.result;
        if (rc == MYLITE_OK && !matched) {
            *out_is_null = true;
        }
    } else if (rc == MYLITE_OK) {
        measured_value = &document;
        matched = true;
    }
    if (rc == MYLITE_OK && matched) {
        rc = json_value_shallow_length(measured_value, out_length);
    }

    value_deinit(&document);
    return rc;
}

int mylite_json_extract(
    const char *text,
    size_t text_length,
    const char *path,
    size_t path_length,
    char **out_text,
    size_t *out_text_length,
    bool *out_is_null,
    struct mylite_json_normalize_result *out_result
) {
    struct json_parser document_parser = {
        .text = text,
        .length = text_length,
        .position = 0U,
        .result = {.status = MYLITE_JSON_NORMALIZE_OK, .position = 0U},
    };
    struct json_parser path_parser = {
        .text = path,
        .length = path_length,
        .position = 0U,
        .result = {.status = MYLITE_JSON_NORMALIZE_OK, .position = 0U},
    };
    struct json_value document = {0};
    struct json_writer writer = {0};
    const struct json_value *matched_value = NULL;
    bool matched = false;
    int rc = MYLITE_OK;

    if (out_text == NULL || out_text_length == NULL || out_is_null == NULL || out_result == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;
    *out_result = (struct mylite_json_normalize_result){
        .status = MYLITE_JSON_NORMALIZE_INVALID,
        .position = 0U,
    };
    if (text == NULL || path == NULL) {
        return MYLITE_ERROR;
    }

    rc = parse_document(&document_parser, &document);
    *out_result = document_parser.result;
    if (rc == MYLITE_OK) {
        rc = extract_path_value(&path_parser, &document, &matched_value, &matched);
        *out_result = path_parser.result;
    }
    if (rc == MYLITE_OK && !matched) {
        *out_is_null = true;
    } else if (rc == MYLITE_OK) {
        rc = emit_value(&writer, matched_value);
        if (rc == MYLITE_OK) {
            *out_text_length = writer.length;
            *out_text = writer_take(&writer);
            if (*out_text == NULL) {
                rc = MYLITE_NOMEM;
            }
        }
    }

    value_deinit(&document);
    writer_deinit(&writer);
    return rc;
}

int mylite_json_contains(
    const char *target,
    size_t target_length,
    const char *candidate,
    size_t candidate_length,
    const char *path,
    size_t path_length,
    bool has_path,
    int64_t *out_contains,
    bool *out_is_null,
    struct mylite_json_normalize_result *out_result
) {
    struct json_parser target_parser = {
        .text = target,
        .length = target_length,
        .position = 0U,
        .result = {.status = MYLITE_JSON_NORMALIZE_OK, .position = 0U},
    };
    struct json_parser candidate_parser = {
        .text = candidate,
        .length = candidate_length,
        .position = 0U,
        .result = {.status = MYLITE_JSON_NORMALIZE_OK, .position = 0U},
    };
    struct json_parser path_parser = {
        .text = path,
        .length = path_length,
        .position = 0U,
        .result = {.status = MYLITE_JSON_NORMALIZE_OK, .position = 0U},
    };
    struct json_value target_value = {0};
    struct json_value candidate_value = {0};
    const struct json_value *matched_target = NULL;
    bool matched = false;
    int rc = MYLITE_OK;

    if (out_contains == NULL || out_is_null == NULL || out_result == NULL) {
        return MYLITE_MISUSE;
    }
    *out_contains = 0;
    *out_is_null = false;
    *out_result = (struct mylite_json_normalize_result){
        .status = MYLITE_JSON_NORMALIZE_INVALID,
        .position = 0U,
    };
    if (target == NULL || candidate == NULL || (has_path && path == NULL)) {
        return MYLITE_ERROR;
    }

    rc = parse_document(&target_parser, &target_value);
    *out_result = target_parser.result;
    if (rc == MYLITE_OK) {
        rc = parse_document(&candidate_parser, &candidate_value);
        *out_result = candidate_parser.result;
    }
    if (rc == MYLITE_OK && has_path) {
        rc = extract_path_value(&path_parser, &target_value, &matched_target, &matched);
        *out_result = path_parser.result;
        if (rc == MYLITE_OK && !matched) {
            *out_is_null = true;
        }
    } else if (rc == MYLITE_OK) {
        matched_target = &target_value;
        matched = true;
    }
    if (rc == MYLITE_OK && matched) {
        if (json_value_contains(matched_target, &candidate_value)) {
            *out_contains = 1;
        }
    }

    value_deinit(&candidate_value);
    value_deinit(&target_value);
    return rc;
}

int mylite_json_contains_path(
    const char *text,
    size_t text_length,
    const char *const *paths,
    const size_t *path_lengths,
    size_t path_count,
    bool require_all,
    int64_t *out_contains,
    struct mylite_json_normalize_result *out_result
) {
    struct json_parser document_parser = {
        .text = text,
        .length = text_length,
        .position = 0U,
        .result = {.status = MYLITE_JSON_NORMALIZE_OK, .position = 0U},
    };
    struct json_value document = {0};
    bool any_match = false;
    int rc = MYLITE_OK;

    if (out_contains == NULL || out_result == NULL) {
        return MYLITE_MISUSE;
    }
    *out_contains = 0;
    *out_result = (struct mylite_json_normalize_result){
        .status = MYLITE_JSON_NORMALIZE_INVALID,
        .position = 0U,
    };
    if (text == NULL || (path_count != 0U && (paths == NULL || path_lengths == NULL))) {
        return MYLITE_ERROR;
    }

    rc = parse_document(&document_parser, &document);
    *out_result = document_parser.result;
    for (size_t path_index = 0U; rc == MYLITE_OK && path_index < path_count; ++path_index) {
        struct json_parser path_parser = {
            .text = paths[path_index],
            .length = path_lengths[path_index],
            .position = 0U,
            .result = {.status = MYLITE_JSON_NORMALIZE_OK, .position = 0U},
        };
        const struct json_value *matched_value = NULL;
        bool matched = false;

        rc = extract_path_value(&path_parser, &document, &matched_value, &matched);
        (void)matched_value;
        *out_result = path_parser.result;
        if (rc != MYLITE_OK) {
            break;
        }
        if (matched) {
            any_match = true;
        } else if (require_all) {
            *out_contains = 0;
            value_deinit(&document);
            return MYLITE_OK;
        }
        if (matched && !require_all) {
            *out_contains = 1;
            value_deinit(&document);
            return MYLITE_OK;
        }
    }
    if (rc == MYLITE_OK && (require_all || any_match)) {
        *out_contains = 1;
    }

    value_deinit(&document);
    return rc;
}

int mylite_json_path_validate(
    const char *path,
    size_t path_length,
    struct mylite_json_normalize_result *out_result
) {
    struct json_parser path_parser = {
        .text = path,
        .length = path_length,
        .position = 0U,
        .result = {.status = MYLITE_JSON_NORMALIZE_OK, .position = 0U},
    };
    const struct json_value *matched_value = NULL;
    bool matched = false;

    if (out_result == NULL) {
        return MYLITE_MISUSE;
    }
    *out_result = (struct mylite_json_normalize_result){
        .status = MYLITE_JSON_NORMALIZE_INVALID,
        .position = 0U,
    };
    if (path == NULL) {
        return MYLITE_ERROR;
    }

    int rc = extract_path_value(&path_parser, NULL, &matched_value, &matched);

    (void)matched_value;
    (void)matched;
    *out_result = path_parser.result;
    return rc;
}

int mylite_json_unquote(
    const char *text,
    size_t text_length,
    char **out_text,
    size_t *out_text_length,
    struct mylite_json_normalize_result *out_result
) {
    struct json_parser parser = {
        .text = text,
        .length = text_length,
        .position = 0U,
        .result = {.status = MYLITE_JSON_NORMALIZE_OK, .position = 0U},
    };
    char *decoded = NULL;
    size_t decoded_length = 0U;
    int rc = MYLITE_OK;

    if (out_text == NULL || out_text_length == NULL || out_result == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_text_length = 0U;
    *out_result = (struct mylite_json_normalize_result){
        .status = MYLITE_JSON_NORMALIZE_INVALID,
        .position = 0U,
    };
    if (text == NULL) {
        return MYLITE_ERROR;
    }

    if (text_length < 2U || text[0] != '"' || text[text_length - 1U] != '"') {
        *out_result = (struct mylite_json_normalize_result){
            .status = MYLITE_JSON_NORMALIZE_OK,
            .position = 0U,
        };
        return copy_result_text(text, text_length, out_text, out_text_length);
    }

    rc = parse_string(&parser, &decoded, &decoded_length);
    if (rc == MYLITE_OK && !parser_at_end(&parser)) {
        rc = parser_invalid(&parser, parser.position);
    }
    *out_result = parser.result;
    if (rc == MYLITE_OK) {
        *out_text = decoded;
        *out_text_length = decoded_length;
        decoded = NULL;
    }

    free(decoded);
    return rc;
}

int mylite_json_quote_string(
    const char *text,
    size_t text_length,
    char **out_text,
    size_t *out_text_length
) {
    struct json_writer writer = {0};
    int rc = MYLITE_OK;

    if (out_text == NULL || out_text_length == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_text_length = 0U;
    if (text == NULL) {
        return MYLITE_ERROR;
    }

    rc = emit_string(&writer, text, text_length);
    if (rc == MYLITE_OK) {
        size_t result_length = writer.length;

        *out_text = writer_take(&writer);
        if (*out_text == NULL) {
            rc = MYLITE_NOMEM;
        } else {
            *out_text_length = result_length;
        }
    }
    writer_deinit(&writer);
    return rc;
}

static const char *json_value_type_name(const struct json_value *value) {
    if (value == NULL) {
        return NULL;
    }

    switch (value->kind) {
    case JSON_VALUE_NULL:
        return "NULL";
    case JSON_VALUE_BOOL:
        return "BOOLEAN";
    case JSON_VALUE_NUMBER:
        return "INTEGER";
    case JSON_VALUE_STRING:
        return "STRING";
    case JSON_VALUE_ARRAY:
        return "ARRAY";
    case JSON_VALUE_OBJECT:
        return "OBJECT";
    }

    return NULL;
}

static int json_value_shallow_length(const struct json_value *value, int64_t *out_length) {
    size_t length = 1U;

    if (value == NULL || out_length == NULL) {
        return MYLITE_MISUSE;
    }
    if (value->kind == JSON_VALUE_ARRAY) {
        length = value->payload.array.count;
    } else if (value->kind == JSON_VALUE_OBJECT) {
        length = value->payload.object.count;
    }
    if (length > (size_t)INT64_MAX) {
        return MYLITE_NOMEM;
    }

    *out_length = (int64_t)length;
    return MYLITE_OK;
}

int mylite_json_array_from_sql_values(
    const struct mylite_json_sql_value *values,
    size_t value_count,
    char **out_text,
    size_t *out_text_length,
    struct mylite_json_normalize_result *out_result
) {
    struct json_value array = {.kind = JSON_VALUE_ARRAY};
    int rc = MYLITE_OK;

    if (out_text == NULL || out_text_length == NULL || out_result == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_text_length = 0U;
    *out_result = (struct mylite_json_normalize_result){
        .status = MYLITE_JSON_NORMALIZE_OK,
        .position = 0U,
    };
    if (value_count != 0U && values == NULL) {
        return MYLITE_MISUSE;
    }

    for (size_t value_index = 0U; rc == MYLITE_OK && value_index < value_count; ++value_index) {
        struct json_value value = {0};
        struct json_value *stored_value = NULL;

        rc = json_value_from_sql_value(&values[value_index], &value, out_result);
        if (rc == MYLITE_OK) {
            rc = array_append_value(&array.payload.array, &value, &stored_value);
            (void)stored_value;
        }
        if (rc != MYLITE_OK) {
            value_deinit(&value);
        }
    }
    if (rc == MYLITE_OK) {
        rc = emit_constructed_json(&array, out_text, out_text_length);
    }

    value_deinit(&array);
    return rc;
}

int mylite_json_object_from_sql_values(
    const struct mylite_json_sql_value *keys,
    const struct mylite_json_sql_value *values,
    size_t pair_count,
    char **out_text,
    size_t *out_text_length,
    struct mylite_json_normalize_result *out_result
) {
    struct json_value object = {.kind = JSON_VALUE_OBJECT};
    int rc = MYLITE_OK;

    if (out_text == NULL || out_text_length == NULL || out_result == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_text_length = 0U;
    *out_result = (struct mylite_json_normalize_result){
        .status = MYLITE_JSON_NORMALIZE_OK,
        .position = 0U,
    };
    if (pair_count != 0U && (keys == NULL || values == NULL)) {
        return MYLITE_MISUSE;
    }

    for (size_t pair_index = 0U; rc == MYLITE_OK && pair_index < pair_count; ++pair_index) {
        char *key = NULL;
        size_t key_length = 0U;
        struct json_value value = {0};
        struct json_value *stored_value = NULL;

        rc = json_object_key_from_sql_value(&keys[pair_index], &key, &key_length);
        if (rc == MYLITE_OK) {
            rc = json_value_from_sql_value(&values[pair_index], &value, out_result);
        }
        if (rc == MYLITE_OK) {
            rc = object_append_member(
                &object.payload.object,
                key,
                key_length,
                &value,
                &stored_value
            );
            if (rc == MYLITE_OK) {
                key = NULL;
            }
            (void)stored_value;
        }
        free(key);
        if (rc != MYLITE_OK) {
            value_deinit(&value);
        }
    }
    if (rc == MYLITE_OK) {
        sort_object_members_by_mysql_display_order(&object.payload.object);
        rc = emit_constructed_json(&object, out_text, out_text_length);
    }

    value_deinit(&object);
    return rc;
}

static int json_value_from_sql_value(
    const struct mylite_json_sql_value *sql_value,
    struct json_value *out_value,
    struct mylite_json_normalize_result *out_result
) {
    if (sql_value == NULL || out_value == NULL || out_result == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = (struct json_value){0};

    switch (sql_value->kind) {
    case MYLITE_JSON_SQL_VALUE_NULL:
        out_value->kind = JSON_VALUE_NULL;
        return MYLITE_OK;
    case MYLITE_JSON_SQL_VALUE_INTEGER:
        out_value->kind = JSON_VALUE_NUMBER;
        return copy_integer_text(
            sql_value->integer,
            &out_value->payload.text.text,
            &out_value->payload.text.length
        );
    case MYLITE_JSON_SQL_VALUE_BOOLEAN:
        out_value->kind = JSON_VALUE_BOOL;
        out_value->payload.boolean = sql_value->boolean;
        return MYLITE_OK;
    case MYLITE_JSON_SQL_VALUE_STRING:
        out_value->kind = JSON_VALUE_STRING;
        return copy_result_text(
            sql_value->text,
            sql_value->text_length,
            &out_value->payload.text.text,
            &out_value->payload.text.length
        );
    case MYLITE_JSON_SQL_VALUE_JSON: {
        struct json_parser parser = {
            .text = sql_value->text,
            .length = sql_value->text_length,
            .position = 0U,
            .result = {.status = MYLITE_JSON_NORMALIZE_OK, .position = 0U},
        };
        int rc = MYLITE_OK;

        if (sql_value->text == NULL) {
            return MYLITE_ERROR;
        }
        rc = parse_document(&parser, out_value);
        *out_result = parser.result;
        return rc;
    }
    }

    return MYLITE_ERROR;
}

static int json_object_key_from_sql_value(
    const struct mylite_json_sql_value *sql_value,
    char **out_key,
    size_t *out_key_length
) {
    if (sql_value == NULL || out_key == NULL || out_key_length == NULL) {
        return MYLITE_MISUSE;
    }
    *out_key = NULL;
    *out_key_length = 0U;

    switch (sql_value->kind) {
    case MYLITE_JSON_SQL_VALUE_STRING:
        return copy_result_text(sql_value->text, sql_value->text_length, out_key, out_key_length);
    case MYLITE_JSON_SQL_VALUE_INTEGER:
        return copy_integer_text(sql_value->integer, out_key, out_key_length);
    case MYLITE_JSON_SQL_VALUE_BOOLEAN:
        if (sql_value->boolean) {
            return copy_result_text("1", 1U, out_key, out_key_length);
        }
        return copy_result_text("0", 1U, out_key, out_key_length);
    case MYLITE_JSON_SQL_VALUE_NULL:
    case MYLITE_JSON_SQL_VALUE_JSON:
        break;
    }

    return MYLITE_ERROR;
}

static int copy_integer_text(int64_t value, char **out_text, size_t *out_text_length) {
    char buffer[json_sql_integer_buffer_length];
    int written = snprintf(buffer, sizeof(buffer), "%" PRId64, value);

    if (written < 0 || (size_t)written >= sizeof(buffer)) {
        return MYLITE_ERROR;
    }
    return copy_result_text(buffer, (size_t)written, out_text, out_text_length);
}

static int emit_constructed_json(
    struct json_value *value,
    char **out_text,
    size_t *out_text_length
) {
    struct json_writer writer = {0};
    int rc = emit_value(&writer, value);

    if (rc == MYLITE_OK) {
        *out_text_length = writer.length;
        *out_text = writer_take(&writer);
        if (*out_text == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    writer_deinit(&writer);
    return rc;
}

static int parse_document(struct json_parser *parser, struct json_value *out_value) {
    struct json_parse_stack stack = {0};
    struct json_parsed_value parsed = {0};
    int rc = MYLITE_OK;

    skip_whitespace(parser);
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

static bool validate_document(struct json_parser *parser) {
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
    skip_whitespace(parser);
    return parser_at_end(parser);
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

    skip_whitespace(parser);
    byte = parser_peek(parser);
    switch (byte) {
    case '{':
        if (!validate_container_depth_available(stack) || !parser_match(parser, '{')) {
            return false;
        }
        return validate_stack_push(stack, JSON_VALIDATE_OBJECT_KEY_OR_END);
    case '[':
        if (!validate_container_depth_available(stack) || !parser_match(parser, '[')) {
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
    skip_whitespace(parser);
    return parser_match(parser, '}');
}

static bool validate_object_key_required_state(
    struct json_parser *parser,
    struct json_validate_stack *stack
) {
    skip_whitespace(parser);
    if (!validate_string(parser)) {
        return false;
    }
    return validate_stack_push(stack, JSON_VALIDATE_OBJECT_COLON);
}

static bool validate_object_colon_state(
    struct json_parser *parser,
    struct json_validate_stack *stack
) {
    skip_whitespace(parser);
    if (!parser_match(parser, ':')) {
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
    skip_whitespace(parser);
    if (parser_match(parser, '}')) {
        return true;
    }
    if (!parser_match(parser, ',')) {
        return false;
    }
    return validate_stack_push(stack, JSON_VALIDATE_OBJECT_KEY_REQUIRED);
}

static bool validate_array_value_or_end_state(
    struct json_parser *parser,
    struct json_validate_stack *stack
) {
    skip_whitespace(parser);
    if (parser_match(parser, ']')) {
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
    skip_whitespace(parser);
    if (parser_match(parser, ']')) {
        return true;
    }
    if (!parser_match(parser, ',')) {
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
    if (!parser_match(parser, '"')) {
        return false;
    }
    while (!parser_at_end(parser)) {
        unsigned char byte = (unsigned char)parser_peek(parser);

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

    if (parser_at_end(parser)) {
        return false;
    }
    byte = parser_peek(parser);
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
            if (parser_at_end(parser) || !is_hex_digit(parser_peek(parser))) {
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
    if (parser_match(parser, '-')) {
        if (parser_at_end(parser)) {
            return false;
        }
    }
    if (!validate_integer_digits(parser)) {
        return false;
    }
    if (parser_peek(parser) == '.' && !validate_fraction_digits(parser)) {
        return false;
    }
    if ((parser_peek(parser) == 'e' || parser_peek(parser) == 'E') &&
        !validate_exponent_digits(parser)) {
        return false;
    }
    return true;
}

static bool validate_integer_digits(struct json_parser *parser) {
    char byte = parser_peek(parser);

    if (byte == '0') {
        ++parser->position;
        return true;
    }
    if (byte < '1' || byte > '9') {
        return false;
    }
    do {
        ++parser->position;
        byte = parser_peek(parser);
    } while (is_decimal_digit(byte));
    return true;
}

static bool validate_fraction_digits(struct json_parser *parser) {
    if (!parser_match(parser, '.')) {
        return false;
    }
    if (!is_decimal_digit(parser_peek(parser))) {
        return false;
    }
    do {
        ++parser->position;
    } while (is_decimal_digit(parser_peek(parser)));
    return true;
}

static bool validate_exponent_digits(struct json_parser *parser) {
    if (parser_peek(parser) != 'e' && parser_peek(parser) != 'E') {
        return false;
    }
    ++parser->position;
    if (parser_peek(parser) == '+' || parser_peek(parser) == '-') {
        ++parser->position;
    }
    if (!is_decimal_digit(parser_peek(parser))) {
        return false;
    }
    do {
        ++parser->position;
    } while (is_decimal_digit(parser_peek(parser)));
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

static int parse_next_value(struct json_parser *parser, struct json_parsed_value *out_value) {
    char byte = parser_peek(parser);

    *out_value = (struct json_parsed_value){0};
    if (byte == '{') {
        parser_match(parser, '{');
        out_value->value.kind = JSON_VALUE_OBJECT;
        skip_whitespace(parser);
        if (!parser_match(parser, '}')) {
            out_value->opens_container = true;
        }
        return MYLITE_OK;
    }
    if (byte == '[') {
        parser_match(parser, '[');
        out_value->value.kind = JSON_VALUE_ARRAY;
        skip_whitespace(parser);
        if (!parser_match(parser, ']')) {
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

    return parser_invalid(parser, parser->position);
}

static int parse_next_object_member(struct json_parser *parser, struct json_parse_stack *stack) {
    struct json_parse_frame *frame = parse_stack_top(stack);
    struct json_object *object = &frame->container->payload.object;
    struct json_parsed_value parsed = {0};
    struct json_value *stored_value = NULL;
    char *key = NULL;
    size_t key_length = 0U;
    int rc = MYLITE_OK;

    if (parser_peek(parser) != '"') {
        return parser_invalid(parser, parser->position);
    }
    rc = parse_string(parser, &key, &key_length);
    if (rc == MYLITE_OK) {
        skip_whitespace(parser);
        if (!parser_match(parser, ':')) {
            rc = parser_invalid(parser, parser->position);
        }
    }
    if (rc == MYLITE_OK) {
        skip_whitespace(parser);
        rc = parse_next_value(parser, &parsed);
    }
    if (rc == MYLITE_OK) {
        rc = object_append_member(object, key, key_length, &parsed.value, &stored_value);
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
        value_deinit(&parsed.value);
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
        rc = array_append_value(array, &parsed.value, &stored_value);
    }
    if (rc == MYLITE_OK) {
        if (parsed.opens_container) {
            rc = parse_stack_push(parser, stack, stored_value);
        } else {
            rc = finish_completed_value(parser, stack);
        }
    }
    if (rc != MYLITE_OK) {
        value_deinit(&parsed.value);
    }
    return rc;
}

static int finish_completed_value(struct json_parser *parser, struct json_parse_stack *stack) {
    while (true) {
        struct json_parse_frame *frame = NULL;

        skip_whitespace(parser);
        frame = parse_stack_top(stack);
        if (frame == NULL) {
            if (parser_at_end(parser)) {
                return MYLITE_OK;
            }
            return parser_invalid(parser, parser->position);
        }
        if (frame->container->kind == JSON_VALUE_OBJECT && parser_match(parser, '}')) {
            close_completed_container(stack);
            continue;
        }
        if (frame->container->kind == JSON_VALUE_ARRAY && parser_match(parser, ']')) {
            close_completed_container(stack);
            continue;
        }
        if (!parser_match(parser, ',')) {
            return parser_invalid(parser, parser->position);
        }
        skip_whitespace(parser);
        return MYLITE_OK;
    }
}

static void close_completed_container(struct json_parse_stack *stack) {
    struct json_parse_frame *frame = parse_stack_top(stack);

    if (frame == NULL) {
        return;
    }
    if (frame->container->kind == JSON_VALUE_OBJECT) {
        sort_object_members_by_mysql_display_order(&frame->container->payload.object);
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
        return parser_unsupported(parser, parser->position);
    }
    stack->frames[stack->count] = (struct json_parse_frame){.container = container};
    ++stack->count;
    return MYLITE_OK;
}

static int object_append_member(
    struct json_object *object,
    char *key,
    size_t key_length,
    struct json_value *value,
    struct json_value **out_stored_value
) {
    struct json_value *owned_value = NULL;

    for (size_t index = 0U; index < object->count; ++index) {
        struct json_member *member = &object->members[index];

        if (member->key_length == key_length && memcmp(member->key, key, key_length) == 0) {
            value_deinit(member->value);
            *member->value = *value;
            *value = (struct json_value){0};
            *out_stored_value = member->value;
            free(key);
            return MYLITE_OK;
        }
    }

    int rc = object_reserve_members(object, object->count + 1U);
    if (rc != MYLITE_OK) {
        return rc;
    }
    owned_value = malloc(sizeof(*owned_value));
    if (owned_value == NULL) {
        return MYLITE_NOMEM;
    }
    *owned_value = *value;
    object->members[object->count] = (struct json_member){
        .key = key,
        .key_length = key_length,
        .value = owned_value,
    };
    *value = (struct json_value){0};
    *out_stored_value = owned_value;
    ++object->count;
    return MYLITE_OK;
}

static int object_reserve_members(struct json_object *object, size_t required_capacity) {
    struct json_member *members = NULL;
    size_t capacity = object->capacity;

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
    if (capacity > SIZE_MAX / sizeof(*members)) {
        return MYLITE_NOMEM;
    }

    members = realloc(object->members, capacity * sizeof(*members));
    if (members == NULL) {
        return MYLITE_NOMEM;
    }
    object->members = members;
    object->capacity = capacity;
    return MYLITE_OK;
}

static void sort_object_members_by_mysql_display_order(struct json_object *object) {
    for (size_t index = 1U; index < object->count; ++index) {
        struct json_member member = object->members[index];
        size_t insert = index;

        while (insert > 0U && object_member_is_after(&object->members[insert - 1U], &member)) {
            object->members[insert] = object->members[insert - 1U];
            --insert;
        }
        object->members[insert] = member;
    }
}

static bool object_member_is_after(
    const struct json_member *left,
    const struct json_member *right
) {
    return compare_object_member_keys(left, right) > 0;
}

static int compare_object_member_keys(
    const struct json_member *left,
    const struct json_member *right
) {
    int cmp = 0;

    if (left->key_length != right->key_length) {
        return left->key_length < right->key_length ? -1 : 1;
    }
    cmp = memcmp(left->key, right->key, left->key_length);
    if (cmp < 0) {
        return -1;
    }
    if (cmp > 0) {
        return 1;
    }
    return 0;
}

static int array_append_value(
    struct json_array *array,
    struct json_value *value,
    struct json_value **out_stored_value
) {
    int rc = array_reserve_values(array, array->count + 1U);

    if (rc != MYLITE_OK) {
        return rc;
    }
    array->values[array->count] = *value;
    *value = (struct json_value){0};
    *out_stored_value = &array->values[array->count];
    ++array->count;
    return MYLITE_OK;
}

static int array_reserve_values(struct json_array *array, size_t required_capacity) {
    struct json_value *values = NULL;
    size_t capacity = array->capacity;

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
    if (capacity > SIZE_MAX / sizeof(*values)) {
        return MYLITE_NOMEM;
    }

    values = realloc(array->values, capacity * sizeof(*values));
    if (values == NULL) {
        return MYLITE_NOMEM;
    }
    array->values = values;
    array->capacity = capacity;
    return MYLITE_OK;
}

static int parse_string_value(struct json_parser *parser, struct json_value *out_value) {
    char *text = NULL;
    size_t text_length = 0U;
    int rc = parse_string(parser, &text, &text_length);

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

static int parse_string(struct json_parser *parser, char **out_text, size_t *out_text_length) {
    struct json_writer string = {0};
    int rc = MYLITE_OK;

    *out_text = NULL;
    *out_text_length = 0U;
    if (!parser_match(parser, '"')) {
        return parser_invalid(parser, parser->position);
    }

    while (rc == MYLITE_OK && !parser_at_end(parser)) {
        size_t position = parser->position;
        unsigned char byte = (unsigned char)parser->text[parser->position++];

        if (byte == '"') {
            if (string.text == NULL) {
                rc = writer_append_text(&string, "", 0U);
            }
            if (rc == MYLITE_OK) {
                *out_text_length = string.length;
                *out_text = writer_take(&string);
                if (*out_text == NULL) {
                    rc = MYLITE_NOMEM;
                }
            }
            writer_deinit(&string);
            return rc;
        }
        if (byte == '\\') {
            rc = append_string_escape(parser, &string, position);
        } else {
            rc = append_string_byte(parser, &string, byte);
        }
    }

    writer_deinit(&string);
    return rc == MYLITE_OK ? parser_invalid(parser, parser->position) : rc;
}

static int append_string_byte(
    struct json_parser *parser,
    struct json_writer *string,
    unsigned char byte
) {
    if (byte == '\0') {
        return parser_unsupported(parser, parser->position - 1U);
    }
    if (byte < json_control_byte_limit) {
        return parser_invalid(parser, parser->position - 1U);
    }
    return writer_append_char(string, (char)byte);
}

static int append_string_escape(
    struct json_parser *parser,
    struct json_writer *string,
    size_t escape_position
) {
    unsigned char escaped = 0U;
    unsigned int codepoint = 0U;

    if (parser_at_end(parser)) {
        return parser_invalid(parser, escape_position);
    }
    escaped = (unsigned char)parser->text[parser->position++];
    switch (escaped) {
    case '"':
    case '\\':
    case '/':
        return writer_append_char(string, (char)escaped);
    case 'b':
        return writer_append_char(string, '\b');
    case 'f':
        return writer_append_char(string, '\f');
    case 'n':
        return writer_append_char(string, '\n');
    case 'r':
        return writer_append_char(string, '\r');
    case 't':
        return writer_append_char(string, '\t');
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
        return parser_invalid(parser, parser->position - 1U);
    }
}

static int parse_hex_digit(struct json_parser *parser, size_t position, unsigned int *out_digit) {
    unsigned char byte = 0U;

    if (position >= parser->length) {
        return parser_invalid(parser, position);
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
    return parser_invalid(parser, position);
}

static int append_ascii_codepoint(
    struct json_parser *parser,
    size_t position,
    struct json_writer *string,
    unsigned int codepoint
) {
    if (codepoint == 0U || codepoint > json_ascii_byte_limit) {
        return parser_unsupported(parser, position);
    }
    return writer_append_char(string, (char)codepoint);
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
        return parser_unsupported(parser, start);
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
    out_part->is_negative = parser_match(parser, '-');
    out_part->start = parser->position;
    if (parser_at_end(parser)) {
        return parser_invalid(parser, start);
    }
    if (parser_peek(parser) == '0') {
        ++parser->position;
        if (!parser_at_end(parser) && is_decimal_digit(parser_peek(parser))) {
            return parser_invalid(parser, parser->position);
        }
    } else if (parser_peek(parser) >= '1' && parser_peek(parser) <= '9') {
        while (!parser_at_end(parser) && is_decimal_digit(parser_peek(parser))) {
            ++parser->position;
        }
    } else {
        return parser_invalid(parser, parser->position);
    }
    out_part->end = parser->position;
    return MYLITE_OK;
}

static int parse_number_fraction_part(struct json_parser *parser, bool *out_has_fraction) {
    *out_has_fraction = false;
    if (parser_at_end(parser) || parser_peek(parser) != '.') {
        return MYLITE_OK;
    }
    *out_has_fraction = true;
    ++parser->position;
    if (parser_at_end(parser) || !is_decimal_digit(parser_peek(parser))) {
        return parser_invalid(parser, parser->position);
    }
    while (!parser_at_end(parser) && is_decimal_digit(parser_peek(parser))) {
        ++parser->position;
    }
    return MYLITE_OK;
}

static int parse_number_exponent_part(struct json_parser *parser, bool *out_has_exponent) {
    *out_has_exponent = false;
    if (parser_at_end(parser) || (parser_peek(parser) != 'e' && parser_peek(parser) != 'E')) {
        return MYLITE_OK;
    }
    *out_has_exponent = true;
    ++parser->position;
    if (!parser_at_end(parser) && (parser_peek(parser) == '+' || parser_peek(parser) == '-')) {
        ++parser->position;
    }
    if (parser_at_end(parser) || !is_decimal_digit(parser_peek(parser))) {
        return parser_invalid(parser, parser->position);
    }
    while (!parser_at_end(parser) && is_decimal_digit(parser_peek(parser))) {
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
        return parser_unsupported(parser, start);
    }
    if (is_negative && digit_count == 1U && digits[0] == '0') {
        negative_zero = true;
    }

    return copy_number_text(
        parser->text + start,
        parser->position - start,
        negative_zero,
        out_value
    );
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
        return parser_invalid(parser, parser->position);
    }
    parser->position += length;
    out_value->kind = kind;
    if (kind == JSON_VALUE_BOOL) {
        out_value->payload.boolean = boolean;
    }
    return MYLITE_OK;
}

static int extract_path_value(
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
    if (!parser_match(parser, '$')) {
        return parser_invalid(parser, parser->position);
    }

    while (rc == MYLITE_OK && !parser_at_end(parser)) {
        char byte = parser_peek(parser);

        if (byte == '.') {
            rc = parse_path_member_leg(parser, out_value, out_matched);
        } else if (byte == '[') {
            rc = parse_path_array_leg(parser, out_value, out_matched);
        } else if (byte == '*') {
            rc = parser_unsupported(parser, parser->position);
        } else {
            rc = parser_invalid(parser, parser->position);
        }
    }

    return rc;
}

static bool json_value_contains(
    const struct json_value *target,
    const struct json_value *candidate
) {
    struct json_contains_stack stack = {0};

    if (!json_contains_stack_push(&stack, target, candidate)) {
        return false;
    }

    while (!stack.done) {
        struct json_contains_frame *frame = &stack.frames[stack.count - 1U];
        bool progressed = false;

        switch (frame->kind) {
        case JSON_CONTAINS_CHECK:
            progressed = json_contains_process_check_frame(&stack, frame);
            break;
        case JSON_CONTAINS_ARRAY_VALUE:
            progressed = json_contains_process_array_value_frame(&stack, frame);
            break;
        case JSON_CONTAINS_ARRAY_CANDIDATE:
            progressed = json_contains_process_array_candidate_frame(&stack, frame);
            break;
        case JSON_CONTAINS_OBJECT_CANDIDATE:
            progressed = json_contains_process_object_candidate_frame(&stack, frame);
            break;
        }
        if (!progressed) {
            return false;
        }
    }
    return stack.result;
}

static bool json_contains_stack_push(
    struct json_contains_stack *stack,
    const struct json_value *target,
    const struct json_value *candidate
) {
    if (stack == NULL || stack->count >= json_contains_stack_capacity) {
        return false;
    }
    stack->frames[stack->count] = (struct json_contains_frame){
        .kind = JSON_CONTAINS_CHECK,
        .target = target,
        .candidate = candidate,
        .target_index = 0U,
        .candidate_index = 0U,
        .waiting_child = false,
        .child_result = false,
    };
    ++stack->count;
    return true;
}

static bool json_contains_stack_complete(struct json_contains_stack *stack, bool result) {
    if (stack == NULL || stack->count == 0U) {
        return false;
    }

    --stack->count;
    if (stack->count == 0U) {
        stack->done = true;
        stack->result = result;
        return true;
    }

    stack->frames[stack->count - 1U].child_result = result;
    return true;
}

static bool json_contains_process_check_frame(
    struct json_contains_stack *stack,
    struct json_contains_frame *frame
) {
    if (frame == NULL || frame->target == NULL || frame->candidate == NULL) {
        return json_contains_stack_complete(stack, false);
    }
    if (frame->target->kind == JSON_VALUE_ARRAY) {
        frame->kind = frame->candidate->kind == JSON_VALUE_ARRAY ? JSON_CONTAINS_ARRAY_CANDIDATE
                                                                 : JSON_CONTAINS_ARRAY_VALUE;
        frame->target_index = 0U;
        frame->candidate_index = 0U;
        frame->waiting_child = false;
        frame->child_result = false;
        return true;
    }
    if (frame->target->kind == JSON_VALUE_OBJECT && frame->candidate->kind == JSON_VALUE_OBJECT) {
        frame->kind = JSON_CONTAINS_OBJECT_CANDIDATE;
        frame->target_index = 0U;
        frame->candidate_index = 0U;
        frame->waiting_child = false;
        frame->child_result = false;
        return true;
    }
    if (frame->target->kind == frame->candidate->kind) {
        return json_contains_stack_complete(
            stack,
            json_scalar_values_equal(frame->target, frame->candidate)
        );
    }
    return json_contains_stack_complete(stack, false);
}

static bool json_contains_process_array_value_frame(
    struct json_contains_stack *stack,
    struct json_contains_frame *frame
) {
    if (frame == NULL || frame->target == NULL || frame->candidate == NULL ||
        frame->target->kind != JSON_VALUE_ARRAY) {
        return json_contains_stack_complete(stack, false);
    }
    if (frame->waiting_child) {
        if (frame->child_result) {
            return json_contains_stack_complete(stack, true);
        }
        ++frame->target_index;
        frame->waiting_child = false;
    }
    if (frame->target_index >= frame->target->payload.array.count) {
        return json_contains_stack_complete(stack, false);
    }

    frame->waiting_child = true;
    return json_contains_stack_push(
        stack,
        &frame->target->payload.array.values[frame->target_index],
        frame->candidate
    );
}

static bool json_contains_process_array_candidate_frame(
    struct json_contains_stack *stack,
    struct json_contains_frame *frame
) {
    if (frame == NULL || frame->target == NULL || frame->candidate == NULL ||
        frame->target->kind != JSON_VALUE_ARRAY || frame->candidate->kind != JSON_VALUE_ARRAY) {
        return json_contains_stack_complete(stack, false);
    }
    if (frame->candidate_index >= frame->candidate->payload.array.count) {
        return json_contains_stack_complete(stack, true);
    }
    if (frame->waiting_child) {
        if (frame->child_result) {
            ++frame->candidate_index;
            frame->target_index = 0U;
        } else {
            ++frame->target_index;
        }
        frame->waiting_child = false;
    }
    if (frame->candidate_index >= frame->candidate->payload.array.count) {
        return json_contains_stack_complete(stack, true);
    }
    if (frame->target_index >= frame->target->payload.array.count) {
        return json_contains_stack_complete(stack, false);
    }

    frame->waiting_child = true;
    return json_contains_stack_push(
        stack,
        &frame->target->payload.array.values[frame->target_index],
        &frame->candidate->payload.array.values[frame->candidate_index]
    );
}

static bool json_contains_process_object_candidate_frame(
    struct json_contains_stack *stack,
    struct json_contains_frame *frame
) {
    const struct json_member *candidate_member = NULL;
    const struct json_value *target_member = NULL;

    if (frame == NULL || frame->target == NULL || frame->candidate == NULL ||
        frame->target->kind != JSON_VALUE_OBJECT || frame->candidate->kind != JSON_VALUE_OBJECT) {
        return json_contains_stack_complete(stack, false);
    }
    if (frame->candidate_index >= frame->candidate->payload.object.count) {
        return json_contains_stack_complete(stack, true);
    }
    if (frame->waiting_child) {
        if (!frame->child_result) {
            return json_contains_stack_complete(stack, false);
        }
        ++frame->candidate_index;
        frame->waiting_child = false;
    }
    if (frame->candidate_index >= frame->candidate->payload.object.count) {
        return json_contains_stack_complete(stack, true);
    }

    candidate_member = &frame->candidate->payload.object.members[frame->candidate_index];
    target_member =
        object_member_value(frame->target, candidate_member->key, candidate_member->key_length);
    if (target_member == NULL) {
        return json_contains_stack_complete(stack, false);
    }

    frame->waiting_child = true;
    return json_contains_stack_push(stack, target_member, candidate_member->value);
}

static bool json_scalar_values_equal(
    const struct json_value *target,
    const struct json_value *candidate
) {
    if (target == NULL || candidate == NULL || target->kind != candidate->kind) {
        return false;
    }

    switch (target->kind) {
    case JSON_VALUE_NULL:
        return true;
    case JSON_VALUE_BOOL:
        return target->payload.boolean == candidate->payload.boolean;
    case JSON_VALUE_NUMBER:
    case JSON_VALUE_STRING:
        return (target->payload.text.length == candidate->payload.text.length &&
                memcmp(
                    target->payload.text.text,
                    candidate->payload.text.text,
                    target->payload.text.length
                ) == 0) != 0;
    case JSON_VALUE_ARRAY:
    case JSON_VALUE_OBJECT:
        break;
    }
    return false;
}

static int parse_path_member_leg(
    struct json_parser *parser,
    const struct json_value **inout_value,
    bool *inout_matched
) {
    if (!parser_match(parser, '.')) {
        return parser_invalid(parser, parser->position);
    }
    if (parser_peek(parser) == '"') {
        return parse_path_quoted_member_leg(parser, inout_value, inout_matched);
    }
    if (!path_identifier_start_byte(parser_peek(parser))) {
        if (parser_at_end(parser)) {
            return parser_invalid(parser, parser->position);
        }
        if (parser_peek(parser) == '*') {
            return parser_unsupported(parser, parser->position);
        }
        return parser_invalid(parser, parser->position);
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
    int rc = parse_string(parser, &member, &member_length);

    if (rc == MYLITE_OK && !path_text_is_ascii(member, member_length)) {
        rc = parser_unsupported(parser, start);
    }
    if (rc == MYLITE_OK && *inout_matched) {
        const struct json_value *next = object_member_value(*inout_value, member, member_length);

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
    while (path_identifier_byte(parser_peek(parser))) {
        ++parser->position;
    }
    if (*inout_matched) {
        const char *member = &parser->text[start];
        size_t member_length = parser->position - start;
        const struct json_value *next = object_member_value(*inout_value, member, member_length);

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

    if (!parser_match(parser, '[')) {
        return parser_invalid(parser, parser->position);
    }
    rc = parse_path_array_index(parser, &index);
    if (rc == MYLITE_OK && !parser_match(parser, ']')) {
        rc = parser_invalid(parser, parser->position);
    }
    if (rc == MYLITE_OK && *inout_matched) {
        const struct json_value *next = array_index_value(*inout_value, index);

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

    if (parser_peek(parser) == '*' || parser_peek(parser) == 'l' || parser_peek(parser) == 'L') {
        return parser_unsupported(parser, parser->position);
    }
    if (!is_decimal_digit(parser_peek(parser))) {
        return parser_invalid(parser, parser->position);
    }
    if (parser_peek(parser) == '0') {
        ++parser->position;
        if (is_decimal_digit(parser_peek(parser))) {
            return parser_invalid(parser, parser->position);
        }
        *out_index = 0U;
        return MYLITE_OK;
    }

    while (is_decimal_digit(parser_peek(parser))) {
        unsigned int digit = (unsigned int)(parser_peek(parser) - '0');

        if (value > (SIZE_MAX - digit) / json_decimal_base) {
            return parser_unsupported(parser, start);
        }
        value = (value * json_decimal_base) + digit;
        ++parser->position;
    }
    *out_index = value;
    return MYLITE_OK;
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

static const struct json_value *object_member_value(
    const struct json_value *value,
    const char *member,
    size_t member_length
) {
    if (value == NULL || value->kind != JSON_VALUE_OBJECT || member == NULL) {
        return NULL;
    }
    for (size_t index = 0U; index < value->payload.object.count; ++index) {
        const struct json_member *candidate = &value->payload.object.members[index];

        if (candidate->key_length == member_length &&
            memcmp(candidate->key, member, member_length) == 0) {
            return candidate->value;
        }
    }
    return NULL;
}

static const struct json_value *array_index_value(const struct json_value *value, size_t index) {
    if (value == NULL || value->kind != JSON_VALUE_ARRAY || index >= value->payload.array.count) {
        return NULL;
    }
    return &value->payload.array.values[index];
}

static int emit_value(struct json_writer *writer, const struct json_value *value) {
    struct json_emit_stack stack = {0};
    int rc = emit_value_start(writer, value, &stack);

    while (rc == MYLITE_OK && stack.count > 0U) {
        struct json_emit_frame *frame = &stack.frames[stack.count - 1U];
        const struct json_value *child = NULL;

        if (frame->container->kind == JSON_VALUE_ARRAY) {
            rc = emit_array_next_value(writer, &stack, &child);
        } else {
            rc = emit_object_next_member(writer, &stack, &child);
        }
        if (rc == MYLITE_OK && child != NULL) {
            rc = emit_value_start(writer, child, &stack);
        }
    }
    return rc;
}

static int emit_value_start(
    struct json_writer *writer,
    const struct json_value *value,
    struct json_emit_stack *stack
) {
    switch (value->kind) {
    case JSON_VALUE_NULL:
        return writer_append_text(writer, "null", json_null_literal_length);
    case JSON_VALUE_BOOL:
        return emit_bool_value(writer, value->payload.boolean);
    case JSON_VALUE_NUMBER:
        return writer_append_text(writer, value->payload.text.text, value->payload.text.length);
    case JSON_VALUE_STRING:
        return emit_string(writer, value->payload.text.text, value->payload.text.length);
    case JSON_VALUE_ARRAY:
        if (writer_append_char(writer, '[') != MYLITE_OK) {
            return MYLITE_NOMEM;
        }
        if (value->payload.array.count == 0U) {
            return writer_append_char(writer, ']');
        }
        return emit_stack_push(stack, value);
    case JSON_VALUE_OBJECT:
        if (writer_append_char(writer, '{') != MYLITE_OK) {
            return MYLITE_NOMEM;
        }
        if (value->payload.object.count == 0U) {
            return writer_append_char(writer, '}');
        }
        return emit_stack_push(stack, value);
    }

    return MYLITE_ERROR;
}

static int emit_array_next_value(
    struct json_writer *writer,
    struct json_emit_stack *stack,
    const struct json_value **out_child
) {
    struct json_emit_frame *frame = &stack->frames[stack->count - 1U];

    *out_child = NULL;
    if (frame->index >= frame->container->payload.array.count) {
        --stack->count;
        return writer_append_char(writer, ']');
    }
    if (frame->index > 0U &&
        writer_append_text(writer, ", ", json_member_separator_length) != MYLITE_OK) {
        return MYLITE_NOMEM;
    }
    *out_child = &frame->container->payload.array.values[frame->index];
    ++frame->index;
    return MYLITE_OK;
}

static int emit_object_next_member(
    struct json_writer *writer,
    struct json_emit_stack *stack,
    const struct json_value **out_child
) {
    struct json_emit_frame *frame = &stack->frames[stack->count - 1U];
    const struct json_member *member = NULL;
    int rc = MYLITE_OK;

    *out_child = NULL;
    if (frame->index >= frame->container->payload.object.count) {
        --stack->count;
        return writer_append_char(writer, '}');
    }
    if (frame->index > 0U) {
        rc = writer_append_text(writer, ", ", json_member_separator_length);
    }
    member = &frame->container->payload.object.members[frame->index];
    if (rc == MYLITE_OK) {
        rc = emit_string(writer, member->key, member->key_length);
    }
    if (rc == MYLITE_OK) {
        rc = writer_append_text(writer, ": ", json_member_separator_length);
    }
    if (rc == MYLITE_OK) {
        *out_child = member->value;
        ++frame->index;
    }
    return rc;
}

static int emit_bool_value(struct json_writer *writer, bool boolean) {
    if (boolean) {
        return writer_append_text(writer, "true", json_true_literal_length);
    }
    return writer_append_text(writer, "false", json_false_literal_length);
}

static int emit_stack_push(struct json_emit_stack *stack, const struct json_value *container) {
    if (stack->count >= json_max_nesting_depth) {
        return MYLITE_ERROR;
    }
    stack->frames[stack->count] = (struct json_emit_frame){.container = container, .index = 0U};
    ++stack->count;
    return MYLITE_OK;
}

static int emit_string(struct json_writer *writer, const char *text, size_t text_length) {
    int rc = writer_append_char(writer, '"');

    for (size_t index = 0U; rc == MYLITE_OK && index < text_length; ++index) {
        unsigned char byte = (unsigned char)text[index];

        if (byte == '"' || byte == '\\' || byte < json_control_byte_limit) {
            rc = writer_append_json_escape(writer, byte);
        } else {
            rc = writer_append_char(writer, (char)byte);
        }
    }
    if (rc == MYLITE_OK) {
        rc = writer_append_char(writer, '"');
    }
    return rc;
}

static int writer_append_json_escape(struct json_writer *writer, unsigned char byte) {
    switch (byte) {
    case '"':
        return writer_append_text(writer, "\\\"", 2U);
    case '\\':
        return writer_append_text(writer, "\\\\", 2U);
    case '\b':
        return writer_append_text(writer, "\\b", 2U);
    case '\f':
        return writer_append_text(writer, "\\f", 2U);
    case '\n':
        return writer_append_text(writer, "\\n", 2U);
    case '\r':
        return writer_append_text(writer, "\\r", 2U);
    case '\t':
        return writer_append_text(writer, "\\t", 2U);
    default:
        if (writer_append_text(writer, "\\u00", json_unicode_escape_prefix_length) != MYLITE_OK) {
            return MYLITE_NOMEM;
        }
        if (writer_append_ascii_hex_digit(writer, (unsigned char)(byte >> json_hex_nibble_bits)) !=
            MYLITE_OK) {
            return MYLITE_NOMEM;
        }
        return writer_append_ascii_hex_digit(writer, (unsigned char)(byte & json_hex_low_nibble));
    }
}

static int writer_append_ascii_hex_digit(struct json_writer *writer, unsigned char value) {
    static const char digits[] = "0123456789abcdef";

    return writer_append_char(writer, digits[value & json_hex_low_nibble]);
}

static int writer_append_char(struct json_writer *writer, char byte) {
    return writer_append_text(writer, &byte, 1U);
}

static int writer_append_text(struct json_writer *writer, const char *text, size_t text_length) {
    int rc = MYLITE_OK;

    if (writer->length > SIZE_MAX - 1U || text_length > SIZE_MAX - writer->length - 1U) {
        return MYLITE_NOMEM;
    }
    rc = writer_reserve(writer, writer->length + text_length + 1U);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (text_length != 0U) {
        memcpy(writer->text + writer->length, text, text_length);
    }
    writer->length += text_length;
    writer->text[writer->length] = '\0';
    return MYLITE_OK;
}

static int writer_reserve(struct json_writer *writer, size_t required_capacity) {
    char *text = NULL;
    size_t capacity = writer->capacity;

    if (required_capacity <= capacity) {
        return MYLITE_OK;
    }
    if (capacity == 0U) {
        capacity = json_writer_initial_capacity;
    }
    while (capacity < required_capacity) {
        if (capacity > SIZE_MAX / 2U) {
            return MYLITE_NOMEM;
        }
        capacity *= 2U;
    }

    text = realloc(writer->text, capacity);
    if (text == NULL) {
        return MYLITE_NOMEM;
    }
    if (writer->capacity == 0U) {
        text[0] = '\0';
    }
    writer->text = text;
    writer->capacity = capacity;
    return MYLITE_OK;
}

static char *writer_take(struct json_writer *writer) {
    char *text = writer->text;

    writer->text = NULL;
    writer->length = 0U;
    writer->capacity = 0U;
    return text;
}

static void writer_deinit(struct json_writer *writer) {
    free(writer->text);
    *writer = (struct json_writer){0};
}

static int copy_result_text(
    const char *text,
    size_t text_length,
    char **out_text,
    size_t *out_text_length
) {
    char *copy = NULL;

    if (out_text == NULL || out_text_length == NULL || text == NULL) {
        return MYLITE_MISUSE;
    }
    if (text_length == SIZE_MAX) {
        return MYLITE_NOMEM;
    }
    copy = malloc(text_length + 1U);
    if (copy == NULL) {
        return MYLITE_NOMEM;
    }
    if (text_length != 0U) {
        memcpy(copy, text, text_length);
    }
    copy[text_length] = '\0';
    *out_text = copy;
    *out_text_length = text_length;
    return MYLITE_OK;
}

static void value_deinit(struct json_value *value) {
    struct json_deinit_stack stack = {0};

    if (value == NULL) {
        return;
    }
    if (!deinit_stack_push(&stack, value, false)) {
        return;
    }
    while (stack.count > 0U) {
        struct json_deinit_frame *frame = &stack.frames[stack.count - 1U];
        struct json_value *current = frame->value;

        switch (current->kind) {
        case JSON_VALUE_STRING:
        case JSON_VALUE_NUMBER:
            free(current->payload.text.text);
            *current = (struct json_value){0};
            deinit_stack_pop(&stack);
            break;
        case JSON_VALUE_ARRAY:
            if (frame->index < current->payload.array.count) {
                deinit_stack_push(&stack, &current->payload.array.values[frame->index], false);
                ++frame->index;
            } else {
                free(current->payload.array.values);
                *current = (struct json_value){0};
                deinit_stack_pop(&stack);
            }
            break;
        case JSON_VALUE_OBJECT:
            if (frame->index < current->payload.object.count) {
                struct json_member *member = &current->payload.object.members[frame->index];

                free(member->key);
                deinit_stack_push(&stack, member->value, true);
                ++frame->index;
            } else {
                free(current->payload.object.members);
                *current = (struct json_value){0};
                deinit_stack_pop(&stack);
            }
            break;
        case JSON_VALUE_NULL:
        case JSON_VALUE_BOOL:
            *current = (struct json_value){0};
            deinit_stack_pop(&stack);
            break;
        }
    }
}

static bool deinit_stack_push(
    struct json_deinit_stack *stack,
    struct json_value *value,
    bool free_value
) {
    if (stack->count >= json_max_nesting_depth + 1U) {
        return false;
    }
    stack->frames[stack->count] = (struct json_deinit_frame){
        .value = value,
        .index = 0U,
        .free_value = free_value,
    };
    ++stack->count;
    return true;
}

static void deinit_stack_pop(struct json_deinit_stack *stack) {
    struct json_deinit_frame frame = stack->frames[stack->count - 1U];

    --stack->count;
    if (frame.free_value) {
        free(frame.value);
    }
}

static void skip_whitespace(struct json_parser *parser) {
    while (!parser_at_end(parser)) {
        char byte = parser_peek(parser);

        if (byte != ' ' && byte != '\t' && byte != '\n' && byte != '\r') {
            return;
        }
        ++parser->position;
    }
}

static bool parser_at_end(const struct json_parser *parser) {
    return parser->position >= parser->length;
}

static char parser_peek(const struct json_parser *parser) {
    if (parser_at_end(parser)) {
        return '\0';
    }
    return parser->text[parser->position];
}

static bool parser_match(struct json_parser *parser, char expected) {
    if (parser_at_end(parser) || parser->text[parser->position] != expected) {
        return false;
    }
    ++parser->position;
    return true;
}

static bool is_decimal_digit(char byte) {
    if (byte < '0') {
        return false;
    }
    return byte <= '9';
}

static bool is_hex_digit(char byte) {
    if (byte >= '0' && byte <= '9') {
        return true;
    }
    if (byte >= 'A' && byte <= 'F') {
        return true;
    }
    return (byte >= 'a' && byte <= 'f') != 0;
}

static int parser_invalid(struct json_parser *parser, size_t position) {
    parser->result = (struct mylite_json_normalize_result){
        .status = MYLITE_JSON_NORMALIZE_INVALID,
        .position = position,
    };
    return MYLITE_ERROR;
}

static int parser_unsupported(struct json_parser *parser, size_t position) {
    parser->result = (struct mylite_json_normalize_result){
        .status = MYLITE_JSON_NORMALIZE_UNSUPPORTED,
        .position = position,
    };
    return MYLITE_ERROR;
}
