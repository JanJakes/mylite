#include "mylite_catalog.h"

#include "mylite_catalog_internal.h"

#include "mylite_connection.h"
#include "sqlite3.h"

#include <stdbool.h>
#include <stdint.h>

enum catalog_index_insert_bind_index {
    catalog_index_insert_index_id_bind = 1,
    catalog_index_insert_table_id_bind = 2,
    catalog_index_insert_name_bind = 3,
    catalog_index_insert_kind_bind = 4,
    catalog_index_insert_is_unique_bind = 5,
    catalog_index_insert_is_visible_bind = 6,
    catalog_index_insert_physical_name_bind = 7,
    catalog_index_insert_comment_bind = 8,
    catalog_index_insert_show_create_explicit_btree_bind = 9,
    catalog_index_insert_generation_bind = 10,
};

enum catalog_index_column_insert_bind_index {
    catalog_index_column_insert_index_id_bind = 1,
    catalog_index_column_insert_table_id_bind = 2,
    catalog_index_column_insert_column_id_bind = 3,
    catalog_index_column_insert_ordinal_position_bind = 4,
    catalog_index_column_insert_prefix_length_bind = 5,
    catalog_index_column_insert_sort_direction_bind = 6,
    catalog_index_column_insert_generation_bind = 7,
};

enum catalog_foreign_key_insert_bind_index {
    catalog_foreign_key_insert_foreign_key_id_bind = 1,
    catalog_foreign_key_insert_child_table_id_bind = 2,
    catalog_foreign_key_insert_parent_table_id_bind = 3,
    catalog_foreign_key_insert_name_bind = 4,
    catalog_foreign_key_insert_parent_index_id_bind = 5,
    catalog_foreign_key_insert_child_index_id_bind = 6,
    catalog_foreign_key_insert_update_rule_bind = 7,
    catalog_foreign_key_insert_delete_rule_bind = 8,
    catalog_foreign_key_insert_match_option_bind = 9,
    catalog_foreign_key_insert_generation_bind = 10,
};

enum catalog_foreign_key_column_insert_bind_index {
    catalog_foreign_key_column_insert_foreign_key_id_bind = 1,
    catalog_foreign_key_column_insert_child_table_id_bind = 2,
    catalog_foreign_key_column_insert_parent_table_id_bind = 3,
    catalog_foreign_key_column_insert_child_column_id_bind = 4,
    catalog_foreign_key_column_insert_parent_column_id_bind = 5,
    catalog_foreign_key_column_insert_ordinal_position_bind = 6,
    catalog_foreign_key_column_insert_position_in_unique_constraint_bind = 7,
    catalog_foreign_key_column_insert_generation_bind = 8,
};

enum catalog_check_constraint_insert_bind_index {
    catalog_check_constraint_insert_check_constraint_id_bind = 1,
    catalog_check_constraint_insert_table_id_bind = 2,
    catalog_check_constraint_insert_name_bind = 3,
    catalog_check_constraint_insert_physical_name_bind = 4,
    catalog_check_constraint_insert_check_clause_bind = 5,
    catalog_check_constraint_insert_sqlite_expression_bind = 6,
    catalog_check_constraint_insert_is_enforced_bind = 7,
    catalog_check_constraint_insert_name_is_generated_bind = 8,
    catalog_check_constraint_insert_generated_ordinal_bind = 9,
    catalog_check_constraint_insert_ordinal_position_bind = 10,
    catalog_check_constraint_insert_generation_bind = 11,
};

static int validate_insert_index_request(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t index_id,
    int64_t table_id,
    const char *name,
    const char *physical_name,
    enum mylite_catalog_index_kind kind,
    const char *comment
);
static int insert_index_descriptor_row(
    sqlite3 *sqlite,
    const struct mylite_catalog_mutation *mutation,
    int64_t index_id,
    int64_t table_id,
    const char *name,
    const char *physical_name,
    enum mylite_catalog_index_kind kind,
    bool is_unique,
    bool is_visible,
    const char *comment,
    bool show_create_explicit_btree
);
static int read_inserted_index_if_requested(
    sqlite3 *sqlite,
    int64_t table_id,
    struct mylite_catalog_index_descriptor *out_index
);
static int mark_table_fulltext_doc_id_initialized_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id
);
static int insert_index_column_row(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t index_id,
    int64_t table_id,
    int64_t column_id,
    int64_t ordinal_position,
    const int64_t *prefix_length,
    enum mylite_catalog_index_sort_direction sort_direction
);
static int insert_foreign_key_column_row(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t foreign_key_id,
    int64_t child_table_id,
    int64_t parent_table_id,
    int64_t child_column_id,
    int64_t parent_column_id,
    int64_t ordinal_position,
    int64_t position_in_unique_constraint
);

