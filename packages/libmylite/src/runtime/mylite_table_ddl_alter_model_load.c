#include "mylite_table_ddl_alter_model.h"

#include "mylite_catalog.h"
#include "mylite_diagnostics.h"
#include "mylite_runtime.h"
#include "mylite_span.h"
#include "mylite_table_ddl.h"
#include "sqlite3.h"

#include <stdbool.h>
#include <stdlib.h>

static int load_alter_table_columns(mylite_db *database, struct mylite_alter_table_model *model);

static int load_alter_table_column_from_catalog_row(
    mylite_db *database,
    sqlite3_stmt *select,
    struct mylite_alter_table_model *model
);

static int load_alter_table_column_text_catalog_fields(
    sqlite3_stmt *select,
    struct mylite_alter_table_column *column
);

static void load_alter_table_column_numeric_catalog_fields(
    sqlite3_stmt *select,
    struct mylite_alter_table_column *column
);

static void load_alter_table_column_flags(struct mylite_alter_table_column *column);

static int load_alter_table_indexes(mylite_db *database, struct mylite_alter_table_model *model);

static int add_alter_table_index_part(
    mylite_db *database,
    struct mylite_alter_table_model *model,
    sqlite3_stmt *select
);

static size_t loaded_alter_table_index_index(
    const struct mylite_alter_table_model *model,
    const char *name
);

static int copy_sqlite_text_column(sqlite3_stmt *stmt, int column, char **out_text);

static int copy_sqlite_nullable_text_column(sqlite3_stmt *stmt, int column, char **out_text);

static sqlite3_destructor_type sqlite_transient_destructor(void);

int mylite_table_ddl_load_alter_table_model(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    bool temporary,
    struct mylite_alter_table_model *model
) {
    sqlite3_stmt *select = NULL;
    char *sql = NULL;
    int rc = SQLITE_OK;
    int status = MYLITE_OK;

    *model = (struct mylite_alter_table_model){0};
    model->temporary = temporary;
    model->schema_name = mylite_copy_nonempty_cstring(schema_name);
    model->table_name = mylite_copy_nonempty_cstring(table_name);
    model->physical_name = mylite_catalog_physical_table_name(schema_name, table_name);
    if (model->schema_name == NULL || model->table_name == NULL || model->physical_name == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        mylite_table_ddl_alter_table_model_deinit(model);
        return MYLITE_NOMEM;
    }

    sql = sqlite3_mprintf(
        "SELECT table_collation FROM %s WHERE table_schema = ? "
        "AND table_name = ?",
        mylite_catalog_table_catalog_name(temporary)
    );
    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        mylite_table_ddl_alter_table_model_deinit(model);
        return MYLITE_NOMEM;
    }
    rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &select, NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        mylite_table_ddl_alter_table_model_deinit(model);
        return mylite_diagnostics_set_sqlite_error(database);
    }
    sqlite3_bind_text(select, 1, schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(select, 2, table_name, -1, sqlite_transient_destructor());
    rc = sqlite3_step(select);
    if (rc == SQLITE_ROW) {
        status = copy_sqlite_nullable_text_column(select, 0, &model->table_collation);
    } else {
        status =
            rc == SQLITE_DONE
                ? mylite_diagnostics_set_table_doesnt_exist_error(database, schema_name, table_name)
                : mylite_diagnostics_set_sqlite_error(database);
    }
    sqlite3_finalize(select);
    if (status == MYLITE_OK) {
        status = load_alter_table_columns(database, model);
    }
    if (status == MYLITE_OK) {
        status = load_alter_table_indexes(database, model);
    }
    if (status != MYLITE_OK) {
        mylite_table_ddl_alter_table_model_deinit(model);
    }
    return status;
}

