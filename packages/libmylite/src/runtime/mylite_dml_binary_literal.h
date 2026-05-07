#ifndef MYLITE_RUNTIME_MYLITE_DML_BINARY_LITERAL_H
#define MYLITE_RUNTIME_MYLITE_DML_BINARY_LITERAL_H

#include "mylite_dml_types.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum mylite_dml_binary_literal_kind {
    MYLITE_DML_BINARY_LITERAL_NONE = 0,
    MYLITE_DML_BINARY_LITERAL_HEX,
    MYLITE_DML_BINARY_LITERAL_BIT,
};

enum mylite_dml_binary_literal_kind mylite_dml_binary_literal_kind_for_ast(
    const struct mylite_sql_ast_node *node
);
enum mylite_dml_binary_literal_kind mylite_dml_binary_literal_kind_for_insert_value(
    enum mylite_insert_value_kind kind
);
int mylite_dml_binary_literal_decode(
    enum mylite_dml_binary_literal_kind kind,
    const char *text,
    size_t text_length,
    char **out_text,
    size_t *out_length
);
bool mylite_dml_binary_literal_uint64(
    enum mylite_dml_binary_literal_kind kind,
    const char *text,
    size_t text_length,
    uint64_t *out_value
);
int mylite_dml_binary_literal_decimal_text(uint64_t value, char **out_text, size_t *out_length);

#endif
