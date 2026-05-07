#include "mylite_table_ddl_statement.h"

#include "mylite_catalog.h"
#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_dml.h"
#include "mylite_error_codes.h"
#include "mylite_foreign_key_catalog.h"
#include "mylite_runtime.h"
#include "mylite_schema_types.h"
#include "mylite_span.h"
#include "mylite_table_ddl.h"
#include "mylite_table_ddl_alter.h"
#include "mylite_table_ddl_alter_catalog.h"
#include "mylite_table_ddl_alter_column_value.h"
#include "mylite_table_ddl_alter_index.h"
#include "mylite_table_ddl_alter_index_build.h"
#include "mylite_table_ddl_alter_index_model.h"
#include "mylite_table_ddl_alter_model.h"
#include "mylite_table_ddl_alter_rebuild.h"
#include "mylite_table_ddl_alter_unique_validate.h"
#include "mylite_transactions.h"
#include "sqlite3.h"

#include <mylite/mylite.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct alter_table_foreign_key_existing_rows_sql_input {
    const char *child_physical_name;
    const char *parent_physical_name;
    const struct mylite_alter_table_model *model;
    const struct mylite_create_table_foreign_key *foreign_key;
};

struct alter_table_check_existing_rows_input {
    const char *constraint_name;
    const char *check_clause;
};

struct alter_table_mixed_constraint_state {
    bool has_add_foreign_key;
    bool has_check;
    int64_t foreign_key_affected_rows;
    int64_t check_affected_rows;
};

static bool alter_table_has_table_rename_action(const mylite_stmt *stmt);

static bool alter_table_has_only_check_actions(const mylite_stmt *stmt);

static bool alter_table_has_check_action(const mylite_stmt *stmt);

static bool alter_table_action_is_check_action(enum mylite_alter_table_action_kind kind);

static bool alter_table_has_enforced_add_check_action(const mylite_stmt *stmt);

static bool alter_table_has_only_foreign_key_actions(const mylite_stmt *stmt);

static bool alter_table_has_foreign_key_action(const mylite_stmt *stmt);

static int execute_alter_table_rename_statement(mylite_stmt *stmt);

static int execute_alter_table_check_statement(mylite_stmt *stmt);

static int execute_alter_table_mixed_check_statement(mylite_stmt *stmt);

static int execute_alter_table_foreign_key_statement(mylite_stmt *stmt);

static int execute_alter_table_mixed_foreign_key_statement(mylite_stmt *stmt);

static int count_alter_table_mixed_foreign_key_rows(
    mylite_stmt *stmt,
    const struct mylite_alter_table_model *model,
    struct alter_table_mixed_constraint_state *state
);

static void mark_alter_table_copied_rows_for_enforced_add_check(
    const mylite_stmt *stmt,
    struct mylite_alter_table_model *model
);

static int apply_alter_table_mixed_check_actions_if_needed(
    mylite_stmt *stmt,
    const struct mylite_alter_table_model *model,
    struct alter_table_mixed_constraint_state *state
);

static void set_alter_table_mixed_constraint_affected_rows(
    mylite_stmt *stmt,
    const struct alter_table_mixed_constraint_state *state
);

static void rollback_alter_table_mixed_foreign_key_statement(
    mylite_db *database,
    struct mylite_statement_atomicity *atomicity,
    const struct alter_table_mixed_constraint_state *state
);

static int add_alter_table_rename_target(mylite_stmt *stmt);

static int validate_alter_table_plan(
    mylite_stmt *stmt,
    const char **out_schema_name,
    bool *out_temporary
);

static int resolve_alter_table_schema(mylite_stmt *stmt);

static int validate_alter_table_target(mylite_stmt *stmt, bool *out_temporary);

static int apply_alter_table_actions(
    mylite_stmt *stmt,
    struct mylite_alter_table_model *model,
    bool defer_foreign_key_actions,
    bool defer_check_actions
);

static int apply_alter_table_check_actions(
    mylite_stmt *stmt,
    const struct mylite_alter_table_model *model,
    int64_t *out_affected_rows
);

static int apply_alter_table_check_action(
    mylite_stmt *stmt,
    const struct mylite_alter_table_model *model,
    struct mylite_alter_table_action *action,
    int64_t *out_affected_rows
);

static int apply_alter_table_add_check_action(
    mylite_stmt *stmt,
    const struct mylite_alter_table_model *model,
    struct mylite_create_table_check *check
);

static int apply_alter_table_drop_check_action(
    mylite_stmt *stmt,
    const struct mylite_alter_table_action *action,
    bool temporary
);

static int apply_alter_table_alter_check_action(
    mylite_stmt *stmt,
    const struct mylite_alter_table_model *model,
    const struct mylite_alter_table_action *action,
    int64_t *out_affected_rows
);

static int apply_alter_table_foreign_key_actions(
    mylite_stmt *stmt,
    struct mylite_alter_table_model *model,
    bool *out_index_catalog_changed
);

static int apply_alter_table_drop_foreign_key_actions(mylite_stmt *stmt);

static int apply_alter_table_add_foreign_key_actions(
    mylite_stmt *stmt,
    struct mylite_alter_table_model *model,
    bool *out_index_catalog_changed
);

static int apply_alter_table_foreign_key_action(
    mylite_stmt *stmt,
    struct mylite_alter_table_model *model,
    struct mylite_alter_table_action *action,
    bool *out_index_catalog_changed
);

static bool alter_table_has_add_foreign_key_action(const mylite_stmt *stmt);

static int count_alter_table_physical_rows(
    mylite_db *database,
    const char *physical_name,
    int64_t *out_row_count
);

static int apply_alter_table_add_foreign_key_action(
    mylite_stmt *stmt,
    struct mylite_alter_table_model *model,
    struct mylite_create_table_foreign_key *foreign_key,
    bool *out_index_catalog_changed
);

static int apply_alter_table_drop_foreign_key_action(
    mylite_stmt *stmt,
    const struct mylite_alter_table_action *action
);

static void apply_alter_table_options(mylite_stmt *stmt, struct mylite_alter_table_model *model);

static bool alter_table_model_has_auto_increment_column(
    const struct mylite_alter_table_model *model
);

static int set_alter_table_unsupported_action_error(mylite_db *database, const char *feature);

static int ensure_alter_table_check_constraint_name(
    mylite_stmt *stmt,
    bool temporary,
    struct mylite_create_table_check *check
);

static int generated_alter_table_check_constraint_name(
    mylite_stmt *stmt,
    bool temporary,
    char **out_name
);

static int max_alter_table_generated_check_suffix(
    mylite_stmt *stmt,
    bool temporary,
    int64_t *out_suffix
);

static bool alter_table_generated_check_suffix(
    const char *table_name,
    const char *constraint_name,
    int64_t *out_suffix
);

static int max_alter_table_check_ordinal(mylite_stmt *stmt, bool temporary, int64_t *out_ordinal);

static int alter_table_check_constraint_name_exists(
    mylite_db *database,
    const char *schema_name,
    bool temporary,
    const char *constraint_name,
    bool *out_exists
);

static int load_alter_table_check_constraint(
    mylite_stmt *stmt,
    bool temporary,
    const char *constraint_name,
    struct mylite_create_table_check *out_check,
    bool *out_exists
);

static int insert_alter_table_check_constraint(
    mylite_stmt *stmt,
    bool temporary,
    const struct mylite_create_table_check *check,
    int64_t ordinal_position
);

static int delete_alter_table_check_constraint(
    mylite_stmt *stmt,
    bool temporary,
    const char *constraint_name
);

static int update_alter_table_check_constraint_enforcement(
    mylite_stmt *stmt,
    bool temporary,
    const char *constraint_name,
    bool enforced
);

static int validate_alter_table_check_existing_rows(
    mylite_stmt *stmt,
    const struct mylite_alter_table_model *model,
    const struct alter_table_check_existing_rows_input *input
);

static char *build_alter_table_check_existing_rows_sql(
    const char *physical_name,
    const char *check_clause
);

static int set_alter_table_duplicate_check_error(mylite_db *database, const char *constraint_name);

static int set_alter_table_missing_check_error(mylite_db *database, const char *constraint_name);

static int set_alter_table_check_violation_error(mylite_db *database, const char *constraint_name);

static int ensure_alter_table_foreign_key_constraint_name(
    mylite_stmt *stmt,
    struct mylite_create_table_foreign_key *foreign_key
);

static char *generated_alter_table_foreign_key_constraint_name(mylite_stmt *stmt);

static sqlite3_destructor_type sqlite_transient_destructor(void);

static int validate_alter_table_foreign_key_columns(
    mylite_db *database,
    const struct mylite_alter_table_model *model,
    const struct mylite_create_table_foreign_key *foreign_key
);

static int resolve_alter_table_foreign_key_parent(
    mylite_stmt *stmt,
    const struct mylite_alter_table_model *model,
    struct mylite_create_table_foreign_key *foreign_key,
    bool *out_parent_exists
);

static int resolve_self_referenced_alter_table_unique_constraint(
    const struct mylite_alter_table_model *model,
    const struct mylite_create_table_foreign_key *foreign_key,
    char **out_constraint_name,
    bool *out_found
);

static int resolve_referenced_alter_table_unique_constraint(
    mylite_db *database,
    const struct mylite_create_table_foreign_key *foreign_key,
    char **out_constraint_name,
    bool *out_found
);

