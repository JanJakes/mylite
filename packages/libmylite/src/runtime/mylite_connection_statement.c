#include "mylite_connection_statement.h"

#include "mylite_charset.h"
#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "mylite_runtime.h"
#include "mylite_span.h"
#include "mylite_statement.h"
#include "mylite_user_variables.h"
#include "sqlite3.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int copy_connection_charset_statement(
    const struct mylite_sql_ast_node *statement,
    struct mylite_connection_charset_plan *plan
);

static int execute_set_default_storage_engine_statement(
    mylite_stmt *stmt,
    const struct mylite_connection_system_variable_plan *plan
);

static int execute_set_foreign_key_checks_statement(
    mylite_stmt *stmt,
    const struct mylite_connection_system_variable_plan *plan
);

static int execute_set_group_concat_max_len_statement(
    mylite_stmt *stmt,
    const struct mylite_connection_system_variable_plan *plan
);

static int execute_set_time_zone_statement(
    mylite_stmt *stmt,
    const struct mylite_connection_system_variable_plan *plan
);

static int execute_set_sql_mode_statement(
    mylite_stmt *stmt,
    const struct mylite_connection_system_variable_plan *plan
);

static int execute_set_sql_notes_statement(
    mylite_stmt *stmt,
    const struct mylite_connection_system_variable_plan *plan
);

static int execute_set_sql_log_bin_statement(
    mylite_stmt *stmt,
    const struct mylite_connection_system_variable_plan *plan
);

static int execute_set_unique_checks_statement(
    mylite_stmt *stmt,
    const struct mylite_connection_system_variable_plan *plan
);

static int execute_set_wait_timeout_statement(
    mylite_stmt *stmt,
    const struct mylite_connection_system_variable_plan *plan
);

static int execute_set_rand_seed_statement(
    mylite_stmt *stmt,
    const struct mylite_connection_system_variable_plan *plan
);

static int execute_set_character_set_client_statement(
    mylite_stmt *stmt,
    const struct mylite_connection_system_variable_plan *plan
);

static int execute_set_character_set_results_statement(
    mylite_stmt *stmt,
    const struct mylite_connection_system_variable_plan *plan
);

static int execute_set_collation_connection_statement(
    mylite_stmt *stmt,
    const struct mylite_connection_system_variable_plan *plan
);

static int execute_set_default_collation_for_utf8mb4_statement(
    mylite_stmt *stmt,
    const struct mylite_connection_system_variable_plan *plan
);

static int copy_connection_group_concat_max_len_value(
    mylite_db *database,
    const struct mylite_sql_ast_node *value,
    struct mylite_connection_system_variable_plan *plan
);

static int copy_connection_boolean_system_variable_value(
    mylite_db *database,
    const struct mylite_sql_ast_node *value,
    const char *variable_name,
    struct mylite_connection_system_variable_plan *plan
);

static int copy_connection_unsigned_system_variable_value(
    mylite_db *database,
    const struct mylite_sql_ast_node *value,
    const char *variable_name,
    struct mylite_connection_system_variable_plan *plan
);

static int copy_connection_rand_seed_value(
    mylite_db *database,
    const struct mylite_sql_ast_node *value,
    const char *variable_name,
    struct mylite_connection_system_variable_plan *plan
);

static int copy_connection_string_system_variable_value(
    mylite_db *database,
    const struct mylite_sql_ast_node *value,
    const char *variable_name,
    struct mylite_connection_system_variable_plan *plan
);

static int copy_connection_charset_system_variable_value(
    mylite_db *database,
    const struct mylite_sql_ast_node *value,
    enum mylite_connection_system_variable variable,
    struct mylite_connection_system_variable_plan *plan
);

static int copy_connection_sql_mode_replace_statement(
    mylite_db *database,
    const struct mylite_sql_ast_node *value,
    struct mylite_connection_system_variable_plan *plan
);

static enum mylite_connection_system_variable set_system_variable_kind(
    const char *variable,
    bool *out_global_scope
);

static bool system_variable_prefix_match(
    const char *variable,
    const char *prefix,
    const char **out_name
);

static const char *set_system_variable_name(enum mylite_connection_system_variable variable);

static int set_system_variable_global_error(
    mylite_db *database,
    enum mylite_connection_system_variable variable
);

static int set_system_variable_read_only_error(mylite_db *database, const char *variable_name);

static int set_system_variable_type_error(mylite_db *database, const char *variable_name);

static int set_system_variable_wrong_value_error(
    mylite_db *database,
    const char *variable_name,
    const struct mylite_sql_ast_node *value
);

static int set_system_variable_wrong_null_value_error(
    mylite_db *database,
    const char *variable_name
);

static int set_system_variable_no_default_error(mylite_db *database, const char *variable_name);

static int set_group_concat_max_len_type_error(mylite_db *database);

static int append_rand_seed_truncation_warning(
    mylite_db *database,
    const char *variable_name,
    const char *value
);

static int append_group_concat_max_len_truncation_warning(mylite_db *database, const char *value);

static int append_default_collation_for_utf8mb4_warning(mylite_db *database);

static bool copy_signed_integer_value(
    const struct mylite_sql_ast_node *value,
    bool *out_negative,
    uint64_t *out_magnitude
);

static bool parse_uint64_text(const char *text, size_t length, uint64_t *out_value);

static int execute_set_sql_mode_replace_statement(mylite_stmt *stmt);

static int copy_system_variable_user_value(
    mylite_db *database,
    const struct mylite_sql_ast_node *value,
    struct mylite_connection_system_variable_plan *plan
);

static int resolve_user_variable_system_value(
    mylite_db *database,
    const struct mylite_connection_system_variable_plan *plan,
    struct mylite_connection_system_variable_plan *out_plan
);

static int resolve_boolean_user_variable_value(
    mylite_db *database,
    const struct mylite_connection_system_variable_plan *plan,
    const struct mylite_expression_value *value,
    struct mylite_connection_system_variable_plan *out_plan
);

static int resolve_unsigned_user_variable_value(
    mylite_db *database,
    const struct mylite_connection_system_variable_plan *plan,
    const struct mylite_expression_value *value,
    struct mylite_connection_system_variable_plan *out_plan
);

static int resolve_rand_seed_user_variable_value(
    mylite_db *database,
    const struct mylite_connection_system_variable_plan *plan,
    const struct mylite_expression_value *value,
    struct mylite_connection_system_variable_plan *out_plan
);

static int resolve_string_user_variable_value(
    mylite_db *database,
    const struct mylite_connection_system_variable_plan *plan,
    const struct mylite_expression_value *value,
    struct mylite_connection_system_variable_plan *out_plan
);

static char *replace_sql_mode_text(
    mylite_db *database,
    const char *value,
    const char *search,
    const char *replacement
);

int mylite_connection_prepare_charset_statement(
    mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_stmt **out_stmt
) {
    mylite_stmt *stmt = NULL;
    enum mylite_stmt_kind kind = MYLITE_STMT_SQLITE;
    int status = MYLITE_OK;

    if (statement->kind == MYLITE_SQL_AST_SET_NAMES_STATEMENT) {
        kind = MYLITE_STMT_SET_NAMES;
    } else if (statement->kind == MYLITE_SQL_AST_SET_CHARACTER_SET_STATEMENT) {
        kind = MYLITE_STMT_SET_CHARACTER_SET;
    } else {
        return MYLITE_UNSUPPORTED;
    }

    stmt = calloc(1U, sizeof(*stmt));
    if (stmt == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    *stmt = (mylite_stmt){
        .database = database,
        .kind = kind,
        .affected_rows = 0,
    };

    status = copy_connection_charset_statement(statement, &stmt->connection_charset);
    if (status != MYLITE_OK) {
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
        }
        mylite_finalize(stmt);
        return status;
    }

    *out_stmt = stmt;
    return MYLITE_OK;
}

