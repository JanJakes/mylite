#ifndef MYLITE_RUNTIME_MYLITE_GROUP_CONCAT_AGGREGATE_H
#define MYLITE_RUNTIME_MYLITE_GROUP_CONCAT_AGGREGATE_H

#include "sqlite3.h"

int mylite_sqlite_register_group_concat_aggregate_function(sqlite3 *sqlite);

#endif
