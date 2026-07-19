#include <mylite/mylite.h>

#include "mylite_ast.h"
#include "mylite_catalog.h"
#include "mylite_connection.h"
#include "mylite_date_interval_second.h"
#include "mylite_dynamic_string.h"
#include "mylite_execution_ast_internal.h"
#include "mylite_execution_catalog.h"
#include "mylite_execution_diagnostics.h"
#include "mylite_execution_information_schema_plan.h"
#include "mylite_execution_information_schema_plan_support.h"
#include "mylite_execution_plan_types.h"
#include "mylite_execution_result_rows.h"
#include "mylite_execution_scalar.h"
#include "mylite_execution_scalar_regexp.h"
#include "mylite_execution_scalar_string_position.h"
#include "mylite_execution_select_order_plan.h"
#include "mylite_execution_sql_normalization.h"
#include "mylite_execution_statement_transaction.h"
#include "mylite_execution_text_internal.h"
#include "mylite_numeric_locale.h"
#include "mylite_result.h"
#include "mylite_spatial.h"
#include "mylite_statement_completion.h"
#include "mylite_statement_context.h"
#include "mylite_string_bitmask.h"
#include "mylite_sys_functions.h"

#include <errno.h>
#include <float.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "mylite_execution_declarations_00_constants.inc"
#include "mylite_execution_declarations_01_types.inc"

enum information_schema_numeric_plan_action {
    INFORMATION_SCHEMA_NUMERIC_PLAN_ENTER = 1,
    INFORMATION_SCHEMA_NUMERIC_PLAN_APPEND_DIVIDE = 2,
};

struct information_schema_numeric_plan_frame {
    enum information_schema_numeric_plan_action action;
    const struct mylite_sql_ast_node *expression;
};

struct information_schema_numeric_plan_stack {
    struct information_schema_numeric_plan_frame *items;
    size_t count;
    size_t capacity;
};

enum information_schema_predicate_plan_action {
    INFORMATION_SCHEMA_PREDICATE_PLAN_VISIT = 0,
    INFORMATION_SCHEMA_PREDICATE_PLAN_APPEND = 1,
};

struct information_schema_predicate_plan_frame {
    const struct mylite_sql_ast_node *node;
    enum information_schema_predicate_plan_action action;
    enum information_schema_predicate_instruction_kind instruction_kind;
};

struct information_schema_predicate_plan_stack {
    struct information_schema_predicate_plan_frame stack_items[if_stack_initial_capacity];
    struct information_schema_predicate_plan_frame *items;
    size_t count;
    size_t capacity;
    bool uses_heap;
};