int mylite_connection_prepare_system_variable_statement(
    mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_stmt **out_stmt
) {
    mylite_stmt *stmt = calloc(1U, sizeof(*stmt));
    int status = MYLITE_OK;

    if (stmt == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    *stmt = (mylite_stmt){
        .database = database,
        .kind = MYLITE_STMT_SET_SYSTEM_VARIABLE,
        .affected_rows = 0,
    };

    status = mylite_connection_copy_system_variable_statement(
        database,
        statement,
        &stmt->connection_system_variable
    );
    if (status != MYLITE_OK) {
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
        }
        mylite_finalize(stmt);
        return status;
    }

    *out_stmt = stmt;
    return MYLITE_OK;
}

int mylite_connection_execute_set_names_statement(mylite_stmt *stmt) {
    const struct mylite_connection_charset_plan *plan = &stmt->connection_charset;

    if (plan->use_default) {
        return mylite_connection_set_default_state(stmt->database);
    }
    return mylite_connection_set_names_state(
        stmt->database,
        (struct mylite_connection_names_state){
            .character_set_name = plan->character_set_name,
            .collation_name = plan->collation_name,
        }
    );
}

int mylite_connection_execute_set_character_set_statement(mylite_stmt *stmt) {
    const struct mylite_connection_charset_plan *plan = &stmt->connection_charset;

    if (plan->use_default) {
        return mylite_connection_set_character_set_state(
            stmt->database,
            mylite_charset_default_name()
        );
    }
    return mylite_connection_set_character_set_state(stmt->database, plan->character_set_name);
}

int mylite_connection_execute_set_system_variable_statement(mylite_stmt *stmt) {
    const struct mylite_connection_system_variable_plan *plan = &stmt->connection_system_variable;

    return mylite_connection_execute_system_variable_plan(stmt, plan);
}

int mylite_connection_execute_system_variable_plan(
    mylite_stmt *stmt,
    const struct mylite_connection_system_variable_plan *plan
) {
    struct mylite_connection_system_variable_plan resolved_plan = {0};
    int status = MYLITE_OK;

    if (plan->use_user_variable_value) {
        status =
            mylite_connection_resolve_system_variable_plan(stmt->database, plan, &resolved_plan);
        if (status != MYLITE_OK) {
            return status;
        }
        plan = &resolved_plan;
    }

    switch (plan->variable) {
    case MYLITE_CONNECTION_SYSTEM_VARIABLE_DEFAULT_STORAGE_ENGINE:
        status = execute_set_default_storage_engine_statement(stmt, plan);
        break;
    case MYLITE_CONNECTION_SYSTEM_VARIABLE_FOREIGN_KEY_CHECKS:
        status = execute_set_foreign_key_checks_statement(stmt, plan);
        break;
    case MYLITE_CONNECTION_SYSTEM_VARIABLE_GROUP_CONCAT_MAX_LEN:
        status = execute_set_group_concat_max_len_statement(stmt, plan);
        break;
    case MYLITE_CONNECTION_SYSTEM_VARIABLE_SQL_MODE:
        status = execute_set_sql_mode_statement(stmt, plan);
        break;
    case MYLITE_CONNECTION_SYSTEM_VARIABLE_SQL_NOTES:
        status = execute_set_sql_notes_statement(stmt, plan);
        break;
    case MYLITE_CONNECTION_SYSTEM_VARIABLE_SQL_LOG_BIN:
        status = execute_set_sql_log_bin_statement(stmt, plan);
        break;
    case MYLITE_CONNECTION_SYSTEM_VARIABLE_TIME_ZONE:
        status = execute_set_time_zone_statement(stmt, plan);
        break;
    case MYLITE_CONNECTION_SYSTEM_VARIABLE_UNIQUE_CHECKS:
        status = execute_set_unique_checks_statement(stmt, plan);
        break;
    case MYLITE_CONNECTION_SYSTEM_VARIABLE_WAIT_TIMEOUT:
        status = execute_set_wait_timeout_statement(stmt, plan);
        break;
    case MYLITE_CONNECTION_SYSTEM_VARIABLE_RAND_SEED1:
    case MYLITE_CONNECTION_SYSTEM_VARIABLE_RAND_SEED2:
        status = execute_set_rand_seed_statement(stmt, plan);
        break;
    case MYLITE_CONNECTION_SYSTEM_VARIABLE_CHARACTER_SET_CLIENT:
        status = execute_set_character_set_client_statement(stmt, plan);
        break;
    case MYLITE_CONNECTION_SYSTEM_VARIABLE_CHARACTER_SET_RESULTS:
        status = execute_set_character_set_results_statement(stmt, plan);
        break;
    case MYLITE_CONNECTION_SYSTEM_VARIABLE_COLLATION_CONNECTION:
        status = execute_set_collation_connection_statement(stmt, plan);
        break;
    case MYLITE_CONNECTION_SYSTEM_VARIABLE_DEFAULT_COLLATION_FOR_UTF8MB4:
        status = execute_set_default_collation_for_utf8mb4_statement(stmt, plan);
        break;
    case MYLITE_CONNECTION_SYSTEM_VARIABLE_SKIP_EXTERNAL_LOCKING:
        status = set_system_variable_read_only_error(
            stmt->database,
            set_system_variable_name(plan->variable)
        );
        break;
    case MYLITE_CONNECTION_SYSTEM_VARIABLE_NONE:
        (void)mylite_diagnostics_set_error_message(stmt->database, "unsupported SET variable");
        status = MYLITE_UNSUPPORTED;
        break;
    }

    mylite_connection_system_variable_plan_deinit(&resolved_plan);
    return status;
}

static int execute_set_default_storage_engine_statement(
    mylite_stmt *stmt,
    const struct mylite_connection_system_variable_plan *plan
) {
    if (plan->use_default) {
        return mylite_connection_set_default_storage_engine(stmt->database);
    }
    return mylite_connection_set_storage_engine(stmt->database, plan->value);
}

static int execute_set_foreign_key_checks_statement(
    mylite_stmt *stmt,
    const struct mylite_connection_system_variable_plan *plan
) {
    if (plan->use_default) {
        return mylite_connection_set_default_foreign_key_checks(stmt->database);
    }
    return mylite_connection_set_foreign_key_checks(stmt->database, plan->unsigned_value != 0U);
}

static int execute_set_group_concat_max_len_statement(
    mylite_stmt *stmt,
    const struct mylite_connection_system_variable_plan *plan
) {
    int status = MYLITE_OK;

    if (plan->use_default) {
        return plan->global_scope
                   ? mylite_connection_set_default_global_group_concat_max_len()
                   : mylite_connection_set_default_group_concat_max_len(stmt->database);
    }
    status = plan->global_scope
                 ? mylite_connection_set_global_group_concat_max_len(plan->unsigned_value)
                 : mylite_connection_set_group_concat_max_len(stmt->database, plan->unsigned_value);
    if (status == MYLITE_OK && plan->emit_truncation_warning) {
        status = append_group_concat_max_len_truncation_warning(stmt->database, plan->value);
    }
    return status;
}

static int execute_set_time_zone_statement(
    mylite_stmt *stmt,
    const struct mylite_connection_system_variable_plan *plan
) {
    if (plan->use_default) {
        return mylite_connection_set_default_time_zone(stmt->database);
    }
    return mylite_connection_set_time_zone(stmt->database, plan->value);
}

