#ifndef MYLITE_FORK_MYLITE_SQLITE_FORK_H
#define MYLITE_FORK_MYLITE_SQLITE_FORK_H

#include "sqlite3.h"

int mylite_sqlite_fork_configure(sqlite3 *database);
int mylite_sqlite_fork_truncate_table(sqlite3 *database, const char *table_name);

#endif
