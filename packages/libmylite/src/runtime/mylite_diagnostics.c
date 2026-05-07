#include "mylite_diagnostics.h"

#include "mylite_error_codes.h"
#include "sqlite3.h"

#include <stdlib.h>
#include <string.h>

static bool database_has_error_condition(const mylite_db *database);

static bool promote_current_error_message_condition(mylite_db *database);

static int append_database_condition(
    mylite_db *database,
    enum mylite_expression_warning_level level,
    unsigned int code,
    const char *message
);

static unsigned int current_error_condition_code(mylite_db *database, unsigned int fallback_code);

static char *copy_message_text(const char *text, size_t length);

const char *mylite_status_name(int status) {
    switch (status) {
    case MYLITE_OK:
        return "ok";
    case MYLITE_MISUSE:
        return "misuse";
    case MYLITE_NOMEM:
        return "nomem";
    case MYLITE_PARSE_ERROR:
        return "parse_error";
    case MYLITE_UNSUPPORTED:
        return "unsupported";
    case MYLITE_SQLITE_ERROR:
        return "sqlite_error";
    case MYLITE_EXEC_ERROR:
        return "exec_error";
    case MYLITE_ROW:
        return "row";
    case MYLITE_DONE:
        return "done";
    default:
        return "unknown";
    }
}

const char *mylite_error_message(const mylite_db *database) {
    if (database == NULL || database->error_message == NULL) {
        return "";
    }

    return database->error_message;
}

int mylite_warning_count(const mylite_db *database) {
    return database == NULL ? 0 : (int)database->warnings.count;
}

unsigned int mylite_warning_code(const mylite_db *database, int warning) {
    if (database == NULL || warning < 0 || (size_t)warning >= database->warnings.count) {
        return 0U;
    }
    return database->warnings.items[warning].code;
}

const char *mylite_warning_message(const mylite_db *database, int warning) {
    if (database == NULL || warning < 0 || (size_t)warning >= database->warnings.count) {
        return NULL;
    }
    return database->warnings.items[warning].message;
}

void mylite_diagnostics_clear_warnings(mylite_db *database) {
    if (database == NULL) {
        return;
    }

    mylite_expression_warnings_deinit(&database->warnings);
}

int mylite_diagnostics_set_error_message(mylite_db *database, const char *message) {
    size_t length = message == NULL ? 0U : strlen(message);
    char *copy = malloc(length + 1U);

    if (copy == NULL) {
        mylite_diagnostics_clear_error_message(database);
        return MYLITE_NOMEM;
    }

    if (length > 0U) {
        memcpy(copy, message, length);
    }
    copy[length] = '\0';

    free(database->error_message);
    database->error_message = copy;
    return MYLITE_OK;
}

int mylite_diagnostics_set_error_message_parts(
    mylite_db *database,
    const char *prefix,
    const char *value,
    const char *suffix
) {
    size_t prefix_length = prefix == NULL ? 0U : strlen(prefix);
    size_t value_length = value == NULL ? 0U : strlen(value);
    size_t suffix_length = suffix == NULL ? 0U : strlen(suffix);
    size_t length = prefix_length + value_length + suffix_length;
    char *message = malloc(length + 1U);
    size_t offset = 0U;
    int status = MYLITE_OK;

    if (message == NULL) {
        mylite_diagnostics_clear_error_message(database);
        return MYLITE_NOMEM;
    }

    if (prefix_length > 0U) {
        memcpy(message + offset, prefix, prefix_length);
        offset += prefix_length;
    }
    if (value_length > 0U) {
        memcpy(message + offset, value, value_length);
        offset += value_length;
    }
    if (suffix_length > 0U) {
        memcpy(message + offset, suffix, suffix_length);
        offset += suffix_length;
    }
    message[offset] = '\0';

    status = mylite_diagnostics_set_error_message(database, message);
    free(message);
    return status;
}

