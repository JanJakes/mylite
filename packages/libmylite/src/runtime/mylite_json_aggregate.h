#ifndef MYLITE_RUNTIME_MYLITE_JSON_AGGREGATE_H
#define MYLITE_RUNTIME_MYLITE_JSON_AGGREGATE_H

#include "sqlite3.h"

int mylite_sqlite_register_json_aggregate_functions(sqlite3 *sqlite);

#endif
