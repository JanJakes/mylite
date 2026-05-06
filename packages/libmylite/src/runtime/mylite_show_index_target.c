#include "mylite_show_index_target.h"

#include "mylite_catalog.h"
#include "mylite_diagnostics.h"
#include "mylite_information_schema.h"
#include "mylite_runtime.h"
#include "mylite_span.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static int copy_show_index_table_target(
    const struct mylite_show_index_source_nodes *source,
    struct mylite_show_index_target *out_target
);

static int copy_show_index_selected_schema(
    mylite_db *database,
    struct mylite_show_index_target *target
);

static int normalize_show_index_schema_name(char **schema_name);

int mylite_show_index_copy_target(
    mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct mylite_show_index_target *out_target
) {
    const struct mylite_sql_ast_node *table_name = mylite_ast_child_at(statement, 0U);
    const struct mylite_sql_ast_node *possible_schema = mylite_ast_child_at(statement, 1U);
    const struct mylite_sql_ast_node *explicit_schema =
        possible_schema != NULL && possible_schema->kind == MYLITE_SQL_AST_IDENTIFIER
            ? possible_schema
            : NULL;
    int status = MYLITE_OK;

    *out_target = (struct mylite_show_index_target){0};
    status = copy_show_index_table_target(
        &(const struct mylite_show_index_source_nodes){
            .table_name = table_name,
            .explicit_schema = explicit_schema,
        },
        out_target
    );
    if (status != MYLITE_OK) {
        if (status == MYLITE_UNSUPPORTED) {
            (void)mylite_diagnostics_set_error_message(
                database,
                "SHOW INDEX table names with more than two parts are not "
                "supported"
            );
        }
        return status;
    }
    if (out_target->schema_name == NULL) {
        status = copy_show_index_selected_schema(database, out_target);
    }
    return status;
}

int mylite_show_index_validate_target(
    mylite_db *database,
    struct mylite_show_index_target *target
) {
    struct mylite_schema_presence presence;
    bool exists = false;
    int status = mylite_catalog_schema_exists(database, target->schema_name, &presence);

    if (status != MYLITE_OK) {
        return status;
    }
    if (!presence.exists) {
        (void)mylite_diagnostics_set_error_message_parts(
            database,
            "Unknown database '",
            target->schema_name,
            "'"
        );
        return MYLITE_EXEC_ERROR;
    }
    if (mylite_ascii_case_equal(target->schema_name, "information_schema")) {
        if (!mylite_information_schema_has_table(target->table_name)) {
            return mylite_information_schema_set_unknown_table_error(database, target->table_name);
        }
        return MYLITE_OK;
    }

    status = mylite_catalog_temporary_table_exists(
        database,
        target->schema_name,
        target->table_name,
        &exists
    );
    if (status != MYLITE_OK) {
        return status;
    }
    if (exists) {
        target->temporary = true;
        return MYLITE_OK;
    }
    status = mylite_catalog_persistent_table_exists(
        database,
        target->schema_name,
        target->table_name,
        &exists
    );
    if (status != MYLITE_OK) {
        return status;
    }
    if (!exists) {
        return mylite_diagnostics_set_table_doesnt_exist_error(
            database,
            target->schema_name,
            target->table_name
        );
    }
    target->temporary = false;
    return MYLITE_OK;
}

void mylite_show_index_target_deinit(struct mylite_show_index_target *target) {
    if (target == NULL) {
        return;
    }

    free(target->schema_name);
    free(target->table_name);
    *target = (struct mylite_show_index_target){0};
}

static int copy_show_index_table_target(
    const struct mylite_show_index_source_nodes *source,
    struct mylite_show_index_target *out_target
) {
    char *parts[3] = {0};
    size_t part_count = 0U;
    int status = mylite_copy_identifier_parts(source->table_name, parts, &part_count);

    if (status != MYLITE_OK) {
        goto cleanup;
    }
    if (part_count == 0U || part_count > 2U) {
        status = MYLITE_UNSUPPORTED;
        goto cleanup;
    }

    if (source->explicit_schema != NULL) {
        out_target->schema_name = mylite_copy_identifier_span(source->explicit_schema);
        if (out_target->schema_name == NULL) {
            status = MYLITE_NOMEM;
            goto cleanup;
        }
        out_target->table_name = parts[part_count - 1U];
        parts[part_count - 1U] = NULL;
    } else if (part_count == 2U) {
        out_target->schema_name = parts[0];
        out_target->table_name = parts[1];
        parts[0] = NULL;
        parts[1] = NULL;
    } else {
        out_target->table_name = parts[0];
        parts[0] = NULL;
    }

    if (out_target->schema_name != NULL) {
        status = normalize_show_index_schema_name(&out_target->schema_name);
    }

cleanup:
    for (size_t index = 0U; index < part_count; ++index) {
        free(parts[index]);
    }
    if (status != MYLITE_OK) {
        mylite_show_index_target_deinit(out_target);
    }
    return status;
}

static int copy_show_index_selected_schema(
    mylite_db *database,
    struct mylite_show_index_target *target
) {
    if (database->selected_schema == NULL || database->selected_schema[0] == '\0') {
        (void)mylite_diagnostics_set_error_message(database, "No database selected");
        return MYLITE_EXEC_ERROR;
    }

    target->schema_name = mylite_copy_nonempty_cstring(database->selected_schema);
    if (target->schema_name == NULL) {
        return MYLITE_NOMEM;
    }
    return normalize_show_index_schema_name(&target->schema_name);
}

static int normalize_show_index_schema_name(char **schema_name) {
    char *normalized = NULL;

    if (schema_name == NULL || *schema_name == NULL ||
        !mylite_ascii_case_equal(*schema_name, "information_schema") ||
        strcmp(*schema_name, "information_schema") == 0) {
        return MYLITE_OK;
    }

    normalized = mylite_copy_nonempty_cstring("information_schema");
    if (normalized == NULL) {
        return MYLITE_NOMEM;
    }

    free(*schema_name);
    *schema_name = normalized;
    return MYLITE_OK;
}
