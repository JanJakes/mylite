#include <mylite/mylite.h>

#include "mylite_json_internal.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static int merge_json_documents(
    const char *const *documents,
    const size_t *document_lengths,
    size_t document_count,
    bool patch_mode,
    char **out_text,
    size_t *out_text_length,
    struct mylite_json_normalize_result *out_result
);
static int validate_json_merge_documents(
    const char *const *documents,
    const size_t *document_lengths,
    size_t document_count,
    struct mylite_json_normalize_result *out_result
);
static int parse_json_merge_document(
    const char *document,
    size_t document_length,
    struct json_value *out_value,
    struct mylite_json_normalize_result *out_result
);
static int emit_json_merge_value(
    struct json_value *value,
    char **out_text,
    size_t *out_text_length
);
static int apply_json_merge_patch_value(struct json_value *target, struct json_value *patch);
static int apply_json_merge_patch_object(struct json_value *target, struct json_value *patch);
static int merge_json_preserve_values(struct json_value *target, struct json_value *source);
static int merge_json_preserve_objects(struct json_object *target, struct json_object *source);
static int merge_json_preserve_autowrap_values(
    struct json_value *target,
    struct json_value *source
);
static int append_json_merge_array_values(struct json_array *target, struct json_array *source);
static int append_json_merge_moved_member(
    struct json_object *target,
    struct json_member *source_member
);
static bool json_merge_object_member_index(
    const struct json_object *object,
    const char *key,
    size_t key_length,
    size_t *out_index
);
static void remove_json_merge_object_member_at(struct json_object *object, size_t index);

int mylite_json_merge_patch(
    const char *const *documents,
    const size_t *document_lengths,
    size_t document_count,
    char **out_text,
    size_t *out_text_length,
    struct mylite_json_normalize_result *out_result
) {
    return merge_json_documents(
        documents,
        document_lengths,
        document_count,
        true,
        out_text,
        out_text_length,
        out_result
    );
}

int mylite_json_merge_preserve(
    const char *const *documents,
    const size_t *document_lengths,
    size_t document_count,
    char **out_text,
    size_t *out_text_length,
    struct mylite_json_normalize_result *out_result
) {
    return merge_json_documents(
        documents,
        document_lengths,
        document_count,
        false,
        out_text,
        out_text_length,
        out_result
    );
}

int mylite_json_merge_patch_validate_documents(
    const char *const *documents,
    const size_t *document_lengths,
    size_t document_count,
    struct mylite_json_normalize_result *out_result
) {
    return validate_json_merge_documents(documents, document_lengths, document_count, out_result);
}

static int merge_json_documents(
    const char *const *documents,
    const size_t *document_lengths,
    size_t document_count,
    bool patch_mode,
    char **out_text,
    size_t *out_text_length,
    struct mylite_json_normalize_result *out_result
) {
    struct json_value result = {0};
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
    if (documents == NULL || document_lengths == NULL || document_count == 0U) {
        return MYLITE_MISUSE;
    }

    rc = parse_json_merge_document(documents[0], document_lengths[0], &result, out_result);
    for (size_t index = 1U; rc == MYLITE_OK && index < document_count; ++index) {
        struct json_value next = {0};

        rc =
            parse_json_merge_document(documents[index], document_lengths[index], &next, out_result);
        if (rc == MYLITE_OK) {
            rc = patch_mode ? apply_json_merge_patch_value(&result, &next)
                            : merge_json_preserve_values(&result, &next);
        }
        mylite_json_internal_value_deinit(&next);
    }
    if (rc == MYLITE_OK) {
        rc = emit_json_merge_value(&result, out_text, out_text_length);
    }

    mylite_json_internal_value_deinit(&result);
    return rc;
}

