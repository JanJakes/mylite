#define MYLITE_EXECUTION_DIAGNOSTICS_NO_SHORT_NAMES

#include "mylite_execution_diagnostics.h"

#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_dynamic_string.h"
#include "mylite_mysql_error_codes.h"

#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    system_variable_body_offset = 2,
};

static bool diagnostics_text_equals_ascii_case_insensitive(const char *left, const char *right);
static unsigned char diagnostics_ascii_lower(unsigned char value);
static int mylite_execution_diagnostics_copy_system_variable_name_for_error(
    const struct mylite_sql_source_span *span,
    char **out_name
);
static int mylite_execution_diagnostics_copy_system_variable_component_name_for_error(
    const struct mylite_sql_source_span *span,
    size_t *offset,
    char **out_name
);
static int mylite_execution_diagnostics_copy_system_variable_raw_body_for_error(
    const struct mylite_sql_source_span *span,
    char **out_name
);
static int mylite_execution_diagnostics_append_system_variable_error_name_byte(
    char value,
    char **name,
    size_t *length,
    size_t capacity
);

void mylite_execution_diagnostics_set_unsupported_error(
    struct mylite_db *database,
    const char *message
) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_parse,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_alter_table_instant_lock_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_wrong_usage,
        "HY000",
        "Incorrect usage of ALGORITHM=INSTANT and LOCK=NONE/SHARED/EXCLUSIVE"
    );
}

void mylite_execution_diagnostics_set_alter_table_instant_algorithm_error(
    struct mylite_db *database
) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_algorithm_not_supported,
        "0A000",
        "ALGORITHM=INSTANT is not supported for this operation. Try ALGORITHM=COPY/INPLACE."
    );
}

void mylite_execution_diagnostics_set_alter_table_add_foreign_key_instant_error(
    struct mylite_db *database
) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_algorithm_not_supported_reason,
        "0A000",
        "ALGORITHM=INSTANT is not supported. Reason: Adding foreign keys needs "
        "foreign_key_checks=OFF. Try ALGORITHM=COPY/INPLACE."
    );
}

void mylite_execution_diagnostics_set_alter_table_add_foreign_key_inplace_error(
    struct mylite_db *database
) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_algorithm_not_supported_reason,
        "0A000",
        "ALGORITHM=INPLACE is not supported. Reason: Adding foreign keys needs "
        "foreign_key_checks=OFF. Try ALGORITHM=COPY."
    );
}

void mylite_execution_diagnostics_set_alter_table_add_foreign_key_lock_none_error(
    struct mylite_db *database,
    enum mylite_sql_ast_alter_algorithm algorithm
) {
    if (algorithm == MYLITE_SQL_AST_ALTER_ALGORITHM_COPY) {
        mylite_execution_diagnostics_set_alter_table_copy_lock_none_error(database);
        return;
    }

    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_algorithm_not_supported_reason,
        "0A000",
        "LOCK=NONE is not supported. Reason: Adding foreign keys needs "
        "foreign_key_checks=OFF. Try LOCK=SHARED."
    );
}

void mylite_execution_diagnostics_set_alter_table_add_fulltext_instant_error(
    struct mylite_db *database
) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_algorithm_not_supported_reason,
        "0A000",
        "ALGORITHM=INSTANT is not supported. Reason: Fulltext index creation requires a lock. "
        "Try ALGORITHM=COPY/INPLACE."
    );
}

void mylite_execution_diagnostics_set_alter_table_copy_lock_none_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_algorithm_not_supported_reason,
        "0A000",
        "LOCK=NONE is not supported. Reason: COPY algorithm requires a lock. Try LOCK=SHARED."
    );
}

void mylite_execution_diagnostics_set_alter_table_key_maintenance_lock_error(
    struct mylite_db *database
) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_algorithm_not_supported,
        "0A000",
        "LOCK=NONE/SHARED is not supported for this operation. Try LOCK=EXCLUSIVE."
    );
}

void mylite_execution_diagnostics_set_alter_table_add_fulltext_lock_none_error(
    struct mylite_db *database
) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_algorithm_not_supported_reason,
        "0A000",
        "LOCK=NONE is not supported. Reason: Fulltext index creation requires a lock. "
        "Try LOCK=SHARED."
    );
}

void mylite_execution_diagnostics_set_no_tables_used_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_no_tables_used,
        "HY000",
        "No tables used"
    );
}

void mylite_execution_diagnostics_set_in_subquery_limit_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_not_supported_yet,
        "42000",
        "This version of MySQL doesn't yet support 'LIMIT & IN/ALL/ANY/SOME subquery'"
    );
}

void mylite_execution_diagnostics_set_scalar_subquery_column_count_error(
    struct mylite_db *database
) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_operand_should_contain_one_column,
        "21000",
        "Operand should contain 1 column(s)"
    );
}

void mylite_execution_diagnostics_set_scalar_subquery_row_count_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_subquery_returns_more_than_one_row,
        "21000",
        "Subquery returns more than 1 row"
    );
}

void mylite_execution_diagnostics_set_union_column_count_mismatch_error(
    struct mylite_db *database
) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_select_reduced,
        "21000",
        "The used SELECT statements have a different number of columns"
    );
}

