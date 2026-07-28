#include "runtime/mylite_spatial.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

enum {
    default_iterations = 1,
    decimal_radix = 10,
    internal_srid_size = 4,
    wkb_polygon_header_size = 9,
    wkb_ring_header_size = 4,
    wkb_point_size = 16,
    wkb_byte_order_offset = internal_srid_size,
    wkb_type_offset = wkb_byte_order_offset + 1,
    wkb_ring_count_offset = wkb_type_offset + 4,
    wkb_point_count_offset = wkb_ring_count_offset + 4,
    wkb_points_offset = wkb_point_count_offset + 4,
    wkb_little_endian = 1,
    wkb_polygon_type = 3,
    spatial_byte_bit_count = 8,
    spatial_byte_mask = 0xff,
    nanoseconds_per_second = 1000000000ULL,
    maximum_candidates_per_segment = 32,
    maximum_candidate_slope_tenths = 25,
    candidate_slope_tenths_scale = 10,
};

static const double full_circle_radians = 6.283185307179586476925286766559;

struct generated_geometry {
    unsigned char *bytes;
    size_t byte_count;
};

struct benchmark_outcome {
    int status;
    uint64_t indexed_segments;
    uint64_t candidate_checks;
};

static struct benchmark_outcome benchmark_vertex_count(size_t vertex_count, size_t iterations);
static struct generated_geometry make_regular_polygon(size_t vertex_count);
static int parse_size_argument(const char *text, size_t *out_value);
static void write_u32_le(unsigned char *destination, uint32_t value);
static void write_double_le(unsigned char *destination, double value);
static uint64_t monotonic_now_ns(void);

int main(int argc, char **argv) {
    static const size_t vertex_counts[] = {8192U, 16384U, 32768U, 65536U};
    size_t iterations = default_iterations;
    size_t selected_vertex_count = 0U;
    uint64_t previous_candidate_checks = 0U;

    if (argc > 3) {
        fprintf(stderr, "usage: %s [iterations [vertices]]\n", argv[0]);
        return 1;
    }
    if (argc >= 2 && parse_size_argument(argv[1], &iterations) != 0) {
        fprintf(stderr, "invalid iteration count: %s\n", argv[1]);
        return 1;
    }
    if (argc == 3 && parse_size_argument(argv[2], &selected_vertex_count) != 0) {
        fprintf(stderr, "invalid vertex count: %s\n", argv[2]);
        return 1;
    }

    puts("vertices,bytes,iterations,ns_per_validation,indexed_segments,candidate_checks,"
         "legacy_segment_pairs,result");
    if (selected_vertex_count != 0U) {
        return benchmark_vertex_count(selected_vertex_count, iterations).status;
    }
    for (size_t index = 0U; index < sizeof(vertex_counts) / sizeof(vertex_counts[0]); ++index) {
        struct benchmark_outcome outcome = benchmark_vertex_count(vertex_counts[index], iterations);

        if (outcome.status != 0) {
            return 1;
        }
        if (outcome.candidate_checks > outcome.indexed_segments * maximum_candidates_per_segment) {
            fprintf(
                stderr,
                "%zu-vertex polygon exceeded 32 candidates per segment\n",
                vertex_counts[index]
            );
            return 1;
        }
        if (index > 0U && outcome.candidate_checks * candidate_slope_tenths_scale >
                              previous_candidate_checks * maximum_candidate_slope_tenths) {
            fprintf(
                stderr,
                "%zu-vertex polygon candidate slope exceeded 2.5x\n",
                vertex_counts[index]
            );
            return 1;
        }
        previous_candidate_checks = outcome.candidate_checks;
    }
    return 0;
}

static struct benchmark_outcome benchmark_vertex_count(size_t vertex_count, size_t iterations) {
    struct generated_geometry geometry = make_regular_polygon(vertex_count);
    const struct mylite_spatial_argument argument = {
        .bytes = geometry.bytes,
        .byte_count = geometry.byte_count,
    };
    struct mylite_spatial_result result = {0};
    struct mylite_spatial_error error = {0};
    struct mylite_spatial_work_statistics statistics = {0};
    const struct mylite_spatial_work_control control = {
        .statistics = &statistics,
    };
    uint64_t started = 0U;
    uint64_t elapsed = 0U;
    uint64_t legacy_pair_count = 0U;

