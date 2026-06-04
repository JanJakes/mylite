#include "mylite_catalog.h"

#include "mylite_catalog_internal.h"

#include "mylite_connection.h"
#include "mylite_file_format.h"
#include "mylite_sqlite_registration.h"
#include "sqlite3.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum {
    catalog_table_count = 10,
    pre_view_catalog_table_count = 9,
    pre_check_constraint_catalog_table_count = 8,
    downgraded_catalog_with_check_constraint_table_count = 7,
    pre_foreign_key_catalog_table_count = 6,
    legacy_catalog_table_count = 4,
};

enum catalog_state_select_column_index {
    catalog_state_select_singleton_id_column = 0,
    catalog_state_select_schema_version_column = 1,
    catalog_state_select_minimum_reader_schema_version_column = 2,
    catalog_state_select_catalog_generation_column = 3,
    catalog_state_select_file_format_version_column = 4,
};

static int ensure_catalog_schema(struct mylite_db *database);
static int load_existing_catalog(struct mylite_db *database);
static int migrate_catalog_schema(struct mylite_db *database, const struct mylite_catalog *catalog);
static int validate_catalog_descriptor_tables(sqlite3 *sqlite);
static int validate_select_shape(sqlite3 *sqlite, const char *sql);
static int initialize_catalog_schema(struct mylite_db *database);
static int existing_catalog_table_count(sqlite3 *sqlite, int *out_count);
static int read_catalog_state(sqlite3 *sqlite, struct mylite_catalog *catalog);
static int apply_catalog_state(struct mylite_db *database, const struct mylite_catalog *catalog);
static int begin_catalog_transaction(sqlite3 *sqlite);
static int commit_catalog_transaction(sqlite3 *sqlite);
static void rollback_catalog_transaction(sqlite3 *sqlite);
static int update_catalog_generation(sqlite3 *sqlite, uint64_t generation);
static void reset_descriptor_cache_state(struct mylite_catalog *catalog);

static const char *catalog_state_table_name(void);
static const char *catalog_schemas_table_name(void);
static const char *catalog_tables_table_name(void);
static const char *catalog_columns_table_name(void);
static const char *catalog_views_table_name(void);
static const char *catalog_indexes_table_name(void);
static const char *catalog_index_columns_table_name(void);
static const char *catalog_foreign_keys_table_name(void);
static const char *catalog_foreign_key_columns_table_name(void);
static const char *catalog_check_constraints_table_name(void);

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

    rc = mylite_catalog_validate_database(database);
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
    rc = mylite_catalog_validate_ready_database(database);
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
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
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

