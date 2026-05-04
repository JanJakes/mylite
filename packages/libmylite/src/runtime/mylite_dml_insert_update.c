#include "mylite_dml.h"

#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "mylite_span.h"

#include <stdlib.h>

static int validate_insert_update_assignment_value(
    mylite_db *database, const struct mylite_insert_values_plan *values_plan,
    const struct mylite_insert_table *table, const char *schema_name,
    const size_t *source_column_indexes, size_t source_column_count,
    const struct mylite_insert_value *value);
static int resolve_insert_update_column_reference(
    mylite_db *database, const struct mylite_insert_values_plan *values_plan,
    const struct mylite_insert_table *table, const char *schema_name,
    const size_t *source_column_indexes, size_t source_column_count,
    const struct mylite_insert_column_reference *ref, bool *out_candidate,
    size_t *out_column_index);
static size_t insert_alias_column_index(const struct mylite_insert_values_plan *plan,
                                        const struct mylite_insert_table *table,
                                        const size_t *source_column_indexes,
                                        size_t source_column_count, const char *column_name);
static bool insert_row_alias_matches(const struct mylite_insert_values_plan *plan,
                                     const char *table_name);
static size_t insert_table_column_index(const struct mylite_insert_table *table,
                                        const char *column_name);
static size_t
insert_table_column_reference_index(const struct mylite_insert_table *table,
                                    const char *schema_name, const char *table_name,
                                    const struct mylite_insert_column_reference *reference);
static bool
insert_column_reference_qualifiers_match(const struct mylite_insert_column_reference *reference,
                                         const char *schema_name, const char *table_name);
static int set_insert_update_unknown_column_error(mylite_db *database, const char *column_name);
static int set_insert_update_ambiguous_column_error(mylite_db *database, const char *column_name);
static int set_insert_unsupported_expression_error(mylite_db *database);
static int append_insert_values_deprecated_warning(mylite_db *database);

