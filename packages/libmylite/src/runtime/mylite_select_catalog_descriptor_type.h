#ifndef MYLITE_RUNTIME_MYLITE_SELECT_CATALOG_DESCRIPTOR_TYPE_H
#define MYLITE_RUNTIME_MYLITE_SELECT_CATALOG_DESCRIPTOR_TYPE_H

#include <mylite/mylite.h>

#include "mylite_field_descriptor.h"

struct mylite_catalog_column_descriptor_source;

int mylite_select_catalog_apply_column_type_descriptor(
    mylite_db *database, const struct mylite_catalog_column_descriptor_source *source,
    struct mylite_field_descriptor *descriptor);

#endif
