#include "mylite_named_locks.h"

#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_mysql_error_codes.h"
#include "mylite_sqlite_bootstrap.h"
#include "mylite_sqlite_registration.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    named_lock_name_max_bytes = 64,
    named_lock_name_capacity = named_lock_name_max_bytes + 1,
    named_lock_initial_entry_capacity = 8,
    named_lock_uint64_text_capacity = 32,
};

struct named_lock_entry {
    char name[named_lock_name_capacity];
    size_t name_size;
    uint64_t owner_connection_id;
    uint64_t hold_count;
};

static void get_lock_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static void is_free_lock_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static void is_used_lock_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static void release_lock_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static void release_all_locks_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
);
static void icu_version_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static void benchmark_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static int sqlite_named_lock_argument(
    sqlite3_context *context,
    sqlite3_value *value,
    const char **out_name,
    size_t *out_name_size
);
static void sqlite_named_lock_error(sqlite3_context *context, int rc, struct mylite_db *database);
static sqlite3_mutex *named_lock_registry_mutex(void);
static int named_lock_validate(struct mylite_db *database, struct mylite_named_lock_name name);
static void set_wrong_name_error(struct mylite_db *database);
static void set_name_too_long_error(struct mylite_db *database);
static struct named_lock_entry *find_named_lock_entry(struct mylite_named_lock_name name);
static int ensure_named_lock_entry_capacity(struct mylite_db *database, size_t required_capacity);
static struct named_lock_entry *append_named_lock_entry(
    struct mylite_db *database,
    struct mylite_named_lock_name name,
    uint64_t owner_connection_id
);
static void remove_named_lock_entry(size_t index);
static int append_benchmark_negative_warning(struct mylite_db *database, int64_t count);
static void result_uint64(sqlite3_context *context, uint64_t value);

static struct named_lock_entry *named_lock_entries = NULL;
static size_t named_lock_entry_count = 0U;
static size_t named_lock_entry_capacity = 0U;

