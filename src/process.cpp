#include "rapl/process.hpp"
#include <cctype>
#include <cstdlib>
#include <fstream>

#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>


bool readFile(const std::string &path, std::string *contents) {
  std::ifstream file(path);
  if (file.fail()) {
    return false;
  }

  std::stringstream buffer;
  buffer << file.rdbuf();

  *contents = buffer.str();
  return true;
}

int process::getCpuCount() { return sysconf(_SC_NPROCESSORS_ONLN); }

int parseSmapsRollupLine(std::string line) {
  // Inefficient, should be improved
  std::string digits;

  for (char c : line) {
    if (std::isdigit(c)) {
      digits.push_back(c);
    }
  }

  return std::atoi(digits.c_str());
}

bool process::getProcessMemory(pid_t pid, ProcessMemory *memory) {
  std::string proc = std::string("/proc/") + std::to_string(pid) +
                     std::string("/smaps_rollup");
  std::string contents;

  if (!readFile(proc, &contents)) {
    return false;
  }

  std::istringstream sstream(contents);
  std::string line;

  int shared_memory = 0;
  int private_memory = 0;
  int swapped_memory = 0;

  while (std::getline(sstream, line)) {
    if (line.rfind("Shared_", 0) == 0) {
      shared_memory += parseSmapsRollupLine(line);
    } else if (line.rfind("Private_", 0) == 0) {
      private_memory += parseSmapsRollupLine(line);
    } else if (line.rfind("Swap:", 0) == 0) { // Note, this doesn't include SwapPss
      swapped_memory += parseSmapsRollupLine(line);
    }
  }

  memory->private_memory = private_memory;
  memory->shared_memory = shared_memory;
  memory->swapped_memory = swapped_memory;

  return true;
}

bool process::getProcessAverageCpuUsage(pid_t pid, double *usage) {
  std::string command =
      std::string("ps -o %cpu --no-header -p ") + std::to_string(pid);

  char buffer[128] = {0};

  std::string result;
  FILE *pipe = popen(command.c_str(), "r");
  if (!pipe) {
    return false;
  }

  (void)!fgets(buffer, 128, pipe);

  *usage = std::atof(buffer);
  return true;
}
