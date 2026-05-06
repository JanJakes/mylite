#include "mylite_table_ddl_statement.h"

#include "mylite_catalog.h"
#include "mylite_diagnostics.h"
#include "mylite_dml.h"
#include "mylite_error_codes.h"
#include "mylite_runtime.h"
#include "mylite_schema_types.h"
#include "mylite_span.h"
#include "mylite_table_ddl.h"
#include "mylite_table_ddl_alter.h"
#include "mylite_table_ddl_alter_catalog.h"
#include "mylite_table_ddl_alter_column_value.h"
#include "mylite_table_ddl_alter_index.h"
#include "mylite_table_ddl_alter_model.h"
#include "mylite_table_ddl_alter_rebuild.h"
#include "mylite_table_ddl_alter_unique_validate.h"
#include "sqlite3.h"

#include <mylite/mylite.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static bool alter_table_has_table_rename_action(const mylite_stmt *stmt);

static int execute_alter_table_rename_statement(mylite_stmt *stmt);

static int add_alter_table_rename_target(mylite_stmt *stmt);

static int validate_alter_table_plan(
    mylite_stmt *stmt,
    const char **out_schema_name,
    bool *out_temporary
);

static int resolve_alter_table_schema(mylite_stmt *stmt);

static int validate_alter_table_target(mylite_stmt *stmt, bool *out_temporary);

static int apply_alter_table_actions(mylite_stmt *stmt, struct mylite_alter_table_model *model);

static void apply_alter_table_options(mylite_stmt *stmt, struct mylite_alter_table_model *model);

static bool alter_table_model_has_auto_increment_column(
    const struct mylite_alter_table_model *model
);

static int set_alter_table_unsupported_action_error(mylite_db *database, const char *feature);

static int validate_alter_table_primary_key_part_not_null(
    void *user_data,
    const struct mylite_alter_table_model *model,
    const struct mylite_create_table_key_part *part
);

static int set_alter_table_unsupported_option_error(
    mylite_db *database,
    const char *kind,
    const char *value
);

static int set_alter_table_invalid_null_error(mylite_db *database);

int mylite_table_ddl_execute_alter_table_prepared_statement(mylite_stmt *stmt) {
    const char *schema_name = NULL;
    struct mylite_alter_table_model model = {0};
    bool temporary = false;
    int status = MYLITE_OK;

    stmt->affected_rows = 0;
    if (alter_table_has_table_rename_action(stmt)) {
        return execute_alter_table_rename_statement(stmt);
    }

    status = validate_alter_table_plan(stmt, &schema_name, &temporary);
    if (status == MYLITE_OK) {
        status = mylite_table_ddl_load_alter_table_model(
            stmt->database,
            schema_name,
            stmt->alter_table.table_name,
            temporary,
            &model
        );
    }
    if (status == MYLITE_OK) {
        status = apply_alter_table_actions(stmt, &model);
    }
    if (status == MYLITE_OK) {
        apply_alter_table_options(stmt, &model);
    }
    if (status == MYLITE_OK) {
        status = mylite_table_ddl_validate_alter_table_final_model(stmt->database, &model);
    }
    if (status == MYLITE_OK) {
        status = mylite_table_ddl_validate_alter_table_unique_indexes(stmt->database, &model);
    }
    if (status == MYLITE_OK) {
        status = mylite_table_ddl_execute_alter_table_rebuild(stmt, &model);
    }

    mylite_table_ddl_alter_table_model_deinit(&model);
    if (status != MYLITE_OK) {
        stmt->affected_rows = -1;
    }
    return status;
}

static bool alter_table_has_table_rename_action(const mylite_stmt *stmt) {
    for (size_t index = 0U; index < stmt->alter_table.action_count; ++index) {
        if (stmt->alter_table.actions[index].kind == MYLITE_ALTER_TABLE_ACTION_RENAME_TABLE) {
            return true;
        }
    }
    return false;
}

