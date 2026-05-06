#ifndef MYLITE_RUNTIME_MYLITE_SHOW_INDEX_TARGET_H
#define MYLITE_RUNTIME_MYLITE_SHOW_INDEX_TARGET_H

#include "mylite_show_types.h"

#include <mylite/mylite.h>

int mylite_show_index_copy_target(
    mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct mylite_show_index_target *out_target
);
int mylite_show_index_validate_target(mylite_db *database, struct mylite_show_index_target *target);
void mylite_show_index_target_deinit(struct mylite_show_index_target *target);

#endif
