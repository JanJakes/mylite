#ifndef MYLITE_RUNTIME_MYLITE_SELECT_MATERIALIZE_H
#define MYLITE_RUNTIME_MYLITE_SELECT_MATERIALIZE_H

#include <mylite/mylite.h>

#include "mylite_select_eval.h"

int mylite_select_materialize_table_result(mylite_stmt *stmt,
                                           const struct mylite_select_eval_callbacks *callbacks);

#endif