static int information_schema_plan_projection(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    struct information_schema_query *out_query
);
static int information_schema_plan_wildcard_projection(
    struct mylite_db *database,
    struct information_schema_query *out_query
);
static int information_schema_plan_select_item_projection(
    struct mylite_db *database,
    bool select_list_has_one_item,
    const struct mylite_sql_ast_node *item,
    struct information_schema_query *out_query
);
static int information_schema_append_projection(
    struct mylite_db *database,
    struct information_schema_query *query,
    size_t column_index,
    const struct mylite_sql_ast_node *alias
);
static int information_schema_append_expression_projection(
    struct mylite_db *database,
    struct information_schema_query *query,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_sql_ast_node *alias
);
static int information_schema_append_integer_literal_projection(
    struct mylite_db *database,
    struct information_schema_query *query,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_sql_ast_node *alias
);
static int information_schema_append_group_concat_ordered_column_projection(
    struct mylite_db *database,
    struct information_schema_query *query,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_sql_ast_node *alias
);
static int information_schema_append_logical_not_column_projection(
    struct mylite_db *database,
    struct information_schema_query *query,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_sql_ast_node *alias
);
static int information_schema_append_projection_slot(
    struct mylite_db *database,
    struct information_schema_query *query,
    const struct information_schema_projection_slot_request *request
);
static int information_schema_reserve_projections(
    struct mylite_db *database,
    struct information_schema_query *query,
    size_t required_count
);
static int information_schema_plan_projection_expression(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct information_schema_projection_slot_request *request,
    const char *column_name,
    struct information_schema_projection_expression *out_expression
);
static int information_schema_compile_unsigned_projection_expression(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *expression,
    struct information_schema_projection_expression *out_expression
);
static int information_schema_compile_numeric_projection_node(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct information_schema_numeric_plan_frame *frame,
    struct information_schema_numeric_plan_stack *stack,
    struct information_schema_projection_expression *out_expression
);
static int information_schema_append_numeric_projection_instruction(
    struct mylite_db *database,
    struct information_schema_projection_expression *expression,
    struct information_schema_numeric_projection_instruction instruction
);
static int information_schema_parse_numeric_projection_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    double *out_value
);
static int information_schema_numeric_plan_stack_push(
    struct mylite_db *database,
    struct information_schema_numeric_plan_stack *stack,
    struct information_schema_numeric_plan_frame frame
);
static void information_schema_numeric_plan_stack_deinit(
    struct information_schema_numeric_plan_stack *stack
);
static int information_schema_plan_group(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *group_clause,
    struct information_schema_query *out_query
);
static bool information_schema_projection_is_grouped_statistics_compatible(
    const struct information_schema_query *query,
    size_t projection_index
);
static bool information_schema_group_contains_column(
    const struct information_schema_query *query,
    size_t column_index
);
static int information_schema_validate_group_concat_ordered_column_expression(
    struct mylite_db *database,
    struct information_schema_query *query,
    const struct mylite_sql_ast_node *expression,
    size_t *out_column_index
);
static int information_schema_validate_logical_not_column_expression(
    struct mylite_db *database,
    struct information_schema_query *query,
    const struct mylite_sql_ast_node *expression,
    size_t *out_column_index
);
static int information_schema_plan_order(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_clause,
    struct information_schema_query *out_query
);
static int information_schema_resolve_order_reference(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *order_reference,
    size_t *out_order_index
);
static int information_schema_plan_limit(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *limit_clause,
    struct information_schema_query *out_query
);
static int information_schema_plan_where(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    struct information_schema_query *query
);
static int information_schema_compile_predicate(
    struct mylite_db *database,
    struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node
);
static int information_schema_compile_predicate_visit(
    struct mylite_db *database,
    struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node,
    struct information_schema_predicate_plan_stack *stack
);
static int information_schema_compile_comparison_predicate(
    struct mylite_db *database,
    struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node
);
static int information_schema_compile_is_null_predicate(
    struct mylite_db *database,
    struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node
);
static int information_schema_compile_between_predicate(
    struct mylite_db *database,
    struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node
);
static int information_schema_compile_in_predicate(
    struct mylite_db *database,
    struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node
);
static int information_schema_append_predicate_instruction(
    struct mylite_db *database,
    struct information_schema_predicate_plan *plan,
    struct information_schema_predicate_instruction *instruction
);
static void information_schema_predicate_instruction_deinit(
    struct information_schema_predicate_instruction *instruction
);
static int information_schema_predicate_plan_stack_push(
    struct mylite_db *database,
    struct information_schema_predicate_plan_stack *stack,
    struct information_schema_predicate_plan_frame frame
);
static bool information_schema_predicate_plan_stack_pop(
    struct information_schema_predicate_plan_stack *stack,
    struct information_schema_predicate_plan_frame *out_frame
);
static void information_schema_predicate_plan_stack_deinit(
    struct information_schema_predicate_plan_stack *stack
);
static int information_schema_resolve_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    struct information_schema_query *out_query
);
static const struct mylite_execution_catalog_table_definition *find_information_schema_table_definition(
    const char *table_name
);
static int information_schema_table_definition_index(
    const struct mylite_execution_catalog_table_definition *definition,
    const char *column_name,
    size_t *out_index
);
static int information_schema_resolve_column_reference(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *column_node,
    enum column_reference_diagnostic_context diagnostic_context,
    size_t *out_column_index
);
static int information_schema_column_reference_text(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    char **out_text
);
static int collect_identifier_parts(
    const struct mylite_sql_ast_node *node,
    char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t part_capacity,
    size_t *part_count,
    struct mylite_db *database
);
static enum planned_count_function count_function_from_expression(
    const struct mylite_sql_ast_node *expression
);
static const struct mylite_sql_ast_node *from_table_alias_node(
    const struct mylite_sql_ast_node *from_table
);
static const struct mylite_sql_ast_node *from_table_index_hint_list_node(
    const struct mylite_sql_ast_node *from_table
);
static bool select_list_is_wildcard(const struct mylite_sql_ast_node *select_list);
static bool schema_name_is_information_schema(const char *schema_name);
static bool selected_schema_is_information_schema(const struct mylite_db *database);
static void set_unknown_column_reference_error(
    struct mylite_db *database,
    enum column_reference_diagnostic_context context,
    const char *column_name
);

int mylite_execution_information_schema_plan_projection(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    struct information_schema_query *out_query
) {
    return information_schema_plan_projection(database, select_list, out_query);
}

int mylite_execution_information_schema_plan_group(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *group_clause,
    struct information_schema_query *out_query
) {
    return information_schema_plan_group(database, group_clause, out_query);
}

int mylite_execution_information_schema_plan_order(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_clause,
    struct information_schema_query *out_query
) {
    return information_schema_plan_order(database, order_clause, out_query);
}

