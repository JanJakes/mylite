#include "mylite_dml.h"

#include "mylite_catalog.h"
#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_dml_insert_column_reference.h"
#include "mylite_dml_insert_sqlite_bind.h"
#include "mylite_error_codes.h"
#include "mylite_span.h"
#include "sqlite3.h"

#include <stdlib.h>
#include <string.h>

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