static int execute_alter_table_rename_statement(mylite_stmt *stmt) {
    int status = MYLITE_OK;

    if (stmt->alter_table.unsupported_algorithm != NULL) {
        stmt->affected_rows = -1;
        return set_alter_table_unsupported_option_error(
            stmt->database,
            "ALGORITHM",
            stmt->alter_table.unsupported_algorithm
        );
    }
    if (stmt->alter_table.unsupported_lock != NULL) {
        stmt->affected_rows = -1;
        return set_alter_table_unsupported_option_error(
            stmt->database,
            "LOCK",
            stmt->alter_table.unsupported_lock
        );
    }
    if (stmt->alter_table.action_count != 1U || stmt->alter_table.has_auto_increment) {
        stmt->affected_rows = -1;
        return set_alter_table_unsupported_action_error(
            stmt->database,
            "ALTER TABLE table rename with other actions"
        );
    }

    status = add_alter_table_rename_target(stmt);
    if (status != MYLITE_OK) {
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        }
        stmt->affected_rows = -1;
        return status;
    }
    return mylite_table_ddl_execute_rename_table_prepared_statement(stmt);
}

static int add_alter_table_rename_target(mylite_stmt *stmt) {
    const struct mylite_alter_table_action *action = &stmt->alter_table.actions[0];
    struct mylite_rename_table_target target = {0};
    int status = MYLITE_OK;

    if (stmt->alter_table.schema_name != NULL) {
        target.source_schema_name = mylite_copy_nonempty_cstring(stmt->alter_table.schema_name);
    }
    target.source_table_name = mylite_copy_nonempty_cstring(stmt->alter_table.table_name);
    if (action->new_schema_name != NULL) {
        target.target_schema_name = mylite_copy_nonempty_cstring(action->new_schema_name);
    }
    target.target_table_name = mylite_copy_nonempty_cstring(action->new_name);
    if ((stmt->alter_table.schema_name != NULL && target.source_schema_name == NULL) ||
        target.source_table_name == NULL ||
        (action->new_schema_name != NULL && target.target_schema_name == NULL) ||
        target.target_table_name == NULL) {
        mylite_table_ddl_rename_table_target_deinit(&target);
        return MYLITE_NOMEM;
    }

    status = mylite_table_ddl_add_rename_table_target(&stmt->rename_table, target);
    if (status != MYLITE_OK) {
        mylite_table_ddl_rename_table_target_deinit(&target);
    }
    return status;
}

static int validate_alter_table_plan(
    mylite_stmt *stmt,
    const char **out_schema_name,
    bool *out_temporary
) {
    int status = MYLITE_OK;

    *out_schema_name = NULL;
    *out_temporary = false;
    if (stmt->alter_table.unsupported_algorithm != NULL) {
        return set_alter_table_unsupported_option_error(
            stmt->database,
            "ALGORITHM",
            stmt->alter_table.unsupported_algorithm
        );
    }
    if (stmt->alter_table.unsupported_lock != NULL) {
        return set_alter_table_unsupported_option_error(
            stmt->database,
            "LOCK",
            stmt->alter_table.unsupported_lock
        );
    }

    status = resolve_alter_table_schema(stmt);
    if (status != MYLITE_OK) {
        return status;
    }
    status = validate_alter_table_target(stmt, out_temporary);
    if (status != MYLITE_OK) {
        return status;
    }

    *out_schema_name = stmt->alter_table.schema_name;
    return MYLITE_OK;
}

