#include "sql/mylite_parser.h"
#include "sql/mylite_parser_resources.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

enum {
    one_mib = 1024 * 1024,
    nanoseconds_per_second = 1000000000ULL,
    cumulative_allocation_input_multiplier = 512,
};

struct scaling_case {
    const char *shape;
    size_t scale;
    char *sql;
    size_t length;
    size_t expected_tokens;
    bool expected_budget_exhaustion;
};

static int run_token_scaling_cases(void);
static int run_one_mib_scaling_cases(void);
static int run_scaling_case(const struct scaling_case *test_case);
static char *make_flat_for_stored_tokens(size_t token_count, size_t *out_length);
static char *make_flat_for_length(size_t length);
static char *make_shallow_for_length(size_t length);
static char *make_comment_padded_for_length(size_t length);
static int expect_scaling_bounds(
    const struct scaling_case *test_case,
    const struct mylite_sql_parse_result *result
);
static size_t retry_workspace_limit_for_input(size_t input_length);
static uint64_t monotonic_now_ns(void);

int main(void) {
    int failures = 0;

    puts("shape,scale,input_bytes,status,tokens,lexer_passes,callbacks,allocation_count,"
         "allocation_bytes,workspace_peak,budget_exhausted,elapsed_ns");
    failures += run_token_scaling_cases();
    failures += run_one_mib_scaling_cases();
    return failures == 0 ? 0 : 1;
}

static int run_token_scaling_cases(void) {
    static const size_t token_counts[] = {
        64U,
        256U,
        1024U,
        4096U,
        16384U,
        65536U,
    };
    int failures = 0;

    for (size_t index = 0U; index < sizeof(token_counts) / sizeof(token_counts[0]); ++index) {
        struct scaling_case test_case = {
            .shape = "flat_tokens",
            .scale = token_counts[index],
            .expected_tokens = token_counts[index],
        };

        test_case.sql = make_flat_for_stored_tokens(test_case.scale, &test_case.length);
        if (test_case.sql == NULL) {
            fprintf(stderr, "failed to allocate flat %zu-token scaling fixture\n", test_case.scale);
            return 1;
        }
        failures += run_scaling_case(&test_case);
        free(test_case.sql);
    }
    return failures;
}

static int run_one_mib_scaling_cases(void) {
    struct scaling_case cases[] = {
        {
            .shape = "flat_one_mib",
            .scale = one_mib,
            .sql = make_flat_for_length(one_mib),
            .length = one_mib,
            .expected_tokens = mylite_sql_parser_retry_token_limit,
            .expected_budget_exhaustion = true,
        },
        {
            .shape = "shallow_one_mib",
            .scale = one_mib,
            .sql = make_shallow_for_length(one_mib),
            .length = one_mib,
            .expected_tokens = mylite_sql_parser_retry_token_limit,
            .expected_budget_exhaustion = true,
        },
        {
            .shape = "comment_one_mib",
            .scale = one_mib,
            .sql = make_comment_padded_for_length(one_mib),
            .length = one_mib,
            .expected_tokens = 3U,
        },
    };
    int failures = 0;

    for (size_t index = 0U; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        if (cases[index].sql == NULL) {
            fprintf(stderr, "failed to allocate %s scaling fixture\n", cases[index].shape);
            failures = 1;
            continue;
        }
        failures += run_scaling_case(&cases[index]);
        free(cases[index].sql);
    }
    return failures;
}

static int run_scaling_case(const struct scaling_case *test_case) {
    struct mylite_sql_parse_result result = {0};
    uint64_t started = monotonic_now_ns();
    enum mylite_sql_parse_status status = mylite_sql_parse(
        (struct mylite_sql_parse_config){
            .input = test_case->sql,
            .length = test_case->length,
        },
        &result
    );
    uint64_t elapsed = monotonic_now_ns() - started;
    int failures = 0;

    if (status != MYLITE_SQL_PARSE_SYNTAX_ERROR) {
        fprintf(
            stderr,
            "%s: expected syntax error, got %s\n",
            test_case->shape,
            mylite_sql_parse_status_name(status)
        );
        failures = 1;
    }
    failures += expect_scaling_bounds(test_case, &result);
    printf(
        "%s,%zu,%zu,%s,%zu,%zu,%zu,%zu,%zu,%zu,%u,%llu\n",
        test_case->shape,
        test_case->scale,
        test_case->length,
        mylite_sql_parse_status_name(status),
        result.retry_token_count,
        result.lexer_pass_count,
        result.retry_callback_count,
        result.retry_allocation_count,
        result.retry_allocation_bytes,
        result.retry_workspace_peak_bytes,
        result.retry_budget_exhausted ? 1U : 0U,
        (unsigned long long)elapsed
    );
    mylite_sql_parse_result_deinit(&result);
    return failures;
}

