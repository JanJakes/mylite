#include "mylite_table_ddl_index_validate.h"

#include "mylite_catalog.h"
#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "mylite_span.h"
#include "mylite_table_ddl_alter.h"
#include "mylite_table_ddl_alter_index_model.h"
#include "mylite_table_ddl_alter_model.h"
#include "sqlite3.h"

#include <stdlib.h>
#include <string.h>

static int resolve_index_ddl_schema(
    mylite_db *database,
    const char *selected_schema,
    struct mylite_index_ddl_plan *plan
);

static int validate_index_ddl_target(mylite_db *database, const struct mylite_index_ddl_plan *plan);

static int set_duplicate_key_name_error(mylite_db *database, const char *index_name);

static int set_drop_index_missing_error(mylite_db *database, const char *index_name);

static int index_has_foreign_key_dependency(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    const char *index_name,
    bool temporary,
    bool *out_has_dependency
);

static int set_drop_index_foreign_key_dependency_error(mylite_db *database, const char *index_name);

static bool drop_index_primary_has_auto_increment_column(
    const struct mylite_alter_table_model *model
);

static char *build_create_unique_index_duplicate_sql(
    mylite_db *database,
    const struct mylite_alter_table_model *model,
    const struct mylite_create_table_index *index
);

static const char *alter_table_column_physical_name(const struct mylite_alter_table_column *column);

int mylite_table_ddl_validate_create_index_plan(
    mylite_db *database,
    const char *selected_schema,
    struct mylite_index_ddl_plan *plan,
    struct mylite_alter_table_model *model
) {
    bool temporary = false;
    int status = resolve_index_ddl_schema(database, selected_schema, plan);

    if (status != MYLITE_OK) {
        return status;
    }
    status = validate_index_ddl_target(database, plan);
    if (status != MYLITE_OK) {
        return status;
    }
    status = mylite_catalog_temporary_table_exists(
        database,
        plan->schema_name,
        plan->table_name,
        &temporary
    );
    if (status != MYLITE_OK) {
        return status;
    }
    status = mylite_table_ddl_load_alter_table_model(
        database,
        plan->schema_name,
        plan->table_name,
        temporary,
        model
    );
    if (status != MYLITE_OK) {
        return status;
    }
    if (mylite_table_ddl_alter_table_index_index(model, plan->index.name) < model->index_count) {
        return set_duplicate_key_name_error(database, plan->index.name);
    }
    return MYLITE_OK;
}