static int execute_set_sql_mode_statement(
    mylite_stmt *stmt,
    const struct mylite_connection_system_variable_plan *plan
) {
    if (plan->global_scope) {
        if (plan->use_default) {
            return mylite_connection_set_default_global_sql_mode();
        }
        return mylite_connection_set_global_sql_mode(stmt->database, plan->value);
    }
    if (plan->use_default) {
        return mylite_connection_set_default_sql_mode(stmt->database);
    }
    if (plan->replace_current_value) {
        return execute_set_sql_mode_replace_statement(stmt);
    }
    return mylite_connection_set_sql_mode(stmt->database, plan->value);
}

static int execute_set_sql_notes_statement(
    mylite_stmt *stmt,
    const struct mylite_connection_system_variable_plan *plan
) {
    if (plan->use_default) {
        return mylite_connection_set_default_sql_notes(stmt->database);
    }
    return mylite_connection_set_sql_notes(stmt->database, plan->unsigned_value != 0U);
}

static int execute_set_sql_log_bin_statement(
    mylite_stmt *stmt,
    const struct mylite_connection_system_variable_plan *plan
) {
    if (plan->use_default) {
        return mylite_connection_set_default_sql_log_bin(stmt->database);
    }
    return mylite_connection_set_sql_log_bin(stmt->database, plan->unsigned_value != 0U);
}

static int execute_set_unique_checks_statement(
    mylite_stmt *stmt,
    const struct mylite_connection_system_variable_plan *plan
) {
    if (plan->use_default) {
        return mylite_connection_set_default_unique_checks(stmt->database);
    }
    return mylite_connection_set_unique_checks(stmt->database, plan->unsigned_value != 0U);
}

static int execute_set_wait_timeout_statement(
    mylite_stmt *stmt,
    const struct mylite_connection_system_variable_plan *plan
) {
    if (plan->use_default) {
        return mylite_connection_set_default_wait_timeout(stmt->database);
    }
    return mylite_connection_set_wait_timeout(stmt->database, plan->unsigned_value);
}

static int execute_set_rand_seed_statement(
    mylite_stmt *stmt,
    const struct mylite_connection_system_variable_plan *plan
) {
    int status = MYLITE_OK;

    if (plan->use_default) {
        return set_system_variable_no_default_error(
            stmt->database,
            set_system_variable_name(plan->variable)
        );
    }
    if (stmt->database == NULL) {
        return MYLITE_MISUSE;
    }

    if (plan->variable == MYLITE_CONNECTION_SYSTEM_VARIABLE_RAND_SEED1) {
        stmt->database->rand_seed1 = (uint32_t)plan->unsigned_value;
    } else {
        stmt->database->rand_seed2 = (uint32_t)plan->unsigned_value;
    }
    stmt->database->rand_seeded = true;
    if (plan->emit_truncation_warning) {
        status = append_rand_seed_truncation_warning(
            stmt->database,
            set_system_variable_name(plan->variable),
            plan->value
        );
    }
    return status;
}

static int execute_set_character_set_client_statement(
    mylite_stmt *stmt,
    const struct mylite_connection_system_variable_plan *plan
) {
    if (plan->use_default) {
        return mylite_connection_set_character_set_client(
            stmt->database,
            mylite_charset_default_name()
        );
    }
    return mylite_connection_set_character_set_client(stmt->database, plan->value);
}

static int execute_set_character_set_results_statement(
    mylite_stmt *stmt,
    const struct mylite_connection_system_variable_plan *plan
) {
    if (plan->use_default) {
        return mylite_connection_set_character_set_results(
            stmt->database,
            mylite_charset_default_name()
        );
    }
    return mylite_connection_set_character_set_results(
        stmt->database,
        plan->use_null_value ? NULL : plan->value
    );
}

static int execute_set_collation_connection_statement(
    mylite_stmt *stmt,
    const struct mylite_connection_system_variable_plan *plan
) {
    if (plan->use_default) {
        return mylite_connection_set_collation_connection(
            stmt->database,
            mylite_charset_default_collation_name()
        );
    }
    return mylite_connection_set_collation_connection(stmt->database, plan->value);
}

static int execute_set_default_collation_for_utf8mb4_statement(
    mylite_stmt *stmt,
    const struct mylite_connection_system_variable_plan *plan
) {
    int status = MYLITE_OK;

    if (plan->global_scope) {
        status =
            plan->use_default
                ? mylite_connection_set_default_global_collation_for_utf8mb4()
                : mylite_connection_set_global_collation_for_utf8mb4(stmt->database, plan->value);
    } else {
        status = plan->use_default
                     ? mylite_connection_set_default_collation_for_utf8mb4(stmt->database)
                     : mylite_connection_set_collation_for_utf8mb4(stmt->database, plan->value);
    }
    if (status != MYLITE_OK) {
        return status;
    }
    return append_default_collation_for_utf8mb4_warning(stmt->database);
}

void mylite_connection_charset_plan_deinit(struct mylite_connection_charset_plan *plan) {
    if (plan == NULL) {
        return;
    }

    free(plan->character_set_name);
    free(plan->collation_name);
    *plan = (struct mylite_connection_charset_plan){0};
}

void mylite_connection_system_variable_plan_deinit(
    struct mylite_connection_system_variable_plan *plan
) {
    if (plan == NULL) {
        return;
    }

    free(plan->value);
    free(plan->user_variable_name);
    free(plan->replace_search);
    free(plan->replace_replacement);
    *plan = (struct mylite_connection_system_variable_plan){0};
}

static int copy_connection_charset_statement(
    const struct mylite_sql_ast_node *statement,
    struct mylite_connection_charset_plan *plan
) {
    const struct mylite_sql_ast_node *character_set = mylite_ast_child_at(statement, 0U);
    const struct mylite_sql_ast_node *collation = mylite_ast_child_at(statement, 1U);

    if (character_set != NULL && character_set->kind == MYLITE_SQL_AST_DEFAULT) {
        plan->use_default = true;
        return MYLITE_OK;
    }

    plan->character_set_name = mylite_copy_schema_text_span(character_set);
    if (plan->character_set_name == NULL) {
        return MYLITE_NOMEM;
    }

    if (collation != NULL) {
        plan->collation_name = mylite_copy_schema_text_span(collation);
        if (plan->collation_name == NULL) {
            return MYLITE_NOMEM;
        }
    }
    return MYLITE_OK;
}

