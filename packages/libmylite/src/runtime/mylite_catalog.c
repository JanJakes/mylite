#include "mylite_catalog.h"

#include "mylite_connection.h"
#include "mylite_file_format.h"
#include "mylite_sqlite_registration.h"
#include "sqlite3.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum {
    catalog_table_count = 6,
    legacy_catalog_table_count = 4,
    catalog_schema_version_v5 = 5U,
    sqlite_use_nul_terminated_string = -1,
};

enum catalog_table_insert_bind_index {
    catalog_table_insert_schema_id_bind = 1,
    catalog_table_insert_name_bind = 2,
    catalog_table_insert_kind_bind = 3,
    catalog_table_insert_physical_name_bind = 4,
    catalog_table_insert_auto_increment_next_bind = 5,
    catalog_table_insert_generation_bind = 6,
};

enum catalog_table_insert_in_mutation_bind_index {
    catalog_table_insert_in_mutation_table_id_bind = 1,
    catalog_table_insert_in_mutation_schema_id_bind = 2,
    catalog_table_insert_in_mutation_name_bind = 3,
    catalog_table_insert_in_mutation_kind_bind = 4,
    catalog_table_insert_in_mutation_physical_name_bind = 5,
    catalog_table_insert_in_mutation_auto_increment_next_bind = 6,
    catalog_table_insert_in_mutation_generation_bind = 7,
};

enum catalog_column_insert_bind_index {
    catalog_column_insert_table_id_bind = 1,
    catalog_column_insert_ordinal_position_bind = 2,
    catalog_column_insert_name_bind = 3,
    catalog_column_insert_logical_type_bind = 4,
    catalog_column_insert_physical_type_bind = 5,
    catalog_column_insert_is_nullable_bind = 6,
    catalog_column_insert_is_visible_bind = 7,
    catalog_column_insert_is_auto_increment_bind = 8,
    catalog_column_insert_default_kind_bind = 9,
    catalog_column_insert_default_integer_bind = 10,
    catalog_column_insert_generation_bind = 11,
};

enum catalog_column_replace_bind_index {
    catalog_column_replace_name_bind = 1,
    catalog_column_replace_logical_type_bind = 2,
    catalog_column_replace_physical_type_bind = 3,
    catalog_column_replace_is_nullable_bind = 4,
    catalog_column_replace_is_visible_bind = 5,
    catalog_column_replace_is_auto_increment_bind = 6,
    catalog_column_replace_default_kind_bind = 7,
    catalog_column_replace_default_integer_bind = 8,
    catalog_column_replace_generation_bind = 9,
    catalog_column_replace_table_id_bind = 10,
    catalog_column_replace_column_id_bind = 11,
};

enum catalog_index_insert_bind_index {
    catalog_index_insert_index_id_bind = 1,
    catalog_index_insert_table_id_bind = 2,
    catalog_index_insert_name_bind = 3,
    catalog_index_insert_kind_bind = 4,
    catalog_index_insert_is_unique_bind = 5,
    catalog_index_insert_physical_name_bind = 6,
    catalog_index_insert_generation_bind = 7,
};

enum catalog_index_column_insert_bind_index {
    catalog_index_column_insert_index_id_bind = 1,
    catalog_index_column_insert_table_id_bind = 2,
    catalog_index_column_insert_column_id_bind = 3,
    catalog_index_column_insert_ordinal_position_bind = 4,
    catalog_index_column_insert_generation_bind = 5,
};

enum catalog_table_select_column_index {
    catalog_table_select_table_id_column = 0,
    catalog_table_select_schema_id_column = 1,
    catalog_table_select_name_column = 2,
    catalog_table_select_kind_column = 3,
    catalog_table_select_physical_name_column = 4,
    catalog_table_select_auto_increment_next_column = 5,
    catalog_table_select_descriptor_version_column = 6,
    catalog_table_select_created_generation_column = 7,
    catalog_table_select_updated_generation_column = 8,
};

enum catalog_column_select_column_index {
    catalog_column_select_column_id_column = 0,
    catalog_column_select_table_id_column = 1,
    catalog_column_select_ordinal_position_column = 2,
    catalog_column_select_name_column = 3,
    catalog_column_select_logical_type_column = 4,
    catalog_column_select_physical_type_column = 5,
    catalog_column_select_is_nullable_column = 6,
    catalog_column_select_is_visible_column = 7,
    catalog_column_select_is_auto_increment_column = 8,
    catalog_column_select_default_kind_column = 9,
    catalog_column_select_default_integer_column = 10,
    catalog_column_select_descriptor_version_column = 11,
    catalog_column_select_created_generation_column = 12,
    catalog_column_select_updated_generation_column = 13,
};

enum catalog_index_select_column_index {
    catalog_index_select_index_id_column = 0,
    catalog_index_select_table_id_column = 1,
    catalog_index_select_name_column = 2,
    catalog_index_select_kind_column = 3,
    catalog_index_select_is_unique_column = 4,
    catalog_index_select_physical_name_column = 5,
    catalog_index_select_descriptor_version_column = 6,
    catalog_index_select_created_generation_column = 7,
    catalog_index_select_updated_generation_column = 8,
};

enum catalog_index_column_select_column_index {
    catalog_index_column_select_index_column_id_column = 0,
    catalog_index_column_select_index_id_column = 1,
    catalog_index_column_select_table_id_column = 2,
    catalog_index_column_select_column_id_column = 3,
    catalog_index_column_select_ordinal_position_column = 4,
    catalog_index_column_select_descriptor_version_column = 5,
    catalog_index_column_select_created_generation_column = 6,
    catalog_index_column_select_updated_generation_column = 7,
};

enum catalog_next_table_id_column_index {
    catalog_next_table_id_column = 0,
};

enum catalog_state_select_column_index {
    catalog_state_select_singleton_id_column = 0,
    catalog_state_select_schema_version_column = 1,
    catalog_state_select_minimum_reader_schema_version_column = 2,
    catalog_state_select_catalog_generation_column = 3,
    catalog_state_select_file_format_version_column = 4,
};

struct catalog_generation_change {
    uint64_t next_generation;
};

static int ensure_catalog_schema(struct mylite_db *database);
static int load_existing_catalog(struct mylite_db *database);
static int migrate_catalog_schema(struct mylite_db *database, const struct mylite_catalog *catalog);
static int migrate_catalog_schema_v1_to_v2(sqlite3 *sqlite);
static int migrate_catalog_schema_v2_to_v3(sqlite3 *sqlite);
static int migrate_catalog_schema_v3_to_v4(sqlite3 *sqlite);
static int migrate_catalog_schema_v4_to_v5(sqlite3 *sqlite);
static int migrate_catalog_schema_v5_to_v6(sqlite3 *sqlite);
static int validate_catalog_descriptor_tables(sqlite3 *sqlite);
static int validate_select_shape(sqlite3 *sqlite, const char *sql);
static int initialize_catalog_schema(struct mylite_db *database);
static int existing_catalog_table_count(sqlite3 *sqlite, int *out_count);
static int read_catalog_state(sqlite3 *sqlite, struct mylite_catalog *catalog);
static int apply_catalog_state(struct mylite_db *database, const struct mylite_catalog *catalog);
static int begin_catalog_transaction(sqlite3 *sqlite);
static int commit_catalog_transaction(sqlite3 *sqlite);
static void rollback_catalog_transaction(sqlite3 *sqlite);
static int begin_generation_change(
    struct mylite_db *database,
    struct catalog_generation_change *out_change
);
static int finish_generation_change(
    struct mylite_db *database,
    const struct catalog_generation_change *change
);
static void abandon_generation_change(sqlite3 *sqlite);
static int update_catalog_generation(sqlite3 *sqlite, uint64_t generation);
static int execute_sql(sqlite3 *sqlite, const char *sql);
static int prepare_statement(sqlite3 *sqlite, const char *sql, sqlite3_stmt **out_statement);
static int bind_text(sqlite3_stmt *statement, int index, const char *value);
static int bind_i64(sqlite3_stmt *statement, int index, int64_t value);
static int bind_nullable_i64(sqlite3_stmt *statement, int index, bool has_value, int64_t value);
static int bind_u64(sqlite3_stmt *statement, int index, uint64_t value);
static int64_t catalog_bool_value(bool value);
static int step_done(sqlite3_stmt *statement);
static int require_changed_row(sqlite3 *sqlite);
static int finalize_statement(sqlite3_stmt *statement, int rc);
static int read_schema_by_name(
    sqlite3 *sqlite,
    const char *name,
    struct mylite_catalog_schema_descriptor *out_schema
);
static int try_read_schema_by_name(
    sqlite3 *sqlite,
    const char *name,
    struct mylite_catalog_schema_descriptor *out_schema,
    bool *out_found
);
static int read_schema_by_id(
    sqlite3 *sqlite,
    int64_t schema_id,
    struct mylite_catalog_schema_descriptor *out_schema
);
static int read_table_by_name(
    sqlite3 *sqlite,
    int64_t schema_id,
    const char *name,
    struct mylite_catalog_table_descriptor *out_table
);
static int try_read_table_by_name(
    sqlite3 *sqlite,
    int64_t schema_id,
    const char *name,
    struct mylite_catalog_table_descriptor *out_table,
    bool *out_found
);
static int read_table_by_id(
    sqlite3 *sqlite,
    int64_t table_id,
    struct mylite_catalog_table_descriptor *out_table
);
static int read_column_by_name(
    sqlite3 *sqlite,
    int64_t table_id,
    const char *name,
    struct mylite_catalog_column_descriptor *out_column
);
static int try_read_primary_index_by_table_id(
    sqlite3 *sqlite,
    int64_t table_id,
    struct mylite_catalog_index_descriptor *out_index,
    bool *out_found
);
static int read_next_table_id(sqlite3 *sqlite, int64_t *out_table_id);
static int read_next_index_id(sqlite3 *sqlite, int64_t *out_index_id);
static int materialize_schema(
    sqlite3_stmt *statement,
    struct mylite_catalog_schema_descriptor *out_schema
);
static int materialize_table(
    sqlite3_stmt *statement,
    struct mylite_catalog_table_descriptor *out_table
);
static int materialize_column(
    sqlite3_stmt *statement,
    struct mylite_catalog_column_descriptor *out_column
);
static int materialize_index(
    sqlite3_stmt *statement,
    struct mylite_catalog_index_descriptor *out_index
);
static int materialize_index_column(
    sqlite3_stmt *statement,
    struct mylite_catalog_index_column_descriptor *out_index_column
);
static int checked_column_i64(sqlite3_stmt *statement, int index, int64_t *out_value);
static int checked_column_u64(sqlite3_stmt *statement, int index, uint64_t *out_value);
static int checked_nullable_column_i64(
    sqlite3_stmt *statement,
    int index,
    bool *out_has_value,
    int64_t *out_value
);
static int checked_column_text(
    sqlite3_stmt *statement,
    int index,
    char *destination,
    size_t destination_size
);
static int validate_database(struct mylite_db *database);
static int validate_catalog_ready_database(struct mylite_db *database);
static int validate_required_name(const char *name, size_t capacity);
static int validate_logical_object_name(const char *name, size_t capacity);
static int validate_table_kind(enum mylite_catalog_table_kind kind);
static int validate_column_default_kind(enum mylite_catalog_column_default_kind kind);
static int validate_index_kind(enum mylite_catalog_index_kind kind);
static int validate_active_mutation(const struct mylite_catalog_mutation *mutation);
static int validate_positive_id(int64_t id);
static int validate_positive_ordinal(int64_t ordinal_position);
static int validate_generation(uint64_t generation);
static int validate_schema_callback(mylite_catalog_schema_callback callback);
static int validate_callback(mylite_catalog_table_callback callback);
static int validate_column_callback(mylite_catalog_column_callback callback);
static int validate_index_callback(mylite_catalog_index_callback callback);
static int validate_index_column_callback(mylite_catalog_index_column_callback callback);
static int u64_to_i64(uint64_t value, int64_t *out_value);
static int i64_to_u32(int64_t value, uint32_t *out_value);
static int i64_to_u64(int64_t value, uint64_t *out_value);
static int text_has_ascii_case_insensitive_prefix(const char *text, const char *prefix);
static char ascii_lower(unsigned char byte);
static void reset_descriptor_cache_state(struct mylite_catalog *catalog);