static int alter_table_referenced_unique_candidate_matches(
    mylite_db *database,
    const char *candidate_name,
    const struct mylite_create_table_foreign_key *foreign_key,
    bool *out_matches
);

static int ensure_alter_table_foreign_key_supporting_index(
    mylite_stmt *stmt,
    struct mylite_alter_table_model *model,
    struct mylite_create_table_foreign_key *foreign_key,
    bool *out_index_catalog_changed
);

static const char *alter_table_supporting_foreign_key_index_name(
    const struct mylite_alter_table_model *model,
    const struct mylite_create_table_foreign_key *foreign_key
);

static bool alter_table_index_supports_foreign_key(
    const struct mylite_alter_table_index *index,
    const struct mylite_create_table_foreign_key *foreign_key
);

static int assign_alter_table_foreign_key_supporting_index_name(
    mylite_stmt *stmt,
    const struct mylite_alter_table_model *model,
    struct mylite_create_table_foreign_key *foreign_key
);

static int add_alter_table_foreign_key_supporting_index(
    mylite_stmt *stmt,
    struct mylite_alter_table_model *model,
    const struct mylite_create_table_foreign_key *foreign_key
);

static int validate_alter_table_foreign_key_existing_rows(
    mylite_stmt *stmt,
    const struct mylite_alter_table_model *model,
    const struct mylite_create_table_foreign_key *foreign_key,
    bool parent_exists
);

static char *build_alter_table_foreign_key_existing_rows_sql(
    mylite_db *database,
    const struct alter_table_foreign_key_existing_rows_sql_input *input
);

static int append_alter_table_foreign_key_child_value_sql(
    mylite_db *database,
    sqlite3_str *sql,
    const struct mylite_alter_table_model *model,
    const char *column_name,
    bool *out_skip_check
);

static int insert_alter_table_foreign_key_catalog_rows(
    mylite_stmt *stmt,
    const struct mylite_create_table_foreign_key *foreign_key
);

static int set_alter_table_duplicate_foreign_key_error(
    mylite_db *database,
    const char *constraint_name
);

static int set_alter_table_cannot_add_foreign_key_error(mylite_db *database);

static int set_alter_table_missing_referenced_table_error(
    mylite_db *database,
    const char *table_name
);

