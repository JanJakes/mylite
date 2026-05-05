#ifndef MYLITE_RUNTIME_MYLITE_FUNCTION_NAME_MATCH_H
#define MYLITE_RUNTIME_MYLITE_FUNCTION_NAME_MATCH_H

#include "mylite_span.h"
#include "sql/mylite_ast.h"

#include <stdbool.h>
#include <stddef.h>

static inline bool mylite_function_name_matches_any(const struct mylite_sql_ast_node *name,
                                                    const char *const *candidates,
                                                    size_t candidate_count)
{
    if (name == NULL) {
        return false;
    }
    for (size_t index = 0U; index < candidate_count; ++index) {
        if (mylite_span_equal_ci(name->span, candidates[index])) {
            return true;
        }
    }
    return false;
}

#endif
