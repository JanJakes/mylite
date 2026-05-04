#ifndef MYLITE_RUNTIME_MYLITE_DIAGNOSTICS_H
#define MYLITE_RUNTIME_MYLITE_DIAGNOSTICS_H

#include "mylite_runtime.h"

void mylite_diagnostics_clear_warnings(mylite_db *database);
int mylite_diagnostics_set_sqlite_error(mylite_db *database);
int mylite_diagnostics_set_error_message(mylite_db *database, const char *message);
int mylite_diagnostics_set_error_message_parts(mylite_db *database, const char *prefix,
                                               const char *value, const char *suffix);
int mylite_diagnostics_append_warning(mylite_db *database, unsigned int code, const char *message);
int mylite_diagnostics_append_note(mylite_db *database, unsigned int code, const char *message);
int mylite_diagnostics_append_error(mylite_db *database, unsigned int code, const char *message);
int mylite_diagnostics_ensure_current_error_condition(mylite_db *database,
                                                      unsigned int fallback_code);
int mylite_diagnostics_append_current_error_condition(mylite_db *database, unsigned int code);
void mylite_diagnostics_clear_error_message(mylite_db *database);

#endif
