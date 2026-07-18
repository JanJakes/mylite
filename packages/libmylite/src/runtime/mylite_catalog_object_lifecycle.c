#include "mylite_catalog.h"

#include "mylite_catalog_internal.h"

#include "mylite_connection.h"
#include "sqlite3.h"

#include <stdint.h>

enum catalog_table_insert_bind_index {
    catalog_table_insert_schema_id_bind = 1,
    catalog_table_insert_name_bind = 2,
    catalog_table_insert_kind_bind = 3,
    catalog_table_insert_physical_name_bind = 4,
    catalog_table_insert_auto_increment_next_bind = 5,
    catalog_table_insert_default_charset_bind = 6,
    catalog_table_insert_default_collation_bind = 7,
    catalog_table_insert_comment_bind = 8,
    catalog_table_insert_created_time_bind = 9,
    catalog_table_insert_updated_time_bind = 10,
    catalog_table_insert_generation_bind = 11,
};

int mylite_catalog_create_schema(
    struct mylite_db *database,
    const char *name,
    struct mylite_catalog_schema_descriptor *out_schema
) {
    return mylite_catalog_create_schema_with_defaults(
        database,
        name,
        MYLITE_CATALOG_DEFAULT_TABLE_CHARSET,
        MYLITE_CATALOG_DEFAULT_TABLE_COLLATION,
        out_schema
    );
}

