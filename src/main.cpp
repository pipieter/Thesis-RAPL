#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <sys/resource.h>
#include <sys/time.h>

#include <argparse/argparse.hpp>
#include <glaze/json/write.hpp>

#include <rapl/benchmark.hpp>

namespace {
std::string json;
std::string command;
} // namespace

void setup(int argc, char** argv) {
    auto program = argparse::ArgumentParser("RAPL", "", argparse::default_arguments::help);

    program.add_argument("--json").required().help("the filename where the results are written").metavar("PATH");
    program.add_argument("command").required().remaining().help("the command to run").metavar("COMMAND...");

    try {
        program.parse_args(argc, argv);
        json = program.get<std::string>("--json");
        // FIXME C++23: Use std::ranges::views::join_with.
        const auto parts = program.get<std::vector<std::string>>("command");
        command = std::accumulate(parts.begin(), parts.end(), std::string(),
                                  [](const auto& a, const auto& b) { return a + " " + b; });
    } catch (const std::exception& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        std::exit(EXIT_FAILURE);
    }
}

int main(int argc, char** argv) {
    setup(argc, argv);

    [[maybe_unused]] const auto result = benchmark::measure(command);

    if (result.status != 0) {
        std::cerr << "The underlying program failed." << std::endl;
        std::exit(EXIT_FAILURE);
    }

    if (!(std::ofstream(json, std::ios_base::app) << glz::write_json(result) << "\n")) {
        std::cerr << "write failed" << std::endl;
        return 1;
    }
}
