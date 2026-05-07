#include "mylite_foreign_key_catalog.h"

#include "mylite_catalog.h"
#include "mylite_diagnostics.h"
#include "mylite_span.h"
#include "sqlite3.h"

#include <stdlib.h>
#include <string.h>

struct mylite_foreign_key_constraint_rename {
    char *old_name;
    char *new_name;
};

struct mylite_foreign_key_table_rewrite {
    const char *source_schema_name;
    const char *source_table_name;
    const char *target_schema_name;
    const char *target_table_name;
};

static int update_table_catalog_rows(
    mylite_db *database,
    const char *sql,
    const struct mylite_foreign_key_table_rewrite *rewrite
);

static int read_child_constraint_renames(
    mylite_db *database,
    const struct mylite_foreign_key_table_rewrite *rewrite,
    struct mylite_foreign_key_constraint_rename **out_renames,
    size_t *out_rename_count
);

static int append_constraint_rename(
    mylite_db *database,
    struct mylite_foreign_key_constraint_rename **renames,
    size_t *rename_count,
    const char *old_name,
    // Ownership is transferred into the rename array and released with sqlite3_free().
    // NOLINTNEXTLINE(readability-non-const-parameter)
    char *new_name
);

static int renamed_generated_constraint_name(
    mylite_db *database,
    const struct mylite_foreign_key_table_rewrite *rewrite,
    const char *constraint_name,
    char **out_new_name
);

static bool foreign_key_name_has_generated_suffix(
    const char *table_name,
    const char *constraint_name,
    const char **out_suffix
);

static int update_child_constraint_name(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    const struct mylite_foreign_key_constraint_rename *rename
);

static void deinit_constraint_renames(
    struct mylite_foreign_key_constraint_rename *renames,
    size_t rename_count
);

static sqlite3_destructor_type sqlite_transient_destructor(void);

int mylite_foreign_key_catalog_child_constraint_exists(
    mylite_db *database,
    const char *schema_name,
    const char *constraint_name,
    bool *out_exists
) {
    static const char sql[] =
        "SELECT 1 FROM __mylite_foreign_key_catalog "
        "WHERE constraint_schema = ? AND constraint_name = ? COLLATE NOCASE LIMIT 1";
    sqlite3_stmt *select = NULL;
    int rc = SQLITE_OK;

    *out_exists = false;
    rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &select, NULL);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    sqlite3_bind_text(select, 1, schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(select, 2, constraint_name, -1, sqlite_transient_destructor());
    rc = sqlite3_step(select);
    if (rc == SQLITE_ROW) {
        *out_exists = true;
    } else if (rc != SQLITE_DONE) {
        sqlite3_finalize(select);
        return mylite_diagnostics_set_sqlite_error(database);
    }
    sqlite3_finalize(select);
    return MYLITE_OK;
}

