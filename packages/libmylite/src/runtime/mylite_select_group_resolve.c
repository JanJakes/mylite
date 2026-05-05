#include "mylite_select_resolve.h"

#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "mylite_select.h"
#include "mylite_select_order_resolve.h"
#include "mylite_span.h"
#include "sqlite3.h"

#include <stdlib.h>

static int maybe_resolve_select_group_table_reference(mylite_db *database,
                                                      const struct mylite_select_plan *plan,
                                                      char **parts, size_t part_count,
                                                      enum mylite_select_group_key_kind *out_kind,
                                                      size_t *out_index, bool *out_resolved);
static int maybe_resolve_select_group_output_reference(mylite_db *database,
                                                       const struct mylite_select_plan *plan,
                                                       char **parts, size_t part_count,
                                                       enum mylite_select_group_key_kind *out_kind,
                                                       size_t *out_index, bool *out_resolved);
static int maybe_resolve_select_having_table_reference(mylite_db *database,
                                                       const struct mylite_select_plan *plan,
                                                       char **parts, size_t part_count,
                                                       enum mylite_select_order_key_kind *out_kind,
                                                       size_t *out_index, bool emit_warnings,
                                                       bool *out_resolved);
static int maybe_resolve_select_having_output_reference(mylite_db *database,
                                                        const struct mylite_select_plan *plan,
                                                        char **parts, size_t part_count,
                                                        enum mylite_select_order_key_kind *out_kind,
                                                        size_t *out_index, bool *out_resolved);
static int set_select_ambiguous_group_column_warning(mylite_db *database, const char *column_name,
                                                     const char *clause_context);
static int set_select_unknown_having_column_error(mylite_db *database, const char *reference);

int mylite_select_set_unknown_group_column_error(mylite_db *database, const char *column_name)
{
    int status = mylite_diagnostics_set_error_message_parts(database, "Unknown column '",
                                                            column_name, "' in 'group statement'");

    if (status == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_BAD_FIELD_ERROR,
                                             mylite_error_message(database));
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

int mylite_select_resolve_group_reference(mylite_db *database,
                                          const struct mylite_select_plan *plan,
                                          const struct mylite_sql_ast_node *expression,
                                          enum mylite_select_group_key_kind *out_kind,
                                          size_t *out_index)
{
    char *parts[3] = {0};
    size_t part_count = 0U;
    bool resolved = false;
    int status = mylite_copy_identifier_parts(expression, parts, &part_count);

    *out_kind = MYLITE_SELECT_GROUP_KEY_EXPRESSION;
    *out_index = 0U;
    if (status != MYLITE_OK) {
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
        }
        return status;
    }

    status = maybe_resolve_select_group_table_reference(database, plan, parts, part_count, out_kind,
                                                        out_index, &resolved);
    if (status != MYLITE_OK || resolved) {
        goto cleanup;
    }
    status = maybe_resolve_select_group_output_reference(database, plan, parts, part_count,
                                                         out_kind, out_index, &resolved);
    if (status != MYLITE_OK || resolved) {
        goto cleanup;
    }

    {
        char *reference = mylite_select_copy_reference_name(expression);

        if (reference == NULL) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
            status = MYLITE_NOMEM;
            goto cleanup;
        }
        status = mylite_select_set_unknown_group_column_error(database, reference);
        free(reference);
    }

cleanup:
    for (size_t index = 0U; index < part_count && index < 3U; ++index) {
        free(parts[index]);
    }
    return status;
}

int mylite_select_resolve_having_reference(mylite_db *database,
                                           const struct mylite_select_plan *plan,
                                           const struct mylite_sql_ast_node *expression,
                                           enum mylite_select_order_key_kind *out_kind,
                                           size_t *out_index)
{
    return mylite_select_resolve_having_reference_internal(database, plan, expression, out_kind,
                                                           out_index, true);
}

