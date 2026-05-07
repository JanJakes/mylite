#include "mylite_table_ddl.h"

#include "mylite_catalog.h"
#include "mylite_diagnostics.h"
#include "mylite_foreign_key_catalog.h"
#include "mylite_table_ddl_rename_validate.h"
#include "mylite_transactions.h"
#include "sqlite3.h"

#include <mylite/mylite.h>

#include <stdlib.h>

static int rename_table_transaction(
    mylite_db *database,
    const struct mylite_rename_table_plan *plan
);

static int rename_table_target(
    mylite_db *database,
    const struct mylite_rename_table_target *target
);

static int rename_physical_table(
    mylite_db *database,
    const struct mylite_rename_table_target *target
);

static int rewrite_rename_table_catalog(
    mylite_db *database,
    const struct mylite_rename_table_target *target
);

static int rewrite_rename_table_catalog_row(
    mylite_db *database,
    const char *sql,
    const struct mylite_rename_table_target *target
);

static int rewrite_rename_table_index_catalog(
    mylite_db *database,
    const struct mylite_rename_table_target *target
);

static int rewrite_rename_table_foreign_key_catalog(
    mylite_db *database,
    const struct mylite_rename_table_target *target
);

static int rewrite_rename_table_child_foreign_key_catalog(
    mylite_db *database,
    const struct mylite_rename_table_target *target
);

static int rewrite_rename_table_parent_foreign_key_catalog(
    mylite_db *database,
    const struct mylite_rename_table_target *target
);

static sqlite3_destructor_type sqlite_transient_destructor(void);

int mylite_table_ddl_execute_rename_table_statement(
    mylite_db *database,
    const char *selected_schema,
    struct mylite_rename_table_plan *plan
) {
    int status = mylite_table_ddl_validate_rename_table_plan(database, selected_schema, plan);

    if (status == MYLITE_OK) {
        status = rename_table_transaction(database, plan);
    }
    return status;
}

static int rename_table_transaction(
    mylite_db *database,
    const struct mylite_rename_table_plan *plan
) {
    struct mylite_statement_atomicity atomicity = {0};
    int status = mylite_transaction_begin_statement_atomicity(database, &atomicity);

    for (size_t index = 0U; status == MYLITE_OK && index < plan->target_count; ++index) {
        status = rename_table_target(database, &plan->targets[index]);
    }
    if (status == MYLITE_OK) {
        status = mylite_transaction_commit_statement_atomicity(database, &atomicity);
        if (status == MYLITE_OK) {
            return MYLITE_OK;
        }
    }

    mylite_transaction_rollback_statement_atomicity(database, &atomicity);
    return status;
}

static int rename_table_target(
    mylite_db *database,
    const struct mylite_rename_table_target *target
) {
    int status = rename_physical_table(database, target);

    if (status == MYLITE_OK) {
        status = rewrite_rename_table_catalog(database, target);
    }
    return status;
}

static int rename_physical_table(
    mylite_db *database,
    const struct mylite_rename_table_target *target
) {
    char *source_physical_name =
        mylite_catalog_physical_table_name(target->source_schema_name, target->source_table_name);
    char *target_physical_name =
        mylite_catalog_physical_table_name(target->target_schema_name, target->target_table_name);
    char *sql = NULL;
    int rc = SQLITE_OK;

    if (source_physical_name == NULL || target_physical_name == NULL) {
        free(source_physical_name);
        free(target_physical_name);
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    sql = sqlite3_mprintf(
        "ALTER TABLE \"%w\" RENAME TO \"%w\"",
        source_physical_name,
        target_physical_name
    );
    free(source_physical_name);
    free(target_physical_name);
    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    rc = sqlite3_exec(database->sqlite, sql, NULL, NULL, NULL);
    sqlite3_free(sql);
    return rc == SQLITE_OK ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static int rewrite_rename_table_catalog(
    mylite_db *database,
    const struct mylite_rename_table_target *target
) {
    static const char update_tables[] =
        "UPDATE __mylite_table_catalog SET table_schema = ?, table_name = ? "
        "WHERE table_schema = ? AND table_name = ?";
    static const char update_columns[] =
        "UPDATE __mylite_column_catalog SET table_schema = ?, table_name = ? "
        "WHERE table_schema = ? AND table_name = ?";
    int status = rewrite_rename_table_catalog_row(database, update_tables, target);

    if (status == MYLITE_OK) {
        status = rewrite_rename_table_catalog_row(database, update_columns, target);
    }
    if (status == MYLITE_OK) {
        status = rewrite_rename_table_index_catalog(database, target);
    }
    if (status == MYLITE_OK) {
        status = rewrite_rename_table_foreign_key_catalog(database, target);
    }
    return status;
}

static int rewrite_rename_table_catalog_row(
    mylite_db *database,
    const char *sql,
    const struct mylite_rename_table_target *target
) {
    sqlite3_stmt *update = NULL;
    int rc =
        sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &update, NULL);

    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    sqlite3_bind_text(update, 1, target->target_schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(update, 2, target->target_table_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(update, 3, target->source_schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(update, 4, target->source_table_name, -1, sqlite_transient_destructor());
    rc = sqlite3_step(update);
    sqlite3_finalize(update);
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static int rewrite_rename_table_index_catalog(
    mylite_db *database,
    const struct mylite_rename_table_target *target
) {
    enum {
        bind_target_schema = 1,
        bind_target_table = 2,
        bind_target_index_schema = 3,
        bind_source_schema = 4,
        bind_source_table = 5,
    };

    sqlite3_stmt *update = NULL;
    static const char sql[] = "UPDATE __mylite_index_catalog "
                              "SET table_schema = ?, table_name = ?, index_schema = ? "
                              "WHERE table_schema = ? AND table_name = ?";
    int rc =
        sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &update, NULL);

    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    sqlite3_bind_text(
        update,
        bind_target_schema,
        target->target_schema_name,
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_text(
        update,
        bind_target_table,
        target->target_table_name,
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_text(
        update,
        bind_target_index_schema,
        target->target_schema_name,
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_text(
        update,
        bind_source_schema,
        target->source_schema_name,
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_text(
        update,
        bind_source_table,
        target->source_table_name,
        -1,
        sqlite_transient_destructor()
    );
    rc = sqlite3_step(update);
    sqlite3_finalize(update);
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static int rewrite_rename_table_foreign_key_catalog(
    mylite_db *database,
    const struct mylite_rename_table_target *target
) {
    int status = rewrite_rename_table_child_foreign_key_catalog(database, target);

    if (status == MYLITE_OK) {
        status = rewrite_rename_table_parent_foreign_key_catalog(database, target);
    }
    return status;
}

static int rewrite_rename_table_child_foreign_key_catalog(
    mylite_db *database,
    const struct mylite_rename_table_target *target
) {
    return mylite_foreign_key_catalog_rewrite_child_table(
        database,
        target->source_schema_name,
        target->source_table_name,
        target->target_schema_name,
        target->target_table_name
    );
}

static int rewrite_rename_table_parent_foreign_key_catalog(
    mylite_db *database,
    const struct mylite_rename_table_target *target
) {
    return mylite_foreign_key_catalog_rewrite_parent_table(
        database,
        target->source_schema_name,
        target->source_table_name,
        target->target_schema_name,
        target->target_table_name
    );
}

static sqlite3_destructor_type sqlite_transient_destructor(void) {
    // SQLite's public macro intentionally uses this sentinel pointer value.
    return SQLITE_TRANSIENT; // NOLINT(performance-no-int-to-ptr)
}
