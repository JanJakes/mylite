#ifndef MYLITE_BENCHMARK_RUNTIME_STRESS_H
#define MYLITE_BENCHMARK_RUNTIME_STRESS_H

#include <stddef.h>
#include <stdint.h>

struct mylite_benchmark_runtime_stress_measurement {
    uint64_t elapsed_ns;
    size_t operations;
    size_t bytes;
    size_t ok_count;
    size_t error_count;
    size_t peak_retained_bytes;
};

size_t mylite_benchmark_runtime_stress_scenario_count(void);
const char *mylite_benchmark_runtime_stress_scenario_name(size_t index);
int mylite_benchmark_run_runtime_stress_scenario(
    const char *name,
    size_t iterations,
    size_t warmup_iterations,
    struct mylite_benchmark_runtime_stress_measurement *out_measurement
);

#endif
