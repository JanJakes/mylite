#include "mylite_information_schema_target.h"

#include "mylite_span.h"

#include <stdbool.h>
#include <stdlib.h>

static bool select_list_is_unqualified_wildcard(const struct mylite_sql_ast_node *select_list);
static int
information_schema_table_from_from_clause(const struct mylite_sql_ast_node *from_clause,
                                          enum mylite_information_schema_table *out_table);
static int information_schema_from_clause_references_table(const struct mylite_sql_ast_node *node,
                                                           bool *out_references_table);
static int
information_schema_table_from_qualified_name(const struct mylite_sql_ast_node *identifier,
                                             enum mylite_information_schema_table *out_table);

int mylite_information_schema_table_from_select(const struct mylite_sql_ast_node *statement,
                                                enum mylite_information_schema_table *out_table)
{
    const struct mylite_sql_ast_node *select_list = mylite_ast_child_at(statement, 0U);
    const struct mylite_sql_ast_node *from_clause = mylite_ast_child_at(statement, 1U);
    enum mylite_information_schema_table table = MYLITE_INFORMATION_SCHEMA_NONE;
    int status = information_schema_table_from_from_clause(from_clause, &table);

    *out_table = MYLITE_INFORMATION_SCHEMA_NONE;
    if (status != MYLITE_OK) {
        return status;
    }
    if (table == MYLITE_INFORMATION_SCHEMA_NONE) {
        return MYLITE_OK;
    }
    if (statement->select_duplicate_mode_explicit) {
        return MYLITE_UNSUPPORTED;
    }
    if (mylite_ast_child_at(statement, 2U) != NULL) {
        return MYLITE_UNSUPPORTED;
    }
    if (!select_list_is_unqualified_wildcard(select_list)) {
        return MYLITE_UNSUPPORTED;
    }

    *out_table = table;
    return MYLITE_OK;
}

enum mylite_information_schema_table mylite_information_schema_table_from_name(const char *name)
{
    if (mylite_ascii_case_equal(name, "schemata")) {
        return MYLITE_INFORMATION_SCHEMA_SCHEMATA;
    }
    if (mylite_ascii_case_equal(name, "tables")) {
        return MYLITE_INFORMATION_SCHEMA_TABLES;
    }
    if (mylite_ascii_case_equal(name, "columns")) {
        return MYLITE_INFORMATION_SCHEMA_COLUMNS;
    }
    if (mylite_ascii_case_equal(name, "statistics")) {
        return MYLITE_INFORMATION_SCHEMA_STATISTICS;
    }
    if (mylite_ascii_case_equal(name, "engines")) {
        return MYLITE_INFORMATION_SCHEMA_ENGINES;
    }
    if (mylite_ascii_case_equal(name, "character_sets")) {
        return MYLITE_INFORMATION_SCHEMA_CHARACTER_SETS;
    }
    if (mylite_ascii_case_equal(name, "collations")) {
        return MYLITE_INFORMATION_SCHEMA_COLLATIONS;
    }
    if (mylite_ascii_case_equal(name, "collation_character_set_applicability")) {
        return MYLITE_INFORMATION_SCHEMA_COLLATION_CHARACTER_SET_APPLICABILITY;
    }
    if (mylite_ascii_case_equal(name, "keywords")) {
        return MYLITE_INFORMATION_SCHEMA_KEYWORDS;
    }
    if (mylite_ascii_case_equal(name, "table_constraints")) {
        return MYLITE_INFORMATION_SCHEMA_TABLE_CONSTRAINTS;
    }
    if (mylite_ascii_case_equal(name, "key_column_usage")) {
        return MYLITE_INFORMATION_SCHEMA_KEY_COLUMN_USAGE;
    }
    if (mylite_ascii_case_equal(name, "check_constraints")) {
        return MYLITE_INFORMATION_SCHEMA_CHECK_CONSTRAINTS;
    }
    if (mylite_ascii_case_equal(name, "referential_constraints")) {
        return MYLITE_INFORMATION_SCHEMA_REFERENTIAL_CONSTRAINTS;
    }
    return MYLITE_INFORMATION_SCHEMA_NONE;
}

