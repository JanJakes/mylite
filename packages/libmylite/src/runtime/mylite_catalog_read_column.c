#include "mylite_catalog.h"

#include "mylite_catalog_internal.h"
#include "mylite_catalog_string_pool.h"

#include "mylite_connection.h"
#include "mylite_sqlite_registration.h"
#include "sqlite3.h"

#include <stdbool.h>
#include <stdint.h>

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
    catalog_column_select_default_text_column = 11,
    catalog_column_select_on_update_current_timestamp_column = 12,
    catalog_column_select_character_set_name_column = 13,
    catalog_column_select_collation_name_column = 14,
    catalog_column_select_comment_column = 15,
    catalog_column_select_is_generated_column = 16,
    catalog_column_select_generated_kind_column = 17,
    catalog_column_select_generation_expression_column = 18,
    catalog_column_select_sqlite_generation_expression_column = 19,
    catalog_column_select_descriptor_version_column = 20,
    catalog_column_select_created_generation_column = 21,
    catalog_column_select_updated_generation_column = 22,
};

static int materialize_column(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    struct mylite_catalog_column_descriptor *out_column
);
static int materialize_column_identity(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    struct mylite_catalog_column_descriptor *out_column
);
static int materialize_column_flags(
    sqlite3_stmt *statement,
    struct mylite_catalog_column_descriptor *out_column
);
static int materialize_column_defaults(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    struct mylite_catalog_column_descriptor *out_column
);
static int materialize_column_generated(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    struct mylite_catalog_column_descriptor *out_column
);
static int materialize_column_generations(
    sqlite3_stmt *statement,
    struct mylite_catalog_column_descriptor *out_column
);
static int intern_column_text(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int index,
    const char **out_text,
    size_t capacity
);
static int intern_nullable_column_text(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int index,
    bool *out_has_value,
    const char **out_text,
    size_t capacity
);

int mylite_catalog_for_each_column_in_table(
    struct mylite_db *database,
    int64_t table_id,
    mylite_catalog_column_callback callback,
    void *user_data
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_column_callback(callback);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "SELECT column_id, table_id, ordinal_position, name, logical_type, physical_type, "
        "is_nullable, is_visible, is_auto_increment, default_kind, default_integer, "
        "default_text, on_update_current_timestamp, character_set_name, collation_name, comment, "
        "is_generated, generated_kind, generation_expression, sqlite_generation_expression, "
        "descriptor_version, created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_columns WHERE table_id = ?1 ORDER BY ordinal_position",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    while (rc == MYLITE_OK) {
        struct mylite_catalog_column_descriptor column = {0};

        sqlite_rc = mylite_catalog_sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            break;
        }
        if (sqlite_rc != SQLITE_ROW) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
            break;
        }

        rc = materialize_column(database, statement, &column);
        if (rc == MYLITE_OK) {
            rc = callback(&column, user_data);
        }
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_read_column_by_name(
    struct mylite_db *database,
    int64_t table_id,
    const char *name,
    struct mylite_catalog_column_descriptor *out_column
) {
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_column == NULL) {
        return MYLITE_MISUSE;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_required_name(name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }

    return mylite_catalog_read_column_by_name_from_sqlite(database, table_id, name, out_column);
}