int mylite_execution_information_schema_plan_limit(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *limit_clause,
    struct information_schema_query *out_query
) {
    return information_schema_plan_limit(database, limit_clause, out_query);
}

int mylite_execution_information_schema_plan_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    struct information_schema_query *out_query
) {
    return information_schema_plan_where(database, where_clause, out_query);
}

int mylite_execution_information_schema_resolve_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    struct information_schema_query *out_query
) {
    return information_schema_resolve_source(database, from_clause, out_query);
}

int mylite_execution_information_schema_resolve_column_reference(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *column_node,
    enum column_reference_diagnostic_context diagnostic_context,
    size_t *out_column_index
) {
    return information_schema_resolve_column_reference(
        database,
        query,
        column_node,
        diagnostic_context,
        out_column_index
    );
}

int mylite_execution_information_schema_table_definition_index(
    const struct mylite_execution_catalog_table_definition *definition,
    const char *column_name,
    size_t *out_index
) {
    return information_schema_table_definition_index(definition, column_name, out_index);
}

const struct mylite_execution_catalog_table_definition *mylite_execution_find_information_schema_table_definition(
    const char *table_name
) {
    return find_information_schema_table_definition(table_name);
}

void mylite_execution_information_schema_projection_expression_deinit(
    struct information_schema_projection_expression *expression
) {
    if (expression == NULL) {
        return;
    }
    free(expression->instructions);
    free(expression->integer_literal_text);
    *expression = (struct information_schema_projection_expression){0};
}

void mylite_execution_information_schema_predicate_plan_deinit(
    struct information_schema_predicate_plan *plan
) {
    if (plan == NULL) {
        return;
    }
    for (size_t index = 0U; index < plan->instruction_count; ++index) {
        information_schema_predicate_instruction_deinit(&plan->instructions[index]);
    }
    free(plan->instructions);
    *plan = (struct information_schema_predicate_plan){0};
}

#include "mylite_execution_information_schema_predicate_validation.inc"
#include "mylite_execution_information_schema_query_planning.inc"

