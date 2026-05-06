#ifndef MYLITE_RUNTIME_MYLITE_SELECT_DIAGNOSTICS_H
#define MYLITE_RUNTIME_MYLITE_SELECT_DIAGNOSTICS_H

#include <mylite/mylite.h>

int mylite_select_set_invalid_group_function_error(mylite_db *database);
int mylite_select_set_duplicate_mode_error(mylite_db *database);
int mylite_select_set_unsupported_window_error(mylite_db *database);
int mylite_select_set_unsupported_projection_error(mylite_db *database);
int mylite_select_set_unsupported_where_error(mylite_db *database);
int mylite_select_set_where_predicate_eval_error(mylite_stmt *stmt);
int mylite_select_set_unsupported_order_error(mylite_db *database);
int mylite_select_set_unsupported_join_grouping_error(mylite_db *database);

#endif