int mylite_connection_copy_system_variable_statement(
    mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct mylite_connection_system_variable_plan *plan
) {
    const struct mylite_sql_ast_node *variable = mylite_ast_child_at(statement, 0U);
    const struct mylite_sql_ast_node *value = mylite_ast_child_at(statement, 1U);
    char *variable_name = mylite_copy_schema_text_span(variable);
    bool variable_global = false;
    bool global_scope = false;

    if (variable_name == NULL) {
        return MYLITE_NOMEM;
    }
    plan->variable = set_system_variable_kind(variable_name, &variable_global);
    global_scope =
        statement->set_system_variable_scope == MYLITE_SQL_AST_SET_SYSTEM_VARIABLE_GLOBAL;
    if (variable_global) {
        global_scope = true;
    }
    free(variable_name);

    if (plan->variable == MYLITE_CONNECTION_SYSTEM_VARIABLE_NONE) {
        (void)mylite_diagnostics_set_error_message(database, "unsupported SET variable");
        return MYLITE_UNSUPPORTED;
    }
    if (plan->variable == MYLITE_CONNECTION_SYSTEM_VARIABLE_SKIP_EXTERNAL_LOCKING) {
        return set_system_variable_read_only_error(
            database,
            set_system_variable_name(plan->variable)
        );
    }
    plan->global_scope = global_scope;
    if (global_scope && plan->variable != MYLITE_CONNECTION_SYSTEM_VARIABLE_GROUP_CONCAT_MAX_LEN &&
        plan->variable != MYLITE_CONNECTION_SYSTEM_VARIABLE_SQL_MODE &&
        plan->variable != MYLITE_CONNECTION_SYSTEM_VARIABLE_DEFAULT_COLLATION_FOR_UTF8MB4) {
        return set_system_variable_global_error(database, plan->variable);
    }
    if (mylite_user_variable_identifier_is_user_variable(value)) {
        return copy_system_variable_user_value(database, value, plan);
    }

    if (plan->variable == MYLITE_CONNECTION_SYSTEM_VARIABLE_DEFAULT_STORAGE_ENGINE) {
        return copy_connection_string_system_variable_value(
            database,
            value,
            set_system_variable_name(plan->variable),
            plan
        );
    }

    if (plan->variable == MYLITE_CONNECTION_SYSTEM_VARIABLE_FOREIGN_KEY_CHECKS ||
        plan->variable == MYLITE_CONNECTION_SYSTEM_VARIABLE_SQL_LOG_BIN ||
        plan->variable == MYLITE_CONNECTION_SYSTEM_VARIABLE_SQL_NOTES ||
        plan->variable == MYLITE_CONNECTION_SYSTEM_VARIABLE_UNIQUE_CHECKS) {
        return copy_connection_boolean_system_variable_value(
            database,
            value,
            set_system_variable_name(plan->variable),
            plan
        );
    }

    if (plan->variable == MYLITE_CONNECTION_SYSTEM_VARIABLE_GROUP_CONCAT_MAX_LEN) {
        return copy_connection_group_concat_max_len_value(database, value, plan);
    }

    if (plan->variable == MYLITE_CONNECTION_SYSTEM_VARIABLE_TIME_ZONE) {
        return copy_connection_string_system_variable_value(
            database,
            value,
            set_system_variable_name(plan->variable),
            plan
        );
    }

    if (plan->variable == MYLITE_CONNECTION_SYSTEM_VARIABLE_DEFAULT_COLLATION_FOR_UTF8MB4) {
        return copy_connection_string_system_variable_value(
            database,
            value,
            set_system_variable_name(plan->variable),
            plan
        );
    }

    if (plan->variable == MYLITE_CONNECTION_SYSTEM_VARIABLE_WAIT_TIMEOUT) {
        return copy_connection_unsigned_system_variable_value(
            database,
            value,
            set_system_variable_name(plan->variable),
            plan
        );
    }

    if (plan->variable == MYLITE_CONNECTION_SYSTEM_VARIABLE_RAND_SEED1 ||
        plan->variable == MYLITE_CONNECTION_SYSTEM_VARIABLE_RAND_SEED2) {
        return copy_connection_rand_seed_value(
            database,
            value,
            set_system_variable_name(plan->variable),
            plan
        );
    }

    if (plan->variable == MYLITE_CONNECTION_SYSTEM_VARIABLE_CHARACTER_SET_CLIENT ||
        plan->variable == MYLITE_CONNECTION_SYSTEM_VARIABLE_CHARACTER_SET_RESULTS ||
        plan->variable == MYLITE_CONNECTION_SYSTEM_VARIABLE_COLLATION_CONNECTION) {
        return copy_connection_charset_system_variable_value(database, value, plan->variable, plan);
    }

    if (value != NULL && value->kind == MYLITE_SQL_AST_DEFAULT) {
        plan->use_default = true;
        return MYLITE_OK;
    }
    if (value != NULL && value->kind == MYLITE_SQL_AST_FUNCTION_CALL) {
        return copy_connection_sql_mode_replace_statement(database, value, plan);
    }
    if (value == NULL || value->kind != MYLITE_SQL_AST_LITERAL ||
        value->literal_kind != MYLITE_SQL_AST_LITERAL_STRING) {
        (void)mylite_diagnostics_set_error_message(database, "unsupported SET sql_mode value");
        return MYLITE_UNSUPPORTED;
    }

    plan->value = mylite_copy_string_literal_span(value);
    return plan->value == NULL ? MYLITE_NOMEM : MYLITE_OK;
}

static int copy_connection_group_concat_max_len_value(
    mylite_db *database,
    const struct mylite_sql_ast_node *value,
    struct mylite_connection_system_variable_plan *plan
) {
    static const uint64_t minimum_value = 4U;
    bool negative = false;
    uint64_t magnitude = 0U;

    if (value != NULL && value->kind == MYLITE_SQL_AST_DEFAULT) {
        plan->use_default = true;
        return MYLITE_OK;
    }
    if (!copy_signed_integer_value(value, &negative, &magnitude)) {
        return set_group_concat_max_len_type_error(database);
    }

    if (negative || magnitude < minimum_value) {
        plan->value = mylite_copy_span_text(value->span.text, value->span.length);
        if (plan->value == NULL) {
            return MYLITE_NOMEM;
        }
        plan->emit_truncation_warning = true;
        plan->unsigned_value = minimum_value;
        return MYLITE_OK;
    }

    plan->unsigned_value = magnitude;
    return MYLITE_OK;
}

static int copy_connection_boolean_system_variable_value(
    mylite_db *database,
    const struct mylite_sql_ast_node *value,
    const char *variable_name,
    struct mylite_connection_system_variable_plan *plan
) {
    bool negative = false;
    uint64_t magnitude = 0U;

    if (value != NULL && value->kind == MYLITE_SQL_AST_DEFAULT) {
        plan->use_default = true;
        return MYLITE_OK;
    }
    if (value != NULL && value->kind == MYLITE_SQL_AST_LITERAL &&
        (value->literal_kind == MYLITE_SQL_AST_LITERAL_TRUE ||
         value->literal_kind == MYLITE_SQL_AST_LITERAL_FALSE)) {
        plan->unsigned_value = value->literal_kind == MYLITE_SQL_AST_LITERAL_TRUE ? 1U : 0U;
        return MYLITE_OK;
    }
    if (!copy_signed_integer_value(value, &negative, &magnitude)) {
        return set_system_variable_type_error(database, variable_name);
    }
    if (negative || magnitude > 1U) {
        return set_system_variable_wrong_value_error(database, variable_name, value);
    }

    plan->unsigned_value = magnitude;
    return MYLITE_OK;
}

static int copy_connection_unsigned_system_variable_value(
    mylite_db *database,
    const struct mylite_sql_ast_node *value,
    const char *variable_name,
    struct mylite_connection_system_variable_plan *plan
) {
    bool negative = false;
    uint64_t magnitude = 0U;

    if (value != NULL && value->kind == MYLITE_SQL_AST_DEFAULT) {
        plan->use_default = true;
        return MYLITE_OK;
    }
    if (!copy_signed_integer_value(value, &negative, &magnitude) || negative) {
        return set_system_variable_type_error(database, variable_name);
    }

    plan->unsigned_value = magnitude;
    return MYLITE_OK;
}