static int information_schema_resolve_column_reference(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *column_node,
    enum column_reference_diagnostic_context diagnostic_context,
    size_t *out_column_index
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char *display_text = NULL;
    size_t part_count = 0U;
    const char *column_name = NULL;
    bool qualifier_matches = false;
    int rc = collect_identifier_parts(
        column_node,
        parts,
        table_name_part_capacity,
        &part_count,
        database
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (part_count == 1U) {
        column_name = parts[0];
    } else {
        if (part_count == 2U) {
            if (query->has_alias) {
                qualifier_matches =
                    mylite_execution_text_equals_ascii_case_insensitive(parts[0], query->alias);
            } else {
                qualifier_matches = mylite_execution_text_equals_ascii_case_insensitive(
                    parts[0],
                    query->definition->name
                );
            }
        }
        if (qualifier_matches) {
            column_name = parts[1];
        } else {
            rc = information_schema_column_reference_text(database, column_node, &display_text);
            if (rc == MYLITE_OK) {
                set_unknown_column_reference_error(database, diagnostic_context, display_text);
                rc = MYLITE_ERROR;
            }
            free(display_text);
            return rc;
        }
    }

    rc =
        information_schema_table_definition_index(query->definition, column_name, out_column_index);
    if (rc == MYLITE_OK) {
        return MYLITE_OK;
    }
    rc = information_schema_column_reference_text(database, column_node, &display_text);
    if (rc == MYLITE_OK) {
        set_unknown_column_reference_error(database, diagnostic_context, display_text);
        rc = MYLITE_ERROR;
    }
    free(display_text);
    return rc;
}

static int information_schema_column_reference_text(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    char **out_text
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    struct mylite_dynamic_string string;
    size_t part_count = 0U;
    int rc = collect_identifier_parts(
        column_node,
        parts,
        table_name_part_capacity,
        &part_count,
        database
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    mylite_dynamic_string_init(&string);
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < part_count; ++part_index) {
        if (part_index != 0U) {
            rc = mylite_dynamic_string_append_char(&string, '.');
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(&string, parts[part_index]);
        }
    }
    if (rc == MYLITE_OK) {
        *out_text = mylite_dynamic_string_take(&string);
        if (*out_text == NULL) {
            rc = MYLITE_NOMEM;
            set_nomem_error(database);
        }
    }
    mylite_dynamic_string_deinit(&string);
    return rc;
}

static int collect_identifier_parts(
    const struct mylite_sql_ast_node *node,
    char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t part_capacity,
    size_t *part_count,
    struct mylite_db *database
) {
    const struct mylite_sql_ast_node *tail_nodes[table_name_part_capacity];
    const struct mylite_sql_ast_node *current = node;
    size_t tail_count = 0U;
    int rc = MYLITE_OK;

    if (current == NULL || part_count == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    while (current->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        if (tail_count >= part_capacity || tail_count >= table_name_part_capacity) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        tail_nodes[tail_count++] = child_at(current, 1U);
        current = child_at(current, 0U);
        if (current == NULL) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
    }
    if (current->kind != MYLITE_SQL_AST_IDENTIFIER) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (*part_count >= part_capacity) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    rc = copy_identifier_text(
        current,
        parts[*part_count],
        MYLITE_CATALOG_IDENTIFIER_CAPACITY,
        database
    );
    if (rc != MYLITE_OK) {
        return MYLITE_ERROR;
    }
    ++(*part_count);

    while (tail_count > 0U) {
        --tail_count;
        if (*part_count >= part_capacity) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        rc = copy_identifier_text(
            tail_nodes[tail_count],
            parts[*part_count],
            MYLITE_CATALOG_IDENTIFIER_CAPACITY,
            database
        );
        if (rc != MYLITE_OK) {
            return MYLITE_ERROR;
        }
        ++(*part_count);
    }

    return MYLITE_OK;
}

static enum planned_count_function count_function_from_expression(
    const struct mylite_sql_ast_node *expression
) {
    if (expression == NULL) {
        return PLANNED_COUNT_NONE;
    }
    switch (expression->kind) {
    case MYLITE_SQL_AST_COUNT_STAR_FUNCTION:
        return PLANNED_COUNT_STAR;
    case MYLITE_SQL_AST_COUNT_COLUMN_FUNCTION:
        return PLANNED_COUNT_COLUMN;
    case MYLITE_SQL_AST_COUNT_LITERAL_FUNCTION:
        return PLANNED_COUNT_LITERAL;
    case MYLITE_SQL_AST_COUNT_DISTINCT_COLUMN_FUNCTION:
        return PLANNED_COUNT_DISTINCT_COLUMN;
    default:
        return PLANNED_COUNT_NONE;
    }
}

static const struct mylite_sql_ast_node *from_table_alias_node(
    const struct mylite_sql_ast_node *from_table
) {
    const struct mylite_sql_ast_node *child = NULL;

    if (from_table == NULL || from_table->kind != MYLITE_SQL_AST_FROM_TABLE) {
        return NULL;
    }
    child = child_at(from_table, 1U);
    while (child != NULL) {
        if (child->kind == MYLITE_SQL_AST_IDENTIFIER) {
            return child;
        }
        child = child->next_sibling;
    }
    return NULL;
}

static const struct mylite_sql_ast_node *from_table_index_hint_list_node(
    const struct mylite_sql_ast_node *from_table
) {
    const struct mylite_sql_ast_node *child = NULL;

    if (from_table == NULL || from_table->kind != MYLITE_SQL_AST_FROM_TABLE) {
        return NULL;
    }
    child = child_at(from_table, 1U);
    while (child != NULL) {
        if (child->kind == MYLITE_SQL_AST_INDEX_HINT_LIST) {
            return child;
        }
        child = child->next_sibling;
    }
    return NULL;
}

static bool select_list_is_wildcard(const struct mylite_sql_ast_node *select_list) {
    const struct mylite_sql_ast_node *item = child_at(select_list, 0U);
    const struct mylite_sql_ast_node *expression = child_at(item, 0U);

    return mylite_sql_ast_node_child_count(select_list) == 1U && expression != NULL &&
           expression->kind == MYLITE_SQL_AST_WILDCARD;
}

static bool schema_name_is_information_schema(const char *schema_name) {
    return mylite_execution_text_equals_ascii_case_insensitive(schema_name, "information_schema");
}

static bool selected_schema_is_information_schema(const struct mylite_db *database) {
    return database != NULL && database->session.has_selected_schema &&
           schema_name_is_information_schema(database->session.selected_schema);
}

static void set_unknown_column_reference_error(
    struct mylite_db *database,
    enum column_reference_diagnostic_context context,
    const char *column_name
) {
    if (context == COLUMN_REFERENCE_WHERE) {
        set_unknown_where_column_error(database, column_name);
    } else if (context == COLUMN_REFERENCE_ORDER) {
        set_unknown_order_column_error(database, column_name);
    } else if (context == COLUMN_REFERENCE_GROUP) {
        set_unknown_group_column_error(database, column_name);
    } else if (context == COLUMN_REFERENCE_HAVING) {
        set_unknown_having_column_error(database, column_name);
    } else if (context == COLUMN_REFERENCE_ON) {
        set_unknown_on_column_error(database, column_name);
    } else {
        set_unknown_column_error(database, column_name);
    }
}
