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
static bool json_scalar_values_equal(
    const struct json_value *target,
    const struct json_value *candidate
);

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
