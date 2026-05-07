#include "mylite_table_ddl_alter.h"

#include "mylite_diagnostics.h"
#include "mylite_dml.h"
#include "mylite_error_codes.h"
#include "mylite_runtime.h"
#include "mylite_span.h"
#include "sqlite3.h"

#include <stdlib.h>

struct mylite_alter_table_foreign_key_column_shape {
    const char *name;
    const char *column_type;
    const char *character_set_name;
    const char *collation_name;
    char *owned_name;
    char *owned_column_type;
    char *owned_character_set_name;
    char *owned_collation_name;
};

static int validate_alter_table_columns(
    mylite_db *database,
    struct mylite_alter_table_model *model,
    bool *out_has_visible_column,
    size_t *out_auto_increment_count
);

static int validate_alter_table_source_not_null(
    mylite_db *database,
    const struct mylite_alter_table_model *model,
    const struct mylite_alter_table_column *column
);

static int validate_alter_table_auto_increment_shape(
    mylite_db *database,
    struct mylite_alter_table_model *model,
    size_t auto_increment_count
);

static const struct mylite_alter_table_column *find_alter_table_auto_increment_column(
    const struct mylite_alter_table_model *model
);

static bool alter_table_auto_increment_column_is_indexed(
    const struct mylite_alter_table_model *model,
    const struct mylite_alter_table_column *auto_column
);

static int validate_alter_table_foreign_key_columns(
    mylite_db *database,
    const struct mylite_alter_table_model *model
);

static int validate_alter_table_child_foreign_key_columns(
    mylite_db *database,
    const struct mylite_alter_table_model *model
);

static int validate_alter_table_parent_foreign_key_columns(
    mylite_db *database,
    const struct mylite_alter_table_model *model
);

static const struct mylite_alter_table_column *find_alter_table_column_by_source_name(
    const struct mylite_alter_table_model *model,
    const char *source_name
);

static int load_alter_table_catalog_column_shape(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    const char *column_name,
    struct mylite_alter_table_foreign_key_column_shape *shape,
    bool *out_found
);

static int copy_shape_column(sqlite3_stmt *stmt, int column, char **out_text);

static int copy_nullable_shape_column(sqlite3_stmt *stmt, int column, char **out_text);

static void init_alter_table_model_column_shape(
    const struct mylite_alter_table_column *column,
    struct mylite_alter_table_foreign_key_column_shape *shape
);

static void deinit_alter_table_foreign_key_column_shape(
    struct mylite_alter_table_foreign_key_column_shape *shape
);

static bool alter_table_foreign_key_column_shapes_compatible(
    const struct mylite_alter_table_foreign_key_column_shape *child,
    const struct mylite_alter_table_foreign_key_column_shape *parent
);

static bool nullable_ascii_case_equal(const char *left, const char *right);

static sqlite3_destructor_type sqlite_transient_destructor(void);

static int set_alter_table_drop_child_foreign_key_column_error(
    mylite_db *database,
    const char *column_name,
    const char *constraint_name
);

static int set_alter_table_drop_parent_foreign_key_column_error(
    mylite_db *database,
    const char *column_name,
    const char *constraint_name,
    const char *child_table_name
);

static int set_alter_table_incompatible_foreign_key_column_error(
    mylite_db *database,
    const char *child_column_name,
    const char *parent_column_name,
    const char *constraint_name
);

static int set_alter_table_all_invisible_error(mylite_db *database);

static int set_alter_table_invalid_null_error(mylite_db *database);

int mylite_table_ddl_validate_alter_table_final_model(
    mylite_db *database,
    struct mylite_alter_table_model *model
) {
    bool has_visible_column = false;
    size_t auto_increment_count = 0U;
    int status = MYLITE_OK;

    if (database == NULL || model == NULL) {
        return MYLITE_MISUSE;
    }
    if (model->column_count == 0U) {
        return mylite_table_ddl_set_alter_table_cant_remove_all_columns_error(database);
    }
    status =
        validate_alter_table_columns(database, model, &has_visible_column, &auto_increment_count);
    if (status != MYLITE_OK) {
        return status;
    }
    if (!has_visible_column) {
        return set_alter_table_all_invisible_error(database);
    }
    status = validate_alter_table_auto_increment_shape(database, model, auto_increment_count);
    if (status != MYLITE_OK) {
        return status;
    }
    return validate_alter_table_foreign_key_columns(database, model);
}