void mylite_execution_diagnostics_set_update_table_used_error(
    struct mylite_db *database,
    const char *table_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "You can't specify target table '%s' for update in FROM clause",
        table_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_update_table_used,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_session_variable_only_error(
    struct mylite_db *database,
    const char *variable_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Variable '%s' is a SESSION variable", variable_name);

    if (written < 0) {
        message[0] = '\0';
    }

    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_session_variable_only,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_global_variable_only_error(
    struct mylite_db *database,
    const char *variable_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Variable '%s' is a GLOBAL variable", variable_name);

    if (written < 0) {
        message[0] = '\0';
    }

    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_session_variable_only,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_global_variable_set_global_required_error(
    struct mylite_db *database,
    const char *variable_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Variable '%s' is a GLOBAL variable and should be set with SET GLOBAL",
        variable_name
    );

    if (written < 0) {
        message[0] = '\0';
    }

    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_global_variable_only,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_session_read_only_system_variable_error(
    struct mylite_db *database,
    const char *variable_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "SESSION variable '%s' is read-only. Use SET GLOBAL to assign the value",
        variable_name
    );

    if (written < 0) {
        message[0] = '\0';
    }

    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_session_variable_read_only,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_read_only_system_variable_error(
    struct mylite_db *database,
    const char *variable_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Variable '%s' is a read only variable", variable_name);

    if (written < 0) {
        message[0] = '\0';
    }

    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_session_variable_only,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_unknown_system_variable_error(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
) {
    char *variable_name = NULL;
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    const char *display_name = "unknown";
    int written = 0;

    if (expression != NULL) {
        (void)mylite_execution_diagnostics_copy_system_variable_name_for_error(
            &expression->span,
            &variable_name
        );
    }
    if (variable_name != NULL) {
        display_name = variable_name;
    }

    written = snprintf(message, sizeof(message), "Unknown system variable '%s'", display_name);
    if (written < 0) {
        message[0] = '\0';
    }
    free(variable_name);

    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown_system_variable,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_unknown_system_variable_name_error(
    struct mylite_db *database,
    const char *variable_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    const char *display_name = variable_name == NULL ? "unknown" : variable_name;
    int written = snprintf(message, sizeof(message), "Unknown system variable '%s'", display_name);

    if (written < 0) {
        message[0] = '\0';
    }

    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown_system_variable,
        "HY000",
        message
    );
}

static int mylite_execution_diagnostics_copy_system_variable_name_for_error(
    const struct mylite_sql_source_span *span,
    char **out_name
) {
    char *first = NULL;
    char *second = NULL;
    size_t offset = system_variable_body_offset;
    int rc = MYLITE_OK;

    if (out_name == NULL) {
        return MYLITE_MISUSE;
    }
    *out_name = NULL;
    if (span == NULL || span->text == NULL || span->length <= system_variable_body_offset) {
        return MYLITE_ERROR;
    }

    rc = mylite_execution_diagnostics_copy_system_variable_component_name_for_error(
        span,
        &offset,
        &first
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (offset < span->length && span->text[offset] == '.') {
        ++offset;
        rc = mylite_execution_diagnostics_copy_system_variable_component_name_for_error(
            span,
            &offset,
            &second
        );
        if (rc != MYLITE_OK) {
            free(first);
            return rc;
        }
        if (diagnostics_text_equals_ascii_case_insensitive(first, "session") ||
            diagnostics_text_equals_ascii_case_insensitive(first, "local") ||
            diagnostics_text_equals_ascii_case_insensitive(first, "global")) {
            free(first);
            *out_name = second;
            return MYLITE_OK;
        }
        free(second);
        free(first);
        return mylite_execution_diagnostics_copy_system_variable_raw_body_for_error(span, out_name);
    }

    *out_name = first;
    return MYLITE_OK;
}

static int mylite_execution_diagnostics_copy_system_variable_component_name_for_error(
    const struct mylite_sql_source_span *span,
    size_t *offset,
    char **out_name
) {
    const size_t capacity = span->length + 1U;
    size_t length = 0U;
    char *name = (char *)malloc(capacity);

    if (name == NULL) {
        return MYLITE_NOMEM;
    }
    name[0] = '\0';

    if (*offset < span->length && span->text[*offset] == '`') {
        ++*offset;
        while (*offset < span->length) {
            char value = span->text[*offset];

            if (value == '`') {
                ++*offset;
                if (*offset < span->length && span->text[*offset] == '`') {
                    int byte_rc =
                        mylite_execution_diagnostics_append_system_variable_error_name_byte(
                            '`',
                            &name,
                            &length,
                            capacity
                        );
                    if (byte_rc != MYLITE_OK) {
                        return byte_rc;
                    }
                    ++*offset;
                    continue;
                }
                *out_name = name;
                return MYLITE_OK;
            }

            {
                int byte_rc = mylite_execution_diagnostics_append_system_variable_error_name_byte(
                    value,
                    &name,
                    &length,
                    capacity
                );
                if (byte_rc != MYLITE_OK) {
                    return byte_rc;
                }
            }
            ++*offset;
        }

        free(name);
        return MYLITE_ERROR;
    }

    while (*offset < span->length && span->text[*offset] != '.') {
        int byte_rc = mylite_execution_diagnostics_append_system_variable_error_name_byte(
            span->text[*offset],
            &name,
            &length,
            capacity
        );
        if (byte_rc != MYLITE_OK) {
            return byte_rc;
        }
        ++*offset;
    }

    *out_name = name;
    return MYLITE_OK;
}

static int mylite_execution_diagnostics_copy_system_variable_raw_body_for_error(
    const struct mylite_sql_source_span *span,
    char **out_name
) {
    const size_t length = span->length - system_variable_body_offset;
    char *name = (char *)malloc(length + 1U);

    if (name == NULL) {
        return MYLITE_NOMEM;
    }

    memcpy(name, span->text + system_variable_body_offset, length);
    name[length] = '\0';
    *out_name = name;
    return MYLITE_OK;
}

static int mylite_execution_diagnostics_append_system_variable_error_name_byte(
    char value,
    char **name,
    size_t *length,
    size_t capacity
) {
    if (*length + 1U >= capacity) {
        free(*name);
        *name = NULL;
        return MYLITE_NOMEM;
    }

    (*name)[*length] = value;
    ++*length;
    (*name)[*length] = '\0';
    return MYLITE_OK;
}

void mylite_execution_diagnostics_set_native_function_parameter_count_error(
    struct mylite_db *database,
    const char *function_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Incorrect parameter count in the call to native function '%s'",
        function_name
    );

    if (written < 0) {
        message[0] = '\0';
    }

    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_incorrect_parameter_count,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_invalid_json_function_text_error(
    struct mylite_db *database,
    size_t position
) {
    struct mylite_json_normalize_result result = {
        .status = MYLITE_JSON_NORMALIZE_INVALID,
        .position = position,
    };
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Invalid JSON text in argument 1 to JSON function: \"%s\" at position %zu.",
        mylite_json_invalid_text_error_message(&result),
        result.position
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_invalid_json_text_in_function,
        "22032",
        message
    );
}

int mylite_execution_diagnostics_append_invalid_json_value_warning(
    struct mylite_db *database,
    const struct mylite_json_normalize_result *result
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    size_t position = result == NULL ? 0U : result->position;
    int written = snprintf(
        message,
        sizeof(message),
        "Invalid JSON text in argument 1 to function json_value: \"%s\" at position %zu.",
        mylite_json_invalid_text_error_message(result),
        position
    );

    if (written < 0) {
        message[0] = '\0';
    }
    return mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_error_invalid_json_text_in_function,
        "22032",
        message
    );
}

void mylite_execution_diagnostics_set_invalid_json_path_error(
    struct mylite_db *database,
    size_t position
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Invalid JSON path expression. The error is around character position %zu.",
        position
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_invalid_json_path,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_json_path_not_allowed_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_json_path_not_allowed,
        "42000",
        "The path expression '$' is not allowed in this context."
    );
}

void mylite_execution_diagnostics_set_invalid_json_data_type_error(
    struct mylite_db *database,
    const char *function_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Invalid data type for JSON data in argument 1 to function %s; a JSON string or JSON type "
        "is required.",
        function_name == NULL ? "JSON" : function_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_invalid_json_data,
        "22032",
        message
    );
}

void mylite_execution_diagnostics_set_invalid_json_one_or_all_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_invalid_json_one_or_all,
        "42000",
        "The oneOrAll argument to json_contains_path may take these values: 'one' or 'all'."
    );
}

void mylite_execution_diagnostics_set_json_unquote_incorrect_type_error(
    struct mylite_db *database
) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_json_unquote_incorrect_type,
        "HY000",
        "Incorrect type for argument 1 in function JSON_UNQUOTE."
    );
}

void mylite_execution_diagnostics_set_json_quote_incorrect_type_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_json_quote_incorrect_type,
        "HY000",
        "Incorrect type for argument 1 in function json_quote."
    );
}

void mylite_execution_diagnostics_set_json_binary_charset_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_invalid_json_charset,
        "22032",
        "Cannot create a JSON value from a string with CHARACTER SET 'binary'."
    );
}

void mylite_execution_diagnostics_set_json_null_member_name_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_json_null_member_name,
        "22032",
        "JSON documents may not contain NULL member names."
    );
}

void mylite_execution_diagnostics_set_no_database_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_no_database_selected,
        "3D000",
        "No database selected"
    );
}

