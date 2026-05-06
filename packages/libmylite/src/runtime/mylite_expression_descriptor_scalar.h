#ifndef MYLITE_RUNTIME_MYLITE_EXPRESSION_DESCRIPTOR_SCALAR_H
#define MYLITE_RUNTIME_MYLITE_EXPRESSION_DESCRIPTOR_SCALAR_H

#include <mylite/mylite.h>

#include "mylite_field_descriptor.h"

#include <stdbool.h>

struct mylite_expression_value;
struct mylite_sql_ast_node;

bool mylite_expression_descriptor_function_result_nullable(
    bool arguments_nullable, const struct mylite_expression_value *value);
bool mylite_expression_descriptor_infer_text_function(
    mylite_db *database, const struct mylite_sql_ast_node *name,
    const struct mylite_expression_value *value, bool result_nullable,
    struct mylite_field_descriptor *out_descriptor);
bool mylite_expression_descriptor_infer_base_conversion_function(
    mylite_db *database, const struct mylite_sql_ast_node *name,
    struct mylite_field_descriptor *out_descriptor);
bool mylite_expression_descriptor_infer_hash_function(
    mylite_db *database, const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_value *value, struct mylite_field_descriptor *out_descriptor);
bool mylite_expression_descriptor_infer_session_or_inet_function(
    mylite_db *database, const struct mylite_sql_ast_node *name,
    struct mylite_field_descriptor *out_descriptor);
bool mylite_expression_descriptor_infer_strcmp_function(
    const struct mylite_sql_ast_node *name, bool result_nullable,
    struct mylite_field_descriptor *out_descriptor);
bool mylite_expression_descriptor_infer_regexp_scalar_function(
    mylite_db *database, const struct mylite_sql_ast_node *name, bool result_nullable,
    struct mylite_field_descriptor *out_descriptor);
bool mylite_expression_descriptor_infer_json_function(
    mylite_db *database, const struct mylite_sql_ast_node *name, bool result_nullable,
    struct mylite_field_descriptor *out_descriptor);
bool mylite_expression_descriptor_infer_uuid_function(
    mylite_db *database, const struct mylite_sql_ast_node *name,
    struct mylite_field_descriptor *out_descriptor);
bool mylite_expression_descriptor_infer_list_index_function(
    const struct mylite_sql_ast_node *name, bool nullable,
    struct mylite_field_descriptor *out_descriptor);
bool mylite_expression_descriptor_infer_code_search_function(
    const struct mylite_sql_ast_node *name, bool nullable,
    struct mylite_field_descriptor *out_descriptor);

#endif
