#include "mylite_catalog.h"

#include "mylite_catalog_internal.h"

#include "mylite_connection.h"
#include "mylite_sqlite_registration.h"
#include "sqlite3.h"

#include <stdbool.h>
#include <stdint.h>

enum catalog_index_select_column_index {
    catalog_index_select_index_id_column = 0,
    catalog_index_select_table_id_column = 1,
    catalog_index_select_name_column = 2,
    catalog_index_select_kind_column = 3,
    catalog_index_select_is_unique_column = 4,
    catalog_index_select_is_visible_column = 5,
    catalog_index_select_physical_name_column = 6,
    catalog_index_select_comment_column = 7,
    catalog_index_select_show_create_explicit_btree_column = 8,
    catalog_index_select_descriptor_version_column = 9,
    catalog_index_select_created_generation_column = 10,
    catalog_index_select_updated_generation_column = 11,
};

enum catalog_index_column_select_column_index {
    catalog_index_column_select_index_column_id_column = 0,
    catalog_index_column_select_index_id_column = 1,
    catalog_index_column_select_table_id_column = 2,
    catalog_index_column_select_column_id_column = 3,
    catalog_index_column_select_ordinal_position_column = 4,
    catalog_index_column_select_prefix_length_column = 5,
    catalog_index_column_select_sort_direction_column = 6,
    catalog_index_column_select_descriptor_version_column = 7,
    catalog_index_column_select_created_generation_column = 8,
    catalog_index_column_select_updated_generation_column = 9,
};

static int materialize_index(
    sqlite3_stmt *statement,
    struct mylite_catalog_index_descriptor *out_index
);
static int materialize_index_column(
    sqlite3_stmt *statement,
    struct mylite_catalog_index_column_descriptor *out_index_column
);

int mylite_catalog_for_each_index_in_table(
    struct mylite_db *database,
    int64_t table_id,
    mylite_catalog_index_callback callback,
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
    rc = mylite_catalog_validate_index_callback(callback);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "SELECT index_id, table_id, name, kind, is_unique, is_visible, physical_name, "
        "comment, show_create_explicit_btree, descriptor_version, created_catalog_generation, "
        "updated_catalog_generation "
        "FROM _mylite_catalog_indexes WHERE table_id = ?1 ORDER BY index_id",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    while (rc == MYLITE_OK) {
        struct mylite_catalog_index_descriptor index = {0};

        sqlite_rc = mylite_catalog_sqlite3_step(statement);
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

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_read_index_by_id(
    struct mylite_db *database,
    int64_t index_id,
    struct mylite_catalog_index_descriptor *out_index
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_index == NULL) {
        return MYLITE_MISUSE;
    }
    *out_index = (struct mylite_catalog_index_descriptor){0};
    rc = mylite_catalog_validate_positive_id(index_id);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "SELECT index_id, table_id, name, kind, is_unique, is_visible, physical_name, "
        "comment, show_create_explicit_btree, descriptor_version, created_catalog_generation, "
        "updated_catalog_generation "
        "FROM _mylite_catalog_indexes WHERE index_id = ?1",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, index_id);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = mylite_catalog_sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            rc = materialize_index(statement, out_index);
            if (rc == MYLITE_OK) {
                sqlite_rc = mylite_catalog_sqlite3_step(statement);
                if (sqlite_rc != SQLITE_DONE) {
                    rc = mylite_sqlite_status_to_mylite(sqlite_rc);
                }
            }
        } else {
            rc =
                sqlite_rc == SQLITE_DONE ? MYLITE_ERROR : mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_for_each_index_column_in_index(
    struct mylite_db *database,
    int64_t index_id,
    mylite_catalog_index_column_callback callback,
    void *user_data
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(index_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_index_column_callback(callback);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "SELECT index_column_id, index_id, table_id, column_id, ordinal_position, "
        "prefix_length, sort_direction, "
        "descriptor_version, created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_index_columns "
        "WHERE index_id = ?1 ORDER BY ordinal_position",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, index_id);
    }
    while (rc == MYLITE_OK) {
        struct mylite_catalog_index_column_descriptor index_column = {0};

        sqlite_rc = mylite_catalog_sqlite3_step(statement);
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

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_try_read_primary_index_by_table_id(
    struct mylite_db *database,
    int64_t table_id,
    struct mylite_catalog_index_descriptor *out_index,
    bool *out_found
) {
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_index == NULL || out_found == NULL) {
        return MYLITE_MISUSE;
    }
    *out_found = false;
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }

    return mylite_catalog_try_read_primary_index_by_table_id_from_sqlite(
        database->sqlite,
        table_id,
        out_index,
        out_found
    );
}

