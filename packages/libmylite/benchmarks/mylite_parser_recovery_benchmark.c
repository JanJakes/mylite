#include "sql/mylite_parser.h"

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
    default_iterations = 1000,
    decimal_radix = 10,
    nanoseconds_per_second = 1000000000ULL,
};

struct parser_recovery_shape {
    const char *name;
    const char *prefix;
    const char *suffix;
};

static int benchmark_shape(
    const struct parser_recovery_shape *shape,
    size_t depth,
    size_t iterations
);
static char *make_nested_query(
    const struct parser_recovery_shape *shape,
    size_t depth,
    size_t *out_length
);
static int parse_size_argument(const char *text, size_t *out_value);
static uint64_t monotonic_now_ns(void);

int main(int argc, char **argv) {
    static const size_t depths[] = {8U, 16U, 32U, 64U, 128U, 192U};
    static const struct parser_recovery_shape shapes[] = {
        {.name = "expression", .prefix = "SELECT ", .suffix = "1 +;"},
        {
            .name = "table_reference",
            .prefix = "SELECT * FROM ",
            .suffix = "t WHERE +;",
        },
    };
    size_t iterations = default_iterations;
    size_t selected_depth = 0U;

    if (argc > 3) {
        fprintf(stderr, "usage: %s [iterations [depth]]\n", argv[0]);
        return 1;
    }
    if (argc >= 2 && parse_size_argument(argv[1], &iterations) != 0) {
        fprintf(stderr, "invalid iteration count: %s\n", argv[1]);
        return 1;
    }
    if (argc == 3 && parse_size_argument(argv[2], &selected_depth) != 0) {
        fprintf(stderr, "invalid depth: %s\n", argv[2]);
        return 1;
    }

    puts("shape,depth,bytes,iterations,ns_per_parse,status,retry_tokenizations,retry_callbacks");
    if (selected_depth != 0U) {
        for (size_t index = 0U; index < sizeof(shapes) / sizeof(shapes[0]); ++index) {
            if (benchmark_shape(&shapes[index], selected_depth, iterations) != 0) {
                return 1;
            }
        }
        return 0;
    }
    for (size_t depth_index = 0U; depth_index < sizeof(depths) / sizeof(depths[0]); ++depth_index) {
        for (size_t shape_index = 0U; shape_index < sizeof(shapes) / sizeof(shapes[0]);
             ++shape_index) {
            if (benchmark_shape(&shapes[shape_index], depths[depth_index], iterations) != 0) {
                return 1;
            }
        }
    }
    return 0;
}

static int benchmark_shape(
    const struct parser_recovery_shape *shape,
    size_t depth,
    size_t iterations
) {
    struct mylite_sql_parse_result result = {0};
    size_t length = 0U;
    char *sql = make_nested_query(shape, depth, &length);
    enum mylite_sql_parse_status expected_status = MYLITE_SQL_PARSE_OK;
    size_t retry_callback_count = 0U;
    uint64_t started = 0U;
    uint64_t elapsed = 0U;

    if (shape == NULL || sql == NULL) {
        fprintf(stderr, "failed to allocate recovery query at depth %zu\n", depth);
        return 1;
    }
    expected_status =
        mylite_sql_parse((struct mylite_sql_parse_config){.input = sql, .length = length}, &result);
    if (result.retry_tokenization_count != 1U) {
        fprintf(stderr, "%s depth %zu did not enter syntax recovery\n", shape->name, depth);
        mylite_sql_parse_result_deinit(&result);
        free(sql);
        return 1;
    }
    retry_callback_count = result.retry_callback_count;
    mylite_sql_parse_result_deinit(&result);

    started = monotonic_now_ns();
    for (size_t iteration = 0U; iteration < iterations; ++iteration) {
        enum mylite_sql_parse_status status = mylite_sql_parse(
            (struct mylite_sql_parse_config){.input = sql, .length = length},
            &result
        );

        if (status != expected_status || result.retry_tokenization_count != 1U ||
            result.retry_callback_count != retry_callback_count) {
            fprintf(stderr, "%s depth %zu produced inconsistent recovery\n", shape->name, depth);
            mylite_sql_parse_result_deinit(&result);
            free(sql);
            return 1;
        }
        mylite_sql_parse_result_deinit(&result);
    }
    elapsed = monotonic_now_ns() - started;
    printf(
        "%s,%zu,%zu,%zu,%.3f,%s,%zu,%zu\n",
        shape->name,
        depth,
        length,
        iterations,
        (double)elapsed / (double)iterations,
        mylite_sql_parse_status_name(expected_status),
        (size_t)1U,
        retry_callback_count
    );
    free(sql);
    return 0;
}

static char *make_nested_query(
    const struct parser_recovery_shape *shape,
    size_t depth,
    size_t *out_length
) {
    size_t prefix_length = 0U;
    size_t suffix_length = 0U;
    size_t length = 0U;
    char *sql = NULL;
    char *cursor = NULL;

    if (shape == NULL || shape->prefix == NULL || shape->suffix == NULL || out_length == NULL) {
        return NULL;
    }
    prefix_length = strlen(shape->prefix);
    suffix_length = strlen(shape->suffix);
    if (suffix_length == 0U || depth > (SIZE_MAX - prefix_length - suffix_length) / 2U) {
        return NULL;
    }
    length = prefix_length + depth * 2U + suffix_length;
    sql = (char *)malloc(length + 1U);
    if (sql == NULL) {
        return NULL;
    }
    cursor = sql;
    for (size_t index = 0U; index < prefix_length; ++index) {
        *cursor++ = shape->prefix[index];
    }
    memset(cursor, '(', depth);
    cursor += depth;
    *cursor++ = shape->suffix[0];
    memset(cursor, ')', depth);
    cursor += depth;
    for (size_t index = 1U; index < suffix_length; ++index) {
        *cursor++ = shape->suffix[index];
    }
    sql[length] = '\0';
    *out_length = length;
    return sql;
}

static int parse_size_argument(const char *text, size_t *out_value) {
    char *end = NULL;
    unsigned long long parsed = 0ULL;

    if (text == NULL || out_value == NULL || text[0] == '\0') {
        return 1;
    }
    parsed = strtoull(text, &end, decimal_radix);
    if (end == NULL || *end != '\0' || parsed == 0ULL || parsed > SIZE_MAX) {
        return 1;
    }
    *out_value = (size_t)parsed;
    return 0;
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
