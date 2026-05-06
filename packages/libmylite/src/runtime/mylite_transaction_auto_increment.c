#include "mylite_transactions.h"

#include "mylite_catalog.h"
#include "mylite_diagnostics.h"
#include "mylite_runtime.h"
#include "mylite_span.h"

#include <stdlib.h>
#include <string.h>

static int record_pending_auto_increment(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    uint64_t next_auto_increment
);

int mylite_transaction_update_table_auto_increment(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    uint64_t next_auto_increment
) {
    int status = MYLITE_OK;

    if (database == NULL || schema_name == NULL || table_name == NULL) {
        return MYLITE_MISUSE;
    }

    status = mylite_catalog_update_auto_increment(
        database,
        schema_name,
        table_name,
        next_auto_increment
    );
    if (status != MYLITE_OK) {
        return status;
    }
    return record_pending_auto_increment(database, schema_name, table_name, next_auto_increment);
}

void mylite_transaction_clear_pending_auto_increments(mylite_db *database) {
    for (size_t index = 0U; index < database->pending_auto_increment_count; ++index) {
        free(database->pending_auto_increments[index].schema_name);
        free(database->pending_auto_increments[index].table_name);
    }
    free(database->pending_auto_increments);
    database->pending_auto_increments = NULL;
    database->pending_auto_increment_count = 0U;
}

int mylite_transaction_reapply_pending_auto_increments(mylite_db *database) {
    for (size_t index = 0U; index < database->pending_auto_increment_count; ++index) {
        const struct mylite_pending_auto_increment *item =
            &database->pending_auto_increments[index];
        int status = mylite_catalog_update_auto_increment(
            database,
            item->schema_name,
            item->table_name,
            item->next_auto_increment
        );

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int record_pending_auto_increment(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    uint64_t next_auto_increment
) {
    struct mylite_pending_auto_increment *items = NULL;
    char *schema_copy = NULL;
    char *table_copy = NULL;

    if (!database->transaction_active) {
        return MYLITE_OK;
    }
    for (size_t index = 0U; index < database->pending_auto_increment_count; ++index) {
        struct mylite_pending_auto_increment *item = &database->pending_auto_increments[index];

        if (strcmp(item->schema_name, schema_name) == 0 &&
            strcmp(item->table_name, table_name) == 0) {
            if (next_auto_increment > item->next_auto_increment) {
                item->next_auto_increment = next_auto_increment;
            }
            return MYLITE_OK;
        }
    }

    if (database->pending_auto_increment_count >=
        SIZE_MAX / sizeof(*database->pending_auto_increments)) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    schema_copy = mylite_copy_span_text(schema_name, strlen(schema_name));
    table_copy = mylite_copy_span_text(table_name, strlen(table_name));
    if (schema_copy == NULL || table_copy == NULL) {
        free(schema_copy);
        free(table_copy);
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    items = realloc(
        database->pending_auto_increments,
        (database->pending_auto_increment_count + 1U) * sizeof(*items)
    );
    if (items == NULL) {
        free(schema_copy);
        free(table_copy);
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    database->pending_auto_increments = items;
    database->pending_auto_increments[database->pending_auto_increment_count] =
        (struct mylite_pending_auto_increment){
            .schema_name = schema_copy,
            .table_name = table_copy,
            .next_auto_increment = next_auto_increment,
        };
    ++database->pending_auto_increment_count;
    return MYLITE_OK;
}
