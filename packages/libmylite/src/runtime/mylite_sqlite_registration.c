#include "mylite_sqlite_registration.h"

#include <stdbool.h>

static int register_function(
    sqlite3 *sqlite,
    const struct mylite_sqlite_function_registration *registration
);
static int register_scalar_function(
    sqlite3 *sqlite,
    const struct mylite_sqlite_function_registration *registration
);
static int register_aggregate_function(
    sqlite3 *sqlite,
    const struct mylite_sqlite_function_registration *registration
);
static int register_window_function(
    sqlite3 *sqlite,
    const struct mylite_sqlite_function_registration *registration
);
static bool function_registration_is_valid(
    const struct mylite_sqlite_function_registration *registration
);
static bool function_registrations_are_valid(
    const struct mylite_sqlite_function_registration *registrations,
    size_t registration_count
);
static bool scalar_function_registration_is_valid(
    const struct mylite_sqlite_function_registration *registration
);
static bool aggregate_function_registration_is_valid(
    const struct mylite_sqlite_function_registration *registration
);
static bool window_function_registration_is_valid(
    const struct mylite_sqlite_function_registration *registration
);
static bool registration_name_is_valid(const char *name);
static bool argument_count_is_valid(int argument_count);
static bool text_representation_is_valid(int text_representation);
static bool function_text_representation_is_valid(int text_representation);
static bool collation_text_representation_is_valid(int text_representation);
static int register_collation(
    sqlite3 *sqlite,
    const struct mylite_sqlite_collation_registration *registration
);
static bool collation_registration_is_valid(
    const struct mylite_sqlite_collation_registration *registration
);
static bool collation_registrations_are_valid(
    const struct mylite_sqlite_collation_registration *registrations,
    size_t registration_count
);

