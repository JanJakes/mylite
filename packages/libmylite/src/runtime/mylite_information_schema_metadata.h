#ifndef MYLITE_RUNTIME_MYLITE_INFORMATION_SCHEMA_METADATA_H
#define MYLITE_RUNTIME_MYLITE_INFORMATION_SCHEMA_METADATA_H

#include <mylite/mylite.h>

#include "mylite_field_descriptor.h"
#include "mylite_information_schema_target.h"

int mylite_information_schema_attach_result_metadata(
    mylite_db *database,
    enum mylite_information_schema_table table,
    mylite_stmt *stmt
);
struct mylite_field_descriptor mylite_information_schema_column_descriptor(
    enum mylite_information_schema_table table,
    const char *name
);

#endif
