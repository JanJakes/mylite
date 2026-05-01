#ifndef MYLITE_MYLITE_INTERNAL_H
#define MYLITE_MYLITE_INTERNAL_H

#include <mylite/mylite.h>

const char *mylite_connection_character_set_client(const mylite_db *database);
const char *mylite_connection_character_set_connection(const mylite_db *database);
const char *mylite_connection_character_set_results(const mylite_db *database);
const char *mylite_connection_collation_connection(const mylite_db *database);

#endif