static char *make_flat_for_stored_tokens(size_t token_count, size_t *out_length) {
    static const char prefix[] = "SELECT";
    static const char item[] = " 1";
    static const char suffix[] = " +";
    size_t integer_count = 0U;
    size_t length = 0U;
    char *sql = NULL;
    char *cursor = NULL;

    if (out_length == NULL || token_count < 2U) {
        return NULL;
    }
    integer_count = token_count - 2U;
    if (integer_count >
        (SIZE_MAX - (sizeof(prefix) - 1U) - (sizeof(suffix) - 1U)) / (sizeof(item) - 1U)) {
        return NULL;
    }
    length = (sizeof(prefix) - 1U) + integer_count * (sizeof(item) - 1U) + (sizeof(suffix) - 1U);
    sql = (char *)malloc(length + 1U);
    if (sql == NULL) {
        return NULL;
    }
    cursor = sql;
    memcpy(cursor, prefix, sizeof(prefix) - 1U);
    cursor += sizeof(prefix) - 1U;
    for (size_t index = 0U; index < integer_count; ++index) {
        memcpy(cursor, item, sizeof(item) - 1U);
        cursor += sizeof(item) - 1U;
    }
    memcpy(cursor, suffix, sizeof(suffix));
    *out_length = length;
    return sql;
}

static char *make_flat_for_length(size_t length) {
    static const char prefix[] = "SELECT";
    static const char item[] = " 1";
    static const char suffix[] = " +";
    size_t framing = (sizeof(prefix) - 1U) + (sizeof(suffix) - 1U);
    size_t item_length = sizeof(item) - 1U;
    size_t integer_count = 0U;
    size_t actual_length = 0U;
    char *sql = NULL;

    if (length < framing || (length - framing) % item_length != 0U) {
        return NULL;
    }
    integer_count = (length - framing) / item_length;
    sql = make_flat_for_stored_tokens(integer_count + 2U, &actual_length);
    if (sql == NULL || actual_length != length) {
        free(sql);
        return NULL;
    }
    return sql;
}

static char *make_shallow_for_length(size_t length) {
    static const char prefix[] = "SELECT";
    static const char item[] = " (1)";
    static const char suffix[] = " +";
    size_t framing = (sizeof(prefix) - 1U) + (sizeof(suffix) - 1U);
    size_t group_count = 0U;
    char *sql = NULL;
    char *cursor = NULL;

    if (length < framing || (length - framing) % (sizeof(item) - 1U) != 0U) {
        return NULL;
    }
    group_count = (length - framing) / (sizeof(item) - 1U);
    sql = (char *)malloc(length + 1U);
    if (sql == NULL) {
        return NULL;
    }
    cursor = sql;
    memcpy(cursor, prefix, sizeof(prefix) - 1U);
    cursor += sizeof(prefix) - 1U;
    for (size_t index = 0U; index < group_count; ++index) {
        memcpy(cursor, item, sizeof(item) - 1U);
        cursor += sizeof(item) - 1U;
    }
    memcpy(cursor, suffix, sizeof(suffix));
    return sql;
}

static char *make_comment_padded_for_length(size_t length) {
    static const char prefix[] = "SELECT 1 /*";
    static const char suffix[] = "*/ +";
    size_t framing = (sizeof(prefix) - 1U) + (sizeof(suffix) - 1U);
    char *sql = NULL;

    if (length < framing) {
        return NULL;
    }
    sql = (char *)malloc(length + 1U);
    if (sql == NULL) {
        return NULL;
    }
    memcpy(sql, prefix, sizeof(prefix) - 1U);
    memset(sql + sizeof(prefix) - 1U, 'a', length - framing);
    memcpy(sql + length - (sizeof(suffix) - 1U), suffix, sizeof(suffix));
    return sql;
}