static const char *catalog_state_table_name(void);
static const char *catalog_schemas_table_name(void);
static const char *catalog_tables_table_name(void);
static const char *catalog_columns_table_name(void);
static const char *catalog_indexes_table_name(void);
static const char *catalog_index_columns_table_name(void);

void mylite_catalog_init(struct mylite_catalog *catalog) {
    if (catalog == NULL) {
        return;
    }

    memset(catalog, 0, sizeof(*catalog));
}

void mylite_catalog_deinit(struct mylite_catalog *catalog) {
    if (catalog == NULL) {
        return;
    }

    memset(catalog, 0, sizeof(*catalog));
}

int mylite_catalog_initialize_file_backed(struct mylite_db *database) {
    int rc = MYLITE_OK;

    rc = validate_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }

    mylite_catalog_deinit(&database->catalog);

    rc = ensure_catalog_schema(database);
    if (rc != MYLITE_OK) {
        mylite_catalog_deinit(&database->catalog);
        return rc;
    }

    database->catalog.initialized = true;
    reset_descriptor_cache_state(&database->catalog);
    database->session.catalog_generation = database->catalog.generation;

    return MYLITE_OK;
}

void mylite_catalog_invalidate_descriptor_cache(struct mylite_db *database) {
    if (database == NULL) {
        return;
    }

    reset_descriptor_cache_state(&database->catalog);
}

void mylite_catalog_mutation_init(struct mylite_catalog_mutation *mutation) {
    if (mutation == NULL) {
        return;
    }

    *mutation = (struct mylite_catalog_mutation){.active = false, .next_generation = 0U};
}

void mylite_catalog_mutation_deinit(struct mylite_catalog_mutation *mutation) {
    if (mutation == NULL) {
        return;
    }

    *mutation = (struct mylite_catalog_mutation){.active = false, .next_generation = 0U};
}

int mylite_catalog_begin_mutation(
    struct mylite_db *database,
    struct mylite_catalog_mutation *mutation
) {
    struct mylite_catalog catalog = {.initialized = false};
    int rc = MYLITE_OK;

    if (mutation != NULL) {
        mylite_catalog_mutation_init(mutation);
    }
    rc = validate_catalog_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (mutation == NULL) {
        return MYLITE_MISUSE;
    }

    rc = begin_catalog_transaction(database->sqlite);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = read_catalog_state(database->sqlite, &catalog);
    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(database->sqlite);
        return rc;
    }
    if (catalog.generation == UINT64_MAX) {
        rollback_catalog_transaction(database->sqlite);
        return MYLITE_ERROR;
    }

    mutation->active = true;
    mutation->next_generation = catalog.generation + 1U;

    return MYLITE_OK;
}

int mylite_catalog_commit_mutation(
    struct mylite_db *database,
    struct mylite_catalog_mutation *mutation
) {
    int rc = validate_catalog_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = update_catalog_generation(database->sqlite, mutation->next_generation);
    if (rc == MYLITE_OK) {
        rc = commit_catalog_transaction(database->sqlite);
    }
    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(database->sqlite);
        mylite_catalog_mutation_deinit(mutation);
        return rc;
    }

    database->catalog.generation = mutation->next_generation;
    database->session.catalog_generation = mutation->next_generation;
    reset_descriptor_cache_state(&database->catalog);
    mylite_catalog_mutation_deinit(mutation);

    return MYLITE_OK;
}

void mylite_catalog_rollback_mutation(
    struct mylite_db *database,
    struct mylite_catalog_mutation *mutation
) {
    if (database != NULL && mutation != NULL && mutation->active) {
        rollback_catalog_transaction(database->sqlite);
    }
    mylite_catalog_mutation_deinit(mutation);
}

uint64_t mylite_catalog_mutation_generation(const struct mylite_catalog_mutation *mutation) {
    if (mutation == NULL || !mutation->active) {
        return 0U;
    }

    return mutation->next_generation;
}

int mylite_catalog_allocate_table_id_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t *out_table_id
) {
    int rc = validate_catalog_ready_database(database);

    if (out_table_id != NULL) {
        *out_table_id = 0;
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_table_id == NULL) {
        return MYLITE_MISUSE;
    }

    return read_next_table_id(database->sqlite, out_table_id);
}

int mylite_catalog_allocate_index_id_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t *out_index_id
) {
    int rc = validate_catalog_ready_database(database);

    if (out_index_id != NULL) {
        *out_index_id = 0;
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_index_id == NULL) {
        return MYLITE_MISUSE;
    }

    return read_next_index_id(database->sqlite, out_index_id);
}

