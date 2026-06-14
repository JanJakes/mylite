#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_SCALAR_BINARY_INTERNAL_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_SCALAR_BINARY_INTERNAL_H

#include "mylite_execution_scalar.h"

#include "mylite_ast.h"
#include "mylite_connection.h"

#include <mylite/mylite.h>

#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    binary_integer_text_capacity = mylite_execution_scalar_integer_text_capacity,
    binary_base_conversion_binary_base = 2,
    binary_base_conversion_octal_base = 8,
    binary_base_conversion_hexadecimal_base = 16,
    binary_base_conversion_max_base = 36,
    binary_base_conversion_text_capacity = mylite_execution_scalar_base_conversion_text_capacity,
};

int mylite_execution_scalar_binary_format_base_conversion_value(
    struct mylite_db *database,
    uint64_t value,
    unsigned int base,
    char *buffer,
    size_t buffer_size
);
int mylite_execution_scalar_binary_evaluate_base_conversion_direct_literal_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_bitwise_value *out_value,
    bool *out_handled
);
int mylite_execution_scalar_binary_argument_bytes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
    struct session_scalar_cell *cell,
    const unsigned char **out_bytes,
    size_t *out_byte_count,
    char **out_owned_bytes,
    bool *out_is_null
);
int mylite_execution_scalar_binary_scalar_argument_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell,
    bool *out_handled
);

#endif
