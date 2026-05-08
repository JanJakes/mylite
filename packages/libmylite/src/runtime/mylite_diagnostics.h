#ifndef MYLITE_RUNTIME_MYLITE_DIAGNOSTICS_H
#define MYLITE_RUNTIME_MYLITE_DIAGNOSTICS_H

#include "mylite_runtime.h"

void mylite_diagnostics_clear_warnings(mylite_db *database);
int mylite_diagnostics_set_error_message(mylite_db *database, const char *message);

static inline int mylite_diagnostics_set_sqlite_error(mylite_db *database) {
    int status = mylite_diagnostics_set_error_message(database, sqlite3_errmsg(database->sqlite));

    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_SQLITE_ERROR;
}

int mylite_diagnostics_set_error_message_parts(
    mylite_db *database,
    const char *prefix,
    const char *value,
    const char *suffix
);
int mylite_diagnostics_set_unknown_charset_error(mylite_db *database, const char *name);
int mylite_diagnostics_set_unknown_collation_error(mylite_db *database, const char *name);
int mylite_diagnostics_set_table_doesnt_exist_error(
    mylite_db *database,
    const char *schema_name,
    const char *table_name
);
int mylite_diagnostics_set_schema_access_denied_error(mylite_db *database, const char *schema_name);
int mylite_diagnostics_set_foreign_key_missing_unique_parent_error(
    mylite_db *database,
    const char *constraint_name,
    const char *referenced_table_name
);
int mylite_diagnostics_set_collation_charset_error(
    mylite_db *database,
    const char *collation,
    const char *character_set
);
int mylite_diagnostics_append_utf8_alias_warning(mylite_db *database);
int mylite_diagnostics_append_national_charset_warning(
    mylite_db *database,
    struct mylite_sql_source_span span
);
int mylite_diagnostics_append_warning(mylite_db *database, unsigned int code, const char *message);
int mylite_diagnostics_append_note(mylite_db *database, unsigned int code, const char *message);
int mylite_diagnostics_append_error(mylite_db *database, unsigned int code, const char *message);
int mylite_diagnostics_ensure_current_error_condition(
    mylite_db *database,
    unsigned int fallback_code
);
int mylite_diagnostics_append_current_error_condition(mylite_db *database, unsigned int code);
void mylite_diagnostics_clear_error_message(mylite_db *database);

#endif