int mylite_select_resolve_having_reference_internal(mylite_db *database,
                                                    const struct mylite_select_plan *plan,
                                                    const struct mylite_sql_ast_node *expression,
                                                    enum mylite_select_order_key_kind *out_kind,
                                                    size_t *out_index, bool emit_warnings)
{
    char *parts[3] = {0};
    size_t part_count = 0U;
    bool resolved = false;
    int status = mylite_copy_identifier_parts(expression, parts, &part_count);

    *out_kind = MYLITE_SELECT_ORDER_KEY_EXPRESSION;
    *out_index = 0U;
    if (status != MYLITE_OK) {
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
        }
        return status;
    }

    status = maybe_resolve_select_having_table_reference(
        database, plan, parts, part_count, out_kind, out_index, emit_warnings, &resolved);
    if (status != MYLITE_OK || resolved) {
        goto cleanup;
    }
    status = maybe_resolve_select_having_output_reference(database, plan, parts, part_count,
                                                          out_kind, out_index, &resolved);
    if (status != MYLITE_OK || resolved) {
        goto cleanup;
    }

    {
        char *reference = mylite_select_copy_reference_name(expression);

        if (reference == NULL) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
            status = MYLITE_NOMEM;
            goto cleanup;
        }
        status = set_select_unknown_having_column_error(database, reference);
        free(reference);
    }

cleanup:
    for (size_t index = 0U; index < part_count && index < 3U; ++index) {
        free(parts[index]);
    }
    return status;
}

bool mylite_select_column_index_is_grouped(const struct mylite_select_plan *plan,
                                           size_t column_index)
{
    for (size_t index = 0U; index < plan->group_key_count; ++index) {
        const struct mylite_select_group_key *group_key = &plan->group_keys[index];
        size_t group_column_index = mylite_select_plan_column_count(plan);
        char *parts[3] = {0};
        size_t part_count = 0U;
        size_t match_count = 0U;

        if (group_key->kind == MYLITE_SELECT_GROUP_KEY_OUTPUT &&
            group_key->output_index < plan->output_count &&
            plan->outputs[group_key->output_index].kind == MYLITE_SELECT_OUTPUT_COLUMN &&
            plan->outputs[group_key->output_index].column_index == column_index) {
            return true;
        }
        if (group_key->kind != MYLITE_SELECT_GROUP_KEY_EXPRESSION ||
            group_key->expression == NULL ||
            (group_key->expression->kind != MYLITE_SQL_AST_IDENTIFIER &&
             group_key->expression->kind != MYLITE_SQL_AST_QUALIFIED_IDENTIFIER)) {
            continue;
        }
        if (mylite_copy_identifier_parts(group_key->expression, parts, &part_count) != MYLITE_OK) {
            for (size_t part = 0U; part < part_count && part < 3U; ++part) {
                free(parts[part]);
            }
            continue;
        }
        match_count = mylite_select_count_plan_column_parts_matches(
            plan, parts, part_count, 0U, mylite_select_plan_table_count(plan), &group_column_index);
        for (size_t part = 0U; part < part_count && part < 3U; ++part) {
            free(parts[part]);
        }
        if (match_count == 1U && group_column_index == column_index) {
            return true;
        }
    }
    return false;
}

static int maybe_resolve_select_group_table_reference(mylite_db *database,
                                                      const struct mylite_select_plan *plan,
                                                      char **parts, size_t part_count,
                                                      enum mylite_select_group_key_kind *out_kind,
                                                      size_t *out_index, bool *out_resolved)
{
    size_t column_index = mylite_select_plan_column_count(plan);
    size_t match_count = 0U;

    *out_resolved = false;
    if (part_count < 1U || part_count > 3U) {
        return MYLITE_OK;
    }

    match_count = mylite_select_count_plan_column_parts_matches(
        plan, parts, part_count, 0U, mylite_select_plan_table_count(plan), &column_index);
    if (match_count == 0U) {
        return MYLITE_OK;
    }
    if (match_count > 1U) {
        return mylite_select_set_ambiguous_column_error(database, parts[part_count - 1U],
                                                        "group statement");
    }

    if (part_count == 1U) {
        size_t output_index = 0U;
        size_t output_matches = mylite_select_output_label_count(plan, parts[0], &output_index);

        if (output_matches != 0U) {
            int status =
                set_select_ambiguous_group_column_warning(database, parts[0], "group statement");

            if (status != MYLITE_OK) {
                return status;
            }
        }
    }
    *out_kind = MYLITE_SELECT_GROUP_KEY_EXPRESSION;
    *out_index = column_index;
    *out_resolved = true;
    return MYLITE_OK;
}

