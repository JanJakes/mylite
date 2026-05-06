#include "mylite_advisory_locks.h"

#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "mylite_expression.h"
#include "mylite_span.h"
#include "sqlite3.h"

#include <stdlib.h>
#include <string.h>

enum mylite_advisory_lock_constants {
    MYLITE_ADVISORY_LOCK_NAME_MAX = 64,
};

struct mylite_advisory_lock {
    mylite_db *owner;
    uint64_t owner_connection_id;
    char *name;
    size_t acquisition_count;
    struct mylite_advisory_lock *next;
};

static struct mylite_advisory_lock *mylite_advisory_lock_registry;

static int copy_lock_name_text(
    const struct mylite_expression_value *value,
    char **out_text,
    size_t *out_length
);

static int validate_lock_name(
    mylite_db *database,
    const char *name,
    size_t length,
    struct mylite_expression_warnings *warnings
);

static int set_lock_name_error(
    mylite_db *database,
    struct mylite_expression_warnings *warnings,
    unsigned int code,
    const char *message
);

static void canonicalize_lock_name(char *name, size_t length);

static struct mylite_advisory_lock *find_lock_entry(const char *name);

static int create_lock_entry(mylite_db *database, const struct mylite_advisory_lock_name *name);

static void remove_lock_entry(
    struct mylite_advisory_lock *previous,
    struct mylite_advisory_lock *entry
);

int mylite_advisory_lock_name_from_value(
    mylite_db *database,
    const struct mylite_expression_value *value,
    struct mylite_expression_warnings *warnings,
    struct mylite_advisory_lock_name *out_name
) {
    char *text = NULL;
    size_t length = 0U;
    int status = MYLITE_OK;

    if (database == NULL || out_name == NULL) {
        return MYLITE_MISUSE;
    }

    *out_name = (struct mylite_advisory_lock_name){0};
    status = copy_lock_name_text(value, &text, &length);
    if (status != MYLITE_OK) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return status;
    }

    status = validate_lock_name(database, text, length, warnings);
    if (status != MYLITE_OK) {
        free(text);
        return status;
    }

    canonicalize_lock_name(text, length);
    *out_name = (struct mylite_advisory_lock_name){.text = text, .length = length};
    return MYLITE_OK;
}

void mylite_advisory_lock_name_deinit(struct mylite_advisory_lock_name *name) {
    if (name == NULL) {
        return;
    }
    free(name->text);
    *name = (struct mylite_advisory_lock_name){0};
}

int mylite_advisory_lock_get(
    mylite_db *database,
    const struct mylite_advisory_lock_name *name,
    uint64_t *out_value
) {
    struct mylite_advisory_lock *entry = NULL;

    if (database == NULL || name == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }

    entry = find_lock_entry(name->text);
    if (entry != NULL) {
        if (entry->owner != database) {
            *out_value = 0U;
            return MYLITE_OK;
        }
        if (entry->acquisition_count == SIZE_MAX) {
            (void)mylite_diagnostics_set_error_message(
                database,
                "Too many user-level lock acquisitions"
            );
            return MYLITE_EXEC_ERROR;
        }
        entry->acquisition_count++;
        *out_value = 1U;
        return MYLITE_OK;
    }

    int status = create_lock_entry(database, name);
    if (status != MYLITE_OK) {
        return status;
    }
    *out_value = 1U;
    return MYLITE_OK;
}

int mylite_advisory_lock_release(
    mylite_db *database,
    const struct mylite_advisory_lock_name *name,
    struct mylite_advisory_lock_result *out_result
) {
    struct mylite_advisory_lock *previous = NULL;
    struct mylite_advisory_lock *entry = mylite_advisory_lock_registry;

    if (database == NULL || name == NULL || out_result == NULL) {
        return MYLITE_MISUSE;
    }

    while (entry != NULL) {
        if (strcmp(entry->name, name->text) == 0) {
            if (entry->owner != database) {
                *out_result = (struct mylite_advisory_lock_result){.value = 0U};
                return MYLITE_OK;
            }
            entry->acquisition_count--;
            if (entry->acquisition_count == 0U) {
                remove_lock_entry(previous, entry);
            }
            *out_result = (struct mylite_advisory_lock_result){.value = 1U};
            return MYLITE_OK;
        }
        previous = entry;
        entry = entry->next;
    }

    *out_result = (struct mylite_advisory_lock_result){.is_null = true};
    return MYLITE_OK;
}

int mylite_advisory_lock_is_free(
    mylite_db *database,
    const struct mylite_advisory_lock_name *name,
    uint64_t *out_value
) {
    if (database == NULL || name == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }

    *out_value = find_lock_entry(name->text) == NULL ? 1U : 0U;
    return MYLITE_OK;
}

int mylite_advisory_lock_is_used(
    mylite_db *database,
    const struct mylite_advisory_lock_name *name,
    struct mylite_advisory_lock_result *out_result
) {
    struct mylite_advisory_lock *entry = NULL;

    if (database == NULL || name == NULL || out_result == NULL) {
        return MYLITE_MISUSE;
    }

    entry = find_lock_entry(name->text);
    if (entry == NULL) {
        *out_result = (struct mylite_advisory_lock_result){.is_null = true};
        return MYLITE_OK;
    }
    *out_result = (struct mylite_advisory_lock_result){.value = entry->owner_connection_id};
    return MYLITE_OK;
}