int mylite_catalog_read_inserted_index_column(
    struct mylite_db *database,
    int64_t index_id,
    int64_t ordinal_position,
    struct mylite_catalog_index_column_descriptor *out_index_column
) {
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "SELECT index_column_id, index_id, table_id, column_id, ordinal_position, "
        "prefix_length, sort_direction, "
        "descriptor_version, created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_index_columns "
        "WHERE index_id = ?1 AND ordinal_position = ?2",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, index_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 2, ordinal_position);
    }
    if (rc == MYLITE_OK) {
        int sqlite_rc = mylite_catalog_sqlite3_step(statement);

        if (sqlite_rc == SQLITE_ROW) {
            rc = materialize_index_column(statement, out_index_column);
        } else {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
            if (sqlite_rc == SQLITE_DONE) {
                rc = MYLITE_ERROR;
            }
        }
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_try_read_primary_index_by_table_id_from_sqlite(
    sqlite3 *sqlite,
    int64_t table_id,
    struct mylite_catalog_index_descriptor *out_index,
    bool *out_found
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_prepare_statement(
        sqlite,
        "SELECT index_id, table_id, name, kind, is_unique, is_visible, physical_name, "
        "comment, show_create_explicit_btree, descriptor_version, created_catalog_generation, "
        "updated_catalog_generation "
        "FROM _mylite_catalog_indexes WHERE table_id = ?1 AND kind = 1",
        &statement
    );

    *out_index = (struct mylite_catalog_index_descriptor){0};
    *out_found = false;
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = mylite_catalog_sqlite3_step(statement);
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

    return mylite_catalog_finalize_statement(statement, rc);
}

static int materialize_index(
    sqlite3_stmt *statement,
    struct mylite_catalog_index_descriptor *out_index
) {
    int64_t kind = 0;
    int64_t is_unique = 0;
    int64_t is_visible = 0;
    int64_t show_create_explicit_btree = 0;
    int rc = mylite_catalog_checked_column_i64(
        statement,
        catalog_index_select_index_id_column,
        &out_index->index_id
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_index_select_table_id_column,
            &out_index->table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_index_select_name_column,
            out_index->name,
            sizeof(out_index->name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(statement, catalog_index_select_kind_column, &kind);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_index_kind((enum mylite_catalog_index_kind)kind);
    }
    if (rc == MYLITE_OK) {
        out_index->kind = (enum mylite_catalog_index_kind)kind;
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_index_select_is_unique_column,
            &is_unique
        );
    }
    if (rc == MYLITE_OK && is_unique != 0 && is_unique != 1) {
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        out_index->is_unique = is_unique != 0;
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_index_select_is_visible_column,
            &is_visible
        );
    }
    if (rc == MYLITE_OK && is_visible != 0 && is_visible != 1) {
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        out_index->is_visible = is_visible != 0;
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_index_select_physical_name_column,
            out_index->physical_name,
            sizeof(out_index->physical_name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_index_select_comment_column,
            out_index->comment,
            sizeof(out_index->comment)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_index_select_show_create_explicit_btree_column,
            &show_create_explicit_btree
        );
    }
    if (rc == MYLITE_OK && show_create_explicit_btree != 0 && show_create_explicit_btree != 1) {
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        out_index->show_create_explicit_btree = show_create_explicit_btree != 0;
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_index_select_descriptor_version_column,
            &out_index->descriptor_version
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_index_select_created_generation_column,
            &out_index->created_catalog_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
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
    int rc = mylite_catalog_checked_column_i64(
        statement,
        catalog_index_column_select_index_column_id_column,
        &out_index_column->index_column_id
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_index_column_select_index_id_column,
            &out_index_column->index_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_index_column_select_table_id_column,
            &out_index_column->table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_index_column_select_column_id_column,
            &out_index_column->column_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_index_column_select_ordinal_position_column,
            &out_index_column->ordinal_position
        );
    }
    if (rc == MYLITE_OK &&
        sqlite3_column_type(statement, catalog_index_column_select_prefix_length_column) ==
            SQLITE_NULL) {
        out_index_column->has_prefix_length = false;
        out_index_column->prefix_length = 0;
    } else if (rc == MYLITE_OK) {
        out_index_column->has_prefix_length = true;
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_index_column_select_prefix_length_column,
            &out_index_column->prefix_length
        );
    }
    if (rc == MYLITE_OK) {
        int64_t sort_direction = 0;

        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_index_column_select_sort_direction_column,
            &sort_direction
        );
        if (rc == MYLITE_OK && (sort_direction == MYLITE_CATALOG_INDEX_SORT_DIRECTION_ASC ||
                                sort_direction == MYLITE_CATALOG_INDEX_SORT_DIRECTION_DESC)) {
            out_index_column->sort_direction =
                (enum mylite_catalog_index_sort_direction)sort_direction;
        } else if (rc == MYLITE_OK) {
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_index_column_select_descriptor_version_column,
            &out_index_column->descriptor_version
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_index_column_select_created_generation_column,
            &out_index_column->created_catalog_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_index_column_select_updated_generation_column,
            &out_index_column->updated_catalog_generation
        );
    }

    return rc;
}