int mylite_named_lock_get(
    struct mylite_db *database,
    struct mylite_named_lock_name name,
    int64_t timeout,
    int64_t *out_value
) {
    sqlite3_mutex *mutex = NULL;
    struct named_lock_entry *entry = NULL;
    uint64_t connection_id = 0U;
    int rc = MYLITE_OK;

    (void)timeout;
    if (database == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = 0;
    rc = named_lock_validate(database, name);
    if (rc != MYLITE_OK) {
        return rc;
    }

    connection_id = database->session.connection_id;
    mutex = named_lock_registry_mutex();
    if (mutex != NULL) {
        sqlite3_mutex_enter(mutex);
    }

    entry = find_named_lock_entry(name);
    if (entry == NULL) {
        entry = append_named_lock_entry(database, name, connection_id);
        if (entry == NULL) {
            if (mutex != NULL) {
                sqlite3_mutex_leave(mutex);
            }
            return MYLITE_NOMEM;
        }
        *out_value = 1;
    } else if (entry->owner_connection_id == connection_id) {
        if (entry->hold_count == UINT64_MAX) {
            if (mutex != NULL) {
                sqlite3_mutex_leave(mutex);
            }
            mylite_diagnostics_set_error(
                &database->diagnostics,
                MYLITE_NOMEM,
                "HY001",
                "too many recursive named lock holds"
            );
            return MYLITE_NOMEM;
        }
        ++entry->hold_count;
        *out_value = 1;
    } else {
        *out_value = 0;
    }

    if (mutex != NULL) {
        sqlite3_mutex_leave(mutex);
    }
    return MYLITE_OK;
}

int mylite_named_lock_is_free(
    struct mylite_db *database,
    struct mylite_named_lock_name name,
    int64_t *out_value
) {
    sqlite3_mutex *mutex = NULL;
    int rc = MYLITE_OK;

    if (database == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = 0;
    rc = named_lock_validate(database, name);
    if (rc != MYLITE_OK) {
        return rc;
    }

    mutex = named_lock_registry_mutex();
    if (mutex != NULL) {
        sqlite3_mutex_enter(mutex);
    }
    *out_value = find_named_lock_entry(name) == NULL ? 1 : 0;
    if (mutex != NULL) {
        sqlite3_mutex_leave(mutex);
    }
    return MYLITE_OK;
}

int mylite_named_lock_is_used(
    struct mylite_db *database,
    struct mylite_named_lock_name name,
    uint64_t *out_connection_id,
    int *out_is_null
) {
    sqlite3_mutex *mutex = NULL;
    struct named_lock_entry *entry = NULL;
    int rc = MYLITE_OK;

    if (database == NULL || out_connection_id == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_connection_id = 0U;
    *out_is_null = 1;
    rc = named_lock_validate(database, name);
    if (rc != MYLITE_OK) {
        return rc;
    }

    mutex = named_lock_registry_mutex();
    if (mutex != NULL) {
        sqlite3_mutex_enter(mutex);
    }
    entry = find_named_lock_entry(name);
    if (entry != NULL) {
        *out_connection_id = entry->owner_connection_id;
        *out_is_null = 0;
    }
    if (mutex != NULL) {
        sqlite3_mutex_leave(mutex);
    }
    return MYLITE_OK;
}

int mylite_named_lock_release(
    struct mylite_db *database,
    struct mylite_named_lock_name name,
    int64_t *out_value,
    int *out_is_null
) {
    sqlite3_mutex *mutex = NULL;
    uint64_t connection_id = 0U;
    int rc = MYLITE_OK;

    if (database == NULL || out_value == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = 0;
    *out_is_null = 1;
    rc = named_lock_validate(database, name);
    if (rc != MYLITE_OK) {
        return rc;
    }

    connection_id = database->session.connection_id;
    mutex = named_lock_registry_mutex();
    if (mutex != NULL) {
        sqlite3_mutex_enter(mutex);
    }
    for (size_t index = 0U; index < named_lock_entry_count; ++index) {
        struct named_lock_entry *entry = &named_lock_entries[index];

        if (entry->name_size != name.size || memcmp(entry->name, name.data, name.size) != 0) {
            continue;
        }
        *out_is_null = 0;
        if (entry->owner_connection_id != connection_id) {
            *out_value = 0;
        } else {
            *out_value = 1;
            --entry->hold_count;
            if (entry->hold_count == 0U) {
                remove_named_lock_entry(index);
            }
        }
        if (mutex != NULL) {
            sqlite3_mutex_leave(mutex);
        }
        return MYLITE_OK;
    }
    if (mutex != NULL) {
        sqlite3_mutex_leave(mutex);
    }
    return MYLITE_OK;
}

int mylite_named_lock_release_all(struct mylite_db *database, uint64_t *out_count) {
    sqlite3_mutex *mutex = NULL;

    if (database == NULL || out_count == NULL) {
        return MYLITE_MISUSE;
    }
    *out_count = 0U;

    mutex = named_lock_registry_mutex();
    if (mutex != NULL) {
        sqlite3_mutex_enter(mutex);
    }
    for (size_t index = 0U; index < named_lock_entry_count;) {
        if (named_lock_entries[index].owner_connection_id == database->session.connection_id) {
            if (*out_count > UINT64_MAX - named_lock_entries[index].hold_count) {
                *out_count = UINT64_MAX;
            } else {
                *out_count += named_lock_entries[index].hold_count;
            }
            remove_named_lock_entry(index);
        } else {
            ++index;
        }
    }
    if (mutex != NULL) {
        sqlite3_mutex_leave(mutex);
    }
    return MYLITE_OK;
}

void mylite_named_lock_release_all_for_connection(uint64_t connection_id) {
    sqlite3_mutex *mutex = named_lock_registry_mutex();

    if (mutex != NULL) {
        sqlite3_mutex_enter(mutex);
    }
    for (size_t index = 0U; index < named_lock_entry_count;) {
        if (named_lock_entries[index].owner_connection_id == connection_id) {
            remove_named_lock_entry(index);
        } else {
            ++index;
        }
    }
    if (mutex != NULL) {
        sqlite3_mutex_leave(mutex);
    }
}

int mylite_sqlite_register_named_lock_functions(sqlite3 *sqlite) {
    static const struct mylite_sqlite_function_registration registrations[] = {
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_get_lock",
            .argument_count = 2,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY,
            .application_data = NULL,
            .scalar_callback = get_lock_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_is_free_lock",
            .argument_count = 1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY,
            .application_data = NULL,
            .scalar_callback = is_free_lock_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_is_used_lock",
            .argument_count = 1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY,
            .application_data = NULL,
            .scalar_callback = is_used_lock_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_release_lock",
            .argument_count = 1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY,
            .application_data = NULL,
            .scalar_callback = release_lock_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_release_all_locks",
            .argument_count = 0,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY,
            .application_data = NULL,
            .scalar_callback = release_all_locks_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_icu_version",
            .argument_count = 0,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_DETERMINISTIC,
            .application_data = NULL,
            .scalar_callback = icu_version_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_benchmark",
            .argument_count = 2,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY,
            .application_data = NULL,
            .scalar_callback = benchmark_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
    };

    return mylite_sqlite_register_functions(
        sqlite,
        registrations,
        sizeof(registrations) / sizeof(registrations[0])
    );
}

static void get_lock_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    struct mylite_db *database = NULL;
    const char *name = NULL;
    size_t name_size = 0U;
    int64_t value = 0;
    int rc = MYLITE_OK;

    if (context == NULL || argc != 2 || argv == NULL) {
        sqlite3_result_error(context, "invalid MyLite GET_LOCK callback", -1);
        return;
    }
    database = mylite_sqlite_bootstrap_owner_from_context(context);
    rc = sqlite_named_lock_argument(context, argv[0], &name, &name_size);
    if (rc == MYLITE_OK) {
        rc = mylite_named_lock_get(
            database,
            (struct mylite_named_lock_name){.data = name, .size = name_size},
            sqlite3_value_type(argv[1]) == SQLITE_NULL ? 0 : sqlite3_value_int64(argv[1]),
            &value
        );
    }
    if (rc != MYLITE_OK) {
        sqlite_named_lock_error(context, rc, database);
        return;
    }
    sqlite3_result_int64(context, value);
}

static void is_free_lock_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    struct mylite_db *database = NULL;
    const char *name = NULL;
    size_t name_size = 0U;
    int64_t value = 0;
    int rc = MYLITE_OK;

    if (context == NULL || argc != 1 || argv == NULL) {
        sqlite3_result_error(context, "invalid MyLite IS_FREE_LOCK callback", -1);
        return;
    }
    database = mylite_sqlite_bootstrap_owner_from_context(context);
    rc = sqlite_named_lock_argument(context, argv[0], &name, &name_size);
    if (rc == MYLITE_OK) {
        rc = mylite_named_lock_is_free(
            database,
            (struct mylite_named_lock_name){.data = name, .size = name_size},
            &value
        );
    }
    if (rc != MYLITE_OK) {
        sqlite_named_lock_error(context, rc, database);
        return;
    }
    sqlite3_result_int64(context, value);
}

static void is_used_lock_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    struct mylite_db *database = NULL;
    const char *name = NULL;
    size_t name_size = 0U;
    uint64_t connection_id = 0U;
    int is_null = 1;
    int rc = MYLITE_OK;

    if (context == NULL || argc != 1 || argv == NULL) {
        sqlite3_result_error(context, "invalid MyLite IS_USED_LOCK callback", -1);
        return;
    }
    database = mylite_sqlite_bootstrap_owner_from_context(context);
    rc = sqlite_named_lock_argument(context, argv[0], &name, &name_size);
    if (rc == MYLITE_OK) {
        rc = mylite_named_lock_is_used(
            database,
            (struct mylite_named_lock_name){.data = name, .size = name_size},
            &connection_id,
            &is_null
        );
    }
    if (rc != MYLITE_OK) {
        sqlite_named_lock_error(context, rc, database);
        return;
    }
    if (is_null) {
        sqlite3_result_null(context);
    } else {
        result_uint64(context, connection_id);
    }
}

static void release_lock_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    struct mylite_db *database = NULL;
    const char *name = NULL;
    size_t name_size = 0U;
    int64_t value = 0;
    int is_null = 1;
    int rc = MYLITE_OK;

    if (context == NULL || argc != 1 || argv == NULL) {
        sqlite3_result_error(context, "invalid MyLite RELEASE_LOCK callback", -1);
        return;
    }
    database = mylite_sqlite_bootstrap_owner_from_context(context);
    rc = sqlite_named_lock_argument(context, argv[0], &name, &name_size);
    if (rc == MYLITE_OK) {
        rc = mylite_named_lock_release(
            database,
            (struct mylite_named_lock_name){.data = name, .size = name_size},
            &value,
            &is_null
        );
    }
    if (rc != MYLITE_OK) {
        sqlite_named_lock_error(context, rc, database);
        return;
    }
    if (is_null) {
        sqlite3_result_null(context);
    } else {
        sqlite3_result_int64(context, value);
    }
}

