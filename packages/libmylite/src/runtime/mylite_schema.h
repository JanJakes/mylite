#ifndef MYLITE_RUNTIME_MYLITE_SCHEMA_H
#define MYLITE_RUNTIME_MYLITE_SCHEMA_H

#include <mylite/mylite.h>

#include "mylite_schema_types.h"

void mylite_schema_options_deinit(struct mylite_schema_options *options);
int mylite_schema_normalize_options(mylite_db *database, struct mylite_schema_options *options);

#endif
