#include "mylite_expression_collation.h"

#include "mylite_charset.h"
#include "mylite_expression_collation_leaf.h"
#include "mylite_expression_descriptor.h"
#include "mylite_metadata_constants.h"
#include "mylite_runtime.h"
#include "mylite_span.h"

// NOLINTNEXTLINE(misc-no-recursion)
int mylite_expression_infer_collation_info(
    mylite_db *database, const struct mylite_expression_collation_context *context,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_collation_callbacks *callbacks,
    struct mylite_charset_collation_info *out_info)
{
    const struct mylite_sql_ast_node *node =
        mylite_sql_ast_unwrap_parenthesized_expression(expression);

    if (node == NULL || out_info == NULL) {
        return MYLITE_UNSUPPORTED;
    }

    switch (node->kind) {
    case MYLITE_SQL_AST_LITERAL:
        return mylite_expression_infer_literal_collation_info(database, node, callbacks, out_info);
    case MYLITE_SQL_AST_IDENTIFIER:
    case MYLITE_SQL_AST_QUALIFIED_IDENTIFIER:
        return mylite_expression_infer_identifier_collation_info(database, context, node, callbacks,
                                                                 out_info);
    case MYLITE_SQL_AST_FUNCTION_CALL:
        return mylite_expression_infer_function_collation_info(database, context, node, callbacks,
                                                               out_info);
    case MYLITE_SQL_AST_CAST_EXPRESSION:
        return mylite_expression_infer_cast_collation_info(database, node, callbacks, out_info);
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_TERNARY_EXPRESSION:
    case MYLITE_SQL_AST_CASE_EXPRESSION:
    case MYLITE_SQL_AST_AGGREGATE_CALL:
    case MYLITE_SQL_AST_SUBQUERY_EXPRESSION:
    case MYLITE_SQL_AST_EXISTS_EXPRESSION:
    case MYLITE_SQL_AST_QUANTIFIED_COMPARISON:
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP:
        return mylite_expression_infer_descriptor_collation_info(
            database, context, node, mylite_mysql_coercibility_coercible, callbacks, out_info);
    case MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION:
    case MYLITE_SQL_AST_CREATE_INDEX_STATEMENT:
    case MYLITE_SQL_AST_DROP_INDEX_STATEMENT:
    case MYLITE_SQL_AST_DDL_TABLE_OPTION_LIST:
    case MYLITE_SQL_AST_DDL_TABLE_OPTION:
    case MYLITE_SQL_AST_ALTER_TABLE_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_ITEM_LIST:
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION:
    case MYLITE_SQL_AST_ALTER_TABLE_COLUMN_POSITION:
    case MYLITE_SQL_AST_RENAME_TABLE_STATEMENT:
    case MYLITE_SQL_AST_TRUNCATE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_SHOW_VARIABLES_STATEMENT:
    case MYLITE_SQL_AST_SHOW_STATUS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_ENGINES_STATEMENT:
    case MYLITE_SQL_AST_SHOW_CHARACTER_SET_STATEMENT:
    case MYLITE_SQL_AST_SHOW_COLLATION_STATEMENT:
    case MYLITE_SQL_AST_SHOW_TABLES_STATEMENT:
    case MYLITE_SQL_AST_SHOW_TABLE_STATUS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_INDEX_STATEMENT:
    case MYLITE_SQL_AST_SHOW_CREATE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_SHOW_CREATE_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_SHOW_DIAGNOSTICS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_DIAGNOSTICS_COUNT_STATEMENT:
    case MYLITE_SQL_AST_DESCRIBE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_RENAME_TABLE_PAIR_LIST:
    case MYLITE_SQL_AST_RENAME_TABLE_PAIR:
    case MYLITE_SQL_AST_ROW_CONSTRUCTOR:
    case MYLITE_SQL_AST_QUERY_EXPRESSION:
    case MYLITE_SQL_AST_UNION_EXPRESSION:
    case MYLITE_SQL_AST_QUERY_PRIMARY:
    case MYLITE_SQL_AST_INSERT_DUPLICATE_UPDATE_CLAUSE:
    case MYLITE_SQL_AST_INSERT_UPDATE_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_INSERT_UPDATE_ASSIGNMENT:
    case MYLITE_SQL_AST_INSERT_ROW_ALIAS:
    case MYLITE_SQL_AST_INSERT_ALIAS_COLUMN_LIST:
    case MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST:
    case MYLITE_SQL_AST_CASE_WHEN_LIST:
    case MYLITE_SQL_AST_CASE_WHEN:
    case MYLITE_SQL_AST_EXPRESSION_LIST:
    case MYLITE_SQL_AST_GROUP_BY_CLAUSE:
    case MYLITE_SQL_AST_GROUP_ITEM_LIST:
    case MYLITE_SQL_AST_GROUP_ITEM:
    case MYLITE_SQL_AST_HAVING_CLAUSE:
    case MYLITE_SQL_AST_SCRIPT:
    case MYLITE_SQL_AST_SELECT_STATEMENT:
    case MYLITE_SQL_AST_USE_STATEMENT:
    case MYLITE_SQL_AST_SELECT_LIST:
    case MYLITE_SQL_AST_SELECT_ITEM:
    case MYLITE_SQL_AST_WILDCARD:
    case MYLITE_SQL_AST_FROM_TABLE:
    case MYLITE_SQL_AST_FROM_TABLE_REFERENCES:
    case MYLITE_SQL_AST_TABLE_REFERENCE_LIST:
    case MYLITE_SQL_AST_JOIN_EXPRESSION:
    case MYLITE_SQL_AST_JOIN_CONDITION:
    case MYLITE_SQL_AST_USING_COLUMN_LIST:
    case MYLITE_SQL_AST_USING_COLUMN:
    case MYLITE_SQL_AST_FROM_DUAL:
    case MYLITE_SQL_AST_CREATE_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_ALTER_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_DROP_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_SHOW_SCHEMAS_STATEMENT:
    case MYLITE_SQL_AST_IF_EXISTS:
    case MYLITE_SQL_AST_IF_NOT_EXISTS:
    case MYLITE_SQL_AST_SCHEMA_OPTION_LIST:
    case MYLITE_SQL_AST_SCHEMA_OPTION:
    case MYLITE_SQL_AST_DEFAULT:
    case MYLITE_SQL_AST_SET_NAMES_STATEMENT:
    case MYLITE_SQL_AST_SET_CHARACTER_SET_STATEMENT:
    case MYLITE_SQL_AST_CREATE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_COLUMN_DEFINITION_LIST:
    case MYLITE_SQL_AST_COLUMN_DEFINITION:
    case MYLITE_SQL_AST_COLUMN_TYPE:
    case MYLITE_SQL_AST_COLUMN_TYPE_ATTRIBUTE_LIST:
    case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_LIST:
    case MYLITE_SQL_AST_COLUMN_ATTRIBUTE:
    case MYLITE_SQL_AST_PRIMARY_KEY_CONSTRAINT:
    case MYLITE_SQL_AST_KEY_PART_LIST:
    case MYLITE_SQL_AST_KEY_PART:
    case MYLITE_SQL_AST_INDEX_TYPE:
    case MYLITE_SQL_AST_INDEX_OPTION_LIST:
    case MYLITE_SQL_AST_INDEX_OPTION:
    case MYLITE_SQL_AST_SECONDARY_INDEX:
    case MYLITE_SQL_AST_UNIQUE_INDEX:
    case MYLITE_SQL_AST_TABLE_OPTION_LIST:
    case MYLITE_SQL_AST_TABLE_OPTION:
    case MYLITE_SQL_AST_DROP_TABLE_STATEMENT:
    case MYLITE_SQL_AST_TABLE_NAME_LIST:
    case MYLITE_SQL_AST_INSERT_VALUES_STATEMENT:
    case MYLITE_SQL_AST_INSERT_COLUMN_LIST:
    case MYLITE_SQL_AST_INSERT_ROW_LIST:
    case MYLITE_SQL_AST_INSERT_ROW:
    case MYLITE_SQL_AST_INSERT_VALUE_LIST:
    case MYLITE_SQL_AST_INSERT_SET_STATEMENT:
    case MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT:
    case MYLITE_SQL_AST_REPLACE_VALUES_STATEMENT:
    case MYLITE_SQL_AST_REPLACE_SET_STATEMENT:
    case MYLITE_SQL_AST_WHERE_CLAUSE:
    case MYLITE_SQL_AST_ORDER_BY_CLAUSE:
    case MYLITE_SQL_AST_ORDER_ITEM_LIST:
    case MYLITE_SQL_AST_ORDER_ITEM:
    case MYLITE_SQL_AST_LIMIT_CLAUSE:
    case MYLITE_SQL_AST_LIMIT_BOUND:
    case MYLITE_SQL_AST_UPDATE_STATEMENT:
    case MYLITE_SQL_AST_UPDATE_TARGET:
    case MYLITE_SQL_AST_UPDATE_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_UPDATE_ASSIGNMENT:
    case MYLITE_SQL_AST_UPDATE_LIMIT_CLAUSE:
    case MYLITE_SQL_AST_DELETE_STATEMENT:
    case MYLITE_SQL_AST_DELETE_TARGET:
    case MYLITE_SQL_AST_DELETE_LIMIT_CLAUSE:
    case MYLITE_SQL_AST_START_TRANSACTION_STATEMENT:
    case MYLITE_SQL_AST_BEGIN_TRANSACTION_STATEMENT:
    case MYLITE_SQL_AST_TRANSACTION_CHARACTERISTIC_LIST:
    case MYLITE_SQL_AST_TRANSACTION_CHARACTERISTIC:
    case MYLITE_SQL_AST_COMMIT_STATEMENT:
    case MYLITE_SQL_AST_ROLLBACK_STATEMENT:
    case MYLITE_SQL_AST_TRANSACTION_COMPLETION:
    case MYLITE_SQL_AST_SAVEPOINT_STATEMENT:
    case MYLITE_SQL_AST_ROLLBACK_TO_SAVEPOINT_STATEMENT:
    case MYLITE_SQL_AST_RELEASE_SAVEPOINT_STATEMENT:
        break;
    }
    return MYLITE_UNSUPPORTED;
}