static void release_all_locks_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
) {
    struct mylite_db *database = NULL;
    uint64_t count = 0U;
    int rc = MYLITE_OK;

    (void)argv;
    if (context == NULL || argc != 0) {
        sqlite3_result_error(context, "invalid MyLite RELEASE_ALL_LOCKS callback", -1);
        return;
    }
    database = mylite_sqlite_bootstrap_owner_from_context(context);
    rc = mylite_named_lock_release_all(database, &count);
    if (rc != MYLITE_OK) {
        sqlite_named_lock_error(context, rc, database);
        return;
    }
    result_uint64(context, count);
}

static void icu_version_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    (void)argv;
    if (context == NULL || argc != 0) {
        sqlite3_result_error(context, "invalid MyLite ICU_VERSION callback", -1);
        return;
    }
    sqlite3_result_text(context, "77.1", -1, SQLITE_STATIC);
}

static void benchmark_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    struct mylite_db *database = NULL;
    int64_t count = 0;
    int rc = MYLITE_OK;

    if (context == NULL || argc != 2 || argv == NULL) {
        sqlite3_result_error(context, "invalid MyLite BENCHMARK callback", -1);
        return;
    }
    if (sqlite3_value_type(argv[0]) == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    }

    count = sqlite3_value_int64(argv[0]);
    if (count < 0) {
        database = mylite_sqlite_bootstrap_owner_from_context(context);
        rc = append_benchmark_negative_warning(database, count);
        if (rc != MYLITE_OK) {
            sqlite_named_lock_error(context, rc, database);
            return;
        }
        sqlite3_result_null(context);
        return;
    }
    sqlite3_result_int64(context, 0);
}