int mylite_catalog_insert_index_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t index_id,
    int64_t table_id,
    const char *name,
    const char *physical_name,
    enum mylite_catalog_index_kind kind,
    bool is_unique,
    bool is_visible,
    const char *comment,
    bool show_create_explicit_btree,
    struct mylite_catalog_index_descriptor *out_index
) {
    int rc = MYLITE_OK;

    if (out_index != NULL) {
        *out_index = (struct mylite_catalog_index_descriptor){0};
    }
    rc = validate_insert_index_request(
        database,
        mutation,
        index_id,
        table_id,
        name,
        physical_name,
        kind,
        comment == NULL ? "" : comment
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = insert_index_descriptor_row(
        database->sqlite,
        mutation,
        index_id,
        table_id,
        name,
        physical_name,
        kind,
        is_unique,
        is_visible,
        comment == NULL ? "" : comment,
        show_create_explicit_btree
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (kind == MYLITE_CATALOG_INDEX_KIND_FULLTEXT) {
        rc = mark_table_fulltext_doc_id_initialized_in_mutation(database, mutation, table_id);
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    return read_inserted_index_if_requested(database->sqlite, table_id, out_index);
}

static int validate_insert_index_request(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t index_id,
    int64_t table_id,
    const char *name,
    const char *physical_name,
    enum mylite_catalog_index_kind kind,
    const char *comment
) {
    struct mylite_catalog_table_descriptor table = {0};
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(index_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_required_name(name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc =
        mylite_catalog_validate_required_name(physical_name, MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_index_kind(kind);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_optional_name(comment, MYLITE_CATALOG_INDEX_COMMENT_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }

    return mylite_catalog_read_table_by_id_from_sqlite(database->sqlite, table_id, &table);
}

static int insert_index_descriptor_row(
    sqlite3 *sqlite,
    const struct mylite_catalog_mutation *mutation,
    int64_t index_id,
    int64_t table_id,
    const char *name,
    const char *physical_name,
    enum mylite_catalog_index_kind kind,
    bool is_unique,
    bool is_visible,
    const char *comment,
    bool show_create_explicit_btree
) {
    sqlite3_stmt *statement = NULL;
    int64_t unique_value = mylite_catalog_bool_value(is_unique);
    int64_t visible_value = mylite_catalog_bool_value(is_visible);
    int64_t explicit_btree_value = mylite_catalog_bool_value(show_create_explicit_btree);
    int rc = mylite_catalog_prepare_statement(
        sqlite,
        "INSERT INTO _mylite_catalog_indexes "
        "(index_id, table_id, name, kind, is_unique, is_visible, physical_name, "
        "comment, show_create_explicit_btree, descriptor_version, created_catalog_generation, "
        "updated_catalog_generation) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, 1, ?10, ?10)",
        &statement
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, catalog_index_insert_index_id_bind, index_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, catalog_index_insert_table_id_bind, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, catalog_index_insert_name_bind, name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, catalog_index_insert_kind_bind, (int64_t)kind);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, catalog_index_insert_is_unique_bind, unique_value);
    }
    if (rc == MYLITE_OK) {
        rc =
            mylite_catalog_bind_i64(statement, catalog_index_insert_is_visible_bind, visible_value);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_index_insert_physical_name_bind,
            physical_name
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, catalog_index_insert_comment_bind, comment);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_index_insert_show_create_explicit_btree_bind,
            explicit_btree_value
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_u64(
            statement,
            catalog_index_insert_generation_bind,
            mutation->next_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

static int read_inserted_index_if_requested(
    sqlite3 *sqlite,
    int64_t table_id,
    struct mylite_catalog_index_descriptor *out_index
) {
    bool found = false;
    int rc = MYLITE_OK;

    if (out_index == NULL) {
        return MYLITE_OK;
    }

    rc = mylite_catalog_try_read_primary_index_by_table_id_from_sqlite(
        sqlite,
        table_id,
        out_index,
        &found
    );
    if (rc == MYLITE_OK && !found) {
        rc = MYLITE_ERROR;
    }
    return rc;
}

static int mark_table_fulltext_doc_id_initialized_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id
) {
    struct mylite_catalog_table_descriptor table = {0};
    sqlite3_stmt *statement = NULL;
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_active_mutation(mutation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(table_id);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_read_table_by_id_from_sqlite(database->sqlite, table_id, &table);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (table.fulltext_doc_id_initialized) {
        return MYLITE_OK;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_tables "
        "SET fulltext_doc_id_initialized = 1, descriptor_version = descriptor_version + 1, "
        "updated_catalog_generation = ?1 "
        "WHERE table_id = ?2",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_u64(statement, 1, mutation->next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 2, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_insert_index_column_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t index_id,
    int64_t table_id,
    int64_t column_id,
    int64_t ordinal_position,
    const int64_t *prefix_length,
    enum mylite_catalog_index_sort_direction sort_direction,
    struct mylite_catalog_index_column_descriptor *out_index_column
) {
    int rc = MYLITE_OK;

    if (out_index_column != NULL) {
        *out_index_column = (struct mylite_catalog_index_column_descriptor){0};
    }
    rc = mylite_catalog_validate_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(index_id);
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
    if (prefix_length != NULL) {
        rc = mylite_catalog_validate_positive_ordinal(*prefix_length);
        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    if (sort_direction != MYLITE_CATALOG_INDEX_SORT_DIRECTION_ASC &&
        sort_direction != MYLITE_CATALOG_INDEX_SORT_DIRECTION_DESC) {
        return MYLITE_MISUSE;
    }

    rc = insert_index_column_row(
        database,
        mutation,
        index_id,
        table_id,
        column_id,
        ordinal_position,
        prefix_length,
        sort_direction
    );
    if (rc != MYLITE_OK || out_index_column == NULL) {
        return rc;
    }

    return mylite_catalog_read_inserted_index_column(
        database,
        index_id,
        ordinal_position,
        out_index_column
    );
}

static int insert_index_column_row(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t index_id,
    int64_t table_id,
    int64_t column_id,
    int64_t ordinal_position,
    const int64_t *prefix_length,
    enum mylite_catalog_index_sort_direction sort_direction
) {
    sqlite3_stmt *statement = NULL;
    bool has_prefix_length = prefix_length != NULL;
    int64_t prefix_length_value = 0;
    int rc = MYLITE_OK;

    if (has_prefix_length) {
        prefix_length_value = *prefix_length;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "INSERT INTO _mylite_catalog_index_columns "
        "(index_id, table_id, column_id, ordinal_position, prefix_length, sort_direction, "
        "descriptor_version, created_catalog_generation, updated_catalog_generation) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, 1, ?7, ?7)",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc =
            mylite_catalog_bind_i64(statement, catalog_index_column_insert_index_id_bind, index_id);
    }
    if (rc == MYLITE_OK) {
        rc =
            mylite_catalog_bind_i64(statement, catalog_index_column_insert_table_id_bind, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_index_column_insert_column_id_bind,
            column_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_index_column_insert_ordinal_position_bind,
            ordinal_position
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_nullable_i64(
            statement,
            catalog_index_column_insert_prefix_length_bind,
            has_prefix_length,
            prefix_length_value
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_index_column_insert_sort_direction_bind,
            (int64_t)sort_direction
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_u64(
            statement,
            catalog_index_column_insert_generation_bind,
            mutation->next_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_insert_foreign_key_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t foreign_key_id,
    int64_t child_table_id,
    int64_t parent_table_id,
    const char *name,
    int64_t parent_index_id,
    int64_t child_index_id,
    const char *update_rule,
    const char *delete_rule,
    const char *match_option,
    struct mylite_catalog_foreign_key_descriptor *out_foreign_key
) {
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    if (out_foreign_key != NULL) {
        *out_foreign_key = (struct mylite_catalog_foreign_key_descriptor){0};
    }
    rc = mylite_catalog_validate_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(foreign_key_id);
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(child_table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(parent_table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(parent_index_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(child_index_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(update_rule, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(delete_rule, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    }
    if (rc == MYLITE_OK) {
        rc =
            mylite_catalog_validate_required_name(match_option, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "INSERT INTO _mylite_catalog_foreign_keys "
        "(foreign_key_id, child_table_id, parent_table_id, name, parent_index_id, "
        "child_index_id, update_rule, delete_rule, match_option, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, 1, ?10, ?10)",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_foreign_key_insert_foreign_key_id_bind,
            foreign_key_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_foreign_key_insert_child_table_id_bind,
            child_table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_foreign_key_insert_parent_table_id_bind,
            parent_table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, catalog_foreign_key_insert_name_bind, name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_foreign_key_insert_parent_index_id_bind,
            parent_index_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_foreign_key_insert_child_index_id_bind,
            child_index_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_foreign_key_insert_update_rule_bind,
            update_rule
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_foreign_key_insert_delete_rule_bind,
            delete_rule
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_foreign_key_insert_match_option_bind,
            match_option
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_u64(
            statement,
            catalog_foreign_key_insert_generation_bind,
            mutation->next_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
    if (rc != MYLITE_OK || out_foreign_key == NULL) {
        return rc;
    }

    return mylite_catalog_read_inserted_foreign_key(
        database,
        child_table_id,
        name,
        out_foreign_key
    );
}

int mylite_catalog_insert_foreign_key_column_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t foreign_key_id,
    int64_t child_table_id,
    int64_t parent_table_id,
    int64_t child_column_id,
    int64_t parent_column_id,
    int64_t ordinal_position,
    int64_t position_in_unique_constraint,
    struct mylite_catalog_foreign_key_column_descriptor *out_foreign_key_column
) {
    int rc = MYLITE_OK;

    if (out_foreign_key_column != NULL) {
        *out_foreign_key_column = (struct mylite_catalog_foreign_key_column_descriptor){0};
    }
    rc = mylite_catalog_validate_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(foreign_key_id);
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(child_table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(parent_table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(child_column_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(parent_column_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_ordinal(ordinal_position);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_ordinal(position_in_unique_constraint);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = insert_foreign_key_column_row(
        database,
        mutation,
        foreign_key_id,
        child_table_id,
        parent_table_id,
        child_column_id,
        parent_column_id,
        ordinal_position,
        position_in_unique_constraint
    );
    if (rc != MYLITE_OK || out_foreign_key_column == NULL) {
        return rc;
    }

    return mylite_catalog_read_inserted_foreign_key_column(
        database,
        foreign_key_id,
        ordinal_position,
        out_foreign_key_column
    );
}

int mylite_catalog_insert_check_constraint_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t check_constraint_id,
    int64_t table_id,
    const char *name,
    const char *physical_name,
    const char *check_clause,
    const char *sqlite_expression,
    bool is_enforced,
    bool name_is_generated,
    int64_t generated_ordinal,
    int64_t ordinal_position,
    struct mylite_catalog_check_constraint_descriptor *out_check_constraint
) {
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    if (out_check_constraint != NULL) {
        *out_check_constraint = (struct mylite_catalog_check_constraint_descriptor){0};
    }
    rc = mylite_catalog_validate_ready_database(database);
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_active_mutation(mutation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(check_constraint_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_logical_object_name(name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(
            physical_name,
            MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(
            check_clause,
            MYLITE_CATALOG_CHECK_CLAUSE_CAPACITY
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(
            sqlite_expression,
            MYLITE_CATALOG_CHECK_CLAUSE_CAPACITY
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_ordinal(generated_ordinal);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_ordinal(ordinal_position);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "INSERT INTO _mylite_catalog_check_constraints "
        "(check_constraint_id, table_id, name, physical_name, check_clause, sqlite_expression, "
        "is_enforced, name_is_generated, generated_ordinal, ordinal_position, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, 1, ?11, ?11)",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_check_constraint_insert_check_constraint_id_bind,
            check_constraint_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_check_constraint_insert_table_id_bind,
            table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, catalog_check_constraint_insert_name_bind, name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_check_constraint_insert_physical_name_bind,
            physical_name
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_check_constraint_insert_check_clause_bind,
            check_clause
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_check_constraint_insert_sqlite_expression_bind,
            sqlite_expression
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_check_constraint_insert_is_enforced_bind,
            mylite_catalog_bool_value(is_enforced)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_check_constraint_insert_name_is_generated_bind,
            mylite_catalog_bool_value(name_is_generated)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_check_constraint_insert_generated_ordinal_bind,
            generated_ordinal
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_check_constraint_insert_ordinal_position_bind,
            ordinal_position
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_u64(
            statement,
            catalog_check_constraint_insert_generation_bind,
            mutation->next_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
    if (rc != MYLITE_OK || out_check_constraint == NULL) {
        return rc;
    }

    return mylite_catalog_read_inserted_check_constraint(
        database,
        table_id,
        name,
        out_check_constraint
    );
}

static int insert_foreign_key_column_row(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t foreign_key_id,
    int64_t child_table_id,
    int64_t parent_table_id,
    int64_t child_column_id,
    int64_t parent_column_id,
    int64_t ordinal_position,
    int64_t position_in_unique_constraint
) {
    sqlite3_stmt *statement = NULL;
    int rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "INSERT INTO _mylite_catalog_foreign_key_columns "
        "(foreign_key_id, child_table_id, parent_table_id, child_column_id, "
        "parent_column_id, ordinal_position, position_in_unique_constraint, "
        "descriptor_version, created_catalog_generation, updated_catalog_generation) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, 1, ?8, ?8)",
        &statement
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_foreign_key_column_insert_foreign_key_id_bind,
            foreign_key_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_foreign_key_column_insert_child_table_id_bind,
            child_table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_foreign_key_column_insert_parent_table_id_bind,
            parent_table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_foreign_key_column_insert_child_column_id_bind,
            child_column_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_foreign_key_column_insert_parent_column_id_bind,
            parent_column_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_foreign_key_column_insert_ordinal_position_bind,
            ordinal_position
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_foreign_key_column_insert_position_in_unique_constraint_bind,
            position_in_unique_constraint
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_u64(
            statement,
            catalog_foreign_key_column_insert_generation_bind,
            mutation->next_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_delete_index_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    int64_t index_id
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
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(index_id);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "DELETE FROM _mylite_catalog_index_columns WHERE table_id = ?1 AND index_id = ?2",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 2, index_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
    statement = NULL;

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_indexes WHERE table_id = ?1 AND index_id = ?2",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 2, index_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_delete_index_column_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    int64_t index_id,
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
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(index_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(column_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_ordinal(ordinal_position);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "DELETE FROM _mylite_catalog_index_columns "
        "WHERE table_id = ?1 AND index_id = ?2 AND column_id = ?3 AND ordinal_position = ?4",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 2, index_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 3, column_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 4, ordinal_position);
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
            "UPDATE _mylite_catalog_index_columns "
            "SET ordinal_position = ordinal_position - 1, "
            "descriptor_version = descriptor_version + 1, "
            "updated_catalog_generation = ?1 "
            "WHERE table_id = ?2 AND index_id = ?3 AND ordinal_position > ?4",
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
        rc = mylite_catalog_bind_i64(statement, 3, index_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 4, ordinal_position);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
    statement = NULL;

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "UPDATE _mylite_catalog_indexes "
            "SET descriptor_version = descriptor_version + 1, "
            "updated_catalog_generation = ?1 "
            "WHERE table_id = ?2 AND index_id = ?3",
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
        rc = mylite_catalog_bind_i64(statement, 3, index_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_rename_index_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    int64_t index_id,
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
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(index_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_indexes "
        "SET name = ?1, descriptor_version = descriptor_version + 1, "
        "updated_catalog_generation = ?2 "
        "WHERE table_id = ?3 AND index_id = ?4",
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
        rc = mylite_catalog_bind_i64(statement, 4, index_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_set_index_visibility_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    int64_t index_id,
    bool is_visible
) {
    sqlite3_stmt *statement = NULL;
    int64_t visible_value = mylite_catalog_bool_value(is_visible);
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(index_id);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_indexes "
        "SET is_visible = ?1, descriptor_version = descriptor_version + 1, "
        "updated_catalog_generation = ?2 "
        "WHERE table_id = ?3 AND index_id = ?4",
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
        rc = mylite_catalog_bind_i64(statement, 4, index_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_delete_foreign_keys_for_child_table_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t child_table_id
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
    rc = mylite_catalog_validate_positive_id(child_table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "DELETE FROM _mylite_catalog_foreign_key_columns WHERE child_table_id = ?1",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, child_table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
    statement = NULL;

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_foreign_keys WHERE child_table_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, child_table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_delete_foreign_key_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t child_table_id,
    int64_t foreign_key_id
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
    rc = mylite_catalog_validate_positive_id(child_table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(foreign_key_id);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "DELETE FROM _mylite_catalog_foreign_key_columns "
        "WHERE child_table_id = ?1 AND foreign_key_id = ?2",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, child_table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 2, foreign_key_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
    statement = NULL;

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_foreign_keys "
            "WHERE child_table_id = ?1 AND foreign_key_id = ?2",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, child_table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 2, foreign_key_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_delete_check_constraints_for_table_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id
) {
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_active_mutation(mutation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(table_id);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    return mylite_catalog_delete_check_constraints_for_table_from_sqlite(
        database->sqlite,
        table_id
    );
}

int mylite_catalog_delete_check_constraint_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    int64_t check_constraint_id
) {
    sqlite3_stmt *statement = NULL;
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_active_mutation(mutation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(check_constraint_id);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "DELETE FROM _mylite_catalog_check_constraints "
        "WHERE table_id = ?1 AND check_constraint_id = ?2",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 2, check_constraint_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_update_check_constraint_enforcement_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    int64_t check_constraint_id,
    bool is_enforced
) {
    sqlite3_stmt *statement = NULL;
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_active_mutation(mutation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(check_constraint_id);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_check_constraints "
        "SET is_enforced = ?1, descriptor_version = descriptor_version + 1, "
        "updated_catalog_generation = ?2 "
        "WHERE table_id = ?3 AND check_constraint_id = ?4",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, mylite_catalog_bool_value(is_enforced));
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_u64(statement, 2, mutation->next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 3, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 4, check_constraint_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_rename_generated_check_constraints_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    const char *table_name
) {
    sqlite3_stmt *statement = NULL;
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_active_mutation(mutation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_logical_object_name(
            table_name,
            MYLITE_CATALOG_IDENTIFIER_CAPACITY
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_check_constraints "
        "SET name = ?1 || '_chk_' || generated_ordinal, "
        "descriptor_version = descriptor_version + 1, updated_catalog_generation = ?2 "
        "WHERE table_id = ?3 AND name_is_generated = 1",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, 1, table_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_u64(statement, 2, mutation->next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 3, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_delete_foreign_keys_for_related_table_from_sqlite(
    sqlite3 *sqlite,
    int64_t table_id
) {
    sqlite3_stmt *statement = NULL;
    int rc = mylite_catalog_prepare_statement(
        sqlite,
        "DELETE FROM _mylite_catalog_foreign_key_columns "
        "WHERE child_table_id = ?1 OR parent_table_id = ?1",
        &statement
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
    statement = NULL;

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            sqlite,
            "DELETE FROM _mylite_catalog_foreign_keys "
            "WHERE child_table_id = ?1 OR parent_table_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_delete_foreign_keys_for_schema_from_sqlite(sqlite3 *sqlite, int64_t schema_id) {
    sqlite3_stmt *statement = NULL;
    int rc = mylite_catalog_prepare_statement(
        sqlite,
        "DELETE FROM _mylite_catalog_foreign_key_columns "
        "WHERE child_table_id IN ("
        "SELECT table_id FROM _mylite_catalog_tables WHERE schema_id = ?1"
        ") OR parent_table_id IN ("
        "SELECT table_id FROM _mylite_catalog_tables WHERE schema_id = ?1"
        ")",
        &statement
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
    statement = NULL;

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            sqlite,
            "DELETE FROM _mylite_catalog_foreign_keys "
            "WHERE child_table_id IN ("
            "SELECT table_id FROM _mylite_catalog_tables WHERE schema_id = ?1"
            ") OR parent_table_id IN ("
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

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_delete_check_constraints_for_table_from_sqlite(
    sqlite3 *sqlite,
    int64_t table_id
) {
    sqlite3_stmt *statement = NULL;
    int rc = mylite_catalog_prepare_statement(
        sqlite,
        "DELETE FROM _mylite_catalog_check_constraints WHERE table_id = ?1",
        &statement
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_delete_check_constraints_for_schema_from_sqlite(
    sqlite3 *sqlite,
    int64_t schema_id
) {
    sqlite3_stmt *statement = NULL;
    int rc = mylite_catalog_prepare_statement(
        sqlite,
        "DELETE FROM _mylite_catalog_check_constraints "
        "WHERE table_id IN (SELECT table_id FROM _mylite_catalog_tables WHERE schema_id = ?1)",
        &statement
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}
