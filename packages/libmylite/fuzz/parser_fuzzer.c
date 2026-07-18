#include "sql/mylite_parser.h"

#include "mylite_fuzzer.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    static const char empty[] = "";
    struct mylite_sql_parse_result result = {0};
    const char *sql = size == 0U ? empty : (const char *)data;

    (void)mylite_sql_parse(
        (struct mylite_sql_parse_config){
            .input = sql,
            .length = size,
            .modes = 0U,
            .allow_parameters = false,
        },
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    (void)mylite_sql_parse(
        (struct mylite_sql_parse_config){
            .input = sql,
            .length = size,
            .modes = 0U,
            .allow_parameters = true,
        },
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    return 0;
}
