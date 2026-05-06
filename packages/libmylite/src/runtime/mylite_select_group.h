#ifndef MYLITE_RUNTIME_MYLITE_SELECT_GROUP_H
#define MYLITE_RUNTIME_MYLITE_SELECT_GROUP_H

#include <mylite/mylite.h>

#include "mylite_select_eval.h"
#include "mylite_select_types.h"

#include <stddef.h>

int mylite_select_group_append(
    mylite_stmt *stmt,
    struct mylite_table_select_group **groups,
    size_t *group_count,
    const struct mylite_table_select_row *row,
    const struct mylite_select_eval_callbacks *callbacks,
    struct mylite_table_select_group **out_group
);
int mylite_select_group_find(
    mylite_stmt *stmt,
    struct mylite_table_select_group *groups,
    size_t group_count,
    const struct mylite_table_select_row *row,
    const struct mylite_select_eval_callbacks *callbacks,
    struct mylite_table_select_group **out_group
);
int mylite_select_group_update(
    mylite_stmt *stmt,
    struct mylite_table_select_group *group,
    const struct mylite_table_select_row *row,
    const struct mylite_select_eval_callbacks *callbacks
);
int mylite_select_group_finalize(
    mylite_stmt *stmt,
    const struct mylite_table_select_group *group,
    struct mylite_table_select_row *out_row
);
int mylite_select_group_append_empty_implicit(
    mylite_stmt *stmt,
    struct mylite_table_select_group **groups,
    size_t *group_count
);
void mylite_select_groups_deinit(struct mylite_table_select_group *groups, size_t group_count);
void mylite_select_group_deinit(struct mylite_table_select_group *group);

#endif
