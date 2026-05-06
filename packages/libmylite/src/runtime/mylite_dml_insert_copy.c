#include "mylite_dml_insert_copy.h"

#include "mylite_dml_insert_duplicate_update_copy.h"
#include "mylite_dml_insert_rows_copy.h"
#include "mylite_dml_insert_set_copy.h"
#include "mylite_span.h"

#include <stdlib.h>

static int copy_insert_table_name(
    const struct mylite_sql_ast_node *table_name,
    struct mylite_insert_values_plan *plan
);

static int copy_insert_column_list(
    const struct mylite_sql_ast_node *columns,
    struct mylite_insert_values_plan *plan
);

static int add_insert_column(struct mylite_insert_values_plan *plan, char *column_name);

static int copy_insert_row_alias(
    const struct mylite_sql_ast_node *row_alias,
    struct mylite_insert_values_plan *plan
);

static int add_insert_alias_column(struct mylite_insert_values_plan *plan, char *column_name);

int mylite_dml_copy_insert_values_statement(
    const struct mylite_sql_ast_node *statement,
    struct mylite_insert_values_plan *values_plan,
    struct mylite_insert_duplicate_update_plan *update_plan
) {
    const struct mylite_sql_ast_node *table_name = mylite_ast_child_at(statement, 0U);
    const struct mylite_sql_ast_node *second_child = mylite_ast_child_at(statement, 1U);
    const struct mylite_sql_ast_node *columns = NULL;
    const struct mylite_sql_ast_node *rows = NULL;
    const struct mylite_sql_ast_node *row_alias = NULL;
    const struct mylite_sql_ast_node *duplicate_update = NULL;
    int status = MYLITE_OK;

    if (statement == NULL || values_plan == NULL || update_plan == NULL) {
        return MYLITE_MISUSE;
    }

    status = copy_insert_table_name(table_name, values_plan);
    values_plan->ignore = statement->insert_ignore;
    if (second_child != NULL && second_child->kind == MYLITE_SQL_AST_INSERT_COLUMN_LIST) {
        columns = second_child;
        rows = mylite_ast_child_at(statement, 2U);
    } else {
        rows = second_child;
    }
    row_alias = mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_INSERT_ROW_ALIAS);
    duplicate_update =
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_INSERT_DUPLICATE_UPDATE_CLAUSE);

    if (status == MYLITE_OK) {
        status = copy_insert_column_list(columns, values_plan);
    }
    if (status == MYLITE_OK) {
        status = mylite_dml_copy_insert_rows(rows, values_plan);
    }
    if (status == MYLITE_OK) {
        status = copy_insert_row_alias(row_alias, values_plan);
    }
    if (status == MYLITE_OK) {
        status = mylite_dml_copy_insert_duplicate_update_clause(duplicate_update, update_plan);
    }
    return status;
}

int mylite_dml_copy_insert_set_statement(
    const struct mylite_sql_ast_node *statement,
    struct mylite_insert_values_plan *values_plan,
    struct mylite_insert_set_plan *set_plan,
    struct mylite_insert_duplicate_update_plan *update_plan
) {
    const struct mylite_sql_ast_node *table_name = mylite_ast_child_at(statement, 0U);
    const struct mylite_sql_ast_node *assignments = mylite_ast_child_at(statement, 1U);
    const struct mylite_sql_ast_node *row_alias =
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_INSERT_ROW_ALIAS);
    const struct mylite_sql_ast_node *duplicate_update =
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_INSERT_DUPLICATE_UPDATE_CLAUSE);
    int status = MYLITE_OK;

    if (statement == NULL || values_plan == NULL || set_plan == NULL || update_plan == NULL) {
        return MYLITE_MISUSE;
    }

    status = copy_insert_table_name(table_name, values_plan);
    values_plan->ignore = statement->insert_ignore;
    if (status == MYLITE_OK) {
        status = mylite_dml_copy_insert_set_assignments(assignments, set_plan);
    }
    if (status == MYLITE_OK) {
        status = copy_insert_row_alias(row_alias, values_plan);
    }
    if (status == MYLITE_OK) {
        status = mylite_dml_copy_insert_duplicate_update_clause(duplicate_update, update_plan);
    }
    return status;
}

int mylite_dml_copy_replace_values_statement(
    const struct mylite_sql_ast_node *statement,
    struct mylite_insert_values_plan *values_plan
) {
    const struct mylite_sql_ast_node *table_name = mylite_ast_child_at(statement, 0U);
    const struct mylite_sql_ast_node *second_child = mylite_ast_child_at(statement, 1U);
    const struct mylite_sql_ast_node *columns = NULL;
    const struct mylite_sql_ast_node *rows = NULL;
    int status = MYLITE_OK;

    if (statement == NULL || values_plan == NULL) {
        return MYLITE_MISUSE;
    }

    status = copy_insert_table_name(table_name, values_plan);
    values_plan->replace_low_priority = statement->replace_low_priority;
    values_plan->replace_delayed = statement->replace_delayed;
    if (second_child != NULL && second_child->kind == MYLITE_SQL_AST_INSERT_COLUMN_LIST) {
        columns = second_child;
        rows = mylite_ast_child_at(statement, 2U);
    } else {
        rows = second_child;
    }

    if (status == MYLITE_OK) {
        status = copy_insert_column_list(columns, values_plan);
    }
    if (status == MYLITE_OK) {
        status = mylite_dml_copy_insert_rows(rows, values_plan);
    }
    return status;
}

