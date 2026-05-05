#ifndef MYLITE_RUNTIME_MYLITE_CONNECTION_H
#define MYLITE_RUNTIME_MYLITE_CONNECTION_H

#include <mylite/mylite.h>

#include <stdbool.h>

struct mylite_connection_names_state {
    const char *character_set_name;
    const char *collation_name;
};

int mylite_connection_set_default_state(mylite_db *database);
int mylite_connection_set_released_error(mylite_db *database);
int mylite_connection_set_selected_schema(mylite_db *database, const char *schema_name);
void mylite_connection_clear_selected_schema_if_matches(mylite_db *database,
                                                        const char *schema_name);
int mylite_connection_set_names_state(mylite_db *database,
                                      struct mylite_connection_names_state state);
int mylite_connection_set_character_set_state(mylite_db *database, const char *character_set_name);
int mylite_connection_set_default_sql_mode(mylite_db *database);
int mylite_connection_set_sql_mode(mylite_db *database, const char *sql_mode);

const char *mylite_connection_character_set_client(const mylite_db *database);
const char *mylite_connection_character_set_connection(const mylite_db *database);
const char *mylite_connection_character_set_results(const mylite_db *database);
const char *mylite_connection_collation_connection(const mylite_db *database);
const char *mylite_connection_default_sql_mode(void);
const char *mylite_connection_sql_mode(const mylite_db *database);
bool mylite_connection_sql_mode_has_only_full_group_by(const mylite_db *database);

#endif
