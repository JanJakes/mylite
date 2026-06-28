#include <mylite/mylite.h>

#include "mylite_json_internal.h"

#include <stdlib.h>
#include <string.h>

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

static int object_reserve_members(struct json_object *object, size_t required_capacity);
static bool object_member_is_after(const struct json_member *left, const struct json_member *right);
static int compare_object_member_keys(
    const struct json_member *left,
    const struct json_member *right
);
static int emit_value_start(
    struct json_writer *writer,
    const struct json_value *value,
    struct json_emit_stack *stack
);
static int emit_pretty_value(
    struct json_writer *writer,
    const struct json_value *value,
    size_t depth
);
static int emit_pretty_scalar_value(struct json_writer *writer, const struct json_value *value);
static int emit_pretty_array_value(
    struct json_writer *writer,
    const struct json_value *value,
    size_t depth
);
static int emit_pretty_object_value(
    struct json_writer *writer,
    const struct json_value *value,
    size_t depth
);
static int emit_pretty_indent(struct json_writer *writer, size_t depth);
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
static int writer_append_json_escape(struct json_writer *writer, unsigned char byte);
static int writer_append_ascii_hex_digit(struct json_writer *writer, unsigned char value);
static int writer_reserve(struct json_writer *writer, size_t required_capacity);
static bool deinit_stack_push(
    struct json_deinit_stack *stack,
    struct json_value *value,
    bool free_value
);
static void deinit_stack_pop(struct json_deinit_stack *stack);

const char *mylite_json_internal_value_type_name(const struct json_value *value) {
    if (value == NULL) {
        return NULL;
    }

    switch (value->kind) {
    case JSON_VALUE_NULL:
        return "NULL";
    case JSON_VALUE_BOOL:
        return "BOOLEAN";
    case JSON_VALUE_NUMBER:
        return value->number_kind == JSON_NUMBER_DOUBLE ? "DOUBLE" : "INTEGER";
    case JSON_VALUE_STRING:
        return "STRING";
    case JSON_VALUE_ARRAY:
        return "ARRAY";
    case JSON_VALUE_OBJECT:
        return "OBJECT";
    }

    return NULL;
}

