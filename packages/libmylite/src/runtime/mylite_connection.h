#ifndef MYLITE_RUNTIME_MYLITE_CONNECTION_H
#define MYLITE_RUNTIME_MYLITE_CONNECTION_H

#include <mylite/mylite.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct mylite_connection_names_state {
    const char *character_set_name;
    const char *collation_name;
};

int mylite_connection_set_default_state(mylite_db *database);
int mylite_connection_set_released_error(mylite_db *database);
int mylite_connection_set_selected_schema(mylite_db *database, const char *schema_name);
void mylite_connection_clear_selected_schema_if_matches(
    mylite_db *database,
    const char *schema_name
);
int mylite_connection_set_names_state(
    mylite_db *database,
    struct mylite_connection_names_state state
);
int mylite_connection_set_character_set_state(mylite_db *database, const char *character_set_name);
int mylite_connection_set_default_sql_mode(mylite_db *database);
int mylite_connection_set_sql_mode(mylite_db *database, const char *sql_mode);
int mylite_connection_set_default_storage_engine(mylite_db *database);
int mylite_connection_set_storage_engine(mylite_db *database, const char *storage_engine);
int mylite_connection_set_default_time_zone(mylite_db *database);
int mylite_connection_set_time_zone(mylite_db *database, const char *time_zone);
int mylite_connection_set_default_group_concat_max_len(mylite_db *database);
int mylite_connection_set_group_concat_max_len(mylite_db *database, uint64_t value);
int mylite_connection_set_default_wait_timeout(mylite_db *database);
int mylite_connection_set_wait_timeout(mylite_db *database, uint64_t value);
int mylite_connection_set_default_foreign_key_checks(mylite_db *database);
int mylite_connection_set_foreign_key_checks(mylite_db *database, bool enabled);
int mylite_connection_set_default_unique_checks(mylite_db *database);
int mylite_connection_set_unique_checks(mylite_db *database, bool enabled);
int mylite_connection_set_default_sql_notes(mylite_db *database);
int mylite_connection_set_sql_notes(mylite_db *database, bool enabled);
int mylite_connection_set_default_sql_log_bin(mylite_db *database);
int mylite_connection_set_sql_log_bin(mylite_db *database, bool enabled);

const char *mylite_connection_character_set_client(const mylite_db *database);
const char *mylite_connection_character_set_connection(const mylite_db *database);
const char *mylite_connection_character_set_results(const mylite_db *database);
const char *mylite_connection_collation_connection(const mylite_db *database);
const char *mylite_connection_default_sql_mode(void);
const char *mylite_connection_sql_mode(const mylite_db *database);
bool mylite_connection_sql_mode_is_strict(const mylite_db *database);
bool mylite_connection_sql_mode_has_ansi_quotes(const mylite_db *database);
bool mylite_connection_sql_mode_has_only_full_group_by(const mylite_db *database);
bool mylite_connection_sql_mode_has_no_auto_value_on_zero(const mylite_db *database);
bool mylite_connection_sql_mode_has_no_backslash_escapes(const mylite_db *database);
bool mylite_connection_sql_mode_has_real_as_float(const mylite_db *database);
bool mylite_connection_sql_mode_has_no_zero_date(const mylite_db *database);
bool mylite_connection_sql_mode_has_no_zero_in_date(const mylite_db *database);
bool mylite_connection_sql_mode_allows_invalid_dates(const mylite_db *database);
const char *mylite_connection_default_storage_engine(void);
const char *mylite_connection_storage_engine(const mylite_db *database);
const char *mylite_connection_default_time_zone(void);
const char *mylite_connection_time_zone(const mylite_db *database);
uint64_t mylite_connection_default_group_concat_max_len(void);
uint64_t mylite_connection_group_concat_max_len(const mylite_db *database);
size_t mylite_connection_group_concat_max_len_size(const mylite_db *database);
uint64_t mylite_connection_default_max_allowed_packet(void);
uint64_t mylite_connection_default_max_connections(void);
uint64_t mylite_connection_default_wait_timeout(void);
uint64_t mylite_connection_wait_timeout(const mylite_db *database);
bool mylite_connection_default_foreign_key_checks(void);
bool mylite_connection_foreign_key_checks(const mylite_db *database);
bool mylite_connection_default_unique_checks(void);
bool mylite_connection_unique_checks(const mylite_db *database);
bool mylite_connection_default_sql_notes(void);
bool mylite_connection_sql_notes(const mylite_db *database);
bool mylite_connection_default_sql_log_bin(void);
bool mylite_connection_sql_log_bin(const mylite_db *database);

#endif