int mylite_catalog_read_column_by_name_from_sqlite(
    struct mylite_db *database,
    int64_t table_id,
    const char *name,
    struct mylite_catalog_column_descriptor *out_column
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "SELECT column_id, table_id, ordinal_position, name, logical_type, physical_type, "
        "is_nullable, is_visible, is_auto_increment, default_kind, default_integer, "
        "default_text, on_update_current_timestamp, character_set_name, collation_name, comment, "
        "is_generated, generated_kind, generation_expression, sqlite_generation_expression, "
        "descriptor_version, created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_columns WHERE table_id = ?1 AND name = ?2",
        &statement
    );

    *out_column = (struct mylite_catalog_column_descriptor){0};
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, 2, name);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = mylite_catalog_sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            rc = materialize_column(database, statement, out_column);
        } else {
            rc =
                sqlite_rc == SQLITE_DONE ? MYLITE_ERROR : mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

static int materialize_column(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    struct mylite_catalog_column_descriptor *out_column
) {
    int rc = materialize_column_identity(database, statement, out_column);

    if (rc == MYLITE_OK) {
        rc = materialize_column_flags(statement, out_column);
    }
    if (rc == MYLITE_OK) {
        rc = materialize_column_defaults(database, statement, out_column);
    }
    if (rc == MYLITE_OK) {
        rc = materialize_column_generated(database, statement, out_column);
    }
    if (rc == MYLITE_OK) {
        rc = materialize_column_generations(statement, out_column);
    }

    return rc;
}

static int materialize_column_identity(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    struct mylite_catalog_column_descriptor *out_column
) {
    int rc = mylite_catalog_checked_column_i64(
        statement,
        catalog_column_select_column_id_column,
        &out_column->column_id
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_column_select_table_id_column,
            &out_column->table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_column_select_ordinal_position_column,
            &out_column->ordinal_position
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_column_select_name_column,
            out_column->name,
            sizeof(out_column->name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_column_select_logical_type_column,
            out_column->logical_type,
            sizeof(out_column->logical_type)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_column_select_physical_type_column,
            out_column->physical_type,
            sizeof(out_column->physical_type)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_column_select_character_set_name_column,
            out_column->character_set_name,
            sizeof(out_column->character_set_name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_column_select_collation_name_column,
            out_column->collation_name,
            sizeof(out_column->collation_name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = intern_column_text(
            database,
            statement,
            catalog_column_select_comment_column,
            &out_column->comment,
            MYLITE_CATALOG_COLUMN_COMMENT_CAPACITY
        );
    }

    return rc;
}

static int materialize_column_flags(
    sqlite3_stmt *statement,
    struct mylite_catalog_column_descriptor *out_column
) {
    int64_t nullable = 0;
    int64_t visible = 0;
    int64_t auto_increment = 0;
    int64_t on_update_current_timestamp = 0;
    int rc = mylite_catalog_checked_column_i64(
        statement,
        catalog_column_select_is_nullable_column,
        &nullable
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_bool_i64(nullable, &out_column->is_nullable);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_column_select_is_visible_column,
            &visible
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_bool_i64(visible, &out_column->is_visible);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_column_select_is_auto_increment_column,
            &auto_increment
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_bool_i64(auto_increment, &out_column->is_auto_increment);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_column_select_on_update_current_timestamp_column,
            &on_update_current_timestamp
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_bool_i64(
            on_update_current_timestamp,
            &out_column->on_update_current_timestamp
        );
    }

    return rc;
}

static int materialize_column_defaults(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    struct mylite_catalog_column_descriptor *out_column
) {
    int64_t default_kind = 0;
    bool has_default_integer = false;
    bool has_default_text = false;
    int rc = mylite_catalog_checked_column_i64(
        statement,
        catalog_column_select_default_kind_column,
        &default_kind
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_column_default_kind((enum mylite_catalog_column_default_kind
        )default_kind);
    }
    if (rc == MYLITE_OK) {
        out_column->default_kind = (enum mylite_catalog_column_default_kind)default_kind;
        rc = mylite_catalog_checked_nullable_column_i64(
            statement,
            catalog_column_select_default_integer_column,
            &has_default_integer,
            &out_column->default_integer
        );
    }
    if (rc == MYLITE_OK &&
        ((mylite_catalog_column_default_kind_stores_integer(out_column->default_kind) &&
          !has_default_integer) ||
         (!mylite_catalog_column_default_kind_stores_integer(out_column->default_kind) &&
          has_default_integer))) {
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = intern_nullable_column_text(
            database,
            statement,
            catalog_column_select_default_text_column,
            &has_default_text,
            &out_column->default_text,
            MYLITE_CATALOG_DEFAULT_TEXT_CAPACITY
        );
    }
    if (rc == MYLITE_OK &&
        ((mylite_catalog_column_default_kind_stores_text(out_column->default_kind) &&
          !has_default_text) ||
         (!mylite_catalog_column_default_kind_stores_text(out_column->default_kind) &&
          has_default_text))) {
        rc = MYLITE_ERROR;
    }

    return rc;
}

static int materialize_column_generations(
    sqlite3_stmt *statement,
    struct mylite_catalog_column_descriptor *out_column
) {
    int rc = mylite_catalog_checked_column_u64(
        statement,
        catalog_column_select_descriptor_version_column,
        &out_column->descriptor_version
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_column_select_created_generation_column,
            &out_column->created_catalog_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_column_select_updated_generation_column,
            &out_column->updated_catalog_generation
        );
    }

    return rc;
}

static int materialize_column_generated(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    struct mylite_catalog_column_descriptor *out_column
) {
    int64_t is_generated = 0;
    int64_t generated_kind = 0;
    int rc = mylite_catalog_checked_column_i64(
        statement,
        catalog_column_select_is_generated_column,
        &is_generated
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_bool_i64(is_generated, &out_column->is_generated);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_column_select_generated_kind_column,
            &generated_kind
        );
    }
    if (rc == MYLITE_OK) {
        out_column->generated_kind = (enum mylite_catalog_generated_column_kind)generated_kind;
    }
    if (rc == MYLITE_OK) {
        rc = intern_column_text(
            database,
            statement,
            catalog_column_select_generation_expression_column,
            &out_column->generation_expression,
            MYLITE_CATALOG_GENERATION_EXPRESSION_CAPACITY
        );
    }
    if (rc == MYLITE_OK) {
        rc = intern_column_text(
            database,
            statement,
            catalog_column_select_sqlite_generation_expression_column,
            &out_column->sqlite_generation_expression,
            MYLITE_CATALOG_GENERATION_EXPRESSION_CAPACITY
        );
    }

    return rc;
}

static int intern_column_text(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int index,
    const char **out_text,
    size_t capacity
) {
    const unsigned char *text = NULL;
    int byte_count = 0;

    *out_text = NULL;
    if (sqlite3_column_type(statement, index) != SQLITE_TEXT) {
        return MYLITE_ERROR;
    }
    text = sqlite3_column_text(statement, index);
    byte_count = sqlite3_column_bytes(statement, index);
    if (text == NULL || byte_count < 0 || (size_t)byte_count >= capacity) {
        return MYLITE_ERROR;
    }

    return mylite_catalog_string_pool_intern(
        &database->catalog_strings,
        (const char *)text,
        (size_t)byte_count,
        out_text
    );
}

static int intern_nullable_column_text(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int index,
    bool *out_has_value,
    const char **out_text,
    size_t capacity
) {
    *out_has_value = false;
    *out_text = NULL;
    if (sqlite3_column_type(statement, index) == SQLITE_NULL) {
        return mylite_catalog_string_pool_intern(&database->catalog_strings, "", 0U, out_text);
    }

    int rc = intern_column_text(database, statement, index, out_text, capacity);

    if (rc == MYLITE_OK) {
        *out_has_value = true;
    }
    return rc;
}
