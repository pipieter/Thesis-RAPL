#pragma once

#include <boost/process.hpp>
#include <boost/process/detail/child_decl.hpp>

#include <chrono>
#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

#include <glaze/core/common.hpp>

#include <rapl/cpu.hpp>
#include <rapl/defines.hpp>
#include <rapl/perf.hpp>
#include <rapl/process.hpp>
#include <rapl/rapl.hpp>
#include <rapl/utils.hpp>

#ifndef RAPL_BENCHMARK_RUNTIME_CLOCK
#define RAPL_BENCHMARK_RUNTIME_CLOCK std::chrono::high_resolution_clock
#endif

struct EnergySample {
    typename RAPL_BENCHMARK_RUNTIME_CLOCK::rep duration_ms;
    std::vector<rapl::DoubleSample> energy;

    EnergySample(typename RAPL_BENCHMARK_RUNTIME_CLOCK::rep duration_ms, std::vector<rapl::DoubleSample> energy)
        : duration_ms(duration_ms), energy(std::move(energy)) {}
};

struct ProcessSample {
    typename RAPL_BENCHMARK_RUNTIME_CLOCK::rep duration_ms;
    int private_memory;
    int shared_memory;
    int swapped_memory;
};

struct ProcessData {
    std::vector<ProcessSample> samples;
    int cpu_count{0};
    double avg_cpu{0};
};

template <>
struct glz::meta<EnergySample> {
    using T = EnergySample;
    [[maybe_unused]] static constexpr auto value = glz::object("duration_ms", &T::duration_ms, "energy", &T::energy);
};

struct Result {
    int status;
    typename RAPL_BENCHMARK_RUNTIME_CLOCK::rep runtime_ms;
    std::vector<std::pair<std::string, std::uint64_t>> counters;
    std::vector<EnergySample> energy_samples;
    ProcessData process;
};

namespace benchmark {

inline Result measure(std::string command) {
    Result result;

    // Setup measurement infrastructure.
    // Counters
#ifndef RAPL_BENCHMARK_COUNTERS_EVENTS
#error "RAPL_BENCHMARK_COUNTERS_EVENTS must be defined."
#endif
    const auto events = std::vector<std::pair<int, int>>(RAPL_BENCHMARK_COUNTERS_EVENTS);
    perf::Group group(events);
    group.reset();

    // Energy
    auto previous_energy_sample = std::vector<rapl::U32Sample>(cpu::getNPackages());
    std::mutex energy_lock;
    typename RAPL_BENCHMARK_RUNTIME_CLOCK::time_point last_energy_timestamp;
    std::deque<EnergySample> energy_samples;

    std::mutex process_lock;
    typename RAPL_BENCHMARK_RUNTIME_CLOCK::time_point last_process_timestamp;
    ProcessData process;

    // Actually start measurements.
    {
        std::lock_guard<std::mutex> guard(energy_lock);
        last_energy_timestamp = RAPL_BENCHMARK_RUNTIME_CLOCK::now();
        for (int package = 0; package < cpu::getNPackages(); ++package) {
            previous_energy_sample.at(package) = rapl::sample(package);
        }
    }
    last_process_timestamp = RAPL_BENCHMARK_RUNTIME_CLOCK::now();

    KillableTimer energyTimer;
    auto energySubprocess = std::thread([&] {
        for (;;) {
            if (!energyTimer.wait(std::chrono::milliseconds(RAPL_BENCHMARK_ENERGY_GRANULARITY_MS))) {
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

    group.enable();
    const auto start = RAPL_BENCHMARK_RUNTIME_CLOCK::now();

    boost::process::child child(command);
    pid_t pid = child.id();

    KillableTimer processTimer;
    auto processSubprocess = std::thread([&] {
        for (;;) {
            if (!energyTimer.wait(std::chrono::milliseconds(RAPL_BENCHMARK_PROCESS_GRANULARITY_MS))) {
                break;
            }

            ProcessMemory memory;
            double avg_cpu;

            if (!process::getProcessMemory(pid, &memory))
                break;
            if (!process::getProcessAverageCpuUsage(pid, &avg_cpu))
                break;

            std::lock_guard<std::mutex> guard(process_lock);
            const auto now = RAPL_BENCHMARK_RUNTIME_CLOCK::now();
            const auto duration_ms
                = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_process_timestamp);
            last_process_timestamp = now;

            ProcessSample sample;
            sample.duration_ms = duration_ms.count();
            sample.private_memory = memory.private_memory;
            sample.shared_memory = memory.shared_memory;
            sample.swapped_memory = memory.swapped_memory;

            process.samples.push_back(sample);
            process.cpu_count = process::getCpuCount();
            process.avg_cpu = avg_cpu;
        }
    });

    child.wait();
    int status = child.exit_code();

    ScopeExit _([&] {
        energyTimer.kill();
        processTimer.kill();
        energySubprocess.join();
        processSubprocess.join();
    });

    // Stop measurements.

    const auto end = RAPL_BENCHMARK_RUNTIME_CLOCK::now();

    // End counters
    group.disable();

    // One final energy measurement
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

    // Populate results.
    result.runtime_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    result.counters.reserve(events.size());
    const auto counters = group.read();
    for (std::size_t i = 0; i < events.size(); ++i) {
        result.counters.emplace_back(perf::toString(events.at(i)), counters.at(i));
    }

    result.energy_samples = std::vector<EnergySample>(energy_samples.begin(), energy_samples.end());
    result.process = process;
    result.status = status;

    return result;
}
}; // namespace benchmark
