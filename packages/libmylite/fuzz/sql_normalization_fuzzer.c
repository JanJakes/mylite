#include "runtime/mylite_execution_sql_normalization.h"

#include "mylite_fuzzer.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    static const char empty[] = "";
    struct mylite_execution_normalized_sql normalized = {0};
    const char *sql = size == 0U ? empty : (const char *)data;

    (void)mylite_execution_normalize_mysql_compat_sql(NULL, sql, size, &normalized);
    mylite_execution_normalized_sql_deinit(&normalized);
    return 0;
}
