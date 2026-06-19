#ifndef MYLITE_RUNTIME_MYLITE_LAST_INSERT_ID_H
#define MYLITE_RUNTIME_MYLITE_LAST_INSERT_ID_H

#include "sqlite3.h"

int mylite_sqlite_register_last_insert_id_functions(sqlite3 *sqlite);

#endif