static int validate_json_merge_documents(
    const char *const *documents,
    const size_t *document_lengths,
    size_t document_count,
    struct mylite_json_normalize_result *out_result
) {
    if (out_result == NULL) {
        return MYLITE_MISUSE;
    }
    *out_result = (struct mylite_json_normalize_result){
        .status = MYLITE_JSON_NORMALIZE_OK,
        .position = 0U,
    };
    if (document_count != 0U && (documents == NULL || document_lengths == NULL)) {
        return MYLITE_MISUSE;
    }

    for (size_t index = 0U; index < document_count; ++index) {
        struct json_value value = {0};
        int rc = parse_json_merge_document(
            documents[index],
            document_lengths[index],
            &value,
            out_result
        );

        mylite_json_internal_value_deinit(&value);
        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    return MYLITE_OK;
}

static int parse_json_merge_document(
    const char *document,
    size_t document_length,
    struct json_value *out_value,
    struct mylite_json_normalize_result *out_result
) {
    struct json_parser parser = {
        .text = document,
        .length = document_length,
        .position = 0U,
        .result = {.status = MYLITE_JSON_NORMALIZE_OK, .position = 0U},
    };
    int rc = MYLITE_OK;

    if (document == NULL || out_value == NULL || out_result == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = (struct json_value){0};

    rc = mylite_json_internal_parse_document(&parser, out_value);
    *out_result = parser.result;
    return rc;
}

static int emit_json_merge_value(
    struct json_value *value,
    char **out_text,
    size_t *out_text_length
) {
    struct json_writer writer = {0};
    int rc = mylite_json_internal_emit_value(&writer, value);

    if (rc == MYLITE_OK) {
        *out_text_length = writer.length;
        *out_text = mylite_json_internal_writer_take(&writer);
        if (*out_text == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    mylite_json_internal_writer_deinit(&writer);
    return rc;
}

static int apply_json_merge_patch_value(struct json_value *target, struct json_value *patch) {
    if (target == NULL || patch == NULL) {
        return MYLITE_MISUSE;
    }
    if (patch->kind != JSON_VALUE_OBJECT) {
        mylite_json_internal_value_deinit(target);
        *target = *patch;
        *patch = (struct json_value){0};
        return MYLITE_OK;
    }
    if (target->kind != JSON_VALUE_OBJECT) {
        mylite_json_internal_value_deinit(target);
        *target = (struct json_value){.kind = JSON_VALUE_OBJECT};
    }
    return apply_json_merge_patch_object(target, patch);
}

static int apply_json_merge_patch_object(struct json_value *target, struct json_value *patch) {
    int rc = MYLITE_OK;

    if (target == NULL || patch == NULL || target->kind != JSON_VALUE_OBJECT ||
        patch->kind != JSON_VALUE_OBJECT) {
        return MYLITE_MISUSE;
    }

    for (size_t index = 0U; rc == MYLITE_OK && index < patch->payload.object.count; ++index) {
        struct json_member *source_member = &patch->payload.object.members[index];
        size_t target_index = 0U;
        bool found = json_merge_object_member_index(
            &target->payload.object,
            source_member->key,
            source_member->key_length,
            &target_index
        );

        if (source_member->value->kind == JSON_VALUE_NULL) {
            if (found) {
                remove_json_merge_object_member_at(&target->payload.object, target_index);
            }
            continue;
        }
        if (!found) {
            rc = append_json_merge_moved_member(&target->payload.object, source_member);
            continue;
        }

        struct json_value *target_value = target->payload.object.members[target_index].value;

        if (target_value->kind == JSON_VALUE_OBJECT &&
            source_member->value->kind == JSON_VALUE_OBJECT) {
            rc = apply_json_merge_patch_object(target_value, source_member->value);
        } else {
            mylite_json_internal_value_deinit(target_value);
            *target_value = *source_member->value;
            *source_member->value = (struct json_value){0};
        }
    }
    if (rc == MYLITE_OK) {
        mylite_json_internal_sort_object_members_by_mysql_display_order(&target->payload.object);
    }
    return rc;
}

static int merge_json_preserve_values(struct json_value *target, struct json_value *source) {
    if (target == NULL || source == NULL) {
        return MYLITE_MISUSE;
    }
    if (target->kind == JSON_VALUE_OBJECT && source->kind == JSON_VALUE_OBJECT) {
        return merge_json_preserve_objects(&target->payload.object, &source->payload.object);
    }
    return merge_json_preserve_autowrap_values(target, source);
}

static int merge_json_preserve_objects(struct json_object *target, struct json_object *source) {
    int rc = MYLITE_OK;

    if (target == NULL || source == NULL) {
        return MYLITE_MISUSE;
    }

    for (size_t index = 0U; rc == MYLITE_OK && index < source->count; ++index) {
        struct json_member *source_member = &source->members[index];
        size_t target_index = 0U;

        if (!json_merge_object_member_index(
                target,
                source_member->key,
                source_member->key_length,
                &target_index
            )) {
            rc = append_json_merge_moved_member(target, source_member);
            continue;
        }

        struct json_value *target_value = target->members[target_index].value;

        if (target_value->kind == JSON_VALUE_OBJECT &&
            source_member->value->kind == JSON_VALUE_OBJECT) {
            rc = merge_json_preserve_objects(
                &target_value->payload.object,
                &source_member->value->payload.object
            );
        } else {
            rc = merge_json_preserve_autowrap_values(target_value, source_member->value);
        }
    }
    if (rc == MYLITE_OK) {
        mylite_json_internal_sort_object_members_by_mysql_display_order(target);
    }
    return rc;
}

static int merge_json_preserve_autowrap_values(
    struct json_value *target,
    struct json_value *source
) {
    if (target == NULL || source == NULL) {
        return MYLITE_MISUSE;
    }
    if (target->kind == JSON_VALUE_ARRAY) {
        if (source->kind == JSON_VALUE_ARRAY) {
            return append_json_merge_array_values(&target->payload.array, &source->payload.array);
        }

        struct json_value *stored_value = NULL;

        return mylite_json_internal_array_append_value(
            &target->payload.array,
            source,
            &stored_value
        );
    }

    size_t source_count = source->kind == JSON_VALUE_ARRAY ? source->payload.array.count : 1U;
    struct json_value array = {.kind = JSON_VALUE_ARRAY};
    int rc = mylite_json_internal_array_reserve_values(&array.payload.array, source_count + 1U);

    if (rc != MYLITE_OK) {
        return rc;
    }

    array.payload.array.values[0] = *target;
    *target = (struct json_value){0};
    array.payload.array.count = 1U;

    if (source->kind == JSON_VALUE_ARRAY) {
        for (size_t index = 0U; index < source->payload.array.count; ++index) {
            array.payload.array.values[array.payload.array.count] =
                source->payload.array.values[index];
            source->payload.array.values[index] = (struct json_value){0};
            ++array.payload.array.count;
        }
    } else {
        array.payload.array.values[array.payload.array.count] = *source;
        *source = (struct json_value){0};
        ++array.payload.array.count;
    }

    *target = array;
    return MYLITE_OK;
}

static int append_json_merge_array_values(struct json_array *target, struct json_array *source) {
    if (target == NULL || source == NULL) {
        return MYLITE_MISUSE;
    }

    for (size_t index = 0U; index < source->count; ++index) {
        struct json_value *stored_value = NULL;
        int rc =
            mylite_json_internal_array_append_value(target, &source->values[index], &stored_value);

        (void)stored_value;
        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    return MYLITE_OK;
}

static int append_json_merge_moved_member(
    struct json_object *target,
    struct json_member *source_member
) {
    struct json_value *stored_value = NULL;
    int rc = MYLITE_OK;

    if (target == NULL || source_member == NULL || source_member->value == NULL) {
        return MYLITE_MISUSE;
    }

    rc = mylite_json_internal_object_append_member(
        target,
        source_member->key,
        source_member->key_length,
        source_member->value,
        &stored_value
    );
    if (rc == MYLITE_OK) {
        source_member->key = NULL;
        source_member->key_length = 0U;
    }
    (void)stored_value;
    return rc;
}

static bool json_merge_object_member_index(
    const struct json_object *object,
    const char *key,
    size_t key_length,
    size_t *out_index
) {
    if (out_index != NULL) {
        *out_index = 0U;
    }
    if (object == NULL || key == NULL || out_index == NULL) {
        return false;
    }
    for (size_t index = 0U; index < object->count; ++index) {
        const struct json_member *member = &object->members[index];

        if (member->key_length == key_length && memcmp(member->key, key, key_length) == 0) {
            *out_index = index;
            return true;
        }
    }
    return false;
}

static void remove_json_merge_object_member_at(struct json_object *object, size_t index) {
    struct json_member *member = NULL;

    if (object == NULL || index >= object->count) {
        return;
    }

    member = &object->members[index];
    free(member->key);
    mylite_json_internal_value_deinit(member->value);
    free(member->value);
    if (index + 1U < object->count) {
        memmove(
            &object->members[index],
            &object->members[index + 1U],
            (object->count - index - 1U) * sizeof(*object->members)
        );
    }
    --object->count;
}