static int load_alter_table_columns(mylite_db *database, struct mylite_alter_table_model *model) {
    sqlite3_stmt *select = NULL;
    char *sql = sqlite3_mprintf(
        "SELECT column_name, column_default, is_nullable, data_type, "
        "character_maximum_length, character_octet_length, numeric_precision, numeric_scale, "
        "datetime_precision, character_set_name, collation_name, column_type, column_key, extra, "
        "column_comment, generation_expression, srs_id FROM %s "
        "WHERE table_schema = ? AND table_name = ? ORDER BY ordinal_position",
        mylite_catalog_column_catalog_name(model->temporary)
    );
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

    sqlite3_bind_text(select, 1, model->schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(select, 2, model->table_name, -1, sqlite_transient_destructor());
    while ((rc = sqlite3_step(select)) == SQLITE_ROW) {
        int status = load_alter_table_column_from_catalog_row(database, select, model);

        if (status != MYLITE_OK) {
            sqlite3_finalize(select);
            return status;
        }
    }
    sqlite3_finalize(select);
    if (rc != SQLITE_DONE) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    if (model->column_count == 0U) {
        return mylite_diagnostics_set_table_doesnt_exist_error(
            database,
            model->schema_name,
            model->table_name
        );
    }
    return MYLITE_OK;
}

static int load_alter_table_column_from_catalog_row(
    mylite_db *database,
    sqlite3_stmt *select,
    struct mylite_alter_table_model *model
) {
    struct mylite_alter_table_column column = {0};
    int status = load_alter_table_column_text_catalog_fields(select, &column);

    load_alter_table_column_numeric_catalog_fields(select, &column);
    if (status != MYLITE_OK) {
        mylite_table_ddl_alter_table_column_deinit(&column);
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
        }
        return status;
    }

    load_alter_table_column_flags(&column);
    status = mylite_table_ddl_add_alter_table_column(model, column);
    if (status != MYLITE_OK) {
        mylite_table_ddl_alter_table_column_deinit(&column);
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
        }
    }
    return status;
}

static int load_alter_table_column_text_catalog_fields(
    sqlite3_stmt *select,
    struct mylite_alter_table_column *column
) {
    int status =
        copy_sqlite_text_column(select, MYLITE_ALTER_TABLE_COLUMN_CATALOG_NAME, &column->name);

    if (status == MYLITE_OK) {
        column->source_name = mylite_copy_nonempty_cstring(column->name);
        if (column->source_name == NULL) {
            status = MYLITE_NOMEM;
        }
    }
    if (status == MYLITE_OK) {
        status = copy_sqlite_nullable_text_column(
            select,
            MYLITE_ALTER_TABLE_COLUMN_CATALOG_DEFAULT,
            &column->column_default
        );
    }
    if (status == MYLITE_OK) {
        status = copy_sqlite_text_column(
            select,
            MYLITE_ALTER_TABLE_COLUMN_CATALOG_IS_NULLABLE,
            &column->is_nullable
        );
    }
    if (status == MYLITE_OK) {
        status = copy_sqlite_nullable_text_column(
            select,
            MYLITE_ALTER_TABLE_COLUMN_CATALOG_DATA_TYPE,
            &column->data_type
        );
    }
    if (status == MYLITE_OK) {
        status = copy_sqlite_nullable_text_column(
            select,
            MYLITE_ALTER_TABLE_COLUMN_CATALOG_CHARACTER_SET_NAME,
            &column->character_set_name
        );
    }
    if (status == MYLITE_OK) {
        status = copy_sqlite_nullable_text_column(
            select,
            MYLITE_ALTER_TABLE_COLUMN_CATALOG_COLLATION_NAME,
            &column->collation_name
        );
    }
    if (status == MYLITE_OK) {
        status = copy_sqlite_text_column(
            select,
            MYLITE_ALTER_TABLE_COLUMN_CATALOG_COLUMN_TYPE,
            &column->column_type
        );
    }
    if (status == MYLITE_OK) {
        status = copy_sqlite_text_column(
            select,
            MYLITE_ALTER_TABLE_COLUMN_CATALOG_COLUMN_KEY,
            &column->column_key
        );
    }
    if (status == MYLITE_OK) {
        status = copy_sqlite_nullable_text_column(
            select,
            MYLITE_ALTER_TABLE_COLUMN_CATALOG_EXTRA,
            &column->extra
        );
    }
    if (status == MYLITE_OK) {
        status = copy_sqlite_text_column(
            select,
            MYLITE_ALTER_TABLE_COLUMN_CATALOG_COMMENT,
            &column->column_comment
        );
    }
    if (status == MYLITE_OK) {
        status = copy_sqlite_text_column(
            select,
            MYLITE_ALTER_TABLE_COLUMN_CATALOG_GENERATION_EXPRESSION,
            &column->generation_expression
        );
    }
    return status;
}

