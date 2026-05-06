#include "mylite_dml.h"

#include "mylite_catalog.h"
#include "mylite_diagnostics.h"
#include "mylite_span.h"

#include <stdlib.h>

static int set_insert_alias_column_count_error(mylite_db *database);

static size_t insert_table_column_index(
    const struct mylite_insert_table *table,
    const char *column_name
);

static size_t insert_table_column_reference_index(
    const struct mylite_insert_table *table,
    const char *schema_name,
    const char *table_name,
    const struct mylite_insert_column_reference *reference
);

static bool insert_column_reference_qualifiers_match(
    const struct mylite_insert_column_reference *reference,
    const char *schema_name,
    const char *table_name
);

int mylite_dml_validate_insert_target(
    mylite_db *database,
    const char *selected_schema,
    const struct mylite_insert_values_plan *plan,
    const char **out_schema_name
) {
    const char *schema_name = NULL;
    struct mylite_schema_presence presence = {false};
    bool exists = false;
    int status = MYLITE_OK;

    if (database == NULL || plan == NULL || out_schema_name == NULL) {
        return MYLITE_MISUSE;
    }

    *out_schema_name = NULL;
    schema_name = plan->schema_name == NULL ? selected_schema : plan->schema_name;
    if (schema_name == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "No database selected");
        return MYLITE_EXEC_ERROR;
    }

    status = mylite_catalog_schema_exists(database, schema_name, &presence);
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

    status = mylite_catalog_table_exists(database, schema_name, plan->table_name, &exists);
    if (status != MYLITE_OK) {
        return status;
    }
    if (!exists) {
        return mylite_diagnostics_set_table_doesnt_exist_error(
            database,
            schema_name,
            plan->table_name
        );
    }

    *out_schema_name = schema_name;
    return MYLITE_OK;
}