int mylite_table_ddl_validate_drop_index_plan(
    mylite_db *database,
    const char *selected_schema,
    struct mylite_index_ddl_plan *plan,
    struct mylite_alter_table_model *model
) {
    char *canonical_name = NULL;
    size_t index = 0U;
    bool temporary = false;
    int status = resolve_index_ddl_schema(database, selected_schema, plan);

    if (status != MYLITE_OK) {
        return status;
    }
    status = validate_index_ddl_target(database, plan);
    if (status != MYLITE_OK) {
        return status;
    }
    status = mylite_catalog_temporary_table_exists(
        database,
        plan->schema_name,
        plan->table_name,
        &temporary
    );
    if (status != MYLITE_OK) {
        return status;
    }
    status = mylite_table_ddl_load_alter_table_model(
        database,
        plan->schema_name,
        plan->table_name,
        temporary,
        model
    );
    if (status != MYLITE_OK) {
        return status;
    }

    index = mylite_table_ddl_alter_table_index_index(model, plan->index_name);
    if (index == model->index_count) {
        return set_drop_index_missing_error(database, plan->index_name);
    }

    canonical_name = mylite_copy_nonempty_cstring(model->indexes[index].name);
    if (canonical_name == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    free(plan->index_name);
    plan->index_name = canonical_name;
    if (mylite_ascii_case_equal(plan->index_name, "PRIMARY") &&
        drop_index_primary_has_auto_increment_column(model)) {
        return mylite_table_ddl_set_alter_table_wrong_auto_increment_error(database);
    }
    return mylite_table_ddl_validate_index_foreign_key_dependencies(
        database,
        plan->schema_name,
        plan->table_name,
        plan->index_name,
        temporary
    );
}

int mylite_table_ddl_validate_index_foreign_key_dependencies(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    const char *index_name,
    bool temporary
) {
    bool has_dependency = false;
    int status = index_has_foreign_key_dependency(
        database,
        schema_name,
        table_name,
        index_name,
        temporary,
        &has_dependency
    );

    if (status != MYLITE_OK) {
        return status;
    }
    if (has_dependency) {
        return set_drop_index_foreign_key_dependency_error(database, index_name);
    }
    return MYLITE_OK;
}

int mylite_table_ddl_validate_create_index_columns(
    mylite_db *database,
    const struct mylite_alter_table_model *model,
    const struct mylite_create_table_index *index
) {
    for (size_t part = 0U; part < index->part_count; ++part) {
        const char *column_name = index->parts[part].column_name;

        if (mylite_table_ddl_alter_table_column_index(model, column_name) == model->column_count) {
            (void)mylite_diagnostics_set_error_message_parts(
                database,
                "Key column '",
                column_name,
                "' doesn't exist in table"
            );
            return MYLITE_EXEC_ERROR;
        }
    }
    return MYLITE_OK;
}

int mylite_table_ddl_validate_create_index_supported_features(
    mylite_db *database,
    const struct mylite_index_ddl_plan *plan
) {
    if (plan->index_class == MYLITE_SQL_AST_INDEX_CLASS_FULLTEXT ||
        plan->index_class == MYLITE_SQL_AST_INDEX_CLASS_SPATIAL) {
        (void)mylite_diagnostics_set_error_message(database, "Unsupported standalone index class");
        return MYLITE_UNSUPPORTED;
    }
    if (plan->index.has_with_parser) {
        (void)mylite_diagnostics_set_error_message(
            database,
            "WITH PARSER is only supported for FULLTEXT indexes"
        );
        return MYLITE_EXEC_ERROR;
    }
    if (plan->index.has_engine_attribute) {
        (void)mylite_diagnostics_set_error_message(
            database,
            "Storage engine 'InnoDB' does not support ENGINE_ATTRIBUTE"
        );
        return MYLITE_EXEC_ERROR;
    }
    return MYLITE_OK;
}

int mylite_table_ddl_validate_create_unique_index_values(
    mylite_db *database,
    const struct mylite_alter_table_model *model,
    const struct mylite_create_table_index *index
) {
    char *sql = build_create_unique_index_duplicate_sql(database, model, index);
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
        (void)mylite_diagnostics_set_error_message_parts(
            database,
            "Duplicate entry for key '",
            index->name,
            "'"
        );
        return MYLITE_EXEC_ERROR;
    }
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static int resolve_index_ddl_schema(
    mylite_db *database,
    const char *selected_schema,
    struct mylite_index_ddl_plan *plan
) {
    if (plan->schema_name != NULL) {
        return MYLITE_OK;
    }
    if (selected_schema == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "No database selected");
        return MYLITE_EXEC_ERROR;
    }

    plan->schema_name = mylite_copy_span_text(selected_schema, strlen(selected_schema));
    if (plan->schema_name == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    return MYLITE_OK;
}

static int validate_index_ddl_target(
    mylite_db *database,
    const struct mylite_index_ddl_plan *plan
) {
    struct mylite_schema_presence presence = {false};
    bool exists = false;
    int status = mylite_catalog_schema_exists(database, plan->schema_name, &presence);

    if (status != MYLITE_OK) {
        return status;
    }
    if (!presence.exists) {
        (void)mylite_diagnostics_set_error_message_parts(
            database,
            "Unknown database '",
            plan->schema_name,
            "'"
        );
        return MYLITE_EXEC_ERROR;
    }
    if (presence.is_system) {
        return mylite_diagnostics_set_schema_access_denied_error(database, plan->schema_name);
    }

    status = mylite_catalog_table_exists(database, plan->schema_name, plan->table_name, &exists);
    if (status != MYLITE_OK) {
        return status;
    }
    if (!exists) {
        return mylite_diagnostics_set_table_doesnt_exist_error(
            database,
            plan->schema_name,
            plan->table_name
        );
    }
    return MYLITE_OK;
}

static int set_duplicate_key_name_error(mylite_db *database, const char *index_name) {
    (void)mylite_diagnostics_set_error_message_parts(
        database,
        "Duplicate key name '",
        index_name,
        "'"
    );
    return MYLITE_EXEC_ERROR;
}

static int set_drop_index_missing_error(mylite_db *database, const char *index_name) {
    (void)mylite_diagnostics_set_error_message_parts(
        database,
        "Can't DROP '",
        index_name,
        "'; check that column/key exists"
    );
    return MYLITE_EXEC_ERROR;
}

static int index_has_foreign_key_dependency(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    const char *index_name,
    bool temporary,
    bool *out_has_dependency
) {
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
    sqlite3_bind_text(select, 1, schema_name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(select, 2, table_name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(select, 3, index_name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(select, 4, schema_name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(select, 5, schema_name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(select, 6, table_name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(select, 7, index_name, -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(select);
    sqlite3_finalize(select);
    if (rc == SQLITE_ROW) {
        *out_has_dependency = true;
        return MYLITE_OK;
    }
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static int set_drop_index_foreign_key_dependency_error(
    mylite_db *database,
    const char *index_name
) {
    char *message =
        sqlite3_mprintf("Cannot drop index '%q': needed in a foreign key constraint", index_name);
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_set_error_message(database, message);
    sqlite3_free(message);
    if (status == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_append_error(
        database,
        MYLITE_MYSQL_ER_DROP_INDEX_FK,
        mylite_error_message(database)
    );
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static bool drop_index_primary_has_auto_increment_column(
    const struct mylite_alter_table_model *model
) {
    size_t primary_index = mylite_table_ddl_alter_table_index_index(model, "PRIMARY");

    if (primary_index == model->index_count) {
        return false;
    }
    for (size_t part = 0U; part < model->indexes[primary_index].part_count; ++part) {
        const struct mylite_alter_table_column *column = mylite_table_ddl_find_alter_table_column(
            model,
            model->indexes[primary_index].parts[part].column_name
        );

        if (column != NULL && column->auto_increment) {
            return true;
        }
    }
    return false;
}

static char *build_create_unique_index_duplicate_sql(
    mylite_db *database,
    const struct mylite_alter_table_model *model,
    const struct mylite_create_table_index *index
) {
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);

    if (sql == NULL) {
        return NULL;
    }

    sqlite3_str_append(sql, "SELECT 1 FROM (SELECT ", (int)strlen("SELECT 1 FROM (SELECT "));
    for (size_t part = 0U; part < index->part_count; ++part) {
        const struct mylite_create_table_key_part *key_part = &index->parts[part];
        size_t column_index =
            mylite_table_ddl_alter_table_column_index(model, key_part->column_name);
        const char *column_name = alter_table_column_physical_name(&model->columns[column_index]);

        if (part != 0U) {
            sqlite3_str_append(sql, ",", 1);
        }
        if (key_part->has_prefix_length) {
            sqlite3_str_appendf(
                sql,
                "substr(\"%w\",1,%llu)",
                column_name,
                (unsigned long long)key_part->prefix_length
            );
        } else {
            sqlite3_str_appendf(sql, "\"%w\"", column_name);
        }
    }
    sqlite3_str_appendf(sql, " FROM \"%w\" WHERE ", model->physical_name);
    for (size_t part = 0U; part < index->part_count; ++part) {
        size_t column_index =
            mylite_table_ddl_alter_table_column_index(model, index->parts[part].column_name);
        const char *column_name = alter_table_column_physical_name(&model->columns[column_index]);

        if (part != 0U) {
            sqlite3_str_append(sql, " AND ", (int)strlen(" AND "));
        }
        sqlite3_str_appendf(sql, "\"%w\" IS NOT NULL", column_name);
    }
    sqlite3_str_append(sql, " GROUP BY ", (int)strlen(" GROUP BY "));
    for (size_t part = 0U; part < index->part_count; ++part) {
        if (part != 0U) {
            sqlite3_str_append(sql, ",", 1);
        }
        sqlite3_str_appendf(sql, "%d", (int)(part + 1U));
    }
    sqlite3_str_append(
        sql,
        " HAVING COUNT(*) > 1) LIMIT 1",
        (int)strlen(" HAVING COUNT(*) > 1) LIMIT 1")
    );
    return sqlite3_str_finish(sql);
}

static const char *alter_table_column_physical_name(
    const struct mylite_alter_table_column *column
) {
    if (column->source_name != NULL) {
        return column->source_name;
    }
    return column->name;
}
