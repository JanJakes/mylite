#include "mylite_table_ddl_create_validate.h"

#include "mylite_catalog.h"
#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "mylite_span.h"
#include "mylite_table_ddl.h"
#include "mylite_table_ddl_create_options.h"
#include "mylite_table_ddl_plan_lookup.h"
#include "sqlite3.h"

#include <stdbool.h>
#include <stddef.h>

static bool validate_create_table_column_names(
    mylite_db *database,
    const struct mylite_create_table_plan *plan
);

static bool validate_create_table_indexes(
    mylite_db *database,
    const struct mylite_create_table_plan *plan
);

static int append_create_table_exists_note(mylite_db *database, const char *table_name);

static void apply_create_table_primary_key_nullability(struct mylite_create_table_plan *plan);

static bool create_table_index_name_exists(
    const struct mylite_create_table_plan *plan,
    const char *name,
    size_t before_index
);

int mylite_table_ddl_validate_create_table_plan(
    mylite_db *database,
    const char *schema_name,
    struct mylite_create_table_plan *plan,
    bool if_not_exists,
    struct mylite_schema_default *schema_default,
    bool *out_skip_create
) {
    struct mylite_schema_presence presence;
    bool exists = false;
    int status = mylite_catalog_schema_exists(database, schema_name, &presence);

    *out_skip_create = false;
    if (status != MYLITE_OK) {
        return status;
    }
    if (!presence.exists) {
        (void)mylite_diagnostics_set_error_message_parts(
            database,
            "Unknown database '",
            schema_name,
            "'"
        );
        return MYLITE_EXEC_ERROR;
    }
    if (presence.is_system) {
        (void)mylite_diagnostics_set_error_message_parts(
            database,
            "Access to system schema '",
            schema_name,
            "' is rejected."
        );
        return MYLITE_EXEC_ERROR;
    }

    if (plan->temporary) {
        status =
            mylite_catalog_temporary_table_exists(database, schema_name, plan->table_name, &exists);
    } else {
        status = mylite_catalog_persistent_table_exists(
            database,
            schema_name,
            plan->table_name,
            &exists
        );
    }
    if (status != MYLITE_OK) {
        return status;
    }
    if (exists) {
        if (if_not_exists) {
            int note_status = append_create_table_exists_note(database, plan->table_name);

            if (note_status == MYLITE_OK) {
                *out_skip_create = true;
            }
            return note_status;
        }
        (void)mylite_diagnostics_set_error_message_parts(
            database,
            "Table '",
            plan->table_name,
            "' already exists"
        );
        return MYLITE_EXEC_ERROR;
    }

    status = mylite_catalog_schema_default_by_name(database, schema_name, schema_default);
    if (status != MYLITE_OK) {
        return status;
    }
    status = mylite_table_ddl_normalize_create_table_options(
        database,
        schema_name,
        schema_default,
        &plan->options
    );
    if (status != MYLITE_OK) {
        return status;
    }
    if (!validate_create_table_column_names(database, plan)) {
        return MYLITE_EXEC_ERROR;
    }
    status = mylite_table_ddl_assign_generated_index_names(database, plan);
    if (status != MYLITE_OK) {
        return status;
    }
    if (!validate_create_table_indexes(database, plan)) {
        return MYLITE_EXEC_ERROR;
    }
    apply_create_table_primary_key_nullability(plan);
    return MYLITE_OK;
}

static bool validate_create_table_column_names(
    mylite_db *database,
    const struct mylite_create_table_plan *plan
) {
    if (plan->column_count == 0U) {
        (void)mylite_diagnostics_set_error_message(
            database,
            "CREATE TABLE requires at least one column"
        );
        return false;
    }

    for (size_t left = 0U; left < plan->column_count; ++left) {
        for (size_t right = left + 1U; right < plan->column_count; ++right) {
            if (mylite_ascii_case_equal(plan->columns[left].name, plan->columns[right].name)) {
                (void)mylite_diagnostics_set_error_message_parts(
                    database,
                    "Duplicate column name '",
                    plan->columns[right].name,
                    "'"
                );
                return false;
            }
        }
    }
    return true;
}

static bool validate_create_table_indexes(
    mylite_db *database,
    const struct mylite_create_table_plan *plan
) {
    bool has_primary = false;

    for (size_t index = 0U; index < plan->index_count; ++index) {
        const struct mylite_create_table_index *table_index = &plan->indexes[index];

        if (table_index->is_primary) {
            if (has_primary) {
                (void)
                    mylite_diagnostics_set_error_message(database, "Multiple primary key defined");
                return false;
            }
            has_primary = true;
        }
        if (table_index->explicit_name &&
            create_table_index_name_exists(plan, table_index->name, index)) {
            (void)mylite_diagnostics_set_error_message_parts(
                database,
                "Duplicate key name '",
                table_index->name,
                "'"
            );
            return false;
        }
        for (size_t part = 0U; part < table_index->part_count; ++part) {
            if (mylite_table_ddl_find_create_table_column(
                    plan,
                    table_index->parts[part].column_name
                ) == NULL) {
                (void)mylite_diagnostics_set_error_message_parts(
                    database,
                    "Key column '",
                    table_index->parts[part].column_name,
                    "' doesn't exist in table"
                );
                return false;
            }
        }
    }
    return true;
}

static int append_create_table_exists_note(mylite_db *database, const char *table_name) {
    char *message = sqlite3_mprintf("Table '%q' already exists", table_name);
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = mylite_diagnostics_append_note(database, MYLITE_MYSQL_ER_TABLE_EXISTS_ERROR, message);
    sqlite3_free(message);
    return status;
}

static void apply_create_table_primary_key_nullability(struct mylite_create_table_plan *plan) {
    for (size_t index = 0U; index < plan->index_count; ++index) {
        const struct mylite_create_table_index *table_index = &plan->indexes[index];

        if (!table_index->is_primary) {
            continue;
        }
        for (size_t part = 0U; part < table_index->part_count; ++part) {
            for (size_t column = 0U; column < plan->column_count; ++column) {
                if (mylite_ascii_case_equal(
                        plan->columns[column].name,
                        table_index->parts[part].column_name
                    )) {
                    plan->columns[column].nullable = false;
                }
            }
        }
    }
}

static bool create_table_index_name_exists(
    const struct mylite_create_table_plan *plan,
    const char *name,
    size_t before_index
) {
    for (size_t index = 0U; index < before_index; ++index) {
        if (plan->indexes[index].name != NULL &&
            mylite_ascii_case_equal(plan->indexes[index].name, name)) {
            return true;
        }
    }
    return false;
}
