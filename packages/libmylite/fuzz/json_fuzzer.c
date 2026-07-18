#include "runtime/mylite_json.h"

#include "mylite_fuzzer.h"

#include <stdlib.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    static const char empty[] = "";
    struct mylite_json_normalize_result result = {0};
    const char *text = size == 0U ? empty : (const char *)data;
    char *normalized = NULL;
    size_t normalized_length = 0U;
    int64_t depth = 0;
    bool is_valid = false;

    (void)mylite_json_validate(text, size, &is_valid);
    (void)mylite_json_normalize(text, size, &normalized, &normalized_length, &result);
    free(normalized);
    (void)mylite_json_depth(text, size, &depth, &result);
    return 0;
}
