# RAPL

Tool based on Intel's Running Average Power Limit (RAPL) functionality. The tool is a modified program based on the work of [van Kempen et al](https://github.com/nicovank/Energy-Languages). The tool only works for Intel processors and Unix-based operating systems.

The tool was verified to work using the following laptops:

| Manufacturer | CPU                    | Operating System |
| ------------ | ---------------------- | ---------------- |
| Lenovo       | 11th Gen Intel Core i7 | Ubuntu 22.04 LTS |
| ASUS         | 13th Gen Intel Core i7 | Debian GNU 12    |

The program executes a command-line program and writes the execution time, energy usage of the system, resource usage such as CPU usage and memory usage, and several *perf* events to a JSON file. Multiple runs are written on separate lines in the JSON file.

## Building

The tool can be built using CMake.

```bash
mkdir build
sudo modprobe msr
sudo cmake . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
sudo cmake --build build
```

`sudo` is required because internally the tool makes use of the model-specific registers (MSR), and access needs to be allowed at compile time.

## Running

Given a command `command` that needs to be run and measured, the tool is used as:

```
sudo ./build/rapl --json [path] [command]
```

For example, the energy usage of the `sleep` command stored in file `sleep.json` is executed as:

```
sudo ./build/rapl --json sleep.json sleep 10
```