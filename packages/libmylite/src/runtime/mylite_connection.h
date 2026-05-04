#ifndef MYLITE_RUNTIME_MYLITE_CONNECTION_H
#define MYLITE_RUNTIME_MYLITE_CONNECTION_H

#include <mylite/mylite.h>

int mylite_connection_set_default_state(mylite_db *database);
int mylite_connection_set_released_error(mylite_db *database);

const char *mylite_connection_character_set_client(const mylite_db *database);
const char *mylite_connection_character_set_connection(const mylite_db *database);
const char *mylite_connection_character_set_results(const mylite_db *database);
const char *mylite_connection_collation_connection(const mylite_db *database);

#endif