static int copy_connection_rand_seed_value(
    mylite_db *database,
    const struct mylite_sql_ast_node *value,
    const char *variable_name,
    struct mylite_connection_system_variable_plan *plan
) {
    static const uint64_t rand_max_value = 0x3fffffffU;
    bool negative = false;
    uint64_t magnitude = 0U;

    if (value != NULL && value->kind == MYLITE_SQL_AST_DEFAULT) {
        plan->use_default = true;
        return MYLITE_OK;
    }
    if (!copy_signed_integer_value(value, &negative, &magnitude)) {
        return set_system_variable_type_error(database, variable_name);
    }
    if (negative) {
        plan->value = mylite_copy_span_text(value->span.text, value->span.length);
        if (plan->value == NULL) {
            return MYLITE_NOMEM;
        }
        plan->emit_truncation_warning = true;
        plan->unsigned_value = 0U;
        return MYLITE_OK;
    }

    plan->unsigned_value = magnitude % rand_max_value;
    return MYLITE_OK;
}

static int copy_connection_string_system_variable_value(
    mylite_db *database,
    const struct mylite_sql_ast_node *value,
    const char *variable_name,
    struct mylite_connection_system_variable_plan *plan
) {
    if (value != NULL && value->kind == MYLITE_SQL_AST_DEFAULT) {
        plan->use_default = true;
        return MYLITE_OK;
    }
    if (value == NULL || (value->kind != MYLITE_SQL_AST_IDENTIFIER &&
                          (value->kind != MYLITE_SQL_AST_LITERAL ||
                           value->literal_kind != MYLITE_SQL_AST_LITERAL_STRING))) {
        return set_system_variable_type_error(database, variable_name);
    }

    plan->value = value->kind == MYLITE_SQL_AST_IDENTIFIER ? mylite_copy_schema_text_span(value)
                                                           : mylite_copy_string_literal_span(value);
    return plan->value == NULL ? MYLITE_NOMEM : MYLITE_OK;
}

static int copy_connection_charset_system_variable_value(
    mylite_db *database,
    const struct mylite_sql_ast_node *value,
    enum mylite_connection_system_variable variable,
    struct mylite_connection_system_variable_plan *plan
) {
    const char *variable_name = set_system_variable_name(variable);

    if (value != NULL && value->kind == MYLITE_SQL_AST_DEFAULT) {
        plan->use_default = true;
        return MYLITE_OK;
    }
    if (value != NULL && value->kind == MYLITE_SQL_AST_LITERAL &&
        value->literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        if (variable == MYLITE_CONNECTION_SYSTEM_VARIABLE_CHARACTER_SET_RESULTS) {
            plan->use_null_value = true;
            return MYLITE_OK;
        }
        return set_system_variable_wrong_value_error(database, variable_name, value);
    }
    if (value == NULL || (value->kind != MYLITE_SQL_AST_IDENTIFIER &&
                          (value->kind != MYLITE_SQL_AST_LITERAL ||
                           value->literal_kind != MYLITE_SQL_AST_LITERAL_STRING))) {
        return set_system_variable_type_error(database, variable_name);
    }

    plan->value = value->kind == MYLITE_SQL_AST_IDENTIFIER ? mylite_copy_schema_text_span(value)
                                                           : mylite_copy_string_literal_span(value);
    return plan->value == NULL ? MYLITE_NOMEM : MYLITE_OK;
}

static int copy_connection_sql_mode_replace_statement(
    mylite_db *database,
    const struct mylite_sql_ast_node *value,
    struct mylite_connection_system_variable_plan *plan
) {
    const struct mylite_sql_ast_node *function_name = mylite_ast_child_at(value, 0U);
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(value, 1U);
    const struct mylite_sql_ast_node *variable =
        arguments == NULL ? NULL : mylite_ast_child_at(arguments, 0U);
    const struct mylite_sql_ast_node *search =
        arguments == NULL ? NULL : mylite_ast_child_at(arguments, 1U);
    const struct mylite_sql_ast_node *replacement =
        arguments == NULL ? NULL : mylite_ast_child_at(arguments, 2U);
    char *variable_name = NULL;
    bool global_scope = false;

    if (plan->global_scope || function_name == NULL ||
        !mylite_span_equal_ci(function_name->span, "REPLACE") || arguments == NULL ||
        mylite_sql_ast_node_child_count(arguments) != 3U || variable == NULL || search == NULL ||
        replacement == NULL || search->kind != MYLITE_SQL_AST_LITERAL ||
        search->literal_kind != MYLITE_SQL_AST_LITERAL_STRING ||
        replacement->kind != MYLITE_SQL_AST_LITERAL ||
        replacement->literal_kind != MYLITE_SQL_AST_LITERAL_STRING) {
        (void)mylite_diagnostics_set_error_message(database, "unsupported SET sql_mode value");
        return MYLITE_UNSUPPORTED;
    }

    variable_name = mylite_copy_schema_text_span(variable);
    if (variable_name == NULL) {
        return MYLITE_NOMEM;
    }
    if (set_system_variable_kind(variable_name, &global_scope) !=
            MYLITE_CONNECTION_SYSTEM_VARIABLE_SQL_MODE ||
        global_scope) {
        free(variable_name);
        (void)mylite_diagnostics_set_error_message(database, "unsupported SET sql_mode value");
        return MYLITE_UNSUPPORTED;
    }
    free(variable_name);

    plan->replace_search = mylite_copy_string_literal_span(search);
    plan->replace_replacement = mylite_copy_string_literal_span(replacement);
    if (plan->replace_search == NULL || plan->replace_replacement == NULL) {
        return MYLITE_NOMEM;
    }
    plan->replace_current_value = true;
    return MYLITE_OK;
}

static int copy_system_variable_user_value(
    mylite_db *database,
    const struct mylite_sql_ast_node *value,
    struct mylite_connection_system_variable_plan *plan
) {
    int status = mylite_user_variable_copy_identifier_name(value, &plan->user_variable_name);

    if (status != MYLITE_OK) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return status;
    }
    plan->use_user_variable_value = true;
    return MYLITE_OK;
}

int mylite_connection_resolve_system_variable_plan(
    mylite_db *database,
    const struct mylite_connection_system_variable_plan *plan,
    struct mylite_connection_system_variable_plan *out_plan
) {
    if (plan == NULL || out_plan == NULL) {
        return MYLITE_MISUSE;
    }

    *out_plan = (struct mylite_connection_system_variable_plan){
        .variable = plan->variable,
    };
    if (!plan->use_user_variable_value) {
        out_plan->value =
            plan->value == NULL ? NULL : mylite_copy_span_text(plan->value, strlen(plan->value));
        out_plan->replace_search =
            plan->replace_search == NULL
                ? NULL
                : mylite_copy_span_text(plan->replace_search, strlen(plan->replace_search));
        out_plan->replace_replacement = plan->replace_replacement == NULL
                                            ? NULL
                                            : mylite_copy_span_text(
                                                  plan->replace_replacement,
                                                  strlen(plan->replace_replacement)
                                              );
        out_plan->unsigned_value = plan->unsigned_value;
        out_plan->use_default = plan->use_default;
        out_plan->replace_current_value = plan->replace_current_value;
        out_plan->emit_truncation_warning = plan->emit_truncation_warning;
        out_plan->use_null_value = plan->use_null_value;
        out_plan->global_scope = plan->global_scope;
        if ((plan->value != NULL && out_plan->value == NULL) ||
            (plan->replace_search != NULL && out_plan->replace_search == NULL) ||
            (plan->replace_replacement != NULL && out_plan->replace_replacement == NULL)) {
            mylite_connection_system_variable_plan_deinit(out_plan);
            return MYLITE_NOMEM;
        }
        return MYLITE_OK;
    }

    return resolve_user_variable_system_value(database, plan, out_plan);
}