int mylite_catalog_insert_table_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    int64_t schema_id,
    const char *name,
    const char *physical_name,
    enum mylite_catalog_table_kind kind,
    int64_t auto_increment_next,
    struct mylite_catalog_table_descriptor *out_table
) {
    struct mylite_catalog_schema_descriptor schema = {0};
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    if (out_table != NULL) {
        *out_table = (struct mylite_catalog_table_descriptor){0};
    }
    rc = validate_catalog_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(schema_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_logical_object_name(name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_required_name(physical_name, MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_table_kind(kind);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (auto_increment_next <= 0) {
        return MYLITE_ERROR;
    }

    rc = read_schema_by_id(database->sqlite, schema_id, &schema);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "INSERT INTO _mylite_catalog_tables "
        "(table_id, schema_id, name, kind, physical_name, auto_increment_next, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, 1, ?7, ?7)",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, catalog_table_insert_in_mutation_table_id_bind, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, catalog_table_insert_in_mutation_schema_id_bind, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, catalog_table_insert_in_mutation_name_bind, name);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, catalog_table_insert_in_mutation_kind_bind, (int64_t)kind);
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(
            statement,
            catalog_table_insert_in_mutation_physical_name_bind,
            physical_name
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(
            statement,
            catalog_table_insert_in_mutation_auto_increment_next_bind,
            auto_increment_next
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_u64(
            statement,
            catalog_table_insert_in_mutation_generation_bind,
            mutation->next_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    rc = finalize_statement(statement, rc);
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (out_table != NULL) {
        return read_table_by_id(database->sqlite, table_id, out_table);
    }

    return MYLITE_OK;
}

int mylite_catalog_insert_column_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    int64_t ordinal_position,
    const char *name,
    const char *logical_type,
    const char *physical_type,
    bool is_nullable,
    bool is_visible,
    bool is_auto_increment,
    enum mylite_catalog_column_default_kind default_kind,
    int64_t default_integer,
    struct mylite_catalog_column_descriptor *out_column
) {
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    if (out_column != NULL) {
        *out_column = (struct mylite_catalog_column_descriptor){0};
    }
    rc = validate_catalog_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_ordinal(ordinal_position);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_required_name(name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_required_name(logical_type, MYLITE_CATALOG_TYPE_NAME_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_required_name(physical_type, MYLITE_CATALOG_TYPE_NAME_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_column_default_kind(default_kind);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "INSERT INTO _mylite_catalog_columns "
        "(table_id, ordinal_position, name, logical_type, physical_type, is_nullable, "
        "is_visible, is_auto_increment, default_kind, default_integer, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, 1, ?11, ?11)",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, catalog_column_insert_table_id_bind, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, catalog_column_insert_ordinal_position_bind, ordinal_position);
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, catalog_column_insert_name_bind, name);
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, catalog_column_insert_logical_type_bind, logical_type);
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, catalog_column_insert_physical_type_bind, physical_type);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(
            statement,
            catalog_column_insert_is_nullable_bind,
            catalog_bool_value(is_nullable)
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(
            statement,
            catalog_column_insert_is_visible_bind,
            catalog_bool_value(is_visible)
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(
            statement,
            catalog_column_insert_is_auto_increment_bind,
            catalog_bool_value(is_auto_increment)
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, catalog_column_insert_default_kind_bind, (int64_t)default_kind);
    }
    if (rc == MYLITE_OK) {
        rc = bind_nullable_i64(
            statement,
            catalog_column_insert_default_integer_bind,
            default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER,
            default_integer
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_u64(statement, catalog_column_insert_generation_bind, mutation->next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    rc = finalize_statement(statement, rc);
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (out_column != NULL) {
        return read_column_by_name(database->sqlite, table_id, name, out_column);
    }

    return MYLITE_OK;
}

int mylite_catalog_insert_index_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t index_id,
    int64_t table_id,
    const char *name,
    const char *physical_name,
    enum mylite_catalog_index_kind kind,
    bool is_unique,
    struct mylite_catalog_index_descriptor *out_index
) {
    struct mylite_catalog_table_descriptor table = {0};
    sqlite3_stmt *statement = NULL;
    int64_t unique_value = 0;
    int rc = MYLITE_OK;

    if (is_unique) {
        unique_value = 1;
    }
    if (out_index != NULL) {
        *out_index = (struct mylite_catalog_index_descriptor){0};
    }
    rc = validate_catalog_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(index_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_required_name(name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_required_name(physical_name, MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_index_kind(kind);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = read_table_by_id(database->sqlite, table_id, &table);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "INSERT INTO _mylite_catalog_indexes "
        "(index_id, table_id, name, kind, is_unique, physical_name, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, 1, ?7, ?7)",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, catalog_index_insert_index_id_bind, index_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, catalog_index_insert_table_id_bind, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, catalog_index_insert_name_bind, name);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, catalog_index_insert_kind_bind, (int64_t)kind);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, catalog_index_insert_is_unique_bind, unique_value);
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, catalog_index_insert_physical_name_bind, physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = bind_u64(statement, catalog_index_insert_generation_bind, mutation->next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    rc = finalize_statement(statement, rc);
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (out_index != NULL) {
        bool found = false;

        rc = try_read_primary_index_by_table_id(database->sqlite, table_id, out_index, &found);
        if (rc == MYLITE_OK && !found) {
            rc = MYLITE_ERROR;
        }
        return rc;
    }

    return MYLITE_OK;
}

int mylite_catalog_insert_index_column_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t index_id,
    int64_t table_id,
    int64_t column_id,
    int64_t ordinal_position,
    struct mylite_catalog_index_column_descriptor *out_index_column
) {
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    if (out_index_column != NULL) {
        *out_index_column = (struct mylite_catalog_index_column_descriptor){0};
    }
    rc = validate_catalog_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(index_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(column_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_ordinal(ordinal_position);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "INSERT INTO _mylite_catalog_index_columns "
        "(index_id, table_id, column_id, ordinal_position, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation) "
        "VALUES (?1, ?2, ?3, ?4, 1, ?5, ?5)",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, catalog_index_column_insert_index_id_bind, index_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, catalog_index_column_insert_table_id_bind, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, catalog_index_column_insert_column_id_bind, column_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(
            statement,
            catalog_index_column_insert_ordinal_position_bind,
            ordinal_position
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_u64(
            statement,
            catalog_index_column_insert_generation_bind,
            mutation->next_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    rc = finalize_statement(statement, rc);
    statement = NULL;
    if (rc != MYLITE_OK || out_index_column == NULL) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "SELECT index_column_id, index_id, table_id, column_id, ordinal_position, "
        "descriptor_version, created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_index_columns "
        "WHERE index_id = ?1 AND ordinal_position = ?2",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, index_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 2, ordinal_position);
    }
    if (rc == MYLITE_OK) {
        int sqlite_rc = sqlite3_step(statement);

        if (sqlite_rc == SQLITE_ROW) {
            rc = materialize_index_column(statement, out_index_column);
        } else {
            rc =
                sqlite_rc == SQLITE_DONE ? MYLITE_ERROR : mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }

    return finalize_statement(statement, rc);
}

int mylite_catalog_delete_table_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id
) {
    sqlite3_stmt *statement = NULL;
    int rc = validate_catalog_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "DELETE FROM _mylite_catalog_index_columns WHERE table_id = ?1",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    rc = finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_indexes WHERE table_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    rc = finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_columns WHERE table_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    rc = finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_tables WHERE table_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = require_changed_row(database->sqlite);
    }

    return finalize_statement(statement, rc);
}

int mylite_catalog_delete_column_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    int64_t column_id,
    int64_t ordinal_position
) {
    sqlite3_stmt *statement = NULL;
    int rc = validate_catalog_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(column_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_ordinal(ordinal_position);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "DELETE FROM _mylite_catalog_columns WHERE table_id = ?1 AND column_id = ?2",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 2, column_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = require_changed_row(database->sqlite);
    }
    rc = finalize_statement(statement, rc);
    statement = NULL;

    if (rc == MYLITE_OK) {
        rc = prepare_statement(
            database->sqlite,
            "UPDATE _mylite_catalog_columns "
            "SET ordinal_position = ordinal_position - 1, "
            "descriptor_version = descriptor_version + 1, "
            "updated_catalog_generation = ?1 "
            "WHERE table_id = ?2 AND ordinal_position > ?3",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_u64(statement, 1, mutation->next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 2, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 3, ordinal_position);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }

    return finalize_statement(statement, rc);
}

int mylite_catalog_rename_column_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    int64_t column_id,
    const char *name
) {
    sqlite3_stmt *statement = NULL;
    int rc = validate_catalog_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(column_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_logical_object_name(name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_columns "
        "SET name = ?1, descriptor_version = descriptor_version + 1, "
        "updated_catalog_generation = ?2 "
        "WHERE table_id = ?3 AND column_id = ?4",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, 1, name);
    }
    if (rc == MYLITE_OK) {
        rc = bind_u64(statement, 2, mutation->next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 3, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 4, column_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = require_changed_row(database->sqlite);
    }

    return finalize_statement(statement, rc);
}

int mylite_catalog_replace_column_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    int64_t column_id,
    const char *name,
    const char *logical_type,
    const char *physical_type,
    bool is_nullable,
    bool is_visible,
    bool is_auto_increment,
    enum mylite_catalog_column_default_kind default_kind,
    int64_t default_integer
) {
    sqlite3_stmt *statement = NULL;
    int64_t nullable_value = 0;
    int64_t visible_value = 0;
    int64_t auto_increment_value = 0;
    int rc = validate_catalog_ready_database(database);

    if (is_nullable) {
        nullable_value = 1;
    }
    if (is_visible) {
        visible_value = 1;
    }
    if (is_auto_increment) {
        auto_increment_value = 1;
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(column_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_logical_object_name(name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_required_name(logical_type, MYLITE_CATALOG_TYPE_NAME_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_required_name(physical_type, MYLITE_CATALOG_TYPE_NAME_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_column_default_kind(default_kind);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_columns "
        "SET name = ?1, logical_type = ?2, physical_type = ?3, is_nullable = ?4, "
        "is_visible = ?5, is_auto_increment = ?6, default_kind = ?7, default_integer = ?8, "
        "descriptor_version = descriptor_version + 1, updated_catalog_generation = ?9 "
        "WHERE table_id = ?10 AND column_id = ?11",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, catalog_column_replace_name_bind, name);
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, catalog_column_replace_logical_type_bind, logical_type);
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, catalog_column_replace_physical_type_bind, physical_type);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, catalog_column_replace_is_nullable_bind, nullable_value);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, catalog_column_replace_is_visible_bind, visible_value);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(
            statement,
            catalog_column_replace_is_auto_increment_bind,
            auto_increment_value
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, catalog_column_replace_default_kind_bind, (int64_t)default_kind);
    }
    if (rc == MYLITE_OK) {
        rc = bind_nullable_i64(
            statement,
            catalog_column_replace_default_integer_bind,
            default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER,
            default_integer
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_u64(statement, catalog_column_replace_generation_bind, mutation->next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, catalog_column_replace_table_id_bind, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, catalog_column_replace_column_id_bind, column_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = require_changed_row(database->sqlite);
    }

    return finalize_statement(statement, rc);
}

int mylite_catalog_set_column_visibility_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    int64_t column_id,
    bool is_visible
) {
    sqlite3_stmt *statement = NULL;
    int64_t visible_value = 0;
    int rc = validate_catalog_ready_database(database);

    if (is_visible) {
        visible_value = 1;
    }

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(column_id);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_columns "
        "SET is_visible = ?1, descriptor_version = descriptor_version + 1, "
        "updated_catalog_generation = ?2 "
        "WHERE table_id = ?3 AND column_id = ?4",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, visible_value);
    }
    if (rc == MYLITE_OK) {
        rc = bind_u64(statement, 2, mutation->next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 3, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 4, column_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = require_changed_row(database->sqlite);
    }

    return finalize_statement(statement, rc);
}

int mylite_catalog_delete_schema_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t schema_id
) {
    sqlite3_stmt *statement = NULL;
    int rc = validate_catalog_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(schema_id);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "DELETE FROM _mylite_catalog_index_columns "
        "WHERE table_id IN ("
        "SELECT table_id FROM _mylite_catalog_tables WHERE schema_id = ?1"
        ")",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    rc = finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_indexes "
            "WHERE table_id IN ("
            "SELECT table_id FROM _mylite_catalog_tables WHERE schema_id = ?1"
            ")",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    rc = finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_columns "
            "WHERE table_id IN ("
            "SELECT table_id FROM _mylite_catalog_tables WHERE schema_id = ?1"
            ")",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    rc = finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_tables WHERE schema_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    rc = finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_schemas WHERE schema_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = require_changed_row(database->sqlite);
    }

    return finalize_statement(statement, rc);
}

int mylite_catalog_update_table_identity_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    int64_t schema_id,
    const char *name,
    struct mylite_catalog_table_descriptor *out_table
) {
    sqlite3_stmt *statement = NULL;
    struct mylite_catalog_schema_descriptor schema = {0};
    int rc = MYLITE_OK;

    if (out_table != NULL) {
        *out_table = (struct mylite_catalog_table_descriptor){0};
    }
    rc = validate_catalog_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(schema_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_logical_object_name(name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = read_schema_by_id(database->sqlite, schema_id, &schema);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_tables "
        "SET schema_id = ?1, name = ?2, descriptor_version = descriptor_version + 1, "
        "updated_catalog_generation = ?3 "
        "WHERE table_id = ?4",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, 2, name);
    }
    if (rc == MYLITE_OK) {
        rc = bind_u64(statement, 3, mutation->next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 4, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = require_changed_row(database->sqlite);
    }
    rc = finalize_statement(statement, rc);
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (out_table != NULL) {
        return read_table_by_id(database->sqlite, table_id, out_table);
    }

    return MYLITE_OK;
}

int mylite_catalog_update_table_auto_increment_next(
    struct mylite_db *database,
    int64_t table_id,
    int64_t auto_increment_next
) {
    sqlite3_stmt *statement = NULL;
    int rc = validate_catalog_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (auto_increment_next <= 0) {
        return MYLITE_ERROR;
    }

    rc = prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_tables SET auto_increment_next = ?1 WHERE table_id = ?2",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, auto_increment_next);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 2, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = require_changed_row(database->sqlite);
    }
    rc = finalize_statement(statement, rc);
    if (rc == MYLITE_OK) {
        mylite_catalog_invalidate_descriptor_cache(database);
    }
    return rc;
}

int mylite_catalog_for_each_schema(
    struct mylite_db *database,
    mylite_catalog_schema_callback callback,
    void *user_data
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = validate_catalog_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_schema_callback(callback);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "SELECT schema_id, name, descriptor_version, created_catalog_generation, "
        "updated_catalog_generation FROM _mylite_catalog_schemas ORDER BY name",
        &statement
    );
    while (rc == MYLITE_OK) {
        struct mylite_catalog_schema_descriptor schema = {0};

        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            break;
        }
        if (sqlite_rc != SQLITE_ROW) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
            break;
        }

        rc = materialize_schema(statement, &schema);
        if (rc == MYLITE_OK) {
            rc = callback(&schema, user_data);
        }
    }

    return finalize_statement(statement, rc);
}

int mylite_catalog_for_each_table_in_schema(
    struct mylite_db *database,
    int64_t schema_id,
    mylite_catalog_table_callback callback,
    void *user_data
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = validate_catalog_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(schema_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_callback(callback);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "SELECT table_id, schema_id, name, kind, physical_name, auto_increment_next, "
        "descriptor_version, created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_tables WHERE schema_id = ?1 ORDER BY name",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, schema_id);
    }
    while (rc == MYLITE_OK) {
        struct mylite_catalog_table_descriptor table = {0};

        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            break;
        }
        if (sqlite_rc != SQLITE_ROW) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
            break;
        }

        rc = materialize_table(statement, &table);
        if (rc == MYLITE_OK) {
            rc = callback(&table, user_data);
        }
    }

    return finalize_statement(statement, rc);
}

int mylite_catalog_for_each_column_in_table(
    struct mylite_db *database,
    int64_t table_id,
    mylite_catalog_column_callback callback,
    void *user_data
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = validate_catalog_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_column_callback(callback);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "SELECT column_id, table_id, ordinal_position, name, logical_type, physical_type, "
        "is_nullable, is_visible, is_auto_increment, default_kind, default_integer, "
        "descriptor_version, created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_columns WHERE table_id = ?1 ORDER BY ordinal_position",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, table_id);
    }
    while (rc == MYLITE_OK) {
        struct mylite_catalog_column_descriptor column = {0};

        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            break;
        }
        if (sqlite_rc != SQLITE_ROW) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
            break;
        }

        rc = materialize_column(statement, &column);
        if (rc == MYLITE_OK) {
            rc = callback(&column, user_data);
        }
    }

    return finalize_statement(statement, rc);
}

int mylite_catalog_for_each_index_in_table(
    struct mylite_db *database,
    int64_t table_id,
    mylite_catalog_index_callback callback,
    void *user_data
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = validate_catalog_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_index_callback(callback);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "SELECT index_id, table_id, name, kind, is_unique, physical_name, "
        "descriptor_version, created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_indexes WHERE table_id = ?1 ORDER BY name",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, table_id);
    }
    while (rc == MYLITE_OK) {
        struct mylite_catalog_index_descriptor index = {0};

        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            break;
        }
        if (sqlite_rc != SQLITE_ROW) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
            break;
        }

        rc = materialize_index(statement, &index);
        if (rc == MYLITE_OK) {
            rc = callback(&index, user_data);
        }
    }

    return finalize_statement(statement, rc);
}

