#include <mylite/mylite.h>

#include "mylite_json_internal.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct json_search_matches {
    char **paths;
    size_t *path_lengths;
    size_t count;
    size_t capacity;
};

enum {
    json_search_array_leg_buffer_capacity = 32,
};

static int json_search_value(
    const struct json_value *value,
    struct json_writer *path,
    const char *pattern,
    size_t pattern_length,
    bool escape_enabled,
    unsigned char escape,
    enum mylite_json_search_mode mode,
    struct json_search_matches *matches
);
static int json_search_array_value(
    const struct json_value *value,
    struct json_writer *path,
    const char *pattern,
    size_t pattern_length,
    bool escape_enabled,
    unsigned char escape,
    enum mylite_json_search_mode mode,
    struct json_search_matches *matches
);
static int json_search_object_value(
    const struct json_value *value,
    struct json_writer *path,
    const char *pattern,
    size_t pattern_length,
    bool escape_enabled,
    unsigned char escape,
    enum mylite_json_search_mode mode,
    struct json_search_matches *matches
);
static int json_search_append_match(
    struct json_search_matches *matches,
    const char *path,
    size_t path_length
);
static bool json_search_has_match(
    const struct json_search_matches *matches,
    const char *path,
    size_t path_length
);
static int json_search_matches_reserve(
    struct json_search_matches *matches,
    size_t required_capacity
);
static int json_search_append_array_leg(struct json_writer *path, size_t index);
static int json_search_append_member_leg(
    struct json_writer *path,
    const char *member,
    size_t member_length
);
static bool json_search_member_can_use_identifier(const char *member, size_t member_length);
static bool json_search_identifier_start_byte(unsigned char byte);
static bool json_search_identifier_byte(unsigned char byte);
static bool json_search_pattern_matches(
    const char *pattern,
    size_t pattern_length,
    const char *value,
    size_t value_length,
    bool escape_enabled,
    unsigned char escape
);
static size_t json_search_skip_percent_run(
    const char *pattern,
    size_t pattern_length,
    size_t pattern_index,
    bool escape_enabled,
    unsigned char escape
);
static bool json_search_pattern_item_matches(
    const char *pattern,
    size_t pattern_length,
    size_t pattern_index,
    const char *value,
    bool escape_enabled,
    unsigned char escape,
    size_t *out_next_pattern_index
);
static int json_search_format_result(
    const struct json_search_matches *matches,
    char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
static void json_search_matches_deinit(struct json_search_matches *matches);

int mylite_json_search(
    enum mylite_json_search_mode mode,
    const char *text,
    size_t text_length,
    const char *pattern,
    size_t pattern_length,
    bool escape_enabled,
    unsigned char escape,
    const char *const *paths,
    const size_t *path_lengths,
    size_t path_count,
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
    struct json_value document = {0};
    struct json_writer path = {0};
    struct json_search_matches matches = {0};
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
    if (text == NULL || pattern == NULL ||
        (path_count != 0U && (paths == NULL || path_lengths == NULL))) {
        return MYLITE_ERROR;
    }

    rc = mylite_json_internal_parse_document(&document_parser, &document);
    *out_result = document_parser.result;
    if (rc != MYLITE_OK) {
        goto done;
    }

    if (path_count == 0U) {
        rc = mylite_json_internal_writer_append_char(&path, '$');
        if (rc == MYLITE_OK) {
            rc = json_search_value(
                &document,
                &path,
                pattern,
                pattern_length,
                escape_enabled,
                escape,
                mode,
                &matches
            );
        }
    } else {
        for (size_t path_index = 0U; rc == MYLITE_OK && path_index < path_count; ++path_index) {
            struct json_parser path_parser = {
                .text = paths[path_index],
                .length = path_lengths[path_index],
                .position = 0U,
                .result = {.status = MYLITE_JSON_NORMALIZE_OK, .position = 0U},
            };
            const struct json_value *matched_value = NULL;
            bool matched = false;

            rc = mylite_json_internal_extract_path_value(
                &path_parser,
                &document,
                &matched_value,
                &matched
            );
            *out_result = path_parser.result;
            if (rc != MYLITE_OK) {
                break;
            }
            if (!matched) {
                continue;
            }
            path.length = 0U;
            if (path.text != NULL) {
                path.text[0] = '\0';
            }
            rc = mylite_json_internal_writer_append_text(
                &path,
                paths[path_index],
                path_lengths[path_index]
            );
            if (rc == MYLITE_OK) {
                rc = json_search_value(
                    matched_value,
                    &path,
                    pattern,
                    pattern_length,
                    escape_enabled,
                    escape,
                    mode,
                    &matches
                );
            }
            if (mode == MYLITE_JSON_SEARCH_ONE && matches.count != 0U) {
                break;
            }
        }
    }
    if (rc == MYLITE_OK) {
        rc = json_search_format_result(&matches, out_text, out_text_length, out_is_null);
    }

done:
    json_search_matches_deinit(&matches);
    mylite_json_internal_writer_deinit(&path);
    mylite_json_internal_value_deinit(&document);
    return rc;
}

static int json_search_value(
    const struct json_value *value,
    struct json_writer *path,
    const char *pattern,
    size_t pattern_length,
    bool escape_enabled,
    unsigned char escape,
    enum mylite_json_search_mode mode,
    struct json_search_matches *matches
) {
    if (value == NULL) {
        return MYLITE_OK;
    }

    switch (value->kind) {
    case JSON_VALUE_STRING:
        if (json_search_pattern_matches(
                pattern,
                pattern_length,
                value->payload.text.text,
                value->payload.text.length,
                escape_enabled,
                escape
            )) {
            return json_search_append_match(matches, path->text, path->length);
        }
        return MYLITE_OK;
    case JSON_VALUE_ARRAY:
        return json_search_array_value(
            value,
            path,
            pattern,
            pattern_length,
            escape_enabled,
            escape,
            mode,
            matches
        );
    case JSON_VALUE_OBJECT:
        return json_search_object_value(
            value,
            path,
            pattern,
            pattern_length,
            escape_enabled,
            escape,
            mode,
            matches
        );
    case JSON_VALUE_NULL:
    case JSON_VALUE_BOOL:
    case JSON_VALUE_NUMBER:
        return MYLITE_OK;
    }
    return MYLITE_ERROR;
}

static int json_search_array_value(
    const struct json_value *value,
    struct json_writer *path,
    const char *pattern,
    size_t pattern_length,
    bool escape_enabled,
    unsigned char escape,
    enum mylite_json_search_mode mode,
    struct json_search_matches *matches
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U; rc == MYLITE_OK && index < value->payload.array.count; ++index) {
        size_t saved_length = path->length;

        rc = json_search_append_array_leg(path, index);
        if (rc == MYLITE_OK) {
            rc = json_search_value(
                &value->payload.array.values[index],
                path,
                pattern,
                pattern_length,
                escape_enabled,
                escape,
                mode,
                matches
            );
        }
        path->length = saved_length;
        if (path->text != NULL) {
            path->text[saved_length] = '\0';
        }
        if (mode == MYLITE_JSON_SEARCH_ONE && matches->count != 0U) {
            return rc;
        }
    }
    return rc;
}