static int resolve_alter_table_schema(mylite_stmt *stmt) {
    if (stmt->alter_table.schema_name != NULL) {
        return MYLITE_OK;
    }
    if (stmt->database->selected_schema == NULL || stmt->database->selected_schema[0] == '\0') {
        (void)mylite_diagnostics_set_error_message(stmt->database, "No database selected");
        return MYLITE_EXEC_ERROR;
    }

    stmt->alter_table.schema_name = mylite_copy_nonempty_cstring(stmt->database->selected_schema);
    if (stmt->alter_table.schema_name == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    return MYLITE_OK;
}

static int validate_alter_table_target(mylite_stmt *stmt, bool *out_temporary) {
    struct mylite_schema_presence presence;
    bool exists = false;
    int status =
        mylite_catalog_schema_exists(stmt->database, stmt->alter_table.schema_name, &presence);

    if (status != MYLITE_OK) {
        return status;
    }
    if (!presence.exists) {
        (void)mylite_diagnostics_set_error_message_parts(
            stmt->database,
            "Unknown database '",
            stmt->alter_table.schema_name,
            "'"
        );
        return MYLITE_EXEC_ERROR;
    }
    if (presence.is_system) {
        (void)mylite_diagnostics_set_error_message_parts(
            stmt->database,
            "Access to system schema '",
            stmt->alter_table.schema_name,
            "' is rejected."
        );
        return MYLITE_EXEC_ERROR;
    }

    status = mylite_catalog_temporary_table_exists(
        stmt->database,
        stmt->alter_table.schema_name,
        stmt->alter_table.table_name,
        &exists
    );
    if (status != MYLITE_OK) {
        return status;
    }
    if (exists) {
        *out_temporary = true;
        return MYLITE_OK;
    }

    status = mylite_catalog_persistent_table_exists(
        stmt->database,
        stmt->alter_table.schema_name,
        stmt->alter_table.table_name,
        &exists
    );
    if (status != MYLITE_OK) {
        return status;
    }
    if (!exists) {
        return mylite_diagnostics_set_table_doesnt_exist_error(
            stmt->database,
            stmt->alter_table.schema_name,
            stmt->alter_table.table_name
        );
    }
    *out_temporary = false;
    return MYLITE_OK;
}

static int apply_alter_table_actions(mylite_stmt *stmt, struct mylite_alter_table_model *model) {
    const struct mylite_table_ddl_alter_callbacks alter_callbacks = {
        .user_data = stmt,
        .validate_primary_key_part_not_null = validate_alter_table_primary_key_part_not_null,
    };
    struct mylite_schema_default schema_default = {0};
    bool schema_default_loaded = false;
    int status = MYLITE_OK;

    for (size_t index = 0U; index < stmt->alter_table.action_count; ++index) {
        const struct mylite_alter_table_action *action = &stmt->alter_table.actions[index];

        switch (action->kind) {
        case MYLITE_ALTER_TABLE_ACTION_ADD_COLUMN:
        case MYLITE_ALTER_TABLE_ACTION_DROP_COLUMN:
        case MYLITE_ALTER_TABLE_ACTION_RENAME_COLUMN:
        case MYLITE_ALTER_TABLE_ACTION_CHANGE_COLUMN:
        case MYLITE_ALTER_TABLE_ACTION_MODIFY_COLUMN:
            if (!schema_default_loaded) {
                status = mylite_catalog_schema_default_by_name(
                    stmt->database,
                    stmt->alter_table.schema_name,
                    &schema_default
                );
                if (status != MYLITE_OK) {
                    break;
                }
                schema_default_loaded = true;
            }
            status = mylite_table_ddl_apply_alter_table_column_action(
                stmt->database,
                &schema_default,
                action,
                model
            );
            break;
        case MYLITE_ALTER_TABLE_ACTION_RENAME_TABLE:
            status = set_alter_table_unsupported_action_error(stmt->database, "mixed table rename");
            break;
        case MYLITE_ALTER_TABLE_ACTION_ADD_PRIMARY_KEY:
        case MYLITE_ALTER_TABLE_ACTION_DROP_PRIMARY_KEY:
        case MYLITE_ALTER_TABLE_ACTION_ADD_UNIQUE_INDEX:
        case MYLITE_ALTER_TABLE_ACTION_ADD_SECONDARY_INDEX:
            status = mylite_table_ddl_apply_alter_table_index_action(
                stmt->database,
                action,
                model,
                &alter_callbacks
            );
            break;
        case MYLITE_ALTER_TABLE_ACTION_ADD_FULLTEXT_INDEX:
            status = set_alter_table_unsupported_action_error(
                stmt->database,
                "FULLTEXT ALTER TABLE indexes"
            );
            break;
        case MYLITE_ALTER_TABLE_ACTION_ADD_SPATIAL_INDEX:
            status = set_alter_table_unsupported_action_error(
                stmt->database,
                "SPATIAL ALTER TABLE indexes"
            );
            break;
        case MYLITE_ALTER_TABLE_ACTION_DROP_INDEX:
        case MYLITE_ALTER_TABLE_ACTION_RENAME_INDEX:
        case MYLITE_ALTER_TABLE_ACTION_ALTER_INDEX_VISIBILITY:
            status = mylite_table_ddl_apply_alter_table_index_action(
                stmt->database,
                action,
                model,
                &alter_callbacks
            );
            break;
        case MYLITE_ALTER_TABLE_ACTION_UNSUPPORTED_CHECK:
            status = set_alter_table_unsupported_action_error(
                stmt->database,
                "CHECK ALTER TABLE constraints"
            );
            break;
        case MYLITE_ALTER_TABLE_ACTION_UNSUPPORTED_FOREIGN_KEY:
            status = set_alter_table_unsupported_action_error(
                stmt->database,
                "FOREIGN KEY ALTER TABLE constraints"
            );
            break;
        }
        if (status != MYLITE_OK) {
            return status;
        }
    }

    return mylite_table_ddl_refresh_alter_table_index_metadata(stmt->database, model);
}

static void apply_alter_table_options(mylite_stmt *stmt, struct mylite_alter_table_model *model) {
    if (!stmt->alter_table.has_auto_increment) {
        return;
    }
    if (!alter_table_model_has_auto_increment_column(model)) {
        model->clear_auto_increment = true;
        return;
    }
    model->auto_increment = stmt->alter_table.auto_increment;
    model->set_auto_increment = true;
    model->clear_auto_increment = false;
}

static bool alter_table_model_has_auto_increment_column(
    const struct mylite_alter_table_model *model
) {
    for (size_t index = 0U; index < model->column_count; ++index) {
        if (model->columns[index].auto_increment) {
            return true;
        }
    }
    return false;
}

static int set_alter_table_unsupported_action_error(mylite_db *database, const char *feature) {
    int status = mylite_diagnostics_set_error_message_parts(database, "Unsupported ", feature, "");

    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_UNSUPPORTED;
}

static int validate_alter_table_primary_key_part_not_null(
    void *user_data,
    const struct mylite_alter_table_model *model,
    const struct mylite_create_table_key_part *part
) {
    mylite_stmt *stmt = user_data;
    const struct mylite_alter_table_column *column =
        mylite_table_ddl_find_alter_table_column(model, part->column_name);
    char *sql = NULL;
    sqlite3_stmt *select = NULL;
    int rc = SQLITE_OK;

    if (stmt == NULL) {
        return MYLITE_MISUSE;
    }
    if (column == NULL) {
        return MYLITE_OK;
    }
    if (column->source_name == NULL) {
        struct mylite_insert_bound_value value = {0};
        int status =
            mylite_table_ddl_resolve_alter_table_added_column_value(stmt->database, column, &value);
        bool is_null = value.kind == MYLITE_INSERT_BOUND_NULL;

        mylite_dml_insert_bound_value_deinit(&value);
        if (status != MYLITE_OK) {
            return status;
        }
        if (is_null) {
            return set_alter_table_invalid_null_error(stmt->database);
        }
        return MYLITE_OK;
    }

    sql = sqlite3_mprintf(
        "SELECT 1 FROM \"%w\" WHERE \"%w\" IS NULL LIMIT 1",
        model->physical_name,
        column->source_name
    );
    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    rc = sqlite3_prepare_v3(
        stmt->database->sqlite,
        sql,
        -1,
        SQLITE_PREPARE_PERSISTENT,
        &select,
        NULL
    );
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(stmt->database);
    }
    rc = sqlite3_step(select);
    sqlite3_finalize(select);
    if (rc == SQLITE_ROW) {
        return set_alter_table_invalid_null_error(stmt->database);
    }
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(stmt->database);
}

static int set_alter_table_unsupported_option_error(
    mylite_db *database,
    const char *kind,
    const char *value
) {
    char *message = sqlite3_mprintf("ALTER TABLE %s option is not supported: %q", kind, value);
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_set_error_message(database, message);
    sqlite3_free(message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_UNSUPPORTED;
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