int mylite_catalog_for_each_index_column_in_index(
    struct mylite_db *database,
    int64_t index_id,
    mylite_catalog_index_column_callback callback,
    void *user_data
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = validate_catalog_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(index_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_index_column_callback(callback);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "SELECT index_column_id, index_id, table_id, column_id, ordinal_position, "
        "descriptor_version, created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_index_columns "
        "WHERE index_id = ?1 ORDER BY ordinal_position",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, index_id);
    }
    while (rc == MYLITE_OK) {
        struct mylite_catalog_index_column_descriptor index_column = {0};

        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            break;
        }
        if (sqlite_rc != SQLITE_ROW) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
            break;
        }

        rc = materialize_index_column(statement, &index_column);
        if (rc == MYLITE_OK) {
            rc = callback(&index_column, user_data);
        }
    }

    return finalize_statement(statement, rc);
}

int mylite_catalog_try_read_primary_index_by_table_id(
    struct mylite_db *database,
    int64_t table_id,
    struct mylite_catalog_index_descriptor *out_index,
    bool *out_found
) {
    int rc = validate_catalog_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_index == NULL || out_found == NULL) {
        return MYLITE_MISUSE;
    }
    *out_found = false;
    rc = validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }

    return try_read_primary_index_by_table_id(database->sqlite, table_id, out_index, out_found);
}

int mylite_catalog_create_schema(
    struct mylite_db *database,
    const char *name,
    struct mylite_catalog_schema_descriptor *out_schema
) {
    struct catalog_generation_change generation = {0};
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    if (out_schema != NULL) {
        *out_schema = (struct mylite_catalog_schema_descriptor){0};
    }
    rc = validate_catalog_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_logical_object_name(name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = begin_generation_change(database, &generation);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "INSERT INTO _mylite_catalog_schemas "
        "(name, descriptor_version, created_catalog_generation, updated_catalog_generation) "
        "VALUES (?1, 1, ?2, ?2)",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, 1, name);
    }
    if (rc == MYLITE_OK) {
        rc = bind_u64(statement, 2, generation.next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = require_changed_row(database->sqlite);
    }
    rc = finalize_statement(statement, rc);
    if (rc == MYLITE_OK) {
        rc = finish_generation_change(database, &generation);
    }
    if (rc != MYLITE_OK) {
        abandon_generation_change(database->sqlite);
        return rc;
    }

    if (out_schema != NULL) {
        return read_schema_by_name(database->sqlite, name, out_schema);
    }

    return MYLITE_OK;
}

int mylite_catalog_read_schema_by_name(
    struct mylite_db *database,
    const char *name,
    struct mylite_catalog_schema_descriptor *out_schema
) {
    bool found = false;
    int rc = mylite_catalog_try_read_schema_by_name(database, name, out_schema, &found);

    if (rc != MYLITE_OK) {
        return rc;
    }

    if (!found) {
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

int mylite_catalog_try_read_schema_by_name(
    struct mylite_db *database,
    const char *name,
    struct mylite_catalog_schema_descriptor *out_schema,
    bool *out_found
) {
    int rc = validate_catalog_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_schema == NULL || out_found == NULL) {
        return MYLITE_MISUSE;
    }
    *out_found = false;
    rc = validate_required_name(name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }

    return try_read_schema_by_name(database->sqlite, name, out_schema, out_found);
}

int mylite_catalog_delete_schema(struct mylite_db *database, int64_t schema_id) {
    struct catalog_generation_change generation = {0};
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    rc = validate_catalog_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(schema_id);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = begin_generation_change(database, &generation);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "DELETE FROM _mylite_catalog_index_columns "
        "WHERE table_id IN ("
        "SELECT table_id FROM _mylite_catalog_tables WHERE schema_id = ?1"
        ")",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    rc = finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_indexes "
            "WHERE table_id IN ("
            "SELECT table_id FROM _mylite_catalog_tables WHERE schema_id = ?1"
            ")",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    rc = finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_columns "
            "WHERE table_id IN ("
            "SELECT table_id FROM _mylite_catalog_tables WHERE schema_id = ?1"
            ")",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    rc = finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_tables WHERE schema_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    rc = finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_schemas WHERE schema_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = require_changed_row(database->sqlite);
    }
    rc = finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = finish_generation_change(database, &generation);
    }
    if (rc != MYLITE_OK) {
        abandon_generation_change(database->sqlite);
        return rc;
    }

    return MYLITE_OK;
}