static int resolve_user_variable_system_value(
    mylite_db *database,
    const struct mylite_connection_system_variable_plan *plan,
    struct mylite_connection_system_variable_plan *out_plan
) {
    struct mylite_expression_value value = {0};
    int status = mylite_user_variable_eval_name(database, plan->user_variable_name, &value);

    if (status != 0) {
        return status;
    }
    out_plan->global_scope = plan->global_scope;

    switch (plan->variable) {
    case MYLITE_CONNECTION_SYSTEM_VARIABLE_DEFAULT_STORAGE_ENGINE:
    case MYLITE_CONNECTION_SYSTEM_VARIABLE_CHARACTER_SET_CLIENT:
    case MYLITE_CONNECTION_SYSTEM_VARIABLE_CHARACTER_SET_RESULTS:
    case MYLITE_CONNECTION_SYSTEM_VARIABLE_COLLATION_CONNECTION:
    case MYLITE_CONNECTION_SYSTEM_VARIABLE_DEFAULT_COLLATION_FOR_UTF8MB4:
    case MYLITE_CONNECTION_SYSTEM_VARIABLE_SQL_MODE:
    case MYLITE_CONNECTION_SYSTEM_VARIABLE_TIME_ZONE:
        status = resolve_string_user_variable_value(database, plan, &value, out_plan);
        break;
    case MYLITE_CONNECTION_SYSTEM_VARIABLE_FOREIGN_KEY_CHECKS:
    case MYLITE_CONNECTION_SYSTEM_VARIABLE_SQL_NOTES:
    case MYLITE_CONNECTION_SYSTEM_VARIABLE_SQL_LOG_BIN:
    case MYLITE_CONNECTION_SYSTEM_VARIABLE_UNIQUE_CHECKS:
        status = resolve_boolean_user_variable_value(database, plan, &value, out_plan);
        break;
    case MYLITE_CONNECTION_SYSTEM_VARIABLE_GROUP_CONCAT_MAX_LEN:
    case MYLITE_CONNECTION_SYSTEM_VARIABLE_WAIT_TIMEOUT:
        status = resolve_unsigned_user_variable_value(database, plan, &value, out_plan);
        break;
    case MYLITE_CONNECTION_SYSTEM_VARIABLE_RAND_SEED1:
    case MYLITE_CONNECTION_SYSTEM_VARIABLE_RAND_SEED2:
        status = resolve_rand_seed_user_variable_value(database, plan, &value, out_plan);
        break;
    case MYLITE_CONNECTION_SYSTEM_VARIABLE_SKIP_EXTERNAL_LOCKING:
        status =
            set_system_variable_read_only_error(database, set_system_variable_name(plan->variable));
        break;
    case MYLITE_CONNECTION_SYSTEM_VARIABLE_NONE:
        status = MYLITE_UNSUPPORTED;
        break;
    }

    mylite_expression_value_deinit(&value);
    return status;
}

static int resolve_boolean_user_variable_value(
    mylite_db *database,
    const struct mylite_connection_system_variable_plan *plan,
    const struct mylite_expression_value *value,
    struct mylite_connection_system_variable_plan *out_plan
) {
    int64_t signed_value = mylite_expression_value_to_int64(value);

    if (value->kind == MYLITE_EXPRESSION_VALUE_NULL || signed_value < 0 || signed_value > 1) {
        return set_system_variable_wrong_value_error(
            database,
            set_system_variable_name(plan->variable),
            NULL
        );
    }
    out_plan->unsigned_value = (uint64_t)signed_value;
    return MYLITE_OK;
}

static int resolve_unsigned_user_variable_value(
    mylite_db *database,
    const struct mylite_connection_system_variable_plan *plan,
    const struct mylite_expression_value *value,
    struct mylite_connection_system_variable_plan *out_plan
) {
    int64_t signed_value = mylite_expression_value_to_int64(value);

    if (value->kind == MYLITE_EXPRESSION_VALUE_NULL || signed_value < 0) {
        return set_system_variable_type_error(database, set_system_variable_name(plan->variable));
    }
    out_plan->unsigned_value = (uint64_t)signed_value;
    if (plan->variable == MYLITE_CONNECTION_SYSTEM_VARIABLE_GROUP_CONCAT_MAX_LEN &&
        out_plan->unsigned_value < 4U) {
        out_plan->unsigned_value = 4U;
    }
    return MYLITE_OK;
}

static int resolve_rand_seed_user_variable_value(
    mylite_db *database,
    const struct mylite_connection_system_variable_plan *plan,
    const struct mylite_expression_value *value,
    struct mylite_connection_system_variable_plan *out_plan
) {
    static const uint64_t rand_max_value = 0x3fffffffU;
    int64_t signed_value = mylite_expression_value_to_int64(value);

    if (value->kind == MYLITE_EXPRESSION_VALUE_NULL) {
        return set_system_variable_type_error(database, set_system_variable_name(plan->variable));
    }
    if (signed_value < 0) {
        out_plan->value = mylite_expression_value_to_text(value);
        if (out_plan->value == NULL) {
            return MYLITE_NOMEM;
        }
        out_plan->emit_truncation_warning = true;
        out_plan->unsigned_value = 0U;
        return MYLITE_OK;
    }
    out_plan->unsigned_value = ((uint64_t)signed_value) % rand_max_value;
    return MYLITE_OK;
}

static int resolve_string_user_variable_value(
    mylite_db *database,
    const struct mylite_connection_system_variable_plan *plan,
    const struct mylite_expression_value *value,
    struct mylite_connection_system_variable_plan *out_plan
) {
    if (value->kind == MYLITE_EXPRESSION_VALUE_NULL) {
        if (plan->variable == MYLITE_CONNECTION_SYSTEM_VARIABLE_CHARACTER_SET_RESULTS) {
            out_plan->use_null_value = true;
            return MYLITE_OK;
        }
        if (plan->variable == MYLITE_CONNECTION_SYSTEM_VARIABLE_CHARACTER_SET_CLIENT ||
            plan->variable == MYLITE_CONNECTION_SYSTEM_VARIABLE_COLLATION_CONNECTION) {
            return set_system_variable_wrong_null_value_error(
                database,
                set_system_variable_name(plan->variable)
            );
        }
        out_plan->value = mylite_copy_span_text("", 0U);
    } else {
        out_plan->value = mylite_expression_value_to_text(value);
    }
    return out_plan->value == NULL ? MYLITE_NOMEM : MYLITE_OK;
}

