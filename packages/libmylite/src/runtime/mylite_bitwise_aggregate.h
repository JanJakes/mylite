#ifndef MYLITE_RUNTIME_MYLITE_BITWISE_AGGREGATE_H
#define MYLITE_RUNTIME_MYLITE_BITWISE_AGGREGATE_H

#include <mylite/mylite.h>

#include "sqlite3.h"

int mylite_sqlite_register_bitwise_aggregate_functions(sqlite3 *sqlite);

#endif
