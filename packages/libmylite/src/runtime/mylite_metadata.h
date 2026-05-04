#ifndef MYLITE_RUNTIME_MYLITE_METADATA_H
#define MYLITE_RUNTIME_MYLITE_METADATA_H

#include "mylite_runtime.h"

const struct mylite_result_column_metadata *mylite_result_metadata_column(const mylite_stmt *stmt,
                                                                          int column);

#endif