int mylite_catalog_begin_generation_change(
    struct mylite_db *database,
    struct mylite_catalog_generation_change *out_change
) {
    struct mylite_catalog catalog = {.initialized = false};
    int rc = begin_catalog_transaction(database->sqlite);

    *out_change = (struct mylite_catalog_generation_change){0};
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

int mylite_catalog_finish_generation_change(
    struct mylite_db *database,
    const struct mylite_catalog_generation_change *change
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

void mylite_catalog_abandon_generation_change(sqlite3 *sqlite) {
    rollback_catalog_transaction(sqlite);
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
    if (table_count != legacy_catalog_table_count &&
        table_count != pre_foreign_key_catalog_table_count &&
        table_count != downgraded_catalog_with_check_constraint_table_count &&
        table_count != pre_check_constraint_catalog_table_count &&
        table_count != pre_view_catalog_table_count && table_count != catalog_table_count) {
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

    while (rc == MYLITE_OK && schema_version < MYLITE_CATALOG_SCHEMA_VERSION) {
        rc = mylite_catalog_migrate_schema_one_step(database->sqlite, &schema_version);
    }
    if (rc == MYLITE_OK && schema_version == MYLITE_CATALOG_SCHEMA_VERSION) {
        return MYLITE_OK;
    }

    return rc == MYLITE_OK ? MYLITE_ERROR : rc;
}

static int validate_catalog_descriptor_tables(sqlite3 *sqlite) {
    int rc = validate_select_shape(
        sqlite,
        "SELECT schema_id, name, default_charset, default_collation, "
        "descriptor_version, created_catalog_generation, "
        "updated_catalog_generation "
        "FROM _mylite_catalog_schemas WHERE 0"
    );

    if (rc == MYLITE_OK) {
        rc = validate_select_shape(
            sqlite,
            "SELECT table_id, schema_id, name, kind, physical_name, auto_increment_next, "
            "auto_increment_status, default_charset, default_collation, comment, "
            "row_format_option, key_block_size, "
            "pack_keys, checksum, stats_persistent, stats_auto_recalc, stats_sample_pages, "
            "fulltext_doc_id_initialized, "
            "created_time_utc_epoch, updated_time_utc_epoch, "
            "descriptor_version, created_catalog_generation, updated_catalog_generation "
            "FROM _mylite_catalog_tables WHERE 0"
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_select_shape(
            sqlite,
            "SELECT table_id, view_definition, show_create_sql, check_option, is_updatable, "
            "definer, security_type, character_set_client, collation_connection, "
            "source_schema_id, source_table_id, source_schema_name, source_table_name, "
            "descriptor_version, created_catalog_generation, updated_catalog_generation "
            "FROM _mylite_catalog_views WHERE 0"
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_select_shape(
            sqlite,
            "SELECT column_id, table_id, ordinal_position, name, logical_type, physical_type, "
            "is_nullable, is_visible, is_auto_increment, default_kind, default_integer, "
            "default_text, on_update_current_timestamp, character_set_name, collation_name, "
            "comment, is_generated, generated_kind, generation_expression, "
            "sqlite_generation_expression, "
            "descriptor_version, created_catalog_generation, updated_catalog_generation "
            "FROM _mylite_catalog_columns WHERE 0"
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_select_shape(
            sqlite,
            "SELECT index_id, table_id, name, kind, is_unique, is_visible, physical_name, "
            "comment, show_create_explicit_btree, descriptor_version, created_catalog_generation, "
            "updated_catalog_generation "
            "FROM _mylite_catalog_indexes WHERE 0"
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_select_shape(
            sqlite,
            "SELECT index_column_id, index_id, table_id, column_id, ordinal_position, "
            "prefix_length, sort_direction, "
            "descriptor_version, created_catalog_generation, updated_catalog_generation "
            "FROM _mylite_catalog_index_columns WHERE 0"
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_select_shape(
            sqlite,
            "SELECT foreign_key_id, child_table_id, parent_table_id, name, parent_index_id, "
            "child_index_id, update_rule, delete_rule, match_option, descriptor_version, "
            "created_catalog_generation, updated_catalog_generation "
            "FROM _mylite_catalog_foreign_keys WHERE 0"
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_select_shape(
            sqlite,
            "SELECT foreign_key_column_id, foreign_key_id, child_table_id, parent_table_id, "
            "child_column_id, parent_column_id, ordinal_position, position_in_unique_constraint, "
            "descriptor_version, created_catalog_generation, updated_catalog_generation "
            "FROM _mylite_catalog_foreign_key_columns WHERE 0"
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_select_shape(
            sqlite,
            "SELECT check_constraint_id, table_id, name, physical_name, check_clause, "
            "sqlite_expression, is_enforced, name_is_generated, generated_ordinal, "
            "ordinal_position, descriptor_version, created_catalog_generation, "
            "updated_catalog_generation "
            "FROM _mylite_catalog_check_constraints WHERE 0"
        );
    }

    return rc;
}

static int validate_select_shape(sqlite3 *sqlite, const char *sql) {
    sqlite3_stmt *statement = NULL;
    int rc = mylite_catalog_prepare_statement(sqlite, sql, &statement);

    return mylite_catalog_finalize_statement(statement, rc);
}

static int initialize_catalog_schema(struct mylite_db *database) {
    static const char *const sql_statements[] = {
        "CREATE TABLE _mylite_catalog_state ("
        "singleton_id INTEGER PRIMARY KEY CHECK(singleton_id = 1),"
        "schema_version INTEGER NOT NULL,"
        "minimum_reader_schema_version INTEGER NOT NULL,"
        "catalog_generation INTEGER NOT NULL,"
        "created_with_file_format_version INTEGER NOT NULL"
        ");",
        "CREATE TABLE _mylite_catalog_schemas ("
        "schema_id INTEGER PRIMARY KEY,"
        "name TEXT NOT NULL UNIQUE,"
        "default_charset TEXT NOT NULL,"
        "default_collation TEXT NOT NULL,"
        "descriptor_version INTEGER NOT NULL,"
        "created_catalog_generation INTEGER NOT NULL,"
        "updated_catalog_generation INTEGER NOT NULL"
        ");",
        "CREATE TABLE _mylite_catalog_tables ("
        "table_id INTEGER PRIMARY KEY,"
        "schema_id INTEGER NOT NULL,"
        "name TEXT NOT NULL,"
        "kind INTEGER NOT NULL CHECK(kind IN (1, 3)),"
        "physical_name TEXT NOT NULL UNIQUE,"
        "auto_increment_next INTEGER NOT NULL CHECK(auto_increment_next > 0),"
        "auto_increment_status INTEGER NOT NULL DEFAULT 0 CHECK(auto_increment_status >= 0),"
        "default_charset TEXT NOT NULL,"
        "default_collation TEXT NOT NULL,"
        "comment TEXT NOT NULL,"
        "row_format_option TEXT NOT NULL DEFAULT '',"
        "key_block_size INTEGER NOT NULL DEFAULT 0 "
        "CHECK(key_block_size IN (0, 1, 2, 4, 8, 16)),"
        "pack_keys INTEGER NOT NULL DEFAULT -1 CHECK(pack_keys IN (-1, 0, 1)),"
        "checksum INTEGER NOT NULL DEFAULT 0 CHECK(checksum IN (0, 1)),"
        "stats_persistent INTEGER NOT NULL DEFAULT -1 CHECK(stats_persistent IN (-1, 0, 1)),"
        "stats_auto_recalc INTEGER NOT NULL DEFAULT -1 "
        "CHECK(stats_auto_recalc IN (-1, 0, 1)),"
        "stats_sample_pages INTEGER NOT NULL DEFAULT 0 "
        "CHECK(stats_sample_pages BETWEEN 0 AND 65535),"
        "fulltext_doc_id_initialized INTEGER NOT NULL "
        "CHECK(fulltext_doc_id_initialized IN (0, 1)),"
        "created_time_utc_epoch INTEGER NOT NULL CHECK(created_time_utc_epoch >= 0),"
        "updated_time_utc_epoch INTEGER NOT NULL CHECK(updated_time_utc_epoch >= 0),"
        "descriptor_version INTEGER NOT NULL,"
        "created_catalog_generation INTEGER NOT NULL,"
        "updated_catalog_generation INTEGER NOT NULL,"
        "UNIQUE(schema_id, name)"
        ");",
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
        "default_kind INTEGER NOT NULL CHECK(default_kind IN "
        "(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12)),"
        "default_integer INTEGER,"
        "default_text TEXT,"
        "on_update_current_timestamp INTEGER NOT NULL "
        "CHECK(on_update_current_timestamp IN (0, 1)),"
        "character_set_name TEXT NOT NULL,"
        "collation_name TEXT NOT NULL,"
        "comment TEXT NOT NULL,"
        "is_generated INTEGER NOT NULL CHECK(is_generated IN (0, 1)),"
        "generated_kind INTEGER NOT NULL CHECK(generated_kind IN (0, 1, 2)),"
        "generation_expression TEXT NOT NULL,"
        "sqlite_generation_expression TEXT NOT NULL,"
        "descriptor_version INTEGER NOT NULL,"
        "created_catalog_generation INTEGER NOT NULL,"
        "updated_catalog_generation INTEGER NOT NULL,"
        "UNIQUE(table_id, ordinal_position),"
        "UNIQUE(table_id, name)"
        ");",
        "CREATE TABLE _mylite_catalog_views ("
        "table_id INTEGER PRIMARY KEY,"
        "view_definition TEXT NOT NULL,"
        "show_create_sql TEXT NOT NULL,"
        "check_option TEXT NOT NULL,"
        "is_updatable TEXT NOT NULL,"
        "definer TEXT NOT NULL,"
        "security_type TEXT NOT NULL,"
        "character_set_client TEXT NOT NULL,"
        "collation_connection TEXT NOT NULL,"
        "source_schema_id INTEGER NOT NULL,"
        "source_table_id INTEGER NOT NULL,"
        "source_schema_name TEXT NOT NULL,"
        "source_table_name TEXT NOT NULL,"
        "descriptor_version INTEGER NOT NULL,"
        "created_catalog_generation INTEGER NOT NULL,"
        "updated_catalog_generation INTEGER NOT NULL"
        ");",
        "CREATE TABLE _mylite_catalog_indexes ("
        "index_id INTEGER PRIMARY KEY,"
        "table_id INTEGER NOT NULL,"
        "name TEXT NOT NULL,"
        "kind INTEGER NOT NULL CHECK(kind IN (1, 2, 3, 4)),"
        "is_unique INTEGER NOT NULL CHECK(is_unique IN (0, 1)),"
        "is_visible INTEGER NOT NULL CHECK(is_visible IN (0, 1)),"
        "physical_name TEXT NOT NULL UNIQUE,"
        "comment TEXT NOT NULL,"
        "show_create_explicit_btree INTEGER NOT NULL "
        "CHECK(show_create_explicit_btree IN (0, 1)),"
        "descriptor_version INTEGER NOT NULL,"
        "created_catalog_generation INTEGER NOT NULL,"
        "updated_catalog_generation INTEGER NOT NULL,"
        "UNIQUE(table_id, name)"
        ");",
        "CREATE TABLE _mylite_catalog_index_columns ("
        "index_column_id INTEGER PRIMARY KEY,"
        "index_id INTEGER NOT NULL,"
        "table_id INTEGER NOT NULL,"
        "column_id INTEGER NOT NULL,"
        "ordinal_position INTEGER NOT NULL CHECK(ordinal_position > 0),"
        "prefix_length INTEGER CHECK(prefix_length IS NULL OR prefix_length > 0),"
        "sort_direction INTEGER NOT NULL CHECK(sort_direction IN (1, 2)),"
        "descriptor_version INTEGER NOT NULL,"
        "created_catalog_generation INTEGER NOT NULL,"
        "updated_catalog_generation INTEGER NOT NULL,"
        "UNIQUE(index_id, ordinal_position),"
        "UNIQUE(index_id, column_id)"
        ");",
        "CREATE TABLE _mylite_catalog_foreign_keys ("
        "foreign_key_id INTEGER PRIMARY KEY,"
        "child_table_id INTEGER NOT NULL,"
        "parent_table_id INTEGER NOT NULL,"
        "name TEXT NOT NULL,"
        "parent_index_id INTEGER NOT NULL,"
        "child_index_id INTEGER NOT NULL,"
        "update_rule TEXT NOT NULL,"
        "delete_rule TEXT NOT NULL,"
        "match_option TEXT NOT NULL,"
        "descriptor_version INTEGER NOT NULL,"
        "created_catalog_generation INTEGER NOT NULL,"
        "updated_catalog_generation INTEGER NOT NULL,"
        "UNIQUE(child_table_id, name)"
        ");",
        "CREATE TABLE _mylite_catalog_foreign_key_columns ("
        "foreign_key_column_id INTEGER PRIMARY KEY,"
        "foreign_key_id INTEGER NOT NULL,"
        "child_table_id INTEGER NOT NULL,"
        "parent_table_id INTEGER NOT NULL,"
        "child_column_id INTEGER NOT NULL,"
        "parent_column_id INTEGER NOT NULL,"
        "ordinal_position INTEGER NOT NULL CHECK(ordinal_position > 0),"
        "position_in_unique_constraint INTEGER NOT NULL "
        "CHECK(position_in_unique_constraint > 0),"
        "descriptor_version INTEGER NOT NULL,"
        "created_catalog_generation INTEGER NOT NULL,"
        "updated_catalog_generation INTEGER NOT NULL,"
        "UNIQUE(foreign_key_id, ordinal_position)"
        ");",
        "CREATE TABLE _mylite_catalog_check_constraints ("
        "check_constraint_id INTEGER PRIMARY KEY,"
        "table_id INTEGER NOT NULL,"
        "name TEXT NOT NULL,"
        "physical_name TEXT NOT NULL,"
        "check_clause TEXT NOT NULL,"
        "sqlite_expression TEXT NOT NULL,"
        "is_enforced INTEGER NOT NULL CHECK(is_enforced IN (0, 1)),"
        "name_is_generated INTEGER NOT NULL CHECK(name_is_generated IN (0, 1)),"
        "generated_ordinal INTEGER NOT NULL CHECK(generated_ordinal > 0),"
        "ordinal_position INTEGER NOT NULL CHECK(ordinal_position > 0),"
        "descriptor_version INTEGER NOT NULL,"
        "created_catalog_generation INTEGER NOT NULL,"
        "updated_catalog_generation INTEGER NOT NULL,"
        "UNIQUE(table_id, name),"
        "UNIQUE(table_id, physical_name),"
        "UNIQUE(table_id, ordinal_position)"
        ");",
        "INSERT INTO _mylite_catalog_state "
        "(singleton_id, schema_version, minimum_reader_schema_version, catalog_generation, "
        "created_with_file_format_version) "
        "VALUES (1, " MYLITE_CATALOG_SCHEMA_VERSION_TEXT
        ", " MYLITE_CATALOG_MINIMUM_READER_SCHEMA_VERSION_TEXT ", 1, 1);",
    };
    struct mylite_catalog catalog = {.initialized = false};
    int rc = mylite_catalog_execute_sql(database->sqlite, "BEGIN IMMEDIATE;");

    for (size_t index = 0U;
         rc == MYLITE_OK && index < sizeof(sql_statements) / sizeof(sql_statements[0U]);
         ++index) {
        rc = mylite_catalog_execute_sql(database->sqlite, sql_statements[index]);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_execute_sql(database->sqlite, "COMMIT;");
    }
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
        catalog_views_name_bind = 5,
        catalog_indexes_name_bind = 6,
        catalog_index_columns_name_bind = 7,
        catalog_foreign_keys_name_bind = 8,
        catalog_foreign_key_columns_name_bind = 9,
        catalog_check_constraints_name_bind = 10,
    };

    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = MYLITE_OK;

    *out_count = 0;
    rc = mylite_catalog_prepare_statement(
        sqlite,
        "SELECT count(*) FROM sqlite_master "
        "WHERE type = 'table' "
        "AND name IN (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10)",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_state_name_bind,
            catalog_state_table_name()
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_schemas_name_bind,
            catalog_schemas_table_name()
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_tables_name_bind,
            catalog_tables_table_name()
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_columns_name_bind,
            catalog_columns_table_name()
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_views_name_bind,
            catalog_views_table_name()
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_indexes_name_bind,
            catalog_indexes_table_name()
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_index_columns_name_bind,
            catalog_index_columns_table_name()
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_foreign_keys_name_bind,
            catalog_foreign_keys_table_name()
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_foreign_key_columns_name_bind,
            catalog_foreign_key_columns_table_name()
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_check_constraints_name_bind,
            catalog_check_constraints_table_name()
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

    return mylite_catalog_finalize_statement(statement, rc);
}

static int read_catalog_state(sqlite3 *sqlite, struct mylite_catalog *catalog) {
    sqlite3_stmt *statement = NULL;
    int64_t singleton_id = 0;
    int64_t schema_version = 0;
    int64_t minimum_reader_schema_version = 0;
    int64_t generation = 0;
    int64_t file_format_version = 0;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_prepare_statement(
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
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_state_select_singleton_id_column,
            &singleton_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_state_select_schema_version_column,
            &schema_version
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_state_select_minimum_reader_schema_version_column,
            &minimum_reader_schema_version
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_state_select_catalog_generation_column,
            &generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
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
        rc = mylite_catalog_i64_to_u32(schema_version, &catalog->schema_version);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_i64_to_u64(generation, &catalog->generation);
    }
    if (rc == MYLITE_OK) {
        catalog->initialized = true;
        reset_descriptor_cache_state(catalog);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

static int apply_catalog_state(struct mylite_db *database, const struct mylite_catalog *catalog) {
    database->catalog = *catalog;
    database->session.catalog_generation = catalog->generation;

    return MYLITE_OK;
}

static int begin_catalog_transaction(sqlite3 *sqlite) {
    return mylite_catalog_execute_sql(sqlite, "BEGIN IMMEDIATE");
}

static int commit_catalog_transaction(sqlite3 *sqlite) {
    return mylite_catalog_execute_sql(sqlite, "COMMIT");
}

static void rollback_catalog_transaction(sqlite3 *sqlite) {
    if (sqlite == NULL) {
        return;
    }

    (void)sqlite3_exec(sqlite, "ROLLBACK", NULL, NULL, NULL);
}

static int update_catalog_generation(sqlite3 *sqlite, uint64_t generation) {
    sqlite3_stmt *statement = NULL;
    int rc = mylite_catalog_validate_generation(generation);

    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        sqlite,
        "UPDATE _mylite_catalog_state SET catalog_generation = ?1 WHERE singleton_id = 1",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_u64(statement, 1, generation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(sqlite);
    }

    return mylite_catalog_finalize_statement(statement, rc);
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

static const char *catalog_views_table_name(void) {
    return "_mylite_catalog_views";
}

static const char *catalog_indexes_table_name(void) {
    return "_mylite_catalog_indexes";
}

static const char *catalog_index_columns_table_name(void) {
    return "_mylite_catalog_index_columns";
}

static const char *catalog_foreign_keys_table_name(void) {
    return "_mylite_catalog_foreign_keys";
}

static const char *catalog_foreign_key_columns_table_name(void) {
    return "_mylite_catalog_foreign_key_columns";
}

static const char *catalog_check_constraints_table_name(void) {
    return "_mylite_catalog_check_constraints";
}
