#include "mylite_execution_scalar_binary_internal.h"

#include "mylite_string_base64.h"

enum {
    base64_function_chain_initial_capacity = 4,
    base64_function_chain_growth_factor = 2,
};

enum base64_function_operation {
    base64_function_operation_to,
    base64_function_operation_from,
};

struct base64_function_chain {
    enum base64_function_operation *operations;
    size_t count;
    size_t capacity;
};

static int base64_function_chain_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    enum base64_function_operation expected_operation,
    struct session_scalar_cell *out_cell
);
static int base64_collect_function_chain(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    enum base64_function_operation expected_operation,
    struct base64_function_chain *chain,
    const struct mylite_sql_ast_node **out_argument
);
static int base64_function_chain_append(
    struct mylite_db *database,
    struct base64_function_chain *chain,
    enum base64_function_operation operation
);
static int base64_apply_function_operation(
    struct mylite_db *database,
    enum base64_function_operation operation,
    const unsigned char *bytes,
    size_t byte_count,
    struct session_scalar_cell *out_cell
);
static const char *base64_function_operation_name(enum base64_function_operation operation);
static void base64_function_chain_deinit(struct base64_function_chain *chain);

int mylite_execution_scalar_to_base64_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return base64_function_chain_value(
        database,
        expression,
        base64_function_operation_to,
        out_cell
    );
}

int mylite_execution_scalar_from_base64_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return base64_function_chain_value(
        database,
        expression,
        base64_function_operation_from,
        out_cell
    );
}

static int base64_function_chain_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    enum base64_function_operation expected_operation,
    struct session_scalar_cell *out_cell
) {
    struct base64_function_chain chain = {0};
    struct session_scalar_cell argument_cell = {0};
    struct session_scalar_cell current_cell = {0};
    const struct mylite_sql_ast_node *argument = NULL;
    const unsigned char *bytes = NULL;
    char *owned_bytes = NULL;
    size_t byte_count = 0U;
    bool is_null = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};

    rc = base64_collect_function_chain(database, expression, expected_operation, &chain, &argument);
    if (rc == MYLITE_OK) {
        rc = mylite_execution_scalar_binary_argument_bytes(
            database,
            argument,
            base64_function_operation_name(chain.operations[chain.count - 1U]),
            &argument_cell,
            &bytes,
            &byte_count,
            &owned_bytes,
            &is_null
        );
        mylite_execution_session_scalar_cell_deinit(&argument_cell);
    }

    for (size_t index = chain.count; rc == MYLITE_OK && !is_null && index > 0U; --index) {
        struct session_scalar_cell next_cell = {0};

        rc = base64_apply_function_operation(
            database,
            chain.operations[index - 1U],
            bytes,
            byte_count,
            &next_cell
        );
        free(owned_bytes);
        owned_bytes = NULL;
        if (rc != MYLITE_OK) {
            mylite_execution_session_scalar_cell_deinit(&next_cell);
            break;
        }

        mylite_execution_session_scalar_cell_deinit(&current_cell);
        current_cell = next_cell;
        if (current_cell.value == NULL) {
            is_null = true;
            bytes = NULL;
            byte_count = 0U;
        } else {
            bytes = (const unsigned char *)current_cell.value;
            byte_count =
                current_cell.has_value_size ? current_cell.value_size : strlen(current_cell.value);
        }
    }

    if (rc == MYLITE_OK && !is_null) {
        *out_cell = current_cell;
        current_cell = (struct session_scalar_cell){0};
    }

    free(owned_bytes);
    mylite_execution_session_scalar_cell_deinit(&current_cell);
    base64_function_chain_deinit(&chain);
    return rc;
}

