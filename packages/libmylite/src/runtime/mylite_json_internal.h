#ifndef MYLITE_RUNTIME_MYLITE_JSON_INTERNAL_H
#define MYLITE_RUNTIME_MYLITE_JSON_INTERNAL_H

#include <mylite/mylite.h>

#include "mylite_json.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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
    json_pretty_indent_width = 2,
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

enum json_set_path_leg_kind {
    JSON_SET_PATH_MEMBER,
    JSON_SET_PATH_ARRAY,
};

struct json_set_path_leg {
    enum json_set_path_leg_kind kind;
    const char *member;
    size_t member_length;
    size_t index;
};

struct json_set_path {
    struct json_set_path_leg *legs;
    size_t count;
    size_t capacity;
};

enum json_mutation_mode {
    JSON_MUTATION_SET,
    JSON_MUTATION_REPLACE,
    JSON_MUTATION_INSERT,
    JSON_MUTATION_ARRAY_APPEND,
    JSON_MUTATION_ARRAY_INSERT,
    JSON_MUTATION_REMOVE,
};

const char *mylite_json_internal_value_type_name(const struct json_value *value);
int mylite_json_internal_value_shallow_length(const struct json_value *value, int64_t *out_length);
int mylite_json_internal_value_depth(const struct json_value *value, int64_t *out_depth);
int mylite_json_internal_object_append_member(
    struct json_object *object,
    char *key,
    size_t key_length,
    struct json_value *value,
    struct json_value **out_stored_value
);
int mylite_json_internal_array_append_value(
    struct json_array *array,
    struct json_value *value,
    struct json_value **out_stored_value
);
int mylite_json_internal_array_reserve_values(struct json_array *array, size_t required_capacity);
void mylite_json_internal_sort_object_members_by_mysql_display_order(struct json_object *object);
const struct json_value *mylite_json_internal_object_member_value(
    const struct json_value *value,
    const char *member,
    size_t member_length
);
struct json_value *mylite_json_internal_object_member_value_mutable(
    struct json_value *value,
    const char *member,
    size_t member_length
);
const struct json_value *mylite_json_internal_array_index_value(
    const struct json_value *value,
    size_t index
);
struct json_value *mylite_json_internal_array_index_value_mutable(
    struct json_value *value,
    size_t index
);
int mylite_json_internal_emit_value(struct json_writer *writer, const struct json_value *value);
int mylite_json_internal_emit_pretty_value(
    struct json_writer *writer,
    const struct json_value *value
);
int mylite_json_internal_emit_object_keys(
    struct json_writer *writer,
    const struct json_object *object
);
int mylite_json_internal_emit_string(
    struct json_writer *writer,
    const char *text,
    size_t text_length
);
int mylite_json_internal_writer_append_char(struct json_writer *writer, char byte);
int mylite_json_internal_writer_append_text(
    struct json_writer *writer,
    const char *text,
    size_t text_length
);
char *mylite_json_internal_writer_take(struct json_writer *writer);
void mylite_json_internal_writer_deinit(struct json_writer *writer);
int mylite_json_internal_copy_result_text(
    const char *text,
    size_t text_length,
    char **out_text,
    size_t *out_text_length
);
void mylite_json_internal_value_deinit(struct json_value *value);
void mylite_json_internal_skip_whitespace(struct json_parser *parser);
bool mylite_json_internal_parser_at_end(const struct json_parser *parser);
char mylite_json_internal_parser_peek(const struct json_parser *parser);
bool mylite_json_internal_parser_match(struct json_parser *parser, char expected);
bool mylite_json_internal_is_decimal_digit(char byte);
bool mylite_json_internal_is_hex_digit(char byte);
int mylite_json_internal_parser_invalid(struct json_parser *parser, size_t position);
int mylite_json_internal_parser_invalid_with_detail(
    struct json_parser *parser,
    size_t position,
    enum mylite_json_error_detail detail
);
int mylite_json_internal_parser_unsupported(struct json_parser *parser, size_t position);

int mylite_json_internal_parse_document(struct json_parser *parser, struct json_value *out_value);
int mylite_json_internal_parse_string(
    struct json_parser *parser,
    char **out_text,
    size_t *out_text_length
);
bool mylite_json_internal_validate_document(struct json_parser *parser);
int mylite_json_internal_extract_path_value(
    struct json_parser *parser,
    const struct json_value *root,
    const struct json_value **out_value,
    bool *out_matched
);
int mylite_json_internal_parse_set_path(struct json_parser *parser, struct json_set_path *out_path);
void mylite_json_internal_set_path_deinit(struct json_set_path *path);
bool mylite_json_internal_value_contains(
    const struct json_value *target,
    const struct json_value *candidate
);
bool mylite_json_internal_values_equal(
    const struct json_value *left,
    const struct json_value *right
);
bool mylite_json_internal_values_overlap(
    const struct json_value *left,
    const struct json_value *right
);
int mylite_json_internal_apply_mutation_path(
    struct json_value *document,
    const struct json_set_path *path,
    struct json_value *value,
    enum json_mutation_mode mode
);

#endif
