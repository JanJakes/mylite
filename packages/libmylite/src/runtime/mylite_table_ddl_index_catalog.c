#include "mylite_table_ddl_index_catalog.h"

#include "mylite_diagnostics.h"
#include "mylite_transactions.h"
#include "sqlite3.h"

static int insert_standalone_index_catalog_rows(
    mylite_db *database,
    const struct mylite_alter_table_model *model,
    const struct mylite_alter_table_index *index
);

static int insert_standalone_index_catalog_part(
    mylite_db *database,
    sqlite3_stmt *insert,
    const struct mylite_alter_table_model *model,
    const struct mylite_alter_table_index *index,
    const struct mylite_alter_table_index_part *part,
    size_t part_index
);

static int delete_index_catalog_rows(mylite_db *database, const struct mylite_index_ddl_plan *plan);

static sqlite3_destructor_type sqlite_transient_destructor(void);

int mylite_table_ddl_create_index_catalog_transaction(
    mylite_db *database,
    const struct mylite_alter_table_model *model,
    const struct mylite_alter_table_index *index
) {
    int status = mylite_transaction_begin_storage(database);

    if (status != MYLITE_OK) {
        return status;
    }

    status = insert_standalone_index_catalog_rows(database, model, index);
    if (status == MYLITE_OK) {
        status = mylite_transaction_commit_storage(database);
        if (status == MYLITE_OK) {
            return MYLITE_OK;
        }
    }

    mylite_transaction_rollback_storage(database);
    return status;
}

int mylite_table_ddl_drop_index_catalog_transaction(
    mylite_db *database,
    const struct mylite_index_ddl_plan *plan
) {
    int status = mylite_transaction_begin_storage(database);

    if (status != MYLITE_OK) {
        return status;
    }

    status = delete_index_catalog_rows(database, plan);
    if (status == MYLITE_OK) {
        status = mylite_transaction_commit_storage(database);
        if (status == MYLITE_OK) {
            return MYLITE_OK;
        }
    }

    mylite_transaction_rollback_storage(database);
    return status;
}

static int insert_standalone_index_catalog_rows(
    mylite_db *database,
    const struct mylite_alter_table_model *model,
    const struct mylite_alter_table_index *index
) {
    sqlite3_stmt *insert = NULL;
    static const char sql[] =
        "INSERT INTO __mylite_index_catalog("
        "table_catalog, table_schema, table_name, non_unique, index_schema, index_name, "
        "seq_in_index, column_name, collation, cardinality, sub_part, packed, nullable, "
        "index_type, comment, index_comment, is_visible, expression)"
        " VALUES('def', ?, ?, ?, ?, ?, ?, ?, ?, NULL, ?, NULL, ?, ?, '', ?, ?, NULL)";
    int rc =
        sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &insert, NULL);

    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    for (size_t part = 0U; part < index->part_count; ++part) {
        int status = insert_standalone_index_catalog_part(
            database,
            insert,
            model,
            index,
            &index->parts[part],
            part
        );

        if (status != MYLITE_OK) {
            sqlite3_finalize(insert);
            return status;
        }
    }

    sqlite3_finalize(insert);
    return MYLITE_OK;
}

static int insert_standalone_index_catalog_part(
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
        bind_index_comment = 12,
        bind_is_visible = 13,
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
    sqlite3_bind_text(insert, bind_collation, part->collation, -1, sqlite_transient_destructor());
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

static int delete_index_catalog_rows(
    mylite_db *database,
    const struct mylite_index_ddl_plan *plan
) {
    sqlite3_stmt *delete_stmt = NULL;
    static const char sql[] = "DELETE FROM __mylite_index_catalog "
                              "WHERE table_schema = ? AND table_name = ? AND index_name = ?";
    int rc = sqlite3_prepare_v3(
        database->sqlite,
        sql,
        -1,
        SQLITE_PREPARE_PERSISTENT,
        &delete_stmt,
        NULL
    );

    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    sqlite3_bind_text(delete_stmt, 1, plan->schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(delete_stmt, 2, plan->table_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(delete_stmt, 3, plan->index_name, -1, sqlite_transient_destructor());
    rc = sqlite3_step(delete_stmt);
    sqlite3_finalize(delete_stmt);
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static sqlite3_destructor_type sqlite_transient_destructor(void) {
    return (sqlite3_destructor_type)SQLITE_TRANSIENT; // NOLINT(performance-no-int-to-ptr)
}
