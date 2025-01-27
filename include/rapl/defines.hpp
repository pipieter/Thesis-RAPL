#pragma once

#include <linux/perf_event.h>

#define RAPL_BENCHMARK_RUNTIME 1
#define RAPL_BENCHMARK_RUSAGE 1
#define RAPL_BENCHMARK_RUSAGE_WHO RUSAGE_CHILDREN
#define RAPL_BENCHMARK_COUNTERS 1

// clang-format off
#define RAPL_BENCHMARK_COUNTERS_EVENTS {                                                                               \
        {PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_REFERENCES},                                                          \
        {PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_MISSES},                                                              \
        {PERF_TYPE_SOFTWARE, PERF_COUNT_SW_TASK_CLOCK}                                                                 \
}
// clang-format on

#define RAPL_BENCHMARK_ENERGY 1
#define RAPL_BENCHMARK_ENERGY_GRANULARITY_MS 1000