void mylite_execution_diagnostics_set_database_access_denied_error(
    struct mylite_db *database,
    const char *schema_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Access denied for user 'root'@'%%' to database '%s'",
        schema_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_database_access_denied,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_system_schema_access_error(
    struct mylite_db *database,
    const char *schema_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Access to system schema '%s' is rejected.",
        schema_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_system_schema_access,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_mysql_data_dictionary_table_access_error(
    struct mylite_db *database,
    const char *table_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    const char *table_kind = strcmp(table_name, "innodb_ddl_log") == 0 ||
                                     strcmp(table_name, "innodb_dynamic_metadata") == 0
                                 ? "system"
                                 : "data dictionary";
    int written = snprintf(
        message,
        sizeof(message),
        "Access to %s table 'mysql.%s' is rejected.",
        table_kind,
        table_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_data_dictionary_access,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_database_exists_error(
    struct mylite_db *database,
    const char *schema_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Can't create database '%s'; database exists",
        schema_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_database_exists,
        "HY000",
        message
    );
}

int mylite_execution_diagnostics_append_database_exists_note(
    struct mylite_db *database,
    const char *schema_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Can't create database '%s'; database exists",
        schema_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    return mylite_diagnostics_append_note(
        mylite_connection_diagnostics(database),
        mysql_error_database_exists,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_cant_drop_database_error(
    struct mylite_db *database,
    const char *schema_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Can't drop database '%s'; database doesn't exist",
        schema_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_cant_drop_database,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_unknown_database_error(
    struct mylite_db *database,
    const char *schema_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Unknown database '%s'", schema_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown_database,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_database_does_not_exist_error(
    struct mylite_db *database,
    const char *schema_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Database '%s' doesn't exist", schema_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_database_does_not_exist,
        "42Y07",
        message
    );
}

void mylite_execution_diagnostics_set_table_exists_error(
    struct mylite_db *database,
    const char *table_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Table '%s' already exists", table_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_table_exists,
        "42S01",
        message
    );
}

int mylite_execution_diagnostics_append_table_exists_note(
    struct mylite_db *database,
    const char *table_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Table '%s' already exists", table_name);

    if (written < 0) {
        message[0] = '\0';
    }
    return mylite_diagnostics_append_note(
        mylite_connection_diagnostics(database),
        mysql_error_table_exists,
        "42S01",
        message
    );
}

void mylite_execution_diagnostics_set_create_table_select_locking_clause_error(
    struct mylite_db *database,
    const char *source_table_name,
    const char *target_table_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Can't update table '%s' while '%s' is being created.",
        source_table_name,
        target_table_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_cannot_update_table_while_creating,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_unknown_table_error(
    struct mylite_db *database,
    const char *schema_name,
    const char *table_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Unknown table '%s.%s'", schema_name, table_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown_table,
        "42S02",
        message
    );
}

void mylite_execution_diagnostics_set_not_view_error(
    struct mylite_db *database,
    const char *schema_name,
    const char *table_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "'%s.%s' is not VIEW", schema_name, table_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_not_view,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_unknown_table_name_error(
    struct mylite_db *database,
    const char *table_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Unknown table '%s'", table_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown_table,
        "42S02",
        message
    );
}

void mylite_execution_diagnostics_set_unknown_multi_delete_table_error(
    struct mylite_db *database,
    const char *table_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Unknown table '%s' in MULTI DELETE", table_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown_table_in_schema,
        "42S02",
        message
    );
}

void mylite_execution_diagnostics_set_wrong_usage_error(
    struct mylite_db *database,
    const char *left,
    const char *right
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Incorrect usage of %s and %s", left, right);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_wrong_usage,
        "HY000",
        message
    );
}

int mylite_execution_diagnostics_set_unknown_drop_tables_error(
    struct mylite_db *database,
    const struct planned_drop_table *plan
) {
    struct mylite_dynamic_string message;
    char *owned_message = NULL;
    size_t missing_index = 0U;
    int rc = MYLITE_OK;

    mylite_dynamic_string_init(&message);
    rc = mylite_dynamic_string_append(&message, "Unknown table '");
    for (size_t target_index = 0U; rc == MYLITE_OK && target_index < plan->target_count;
         ++target_index) {
        const struct planned_drop_table_target *target = &plan->targets[target_index];

        if (!target->missing) {
            continue;
        }
        if (missing_index != 0U) {
            rc = mylite_dynamic_string_append_char(&message, ',');
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(&message, target->target.schema.name);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(&message, '.');
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(&message, target->target.table_name);
        }
        ++missing_index;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(&message, '\'');
    }
    if (rc == MYLITE_OK) {
        owned_message = mylite_dynamic_string_take(&message);
        if (owned_message == NULL) {
            rc = MYLITE_NOMEM;
        }
    }
    if (rc != MYLITE_OK) {
        mylite_dynamic_string_deinit(&message);
        mylite_execution_diagnostics_set_nomem_error(database);
        return rc;
    }

    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown_table,
        "42S02",
        owned_message
    );
    free(owned_message);

    return MYLITE_ERROR;
}

int mylite_execution_diagnostics_append_unknown_table_note(
    struct mylite_db *database,
    const char *schema_name,
    const char *table_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Unknown table '%s.%s'", schema_name, table_name);

    if (written < 0) {
        message[0] = '\0';
    }
    return mylite_diagnostics_append_note(
        mylite_connection_diagnostics(database),
        mysql_error_unknown_table,
        "42S02",
        message
    );
}

void mylite_execution_diagnostics_set_table_does_not_exist_error(
    struct mylite_db *database,
    const char *schema_name,
    const char *table_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Table '%s.%s' doesn't exist", schema_name, table_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_table_does_not_exist,
        "42S02",
        message
    );
}

void mylite_execution_diagnostics_set_unknown_storage_engine_error(
    struct mylite_db *database,
    const char *engine_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Unknown storage engine '%s'", engine_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown_storage_engine,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_table_storage_engine_option_error(
    struct mylite_db *database,
    const char *table_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Table storage engine for '%s' doesn't have this option",
        table_name == NULL ? "" : table_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_table_storage_engine_option,
        "HY000",
        message
    );
}

int mylite_execution_diagnostics_append_table_storage_engine_option_note(
    struct mylite_db *database,
    const char *table_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Table storage engine for '%s' doesn't have this option",
        table_name == NULL ? "" : table_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    return mylite_diagnostics_append_note(
        mylite_connection_diagnostics(database),
        mysql_error_table_storage_engine_option,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_failed_read_auto_increment_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_failed_read_auto_increment,
        "HY000",
        "Failed to read auto-increment value from storage engine"
    );
}

void mylite_execution_diagnostics_set_unknown_character_set_error(
    struct mylite_db *database,
    const char *charset_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Unknown character set: '%s'", charset_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown_character_set,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_unknown_collation_error(
    struct mylite_db *database,
    const char *collation_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Unknown collation: '%s'", collation_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown_collation,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_savepoint_does_not_exist_error(
    struct mylite_db *database,
    const char *savepoint_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "SAVEPOINT %s does not exist", savepoint_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_savepoint_does_not_exist,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_collation_not_valid_for_charset_error(
    struct mylite_db *database,
    const char *collation_name,
    const char *charset_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "COLLATION '%s' is not valid for CHARACTER SET '%s'",
        collation_name,
        charset_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_collation_not_valid_for_character_set,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_illegal_mix_of_collations_error(
    struct mylite_db *database,
    const char *first_collation,
    const char *second_collation,
    const char *operation
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Illegal mix of collations (%s,EXPLICIT) and (%s,EXPLICIT) for operation '%s'",
        first_collation,
        second_collation,
        operation
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_illegal_mix_of_collations,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_conflicting_character_set_declarations_error(
    struct mylite_db *database,
    const char *first_charset,
    const char *second_charset
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Conflicting declarations: 'CHARACTER SET %s' and 'CHARACTER SET %s'",
        first_charset,
        second_charset
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_conflicting_declarations,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_duplicate_column_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Duplicate column name '%s'", column_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_duplicate_column,
        "42S21",
        message
    );
}

void mylite_execution_diagnostics_set_duplicated_enum_value_error(
    struct mylite_db *database,
    const char *column_name,
    const char *value,
    size_t value_length
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int display_length = value_length > (size_t)INT_MAX ? INT_MAX : (int)value_length;
    int written = snprintf(
        message,
        sizeof(message),
        "Column '%s' has duplicated value '%.*s' in ENUM",
        column_name,
        display_length,
        value == NULL ? "" : value
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_duplicated_value_in_enum,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_duplicated_set_value_error(
    struct mylite_db *database,
    const char *column_name,
    const char *value,
    size_t value_length
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int display_length = value_length > (size_t)INT_MAX ? INT_MAX : (int)value_length;
    int written = snprintf(
        message,
        sizeof(message),
        "Column '%s' has duplicated value '%.*s' in SET",
        column_name,
        display_length,
        value == NULL ? "" : value
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_duplicated_value_in_set,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_illegal_set_value_error(
    struct mylite_db *database,
    const char *value,
    size_t value_length
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int display_length = value_length > (size_t)INT_MAX ? INT_MAX : (int)value_length;
    int written = snprintf(
        message,
        sizeof(message),
        "Illegal set '%.*s' value found during parsing",
        display_length,
        value == NULL ? "" : value
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_illegal_set_value,
        "22007",
        message
    );
}

void mylite_execution_diagnostics_set_multiple_primary_key_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_multiple_primary_key,
        "42000",
        "Multiple primary key defined"
    );
}

void mylite_execution_diagnostics_set_sql_require_primary_key_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_primary_key_required,
        "HY000",
        "Unable to create or change a table without a primary key, when the system variable "
        "'sql_require_primary_key' is set. Add a primary key to the table or unset this variable "
        "to avoid this message. Note that tables without a primary key can cause performance "
        "problems in row-based replication, so please consult your DBA before changing this "
        "setting."
    );
}

void mylite_execution_diagnostics_set_wrong_auto_key_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_wrong_auto_key,
        "42000",
        "Incorrect table definition; there can be only one auto column and it must be defined as a "
        "key"
    );
}

