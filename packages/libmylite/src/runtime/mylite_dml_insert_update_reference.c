#include "mylite_dml_insert_update_reference.h"

#include "mylite_diagnostics.h"
#include "mylite_dml_insert_column_reference.h"
#include "mylite_span.h"

static size_t insert_alias_column_index(
    const struct mylite_insert_values_plan *plan,
    const struct mylite_insert_table *table,
    const size_t *source_column_indexes,
    size_t source_column_count,
    const char *column_name
);

static bool insert_row_alias_matches(
    const struct mylite_insert_values_plan *plan,
    const char *table_name
);

static int set_insert_update_ambiguous_column_error(mylite_db *database, const char *column_name);

int mylite_dml_resolve_insert_update_column_reference(
    mylite_db *database,
    const struct mylite_insert_values_plan *values_plan,
    const struct mylite_insert_table *table,
    const char *schema_name,
    const size_t *source_column_indexes,
    size_t source_column_count,
    const struct mylite_insert_column_reference *ref,
    bool *out_candidate,
    size_t *out_column_index
) {
    size_t target_index;
    size_t alias_index = table->column_count;

    *out_candidate = false;
    *out_column_index = table->column_count;
    if (ref->schema_name != NULL) {
        target_index = mylite_dml_insert_table_column_reference_index(
            table,
            schema_name,
            values_plan->table_name,
            ref
        );
        if (target_index == table->column_count) {
            return mylite_dml_set_insert_update_unknown_column_error(database, ref->column_name);
        }
        *out_column_index = target_index;
        return MYLITE_OK;
    }
    if (ref->table_name != NULL) {
        if (insert_row_alias_matches(values_plan, ref->table_name)) {
            alias_index = values_plan->alias_column_count == 0U
                              ? mylite_dml_insert_table_column_index(table, ref->column_name)
                              : insert_alias_column_index(
                                    values_plan,
                                    table,
                                    source_column_indexes,
                                    source_column_count,
                                    ref->column_name
                                );
            if (alias_index == table->column_count) {
                return mylite_dml_set_insert_update_unknown_column_error(
                    database,
                    ref->column_name
                );
            }
            *out_candidate = true;
            *out_column_index = alias_index;
            return MYLITE_OK;
        }

        target_index = mylite_dml_insert_table_column_reference_index(
            table,
            schema_name,
            values_plan->table_name,
            ref
        );
        if (target_index == table->column_count) {
            return mylite_dml_set_insert_update_unknown_column_error(database, ref->column_name);
        }
        *out_column_index = target_index;
        return MYLITE_OK;
    }

    target_index = mylite_dml_insert_table_column_index(table, ref->column_name);
    if (values_plan->alias_column_count != 0U) {
        alias_index = insert_alias_column_index(
            values_plan,
            table,
            source_column_indexes,
            source_column_count,
            ref->column_name
        );
    }
    if (target_index != table->column_count && alias_index != table->column_count) {
        return set_insert_update_ambiguous_column_error(database, ref->column_name);
    }
    if (alias_index != table->column_count) {
        *out_candidate = true;
        *out_column_index = alias_index;
        return MYLITE_OK;
    }
    if (target_index != table->column_count) {
        *out_column_index = target_index;
        return MYLITE_OK;
    }
    return mylite_dml_set_insert_update_unknown_column_error(database, ref->column_name);
}

int mylite_dml_set_insert_update_unknown_column_error(
    mylite_db *database,
    const char *column_name
) {
    int status = mylite_diagnostics_set_error_message_parts(
        database,
        "Unknown column '",
        column_name,
        "' in 'field list'"
    );

    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static size_t insert_alias_column_index(
    const struct mylite_insert_values_plan *plan,
    const struct mylite_insert_table *table,
    const size_t *source_column_indexes,
    size_t source_column_count,
    const char *column_name
) {
    for (size_t index = 0U; index < plan->alias_column_count; ++index) {
        if (mylite_ascii_case_equal(plan->alias_columns[index], column_name)) {
            if (index >= source_column_count) {
                return table->column_count;
            }
            if (source_column_indexes != NULL) {
                return source_column_indexes[index];
            }
            if (plan->has_column_list) {
                return mylite_dml_insert_table_column_index(table, plan->columns[index]);
            }
            return index;
        }
    }
    return table->column_count;
}

static bool insert_row_alias_matches(
    const struct mylite_insert_values_plan *plan,
    const char *table_name
) {
    if (plan->row_alias == NULL || table_name == NULL) {
        return false;
    }
    return mylite_ascii_case_equal(plan->row_alias, table_name);
}

static int set_insert_update_ambiguous_column_error(mylite_db *database, const char *column_name) {
    int status = mylite_diagnostics_set_error_message_parts(
        database,
        "Column '",
        column_name,
        "' in field list is ambiguous"
    );

    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}