int mylite_sqlite_register_functions(
    sqlite3 *sqlite,
    const struct mylite_sqlite_function_registration *registrations,
    size_t registration_count
) {
    size_t index = 0U;
    int rc = MYLITE_OK;

    if (sqlite == NULL || (registrations == NULL && registration_count != 0U)) {
        return MYLITE_MISUSE;
    }
    if (!function_registrations_are_valid(registrations, registration_count)) {
        return MYLITE_MISUSE;
    }

    for (index = 0U; index < registration_count; ++index) {
        rc = register_function(sqlite, &registrations[index]);
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    return MYLITE_OK;
}

int mylite_sqlite_register_collations(
    sqlite3 *sqlite,
    const struct mylite_sqlite_collation_registration *registrations,
    size_t registration_count
) {
    size_t index = 0U;
    int rc = MYLITE_OK;

    if (sqlite == NULL || (registrations == NULL && registration_count != 0U)) {
        return MYLITE_MISUSE;
    }
    if (!collation_registrations_are_valid(registrations, registration_count)) {
        return MYLITE_MISUSE;
    }

    for (index = 0U; index < registration_count; ++index) {
        rc = register_collation(sqlite, &registrations[index]);
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    return MYLITE_OK;
}

int mylite_sqlite_status_to_mylite(int sqlite_status) {
    if (sqlite_status == SQLITE_OK) {
        return MYLITE_OK;
    }
    if (sqlite_status == SQLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    if (sqlite_status == SQLITE_MISUSE) {
        return MYLITE_MISUSE;
    }

    return MYLITE_ERROR;
}

static int register_function(
    sqlite3 *sqlite,
    const struct mylite_sqlite_function_registration *registration
) {
    if (!function_registration_is_valid(registration)) {
        return MYLITE_MISUSE;
    }

    switch (registration->kind) {
    case MYLITE_SQLITE_FUNCTION_SCALAR:
        return register_scalar_function(sqlite, registration);
    case MYLITE_SQLITE_FUNCTION_AGGREGATE:
        return register_aggregate_function(sqlite, registration);
    case MYLITE_SQLITE_FUNCTION_WINDOW:
        return register_window_function(sqlite, registration);
    }

    return MYLITE_MISUSE;
}

static int register_scalar_function(
    sqlite3 *sqlite,
    const struct mylite_sqlite_function_registration *registration
) {
    int rc = sqlite3_create_function_v2(
        sqlite,
        registration->name,
        registration->argument_count,
        registration->text_representation,
        registration->application_data,
        registration->scalar_callback,
        NULL,
        NULL,
        registration->destroy_callback
    );

    return mylite_sqlite_status_to_mylite(rc);
}

static int register_aggregate_function(
    sqlite3 *sqlite,
    const struct mylite_sqlite_function_registration *registration
) {
    int rc = sqlite3_create_function_v2(
        sqlite,
        registration->name,
        registration->argument_count,
        registration->text_representation,
        registration->application_data,
        NULL,
        registration->step_callback,
        registration->final_callback,
        registration->destroy_callback
    );

    return mylite_sqlite_status_to_mylite(rc);
}

static int register_window_function(
    sqlite3 *sqlite,
    const struct mylite_sqlite_function_registration *registration
) {
    int rc = sqlite3_create_window_function(
        sqlite,
        registration->name,
        registration->argument_count,
        registration->text_representation,
        registration->application_data,
        registration->step_callback,
        registration->final_callback,
        registration->value_callback,
        registration->inverse_callback,
        registration->destroy_callback
    );

    return mylite_sqlite_status_to_mylite(rc);
}

static bool function_registration_is_valid(
    const struct mylite_sqlite_function_registration *registration
) {
    if (registration == NULL || !registration_name_is_valid(registration->name) ||
        !argument_count_is_valid(registration->argument_count) ||
        !function_text_representation_is_valid(registration->text_representation)) {
        return false;
    }

    switch (registration->kind) {
    case MYLITE_SQLITE_FUNCTION_SCALAR:
        return scalar_function_registration_is_valid(registration);
    case MYLITE_SQLITE_FUNCTION_AGGREGATE:
        return aggregate_function_registration_is_valid(registration);
    case MYLITE_SQLITE_FUNCTION_WINDOW:
        return window_function_registration_is_valid(registration);
    }

    return false;
}

static bool function_registrations_are_valid(
    const struct mylite_sqlite_function_registration *registrations,
    size_t registration_count
) {
    size_t index = 0U;

    for (index = 0U; index < registration_count; ++index) {
        if (!function_registration_is_valid(&registrations[index])) {
            return false;
        }
    }

    return true;
}

static bool scalar_function_registration_is_valid(
    const struct mylite_sqlite_function_registration *registration
) {
    if (registration->scalar_callback == NULL || registration->step_callback != NULL ||
        registration->final_callback != NULL || registration->value_callback != NULL ||
        registration->inverse_callback != NULL) {
        return false;
    }

    return true;
}

static bool aggregate_function_registration_is_valid(
    const struct mylite_sqlite_function_registration *registration
) {
    if (registration->scalar_callback != NULL || registration->step_callback == NULL ||
        registration->final_callback == NULL || registration->value_callback != NULL ||
        registration->inverse_callback != NULL) {
        return false;
    }

    return true;
}

static bool window_function_registration_is_valid(
    const struct mylite_sqlite_function_registration *registration
) {
    if (registration->scalar_callback != NULL || registration->step_callback == NULL ||
        registration->final_callback == NULL || registration->value_callback == NULL ||
        registration->inverse_callback == NULL) {
        return false;
    }

    return true;
}

static bool registration_name_is_valid(const char *name) {
    if (name == NULL || name[0] == '\0') {
        return false;
    }

    return true;
}

static bool argument_count_is_valid(int argument_count) {
    enum { variable_argument_count = -1 };

    return argument_count >= variable_argument_count;
}

static bool text_representation_is_valid(int text_representation) {
    switch (text_representation) {
    case SQLITE_UTF8:
    case SQLITE_UTF16LE:
    case SQLITE_UTF16BE:
    case SQLITE_UTF16:
        return true;
    default:
        break;
    }

    return false;
}

static bool function_text_representation_is_valid(int text_representation) {
    enum { sqlite_text_representation_base_mask = 0xff };

    const int allowed_flags = SQLITE_DETERMINISTIC | SQLITE_DIRECTONLY | SQLITE_SUBTYPE |
                              SQLITE_INNOCUOUS | SQLITE_RESULT_SUBTYPE | SQLITE_SELFORDER1;
    int base = text_representation & sqlite_text_representation_base_mask;
    int flags = text_representation & ~sqlite_text_representation_base_mask;

    if (!text_representation_is_valid(base)) {
        return false;
    }

    return (flags & ~allowed_flags) == 0;
}

static bool collation_text_representation_is_valid(int text_representation) {
    if (text_representation == SQLITE_UTF16_ALIGNED) {
        return true;
    }

    return text_representation_is_valid(text_representation);
}

static int register_collation(
    sqlite3 *sqlite,
    const struct mylite_sqlite_collation_registration *registration
) {
    int rc = SQLITE_OK;

    if (!collation_registration_is_valid(registration)) {
        return MYLITE_MISUSE;
    }

    rc = sqlite3_create_collation_v2(
        sqlite,
        registration->name,
        registration->text_representation,
        registration->application_data,
        registration->compare_callback,
        registration->destroy_callback
    );

    return mylite_sqlite_status_to_mylite(rc);
}

static bool collation_registration_is_valid(
    const struct mylite_sqlite_collation_registration *registration
) {
    if (registration == NULL || !registration_name_is_valid(registration->name) ||
        !collation_text_representation_is_valid(registration->text_representation) ||
        registration->compare_callback == NULL) {
        return false;
    }

    return true;
}

static bool collation_registrations_are_valid(
    const struct mylite_sqlite_collation_registration *registrations,
    size_t registration_count
) {
    size_t index = 0U;

    for (index = 0U; index < registration_count; ++index) {
        if (!collation_registration_is_valid(&registrations[index])) {
            return false;
        }
    }

    return true;
}