void mylite_execution_diagnostics_set_column_length_too_big_error(
    struct mylite_db *database,
    const char *column_name,
    uint64_t maximum_length
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Column length too big for column '%s' (max = %" PRIu64 "); use BLOB or TEXT instead",
        column_name,
        maximum_length
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_column_length_too_big,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_row_size_too_large_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_row_size_too_large,
        "42000",
        "Row size too large. The maximum row size for the used table type, not counting BLOBs, is "
        "65535. This includes storage overhead, check the manual. You have to change some columns "
        "to TEXT or BLOBs"
    );
}

void mylite_execution_diagnostics_set_incorrect_column_specifier_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Incorrect column specifier for column '%s'",
        column_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_incorrect_column_specifier,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_key_column_missing_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Key column '%s' doesn't exist in table", column_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_key_column_does_not_exist,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_invalid_use_of_null_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_invalid_use_of_null,
        "22004",
        "Invalid use of NULL value"
    );
}

void mylite_execution_diagnostics_set_primary_key_part_null_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_primary_key_part_null,
        "42000",
        "All parts of a PRIMARY KEY must be NOT NULL; if you need NULL in a key, use UNIQUE instead"
    );
}

void mylite_execution_diagnostics_set_duplicate_key_name_error(
    struct mylite_db *database,
    const char *index_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Duplicate key name '%s'", index_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_duplicate_key_name,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_incorrect_index_name_error(
    struct mylite_db *database,
    const char *index_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Incorrect index name '%s'", index_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_incorrect_index_name,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_index_hint_use_force_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_wrong_usage,
        "HY000",
        "Incorrect usage of USE INDEX and FORCE INDEX"
    );
}

void mylite_execution_diagnostics_set_key_does_not_exist_in_table_error(
    struct mylite_db *database,
    const char *index_name,
    const char *table_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Key '%s' doesn't exist in table '%s'",
        index_name,
        table_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_key_does_not_exist,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_primary_key_index_invisible_error(
    struct mylite_db *database
) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_primary_key_index_invisible,
        "HY000",
        "A primary key index cannot be invisible."
    );
}

void mylite_execution_diagnostics_set_storage_engine_cant_index_column_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "The used storage engine can't index column '%s'",
        column_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_storage_engine_cant_index_column,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_fulltext_column_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Column '%s' cannot be part of FULLTEXT index",
        column_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_fulltext_column,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_fulltext_explicit_order_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_wrong_usage,
        "HY000",
        "Incorrect usage of spatial/fulltext/hash index and explicit index order"
    );
}

void mylite_execution_diagnostics_set_temporary_fulltext_index_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_temporary_fulltext_index,
        "HY000",
        "Cannot create FULLTEXT index on temporary InnoDB table"
    );
}

void mylite_execution_diagnostics_set_spatial_index_non_geometric_error(
    struct mylite_db *database
) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_spatial_index_non_geometric,
        "42000",
        "A SPATIAL index may only contain a geometrical type column"
    );
}

void mylite_execution_diagnostics_set_spatial_index_must_be_not_null_error(
    struct mylite_db *database
) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_spatial_must_be_not_null,
        "42000",
        "All parts of a SPATIAL index must be NOT NULL"
    );
}

void mylite_execution_diagnostics_set_spatial_unique_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_spatial_unique,
        "HY000",
        "Spatial indexes can't be primary or unique indexes."
    );
}

void mylite_execution_diagnostics_set_spatial_index_type_not_supported_error(
    struct mylite_db *database,
    const char *index_type
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "The index type %s is not supported for spatial indexes.",
        index_type
    );

    if (written < 0 || (size_t)written >= sizeof(message)) {
        mylite_execution_diagnostics_set_runtime_error(
            database,
            "spatial index type diagnostic is too long"
        );
        return;
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_spatial_index_type_not_supported,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_spatial_too_many_key_parts_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_too_many_key_parts,
        "42000",
        "Too many key parts specified; max 1 parts allowed"
    );
}

void mylite_execution_diagnostics_set_blob_key_without_length_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "BLOB/TEXT column '%s' used in key specification without a key length",
        column_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_blob_key_without_length,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_json_key_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "JSON column '%s' supports indexing only via generated columns on a specified JSON path.",
        column_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_json_used_as_key,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_incorrect_prefix_key_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_incorrect_prefix_key,
        "HY000",
        "Incorrect prefix key; the used key part isn't a string, the used length is longer than "
        "the key part, or the storage engine doesn't support unique prefix keys"
    );
}

void mylite_execution_diagnostics_set_key_part_length_cannot_be_zero_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Key part '%s' length cannot be 0", column_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_key_part_length_cannot_be_zero,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_key_too_long_error(
    struct mylite_db *database,
    uint64_t maximum_key_length_bytes
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Specified key was too long; max key length is %" PRIu64 " bytes",
        maximum_key_length_bytes
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_key_too_long,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_table_comment_too_long_error(
    struct mylite_db *database,
    const char *table_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Comment for table '%s' is too long (max = 2048)",
        table_name == NULL ? "" : table_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_table_comment_too_long,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_column_comment_too_long_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Comment for field '%s' is too long (max = 1024)",
        column_name == NULL ? "" : column_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_column_comment_too_long,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_index_comment_too_long_error(
    struct mylite_db *database,
    const char *index_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Comment for index '%s' is too long (max = 1024)",
        index_name == NULL ? "" : index_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_index_comment_too_long,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_non_ascii_string_key_error(struct mylite_db *database) {
    mylite_execution_diagnostics_set_unsupported_error(
        database,
        "non-ASCII string key values are not supported"
    );
}

void mylite_execution_diagnostics_set_duplicate_key_error(
    struct mylite_db *database,
    const char *table_name,
    const char *index_name,
    const char *value
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Duplicate entry '%s' for key '%s.%s'",
        value,
        table_name,
        index_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_duplicate_key,
        "23000",
        message
    );
}

void mylite_execution_diagnostics_set_no_referenced_row_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_no_referenced_row,
        "23000",
        "Cannot add or update a child row: a foreign key constraint fails"
    );
}

void mylite_execution_diagnostics_set_row_is_referenced_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_row_is_referenced,
        "23000",
        "Cannot delete or update a parent row: a foreign key constraint fails"
    );
}

