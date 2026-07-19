#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_INFORMATION_SCHEMA_PLAN_SUPPORT_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_INFORMATION_SCHEMA_PLAN_SUPPORT_H

struct mylite_db;
struct information_schema_predicate_value;
struct mylite_result_column_descriptor;
struct mylite_sql_source_span;
struct mylite_sql_ast_node;

enum mylite_execution_information_schema_predicate_value_kind {
    MYLITE_EXECUTION_INFORMATION_SCHEMA_PREDICATE_VALUE = 0,
    MYLITE_EXECUTION_INFORMATION_SCHEMA_PREDICATE_LITERAL = 1,
    MYLITE_EXECUTION_INFORMATION_SCHEMA_PREDICATE_LIKE_PATTERN = 2,
};

int mylite_execution_information_schema_copy_select_item_alias(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *alias,
    char **out_text
);
int mylite_execution_information_schema_copy_aggregate_label(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    char **out_text
);
int mylite_execution_information_schema_make_scalar_result_descriptor(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *label,
    struct mylite_result_column_descriptor *out_descriptor
);
int mylite_execution_information_schema_copy_predicate_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value,
    enum mylite_execution_information_schema_predicate_value_kind kind,
    struct information_schema_predicate_value *out_value
);
int mylite_execution_information_schema_predicate_escape_character(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    char *out_escape_character
);

#endif
