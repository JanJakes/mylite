#ifndef MYLITE_RUNTIME_MYLITE_STATEMENT_PREPARE_H
#define MYLITE_RUNTIME_MYLITE_STATEMENT_PREPARE_H

#include <mylite/mylite.h>

#include "mylite_parser.h"
#include "mylite_sqlite_translator.h"

int mylite_statement_map_parse_status(mylite_db *database, enum mylite_sql_parse_status status);
int mylite_statement_map_translate_status(mylite_db *database,
                                          enum mylite_sqlite_translate_status status);

#endif