int mylite_catalog_create_table(
    struct mylite_db *database,
    int64_t schema_id,
    const char *name,
    const char *physical_name,
    enum mylite_catalog_table_kind kind,
    struct mylite_catalog_table_descriptor *out_table
) {
    struct catalog_generation_change generation = {0};
    struct mylite_catalog_schema_descriptor schema = {0};
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    if (out_table != NULL) {
        *out_table = (struct mylite_catalog_table_descriptor){0};
    }
    rc = validate_catalog_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(schema_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_logical_object_name(name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_required_name(physical_name, MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_table_kind(kind);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = begin_generation_change(database, &generation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = read_schema_by_id(database->sqlite, schema_id, &schema);
    if (rc != MYLITE_OK) {
        abandon_generation_change(database->sqlite);
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "INSERT INTO _mylite_catalog_tables "
        "(schema_id, name, kind, physical_name, auto_increment_next, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation) "
        "VALUES (?1, ?2, ?3, ?4, ?5, 1, ?6, ?6)",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, catalog_table_insert_schema_id_bind, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, catalog_table_insert_name_bind, name);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, catalog_table_insert_kind_bind, (int64_t)kind);
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, catalog_table_insert_physical_name_bind, physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, catalog_table_insert_auto_increment_next_bind, 1);
    }
    if (rc == MYLITE_OK) {
        rc = bind_u64(statement, catalog_table_insert_generation_bind, generation.next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    rc = finalize_statement(statement, rc);
    if (rc == MYLITE_OK) {
        rc = finish_generation_change(database, &generation);
    }
    if (rc != MYLITE_OK) {
        abandon_generation_change(database->sqlite);
        return rc;
    }

    if (out_table != NULL) {
        return read_table_by_name(database->sqlite, schema_id, name, out_table);
    }

    return MYLITE_OK;
}

int mylite_catalog_read_table_by_name(
    struct mylite_db *database,
    int64_t schema_id,
    const char *name,
    struct mylite_catalog_table_descriptor *out_table
) {
    bool found = false;
    int rc = mylite_catalog_try_read_table_by_name(database, schema_id, name, out_table, &found);

    if (rc != MYLITE_OK) {
        return rc;
    }

    if (!found) {
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

int mylite_catalog_try_read_table_by_name(
    struct mylite_db *database,
    int64_t schema_id,
    const char *name,
    struct mylite_catalog_table_descriptor *out_table,
    bool *out_found
) {
    int rc = validate_catalog_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_table == NULL || out_found == NULL) {
        return MYLITE_MISUSE;
    }
    *out_found = false;
    rc = validate_positive_id(schema_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_required_name(name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }

    return try_read_table_by_name(database->sqlite, schema_id, name, out_table, out_found);
}

int mylite_catalog_update_table_name(
    struct mylite_db *database,
    int64_t table_id,
    const char *name
) {
    struct catalog_generation_change generation = {0};
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    rc = validate_catalog_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_logical_object_name(name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = begin_generation_change(database, &generation);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_tables "
        "SET name = ?1, descriptor_version = descriptor_version + 1, "
        "updated_catalog_generation = ?2 "
        "WHERE table_id = ?3",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, 1, name);
    }
    if (rc == MYLITE_OK) {
        rc = bind_u64(statement, 2, generation.next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 3, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = require_changed_row(database->sqlite);
    }
    rc = finalize_statement(statement, rc);
    if (rc == MYLITE_OK) {
        rc = finish_generation_change(database, &generation);
    }
    if (rc != MYLITE_OK) {
        abandon_generation_change(database->sqlite);
        return rc;
    }

    return MYLITE_OK;
}

int mylite_catalog_delete_table(struct mylite_db *database, int64_t table_id) {
    struct catalog_generation_change generation = {0};
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    rc = validate_catalog_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = begin_generation_change(database, &generation);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "DELETE FROM _mylite_catalog_index_columns WHERE table_id = ?1",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    rc = finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_indexes WHERE table_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    rc = finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_columns WHERE table_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    rc = finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_tables WHERE table_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = require_changed_row(database->sqlite);
    }
    rc = finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = finish_generation_change(database, &generation);
    }
    if (rc != MYLITE_OK) {
        abandon_generation_change(database->sqlite);
        return rc;
    }

    return MYLITE_OK;
}

int mylite_catalog_create_column(
    struct mylite_db *database,
    int64_t table_id,
    int64_t ordinal_position,
    const char *name,
    const char *logical_type,
    const char *physical_type,
    bool is_nullable,
    enum mylite_catalog_column_default_kind default_kind,
    int64_t default_integer,
    struct mylite_catalog_column_descriptor *out_column
) {
    struct catalog_generation_change generation = {0};
    struct mylite_catalog_table_descriptor table = {0};
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    if (out_column != NULL) {
        *out_column = (struct mylite_catalog_column_descriptor){0};
    }
    rc = validate_catalog_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_ordinal(ordinal_position);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_required_name(name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_required_name(logical_type, MYLITE_CATALOG_TYPE_NAME_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_required_name(physical_type, MYLITE_CATALOG_TYPE_NAME_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_column_default_kind(default_kind);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = begin_generation_change(database, &generation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = read_table_by_id(database->sqlite, table_id, &table);
    if (rc != MYLITE_OK) {
        abandon_generation_change(database->sqlite);
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "INSERT INTO _mylite_catalog_columns "
        "(table_id, ordinal_position, name, logical_type, physical_type, is_nullable, "
        "is_visible, is_auto_increment, default_kind, default_integer, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, 1, ?11, ?11)",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, catalog_column_insert_table_id_bind, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, catalog_column_insert_ordinal_position_bind, ordinal_position);
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, catalog_column_insert_name_bind, name);
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, catalog_column_insert_logical_type_bind, logical_type);
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, catalog_column_insert_physical_type_bind, physical_type);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(
            statement,
            catalog_column_insert_is_nullable_bind,
            catalog_bool_value(is_nullable)
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, catalog_column_insert_is_visible_bind, 1);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, catalog_column_insert_is_auto_increment_bind, 0);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, catalog_column_insert_default_kind_bind, (int64_t)default_kind);
    }
    if (rc == MYLITE_OK) {
        rc = bind_nullable_i64(
            statement,
            catalog_column_insert_default_integer_bind,
            default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER,
            default_integer
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_u64(statement, catalog_column_insert_generation_bind, generation.next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    rc = finalize_statement(statement, rc);
    if (rc == MYLITE_OK) {
        rc = finish_generation_change(database, &generation);
    }
    if (rc != MYLITE_OK) {
        abandon_generation_change(database->sqlite);
        return rc;
    }

    if (out_column != NULL) {
        return read_column_by_name(database->sqlite, table_id, name, out_column);
    }

    return MYLITE_OK;
}

int mylite_catalog_read_column_by_name(
    struct mylite_db *database,
    int64_t table_id,
    const char *name,
    struct mylite_catalog_column_descriptor *out_column
) {
    int rc = validate_catalog_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_column == NULL) {
        return MYLITE_MISUSE;
    }
    rc = validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_required_name(name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }

    return read_column_by_name(database->sqlite, table_id, name, out_column);
}

int mylite_catalog_delete_column(struct mylite_db *database, int64_t column_id) {
    struct catalog_generation_change generation = {0};
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    rc = validate_catalog_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(column_id);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = begin_generation_change(database, &generation);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "DELETE FROM _mylite_catalog_columns WHERE column_id = ?1",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, column_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = require_changed_row(database->sqlite);
    }
    rc = finalize_statement(statement, rc);
    if (rc == MYLITE_OK) {
        rc = finish_generation_change(database, &generation);
    }
    if (rc != MYLITE_OK) {
        abandon_generation_change(database->sqlite);
        return rc;
    }

    return MYLITE_OK;
}

bool mylite_catalog_name_is_reserved(const char *name) {
    static const char prefix[] = "_mylite_";

    if (name == NULL) {
        return false;
    }

    return text_has_ascii_case_insensitive_prefix(name, prefix) != 0;
}

static int ensure_catalog_schema(struct mylite_db *database) {
    int table_count = 0;
    int rc = existing_catalog_table_count(database->sqlite, &table_count);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (table_count == 0) {
        return initialize_catalog_schema(database);
    }
    if (table_count != legacy_catalog_table_count && table_count != catalog_table_count) {
        return MYLITE_ERROR;
    }

    return load_existing_catalog(database);
}

static int load_existing_catalog(struct mylite_db *database) {
    struct mylite_catalog catalog = {.initialized = false};
    int rc = read_catalog_state(database->sqlite, &catalog);

    if (rc == MYLITE_OK && catalog.schema_version < MYLITE_CATALOG_SCHEMA_VERSION) {
        rc = migrate_catalog_schema(database, &catalog);
    }
    if (rc == MYLITE_OK) {
        rc = validate_catalog_descriptor_tables(database->sqlite);
    }
    if (rc == MYLITE_OK) {
        rc = read_catalog_state(database->sqlite, &catalog);
    }

    if (rc != MYLITE_OK) {
        return rc;
    }

    return apply_catalog_state(database, &catalog);
}

static int migrate_catalog_schema(
    struct mylite_db *database,
    const struct mylite_catalog *catalog
) {
    uint32_t schema_version = catalog->schema_version;
    int rc = MYLITE_OK;

    if (schema_version == 1U) {
        rc = migrate_catalog_schema_v1_to_v2(database->sqlite);
        schema_version = 2U;
    }
    if (rc == MYLITE_OK && schema_version == 2U) {
        rc = migrate_catalog_schema_v2_to_v3(database->sqlite);
        schema_version = 3U;
    }
    if (rc == MYLITE_OK && schema_version == 3U) {
        rc = migrate_catalog_schema_v3_to_v4(database->sqlite);
        schema_version = 4U;
    }
    if (rc == MYLITE_OK && schema_version == 4U) {
        rc = migrate_catalog_schema_v4_to_v5(database->sqlite);
        schema_version = catalog_schema_version_v5;
    }
    if (rc == MYLITE_OK && schema_version == catalog_schema_version_v5) {
        rc = migrate_catalog_schema_v5_to_v6(database->sqlite);
        schema_version = MYLITE_CATALOG_SCHEMA_VERSION;
    }
    if (rc == MYLITE_OK && schema_version == MYLITE_CATALOG_SCHEMA_VERSION) {
        return MYLITE_OK;
    }

    return rc == MYLITE_OK ? MYLITE_ERROR : rc;
}

static int migrate_catalog_schema_v1_to_v2(sqlite3 *sqlite) {
    static const char *sql = "BEGIN IMMEDIATE;"
                             "ALTER TABLE _mylite_catalog_columns "
                             "ADD COLUMN default_kind INTEGER NOT NULL DEFAULT 0;"
                             "ALTER TABLE _mylite_catalog_columns "
                             "ADD COLUMN default_integer INTEGER;"
                             "UPDATE _mylite_catalog_state "
                             "SET schema_version = 2, minimum_reader_schema_version = 2;"
                             "COMMIT;";
    int rc = execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v2_to_v3(sqlite3 *sqlite) {
    static const char *sql =
        "BEGIN IMMEDIATE;"
        "ALTER TABLE _mylite_catalog_columns RENAME TO _mylite_catalog_columns_v2;"
        "CREATE TABLE _mylite_catalog_columns ("
        "column_id INTEGER PRIMARY KEY,"
        "table_id INTEGER NOT NULL,"
        "ordinal_position INTEGER NOT NULL CHECK(ordinal_position > 0),"
        "name TEXT NOT NULL,"
        "logical_type TEXT NOT NULL,"
        "physical_type TEXT NOT NULL,"
        "is_nullable INTEGER NOT NULL CHECK(is_nullable IN (0, 1)),"
        "default_kind INTEGER NOT NULL CHECK(default_kind IN (0, 1, 2)),"
        "default_integer INTEGER,"
        "descriptor_version INTEGER NOT NULL,"
        "created_catalog_generation INTEGER NOT NULL,"
        "updated_catalog_generation INTEGER NOT NULL,"
        "UNIQUE(table_id, ordinal_position),"
        "UNIQUE(table_id, name)"
        ");"
        "INSERT INTO _mylite_catalog_columns "
        "(column_id, table_id, ordinal_position, name, logical_type, physical_type, "
        "is_nullable, default_kind, default_integer, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation) "
        "SELECT column_id, table_id, ordinal_position, name, logical_type, physical_type, "
        "is_nullable, default_kind, default_integer, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_columns_v2;"
        "DROP TABLE _mylite_catalog_columns_v2;"
        "UPDATE _mylite_catalog_state "
        "SET schema_version = 3, minimum_reader_schema_version = 3;"
        "COMMIT;";
    int rc = execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v3_to_v4(sqlite3 *sqlite) {
    static const char *sql =
        "BEGIN IMMEDIATE;"
        "ALTER TABLE _mylite_catalog_columns RENAME TO _mylite_catalog_columns_v3;"
        "CREATE TABLE _mylite_catalog_columns ("
        "column_id INTEGER PRIMARY KEY,"
        "table_id INTEGER NOT NULL,"
        "ordinal_position INTEGER NOT NULL CHECK(ordinal_position > 0),"
        "name TEXT NOT NULL,"
        "logical_type TEXT NOT NULL,"
        "physical_type TEXT NOT NULL,"
        "is_nullable INTEGER NOT NULL CHECK(is_nullable IN (0, 1)),"
        "is_visible INTEGER NOT NULL CHECK(is_visible IN (0, 1)),"
        "default_kind INTEGER NOT NULL CHECK(default_kind IN (0, 1, 2)),"
        "default_integer INTEGER,"
        "descriptor_version INTEGER NOT NULL,"
        "created_catalog_generation INTEGER NOT NULL,"
        "updated_catalog_generation INTEGER NOT NULL,"
        "UNIQUE(table_id, ordinal_position),"
        "UNIQUE(table_id, name)"
        ");"
        "INSERT INTO _mylite_catalog_columns "
        "(column_id, table_id, ordinal_position, name, logical_type, physical_type, "
        "is_nullable, is_visible, default_kind, default_integer, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation) "
        "SELECT column_id, table_id, ordinal_position, name, logical_type, physical_type, "
        "is_nullable, 1, default_kind, default_integer, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_columns_v3;"
        "DROP TABLE _mylite_catalog_columns_v3;"
        "UPDATE _mylite_catalog_state "
        "SET schema_version = 4, minimum_reader_schema_version = 4;"
        "COMMIT;";
    int rc = execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v4_to_v5(sqlite3 *sqlite) {
    static const char *sql = "BEGIN IMMEDIATE;"
                             "CREATE TABLE _mylite_catalog_indexes ("
                             "index_id INTEGER PRIMARY KEY,"
                             "table_id INTEGER NOT NULL,"
                             "name TEXT NOT NULL,"
                             "kind INTEGER NOT NULL CHECK(kind = 1),"
                             "is_unique INTEGER NOT NULL CHECK(is_unique IN (0, 1)),"
                             "physical_name TEXT NOT NULL UNIQUE,"
                             "descriptor_version INTEGER NOT NULL,"
                             "created_catalog_generation INTEGER NOT NULL,"
                             "updated_catalog_generation INTEGER NOT NULL,"
                             "UNIQUE(table_id, name)"
                             ");"
                             "CREATE TABLE _mylite_catalog_index_columns ("
                             "index_column_id INTEGER PRIMARY KEY,"
                             "index_id INTEGER NOT NULL,"
                             "table_id INTEGER NOT NULL,"
                             "column_id INTEGER NOT NULL,"
                             "ordinal_position INTEGER NOT NULL CHECK(ordinal_position > 0),"
                             "descriptor_version INTEGER NOT NULL,"
                             "created_catalog_generation INTEGER NOT NULL,"
                             "updated_catalog_generation INTEGER NOT NULL,"
                             "UNIQUE(index_id, ordinal_position),"
                             "UNIQUE(index_id, column_id)"
                             ");"
                             "UPDATE _mylite_catalog_state "
                             "SET schema_version = 5, minimum_reader_schema_version = 5;"
                             "COMMIT;";
    int rc = execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v5_to_v6(sqlite3 *sqlite) {
    static const char *sql = "BEGIN IMMEDIATE;"
                             "ALTER TABLE _mylite_catalog_tables "
                             "ADD COLUMN auto_increment_next INTEGER NOT NULL DEFAULT 1;"
                             "ALTER TABLE _mylite_catalog_columns "
                             "ADD COLUMN is_auto_increment INTEGER NOT NULL DEFAULT 0 "
                             "CHECK(is_auto_increment IN (0, 1));"
                             "UPDATE _mylite_catalog_state "
                             "SET schema_version = 6, minimum_reader_schema_version = 6;"
                             "COMMIT;";
    int rc = execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int validate_catalog_descriptor_tables(sqlite3 *sqlite) {
    int rc = validate_select_shape(
        sqlite,
        "SELECT schema_id, name, descriptor_version, created_catalog_generation, "
        "updated_catalog_generation "
        "FROM _mylite_catalog_schemas WHERE 0"
    );

    if (rc == MYLITE_OK) {
        rc = validate_select_shape(
            sqlite,
            "SELECT table_id, schema_id, name, kind, physical_name, auto_increment_next, "
            "descriptor_version, created_catalog_generation, updated_catalog_generation "
            "FROM _mylite_catalog_tables WHERE 0"
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_select_shape(
            sqlite,
            "SELECT column_id, table_id, ordinal_position, name, logical_type, physical_type, "
            "is_nullable, is_visible, is_auto_increment, default_kind, default_integer, "
            "descriptor_version, created_catalog_generation, updated_catalog_generation "
            "FROM _mylite_catalog_columns WHERE 0"
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_select_shape(
            sqlite,
            "SELECT index_id, table_id, name, kind, is_unique, physical_name, "
            "descriptor_version, created_catalog_generation, updated_catalog_generation "
            "FROM _mylite_catalog_indexes WHERE 0"
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_select_shape(
            sqlite,
            "SELECT index_column_id, index_id, table_id, column_id, ordinal_position, "
            "descriptor_version, created_catalog_generation, updated_catalog_generation "
            "FROM _mylite_catalog_index_columns WHERE 0"
        );
    }

    return rc;
}

static int validate_select_shape(sqlite3 *sqlite, const char *sql) {
    sqlite3_stmt *statement = NULL;
    int rc = prepare_statement(sqlite, sql, &statement);

    return finalize_statement(statement, rc);
}

static int initialize_catalog_schema(struct mylite_db *database) {
    static const char *sql =
        "BEGIN IMMEDIATE;"
        "CREATE TABLE _mylite_catalog_state ("
        "singleton_id INTEGER PRIMARY KEY CHECK(singleton_id = 1),"
        "schema_version INTEGER NOT NULL,"
        "minimum_reader_schema_version INTEGER NOT NULL,"
        "catalog_generation INTEGER NOT NULL,"
        "created_with_file_format_version INTEGER NOT NULL"
        ");"
        "CREATE TABLE _mylite_catalog_schemas ("
        "schema_id INTEGER PRIMARY KEY,"
        "name TEXT NOT NULL UNIQUE,"
        "descriptor_version INTEGER NOT NULL,"
        "created_catalog_generation INTEGER NOT NULL,"
        "updated_catalog_generation INTEGER NOT NULL"
        ");"
        "CREATE TABLE _mylite_catalog_tables ("
        "table_id INTEGER PRIMARY KEY,"
        "schema_id INTEGER NOT NULL,"
        "name TEXT NOT NULL,"
        "kind INTEGER NOT NULL CHECK(kind = 1),"
        "physical_name TEXT NOT NULL UNIQUE,"
        "auto_increment_next INTEGER NOT NULL CHECK(auto_increment_next > 0),"
        "descriptor_version INTEGER NOT NULL,"
        "created_catalog_generation INTEGER NOT NULL,"
        "updated_catalog_generation INTEGER NOT NULL,"
        "UNIQUE(schema_id, name)"
        ");"
        "CREATE TABLE _mylite_catalog_columns ("
        "column_id INTEGER PRIMARY KEY,"
        "table_id INTEGER NOT NULL,"
        "ordinal_position INTEGER NOT NULL CHECK(ordinal_position > 0),"
        "name TEXT NOT NULL,"
        "logical_type TEXT NOT NULL,"
        "physical_type TEXT NOT NULL,"
        "is_nullable INTEGER NOT NULL CHECK(is_nullable IN (0, 1)),"
        "is_visible INTEGER NOT NULL CHECK(is_visible IN (0, 1)),"
        "is_auto_increment INTEGER NOT NULL CHECK(is_auto_increment IN (0, 1)),"
        "default_kind INTEGER NOT NULL CHECK(default_kind IN (0, 1, 2)),"
        "default_integer INTEGER,"
        "descriptor_version INTEGER NOT NULL,"
        "created_catalog_generation INTEGER NOT NULL,"
        "updated_catalog_generation INTEGER NOT NULL,"
        "UNIQUE(table_id, ordinal_position),"
        "UNIQUE(table_id, name)"
        ");"
        "CREATE TABLE _mylite_catalog_indexes ("
        "index_id INTEGER PRIMARY KEY,"
        "table_id INTEGER NOT NULL,"
        "name TEXT NOT NULL,"
        "kind INTEGER NOT NULL CHECK(kind = 1),"
        "is_unique INTEGER NOT NULL CHECK(is_unique IN (0, 1)),"
        "physical_name TEXT NOT NULL UNIQUE,"
        "descriptor_version INTEGER NOT NULL,"
        "created_catalog_generation INTEGER NOT NULL,"
        "updated_catalog_generation INTEGER NOT NULL,"
        "UNIQUE(table_id, name)"
        ");"
        "CREATE TABLE _mylite_catalog_index_columns ("
        "index_column_id INTEGER PRIMARY KEY,"
        "index_id INTEGER NOT NULL,"
        "table_id INTEGER NOT NULL,"
        "column_id INTEGER NOT NULL,"
        "ordinal_position INTEGER NOT NULL CHECK(ordinal_position > 0),"
        "descriptor_version INTEGER NOT NULL,"
        "created_catalog_generation INTEGER NOT NULL,"
        "updated_catalog_generation INTEGER NOT NULL,"
        "UNIQUE(index_id, ordinal_position),"
        "UNIQUE(index_id, column_id)"
        ");"
        "INSERT INTO _mylite_catalog_state "
        "(singleton_id, schema_version, minimum_reader_schema_version, catalog_generation, "
        "created_with_file_format_version) "
        "VALUES (1, 6, 6, 1, 1);"
        "COMMIT;";
    struct mylite_catalog catalog = {.initialized = false};
    int rc = execute_sql(database->sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(database->sqlite);
        return rc;
    }

    rc = read_catalog_state(database->sqlite, &catalog);
    if (rc != MYLITE_OK) {
        return rc;
    }

    return apply_catalog_state(database, &catalog);
}

static int existing_catalog_table_count(sqlite3 *sqlite, int *out_count) {
    enum {
        catalog_state_name_bind = 1,
        catalog_schemas_name_bind = 2,
        catalog_tables_name_bind = 3,
        catalog_columns_name_bind = 4,
        catalog_indexes_name_bind = 5,
        catalog_index_columns_name_bind = 6,
    };

    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = MYLITE_OK;

    *out_count = 0;
    rc = prepare_statement(
        sqlite,
        "SELECT count(*) FROM sqlite_master "
        "WHERE type = 'table' "
        "AND name IN (?1, ?2, ?3, ?4, ?5, ?6)",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, catalog_state_name_bind, catalog_state_table_name());
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, catalog_schemas_name_bind, catalog_schemas_table_name());
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, catalog_tables_name_bind, catalog_tables_table_name());
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, catalog_columns_name_bind, catalog_columns_table_name());
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, catalog_indexes_name_bind, catalog_indexes_table_name());
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(
            statement,
            catalog_index_columns_name_bind,
            catalog_index_columns_table_name()
        );
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            *out_count = sqlite3_column_int(statement, 0);
        } else {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }

    return finalize_statement(statement, rc);
}

static int read_catalog_state(sqlite3 *sqlite, struct mylite_catalog *catalog) {
    sqlite3_stmt *statement = NULL;
    int64_t singleton_id = 0;
    int64_t schema_version = 0;
    int64_t minimum_reader_schema_version = 0;
    int64_t generation = 0;
    int64_t file_format_version = 0;
    int sqlite_rc = SQLITE_OK;
    int rc = prepare_statement(
        sqlite,
        "SELECT singleton_id, schema_version, minimum_reader_schema_version, catalog_generation, "
        "created_with_file_format_version "
        "FROM _mylite_catalog_state",
        &statement
    );

    *catalog = (struct mylite_catalog){.initialized = false};
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc != SQLITE_ROW) {
            rc =
                sqlite_rc == SQLITE_DONE ? MYLITE_ERROR : mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_i64(statement, catalog_state_select_singleton_id_column, &singleton_id);
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_i64(
            statement,
            catalog_state_select_schema_version_column,
            &schema_version
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_i64(
            statement,
            catalog_state_select_minimum_reader_schema_version_column,
            &minimum_reader_schema_version
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_i64(
            statement,
            catalog_state_select_catalog_generation_column,
            &generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_i64(
            statement,
            catalog_state_select_file_format_version_column,
            &file_format_version
        );
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc != SQLITE_DONE) {
            rc = sqlite_rc == SQLITE_ROW ? MYLITE_ERROR : mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    if (rc == MYLITE_OK && (singleton_id != 1 || schema_version < 1 ||
                            schema_version > MYLITE_CATALOG_SCHEMA_VERSION ||
                            minimum_reader_schema_version > MYLITE_CATALOG_SCHEMA_VERSION ||
                            minimum_reader_schema_version < 1 ||
                            file_format_version != MYLITE_FILE_FORMAT_VERSION || generation < 1)) {
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = i64_to_u32(schema_version, &catalog->schema_version);
    }
    if (rc == MYLITE_OK) {
        rc = i64_to_u64(generation, &catalog->generation);
    }
    if (rc == MYLITE_OK) {
        catalog->initialized = true;
        reset_descriptor_cache_state(catalog);
    }

    return finalize_statement(statement, rc);
}

static int apply_catalog_state(struct mylite_db *database, const struct mylite_catalog *catalog) {
    database->catalog = *catalog;
    database->session.catalog_generation = catalog->generation;

    return MYLITE_OK;
}

static int begin_catalog_transaction(sqlite3 *sqlite) {
    return execute_sql(sqlite, "BEGIN IMMEDIATE");
}

static int commit_catalog_transaction(sqlite3 *sqlite) {
    return execute_sql(sqlite, "COMMIT");
}

static void rollback_catalog_transaction(sqlite3 *sqlite) {
    if (sqlite == NULL) {
        return;
    }

    (void)sqlite3_exec(sqlite, "ROLLBACK", NULL, NULL, NULL);
}

static int begin_generation_change(
    struct mylite_db *database,
    struct catalog_generation_change *out_change
) {
    struct mylite_catalog catalog = {.initialized = false};
    int rc = begin_catalog_transaction(database->sqlite);

    *out_change = (struct catalog_generation_change){0};
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = read_catalog_state(database->sqlite, &catalog);
    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(database->sqlite);
        return rc;
    }
    if (catalog.generation == UINT64_MAX) {
        rollback_catalog_transaction(database->sqlite);
        return MYLITE_ERROR;
    }

    out_change->next_generation = catalog.generation + 1U;

    return MYLITE_OK;
}

static int finish_generation_change(
    struct mylite_db *database,
    const struct catalog_generation_change *change
) {
    int rc = update_catalog_generation(database->sqlite, change->next_generation);

    if (rc == MYLITE_OK) {
        rc = commit_catalog_transaction(database->sqlite);
    }
    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(database->sqlite);
        return rc;
    }

    database->catalog.generation = change->next_generation;
    database->session.catalog_generation = change->next_generation;
    reset_descriptor_cache_state(&database->catalog);

    return MYLITE_OK;
}

static void abandon_generation_change(sqlite3 *sqlite) {
    rollback_catalog_transaction(sqlite);
}

static int update_catalog_generation(sqlite3 *sqlite, uint64_t generation) {
    sqlite3_stmt *statement = NULL;
    int rc = validate_generation(generation);

    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        sqlite,
        "UPDATE _mylite_catalog_state SET catalog_generation = ?1 WHERE singleton_id = 1",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_u64(statement, 1, generation);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = require_changed_row(sqlite);
    }

    return finalize_statement(statement, rc);
}

static int execute_sql(sqlite3 *sqlite, const char *sql) {
    int sqlite_rc = sqlite3_exec(sqlite, sql, NULL, NULL, NULL);

    return mylite_sqlite_status_to_mylite(sqlite_rc);
}

static int prepare_statement(sqlite3 *sqlite, const char *sql, sqlite3_stmt **out_statement) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;

    *out_statement = NULL;
    sqlite_rc = sqlite3_prepare_v2(sqlite, sql, sqlite_use_nul_terminated_string, &statement, NULL);
    if (sqlite_rc != SQLITE_OK) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    *out_statement = statement;

    return MYLITE_OK;
}

static int bind_text(sqlite3_stmt *statement, int index, const char *value) {
    int sqlite_rc = sqlite3_bind_text(
        statement,
        index,
        value,
        sqlite_use_nul_terminated_string,
        SQLITE_TRANSIENT
    );

    return mylite_sqlite_status_to_mylite(sqlite_rc);
}

static int bind_i64(sqlite3_stmt *statement, int index, int64_t value) {
    int sqlite_rc = sqlite3_bind_int64(statement, index, (sqlite3_int64)value);

    return mylite_sqlite_status_to_mylite(sqlite_rc);
}

static int bind_nullable_i64(sqlite3_stmt *statement, int index, bool has_value, int64_t value) {
    int sqlite_rc = SQLITE_OK;

    if (!has_value) {
        sqlite_rc = sqlite3_bind_null(statement, index);
    } else {
        sqlite_rc = sqlite3_bind_int64(statement, index, (sqlite3_int64)value);
    }

    return mylite_sqlite_status_to_mylite(sqlite_rc);
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): mirrors SQLite bind helper order.
static int bind_u64(sqlite3_stmt *statement, int index, uint64_t value) {
    int64_t signed_value = 0;
    int rc = u64_to_i64(value, &signed_value);

    if (rc != MYLITE_OK) {
        return rc;
    }

    return bind_i64(statement, index, signed_value);
}

static int64_t catalog_bool_value(bool value) {
    if (value) {
        return 1;
    }

    return 0;
}

static int step_done(sqlite3_stmt *statement) {
    int sqlite_rc = sqlite3_step(statement);

    if (sqlite_rc != SQLITE_DONE) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    return MYLITE_OK;
}

static int require_changed_row(sqlite3 *sqlite) {
    if (sqlite3_changes(sqlite) != 1) {
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int finalize_statement(sqlite3_stmt *statement, int rc) {
    int sqlite_rc = SQLITE_OK;

    if (statement == NULL) {
        return rc;
    }

    sqlite_rc = sqlite3_finalize(statement);
    if (rc != MYLITE_OK) {
        return rc;
    }

    return mylite_sqlite_status_to_mylite(sqlite_rc);
}

static int read_schema_by_name(
    sqlite3 *sqlite,
    const char *name,
    struct mylite_catalog_schema_descriptor *out_schema
) {
    bool found = false;
    int rc = try_read_schema_by_name(sqlite, name, out_schema, &found);

    if (rc != MYLITE_OK) {
        return rc;
    }

    if (!found) {
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int try_read_schema_by_name(
    sqlite3 *sqlite,
    const char *name,
    struct mylite_catalog_schema_descriptor *out_schema,
    bool *out_found
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = prepare_statement(
        sqlite,
        "SELECT schema_id, name, descriptor_version, created_catalog_generation, "
        "updated_catalog_generation "
        "FROM _mylite_catalog_schemas WHERE name = ?1",
        &statement
    );

    *out_schema = (struct mylite_catalog_schema_descriptor){0};
    *out_found = false;
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, 1, name);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            rc = materialize_schema(statement, out_schema);
            if (rc == MYLITE_OK) {
                *out_found = true;
            }
        } else if (sqlite_rc != SQLITE_DONE) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        } else {
            *out_schema = (struct mylite_catalog_schema_descriptor){0};
        }
    }

    return finalize_statement(statement, rc);
}

static int read_schema_by_id(
    sqlite3 *sqlite,
    int64_t schema_id,
    struct mylite_catalog_schema_descriptor *out_schema
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = prepare_statement(
        sqlite,
        "SELECT schema_id, name, descriptor_version, created_catalog_generation, "
        "updated_catalog_generation "
        "FROM _mylite_catalog_schemas WHERE schema_id = ?1",
        &statement
    );

    *out_schema = (struct mylite_catalog_schema_descriptor){0};
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            rc = materialize_schema(statement, out_schema);
        } else {
            rc =
                sqlite_rc == SQLITE_DONE ? MYLITE_ERROR : mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }

    return finalize_statement(statement, rc);
}

static int read_table_by_name(
    sqlite3 *sqlite,
    int64_t schema_id,
    const char *name,
    struct mylite_catalog_table_descriptor *out_table
) {
    bool found = false;
    int rc = try_read_table_by_name(sqlite, schema_id, name, out_table, &found);

    if (rc != MYLITE_OK) {
        return rc;
    }

    if (!found) {
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int try_read_table_by_name(
    sqlite3 *sqlite,
    int64_t schema_id,
    const char *name,
    struct mylite_catalog_table_descriptor *out_table,
    bool *out_found
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = prepare_statement(
        sqlite,
        "SELECT table_id, schema_id, name, kind, physical_name, auto_increment_next, "
        "descriptor_version, created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_tables WHERE schema_id = ?1 AND name = ?2",
        &statement
    );

    *out_table = (struct mylite_catalog_table_descriptor){0};
    *out_found = false;
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, 2, name);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            rc = materialize_table(statement, out_table);
            if (rc == MYLITE_OK) {
                *out_found = true;
            }
        } else if (sqlite_rc != SQLITE_DONE) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        } else {
            *out_table = (struct mylite_catalog_table_descriptor){0};
        }
    }

    return finalize_statement(statement, rc);
}

static int read_table_by_id(
    sqlite3 *sqlite,
    int64_t table_id,
    struct mylite_catalog_table_descriptor *out_table
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = prepare_statement(
        sqlite,
        "SELECT table_id, schema_id, name, kind, physical_name, auto_increment_next, "
        "descriptor_version, created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_tables WHERE table_id = ?1",
        &statement
    );

    *out_table = (struct mylite_catalog_table_descriptor){0};
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            rc = materialize_table(statement, out_table);
        } else {
            rc =
                sqlite_rc == SQLITE_DONE ? MYLITE_ERROR : mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }

    return finalize_statement(statement, rc);
}