static void load_alter_table_column_numeric_catalog_fields(
    sqlite3_stmt *select,
    struct mylite_alter_table_column *column
) {
    if (sqlite3_column_type(select, MYLITE_ALTER_TABLE_COLUMN_CATALOG_CHARACTER_MAXIMUM_LENGTH) !=
        SQLITE_NULL) {
        column->character_maximum_length = sqlite3_column_int64(
            select,
            MYLITE_ALTER_TABLE_COLUMN_CATALOG_CHARACTER_MAXIMUM_LENGTH
        );
        column->has_character_maximum_length = true;
    }
    if (sqlite3_column_type(select, MYLITE_ALTER_TABLE_COLUMN_CATALOG_CHARACTER_OCTET_LENGTH) !=
        SQLITE_NULL) {
        column->character_octet_length =
            sqlite3_column_int64(select, MYLITE_ALTER_TABLE_COLUMN_CATALOG_CHARACTER_OCTET_LENGTH);
        column->has_character_octet_length = true;
    }
    if (sqlite3_column_type(select, MYLITE_ALTER_TABLE_COLUMN_CATALOG_NUMERIC_PRECISION) !=
        SQLITE_NULL) {
        column->numeric_precision =
            sqlite3_column_int64(select, MYLITE_ALTER_TABLE_COLUMN_CATALOG_NUMERIC_PRECISION);
        column->has_numeric_precision = true;
    }
    if (sqlite3_column_type(select, MYLITE_ALTER_TABLE_COLUMN_CATALOG_NUMERIC_SCALE) !=
        SQLITE_NULL) {
        column->numeric_scale =
            sqlite3_column_int64(select, MYLITE_ALTER_TABLE_COLUMN_CATALOG_NUMERIC_SCALE);
        column->has_numeric_scale = true;
    }
    if (sqlite3_column_type(select, MYLITE_ALTER_TABLE_COLUMN_CATALOG_DATETIME_PRECISION) !=
        SQLITE_NULL) {
        column->datetime_precision =
            sqlite3_column_int64(select, MYLITE_ALTER_TABLE_COLUMN_CATALOG_DATETIME_PRECISION);
        column->has_datetime_precision = true;
    }
    if (sqlite3_column_type(select, MYLITE_ALTER_TABLE_COLUMN_CATALOG_SRS_ID) != SQLITE_NULL) {
        column->srs_id = sqlite3_column_int64(select, MYLITE_ALTER_TABLE_COLUMN_CATALOG_SRS_ID);
        column->has_srs_id = true;
    }
}

static void load_alter_table_column_flags(struct mylite_alter_table_column *column) {
    column->nullable = mylite_ascii_case_equal(column->is_nullable, "YES");
    column->auto_increment = mylite_text_contains_word(column->extra, "auto_increment");
    column->visible = true;
    if (mylite_text_contains_word(column->extra, "INVISIBLE")) {
        column->visible = false;
    }
}

