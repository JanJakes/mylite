#include <mylite/mylite.h>

#include "mylite_ast.h"
#include "mylite_execution_select_analysis.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

enum { select_parameter_context_stack_initial_capacity = 32 };

struct select_parameter_context_frame {
    const struct mylite_sql_ast_node *node;
    enum mylite_sql_ast_node_kind parent_kind;
    enum mylite_sql_ast_node_kind grandparent_kind;
    size_t child_index;
};

struct select_parameter_context_stack {
    struct select_parameter_context_frame *items;
    size_t count;
    size_t capacity;
};

static bool select_parameter_context_is_reusable(const struct select_parameter_context_frame *frame
);
static bool select_parameter_context_stack_push(
    struct select_parameter_context_stack *stack,
    const struct select_parameter_context_frame *frame
);
static void select_parameter_context_stack_deinit(struct select_parameter_context_stack *stack);

int mylite_execution_select_parameters_are_plan_reusable(
    const struct mylite_sql_ast_node *statement,
    bool *out_reusable
) {
    struct select_parameter_context_stack stack = {0};
    const struct select_parameter_context_frame root = {
        .node = statement,
        .parent_kind = MYLITE_SQL_AST_SCRIPT,
        .grandparent_kind = MYLITE_SQL_AST_SCRIPT,
        .child_index = SIZE_MAX,
    };
    int rc = MYLITE_OK;

    if (statement == NULL || out_reusable == NULL) {
        return MYLITE_MISUSE;
    }
    *out_reusable = true;
    if (!select_parameter_context_stack_push(&stack, &root)) {
        return MYLITE_NOMEM;
    }
    while (stack.count != 0U && *out_reusable) {
        struct select_parameter_context_frame frame = stack.items[--stack.count];
        const struct mylite_sql_ast_node *child = NULL;
        size_t child_index = 0U;

        if (frame.node->kind == MYLITE_SQL_AST_PARAMETER) {
            *out_reusable = select_parameter_context_is_reusable(&frame);
            continue;
        }
        for (child = frame.node->first_child; child != NULL; child = child->next_sibling) {
            struct select_parameter_context_frame child_frame = {
                .node = child,
                .parent_kind = frame.node->kind,
                .grandparent_kind = frame.parent_kind,
                .child_index = child_index,
            };

            if (frame.node->kind == MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION) {
                child_frame.parent_kind = frame.parent_kind;
                child_frame.grandparent_kind = frame.grandparent_kind;
                child_frame.child_index = frame.child_index;
            }
            if (!select_parameter_context_stack_push(&stack, &child_frame)) {
                rc = MYLITE_NOMEM;
                break;
            }
            ++child_index;
        }
        if (rc != MYLITE_OK) {
            break;
        }
    }
    select_parameter_context_stack_deinit(&stack);
    return rc;
}

bool mylite_execution_select_analysis_matches(
    const struct mylite_select_analysis_state *analysis,
    const struct mylite_stmt_binding *bindings,
    size_t binding_count,
    struct mylite_select_analysis_session_key session_key,
    struct mylite_select_analysis_generation_key generation_key
) {
    if (analysis == NULL || !analysis->valid || !analysis->parameter_values_are_reusable ||
        analysis->binding_type_count != binding_count ||
        (binding_count != 0U && analysis->binding_types == NULL) ||
        (binding_count != 0U && bindings == NULL) ||
        analysis->generation_key.catalog != generation_key.catalog ||
        analysis->generation_key.sqlite_schema != generation_key.sqlite_schema ||
        analysis->session_key.sql_mode != session_key.sql_mode ||
        analysis->session_key.last_insert_id != session_key.last_insert_id ||
        analysis->session_key.time_zone_offset_minutes != session_key.time_zone_offset_minutes ||
        analysis->session_key.sql_auto_is_null != session_key.sql_auto_is_null) {
        return false;
    }

    for (size_t index = 0U; index < binding_count; ++index) {
        if (analysis->binding_types[index] != bindings[index].type) {
            return false;
        }
    }
    return true;
}

int mylite_execution_select_analysis_capture(
    struct mylite_select_analysis_state *analysis,
    const struct mylite_stmt_binding *bindings,
    size_t binding_count,
    struct mylite_select_analysis_session_key session_key,
    struct mylite_select_analysis_generation_key generation_key,
    bool parameter_values_are_reusable
) {
    enum mylite_stmt_binding_type *binding_types = NULL;

    if (analysis == NULL || analysis->valid || analysis->lowered_sql != NULL ||
        analysis->binding_types != NULL || analysis->binding_type_count != 0U ||
        (binding_count != 0U && bindings == NULL)) {
        return MYLITE_MISUSE;
    }
    if (binding_count > SIZE_MAX / sizeof(*binding_types)) {
        return MYLITE_NOMEM;
    }
    if (binding_count != 0U) {
        binding_types = malloc(binding_count * sizeof(*binding_types));
        if (binding_types == NULL) {
            return MYLITE_NOMEM;
        }
        for (size_t index = 0U; index < binding_count; ++index) {
            binding_types[index] = bindings[index].type;
        }
    }

    analysis->binding_types = binding_types;
    analysis->binding_type_count = binding_count;
    analysis->session_key = session_key;
    analysis->generation_key = generation_key;
    analysis->parameter_values_are_reusable = parameter_values_are_reusable;
    analysis->valid = true;
    return MYLITE_OK;
}

void mylite_execution_select_analysis_deinit(struct mylite_select_analysis_state *analysis) {
    if (analysis == NULL) {
        return;
    }

    free(analysis->lowered_sql);
    free(analysis->binding_types);
    *analysis = (struct mylite_select_analysis_state){0};
}

static bool select_parameter_context_is_reusable(const struct select_parameter_context_frame *frame
) {
    if (frame == NULL) {
        return false;
    }
    if (frame->parent_kind == MYLITE_SQL_AST_COMPARISON_PREDICATE) {
        return frame->child_index == 1U;
    }
    if (frame->parent_kind == MYLITE_SQL_AST_BETWEEN_PREDICATE) {
        return frame->child_index == 1U || frame->child_index == 2U;
    }
    return frame->parent_kind == MYLITE_SQL_AST_PREDICATE_VALUE_LIST &&
           frame->grandparent_kind == MYLITE_SQL_AST_IN_PREDICATE;
}

static bool select_parameter_context_stack_push(
    struct select_parameter_context_stack *stack,
    const struct select_parameter_context_frame *frame
) {
    struct select_parameter_context_frame *items = NULL;
    size_t capacity = 0U;

    if (stack == NULL || frame == NULL) {
        return false;
    }
    if (stack->count == stack->capacity) {
        capacity = stack->capacity == 0U ? select_parameter_context_stack_initial_capacity
                                         : stack->capacity * 2U;
        if (capacity < stack->capacity || capacity > SIZE_MAX / sizeof(*items)) {
            return false;
        }
        items = realloc(stack->items, capacity * sizeof(*items));
        if (items == NULL) {
            return false;
        }
        stack->items = items;
        stack->capacity = capacity;
    }
    stack->items[stack->count++] = *frame;
    return true;
}

static void select_parameter_context_stack_deinit(struct select_parameter_context_stack *stack) {
    if (stack == NULL) {
        return;
    }
    free(stack->items);
    *stack = (struct select_parameter_context_stack){0};
}