static enum mylite_connection_system_variable set_system_variable_kind(
    const char *variable,
    bool *out_global_scope
) {
    const char *name = variable;

    if (out_global_scope != NULL) {
        *out_global_scope = false;
    }
    if (system_variable_prefix_match(variable, "@@global.", &name)) {
        if (out_global_scope != NULL) {
            *out_global_scope = true;
        }
    } else if (
        system_variable_prefix_match(variable, "@@session.", &name) ||
        system_variable_prefix_match(variable, "@@local.", &name) ||
        system_variable_prefix_match(variable, "@@", &name)
    ) {
        /* Keep name advanced past the recognized session prefix. */
    }

    if (mylite_ascii_case_equal(name, "sql_mode")) {
        return MYLITE_CONNECTION_SYSTEM_VARIABLE_SQL_MODE;
    }
    if (mylite_ascii_case_equal(name, "group_concat_max_len")) {
        return MYLITE_CONNECTION_SYSTEM_VARIABLE_GROUP_CONCAT_MAX_LEN;
    }
    if (mylite_ascii_case_equal(name, "default_storage_engine")) {
        return MYLITE_CONNECTION_SYSTEM_VARIABLE_DEFAULT_STORAGE_ENGINE;
    }
    if (mylite_ascii_case_equal(name, "foreign_key_checks")) {
        return MYLITE_CONNECTION_SYSTEM_VARIABLE_FOREIGN_KEY_CHECKS;
    }
    if (mylite_ascii_case_equal(name, "time_zone")) {
        return MYLITE_CONNECTION_SYSTEM_VARIABLE_TIME_ZONE;
    }
    if (mylite_ascii_case_equal(name, "sql_notes")) {
        return MYLITE_CONNECTION_SYSTEM_VARIABLE_SQL_NOTES;
    }
    if (mylite_ascii_case_equal(name, "sql_log_bin")) {
        return MYLITE_CONNECTION_SYSTEM_VARIABLE_SQL_LOG_BIN;
    }
    if (mylite_ascii_case_equal(name, "unique_checks")) {
        return MYLITE_CONNECTION_SYSTEM_VARIABLE_UNIQUE_CHECKS;
    }
    if (mylite_ascii_case_equal(name, "wait_timeout")) {
        return MYLITE_CONNECTION_SYSTEM_VARIABLE_WAIT_TIMEOUT;
    }
    if (mylite_ascii_case_equal(name, "character_set_client")) {
        return MYLITE_CONNECTION_SYSTEM_VARIABLE_CHARACTER_SET_CLIENT;
    }
    if (mylite_ascii_case_equal(name, "character_set_results")) {
        return MYLITE_CONNECTION_SYSTEM_VARIABLE_CHARACTER_SET_RESULTS;
    }
    if (mylite_ascii_case_equal(name, "collation_connection")) {
        return MYLITE_CONNECTION_SYSTEM_VARIABLE_COLLATION_CONNECTION;
    }
    if (mylite_ascii_case_equal(name, "default_collation_for_utf8mb4")) {
        return MYLITE_CONNECTION_SYSTEM_VARIABLE_DEFAULT_COLLATION_FOR_UTF8MB4;
    }
    if (mylite_ascii_case_equal(name, "skip_external_locking")) {
        return MYLITE_CONNECTION_SYSTEM_VARIABLE_SKIP_EXTERNAL_LOCKING;
    }
    if (mylite_ascii_case_equal(name, "rand_seed1")) {
        return MYLITE_CONNECTION_SYSTEM_VARIABLE_RAND_SEED1;
    }
    if (mylite_ascii_case_equal(name, "rand_seed2")) {
        return MYLITE_CONNECTION_SYSTEM_VARIABLE_RAND_SEED2;
    }
    return MYLITE_CONNECTION_SYSTEM_VARIABLE_NONE;
}

static bool system_variable_prefix_match(
    const char *variable,
    const char *prefix,
    const char **out_name
) {
    size_t prefix_length = prefix == NULL ? 0U : strlen(prefix);

    if (variable == NULL || prefix == NULL) {
        return false;
    }
    for (size_t index = 0U; index < prefix_length; ++index) {
        unsigned char left = (unsigned char)variable[index];
        unsigned char right = (unsigned char)prefix[index];

        if (left == '\0') {
            return false;
        }
        if (left >= 'A' && left <= 'Z') {
            left = (unsigned char)(left - 'A' + 'a');
        }
        if (right >= 'A' && right <= 'Z') {
            right = (unsigned char)(right - 'A' + 'a');
        }
        if (left != right) {
            return false;
        }
    }
    if (out_name != NULL) {
        *out_name = variable + prefix_length;
    }
    return true;
}

static const char *set_system_variable_name(enum mylite_connection_system_variable variable) {
    switch (variable) {
    case MYLITE_CONNECTION_SYSTEM_VARIABLE_DEFAULT_STORAGE_ENGINE:
        return "default_storage_engine";
    case MYLITE_CONNECTION_SYSTEM_VARIABLE_FOREIGN_KEY_CHECKS:
        return "foreign_key_checks";
    case MYLITE_CONNECTION_SYSTEM_VARIABLE_GROUP_CONCAT_MAX_LEN:
        return "group_concat_max_len";
    case MYLITE_CONNECTION_SYSTEM_VARIABLE_SQL_MODE:
        return "sql_mode";
    case MYLITE_CONNECTION_SYSTEM_VARIABLE_SQL_NOTES:
        return "sql_notes";
    case MYLITE_CONNECTION_SYSTEM_VARIABLE_SQL_LOG_BIN:
        return "sql_log_bin";
    case MYLITE_CONNECTION_SYSTEM_VARIABLE_TIME_ZONE:
        return "time_zone";
    case MYLITE_CONNECTION_SYSTEM_VARIABLE_UNIQUE_CHECKS:
        return "unique_checks";
    case MYLITE_CONNECTION_SYSTEM_VARIABLE_WAIT_TIMEOUT:
        return "wait_timeout";
    case MYLITE_CONNECTION_SYSTEM_VARIABLE_CHARACTER_SET_CLIENT:
        return "character_set_client";
    case MYLITE_CONNECTION_SYSTEM_VARIABLE_CHARACTER_SET_RESULTS:
        return "character_set_results";
    case MYLITE_CONNECTION_SYSTEM_VARIABLE_COLLATION_CONNECTION:
        return "collation_connection";
    case MYLITE_CONNECTION_SYSTEM_VARIABLE_DEFAULT_COLLATION_FOR_UTF8MB4:
        return "default_collation_for_utf8mb4";
    case MYLITE_CONNECTION_SYSTEM_VARIABLE_SKIP_EXTERNAL_LOCKING:
        return "skip_external_locking";
    case MYLITE_CONNECTION_SYSTEM_VARIABLE_RAND_SEED1:
        return "rand_seed1";
    case MYLITE_CONNECTION_SYSTEM_VARIABLE_RAND_SEED2:
        return "rand_seed2";
    case MYLITE_CONNECTION_SYSTEM_VARIABLE_NONE:
        break;
    }
    return "variable";
}

