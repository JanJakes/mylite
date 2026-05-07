#include "mylite_expression_collation.h"

#include "mylite_expression_validation.h"
#include "mylite_function_names.h"
#include "mylite_metadata_constants.h"
#include "mylite_span.h"

#include <stdlib.h>

static int infer_char_function_collation_info(
    mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct mylite_charset_collation_info *out_info
);

static int infer_quote_function_collation_info(
    mylite_db *database,
    const struct mylite_expression_collation_context *context,
    const struct mylite_sql_ast_node *arguments,
    const struct mylite_expression_collation_callbacks *callbacks,
    struct mylite_charset_collation_info *out_info
);

static bool function_name_has_binary_numeric_collation_result(
    const struct mylite_sql_ast_node *name
);

int mylite_expression_infer_function_collation_info( // NOLINT(misc-no-recursion)
    mylite_db *database, const struct mylite_expression_collation_context *context,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_collation_callbacks *callbacks,
    struct mylite_charset_collation_info *out_info)
{
    const struct mylite_sql_ast_node *name = mylite_ast_child_at(expression, 0U);
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);

    if (mylite_function_name_is_char(name)) {
        return infer_char_function_collation_info(database, expression, out_info);
    }
    if (mylite_function_name_has_binary_string_result(name)) {
        *out_info = mylite_expression_binary_collation_info(mylite_mysql_coercibility_coercible);
        return MYLITE_OK;
    }
    if (mylite_function_name_has_connection_string_result(name)) {
        *out_info = mylite_expression_connection_collation_info(
            database,
            mylite_mysql_coercibility_coercible
        );
        return MYLITE_OK;
    }
    if (mylite_function_name_is_uuid(name)) {
        *out_info =
            mylite_expression_utf8mb3_general_collation_info(mylite_mysql_coercibility_coercible);
        return MYLITE_OK;
    }
    if (mylite_function_name_is_inet_ntoa(name)) {
        *out_info = mylite_expression_connection_collation_info(
            database,
            mylite_mysql_coercibility_coercible
        );
        return MYLITE_OK;
    }
    if (mylite_function_name_is_charset(name) || mylite_function_name_is_collation(name)) {
        *out_info =
            mylite_expression_utf8mb3_general_collation_info(mylite_mysql_coercibility_coercible);
        return MYLITE_OK;
    }
    if (function_name_has_binary_numeric_collation_result(name)) {
        *out_info = mylite_expression_binary_collation_info(mylite_mysql_coercibility_numeric);
        return MYLITE_OK;
    }
    if (name != NULL && (mylite_span_equal_ci(name->span, "DATABASE") ||
                         mylite_span_equal_ci(name->span, "SCHEMA") ||
                         mylite_span_equal_ci(name->span, "VERSION"))) {
        *out_info = mylite_expression_utf8mb3_general_collation_info(
            mylite_mysql_coercibility_system_constant
        );
        return MYLITE_OK;
    }
    if (name != NULL && (mylite_span_equal_ci(name->span, "USER") ||
                         mylite_span_equal_ci(name->span, "SESSION_USER") ||
                         mylite_span_equal_ci(name->span, "SYSTEM_USER") ||
                         mylite_span_equal_ci(name->span, "CURRENT_USER") ||
                         mylite_span_equal_ci(name->span, "CURRENT_ROLE"))) {
        *out_info = mylite_expression_utf8mb3_general_collation_info(
            mylite_mysql_coercibility_system_constant
        );
        return MYLITE_OK;
    }
    if (name != NULL && mylite_span_equal_ci(name->span, "IF")) {
        return mylite_expression_infer_function_arguments_collation_info(
            database,
            context,
            arguments,
            1U,
            false,
            callbacks,
            out_info
        );
    }
    if (name != NULL &&
        (mylite_span_equal_ci(name->span, "IFNULL") || mylite_span_equal_ci(name->span, "NULLIF") ||
         mylite_span_equal_ci(name->span, "COALESCE"))) {
        return mylite_expression_infer_function_arguments_collation_info(
            database,
            context,
            arguments,
            0U,
            false,
            callbacks,
            out_info
        );
    }
    if (mylite_function_name_is_quote(name)) {
        return infer_quote_function_collation_info(
            database,
            context,
            arguments,
            callbacks,
            out_info
        );
    }
    if (mylite_function_name_has_text_result(name) ||
        mylite_function_name_has_slice_string_result(name)) {
        size_t first_argument =
            mylite_function_name_is_make_set(name) || mylite_function_name_is_elt(name) ? 1U : 0U;

        return mylite_expression_infer_function_arguments_collation_info(
            database,
            context,
            arguments,
            first_argument,
            true,
            callbacks,
            out_info
        );
    }

    return mylite_expression_infer_descriptor_collation_info(
        database,
        context,
        expression,
        mylite_mysql_coercibility_coercible,
        callbacks,
        out_info
    );
}

