#ifndef MYLITE_RUNTIME_MYLITE_SELECT_METADATA_H
#define MYLITE_RUNTIME_MYLITE_SELECT_METADATA_H

#include <mylite/mylite.h>

#include "mylite_expression.h"
#include "mylite_field_descriptor.h"
#include "mylite_select_types.h"

struct mylite_select_metadata_callbacks {
    int (*infer_expression_descriptor)(mylite_db *database, const struct mylite_select_plan *plan,
                                       const struct mylite_sql_ast_node *expression,
                                       struct mylite_field_descriptor *out_descriptor);
};

int mylite_select_attach_result_metadata(mylite_stmt *stmt, const struct mylite_select_plan *plan,
                                         const struct mylite_select_metadata_callbacks *callbacks);

#endif
