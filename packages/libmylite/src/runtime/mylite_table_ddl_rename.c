#include "mylite_table_ddl.h"

#include "mylite_catalog.h"
#include "mylite_diagnostics.h"
#include "mylite_span.h"
#include "mylite_table_ddl_rename_validate.h"
#include "mylite_transactions.h"
#include "sqlite3.h"

#include <mylite/mylite.h>

#include <stdlib.h>
#include <string.h>

struct rename_table_foreign_key_constraint_rename {
    char *old_name;
    char *new_name;
};

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

static int read_rename_table_child_foreign_key_constraint_renames(
    mylite_db *database,
    const struct mylite_rename_table_target *target,
    struct rename_table_foreign_key_constraint_rename **out_renames,
    size_t *out_rename_count
);

static int append_rename_table_foreign_key_constraint_rename(
    mylite_db *database,
    struct rename_table_foreign_key_constraint_rename **renames,
    size_t *rename_count,
    const char *old_name,
    char *new_name
);

static int renamed_rename_table_foreign_key_constraint_name(
    mylite_db *database,
    const struct mylite_rename_table_target *target,
    const char *constraint_name,
    char **out_new_name
);

static bool rename_table_foreign_key_name_has_generated_suffix(
    const char *table_name,
    const char *constraint_name,
    const char **out_suffix
);

static int update_rename_table_child_foreign_key_catalog(
    mylite_db *database,
    const struct mylite_rename_table_target *target
);

static int update_rename_table_child_foreign_key_constraint_name(
    mylite_db *database,
    const struct mylite_rename_table_target *target,
    const struct rename_table_foreign_key_constraint_rename *rename
);

static void deinit_rename_table_foreign_key_constraint_renames(
    struct rename_table_foreign_key_constraint_rename *renames,
    size_t rename_count
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
    struct rename_table_foreign_key_constraint_rename *renames = NULL;
    size_t rename_count = 0U;
    int status = read_rename_table_child_foreign_key_constraint_renames(
        database,
        target,
        &renames,
        &rename_count
    );

    if (status == MYLITE_OK) {
        status = update_rename_table_child_foreign_key_catalog(database, target);
    }
    for (size_t index = 0U; status == MYLITE_OK && index < rename_count; ++index) {
        status = update_rename_table_child_foreign_key_constraint_name(
            database,
            target,
            &renames[index]
        );
    }

    deinit_rename_table_foreign_key_constraint_renames(renames, rename_count);
    return status;
}

static int read_rename_table_child_foreign_key_constraint_renames(
    mylite_db *database,
    const struct mylite_rename_table_target *target,
    struct rename_table_foreign_key_constraint_rename **out_renames,
    size_t *out_rename_count
) {
    static const char sql[] = "SELECT DISTINCT constraint_name "
                              "FROM __mylite_foreign_key_catalog "
                              "WHERE constraint_schema = ? AND table_name = ? "
                              "ORDER BY constraint_name";
    sqlite3_stmt *select = NULL;
    int rc =
        sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &select, NULL);
    int status = MYLITE_OK;

    *out_renames = NULL;
    *out_rename_count = 0U;
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    sqlite3_bind_text(select, 1, target->source_schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(select, 2, target->source_table_name, -1, sqlite_transient_destructor());

    while ((rc = sqlite3_step(select)) == SQLITE_ROW) {
        const char *old_name = (const char *)sqlite3_column_text(select, 0);
        char *new_name = NULL;

        status =
            renamed_rename_table_foreign_key_constraint_name(database, target, old_name, &new_name);
        if (status != MYLITE_OK) {
            break;
        }
        if (new_name == NULL) {
            continue;
        }
        status = append_rename_table_foreign_key_constraint_rename(
            database,
            out_renames,
            out_rename_count,
            old_name,
            new_name
        );
        if (status != MYLITE_OK) {
            sqlite3_free(new_name);
            break;
        }
    }
    sqlite3_finalize(select);
    if (status != MYLITE_OK) {
        deinit_rename_table_foreign_key_constraint_renames(*out_renames, *out_rename_count);
        *out_renames = NULL;
        *out_rename_count = 0U;
        return status;
    }
    if (rc != SQLITE_DONE) {
        deinit_rename_table_foreign_key_constraint_renames(*out_renames, *out_rename_count);
        *out_renames = NULL;
        *out_rename_count = 0U;
        return mylite_diagnostics_set_sqlite_error(database);
    }
    return MYLITE_OK;
}

