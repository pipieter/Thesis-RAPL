#pragma once

#include <linux/perf_event.h>

// clang-format off
#define RAPL_BENCHMARK_COUNTERS_EVENTS {                                                                               \
        {PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES},                                                                \
        {PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS},                                                              \
        {PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_REFERENCES},                                                          \
        {PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_MISSES},                                                              \
        {PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_INSTRUCTIONS},                                                       \
        {PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_MISSES},                                                             \
        {PERF_TYPE_HARDWARE, PERF_COUNT_HW_BUS_CYCLES},                                                                \
        {PERF_TYPE_HARDWARE, PERF_COUNT_HW_REF_CPU_CYCLES},                                                            \
        {PERF_TYPE_SOFTWARE, PERF_COUNT_SW_TASK_CLOCK},                                                                \
        {PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS}                                                                \
}
// clang-format on

#define RAPL_BENCHMARK_ENERGY_GRANULARITY_MS 1000
#define RAPL_BENCHMARK_PROCESS_GRANULARITY_MS 100