int mylite_dml_validate_insert_update_assignments(
    mylite_db *database, const struct mylite_insert_values_plan *values_plan,
    const struct mylite_insert_duplicate_update_plan *update_plan,
    const struct mylite_insert_table *table, const char *schema_name,
    const size_t *source_column_indexes, size_t source_column_count, size_t **out_column_indexes)
{
    size_t assignment_count;
    size_t *column_indexes = NULL;

    if (database == NULL || values_plan == NULL || update_plan == NULL || table == NULL ||
        schema_name == NULL || out_column_indexes == NULL) {
        return MYLITE_MISUSE;
    }

    *out_column_indexes = NULL;
    if (!update_plan->has_clause) {
        return MYLITE_OK;
    }

    assignment_count = update_plan->assignment_count;
    if (assignment_count == 0U) {
        return MYLITE_UNSUPPORTED;
    }

    column_indexes = calloc(assignment_count, sizeof(*column_indexes));
    if (column_indexes == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    for (size_t index = 0U; index < assignment_count; ++index) {
        const struct mylite_insert_update_assignment *assignment = &update_plan->assignments[index];
        size_t column_index = insert_table_column_reference_index(
            table, schema_name, values_plan->table_name, &assignment->target);
        int status = MYLITE_OK;

        if (column_index == table->column_count) {
            status =
                set_insert_update_unknown_column_error(database, assignment->target.column_name);
            free(column_indexes);
            return status;
        }
        column_indexes[index] = column_index;

        status = validate_insert_update_assignment_value(database, values_plan, table, schema_name,
                                                         source_column_indexes, source_column_count,
                                                         &assignment->value);
        if (status != MYLITE_OK) {
            free(column_indexes);
            return status;
        }
    }

    *out_column_indexes = column_indexes;
    return MYLITE_OK;
}

int mylite_dml_append_insert_update_deprecated_warnings(
    mylite_db *database, const struct mylite_insert_duplicate_update_plan *plan)
{
    size_t warning_count = 0U;

    if (database == NULL || plan == NULL) {
        return MYLITE_MISUSE;
    }
    if (!plan->has_clause) {
        return MYLITE_OK;
    }
    for (size_t index = 0U; index < plan->assignment_count; ++index) {
        warning_count += plan->assignments[index].value.values_function_count;
    }
    for (size_t index = 0U; index < warning_count; ++index) {
        int status = append_insert_values_deprecated_warning(database);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int validate_insert_update_assignment_value(
    mylite_db *database, const struct mylite_insert_values_plan *values_plan,
    const struct mylite_insert_table *table, const char *schema_name,
    const size_t *source_column_indexes, size_t source_column_count,
    const struct mylite_insert_value *value)
{
    if (value == NULL) {
        return set_insert_unsupported_expression_error(database);
    }

    switch (value->kind) {
    case MYLITE_INSERT_VALUE_DEFAULT:
    case MYLITE_INSERT_VALUE_NULL:
    case MYLITE_INSERT_VALUE_INTEGER:
    case MYLITE_INSERT_VALUE_REAL:
    case MYLITE_INSERT_VALUE_TEXT:
    case MYLITE_INSERT_VALUE_CURRENT_TIMESTAMP:
        return MYLITE_OK;
    case MYLITE_INSERT_VALUE_COLUMN_REFERENCE: {
        bool candidate = false;
        size_t column_index = table->column_count;

        (void)candidate;
        return resolve_insert_update_column_reference(
            database, values_plan, table, schema_name, source_column_indexes, source_column_count,
            &value->column_reference, &candidate, &column_index);
    }
    case MYLITE_INSERT_VALUE_VALUES_FUNCTION: {
        size_t column_index = insert_table_column_index(table, value->column_reference.column_name);

        if (column_index == table->column_count) {
            return set_insert_update_unknown_column_error(database,
                                                          value->column_reference.column_name);
        }
        return MYLITE_OK;
    }
    case MYLITE_INSERT_VALUE_UNARY_EXPRESSION:
        return validate_insert_update_assignment_value(database, values_plan, table, schema_name,
                                                       source_column_indexes, source_column_count,
                                                       value->left);
    case MYLITE_INSERT_VALUE_BINARY_EXPRESSION: {
        int status = validate_insert_update_assignment_value(database, values_plan, table,
                                                             schema_name, source_column_indexes,
                                                             source_column_count, value->left);

        if (status != MYLITE_OK) {
            return status;
        }
        return validate_insert_update_assignment_value(database, values_plan, table, schema_name,
                                                       source_column_indexes, source_column_count,
                                                       value->right);
    }
    case MYLITE_INSERT_VALUE_UNSUPPORTED:
        return set_insert_unsupported_expression_error(database);
    }

    return set_insert_unsupported_expression_error(database);
}

static int resolve_insert_update_column_reference(
    mylite_db *database, const struct mylite_insert_values_plan *values_plan,
    const struct mylite_insert_table *table, const char *schema_name,
    const size_t *source_column_indexes, size_t source_column_count,
    const struct mylite_insert_column_reference *ref, bool *out_candidate, size_t *out_column_index)
{
    size_t target_index;
    size_t alias_index = table->column_count;

    *out_candidate = false;
    *out_column_index = table->column_count;
    if (ref->schema_name != NULL) {
        target_index =
            insert_table_column_reference_index(table, schema_name, values_plan->table_name, ref);
        if (target_index == table->column_count) {
            return set_insert_update_unknown_column_error(database, ref->column_name);
        }
        *out_column_index = target_index;
        return MYLITE_OK;
    }
    if (ref->table_name != NULL) {
        if (insert_row_alias_matches(values_plan, ref->table_name)) {
            alias_index = values_plan->alias_column_count == 0U
                              ? insert_table_column_index(table, ref->column_name)
                              : insert_alias_column_index(values_plan, table, source_column_indexes,
                                                          source_column_count, ref->column_name);
            if (alias_index == table->column_count) {
                return set_insert_update_unknown_column_error(database, ref->column_name);
            }
            *out_candidate = true;
            *out_column_index = alias_index;
            return MYLITE_OK;
        }

        target_index =
            insert_table_column_reference_index(table, schema_name, values_plan->table_name, ref);
        if (target_index == table->column_count) {
            return set_insert_update_unknown_column_error(database, ref->column_name);
        }
        *out_column_index = target_index;
        return MYLITE_OK;
    }

    target_index = insert_table_column_index(table, ref->column_name);
    if (values_plan->alias_column_count != 0U) {
        alias_index = insert_alias_column_index(values_plan, table, source_column_indexes,
                                                source_column_count, ref->column_name);
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
    return set_insert_update_unknown_column_error(database, ref->column_name);
}

static size_t insert_alias_column_index(const struct mylite_insert_values_plan *plan,
                                        const struct mylite_insert_table *table,
                                        const size_t *source_column_indexes,
                                        size_t source_column_count, const char *column_name)
{
    for (size_t index = 0U; index < plan->alias_column_count; ++index) {
        if (mylite_ascii_case_equal(plan->alias_columns[index], column_name)) {
            if (index >= source_column_count) {
                return table->column_count;
            }
            if (source_column_indexes != NULL) {
                return source_column_indexes[index];
            }
            if (plan->has_column_list) {
                return insert_table_column_index(table, plan->columns[index]);
            }
            return index;
        }
    }
    return table->column_count;
}

static bool insert_row_alias_matches(const struct mylite_insert_values_plan *plan,
                                     const char *table_name)
{
    if (plan->row_alias == NULL || table_name == NULL) {
        return false;
    }
    return mylite_ascii_case_equal(plan->row_alias, table_name);
}

static size_t insert_table_column_index(const struct mylite_insert_table *table,
                                        const char *column_name)
{
    for (size_t index = 0U; index < table->column_count; ++index) {
        if (mylite_ascii_case_equal(table->columns[index].name, column_name)) {
            return index;
        }
    }
    return table->column_count;
}

static size_t
insert_table_column_reference_index(const struct mylite_insert_table *table,
                                    const char *schema_name, const char *table_name,
                                    const struct mylite_insert_column_reference *reference)
{
    if (!insert_column_reference_qualifiers_match(reference, schema_name, table_name)) {
        return table->column_count;
    }
    return insert_table_column_index(table, reference->column_name);
}

static bool
insert_column_reference_qualifiers_match(const struct mylite_insert_column_reference *reference,
                                         const char *schema_name, const char *table_name)
{
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

static int set_insert_update_unknown_column_error(mylite_db *database, const char *column_name)
{
    int status = mylite_diagnostics_set_error_message_parts(database, "Unknown column '",
                                                            column_name, "' in 'field list'");

    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int set_insert_update_ambiguous_column_error(mylite_db *database, const char *column_name)
{
    int status = mylite_diagnostics_set_error_message_parts(database, "Column '", column_name,
                                                            "' in field list is ambiguous");

    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int set_insert_unsupported_expression_error(mylite_db *database)
{
    if (mylite_diagnostics_set_error_message(database, "Unsupported INSERT value expression") ==
        MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    return MYLITE_EXEC_ERROR;
}

static int append_insert_values_deprecated_warning(mylite_db *database)
{
    static const char message[] =
        "'VALUES function' is deprecated and will be removed in a future release. Please use an "
        "alias (INSERT INTO ... VALUES (...) AS alias) and replace VALUES(col) in the ON "
        "DUPLICATE KEY UPDATE clause with alias.col instead";

    return mylite_diagnostics_append_warning(database, MYLITE_MYSQL_ER_WARN_DEPRECATED_SYNTAX,
                                             message);
}
