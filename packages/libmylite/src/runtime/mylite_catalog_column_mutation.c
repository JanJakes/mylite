#include "mylite_catalog.h"

#include "mylite_catalog_internal.h"

#include "mylite_connection.h"
#include "sqlite3.h"

#include <stddef.h>
#include <stdint.h>

enum catalog_column_reorder_offset_bind_index {
    catalog_column_reorder_offset_bind = 1,
    catalog_column_reorder_offset_table_id_bind = 2,
};

enum catalog_column_reorder_bind_index {
    catalog_column_reorder_ordinal_bind = 1,
    catalog_column_reorder_version_increment_bind = 2,
    catalog_column_reorder_generation_bind = 3,
    catalog_column_reorder_table_id_bind = 4,
    catalog_column_reorder_column_id_bind = 5,
};

static int offset_catalog_column_ordinals_for_reorder(
    struct mylite_db *database,
    const struct mylite_catalog_column_reorder *reorder
);
static int apply_catalog_column_reorder(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    const struct mylite_catalog_column_reorder *reorder,
    size_t column_index
);
static int validate_catalog_column_reorder_request(
    const struct mylite_catalog_column_reorder *reorder
);

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
        .is_visible = is_visible,
        .is_auto_increment = is_auto_increment,
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
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    if (out_column != NULL) {
        *out_column = (struct mylite_catalog_column_descriptor){0};
    }
    rc = mylite_catalog_validate_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
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
            mutation->next_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (out_column != NULL) {
        return mylite_catalog_read_column_by_name_from_sqlite(
            database->sqlite,
            table_id,
            name,
            out_column
        );
    }

    return MYLITE_OK;
}

int mylite_catalog_delete_column_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    int64_t column_id,
    int64_t ordinal_position
) {
    sqlite3_stmt *statement = NULL;
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(column_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_ordinal(ordinal_position);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "DELETE FROM _mylite_catalog_columns WHERE table_id = ?1 AND column_id = ?2",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 2, column_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
    statement = NULL;

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
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
        rc = mylite_catalog_bind_u64(statement, 1, mutation->next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 2, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 3, ordinal_position);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_rename_column_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    int64_t column_id,
    const char *name
) {
    sqlite3_stmt *statement = NULL;
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(column_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_logical_object_name(name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_columns "
        "SET name = ?1, descriptor_version = descriptor_version + 1, "
        "updated_catalog_generation = ?2 "
        "WHERE table_id = ?3 AND column_id = ?4",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, 1, name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_u64(statement, 2, mutation->next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 3, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 4, column_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }

    return mylite_catalog_finalize_statement(statement, rc);
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
    int64_t default_integer,
    const char *default_text,
    bool on_update_current_timestamp,
    const char *character_set_name,
    const char *collation_name,
    const char *comment,
    bool is_generated,
    enum mylite_catalog_generated_column_kind generated_kind,
    const char *generation_expression,
    const char *sqlite_generation_expression
) {
    const struct mylite_catalog_column_values values = {
        .name = name,
        .logical_type = logical_type,
        .physical_type = physical_type,
        .is_nullable = is_nullable,
        .is_visible = is_visible,
        .is_auto_increment = is_auto_increment,
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
    sqlite3_stmt *statement = NULL;
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(column_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_column_values(&values, true);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_columns "
        "SET name = ?1, logical_type = ?2, physical_type = ?3, is_nullable = ?4, "
        "is_visible = ?5, is_auto_increment = ?6, default_kind = ?7, default_integer = ?8, "
        "default_text = ?9, on_update_current_timestamp = ?10, character_set_name = ?11, "
        "collation_name = ?12, comment = ?13, is_generated = ?14, generated_kind = ?15, "
        "generation_expression = ?16, sqlite_generation_expression = ?17, "
        "descriptor_version = descriptor_version + 1, updated_catalog_generation = ?18 "
        "WHERE table_id = ?19 AND column_id = ?20",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_column_replace_values(
            statement,
            table_id,
            column_id,
            &values,
            mutation->next_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_reorder_columns_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    const struct mylite_catalog_column_reorder *reorder
) {
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_catalog_column_reorder_request(reorder);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = offset_catalog_column_ordinals_for_reorder(database, reorder);
    for (size_t index = 0U; rc == MYLITE_OK && index < reorder->column_count; ++index) {
        rc = apply_catalog_column_reorder(database, mutation, reorder, index);
    }

    return rc;
}

static int offset_catalog_column_ordinals_for_reorder(
    struct mylite_db *database,
    const struct mylite_catalog_column_reorder *reorder
) {
    sqlite3_stmt *statement = NULL;
    int rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_columns "
        "SET ordinal_position = ordinal_position + ?1 WHERE table_id = ?2",
        &statement
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_column_reorder_offset_bind,
            (int64_t)reorder->column_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_column_reorder_offset_table_id_bind,
            reorder->table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK &&
        sqlite3_changes64(database->sqlite) != (sqlite3_int64)reorder->column_count) {
        rc = MYLITE_ERROR;
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

static int apply_catalog_column_reorder(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    const struct mylite_catalog_column_reorder *reorder,
    size_t column_index
) {
    const struct mylite_catalog_column_descriptor *column = &reorder->columns[column_index];
    int64_t final_ordinal = (int64_t)column_index + 1;
    bool ordinal_changed = column->ordinal_position != final_ordinal;
    int64_t increment_descriptor_version =
        (ordinal_changed && column->column_id != reorder->metadata_replaced_column_id) ? 1 : 0;
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    if (column->table_id != reorder->table_id) {
        return MYLITE_MISUSE;
    }
    rc = mylite_catalog_validate_positive_id(column->column_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_ordinal(final_ordinal);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_columns "
        "SET ordinal_position = ?1, "
        "descriptor_version = descriptor_version + ?2, "
        "updated_catalog_generation = CASE WHEN ?2 <> 0 THEN ?3 "
        "ELSE updated_catalog_generation END "
        "WHERE table_id = ?4 AND column_id = ?5",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, catalog_column_reorder_ordinal_bind, final_ordinal);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_column_reorder_version_increment_bind,
            increment_descriptor_version
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_u64(
            statement,
            catalog_column_reorder_generation_bind,
            mutation->next_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_column_reorder_table_id_bind,
            reorder->table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_column_reorder_column_id_bind,
            column->column_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

static int validate_catalog_column_reorder_request(
    const struct mylite_catalog_column_reorder *reorder
) {
    int rc = MYLITE_OK;

    if (reorder == NULL || reorder->columns == NULL || reorder->column_count == 0U ||
        reorder->column_count > (size_t)(INT64_MAX / 2)) {
        return MYLITE_MISUSE;
    }
    rc = mylite_catalog_validate_positive_id(reorder->table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(reorder->metadata_replaced_column_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    return MYLITE_OK;
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
    int rc = mylite_catalog_validate_ready_database(database);

    if (is_visible) {
        visible_value = 1;
    }

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(column_id);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_columns "
        "SET is_visible = ?1, descriptor_version = descriptor_version + 1, "
        "updated_catalog_generation = ?2 "
        "WHERE table_id = ?3 AND column_id = ?4",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, visible_value);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_u64(statement, 2, mutation->next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 3, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 4, column_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}