static int infer_char_function_collation_info(
    mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct mylite_charset_collation_info *out_info
) {
    const struct mylite_sql_ast_node *charset_node = mylite_ast_child_at(expression, 2U);
    char *charset_name = mylite_copy_schema_text_span(charset_node);

    if (charset_node == NULL) {
        *out_info = mylite_expression_binary_collation_info(mylite_mysql_coercibility_coercible);
        return MYLITE_OK;
    }
    if (charset_name == NULL) {
        return MYLITE_NOMEM;
    }
    int status = mylite_expression_validate_char_function_charset(database, expression);

    if (status != MYLITE_OK) {
        free(charset_name);
        return status;
    }
    *out_info = mylite_expression_charset_collation_info(charset_name);
    free(charset_name);
    return MYLITE_OK;
}

static int infer_quote_function_collation_info( // NOLINT(misc-no-recursion)
    mylite_db *database, const struct mylite_expression_collation_context *context,
    const struct mylite_sql_ast_node *arguments,
    const struct mylite_expression_collation_callbacks *callbacks,
    struct mylite_charset_collation_info *out_info)
{
    struct mylite_charset_collation_info source =
        mylite_expression_binary_collation_info(mylite_mysql_coercibility_ignorable);
    int status = mylite_expression_infer_collation_info(
        database,
        context,
        mylite_ast_child_at(arguments, 0U),
        callbacks,
        &source
    );

    if (status != MYLITE_OK) {
        return status;
    }
    if (source.coercibility == mylite_mysql_coercibility_numeric &&
        mylite_ascii_case_equal(source.character_set, mylite_mysql_binary_charset_name)) {
        *out_info = mylite_expression_latin1_swedish_collation_info(source.coercibility);
        return MYLITE_OK;
    }
    if (mylite_ascii_case_equal(source.character_set, mylite_mysql_binary_charset_name)) {
        *out_info = mylite_expression_connection_collation_info(database, source.coercibility);
        return MYLITE_OK;
    }
    *out_info = source;
    return MYLITE_OK;
}

static bool function_name_has_binary_numeric_collation_result(
    const struct mylite_sql_ast_node *name
) {
    if (mylite_function_name_is_coercibility(name) ||
        mylite_function_name_has_length_result(name) || mylite_function_name_is_bit_count(name) ||
        mylite_function_name_is_crc32(name) || mylite_function_name_is_inet_aton(name) ||
        mylite_function_name_is_is_uuid(name) || mylite_function_name_has_integer_result(name) ||
        mylite_function_name_is_strcmp(name) || mylite_function_name_is_ascii(name) ||
        mylite_function_name_is_ord(name) || mylite_function_name_has_search_result(name) ||
        mylite_function_name_is_field(name) || mylite_function_name_is_find_in_set(name)) {
        return true;
    }
    if (name == NULL) {
        return false;
    }
    if (mylite_span_equal_ci(name->span, "PI") || mylite_span_equal_ci(name->span, "MOD") ||
        mylite_function_name_is_exp(name) || mylite_function_name_is_logarithm(name) ||
        mylite_function_name_is_power(name) || mylite_function_name_is_sqrt(name) ||
        mylite_function_name_is_trigonometric(name) ||
        mylite_function_name_is_inverse_trigonometric(name) ||
        mylite_function_name_is_angle_conversion(name) ||
        mylite_span_equal_ci(name->span, "ISNULL") ||
        mylite_span_equal_ci(name->span, "LAST_INSERT_ID") ||
        mylite_span_equal_ci(name->span, "CONNECTION_ID")) {
        return true;
    }
    if (mylite_function_name_is_uuid_short(name)) {
        return true;
    }
    return mylite_span_equal_ci(name->span, "ROW_COUNT");
}