int mylite_json_internal_value_shallow_length(const struct json_value *value, int64_t *out_length) {
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

int mylite_json_internal_value_depth(const struct json_value *value, int64_t *out_depth) {
    int64_t maximum_child_depth = 0;

    if (value == NULL || out_depth == NULL) {
        return MYLITE_MISUSE;
    }

    if (value->kind == JSON_VALUE_ARRAY) {
        for (size_t index = 0U; index < value->payload.array.count; ++index) {
            int64_t child_depth = 0;
            int rc =
                mylite_json_internal_value_depth(&value->payload.array.values[index], &child_depth);

            if (rc != MYLITE_OK) {
                return rc;
            }
            if (child_depth > maximum_child_depth) {
                maximum_child_depth = child_depth;
            }
        }
        *out_depth = maximum_child_depth + 1;
        return MYLITE_OK;
    }
    if (value->kind == JSON_VALUE_OBJECT) {
        for (size_t index = 0U; index < value->payload.object.count; ++index) {
            int64_t child_depth = 0;
            int rc = mylite_json_internal_value_depth(
                value->payload.object.members[index].value,
                &child_depth
            );

            if (rc != MYLITE_OK) {
                return rc;
            }
            if (child_depth > maximum_child_depth) {
                maximum_child_depth = child_depth;
            }
        }
        *out_depth = maximum_child_depth + 1;
        return MYLITE_OK;
    }

    *out_depth = 1;
    return MYLITE_OK;
}

int mylite_json_internal_object_append_member(
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
            mylite_json_internal_value_deinit(member->value);
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

void mylite_json_internal_sort_object_members_by_mysql_display_order(struct json_object *object) {
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

int mylite_json_internal_array_append_value(
    struct json_array *array,
    struct json_value *value,
    struct json_value **out_stored_value
) {
    int rc = mylite_json_internal_array_reserve_values(array, array->count + 1U);

    if (rc != MYLITE_OK) {
        return rc;
    }
    array->values[array->count] = *value;
    *value = (struct json_value){0};
    *out_stored_value = &array->values[array->count];
    ++array->count;
    return MYLITE_OK;
}

int mylite_json_internal_array_reserve_values(struct json_array *array, size_t required_capacity) {
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

struct json_value *mylite_json_internal_object_member_value_mutable(
    struct json_value *value,
    const char *member,
    size_t member_length
) {
    if (value == NULL || value->kind != JSON_VALUE_OBJECT || member == NULL) {
        return NULL;
    }
    for (size_t index = 0U; index < value->payload.object.count; ++index) {
        struct json_member *candidate = &value->payload.object.members[index];

        if (candidate->key_length == member_length &&
            memcmp(candidate->key, member, member_length) == 0) {
            return candidate->value;
        }
    }
    return NULL;
}

struct json_value *mylite_json_internal_array_index_value_mutable(
    struct json_value *value,
    size_t index
) {
    if (value == NULL || value->kind != JSON_VALUE_ARRAY || index >= value->payload.array.count) {
        return NULL;
    }
    return &value->payload.array.values[index];
}

const struct json_value *mylite_json_internal_object_member_value(
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

const struct json_value *mylite_json_internal_array_index_value(
    const struct json_value *value,
    size_t index
) {
    if (value == NULL || value->kind != JSON_VALUE_ARRAY || index >= value->payload.array.count) {
        return NULL;
    }
    return &value->payload.array.values[index];
}

int mylite_json_internal_emit_value(struct json_writer *writer, const struct json_value *value) {
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

int mylite_json_internal_emit_pretty_value(
    struct json_writer *writer,
    const struct json_value *value
) {
    if (writer == NULL || value == NULL) {
        return MYLITE_MISUSE;
    }
    return emit_pretty_value(writer, value, 0U);
}

static int emit_pretty_value(
    struct json_writer *writer,
    const struct json_value *value,
    size_t depth
) {
    if (value->kind == JSON_VALUE_ARRAY) {
        return emit_pretty_array_value(writer, value, depth);
    }
    if (value->kind == JSON_VALUE_OBJECT) {
        return emit_pretty_object_value(writer, value, depth);
    }
    return emit_pretty_scalar_value(writer, value);
}

static int emit_pretty_scalar_value(struct json_writer *writer, const struct json_value *value) {
    struct json_emit_stack stack = {0};

    return emit_value_start(writer, value, &stack);
}

static int emit_pretty_array_value(
    struct json_writer *writer,
    const struct json_value *value,
    size_t depth
) {
    int rc = mylite_json_internal_writer_append_char(writer, '[');

    if (rc != MYLITE_OK || value->payload.array.count == 0U) {
        return rc == MYLITE_OK ? mylite_json_internal_writer_append_char(writer, ']') : rc;
    }
    rc = mylite_json_internal_writer_append_char(writer, '\n');
    for (size_t index = 0U; rc == MYLITE_OK && index < value->payload.array.count; ++index) {
        rc = emit_pretty_indent(writer, depth + 1U);
        if (rc == MYLITE_OK) {
            rc = emit_pretty_value(writer, &value->payload.array.values[index], depth + 1U);
        }
        if (rc == MYLITE_OK && index + 1U < value->payload.array.count) {
            rc = mylite_json_internal_writer_append_char(writer, ',');
        }
        if (rc == MYLITE_OK) {
            rc = mylite_json_internal_writer_append_char(writer, '\n');
        }
    }
    if (rc == MYLITE_OK) {
        rc = emit_pretty_indent(writer, depth);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_json_internal_writer_append_char(writer, ']');
    }
    return rc;
}

static int emit_pretty_object_value(
    struct json_writer *writer,
    const struct json_value *value,
    size_t depth
) {
    int rc = mylite_json_internal_writer_append_char(writer, '{');

    if (rc != MYLITE_OK || value->payload.object.count == 0U) {
        return rc == MYLITE_OK ? mylite_json_internal_writer_append_char(writer, '}') : rc;
    }
    rc = mylite_json_internal_writer_append_char(writer, '\n');
    for (size_t index = 0U; rc == MYLITE_OK && index < value->payload.object.count; ++index) {
        const struct json_member *member = &value->payload.object.members[index];

        rc = emit_pretty_indent(writer, depth + 1U);
        if (rc == MYLITE_OK) {
            rc = mylite_json_internal_emit_string(writer, member->key, member->key_length);
        }
        if (rc == MYLITE_OK) {
            rc =
                mylite_json_internal_writer_append_text(writer, ": ", json_member_separator_length);
        }
        if (rc == MYLITE_OK) {
            rc = emit_pretty_value(writer, member->value, depth + 1U);
        }
        if (rc == MYLITE_OK && index + 1U < value->payload.object.count) {
            rc = mylite_json_internal_writer_append_char(writer, ',');
        }
        if (rc == MYLITE_OK) {
            rc = mylite_json_internal_writer_append_char(writer, '\n');
        }
    }
    if (rc == MYLITE_OK) {
        rc = emit_pretty_indent(writer, depth);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_json_internal_writer_append_char(writer, '}');
    }
    return rc;
}

static int emit_pretty_indent(struct json_writer *writer, size_t depth) {
    if (depth > SIZE_MAX / json_pretty_indent_width) {
        return MYLITE_NOMEM;
    }
    for (size_t index = 0U; index < depth * json_pretty_indent_width; ++index) {
        int rc = mylite_json_internal_writer_append_char(writer, ' ');

        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    return MYLITE_OK;
}

static int emit_value_start(
    struct json_writer *writer,
    const struct json_value *value,
    struct json_emit_stack *stack
) {
    switch (value->kind) {
    case JSON_VALUE_NULL:
        return mylite_json_internal_writer_append_text(writer, "null", json_null_literal_length);
    case JSON_VALUE_BOOL:
        return emit_bool_value(writer, value->payload.boolean);
    case JSON_VALUE_NUMBER:
        return mylite_json_internal_writer_append_text(
            writer,
            value->payload.text.text,
            value->payload.text.length
        );
    case JSON_VALUE_STRING:
        return mylite_json_internal_emit_string(
            writer,
            value->payload.text.text,
            value->payload.text.length
        );
    case JSON_VALUE_ARRAY:
        if (mylite_json_internal_writer_append_char(writer, '[') != MYLITE_OK) {
            return MYLITE_NOMEM;
        }
        if (value->payload.array.count == 0U) {
            return mylite_json_internal_writer_append_char(writer, ']');
        }
        return emit_stack_push(stack, value);
    case JSON_VALUE_OBJECT:
        if (mylite_json_internal_writer_append_char(writer, '{') != MYLITE_OK) {
            return MYLITE_NOMEM;
        }
        if (value->payload.object.count == 0U) {
            return mylite_json_internal_writer_append_char(writer, '}');
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
        return mylite_json_internal_writer_append_char(writer, ']');
    }
    if (frame->index > 0U &&
        mylite_json_internal_writer_append_text(writer, ", ", json_member_separator_length) !=
            MYLITE_OK) {
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
        return mylite_json_internal_writer_append_char(writer, '}');
    }
    if (frame->index > 0U) {
        rc = mylite_json_internal_writer_append_text(writer, ", ", json_member_separator_length);
    }
    member = &frame->container->payload.object.members[frame->index];
    if (rc == MYLITE_OK) {
        rc = mylite_json_internal_emit_string(writer, member->key, member->key_length);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_json_internal_writer_append_text(writer, ": ", json_member_separator_length);
    }
    if (rc == MYLITE_OK) {
        *out_child = member->value;
        ++frame->index;
    }
    return rc;
}

int mylite_json_internal_emit_object_keys(
    struct json_writer *writer,
    const struct json_object *object
) {
    int rc = MYLITE_OK;

    if (writer == NULL || object == NULL) {
        return MYLITE_MISUSE;
    }
    rc = mylite_json_internal_writer_append_char(writer, '[');
    for (size_t index = 0U; rc == MYLITE_OK && index < object->count; ++index) {
        const struct json_member *member = &object->members[index];

        if (index > 0U) {
            rc =
                mylite_json_internal_writer_append_text(writer, ", ", json_member_separator_length);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_json_internal_emit_string(writer, member->key, member->key_length);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_json_internal_writer_append_char(writer, ']');
    }
    return rc;
}

static int emit_bool_value(struct json_writer *writer, bool boolean) {
    if (boolean) {
        return mylite_json_internal_writer_append_text(writer, "true", json_true_literal_length);
    }
    return mylite_json_internal_writer_append_text(writer, "false", json_false_literal_length);
}

static int emit_stack_push(struct json_emit_stack *stack, const struct json_value *container) {
    if (stack->count >= json_max_nesting_depth) {
        return MYLITE_ERROR;
    }
    stack->frames[stack->count] = (struct json_emit_frame){.container = container, .index = 0U};
    ++stack->count;
    return MYLITE_OK;
}

int mylite_json_internal_emit_string(
    struct json_writer *writer,
    const char *text,
    size_t text_length
) {
    int rc = mylite_json_internal_writer_append_char(writer, '"');

    for (size_t index = 0U; rc == MYLITE_OK && index < text_length; ++index) {
        unsigned char byte = (unsigned char)text[index];

        if (byte == '"' || byte == '\\' || byte < json_control_byte_limit) {
            rc = writer_append_json_escape(writer, byte);
        } else {
            rc = mylite_json_internal_writer_append_char(writer, (char)byte);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_json_internal_writer_append_char(writer, '"');
    }
    return rc;
}

static int writer_append_json_escape(struct json_writer *writer, unsigned char byte) {
    switch (byte) {
    case '"':
        return mylite_json_internal_writer_append_text(writer, "\\\"", 2U);
    case '\\':
        return mylite_json_internal_writer_append_text(writer, "\\\\", 2U);
    case '\b':
        return mylite_json_internal_writer_append_text(writer, "\\b", 2U);
    case '\f':
        return mylite_json_internal_writer_append_text(writer, "\\f", 2U);
    case '\n':
        return mylite_json_internal_writer_append_text(writer, "\\n", 2U);
    case '\r':
        return mylite_json_internal_writer_append_text(writer, "\\r", 2U);
    case '\t':
        return mylite_json_internal_writer_append_text(writer, "\\t", 2U);
    default:
        if (mylite_json_internal_writer_append_text(
                writer,
                "\\u00",
                json_unicode_escape_prefix_length
            ) != MYLITE_OK) {
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

    return mylite_json_internal_writer_append_char(writer, digits[value & json_hex_low_nibble]);
}

int mylite_json_internal_writer_append_char(struct json_writer *writer, char byte) {
    return mylite_json_internal_writer_append_text(writer, &byte, 1U);
}

int mylite_json_internal_writer_append_text(
    struct json_writer *writer,
    const char *text,
    size_t text_length
) {
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

char *mylite_json_internal_writer_take(struct json_writer *writer) {
    char *text = writer->text;

    writer->text = NULL;
    writer->length = 0U;
    writer->capacity = 0U;
    return text;
}

void mylite_json_internal_writer_deinit(struct json_writer *writer) {
    free(writer->text);
    *writer = (struct json_writer){0};
}

int mylite_json_internal_copy_result_text(
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

void mylite_json_internal_value_deinit(struct json_value *value) {
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

void mylite_json_internal_skip_whitespace(struct json_parser *parser) {
    while (!mylite_json_internal_parser_at_end(parser)) {
        char byte = mylite_json_internal_parser_peek(parser);

        if (byte != ' ' && byte != '\t' && byte != '\n' && byte != '\r') {
            return;
        }
        ++parser->position;
    }
}

bool mylite_json_internal_parser_at_end(const struct json_parser *parser) {
    return parser->position >= parser->length;
}

char mylite_json_internal_parser_peek(const struct json_parser *parser) {
    if (mylite_json_internal_parser_at_end(parser)) {
        return '\0';
    }
    return parser->text[parser->position];
}

bool mylite_json_internal_parser_match(struct json_parser *parser, char expected) {
    if (mylite_json_internal_parser_at_end(parser) || parser->text[parser->position] != expected) {
        return false;
    }
    ++parser->position;
    return true;
}

bool mylite_json_internal_is_decimal_digit(char byte) {
    if (byte < '0') {
        return false;
    }
    return byte <= '9';
}

bool mylite_json_internal_is_hex_digit(char byte) {
    if (byte >= '0' && byte <= '9') {
        return true;
    }
    if (byte >= 'A' && byte <= 'F') {
        return true;
    }
    return (byte >= 'a' && byte <= 'f') != 0;
}

int mylite_json_internal_parser_invalid(struct json_parser *parser, size_t position) {
    return mylite_json_internal_parser_invalid_with_detail(
        parser,
        position,
        MYLITE_JSON_ERROR_INVALID_VALUE
    );
}

int mylite_json_internal_parser_invalid_with_detail(
    struct json_parser *parser,
    size_t position,
    enum mylite_json_error_detail detail
) {
    parser->result = (struct mylite_json_normalize_result){
        .status = MYLITE_JSON_NORMALIZE_INVALID,
        .position = position,
        .error_detail = detail,
    };
    return MYLITE_ERROR;
}

int mylite_json_internal_parser_unsupported(struct json_parser *parser, size_t position) {
    parser->result = (struct mylite_json_normalize_result){
        .status = MYLITE_JSON_NORMALIZE_UNSUPPORTED,
        .position = position,
    };
    return MYLITE_ERROR;
}