void mylite_execution_diagnostics_set_cannot_drop_index_needed_foreign_key_error(
    struct mylite_db *database,
    const char *index_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Cannot drop index '%s': needed in a foreign key constraint",
        index_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_cannot_drop_index_needed_fk,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_failed_to_open_referenced_table_error(
    struct mylite_db *database,
    const char *table_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Failed to open the referenced table '%s'", table_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_failed_to_open_referenced_table,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_incorrect_foreign_key_definition_error(
    struct mylite_db *database
) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_incorrect_foreign_key_definition,
        "42000",
        "Key reference and table reference don't match"
    );
}

void mylite_execution_diagnostics_set_foreign_key_column_incompatible_error(
    struct mylite_db *database
) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_foreign_key_column_incompatible,
        "HY000",
        "Referencing column and referenced column in foreign key constraint are incompatible"
    );
}

void mylite_execution_diagnostics_set_foreign_key_missing_unique_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_foreign_key_missing_unique,
        "HY000",
        "Missing unique key for constraint in the referenced table"
    );
}

void mylite_execution_diagnostics_set_duplicate_foreign_key_error(
    struct mylite_db *database,
    const char *foreign_key_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Duplicate foreign key constraint name '%s'",
        foreign_key_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_duplicate_foreign_key,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_drop_column_foreign_key_child_error(
    struct mylite_db *database,
    const char *column_name,
    const char *foreign_key_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Cannot drop column '%s': needed in a foreign key constraint '%s'",
        column_name,
        foreign_key_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_drop_column_fk_child,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_drop_column_foreign_key_parent_error(
    struct mylite_db *database,
    const char *column_name,
    const char *foreign_key_name,
    const char *child_table_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Cannot drop column '%s': needed in a foreign key constraint '%s' of table '%s'",
        column_name,
        foreign_key_name,
        child_table_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_drop_column_fk_parent,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_foreign_key_set_null_not_nullable_error(
    struct mylite_db *database,
    const char *column_name,
    const char *foreign_key_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Column '%s' cannot be NOT NULL: needed in a foreign key constraint '%s' SET NULL",
        column_name,
        foreign_key_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_column_not_null_for_set_null,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_foreign_key_cascade_duplicate_error(
    struct mylite_db *database,
    const char *parent_table_name,
    const char *record_value,
    const char *child_table_name,
    const char *index_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Foreign key constraint for table '%s', record '%s' would lead to a duplicate "
        "entry in table '%s', key '%s'",
        parent_table_name,
        record_value,
        child_table_name,
        index_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_foreign_key_cascade_duplicate,
        "23000",
        message
    );
}

void mylite_execution_diagnostics_set_check_constraint_non_boolean_error(
    struct mylite_db *database
) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_check_constraint_non_boolean,
        "HY000",
        "An expression of a check constraint is not boolean"
    );
}

void mylite_execution_diagnostics_set_check_constraint_column_ref_error(
    struct mylite_db *database,
    const char *constraint_name,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Column check constraint '%s' references other column '%s'",
        constraint_name,
        column_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_check_constraint_column_ref,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_check_constraint_function_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_check_constraint_function,
        "HY000",
        "An expression of a check constraint contains disallowed function"
    );
}

void mylite_execution_diagnostics_set_check_constraint_subquery_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_check_constraint_subquery,
        "HY000",
        "An expression of a check constraint contains a disallowed subquery"
    );
}

void mylite_execution_diagnostics_set_check_constraint_variable_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_check_constraint_variable,
        "HY000",
        "An expression of a check constraint contains disallowed variable"
    );
}

void mylite_execution_diagnostics_set_check_constraint_auto_increment_error(
    struct mylite_db *database
) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_check_constraint_auto_increment,
        "HY000",
        "An expression of a check constraint cannot refer to an AUTO_INCREMENT column"
    );
}

void mylite_execution_diagnostics_set_check_constraint_violated_error(
    struct mylite_db *database,
    const char *constraint_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Check constraint '%s' is violated.", constraint_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_check_constraint_violated,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_check_constraint_not_found_error(
    struct mylite_db *database,
    const char *constraint_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Check constraint '%s' is not found in the table.",
        constraint_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_check_constraint_not_found,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_drop_constraint_ambiguous_error(
    struct mylite_db *database,
    const char *constraint_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Table has multiple constraints with the name '%s'. "
        "Please use constraint specific 'DROP' clause.",
        constraint_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_drop_constraint_ambiguous,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_constraint_does_not_exist_error(
    struct mylite_db *database,
    const char *constraint_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Constraint '%s' does not exist.", constraint_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_constraint_does_not_exist,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_check_constraint_unknown_column_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Check constraint contains column '%s' that does not exist",
        column_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_check_constraint_unknown_column,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_alter_check_constraint_unknown_column_error(
    struct mylite_db *database,
    const char *column_name,
    const char *constraint_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Unknown column '%s' in 'check constraint %s expression'",
        column_name,
        constraint_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown_column,
        "42S22",
        message
    );
}

void mylite_execution_diagnostics_set_duplicate_check_constraint_error(
    struct mylite_db *database,
    const char *check_constraint_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Duplicate check constraint name '%s'",
        check_constraint_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_duplicate_check_constraint,
        "HY000",
        message
    );
}

int mylite_execution_diagnostics_append_check_constraint_warning(
    struct mylite_db *database,
    const char *constraint_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Check constraint '%s' is violated.", constraint_name);
    int rc = MYLITE_OK;

    if (written < 0) {
        message[0] = '\0';
    }
    rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_error_check_constraint_violated,
        "HY000",
        message
    );
    if (rc == MYLITE_NOMEM) {
        mylite_execution_diagnostics_set_nomem_error(database);
    }
    return rc;
}

int mylite_execution_diagnostics_append_duplicate_key_warning(
    struct mylite_db *database,
    const char *table_name,
    const char *index_name,
    const char *value
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Duplicate entry '%s' for key '%s.%s'",
        value,
        table_name,
        index_name
    );
    int rc = MYLITE_OK;

    if (written < 0) {
        message[0] = '\0';
    }
    rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_error_duplicate_key,
        "23000",
        message
    );
    if (rc == MYLITE_NOMEM) {
        mylite_execution_diagnostics_set_nomem_error(database);
    }
    return rc;
}

int mylite_execution_diagnostics_append_no_referenced_row_warning(struct mylite_db *database) {
    int rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_error_no_referenced_row,
        "23000",
        "Cannot add or update a child row: a foreign key constraint fails"
    );

    if (rc == MYLITE_NOMEM) {
        mylite_execution_diagnostics_set_nomem_error(database);
    }
    return rc;
}

void mylite_execution_diagnostics_set_duplicate_table_alias_error(
    struct mylite_db *database,
    const char *table_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Not unique table/alias: '%s'", table_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_not_unique_table_alias,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_cant_drop_field_or_key_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Can't DROP '%s'; check that column/key exists",
        column_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_cant_drop_field_or_key,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_cant_remove_all_fields_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_cant_remove_all_fields,
        "42000",
        "You can't delete all columns with ALTER TABLE; use DROP TABLE instead"
    );
}

void mylite_execution_diagnostics_set_must_have_visible_column_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_must_have_visible_column,
        "HY000",
        "A table must have at least one visible column."
    );
}

void mylite_execution_diagnostics_set_unknown_column_in_table_error(
    struct mylite_db *database,
    const char *column_name,
    const char *table_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Unknown column '%s' in '%s'", column_name, table_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown_column,
        "42S22",
        message
    );
}

void mylite_execution_diagnostics_set_unknown_information_schema_table_error(
    struct mylite_db *database,
    const char *table_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Unknown table '%s' in information_schema", table_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown_table_in_schema,
        "42S02",
        message
    );
}

