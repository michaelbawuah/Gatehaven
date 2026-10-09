#include "gatehaven/simulation.hpp"

#include <charconv>
#include <chrono>
#include <iostream>
#include <string_view>

int main(int argc, char** argv) {
    using namespace gatehaven;
    unsigned count = 10000;
    unsigned steps = 20;
    const auto parse = [](const char* raw, unsigned& value) {
        const std::string_view text(raw);
        const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
        return result.ec == std::errc{} && result.ptr == text.data() + text.size();
    };
    if (argc > 3 || (argc > 1 && !parse(argv[1], count)) || (argc > 2 && !parse(argv[2], steps)) ||
        count < 2 || count > 1000000 || steps == 0 || steps > 10000) {
        std::cerr << "Usage: gatehaven-bench [CELLS 2..1000000] [STEPS 1..10000]\n";
        return 2;
    }
    Circuit circuit;
    circuit.set({0, 0}, Element::source);
    for (unsigned i = 1; i < count; ++i) {
        circuit.set({static_cast<Coordinate>(i), 0}, Element::wire);
    }
    Simulation simulation;
    simulation.step(circuit); // Warm up outside the timed region.
    const auto start = std::chrono::steady_clock::now();
    for (unsigned i = 0; i < steps; ++i) simulation.step(circuit);
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    if (simulation.powered_count() != count) {
        std::cerr << "Benchmark circuit did not produce its expected state\n";
        return 1;
    }
    std::cout << "{\"workload\":\"wire-chain\",\"cells\":" << count << ",\"steps\":" << steps
              << ",\"total_ms\":" << ms << ",\"ms_per_step\":" << ms / steps << "}\n";
}