    if (geometry.bytes == NULL) {
        fprintf(stderr, "failed to allocate %zu-vertex polygon\n", vertex_count);
        return (struct benchmark_outcome){.status = 1};
    }

    started = monotonic_now_ns();
    for (size_t iteration = 0U; iteration < iterations; ++iteration) {
        int rc = mylite_spatial_evaluate_with_control(
            MYLITE_SPATIAL_FUNCTION_ST_ISVALID,
            &argument,
            1U,
            &result,
            &error,
            &control
        );

        if (rc != 0 || result.kind != MYLITE_SPATIAL_RESULT_INTEGER || result.integer != 1) {
            fprintf(
                stderr,
                "%zu-vertex polygon validation failed: rc=%d code=%d message=%s\n",
                vertex_count,
                rc,
                error.code,
                error.message
            );
            mylite_spatial_result_deinit(&result);
            free(geometry.bytes);
            return (struct benchmark_outcome){.status = 1};
        }
        mylite_spatial_result_deinit(&result);
    }
    elapsed = monotonic_now_ns() - started;
    legacy_pair_count = ((uint64_t)vertex_count * (uint64_t)(vertex_count - 3U)) / 2U;
    printf(
        "%zu,%zu,%zu,%.3f,%llu,%llu,%llu,1\n",
        vertex_count,
        geometry.byte_count,
        iterations,
        (double)elapsed / (double)iterations,
        (unsigned long long)statistics.indexed_segment_count,
        (unsigned long long)statistics.candidate_check_count,
        (unsigned long long)legacy_pair_count
    );
    free(geometry.bytes);
    return (struct benchmark_outcome){
        .indexed_segments = statistics.indexed_segment_count,
        .candidate_checks = statistics.candidate_check_count,
    };
}

static struct generated_geometry make_regular_polygon(size_t vertex_count) {
    struct generated_geometry geometry = {0};
    size_t point_count = vertex_count + 1U;
    size_t byte_count = 0U;

    if (vertex_count < 3U || vertex_count > UINT32_MAX - 1U ||
        point_count >
            (SIZE_MAX - internal_srid_size - wkb_polygon_header_size - wkb_ring_header_size) /
                wkb_point_size) {
        return geometry;
    }
    byte_count = internal_srid_size + wkb_polygon_header_size + wkb_ring_header_size +
                 (point_count * wkb_point_size);
    geometry.bytes = (unsigned char *)calloc(byte_count, 1U);
    if (geometry.bytes == NULL) {
        return geometry;
    }
    geometry.byte_count = byte_count;
    geometry.bytes[wkb_byte_order_offset] = wkb_little_endian;
    write_u32_le(&geometry.bytes[wkb_type_offset], wkb_polygon_type);
    write_u32_le(&geometry.bytes[wkb_ring_count_offset], 1U);
    write_u32_le(&geometry.bytes[wkb_point_count_offset], (uint32_t)point_count);
    for (size_t index = 0U; index < vertex_count; ++index) {
        double angle = full_circle_radians * (double)index / (double)vertex_count;
        size_t point_offset = wkb_points_offset + (index * wkb_point_size);

        write_double_le(&geometry.bytes[point_offset], cos(angle));
        write_double_le(&geometry.bytes[point_offset + sizeof(double)], sin(angle));
    }
    memcpy(
        &geometry.bytes[wkb_points_offset + (vertex_count * wkb_point_size)],
        &geometry.bytes[wkb_points_offset],
        wkb_point_size
    );
    return geometry;
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

static void write_u32_le(unsigned char *destination, uint32_t value) {
    for (size_t index = 0U; index < sizeof(value); ++index) {
        destination[index] =
            (unsigned char)((value >> (index * spatial_byte_bit_count)) & spatial_byte_mask);
    }
}

static void write_double_le(unsigned char *destination, double value) {
    uint64_t bits = 0U;

    memcpy(&bits, &value, sizeof(bits));
    for (size_t index = 0U; index < sizeof(bits); ++index) {
        destination[index] =
            (unsigned char)((bits >> (index * spatial_byte_bit_count)) & spatial_byte_mask);
    }
}

static uint64_t monotonic_now_ns(void) {
#ifdef _WIN32
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