void mylite_execution_diagnostics_set_unknown_column_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Unknown column '%s' in 'field list'", column_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown_column,
        "42S22",
        message
    );
}

void mylite_execution_diagnostics_set_unknown_where_column_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Unknown column '%s' in 'where clause'", column_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown_column,
        "42S22",
        message
    );
}

void mylite_execution_diagnostics_set_unknown_order_column_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Unknown column '%s' in 'order clause'", column_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown_column,
        "42S22",
        message
    );
}

void mylite_execution_diagnostics_set_unknown_group_column_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Unknown column '%s' in 'group statement'", column_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown_column,
        "42S22",
        message
    );
}

void mylite_execution_diagnostics_set_unknown_having_column_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Unknown column '%s' in 'having clause'", column_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown_column,
        "42S22",
        message
    );
}

void mylite_execution_diagnostics_set_unknown_on_column_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Unknown column '%s' in 'on clause'", column_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown_column,
        "42S22",
        message
    );
}

void mylite_execution_diagnostics_set_ambiguous_column_reference_error(
    struct mylite_db *database,
    enum column_reference_diagnostic_context context,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    const char *context_text = "field list";
    int written = 0;

    if (context == COLUMN_REFERENCE_WHERE) {
        context_text = "where clause";
    } else if (context == COLUMN_REFERENCE_ORDER) {
        context_text = "order clause";
    } else if (context == COLUMN_REFERENCE_ON) {
        context_text = "on clause";
    } else if (context == COLUMN_REFERENCE_GROUP) {
        context_text = "group statement";
    } else if (context == COLUMN_REFERENCE_HAVING) {
        context_text = "having clause";
    }

    written = snprintf(
        message,
        sizeof(message),
        "Column '%s' in %s is ambiguous",
        column_name,
        context_text
    );
    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_column_ambiguous,
        "23000",
        message
    );
}

void mylite_execution_diagnostics_set_ambiguous_order_column_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Column '%s' in order clause is ambiguous", column_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_column_ambiguous,
        "23000",
        message
    );
}

void mylite_execution_diagnostics_set_not_unique_table_alias_error(
    struct mylite_db *database,
    const char *alias
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Not unique table/alias: '%s'", alias);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_not_unique_table_alias,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_only_full_group_by_error(
    struct mylite_db *database,
    size_t expression_index,
    const char *clause_name,
    const struct table_name_resolution *source,
    const struct mylite_catalog_column_descriptor *column
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Expression #%zu of %s is not in GROUP BY clause and contains nonaggregated "
        "column '%s.%s.%s' which is not functionally dependent on columns in GROUP BY clause; "
        "this is incompatible with sql_mode=only_full_group_by",
        expression_index,
        clause_name,
        source->schema.name,
        source->table_name,
        column->name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_not_group_by,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_column_specified_twice_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Column '%s' specified twice", column_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_column_specified_twice,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_column_count_mismatch_error(
    struct mylite_db *database,
    size_t row_number
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Column count doesn't match value count at row %zu",
        row_number
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_column_count_mismatch,
        "21S01",
        message
    );
}

void mylite_execution_diagnostics_set_values_empty_row_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_empty_values_row,
        "HY000",
        "Each row of a VALUES clause must have at least one column, unless when used as source in "
        "an INSERT statement."
    );
}

void mylite_execution_diagnostics_set_values_default_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_values_default,
        "HY000",
        "A VALUES clause cannot use DEFAULT values, unless used as a source in an INSERT statement."
    );
}

void mylite_execution_diagnostics_set_values_integer_out_of_range_error(
    struct mylite_db *database
) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_data_out_of_range,
        "22003",
        "VALUES integer literal is outside the supported signed 64-bit range"
    );
}

void mylite_execution_diagnostics_set_bad_null_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Column '%s' cannot be null", column_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_bad_null,
        "23000",
        message
    );
}

void mylite_execution_diagnostics_set_load_data_file_error(
    struct mylite_db *database,
    const char *file_path,
    int os_error
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    const char *error_text = os_error == 0 ? "Unknown error" : strerror(os_error);
    int written = snprintf(
        message,
        sizeof(message),
        "Can't get stat of '%s' (OS errno %d - %s)",
        file_path == NULL ? "" : file_path,
        os_error,
        error_text
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_cant_get_stat,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_load_data_local_disabled_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_load_data_local_disabled,
        "42000",
        "Loading local data is disabled; this must be enabled on both the client and server sides"
    );
}

void mylite_execution_diagnostics_set_load_data_row_missing_error(
    struct mylite_db *database,
    size_t row_number
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Row %zu doesn't contain data for all columns",
        row_number
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_load_data_row_missing,
        "01000",
        message
    );
}

int mylite_execution_diagnostics_append_load_data_row_missing_warnings(
    struct mylite_db *database,
    struct load_data_missing_warning_request request
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Row %zu doesn't contain data for all columns",
        request.row_number
    );
    int rc = MYLITE_OK;

    if (written < 0) {
        message[0] = '\0';
    }
    for (size_t warning_index = 0U; rc == MYLITE_OK && warning_index < request.warning_count;
         ++warning_index) {
        rc = mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_error_load_data_row_missing,
            "01000",
            message
        );
        if (rc == MYLITE_NOMEM) {
            mylite_execution_diagnostics_set_nomem_error(database);
        }
    }
    return rc;
}

void mylite_execution_diagnostics_set_load_data_row_truncated_error(
    struct mylite_db *database,
    size_t row_number
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Row %zu was truncated; it contained more data than there were input columns",
        row_number
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_load_data_row_truncated,
        "01000",
        message
    );
}

int mylite_execution_diagnostics_append_load_data_row_truncated_warning(
    struct mylite_db *database,
    size_t row_number
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Row %zu was truncated; it contained more data than there were input columns",
        row_number
    );
    int rc = MYLITE_OK;

    if (written < 0) {
        message[0] = '\0';
    }
    rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_error_load_data_row_truncated,
        "01000",
        message
    );
    if (rc == MYLITE_NOMEM) {
        mylite_execution_diagnostics_set_nomem_error(database);
    }
    return rc;
}

void mylite_execution_diagnostics_set_load_data_null_to_not_null_error(
    struct mylite_db *database,
    const char *column_name,
    size_t row_number
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Column set to default value; NULL supplied to NOT NULL column '%s' at row %zu",
        column_name,
        row_number
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_load_data_null_to_not_null,
        "22004",
        message
    );
}

int mylite_execution_diagnostics_append_load_data_null_to_not_null_warning(
    struct mylite_db *database,
    const char *column_name,
    size_t row_number
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Column set to default value; NULL supplied to NOT NULL column '%s' at row %zu",
        column_name,
        row_number
    );
    int rc = MYLITE_OK;

    if (written < 0) {
        message[0] = '\0';
    }
    rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_error_load_data_null_to_not_null,
        "22004",
        message
    );
    if (rc == MYLITE_NOMEM) {
        mylite_execution_diagnostics_set_nomem_error(database);
    }
    return rc;
}

void mylite_execution_diagnostics_set_spatial_bad_null_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Column '%s' cannot be null", column_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_spatial_column_cannot_be_null,
        "23000",
        message
    );
}

void mylite_execution_diagnostics_set_generated_column_value_error(
    struct mylite_db *database,
    const char *column_name,
    const char *table_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "The value specified for generated column '%s' in table '%s' is not allowed.",
        column_name,
        table_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_generated_column_value,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_data_truncated_error(
    struct mylite_db *database,
    const char *column_name,
    size_t row_number
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Data truncated for column '%s' at row %zu",
        column_name,
        row_number
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_data_truncated,
        "01000",
        message
    );
}