static int validate_alter_table_columns(
    mylite_db *database,
    struct mylite_alter_table_model *model,
    bool *out_has_visible_column,
    size_t *out_auto_increment_count
) {
    *out_has_visible_column = false;
    *out_auto_increment_count = 0U;
    if (model->columns == NULL) {
        return MYLITE_MISUSE;
    }

    for (size_t column = 0U; column < model->column_count; ++column) {
        for (size_t next = column + 1U; next < model->column_count; ++next) {
            if (mylite_ascii_case_equal(model->columns[column].name, model->columns[next].name)) {
                return mylite_table_ddl_set_alter_table_duplicate_column_error(
                    database,
                    model->columns[next].name
                );
            }
        }
        if (model->columns[column].visible) {
            *out_has_visible_column = true;
        }
        if (model->columns[column].auto_increment) {
            ++*out_auto_increment_count;
        }
        if (!model->columns[column].nullable && model->columns[column].source_name != NULL) {
            int status =
                validate_alter_table_source_not_null(database, model, &model->columns[column]);

            if (status != MYLITE_OK) {
                return status;
            }
        }
    }

    return MYLITE_OK;
}

static int validate_alter_table_source_not_null(
    mylite_db *database,
    const struct mylite_alter_table_model *model,
    const struct mylite_alter_table_column *column
) {
    char *sql = sqlite3_mprintf(
        "SELECT 1 FROM \"%w\" WHERE \"%w\" IS NULL LIMIT 1",
        model->physical_name,
        column->source_name
    );
    sqlite3_stmt *select = NULL;
    int rc = SQLITE_OK;

    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &select, NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    rc = sqlite3_step(select);
    sqlite3_finalize(select);
    if (rc == SQLITE_ROW) {
        return set_alter_table_invalid_null_error(database);
    }
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static int validate_alter_table_auto_increment_shape(
    mylite_db *database,
    struct mylite_alter_table_model *model,
    size_t auto_increment_count
) {
    if (auto_increment_count > 1U) {
        return mylite_table_ddl_set_alter_table_wrong_auto_increment_error(database);
    }
    if (auto_increment_count == 1U) {
        const struct mylite_alter_table_column *auto_column =
            find_alter_table_auto_increment_column(model);

        if (auto_column == NULL || auto_column->nullable ||
            !alter_table_auto_increment_column_is_indexed(model, auto_column)) {
            return mylite_table_ddl_set_alter_table_wrong_auto_increment_error(database);
        }
    }
    if (auto_increment_count == 0U) {
        model->clear_auto_increment = true;
    }
    return MYLITE_OK;
}

static const struct mylite_alter_table_column *find_alter_table_auto_increment_column(
    const struct mylite_alter_table_model *model
) {
    if (model->columns == NULL) {
        return NULL;
    }
    for (size_t column = 0U; column < model->column_count; ++column) {
        if (model->columns[column].auto_increment) {
            return &model->columns[column];
        }
    }
    return NULL;
}

static bool alter_table_auto_increment_column_is_indexed(
    const struct mylite_alter_table_model *model,
    const struct mylite_alter_table_column *auto_column
) {
    for (size_t index = 0U; index < model->index_count; ++index) {
        for (size_t part = 0U; part < model->indexes[index].part_count; ++part) {
            if (mylite_ascii_case_equal(
                    model->indexes[index].parts[part].column_name,
                    auto_column->name
                )) {
                return true;
            }
        }
    }
    return false;
}

static int validate_alter_table_foreign_key_columns(
    mylite_db *database,
    const struct mylite_alter_table_model *model
) {
    int status = MYLITE_OK;

    if (model->temporary) {
        return MYLITE_OK;
    }
    status = validate_alter_table_child_foreign_key_columns(database, model);
    if (status != MYLITE_OK) {
        return status;
    }
    return validate_alter_table_parent_foreign_key_columns(database, model);
}

static int validate_alter_table_child_foreign_key_columns(
    mylite_db *database,
    const struct mylite_alter_table_model *model
) {
    static const char sql[] =
        "SELECT constraint_name, column_name, referenced_table_schema, referenced_table_name, "
        "referenced_column_name "
        "FROM __mylite_foreign_key_catalog "
        "WHERE table_schema = ? AND table_name = ? "
        "ORDER BY constraint_schema, constraint_name, ordinal_position";
    sqlite3_stmt *select = NULL;
    int rc =
        sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &select, NULL);

    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    sqlite3_bind_text(select, 1, model->schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(select, 2, model->table_name, -1, sqlite_transient_destructor());
    while ((rc = sqlite3_step(select)) == SQLITE_ROW) {
        const char *constraint_name = (const char *)sqlite3_column_text(select, 0);
        const char *child_source_name = (const char *)sqlite3_column_text(select, 1);
        const char *parent_schema_name = (const char *)sqlite3_column_text(select, 2);
        const char *parent_table_name = (const char *)sqlite3_column_text(select, 3);
        const char *parent_source_name = (const char *)sqlite3_column_text(select, 4);
        const struct mylite_alter_table_column *child_column =
            find_alter_table_column_by_source_name(model, child_source_name);
        const struct mylite_alter_table_column *parent_model_column = NULL;
        struct mylite_alter_table_foreign_key_column_shape child_shape = {0};
        struct mylite_alter_table_foreign_key_column_shape parent_shape = {0};
        bool parent_found = false;
        int status = MYLITE_OK;

        if (child_column == NULL) {
            status = set_alter_table_drop_child_foreign_key_column_error(
                database,
                child_source_name,
                constraint_name
            );
            sqlite3_finalize(select);
            return status;
        }

        init_alter_table_model_column_shape(child_column, &child_shape);
        if (mylite_ascii_case_equal(parent_schema_name, model->schema_name) &&
            mylite_ascii_case_equal(parent_table_name, model->table_name)) {
            parent_model_column = find_alter_table_column_by_source_name(model, parent_source_name);
            if (parent_model_column == NULL) {
                status = set_alter_table_drop_parent_foreign_key_column_error(
                    database,
                    parent_source_name,
                    constraint_name,
                    model->table_name
                );
                sqlite3_finalize(select);
                return status;
            }
            init_alter_table_model_column_shape(parent_model_column, &parent_shape);
            parent_found = true;
        } else {
            status = load_alter_table_catalog_column_shape(
                database,
                parent_schema_name,
                parent_table_name,
                parent_source_name,
                &parent_shape,
                &parent_found
            );
        }
        if (status != MYLITE_OK) {
            deinit_alter_table_foreign_key_column_shape(&parent_shape);
            sqlite3_finalize(select);
            return status;
        }
        if (parent_found &&
            !alter_table_foreign_key_column_shapes_compatible(&child_shape, &parent_shape)) {
            status = set_alter_table_incompatible_foreign_key_column_error(
                database,
                child_shape.name,
                parent_shape.name,
                constraint_name
            );
            deinit_alter_table_foreign_key_column_shape(&parent_shape);
            sqlite3_finalize(select);
            return status;
        }
        deinit_alter_table_foreign_key_column_shape(&parent_shape);
    }
    sqlite3_finalize(select);
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static int validate_alter_table_parent_foreign_key_columns(
    mylite_db *database,
    const struct mylite_alter_table_model *model
) {
    static const char sql[] =
        "SELECT constraint_name, table_schema, table_name, column_name, referenced_column_name "
        "FROM __mylite_foreign_key_catalog "
        "WHERE referenced_table_schema = ? AND referenced_table_name = ? "
        "ORDER BY constraint_schema, table_name, constraint_name, ordinal_position";
    sqlite3_stmt *select = NULL;
    int rc =
        sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &select, NULL);

    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    sqlite3_bind_text(select, 1, model->schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(select, 2, model->table_name, -1, sqlite_transient_destructor());
    while ((rc = sqlite3_step(select)) == SQLITE_ROW) {
        const char *constraint_name = (const char *)sqlite3_column_text(select, 0);
        const char *child_schema_name = (const char *)sqlite3_column_text(select, 1);
        const char *child_table_name = (const char *)sqlite3_column_text(select, 2);
        const char *child_source_name = (const char *)sqlite3_column_text(select, 3);
        const char *parent_source_name = (const char *)sqlite3_column_text(select, 4);
        const struct mylite_alter_table_column *parent_column =
            find_alter_table_column_by_source_name(model, parent_source_name);
        const struct mylite_alter_table_column *child_model_column = NULL;
        struct mylite_alter_table_foreign_key_column_shape child_shape = {0};
        struct mylite_alter_table_foreign_key_column_shape parent_shape = {0};
        bool child_found = false;
        int status = MYLITE_OK;

        if (parent_column == NULL) {
            status = set_alter_table_drop_parent_foreign_key_column_error(
                database,
                parent_source_name,
                constraint_name,
                child_table_name
            );
            sqlite3_finalize(select);
            return status;
        }

        init_alter_table_model_column_shape(parent_column, &parent_shape);
        if (mylite_ascii_case_equal(child_schema_name, model->schema_name) &&
            mylite_ascii_case_equal(child_table_name, model->table_name)) {
            child_model_column = find_alter_table_column_by_source_name(model, child_source_name);
            if (child_model_column == NULL) {
                status = set_alter_table_drop_child_foreign_key_column_error(
                    database,
                    child_source_name,
                    constraint_name
                );
                sqlite3_finalize(select);
                return status;
            }
            init_alter_table_model_column_shape(child_model_column, &child_shape);
            child_found = true;
        } else {
            status = load_alter_table_catalog_column_shape(
                database,
                child_schema_name,
                child_table_name,
                child_source_name,
                &child_shape,
                &child_found
            );
        }
        if (status != MYLITE_OK) {
            deinit_alter_table_foreign_key_column_shape(&child_shape);
            sqlite3_finalize(select);
            return status;
        }
        if (child_found &&
            !alter_table_foreign_key_column_shapes_compatible(&child_shape, &parent_shape)) {
            status = set_alter_table_incompatible_foreign_key_column_error(
                database,
                child_shape.name,
                parent_shape.name,
                constraint_name
            );
            deinit_alter_table_foreign_key_column_shape(&child_shape);
            sqlite3_finalize(select);
            return status;
        }
        deinit_alter_table_foreign_key_column_shape(&child_shape);
    }
    sqlite3_finalize(select);
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static const struct mylite_alter_table_column *find_alter_table_column_by_source_name(
    const struct mylite_alter_table_model *model,
    const char *source_name
) {
    for (size_t index = 0U; index < model->column_count; ++index) {
        const char *column_source_name = model->columns[index].source_name;

        if (column_source_name != NULL &&
            mylite_ascii_case_equal(column_source_name, source_name)) {
            return &model->columns[index];
        }
        if (column_source_name == NULL &&
            mylite_ascii_case_equal(model->columns[index].name, source_name)) {
            return &model->columns[index];
        }
    }
    return NULL;
}

static int load_alter_table_catalog_column_shape(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    const char *column_name,
    struct mylite_alter_table_foreign_key_column_shape *shape,
    bool *out_found
) {
    static const char sql[] = "SELECT column_name, column_type, character_set_name, collation_name "
                              "FROM __mylite_column_catalog "
                              "WHERE table_schema = ? AND table_name = ? "
                              "AND column_name = ? COLLATE NOCASE "
                              "LIMIT 1";
    sqlite3_stmt *select = NULL;
    int rc =
        sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &select, NULL);
    int status = MYLITE_OK;

    *shape = (struct mylite_alter_table_foreign_key_column_shape){0};
    *out_found = false;
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    sqlite3_bind_text(select, 1, schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(select, 2, table_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(select, 3, column_name, -1, sqlite_transient_destructor());
    rc = sqlite3_step(select);
    if (rc == SQLITE_ROW) {
        status = copy_shape_column(select, 0, &shape->owned_name);
        if (status == MYLITE_OK) {
            status = copy_shape_column(select, 1, &shape->owned_column_type);
        }
        if (status == MYLITE_OK) {
            status = copy_nullable_shape_column(select, 2, &shape->owned_character_set_name);
        }
        if (status == MYLITE_OK) {
            status = copy_nullable_shape_column(select, 3, &shape->owned_collation_name);
        }
        if (status == MYLITE_OK) {
            shape->name = shape->owned_name;
            shape->column_type = shape->owned_column_type;
            shape->character_set_name = shape->owned_character_set_name;
            shape->collation_name = shape->owned_collation_name;
            *out_found = true;
        } else {
            deinit_alter_table_foreign_key_column_shape(shape);
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
        }
    } else if (rc != SQLITE_DONE) {
        status = mylite_diagnostics_set_sqlite_error(database);
    }
    sqlite3_finalize(select);
    return status;
}

static int copy_shape_column(sqlite3_stmt *stmt, int column, char **out_text) {
    const unsigned char *text = sqlite3_column_text(stmt, column);
    int byte_count = sqlite3_column_bytes(stmt, column);

    *out_text = mylite_copy_span_text(
        text == NULL ? "" : (const char *)text,
        text == NULL || byte_count < 0 ? 0U : (size_t)byte_count
    );
    return *out_text == NULL ? MYLITE_NOMEM : MYLITE_OK;
}

static int copy_nullable_shape_column(sqlite3_stmt *stmt, int column, char **out_text) {
    if (sqlite3_column_type(stmt, column) == SQLITE_NULL) {
        *out_text = NULL;
        return MYLITE_OK;
    }
    return copy_shape_column(stmt, column, out_text);
}

static void init_alter_table_model_column_shape(
    const struct mylite_alter_table_column *column,
    struct mylite_alter_table_foreign_key_column_shape *shape
) {
    *shape = (struct mylite_alter_table_foreign_key_column_shape){
        .name = column->name,
        .column_type = column->column_type,
        .character_set_name = column->character_set_name,
        .collation_name = column->collation_name,
    };
}

static void deinit_alter_table_foreign_key_column_shape(
    struct mylite_alter_table_foreign_key_column_shape *shape
) {
    free(shape->owned_name);
    free(shape->owned_column_type);
    free(shape->owned_character_set_name);
    free(shape->owned_collation_name);
    *shape = (struct mylite_alter_table_foreign_key_column_shape){0};
}

static bool alter_table_foreign_key_column_shapes_compatible(
    const struct mylite_alter_table_foreign_key_column_shape *child,
    const struct mylite_alter_table_foreign_key_column_shape *parent
) {
    if (!nullable_ascii_case_equal(child->column_type, parent->column_type)) {
        return false;
    }
    if (!nullable_ascii_case_equal(child->character_set_name, parent->character_set_name)) {
        return false;
    }
    return nullable_ascii_case_equal(child->collation_name, parent->collation_name);
}

static bool nullable_ascii_case_equal(const char *left, const char *right) {
    if (left == NULL || right == NULL) {
        return left == right;
    }
    return mylite_ascii_case_equal(left, right);
}

static int set_alter_table_drop_child_foreign_key_column_error(
    mylite_db *database,
    const char *column_name,
    const char *constraint_name
) {
    char *message = sqlite3_mprintf(
        "Cannot drop column '%q': needed in a foreign key constraint '%q'",
        column_name,
        constraint_name
    );
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_set_error_message(database, message);
    if (status == MYLITE_OK) {
        status = mylite_diagnostics_append_error(
            database,
            MYLITE_MYSQL_ER_FK_COLUMN_CANNOT_DROP,
            message
        );
    }
    sqlite3_free(message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int set_alter_table_drop_parent_foreign_key_column_error(
    mylite_db *database,
    const char *column_name,
    const char *constraint_name,
    const char *child_table_name
) {
    char *message = sqlite3_mprintf(
        "Cannot drop column '%q': needed in a foreign key constraint '%q' of table '%q'",
        column_name,
        constraint_name,
        child_table_name
    );
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_set_error_message(database, message);
    if (status == MYLITE_OK) {
        status = mylite_diagnostics_append_error(
            database,
            MYLITE_MYSQL_ER_FK_COLUMN_CANNOT_DROP_CHILD,
            message
        );
    }
    sqlite3_free(message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int set_alter_table_incompatible_foreign_key_column_error(
    mylite_db *database,
    const char *child_column_name,
    const char *parent_column_name,
    const char *constraint_name
) {
    char *message = sqlite3_mprintf(
        "Referencing column '%q' and referenced column '%q' in foreign key constraint '%q' "
        "are incompatible.",
        child_column_name,
        parent_column_name,
        constraint_name
    );
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_set_error_message(database, message);
    if (status == MYLITE_OK) {
        status = mylite_diagnostics_append_error(
            database,
            MYLITE_MYSQL_ER_FK_INCOMPATIBLE_COLUMNS,
            message
        );
    }
    sqlite3_free(message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int set_alter_table_all_invisible_error(mylite_db *database) {
    int status = mylite_diagnostics_set_error_message(
        database,
        "A table must have at least one visible column"
    );

    if (status == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_append_error(
        database,
        MYLITE_MYSQL_ER_INVISIBLE_NOT_NULL_WITHOUT_DEFAULT,
        mylite_error_message(database)
    );
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int set_alter_table_invalid_null_error(mylite_db *database) {
    int status = mylite_diagnostics_set_error_message(database, "Invalid use of NULL value");

    if (status == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_append_error(
        database,
        MYLITE_MYSQL_ER_INVALID_USE_OF_NULL,
        mylite_error_message(database)
    );
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static sqlite3_destructor_type sqlite_transient_destructor(void) {
    return SQLITE_TRANSIENT; // NOLINT(performance-no-int-to-ptr)
}
