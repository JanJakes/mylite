#include "mylite_dml.h"

#include "mylite_catalog.h"
#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_dml_insert_column_reference.h"
#include "mylite_dml_insert_sqlite_bind.h"
#include "mylite_error_codes.h"
#include "mylite_select_types.h"
#include "mylite_span.h"
#include "sql/mylite_expression.h"
#include "sqlite3.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum mylite_parent_foreign_key_action {
    MYLITE_PARENT_FOREIGN_KEY_UPDATE,
    MYLITE_PARENT_FOREIGN_KEY_DELETE,
};

enum mylite_parent_foreign_key_row_kind {
    MYLITE_PARENT_FOREIGN_KEY_UPDATE_ROW,
    MYLITE_PARENT_FOREIGN_KEY_INSERT_ROW,
};

struct mylite_parent_foreign_key_row {
    enum mylite_parent_foreign_key_row_kind kind;
    const struct mylite_select_table *select_table;
    const struct mylite_update_row *update_row;
    const struct mylite_insert_table *insert_table;
    const struct mylite_insert_bound_value *insert_values;
};

static int validate_child_foreign_key_constraint(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    const struct mylite_insert_table *table,
    const struct mylite_insert_bound_value *values,
    sqlite3_stmt *constraint,
    bool ignore,
    bool *out_ignored
);

static int child_foreign_key_has_parent(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    const struct mylite_insert_table *table,
    const struct mylite_insert_bound_value *values,
    sqlite3_stmt *constraint,
    bool *out_has_parent
);

static int append_child_foreign_key_part(
    mylite_db *database,
    sqlite3_str *sql,
    const struct mylite_insert_table *table,
    const struct mylite_insert_bound_value *values,
    const char *column_name,
    const char *referenced_column_name,
    bool self_referencing,
    bool *self_row_matches,
    size_t **child_indexes,
    size_t *child_index_count,
    bool *out_skip_constraint
);

static int child_foreign_key_query_parent(
    mylite_db *database,
    char *sql,
    const size_t *child_indexes,
    size_t child_index_count,
    const struct mylite_insert_bound_value *values,
    bool *out_has_parent
);

static int set_child_foreign_key_violation(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    const char *constraint_name,
    bool ignore,
    bool *out_ignored
);

static bool insert_bound_values_equal(
    const struct mylite_insert_bound_value *left,
    const struct mylite_insert_bound_value *right
);

static int validate_parent_foreign_key_references(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    const struct mylite_parent_foreign_key_row *stored,
    const struct mylite_parent_foreign_key_row *candidate,
    enum mylite_parent_foreign_key_action action
);

static int validate_parent_foreign_key_constraint(
    mylite_db *database,
    const struct mylite_parent_foreign_key_row *stored,
    const struct mylite_parent_foreign_key_row *candidate,
    sqlite3_stmt *constraint,
    enum mylite_parent_foreign_key_action action
);

static int parent_foreign_key_has_child(
    mylite_db *database,
    const struct mylite_parent_foreign_key_row *stored,
    const struct mylite_parent_foreign_key_row *candidate,
    sqlite3_stmt *constraint,
    enum mylite_parent_foreign_key_action action,
    bool *out_has_child
);

static int append_parent_foreign_key_part(
    mylite_db *database,
    sqlite3_str *sql,
    const struct mylite_parent_foreign_key_row *stored,
    const struct mylite_parent_foreign_key_row *candidate,
    const char *child_column_name,
    const char *referenced_column_name,
    bool *referenced_key_changed,
    size_t **parent_indexes,
    size_t *parent_index_count,
    bool *out_skip_constraint
);

static int parent_foreign_key_query_child(
    mylite_db *database,
    char *sql,
    const size_t *parent_indexes,
    size_t parent_index_count,
    const struct mylite_parent_foreign_key_row *stored,
    bool *out_has_child
);

static int set_parent_foreign_key_violation(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    const char *constraint_name
);

static int apply_parent_delete_foreign_key_action_constraint(
    mylite_db *database,
    const struct mylite_parent_foreign_key_row *stored,
    sqlite3_stmt *constraint
);

static int apply_parent_update_foreign_key_action_constraint(
    mylite_db *database,
    const struct mylite_parent_foreign_key_row *stored,
    const struct mylite_parent_foreign_key_row *candidate,
    sqlite3_stmt *constraint
);

static int append_parent_delete_action_part(
    mylite_db *database,
    sqlite3_str *sql,
    sqlite3_str *where_sql,
    const struct mylite_parent_foreign_key_row *stored,
    const char *child_column_name,
    const char *referenced_column_name,
    bool set_null,
    size_t **parent_indexes,
    size_t *parent_index_count,
    bool *out_skip_constraint
);

static int append_parent_update_action_part(
    mylite_db *database,
    sqlite3_str *sql,
    sqlite3_str *where_sql,
    const struct mylite_parent_foreign_key_row *stored,
    const struct mylite_parent_foreign_key_row *candidate,
    const char *child_column_name,
    const char *referenced_column_name,
    bool set_null,
    bool *referenced_key_changed,
    size_t **set_parent_indexes,
    size_t *set_parent_index_count,
    size_t **where_parent_indexes,
    size_t *where_parent_index_count,
    bool *out_skip_constraint
);

static int execute_parent_delete_action(
    mylite_db *database,
    const char *sql,
    const size_t *parent_indexes,
    size_t parent_index_count,
    const struct mylite_parent_foreign_key_row *stored
);

static int execute_parent_update_action(
    mylite_db *database,
    const char *sql,
    const size_t *set_parent_indexes,
    size_t set_parent_index_count,
    const size_t *where_parent_indexes,
    size_t where_parent_index_count,
    const struct mylite_parent_foreign_key_row *stored,
    const struct mylite_parent_foreign_key_row *candidate
);

static size_t parent_foreign_key_row_column_index(
    const struct mylite_parent_foreign_key_row *row,
    const char *column_name
);

static bool parent_foreign_key_row_value_is_null(
    const struct mylite_parent_foreign_key_row *row,
    size_t column_index
);

static bool parent_foreign_key_row_values_equal(
    const struct mylite_parent_foreign_key_row *left,
    const struct mylite_parent_foreign_key_row *right,
    size_t column_index
);

static int bind_parent_foreign_key_row_value(
    mylite_db *database,
    sqlite3_stmt *stmt,
    int index,
    const struct mylite_parent_foreign_key_row *row,
    size_t column_index
);

static int bind_parent_update_row_value(
    sqlite3_stmt *stmt,
    int index,
    const struct mylite_expression_value *value
);

static bool expression_values_equal(
    const struct mylite_expression_value *left,
    const struct mylite_expression_value *right
);

static int update_child_foreign_key_constraint_changed(
    mylite_db *database,
    const struct mylite_select_table *table,
    const struct mylite_update_row *stored,
    const struct mylite_update_row *candidate,
    sqlite3_stmt *constraint,
    bool *out_changed
);

static int copy_update_row_to_insert_bound_values(
    mylite_db *database,
    const struct mylite_update_row *row,
    size_t column_count,
    struct mylite_insert_bound_value **out_values
);

static int copy_update_value_to_insert_bound_value(
    mylite_db *database,
    const struct mylite_expression_value *value,
    struct mylite_insert_bound_value *out_value
);

