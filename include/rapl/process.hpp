#pragma once

#include <sys/types.h>

struct ProcessMemory {
    int private_memory;
    int shared_memory;
    int swapped_memory;
};

namespace process {

bool getProcessMemory(pid_t pid, ProcessMemory* memory);
bool getProcessAverageCpuUsage(pid_t pid, double* usage);
int getCpuCount();

} // namespace process