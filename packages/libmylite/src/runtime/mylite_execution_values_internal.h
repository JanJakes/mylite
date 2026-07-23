#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_VALUES_INTERNAL_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_VALUES_INTERNAL_H

struct values_statement_row_shape {
    size_t column_count;
    size_t row_number;
};

static int execute_values_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int validate_values_statement_rows(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *rows,
    size_t *out_column_count
);
static int validate_values_statement_row(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *row,
    struct values_statement_row_shape shape
);
static int validate_values_statement_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value
);
static int validate_values_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value
);
static int validate_values_order_clause(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_clause,
    size_t column_count
);
static int validate_values_order_item(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *item,
    size_t column_count
);
static int validate_values_order_key(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    size_t column_count
);
static int validate_values_order_ordinal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    size_t column_count
);
static int validate_values_order_identifier(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    size_t column_count
);
static bool values_column_name_to_index(
    const char *column_name,
    size_t column_count,
    size_t *out_index
);
static bool values_text_has_ascii_case_insensitive_prefix(const char *text, const char *prefix);
static int plan_values_limit(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *limit_clause,
    struct planned_select_limit *out_limit
);
static int append_values_result_columns(
    struct mylite_db *database,
    mylite_result *result,
    size_t column_count
);
static int append_values_result_rows(
    struct mylite_db *database,
    mylite_result *result,
    const struct mylite_sql_ast_node *rows,
    const struct planned_select_limit *limit,
    size_t column_count
);
static int append_values_result_row(
    struct mylite_db *database,
    mylite_result *result,
    const struct mylite_sql_ast_node *row,
    struct values_statement_row_shape shape
);
static int values_statement_value_cell(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value,
    struct session_scalar_cell *out_cell
);
static int values_statement_integer_cell(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value,
    struct session_scalar_cell *out_cell
);
static struct mylite_result_cell values_result_cell(const struct session_scalar_cell *cell);
static void values_scalar_cells_deinit(struct session_scalar_cell *cells, size_t cell_count);

#endif