static int json_search_object_value(
    const struct json_value *value,
    struct json_writer *path,
    const char *pattern,
    size_t pattern_length,
    bool escape_enabled,
    unsigned char escape,
    enum mylite_json_search_mode mode,
    struct json_search_matches *matches
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U; rc == MYLITE_OK && index < value->payload.object.count; ++index) {
        const struct json_member *member = &value->payload.object.members[index];
        size_t saved_length = path->length;

        rc = json_search_append_member_leg(path, member->key, member->key_length);
        if (rc == MYLITE_OK) {
            rc = json_search_value(
                member->value,
                path,
                pattern,
                pattern_length,
                escape_enabled,
                escape,
                mode,
                matches
            );
        }
        path->length = saved_length;
        if (path->text != NULL) {
            path->text[saved_length] = '\0';
        }
        if (mode == MYLITE_JSON_SEARCH_ONE && matches->count != 0U) {
            return rc;
        }
    }
    return rc;
}

static int json_search_append_match(
    struct json_search_matches *matches,
    const char *path,
    size_t path_length
) {
    char *copy = NULL;
    int rc = MYLITE_OK;

    if (matches == NULL || path == NULL) {
        return MYLITE_MISUSE;
    }
    if (json_search_has_match(matches, path, path_length)) {
        return MYLITE_OK;
    }
    rc = json_search_matches_reserve(matches, matches->count + 1U);
    if (rc != MYLITE_OK) {
        return rc;
    }
    copy = (char *)malloc(path_length + 1U);
    if (copy == NULL) {
        return MYLITE_NOMEM;
    }
    memcpy(copy, path, path_length);
    copy[path_length] = '\0';
    matches->paths[matches->count] = copy;
    matches->path_lengths[matches->count] = path_length;
    ++matches->count;
    return MYLITE_OK;
}

