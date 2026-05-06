#include "mylite_table_ddl_rename_validate.h"

#include "mylite_catalog.h"
#include "mylite_diagnostics.h"
#include "mylite_span.h"
#include "sqlite3.h"

#include <string.h>

static int resolve_rename_table_names(
    mylite_db *database,
    const char *selected_schema,
    struct mylite_rename_table_plan *plan
);

static int validate_rename_table_target_schemas(
    mylite_db *database,
    const struct mylite_rename_table_target *target
);

static int validate_rename_table_target(
    mylite_db *database,
    const struct mylite_rename_table_plan *plan,
    size_t target_index
);

static int simulated_rename_table_exists_before_target(
    mylite_db *database,
    const struct mylite_rename_table_plan *plan,
    const char *schema_name,
    const char *table_name,
    size_t target_index,
    bool *out_exists
);

static bool rename_table_target_source_matches(
    const struct mylite_rename_table_target *target,
    const char *schema_name,
    const char *table_name
);

static bool rename_table_target_destination_matches(
    const struct mylite_rename_table_target *target,
    const char *schema_name,
    const char *table_name
);

static int set_rename_table_exists_error(
    mylite_db *database,
    const char *schema_name,
    const char *table_name
);

int mylite_table_ddl_validate_rename_table_plan(
    mylite_db *database,
    const char *selected_schema,
    struct mylite_rename_table_plan *plan
) {
    int status = MYLITE_OK;

    if (plan->target_count == 0U) {
        return MYLITE_UNSUPPORTED;
    }

    status = resolve_rename_table_names(database, selected_schema, plan);
    if (status != MYLITE_OK) {
        return status;
    }

    for (size_t index = 0U; index < plan->target_count; ++index) {
        status = validate_rename_table_target_schemas(database, &plan->targets[index]);
        if (status != MYLITE_OK) {
            return status;
        }
    }
    for (size_t index = 0U; index < plan->target_count; ++index) {
        status = validate_rename_table_target(database, plan, index);
        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int resolve_rename_table_names(
    mylite_db *database,
    const char *selected_schema,
    struct mylite_rename_table_plan *plan
) {
    for (size_t index = 0U; index < plan->target_count; ++index) {
        struct mylite_rename_table_target *target = &plan->targets[index];

        if ((target->source_schema_name == NULL || target->target_schema_name == NULL) &&
            (selected_schema == NULL || selected_schema[0] == '\0')) {
            (void)mylite_diagnostics_set_error_message(database, "No database selected");
            return MYLITE_EXEC_ERROR;
        }
        if (target->source_schema_name == NULL) {
            target->source_schema_name = mylite_copy_nonempty_cstring(selected_schema);
            if (target->source_schema_name == NULL) {
                (void)mylite_diagnostics_set_error_message(database, "out of memory");
                return MYLITE_NOMEM;
            }
        }
        if (target->target_schema_name == NULL) {
            target->target_schema_name = mylite_copy_nonempty_cstring(selected_schema);
            if (target->target_schema_name == NULL) {
                (void)mylite_diagnostics_set_error_message(database, "out of memory");
                return MYLITE_NOMEM;
            }
        }
    }
    return MYLITE_OK;
}

static int validate_rename_table_target_schemas(
    mylite_db *database,
    const struct mylite_rename_table_target *target
) {
    struct mylite_schema_presence source_presence;
    struct mylite_schema_presence target_presence;
    int status =
        mylite_catalog_schema_exists(database, target->source_schema_name, &source_presence);

    if (status != MYLITE_OK) {
        return status;
    }
    if (!source_presence.exists) {
        (void)mylite_diagnostics_set_error_message_parts(
            database,
            "Unknown database '",
            target->source_schema_name,
            "'"
        );
        return MYLITE_EXEC_ERROR;
    }
    if (source_presence.is_system) {
        return mylite_diagnostics_set_schema_access_denied_error(
            database,
            target->source_schema_name
        );
    }

    status = mylite_catalog_schema_exists(database, target->target_schema_name, &target_presence);
    if (status != MYLITE_OK) {
        return status;
    }
    if (!target_presence.exists) {
        (void)mylite_diagnostics_set_error_message_parts(
            database,
            "Unknown database '",
            target->target_schema_name,
            "'"
        );
        return MYLITE_EXEC_ERROR;
    }
    if (target_presence.is_system) {
        return mylite_diagnostics_set_schema_access_denied_error(
            database,
            target->target_schema_name
        );
    }
    return MYLITE_OK;
}

static int validate_rename_table_target(
    mylite_db *database,
    const struct mylite_rename_table_plan *plan,
    size_t target_index
) {
    const struct mylite_rename_table_target *target = &plan->targets[target_index];
    bool source_exists = false;
    bool target_exists = false;
    int status = simulated_rename_table_exists_before_target(
        database,
        plan,
        target->source_schema_name,
        target->source_table_name,
        target_index,
        &source_exists
    );

    if (status != MYLITE_OK) {
        return status;
    }
    if (!source_exists) {
        return mylite_diagnostics_set_table_doesnt_exist_error(
            database,
            target->source_schema_name,
            target->source_table_name
        );
    }

    status = simulated_rename_table_exists_before_target(
        database,
        plan,
        target->target_schema_name,
        target->target_table_name,
        target_index,
        &target_exists
    );
    if (status != MYLITE_OK) {
        return status;
    }
    if (target_exists) {
        return set_rename_table_exists_error(
            database,
            target->target_schema_name,
            target->target_table_name
        );
    }
    return MYLITE_OK;
}

static int simulated_rename_table_exists_before_target(
    mylite_db *database,
    const struct mylite_rename_table_plan *plan,
    const char *schema_name,
    const char *table_name,
    size_t target_index,
    bool *out_exists
) {
    int status = mylite_catalog_table_exists(database, schema_name, table_name, out_exists);

    if (status != MYLITE_OK) {
        return status;
    }
    for (size_t index = 0U; index < target_index; ++index) {
        const struct mylite_rename_table_target *target = &plan->targets[index];

        if (rename_table_target_source_matches(target, schema_name, table_name)) {
            *out_exists = false;
        }
        if (rename_table_target_destination_matches(target, schema_name, table_name)) {
            *out_exists = true;
        }
    }
    return MYLITE_OK;
}

static bool rename_table_target_source_matches(
    const struct mylite_rename_table_target *target,
    const char *schema_name,
    const char *table_name
) {
    const int schema_comparison = strcmp(target->source_schema_name, schema_name);
    const int table_comparison = strcmp(target->source_table_name, table_name);

    if (schema_comparison != 0) {
        return false;
    }
    if (table_comparison != 0) {
        return false;
    }
    return true;
}

static bool rename_table_target_destination_matches(
    const struct mylite_rename_table_target *target,
    const char *schema_name,
    const char *table_name
) {
    const int schema_comparison = strcmp(target->target_schema_name, schema_name);
    const int table_comparison = strcmp(target->target_table_name, table_name);

    if (schema_comparison != 0) {
        return false;
    }
    if (table_comparison != 0) {
        return false;
    }
    return true;
}

static int set_rename_table_exists_error(
    mylite_db *database,
    const char *schema_name,
    const char *table_name
) {
    char *message = sqlite3_mprintf("Table '%q.%q' already exists", schema_name, table_name);
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = mylite_diagnostics_set_error_message(database, message);
    sqlite3_free(message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}
