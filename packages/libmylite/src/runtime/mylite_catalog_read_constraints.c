#include "mylite_catalog.h"

#include "mylite_catalog_internal.h"

#include "mylite_connection.h"
#include "mylite_sqlite_registration.h"
#include "sqlite3.h"

#include <stdbool.h>
#include <stdint.h>

enum catalog_foreign_key_select_column_index {
    catalog_foreign_key_select_foreign_key_id_column = 0,
    catalog_foreign_key_select_child_table_id_column = 1,
    catalog_foreign_key_select_parent_table_id_column = 2,
    catalog_foreign_key_select_name_column = 3,
    catalog_foreign_key_select_parent_index_id_column = 4,
    catalog_foreign_key_select_child_index_id_column = 5,
    catalog_foreign_key_select_update_rule_column = 6,
    catalog_foreign_key_select_delete_rule_column = 7,
    catalog_foreign_key_select_match_option_column = 8,
    catalog_foreign_key_select_descriptor_version_column = 9,
    catalog_foreign_key_select_created_generation_column = 10,
    catalog_foreign_key_select_updated_generation_column = 11,
};

enum catalog_foreign_key_column_select_column_index {
    catalog_foreign_key_column_select_foreign_key_column_id_column = 0,
    catalog_foreign_key_column_select_foreign_key_id_column = 1,
    catalog_foreign_key_column_select_child_table_id_column = 2,
    catalog_foreign_key_column_select_parent_table_id_column = 3,
    catalog_foreign_key_column_select_child_column_id_column = 4,
    catalog_foreign_key_column_select_parent_column_id_column = 5,
    catalog_foreign_key_column_select_ordinal_position_column = 6,
    catalog_foreign_key_column_select_position_in_unique_constraint_column = 7,
    catalog_foreign_key_column_select_descriptor_version_column = 8,
    catalog_foreign_key_column_select_created_generation_column = 9,
    catalog_foreign_key_column_select_updated_generation_column = 10,
};

enum catalog_check_constraint_select_column_index {
    catalog_check_constraint_select_check_constraint_id_column = 0,
    catalog_check_constraint_select_table_id_column = 1,
    catalog_check_constraint_select_name_column = 2,
    catalog_check_constraint_select_physical_name_column = 3,
    catalog_check_constraint_select_check_clause_column = 4,
    catalog_check_constraint_select_sqlite_expression_column = 5,
    catalog_check_constraint_select_is_enforced_column = 6,
    catalog_check_constraint_select_name_is_generated_column = 7,
    catalog_check_constraint_select_generated_ordinal_column = 8,
    catalog_check_constraint_select_ordinal_position_column = 9,
    catalog_check_constraint_select_descriptor_version_column = 10,
    catalog_check_constraint_select_created_generation_column = 11,
    catalog_check_constraint_select_updated_generation_column = 12,
};

static int materialize_foreign_key(
    sqlite3_stmt *statement,
    struct mylite_catalog_foreign_key_descriptor *out_foreign_key
);
static int materialize_foreign_key_column(
    sqlite3_stmt *statement,
    struct mylite_catalog_foreign_key_column_descriptor *out_foreign_key_column
);
static int materialize_check_constraint(
    sqlite3_stmt *statement,
    struct mylite_catalog_check_constraint_descriptor *out_check_constraint
);

