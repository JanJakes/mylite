#include "mylite_table_ddl_alter_catalog.h"

#include "mylite_catalog.h"
#include "mylite_diagnostics.h"
#include "mylite_foreign_key_catalog.h"
#include "mylite_runtime.h"
#include "mylite_span.h"
#include "mylite_table_ddl_types.h"
#include "mylite_uint64_text.h"
#include "sqlite3.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static int delete_alter_table_catalog_rows(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    bool temporary
);

static int insert_alter_table_column_catalog_rows(
    mylite_db *database,
    const struct mylite_alter_table_model *model
);

static int insert_alter_table_column_catalog_row(
    mylite_db *database,
    sqlite3_stmt *insert,
    const struct mylite_alter_table_model *model,
    const struct mylite_alter_table_column *column,
    size_t column_index
);

static int insert_alter_table_index_catalog_rows(
    mylite_db *database,
    const struct mylite_alter_table_model *model
);

static int insert_alter_table_index_catalog_part(
    mylite_db *database,
    sqlite3_stmt *insert,
    const struct mylite_alter_table_model *model,
    const struct mylite_alter_table_index *index,
    const struct mylite_alter_table_index_part *part,
    size_t part_index
);

static int update_alter_table_foreign_key_column_names(
    mylite_db *database,
    const struct mylite_alter_table_model *model
);

static int update_alter_table_auto_increment(
    mylite_db *database,
    const struct mylite_alter_table_model *model
);

static int alter_table_effective_auto_increment(
    mylite_db *database,
    const struct mylite_alter_table_model *model,
    uint64_t requested_auto_increment,
    uint64_t *out_auto_increment
);

static const struct mylite_alter_table_column *find_alter_table_auto_increment_column(
    const struct mylite_alter_table_model *model
);

static int refresh_alter_table_statistics(
    mylite_db *database,
    const struct mylite_alter_table_model *model
);

static sqlite3_destructor_type sqlite_transient_destructor(void);

int mylite_table_ddl_rewrite_alter_table_catalog(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    const struct mylite_alter_table_model *model
) {
    int status =
        delete_alter_table_catalog_rows(database, schema_name, table_name, model->temporary);

    if (status == MYLITE_OK) {
        status = insert_alter_table_column_catalog_rows(database, model);
    }
    if (status == MYLITE_OK) {
        status = insert_alter_table_index_catalog_rows(database, model);
    }
    if (status == MYLITE_OK) {
        status = update_alter_table_foreign_key_column_names(database, model);
    }
    if (status == MYLITE_OK) {
        status = update_alter_table_auto_increment(database, model);
    }
    if (status == MYLITE_OK) {
        status = refresh_alter_table_statistics(database, model);
    }
    return status;
}

static int delete_alter_table_catalog_rows(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    bool temporary
) {
    if (temporary) {
        return mylite_catalog_delete_temporary_table_rows(
            database,
            schema_name,
            table_name,
            MYLITE_CATALOG_DELETE_TABLE_INDEXES | MYLITE_CATALOG_DELETE_TABLE_COLUMNS
        );
    }
    return mylite_catalog_delete_table_rows(
        database,
        schema_name,
        table_name,
        MYLITE_CATALOG_DELETE_TABLE_INDEXES | MYLITE_CATALOG_DELETE_TABLE_COLUMNS
    );
}

static int insert_alter_table_column_catalog_rows(
    mylite_db *database,
    const struct mylite_alter_table_model *model
) {
    sqlite3_stmt *insert = NULL;
    char *sql = sqlite3_mprintf(
        "INSERT INTO %s("
        "table_catalog, table_schema, table_name, column_name, ordinal_position, column_default, "
        "has_default, is_nullable, data_type, character_maximum_length, character_octet_length, "
        "numeric_precision, numeric_scale, datetime_precision, character_set_name, "
        "collation_name, column_type, column_key, extra, privileges, column_comment, "
        "generation_expression, srs_id)"
        " VALUES('def', ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "'select,insert,update,references', ?, ?, ?)",
        mylite_catalog_column_catalog_name(model->temporary)
    );
    int rc = SQLITE_OK;

    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &insert, NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    for (size_t index = 0U; index < model->column_count; ++index) {
        int status = insert_alter_table_column_catalog_row(
            database,
            insert,
            model,
            &model->columns[index],
            index
        );

        if (status != MYLITE_OK) {
            sqlite3_finalize(insert);
            return status;
        }
    }
    sqlite3_finalize(insert);
    return MYLITE_OK;
}

