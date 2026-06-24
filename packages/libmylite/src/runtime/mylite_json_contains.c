#include <mylite/mylite.h>

#include "mylite_json_internal.h"

#include <string.h>

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
static bool json_arrays_equal(const struct json_value *left, const struct json_value *right);
static bool json_objects_equal(const struct json_value *left, const struct json_value *right);
static bool json_array_overlaps_value(
    const struct json_value *array_value,
    const struct json_value *other
);
static bool json_objects_overlap(const struct json_value *left, const struct json_value *right);
static bool json_scalar_values_equal(const struct json_value *left, const struct json_value *right);

bool mylite_json_internal_value_contains(
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

bool mylite_json_internal_values_equal(
    const struct json_value *left,
    const struct json_value *right
) {
    if (left == NULL || right == NULL || left->kind != right->kind) {
        return false;
    }

    switch (left->kind) {
    case JSON_VALUE_NULL:
    case JSON_VALUE_BOOL:
    case JSON_VALUE_NUMBER:
    case JSON_VALUE_STRING:
        return json_scalar_values_equal(left, right);
    case JSON_VALUE_ARRAY:
        return json_arrays_equal(left, right);
    case JSON_VALUE_OBJECT:
        return json_objects_equal(left, right);
    }
    return false;
}

bool mylite_json_internal_values_overlap(
    const struct json_value *left,
    const struct json_value *right
) {
    if (left == NULL || right == NULL) {
        return false;
    }
    if (left->kind == JSON_VALUE_ARRAY) {
        return json_array_overlaps_value(left, right);
    }
    if (right->kind == JSON_VALUE_ARRAY) {
        return json_array_overlaps_value(right, left);
    }
    if (left->kind == JSON_VALUE_OBJECT && right->kind == JSON_VALUE_OBJECT) {
        return json_objects_overlap(left, right);
    }
    return mylite_json_internal_values_equal(left, right);
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
    target_member = mylite_json_internal_object_member_value(
        frame->target,
        candidate_member->key,
        candidate_member->key_length
    );
    if (target_member == NULL) {
        return json_contains_stack_complete(stack, false);
    }

    frame->waiting_child = true;
    return json_contains_stack_push(stack, target_member, candidate_member->value);
}

static bool json_arrays_equal(const struct json_value *left, const struct json_value *right) {
    if (left == NULL || right == NULL || left->kind != JSON_VALUE_ARRAY ||
        right->kind != JSON_VALUE_ARRAY ||
        left->payload.array.count != right->payload.array.count) {
        return false;
    }

    for (size_t index = 0U; index < left->payload.array.count; ++index) {
        if (!mylite_json_internal_values_equal(
                &left->payload.array.values[index],
                &right->payload.array.values[index]
            )) {
            return false;
        }
    }
    return true;
}

static bool json_objects_equal(const struct json_value *left, const struct json_value *right) {
    if (left == NULL || right == NULL || left->kind != JSON_VALUE_OBJECT ||
        right->kind != JSON_VALUE_OBJECT ||
        left->payload.object.count != right->payload.object.count) {
        return false;
    }

    for (size_t index = 0U; index < left->payload.object.count; ++index) {
        const struct json_member *member = &left->payload.object.members[index];
        const struct json_value *right_value =
            mylite_json_internal_object_member_value(right, member->key, member->key_length);

        if (right_value == NULL || !mylite_json_internal_values_equal(member->value, right_value)) {
            return false;
        }
    }
    return true;
}

static bool json_array_overlaps_value(
    const struct json_value *array_value,
    const struct json_value *other
) {
    if (array_value == NULL || array_value->kind != JSON_VALUE_ARRAY || other == NULL) {
        return false;
    }

    if (other->kind == JSON_VALUE_ARRAY) {
        for (size_t left_index = 0U; left_index < array_value->payload.array.count; ++left_index) {
            for (size_t right_index = 0U; right_index < other->payload.array.count; ++right_index) {
                if (mylite_json_internal_values_equal(
                        &array_value->payload.array.values[left_index],
                        &other->payload.array.values[right_index]
                    )) {
                    return true;
                }
            }
        }
        return false;
    }

    for (size_t index = 0U; index < array_value->payload.array.count; ++index) {
        if (mylite_json_internal_values_equal(&array_value->payload.array.values[index], other)) {
            return true;
        }
    }
    return false;
}

static bool json_objects_overlap(const struct json_value *left, const struct json_value *right) {
    if (left == NULL || right == NULL || left->kind != JSON_VALUE_OBJECT ||
        right->kind != JSON_VALUE_OBJECT) {
        return false;
    }

    for (size_t index = 0U; index < left->payload.object.count; ++index) {
        const struct json_member *member = &left->payload.object.members[index];
        const struct json_value *right_value =
            mylite_json_internal_object_member_value(right, member->key, member->key_length);

        if (right_value != NULL && mylite_json_internal_values_equal(member->value, right_value)) {
            return true;
        }
    }
    return false;
}

static bool json_scalar_values_equal(
    const struct json_value *left,
    const struct json_value *right
) {
    if (left == NULL || right == NULL || left->kind != right->kind) {
        return false;
    }

    switch (left->kind) {
    case JSON_VALUE_NULL:
        return true;
    case JSON_VALUE_BOOL:
        return left->payload.boolean == right->payload.boolean;
    case JSON_VALUE_NUMBER:
    case JSON_VALUE_STRING:
        return (left->payload.text.length == right->payload.text.length &&
                memcmp(
                    left->payload.text.text,
                    right->payload.text.text,
                    left->payload.text.length
                ) == 0) != 0;
    case JSON_VALUE_ARRAY:
    case JSON_VALUE_OBJECT:
        break;
    }
    return false;
}