uint64_t mylite_advisory_locks_release_all(mylite_db *database) {
    uint64_t released = 0U;
    struct mylite_advisory_lock *previous = NULL;
    struct mylite_advisory_lock *entry = mylite_advisory_lock_registry;

    while (entry != NULL) {
        struct mylite_advisory_lock *next = entry->next;

        if (entry->owner == database) {
            released += (uint64_t)entry->acquisition_count;
            remove_lock_entry(previous, entry);
        } else {
            previous = entry;
        }
        entry = next;
    }
    return released;
}

void mylite_advisory_locks_release_handle(mylite_db *database) {
    (void)mylite_advisory_locks_release_all(database);
}

static int copy_lock_name_text(
    const struct mylite_expression_value *value,
    char **out_text,
    size_t *out_length
) {
    char *text = NULL;
    size_t length = 0U;

    if (out_text == NULL || out_length == NULL) {
        return MYLITE_MISUSE;
    }

    *out_text = NULL;
    *out_length = 0U;
    if (value == NULL || value->kind == MYLITE_EXPRESSION_VALUE_NULL) {
        return MYLITE_OK;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_TEXT) {
        length = value->text_length;
        text = mylite_copy_span_text(value->text_value == NULL ? "" : value->text_value, length);
    } else {
        text = mylite_expression_value_to_text(value);
        length = text == NULL ? 0U : strlen(text);
    }
    if (text == NULL) {
        return MYLITE_NOMEM;
    }
    *out_text = text;
    *out_length = length;
    return MYLITE_OK;
}

static int validate_lock_name(
    mylite_db *database,
    const char *name,
    size_t length,
    struct mylite_expression_warnings *warnings
) {
    if (name == NULL) {
        static const char null_name_message[] =
            "Incorrect user-level lock name 'NULL'. The name is empty, NULL, or can not be "
            "expressed in the current character-set.";

        return set_lock_name_error(
            database,
            warnings,
            MYLITE_MYSQL_ER_USER_LOCK_WRONG_NAME,
            null_name_message
        );
    }
    if (length == 0U || memchr(name, '\0', length) != NULL) {
        char *message = sqlite3_mprintf(
            "Incorrect user-level lock name '%q'. The name is empty, NULL, or can not be "
            "expressed in the current character-set.",
            name
        );
        int status = MYLITE_OK;

        if (message == NULL) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
            return MYLITE_NOMEM;
        }
        status =
            set_lock_name_error(database, warnings, MYLITE_MYSQL_ER_USER_LOCK_WRONG_NAME, message);
        sqlite3_free(message);
        return status;
    }
    if (length > MYLITE_ADVISORY_LOCK_NAME_MAX) {
        char *message =
            sqlite3_mprintf("User-level lock name '%q' should not exceed 64 characters.", name);
        int status = MYLITE_OK;

        if (message == NULL) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
            return MYLITE_NOMEM;
        }
        status = set_lock_name_error(
            database,
            warnings,
            MYLITE_MYSQL_ER_USER_LOCK_NAME_TOO_LONG,
            message
        );
        sqlite3_free(message);
        return status;
    }
    return MYLITE_OK;
}

static int set_lock_name_error(
    mylite_db *database,
    struct mylite_expression_warnings *warnings,
    unsigned int code,
    const char *message
) {
    int status = mylite_diagnostics_set_error_message(database, message);

    if (status != MYLITE_OK) {
        return status;
    }
    if (mylite_expression_warnings_append_condition(
            warnings,
            MYLITE_EXPRESSION_WARNING_LEVEL_ERROR,
            code,
            message
        ) != 0) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    return MYLITE_EXEC_ERROR;
}

static void canonicalize_lock_name(char *name, size_t length) {
    if (name == NULL) {
        return;
    }
    for (size_t index = 0U; index < length; ++index) {
        if (name[index] >= 'A' && name[index] <= 'Z') {
            name[index] = (char)(name[index] - 'A' + 'a');
        }
    }
}

static struct mylite_advisory_lock *find_lock_entry(const char *name) {
    for (struct mylite_advisory_lock *entry = mylite_advisory_lock_registry; entry != NULL;
         entry = entry->next) {
        if (strcmp(entry->name, name) == 0) {
            return entry;
        }
    }
    return NULL;
}

static int create_lock_entry(mylite_db *database, const struct mylite_advisory_lock_name *name) {
    struct mylite_advisory_lock *entry = calloc(1U, sizeof(*entry));

    if (entry == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    entry->name = mylite_copy_span_text(name->text, name->length);
    if (entry->name == NULL) {
        free(entry);
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    entry->owner = database;
    entry->owner_connection_id = database->connection_id;
    entry->acquisition_count = 1U;
    entry->next = mylite_advisory_lock_registry;
    mylite_advisory_lock_registry = entry;
    return MYLITE_OK;
}

static void remove_lock_entry(
    struct mylite_advisory_lock *previous,
    struct mylite_advisory_lock *entry
) {
    if (entry == NULL) {
        return;
    }
    if (previous == NULL) {
        mylite_advisory_lock_registry = entry->next;
    } else {
        previous->next = entry->next;
    }
    free(entry->name);
    free(entry);
}
