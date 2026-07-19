#include <mylite/mylite.h>

#include "mylite_execution_catalog.h"
#include "mylite_execution_diagnostics.h"
#include "mylite_execution_information_schema_values.h"

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char information_schema_ascii_lower(unsigned char byte);
static bool information_schema_ascii_equals_case_insensitive(const char *left, const char *right);

enum {
    information_schema_decimal_base = 10,
};

#include "mylite_execution_information_schema_compare_format_helpers.inc"

static unsigned char information_schema_ascii_lower(unsigned char byte) {
    if (byte >= (unsigned char)'A' && byte <= (unsigned char)'Z') {
        return (unsigned char)(byte + ((unsigned char)'a' - (unsigned char)'A'));
    }
    return byte;
}

static bool information_schema_ascii_equals_case_insensitive(const char *left, const char *right) {
    size_t index = 0U;

    while (left[index] != '\0' && right[index] != '\0') {
        if (information_schema_ascii_lower((unsigned char)left[index]) !=
            information_schema_ascii_lower((unsigned char)right[index])) {
            return false;
        }
        ++index;
    }
    return left[index] == right[index];
}