static bool json_search_has_match(
    const struct json_search_matches *matches,
    const char *path,
    size_t path_length
) {
    if (matches == NULL || path == NULL) {
        return false;
    }
    for (size_t index = 0U; index < matches->count; ++index) {
        const char *candidate = matches->paths[index];

        if (candidate != NULL && matches->path_lengths[index] == path_length &&
            memcmp(candidate, path, path_length) == 0) {
            return true;
        }
    }
    return false;
}

static int json_search_matches_reserve(
    struct json_search_matches *matches,
    size_t required_capacity
) {
    char **paths = NULL;
    size_t *path_lengths = NULL;
    size_t capacity = matches->capacity;

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
    if (capacity > SIZE_MAX / sizeof(*paths)) {
        return MYLITE_NOMEM;
    }
    paths = (char **)realloc((void *)matches->paths, capacity * sizeof(*paths));
    if (paths == NULL) {
        return MYLITE_NOMEM;
    }
    path_lengths = (size_t *)realloc(matches->path_lengths, capacity * sizeof(*path_lengths));
    if (path_lengths == NULL) {
        matches->paths = paths;
        return MYLITE_NOMEM;
    }
    matches->paths = paths;
    matches->path_lengths = path_lengths;
    matches->capacity = capacity;
    return MYLITE_OK;
}

static int json_search_append_array_leg(struct json_writer *path, size_t index) {
    char text[json_search_array_leg_buffer_capacity];
    int written = snprintf(text, sizeof(text), "[%zu]", index);

    if (written < 0 || (size_t)written >= sizeof(text)) {
        return MYLITE_NOMEM;
    }
    return mylite_json_internal_writer_append_text(path, text, (size_t)written);
}

static int json_search_append_member_leg(
    struct json_writer *path,
    const char *member,
    size_t member_length
) {
    int rc = mylite_json_internal_writer_append_char(path, '.');

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (json_search_member_can_use_identifier(member, member_length)) {
        return mylite_json_internal_writer_append_text(path, member, member_length);
    }
    return mylite_json_internal_emit_string(path, member, member_length);
}

static bool json_search_member_can_use_identifier(const char *member, size_t member_length) {
    if (member == NULL || member_length == 0U ||
        !json_search_identifier_start_byte((unsigned char)member[0])) {
        return false;
    }
    for (size_t index = 1U; index < member_length; ++index) {
        if (!json_search_identifier_byte((unsigned char)member[index])) {
            return false;
        }
    }
    return true;
}

