#include "mylite_catalog_internal.h"

#include "mylite_connection.h"

#include <string.h>

int mylite_catalog_validate_database(struct mylite_db *database) {
    if (database == NULL || database->sqlite == NULL) {
        return MYLITE_MISUSE;
    }

    return MYLITE_OK;
}

int mylite_catalog_validate_ready_database(struct mylite_db *database) {
    int rc = mylite_catalog_validate_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!database->catalog.initialized) {
        return MYLITE_MISUSE;
    }

    return MYLITE_OK;
}

int mylite_catalog_validate_required_name(const char *name, size_t capacity) {
    size_t length = 0U;

    if (name == NULL || name[0] == '\0') {
        return MYLITE_MISUSE;
    }

    length = strlen(name);
    if (length >= capacity) {
        return MYLITE_MISUSE;
    }

    return MYLITE_OK;
}

int mylite_catalog_validate_optional_name(const char *name, size_t capacity) {
    if (name == NULL || name[0] == '\0') {
        return MYLITE_OK;
    }

    return mylite_catalog_validate_required_name(name, capacity);
}

int mylite_catalog_validate_logical_object_name(const char *name, size_t capacity) {
    int rc = mylite_catalog_validate_required_name(name, capacity);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (mylite_catalog_name_is_reserved(name)) {
        return MYLITE_MISUSE;
    }

    return MYLITE_OK;
}

int mylite_catalog_validate_table_kind(enum mylite_catalog_table_kind kind) {
    if (kind != MYLITE_CATALOG_TABLE_KIND_BASE && kind != MYLITE_CATALOG_TABLE_KIND_VIEW) {
        return MYLITE_MISUSE;
    }

    return MYLITE_OK;
}

int mylite_catalog_validate_column_default_kind(enum mylite_catalog_column_default_kind kind) {
    if (kind != MYLITE_CATALOG_COLUMN_DEFAULT_NONE &&
        kind != MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER &&
        kind != MYLITE_CATALOG_COLUMN_DEFAULT_NO_EXPLICIT &&
        kind != MYLITE_CATALOG_COLUMN_DEFAULT_DECIMAL &&
        kind != MYLITE_CATALOG_COLUMN_DEFAULT_TEXT &&
        kind != MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER_EXPRESSION &&
        kind != MYLITE_CATALOG_COLUMN_DEFAULT_NULL_EXPRESSION &&
        kind != MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_TIMESTAMP &&
        kind != MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_DATE &&
        kind != MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_TIME &&
        kind != MYLITE_CATALOG_COLUMN_DEFAULT_BINARY &&
        kind != MYLITE_CATALOG_COLUMN_DEFAULT_TEXT_EXPRESSION &&
        kind != MYLITE_CATALOG_COLUMN_DEFAULT_SCALAR_EXPRESSION) {
        return MYLITE_MISUSE;
    }

    return MYLITE_OK;
}

int mylite_catalog_validate_bool_i64(int64_t value, bool *out_bool) {
    if (value != 0 && value != 1) {
        return MYLITE_ERROR;
    }

    *out_bool = value != 0;
    return MYLITE_OK;
}

int mylite_catalog_validate_index_kind(enum mylite_catalog_index_kind kind) {
    if (kind != MYLITE_CATALOG_INDEX_KIND_PRIMARY && kind != MYLITE_CATALOG_INDEX_KIND_SECONDARY &&
        kind != MYLITE_CATALOG_INDEX_KIND_FULLTEXT && kind != MYLITE_CATALOG_INDEX_KIND_SPATIAL) {
        return MYLITE_MISUSE;
    }

    return MYLITE_OK;
}

int mylite_catalog_validate_active_mutation(const struct mylite_catalog_mutation *mutation) {
    if (mutation == NULL || !mutation->active || mutation->next_generation == 0U) {
        return MYLITE_MISUSE;
    }

    return MYLITE_OK;
}

int mylite_catalog_validate_positive_id(int64_t id) {
    if (id <= 0) {
        return MYLITE_MISUSE;
    }

    return MYLITE_OK;
}

int mylite_catalog_validate_positive_ordinal(int64_t ordinal_position) {
    if (ordinal_position <= 0) {
        return MYLITE_MISUSE;
    }

    return MYLITE_OK;
}

int mylite_catalog_validate_generation(uint64_t generation) {
    int64_t signed_generation = 0;

    if (generation == 0U) {
        return MYLITE_ERROR;
    }

    return mylite_catalog_u64_to_i64(generation, &signed_generation);
}

int mylite_catalog_validate_schema_callback(mylite_catalog_schema_callback callback) {
    if (callback == NULL) {
        return MYLITE_MISUSE;
    }

    return MYLITE_OK;
}

int mylite_catalog_validate_table_callback(mylite_catalog_table_callback callback) {
    if (callback == NULL) {
        return MYLITE_MISUSE;
    }

    return MYLITE_OK;
}

int mylite_catalog_validate_column_callback(mylite_catalog_column_callback callback) {
    if (callback == NULL) {
        return MYLITE_MISUSE;
    }

    return MYLITE_OK;
}

int mylite_catalog_validate_index_callback(mylite_catalog_index_callback callback) {
    if (callback == NULL) {
        return MYLITE_MISUSE;
    }

    return MYLITE_OK;
}

int mylite_catalog_validate_index_column_callback(mylite_catalog_index_column_callback callback) {
    if (callback == NULL) {
        return MYLITE_MISUSE;
    }

    return MYLITE_OK;
}

int mylite_catalog_validate_foreign_key_callback(mylite_catalog_foreign_key_callback callback) {
    if (callback == NULL) {
        return MYLITE_MISUSE;
    }

    return MYLITE_OK;
}

int mylite_catalog_validate_foreign_key_column_callback(
    mylite_catalog_foreign_key_column_callback callback
) {
    if (callback == NULL) {
        return MYLITE_MISUSE;
    }

    return MYLITE_OK;
}

int mylite_catalog_validate_check_constraint_callback(
    mylite_catalog_check_constraint_callback callback
) {
    if (callback == NULL) {
        return MYLITE_MISUSE;
    }

    return MYLITE_OK;
}
