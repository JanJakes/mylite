#include "mylite_table_ddl_check_validate.h"

#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "mylite_span.h"
#include "sqlite3.h"

#include <mylite/mylite.h>

#include <stdlib.h>
#include <string.h>

static int validate_check_expression_node(
    mylite_db *database,
    const struct mylite_table_ddl_check_validation_input *input,
    const struct mylite_sql_ast_node *node
);

static int validate_check_expression_children(
    mylite_db *database,
    const struct mylite_table_ddl_check_validation_input *input,
    const struct mylite_sql_ast_node *node
);

static int validate_check_expression_function_call(
    mylite_db *database,
    const struct mylite_table_ddl_check_validation_input *input,
    const struct mylite_sql_ast_node *node
);

static bool check_expression_function_is_allowed(const char *name);

static int validate_check_expression_column_reference(
    mylite_db *database,
    const struct mylite_table_ddl_check_validation_input *input,
    const struct mylite_sql_ast_node *node
);

static bool check_identifier_contains_variable(const struct mylite_sql_ast_node *node);

static int copy_check_identifier_last_part(const struct mylite_sql_ast_node *node, char **out_name);

static const struct mylite_table_ddl_check_column *find_check_column(
    const struct mylite_table_ddl_check_validation_input *input,
    const char *name
);

static int set_check_disallowed_function_error(
    mylite_db *database,
    const struct mylite_table_ddl_check_validation_input *input,
    const char *function_name
);

static int set_check_disallowed_function_without_name_error(
    mylite_db *database,
    const struct mylite_table_ddl_check_validation_input *input
);

static int set_check_variable_error(
    mylite_db *database,
    const struct mylite_table_ddl_check_validation_input *input
);

static int set_check_auto_increment_error(
    mylite_db *database,
    const struct mylite_table_ddl_check_validation_input *input
);

static int set_check_unknown_column_error(
    mylite_db *database,
    const struct mylite_table_ddl_check_validation_input *input,
    const char *column_name
);

static int set_check_validation_error(mylite_db *database, unsigned int code, char *message);

static char *copy_lower_ascii(const char *text);

int mylite_table_ddl_validate_check_expression(
    mylite_db *database,
    const struct mylite_table_ddl_check_validation_input *input
) {
    if (database == NULL || input == NULL || input->expression == NULL) {
        return MYLITE_MISUSE;
    }
    return validate_check_expression_node(database, input, input->expression);
}

// NOLINTNEXTLINE(misc-no-recursion)
static int validate_check_expression_node(
    mylite_db *database,
    const struct mylite_table_ddl_check_validation_input *input,
    const struct mylite_sql_ast_node *node
) {
    if (node == NULL) {
        return MYLITE_OK;
    }

    switch (node->kind) {
    case MYLITE_SQL_AST_IDENTIFIER:
    case MYLITE_SQL_AST_QUALIFIED_IDENTIFIER:
        return validate_check_expression_column_reference(database, input, node);
    case MYLITE_SQL_AST_FUNCTION_CALL:
        return validate_check_expression_function_call(database, input, node);
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP:
        return set_check_disallowed_function_error(database, input, "now");
    case MYLITE_SQL_AST_SUBQUERY_EXPRESSION:
    case MYLITE_SQL_AST_EXISTS_EXPRESSION:
    case MYLITE_SQL_AST_QUANTIFIED_COMPARISON:
    case MYLITE_SQL_AST_SELECT_STATEMENT:
    case MYLITE_SQL_AST_QUERY_EXPRESSION:
    case MYLITE_SQL_AST_UNION_EXPRESSION:
    case MYLITE_SQL_AST_QUERY_PRIMARY:
    case MYLITE_SQL_AST_VALUES_STATEMENT:
    case MYLITE_SQL_AST_AGGREGATE_CALL:
    case MYLITE_SQL_AST_WINDOW_FUNCTION_CALL:
        return set_check_disallowed_function_without_name_error(database, input);
    case MYLITE_SQL_AST_CAST_EXPRESSION:
        return validate_check_expression_node(database, input, mylite_ast_child_at(node, 0U));
    default:
        return validate_check_expression_children(database, input, node);
    }
}

