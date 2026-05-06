#ifndef MYLITE_RUNTIME_MYLITE_SELECT_PROJECTION_WILDCARD_SEQUENCE_H
#define MYLITE_RUNTIME_MYLITE_SELECT_PROJECTION_WILDCARD_SEQUENCE_H

#include <mylite/mylite.h>

#include "mylite_select_types.h"

int mylite_select_build_wildcard_column_sequence(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    struct mylite_select_table_range range,
    struct mylite_select_column_sequence *out_sequence
);

#endif