int mylite_dml_validate_insert_child_foreign_keys(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    bool ignore,
    const struct mylite_insert_table *table,
    const struct mylite_insert_bound_value *values,
    bool *out_ignored
) {
    static const char sql[] =
        "SELECT constraint_name, referenced_table_schema, referenced_table_name "
        "FROM __mylite_foreign_key_catalog "
        "WHERE table_schema = ? AND table_name = ? AND ordinal_position = 1 "
        "ORDER BY rowid";
    sqlite3_stmt *constraint = NULL;
    int rc = SQLITE_OK;
    int status = MYLITE_OK;

    if (database == NULL || schema_name == NULL || table_name == NULL || table == NULL ||
        values == NULL || out_ignored == NULL) {
        return MYLITE_MISUSE;
    }

    *out_ignored = false;
    if (!mylite_connection_foreign_key_checks(database)) {
        return MYLITE_OK;
    }

    rc =
        sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &constraint, NULL);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    sqlite3_bind_text(constraint, 1, schema_name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(constraint, 2, table_name, -1, SQLITE_TRANSIENT);

    while ((rc = sqlite3_step(constraint)) == SQLITE_ROW) {
        status = validate_child_foreign_key_constraint(
            database,
            schema_name,
            table_name,
            table,
            values,
            constraint,
            ignore,
            out_ignored
        );
        if (status != MYLITE_OK || *out_ignored) {
            break;
        }
    }

    sqlite3_finalize(constraint);
    if (status != MYLITE_OK || *out_ignored) {
        return status;
    }
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

int mylite_dml_validate_parent_update_foreign_keys(
    mylite_db *database,
    const struct mylite_select_table *table,
    const struct mylite_update_row *stored,
    const struct mylite_update_row *candidate
) {
    struct mylite_parent_foreign_key_row stored_row = {
        .kind = MYLITE_PARENT_FOREIGN_KEY_UPDATE_ROW,
        .select_table = table,
        .update_row = stored,
    };
    struct mylite_parent_foreign_key_row candidate_row = {
        .kind = MYLITE_PARENT_FOREIGN_KEY_UPDATE_ROW,
        .select_table = table,
        .update_row = candidate,
    };

    if (database == NULL || table == NULL || stored == NULL || candidate == NULL ||
        table->schema_name == NULL || table->table_name == NULL) {
        return MYLITE_MISUSE;
    }
    if (!mylite_connection_foreign_key_checks(database)) {
        return MYLITE_OK;
    }
    return validate_parent_foreign_key_references(
        database,
        table->schema_name,
        table->table_name,
        &stored_row,
        &candidate_row,
        MYLITE_PARENT_FOREIGN_KEY_UPDATE
    );
}

int mylite_dml_validate_parent_delete_foreign_keys(
    mylite_db *database,
    const struct mylite_select_table *table,
    const struct mylite_update_row *stored
) {
    struct mylite_parent_foreign_key_row stored_row = {
        .kind = MYLITE_PARENT_FOREIGN_KEY_UPDATE_ROW,
        .select_table = table,
        .update_row = stored,
    };

    if (database == NULL || table == NULL || stored == NULL || table->schema_name == NULL ||
        table->table_name == NULL) {
        return MYLITE_MISUSE;
    }
    if (!mylite_connection_foreign_key_checks(database)) {
        return MYLITE_OK;
    }
    return validate_parent_foreign_key_references(
        database,
        table->schema_name,
        table->table_name,
        &stored_row,
        NULL,
        MYLITE_PARENT_FOREIGN_KEY_DELETE
    );
}

int mylite_dml_apply_parent_delete_foreign_key_actions(
    mylite_db *database,
    const struct mylite_select_table *table,
    const struct mylite_update_row *stored
) {
    static const char sql[] =
        "SELECT constraint_schema, constraint_name, table_schema, table_name, delete_rule "
        "FROM __mylite_foreign_key_catalog "
        "WHERE referenced_table_schema = ? AND referenced_table_name = ? "
        "AND ordinal_position = 1 AND delete_rule IN ('CASCADE', 'SET NULL') "
        "ORDER BY rowid";
    struct mylite_parent_foreign_key_row stored_row = {
        .kind = MYLITE_PARENT_FOREIGN_KEY_UPDATE_ROW,
        .select_table = table,
        .update_row = stored,
    };
    sqlite3_stmt *constraint = NULL;
    int rc = SQLITE_OK;
    int status = MYLITE_OK;

    if (database == NULL || table == NULL || stored == NULL || table->schema_name == NULL ||
        table->table_name == NULL) {
        return MYLITE_MISUSE;
    }
    if (!mylite_connection_foreign_key_checks(database)) {
        return MYLITE_OK;
    }

    rc =
        sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &constraint, NULL);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    sqlite3_bind_text(constraint, 1, table->schema_name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(constraint, 2, table->table_name, -1, SQLITE_TRANSIENT);

    while ((rc = sqlite3_step(constraint)) == SQLITE_ROW) {
        status =
            apply_parent_delete_foreign_key_action_constraint(database, &stored_row, constraint);
        if (status != MYLITE_OK) {
            break;
        }
    }

    sqlite3_finalize(constraint);
    if (status != MYLITE_OK) {
        return status;
    }
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

int mylite_dml_apply_parent_update_foreign_key_actions(
    mylite_db *database,
    const struct mylite_select_table *table,
    const struct mylite_update_row *stored,
    const struct mylite_update_row *candidate
) {
    static const char sql[] =
        "SELECT constraint_schema, constraint_name, table_schema, table_name, update_rule "
        "FROM __mylite_foreign_key_catalog "
        "WHERE referenced_table_schema = ? AND referenced_table_name = ? "
        "AND ordinal_position = 1 AND update_rule IN ('CASCADE', 'SET NULL') "
        "ORDER BY rowid";
    struct mylite_parent_foreign_key_row stored_row = {
        .kind = MYLITE_PARENT_FOREIGN_KEY_UPDATE_ROW,
        .select_table = table,
        .update_row = stored,
    };
    struct mylite_parent_foreign_key_row candidate_row = {
        .kind = MYLITE_PARENT_FOREIGN_KEY_UPDATE_ROW,
        .select_table = table,
        .update_row = candidate,
    };
    sqlite3_stmt *constraint = NULL;
    int rc = SQLITE_OK;
    int status = MYLITE_OK;

    if (database == NULL || table == NULL || stored == NULL || candidate == NULL ||
        table->schema_name == NULL || table->table_name == NULL) {
        return MYLITE_MISUSE;
    }
    if (!mylite_connection_foreign_key_checks(database)) {
        return MYLITE_OK;
    }

    rc =
        sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &constraint, NULL);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    sqlite3_bind_text(constraint, 1, table->schema_name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(constraint, 2, table->table_name, -1, SQLITE_TRANSIENT);

    while ((rc = sqlite3_step(constraint)) == SQLITE_ROW) {
        status = apply_parent_update_foreign_key_action_constraint(
            database,
            &stored_row,
            &candidate_row,
            constraint
        );
        if (status != MYLITE_OK) {
            break;
        }
    }

    sqlite3_finalize(constraint);
    if (status != MYLITE_OK) {
        return status;
    }
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

int mylite_dml_validate_replace_parent_delete_foreign_keys(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    const struct mylite_insert_table *table,
    const struct mylite_insert_bound_value *stored
) {
    struct mylite_parent_foreign_key_row stored_row = {
        .kind = MYLITE_PARENT_FOREIGN_KEY_INSERT_ROW,
        .insert_table = table,
        .insert_values = stored,
    };

    if (database == NULL || schema_name == NULL || table_name == NULL || table == NULL ||
        stored == NULL) {
        return MYLITE_MISUSE;
    }
    if (!mylite_connection_foreign_key_checks(database)) {
        return MYLITE_OK;
    }
    return validate_parent_foreign_key_references(
        database,
        schema_name,
        table_name,
        &stored_row,
        NULL,
        MYLITE_PARENT_FOREIGN_KEY_DELETE
    );
}

int mylite_dml_apply_replace_parent_delete_foreign_key_actions(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    const struct mylite_insert_table *table,
    const struct mylite_insert_bound_value *stored
) {
    static const char sql[] =
        "SELECT constraint_schema, constraint_name, table_schema, table_name, delete_rule "
        "FROM __mylite_foreign_key_catalog "
        "WHERE referenced_table_schema = ? AND referenced_table_name = ? "
        "AND ordinal_position = 1 AND delete_rule IN ('CASCADE', 'SET NULL') "
        "ORDER BY rowid";
    struct mylite_parent_foreign_key_row stored_row = {
        .kind = MYLITE_PARENT_FOREIGN_KEY_INSERT_ROW,
        .insert_table = table,
        .insert_values = stored,
    };
    sqlite3_stmt *constraint = NULL;
    int rc = SQLITE_OK;
    int status = MYLITE_OK;

    if (database == NULL || schema_name == NULL || table_name == NULL || table == NULL ||
        stored == NULL) {
        return MYLITE_MISUSE;
    }
    if (!mylite_connection_foreign_key_checks(database)) {
        return MYLITE_OK;
    }

    rc =
        sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &constraint, NULL);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    sqlite3_bind_text(constraint, 1, schema_name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(constraint, 2, table_name, -1, SQLITE_TRANSIENT);

    while ((rc = sqlite3_step(constraint)) == SQLITE_ROW) {
        status =
            apply_parent_delete_foreign_key_action_constraint(database, &stored_row, constraint);
        if (status != MYLITE_OK) {
            break;
        }
    }

    sqlite3_finalize(constraint);
    if (status != MYLITE_OK) {
        return status;
    }
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

int mylite_dml_validate_update_child_foreign_keys(
    mylite_db *database,
    const struct mylite_select_table *table,
    const struct mylite_insert_table *write_table,
    const struct mylite_update_row *stored,
    const struct mylite_update_row *candidate
) {
    static const char sql[] =
        "SELECT constraint_name, referenced_table_schema, referenced_table_name "
        "FROM __mylite_foreign_key_catalog "
        "WHERE table_schema = ? AND table_name = ? AND ordinal_position = 1 "
        "ORDER BY rowid";
    sqlite3_stmt *constraint = NULL;
    struct mylite_insert_bound_value *candidate_values = NULL;
    bool ignored = false;
    int rc = SQLITE_OK;
    int status = MYLITE_OK;

    if (database == NULL || table == NULL || write_table == NULL || stored == NULL ||
        candidate == NULL || table->schema_name == NULL || table->table_name == NULL) {
        return MYLITE_MISUSE;
    }
    if (!mylite_connection_foreign_key_checks(database)) {
        return MYLITE_OK;
    }

    status = copy_update_row_to_insert_bound_values(
        database,
        candidate,
        write_table->column_count,
        &candidate_values
    );
    if (status != MYLITE_OK) {
        return status;
    }

    rc =
        sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &constraint, NULL);
    if (rc != SQLITE_OK) {
        mylite_dml_insert_bound_values_deinit(candidate_values, write_table->column_count);
        return mylite_diagnostics_set_sqlite_error(database);
    }
    sqlite3_bind_text(constraint, 1, table->schema_name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(constraint, 2, table->table_name, -1, SQLITE_TRANSIENT);

    while ((rc = sqlite3_step(constraint)) == SQLITE_ROW) {
        bool changed = false;

        status = update_child_foreign_key_constraint_changed(
            database,
            table,
            stored,
            candidate,
            constraint,
            &changed
        );
        if (status == MYLITE_OK && changed) {
            status = validate_child_foreign_key_constraint(
                database,
                table->schema_name,
                table->table_name,
                write_table,
                candidate_values,
                constraint,
                false,
                &ignored
            );
        }
        if (status != MYLITE_OK) {
            break;
        }
    }

    sqlite3_finalize(constraint);
    mylite_dml_insert_bound_values_deinit(candidate_values, write_table->column_count);
    if (status != MYLITE_OK) {
        return status;
    }
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static int validate_child_foreign_key_constraint(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    const struct mylite_insert_table *table,
    const struct mylite_insert_bound_value *values,
    sqlite3_stmt *constraint,
    bool ignore,
    bool *out_ignored
) {
    const char *constraint_name = (const char *)sqlite3_column_text(constraint, 0);
    bool has_parent = false;
    int status = child_foreign_key_has_parent(
        database,
        schema_name,
        table_name,
        table,
        values,
        constraint,
        &has_parent
    );

    if (status != MYLITE_OK || has_parent) {
        return status;
    }
    return set_child_foreign_key_violation(
        database,
        schema_name,
        table_name,
        constraint_name,
        ignore,
        out_ignored
    );
}

static int child_foreign_key_has_parent(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    const struct mylite_insert_table *table,
    const struct mylite_insert_bound_value *values,
    sqlite3_stmt *constraint,
    bool *out_has_parent
) {
    static const char part_sql[] =
        "SELECT column_name, referenced_column_name "
        "FROM __mylite_foreign_key_catalog "
        "WHERE constraint_schema = ? AND table_name = ? AND constraint_name = ? "
        "ORDER BY ordinal_position";
    const char *constraint_name = (const char *)sqlite3_column_text(constraint, 0);
    const char *referenced_schema = (const char *)sqlite3_column_text(constraint, 1);
    const char *referenced_table = (const char *)sqlite3_column_text(constraint, 2);
    sqlite3_stmt *parts = NULL;
    sqlite3_str *sql = NULL;
    char *parent_sql = NULL;
    char *parent_physical_name = NULL;
    size_t *child_indexes = NULL;
    size_t child_index_count = 0U;
    bool parent_exists = false;
    bool self_referencing = mylite_ascii_case_equal(schema_name, referenced_schema) &&
                            mylite_ascii_case_equal(table_name, referenced_table);
    bool self_row_matches = self_referencing;
    bool skipped_by_null = false;
    int rc = SQLITE_OK;
    int status = MYLITE_OK;

    *out_has_parent = false;
    status = mylite_catalog_persistent_table_exists(
        database,
        referenced_schema,
        referenced_table,
        &parent_exists
    );
    if (status != MYLITE_OK) {
        return status;
    }
    if (!parent_exists) {
        return MYLITE_OK;
    }

    parent_physical_name = mylite_catalog_physical_table_name(referenced_schema, referenced_table);
    sql = sqlite3_str_new(database->sqlite);
    if (parent_physical_name == NULL || sql == NULL) {
        free(parent_physical_name);
        if (sql != NULL) {
            sqlite3_free(sqlite3_str_finish(sql));
        }
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    sqlite3_str_appendf(sql, "SELECT 1 FROM \"%w\" WHERE ", parent_physical_name);
    free(parent_physical_name);

    rc =
        sqlite3_prepare_v3(database->sqlite, part_sql, -1, SQLITE_PREPARE_PERSISTENT, &parts, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_free(sqlite3_str_finish(sql));
        return mylite_diagnostics_set_sqlite_error(database);
    }
    sqlite3_bind_text(parts, 1, schema_name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(parts, 2, table_name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(parts, 3, constraint_name, -1, SQLITE_TRANSIENT);

    while ((rc = sqlite3_step(parts)) == SQLITE_ROW) {
        const char *column_name = (const char *)sqlite3_column_text(parts, 0);
        const char *referenced_column_name = (const char *)sqlite3_column_text(parts, 1);
        bool skip_constraint = false;

        status = append_child_foreign_key_part(
            database,
            sql,
            table,
            values,
            column_name,
            referenced_column_name,
            self_referencing,
            &self_row_matches,
            &child_indexes,
            &child_index_count,
            &skip_constraint
        );
        if (skip_constraint) {
            skipped_by_null = true;
        }
        if (status != MYLITE_OK || skip_constraint) {
            break;
        }
    }
    sqlite3_finalize(parts);

    if (status == MYLITE_OK && skipped_by_null) {
        sqlite3_free(sqlite3_str_finish(sql));
        free(child_indexes);
        *out_has_parent = true;
        return MYLITE_OK;
    }
    if (status == MYLITE_OK && rc != SQLITE_DONE) {
        status = mylite_diagnostics_set_sqlite_error(database);
    }
    if (status == MYLITE_OK && child_index_count == 0U) {
        (void)mylite_diagnostics_set_error_message(
            database,
            "Foreign key constraint has no column parts"
        );
        status = MYLITE_EXEC_ERROR;
    }
    if (status != MYLITE_OK) {
        sqlite3_free(sqlite3_str_finish(sql));
        free(child_indexes);
        return status;
    }
    if (self_row_matches) {
        sqlite3_free(sqlite3_str_finish(sql));
        free(child_indexes);
        *out_has_parent = true;
        return MYLITE_OK;
    }

    sqlite3_str_appendall(sql, " LIMIT 1");
    parent_sql = sqlite3_str_finish(sql);
    if (parent_sql == NULL) {
        free(child_indexes);
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = child_foreign_key_query_parent(
        database,
        parent_sql,
        child_indexes,
        child_index_count,
        values,
        out_has_parent
    );
    sqlite3_free(parent_sql);
    free(child_indexes);
    return status;
}

static int append_child_foreign_key_part(
    mylite_db *database,
    sqlite3_str *sql,
    const struct mylite_insert_table *table,
    const struct mylite_insert_bound_value *values,
    const char *column_name,
    const char *referenced_column_name,
    bool self_referencing,
    bool *self_row_matches,
    size_t **child_indexes,
    size_t *child_index_count,
    bool *out_skip_constraint
) {
    size_t child_index = mylite_dml_insert_table_column_index(table, column_name);
    size_t *indexes = NULL;

    *out_skip_constraint = false;
    if (child_index == table->column_count) {
        (void)mylite_diagnostics_set_error_message_parts(
            database,
            "Foreign key references unknown column '",
            column_name,
            "'"
        );
        return MYLITE_EXEC_ERROR;
    }
    if (values[child_index].kind == MYLITE_INSERT_BOUND_NULL) {
        *out_skip_constraint = true;
        return MYLITE_OK;
    }

    if (*child_index_count != 0U) {
        sqlite3_str_appendall(sql, " AND ");
    }
    sqlite3_str_appendf(sql, "\"%w\" = ?", referenced_column_name);

    if (self_referencing && *self_row_matches) {
        size_t referenced_index =
            mylite_dml_insert_table_column_index(table, referenced_column_name);

        if (referenced_index == table->column_count ||
            !insert_bound_values_equal(&values[child_index], &values[referenced_index])) {
            *self_row_matches = false;
        }
    }

    indexes = realloc(*child_indexes, (*child_index_count + 1U) * sizeof(**child_indexes));
    if (indexes == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    *child_indexes = indexes;
    (*child_indexes)[(*child_index_count)++] = child_index;
    if (sqlite3_str_errcode(sql) != SQLITE_OK) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    return MYLITE_OK;
}

static int child_foreign_key_query_parent(
    mylite_db *database,
    char *sql,
    const size_t *child_indexes,
    size_t child_index_count,
    const struct mylite_insert_bound_value *values,
    bool *out_has_parent
) {
    sqlite3_stmt *check = NULL;
    int rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &check, NULL);
    int status = MYLITE_OK;

    *out_has_parent = false;
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    for (size_t index = 0U; index < child_index_count; ++index) {
        rc = mylite_dml_bind_insert_bound_value(
            check,
            (int)index + 1,
            &values[child_indexes[index]]
        );
        if (rc != SQLITE_OK) {
            sqlite3_finalize(check);
            return mylite_diagnostics_set_sqlite_error(database);
        }
    }

    rc = sqlite3_step(check);
    if (rc == SQLITE_ROW) {
        *out_has_parent = true;
    } else if (rc != SQLITE_DONE) {
        status = mylite_diagnostics_set_sqlite_error(database);
    }
    sqlite3_finalize(check);
    return status;
}

static int set_child_foreign_key_violation(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    const char *constraint_name,
    bool ignore,
    bool *out_ignored
) {
    char *message = sqlite3_mprintf(
        "Cannot add or update a child row: a foreign key constraint fails "
        "(`%q`.`%q`, CONSTRAINT `%q`)",
        schema_name,
        table_name,
        constraint_name
    );
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    if (ignore) {
        *out_ignored = true;
        status = mylite_diagnostics_append_warning(
            database,
            MYLITE_MYSQL_ER_NO_REFERENCED_ROW_2,
            message
        );
    } else {
        status = mylite_diagnostics_set_error_message(database, message);
        if (status == MYLITE_OK) {
            status = mylite_diagnostics_append_error(
                database,
                MYLITE_MYSQL_ER_NO_REFERENCED_ROW_2,
                message
            );
        }
    }

    sqlite3_free(message);
    if (status != MYLITE_OK) {
        return status;
    }
    return ignore ? MYLITE_OK : MYLITE_EXEC_ERROR;
}

static int validate_parent_foreign_key_references(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    const struct mylite_parent_foreign_key_row *stored,
    const struct mylite_parent_foreign_key_row *candidate,
    enum mylite_parent_foreign_key_action action
) {
    static const char update_sql[] =
        "SELECT constraint_schema, constraint_name, table_schema, table_name "
        "FROM __mylite_foreign_key_catalog "
        "WHERE referenced_table_schema = ? AND referenced_table_name = ? "
        "AND ordinal_position = 1 AND update_rule IN ('RESTRICT', 'NO ACTION', 'SET DEFAULT') "
        "ORDER BY rowid";
    static const char delete_sql[] =
        "SELECT constraint_schema, constraint_name, table_schema, table_name "
        "FROM __mylite_foreign_key_catalog "
        "WHERE referenced_table_schema = ? AND referenced_table_name = ? "
        "AND ordinal_position = 1 AND delete_rule IN ('RESTRICT', 'NO ACTION', 'SET DEFAULT') "
        "ORDER BY rowid";
    sqlite3_stmt *constraint = NULL;
    const char *sql = action == MYLITE_PARENT_FOREIGN_KEY_UPDATE ? update_sql : delete_sql;
    int rc = SQLITE_OK;
    int status = MYLITE_OK;

    rc =
        sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &constraint, NULL);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    sqlite3_bind_text(constraint, 1, schema_name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(constraint, 2, table_name, -1, SQLITE_TRANSIENT);

    while ((rc = sqlite3_step(constraint)) == SQLITE_ROW) {
        status =
            validate_parent_foreign_key_constraint(database, stored, candidate, constraint, action);
        if (status != MYLITE_OK) {
            break;
        }
    }

    sqlite3_finalize(constraint);
    if (status != MYLITE_OK) {
        return status;
    }
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static int validate_parent_foreign_key_constraint(
    mylite_db *database,
    const struct mylite_parent_foreign_key_row *stored,
    const struct mylite_parent_foreign_key_row *candidate,
    sqlite3_stmt *constraint,
    enum mylite_parent_foreign_key_action action
) {
    const char *constraint_name = (const char *)sqlite3_column_text(constraint, 1);
    const char *child_schema = (const char *)sqlite3_column_text(constraint, 2);
    const char *child_table = (const char *)sqlite3_column_text(constraint, 3);
    bool has_child = false;
    int status =
        parent_foreign_key_has_child(database, stored, candidate, constraint, action, &has_child);

    if (status != MYLITE_OK || !has_child) {
        return status;
    }
    return set_parent_foreign_key_violation(database, child_schema, child_table, constraint_name);
}

static int parent_foreign_key_has_child(
    mylite_db *database,
    const struct mylite_parent_foreign_key_row *stored,
    const struct mylite_parent_foreign_key_row *candidate,
    sqlite3_stmt *constraint,
    enum mylite_parent_foreign_key_action action,
    bool *out_has_child
) {
    static const char part_sql[] =
        "SELECT column_name, referenced_column_name "
        "FROM __mylite_foreign_key_catalog "
        "WHERE constraint_schema = ? AND table_schema = ? AND table_name = ? "
        "AND constraint_name = ? "
        "ORDER BY ordinal_position";
    const char *constraint_schema = (const char *)sqlite3_column_text(constraint, 0);
    const char *constraint_name = (const char *)sqlite3_column_text(constraint, 1);
    const char *child_schema = (const char *)sqlite3_column_text(constraint, 2);
    const char *child_table = (const char *)sqlite3_column_text(constraint, 3);
    sqlite3_stmt *parts = NULL;
    sqlite3_str *sql = NULL;
    char *child_sql = NULL;
    char *child_physical_name = NULL;
    size_t *parent_indexes = NULL;
    size_t parent_index_count = 0U;
    bool child_exists = false;
    bool referenced_key_changed = action == MYLITE_PARENT_FOREIGN_KEY_DELETE;
    bool skipped_by_null = false;
    int rc = SQLITE_OK;
    int status = MYLITE_OK;

    *out_has_child = false;
    status =
        mylite_catalog_persistent_table_exists(database, child_schema, child_table, &child_exists);
    if (status != MYLITE_OK) {
        return status;
    }
    if (!child_exists) {
        return MYLITE_OK;
    }

    child_physical_name = mylite_catalog_physical_table_name(child_schema, child_table);
    sql = sqlite3_str_new(database->sqlite);
    if (child_physical_name == NULL || sql == NULL) {
        free(child_physical_name);
        if (sql != NULL) {
            sqlite3_free(sqlite3_str_finish(sql));
        }
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    sqlite3_str_appendf(sql, "SELECT 1 FROM \"%w\" WHERE ", child_physical_name);
    free(child_physical_name);

    rc =
        sqlite3_prepare_v3(database->sqlite, part_sql, -1, SQLITE_PREPARE_PERSISTENT, &parts, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_free(sqlite3_str_finish(sql));
        return mylite_diagnostics_set_sqlite_error(database);
    }
    sqlite3_bind_text(parts, 1, constraint_schema, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(parts, 2, child_schema, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(parts, 3, child_table, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(parts, 4, constraint_name, -1, SQLITE_TRANSIENT);

    while ((rc = sqlite3_step(parts)) == SQLITE_ROW) {
        const char *child_column_name = (const char *)sqlite3_column_text(parts, 0);
        const char *referenced_column_name = (const char *)sqlite3_column_text(parts, 1);
        bool skip_constraint = false;

        status = append_parent_foreign_key_part(
            database,
            sql,
            stored,
            candidate,
            child_column_name,
            referenced_column_name,
            &referenced_key_changed,
            &parent_indexes,
            &parent_index_count,
            &skip_constraint
        );
        if (skip_constraint) {
            skipped_by_null = true;
        }
        if (status != MYLITE_OK || skip_constraint) {
            break;
        }
    }
    sqlite3_finalize(parts);

    if (status == MYLITE_OK && (skipped_by_null || !referenced_key_changed)) {
        sqlite3_free(sqlite3_str_finish(sql));
        free(parent_indexes);
        return MYLITE_OK;
    }
    if (status == MYLITE_OK && rc != SQLITE_DONE) {
        status = mylite_diagnostics_set_sqlite_error(database);
    }
    if (status == MYLITE_OK && parent_index_count == 0U) {
        (void)mylite_diagnostics_set_error_message(
            database,
            "Foreign key constraint has no column parts"
        );
        status = MYLITE_EXEC_ERROR;
    }
    if (status != MYLITE_OK) {
        sqlite3_free(sqlite3_str_finish(sql));
        free(parent_indexes);
        return status;
    }

    sqlite3_str_appendall(sql, " LIMIT 1");
    child_sql = sqlite3_str_finish(sql);
    if (child_sql == NULL) {
        free(parent_indexes);
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = parent_foreign_key_query_child(
        database,
        child_sql,
        parent_indexes,
        parent_index_count,
        stored,
        out_has_child
    );
    sqlite3_free(child_sql);
    free(parent_indexes);
    return status;
}

static int append_parent_foreign_key_part(
    mylite_db *database,
    sqlite3_str *sql,
    const struct mylite_parent_foreign_key_row *stored,
    const struct mylite_parent_foreign_key_row *candidate,
    const char *child_column_name,
    const char *referenced_column_name,
    bool *referenced_key_changed,
    size_t **parent_indexes,
    size_t *parent_index_count,
    bool *out_skip_constraint
) {
    size_t parent_index = parent_foreign_key_row_column_index(stored, referenced_column_name);
    size_t *indexes = NULL;

    *out_skip_constraint = false;
    if (parent_index == SIZE_MAX) {
        (void)mylite_diagnostics_set_error_message_parts(
            database,
            "Foreign key references unknown column '",
            referenced_column_name,
            "'"
        );
        return MYLITE_EXEC_ERROR;
    }
    if (parent_foreign_key_row_value_is_null(stored, parent_index)) {
        *out_skip_constraint = true;
        return MYLITE_OK;
    }
    if (candidate != NULL &&
        !parent_foreign_key_row_values_equal(stored, candidate, parent_index)) {
        *referenced_key_changed = true;
    }

    if (*parent_index_count != 0U) {
        sqlite3_str_appendall(sql, " AND ");
    }
    sqlite3_str_appendf(sql, "\"%w\" = ?", child_column_name);

    indexes = realloc(*parent_indexes, (*parent_index_count + 1U) * sizeof(**parent_indexes));
    if (indexes == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    *parent_indexes = indexes;
    (*parent_indexes)[(*parent_index_count)++] = parent_index;
    if (sqlite3_str_errcode(sql) != SQLITE_OK) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    return MYLITE_OK;
}

static int parent_foreign_key_query_child(
    mylite_db *database,
    char *sql,
    const size_t *parent_indexes,
    size_t parent_index_count,
    const struct mylite_parent_foreign_key_row *stored,
    bool *out_has_child
) {
    sqlite3_stmt *check = NULL;
    int rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &check, NULL);
    int status = MYLITE_OK;

    *out_has_child = false;
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    for (size_t index = 0U; index < parent_index_count; ++index) {
        status = bind_parent_foreign_key_row_value(
            database,
            check,
            (int)index + 1,
            stored,
            parent_indexes[index]
        );
        if (status != MYLITE_OK) {
            sqlite3_finalize(check);
            return status;
        }
    }

    rc = sqlite3_step(check);
    if (rc == SQLITE_ROW) {
        *out_has_child = true;
    } else if (rc != SQLITE_DONE) {
        status = mylite_diagnostics_set_sqlite_error(database);
    }
    sqlite3_finalize(check);
    return status;
}

static int set_parent_foreign_key_violation(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    const char *constraint_name
) {
    char *message = sqlite3_mprintf(
        "Cannot delete or update a parent row: a foreign key constraint fails "
        "(`%q`.`%q`, CONSTRAINT `%q`)",
        schema_name,
        table_name,
        constraint_name
    );
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = mylite_diagnostics_set_error_message(database, message);
    if (status == MYLITE_OK) {
        status =
            mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_ROW_IS_REFERENCED_2, message);
    }
    sqlite3_free(message);
    return status == MYLITE_OK ? MYLITE_EXEC_ERROR : status;
}

static int apply_parent_delete_foreign_key_action_constraint(
    mylite_db *database,
    const struct mylite_parent_foreign_key_row *stored,
    sqlite3_stmt *constraint
) {
    static const char part_sql[] =
        "SELECT column_name, referenced_column_name "
        "FROM __mylite_foreign_key_catalog "
        "WHERE constraint_schema = ? AND table_schema = ? AND table_name = ? "
        "AND constraint_name = ? "
        "ORDER BY ordinal_position";
    const char *constraint_schema = (const char *)sqlite3_column_text(constraint, 0);
    const char *constraint_name = (const char *)sqlite3_column_text(constraint, 1);
    const char *child_schema = (const char *)sqlite3_column_text(constraint, 2);
    const char *child_table = (const char *)sqlite3_column_text(constraint, 3);
    const char *delete_rule = (const char *)sqlite3_column_text(constraint, 4);
    sqlite3_stmt *parts = NULL;
    sqlite3_str *sql = NULL;
    sqlite3_str *where_sql = NULL;
    char *action_sql = NULL;
    char *where_clause = NULL;
    char *child_physical_name = NULL;
    size_t *parent_indexes = NULL;
    size_t parent_index_count = 0U;
    bool child_exists = false;
    bool set_null = mylite_ascii_case_equal(delete_rule, "SET NULL");
    bool skipped_by_null = false;
    int rc = SQLITE_OK;
    int status = MYLITE_OK;

    status =
        mylite_catalog_persistent_table_exists(database, child_schema, child_table, &child_exists);
    if (status != MYLITE_OK) {
        return status;
    }
    if (!child_exists) {
        return MYLITE_OK;
    }

    child_physical_name = mylite_catalog_physical_table_name(child_schema, child_table);
    sql = sqlite3_str_new(database->sqlite);
    if (set_null) {
        where_sql = sqlite3_str_new(database->sqlite);
    }
    if (child_physical_name == NULL || sql == NULL || (set_null && where_sql == NULL)) {
        free(child_physical_name);
        if (sql != NULL) {
            sqlite3_free(sqlite3_str_finish(sql));
        }
        if (where_sql != NULL) {
            sqlite3_free(sqlite3_str_finish(where_sql));
        }
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    if (set_null) {
        sqlite3_str_appendf(sql, "UPDATE \"%w\" SET ", child_physical_name);
    } else {
        sqlite3_str_appendf(sql, "DELETE FROM \"%w\" WHERE ", child_physical_name);
    }

    rc =
        sqlite3_prepare_v3(database->sqlite, part_sql, -1, SQLITE_PREPARE_PERSISTENT, &parts, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_free(sqlite3_str_finish(sql));
        if (where_sql != NULL) {
            sqlite3_free(sqlite3_str_finish(where_sql));
        }
        free(child_physical_name);
        return mylite_diagnostics_set_sqlite_error(database);
    }
    sqlite3_bind_text(parts, 1, constraint_schema, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(parts, 2, child_schema, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(parts, 3, child_table, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(parts, 4, constraint_name, -1, SQLITE_TRANSIENT);

    while ((rc = sqlite3_step(parts)) == SQLITE_ROW) {
        const char *child_column_name = (const char *)sqlite3_column_text(parts, 0);
        const char *referenced_column_name = (const char *)sqlite3_column_text(parts, 1);
        bool skip_constraint = false;

        status = append_parent_delete_action_part(
            database,
            sql,
            where_sql,
            stored,
            child_column_name,
            referenced_column_name,
            set_null,
            &parent_indexes,
            &parent_index_count,
            &skip_constraint
        );
        if (skip_constraint) {
            skipped_by_null = true;
        }
        if (status != MYLITE_OK || skip_constraint) {
            break;
        }
    }
    sqlite3_finalize(parts);

    if (status == MYLITE_OK && skipped_by_null) {
        sqlite3_free(sqlite3_str_finish(sql));
        if (where_sql != NULL) {
            sqlite3_free(sqlite3_str_finish(where_sql));
        }
        free(child_physical_name);
        free(parent_indexes);
        return MYLITE_OK;
    }
    if (status == MYLITE_OK && rc != SQLITE_DONE) {
        status = mylite_diagnostics_set_sqlite_error(database);
    }
    if (status == MYLITE_OK && parent_index_count == 0U) {
        (void)mylite_diagnostics_set_error_message(
            database,
            "Foreign key constraint has no column parts"
        );
        status = MYLITE_EXEC_ERROR;
    }
    if (status != MYLITE_OK) {
        sqlite3_free(sqlite3_str_finish(sql));
        if (where_sql != NULL) {
            sqlite3_free(sqlite3_str_finish(where_sql));
        }
        free(child_physical_name);
        free(parent_indexes);
        return status;
    }

    if (set_null) {
        where_clause = sqlite3_str_finish(where_sql);
        where_sql = NULL;
        if (where_clause == NULL) {
            sqlite3_free(sqlite3_str_finish(sql));
            free(child_physical_name);
            free(parent_indexes);
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
            return MYLITE_NOMEM;
        }
        sqlite3_str_appendall(sql, " WHERE ");
        sqlite3_str_appendall(sql, where_clause);
        sqlite3_free(where_clause);
    }
    action_sql = sqlite3_str_finish(sql);
    if (action_sql == NULL) {
        free(child_physical_name);
        free(parent_indexes);
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = execute_parent_delete_action(
        database,
        action_sql,
        parent_indexes,
        parent_index_count,
        stored
    );
    if (status == MYLITE_OK) {
        status = mylite_catalog_refresh_table_statistics(
            database,
            child_schema,
            child_table,
            child_physical_name
        );
    }
    sqlite3_free(action_sql);
    free(child_physical_name);
    free(parent_indexes);
    return status;
}

static int apply_parent_update_foreign_key_action_constraint(
    mylite_db *database,
    const struct mylite_parent_foreign_key_row *stored,
    const struct mylite_parent_foreign_key_row *candidate,
    sqlite3_stmt *constraint
) {
    static const char part_sql[] =
        "SELECT column_name, referenced_column_name "
        "FROM __mylite_foreign_key_catalog "
        "WHERE constraint_schema = ? AND table_schema = ? AND table_name = ? "
        "AND constraint_name = ? "
        "ORDER BY ordinal_position";
    const char *constraint_schema = (const char *)sqlite3_column_text(constraint, 0);
    const char *constraint_name = (const char *)sqlite3_column_text(constraint, 1);
    const char *child_schema = (const char *)sqlite3_column_text(constraint, 2);
    const char *child_table = (const char *)sqlite3_column_text(constraint, 3);
    const char *update_rule = (const char *)sqlite3_column_text(constraint, 4);
    sqlite3_stmt *parts = NULL;
    sqlite3_str *sql = NULL;
    sqlite3_str *where_sql = NULL;
    char *action_sql = NULL;
    char *where_clause = NULL;
    char *child_physical_name = NULL;
    size_t *set_parent_indexes = NULL;
    size_t *where_parent_indexes = NULL;
    size_t set_parent_index_count = 0U;
    size_t where_parent_index_count = 0U;
    bool child_exists = false;
    bool set_null = mylite_ascii_case_equal(update_rule, "SET NULL");
    bool self_referencing =
        stored->kind == MYLITE_PARENT_FOREIGN_KEY_UPDATE_ROW &&
        mylite_ascii_case_equal(stored->select_table->schema_name, child_schema) &&
        mylite_ascii_case_equal(stored->select_table->table_name, child_table);
    bool referenced_key_changed = false;
    bool skipped_by_null = false;
    int rc = SQLITE_OK;
    int status = MYLITE_OK;

    if (self_referencing) {
        return validate_parent_foreign_key_constraint(
            database,
            stored,
            candidate,
            constraint,
            MYLITE_PARENT_FOREIGN_KEY_UPDATE
        );
    }

    status =
        mylite_catalog_persistent_table_exists(database, child_schema, child_table, &child_exists);
    if (status != MYLITE_OK) {
        return status;
    }
    if (!child_exists) {
        return MYLITE_OK;
    }

    child_physical_name = mylite_catalog_physical_table_name(child_schema, child_table);
    sql = sqlite3_str_new(database->sqlite);
    where_sql = sqlite3_str_new(database->sqlite);
    if (child_physical_name == NULL || sql == NULL || where_sql == NULL) {
        free(child_physical_name);
        if (sql != NULL) {
            sqlite3_free(sqlite3_str_finish(sql));
        }
        if (where_sql != NULL) {
            sqlite3_free(sqlite3_str_finish(where_sql));
        }
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    sqlite3_str_appendf(sql, "UPDATE \"%w\" SET ", child_physical_name);

    rc =
        sqlite3_prepare_v3(database->sqlite, part_sql, -1, SQLITE_PREPARE_PERSISTENT, &parts, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_free(sqlite3_str_finish(sql));
        sqlite3_free(sqlite3_str_finish(where_sql));
        free(child_physical_name);
        return mylite_diagnostics_set_sqlite_error(database);
    }
    sqlite3_bind_text(parts, 1, constraint_schema, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(parts, 2, child_schema, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(parts, 3, child_table, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(parts, 4, constraint_name, -1, SQLITE_TRANSIENT);

    while ((rc = sqlite3_step(parts)) == SQLITE_ROW) {
        const char *child_column_name = (const char *)sqlite3_column_text(parts, 0);
        const char *referenced_column_name = (const char *)sqlite3_column_text(parts, 1);
        bool skip_constraint = false;

        status = append_parent_update_action_part(
            database,
            sql,
            where_sql,
            stored,
            candidate,
            child_column_name,
            referenced_column_name,
            set_null,
            &referenced_key_changed,
            &set_parent_indexes,
            &set_parent_index_count,
            &where_parent_indexes,
            &where_parent_index_count,
            &skip_constraint
        );
        if (skip_constraint) {
            skipped_by_null = true;
        }
        if (status != MYLITE_OK || skip_constraint) {
            break;
        }
    }
    sqlite3_finalize(parts);

    if (status == MYLITE_OK && (skipped_by_null || !referenced_key_changed)) {
        sqlite3_free(sqlite3_str_finish(sql));
        sqlite3_free(sqlite3_str_finish(where_sql));
        free(child_physical_name);
        free(set_parent_indexes);
        free(where_parent_indexes);
        return MYLITE_OK;
    }
    if (status == MYLITE_OK && rc != SQLITE_DONE) {
        status = mylite_diagnostics_set_sqlite_error(database);
    }
    if (status == MYLITE_OK && where_parent_index_count == 0U) {
        (void)mylite_diagnostics_set_error_message(
            database,
            "Foreign key constraint has no column parts"
        );
        status = MYLITE_EXEC_ERROR;
    }
    if (status != MYLITE_OK) {
        sqlite3_free(sqlite3_str_finish(sql));
        sqlite3_free(sqlite3_str_finish(where_sql));
        free(child_physical_name);
        free(set_parent_indexes);
        free(where_parent_indexes);
        return status;
    }

    where_clause = sqlite3_str_finish(where_sql);
    where_sql = NULL;
    if (where_clause == NULL) {
        sqlite3_free(sqlite3_str_finish(sql));
        free(child_physical_name);
        free(set_parent_indexes);
        free(where_parent_indexes);
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    sqlite3_str_appendall(sql, " WHERE ");
    sqlite3_str_appendall(sql, where_clause);
    sqlite3_free(where_clause);

    action_sql = sqlite3_str_finish(sql);
    if (action_sql == NULL) {
        free(child_physical_name);
        free(set_parent_indexes);
        free(where_parent_indexes);
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = execute_parent_update_action(
        database,
        action_sql,
        set_parent_indexes,
        set_parent_index_count,
        where_parent_indexes,
        where_parent_index_count,
        stored,
        candidate
    );
    sqlite3_free(action_sql);
    free(child_physical_name);
    free(set_parent_indexes);
    free(where_parent_indexes);
    return status;
}

static int append_parent_delete_action_part(
    mylite_db *database,
    sqlite3_str *sql,
    sqlite3_str *where_sql,
    const struct mylite_parent_foreign_key_row *stored,
    const char *child_column_name,
    const char *referenced_column_name,
    bool set_null,
    size_t **parent_indexes,
    size_t *parent_index_count,
    bool *out_skip_constraint
) {
    size_t parent_index = parent_foreign_key_row_column_index(stored, referenced_column_name);
    size_t *indexes = NULL;

    *out_skip_constraint = false;
    if (parent_index == SIZE_MAX) {
        (void)mylite_diagnostics_set_error_message_parts(
            database,
            "Foreign key references unknown column '",
            referenced_column_name,
            "'"
        );
        return MYLITE_EXEC_ERROR;
    }
    if (parent_foreign_key_row_value_is_null(stored, parent_index)) {
        *out_skip_constraint = true;
        return MYLITE_OK;
    }

    if (*parent_index_count != 0U) {
        if (set_null) {
            sqlite3_str_appendall(sql, ", ");
            sqlite3_str_appendall(where_sql, " AND ");
        } else {
            sqlite3_str_appendall(sql, " AND ");
        }
    }
    if (set_null) {
        sqlite3_str_appendf(sql, "\"%w\" = NULL", child_column_name);
    } else {
        sqlite3_str_appendf(sql, "\"%w\" = ?", child_column_name);
    }
    if (set_null) {
        sqlite3_str_appendf(where_sql, "\"%w\" = ?", child_column_name);
    }

    indexes = realloc(*parent_indexes, (*parent_index_count + 1U) * sizeof(**parent_indexes));
    if (indexes == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    *parent_indexes = indexes;
    (*parent_indexes)[(*parent_index_count)++] = parent_index;
    if (sqlite3_str_errcode(sql) != SQLITE_OK) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    if (where_sql != NULL && sqlite3_str_errcode(where_sql) != SQLITE_OK) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    return MYLITE_OK;
}

static int append_parent_update_action_part(
    mylite_db *database,
    sqlite3_str *sql,
    sqlite3_str *where_sql,
    const struct mylite_parent_foreign_key_row *stored,
    const struct mylite_parent_foreign_key_row *candidate,
    const char *child_column_name,
    const char *referenced_column_name,
    bool set_null,
    bool *referenced_key_changed,
    size_t **set_parent_indexes,
    size_t *set_parent_index_count,
    size_t **where_parent_indexes,
    size_t *where_parent_index_count,
    bool *out_skip_constraint
) {
    size_t parent_index = parent_foreign_key_row_column_index(stored, referenced_column_name);
    size_t *indexes = NULL;

    *out_skip_constraint = false;
    if (parent_index == SIZE_MAX) {
        (void)mylite_diagnostics_set_error_message_parts(
            database,
            "Foreign key references unknown column '",
            referenced_column_name,
            "'"
        );
        return MYLITE_EXEC_ERROR;
    }
    if (parent_foreign_key_row_value_is_null(stored, parent_index)) {
        *out_skip_constraint = true;
        return MYLITE_OK;
    }
    if (!parent_foreign_key_row_values_equal(stored, candidate, parent_index)) {
        *referenced_key_changed = true;
    }

    if (*where_parent_index_count != 0U) {
        sqlite3_str_appendall(sql, ", ");
        sqlite3_str_appendall(where_sql, " AND ");
    }
    if (set_null) {
        sqlite3_str_appendf(sql, "\"%w\" = NULL", child_column_name);
    } else {
        sqlite3_str_appendf(sql, "\"%w\" = ?", child_column_name);
        indexes = realloc(
            *set_parent_indexes,
            (*set_parent_index_count + 1U) * sizeof(**set_parent_indexes)
        );
        if (indexes == NULL) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
            return MYLITE_NOMEM;
        }
        *set_parent_indexes = indexes;
        (*set_parent_indexes)[(*set_parent_index_count)++] = parent_index;
    }
    sqlite3_str_appendf(where_sql, "\"%w\" = ?", child_column_name);

    indexes = realloc(
        *where_parent_indexes,
        (*where_parent_index_count + 1U) * sizeof(**where_parent_indexes)
    );
    if (indexes == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    *where_parent_indexes = indexes;
    (*where_parent_indexes)[(*where_parent_index_count)++] = parent_index;
    if (sqlite3_str_errcode(sql) != SQLITE_OK || sqlite3_str_errcode(where_sql) != SQLITE_OK) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    return MYLITE_OK;
}

static int execute_parent_delete_action(
    mylite_db *database,
    const char *sql,
    const size_t *parent_indexes,
    size_t parent_index_count,
    const struct mylite_parent_foreign_key_row *stored
) {
    sqlite3_stmt *action = NULL;
    int rc =
        sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &action, NULL);
    int status = MYLITE_OK;

    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    for (size_t index = 0U; index < parent_index_count; ++index) {
        status = bind_parent_foreign_key_row_value(
            database,
            action,
            (int)index + 1,
            stored,
            parent_indexes[index]
        );
        if (status != MYLITE_OK) {
            sqlite3_finalize(action);
            return status;
        }
    }

    rc = sqlite3_step(action);
    if (rc != SQLITE_DONE) {
        status = mylite_diagnostics_set_sqlite_error(database);
    }
    sqlite3_finalize(action);
    return status;
}

static int execute_parent_update_action(
    mylite_db *database,
    const char *sql,
    const size_t *set_parent_indexes,
    size_t set_parent_index_count,
    const size_t *where_parent_indexes,
    size_t where_parent_index_count,
    const struct mylite_parent_foreign_key_row *stored,
    const struct mylite_parent_foreign_key_row *candidate
) {
    sqlite3_stmt *action = NULL;
    int rc =
        sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &action, NULL);
    int status = MYLITE_OK;
    int parameter = 1;

    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    for (size_t index = 0U; index < set_parent_index_count; ++index) {
        status = bind_parent_foreign_key_row_value(
            database,
            action,
            parameter++,
            candidate,
            set_parent_indexes[index]
        );
        if (status != MYLITE_OK) {
            sqlite3_finalize(action);
            return status;
        }
    }
    for (size_t index = 0U; index < where_parent_index_count; ++index) {
        status = bind_parent_foreign_key_row_value(
            database,
            action,
            parameter++,
            stored,
            where_parent_indexes[index]
        );
        if (status != MYLITE_OK) {
            sqlite3_finalize(action);
            return status;
        }
    }

    rc = sqlite3_step(action);
    if (rc != SQLITE_DONE) {
        status = mylite_diagnostics_set_sqlite_error(database);
    }
    sqlite3_finalize(action);
    return status;
}

static size_t parent_foreign_key_row_column_index(
    const struct mylite_parent_foreign_key_row *row,
    const char *column_name
) {
    size_t column_count = 0U;

    if (row->kind == MYLITE_PARENT_FOREIGN_KEY_UPDATE_ROW) {
        column_count = row->select_table->column_count;
        for (size_t index = 0U; index < column_count; ++index) {
            if (mylite_ascii_case_equal(row->select_table->columns[index].name, column_name)) {
                return index;
            }
        }
        return SIZE_MAX;
    }

    column_count = row->insert_table->column_count;
    for (size_t index = 0U; index < column_count; ++index) {
        if (mylite_ascii_case_equal(row->insert_table->columns[index].name, column_name)) {
            return index;
        }
    }
    return SIZE_MAX;
}

static bool parent_foreign_key_row_value_is_null(
    const struct mylite_parent_foreign_key_row *row,
    size_t column_index
) {
    if (row->kind == MYLITE_PARENT_FOREIGN_KEY_UPDATE_ROW) {
        return row->update_row->values[column_index].kind == MYLITE_EXPRESSION_VALUE_NULL;
    }
    return row->insert_values[column_index].kind == MYLITE_INSERT_BOUND_NULL;
}

static bool parent_foreign_key_row_values_equal(
    const struct mylite_parent_foreign_key_row *left,
    const struct mylite_parent_foreign_key_row *right,
    size_t column_index
) {
    if (left->kind != right->kind) {
        return false;
    }
    if (left->kind == MYLITE_PARENT_FOREIGN_KEY_UPDATE_ROW) {
        return expression_values_equal(
            &left->update_row->values[column_index],
            &right->update_row->values[column_index]
        );
    }
    return insert_bound_values_equal(
        &left->insert_values[column_index],
        &right->insert_values[column_index]
    );
}

static int update_child_foreign_key_constraint_changed(
    mylite_db *database,
    const struct mylite_select_table *table,
    const struct mylite_update_row *stored,
    const struct mylite_update_row *candidate,
    sqlite3_stmt *constraint,
    bool *out_changed
) {
    static const char part_sql[] =
        "SELECT column_name "
        "FROM __mylite_foreign_key_catalog "
        "WHERE constraint_schema = ? AND table_name = ? AND constraint_name = ? "
        "ORDER BY ordinal_position";
    struct mylite_parent_foreign_key_row stored_row = {
        .kind = MYLITE_PARENT_FOREIGN_KEY_UPDATE_ROW,
        .select_table = table,
        .update_row = stored,
    };
    const char *constraint_name = (const char *)sqlite3_column_text(constraint, 0);
    sqlite3_stmt *parts = NULL;
    int rc = SQLITE_OK;
    int status = MYLITE_OK;

    *out_changed = false;
    rc =
        sqlite3_prepare_v3(database->sqlite, part_sql, -1, SQLITE_PREPARE_PERSISTENT, &parts, NULL);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    sqlite3_bind_text(parts, 1, table->schema_name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(parts, 2, table->table_name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(parts, 3, constraint_name, -1, SQLITE_TRANSIENT);

    while ((rc = sqlite3_step(parts)) == SQLITE_ROW) {
        const char *column_name = (const char *)sqlite3_column_text(parts, 0);
        size_t column_index = parent_foreign_key_row_column_index(&stored_row, column_name);

        if (column_index == SIZE_MAX || column_index >= candidate->value_count) {
            (void)mylite_diagnostics_set_error_message_parts(
                database,
                "Foreign key references unknown column '",
                column_name,
                "'"
            );
            status = MYLITE_EXEC_ERROR;
            break;
        }
        if (!expression_values_equal(
                &stored->values[column_index],
                &candidate->values[column_index]
            )) {
            *out_changed = true;
            break;
        }
    }

    sqlite3_finalize(parts);
    if (status != MYLITE_OK) {
        return status;
    }
    return rc == SQLITE_DONE || *out_changed ? MYLITE_OK
                                             : mylite_diagnostics_set_sqlite_error(database);
}

static int copy_update_row_to_insert_bound_values(
    mylite_db *database,
    const struct mylite_update_row *row,
    size_t column_count,
    struct mylite_insert_bound_value **out_values
) {
    struct mylite_insert_bound_value *values = NULL;

    *out_values = NULL;
    if (row->value_count < column_count) {
        (void)mylite_diagnostics_set_error_message(database, "UPDATE row has too few columns");
        return MYLITE_EXEC_ERROR;
    }
    values = calloc(column_count, sizeof(*values));
    if (values == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    for (size_t index = 0U; index < column_count; ++index) {
        int status =
            copy_update_value_to_insert_bound_value(database, &row->values[index], &values[index]);

        if (status != MYLITE_OK) {
            mylite_dml_insert_bound_values_deinit(values, column_count);
            return status;
        }
    }
    *out_values = values;
    return MYLITE_OK;
}

static int copy_update_value_to_insert_bound_value(
    mylite_db *database,
    const struct mylite_expression_value *value,
    struct mylite_insert_bound_value *out_value
) {
    switch (value->kind) {
    case MYLITE_EXPRESSION_VALUE_NULL:
        *out_value = (struct mylite_insert_bound_value){.kind = MYLITE_INSERT_BOUND_NULL};
        return MYLITE_OK;
    case MYLITE_EXPRESSION_VALUE_INT64:
        *out_value = (struct mylite_insert_bound_value){
            .kind = MYLITE_INSERT_BOUND_INTEGER,
            .integer_value = value->int64_value,
        };
        return MYLITE_OK;
    case MYLITE_EXPRESSION_VALUE_UINT64:
        if (value->uint64_value > (uint64_t)INT64_MAX) {
            (void)mylite_diagnostics_set_error_message(database, "out of range value");
            return MYLITE_EXEC_ERROR;
        }
        *out_value = (struct mylite_insert_bound_value){
            .kind = MYLITE_INSERT_BOUND_INTEGER,
            .integer_value = (int64_t)value->uint64_value,
        };
        return MYLITE_OK;
    case MYLITE_EXPRESSION_VALUE_REAL:
        *out_value = (struct mylite_insert_bound_value){
            .kind = MYLITE_INSERT_BOUND_REAL,
            .real_value = value->real_value,
        };
        return MYLITE_OK;
    case MYLITE_EXPRESSION_VALUE_TEXT:
        *out_value = (struct mylite_insert_bound_value){
            .kind = MYLITE_INSERT_BOUND_TEXT,
            .text_length = value->text_length,
            .text_value = mylite_copy_span_text(value->text_value, value->text_length),
        };
        if (out_value->text_value == NULL) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
            return MYLITE_NOMEM;
        }
        return MYLITE_OK;
    }
    return MYLITE_MISUSE;
}

static int bind_parent_foreign_key_row_value(
    mylite_db *database,
    sqlite3_stmt *stmt,
    int index,
    const struct mylite_parent_foreign_key_row *row,
    size_t column_index
) {
    int rc = SQLITE_OK;

    if (row->kind == MYLITE_PARENT_FOREIGN_KEY_UPDATE_ROW) {
        rc = bind_parent_update_row_value(stmt, index, &row->update_row->values[column_index]);
    } else {
        rc = mylite_dml_bind_insert_bound_value(stmt, index, &row->insert_values[column_index]);
    }
    return rc == SQLITE_OK ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static int bind_parent_update_row_value(
    sqlite3_stmt *stmt,
    int index,
    const struct mylite_expression_value *value
) {
    if (stmt == NULL || value == NULL) {
        return SQLITE_MISUSE;
    }

    switch (value->kind) {
    case MYLITE_EXPRESSION_VALUE_NULL:
        return sqlite3_bind_null(stmt, index);
    case MYLITE_EXPRESSION_VALUE_INT64:
        return sqlite3_bind_int64(stmt, index, value->int64_value);
    case MYLITE_EXPRESSION_VALUE_UINT64:
        if (value->uint64_value > (uint64_t)INT64_MAX) {
            return SQLITE_RANGE;
        }
        return sqlite3_bind_int64(stmt, index, (sqlite3_int64)value->uint64_value);
    case MYLITE_EXPRESSION_VALUE_REAL:
        return sqlite3_bind_double(stmt, index, value->real_value);
    case MYLITE_EXPRESSION_VALUE_TEXT:
        return sqlite3_bind_text(
            stmt,
            index,
            value->text_value,
            (int)value->text_length,
            SQLITE_TRANSIENT
        );
    }
    return SQLITE_MISUSE;
}

static bool expression_values_equal(
    const struct mylite_expression_value *left,
    const struct mylite_expression_value *right
) {
    if (left->kind != right->kind) {
        return false;
    }
    switch (left->kind) {
    case MYLITE_EXPRESSION_VALUE_NULL:
        return true;
    case MYLITE_EXPRESSION_VALUE_INT64:
        return left->int64_value == right->int64_value;
    case MYLITE_EXPRESSION_VALUE_UINT64:
        return left->uint64_value == right->uint64_value;
    case MYLITE_EXPRESSION_VALUE_REAL:
        return left->real_value == right->real_value;
    case MYLITE_EXPRESSION_VALUE_TEXT:
        if (left->text_value == NULL || right->text_value == NULL) {
            return left->text_value == right->text_value;
        }
        return left->text_length == right->text_length &&
               (left->text_length == 0U ||
                memcmp(left->text_value, right->text_value, left->text_length) == 0);
    }
    return false;
}

static bool insert_bound_values_equal(
    const struct mylite_insert_bound_value *left,
    const struct mylite_insert_bound_value *right
) {
    if (left->kind != right->kind) {
        return false;
    }
    switch (left->kind) {
    case MYLITE_INSERT_BOUND_NULL:
        return true;
    case MYLITE_INSERT_BOUND_INTEGER:
        return left->integer_value == right->integer_value;
    case MYLITE_INSERT_BOUND_REAL:
        return left->real_value == right->real_value;
    case MYLITE_INSERT_BOUND_TEXT:
        return left->text_length == right->text_length &&
               (left->text_length == 0U ||
                memcmp(left->text_value, right->text_value, left->text_length) == 0);
    }
    return false;
}