// NOLINTNEXTLINE(misc-no-recursion)
int mylite_expression_infer_function_arguments_collation_info(
    mylite_db *database, const struct mylite_expression_collation_context *context,
    const struct mylite_sql_ast_node *arguments, size_t first_argument, bool numeric_as_connection,
    const struct mylite_expression_collation_callbacks *callbacks,
    struct mylite_charset_collation_info *out_info)
{
    struct mylite_charset_collation_info best =
        mylite_expression_binary_collation_info(mylite_mysql_coercibility_ignorable);
    size_t index = 0U;
    bool saw_candidate = false;

    for (const struct mylite_sql_ast_node *argument = arguments == NULL ? NULL
                                                                        : arguments->first_child;
         argument != NULL; argument = argument->next_sibling, ++index) {
        struct mylite_charset_collation_info current =
            mylite_expression_binary_collation_info(mylite_mysql_coercibility_ignorable);
        int status = MYLITE_OK;

        if (index < first_argument) {
            continue;
        }
        status = mylite_expression_infer_collation_info(database, context, argument, callbacks,
                                                        &current);
        if (status != MYLITE_OK) {
            return status;
        }
        if (numeric_as_connection && current.coercibility == mylite_mysql_coercibility_numeric &&
            mylite_ascii_case_equal(current.character_set, mylite_mysql_binary_charset_name)) {
            current = mylite_expression_connection_collation_info(
                database, mylite_mysql_coercibility_coercible);
        }
        if (current.coercibility == mylite_mysql_coercibility_ignorable) {
            continue;
        }
        if (!saw_candidate || current.coercibility < best.coercibility ||
            (current.coercibility == best.coercibility &&
             mylite_ascii_case_equal(current.character_set, mylite_mysql_binary_charset_name))) {
            best = current;
            saw_candidate = true;
        }
    }
    if (saw_candidate) {
        *out_info = best;
    } else {
        *out_info = mylite_expression_binary_collation_info(mylite_mysql_coercibility_ignorable);
    }
    return MYLITE_OK;
}

