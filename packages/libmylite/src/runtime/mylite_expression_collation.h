#ifndef MYLITE_RUNTIME_MYLITE_EXPRESSION_COLLATION_H
#define MYLITE_RUNTIME_MYLITE_EXPRESSION_COLLATION_H

#include <mylite/mylite.h>

#include "mylite_expression_collation_types.h"
#include "mylite_field_descriptor.h"
#include "mylite_select_types.h"
#include "sql/mylite_ast.h"

#include <stdbool.h>
#include <stddef.h>

struct mylite_expression_collation_callbacks {
    int (*infer_expression_descriptor)(mylite_db *database, const struct mylite_select_plan *plan,
                                       const struct mylite_sql_ast_node *expression,
                                       struct mylite_field_descriptor *out_descriptor);
};

int mylite_expression_infer_collation_info(
    mylite_db *database, const struct mylite_expression_collation_context *context,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_collation_callbacks *callbacks,
    struct mylite_charset_collation_info *out_info);
int mylite_expression_infer_function_arguments_collation_info(
    mylite_db *database, const struct mylite_expression_collation_context *context,
    const struct mylite_sql_ast_node *arguments, size_t first_argument, bool numeric_as_connection,
    const struct mylite_expression_collation_callbacks *callbacks,
    struct mylite_charset_collation_info *out_info);
int mylite_expression_infer_function_collation_info(
    mylite_db *database, const struct mylite_expression_collation_context *context,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_collation_callbacks *callbacks,
    struct mylite_charset_collation_info *out_info);
int mylite_expression_infer_descriptor_collation_info(
    mylite_db *database, const struct mylite_expression_collation_context *context,
    const struct mylite_sql_ast_node *expression, int text_coercibility,
    const struct mylite_expression_collation_callbacks *callbacks,
    struct mylite_charset_collation_info *out_info);
struct mylite_charset_collation_info mylite_expression_binary_collation_info(int coercibility);
struct mylite_charset_collation_info
mylite_expression_connection_collation_info(const mylite_db *database, int coercibility);
struct mylite_charset_collation_info
mylite_expression_descriptor_collation_info(const struct mylite_field_descriptor *descriptor,
                                            int text_coercibility);
struct mylite_charset_collation_info
mylite_expression_latin1_swedish_collation_info(int coercibility);
struct mylite_charset_collation_info
mylite_expression_utf8mb3_general_collation_info(int coercibility);
struct mylite_charset_collation_info
mylite_expression_charset_collation_info(const char *charset_name);

#endif