static bool json_search_identifier_start_byte(unsigned char byte) {
    return (byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z') || byte == '_' ||
           byte == '$';
}

static bool json_search_identifier_byte(unsigned char byte) {
    return json_search_identifier_start_byte(byte) || (byte >= '0' && byte <= '9');
}

static bool json_search_pattern_matches(
    const char *pattern,
    size_t pattern_length,
    const char *value,
    size_t value_length,
    bool escape_enabled,
    unsigned char escape
) {
    const size_t no_retry_pattern = (size_t)-1;
    size_t pattern_index = 0U;
    size_t value_index = 0U;
    size_t retry_pattern_index = no_retry_pattern;
    size_t retry_value_index = 0U;

    while (value_index < value_length) {
        size_t next_pattern_index = pattern_index;

        if (pattern_index < pattern_length && pattern[pattern_index] == '%' &&
            (!escape_enabled || (unsigned char)pattern[pattern_index] != escape)) {
            pattern_index = json_search_skip_percent_run(
                pattern,
                pattern_length,
                pattern_index,
                escape_enabled,
                escape
            );
            if (pattern_index == pattern_length) {
                return true;
            }
            retry_pattern_index = pattern_index;
            retry_value_index = value_index;
            continue;
        }
        if (json_search_pattern_item_matches(
                pattern,
                pattern_length,
                pattern_index,
                &value[value_index],
                escape_enabled,
                escape,
                &next_pattern_index
            )) {
            pattern_index = next_pattern_index;
            ++value_index;
            continue;
        }
        if (retry_pattern_index == no_retry_pattern || retry_value_index >= value_length) {
            return false;
        }
        ++retry_value_index;
        value_index = retry_value_index;
        pattern_index = retry_pattern_index;
    }

    pattern_index = json_search_skip_percent_run(
        pattern,
        pattern_length,
        pattern_index,
        escape_enabled,
        escape
    );
    return pattern_index == pattern_length;
}

static size_t json_search_skip_percent_run(
    const char *pattern,
    size_t pattern_length,
    size_t pattern_index,
    bool escape_enabled,
    unsigned char escape
) {
    while (pattern_index < pattern_length && pattern[pattern_index] == '%' &&
           (!escape_enabled || (unsigned char)pattern[pattern_index] != escape)) {
        ++pattern_index;
    }
    return pattern_index;
}

static bool json_search_pattern_item_matches(
    const char *pattern,
    size_t pattern_length,
    size_t pattern_index,
    const char *value,
    bool escape_enabled,
    unsigned char escape,
    size_t *out_next_pattern_index
) {
    unsigned char pattern_byte = '\0';
    size_t next_pattern_index = pattern_index;

    if (pattern_index >= pattern_length || value == NULL || out_next_pattern_index == NULL) {
        return false;
    }
    pattern_byte = (unsigned char)pattern[pattern_index];
    if (escape_enabled && pattern_byte == escape) {
        ++next_pattern_index;
        if (next_pattern_index < pattern_length) {
            pattern_byte = (unsigned char)pattern[next_pattern_index];
            ++next_pattern_index;
        }
        if (pattern_byte != (unsigned char)*value) {
            return false;
        }
        *out_next_pattern_index = next_pattern_index;
        return true;
    }
    if (pattern_byte == '_') {
        *out_next_pattern_index = pattern_index + 1U;
        return true;
    }
    ++next_pattern_index;
    if (pattern_byte != (unsigned char)*value) {
        return false;
    }

    *out_next_pattern_index = next_pattern_index;
    return true;
}

static int json_search_format_result(
    const struct json_search_matches *matches,
    char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    struct json_writer writer = {0};
    int rc = MYLITE_OK;

    if (matches == NULL || out_text == NULL || out_text_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    if (matches->count == 0U) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    if (matches->count == 1U) {
        rc = mylite_json_internal_emit_string(&writer, matches->paths[0], matches->path_lengths[0]);
    } else {
        rc = mylite_json_internal_writer_append_char(&writer, '[');
        for (size_t index = 0U; rc == MYLITE_OK && index < matches->count; ++index) {
            if (index != 0U) {
                rc = mylite_json_internal_writer_append_text(&writer, ", ", 2U);
            }
            if (rc == MYLITE_OK) {
                rc = mylite_json_internal_emit_string(
                    &writer,
                    matches->paths[index],
                    matches->path_lengths[index]
                );
            }
        }
        if (rc == MYLITE_OK) {
            rc = mylite_json_internal_writer_append_char(&writer, ']');
        }
    }
    if (rc == MYLITE_OK) {
        *out_text_length = writer.length;
        *out_text = mylite_json_internal_writer_take(&writer);
    }
    mylite_json_internal_writer_deinit(&writer);
    return rc;
}

static void json_search_matches_deinit(struct json_search_matches *matches) {
    if (matches == NULL) {
        return;
    }
    for (size_t index = 0U; index < matches->count; ++index) {
        free(matches->paths[index]);
    }
    free((void *)matches->paths);
    free(matches->path_lengths);
    *matches = (struct json_search_matches){0};
}