static int expect_scaling_bounds(
    const struct scaling_case *test_case,
    const struct mylite_sql_parse_result *result
) {
    size_t workspace_limit = retry_workspace_limit_for_input(test_case->length);
    size_t cumulative_allocation_limit = SIZE_MAX;
    int failures = 0;

    if (test_case->length <= SIZE_MAX / cumulative_allocation_input_multiplier) {
        cumulative_allocation_limit = test_case->length * cumulative_allocation_input_multiplier;
    }
    if (result->retry_token_count != test_case->expected_tokens) {
        fprintf(
            stderr,
            "%s: expected %zu stored tokens, got %zu\n",
            test_case->shape,
            test_case->expected_tokens,
            result->retry_token_count
        );
        failures = 1;
    }
    if (result->lexer_pass_count > (size_t)mylite_sql_parser_lexer_pass_limit) {
        fprintf(stderr, "%s: lexer-pass limit exceeded\n", test_case->shape);
        failures = 1;
    }
    if (result->retry_callback_count > (size_t)mylite_sql_parser_retry_callback_limit) {
        fprintf(stderr, "%s: callback limit exceeded\n", test_case->shape);
        failures = 1;
    }
    if (result->retry_workspace_peak_bytes > workspace_limit) {
        fprintf(
            stderr,
            "%s: workspace peak %zu exceeds %zu\n",
            test_case->shape,
            result->retry_workspace_peak_bytes,
            workspace_limit
        );
        failures = 1;
    }
    if (result->retry_allocation_bytes > cumulative_allocation_limit) {
        fprintf(
            stderr,
            "%s: cumulative allocation %zu exceeds linear ceiling %zu\n",
            test_case->shape,
            result->retry_allocation_bytes,
            cumulative_allocation_limit
        );
        failures = 1;
    }
    if (result->retry_budget_exhausted != test_case->expected_budget_exhaustion) {
        fprintf(
            stderr,
            "%s: expected budget exhaustion %u, got %u\n",
            test_case->shape,
            test_case->expected_budget_exhaustion ? 1U : 0U,
            result->retry_budget_exhausted ? 1U : 0U
        );
        failures = 1;
    }
    if (test_case->expected_budget_exhaustion && result->retry_callback_count != 0U) {
        fprintf(stderr, "%s: callback ran after token-budget exhaustion\n", test_case->shape);
        failures = 1;
    }
    if (!test_case->expected_budget_exhaustion &&
        result->retry_callback_count != (size_t)mylite_sql_parser_retry_callback_limit) {
        fprintf(stderr, "%s: did not exercise the callback boundary\n", test_case->shape);
        failures = 1;
    }
    return failures;
}

static size_t retry_workspace_limit_for_input(size_t input_length) {
    size_t limit = mylite_sql_parser_retry_workspace_limit;

    if (input_length <= SIZE_MAX / (size_t)mylite_sql_parser_retry_workspace_input_multiplier) {
        limit = input_length * (size_t)mylite_sql_parser_retry_workspace_input_multiplier;
        if (limit < (size_t)mylite_sql_parser_retry_workspace_minimum) {
            limit = mylite_sql_parser_retry_workspace_minimum;
        }
        if (limit > (size_t)mylite_sql_parser_retry_workspace_limit) {
            limit = mylite_sql_parser_retry_workspace_limit;
        }
    }
    return limit;
}

static uint64_t monotonic_now_ns(void) {
#if defined(_WIN32)
    LARGE_INTEGER frequency = {0};
    LARGE_INTEGER counter = {0};

    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (uint64_t)((counter.QuadPart * (LONGLONG)nanoseconds_per_second) / frequency.QuadPart);
#elif defined(CLOCK_MONOTONIC)
    struct timespec timestamp = {0};

    clock_gettime(CLOCK_MONOTONIC, &timestamp);
    return ((uint64_t)timestamp.tv_sec * nanoseconds_per_second) + (uint64_t)timestamp.tv_nsec;
#else
    struct timespec timestamp = {0};

    timespec_get(&timestamp, TIME_UTC);
    return ((uint64_t)timestamp.tv_sec * nanoseconds_per_second) + (uint64_t)timestamp.tv_nsec;
#endif
}