static int insert_alter_table_column_catalog_row(
    mylite_db *database,
    sqlite3_stmt *insert,
    const struct mylite_alter_table_model *model,
    const struct mylite_alter_table_column *column,
    size_t column_index
) {
    enum {
        bind_table_schema = 1,
        bind_table_name = 2,
        bind_column_name = 3,
        bind_ordinal_position = 4,
        bind_column_default = 5,
        bind_has_default = 6,
        bind_is_nullable = 7,
        bind_data_type = 8,
        bind_character_maximum_length = 9,
        bind_character_octet_length = 10,
        bind_numeric_precision = 11,
        bind_numeric_scale = 12,
        bind_datetime_precision = 13,
        bind_character_set_name = 14,
        bind_collation_name = 15,
        bind_column_type = 16,
        bind_column_key = 17,
        bind_extra = 18,
        bind_column_comment = 19,
        bind_generation_expression = 20,
        bind_srs_id = 21,
    };

    int rc = SQLITE_OK;

    sqlite3_reset(insert);
    sqlite3_clear_bindings(insert);
    sqlite3_bind_text(
        insert,
        bind_table_schema,
        model->schema_name,
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_text(
        insert,
        bind_table_name,
        model->table_name,
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_text(insert, bind_column_name, column->name, -1, sqlite_transient_destructor());
    sqlite3_bind_int64(insert, bind_ordinal_position, (sqlite3_int64)column_index + 1);
    if (column->column_default == NULL) {
        sqlite3_bind_null(insert, bind_column_default);
    } else {
        sqlite3_bind_text(
            insert,
            bind_column_default,
            column->column_default,
            -1,
            sqlite_transient_destructor()
        );
    }
    sqlite3_bind_int(insert, bind_has_default, (int)column->has_default);
    sqlite3_bind_text(
        insert,
        bind_is_nullable,
        column->is_nullable,
        -1,
        sqlite_transient_destructor()
    );
    if (column->data_type == NULL) {
        sqlite3_bind_null(insert, bind_data_type);
    } else {
        sqlite3_bind_text(
            insert,
            bind_data_type,
            column->data_type,
            -1,
            sqlite_transient_destructor()
        );
    }
    if (column->has_character_maximum_length) {
        sqlite3_bind_int64(
            insert,
            bind_character_maximum_length,
            (sqlite3_int64)column->character_maximum_length
        );
    } else {
        sqlite3_bind_null(insert, bind_character_maximum_length);
    }
    if (column->has_character_octet_length) {
        sqlite3_bind_int64(
            insert,
            bind_character_octet_length,
            (sqlite3_int64)column->character_octet_length
        );
    } else {
        sqlite3_bind_null(insert, bind_character_octet_length);
    }
    if (column->has_numeric_precision) {
        sqlite3_bind_int64(
            insert,
            bind_numeric_precision,
            (sqlite3_int64)column->numeric_precision
        );
    } else {
        sqlite3_bind_null(insert, bind_numeric_precision);
    }
    if (column->has_numeric_scale) {
        sqlite3_bind_int64(insert, bind_numeric_scale, (sqlite3_int64)column->numeric_scale);
    } else {
        sqlite3_bind_null(insert, bind_numeric_scale);
    }
    if (column->has_datetime_precision) {
        sqlite3_bind_int64(
            insert,
            bind_datetime_precision,
            (sqlite3_int64)column->datetime_precision
        );
    } else {
        sqlite3_bind_null(insert, bind_datetime_precision);
    }
    if (column->character_set_name == NULL) {
        sqlite3_bind_null(insert, bind_character_set_name);
    } else {
        sqlite3_bind_text(
            insert,
            bind_character_set_name,
            column->character_set_name,
            -1,
            sqlite_transient_destructor()
        );
    }
    if (column->collation_name == NULL) {
        sqlite3_bind_null(insert, bind_collation_name);
    } else {
        sqlite3_bind_text(
            insert,
            bind_collation_name,
            column->collation_name,
            -1,
            sqlite_transient_destructor()
        );
    }
    sqlite3_bind_text(
        insert,
        bind_column_type,
        column->column_type,
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_text(
        insert,
        bind_column_key,
        column->column_key,
        -1,
        sqlite_transient_destructor()
    );
    if (column->extra == NULL) {
        sqlite3_bind_null(insert, bind_extra);
    } else {
        sqlite3_bind_text(insert, bind_extra, column->extra, -1, sqlite_transient_destructor());
    }
    sqlite3_bind_text(
        insert,
        bind_column_comment,
        column->column_comment,
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_text(
        insert,
        bind_generation_expression,
        column->generation_expression,
        -1,
        sqlite_transient_destructor()
    );
    if (column->has_srs_id) {
        sqlite3_bind_int64(insert, bind_srs_id, (sqlite3_int64)column->srs_id);
    } else {
        sqlite3_bind_null(insert, bind_srs_id);
    }

    rc = sqlite3_step(insert);
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static int insert_alter_table_index_catalog_rows(
    mylite_db *database,
    const struct mylite_alter_table_model *model
) {
    sqlite3_stmt *insert = NULL;
    char *sql = sqlite3_mprintf(
        "INSERT INTO %s("
        "table_catalog, table_schema, table_name, non_unique, index_schema, index_name, "
        "seq_in_index, column_name, collation, cardinality, sub_part, packed, nullable, "
        "index_type, display_index_type, parser_name, comment, index_comment, is_visible, "
        "expression)"
        " VALUES('def', ?, ?, ?, ?, ?, ?, ?, ?, 0, ?, NULL, ?, ?, ?, ?, ?, ?, ?, NULL)",
        mylite_catalog_index_catalog_name(model->temporary)
    );
    int rc = SQLITE_OK;

    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &insert, NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    for (size_t index = 0U; index < model->index_count; ++index) {
        for (size_t part = 0U; part < model->indexes[index].part_count; ++part) {
            int status = insert_alter_table_index_catalog_part(
                database,
                insert,
                model,
                &model->indexes[index],
                &model->indexes[index].parts[part],
                part
            );

            if (status != MYLITE_OK) {
                sqlite3_finalize(insert);
                return status;
            }
        }
    }
    sqlite3_finalize(insert);
    return MYLITE_OK;
}

static int insert_alter_table_index_catalog_part(
    mylite_db *database,
    sqlite3_stmt *insert,
    const struct mylite_alter_table_model *model,
    const struct mylite_alter_table_index *index,
    const struct mylite_alter_table_index_part *part,
    size_t part_index
) {
    enum {
        bind_table_schema = 1,
        bind_table_name = 2,
        bind_non_unique = 3,
        bind_index_schema = 4,
        bind_index_name = 5,
        bind_seq_in_index = 6,
        bind_column_name = 7,
        bind_collation = 8,
        bind_sub_part = 9,
        bind_nullable = 10,
        bind_index_type = 11,
        bind_display_index_type = 12,
        bind_parser_name = 13,
        bind_comment = 14,
        bind_index_comment = 15,
        bind_is_visible = 16,
    };

    int rc = SQLITE_OK;

    sqlite3_reset(insert);
    sqlite3_clear_bindings(insert);
    sqlite3_bind_text(
        insert,
        bind_table_schema,
        model->schema_name,
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_text(
        insert,
        bind_table_name,
        model->table_name,
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_int(insert, bind_non_unique, index->non_unique);
    sqlite3_bind_text(
        insert,
        bind_index_schema,
        index->index_schema,
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_text(insert, bind_index_name, index->name, -1, sqlite_transient_destructor());
    sqlite3_bind_int64(insert, bind_seq_in_index, (sqlite3_int64)part_index + 1);
    sqlite3_bind_text(
        insert,
        bind_column_name,
        part->column_name,
        -1,
        sqlite_transient_destructor()
    );
    if (part->collation == NULL) {
        sqlite3_bind_null(insert, bind_collation);
    } else {
        sqlite3_bind_text(
            insert,
            bind_collation,
            part->collation,
            -1,
            sqlite_transient_destructor()
        );
    }
    if (part->has_sub_part) {
        sqlite3_bind_int64(insert, bind_sub_part, (sqlite3_int64)part->sub_part);
    } else {
        sqlite3_bind_null(insert, bind_sub_part);
    }
    sqlite3_bind_text(insert, bind_nullable, part->nullable, -1, sqlite_transient_destructor());
    sqlite3_bind_text(
        insert,
        bind_index_type,
        index->index_type,
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_int(insert, bind_display_index_type, (int)index->display_index_type);
    sqlite3_bind_text(
        insert,
        bind_parser_name,
        index->parser_name == NULL ? "" : index->parser_name,
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_text(insert, bind_comment, index->comment, -1, sqlite_transient_destructor());
    sqlite3_bind_text(
        insert,
        bind_index_comment,
        index->index_comment,
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_text(
        insert,
        bind_is_visible,
        index->is_visible,
        -1,
        sqlite_transient_destructor()
    );

    rc = sqlite3_step(insert);
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static int update_alter_table_foreign_key_column_names(
    mylite_db *database,
    const struct mylite_alter_table_model *model
) {
    if (model->temporary) {
        return MYLITE_OK;
    }
    for (size_t index = 0U; index < model->column_count; ++index) {
        const struct mylite_alter_table_column *column = &model->columns[index];
        int status = MYLITE_OK;

        if (column->source_name == NULL ||
            mylite_ascii_case_equal(column->source_name, column->name)) {
            continue;
        }
        status = mylite_foreign_key_catalog_rewrite_child_column(
            database,
            model->schema_name,
            model->table_name,
            column->source_name,
            column->name
        );
        if (status != MYLITE_OK) {
            return status;
        }
        status = mylite_foreign_key_catalog_rewrite_parent_column(
            database,
            model->schema_name,
            model->table_name,
            column->source_name,
            column->name
        );
        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int update_alter_table_auto_increment(
    mylite_db *database,
    const struct mylite_alter_table_model *model
) {
    sqlite3_stmt *update = NULL;
    char *sql = NULL;
    int rc = SQLITE_OK;

    if (model->set_auto_increment) {
        uint64_t auto_increment = 0U;
        int status = alter_table_effective_auto_increment(
            database,
            model,
            model->auto_increment,
            &auto_increment
        );

        if (status != MYLITE_OK) {
            return status;
        }
        return mylite_catalog_update_auto_increment(
            database,
            model->schema_name,
            model->table_name,
            auto_increment
        );
    }
    if (!model->clear_auto_increment) {
        return MYLITE_OK;
    }
    sql = sqlite3_mprintf(
        "UPDATE %s SET auto_increment = NULL "
        "WHERE table_schema = ? AND table_name = ?",
        mylite_catalog_table_catalog_name(model->temporary)
    );
    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &update, NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    sqlite3_bind_text(update, 1, model->schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(update, 2, model->table_name, -1, sqlite_transient_destructor());
    rc = sqlite3_step(update);
    sqlite3_finalize(update);
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static int alter_table_effective_auto_increment(
    mylite_db *database,
    const struct mylite_alter_table_model *model,
    uint64_t requested_auto_increment,
    uint64_t *out_auto_increment
) {
    const struct mylite_alter_table_column *column = find_alter_table_auto_increment_column(model);
    sqlite3_stmt *select = NULL;
    char *sql = NULL;
    int rc = SQLITE_OK;

    *out_auto_increment = requested_auto_increment;
    if (column == NULL) {
        return MYLITE_OK;
    }

    sql = sqlite3_mprintf(
        "SELECT \"%w\" FROM \"%w\" WHERE \"%w\" IS NOT NULL",
        column->name,
        model->physical_name,
        column->name
    );
    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &select, NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    while ((rc = sqlite3_step(select)) == SQLITE_ROW) {
        uint64_t max_value = 0U;

        if (mylite_sqlite_column_uint64(select, 0, &max_value) &&
            max_value >= *out_auto_increment && max_value < UINT64_MAX) {
            *out_auto_increment = max_value + 1U;
        }
    }
    sqlite3_finalize(select);
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static const struct mylite_alter_table_column *find_alter_table_auto_increment_column(
    const struct mylite_alter_table_model *model
) {
    for (size_t index = 0U; index < model->column_count; ++index) {
        if (model->columns[index].auto_increment) {
            return &model->columns[index];
        }
    }
    return NULL;
}

static int refresh_alter_table_statistics(
    mylite_db *database,
    const struct mylite_alter_table_model *model
) {
    return mylite_catalog_refresh_table_statistics(
        database,
        model->schema_name,
        model->table_name,
        model->physical_name
    );
}

static sqlite3_destructor_type sqlite_transient_destructor(void) {
    // SQLite's public macro intentionally uses this sentinel pointer value.
    return SQLITE_TRANSIENT; // NOLINT(performance-no-int-to-ptr)
}
