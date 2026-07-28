#include "mylite_parser_resources.h"

#include "mylite_parser.h"

#include <stdint.h>

static size_t retry_workspace_limit_for_input(size_t input_length);
static void add_saturated(size_t *value, size_t increment);

void mylite_sql_parser_resource_tracker_init(
    struct mylite_sql_parser_resource_tracker *tracker,
    size_t input_length
) {
    if (tracker == NULL) {
        return;
    }
    *tracker = (struct mylite_sql_parser_resource_tracker){
        .retry_workspace_limit = retry_workspace_limit_for_input(input_length),
    };
}

bool mylite_sql_parser_resource_record_lexer_pass(struct mylite_sql_parser_resource_tracker *tracker
) {
    if (tracker == NULL) {
        return true;
    }
    if (tracker->lexer_pass_count >= (size_t)mylite_sql_parser_lexer_pass_limit) {
        tracker->retry_budget_exhausted = true;
        return false;
    }
    ++tracker->lexer_pass_count;
    return true;
}

bool mylite_sql_parser_resource_record_retry_token(
    struct mylite_sql_parser_resource_tracker *tracker
) {
    if (tracker == NULL) {
        return true;
    }
    if (tracker->retry_token_count >= (size_t)mylite_sql_parser_retry_token_limit) {
        tracker->retry_budget_exhausted = true;
        return false;
    }
    ++tracker->retry_token_count;
    return true;
}

bool mylite_sql_parser_resource_workspace_fits(
    struct mylite_sql_parser_resource_tracker *tracker,
    size_t old_bytes,
    size_t new_bytes
) {
    size_t retained_bytes = 0U;

    if (tracker == NULL) {
        return true;
    }
    if (old_bytes > tracker->retry_workspace_bytes) {
        tracker->retry_budget_exhausted = true;
        return false;
    }
    retained_bytes = tracker->retry_workspace_bytes - old_bytes;
    if (retained_bytes > tracker->retry_workspace_limit) {
        tracker->retry_budget_exhausted = true;
        return false;
    }
    if (new_bytes > tracker->retry_workspace_limit - retained_bytes) {
        tracker->retry_budget_exhausted = true;
        return false;
    }
    return true;
}

void mylite_sql_parser_resource_record_workspace(
    struct mylite_sql_parser_resource_tracker *tracker,
    size_t old_bytes,
    size_t new_bytes
) {
    size_t retained_bytes = 0U;

    if (tracker == NULL) {
        return;
    }
    if (old_bytes > tracker->retry_workspace_bytes) {
        tracker->retry_budget_exhausted = true;
        return;
    }
    retained_bytes = tracker->retry_workspace_bytes - old_bytes;
    if (retained_bytes > tracker->retry_workspace_limit ||
        new_bytes > tracker->retry_workspace_limit - retained_bytes) {
        tracker->retry_budget_exhausted = true;
        return;
    }
    tracker->retry_workspace_bytes = retained_bytes + new_bytes;
    if (tracker->retry_workspace_bytes > tracker->retry_workspace_peak_bytes) {
        tracker->retry_workspace_peak_bytes = tracker->retry_workspace_bytes;
    }
    ++tracker->retry_allocation_count;
    add_saturated(&tracker->retry_allocation_bytes, new_bytes);
}

void mylite_sql_parser_resource_release_workspace(
    struct mylite_sql_parser_resource_tracker *tracker,
    size_t bytes
) {
    if (tracker == NULL) {
        return;
    }
    if (bytes > tracker->retry_workspace_bytes) {
        tracker->retry_workspace_bytes = 0U;
        return;
    }
    tracker->retry_workspace_bytes -= bytes;
}

void mylite_sql_parser_resource_exhaust(struct mylite_sql_parser_resource_tracker *tracker) {
    if (tracker != NULL) {
        tracker->retry_budget_exhausted = true;
    }
}

void mylite_sql_parser_resource_publish(
    const struct mylite_sql_parser_resource_tracker *tracker,
    struct mylite_sql_parse_result *result
) {
    if (tracker == NULL || result == NULL) {
        return;
    }
    result->lexer_pass_count = tracker->lexer_pass_count;
    result->retry_token_count = tracker->retry_token_count;
    result->retry_allocation_count = tracker->retry_allocation_count;
    result->retry_allocation_bytes = tracker->retry_allocation_bytes;
    result->retry_workspace_peak_bytes = tracker->retry_workspace_peak_bytes;
    result->retry_budget_exhausted = tracker->retry_budget_exhausted;
}

static size_t retry_workspace_limit_for_input(size_t input_length) {
    size_t limit = mylite_sql_parser_retry_workspace_limit;

    if (input_length <= SIZE_MAX / (size_t)mylite_sql_parser_retry_workspace_input_multiplier) {
        limit = input_length * (size_t)mylite_sql_parser_retry_workspace_input_multiplier;
        if (limit < (size_t)mylite_sql_parser_retry_workspace_minimum) {
            limit = mylite_sql_parser_retry_workspace_minimum;
        }
        if (limit > (size_t)mylite_sql_parser_retry_workspace_limit) {
            limit = mylite_sql_parser_retry_workspace_limit;
        }
    }
    return limit;
}

static void add_saturated(size_t *value, size_t increment) {
    if (value == NULL) {
        return;
    }
    if (increment > SIZE_MAX - *value) {
        *value = SIZE_MAX;
        return;
    }
    *value += increment;
}