int mylite_dml_validate_insert_column_list(
    mylite_db *database,
    const struct mylite_insert_values_plan *plan,
    const struct mylite_insert_table *table,
    size_t **out_column_indexes
) {
    size_t *column_indexes = NULL;

    if (database == NULL || plan == NULL || table == NULL || out_column_indexes == NULL) {
        return MYLITE_MISUSE;
    }

    *out_column_indexes = NULL;
    if (!plan->has_column_list) {
        return MYLITE_OK;
    }
    if (plan->column_count == 0U) {
        return MYLITE_OK;
    }

    column_indexes = calloc(plan->column_count, sizeof(*column_indexes));
    if (column_indexes == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    for (size_t index = 0U; index < plan->column_count; ++index) {
        size_t column_index = insert_table_column_index(table, plan->columns[index]);

        if (column_index == table->column_count) {
            int status = mylite_diagnostics_set_error_message_parts(
                database,
                "Unknown column '",
                plan->columns[index],
                "' in 'field list'"
            );
            free(column_indexes);
            return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
        }
        for (size_t previous = 0U; previous < index; ++previous) {
            if (column_indexes[previous] == column_index) {
                int status = mylite_diagnostics_set_error_message_parts(
                    database,
                    "Column '",
                    plan->columns[index],
                    "' specified twice"
                );
                free(column_indexes);
                return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
            }
        }
        column_indexes[index] = column_index;
    }

    *out_column_indexes = column_indexes;
    return MYLITE_OK;
}

int mylite_dml_validate_insert_row_alias(
    mylite_db *database,
    const struct mylite_insert_values_plan *plan,
    size_t source_column_count
) {
    if (database == NULL || plan == NULL) {
        return MYLITE_MISUSE;
    }

    if (plan->row_alias == NULL) {
        return MYLITE_OK;
    }
    if (mylite_ascii_case_equal(plan->row_alias, plan->table_name)) {
        int status = mylite_diagnostics_set_error_message_parts(
            database,
            "Not unique table/alias: '",
            plan->row_alias,
            "'"
        );

        return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
    }
    if (plan->alias_column_count != 0U && plan->alias_column_count != source_column_count) {
        return set_insert_alias_column_count_error(database);
    }
    for (size_t index = 0U; index < plan->alias_column_count; ++index) {
        for (size_t previous = 0U; previous < index; ++previous) {
            if (mylite_ascii_case_equal(
                    plan->alias_columns[previous],
                    plan->alias_columns[index]
                )) {
                int status = mylite_diagnostics_set_error_message_parts(
                    database,
                    "Duplicate column name '",
                    plan->alias_columns[index],
                    "'"
                );

                return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
            }
        }
    }
    return MYLITE_OK;
}

int mylite_dml_validate_insert_set_assignments(
    mylite_db *database,
    const struct mylite_insert_values_plan *values_plan,
    const struct mylite_insert_set_plan *set_plan,
    const struct mylite_insert_table *table,
    const char *schema_name,
    size_t **out_column_indexes,
    size_t *out_column_index_count
) {
    size_t assignment_count;
    size_t *column_indexes = NULL;

    if (database == NULL || values_plan == NULL || set_plan == NULL || table == NULL ||
        schema_name == NULL || out_column_indexes == NULL || out_column_index_count == NULL) {
        return MYLITE_MISUSE;
    }

    assignment_count = set_plan->assignment_count;
    *out_column_indexes = NULL;
    *out_column_index_count = 0U;
    if (assignment_count == 0U) {
        return MYLITE_UNSUPPORTED;
    }

    column_indexes = calloc(assignment_count, sizeof(*column_indexes));
    if (column_indexes == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    for (size_t index = 0U; index < assignment_count; ++index) {
        const struct mylite_insert_column_reference *target = &set_plan->assignments[index].target;
        size_t column_index = insert_table_column_reference_index(
            table,
            schema_name,
            values_plan->table_name,
            target
        );

        if (column_index == table->column_count) {
            int status = mylite_diagnostics_set_error_message_parts(
                database,
                "Unknown column '",
                target->column_name,
                "' in 'field list'"
            );

            free(column_indexes);
            return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
        }
        column_indexes[index] = column_index;
    }

    for (size_t index = 0U; index < assignment_count; ++index) {
        for (size_t previous = 0U; previous < index; ++previous) {
            if (column_indexes[previous] == column_indexes[index]) {
                int status = mylite_diagnostics_set_error_message_parts(
                    database,
                    "Column '",
                    set_plan->assignments[index].target.column_name,
                    "' specified twice"
                );

                free(column_indexes);
                return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
            }
        }
    }

    *out_column_indexes = column_indexes;
    *out_column_index_count = assignment_count;
    return MYLITE_OK;
}

static int set_insert_alias_column_count_error(mylite_db *database) {
    static const char message[] =
        "In definition of view, derived table or common table expression, "
        "SELECT list and column names list have different column counts";
    int status = mylite_diagnostics_set_error_message(database, message);

    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static size_t insert_table_column_index(
    const struct mylite_insert_table *table,
    const char *column_name
) {
    for (size_t index = 0U; index < table->column_count; ++index) {
        if (mylite_ascii_case_equal(table->columns[index].name, column_name)) {
            return index;
        }
    }
    return table->column_count;
}

static size_t insert_table_column_reference_index(
    const struct mylite_insert_table *table,
    const char *schema_name,
    const char *table_name,
    const struct mylite_insert_column_reference *reference
) {
    if (!insert_column_reference_qualifiers_match(reference, schema_name, table_name)) {
        return table->column_count;
    }
    return insert_table_column_index(table, reference->column_name);
}

static bool insert_column_reference_qualifiers_match(
    const struct mylite_insert_column_reference *reference,
    const char *schema_name,
    const char *table_name
) {
    if (reference->schema_name != NULL &&
        !mylite_ascii_case_equal(reference->schema_name, schema_name)) {
        return false;
    }
    if (reference->table_name != NULL &&
        !mylite_ascii_case_equal(reference->table_name, table_name)) {
        return false;
    }
    return true;
}
