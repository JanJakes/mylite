#include "mylite_select.h"

#include "mylite_span.h"

#include <stdint.h>

bool mylite_select_parse_uint64_span(struct mylite_sql_source_span span, uint64_t *out_value) {
    enum { decimal_radix = 10U };

    uint64_t value = 0U;

    *out_value = 0U;
    if (span.text == NULL || span.length == 0U) {
        return false;
    }
    for (size_t index = 0U; index < span.length; ++index) {
        unsigned char byte = (unsigned char)span.text[index];
        uint64_t digit = 0U;

        if (byte < '0' || byte > '9') {
            return false;
        }
        digit = (uint64_t)(byte - '0');
        if (value > (UINT64_MAX - digit) / decimal_radix) {
            return false;
        }
        value = (value * decimal_radix) + digit;
    }
    *out_value = value;
    return true;
}

int mylite_select_bind_limit_clause(
    const struct mylite_sql_ast_node *limit_clause,
    struct mylite_select_plan *plan
) {
    const struct mylite_sql_ast_node *offset = mylite_ast_child_at(limit_clause, 0U);
    const struct mylite_sql_ast_node *row_count = mylite_ast_child_at(limit_clause, 1U);

    if (limit_clause == NULL || limit_clause->kind != MYLITE_SQL_AST_LIMIT_CLAUSE ||
        offset == NULL || offset->kind != MYLITE_SQL_AST_LIMIT_BOUND ||
        !offset->has_limit_bound_value || row_count == NULL ||
        row_count->kind != MYLITE_SQL_AST_LIMIT_BOUND || !row_count->has_limit_bound_value) {
        return MYLITE_UNSUPPORTED;
    }

    plan->limit = (struct mylite_select_limit){
        .offset = offset->limit_bound_value,
        .row_count = row_count->limit_bound_value,
        .has_limit = true,
    };
    return MYLITE_OK;
}

bool mylite_select_limit_row_is_kept(
    const struct mylite_select_limit *limit,
    struct mylite_select_limit_position position
) {
    if (!limit->has_limit) {
        return true;
    }
    if (position.matched_row < limit->offset) {
        return false;
    }
    if (mylite_select_limit_is_full(limit, position.kept_count)) {
        return false;
    }
    return true;
}

bool mylite_select_limit_is_full(const struct mylite_select_limit *limit, size_t kept_count) {
    if (!limit->has_limit) {
        return false;
    }
    if (limit->row_count > (uint64_t)SIZE_MAX) {
        return false;
    }
    return kept_count >= (size_t)limit->row_count;
}
