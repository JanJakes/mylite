#include "mylite_expression_descriptor_string.h"

#include "mylite_diagnostics.h"
#include "mylite_expression_descriptor.h"
#include "mylite_expression_validation.h"
#include "mylite_function_names.h"
#include "mylite_metadata_constants.h"
#include "mylite_span.h"
#include "sql/mylite_ast.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int mylite_expression_descriptor_infer_char_function(mylite_db *database,
                                                     const struct mylite_sql_ast_node *expression,
                                                     struct mylite_field_descriptor *out_descriptor)
{
    const struct mylite_sql_ast_node *name = mylite_ast_child_at(expression, 0U);
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    const struct mylite_sql_ast_node *charset_node = mylite_ast_child_at(expression, 2U);
    char *charset_name = NULL;
    bool binary_result = false;
    size_t arity = 0U;
    uint64_t length = 0U;
    unsigned int flags = 0U;
    unsigned int charset_id = mylite_expression_descriptor_connection_charset_id(database);

    if (!mylite_function_name_is_char(name)) {
        return MYLITE_UNSUPPORTED;
    }

    charset_name = charset_node == NULL
                       ? mylite_copy_span_text(mylite_mysql_binary_charset_name,
                                               strlen(mylite_mysql_binary_charset_name))
                       : mylite_copy_schema_text_span(charset_node);
    if (charset_name == NULL) {
        return MYLITE_NOMEM;
    }

    if (mylite_ascii_case_equal(charset_name, "binary")) {
        binary_result = true;
    } else if (!mylite_expression_char_function_charset_name_is_supported(charset_name)) {
        int status = mylite_diagnostics_set_unknown_charset_error(database, charset_name);

        free(charset_name);
        return status;
    }
    free(charset_name);
    if (binary_result) {
        flags = MYLITE_FIELD_FLAG_BINARY;
        charset_id = mylite_mysql_binary_charset_id;
    }

    arity = mylite_sql_ast_node_child_count(arguments);
    if (arity > UINT64_MAX / mylite_mysql_char_function_argument_bytes) {
        length = mylite_mysql_long_text_length;
    } else {
        length = (uint64_t)arity * mylite_mysql_char_function_argument_bytes;
        if (!binary_result) {
            uint64_t max_bytes_per_character =
                mylite_expression_descriptor_connection_character_max_length(database);

            if (max_bytes_per_character != 0U && length > UINT64_MAX / max_bytes_per_character) {
                length = mylite_mysql_long_text_length;
            } else {
                length *= max_bytes_per_character;
            }
        }
    }

    *out_descriptor = (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_VAR_STRING,
        .flags = flags,
        .length = length,
        .decimals = mylite_mysql_not_fixed_decimals,
        .charset_id = charset_id,
        .nullable = true,
    };
    mylite_field_descriptor_set_nullable(out_descriptor, true);
    return MYLITE_OK;
}
