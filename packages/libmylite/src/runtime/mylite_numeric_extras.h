#ifndef MYLITE_RUNTIME_MYLITE_NUMERIC_EXTRAS_H
#define MYLITE_RUNTIME_MYLITE_NUMERIC_EXTRAS_H

struct sqlite3;

int mylite_sqlite_register_numeric_extra_functions(struct sqlite3 *sqlite);

#endif