static int read_column_by_name(
    sqlite3 *sqlite,
    int64_t table_id,
    const char *name,
    struct mylite_catalog_column_descriptor *out_column
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = prepare_statement(
        sqlite,
        "SELECT column_id, table_id, ordinal_position, name, logical_type, physical_type, "
        "is_nullable, is_visible, is_auto_increment, default_kind, default_integer, "
        "descriptor_version, created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_columns WHERE table_id = ?1 AND name = ?2",
        &statement
    );

    *out_column = (struct mylite_catalog_column_descriptor){0};
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, 2, name);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            rc = materialize_column(statement, out_column);
        } else {
            rc =
                sqlite_rc == SQLITE_DONE ? MYLITE_ERROR : mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }

    return finalize_statement(statement, rc);
}

static int try_read_primary_index_by_table_id(
    sqlite3 *sqlite,
    int64_t table_id,
    struct mylite_catalog_index_descriptor *out_index,
    bool *out_found
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = prepare_statement(
        sqlite,
        "SELECT index_id, table_id, name, kind, is_unique, physical_name, "
        "descriptor_version, created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_indexes WHERE table_id = ?1 AND kind = 1",
        &statement
    );

    *out_index = (struct mylite_catalog_index_descriptor){0};
    *out_found = false;
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            rc = materialize_index(statement, out_index);
            if (rc == MYLITE_OK) {
                *out_found = true;
            }
        } else if (sqlite_rc != SQLITE_DONE) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        } else {
            *out_index = (struct mylite_catalog_index_descriptor){0};
        }
    }

    return finalize_statement(statement, rc);
}