struct mylite_charset_collation_info mylite_expression_binary_collation_info(int coercibility)
{
    return (struct mylite_charset_collation_info){
        .character_set = mylite_mysql_binary_charset_name,
        .collation = mylite_mysql_binary_charset_name,
        .coercibility = coercibility,
    };
}

struct mylite_charset_collation_info
mylite_expression_connection_collation_info(const mylite_db *database, int coercibility)
{
    const char *character_set = database == NULL || database->character_set_connection == NULL
                                    ? mylite_charset_default_name()
                                    : database->character_set_connection;
    const char *collation = database == NULL || database->collation_connection == NULL
                                ? mylite_charset_default_collation_name()
                                : database->collation_connection;

    return (struct mylite_charset_collation_info){
        .character_set = character_set,
        .collation = collation,
        .coercibility = coercibility,
    };
}

struct mylite_charset_collation_info
mylite_expression_descriptor_collation_info(const struct mylite_field_descriptor *descriptor,
                                            int text_coercibility)
{
    const struct mylite_collation *collation = NULL;

    if (descriptor == NULL || descriptor->type == MYLITE_FIELD_TYPE_NULL) {
        return mylite_expression_binary_collation_info(mylite_mysql_coercibility_ignorable);
    }
    if (mylite_expression_descriptor_has_numeric_result(descriptor) ||
        descriptor->type == MYLITE_FIELD_TYPE_DATE || descriptor->type == MYLITE_FIELD_TYPE_TIME ||
        descriptor->type == MYLITE_FIELD_TYPE_DATETIME ||
        descriptor->type == MYLITE_FIELD_TYPE_TIMESTAMP ||
        descriptor->type == MYLITE_FIELD_TYPE_YEAR) {
        return mylite_expression_binary_collation_info(mylite_mysql_coercibility_numeric);
    }
    if (descriptor->type != MYLITE_FIELD_TYPE_STRING &&
        descriptor->type != MYLITE_FIELD_TYPE_VAR_STRING &&
        descriptor->type != MYLITE_FIELD_TYPE_BLOB) {
        return mylite_expression_binary_collation_info(mylite_mysql_coercibility_numeric);
    }
    collation = mylite_expression_descriptor_collation_lookup_id(descriptor->charset_id);
    if (collation == NULL) {
        return mylite_expression_binary_collation_info(text_coercibility);
    }
    return (struct mylite_charset_collation_info){
        .character_set = collation->character_set,
        .collation = collation->name,
        .coercibility = text_coercibility,
    };
}