static int maybe_resolve_select_group_output_reference(mylite_db *database,
                                                       const struct mylite_select_plan *plan,
                                                       char **parts, size_t part_count,
                                                       enum mylite_select_group_key_kind *out_kind,
                                                       size_t *out_index, bool *out_resolved)
{
    size_t output_index = 0U;
    size_t output_matches = 0U;

    *out_resolved = false;
    if (part_count != 1U) {
        return MYLITE_OK;
    }

    output_matches = mylite_select_output_label_count(plan, parts[0], &output_index);
    if (output_matches > 1U) {
        return mylite_select_set_ambiguous_order_column_error(database, parts[0]);
    }
    if (output_matches == 1U) {
        *out_kind = MYLITE_SELECT_GROUP_KEY_OUTPUT;
        *out_index = output_index;
        *out_resolved = true;
    }
    return MYLITE_OK;
}

static int maybe_resolve_select_having_table_reference(mylite_db *database,
                                                       const struct mylite_select_plan *plan,
                                                       char **parts, size_t part_count,
                                                       enum mylite_select_order_key_kind *out_kind,
                                                       size_t *out_index, bool emit_warnings,
                                                       bool *out_resolved)
{
    size_t column_index = mylite_select_plan_column_count(plan);
    size_t match_count = 0U;

    *out_resolved = false;
    if (part_count < 1U || part_count > 3U) {
        return MYLITE_OK;
    }

    match_count = mylite_select_count_plan_column_parts_matches(
        plan, parts, part_count, 0U, mylite_select_plan_table_count(plan), &column_index);
    if (match_count == 0U) {
        return MYLITE_OK;
    }
    if (match_count > 1U) {
        return mylite_select_set_ambiguous_column_error(database, parts[part_count - 1U],
                                                        "having clause");
    }
    if (!mylite_select_column_index_is_grouped(plan, column_index)) {
        return MYLITE_OK;
    }

    if (part_count == 1U) {
        size_t output_index = 0U;
        size_t output_matches = mylite_select_output_label_count(plan, parts[0], &output_index);

        if (emit_warnings && output_matches != 0U) {
            int status =
                set_select_ambiguous_group_column_warning(database, parts[0], "having clause");

            if (status != MYLITE_OK) {
                return status;
            }
        }
    }
    *out_kind = MYLITE_SELECT_ORDER_KEY_EXPRESSION;
    *out_index = column_index;
    *out_resolved = true;
    return MYLITE_OK;
}

static int maybe_resolve_select_having_output_reference(mylite_db *database,
                                                        const struct mylite_select_plan *plan,
                                                        char **parts, size_t part_count,
                                                        enum mylite_select_order_key_kind *out_kind,
                                                        size_t *out_index, bool *out_resolved)
{
    size_t output_index = 0U;
    size_t output_matches = 0U;

    *out_resolved = false;
    if (part_count != 1U) {
        return MYLITE_OK;
    }

    output_matches = mylite_select_output_label_count(plan, parts[0], &output_index);
    if (output_matches > 1U) {
        return mylite_select_set_ambiguous_order_column_error(database, parts[0]);
    }
    if (output_matches == 1U) {
        *out_kind = MYLITE_SELECT_ORDER_KEY_OUTPUT;
        *out_index = output_index;
        *out_resolved = true;
    }
    return MYLITE_OK;
}

static int set_select_ambiguous_group_column_warning(mylite_db *database, const char *column_name,
                                                     const char *clause_context)
{
    char *message = sqlite3_mprintf("Column '%q' in %s is ambiguous", column_name,
                                    clause_context == NULL ? "group statement" : clause_context);
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = mylite_diagnostics_append_warning(database, MYLITE_MYSQL_ER_NON_UNIQ_ERROR, message);
    sqlite3_free(message);
    return status;
}

static int set_select_unknown_having_column_error(mylite_db *database, const char *reference)
{
    int status = mylite_diagnostics_set_error_message_parts(database, "Unknown column '", reference,
                                                            "' in 'having clause'");

    if (status == MYLITE_OK) {
        status = mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_BAD_FIELD_ERROR,
                                                 mylite_error_message(database));
        status = status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
    }
    return status;
}
