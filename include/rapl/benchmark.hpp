#pragma once

#include "defines.hpp"

#include <boost/process.hpp>

#include <boost/process/detail/child_decl.hpp>
#include <chrono>
#include <cstdint>
#include <vector>

#include <glaze/core/common.hpp>

#ifndef RAPL_BENCHMARK_RUNTIME_CLOCK
#define RAPL_BENCHMARK_RUNTIME_CLOCK std::chrono::high_resolution_clock
#endif

#if RAPL_BENCHMARK_COUNTERS
#include <rapl/perf.hpp>
#endif

#if RAPL_BENCHMARK_ENERGY
#include <deque>
#include <mutex>
#include <thread>

#include <rapl/cpu.hpp>
#include <rapl/rapl.hpp>
#include <rapl/utils.hpp>

struct EnergySample {
    typename RAPL_BENCHMARK_RUNTIME_CLOCK::rep duration_ms;
    std::vector<rapl::DoubleSample> energy;

    EnergySample(typename RAPL_BENCHMARK_RUNTIME_CLOCK::rep duration_ms, std::vector<rapl::DoubleSample> energy)
        : duration_ms(duration_ms), energy(std::move(energy)) {}
};

template <>
struct glz::meta<EnergySample> {
    using T = EnergySample;
    [[maybe_unused]] static constexpr auto value = glz::object("duration_ms", &T::duration_ms, "energy", &T::energy);
};
#endif

struct Result {
#if RAPL_BENCHMARK_RUNTIME
    typename RAPL_BENCHMARK_RUNTIME_CLOCK::rep runtime_ms;
#endif
#if RAPL_BENCHMARK_COUNTERS
    std::vector<std::pair<std::string, std::uint64_t>> counters;
#endif
#if RAPL_BENCHMARK_ENERGY
    std::vector<EnergySample> energy_samples;
#endif
};

extern void teardown(int status);

namespace benchmark {

inline Result measure(std::string command) {
    Result result;

    // Setup measurement infrastructure.
#if RAPL_BENCHMARK_COUNTERS
#ifndef RAPL_BENCHMARK_COUNTERS_EVENTS
#error "RAPL_BENCHMARK_COUNTERS_EVENTS must be defined."
#endif
    const auto events = std::vector<std::pair<int, int>>(RAPL_BENCHMARK_COUNTERS_EVENTS);
    perf::Group group(events);
    group.reset();
#endif
#if RAPL_BENCHMARK_ENERGY
    auto previous_energy_sample = std::vector<rapl::U32Sample>(cpu::getNPackages());
    std::mutex energy_lock;
    typename RAPL_BENCHMARK_RUNTIME_CLOCK::time_point last_energy_timestamp;
    std::deque<EnergySample> energy_samples;
#endif

    // Actually start measurements.
#if RAPL_BENCHMARK_ENERGY
    {
        std::lock_guard<std::mutex> guard(energy_lock);
        last_energy_timestamp = RAPL_BENCHMARK_RUNTIME_CLOCK::now();
        for (int package = 0; package < cpu::getNPackages(); ++package) {
            previous_energy_sample.at(package) = rapl::sample(package);
        }
    }

    KillableTimer timer;
    auto subprocess = std::thread([&] {
        for (;;) {
            if (!timer.wait(std::chrono::milliseconds(RAPL_BENCHMARK_ENERGY_GRANULARITY_MS))) {
                break;
            }

            std::lock_guard<std::mutex> guard(energy_lock);
            const auto now = RAPL_BENCHMARK_RUNTIME_CLOCK::now();
            const auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_energy_timestamp);
            last_energy_timestamp = now;
            std::vector<rapl::DoubleSample> per_pkg_samples;
            per_pkg_samples.reserve(cpu::getNPackages());
            for (int package = 0; package < cpu::getNPackages(); ++package) {
                const auto sample = rapl::sample(package);
                per_pkg_samples.emplace_back(rapl::scale(sample - previous_energy_sample.at(package), package));
                previous_energy_sample.at(package) = sample;
            }
            energy_samples.emplace_back(duration_ms.count(), per_pkg_samples);
        }
    });
    ScopeExit _([&] {
        timer.kill();
        subprocess.join();
    });
#endif
#if RAPL_BENCHMARK_COUNTERS
    group.enable();
#endif
#if RAPL_BENCHMARK_RUNTIME
    const auto start = RAPL_BENCHMARK_RUNTIME_CLOCK::now();
#endif

    boost::process::child child(command);

    while (child.running()) {
        // ...
    }

    child.wait();

    // Stop measurements.
#if RAPL_BENCHMARK_RUNTIME
    const auto end = RAPL_BENCHMARK_RUNTIME_CLOCK::now();
#endif
#if RAPL_BENCHMARK_COUNTERS
    group.disable();
#endif
#if RAPL_BENCHMARK_ENERGY
    std::lock_guard<std::mutex> guard(energy_lock);
    const auto now = RAPL_BENCHMARK_RUNTIME_CLOCK::now();
    const auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_energy_timestamp);
    std::vector<rapl::DoubleSample> per_pkg_samples;
    per_pkg_samples.reserve(cpu::getNPackages());
    for (int package = 0; package < cpu::getNPackages(); ++package) {
        const auto sample = rapl::sample(package);
        per_pkg_samples.emplace_back(rapl::scale(sample - previous_energy_sample.at(package), package));
        previous_energy_sample.at(package) = sample;
    }
    energy_samples.emplace_back(duration_ms.count(), per_pkg_samples);
#endif

    teardown(child.exit_code());

    // Populate results.
#if RAPL_BENCHMARK_RUNTIME
    result.runtime_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
#endif
#if RAPL_BENCHMARK_COUNTERS
    result.counters.reserve(events.size());
    const auto counters = group.read();
    for (std::size_t i = 0; i < events.size(); ++i) {
        result.counters.emplace_back(perf::toString(events.at(i)), counters.at(i));
    }
#endif
#if RAPL_BENCHMARK_ENERGY
    result.energy_samples = std::vector<EnergySample>(energy_samples.begin(), energy_samples.end());
#endif

    return result;
}
}; // namespace benchmark