static int append_rename_table_foreign_key_constraint_rename(
    mylite_db *database,
    struct rename_table_foreign_key_constraint_rename **renames,
    size_t *rename_count,
    const char *old_name,
    char *new_name
) {
    struct rename_table_foreign_key_constraint_rename *items =
        realloc(*renames, (*rename_count + 1U) * sizeof(**renames));
    char *old_name_copy = NULL;

    if (items == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    *renames = items;
    old_name_copy = mylite_copy_nonempty_cstring(old_name);
    if (old_name_copy == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    (*renames)[*rename_count] = (struct rename_table_foreign_key_constraint_rename){
        .old_name = old_name_copy,
        .new_name = new_name,
    };
    ++(*rename_count);
    return MYLITE_OK;
}

static int renamed_rename_table_foreign_key_constraint_name(
    mylite_db *database,
    const struct mylite_rename_table_target *target,
    const char *constraint_name,
    char **out_new_name
) {
    const char *suffix = NULL;

    *out_new_name = NULL;
    if (!rename_table_foreign_key_name_has_generated_suffix(
            target->source_table_name,
            constraint_name,
            &suffix
        )) {
        return MYLITE_OK;
    }
    *out_new_name = sqlite3_mprintf("%s_ibfk_%s", target->target_table_name, suffix);
    if (*out_new_name == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    return MYLITE_OK;
}

static bool rename_table_foreign_key_name_has_generated_suffix(
    const char *table_name,
    const char *constraint_name,
    const char **out_suffix
) {
    static const char marker[] = "_ibfk_";
    size_t table_length = strlen(table_name);
    size_t marker_length = strlen(marker);
    const char *suffix = constraint_name + table_length + marker_length;

    *out_suffix = NULL;
    if (strncmp(constraint_name, table_name, table_length) != 0 ||
        strncmp(constraint_name + table_length, marker, marker_length) != 0 || *suffix == '\0') {
        return false;
    }
    for (const char *cursor = suffix; *cursor != '\0'; ++cursor) {
        if (*cursor < '0' || *cursor > '9') {
            return false;
        }
    }
    *out_suffix = suffix;
    return true;
}

static int update_rename_table_child_foreign_key_catalog(
    mylite_db *database,
    const struct mylite_rename_table_target *target
) {
    enum {
        bind_target_constraint_schema = 1,
        bind_target_table_schema = 2,
        bind_target_table = 3,
        bind_source_constraint_schema = 4,
        bind_source_table = 5,
    };

    static const char sql[] = "UPDATE __mylite_foreign_key_catalog "
                              "SET constraint_schema = ?, table_schema = ?, table_name = ? "
                              "WHERE constraint_schema = ? AND table_name = ?";
    sqlite3_stmt *update = NULL;
    int rc =
        sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &update, NULL);

    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    sqlite3_bind_text(
        update,
        bind_target_constraint_schema,
        target->target_schema_name,
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_text(
        update,
        bind_target_table_schema,
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
        bind_source_constraint_schema,
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

static int update_rename_table_child_foreign_key_constraint_name(
    mylite_db *database,
    const struct mylite_rename_table_target *target,
    const struct rename_table_foreign_key_constraint_rename *rename
) {
    enum {
        bind_new_name = 1,
        bind_constraint_schema = 2,
        bind_table_name = 3,
        bind_old_name = 4,
    };

    static const char sql[] = "UPDATE __mylite_foreign_key_catalog "
                              "SET constraint_name = ? "
                              "WHERE constraint_schema = ? AND table_name = ? "
                              "AND constraint_name = ?";
    sqlite3_stmt *update = NULL;
    int rc =
        sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &update, NULL);

    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    sqlite3_bind_text(update, bind_new_name, rename->new_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(
        update,
        bind_constraint_schema,
        target->target_schema_name,
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_text(
        update,
        bind_table_name,
        target->target_table_name,
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_text(update, bind_old_name, rename->old_name, -1, sqlite_transient_destructor());
    rc = sqlite3_step(update);
    sqlite3_finalize(update);
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static void deinit_rename_table_foreign_key_constraint_renames(
    struct rename_table_foreign_key_constraint_rename *renames,
    size_t rename_count
) {
    for (size_t index = 0U; index < rename_count; ++index) {
        free(renames[index].old_name);
        sqlite3_free(renames[index].new_name);
    }
    free(renames);
}

static int rewrite_rename_table_parent_foreign_key_catalog(
    mylite_db *database,
    const struct mylite_rename_table_target *target
) {
    enum {
        bind_unique_constraint_schema = 1,
        bind_referenced_table_schema = 2,
        bind_referenced_table_name = 3,
        bind_source_schema = 4,
        bind_source_table = 5,
    };

    static const char sql[] =
        "UPDATE __mylite_foreign_key_catalog "
        "SET unique_constraint_schema = ?, referenced_table_schema = ?, referenced_table_name = ? "
        "WHERE referenced_table_schema = ? AND referenced_table_name = ?";
    sqlite3_stmt *update = NULL;
    int rc =
        sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &update, NULL);

    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    sqlite3_bind_text(
        update,
        bind_unique_constraint_schema,
        target->target_schema_name,
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_text(
        update,
        bind_referenced_table_schema,
        target->target_schema_name,
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_text(
        update,
        bind_referenced_table_name,
        target->target_table_name,
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

static sqlite3_destructor_type sqlite_transient_destructor(void) {
    // SQLite's public macro intentionally uses this sentinel pointer value.
    return SQLITE_TRANSIENT; // NOLINT(performance-no-int-to-ptr)
}