int mylite_dml_copy_replace_set_statement(
    const struct mylite_sql_ast_node *statement,
    struct mylite_insert_values_plan *values_plan,
    struct mylite_insert_set_plan *set_plan
) {
    const struct mylite_sql_ast_node *table_name = mylite_ast_child_at(statement, 0U);
    const struct mylite_sql_ast_node *assignments = mylite_ast_child_at(statement, 1U);
    int status = MYLITE_OK;

    if (statement == NULL || values_plan == NULL || set_plan == NULL) {
        return MYLITE_MISUSE;
    }

    status = copy_insert_table_name(table_name, values_plan);
    values_plan->replace_low_priority = statement->replace_low_priority;
    values_plan->replace_delayed = statement->replace_delayed;
    if (status == MYLITE_OK) {
        status = mylite_dml_copy_insert_set_assignments(assignments, set_plan);
    }
    return status;
}

static int copy_insert_table_name(
    const struct mylite_sql_ast_node *table_name,
    struct mylite_insert_values_plan *plan
) {
    if (table_name == NULL) {
        return MYLITE_NOMEM;
    }
    if (table_name->kind == MYLITE_SQL_AST_IDENTIFIER) {
        plan->table_name = mylite_copy_identifier_span(table_name);
        return plan->table_name == NULL ? MYLITE_NOMEM : MYLITE_OK;
    }
    if (table_name->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER &&
        mylite_ast_child_at(table_name, 0U) != NULL &&
        mylite_ast_child_at(table_name, 1U) != NULL &&
        mylite_ast_child_at(table_name, 0U)->kind == MYLITE_SQL_AST_IDENTIFIER &&
        mylite_ast_child_at(table_name, 1U)->kind == MYLITE_SQL_AST_IDENTIFIER) {
        plan->schema_name = mylite_copy_identifier_span(mylite_ast_child_at(table_name, 0U));
        plan->table_name = mylite_copy_identifier_span(mylite_ast_child_at(table_name, 1U));
        if (plan->schema_name == NULL || plan->table_name == NULL) {
            return MYLITE_NOMEM;
        }
        return MYLITE_OK;
    }
    return MYLITE_UNSUPPORTED;
}

static int copy_insert_column_list(
    const struct mylite_sql_ast_node *columns,
    struct mylite_insert_values_plan *plan
) {
    if (columns == NULL) {
        plan->has_column_list = false;
        return MYLITE_OK;
    }

    plan->has_column_list = true;
    for (const struct mylite_sql_ast_node *column = columns->first_child; column != NULL;
         column = column->next_sibling) {
        char *column_name = mylite_copy_identifier_span(column);
        int status = MYLITE_OK;

        if (column_name == NULL) {
            return MYLITE_NOMEM;
        }
        status = add_insert_column(plan, column_name);
        if (status != MYLITE_OK) {
            free(column_name);
            return status;
        }
    }
    return MYLITE_OK;
}

static int add_insert_column(struct mylite_insert_values_plan *plan, char *column_name) {
    char **columns =
        (char **)realloc((void *)plan->columns, (plan->column_count + 1U) * sizeof(*plan->columns));

    if (columns == NULL) {
        return MYLITE_NOMEM;
    }

    plan->columns = columns;
    plan->columns[plan->column_count++] = column_name;
    return MYLITE_OK;
}

static int copy_insert_row_alias(
    const struct mylite_sql_ast_node *row_alias,
    struct mylite_insert_values_plan *plan
) {
    const struct mylite_sql_ast_node *alias = mylite_ast_child_at(row_alias, 0U);
    const struct mylite_sql_ast_node *columns = mylite_ast_child_at(row_alias, 1U);

    if (row_alias == NULL) {
        return MYLITE_OK;
    }
    if (row_alias->kind != MYLITE_SQL_AST_INSERT_ROW_ALIAS || alias == NULL ||
        alias->kind != MYLITE_SQL_AST_IDENTIFIER) {
        return MYLITE_UNSUPPORTED;
    }

    plan->row_alias = mylite_copy_identifier_span(alias);
    if (plan->row_alias == NULL) {
        return MYLITE_NOMEM;
    }

    if (columns == NULL) {
        return MYLITE_OK;
    }
    if (columns->kind != MYLITE_SQL_AST_INSERT_ALIAS_COLUMN_LIST) {
        return MYLITE_UNSUPPORTED;
    }
    for (const struct mylite_sql_ast_node *column = columns->first_child; column != NULL;
         column = column->next_sibling) {
        char *column_name = mylite_copy_identifier_span(column);
        int status = MYLITE_OK;

        if (column_name == NULL) {
            return MYLITE_NOMEM;
        }
        status = add_insert_alias_column(plan, column_name);
        if (status != MYLITE_OK) {
            free(column_name);
            return status;
        }
    }
    return MYLITE_OK;
}

static int add_insert_alias_column(struct mylite_insert_values_plan *plan, char *column_name) {
    char **columns = (char **)realloc(
        (void *)plan->alias_columns,
        (plan->alias_column_count + 1U) * sizeof(*plan->alias_columns)
    );

    if (columns == NULL) {
        return MYLITE_NOMEM;
    }

    plan->alias_columns = columns;
    plan->alias_columns[plan->alias_column_count++] = column_name;
    return MYLITE_OK;
}