void mylite_execution_diagnostics_set_data_too_long_error(
    struct mylite_db *database,
    const char *column_name,
    size_t row_number
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Data too long for column '%s' at row %zu",
        column_name,
        row_number
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_data_too_long,
        "22001",
        message
    );
}

void mylite_execution_diagnostics_set_invalid_default_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Invalid default value for '%s'", column_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_invalid_default,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_json_cant_have_default_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "BLOB, TEXT, GEOMETRY or JSON column '%s' can't have a default value",
        column_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_blob_text_cant_have_default,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_invalid_json_text_error(
    struct mylite_db *database,
    size_t position,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Invalid JSON text: \"Invalid value.\" at position %zu in value for column '%s'.",
        position,
        column_name == NULL ? "" : column_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_invalid_json_text,
        "22032",
        message
    );
}

void mylite_execution_diagnostics_set_no_default_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Field '%s' doesn't have a default value", column_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_field_no_default,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_default_function_expression_error(
    struct mylite_db *database
) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_default_val_generated,
        "HY000",
        "DEFAULT function cannot be used with default value expressions"
    );
}

void mylite_execution_diagnostics_set_out_of_range_error(
    struct mylite_db *database,
    const char *column_name,
    size_t row_number
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Out of range value for column '%s' at row %zu",
        column_name,
        row_number
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_data_out_of_range,
        "22003",
        message
    );
}

void mylite_execution_diagnostics_set_invalid_column_size_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Invalid size for column '%s'.", column_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_invalid_column_size,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_incorrect_date_value_error(
    struct mylite_db *database,
    const char *value_text,
    const char *column_name,
    size_t row_number
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Incorrect date value: '%s' for column '%s' at row %zu",
        value_text,
        column_name,
        row_number
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_incorrect_date_value,
        "22007",
        message
    );
}

void mylite_execution_diagnostics_set_incorrect_time_value_error(
    struct mylite_db *database,
    const char *value_text,
    const char *column_name,
    size_t row_number
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Incorrect time value: '%s' for column '%s' at row %zu",
        value_text,
        column_name,
        row_number
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_incorrect_time_value,
        "22007",
        message
    );
}

void mylite_execution_diagnostics_set_incorrect_datetime_value_error(
    struct mylite_db *database,
    const char *value_text,
    const char *column_name,
    size_t row_number
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Incorrect datetime value: '%s' for column '%s' at row %zu",
        value_text,
        column_name,
        row_number
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_incorrect_date_value,
        "22007",
        message
    );
}

void mylite_execution_diagnostics_set_incorrect_date_literal_error(
    struct mylite_db *database,
    const char *value_text
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Incorrect DATE value: '%s'", value_text);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_incorrect_timestamp_value,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_incorrect_datetime_literal_error(
    struct mylite_db *database,
    const char *value_text
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Incorrect DATETIME value: '%s'", value_text);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_incorrect_timestamp_value,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_incorrect_timestamp_value_error(
    struct mylite_db *database,
    const char *value_text
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Incorrect TIMESTAMP value: '%s'", value_text);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_incorrect_timestamp_value,
        "HY000",
        message
    );
}

int mylite_execution_diagnostics_append_incorrect_datetime_predicate_warning(
    struct mylite_db *database,
    const char *value_text,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Incorrect datetime value: '%s' for column '%s' at row 1",
        value_text,
        column_name
    );
    int rc = MYLITE_OK;

    if (written < 0) {
        message[0] = '\0';
    }
    rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_truncated_incorrect_temporal,
        "22007",
        message
    );
    if (rc == MYLITE_NOMEM) {
        mylite_execution_diagnostics_set_nomem_error(database);
    }
    return rc;
}

int mylite_execution_diagnostics_append_incorrect_date_predicate_warning(
    struct mylite_db *database,
    const char *value_text,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Incorrect date value: '%s' for column '%s' at row 1",
        value_text,
        column_name
    );
    int rc = MYLITE_OK;

    if (written < 0) {
        message[0] = '\0';
    }
    rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_truncated_incorrect_temporal,
        "22007",
        message
    );
    if (rc == MYLITE_NOMEM) {
        mylite_execution_diagnostics_set_nomem_error(database);
    }
    return rc;
}

int mylite_execution_diagnostics_append_incorrect_date_value_note(
    struct mylite_db *database,
    const char *value_text,
    const char *column_name,
    size_t row_number
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Incorrect date value: '%s' for column '%s' at row %zu",
        value_text,
        column_name,
        row_number
    );
    int rc = MYLITE_OK;

    if (written < 0) {
        message[0] = '\0';
    }
    rc = mylite_diagnostics_append_note(
        mylite_connection_diagnostics(database),
        mysql_error_incorrect_date_value,
        "22007",
        message
    );
    if (rc == MYLITE_NOMEM) {
        mylite_execution_diagnostics_set_nomem_error(database);
    }
    return rc;
}

int mylite_execution_diagnostics_append_bad_null_warning(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Column '%s' cannot be null", column_name);
    int rc = MYLITE_OK;

    if (written < 0) {
        message[0] = '\0';
    }
    rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_error_bad_null,
        "23000",
        message
    );
    if (rc == MYLITE_NOMEM) {
        mylite_execution_diagnostics_set_nomem_error(database);
    }
    return rc;
}

int mylite_execution_diagnostics_append_no_default_warning(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Field '%s' doesn't have a default value", column_name);
    int rc = MYLITE_OK;

    if (written < 0) {
        message[0] = '\0';
    }
    rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_error_field_no_default,
        "HY000",
        message
    );
    if (rc == MYLITE_NOMEM) {
        mylite_execution_diagnostics_set_nomem_error(database);
    }
    return rc;
}

int mylite_execution_diagnostics_append_out_of_range_warning(
    struct mylite_db *database,
    const char *column_name,
    size_t row_number
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Out of range value for column '%s' at row %zu",
        column_name,
        row_number
    );
    int rc = MYLITE_OK;

    if (written < 0) {
        message[0] = '\0';
    }
    rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_error_data_out_of_range,
        "22003",
        message
    );
    if (rc == MYLITE_NOMEM) {
        mylite_execution_diagnostics_set_nomem_error(database);
    }
    return rc;
}

void mylite_execution_diagnostics_set_incorrect_integer_value_error(
    struct mylite_db *database,
    const char *value_text,
    const char *column_name,
    size_t row_number
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Incorrect integer value: '%s' for column '%s' at row %zu",
        value_text,
        column_name,
        row_number
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_truncated_wrong_value_for_field,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_incorrect_decimal_value_error(
    struct mylite_db *database,
    const char *value_text,
    const char *column_name,
    size_t row_number
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Incorrect decimal value: '%s' for column '%s' at row %zu",
        value_text,
        column_name,
        row_number
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_truncated_wrong_value_for_field,
        "HY000",
        message
    );
}

int mylite_execution_diagnostics_append_incorrect_integer_value_warning(
    struct mylite_db *database,
    const char *value_text,
    const char *column_name,
    size_t row_number
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Incorrect integer value: '%s' for column '%s' at row %zu",
        value_text,
        column_name,
        row_number
    );
    int rc = MYLITE_OK;

    if (written < 0) {
        message[0] = '\0';
    }
    rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_error_truncated_wrong_value_for_field,
        "HY000",
        message
    );
    if (rc == MYLITE_NOMEM) {
        mylite_execution_diagnostics_set_nomem_error(database);
    }
    return rc;
}

