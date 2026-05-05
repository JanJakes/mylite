#include "mylite_statement_functions.h"

#include "mylite_expression.h"
#include "mylite_expression_collation.h"
#include "mylite_function_names.h"
#include "mylite_metadata_constants.h"
#include "mylite_runtime.h"
#include "mylite_session_functions.h"
#include "mylite_span.h"
#include "mylite_statement_strcmp.h"

static int evaluate_charset_collation_function(
    mylite_stmt *stmt, const struct mylite_sql_ast_node *function_call,
    const struct mylite_expression_eval_context *expression_context,
    struct mylite_expression_warnings *warnings, const struct mylite_select_table *table,
    const struct mylite_expression_collation_callbacks *collation_callbacks,
    struct mylite_expression_value *out_value);
static int set_charset_collation_function_result(mylite_db *database,
                                                 const struct mylite_sql_ast_node *name,
                                                 const struct mylite_charset_collation_info *info,
                                                 struct mylite_expression_value *out_value);

int mylite_statement_evaluate_session_function(
    mylite_stmt *stmt, const struct mylite_sql_ast_node *function_call,
    const struct mylite_expression_eval_context *expression_context,
    struct mylite_expression_warnings *warnings, const struct mylite_select_table *table,
    const struct mylite_expression_collation_callbacks *collation_callbacks,
    struct mylite_expression_value *out_value)
{
    const struct mylite_sql_ast_node *name = NULL;
    int status = mylite_session_evaluate_core_function(stmt, function_call, expression_context,
                                                       warnings, out_value);

    if (status != -1) {
        return status;
    }
    name = mylite_ast_child_at(function_call, 0U);
    if (name == NULL || name->kind != MYLITE_SQL_AST_IDENTIFIER) {
        return -1;
    }
    if (mylite_function_name_is_strcmp(name)) {
        return mylite_statement_evaluate_strcmp_function(stmt, function_call, expression_context,
                                                         warnings, table, collation_callbacks,
                                                         out_value);
    }
    if (mylite_function_name_is_charset_collation_introspection(name)) {
        return evaluate_charset_collation_function(stmt, function_call, expression_context,
                                                   warnings, table, collation_callbacks, out_value);
    }
    return -1;
}

static int evaluate_charset_collation_function(
    mylite_stmt *stmt, const struct mylite_sql_ast_node *function_call,
    const struct mylite_expression_eval_context *expression_context,
    struct mylite_expression_warnings *warnings, const struct mylite_select_table *table,
    const struct mylite_expression_collation_callbacks *collation_callbacks,
    struct mylite_expression_value *out_value)
{
    const struct mylite_sql_ast_node *name = mylite_ast_child_at(function_call, 0U);
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(function_call, 1U);
    const struct mylite_sql_ast_node *argument = mylite_ast_child_at(arguments, 0U);
    struct mylite_expression_collation_context collation_context = {
        .plan = stmt == NULL ? NULL : &stmt->select_plan,
        .table = table,
    };
    struct mylite_charset_collation_info info =
        mylite_expression_binary_collation_info(mylite_mysql_coercibility_ignorable);
    struct mylite_expression_value value = {0};
    int status = MYLITE_OK;

    if (stmt == NULL || stmt->database == NULL || argument == NULL) {
        return -1;
    }

    status = mylite_expression_infer_collation_info(stmt->database, &collation_context, argument,
                                                    collation_callbacks, &info);
    if (status != MYLITE_OK) {
        return status;
    }

    status = mylite_expression_eval_with_context(argument, expression_context, warnings, &value);
    mylite_expression_value_deinit(&value);
    if (status != 0) {
        return status;
    }
    return set_charset_collation_function_result(stmt->database, name, &info, out_value);
}

static int set_charset_collation_function_result(mylite_db *database,
                                                 const struct mylite_sql_ast_node *name,
                                                 const struct mylite_charset_collation_info *info,
                                                 struct mylite_expression_value *out_value)
{
    if (mylite_function_name_is_charset(name)) {
        return mylite_session_set_text_function_value(database, info->character_set, out_value);
    }
    if (mylite_function_name_is_collation(name)) {
        return mylite_session_set_text_function_value(database, info->collation, out_value);
    }
    if (mylite_function_name_is_coercibility(name)) {
        *out_value = (struct mylite_expression_value){
            .kind = MYLITE_EXPRESSION_VALUE_INT64,
            .int64_value = info->coercibility,
        };
        return MYLITE_OK;
    }
    return MYLITE_UNSUPPORTED;
}