static int load_alter_table_indexes(mylite_db *database, struct mylite_alter_table_model *model) {
    sqlite3_stmt *select = NULL;
    char *sql = sqlite3_mprintf(
        "SELECT non_unique, index_schema, index_name, seq_in_index, column_name, collation, "
        "sub_part, nullable, index_type, comment, index_comment, is_visible "
        "FROM %s WHERE table_schema = ? AND table_name = ? ORDER BY rowid",
        mylite_catalog_index_catalog_name(model->temporary)
    );
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

    sqlite3_bind_text(select, 1, model->schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(select, 2, model->table_name, -1, sqlite_transient_destructor());
    while ((rc = sqlite3_step(select)) == SQLITE_ROW) {
        int status = add_alter_table_index_part(database, model, select);

        if (status != MYLITE_OK) {
            sqlite3_finalize(select);
            return status;
        }
    }
    sqlite3_finalize(select);
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static int add_alter_table_index_part(
    mylite_db *database,
    struct mylite_alter_table_model *model,
    sqlite3_stmt *select
) {
    const char *index_name =
        (const char *)sqlite3_column_text(select, MYLITE_ALTER_TABLE_INDEX_CATALOG_NAME);
    size_t index = loaded_alter_table_index_index(model, index_name);
    struct mylite_alter_table_index_part part = {0};
    int status = MYLITE_OK;

    if (index == model->index_count) {
        struct mylite_alter_table_index *indexes =
            realloc(model->indexes, (model->index_count + 1U) * sizeof(*model->indexes));

        if (indexes == NULL) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
            return MYLITE_NOMEM;
        }
        model->indexes = indexes;
        model->indexes[model->index_count] = (struct mylite_alter_table_index){
            .non_unique = sqlite3_column_int(select, MYLITE_ALTER_TABLE_INDEX_CATALOG_NON_UNIQUE),
        };
        index = model->index_count++;

        status = copy_sqlite_text_column(
            select,
            MYLITE_ALTER_TABLE_INDEX_CATALOG_SCHEMA,
            &model->indexes[index].index_schema
        );
        if (status == MYLITE_OK) {
            status = copy_sqlite_text_column(
                select,
                MYLITE_ALTER_TABLE_INDEX_CATALOG_NAME,
                &model->indexes[index].name
            );
        }
        if (status == MYLITE_OK) {
            status = copy_sqlite_text_column(
                select,
                MYLITE_ALTER_TABLE_INDEX_CATALOG_TYPE,
                &model->indexes[index].index_type
            );
        }
        if (status == MYLITE_OK) {
            status = copy_sqlite_text_column(
                select,
                MYLITE_ALTER_TABLE_INDEX_CATALOG_COMMENT,
                &model->indexes[index].comment
            );
        }
        if (status == MYLITE_OK) {
            status = copy_sqlite_text_column(
                select,
                MYLITE_ALTER_TABLE_INDEX_CATALOG_INDEX_COMMENT,
                &model->indexes[index].index_comment
            );
        }
        if (status == MYLITE_OK) {
            status = copy_sqlite_text_column(
                select,
                MYLITE_ALTER_TABLE_INDEX_CATALOG_VISIBLE,
                &model->indexes[index].is_visible
            );
        }
        if (status != MYLITE_OK) {
            if (status == MYLITE_NOMEM) {
                (void)mylite_diagnostics_set_error_message(database, "out of memory");
            }
            return status;
        }
    }

    status = copy_sqlite_nullable_text_column(
        select,
        MYLITE_ALTER_TABLE_INDEX_CATALOG_COLUMN_NAME,
        &part.column_name
    );
    if (status == MYLITE_OK) {
        status = copy_sqlite_nullable_text_column(
            select,
            MYLITE_ALTER_TABLE_INDEX_CATALOG_COLLATION,
            &part.collation
        );
    }
    if (sqlite3_column_type(select, MYLITE_ALTER_TABLE_INDEX_CATALOG_SUB_PART) != SQLITE_NULL) {
        part.sub_part = sqlite3_column_int64(select, MYLITE_ALTER_TABLE_INDEX_CATALOG_SUB_PART);
        part.has_sub_part = true;
    }
    if (status == MYLITE_OK) {
        status = copy_sqlite_text_column(
            select,
            MYLITE_ALTER_TABLE_INDEX_CATALOG_NULLABLE,
            &part.nullable
        );
    }
    if (status == MYLITE_OK) {
        status = mylite_table_ddl_append_alter_table_index_part(&model->indexes[index], part);
    }
    if (status != MYLITE_OK) {
        mylite_table_ddl_alter_table_index_part_deinit(&part);
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
        }
    }
    return status;
}

static size_t loaded_alter_table_index_index(
    const struct mylite_alter_table_model *model,
    const char *name
) {
    for (size_t index = 0U; index < model->index_count; ++index) {
        if (mylite_ascii_case_equal(model->indexes[index].name, name)) {
            return index;
        }
    }
    return model->index_count;
}

static int copy_sqlite_text_column(sqlite3_stmt *stmt, int column, char **out_text) {
    const unsigned char *text = sqlite3_column_text(stmt, column);
    int byte_count = sqlite3_column_bytes(stmt, column);

    *out_text = mylite_copy_span_text(
        text == NULL ? "" : (const char *)text,
        text == NULL || byte_count < 0 ? 0U : (size_t)byte_count
    );
    return *out_text == NULL ? MYLITE_NOMEM : MYLITE_OK;
}

static int copy_sqlite_nullable_text_column(sqlite3_stmt *stmt, int column, char **out_text) {
    if (sqlite3_column_type(stmt, column) == SQLITE_NULL) {
        *out_text = NULL;
        return MYLITE_OK;
    }
    return copy_sqlite_text_column(stmt, column, out_text);
}

static sqlite3_destructor_type sqlite_transient_destructor(void) {
    return SQLITE_TRANSIENT; // NOLINT(performance-no-int-to-ptr)
}
