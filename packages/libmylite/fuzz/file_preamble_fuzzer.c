#include "storage/mylite_file_format.h"

#include "mylite_fuzzer.h"

#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    unsigned char raw_preamble[MYLITE_FILE_PREAMBLE_SIZE] = {0};
    unsigned char near_valid_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    size_t copy_size = size < MYLITE_FILE_PREAMBLE_SIZE ? size : MYLITE_FILE_PREAMBLE_SIZE;

    if (copy_size > 0U) {
        memcpy(raw_preamble, data, copy_size);
    }
    (void)mylite_file_preamble_validate(raw_preamble);
    (void)mylite_file_preamble_lifecycle_state(raw_preamble);
    (void)mylite_file_preamble_format_version(raw_preamble);

    mylite_file_preamble_init(near_valid_preamble);
    if (copy_size > 0U) {
        memcpy(near_valid_preamble, data, copy_size);
    }
    (void)mylite_file_preamble_validate(near_valid_preamble);
    (void)mylite_file_preamble_lifecycle_state(near_valid_preamble);
    (void)mylite_file_preamble_format_version(near_valid_preamble);
    return 0;
}