int mylite_execution_diagnostics_append_incorrect_decimal_value_warning(
    struct mylite_db *database,
    const char *value_text,
    const char *column_name,
    size_t row_number
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Incorrect decimal value: '%s' for column '%s' at row %zu",
        value_text,
        column_name,
        row_number
    );
    int rc = MYLITE_OK;

    if (written < 0) {
        message[0] = '\0';
    }
    rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_error_truncated_wrong_value_for_field,
        "HY000",
        message
    );
    if (rc == MYLITE_NOMEM) {
        mylite_execution_diagnostics_set_nomem_error(database);
    }
    return rc;
}

int mylite_execution_diagnostics_append_data_truncated_warning(
    struct mylite_db *database,
    const char *column_name,
    size_t row_number
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Data truncated for column '%s' at row %zu",
        column_name,
        row_number
    );
    int rc = MYLITE_OK;

    if (written < 0) {
        message[0] = '\0';
    }
    rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_error_data_truncated,
        "01000",
        message
    );
    if (rc == MYLITE_NOMEM) {
        mylite_execution_diagnostics_set_nomem_error(database);
    }
    return rc;
}

int mylite_execution_diagnostics_append_data_too_long_warning(
    struct mylite_db *database,
    const char *column_name,
    size_t row_number
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Data too long for column '%s' at row %zu",
        column_name,
        row_number
    );
    int rc = MYLITE_OK;

    if (written < 0) {
        message[0] = '\0';
    }
    rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_error_data_too_long,
        "22001",
        message
    );
    if (rc == MYLITE_NOMEM) {
        mylite_execution_diagnostics_set_nomem_error(database);
    }
    return rc;
}

int mylite_execution_diagnostics_append_data_truncated_note(
    struct mylite_db *database,
    const char *column_name,
    size_t row_number
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Data truncated for column '%s' at row %zu",
        column_name,
        row_number
    );
    int rc = MYLITE_OK;

    if (written < 0) {
        message[0] = '\0';
    }
    rc = mylite_diagnostics_append_note(
        mylite_connection_diagnostics(database),
        mysql_error_data_truncated,
        "01000",
        message
    );
    if (rc == MYLITE_NOMEM) {
        mylite_execution_diagnostics_set_nomem_error(database);
    }
    return rc;
}

int mylite_execution_diagnostics_append_decimal_truncated_note(
    struct mylite_db *database,
    const char *column_name,
    size_t row_number
) {
    return mylite_execution_diagnostics_append_data_truncated_note(
        database,
        column_name,
        row_number
    );
}

void mylite_execution_diagnostics_set_display_width_out_of_range_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Display width out of range for column '%s' (max = 255)",
        column_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_display_width_out_of_range,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_text_display_width_out_of_range_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Display width out of range for column '%s' (max = 4294967295)",
        column_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_display_width_out_of_range,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_bit_display_width_out_of_range_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Display width out of range for column '%s' (max = 64)",
        column_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_display_width_out_of_range,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_invalid_year_display_width_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_invalid_year_display_width,
        "HY000",
        "Invalid display width. Use YEAR instead."
    );
}

void mylite_execution_diagnostics_set_decimal_precision_too_big_error(
    struct mylite_db *database,
    const char *column_name,
    uint64_t precision
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Too-big precision %" PRIu64 " specified for '%s'. Maximum is 65.",
        precision,
        column_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_decimal_precision_too_big,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_decimal_scale_too_big_error(
    struct mylite_db *database,
    const char *column_name,
    uint64_t scale
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Too big scale %" PRIu64 " specified for column '%s'. Maximum is 30.",
        scale,
        column_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_decimal_scale_too_big,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_decimal_scale_greater_than_precision_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "For float(M,D), double(M,D) or decimal(M,D), M must be >= D (column '%s').",
        column_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_decimal_must_be_greater_or_equal_to_d,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_predicate_out_of_range_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Out of range value for column '%s' in WHERE",
        column_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_data_out_of_range,
        "22003",
        message
    );
}

void mylite_execution_diagnostics_set_having_out_of_range_error(
    struct mylite_db *database,
    const char *operand_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Out of range value for '%s' in HAVING", operand_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_data_out_of_range,
        "22003",
        message
    );
}

void mylite_execution_diagnostics_set_limit_out_of_range_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_parse,
        "42000",
        "LIMIT literal is outside the supported range"
    );
}

void mylite_execution_diagnostics_set_regexp_error(
    struct mylite_db *database,
    const char *message
) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_regular_expression,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_regexp_illegal_argument_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_regexp_illegal_argument,
        "HY000",
        "Illegal argument to a regular expression."
    );
}

void mylite_execution_diagnostics_set_regexp_character_range_error(
    struct mylite_db *database,
    const char *message
) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_regular_expression_character_range,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_identifier_too_long_error(
    struct mylite_db *database,
    const char *kind
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "%s identifier is too long", kind);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_identifier_too_long,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_reserved_name_error(
    struct mylite_db *database,
    const char *kind,
    const char *name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int code = mysql_error_incorrect_table_name;
    int written = 0;

    if (strcmp(kind, "database") == 0) {
        code = mysql_error_incorrect_database_name;
    } else if (strcmp(kind, "column") == 0) {
        code = mysql_error_incorrect_column_name;
    }

    written = snprintf(message, sizeof(message), "Incorrect %s name '%s'", kind, name);
    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(mylite_connection_diagnostics(database), code, "42000", message);
}

void mylite_execution_diagnostics_set_nomem_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        MYLITE_NOMEM,
        "HY001",
        "out of memory"
    );
}

void mylite_execution_diagnostics_set_physical_sqlite_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown,
        "HY000",
        "internal SQLite schema operation failed"
    );
}

void mylite_execution_diagnostics_set_physical_sqlite_row_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown,
        "HY000",
        "internal SQLite row operation failed"
    );
}

void mylite_execution_diagnostics_set_runtime_error(
    struct mylite_db *database,
    const char *message
) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_internal_error_if_clear(
    struct mylite_db *database,
    int rc,
    const char *message
) {
    if (database == NULL) {
        return;
    }
    if (mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) != MYLITE_OK) {
        return;
    }
    if (rc == MYLITE_NOMEM) {
        mylite_execution_diagnostics_set_nomem_error(database);
        return;
    }
    if (rc == MYLITE_MISUSE) {
        mylite_diagnostics_set_error(
            mylite_connection_diagnostics(database),
            MYLITE_MISUSE,
            "HY000",
            mylite_diagnostics_misuse_message()
        );
        return;
    }

    mylite_execution_diagnostics_set_runtime_error(database, message);
}

int mylite_execution_diagnostics_status_from_parse_status(enum mylite_sql_parse_status status) {
    switch (status) {
    case MYLITE_SQL_PARSE_OK:
        return MYLITE_OK;
    case MYLITE_SQL_PARSE_NOMEM:
        return MYLITE_NOMEM;
    case MYLITE_SQL_PARSE_MISUSE:
        return MYLITE_MISUSE;
    case MYLITE_SQL_PARSE_LEXER_ERROR:
    case MYLITE_SQL_PARSE_SYNTAX_ERROR:
    case MYLITE_SQL_PARSE_STACK_OVERFLOW:
        return MYLITE_ERROR;
    }

    return MYLITE_ERROR;
}

static bool diagnostics_text_equals_ascii_case_insensitive(const char *left, const char *right) {
    size_t index = 0U;

    if (left == NULL || right == NULL) {
        return false;
    }
    while (left[index] != '\0' && right[index] != '\0') {
        if (diagnostics_ascii_lower((unsigned char)left[index]) !=
            diagnostics_ascii_lower((unsigned char)right[index])) {
            return false;
        }
        ++index;
    }

    if (left[index] != '\0') {
        return false;
    }
    if (right[index] != '\0') {
        return false;
    }

    return true;
}

static unsigned char diagnostics_ascii_lower(unsigned char value) {
    if (value >= 'A' && value <= 'Z') {
        return (unsigned char)(value - 'A' + 'a');
    }
    return value;
}