int mylite_expression_infer_descriptor_collation_info(
    mylite_db *database, const struct mylite_expression_collation_context *context,
    const struct mylite_sql_ast_node *expression, int text_coercibility,
    const struct mylite_expression_collation_callbacks *callbacks,
    struct mylite_charset_collation_info *out_info)
{
    struct mylite_field_descriptor descriptor = mylite_expression_descriptor_defaults();
    int status =
        callbacks == NULL || callbacks->infer_expression_descriptor == NULL
            ? MYLITE_UNSUPPORTED
            : callbacks->infer_expression_descriptor(
                  database, context == NULL ? NULL : context->plan, expression, &descriptor);

    if (status != MYLITE_OK) {
        return status;
    }
    *out_info = mylite_expression_descriptor_collation_info(&descriptor, text_coercibility);
    return MYLITE_OK;
}

struct mylite_charset_collation_info
mylite_expression_latin1_swedish_collation_info(int coercibility)
{
    return (struct mylite_charset_collation_info){
        .character_set = mylite_mysql_latin1_charset_name,
        .collation = mylite_mysql_latin1_swedish_ci_collation_name,
        .coercibility = coercibility,
    };
}

struct mylite_charset_collation_info
mylite_expression_utf8mb3_general_collation_info(int coercibility)
{
    return (struct mylite_charset_collation_info){
        .character_set = mylite_mysql_utf8mb3_charset_name,
        .collation = mylite_mysql_utf8mb3_general_ci_collation_name,
        .coercibility = coercibility,
    };
}

struct mylite_charset_collation_info
mylite_expression_charset_collation_info(const char *charset_name)
{
    const struct mylite_charset *charset = mylite_charset_lookup(charset_name);
    const struct mylite_collation *collation =
        charset == NULL ? NULL : mylite_collation_lookup(charset->default_collation);

    if (mylite_ascii_case_equal(charset_name, mylite_mysql_binary_charset_name)) {
        return mylite_expression_binary_collation_info(mylite_mysql_coercibility_coercible);
    }
    if (mylite_ascii_case_equal(charset_name, "utf8")) {
        return mylite_expression_utf8mb3_general_collation_info(
            mylite_mysql_coercibility_coercible);
    }
    if (mylite_ascii_case_equal(charset_name, mylite_mysql_ascii_charset_name)) {
        return (struct mylite_charset_collation_info){
            .character_set = mylite_mysql_ascii_charset_name,
            .collation = mylite_mysql_ascii_general_ci_collation_name,
            .coercibility = mylite_mysql_coercibility_coercible,
        };
    }
    if (charset != NULL && collation != NULL) {
        return (struct mylite_charset_collation_info){
            .character_set = charset->name,
            .collation = collation->name,
            .coercibility = mylite_mysql_coercibility_coercible,
        };
    }
    return mylite_expression_binary_collation_info(mylite_mysql_coercibility_coercible);
}
