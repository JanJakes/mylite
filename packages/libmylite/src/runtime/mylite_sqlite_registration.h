#ifndef MYLITE_RUNTIME_MYLITE_SQLITE_REGISTRATION_H
#define MYLITE_RUNTIME_MYLITE_SQLITE_REGISTRATION_H

#include <mylite/mylite.h>

#include "sqlite3.h"

#include <stddef.h>

enum mylite_sqlite_function_kind {
    MYLITE_SQLITE_FUNCTION_SCALAR = 0,
    MYLITE_SQLITE_FUNCTION_AGGREGATE = 1,
    MYLITE_SQLITE_FUNCTION_WINDOW = 2,
};

struct mylite_sqlite_function_registration {
    enum mylite_sqlite_function_kind kind;
    const char *name;
    int argument_count;
    int text_representation;
    void *application_data;
    void (*scalar_callback)(sqlite3_context *context, int argc, sqlite3_value **argv);
    void (*step_callback)(sqlite3_context *context, int argc, sqlite3_value **argv);
    void (*final_callback)(sqlite3_context *context);
    void (*value_callback)(sqlite3_context *context);
    void (*inverse_callback)(sqlite3_context *context, int argc, sqlite3_value **argv);
    void (*destroy_callback)(void *application_data);
};

struct mylite_sqlite_collation_registration {
    const char *name;
    int text_representation;
    void *application_data;
    int (*compare_callback)(
        void *application_data,
        int left_size,
        const void *left,
        int right_size,
        const void *right
    );
    void (*destroy_callback)(void *application_data);
};

int mylite_sqlite_register_functions(
    sqlite3 *sqlite,
    const struct mylite_sqlite_function_registration *registrations,
    size_t registration_count
);
int mylite_sqlite_register_collations(
    sqlite3 *sqlite,
    const struct mylite_sqlite_collation_registration *registrations,
    size_t registration_count
);

int mylite_sqlite_status_to_mylite(int sqlite_status);

#endif