// NOLINTNEXTLINE(misc-no-recursion)
static int validate_check_expression_children(
    mylite_db *database,
    const struct mylite_table_ddl_check_validation_input *input,
    const struct mylite_sql_ast_node *node
) {
    for (const struct mylite_sql_ast_node *child = node == NULL ? NULL : node->first_child;
         child != NULL;
         child = child->next_sibling) {
        int status = validate_check_expression_node(database, input, child);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int validate_check_expression_function_call(
    mylite_db *database,
    const struct mylite_table_ddl_check_validation_input *input,
    const struct mylite_sql_ast_node *node
) {
    char *name = NULL;
    char *lower_name = NULL;
    int status = copy_check_identifier_last_part(mylite_ast_child_at(node, 0U), &name);

    if (status == MYLITE_UNSUPPORTED) {
        return set_check_disallowed_function_without_name_error(database, input);
    }
    if (status != MYLITE_OK) {
        return status;
    }
    lower_name = copy_lower_ascii(name);
    free(name);
    if (lower_name == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    if (!check_expression_function_is_allowed(lower_name)) {
        status = set_check_disallowed_function_error(database, input, lower_name);
        free(lower_name);
        return status;
    }
    free(lower_name);
    return validate_check_expression_children(database, input, mylite_ast_child_at(node, 1U));
}

static bool check_expression_function_is_allowed(const char *name) {
    static const char *const allowed_functions[] = {
        "abs",
        "coalesce",
        "ifnull",
        "length",
        "lower",
        "nullif",
        "round",
        "upper",
    };

    for (size_t index = 0U; index < sizeof(allowed_functions) / sizeof(allowed_functions[0]);
         ++index) {
        if (mylite_ascii_case_equal(name, allowed_functions[index])) {
            return true;
        }
    }
    return false;
}

static int validate_check_expression_column_reference(
    mylite_db *database,
    const struct mylite_table_ddl_check_validation_input *input,
    const struct mylite_sql_ast_node *node
) {
    const struct mylite_table_ddl_check_column *column = NULL;
    char *name = NULL;
    int status = MYLITE_OK;

    if (check_identifier_contains_variable(node)) {
        return set_check_variable_error(database, input);
    }
    status = copy_check_identifier_last_part(node, &name);
    if (status != MYLITE_OK) {
        return status;
    }
    column = find_check_column(input, name);
    if (column == NULL) {
        status = set_check_unknown_column_error(database, input, name);
        free(name);
        return status;
    }
    if (column->auto_increment) {
        status = set_check_auto_increment_error(database, input);
        free(name);
        return status;
    }
    free(name);
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static bool check_identifier_contains_variable(const struct mylite_sql_ast_node *node) {
    if (node == NULL) {
        return false;
    }
    if (node->kind == MYLITE_SQL_AST_IDENTIFIER) {
        if (node->span.text == NULL || node->span.length == 0U) {
            return false;
        }
        if (node->span.text[0] == '@') {
            return true;
        }
        return false;
    }
    if (node->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        if (check_identifier_contains_variable(mylite_ast_child_at(node, 0U))) {
            return true;
        }
        return check_identifier_contains_variable(mylite_ast_child_at(node, 1U));
    }
    return false;
}

static int copy_check_identifier_last_part(
    const struct mylite_sql_ast_node *node,
    char **out_name
) {
    const struct mylite_sql_ast_node *current = node;

    *out_name = NULL;
    while (current != NULL && current->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        current = mylite_ast_child_at(current, 1U);
    }
    if (current == NULL || current->kind != MYLITE_SQL_AST_IDENTIFIER) {
        return MYLITE_UNSUPPORTED;
    }

    *out_name = mylite_copy_identifier_span(current);
    return *out_name == NULL ? MYLITE_NOMEM : MYLITE_OK;
}

static const struct mylite_table_ddl_check_column *find_check_column(
    const struct mylite_table_ddl_check_validation_input *input,
    const char *name
) {
    for (size_t index = 0U; index < input->column_count; ++index) {
        if (mylite_ascii_case_equal(input->columns[index].name, name)) {
            return &input->columns[index];
        }
    }
    return NULL;
}

static int set_check_disallowed_function_error(
    mylite_db *database,
    const struct mylite_table_ddl_check_validation_input *input,
    const char *function_name
) {
    char *message = sqlite3_mprintf(
        "An expression of a check constraint '%q' contains disallowed function: %q.",
        input->constraint_name,
        function_name
    );

    return set_check_validation_error(
        database,
        MYLITE_MYSQL_ER_CHECK_CONSTRAINT_FUNCTION_IS_NOT_ALLOWED,
        message
    );
}

static int set_check_disallowed_function_without_name_error(
    mylite_db *database,
    const struct mylite_table_ddl_check_validation_input *input
) {
    char *message = sqlite3_mprintf(
        "An expression of a check constraint '%q' contains disallowed function.",
        input->constraint_name
    );

    return set_check_validation_error(
        database,
        MYLITE_MYSQL_ER_CHECK_CONSTRAINT_FUNCTION_IS_NOT_ALLOWED_NO_NAME,
        message
    );
}

static int set_check_variable_error(
    mylite_db *database,
    const struct mylite_table_ddl_check_validation_input *input
) {
    char *message = sqlite3_mprintf(
        "An expression of a check constraint '%q' cannot refer to a user or system variable.",
        input->constraint_name
    );

    return set_check_validation_error(
        database,
        MYLITE_MYSQL_ER_CHECK_CONSTRAINT_VARIABLES,
        message
    );
}

static int set_check_auto_increment_error(
    mylite_db *database,
    const struct mylite_table_ddl_check_validation_input *input
) {
    char *message = sqlite3_mprintf(
        "Check constraint '%q' cannot refer to an auto-increment column.",
        input->constraint_name
    );

    return set_check_validation_error(
        database,
        MYLITE_MYSQL_ER_CHECK_CONSTRAINT_REFERS_AUTO_INCREMENT_COLUMN,
        message
    );
}

static int set_check_unknown_column_error(
    mylite_db *database,
    const struct mylite_table_ddl_check_validation_input *input,
    const char *column_name
) {
    char *message = NULL;
    unsigned int code = MYLITE_MYSQL_ER_CHECK_CONSTRAINT_REFERS_UNKNOWN_COLUMN;

    if (input->context == MYLITE_TABLE_DDL_CHECK_VALIDATE_ALTER_TABLE) {
        code = MYLITE_MYSQL_ER_BAD_FIELD_ERROR;
        message = sqlite3_mprintf(
            "Unknown column '%q' in 'check constraint %q expression'",
            column_name,
            input->constraint_name
        );
    } else {
        message = sqlite3_mprintf(
            "Check constraint '%q' refers to non-existing column '%q'.",
            input->constraint_name,
            column_name
        );
    }
    return set_check_validation_error(database, code, message);
}

static int set_check_validation_error(mylite_db *database, unsigned int code, char *message) {
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_set_error_message(database, message);
    if (status == MYLITE_OK) {
        status = mylite_diagnostics_append_error(database, code, message);
    }
    sqlite3_free(message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static char *copy_lower_ascii(const char *text) {
    size_t length = text == NULL ? 0U : strlen(text);
    char *copy = malloc(length + 1U);

    if (copy == NULL) {
        return NULL;
    }
    for (size_t index = 0U; index < length; ++index) {
        char character = text[index];

        if (character >= 'A' && character <= 'Z') {
            character = (char)(character - 'A' + 'a');
        }
        copy[index] = character;
    }
    copy[length] = '\0';
    return copy;
}