static int sqlite_named_lock_argument(
    sqlite3_context *context,
    sqlite3_value *value,
    const char **out_name,
    size_t *out_name_size
) {
    const unsigned char *text = NULL;
    int byte_count = 0;

    if (context == NULL || value == NULL || out_name == NULL || out_name_size == NULL) {
        return MYLITE_MISUSE;
    }
    *out_name = NULL;
    *out_name_size = 0U;
    if (sqlite3_value_type(value) == SQLITE_NULL) {
        return MYLITE_OK;
    }
    text = sqlite3_value_text(value);
    byte_count = sqlite3_value_bytes(value);
    if (text == NULL || byte_count < 0) {
        return MYLITE_NOMEM;
    }
    *out_name = (const char *)text;
    *out_name_size = (size_t)byte_count;
    return MYLITE_OK;
}

static void sqlite_named_lock_error(sqlite3_context *context, int rc, struct mylite_db *database) {
    const char *message = "MyLite named-lock function failed";

    if (database != NULL) {
        message = mylite_diagnostics_errmsg(&database->diagnostics);
    }
    sqlite3_result_error(context, message, -1);
    if (rc == MYLITE_NOMEM) {
        sqlite3_result_error_nomem(context);
    }
}

static sqlite3_mutex *named_lock_registry_mutex(void) {
    return sqlite3_mutex_alloc(SQLITE_MUTEX_STATIC_APP2);
}

static int named_lock_validate(struct mylite_db *database, struct mylite_named_lock_name name) {
    if (database == NULL) {
        return MYLITE_MISUSE;
    }
    if (name.data == NULL || name.size == 0U) {
        set_wrong_name_error(database);
        return MYLITE_ERROR;
    }
    if (name.size > named_lock_name_max_bytes) {
        set_name_too_long_error(database);
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static void set_wrong_name_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        &database->diagnostics,
        mysql_error_user_lock_wrong_name,
        "42000",
        "Incorrect user-level lock name. The name is empty or NULL"
    );
}

