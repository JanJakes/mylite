#include "mylite_select_resolve.h"

#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "mylite_select.h"
#include "mylite_select_order_resolve.h"
#include "mylite_span.h"
#include "sqlite3.h"

#include <stdlib.h>
#include <string.h>

static int resolve_select_plan_column_parts_in_scope(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    char **parts,
    size_t part_count,
    const char *clause_context,
    size_t first_table,
    size_t table_count,
    size_t *out_index
);

static bool select_plan_table_qualifier_matches(
    const struct mylite_select_table *table,
    char **parts,
    size_t part_count
);

static int set_select_unknown_column_parts_error(
    mylite_db *database,
    char **parts,
    size_t part_count,
    const char *clause_context
);

static int set_select_unknown_column_in_clause_error(
    mylite_db *database,
    const char *reference,
    const char *clause_context
);

int mylite_select_resolve_plan_wildcard(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *wildcard,
    size_t *out_table_index,
    bool *out_all
) {
    const struct mylite_sql_ast_node *first = mylite_ast_child_at(wildcard, 0U);
    const struct mylite_sql_ast_node *second = mylite_ast_child_at(wildcard, 1U);
    char *first_name = NULL;
    char *second_name = NULL;
    size_t table_count = mylite_select_plan_table_count(plan);

    *out_table_index = table_count;
    *out_all = false;
    if (first == NULL) {
        *out_all = true;
        return MYLITE_OK;
    }

    first_name = mylite_copy_identifier_span(first);
    if (first_name == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    if (second != NULL) {
        second_name = mylite_copy_identifier_span(second);
        if (second_name == NULL) {
            free(first_name);
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
            return MYLITE_NOMEM;
        }
    }

    for (size_t index = 0U; index < table_count; ++index) {
        const struct mylite_select_table *table = mylite_select_plan_table_const(plan, index);

        if (second == NULL) {
            const char *visible_table = table->alias == NULL ? table->table_name : table->alias;

            if (strcmp(first_name, visible_table) == 0) {
                *out_table_index = index;
                break;
            }
        } else if (
            table->alias == NULL && strcmp(first_name, table->schema_name) == 0 &&
            strcmp(second_name, table->table_name) == 0
        ) {
            *out_table_index = index;
            break;
        }
    }

    free(first_name);
    free(second_name);
    return MYLITE_OK;
}

int mylite_select_resolve_plan_column_reference(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    const char *clause_context,
    size_t *out_index
) {
    return mylite_select_resolve_plan_column_reference_in_scope(
        database,
        plan,
        expression,
        clause_context,
        0U,
        mylite_select_plan_table_count(plan),
        out_index
    );
}

int mylite_select_resolve_plan_column_reference_in_scope(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    const char *clause_context,
    size_t first_table,
    size_t table_count,
    size_t *out_index
) {
    char *parts[3] = {0};
    size_t part_count = 0U;
    int status = mylite_copy_identifier_parts(expression, parts, &part_count);

    *out_index = mylite_select_plan_column_count(plan);
    if (status == MYLITE_OK) {
        status = resolve_select_plan_column_parts_in_scope(
            database,
            plan,
            parts,
            part_count,
            clause_context,
            first_table,
            table_count,
            out_index
        );
    }
    for (size_t index = 0U; index < part_count && index < 3U; ++index) {
        free(parts[index]);
    }
    return status;
}

int mylite_select_resolve_plan_column_parts(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    char **parts,
    size_t part_count,
    const char *clause_context,
    size_t *out_index
) {
    return resolve_select_plan_column_parts_in_scope(
        database,
        plan,
        parts,
        part_count,
        clause_context,
        0U,
        mylite_select_plan_table_count(plan),
        out_index
    );
}

size_t mylite_select_count_plan_column_parts_matches(
    const struct mylite_select_plan *plan,
    char **parts,
    size_t part_count,
    size_t first_table,
    size_t table_count,
    size_t *match_index
) {
    size_t plan_table_count = mylite_select_plan_table_count(plan);
    size_t last_table = first_table + table_count;
    struct mylite_select_table_range range = {
        .first_table = first_table,
        .table_count = table_count,
    };
    size_t match_count = 0U;

    *match_index = mylite_select_plan_column_count(plan);
    if (part_count < 1U || part_count > 3U || first_table > plan_table_count ||
        table_count > plan_table_count - first_table) {
        return 0U;
    }
    if (part_count == 1U) {
        match_count =
            mylite_select_count_column_parts_using_matches(plan, parts[0], range, match_index);
    }

    for (size_t table_index = first_table; table_index < last_table; ++table_index) {
        const struct mylite_select_table *table = mylite_select_plan_table_const(plan, table_index);
        size_t column_index = mylite_select_plan_column_count(plan);

        if (!select_plan_table_qualifier_matches(table, parts, part_count) ||
            mylite_select_resolve_column_in_table(
                plan,
                table,
                parts[part_count - 1U],
                &column_index
            ) != MYLITE_OK) {
            continue;
        }
        if (part_count == 1U &&
            mylite_select_column_index_is_using_column_in_range(plan, column_index, range)) {
            continue;
        }
        *match_index = column_index;
        ++match_count;
    }
    return match_count;
}

char *mylite_select_copy_wildcard_qualifier_name(const struct mylite_sql_ast_node *wildcard) {
    const struct mylite_sql_ast_node *first = mylite_ast_child_at(wildcard, 0U);
    const struct mylite_sql_ast_node *second = mylite_ast_child_at(wildcard, 1U);
    char *first_name = NULL;
    char *second_name = NULL;
    char *name = NULL;

    if (first == NULL) {
        return mylite_copy_span_text("*", 1U);
    }

    first_name = mylite_copy_identifier_span(first);
    if (first_name == NULL) {
        return NULL;
    }
    if (second == NULL) {
        return first_name;
    }

    second_name = mylite_copy_identifier_span(second);
    if (second_name == NULL) {
        free(first_name);
        return NULL;
    }

    name = malloc(strlen(first_name) + strlen(second_name) + 2U);
    if (name != NULL) {
        size_t first_length = strlen(first_name);
        size_t second_length = strlen(second_name);

        memcpy(name, first_name, first_length);
        name[first_length] = '.';
        memcpy(name + first_length + 1U, second_name, second_length);
        name[first_length + 1U + second_length] = '\0';
    }
    free(first_name);
    free(second_name);
    return name;
}

int mylite_select_set_unknown_table_error(mylite_db *database, const char *table_name) {
    int status =
        mylite_diagnostics_set_error_message_parts(database, "Unknown table '", table_name, "'");

    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

int mylite_select_set_unknown_where_column_error(mylite_db *database, const char *column_name) {
    int status = mylite_diagnostics_set_error_message_parts(
        database,
        "Unknown column '",
        column_name,
        "' in 'where clause'"
    );

    if (status == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_append_error(
        database,
        MYLITE_MYSQL_ER_BAD_FIELD_ERROR,
        mylite_error_message(database)
    );
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int resolve_select_plan_column_parts_in_scope(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    char **parts,
    size_t part_count,
    const char *clause_context,
    size_t first_table,
    size_t table_count,
    size_t *out_index
) {
    size_t plan_table_count = mylite_select_plan_table_count(plan);
    size_t match_count = 0U;
    size_t match_index = mylite_select_plan_column_count(plan);

    if (part_count < 1U || part_count > 3U) {
        return MYLITE_UNSUPPORTED;
    }
    if (first_table > plan_table_count || table_count > plan_table_count - first_table) {
        return MYLITE_UNSUPPORTED;
    }

    match_count = mylite_select_count_plan_column_parts_matches(
        plan,
        parts,
        part_count,
        first_table,
        table_count,
        &match_index
    );
    if (match_count == 1U) {
        *out_index = match_index;
        return MYLITE_OK;
    }
    if (match_count > 1U) {
        return mylite_select_set_ambiguous_column_error(
            database,
            parts[part_count - 1U],
            clause_context
        );
    }

    return set_select_unknown_column_parts_error(database, parts, part_count, clause_context);
}

static bool select_plan_table_qualifier_matches(
    const struct mylite_select_table *table,
    char **parts,
    size_t part_count
) {
    if (table == NULL) {
        return false;
    }
    if (part_count == 1U) {
        return true;
    }
    if (part_count == 2U) {
        const char *visible_table = table->alias == NULL ? table->table_name : table->alias;

        return strcmp(parts[0], visible_table) == 0;
    }
    if (part_count == 3U && table->alias == NULL) {
        if (strcmp(parts[0], table->schema_name) != 0) {
            return false;
        }
        return strcmp(parts[1], table->table_name) == 0;
    }
    return false;
}

static int set_select_unknown_column_parts_error(
    mylite_db *database,
    char **parts,
    size_t part_count,
    const char *clause_context
) {
    char *reference = NULL;
    sqlite3_str *text = sqlite3_str_new(database->sqlite);
    int status = MYLITE_OK;

    if (text == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    for (size_t index = 0U; index < part_count; ++index) {
        if (index != 0U) {
            sqlite3_str_append(text, ".", 1);
        }
        sqlite3_str_appendall(text, parts[index]);
    }
    reference = sqlite3_str_finish(text);
    if (reference == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    status = set_select_unknown_column_in_clause_error(database, reference, clause_context);
    sqlite3_free(reference);
    return status;
}

static int set_select_unknown_column_in_clause_error(
    mylite_db *database,
    const char *reference,
    const char *clause_context
) {
    char *message = NULL;
    int status = MYLITE_OK;

    if (clause_context != NULL && strcmp(clause_context, "where clause") == 0) {
        return mylite_select_set_unknown_where_column_error(database, reference);
    }
    if (clause_context != NULL && strcmp(clause_context, "order clause") == 0) {
        return mylite_select_set_unknown_order_column_error(database, reference);
    }
    message = sqlite3_mprintf(
        "Unknown column '%q' in '%q'",
        reference,
        clause_context == NULL ? "field list" : clause_context
    );
    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_set_error_message(database, message);
    if (status == MYLITE_OK) {
        status =
            mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_BAD_FIELD_ERROR, message);
    }
    sqlite3_free(message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}