int mylite_catalog_create_schema_with_defaults(
    struct mylite_db *database,
    const char *name,
    const char *default_charset,
    const char *default_collation,
    struct mylite_catalog_schema_descriptor *out_schema
) {
    struct mylite_catalog_generation_change generation = {0};
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    if (out_schema != NULL) {
        *out_schema = (struct mylite_catalog_schema_descriptor){0};
    }
    rc = mylite_catalog_validate_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_logical_object_name(name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_required_name(default_charset, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_required_name(
        default_collation,
        MYLITE_CATALOG_IDENTIFIER_CAPACITY
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_begin_generation_change(database, &generation);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "INSERT INTO _mylite_catalog_schemas "
        "(name, default_charset, default_collation, "
        "descriptor_version, created_catalog_generation, updated_catalog_generation) "
        "VALUES (?1, ?2, ?3, 1, ?4, ?4)",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, 1, name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, 2, default_charset);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, 3, default_collation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_u64(statement, 4, generation.next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_finish_generation_change(database, &generation);
    }
    if (rc != MYLITE_OK) {
        mylite_catalog_abandon_generation_change(database->sqlite);
        return rc;
    }

    if (out_schema != NULL) {
        return mylite_catalog_read_schema_by_name_from_sqlite(database->sqlite, name, out_schema);
    }

    return MYLITE_OK;
}

int mylite_catalog_delete_schema(struct mylite_db *database, int64_t schema_id) {
    struct mylite_catalog_generation_change generation = {0};
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    rc = mylite_catalog_validate_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(schema_id);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_begin_generation_change(database, &generation);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_delete_foreign_keys_for_schema_from_sqlite(database->sqlite, schema_id);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_index_columns "
            "WHERE table_id IN ("
            "SELECT table_id FROM _mylite_catalog_tables WHERE schema_id = ?1"
            ")",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_indexes "
            "WHERE table_id IN ("
            "SELECT table_id FROM _mylite_catalog_tables WHERE schema_id = ?1"
            ")",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_views "
            "WHERE table_id IN ("
            "SELECT table_id FROM _mylite_catalog_tables WHERE schema_id = ?1"
            ")",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_columns "
            "WHERE table_id IN ("
            "SELECT table_id FROM _mylite_catalog_tables WHERE schema_id = ?1"
            ")",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_tables WHERE schema_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_schemas WHERE schema_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_finish_generation_change(database, &generation);
    }
    if (rc != MYLITE_OK) {
        mylite_catalog_abandon_generation_change(database->sqlite);
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
    const char *default_charset,
    const char *default_collation,
    const char *comment,
    int64_t created_time_utc_epoch,
    int64_t updated_time_utc_epoch,
    struct mylite_catalog_table_descriptor *out_table
) {
    struct mylite_catalog_generation_change generation = {0};
    struct mylite_catalog_schema_descriptor schema = {0};
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    if (out_table != NULL) {
        *out_table = (struct mylite_catalog_table_descriptor){0};
    }
    rc = mylite_catalog_validate_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_table_descriptor_input(
        &(const struct mylite_catalog_table_descriptor_input){
            .schema_id = schema_id,
            .name = name,
            .physical_name = physical_name,
            .kind = kind,
            .default_charset = default_charset,
            .default_collation = default_collation,
            .comment = comment,
            .created_time_utc_epoch = created_time_utc_epoch,
            .updated_time_utc_epoch = updated_time_utc_epoch,
        }
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_begin_generation_change(database, &generation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_read_schema_by_id_from_sqlite(database->sqlite, schema_id, &schema);
    if (rc != MYLITE_OK) {
        mylite_catalog_abandon_generation_change(database->sqlite);
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "INSERT INTO _mylite_catalog_tables "
        "(schema_id, name, kind, physical_name, auto_increment_next, default_charset, "
        "default_collation, comment, fulltext_doc_id_initialized, created_time_utc_epoch, "
        "updated_time_utc_epoch, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, 0, ?9, ?10, 1, ?11, ?11)",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, catalog_table_insert_schema_id_bind, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, catalog_table_insert_name_bind, name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, catalog_table_insert_kind_bind, (int64_t)kind);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_table_insert_physical_name_bind,
            physical_name
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, catalog_table_insert_auto_increment_next_bind, 1);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_table_insert_default_charset_bind,
            default_charset
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_table_insert_default_collation_bind,
            default_collation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, catalog_table_insert_comment_bind, comment);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_table_insert_created_time_bind,
            created_time_utc_epoch
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_table_insert_updated_time_bind,
            updated_time_utc_epoch
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_u64(
            statement,
            catalog_table_insert_generation_bind,
            generation.next_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_finish_generation_change(database, &generation);
    }
    if (rc != MYLITE_OK) {
        mylite_catalog_abandon_generation_change(database->sqlite);
        return rc;
    }

    if (out_table != NULL) {
        return mylite_catalog_read_table_by_name_from_sqlite(
            database->sqlite,
            schema_id,
            name,
            out_table
        );
    }

    return MYLITE_OK;
}

int mylite_catalog_update_table_name(
    struct mylite_db *database,
    int64_t table_id,
    const char *name
) {
    struct mylite_catalog_generation_change generation = {0};
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    rc = mylite_catalog_validate_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_logical_object_name(name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_begin_generation_change(database, &generation);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_tables "
        "SET name = ?1, descriptor_version = descriptor_version + 1, "
        "updated_catalog_generation = ?2 "
        "WHERE table_id = ?3",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, 1, name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_u64(statement, 2, generation.next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 3, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_finish_generation_change(database, &generation);
    }
    if (rc != MYLITE_OK) {
        mylite_catalog_abandon_generation_change(database->sqlite);
        return rc;
    }

    return MYLITE_OK;
}

int mylite_catalog_delete_table(struct mylite_db *database, int64_t table_id) {
    struct mylite_catalog_generation_change generation = {0};
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    rc = mylite_catalog_validate_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_begin_generation_change(database, &generation);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_delete_foreign_keys_for_related_table_from_sqlite(
        database->sqlite,
        table_id
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_delete_check_constraints_for_table_from_sqlite(
            database->sqlite,
            table_id
        );
    }

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_views WHERE table_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_index_columns WHERE table_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_indexes WHERE table_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_columns WHERE table_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_tables WHERE table_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_finish_generation_change(database, &generation);
    }
    if (rc != MYLITE_OK) {
        mylite_catalog_abandon_generation_change(database->sqlite);
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
    const char *default_text,
    bool on_update_current_timestamp,
    const char *character_set_name,
    const char *collation_name,
    const char *comment,
    bool is_generated,
    enum mylite_catalog_generated_column_kind generated_kind,
    const char *generation_expression,
    const char *sqlite_generation_expression,
    struct mylite_catalog_column_descriptor *out_column
) {
    const struct mylite_catalog_column_values values = {
        .name = name,
        .logical_type = logical_type,
        .physical_type = physical_type,
        .is_nullable = is_nullable,
        .is_visible = true,
        .is_auto_increment = false,
        .default_kind = default_kind,
        .default_integer = default_integer,
        .default_text = default_text,
        .on_update_current_timestamp = on_update_current_timestamp,
        .character_set_name = character_set_name,
        .collation_name = collation_name,
        .comment = comment == NULL ? "" : comment,
        .is_generated = is_generated,
        .generated_kind = generated_kind,
        .generation_expression = generation_expression == NULL ? "" : generation_expression,
        .sqlite_generation_expression =
            sqlite_generation_expression == NULL ? "" : sqlite_generation_expression,
    };
    struct mylite_catalog_generation_change generation = {0};
    struct mylite_catalog_table_descriptor table = {0};
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    if (out_column != NULL) {
        *out_column = (struct mylite_catalog_column_descriptor){0};
    }
    rc = mylite_catalog_validate_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_ordinal(ordinal_position);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_column_values(&values, false);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_begin_generation_change(database, &generation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_read_table_by_id_from_sqlite(database->sqlite, table_id, &table);
    if (rc != MYLITE_OK) {
        mylite_catalog_abandon_generation_change(database->sqlite);
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "INSERT INTO _mylite_catalog_columns "
        "(table_id, ordinal_position, name, logical_type, physical_type, is_nullable, "
        "is_visible, is_auto_increment, default_kind, default_integer, default_text, "
        "on_update_current_timestamp, character_set_name, collation_name, comment, "
        "is_generated, generated_kind, generation_expression, sqlite_generation_expression, "
        "descriptor_version, created_catalog_generation, updated_catalog_generation) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14, ?15, "
        "?16, ?17, ?18, ?19, 1, ?20, ?20)",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_column_insert_values(
            statement,
            table_id,
            ordinal_position,
            &values,
            generation.next_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_finish_generation_change(database, &generation);
    }
    if (rc != MYLITE_OK) {
        mylite_catalog_abandon_generation_change(database->sqlite);
        return rc;
    }

    if (out_column != NULL) {
        return mylite_catalog_read_column_by_name_from_sqlite(database, table_id, name, out_column);
    }

    return MYLITE_OK;
}

int mylite_catalog_delete_column(struct mylite_db *database, int64_t column_id) {
    struct mylite_catalog_generation_change generation = {0};
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    rc = mylite_catalog_validate_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(column_id);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_begin_generation_change(database, &generation);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "DELETE FROM _mylite_catalog_columns WHERE column_id = ?1",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, column_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_finish_generation_change(database, &generation);
    }
    if (rc != MYLITE_OK) {
        mylite_catalog_abandon_generation_change(database->sqlite);
        return rc;
    }

    return MYLITE_OK;
}