static int base64_collect_function_chain(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    enum base64_function_operation expected_operation,
    struct base64_function_chain *chain,
    const struct mylite_sql_ast_node **out_argument
) {
    const struct mylite_sql_ast_node *current =
        mylite_execution_unwrap_parenthesized_expression(expression);
    enum base64_function_operation operation = base64_function_operation_to;
    bool first = true;
    int rc = MYLITE_OK;

    if (chain == NULL || out_argument == NULL) {
        return MYLITE_MISUSE;
    }
    *out_argument = NULL;

    while (current != NULL) {
        switch (current->kind) {
        case MYLITE_SQL_AST_TO_BASE64_FUNCTION:
            operation = base64_function_operation_to;
            break;
        case MYLITE_SQL_AST_FROM_BASE64_FUNCTION:
            operation = base64_function_operation_from;
            break;
        default:
            if (first) {
                mylite_execution_set_native_function_parameter_count_error(
                    database,
                    base64_function_operation_name(expected_operation)
                );
                return MYLITE_ERROR;
            }
            *out_argument = current;
            return MYLITE_OK;
        }

        if (first && operation != expected_operation) {
            break;
        }
        first = false;

        if (mylite_sql_ast_node_child_count(current) != 1U) {
            mylite_execution_set_native_function_parameter_count_error(
                database,
                base64_function_operation_name(operation)
            );
            return MYLITE_ERROR;
        }
        rc = base64_function_chain_append(database, chain, operation);
        if (rc != MYLITE_OK) {
            return rc;
        }

        current =
            mylite_execution_unwrap_parenthesized_expression(mylite_execution_child_at(current, 0U)
            );
    }

    mylite_execution_set_native_function_parameter_count_error(
        database,
        base64_function_operation_name(expected_operation)
    );
    return MYLITE_ERROR;
}

static int base64_function_chain_append(
    struct mylite_db *database,
    struct base64_function_chain *chain,
    enum base64_function_operation operation
) {
    enum base64_function_operation *operations = NULL;
    size_t new_capacity = 0U;

    if (chain == NULL) {
        return MYLITE_MISUSE;
    }
    if (chain->count < chain->capacity) {
        chain->operations[chain->count++] = operation;
        return MYLITE_OK;
    }

    if (chain->capacity == 0U) {
        new_capacity = base64_function_chain_initial_capacity;
    } else if (chain->capacity > SIZE_MAX / base64_function_chain_growth_factor) {
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    } else {
        new_capacity = chain->capacity * base64_function_chain_growth_factor;
    }
    if (new_capacity > SIZE_MAX / sizeof(*chain->operations)) {
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    operations = (enum base64_function_operation *)
        realloc(chain->operations, new_capacity * sizeof(*chain->operations));
    if (operations == NULL) {
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    chain->operations = operations;
    chain->capacity = new_capacity;
    chain->operations[chain->count++] = operation;
    return MYLITE_OK;
}

static int base64_apply_function_operation(
    struct mylite_db *database,
    enum base64_function_operation operation,
    const unsigned char *bytes,
    size_t byte_count,
    struct session_scalar_cell *out_cell
) {
    unsigned char *decoded = NULL;
    char *encoded = NULL;
    size_t result_size = 0U;
    bool valid = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL || (bytes == NULL && byte_count != 0U)) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};

    if (operation == base64_function_operation_to) {
        rc = mylite_string_base64_encode(bytes, byte_count, &encoded, &result_size);
        if (rc != MYLITE_OK) {
            if (rc == MYLITE_NOMEM) {
                mylite_execution_set_nomem_error(database);
            }
            return rc;
        }
        out_cell->owned_text = encoded;
    } else {
        rc = mylite_string_base64_decode(bytes, byte_count, &decoded, &result_size, &valid);
        if (rc != MYLITE_OK) {
            if (rc == MYLITE_NOMEM) {
                mylite_execution_set_nomem_error(database);
            }
            return rc;
        }
        if (!valid) {
            free(decoded);
            return MYLITE_OK;
        }
        out_cell->owned_text = (char *)decoded;
    }

    out_cell->value = out_cell->owned_text;
    out_cell->value_size = result_size;
    out_cell->has_value_size = true;
    return MYLITE_OK;
}

static const char *base64_function_operation_name(enum base64_function_operation operation) {
    return operation == base64_function_operation_to ? "TO_BASE64" : "FROM_BASE64";
}

static void base64_function_chain_deinit(struct base64_function_chain *chain) {
    if (chain == NULL) {
        return;
    }

    free(chain->operations);
    *chain = (struct base64_function_chain){0};
}
