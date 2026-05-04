#include "mylite_dml.h"

#include "mylite_diagnostics.h"
#include "mylite_select_types.h"
#include "mylite_span.h"

#include <stdlib.h>
#include <string.h>

static int copy_update_target_name(mylite_db *database, const char *source, char **out_name);
static size_t update_column_reference_index(const struct mylite_select_table *table,
                                            const struct mylite_update_column_reference *reference);
static bool
update_column_reference_qualifiers_match(const struct mylite_select_table *table,
                                         const struct mylite_update_column_reference *reference);
static size_t update_select_column_index(const struct mylite_select_table *table,
                                         const char *column_name);
static char *
copy_update_column_reference_name(const struct mylite_update_column_reference *reference);
static int set_update_unknown_field_error(mylite_db *database, const char *column_name);
static int set_update_unknown_column_error(mylite_db *database, const char *column_name,
                                           const char *clause_context);

int mylite_dml_copy_update_target_to_select_table(mylite_db *database,
                                                  const struct mylite_update_plan *plan,
                                                  struct mylite_select_table *table)
{
    const struct mylite_update_target *target = NULL;
    int status = MYLITE_OK;

    if (database == NULL || plan == NULL || table == NULL) {
        return MYLITE_MISUSE;
    }

    target = &plan->target;
    if (target->schema_name != NULL) {
        status = copy_update_target_name(database, target->schema_name, &table->schema_name);
        if (status != MYLITE_OK) {
            return status;
        }
    }
    status = copy_update_target_name(database, target->table_name, &table->table_name);
    if (status != MYLITE_OK) {
        return status;
    }
    if (target->alias != NULL) {
        status = copy_update_target_name(database, target->alias, &table->alias);
        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

int mylite_dml_bind_update_assignment_targets(mylite_db *database,
                                              const struct mylite_update_plan *plan,
                                              const struct mylite_select_table *table,
                                              struct mylite_update_bound_assignment *assignments,
                                              size_t assignment_count)
{
    if (database == NULL || plan == NULL || table == NULL) {
        return MYLITE_MISUSE;
    }
    if (assignment_count != plan->assignment_count) {
        return MYLITE_MISUSE;
    }
    if (assignment_count != 0U && (plan->assignments == NULL || assignments == NULL)) {
        return MYLITE_MISUSE;
    }

    for (size_t index = 0U; index < assignment_count; ++index) {
        const struct mylite_update_assignment *assignment = &plan->assignments[index];
        size_t column_index = update_column_reference_index(table, &assignment->target);

        if (column_index == table->column_count) {
            char *reference = copy_update_column_reference_name(&assignment->target);
            int status = MYLITE_OK;

            if (reference == NULL) {
                (void)mylite_diagnostics_set_error_message(database, "out of memory");
                return MYLITE_NOMEM;
            }
            status = set_update_unknown_field_error(database, reference);
            free(reference);
            return status;
        }
        assignments[index] = (struct mylite_update_bound_assignment){
            .column_index = column_index,
            .value = assignment->value,
        };
    }
    return MYLITE_OK;
}

static int copy_update_target_name(mylite_db *database, const char *source, char **out_name)
{
    *out_name = mylite_copy_nonempty_cstring(source);
    if (*out_name == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    return MYLITE_OK;
}

static size_t update_column_reference_index(const struct mylite_select_table *table,
                                            const struct mylite_update_column_reference *reference)
{
    if (!update_column_reference_qualifiers_match(table, reference)) {
        return table->column_count;
    }
    return update_select_column_index(table, reference->column_name);
}

static bool
update_column_reference_qualifiers_match(const struct mylite_select_table *table,
                                         const struct mylite_update_column_reference *reference)
{
    if (reference->schema_name != NULL) {
        if (table->alias != NULL || reference->table_name == NULL) {
            return false;
        }
        if (strcmp(reference->schema_name, table->schema_name) != 0) {
            return false;
        }
        if (strcmp(reference->table_name, table->table_name) != 0) {
            return false;
        }
        return true;
    }
    if (reference->table_name != NULL) {
        const char *visible_table = table->alias == NULL ? table->table_name : table->alias;

        if (strcmp(reference->table_name, visible_table) != 0) {
            return false;
        }
        return true;
    }
    return true;
}

static size_t update_select_column_index(const struct mylite_select_table *table,
                                         const char *column_name)
{
    for (size_t index = 0U; index < table->column_count; ++index) {
        if (mylite_ascii_case_equal(table->columns[index].name, column_name)) {
            return index;
        }
    }
    return table->column_count;
}

static char *
copy_update_column_reference_name(const struct mylite_update_column_reference *reference)
{
    sqlite3_str *text = sqlite3_str_new(NULL);

    if (text == NULL) {
        return NULL;
    }
    if (reference->schema_name != NULL) {
        sqlite3_str_appendf(text, "%s.", reference->schema_name);
    }
    if (reference->table_name != NULL) {
        sqlite3_str_appendf(text, "%s.", reference->table_name);
    }
    sqlite3_str_append(text, reference->column_name == NULL ? "" : reference->column_name,
                       reference->column_name == NULL ? 0 : (int)strlen(reference->column_name));
    return sqlite3_str_finish(text);
}

static int set_update_unknown_field_error(mylite_db *database, const char *column_name)
{
    return set_update_unknown_column_error(database, column_name, "field list");
}

static int set_update_unknown_column_error(mylite_db *database, const char *column_name,
                                           const char *clause_context)
{
    char *message = sqlite3_mprintf("Unknown column '%q' in '%q'", column_name,
                                    clause_context == NULL ? "field list" : clause_context);
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = mylite_diagnostics_set_error_message(database, message);
    sqlite3_free(message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}