static int set_alter_table_foreign_key_existing_row_error(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    const char *constraint_name
);

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
    if (alter_table_has_only_check_actions(stmt) && !stmt->alter_table.has_auto_increment) {
        return execute_alter_table_check_statement(stmt);
    }
    if (alter_table_has_only_foreign_key_actions(stmt)) {
        return execute_alter_table_foreign_key_statement(stmt);
    }
    if (alter_table_has_foreign_key_action(stmt)) {
        return execute_alter_table_mixed_foreign_key_statement(stmt);
    }
    if (alter_table_has_check_action(stmt)) {
        return execute_alter_table_mixed_check_statement(stmt);
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
        status = apply_alter_table_actions(stmt, &model, false, false);
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

static bool alter_table_has_only_check_actions(const mylite_stmt *stmt) {
    if (stmt->alter_table.action_count == 0U) {
        return false;
    }
    for (size_t index = 0U; index < stmt->alter_table.action_count; ++index) {
        if (!alter_table_action_is_check_action(stmt->alter_table.actions[index].kind)) {
            return false;
        }
    }
    return true;
}

static bool alter_table_has_check_action(const mylite_stmt *stmt) {
    for (size_t index = 0U; index < stmt->alter_table.action_count; ++index) {
        if (alter_table_action_is_check_action(stmt->alter_table.actions[index].kind)) {
            return true;
        }
    }
    return false;
}

static bool alter_table_action_is_check_action(enum mylite_alter_table_action_kind kind) {
    if (kind == MYLITE_ALTER_TABLE_ACTION_ADD_CHECK ||
        kind == MYLITE_ALTER_TABLE_ACTION_DROP_CHECK ||
        kind == MYLITE_ALTER_TABLE_ACTION_ALTER_CHECK) {
        return true;
    }
    return false;
}

static bool alter_table_has_enforced_add_check_action(const mylite_stmt *stmt) {
    for (size_t index = 0U; index < stmt->alter_table.action_count; ++index) {
        const struct mylite_alter_table_action *action = &stmt->alter_table.actions[index];

        if (action->kind == MYLITE_ALTER_TABLE_ACTION_ADD_CHECK && action->check.enforced) {
            return true;
        }
    }
    return false;
}

static bool alter_table_has_only_foreign_key_actions(const mylite_stmt *stmt) {
    if (stmt->alter_table.action_count == 0U) {
        return false;
    }
    for (size_t index = 0U; index < stmt->alter_table.action_count; ++index) {
        enum mylite_alter_table_action_kind kind = stmt->alter_table.actions[index].kind;

        if (kind != MYLITE_ALTER_TABLE_ACTION_ADD_FOREIGN_KEY &&
            kind != MYLITE_ALTER_TABLE_ACTION_DROP_FOREIGN_KEY) {
            return false;
        }
    }
    return true;
}

static bool alter_table_has_foreign_key_action(const mylite_stmt *stmt) {
    for (size_t index = 0U; index < stmt->alter_table.action_count; ++index) {
        enum mylite_alter_table_action_kind kind = stmt->alter_table.actions[index].kind;

        if (kind == MYLITE_ALTER_TABLE_ACTION_ADD_FOREIGN_KEY ||
            kind == MYLITE_ALTER_TABLE_ACTION_DROP_FOREIGN_KEY) {
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

static int execute_alter_table_check_statement(mylite_stmt *stmt) {
    const char *schema_name = NULL;
    struct mylite_alter_table_model model = {0};
    struct mylite_statement_atomicity atomicity = {0};
    bool atomicity_started = false;
    bool temporary = false;
    int64_t affected_rows = 0;
    int status = MYLITE_OK;

    if (stmt->alter_table.has_auto_increment) {
        stmt->affected_rows = -1;
        return set_alter_table_unsupported_action_error(
            stmt->database,
            "CHECK ALTER TABLE constraints with other actions"
        );
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
        status = mylite_transaction_begin_statement_atomicity(stmt->database, &atomicity);
        atomicity_started = status == MYLITE_OK;
    }
    if (status == MYLITE_OK) {
        status = apply_alter_table_check_actions(stmt, &model, &affected_rows);
    }
    if (status == MYLITE_OK) {
        status = mylite_transaction_commit_statement_atomicity(stmt->database, &atomicity);
        if (status == MYLITE_OK) {
            stmt->affected_rows = affected_rows;
            mylite_table_ddl_alter_table_model_deinit(&model);
            return MYLITE_OK;
        }
    }

    if (atomicity_started) {
        mylite_transaction_rollback_statement_atomicity(stmt->database, &atomicity);
    }
    mylite_table_ddl_alter_table_model_deinit(&model);
    stmt->affected_rows = -1;
    return status;
}

static int execute_alter_table_mixed_check_statement(mylite_stmt *stmt) {
    const char *schema_name = NULL;
    struct mylite_alter_table_model model = {0};
    struct mylite_statement_atomicity atomicity = {0};
    bool atomicity_started = false;
    bool temporary = false;
    int64_t check_affected_rows = 0;
    int status = validate_alter_table_plan(stmt, &schema_name, &temporary);

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
        status = mylite_transaction_begin_statement_atomicity(stmt->database, &atomicity);
        atomicity_started = status == MYLITE_OK;
    }
    if (status == MYLITE_OK) {
        status = apply_alter_table_actions(stmt, &model, false, true);
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
    if (status == MYLITE_OK && alter_table_has_enforced_add_check_action(stmt)) {
        model.report_copied_rows = true;
    }
    if (status == MYLITE_OK) {
        status = mylite_table_ddl_execute_alter_table_rebuild_in_atomicity(stmt, &model);
    }
    if (status == MYLITE_OK) {
        status = apply_alter_table_check_actions(stmt, &model, &check_affected_rows);
    }
    if (status == MYLITE_OK && check_affected_rows > 0) {
        stmt->affected_rows = check_affected_rows;
    }
    if (status == MYLITE_OK) {
        status = mylite_transaction_commit_statement_atomicity(stmt->database, &atomicity);
        if (status == MYLITE_OK) {
            mylite_table_ddl_alter_table_model_deinit(&model);
            return MYLITE_OK;
        }
    }

    if (atomicity_started) {
        mylite_transaction_rollback_statement_atomicity(stmt->database, &atomicity);
    }
    mylite_table_ddl_alter_table_model_deinit(&model);
    stmt->affected_rows = -1;
    return status;
}

static int execute_alter_table_foreign_key_statement(mylite_stmt *stmt) {
    const char *schema_name = NULL;
    struct mylite_alter_table_model model = {0};
    struct mylite_statement_atomicity atomicity = {0};
    bool atomicity_started = false;
    bool has_add_foreign_key = alter_table_has_add_foreign_key_action(stmt);
    bool temporary = false;
    bool index_catalog_changed = false;
    int64_t affected_rows = 0;
    int status = MYLITE_OK;

    if (stmt->alter_table.has_auto_increment) {
        stmt->affected_rows = -1;
        return set_alter_table_unsupported_action_error(
            stmt->database,
            "FOREIGN KEY ALTER TABLE constraints with other actions"
        );
    }

    status = validate_alter_table_plan(stmt, &schema_name, &temporary);
    if (status == MYLITE_OK && temporary) {
        status = set_alter_table_unsupported_action_error(
            stmt->database,
            "FOREIGN KEY ALTER TABLE constraints on temporary tables"
        );
    }
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
        status = mylite_transaction_begin_statement_atomicity(stmt->database, &atomicity);
        atomicity_started = status == MYLITE_OK;
    }
    if (status == MYLITE_OK) {
        status = apply_alter_table_foreign_key_actions(stmt, &model, &index_catalog_changed);
    }
    if (status == MYLITE_OK && index_catalog_changed) {
        status = mylite_table_ddl_rewrite_alter_table_catalog(
            stmt->database,
            stmt->alter_table.schema_name,
            stmt->alter_table.table_name,
            &model
        );
    }
    if (status == MYLITE_OK && has_add_foreign_key) {
        status =
            count_alter_table_physical_rows(stmt->database, model.physical_name, &affected_rows);
    }
    if (status == MYLITE_OK) {
        status = mylite_transaction_commit_statement_atomicity(stmt->database, &atomicity);
        if (status == MYLITE_OK) {
            stmt->affected_rows = affected_rows;
            mylite_table_ddl_alter_table_model_deinit(&model);
            return MYLITE_OK;
        }
    }

    if (atomicity_started) {
        mylite_transaction_rollback_statement_atomicity(stmt->database, &atomicity);
    }
    mylite_table_ddl_alter_table_model_deinit(&model);
    stmt->affected_rows = -1;
    return status;
}

static int execute_alter_table_mixed_foreign_key_statement(mylite_stmt *stmt) {
    const char *schema_name = NULL;
    struct mylite_alter_table_model model = {0};
    struct mylite_statement_atomicity atomicity = {0};
    struct alter_table_mixed_constraint_state constraint_state = {
        .has_add_foreign_key = alter_table_has_add_foreign_key_action(stmt),
        .has_check = alter_table_has_check_action(stmt),
    };
    bool atomicity_started = false;
    bool temporary = false;
    bool index_catalog_changed = false;
    int status = validate_alter_table_plan(stmt, &schema_name, &temporary);

    if (status == MYLITE_OK && temporary) {
        status = set_alter_table_unsupported_action_error(
            stmt->database,
            "FOREIGN KEY ALTER TABLE constraints on temporary tables"
        );
    }
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
        status = mylite_transaction_begin_statement_atomicity(stmt->database, &atomicity);
        atomicity_started = status == MYLITE_OK;
    }
    if (status == MYLITE_OK) {
        status = apply_alter_table_drop_foreign_key_actions(stmt);
    }
    if (status == MYLITE_OK) {
        status = apply_alter_table_actions(stmt, &model, true, constraint_state.has_check);
    }
    if (status == MYLITE_OK) {
        apply_alter_table_options(stmt, &model);
    }
    if (status == MYLITE_OK) {
        status = apply_alter_table_add_foreign_key_actions(stmt, &model, &index_catalog_changed);
    }
    if (status == MYLITE_OK && index_catalog_changed) {
        status = mylite_table_ddl_refresh_alter_table_index_metadata(stmt->database, &model);
    }
    if (status == MYLITE_OK) {
        status = mylite_table_ddl_validate_alter_table_final_model(stmt->database, &model);
    }
    if (status == MYLITE_OK) {
        status = mylite_table_ddl_validate_alter_table_unique_indexes(stmt->database, &model);
    }
    if (status == MYLITE_OK) {
        status = count_alter_table_mixed_foreign_key_rows(stmt, &model, &constraint_state);
    }
    mark_alter_table_copied_rows_for_enforced_add_check(stmt, &model);
    if (status == MYLITE_OK) {
        status = mylite_table_ddl_execute_alter_table_rebuild_in_atomicity(stmt, &model);
    }
    if (status == MYLITE_OK) {
        status = apply_alter_table_mixed_check_actions_if_needed(stmt, &model, &constraint_state);
    }
    set_alter_table_mixed_constraint_affected_rows(stmt, &constraint_state);
    if (status == MYLITE_OK) {
        status = mylite_transaction_commit_statement_atomicity(stmt->database, &atomicity);
        if (status == MYLITE_OK) {
            mylite_table_ddl_alter_table_model_deinit(&model);
            return MYLITE_OK;
        }
    }

    if (atomicity_started) {
        rollback_alter_table_mixed_foreign_key_statement(
            stmt->database,
            &atomicity,
            &constraint_state
        );
    }
    mylite_table_ddl_alter_table_model_deinit(&model);
    stmt->affected_rows = -1;
    return status;
}

static int count_alter_table_mixed_foreign_key_rows(
    mylite_stmt *stmt,
    const struct mylite_alter_table_model *model,
    struct alter_table_mixed_constraint_state *state
) {
    if (!state->has_add_foreign_key) {
        return MYLITE_OK;
    }
    return count_alter_table_physical_rows(
        stmt->database,
        model->physical_name,
        &state->foreign_key_affected_rows
    );
}

static void mark_alter_table_copied_rows_for_enforced_add_check(
    const mylite_stmt *stmt,
    struct mylite_alter_table_model *model
) {
    if (alter_table_has_enforced_add_check_action(stmt)) {
        model->report_copied_rows = true;
    }
}

static int apply_alter_table_mixed_check_actions_if_needed(
    mylite_stmt *stmt,
    const struct mylite_alter_table_model *model,
    struct alter_table_mixed_constraint_state *state
) {
    if (!state->has_check) {
        return MYLITE_OK;
    }
    return apply_alter_table_check_actions(stmt, model, &state->check_affected_rows);
}

static void set_alter_table_mixed_constraint_affected_rows(
    mylite_stmt *stmt,
    const struct alter_table_mixed_constraint_state *state
) {
    if (state->has_add_foreign_key) {
        stmt->affected_rows = state->foreign_key_affected_rows;
    }
    if (state->check_affected_rows > 0) {
        stmt->affected_rows = state->check_affected_rows;
    }
}

static void rollback_alter_table_mixed_foreign_key_statement(
    mylite_db *database,
    struct mylite_statement_atomicity *atomicity,
    const struct alter_table_mixed_constraint_state *state
) {
    mylite_transaction_rollback_statement_atomicity(database, atomicity);
    if (!state->has_check) {
        mylite_diagnostics_clear_warnings(database);
    }
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
        return mylite_diagnostics_set_schema_access_denied_error(
            stmt->database,
            stmt->alter_table.schema_name
        );
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

static int apply_alter_table_actions(
    mylite_stmt *stmt,
    struct mylite_alter_table_model *model,
    bool defer_foreign_key_actions,
    bool defer_check_actions
) {
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
        case MYLITE_ALTER_TABLE_ACTION_ADD_CHECK:
        case MYLITE_ALTER_TABLE_ACTION_DROP_CHECK:
        case MYLITE_ALTER_TABLE_ACTION_ALTER_CHECK:
            if (defer_check_actions) {
                break;
            }
            status = set_alter_table_unsupported_action_error(
                stmt->database,
                "CHECK ALTER TABLE constraints with other actions"
            );
            break;
        case MYLITE_ALTER_TABLE_ACTION_UNSUPPORTED_CHECK:
            status = set_alter_table_unsupported_action_error(
                stmt->database,
                "CHECK ALTER TABLE constraints"
            );
            break;
        case MYLITE_ALTER_TABLE_ACTION_ADD_FOREIGN_KEY:
        case MYLITE_ALTER_TABLE_ACTION_DROP_FOREIGN_KEY:
            if (defer_foreign_key_actions) {
                break;
            }
            status = set_alter_table_unsupported_action_error(
                stmt->database,
                "FOREIGN KEY ALTER TABLE constraints with other actions"
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

static int apply_alter_table_check_actions(
    mylite_stmt *stmt,
    const struct mylite_alter_table_model *model,
    int64_t *out_affected_rows
) {
    *out_affected_rows = 0;
    for (size_t index = 0U; index < stmt->alter_table.action_count; ++index) {
        struct mylite_alter_table_action *action = &stmt->alter_table.actions[index];
        int status = MYLITE_OK;

        if (!alter_table_action_is_check_action(action->kind)) {
            continue;
        }
        status = apply_alter_table_check_action(stmt, model, action, out_affected_rows);
        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int apply_alter_table_check_action(
    mylite_stmt *stmt,
    const struct mylite_alter_table_model *model,
    struct mylite_alter_table_action *action,
    int64_t *out_affected_rows
) {
    switch (action->kind) {
    case MYLITE_ALTER_TABLE_ACTION_ADD_CHECK:
        return apply_alter_table_add_check_action(stmt, model, &action->check);
    case MYLITE_ALTER_TABLE_ACTION_DROP_CHECK:
        return apply_alter_table_drop_check_action(stmt, action, model->temporary);
    case MYLITE_ALTER_TABLE_ACTION_ALTER_CHECK:
        return apply_alter_table_alter_check_action(stmt, model, action, out_affected_rows);
    case MYLITE_ALTER_TABLE_ACTION_ADD_COLUMN:
    case MYLITE_ALTER_TABLE_ACTION_DROP_COLUMN:
    case MYLITE_ALTER_TABLE_ACTION_RENAME_COLUMN:
    case MYLITE_ALTER_TABLE_ACTION_CHANGE_COLUMN:
    case MYLITE_ALTER_TABLE_ACTION_MODIFY_COLUMN:
    case MYLITE_ALTER_TABLE_ACTION_ADD_PRIMARY_KEY:
    case MYLITE_ALTER_TABLE_ACTION_DROP_PRIMARY_KEY:
    case MYLITE_ALTER_TABLE_ACTION_ADD_UNIQUE_INDEX:
    case MYLITE_ALTER_TABLE_ACTION_ADD_SECONDARY_INDEX:
    case MYLITE_ALTER_TABLE_ACTION_ADD_FULLTEXT_INDEX:
    case MYLITE_ALTER_TABLE_ACTION_ADD_SPATIAL_INDEX:
    case MYLITE_ALTER_TABLE_ACTION_DROP_INDEX:
    case MYLITE_ALTER_TABLE_ACTION_RENAME_INDEX:
    case MYLITE_ALTER_TABLE_ACTION_ALTER_INDEX_VISIBILITY:
    case MYLITE_ALTER_TABLE_ACTION_UNSUPPORTED_CHECK:
    case MYLITE_ALTER_TABLE_ACTION_ADD_FOREIGN_KEY:
    case MYLITE_ALTER_TABLE_ACTION_DROP_FOREIGN_KEY:
    case MYLITE_ALTER_TABLE_ACTION_UNSUPPORTED_FOREIGN_KEY:
    case MYLITE_ALTER_TABLE_ACTION_RENAME_TABLE:
        return MYLITE_MISUSE;
    }
    return MYLITE_MISUSE;
}

static int apply_alter_table_add_check_action(
    mylite_stmt *stmt,
    const struct mylite_alter_table_model *model,
    struct mylite_create_table_check *check
) {
    bool duplicate_name = false;
    int64_t ordinal_position = 0;
    int status = ensure_alter_table_check_constraint_name(stmt, model->temporary, check);

    if (status == MYLITE_OK) {
        status = alter_table_check_constraint_name_exists(
            stmt->database,
            stmt->alter_table.schema_name,
            model->temporary,
            check->name,
            &duplicate_name
        );
    }
    if (status == MYLITE_OK && duplicate_name) {
        status = set_alter_table_duplicate_check_error(stmt->database, check->name);
    }
    if (status == MYLITE_OK && check->enforced) {
        const struct alter_table_check_existing_rows_input input = {
            .constraint_name = check->name,
            .check_clause = check->clause,
        };

        status = validate_alter_table_check_existing_rows(stmt, model, &input);
    }
    if (status == MYLITE_OK) {
        status = max_alter_table_check_ordinal(stmt, model->temporary, &ordinal_position);
    }
    if (status == MYLITE_OK) {
        status = insert_alter_table_check_constraint(
            stmt,
            model->temporary,
            check,
            ordinal_position + 1
        );
    }
    return status;
}

static int apply_alter_table_drop_check_action(
    mylite_stmt *stmt,
    const struct mylite_alter_table_action *action,
    bool temporary
) {
    struct mylite_create_table_check check = {0};
    bool exists = false;
    int status =
        load_alter_table_check_constraint(stmt, temporary, action->old_name, &check, &exists);

    mylite_table_ddl_create_table_check_deinit(&check);
    if (status != MYLITE_OK) {
        return status;
    }
    if (!exists) {
        return set_alter_table_missing_check_error(stmt->database, action->old_name);
    }
    return delete_alter_table_check_constraint(stmt, temporary, action->old_name);
}

static int apply_alter_table_alter_check_action(
    mylite_stmt *stmt,
    const struct mylite_alter_table_model *model,
    const struct mylite_alter_table_action *action,
    int64_t *out_affected_rows
) {
    struct mylite_create_table_check check = {0};
    bool exists = false;
    int status = load_alter_table_check_constraint(
        stmt,
        model->temporary,
        action->old_name,
        &check,
        &exists
    );

    if (status != MYLITE_OK) {
        mylite_table_ddl_create_table_check_deinit(&check);
        return status;
    }
    if (!exists) {
        mylite_table_ddl_create_table_check_deinit(&check);
        return set_alter_table_missing_check_error(stmt->database, action->old_name);
    }
    if (action->check.enforced) {
        const struct alter_table_check_existing_rows_input input = {
            .constraint_name = action->old_name,
            .check_clause = check.clause,
        };

        status = validate_alter_table_check_existing_rows(stmt, model, &input);
    }
    if (status == MYLITE_OK) {
        status = update_alter_table_check_constraint_enforcement(
            stmt,
            model->temporary,
            action->old_name,
            action->check.enforced
        );
    }
    if (status == MYLITE_OK && action->check.enforced && !check.enforced) {
        status = count_alter_table_physical_rows(
            stmt->database,
            model->physical_name,
            out_affected_rows
        );
    }

    mylite_table_ddl_create_table_check_deinit(&check);
    return status;
}

static int apply_alter_table_foreign_key_actions(
    mylite_stmt *stmt,
    struct mylite_alter_table_model *model,
    bool *out_index_catalog_changed
) {
    for (size_t index = 0U; index < stmt->alter_table.action_count; ++index) {
        struct mylite_alter_table_action *action = &stmt->alter_table.actions[index];

        if (action->kind == MYLITE_ALTER_TABLE_ACTION_ADD_FOREIGN_KEY ||
            action->kind == MYLITE_ALTER_TABLE_ACTION_DROP_FOREIGN_KEY) {
            int status = apply_alter_table_foreign_key_action(
                stmt,
                model,
                action,
                out_index_catalog_changed
            );

            if (status != MYLITE_OK) {
                return status;
            }
        }
    }
    return MYLITE_OK;
}

static int apply_alter_table_drop_foreign_key_actions(mylite_stmt *stmt) {
    for (size_t index = 0U; index < stmt->alter_table.action_count; ++index) {
        const struct mylite_alter_table_action *action = &stmt->alter_table.actions[index];

        if (action->kind == MYLITE_ALTER_TABLE_ACTION_DROP_FOREIGN_KEY) {
            int status = apply_alter_table_drop_foreign_key_action(stmt, action);

            if (status != MYLITE_OK) {
                return status;
            }
        }
    }
    return MYLITE_OK;
}

static int apply_alter_table_add_foreign_key_actions(
    mylite_stmt *stmt,
    struct mylite_alter_table_model *model,
    bool *out_index_catalog_changed
) {
    for (size_t index = 0U; index < stmt->alter_table.action_count; ++index) {
        struct mylite_alter_table_action *action = &stmt->alter_table.actions[index];

        if (action->kind == MYLITE_ALTER_TABLE_ACTION_ADD_FOREIGN_KEY) {
            int status = apply_alter_table_add_foreign_key_action(
                stmt,
                model,
                &action->foreign_key,
                out_index_catalog_changed
            );

            if (status != MYLITE_OK) {
                return status;
            }
        }
    }
    return MYLITE_OK;
}

static int apply_alter_table_foreign_key_action(
    mylite_stmt *stmt,
    struct mylite_alter_table_model *model,
    struct mylite_alter_table_action *action,
    bool *out_index_catalog_changed
) {
    switch (action->kind) {
    case MYLITE_ALTER_TABLE_ACTION_ADD_FOREIGN_KEY:
        return apply_alter_table_add_foreign_key_action(
            stmt,
            model,
            &action->foreign_key,
            out_index_catalog_changed
        );
    case MYLITE_ALTER_TABLE_ACTION_DROP_FOREIGN_KEY:
        return apply_alter_table_drop_foreign_key_action(stmt, action);
    case MYLITE_ALTER_TABLE_ACTION_ADD_COLUMN:
    case MYLITE_ALTER_TABLE_ACTION_DROP_COLUMN:
    case MYLITE_ALTER_TABLE_ACTION_RENAME_COLUMN:
    case MYLITE_ALTER_TABLE_ACTION_CHANGE_COLUMN:
    case MYLITE_ALTER_TABLE_ACTION_MODIFY_COLUMN:
    case MYLITE_ALTER_TABLE_ACTION_ADD_PRIMARY_KEY:
    case MYLITE_ALTER_TABLE_ACTION_DROP_PRIMARY_KEY:
    case MYLITE_ALTER_TABLE_ACTION_ADD_UNIQUE_INDEX:
    case MYLITE_ALTER_TABLE_ACTION_ADD_SECONDARY_INDEX:
    case MYLITE_ALTER_TABLE_ACTION_ADD_FULLTEXT_INDEX:
    case MYLITE_ALTER_TABLE_ACTION_ADD_SPATIAL_INDEX:
    case MYLITE_ALTER_TABLE_ACTION_DROP_INDEX:
    case MYLITE_ALTER_TABLE_ACTION_RENAME_INDEX:
    case MYLITE_ALTER_TABLE_ACTION_ALTER_INDEX_VISIBILITY:
    case MYLITE_ALTER_TABLE_ACTION_ADD_CHECK:
    case MYLITE_ALTER_TABLE_ACTION_DROP_CHECK:
    case MYLITE_ALTER_TABLE_ACTION_ALTER_CHECK:
    case MYLITE_ALTER_TABLE_ACTION_UNSUPPORTED_CHECK:
    case MYLITE_ALTER_TABLE_ACTION_UNSUPPORTED_FOREIGN_KEY:
    case MYLITE_ALTER_TABLE_ACTION_RENAME_TABLE:
        return MYLITE_MISUSE;
    }
    return MYLITE_MISUSE;
}

static bool alter_table_has_add_foreign_key_action(const mylite_stmt *stmt) {
    for (size_t index = 0U; index < stmt->alter_table.action_count; ++index) {
        if (stmt->alter_table.actions[index].kind == MYLITE_ALTER_TABLE_ACTION_ADD_FOREIGN_KEY) {
            return true;
        }
    }
    return false;
}

static int count_alter_table_physical_rows(
    mylite_db *database,
    const char *physical_name,
    int64_t *out_row_count
) {
    sqlite3_stmt *select = NULL;
    char *sql = sqlite3_mprintf("SELECT COUNT(*) FROM \"%w\"", physical_name);
    int rc = SQLITE_OK;

    *out_row_count = 0;
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
    if (rc == SQLITE_ROW) {
        *out_row_count = sqlite3_column_int64(select, 0);
    } else if (rc != SQLITE_DONE) {
        sqlite3_finalize(select);
        return mylite_diagnostics_set_sqlite_error(database);
    }
    sqlite3_finalize(select);
    return MYLITE_OK;
}

static int apply_alter_table_add_foreign_key_action(
    mylite_stmt *stmt,
    struct mylite_alter_table_model *model,
    struct mylite_create_table_foreign_key *foreign_key,
    bool *out_index_catalog_changed
) {
    bool parent_exists = false;
    bool duplicate_name = false;
    int status = ensure_alter_table_foreign_key_constraint_name(stmt, foreign_key);

    if (status == MYLITE_OK) {
        status = mylite_foreign_key_catalog_child_constraint_exists(
            stmt->database,
            stmt->alter_table.schema_name,
            foreign_key->constraint_name,
            &duplicate_name
        );
    }
    if (status == MYLITE_OK && duplicate_name) {
        status = set_alter_table_duplicate_foreign_key_error(
            stmt->database,
            foreign_key->constraint_name
        );
    }
    if (status == MYLITE_OK && foreign_key->referenced_schema_name == NULL) {
        foreign_key->referenced_schema_name =
            mylite_copy_nonempty_cstring(stmt->alter_table.schema_name);
        if (foreign_key->referenced_schema_name == NULL) {
            (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
            status = MYLITE_NOMEM;
        }
    }
    if (status == MYLITE_OK) {
        status = validate_alter_table_foreign_key_columns(stmt->database, model, foreign_key);
    }
    if (status == MYLITE_OK) {
        status = resolve_alter_table_foreign_key_parent(stmt, model, foreign_key, &parent_exists);
    }
    if (status == MYLITE_OK) {
        status = ensure_alter_table_foreign_key_supporting_index(
            stmt,
            model,
            foreign_key,
            out_index_catalog_changed
        );
    }
    if (status == MYLITE_OK) {
        status =
            validate_alter_table_foreign_key_existing_rows(stmt, model, foreign_key, parent_exists);
    }
    if (status == MYLITE_OK) {
        status = insert_alter_table_foreign_key_catalog_rows(stmt, foreign_key);
    }
    return status;
}

static int apply_alter_table_drop_foreign_key_action(
    mylite_stmt *stmt,
    const struct mylite_alter_table_action *action
) {
    bool exists = false;
    int status = mylite_foreign_key_catalog_child_constraint_exists(
        stmt->database,
        stmt->alter_table.schema_name,
        action->old_name,
        &exists
    );

    if (status != MYLITE_OK) {
        return status;
    }
    if (!exists) {
        return mylite_table_ddl_set_alter_table_cant_drop_column_error(
            stmt->database,
            action->old_name
        );
    }
    return mylite_foreign_key_catalog_delete_child_constraint(
        stmt->database,
        stmt->alter_table.schema_name,
        stmt->alter_table.table_name,
        action->old_name,
        false
    );
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

static int ensure_alter_table_check_constraint_name(
    mylite_stmt *stmt,
    bool temporary,
    struct mylite_create_table_check *check
) {
    if (check->name != NULL) {
        return MYLITE_OK;
    }
    return generated_alter_table_check_constraint_name(stmt, temporary, &check->name);
}

static int generated_alter_table_check_constraint_name(
    mylite_stmt *stmt,
    bool temporary,
    char **out_name
) {
    int64_t max_suffix = 0;
    int status = max_alter_table_generated_check_suffix(stmt, temporary, &max_suffix);

    *out_name = NULL;
    if (status != MYLITE_OK) {
        return status;
    }
    for (int64_t suffix = max_suffix + 1; suffix > 0; ++suffix) {
        bool exists = false;
        char *candidate =
            sqlite3_mprintf("%s_chk_%lld", stmt->alter_table.table_name, (long long)suffix);

        if (candidate == NULL) {
            (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
            return MYLITE_NOMEM;
        }
        status = alter_table_check_constraint_name_exists(
            stmt->database,
            stmt->alter_table.schema_name,
            temporary,
            candidate,
            &exists
        );
        if (status != MYLITE_OK) {
            sqlite3_free(candidate);
            return status;
        }
        if (!exists) {
            *out_name = mylite_copy_nonempty_cstring(candidate);
            sqlite3_free(candidate);
            if (*out_name == NULL) {
                (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
                return MYLITE_NOMEM;
            }
            return MYLITE_OK;
        }
        sqlite3_free(candidate);
    }

    (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
    return MYLITE_NOMEM;
}

static int max_alter_table_generated_check_suffix(
    mylite_stmt *stmt,
    bool temporary,
    int64_t *out_suffix
) {
    sqlite3_stmt *select = NULL;
    char *sql = sqlite3_mprintf(
        "SELECT constraint_name FROM %s WHERE table_schema = ? AND table_name = ?",
        mylite_catalog_check_constraint_catalog_name(temporary)
    );
    int rc = SQLITE_OK;

    *out_suffix = 0;
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
    sqlite3_bind_text(select, 1, stmt->alter_table.schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(select, 2, stmt->alter_table.table_name, -1, sqlite_transient_destructor());

    while ((rc = sqlite3_step(select)) == SQLITE_ROW) {
        const unsigned char *constraint_name = sqlite3_column_text(select, 0);
        int64_t suffix = 0;

        if (constraint_name != NULL &&
            alter_table_generated_check_suffix(
                stmt->alter_table.table_name,
                (const char *)constraint_name,
                &suffix
            ) &&
            suffix > *out_suffix) {
            *out_suffix = suffix;
        }
    }
    if (rc != SQLITE_DONE) {
        sqlite3_finalize(select);
        return mylite_diagnostics_set_sqlite_error(stmt->database);
    }
    sqlite3_finalize(select);
    return MYLITE_OK;
}

static bool alter_table_generated_check_suffix(
    const char *table_name,
    const char *constraint_name,
    int64_t *out_suffix
) {
    enum {
        decimal_radix = 10,
    };

    static const char separator[] = "_chk_";
    size_t table_name_length = strlen(table_name);
    size_t constraint_name_length = strlen(constraint_name);
    size_t separator_length = sizeof(separator) - 1U;
    const char *suffix_start = constraint_name + table_name_length + separator_length;
    int64_t suffix = 0;

    *out_suffix = 0;
    if (constraint_name_length <= table_name_length + separator_length ||
        strncmp(constraint_name, table_name, table_name_length) != 0 ||
        strncmp(constraint_name + table_name_length, separator, separator_length) != 0 ||
        *suffix_start == '\0') {
        return false;
    }
    for (const char *cursor = suffix_start; *cursor != '\0'; ++cursor) {
        int digit = *cursor - '0';

        if (*cursor < '0' || *cursor > '9' || suffix > (INT64_MAX - digit) / decimal_radix) {
            return false;
        }
        suffix = (suffix * decimal_radix) + digit;
    }
    if (suffix <= 0) {
        return false;
    }
    *out_suffix = suffix;
    return true;
}

static int max_alter_table_check_ordinal(mylite_stmt *stmt, bool temporary, int64_t *out_ordinal) {
    sqlite3_stmt *select = NULL;
    char *sql = sqlite3_mprintf(
        "SELECT COALESCE(MAX(ordinal_position), 0) FROM %s "
        "WHERE table_schema = ? AND table_name = ?",
        mylite_catalog_check_constraint_catalog_name(temporary)
    );
    int rc = SQLITE_OK;

    *out_ordinal = 0;
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
    sqlite3_bind_text(select, 1, stmt->alter_table.schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(select, 2, stmt->alter_table.table_name, -1, sqlite_transient_destructor());
    rc = sqlite3_step(select);
    if (rc == SQLITE_ROW) {
        *out_ordinal = sqlite3_column_int64(select, 0);
    } else if (rc != SQLITE_DONE) {
        sqlite3_finalize(select);
        return mylite_diagnostics_set_sqlite_error(stmt->database);
    }
    sqlite3_finalize(select);
    return MYLITE_OK;
}

static int alter_table_check_constraint_name_exists(
    mylite_db *database,
    const char *schema_name,
    bool temporary,
    const char *constraint_name,
    bool *out_exists
) {
    sqlite3_stmt *select = NULL;
    char *sql = sqlite3_mprintf(
        "SELECT 1 FROM %s WHERE constraint_schema = ? AND constraint_name = ? LIMIT 1",
        mylite_catalog_check_constraint_catalog_name(temporary)
    );
    int rc = SQLITE_OK;

    *out_exists = false;
    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &select, NULL);
    sqlite3_free(sql);
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

static int load_alter_table_check_constraint(
    mylite_stmt *stmt,
    bool temporary,
    const char *constraint_name,
    struct mylite_create_table_check *out_check,
    bool *out_exists
) {
    sqlite3_stmt *select = NULL;
    char *sql = sqlite3_mprintf(
        "SELECT constraint_name, check_clause, enforced FROM %s "
        "WHERE table_schema = ? AND table_name = ? AND constraint_name = ?",
        mylite_catalog_check_constraint_catalog_name(temporary)
    );
    int rc = SQLITE_OK;

    *out_check = (struct mylite_create_table_check){0};
    *out_exists = false;
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
    sqlite3_bind_text(select, 1, stmt->alter_table.schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(select, 2, stmt->alter_table.table_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(select, 3, constraint_name, -1, sqlite_transient_destructor());

    rc = sqlite3_step(select);
    if (rc == SQLITE_ROW) {
        const char *name = (const char *)sqlite3_column_text(select, 0);
        const char *clause = (const char *)sqlite3_column_text(select, 1);
        const char *enforced = (const char *)sqlite3_column_text(select, 2);

        out_check->name = mylite_copy_nonempty_cstring(name);
        out_check->clause = mylite_copy_nonempty_cstring(clause);
        out_check->enforced = false;
        if (enforced != NULL && strcmp(enforced, "YES") == 0) {
            out_check->enforced = true;
        }
        if (out_check->name == NULL || out_check->clause == NULL) {
            sqlite3_finalize(select);
            mylite_table_ddl_create_table_check_deinit(out_check);
            (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
            return MYLITE_NOMEM;
        }
        *out_exists = true;
    } else if (rc != SQLITE_DONE) {
        sqlite3_finalize(select);
        return mylite_diagnostics_set_sqlite_error(stmt->database);
    }
    sqlite3_finalize(select);
    return MYLITE_OK;
}

static int insert_alter_table_check_constraint(
    mylite_stmt *stmt,
    bool temporary,
    const struct mylite_create_table_check *check,
    int64_t ordinal_position
) {
    enum {
        bind_constraint_schema = 1,
        bind_constraint_name = 2,
        bind_table_schema = 3,
        bind_table_name = 4,
        bind_check_clause = 5,
        bind_enforced = 6,
        bind_ordinal_position = 7,
    };

    sqlite3_stmt *insert = NULL;
    char *sql = sqlite3_mprintf(
        "INSERT INTO %s("
        "constraint_catalog, constraint_schema, constraint_name, table_schema, table_name, "
        "check_clause, enforced, ordinal_position) VALUES('def', ?, ?, ?, ?, ?, ?, ?)",
        mylite_catalog_check_constraint_catalog_name(temporary)
    );
    const char *enforced = "NO";
    int rc = SQLITE_OK;

    if (check->enforced) {
        enforced = "YES";
    }

    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    rc = sqlite3_prepare_v3(
        stmt->database->sqlite,
        sql,
        -1,
        SQLITE_PREPARE_PERSISTENT,
        &insert,
        NULL
    );
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(stmt->database);
    }
    sqlite3_bind_text(
        insert,
        bind_constraint_schema,
        stmt->alter_table.schema_name,
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_text(insert, bind_constraint_name, check->name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(
        insert,
        bind_table_schema,
        stmt->alter_table.schema_name,
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_text(
        insert,
        bind_table_name,
        stmt->alter_table.table_name,
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_text(insert, bind_check_clause, check->clause, -1, sqlite_transient_destructor());
    sqlite3_bind_text(insert, bind_enforced, enforced, -1, SQLITE_STATIC);
    sqlite3_bind_int64(insert, bind_ordinal_position, ordinal_position);

    rc = sqlite3_step(insert);
    sqlite3_finalize(insert);
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(stmt->database);
}

static int delete_alter_table_check_constraint(
    mylite_stmt *stmt,
    bool temporary,
    const char *constraint_name
) {
    sqlite3_stmt *delete_stmt = NULL;
    char *sql = sqlite3_mprintf(
        "DELETE FROM %s WHERE table_schema = ? AND table_name = ? AND constraint_name = ?",
        mylite_catalog_check_constraint_catalog_name(temporary)
    );
    int rc = SQLITE_OK;

    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    rc = sqlite3_prepare_v3(
        stmt->database->sqlite,
        sql,
        -1,
        SQLITE_PREPARE_PERSISTENT,
        &delete_stmt,
        NULL
    );
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(stmt->database);
    }
    sqlite3_bind_text(
        delete_stmt,
        1,
        stmt->alter_table.schema_name,
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_text(
        delete_stmt,
        2,
        stmt->alter_table.table_name,
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_text(delete_stmt, 3, constraint_name, -1, sqlite_transient_destructor());
    rc = sqlite3_step(delete_stmt);
    sqlite3_finalize(delete_stmt);
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(stmt->database);
}

static int update_alter_table_check_constraint_enforcement(
    mylite_stmt *stmt,
    bool temporary,
    const char *constraint_name,
    bool enforced
) {
    sqlite3_stmt *update = NULL;
    char *sql = sqlite3_mprintf(
        "UPDATE %s SET enforced = ? "
        "WHERE table_schema = ? AND table_name = ? AND constraint_name = ?",
        mylite_catalog_check_constraint_catalog_name(temporary)
    );
    int rc = SQLITE_OK;

    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    rc = sqlite3_prepare_v3(
        stmt->database->sqlite,
        sql,
        -1,
        SQLITE_PREPARE_PERSISTENT,
        &update,
        NULL
    );
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(stmt->database);
    }
    if (enforced) {
        sqlite3_bind_text(update, 1, "YES", -1, SQLITE_STATIC);
    } else {
        sqlite3_bind_text(update, 1, "NO", -1, SQLITE_STATIC);
    }
    sqlite3_bind_text(update, 2, stmt->alter_table.schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(update, 3, stmt->alter_table.table_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(update, 4, constraint_name, -1, sqlite_transient_destructor());
    rc = sqlite3_step(update);
    sqlite3_finalize(update);
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(stmt->database);
}

static int validate_alter_table_check_existing_rows(
    mylite_stmt *stmt,
    const struct mylite_alter_table_model *model,
    const struct alter_table_check_existing_rows_input *input
) {
    sqlite3_stmt *select = NULL;
    char *sql =
        build_alter_table_check_existing_rows_sql(model->physical_name, input->check_clause);
    int rc = SQLITE_OK;

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
        return set_alter_table_check_violation_error(stmt->database, input->constraint_name);
    }
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(stmt->database);
}

static char *build_alter_table_check_existing_rows_sql(
    const char *physical_name,
    const char *check_clause
) {
    sqlite3_str *sql = sqlite3_str_new(NULL);

    if (sql == NULL) {
        return NULL;
    }
    sqlite3_str_appendf(
        sql,
        "SELECT 1 FROM \"%w\" WHERE NOT ((%s) OR (%s) IS NULL) LIMIT 1",
        physical_name,
        check_clause,
        check_clause
    );
    return sqlite3_str_finish(sql);
}

static int set_alter_table_duplicate_check_error(mylite_db *database, const char *constraint_name) {
    char *message = sqlite3_mprintf("Duplicate check constraint name '%q'.", constraint_name);
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_set_error_message(database, message);
    if (status == MYLITE_OK) {
        status = mylite_diagnostics_append_error(
            database,
            MYLITE_MYSQL_ER_CHECK_CONSTRAINT_DUP_NAME,
            message
        );
    }
    sqlite3_free(message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int set_alter_table_missing_check_error(mylite_db *database, const char *constraint_name) {
    char *message =
        sqlite3_mprintf("Check constraint '%q' is not found in the table.", constraint_name);
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_set_error_message(database, message);
    if (status == MYLITE_OK) {
        status = mylite_diagnostics_append_error(
            database,
            MYLITE_MYSQL_ER_CHECK_CONSTRAINT_NOT_FOUND,
            message
        );
    }
    sqlite3_free(message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int set_alter_table_check_violation_error(mylite_db *database, const char *constraint_name) {
    char *message = sqlite3_mprintf(
        "Check constraint '%q' is violated.",
        constraint_name == NULL ? "" : constraint_name
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
            MYLITE_MYSQL_ER_CHECK_CONSTRAINT_VIOLATED,
            message
        );
    }
    sqlite3_free(message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int ensure_alter_table_foreign_key_constraint_name(
    mylite_stmt *stmt,
    struct mylite_create_table_foreign_key *foreign_key
) {
    if (foreign_key->constraint_name != NULL) {
        return MYLITE_OK;
    }

    foreign_key->constraint_name = generated_alter_table_foreign_key_constraint_name(stmt);
    if (foreign_key->constraint_name == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    return MYLITE_OK;
}

static char *generated_alter_table_foreign_key_constraint_name(mylite_stmt *stmt) {
    for (unsigned int suffix = 1U;; ++suffix) {
        bool exists = false;
        char *candidate = sqlite3_mprintf("%s_ibfk_%u", stmt->alter_table.table_name, suffix);
        int status = MYLITE_OK;

        if (candidate == NULL) {
            return NULL;
        }
        status = mylite_foreign_key_catalog_child_constraint_exists(
            stmt->database,
            stmt->alter_table.schema_name,
            candidate,
            &exists
        );
        if (status != MYLITE_OK) {
            sqlite3_free(candidate);
            return NULL;
        }
        if (!exists) {
            char *copy = mylite_copy_nonempty_cstring(candidate);

            sqlite3_free(candidate);
            return copy;
        }
        sqlite3_free(candidate);
    }
}

static int validate_alter_table_foreign_key_columns(
    mylite_db *database,
    const struct mylite_alter_table_model *model,
    const struct mylite_create_table_foreign_key *foreign_key
) {
    for (size_t index = 0U; index < foreign_key->column_count; ++index) {
        if (mylite_table_ddl_find_alter_table_column(model, foreign_key->column_names[index]) ==
            NULL) {
            (void)mylite_diagnostics_set_error_message_parts(
                database,
                "Key column '",
                foreign_key->column_names[index],
                "' doesn't exist in table"
            );
            return MYLITE_EXEC_ERROR;
        }
    }
    return MYLITE_OK;
}

static int resolve_alter_table_foreign_key_parent(
    mylite_stmt *stmt,
    const struct mylite_alter_table_model *model,
    struct mylite_create_table_foreign_key *foreign_key,
    bool *out_parent_exists
) {
    char *unique_constraint_name = NULL;
    bool found_unique_constraint = false;
    int status = MYLITE_OK;

    *out_parent_exists = false;
    free(foreign_key->unique_constraint_name);
    foreign_key->unique_constraint_name = NULL;

    if (mylite_ascii_case_equal(
            foreign_key->referenced_schema_name,
            stmt->alter_table.schema_name
        ) &&
        mylite_ascii_case_equal(foreign_key->referenced_table_name, stmt->alter_table.table_name)) {
        *out_parent_exists = true;
        status = resolve_self_referenced_alter_table_unique_constraint(
            model,
            foreign_key,
            &unique_constraint_name,
            &found_unique_constraint
        );
    } else {
        status = mylite_catalog_persistent_table_exists(
            stmt->database,
            foreign_key->referenced_schema_name,
            foreign_key->referenced_table_name,
            out_parent_exists
        );
        if (status == MYLITE_OK && *out_parent_exists) {
            status = resolve_referenced_alter_table_unique_constraint(
                stmt->database,
                foreign_key,
                &unique_constraint_name,
                &found_unique_constraint
            );
        }
    }

    if (status != MYLITE_OK) {
        free(unique_constraint_name);
        return status;
    }
    if (!*out_parent_exists && !mylite_connection_foreign_key_checks(stmt->database)) {
        return MYLITE_OK;
    }
    if (!*out_parent_exists) {
        return set_alter_table_missing_referenced_table_error(
            stmt->database,
            foreign_key->referenced_table_name
        );
    }
    if (!found_unique_constraint) {
        free(unique_constraint_name);
        return set_alter_table_cannot_add_foreign_key_error(stmt->database);
    }
    foreign_key->unique_constraint_name = unique_constraint_name;
    return MYLITE_OK;
}

static int resolve_self_referenced_alter_table_unique_constraint(
    const struct mylite_alter_table_model *model,
    const struct mylite_create_table_foreign_key *foreign_key,
    char **out_constraint_name,
    bool *out_found
) {
    *out_constraint_name = NULL;
    *out_found = false;
    for (size_t index = 0U; index < model->index_count; ++index) {
        const struct mylite_alter_table_index *candidate = &model->indexes[index];
        bool matches = false;

        if (candidate->non_unique == 0 &&
            candidate->part_count == foreign_key->referenced_column_count) {
            matches = true;
        }

        for (size_t part = 0U; matches && part < candidate->part_count; ++part) {
            matches = mylite_ascii_case_equal(
                candidate->parts[part].column_name,
                foreign_key->referenced_column_names[part]
            );
        }
        if (matches) {
            *out_constraint_name = mylite_copy_nonempty_cstring(candidate->name);
            if (*out_constraint_name == NULL) {
                return MYLITE_NOMEM;
            }
            *out_found = true;
            return MYLITE_OK;
        }
    }
    return MYLITE_OK;
}

static int resolve_referenced_alter_table_unique_constraint(
    mylite_db *database,
    const struct mylite_create_table_foreign_key *foreign_key,
    char **out_constraint_name,
    bool *out_found
) {
    static const char sql[] =
        "SELECT index_name FROM __mylite_index_catalog "
        "WHERE table_schema = ? AND table_name = ? AND non_unique = 0 "
        "GROUP BY index_name "
        "ORDER BY CASE WHEN index_name = 'PRIMARY' THEN 0 ELSE 1 END, MIN(rowid)";
    sqlite3_stmt *select = NULL;
    int rc = SQLITE_OK;

    *out_constraint_name = NULL;
    *out_found = false;
    rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &select, NULL);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    sqlite3_bind_text(
        select,
        1,
        foreign_key->referenced_schema_name,
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_text(
        select,
        2,
        foreign_key->referenced_table_name,
        -1,
        sqlite_transient_destructor()
    );

    while ((rc = sqlite3_step(select)) == SQLITE_ROW) {
        const char *candidate_name = (const char *)sqlite3_column_text(select, 0);
        bool matches = false;
        int status = MYLITE_OK;

        if (candidate_name != NULL) {
            status = alter_table_referenced_unique_candidate_matches(
                database,
                candidate_name,
                foreign_key,
                &matches
            );
            if (status != MYLITE_OK) {
                sqlite3_finalize(select);
                return status;
            }
        }
        if (candidate_name != NULL && matches) {
            *out_constraint_name = mylite_copy_nonempty_cstring(candidate_name);
            if (*out_constraint_name == NULL) {
                sqlite3_finalize(select);
                return MYLITE_NOMEM;
            }
            *out_found = true;
            break;
        }
    }

    sqlite3_finalize(select);
    return rc == SQLITE_DONE || *out_found ? MYLITE_OK
                                           : mylite_diagnostics_set_sqlite_error(database);
}

static int alter_table_referenced_unique_candidate_matches(
    mylite_db *database,
    const char *candidate_name,
    const struct mylite_create_table_foreign_key *foreign_key,
    bool *out_matches
) {
    static const char sql[] = "SELECT column_name FROM __mylite_index_catalog "
                              "WHERE table_schema = ? AND table_name = ? AND index_name = ? "
                              "ORDER BY seq_in_index";
    sqlite3_stmt *select = NULL;
    size_t matched = 0U;
    int rc = SQLITE_OK;

    *out_matches = false;
    rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &select, NULL);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    sqlite3_bind_text(
        select,
        1,
        foreign_key->referenced_schema_name,
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_text(
        select,
        2,
        foreign_key->referenced_table_name,
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_text(select, 3, candidate_name, -1, sqlite_transient_destructor());

    while ((rc = sqlite3_step(select)) == SQLITE_ROW) {
        const char *column_name = (const char *)sqlite3_column_text(select, 0);

        if (matched >= foreign_key->referenced_column_count || column_name == NULL ||
            !mylite_ascii_case_equal(column_name, foreign_key->referenced_column_names[matched])) {
            sqlite3_finalize(select);
            return MYLITE_OK;
        }
        ++matched;
    }

    if (rc != SQLITE_DONE) {
        sqlite3_finalize(select);
        return mylite_diagnostics_set_sqlite_error(database);
    }
    *out_matches = matched == foreign_key->referenced_column_count;
    sqlite3_finalize(select);
    return MYLITE_OK;
}

static int ensure_alter_table_foreign_key_supporting_index(
    mylite_stmt *stmt,
    struct mylite_alter_table_model *model,
    struct mylite_create_table_foreign_key *foreign_key,
    bool *out_index_catalog_changed
) {
    const char *existing_name = alter_table_supporting_foreign_key_index_name(model, foreign_key);
    char *copy = NULL;
    int status = MYLITE_OK;

    if (existing_name != NULL) {
        copy = mylite_copy_nonempty_cstring(existing_name);
        if (copy == NULL) {
            (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
            return MYLITE_NOMEM;
        }
        free(foreign_key->supporting_index_name);
        foreign_key->supporting_index_name = copy;
        return MYLITE_OK;
    }

    status = assign_alter_table_foreign_key_supporting_index_name(stmt, model, foreign_key);
    if (status == MYLITE_OK) {
        status = add_alter_table_foreign_key_supporting_index(stmt, model, foreign_key);
    }
    if (status == MYLITE_OK) {
        *out_index_catalog_changed = true;
    }
    return status;
}

static const char *alter_table_supporting_foreign_key_index_name(
    const struct mylite_alter_table_model *model,
    const struct mylite_create_table_foreign_key *foreign_key
) {
    for (size_t index = 0U; index < model->index_count; ++index) {
        if (alter_table_index_supports_foreign_key(&model->indexes[index], foreign_key)) {
            return model->indexes[index].name;
        }
    }
    return NULL;
}

static bool alter_table_index_supports_foreign_key(
    const struct mylite_alter_table_index *index,
    const struct mylite_create_table_foreign_key *foreign_key
) {
    if (index->part_count < foreign_key->column_count) {
        return false;
    }
    for (size_t part = 0U; part < foreign_key->column_count; ++part) {
        if (!mylite_ascii_case_equal(
                index->parts[part].column_name,
                foreign_key->column_names[part]
            )) {
            return false;
        }
    }
    return true;
}

static int assign_alter_table_foreign_key_supporting_index_name(
    mylite_stmt *stmt,
    const struct mylite_alter_table_model *model,
    struct mylite_create_table_foreign_key *foreign_key
) {
    struct mylite_create_table_index source = {0};
    char *generated_name = NULL;
    int status = MYLITE_OK;

    if (foreign_key->supporting_index_name != NULL) {
        return MYLITE_OK;
    }

    source.part_count = 1U;
    source.parts = calloc(1U, sizeof(*source.parts));
    if (source.parts == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    source.parts[0].column_name = mylite_copy_nonempty_cstring(foreign_key->column_names[0]);
    if (source.parts[0].column_name == NULL) {
        mylite_table_ddl_create_table_index_deinit(&source);
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    status = mylite_table_ddl_assign_alter_table_generated_index_name(
        stmt->database,
        model,
        &source,
        &generated_name
    );
    mylite_table_ddl_create_table_index_deinit(&source);
    if (status != MYLITE_OK) {
        return status;
    }
    foreign_key->supporting_index_name = generated_name;
    return MYLITE_OK;
}

static int add_alter_table_foreign_key_supporting_index(
    mylite_stmt *stmt,
    struct mylite_alter_table_model *model,
    const struct mylite_create_table_foreign_key *foreign_key
) {
    struct mylite_alter_table_action action = {
        .kind = MYLITE_ALTER_TABLE_ACTION_ADD_SECONDARY_INDEX,
    };
    const struct mylite_table_ddl_alter_callbacks callbacks = {0};
    int status = MYLITE_OK;

    action.index.name = mylite_copy_nonempty_cstring(foreign_key->supporting_index_name);
    action.index.algorithm = MYLITE_SQL_AST_INDEX_ALGORITHM_BTREE;
    action.index.is_visible = true;
    action.index.explicit_name = true;
    if (action.index.name == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    for (size_t index = 0U; index < foreign_key->column_count; ++index) {
        struct mylite_create_table_key_part *parts =
            realloc(action.index.parts, (action.index.part_count + 1U) * sizeof(*parts));
        struct mylite_create_table_key_part part = {0};

        if (parts == NULL) {
            mylite_table_ddl_alter_table_action_deinit(&action);
            (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
            return MYLITE_NOMEM;
        }
        action.index.parts = parts;
        part = (struct mylite_create_table_key_part){
            .column_name = mylite_copy_nonempty_cstring(foreign_key->column_names[index]),
            .order = MYLITE_SQL_AST_KEY_PART_ORDER_NONE,
        };
        if (part.column_name == NULL) {
            mylite_table_ddl_alter_table_action_deinit(&action);
            (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
            return MYLITE_NOMEM;
        }
        action.index.parts[action.index.part_count++] = part;
    }

    status =
        mylite_table_ddl_apply_alter_table_index_action(stmt->database, &action, model, &callbacks);
    mylite_table_ddl_alter_table_action_deinit(&action);
    return status;
}

static int validate_alter_table_foreign_key_existing_rows(
    mylite_stmt *stmt,
    const struct mylite_alter_table_model *model,
    const struct mylite_create_table_foreign_key *foreign_key,
    bool parent_exists
) {
    sqlite3_stmt *select = NULL;
    char *parent_physical_name = NULL;
    char *sql = NULL;
    int rc = SQLITE_OK;

    if (!mylite_connection_foreign_key_checks(stmt->database) || !parent_exists) {
        return MYLITE_OK;
    }

    parent_physical_name = mylite_catalog_physical_table_name(
        foreign_key->referenced_schema_name,
        foreign_key->referenced_table_name
    );
    if (parent_physical_name == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    const struct alter_table_foreign_key_existing_rows_sql_input input = {
        .child_physical_name = model->physical_name,
        .parent_physical_name = parent_physical_name,
        .model = model,
        .foreign_key = foreign_key,
    };
    sql = build_alter_table_foreign_key_existing_rows_sql(stmt->database, &input);
    free(parent_physical_name);
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
        return set_alter_table_foreign_key_existing_row_error(
            stmt->database,
            stmt->alter_table.schema_name,
            stmt->alter_table.table_name,
            foreign_key->constraint_name
        );
    }
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(stmt->database);
}

static char *build_alter_table_foreign_key_existing_rows_sql(
    mylite_db *database,
    const struct alter_table_foreign_key_existing_rows_sql_input *input
) {
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);
    const struct mylite_create_table_foreign_key *foreign_key = input->foreign_key;
    bool skip_check = false;

    if (sql == NULL) {
        return NULL;
    }
    sqlite3_str_appendf(sql, "SELECT 1 FROM \"%w\" AS child WHERE ", input->child_physical_name);
    for (size_t index = 0U; index < foreign_key->column_count; ++index) {
        int status = MYLITE_OK;

        if (index != 0U) {
            sqlite3_str_appendall(sql, " AND ");
        }
        status = append_alter_table_foreign_key_child_value_sql(
            database,
            sql,
            input->model,
            foreign_key->column_names[index],
            &skip_check
        );
        if (status != MYLITE_OK || skip_check) {
            sqlite3_free(sqlite3_str_finish(sql));
            return status == MYLITE_OK ? sqlite3_mprintf("SELECT 1 WHERE 0") : NULL;
        }
        sqlite3_str_appendall(sql, " IS NOT NULL");
    }
    sqlite3_str_appendf(
        sql,
        " AND NOT EXISTS (SELECT 1 FROM \"%w\" AS parent WHERE ",
        input->parent_physical_name
    );
    for (size_t index = 0U; index < foreign_key->column_count; ++index) {
        int status = MYLITE_OK;

        if (index != 0U) {
            sqlite3_str_appendall(sql, " AND ");
        }
        sqlite3_str_appendf(sql, "parent.\"%w\" = ", foreign_key->referenced_column_names[index]);
        status = append_alter_table_foreign_key_child_value_sql(
            database,
            sql,
            input->model,
            foreign_key->column_names[index],
            &skip_check
        );
        if (status != MYLITE_OK || skip_check) {
            sqlite3_free(sqlite3_str_finish(sql));
            return status == MYLITE_OK ? sqlite3_mprintf("SELECT 1 WHERE 0") : NULL;
        }
    }
    sqlite3_str_appendall(sql, ") LIMIT 1");
    return sqlite3_str_finish(sql);
}

static int append_alter_table_foreign_key_child_value_sql(
    mylite_db *database,
    sqlite3_str *sql,
    const struct mylite_alter_table_model *model,
    const char *column_name,
    bool *out_skip_check
) {
    const struct mylite_alter_table_column *column =
        mylite_table_ddl_find_alter_table_column(model, column_name);
    struct mylite_insert_bound_value value = {0};
    int status = MYLITE_OK;

    *out_skip_check = false;
    if (column == NULL) {
        return mylite_table_ddl_set_alter_table_unknown_column_error(
            database,
            model->table_name,
            column_name
        );
    }
    if (column->source_name != NULL) {
        sqlite3_str_appendf(sql, "child.\"%w\"", column->source_name);
        return sqlite3_str_errcode(sql) == SQLITE_OK ? MYLITE_OK : MYLITE_NOMEM;
    }

    status = mylite_table_ddl_resolve_alter_table_added_column_value(database, column, &value);
    if (status != MYLITE_OK) {
        return status;
    }
    switch (value.kind) {
    case MYLITE_INSERT_BOUND_NULL:
        *out_skip_check = true;
        break;
    case MYLITE_INSERT_BOUND_INTEGER:
        sqlite3_str_appendf(sql, "%lld", (long long)value.integer_value);
        break;
    case MYLITE_INSERT_BOUND_REAL:
        sqlite3_str_appendf(sql, "%!.17g", value.real_value);
        break;
    case MYLITE_INSERT_BOUND_TEXT:
        sqlite3_str_appendf(sql, "%Q", value.text_value == NULL ? "" : value.text_value);
        break;
    }
    mylite_dml_insert_bound_value_deinit(&value);
    if (sqlite3_str_errcode(sql) != SQLITE_OK) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    return MYLITE_OK;
}

static int insert_alter_table_foreign_key_catalog_rows(
    mylite_stmt *stmt,
    const struct mylite_create_table_foreign_key *foreign_key
) {
    return mylite_table_ddl_insert_foreign_key_catalog_rows(
        stmt->database,
        stmt->alter_table.schema_name,
        stmt->alter_table.table_name,
        false,
        foreign_key,
        1U
    );
}

static int set_alter_table_duplicate_foreign_key_error(
    mylite_db *database,
    const char *constraint_name
) {
    int status = mylite_diagnostics_set_error_message_parts(
        database,
        "Duplicate foreign key constraint name '",
        constraint_name,
        "'"
    );

    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int set_alter_table_cannot_add_foreign_key_error(mylite_db *database) {
    int status =
        mylite_diagnostics_set_error_message(database, "Cannot add foreign key constraint");

    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int set_alter_table_missing_referenced_table_error(
    mylite_db *database,
    const char *table_name
) {
    int status = mylite_diagnostics_set_error_message_parts(
        database,
        "Failed to open the referenced table '",
        table_name,
        "'"
    );

    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int set_alter_table_foreign_key_existing_row_error(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    const char *constraint_name
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
    status = mylite_diagnostics_set_error_message(database, message);
    if (status == MYLITE_OK) {
        status =
            mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_NO_REFERENCED_ROW_2, message);
    }
    sqlite3_free(message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
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

static sqlite3_destructor_type sqlite_transient_destructor(void) {
    // SQLite's public macro intentionally uses this sentinel pointer value.
    return SQLITE_TRANSIENT; // NOLINT(performance-no-int-to-ptr)
}