static int read_next_table_id(sqlite3 *sqlite, int64_t *out_table_id) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = prepare_statement(
        sqlite,
        "SELECT COALESCE(MAX(table_id), 0) + 1 FROM _mylite_catalog_tables",
        &statement
    );

    *out_table_id = 0;
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            rc = checked_column_i64(statement, catalog_next_table_id_column, out_table_id);
        } else {
            rc =
                sqlite_rc == SQLITE_DONE ? MYLITE_ERROR : mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    if (rc == MYLITE_OK) {
        rc = validate_positive_id(*out_table_id);
    }

    return finalize_statement(statement, rc);
}

static int read_next_index_id(sqlite3 *sqlite, int64_t *out_index_id) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = prepare_statement(
        sqlite,
        "SELECT COALESCE(MAX(index_id), 0) + 1 FROM _mylite_catalog_indexes",
        &statement
    );

    *out_index_id = 0;
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            rc = checked_column_i64(statement, 0, out_index_id);
        } else {
            rc =
                sqlite_rc == SQLITE_DONE ? MYLITE_ERROR : mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    if (rc == MYLITE_OK) {
        rc = validate_positive_id(*out_index_id);
    }

    return finalize_statement(statement, rc);
}

static int materialize_schema(
    sqlite3_stmt *statement,
    struct mylite_catalog_schema_descriptor *out_schema
) {
    int rc = checked_column_i64(statement, 0, &out_schema->schema_id);

    if (rc == MYLITE_OK) {
        rc = checked_column_text(statement, 1, out_schema->name, sizeof(out_schema->name));
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_u64(statement, 2, &out_schema->descriptor_version);
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_u64(statement, 3, &out_schema->created_catalog_generation);
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_u64(statement, 4, &out_schema->updated_catalog_generation);
    }

    return rc;
}