static int set_system_variable_global_error(
    mylite_db *database,
    enum mylite_connection_system_variable variable
) {
    char *message = NULL;
    unsigned int code = MYLITE_MYSQL_ER_UNKNOWN_ERROR;
    int status = MYLITE_OK;

    if (variable == MYLITE_CONNECTION_SYSTEM_VARIABLE_SQL_LOG_BIN ||
        variable == MYLITE_CONNECTION_SYSTEM_VARIABLE_RAND_SEED1 ||
        variable == MYLITE_CONNECTION_SYSTEM_VARIABLE_RAND_SEED2) {
        message = sqlite3_mprintf(
            "Variable '%q' is a SESSION variable and can't be used with SET GLOBAL",
            set_system_variable_name(variable)
        );
        code = MYLITE_MYSQL_ER_LOCAL_VARIABLE;
    } else {
        message =
            sqlite3_mprintf("SET GLOBAL %s is not supported", set_system_variable_name(variable));
    }

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_set_error_message(database, message);
    if (status == MYLITE_OK && (variable == MYLITE_CONNECTION_SYSTEM_VARIABLE_SQL_LOG_BIN ||
                                variable == MYLITE_CONNECTION_SYSTEM_VARIABLE_RAND_SEED1 ||
                                variable == MYLITE_CONNECTION_SYSTEM_VARIABLE_RAND_SEED2)) {
        status = mylite_diagnostics_append_error(database, code, message);
    }
    sqlite3_free(message);
    if (status == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    return code == MYLITE_MYSQL_ER_LOCAL_VARIABLE ? MYLITE_EXEC_ERROR : MYLITE_UNSUPPORTED;
}

static int set_system_variable_read_only_error(mylite_db *database, const char *variable_name) {
    char *message = sqlite3_mprintf("Variable '%q' is a read only variable", variable_name);
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_set_error_message(database, message);
    if (status == MYLITE_OK) {
        status = mylite_diagnostics_append_error(
            database,
            MYLITE_MYSQL_ER_INCORRECT_GLOBAL_LOCAL_VAR,
            message
        );
    }
    sqlite3_free(message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int set_system_variable_type_error(mylite_db *database, const char *variable_name) {
    char *message = sqlite3_mprintf("Incorrect argument type to variable '%q'", variable_name);
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_set_error_message(database, message);
    if (status == MYLITE_OK) {
        status =
            mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_WRONG_TYPE_FOR_VAR, message);
    }
    sqlite3_free(message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int set_system_variable_wrong_value_error(
    mylite_db *database,
    const char *variable_name,
    const struct mylite_sql_ast_node *value
) {
    char *value_text = NULL;
    char *message = NULL;
    int status = MYLITE_OK;

    if (value == NULL) {
        return set_system_variable_type_error(database, variable_name);
    }
    value_text = mylite_copy_span_text(value->span.text, value->span.length);
    if (value_text == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    message = sqlite3_mprintf(
        "Variable '%q' can't be set to the value of '%q'",
        variable_name,
        value_text
    );
    free(value_text);
    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = mylite_diagnostics_set_error_message(database, message);
    if (status == MYLITE_OK) {
        status =
            mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_WRONG_VALUE_FOR_VAR, message);
    }
    sqlite3_free(message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int set_system_variable_wrong_null_value_error(
    mylite_db *database,
    const char *variable_name
) {
    char *message =
        sqlite3_mprintf("Variable '%q' can't be set to the value of 'NULL'", variable_name);
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = mylite_diagnostics_set_error_message(database, message);
    if (status == MYLITE_OK) {
        status =
            mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_WRONG_VALUE_FOR_VAR, message);
    }
    sqlite3_free(message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int set_system_variable_no_default_error(mylite_db *database, const char *variable_name) {
    char *message = sqlite3_mprintf("Variable '%q' doesn't have a default value", variable_name);
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = mylite_diagnostics_set_error_message(database, message);
    if (status == MYLITE_OK) {
        status = mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_NO_DEFAULT, message);
    }
    sqlite3_free(message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int set_group_concat_max_len_type_error(mylite_db *database) {
    static const char message[] = "Incorrect argument type to variable 'group_concat_max_len'";
    int status = mylite_diagnostics_set_error_message(database, message);

    if (status == MYLITE_OK) {
        status =
            mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_WRONG_TYPE_FOR_VAR, message);
    }
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int append_rand_seed_truncation_warning(
    mylite_db *database,
    const char *variable_name,
    const char *value
) {
    char *message = sqlite3_mprintf("Truncated incorrect %s value: '%q'", variable_name, value);
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status =
        mylite_diagnostics_append_warning(database, MYLITE_MYSQL_ER_TRUNCATED_WRONG_VALUE, message);
    sqlite3_free(message);
    return status;
}

static int append_group_concat_max_len_truncation_warning(mylite_db *database, const char *value) {
    char *message = NULL;
    int status = MYLITE_OK;

    message = sqlite3_mprintf("Truncated incorrect group_concat_max_len value: '%q'", value);
    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status =
        mylite_diagnostics_append_warning(database, MYLITE_MYSQL_ER_TRUNCATED_WRONG_VALUE, message);
    sqlite3_free(message);
    return status;
}

static int append_default_collation_for_utf8mb4_warning(mylite_db *database) {
    return mylite_diagnostics_append_warning(
        database,
        MYLITE_MYSQL_ER_WARN_DEPRECATED_SYNTAX_NO_REPLACEMENT,
        "Updating 'default_collation_for_utf8mb4' is deprecated. It will be made read-only in a "
        "future release."
    );
}

static bool copy_signed_integer_value(
    const struct mylite_sql_ast_node *value,
    bool *out_negative,
    uint64_t *out_magnitude
) {
    const struct mylite_sql_ast_node *literal = value;
    bool negative = false;

    if (out_negative == NULL || out_magnitude == NULL) {
        return false;
    }
    *out_negative = false;
    *out_magnitude = 0U;

    if (value != NULL && value->kind == MYLITE_SQL_AST_UNARY_EXPRESSION &&
        (value->operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE ||
         value->operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE)) {
        negative = value->operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE;
        literal = mylite_ast_child_at(value, 0U);
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
        literal->literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        return false;
    }
    if (!parse_uint64_text(literal->span.text, literal->span.length, out_magnitude)) {
        return false;
    }
    *out_negative = negative;
    return true;
}

static bool parse_uint64_text(const char *text, size_t length, uint64_t *out_value) {
    enum { decimal_radix = 10U };

    uint64_t value = 0U;

    if (out_value == NULL) {
        return false;
    }
    *out_value = 0U;
    if (text == NULL || length == 0U) {
        return false;
    }
    for (size_t index = 0U; index < length; ++index) {
        unsigned char byte = (unsigned char)text[index];
        uint64_t digit = 0U;

        if (byte < '0' || byte > '9') {
            return false;
        }
        digit = (uint64_t)(byte - '0');
        if (value > (UINT64_MAX - digit) / decimal_radix) {
            return false;
        }
        value = (value * decimal_radix) + digit;
    }
    *out_value = value;
    return true;
}

static int execute_set_sql_mode_replace_statement(mylite_stmt *stmt) {
    const struct mylite_connection_system_variable_plan *plan = &stmt->connection_system_variable;
    char *value = replace_sql_mode_text(
        stmt->database,
        mylite_connection_sql_mode(stmt->database),
        plan->replace_search,
        plan->replace_replacement
    );
    int status = MYLITE_OK;

    if (value == NULL) {
        return MYLITE_NOMEM;
    }
    status = mylite_connection_set_sql_mode(stmt->database, value);
    free(value);
    return status;
}

static char *replace_sql_mode_text(
    mylite_db *database,
    const char *value,
    const char *search,
    const char *replacement
) {
    size_t value_length = value == NULL ? 0U : strlen(value);
    size_t search_length = search == NULL ? 0U : strlen(search);
    size_t replacement_length = replacement == NULL ? 0U : strlen(replacement);
    size_t occurrence_count = 0U;
    size_t result_length = value_length;
    char *result = NULL;
    char *writer = NULL;
    const char *cursor = value;

    if (search_length == 0U) {
        result = mylite_copy_span_text(value == NULL ? "" : value, value_length);
        if (result == NULL) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
        }
        return result;
    }
    while (cursor != NULL) {
        const char *match = strstr(cursor, search);

        if (match == NULL) {
            break;
        }
        ++occurrence_count;
        cursor = match + search_length;
    }
    if (replacement_length > search_length &&
        occurrence_count > (SIZE_MAX - value_length) / (replacement_length - search_length)) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return NULL;
    }
    if (replacement_length > search_length) {
        result_length += occurrence_count * (replacement_length - search_length);
    } else {
        result_length -= occurrence_count * (search_length - replacement_length);
    }

    result = malloc(result_length + 1U);
    if (result == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return NULL;
    }

    writer = result;
    cursor = value == NULL ? "" : value;
    for (;;) {
        const char *match = strstr(cursor, search);
        size_t prefix_length = 0U;

        if (match == NULL) {
            strcpy(writer, cursor);
            break;
        }
        prefix_length = (size_t)(match - cursor);
        memcpy(writer, cursor, prefix_length);
        writer += prefix_length;
        memcpy(writer, replacement, replacement_length);
        writer += replacement_length;
        cursor = match + search_length;
    }
    return result;
}