int mylite_catalog_for_each_foreign_key_in_child_table(
    struct mylite_db *database,
    int64_t child_table_id,
    mylite_catalog_foreign_key_callback callback,
    void *user_data
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(child_table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_foreign_key_callback(callback);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "SELECT foreign_key_id, child_table_id, parent_table_id, name, parent_index_id, "
        "child_index_id, update_rule, delete_rule, match_option, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_foreign_keys WHERE child_table_id = ?1 ORDER BY foreign_key_id",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, child_table_id);
    }
    while (rc == MYLITE_OK) {
        struct mylite_catalog_foreign_key_descriptor foreign_key = {0};

        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            break;
        }
        if (sqlite_rc != SQLITE_ROW) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
            break;
        }

        rc = materialize_foreign_key(statement, &foreign_key);
        if (rc == MYLITE_OK) {
            rc = callback(&foreign_key, user_data);
        }
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_for_each_foreign_key_for_parent_table(
    struct mylite_db *database,
    int64_t parent_table_id,
    mylite_catalog_foreign_key_callback callback,
    void *user_data
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(parent_table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_foreign_key_callback(callback);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "SELECT foreign_key_id, child_table_id, parent_table_id, name, parent_index_id, "
        "child_index_id, update_rule, delete_rule, match_option, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_foreign_keys WHERE parent_table_id = ?1 ORDER BY foreign_key_id",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, parent_table_id);
    }
    while (rc == MYLITE_OK) {
        struct mylite_catalog_foreign_key_descriptor foreign_key = {0};

        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            break;
        }
        if (sqlite_rc != SQLITE_ROW) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
            break;
        }

        rc = materialize_foreign_key(statement, &foreign_key);
        if (rc == MYLITE_OK) {
            rc = callback(&foreign_key, user_data);
        }
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_read_table_foreign_key_roles(
    struct mylite_db *database,
    int64_t table_id,
    bool *out_has_child_foreign_keys,
    bool *out_has_parent_foreign_keys
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_has_child_foreign_keys == NULL || out_has_parent_foreign_keys == NULL) {
        return MYLITE_MISUSE;
    }
    *out_has_child_foreign_keys = false;
    *out_has_parent_foreign_keys = false;
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (mylite_catalog_find_cached_foreign_key_roles(
            &database->catalog,
            table_id,
            out_has_child_foreign_keys,
            out_has_parent_foreign_keys
        )) {
        return MYLITE_OK;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "SELECT EXISTS(SELECT 1 FROM _mylite_catalog_foreign_keys "
        "WHERE child_table_id = ?1 LIMIT 1), "
        "EXISTS(SELECT 1 FROM _mylite_catalog_foreign_keys "
        "WHERE parent_table_id = ?1 LIMIT 1)",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            *out_has_child_foreign_keys = sqlite3_column_int(statement, 0) != 0;
            *out_has_parent_foreign_keys = sqlite3_column_int(statement, 1) != 0;
            sqlite_rc = sqlite3_step(statement);
            if (sqlite_rc != SQLITE_DONE) {
                rc = mylite_sqlite_status_to_mylite(sqlite_rc);
            }
        } else {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
    if (rc == MYLITE_OK) {
        mylite_catalog_cache_foreign_key_roles(
            &database->catalog,
            table_id,
            *out_has_child_foreign_keys,
            *out_has_parent_foreign_keys
        );
    }
    return rc;
}