static int materialize_table(
    sqlite3_stmt *statement,
    struct mylite_catalog_table_descriptor *out_table
) {
    int64_t kind = 0;
    int rc =
        checked_column_i64(statement, catalog_table_select_table_id_column, &out_table->table_id);

    if (rc == MYLITE_OK) {
        rc = checked_column_i64(
            statement,
            catalog_table_select_schema_id_column,
            &out_table->schema_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_text(
            statement,
            catalog_table_select_name_column,
            out_table->name,
            sizeof(out_table->name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_i64(statement, catalog_table_select_kind_column, &kind);
    }
    if (rc == MYLITE_OK) {
        rc = validate_table_kind((enum mylite_catalog_table_kind)kind);
    }
    if (rc == MYLITE_OK) {
        out_table->kind = (enum mylite_catalog_table_kind)kind;
        rc = checked_column_text(
            statement,
            catalog_table_select_physical_name_column,
            out_table->physical_name,
            sizeof(out_table->physical_name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_i64(
            statement,
            catalog_table_select_auto_increment_next_column,
            &out_table->auto_increment_next
        );
    }
    if (rc == MYLITE_OK && out_table->auto_increment_next <= 0) {
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_u64(
            statement,
            catalog_table_select_descriptor_version_column,
            &out_table->descriptor_version
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_u64(
            statement,
            catalog_table_select_created_generation_column,
            &out_table->created_catalog_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_u64(
            statement,
            catalog_table_select_updated_generation_column,
            &out_table->updated_catalog_generation
        );
    }

    return rc;
}

static int materialize_column(
    sqlite3_stmt *statement,
    struct mylite_catalog_column_descriptor *out_column
) {
    int64_t nullable = 0;
    int64_t visible = 0;
    int64_t auto_increment = 0;
    int64_t default_kind = 0;
    bool has_default_integer = false;
    int rc = checked_column_i64(
        statement,
        catalog_column_select_column_id_column,
        &out_column->column_id
    );

    if (rc == MYLITE_OK) {
        rc = checked_column_i64(
            statement,
            catalog_column_select_table_id_column,
            &out_column->table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_i64(
            statement,
            catalog_column_select_ordinal_position_column,
            &out_column->ordinal_position
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_text(
            statement,
            catalog_column_select_name_column,
            out_column->name,
            sizeof(out_column->name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_text(
            statement,
            catalog_column_select_logical_type_column,
            out_column->logical_type,
            sizeof(out_column->logical_type)
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_text(
            statement,
            catalog_column_select_physical_type_column,
            out_column->physical_type,
            sizeof(out_column->physical_type)
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_i64(statement, catalog_column_select_is_nullable_column, &nullable);
    }
    if (rc == MYLITE_OK && nullable != 0 && nullable != 1) {
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        out_column->is_nullable = nullable != 0;
        rc = checked_column_i64(statement, catalog_column_select_is_visible_column, &visible);
    }
    if (rc == MYLITE_OK && visible != 0 && visible != 1) {
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        out_column->is_visible = visible != 0;
        rc = checked_column_i64(
            statement,
            catalog_column_select_is_auto_increment_column,
            &auto_increment
        );
    }
    if (rc == MYLITE_OK && auto_increment != 0 && auto_increment != 1) {
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        out_column->is_auto_increment = auto_increment != 0;
        rc =
            checked_column_i64(statement, catalog_column_select_default_kind_column, &default_kind);
    }
    if (rc == MYLITE_OK) {
        rc = validate_column_default_kind((enum mylite_catalog_column_default_kind)default_kind);
    }
    if (rc == MYLITE_OK) {
        out_column->default_kind = (enum mylite_catalog_column_default_kind)default_kind;
        rc = checked_nullable_column_i64(
            statement,
            catalog_column_select_default_integer_column,
            &has_default_integer,
            &out_column->default_integer
        );
    }
    if (rc == MYLITE_OK && ((out_column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER &&
                             !has_default_integer) ||
                            (out_column->default_kind != MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER &&
                             has_default_integer))) {
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_u64(
            statement,
            catalog_column_select_descriptor_version_column,
            &out_column->descriptor_version
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_u64(
            statement,
            catalog_column_select_created_generation_column,
            &out_column->created_catalog_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_u64(
            statement,
            catalog_column_select_updated_generation_column,
            &out_column->updated_catalog_generation
        );
    }

    return rc;
}

static int materialize_index(
    sqlite3_stmt *statement,
    struct mylite_catalog_index_descriptor *out_index
) {
    int64_t kind = 0;
    int64_t is_unique = 0;
    int rc =
        checked_column_i64(statement, catalog_index_select_index_id_column, &out_index->index_id);

    if (rc == MYLITE_OK) {
        rc = checked_column_i64(
            statement,
            catalog_index_select_table_id_column,
            &out_index->table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_text(
            statement,
            catalog_index_select_name_column,
            out_index->name,
            sizeof(out_index->name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_i64(statement, catalog_index_select_kind_column, &kind);
    }
    if (rc == MYLITE_OK) {
        rc = validate_index_kind((enum mylite_catalog_index_kind)kind);
    }
    if (rc == MYLITE_OK) {
        out_index->kind = (enum mylite_catalog_index_kind)kind;
        rc = checked_column_i64(statement, catalog_index_select_is_unique_column, &is_unique);
    }
    if (rc == MYLITE_OK && is_unique != 0 && is_unique != 1) {
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        out_index->is_unique = is_unique != 0;
        rc = checked_column_text(
            statement,
            catalog_index_select_physical_name_column,
            out_index->physical_name,
            sizeof(out_index->physical_name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_u64(
            statement,
            catalog_index_select_descriptor_version_column,
            &out_index->descriptor_version
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_u64(
            statement,
            catalog_index_select_created_generation_column,
            &out_index->created_catalog_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_u64(
            statement,
            catalog_index_select_updated_generation_column,
            &out_index->updated_catalog_generation
        );
    }

    return rc;
}

static int materialize_index_column(
    sqlite3_stmt *statement,
    struct mylite_catalog_index_column_descriptor *out_index_column
) {
    int rc = checked_column_i64(
        statement,
        catalog_index_column_select_index_column_id_column,
        &out_index_column->index_column_id
    );

    if (rc == MYLITE_OK) {
        rc = checked_column_i64(
            statement,
            catalog_index_column_select_index_id_column,
            &out_index_column->index_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_i64(
            statement,
            catalog_index_column_select_table_id_column,
            &out_index_column->table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_i64(
            statement,
            catalog_index_column_select_column_id_column,
            &out_index_column->column_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_i64(
            statement,
            catalog_index_column_select_ordinal_position_column,
            &out_index_column->ordinal_position
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_u64(
            statement,
            catalog_index_column_select_descriptor_version_column,
            &out_index_column->descriptor_version
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_u64(
            statement,
            catalog_index_column_select_created_generation_column,
            &out_index_column->created_catalog_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_u64(
            statement,
            catalog_index_column_select_updated_generation_column,
            &out_index_column->updated_catalog_generation
        );
    }

    return rc;
}

static int checked_column_i64(sqlite3_stmt *statement, int index, int64_t *out_value) {
    if (sqlite3_column_type(statement, index) != SQLITE_INTEGER) {
        return MYLITE_ERROR;
    }

    *out_value = (int64_t)sqlite3_column_int64(statement, index);

    return MYLITE_OK;
}

static int checked_column_u64(sqlite3_stmt *statement, int index, uint64_t *out_value) {
    int64_t signed_value = 0;
    int rc = checked_column_i64(statement, index, &signed_value);

    if (rc != MYLITE_OK) {
        return rc;
    }

    return i64_to_u64(signed_value, out_value);
}

static int checked_nullable_column_i64(
    sqlite3_stmt *statement,
    int index,
    bool *out_has_value,
    int64_t *out_value
) {
    int column_type = sqlite3_column_type(statement, index);

    *out_has_value = false;
    *out_value = 0;
    if (column_type == SQLITE_NULL) {
        return MYLITE_OK;
    }
    if (column_type != SQLITE_INTEGER) {
        return MYLITE_ERROR;
    }

    *out_has_value = true;
    *out_value = (int64_t)sqlite3_column_int64(statement, index);

    return MYLITE_OK;
}

static int checked_column_text(
    sqlite3_stmt *statement,
    int index,
    char *destination,
    size_t destination_size
) {
    const unsigned char *source = NULL;
    int byte_count = 0;

    if (sqlite3_column_type(statement, index) != SQLITE_TEXT) {
        return MYLITE_ERROR;
    }

    source = sqlite3_column_text(statement, index);
    byte_count = sqlite3_column_bytes(statement, index);
    if (source == NULL || byte_count < 0 || (size_t)byte_count >= destination_size) {
        return MYLITE_ERROR;
    }

    memcpy(destination, source, (size_t)byte_count);
    destination[(size_t)byte_count] = '\0';

    return MYLITE_OK;
}

static int validate_database(struct mylite_db *database) {
    if (database == NULL || database->sqlite == NULL) {
        return MYLITE_MISUSE;
    }

    return MYLITE_OK;
}

static int validate_catalog_ready_database(struct mylite_db *database) {
    int rc = validate_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!database->catalog.initialized) {
        return MYLITE_MISUSE;
    }

    return MYLITE_OK;
}

static int validate_required_name(const char *name, size_t capacity) {
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

static int validate_logical_object_name(const char *name, size_t capacity) {
    int rc = validate_required_name(name, capacity);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (mylite_catalog_name_is_reserved(name)) {
        return MYLITE_MISUSE;
    }

    return MYLITE_OK;
}

static int validate_table_kind(enum mylite_catalog_table_kind kind) {
    if (kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        return MYLITE_MISUSE;
    }

    return MYLITE_OK;
}

static int validate_column_default_kind(enum mylite_catalog_column_default_kind kind) {
    if (kind != MYLITE_CATALOG_COLUMN_DEFAULT_NONE &&
        kind != MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER &&
        kind != MYLITE_CATALOG_COLUMN_DEFAULT_NO_EXPLICIT) {
        return MYLITE_MISUSE;
    }

    return MYLITE_OK;
}

static int validate_index_kind(enum mylite_catalog_index_kind kind) {
    if (kind != MYLITE_CATALOG_INDEX_KIND_PRIMARY) {
        return MYLITE_MISUSE;
    }

    return MYLITE_OK;
}

static int validate_active_mutation(const struct mylite_catalog_mutation *mutation) {
    if (mutation == NULL || !mutation->active || mutation->next_generation == 0U) {
        return MYLITE_MISUSE;
    }

    return MYLITE_OK;
}

static int validate_positive_id(int64_t id) {
    if (id <= 0) {
        return MYLITE_MISUSE;
    }

    return MYLITE_OK;
}

static int validate_positive_ordinal(int64_t ordinal_position) {
    if (ordinal_position <= 0) {
        return MYLITE_MISUSE;
    }

    return MYLITE_OK;
}

static int validate_generation(uint64_t generation) {
    int64_t signed_generation = 0;

    if (generation == 0U) {
        return MYLITE_ERROR;
    }

    return u64_to_i64(generation, &signed_generation);
}

static int validate_schema_callback(mylite_catalog_schema_callback callback) {
    if (callback == NULL) {
        return MYLITE_MISUSE;
    }

    return MYLITE_OK;
}

static int validate_callback(mylite_catalog_table_callback callback) {
    if (callback == NULL) {
        return MYLITE_MISUSE;
    }

    return MYLITE_OK;
}

static int validate_column_callback(mylite_catalog_column_callback callback) {
    if (callback == NULL) {
        return MYLITE_MISUSE;
    }

    return MYLITE_OK;
}

static int validate_index_callback(mylite_catalog_index_callback callback) {
    if (callback == NULL) {
        return MYLITE_MISUSE;
    }

    return MYLITE_OK;
}

static int validate_index_column_callback(mylite_catalog_index_column_callback callback) {
    if (callback == NULL) {
        return MYLITE_MISUSE;
    }

    return MYLITE_OK;
}

static int u64_to_i64(uint64_t value, int64_t *out_value) {
    if (value > INT64_MAX) {
        return MYLITE_ERROR;
    }

    *out_value = (int64_t)value;

    return MYLITE_OK;
}

static int i64_to_u32(int64_t value, uint32_t *out_value) {
    if (value < 0 || value > UINT32_MAX) {
        return MYLITE_ERROR;
    }

    *out_value = (uint32_t)value;

    return MYLITE_OK;
}

static int i64_to_u64(int64_t value, uint64_t *out_value) {
    if (value < 0) {
        return MYLITE_ERROR;
    }

    *out_value = (uint64_t)value;

    return MYLITE_OK;
}

static int text_has_ascii_case_insensitive_prefix(const char *text, const char *prefix) {
    size_t index = 0U;

    while (prefix[index] != '\0') {
        if (text[index] == '\0' ||
            ascii_lower((unsigned char)text[index]) != ascii_lower((unsigned char)prefix[index])) {
            return 0;
        }
        ++index;
    }

    return 1;
}

static char ascii_lower(unsigned char byte) {
    if (byte >= 'A' && byte <= 'Z') {
        return (char)(byte + ('a' - 'A'));
    }
    return (char)byte;
}

static void reset_descriptor_cache_state(struct mylite_catalog *catalog) {
    catalog->cached_generation = 0U;
    catalog->descriptor_cache_is_valid = false;
}

static const char *catalog_state_table_name(void) {
    return "_mylite_catalog_state";
}

static const char *catalog_schemas_table_name(void) {
    return "_mylite_catalog_schemas";
}

static const char *catalog_tables_table_name(void) {
    return "_mylite_catalog_tables";
}

static const char *catalog_columns_table_name(void) {
    return "_mylite_catalog_columns";
}

static const char *catalog_indexes_table_name(void) {
    return "_mylite_catalog_indexes";
}

static const char *catalog_index_columns_table_name(void) {
    return "_mylite_catalog_index_columns";
}
