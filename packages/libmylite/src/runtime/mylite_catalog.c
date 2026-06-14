#include "mylite_catalog.h"

#include "mylite_catalog_internal.h"

#include "mylite_connection.h"

#include <stdint.h>

int mylite_catalog_allocate_table_id_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t *out_table_id
) {
    int rc = mylite_catalog_validate_ready_database(database);

    if (out_table_id != NULL) {
        *out_table_id = 0;
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_table_id == NULL) {
        return MYLITE_MISUSE;
    }

    return mylite_catalog_read_next_table_id(database->sqlite, out_table_id);
}

int mylite_catalog_allocate_index_id_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t *out_index_id
) {
    int rc = mylite_catalog_validate_ready_database(database);

    if (out_index_id != NULL) {
        *out_index_id = 0;
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_index_id == NULL) {
        return MYLITE_MISUSE;
    }

    return mylite_catalog_read_next_index_id(database->sqlite, out_index_id);
}

int mylite_catalog_allocate_foreign_key_id_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t *out_foreign_key_id
) {
    int rc = mylite_catalog_validate_ready_database(database);

    if (out_foreign_key_id != NULL) {
        *out_foreign_key_id = 0;
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_foreign_key_id == NULL) {
        return MYLITE_MISUSE;
    }

    return mylite_catalog_read_next_foreign_key_id(database->sqlite, out_foreign_key_id);
}

int mylite_catalog_allocate_check_constraint_id_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t *out_check_constraint_id
) {
    int rc = MYLITE_OK;

    if (out_check_constraint_id != NULL) {
        *out_check_constraint_id = 0;
    }
    rc = mylite_catalog_validate_ready_database(database);
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_active_mutation(mutation);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_check_constraint_id == NULL) {
        return MYLITE_MISUSE;
    }

    return mylite_catalog_read_next_check_constraint_id(database->sqlite, out_check_constraint_id);
}
