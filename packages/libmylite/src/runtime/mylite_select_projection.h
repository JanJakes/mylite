#ifndef MYLITE_RUNTIME_MYLITE_SELECT_PROJECTION_H
#define MYLITE_RUNTIME_MYLITE_SELECT_PROJECTION_H

#include <mylite/mylite.h>

#include "mylite_select_types.h"

#include <stdbool.h>

struct mylite_select_projection_callbacks {
    int (*bind_expression)(
        mylite_db *database,
        const struct mylite_sql_ast_node *expression,
        struct mylite_select_plan *plan
    );
    int (*set_unsupported_projection_error)(mylite_db *database);
};

int mylite_select_build_outputs(
    mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    bool allow_expression_outputs,
    struct mylite_select_plan *plan,
    const struct mylite_select_projection_callbacks *callbacks
);
int mylite_select_append_wildcard_outputs(
    mylite_db *database,
    const struct mylite_sql_ast_node *wildcard,
    struct mylite_select_plan *plan
);

#endif