int mylite_diagnostics_set_unknown_charset_error(mylite_db *database, const char *name) {
    int status =
        mylite_diagnostics_set_error_message_parts(database, "Unknown character set: '", name, "'");

    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

int mylite_diagnostics_set_unknown_collation_error(mylite_db *database, const char *name) {
    int status =
        mylite_diagnostics_set_error_message_parts(database, "Unknown collation: '", name, "'");

    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

int mylite_diagnostics_set_table_doesnt_exist_error(
    mylite_db *database,
    const char *schema_name,
    const char *table_name
) {
    char *message = sqlite3_mprintf("Table '%q.%q' doesn't exist", schema_name, table_name);
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = mylite_diagnostics_set_error_message(database, message);
    if (status == MYLITE_OK) {
        status = mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_NO_SUCH_TABLE, message);
    }
    sqlite3_free(message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

int mylite_diagnostics_set_schema_access_denied_error(
    mylite_db *database,
    const char *schema_name
) {
    char *message =
        sqlite3_mprintf("Access denied for user 'root'@'localhost' to database '%q'", schema_name);
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = mylite_diagnostics_set_error_message(database, message);
    if (status == MYLITE_OK) {
        status = mylite_diagnostics_append_error(
            database,
            MYLITE_MYSQL_ER_DBACCESS_DENIED_ERROR,
            message
        );
    }
    sqlite3_free(message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

int mylite_diagnostics_set_collation_charset_error(
    mylite_db *database,
    const char *collation,
    const char *character_set
) {
    char *prefix = NULL;
    int status = MYLITE_EXEC_ERROR;

    if (mylite_diagnostics_set_error_message_parts(
            database,
            "COLLATION '",
            collation,
            "' is not valid for CHARACTER SET '"
        ) == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }

    prefix = database->error_message;
    database->error_message = NULL;
    status = mylite_diagnostics_set_error_message_parts(database, prefix, character_set, "'");
    free(prefix);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

int mylite_diagnostics_append_utf8_alias_warning(mylite_db *database) {
    return mylite_diagnostics_append_warning(
        database,
        MYLITE_MYSQL_ER_WARN_DEPRECATED_UTF8_ALIAS,
        "'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for "
        "UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous."
    );
}

int mylite_diagnostics_append_warning(mylite_db *database, unsigned int code, const char *message) {
    return append_database_condition(
        database,
        MYLITE_EXPRESSION_WARNING_LEVEL_WARNING,
        code,
        message
    );
}

int mylite_diagnostics_append_note(mylite_db *database, unsigned int code, const char *message) {
    return append_database_condition(database, MYLITE_EXPRESSION_WARNING_LEVEL_NOTE, code, message);
}

int mylite_diagnostics_append_error(mylite_db *database, unsigned int code, const char *message) {
    return append_database_condition(
        database,
        MYLITE_EXPRESSION_WARNING_LEVEL_ERROR,
        code,
        message
    );
}

int mylite_diagnostics_ensure_current_error_condition(
    mylite_db *database,
    unsigned int fallback_code
) {
    if (database_has_error_condition(database)) {
        if (database != NULL && database->error_message == NULL) {
            for (size_t index = 0U; index < database->warnings.count; ++index) {
                const struct mylite_expression_warning *condition =
                    &database->warnings.items[index];

                if (condition->level == MYLITE_EXPRESSION_WARNING_LEVEL_ERROR) {
                    return mylite_diagnostics_set_error_message(database, condition->message);
                }
            }
        }
        return MYLITE_OK;
    }
    if (promote_current_error_message_condition(database)) {
        return MYLITE_OK;
    }
    return mylite_diagnostics_append_current_error_condition(database, fallback_code);
}

int mylite_diagnostics_append_current_error_condition(mylite_db *database, unsigned int code) {
    const char *message = mylite_error_message(database);

    if (message == NULL || message[0] == '\0') {
        message = "Unknown error";
    } else {
        code = current_error_condition_code(database, code);
    }
    return mylite_diagnostics_append_error(database, code, message);
}

void mylite_diagnostics_clear_error_message(mylite_db *database) {
    if (database == NULL) {
        return;
    }

    free(database->error_message);
    database->error_message = NULL;
}

static bool database_has_error_condition(const mylite_db *database) {
    if (database == NULL) {
        return false;
    }
    for (size_t index = 0U; index < database->warnings.count; ++index) {
        if (database->warnings.items[index].level == MYLITE_EXPRESSION_WARNING_LEVEL_ERROR) {
            return true;
        }
    }
    return false;
}

static bool promote_current_error_message_condition(mylite_db *database) {
    const char *message = mylite_error_message(database);

    if (database == NULL || message == NULL || message[0] == '\0') {
        return false;
    }
    for (size_t index = database->warnings.count; index > 0U; --index) {
        struct mylite_expression_warning *condition = &database->warnings.items[index - 1U];

        if (condition->level == MYLITE_EXPRESSION_WARNING_LEVEL_WARNING &&
            condition->message != NULL && strcmp(condition->message, message) == 0) {
            condition->level = MYLITE_EXPRESSION_WARNING_LEVEL_ERROR;
            return true;
        }
    }
    return false;
}

static int append_database_condition(
    mylite_db *database,
    enum mylite_expression_warning_level level,
    unsigned int code,
    const char *message
) {
    struct mylite_expression_warning *items = NULL;
    char *copy = NULL;

    if (database == NULL) {
        return MYLITE_MISUSE;
    }

    copy =
        copy_message_text(message == NULL ? "" : message, message == NULL ? 0U : strlen(message));
    if (copy == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    items = realloc(
        database->warnings.items,
        (database->warnings.count + 1U) * sizeof(*database->warnings.items)
    );
    if (items == NULL) {
        free(copy);
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    database->warnings.items = items;
    database->warnings.items[database->warnings.count++] =
        (struct mylite_expression_warning){.code = code, .message = copy, .level = level};
    return MYLITE_OK;
}

static unsigned int current_error_condition_code(mylite_db *database, unsigned int fallback_code) {
    const char *message = mylite_error_message(database);

    if (message != NULL && strcmp(message, "No database selected") == 0) {
        return MYLITE_MYSQL_ER_NO_DB_ERROR;
    }
    return fallback_code;
}

static char *copy_message_text(const char *text, size_t length) {
    char *copy = malloc(length + 1U);

    if (copy == NULL) {
        return NULL;
    }
    if (length > 0U) {
        memcpy(copy, text, length);
    }
    copy[length] = '\0';
    return copy;
}
