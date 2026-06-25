#include <mylite/mylite.h>

#include "mylite_json_internal.h"

#include <stdlib.h>
#include <string.h>

static int apply_json_set_path(
    struct json_value *document,
    const struct json_set_path *path,
    struct json_value *value
);
static int apply_json_insert_path(
    struct json_value *document,
    const struct json_set_path *path,
    struct json_value *value
);
static int apply_json_insert_member_leg(
    struct json_value *target,
    const struct json_set_path_leg *leg,
    struct json_value *value
);
static int apply_json_insert_array_leg(
    struct json_value *target,
    size_t index,
    struct json_value *value
);
static int apply_json_set_member_leg(
    struct json_value *target,
    const struct json_set_path_leg *leg,
    struct json_value *value
);
static int apply_json_set_array_leg(
    struct json_value *target,
    size_t index,
    struct json_value *value
);
static int apply_json_array_append_path(
    struct json_value *document,
    const struct json_set_path *path,
    struct json_value *value
);
static int append_json_array_append_value(struct json_value *target, struct json_value *value);
static int apply_json_array_insert_path(
    struct json_value *document,
    const struct json_set_path *path,
    struct json_value *value
);
static int insert_json_array_value(
    struct json_value *target,
    size_t index,
    struct json_value *value
);
static int apply_json_replace_path(
    struct json_value *document,
    const struct json_set_path *path,
    struct json_value *value
);
static int apply_json_replace_member_leg(
    struct json_value *target,
    const struct json_set_path_leg *leg,
    struct json_value *value
);
static int apply_json_replace_array_leg(
    struct json_value *target,
    size_t index,
    struct json_value *value
);
static int apply_json_remove_path(struct json_value *document, const struct json_set_path *path);
static int apply_json_remove_member_leg(
    struct json_value *target,
    const struct json_set_path_leg *leg
);
static int apply_json_remove_array_leg(struct json_value *target, size_t index);
static struct json_value *remove_path_array_index_value_mutable(
    struct json_value *value,
    size_t index
);

int mylite_json_internal_apply_mutation_path(
    struct json_value *document,
    const struct json_set_path *path,
    struct json_value *value,
    enum json_mutation_mode mode
) {
    switch (mode) {
    case JSON_MUTATION_SET:
        return apply_json_set_path(document, path, value);
    case JSON_MUTATION_REPLACE:
        return apply_json_replace_path(document, path, value);
    case JSON_MUTATION_INSERT:
        return apply_json_insert_path(document, path, value);
    case JSON_MUTATION_ARRAY_APPEND:
        return apply_json_array_append_path(document, path, value);
    case JSON_MUTATION_ARRAY_INSERT:
        return apply_json_array_insert_path(document, path, value);
    case JSON_MUTATION_REMOVE:
        return apply_json_remove_path(document, path);
    }

    return MYLITE_MISUSE;
}

static int apply_json_set_path(
    struct json_value *document,
    const struct json_set_path *path,
    struct json_value *value
) {
    struct json_value *target = document;

    if (document == NULL || path == NULL || value == NULL) {
        return MYLITE_MISUSE;
    }
    if (path->count == 0U) {
        mylite_json_internal_value_deinit(document);
        *document = *value;
        *value = (struct json_value){0};
        return MYLITE_OK;
    }

    for (size_t leg_index = 0U; leg_index + 1U < path->count; ++leg_index) {
        const struct json_set_path_leg *leg = &path->legs[leg_index];

        if (leg->kind == JSON_SET_PATH_MEMBER) {
            target = mylite_json_internal_object_member_value_mutable(
                target,
                leg->member,
                leg->member_length
            );
        } else {
            target = mylite_json_internal_array_index_value_mutable(target, leg->index);
        }
        if (target == NULL) {
            return MYLITE_OK;
        }
    }

    const struct json_set_path_leg *last = &path->legs[path->count - 1U];

    if (last->kind == JSON_SET_PATH_MEMBER) {
        return apply_json_set_member_leg(target, last, value);
    }
    return apply_json_set_array_leg(target, last->index, value);
}