static bool select_list_is_unqualified_wildcard(const struct mylite_sql_ast_node *select_list)
{
    const struct mylite_sql_ast_node *select_item = mylite_ast_child_at(select_list, 0U);
    const struct mylite_sql_ast_node *expression = mylite_ast_child_at(select_item, 0U);

    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST ||
        select_item == NULL || select_item->next_sibling != NULL ||
        select_item->kind != MYLITE_SQL_AST_SELECT_ITEM || expression == NULL ||
        expression->kind != MYLITE_SQL_AST_WILDCARD ||
        mylite_ast_child_at(expression, 0U) != NULL) {
        return false;
    }
    return true;
}

static int
information_schema_table_from_from_clause(const struct mylite_sql_ast_node *from_clause,
                                          enum mylite_information_schema_table *out_table)
{
    const struct mylite_sql_ast_node *identifier = mylite_ast_child_at(from_clause, 0U);
    bool references_table = false;
    int status = MYLITE_OK;

    *out_table = MYLITE_INFORMATION_SCHEMA_NONE;
    if (from_clause == NULL) {
        return MYLITE_OK;
    }
    if (from_clause->kind != MYLITE_SQL_AST_FROM_TABLE) {
        status = information_schema_from_clause_references_table(from_clause, &references_table);
        if (status != MYLITE_OK) {
            return status;
        }
        if (references_table) {
            return MYLITE_UNSUPPORTED;
        }
        return MYLITE_OK;
    }

    status = information_schema_table_from_qualified_name(identifier, out_table);
    if (status != MYLITE_OK || *out_table == MYLITE_INFORMATION_SCHEMA_NONE) {
        return status;
    }
    if (mylite_ast_child_at(from_clause, 1U) != NULL) {
        return MYLITE_UNSUPPORTED;
    }
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int information_schema_from_clause_references_table(const struct mylite_sql_ast_node *node,
                                                           bool *out_references_table)
{
    enum mylite_information_schema_table table = MYLITE_INFORMATION_SCHEMA_NONE;
    int status = MYLITE_OK;

    if (node == NULL) {
        return MYLITE_OK;
    }

    if (node->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        status = information_schema_table_from_qualified_name(node, &table);
        if (status != MYLITE_OK) {
            return status;
        }
        if (table != MYLITE_INFORMATION_SCHEMA_NONE) {
            *out_references_table = true;
        }
    }

    for (const struct mylite_sql_ast_node *child = node->first_child; child != NULL;
         child = child->next_sibling) {
        status = information_schema_from_clause_references_table(child, out_references_table);
        if (status != MYLITE_OK || *out_references_table) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int
information_schema_table_from_qualified_name(const struct mylite_sql_ast_node *identifier,
                                             enum mylite_information_schema_table *out_table)
{
    const struct mylite_sql_ast_node *schema = mylite_ast_child_at(identifier, 0U);
    const struct mylite_sql_ast_node *table = mylite_ast_child_at(identifier, 1U);
    char *schema_name = NULL;
    char *table_name = NULL;

    *out_table = MYLITE_INFORMATION_SCHEMA_NONE;
    if (identifier == NULL || identifier->kind != MYLITE_SQL_AST_QUALIFIED_IDENTIFIER ||
        schema == NULL || schema->kind != MYLITE_SQL_AST_IDENTIFIER || table == NULL ||
        table->kind != MYLITE_SQL_AST_IDENTIFIER) {
        return MYLITE_OK;
    }

    schema_name = mylite_copy_identifier_span(schema);
    table_name = mylite_copy_identifier_span(table);
    if (schema_name == NULL || table_name == NULL) {
        free(schema_name);
        free(table_name);
        return MYLITE_NOMEM;
    }

    if (mylite_ascii_case_equal(schema_name, "information_schema")) {
        *out_table = mylite_information_schema_table_from_name(table_name);
        if (*out_table == MYLITE_INFORMATION_SCHEMA_NONE) {
            free(schema_name);
            free(table_name);
            return MYLITE_UNSUPPORTED;
        }
    }

    free(schema_name);
    free(table_name);
    return MYLITE_OK;
}
