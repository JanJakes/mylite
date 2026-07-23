#include "mylite_test_support.h"

int mylite_test_escape_sql_string(char *output, size_t output_size, const char *input) {
    size_t read_offset = 0U;
    size_t write_offset = 0U;

    if (output == NULL || output_size == 0U || input == NULL) {
        return 1;
    }

    while (input[read_offset] != '\0') {
        if (input[read_offset] == '\\' || input[read_offset] == '\'') {
            if (write_offset + 1U >= output_size) {
                return 1;
            }
            output[write_offset] = '\\';
            ++write_offset;
        }
        if (write_offset + 1U >= output_size) {
            return 1;
        }
        output[write_offset] = input[read_offset];
        ++write_offset;
        ++read_offset;
    }

    output[write_offset] = '\0';
    return 0;
}