int mylite_catalog_for_each_foreign_key_column_in_foreign_key(
    struct mylite_db *database,
    int64_t foreign_key_id,
    mylite_catalog_foreign_key_column_callback callback,
    void *user_data
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(foreign_key_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_foreign_key_column_callback(callback);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "SELECT foreign_key_column_id, foreign_key_id, child_table_id, parent_table_id, "
        "child_column_id, parent_column_id, ordinal_position, position_in_unique_constraint, "
        "descriptor_version, created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_foreign_key_columns "
        "WHERE foreign_key_id = ?1 ORDER BY ordinal_position",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, foreign_key_id);
    }
    while (rc == MYLITE_OK) {
        struct mylite_catalog_foreign_key_column_descriptor foreign_key_column = {0};

        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            break;
        }
        if (sqlite_rc != SQLITE_ROW) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
            break;
        }

        rc = materialize_foreign_key_column(statement, &foreign_key_column);
        if (rc == MYLITE_OK) {
            rc = callback(&foreign_key_column, user_data);
        }
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_for_each_check_constraint_in_table(
    struct mylite_db *database,
    int64_t table_id,
    mylite_catalog_check_constraint_callback callback,
    void *user_data
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_check_constraint_callback(callback);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "SELECT check_constraint_id, table_id, name, physical_name, check_clause, "
        "sqlite_expression, is_enforced, name_is_generated, generated_ordinal, "
        "ordinal_position, descriptor_version, created_catalog_generation, "
        "updated_catalog_generation "
        "FROM _mylite_catalog_check_constraints WHERE table_id = ?1 ORDER BY name",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    while (rc == MYLITE_OK) {
        struct mylite_catalog_check_constraint_descriptor check_constraint = {0};

        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            break;
        }
        if (sqlite_rc != SQLITE_ROW) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
            break;
        }

        rc = materialize_check_constraint(statement, &check_constraint);
        if (rc == MYLITE_OK) {
            rc = callback(&check_constraint, user_data);
        }
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_for_each_check_constraint_in_schema(
    struct mylite_db *database,
    int64_t schema_id,
    mylite_catalog_check_constraint_callback callback,
    void *user_data
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_check_constraint_callback(callback);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "SELECT c.check_constraint_id, c.table_id, c.name, c.physical_name, c.check_clause, "
        "c.sqlite_expression, c.is_enforced, c.name_is_generated, c.generated_ordinal, "
        "c.ordinal_position, c.descriptor_version, c.created_catalog_generation, "
        "c.updated_catalog_generation "
        "FROM _mylite_catalog_check_constraints c "
        "JOIN _mylite_catalog_tables t ON t.table_id = c.table_id "
        "WHERE t.schema_id = ?1 ORDER BY c.name",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, schema_id);
    }
    while (rc == MYLITE_OK) {
        struct mylite_catalog_check_constraint_descriptor check_constraint = {0};

        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            break;
        }
        if (sqlite_rc != SQLITE_ROW) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
            break;
        }

        rc = materialize_check_constraint(statement, &check_constraint);
        if (rc == MYLITE_OK) {
            rc = callback(&check_constraint, user_data);
        }
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_try_read_check_constraint_by_physical_name(
    struct mylite_db *database,
    int64_t table_id,
    const char *physical_name,
    struct mylite_catalog_check_constraint_descriptor *out_check_constraint,
    bool *out_found
) {
    sqlite3_stmt *statement = NULL;
    int rc = mylite_catalog_validate_ready_database(database);

    if (out_check_constraint != NULL) {
        *out_check_constraint = (struct mylite_catalog_check_constraint_descriptor){0};
    }
    if (out_found != NULL) {
        *out_found = false;
    }
    if (rc == MYLITE_OK && (out_check_constraint == NULL || out_found == NULL)) {
        return MYLITE_MISUSE;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(
            physical_name,
            MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "SELECT check_constraint_id, table_id, name, physical_name, check_clause, "
        "sqlite_expression, is_enforced, name_is_generated, generated_ordinal, "
        "ordinal_position, descriptor_version, created_catalog_generation, "
        "updated_catalog_generation "
        "FROM _mylite_catalog_check_constraints "
        "WHERE table_id = ?1 AND physical_name = ?2",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, 2, physical_name);
    }
    if (rc == MYLITE_OK) {
        int sqlite_rc = sqlite3_step(statement);

        if (sqlite_rc == SQLITE_ROW) {
            rc = materialize_check_constraint(statement, out_check_constraint);
            *out_found = rc == MYLITE_OK;
        } else if (sqlite_rc != SQLITE_DONE) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_read_inserted_foreign_key(
    struct mylite_db *database,
    int64_t child_table_id,
    const char *name,
    struct mylite_catalog_foreign_key_descriptor *out_foreign_key
) {
    sqlite3_stmt *statement = NULL;
    int rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "SELECT foreign_key_id, child_table_id, parent_table_id, name, parent_index_id, "
        "child_index_id, update_rule, delete_rule, match_option, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_foreign_keys WHERE child_table_id = ?1 AND name = ?2",
        &statement
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, child_table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, 2, name);
    }
    if (rc == MYLITE_OK) {
        int sqlite_rc = sqlite3_step(statement);

        if (sqlite_rc == SQLITE_ROW) {
            rc = materialize_foreign_key(statement, out_foreign_key);
        } else {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
            if (sqlite_rc == SQLITE_DONE) {
                rc = MYLITE_ERROR;
            }
        }
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_read_inserted_foreign_key_column(
    struct mylite_db *database,
    int64_t foreign_key_id,
    int64_t ordinal_position,
    struct mylite_catalog_foreign_key_column_descriptor *out_foreign_key_column
) {
    sqlite3_stmt *statement = NULL;
    int rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "SELECT foreign_key_column_id, foreign_key_id, child_table_id, parent_table_id, "
        "child_column_id, parent_column_id, ordinal_position, position_in_unique_constraint, "
        "descriptor_version, created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_foreign_key_columns "
        "WHERE foreign_key_id = ?1 AND ordinal_position = ?2",
        &statement
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, foreign_key_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 2, ordinal_position);
    }
    if (rc == MYLITE_OK) {
        int sqlite_rc = sqlite3_step(statement);

        if (sqlite_rc == SQLITE_ROW) {
            rc = materialize_foreign_key_column(statement, out_foreign_key_column);
        } else {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
            if (sqlite_rc == SQLITE_DONE) {
                rc = MYLITE_ERROR;
            }
        }
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_read_inserted_check_constraint(
    struct mylite_db *database,
    int64_t table_id,
    const char *name,
    struct mylite_catalog_check_constraint_descriptor *out_check_constraint
) {
    sqlite3_stmt *statement = NULL;
    int rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "SELECT check_constraint_id, table_id, name, physical_name, check_clause, "
        "sqlite_expression, is_enforced, name_is_generated, generated_ordinal, "
        "ordinal_position, descriptor_version, created_catalog_generation, "
        "updated_catalog_generation "
        "FROM _mylite_catalog_check_constraints WHERE table_id = ?1 AND name = ?2",
        &statement
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, 2, name);
    }
    if (rc == MYLITE_OK) {
        int sqlite_rc = sqlite3_step(statement);

        if (sqlite_rc == SQLITE_ROW) {
            rc = materialize_check_constraint(statement, out_check_constraint);
        } else {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
            if (sqlite_rc == SQLITE_DONE) {
                rc = MYLITE_ERROR;
            }
        }
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

static int materialize_foreign_key(
    sqlite3_stmt *statement,
    struct mylite_catalog_foreign_key_descriptor *out_foreign_key
) {
    int rc = mylite_catalog_checked_column_i64(
        statement,
        catalog_foreign_key_select_foreign_key_id_column,
        &out_foreign_key->foreign_key_id
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_foreign_key_select_child_table_id_column,
            &out_foreign_key->child_table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_foreign_key_select_parent_table_id_column,
            &out_foreign_key->parent_table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_foreign_key_select_name_column,
            out_foreign_key->name,
            sizeof(out_foreign_key->name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_foreign_key_select_parent_index_id_column,
            &out_foreign_key->parent_index_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_foreign_key_select_child_index_id_column,
            &out_foreign_key->child_index_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_foreign_key_select_update_rule_column,
            out_foreign_key->update_rule,
            sizeof(out_foreign_key->update_rule)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_foreign_key_select_delete_rule_column,
            out_foreign_key->delete_rule,
            sizeof(out_foreign_key->delete_rule)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_foreign_key_select_match_option_column,
            out_foreign_key->match_option,
            sizeof(out_foreign_key->match_option)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_foreign_key_select_descriptor_version_column,
            &out_foreign_key->descriptor_version
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_foreign_key_select_created_generation_column,
            &out_foreign_key->created_catalog_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_foreign_key_select_updated_generation_column,
            &out_foreign_key->updated_catalog_generation
        );
    }

    return rc;
}

static int materialize_foreign_key_column(
    sqlite3_stmt *statement,
    struct mylite_catalog_foreign_key_column_descriptor *out_foreign_key_column
) {
    int rc = mylite_catalog_checked_column_i64(
        statement,
        catalog_foreign_key_column_select_foreign_key_column_id_column,
        &out_foreign_key_column->foreign_key_column_id
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_foreign_key_column_select_foreign_key_id_column,
            &out_foreign_key_column->foreign_key_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_foreign_key_column_select_child_table_id_column,
            &out_foreign_key_column->child_table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_foreign_key_column_select_parent_table_id_column,
            &out_foreign_key_column->parent_table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_foreign_key_column_select_child_column_id_column,
            &out_foreign_key_column->child_column_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_foreign_key_column_select_parent_column_id_column,
            &out_foreign_key_column->parent_column_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_foreign_key_column_select_ordinal_position_column,
            &out_foreign_key_column->ordinal_position
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_foreign_key_column_select_position_in_unique_constraint_column,
            &out_foreign_key_column->position_in_unique_constraint
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_foreign_key_column_select_descriptor_version_column,
            &out_foreign_key_column->descriptor_version
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_foreign_key_column_select_created_generation_column,
            &out_foreign_key_column->created_catalog_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_foreign_key_column_select_updated_generation_column,
            &out_foreign_key_column->updated_catalog_generation
        );
    }

    return rc;
}

static int materialize_check_constraint(
    sqlite3_stmt *statement,
    struct mylite_catalog_check_constraint_descriptor *out_check_constraint
) {
    int64_t is_enforced = 0;
    int64_t name_is_generated = 0;
    int rc = mylite_catalog_checked_column_i64(
        statement,
        catalog_check_constraint_select_check_constraint_id_column,
        &out_check_constraint->check_constraint_id
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_check_constraint_select_table_id_column,
            &out_check_constraint->table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_check_constraint_select_name_column,
            out_check_constraint->name,
            sizeof(out_check_constraint->name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_check_constraint_select_physical_name_column,
            out_check_constraint->physical_name,
            sizeof(out_check_constraint->physical_name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_check_constraint_select_check_clause_column,
            out_check_constraint->check_clause,
            sizeof(out_check_constraint->check_clause)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_check_constraint_select_sqlite_expression_column,
            out_check_constraint->sqlite_expression,
            sizeof(out_check_constraint->sqlite_expression)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_check_constraint_select_is_enforced_column,
            &is_enforced
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_bool_i64(is_enforced, &out_check_constraint->is_enforced);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_check_constraint_select_name_is_generated_column,
            &name_is_generated
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_bool_i64(
            name_is_generated,
            &out_check_constraint->name_is_generated
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_check_constraint_select_generated_ordinal_column,
            &out_check_constraint->generated_ordinal
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_check_constraint_select_ordinal_position_column,
            &out_check_constraint->ordinal_position
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_check_constraint_select_descriptor_version_column,
            &out_check_constraint->descriptor_version
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_check_constraint_select_created_generation_column,
            &out_check_constraint->created_catalog_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_check_constraint_select_updated_generation_column,
            &out_check_constraint->updated_catalog_generation
        );
    }

    return rc;
}