static int apply_json_set_member_leg(
    struct json_value *target,
    const struct json_set_path_leg *leg,
    struct json_value *value
) {
    char *member = NULL;
    size_t member_length = 0U;
    struct json_value *stored_value = NULL;
    int rc = MYLITE_OK;

    if (target == NULL || leg == NULL || value == NULL) {
        return MYLITE_MISUSE;
    }
    if (target->kind != JSON_VALUE_OBJECT) {
        return MYLITE_OK;
    }
    rc = mylite_json_internal_copy_result_text(
        leg->member,
        leg->member_length,
        &member,
        &member_length
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_json_internal_object_append_member(
        &target->payload.object,
        member,
        member_length,
        value,
        &stored_value
    );
    if (rc == MYLITE_OK) {
        member = NULL;
        mylite_json_internal_sort_object_members_by_mysql_display_order(&target->payload.object);
    }

    free(member);
    return rc;
}

static int apply_json_set_array_leg(
    struct json_value *target,
    size_t index,
    struct json_value *value
) {
    struct json_value *stored_value = NULL;
    int rc = MYLITE_OK;

    if (target == NULL || value == NULL) {
        return MYLITE_MISUSE;
    }
    if (target->kind == JSON_VALUE_ARRAY) {
        if (index < target->payload.array.count) {
            mylite_json_internal_value_deinit(&target->payload.array.values[index]);
            target->payload.array.values[index] = *value;
            *value = (struct json_value){0};
            return MYLITE_OK;
        }
        return mylite_json_internal_array_append_value(
            &target->payload.array,
            value,
            &stored_value
        );
    }
    if (index == 0U) {
        mylite_json_internal_value_deinit(target);
        *target = *value;
        *value = (struct json_value){0};
        return MYLITE_OK;
    }

    struct json_value array = {.kind = JSON_VALUE_ARRAY};

    rc = mylite_json_internal_array_reserve_values(&array.payload.array, 2U);
    if (rc != MYLITE_OK) {
        return rc;
    }
    array.payload.array.values[0] = *target;
    array.payload.array.values[1] = *value;
    array.payload.array.count = 2U;
    *target = array;
    *value = (struct json_value){0};
    return MYLITE_OK;
}

static int apply_json_insert_path(
    struct json_value *document,
    const struct json_set_path *path,
    struct json_value *value
) {
    struct json_value *target = document;

    if (document == NULL || path == NULL || value == NULL) {
        return MYLITE_MISUSE;
    }
    if (path->count == 0U) {
        return MYLITE_OK;
    }

    for (size_t leg_index = 0U; leg_index + 1U < path->count; ++leg_index) {
        const struct json_set_path_leg *leg = &path->legs[leg_index];

        if (leg->kind == JSON_SET_PATH_MEMBER) {
            target = mylite_json_internal_object_member_value_mutable(
                target,
                leg->member,
                leg->member_length
            );
        } else {
            target = mylite_json_internal_array_index_value_mutable(target, leg->index);
        }
        if (target == NULL) {
            return MYLITE_OK;
        }
    }

    const struct json_set_path_leg *last = &path->legs[path->count - 1U];

    if (last->kind == JSON_SET_PATH_MEMBER) {
        return apply_json_insert_member_leg(target, last, value);
    }
    return apply_json_insert_array_leg(target, last->index, value);
}

static int apply_json_insert_member_leg(
    struct json_value *target,
    const struct json_set_path_leg *leg,
    struct json_value *value
) {
    char *member = NULL;
    size_t member_length = 0U;
    struct json_value *stored_value = NULL;
    int rc = MYLITE_OK;

    if (target == NULL || leg == NULL || value == NULL) {
        return MYLITE_MISUSE;
    }
    if (target->kind != JSON_VALUE_OBJECT ||
        mylite_json_internal_object_member_value_mutable(target, leg->member, leg->member_length) !=
            NULL) {
        return MYLITE_OK;
    }
    rc = mylite_json_internal_copy_result_text(
        leg->member,
        leg->member_length,
        &member,
        &member_length
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_json_internal_object_append_member(
        &target->payload.object,
        member,
        member_length,
        value,
        &stored_value
    );
    if (rc == MYLITE_OK) {
        member = NULL;
        mylite_json_internal_sort_object_members_by_mysql_display_order(&target->payload.object);
    }

    free(member);
    return rc;
}

static int apply_json_insert_array_leg(
    struct json_value *target,
    size_t index,
    struct json_value *value
) {
    struct json_value *stored_value = NULL;
    int rc = MYLITE_OK;

    if (target == NULL || value == NULL) {
        return MYLITE_MISUSE;
    }
    if (target->kind == JSON_VALUE_ARRAY) {
        if (index < target->payload.array.count) {
            return MYLITE_OK;
        }
        return mylite_json_internal_array_append_value(
            &target->payload.array,
            value,
            &stored_value
        );
    }
    if (index == 0U) {
        return MYLITE_OK;
    }

    struct json_value array = {.kind = JSON_VALUE_ARRAY};

    rc = mylite_json_internal_array_reserve_values(&array.payload.array, 2U);
    if (rc != MYLITE_OK) {
        return rc;
    }
    array.payload.array.values[0] = *target;
    array.payload.array.values[1] = *value;
    array.payload.array.count = 2U;
    *target = array;
    *value = (struct json_value){0};
    return MYLITE_OK;
}

static int apply_json_array_append_path(
    struct json_value *document,
    const struct json_set_path *path,
    struct json_value *value
) {
    struct json_value *target = document;

    if (document == NULL || path == NULL || value == NULL) {
        return MYLITE_MISUSE;
    }

    for (size_t leg_index = 0U; leg_index < path->count; ++leg_index) {
        const struct json_set_path_leg *leg = &path->legs[leg_index];

        if (leg->kind == JSON_SET_PATH_MEMBER) {
            target = mylite_json_internal_object_member_value_mutable(
                target,
                leg->member,
                leg->member_length
            );
        } else if (target->kind == JSON_VALUE_ARRAY) {
            target = mylite_json_internal_array_index_value_mutable(target, leg->index);
        } else if (leg->index != 0U) {
            return MYLITE_OK;
        }
        if (target == NULL) {
            return MYLITE_OK;
        }
    }

    return append_json_array_append_value(target, value);
}

static int append_json_array_append_value(struct json_value *target, struct json_value *value) {
    struct json_value *stored_value = NULL;
    int rc = MYLITE_OK;

    if (target == NULL || value == NULL) {
        return MYLITE_MISUSE;
    }
    if (target->kind == JSON_VALUE_ARRAY) {
        return mylite_json_internal_array_append_value(
            &target->payload.array,
            value,
            &stored_value
        );
    }

    struct json_value array = {.kind = JSON_VALUE_ARRAY};

    rc = mylite_json_internal_array_reserve_values(&array.payload.array, 2U);
    if (rc != MYLITE_OK) {
        return rc;
    }
    array.payload.array.values[0] = *target;
    array.payload.array.values[1] = *value;
    array.payload.array.count = 2U;
    *target = array;
    *value = (struct json_value){0};
    return MYLITE_OK;
}

static int apply_json_array_insert_path(
    struct json_value *document,
    const struct json_set_path *path,
    struct json_value *value
) {
    struct json_value *target = document;

    if (document == NULL || path == NULL || value == NULL) {
        return MYLITE_MISUSE;
    }
    if (path->count == 0U) {
        return MYLITE_MISUSE;
    }

    for (size_t leg_index = 0U; leg_index + 1U < path->count; ++leg_index) {
        const struct json_set_path_leg *leg = &path->legs[leg_index];

        if (leg->kind == JSON_SET_PATH_MEMBER) {
            target = mylite_json_internal_object_member_value_mutable(
                target,
                leg->member,
                leg->member_length
            );
        } else {
            target = mylite_json_internal_array_index_value_mutable(target, leg->index);
        }
        if (target == NULL) {
            return MYLITE_OK;
        }
    }

    const struct json_set_path_leg *last = &path->legs[path->count - 1U];

    if (last->kind != JSON_SET_PATH_ARRAY) {
        return MYLITE_MISUSE;
    }
    return insert_json_array_value(target, last->index, value);
}

static int insert_json_array_value(
    struct json_value *target,
    size_t index,
    struct json_value *value
) {
    struct json_value *stored_value = NULL;
    struct json_array *array = NULL;
    int rc = MYLITE_OK;

    if (target == NULL || value == NULL) {
        return MYLITE_MISUSE;
    }
    if (target->kind != JSON_VALUE_ARRAY) {
        return MYLITE_OK;
    }
    array = &target->payload.array;
    if (index >= array->count) {
        return mylite_json_internal_array_append_value(array, value, &stored_value);
    }

    rc = mylite_json_internal_array_reserve_values(array, array->count + 1U);
    if (rc != MYLITE_OK) {
        return rc;
    }
    memmove(
        &array->values[index + 1U],
        &array->values[index],
        (array->count - index) * sizeof(*array->values)
    );
    array->values[index] = *value;
    *value = (struct json_value){0};
    ++array->count;
    return MYLITE_OK;
}

static int apply_json_replace_path(
    struct json_value *document,
    const struct json_set_path *path,
    struct json_value *value
) {
    struct json_value *target = document;

    if (document == NULL || path == NULL || value == NULL) {
        return MYLITE_MISUSE;
    }
    if (path->count == 0U) {
        mylite_json_internal_value_deinit(document);
        *document = *value;
        *value = (struct json_value){0};
        return MYLITE_OK;
    }

    for (size_t leg_index = 0U; leg_index + 1U < path->count; ++leg_index) {
        const struct json_set_path_leg *leg = &path->legs[leg_index];

        if (leg->kind == JSON_SET_PATH_MEMBER) {
            target = mylite_json_internal_object_member_value_mutable(
                target,
                leg->member,
                leg->member_length
            );
        } else {
            target = mylite_json_internal_array_index_value_mutable(target, leg->index);
        }
        if (target == NULL) {
            return MYLITE_OK;
        }
    }

    const struct json_set_path_leg *last = &path->legs[path->count - 1U];

    if (last->kind == JSON_SET_PATH_MEMBER) {
        return apply_json_replace_member_leg(target, last, value);
    }
    return apply_json_replace_array_leg(target, last->index, value);
}

static int apply_json_replace_member_leg(
    struct json_value *target,
    const struct json_set_path_leg *leg,
    struct json_value *value
) {
    struct json_value *stored_value = NULL;

    if (target == NULL || leg == NULL || value == NULL) {
        return MYLITE_MISUSE;
    }
    if (target->kind != JSON_VALUE_OBJECT) {
        return MYLITE_OK;
    }
    stored_value =
        mylite_json_internal_object_member_value_mutable(target, leg->member, leg->member_length);
    if (stored_value == NULL) {
        return MYLITE_OK;
    }
    mylite_json_internal_value_deinit(stored_value);
    *stored_value = *value;
    *value = (struct json_value){0};
    return MYLITE_OK;
}

static int apply_json_replace_array_leg(
    struct json_value *target,
    size_t index,
    struct json_value *value
) {
    if (target == NULL || value == NULL) {
        return MYLITE_MISUSE;
    }
    if (target->kind == JSON_VALUE_ARRAY) {
        if (index >= target->payload.array.count) {
            return MYLITE_OK;
        }
        mylite_json_internal_value_deinit(&target->payload.array.values[index]);
        target->payload.array.values[index] = *value;
        *value = (struct json_value){0};
        return MYLITE_OK;
    }
    if (index == 0U) {
        mylite_json_internal_value_deinit(target);
        *target = *value;
        *value = (struct json_value){0};
    }
    return MYLITE_OK;
}

static int apply_json_remove_path(struct json_value *document, const struct json_set_path *path) {
    struct json_value *target = document;

    if (document == NULL || path == NULL || path->count == 0U) {
        return MYLITE_MISUSE;
    }

    for (size_t leg_index = 0U; leg_index + 1U < path->count; ++leg_index) {
        const struct json_set_path_leg *leg = &path->legs[leg_index];

        if (leg->kind == JSON_SET_PATH_MEMBER) {
            target = mylite_json_internal_object_member_value_mutable(
                target,
                leg->member,
                leg->member_length
            );
        } else {
            target = remove_path_array_index_value_mutable(target, leg->index);
        }
        if (target == NULL) {
            return MYLITE_OK;
        }
    }

    const struct json_set_path_leg *last = &path->legs[path->count - 1U];

    if (last->kind == JSON_SET_PATH_MEMBER) {
        return apply_json_remove_member_leg(target, last);
    }
    return apply_json_remove_array_leg(target, last->index);
}

static int apply_json_remove_member_leg(
    struct json_value *target,
    const struct json_set_path_leg *leg
) {
    if (target == NULL || leg == NULL) {
        return MYLITE_MISUSE;
    }
    if (target->kind != JSON_VALUE_OBJECT) {
        return MYLITE_OK;
    }
    for (size_t index = 0U; index < target->payload.object.count; ++index) {
        struct json_member *member = &target->payload.object.members[index];

        if (member->key_length == leg->member_length &&
            memcmp(member->key, leg->member, leg->member_length) == 0) {
            mylite_json_internal_value_deinit(member->value);
            free(member->value);
            free(member->key);
            for (size_t move_index = index + 1U; move_index < target->payload.object.count;
                 ++move_index) {
                target->payload.object.members[move_index - 1U] =
                    target->payload.object.members[move_index];
            }
            --target->payload.object.count;
            target->payload.object.members[target->payload.object.count] = (struct json_member){0};
            return MYLITE_OK;
        }
    }
    return MYLITE_OK;
}

static int apply_json_remove_array_leg(struct json_value *target, size_t index) {
    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (target->kind != JSON_VALUE_ARRAY || index >= target->payload.array.count) {
        return MYLITE_OK;
    }
    mylite_json_internal_value_deinit(&target->payload.array.values[index]);
    for (size_t move_index = index + 1U; move_index < target->payload.array.count; ++move_index) {
        target->payload.array.values[move_index - 1U] = target->payload.array.values[move_index];
    }
    --target->payload.array.count;
    target->payload.array.values[target->payload.array.count] = (struct json_value){0};
    return MYLITE_OK;
}

static struct json_value *remove_path_array_index_value_mutable(
    struct json_value *value,
    size_t index
) {
    if (value == NULL) {
        return NULL;
    }
    if (value->kind == JSON_VALUE_ARRAY) {
        return mylite_json_internal_array_index_value_mutable(value, index);
    }
    return index == 0U ? value : NULL;
}