int mylite_foreign_key_catalog_delete_child_constraint(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    const char *constraint_name,
    bool temporary
) {
    const char *catalog_name = mylite_catalog_foreign_key_catalog_name(temporary);
    sqlite3_stmt *delete_stmt = NULL;
    char *sql = sqlite3_mprintf(
        "DELETE FROM \"%w\" "
        "WHERE constraint_schema = ? AND table_schema = ? AND table_name = ? "
        "AND constraint_name = ? COLLATE NOCASE",
        catalog_name
    );
    int rc = SQLITE_OK;

    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    rc = sqlite3_prepare_v3(
        database->sqlite,
        sql,
        -1,
        SQLITE_PREPARE_PERSISTENT,
        &delete_stmt,
        NULL
    );
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    sqlite3_bind_text(delete_stmt, 1, schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(delete_stmt, 2, schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(delete_stmt, 3, table_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(delete_stmt, 4, constraint_name, -1, sqlite_transient_destructor());
    rc = sqlite3_step(delete_stmt);
    sqlite3_finalize(delete_stmt);
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

int mylite_foreign_key_catalog_index_dependency_exists(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    const char *index_name,
    bool temporary,
    bool *out_has_dependency
) {
    enum {
        bind_child_schema = 1,
        bind_child_table = 2,
        bind_supporting_index = 3,
        bind_parent_constraint_schema = 4,
        bind_parent_schema = 5,
        bind_parent_table = 6,
        bind_parent_unique_constraint = 7,
    };

    const char *catalog_name = mylite_catalog_foreign_key_catalog_name(temporary);
    sqlite3_stmt *select = NULL;
    char *sql = sqlite3_mprintf(
        "SELECT 1 FROM \"%w\" "
        "WHERE (constraint_schema = ? COLLATE NOCASE "
        "AND table_name = ? COLLATE NOCASE "
        "AND supporting_index_name = ? COLLATE NOCASE) "
        "OR (unique_constraint_schema = ? COLLATE NOCASE "
        "AND referenced_table_schema = ? COLLATE NOCASE "
        "AND referenced_table_name = ? COLLATE NOCASE "
        "AND unique_constraint_name = ? COLLATE NOCASE) "
        "LIMIT 1",
        catalog_name
    );
    int rc = SQLITE_OK;

    *out_has_dependency = false;
    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &select, NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    sqlite3_bind_text(select, bind_child_schema, schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(select, bind_child_table, table_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(select, bind_supporting_index, index_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(
        select,
        bind_parent_constraint_schema,
        schema_name,
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_text(select, bind_parent_schema, schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(select, bind_parent_table, table_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(
        select,
        bind_parent_unique_constraint,
        index_name,
        -1,
        sqlite_transient_destructor()
    );

    rc = sqlite3_step(select);
    sqlite3_finalize(select);
    if (rc == SQLITE_ROW) {
        *out_has_dependency = true;
        return MYLITE_OK;
    }
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

int mylite_foreign_key_catalog_rewrite_child_table(
    mylite_db *database,
    const char *source_schema_name,
    const char *source_table_name,
    const char *target_schema_name,
    const char *target_table_name
) {
    static const char update_child_table[] =
        "UPDATE __mylite_foreign_key_catalog "
        "SET constraint_schema = ?, table_schema = ?, table_name = ? "
        "WHERE constraint_schema = ? AND table_name = ?";
    const struct mylite_foreign_key_table_rewrite rewrite = {
        .source_schema_name = source_schema_name,
        .source_table_name = source_table_name,
        .target_schema_name = target_schema_name,
        .target_table_name = target_table_name,
    };
    struct mylite_foreign_key_constraint_rename *renames = NULL;
    size_t rename_count = 0U;
    int status = read_child_constraint_renames(database, &rewrite, &renames, &rename_count);

    if (status == MYLITE_OK) {
        status = update_table_catalog_rows(database, update_child_table, &rewrite);
    }
    for (size_t index = 0U; status == MYLITE_OK && index < rename_count; ++index) {
        status = update_child_constraint_name(
            database,
            target_schema_name,
            target_table_name,
            &renames[index]
        );
    }

    deinit_constraint_renames(renames, rename_count);
    return status;
}

int mylite_foreign_key_catalog_rewrite_parent_table(
    mylite_db *database,
    const char *source_schema_name,
    const char *source_table_name,
    const char *target_schema_name,
    const char *target_table_name
) {
    static const char update_parent_table[] =
        "UPDATE __mylite_foreign_key_catalog "
        "SET unique_constraint_schema = ?, referenced_table_schema = ?, referenced_table_name = ? "
        "WHERE referenced_table_schema = ? AND referenced_table_name = ?";
    const struct mylite_foreign_key_table_rewrite rewrite = {
        .source_schema_name = source_schema_name,
        .source_table_name = source_table_name,
        .target_schema_name = target_schema_name,
        .target_table_name = target_table_name,
    };

    return update_table_catalog_rows(database, update_parent_table, &rewrite);
}

int mylite_foreign_key_catalog_rewrite_child_column(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    const char *source_column_name,
    const char *target_column_name
) {
    static const char sql[] = "UPDATE __mylite_foreign_key_catalog SET column_name = ? "
                              "WHERE table_schema = ? AND table_name = ? "
                              "AND column_name = ? COLLATE NOCASE";
    sqlite3_stmt *update = NULL;
    int rc =
        sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &update, NULL);

    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    sqlite3_bind_text(update, 1, target_column_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(update, 2, schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(update, 3, table_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(update, 4, source_column_name, -1, sqlite_transient_destructor());
    rc = sqlite3_step(update);
    sqlite3_finalize(update);
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

int mylite_foreign_key_catalog_rewrite_parent_column(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    const char *source_column_name,
    const char *target_column_name
) {
    static const char sql[] = "UPDATE __mylite_foreign_key_catalog "
                              "SET referenced_column_name = ? "
                              "WHERE referenced_table_schema = ? "
                              "AND referenced_table_name = ? "
                              "AND referenced_column_name = ? COLLATE NOCASE";
    sqlite3_stmt *update = NULL;
    int rc =
        sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &update, NULL);

    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    sqlite3_bind_text(update, 1, target_column_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(update, 2, schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(update, 3, table_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(update, 4, source_column_name, -1, sqlite_transient_destructor());
    rc = sqlite3_step(update);
    sqlite3_finalize(update);
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static int update_table_catalog_rows(
    mylite_db *database,
    const char *sql,
    const struct mylite_foreign_key_table_rewrite *rewrite
) {
    enum {
        bind_target_schema = 1,
        bind_target_schema_again = 2,
        bind_target_table = 3,
        bind_source_schema = 4,
        bind_source_table = 5,
    };

    sqlite3_stmt *update = NULL;
    int rc =
        sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &update, NULL);

    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    sqlite3_bind_text(
        update,
        bind_target_schema,
        rewrite->target_schema_name,
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_text(
        update,
        bind_target_schema_again,
        rewrite->target_schema_name,
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_text(
        update,
        bind_target_table,
        rewrite->target_table_name,
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_text(
        update,
        bind_source_schema,
        rewrite->source_schema_name,
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_text(
        update,
        bind_source_table,
        rewrite->source_table_name,
        -1,
        sqlite_transient_destructor()
    );
    rc = sqlite3_step(update);
    sqlite3_finalize(update);
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static int read_child_constraint_renames(
    mylite_db *database,
    const struct mylite_foreign_key_table_rewrite *rewrite,
    struct mylite_foreign_key_constraint_rename **out_renames,
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
    sqlite3_bind_text(select, 1, rewrite->source_schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(select, 2, rewrite->source_table_name, -1, sqlite_transient_destructor());

    while ((rc = sqlite3_step(select)) == SQLITE_ROW) {
        const char *old_name = (const char *)sqlite3_column_text(select, 0);
        char *new_name = NULL;

        status = renamed_generated_constraint_name(database, rewrite, old_name, &new_name);
        if (status != MYLITE_OK) {
            break;
        }
        if (new_name == NULL) {
            continue;
        }
        status =
            append_constraint_rename(database, out_renames, out_rename_count, old_name, new_name);
        if (status != MYLITE_OK) {
            sqlite3_free(new_name);
            break;
        }
    }
    sqlite3_finalize(select);
    if (status != MYLITE_OK) {
        deinit_constraint_renames(*out_renames, *out_rename_count);
        *out_renames = NULL;
        *out_rename_count = 0U;
        return status;
    }
    if (rc != SQLITE_DONE) {
        deinit_constraint_renames(*out_renames, *out_rename_count);
        *out_renames = NULL;
        *out_rename_count = 0U;
        return mylite_diagnostics_set_sqlite_error(database);
    }
    return MYLITE_OK;
}

static int append_constraint_rename(
    mylite_db *database,
    struct mylite_foreign_key_constraint_rename **renames,
    size_t *rename_count,
    const char *old_name,
    // Ownership is transferred into the rename array and released with sqlite3_free().
    // NOLINTNEXTLINE(readability-non-const-parameter)
    char *new_name
) {
    struct mylite_foreign_key_constraint_rename *items =
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

    (*renames)[*rename_count] = (struct mylite_foreign_key_constraint_rename){
        .old_name = old_name_copy,
        .new_name = new_name,
    };
    ++(*rename_count);
    return MYLITE_OK;
}

static int renamed_generated_constraint_name(
    mylite_db *database,
    const struct mylite_foreign_key_table_rewrite *rewrite,
    const char *constraint_name,
    char **out_new_name
) {
    const char *suffix = NULL;

    *out_new_name = NULL;
    if (!foreign_key_name_has_generated_suffix(
            rewrite->source_table_name,
            constraint_name,
            &suffix
        )) {
        return MYLITE_OK;
    }
    *out_new_name = sqlite3_mprintf("%s_ibfk_%s", rewrite->target_table_name, suffix);
    if (*out_new_name == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    return MYLITE_OK;
}

static bool foreign_key_name_has_generated_suffix(
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

static int update_child_constraint_name(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    const struct mylite_foreign_key_constraint_rename *rename
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
        schema_name,
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_text(update, bind_table_name, table_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(update, bind_old_name, rename->old_name, -1, sqlite_transient_destructor());
    rc = sqlite3_step(update);
    sqlite3_finalize(update);
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static void deinit_constraint_renames(
    struct mylite_foreign_key_constraint_rename *renames,
    size_t rename_count
) {
    for (size_t index = 0U; index < rename_count; ++index) {
        free(renames[index].old_name);
        sqlite3_free(renames[index].new_name);
    }
    free(renames);
}

static sqlite3_destructor_type sqlite_transient_destructor(void) {
    // SQLite's public macro intentionally uses this sentinel pointer value.
    return SQLITE_TRANSIENT; // NOLINT(performance-no-int-to-ptr)
}