static void set_name_too_long_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        &database->diagnostics,
        mysql_error_user_lock_name_too_long,
        "42000",
        "User-level lock name is too long"
    );
}

static struct named_lock_entry *find_named_lock_entry(struct mylite_named_lock_name name) {
    for (size_t index = 0U; index < named_lock_entry_count; ++index) {
        struct named_lock_entry *entry = &named_lock_entries[index];

        if (entry->name_size == name.size && memcmp(entry->name, name.data, name.size) == 0) {
            return entry;
        }
    }
    return NULL;
}

static int ensure_named_lock_entry_capacity(struct mylite_db *database, size_t required_capacity) {
    struct named_lock_entry *entries = NULL;
    size_t capacity = named_lock_entry_capacity == 0U ? named_lock_initial_entry_capacity
                                                      : named_lock_entry_capacity;

    if (required_capacity <= named_lock_entry_capacity) {
        return MYLITE_OK;
    }
    while (capacity < required_capacity) {
        if (capacity > SIZE_MAX / 2U) {
            mylite_diagnostics_set_error(
                &database->diagnostics,
                MYLITE_NOMEM,
                "HY001",
                "too many named locks"
            );
            return MYLITE_NOMEM;
        }
        capacity *= 2U;
    }
    entries = (struct named_lock_entry *)
        realloc(named_lock_entries, capacity * sizeof(*named_lock_entries));
    if (entries == NULL) {
        mylite_diagnostics_set_error(
            &database->diagnostics,
            MYLITE_NOMEM,
            "HY001",
            "out of memory while recording named lock"
        );
        return MYLITE_NOMEM;
    }
    named_lock_entries = entries;
    named_lock_entry_capacity = capacity;
    return MYLITE_OK;
}

static struct named_lock_entry *append_named_lock_entry(
    struct mylite_db *database,
    struct mylite_named_lock_name name,
    uint64_t owner_connection_id
) {
    struct named_lock_entry *entry = NULL;

    if (ensure_named_lock_entry_capacity(database, named_lock_entry_count + 1U) != MYLITE_OK) {
        return NULL;
    }
    entry = &named_lock_entries[named_lock_entry_count];
    memset(entry, 0, sizeof(*entry));
    memcpy(entry->name, name.data, name.size);
    entry->name_size = name.size;
    entry->owner_connection_id = owner_connection_id;
    entry->hold_count = 1U;
    ++named_lock_entry_count;
    return entry;
}

static void remove_named_lock_entry(size_t index) {
    if (index >= named_lock_entry_count) {
        return;
    }
    --named_lock_entry_count;
    if (index != named_lock_entry_count) {
        named_lock_entries[index] = named_lock_entries[named_lock_entry_count];
    }
    memset(&named_lock_entries[named_lock_entry_count], 0, sizeof(named_lock_entries[0]));
}

static int append_benchmark_negative_warning(struct mylite_db *database, int64_t count) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = 0;

    if (database == NULL) {
        return MYLITE_MISUSE;
    }
    written = snprintf(
        message,
        sizeof(message),
        "Incorrect count value: '%" PRId64 "' for function benchmark",
        count
    );
    if (written < 0 || (size_t)written >= sizeof(message)) {
        mylite_diagnostics_set_error(
            &database->diagnostics,
            MYLITE_NOMEM,
            "HY001",
            "failed to format BENCHMARK() warning"
        );
        return MYLITE_NOMEM;
    }
    return mylite_diagnostics_append_warning(
        &database->diagnostics,
        mysql_warning_incorrect_count,
        "HY000",
        message
    );
}

static void result_uint64(sqlite3_context *context, uint64_t value) {
    char text[named_lock_uint64_text_capacity];
    int written = 0;

    if (value <= (uint64_t)INT64_MAX) {
        sqlite3_result_int64(context, (sqlite3_int64)value);
        return;
    }
    written = snprintf(text, sizeof(text), "%" PRIu64, value);
    if (written < 0 || (size_t)written >= sizeof(text)) {
        sqlite3_result_error(context, "failed to format MyLite unsigned integer", -1);
        return;
    }
    sqlite3_result_text(context, text, -1, SQLITE_TRANSIENT);
}
