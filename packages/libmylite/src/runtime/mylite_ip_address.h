#ifndef MYLITE_RUNTIME_MYLITE_IP_ADDRESS_H
#define MYLITE_RUNTIME_MYLITE_IP_ADDRESS_H

#include "sqlite3.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct mylite_db;

enum {
    mylite_ip_address_text_capacity = 16,
};

int mylite_ip_address_parse_inet_aton(
    const void *input,
    size_t input_size,
    uint32_t *out_value,
    bool *out_valid
);
void mylite_ip_address_format_inet_ntoa(
    uint32_t value,
    char out_text[mylite_ip_address_text_capacity]
);
int mylite_ip_address_parse_inet_ntoa_integer_text(
    const void *input,
    size_t input_size,
    uint32_t *out_value,
    bool *out_out_of_range,
    bool *out_truncated
);
int mylite_ip_address_round_inet_ntoa_real(
    double value,
    uint32_t *out_value,
    bool *out_out_of_range
);
int mylite_ip_address_append_inet_aton_warning(
    struct mylite_db *database,
    const void *input,
    size_t input_size
);
int mylite_ip_address_append_inet_ntoa_range_warning(
    struct mylite_db *database,
    const void *input,
    size_t input_size
);
int mylite_ip_address_append_inet_ntoa_truncated_integer_warning(
    struct mylite_db *database,
    const void *input,
    size_t input_size
);
int mylite_ip_address_append_inet_ntoa_binary_warning(
    struct mylite_db *database,
    const void *input,
    size_t input_size
);
int mylite_sqlite_register_ip_address_functions(sqlite3 *sqlite);

#endif
