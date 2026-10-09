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
    const std::string_view workload = argc > 3 ? argv[3] : "wire-chain";
    if (argc > 4 || (argc > 1 && !parse(argv[1], count)) || (argc > 2 && !parse(argv[2], steps)) ||
        (workload != "wire-chain" && workload != "wire-grid" && workload != "gates" && workload != "screens") ||
        count < 2 || count > 1000000 || steps == 0 || steps > 10000) {
        std::cerr << "Usage: gatehaven-bench [CELLS 2..1000000] [STEPS 1..10000] [wire-chain|wire-grid|gates|screens]\n";
        return 2;
    }
    Circuit circuit;
    for (unsigned i = 0; i < count; ++i) {
        if (workload == "wire-grid") circuit.set({static_cast<Coordinate>(i % 100), static_cast<Coordinate>(i / 100)}, i == 0 ? Element::source : Element::wire);
        else if (workload == "gates" || workload == "screens") circuit.set({static_cast<Coordinate>(i * 2), 0}, workload == "gates" ? Element::nor_gate : Element::screen);
        else circuit.set({static_cast<Coordinate>(i), 0}, i == 0 ? Element::source : Element::wire);
    }
    Simulation simulation;
    const Simulation::Exchange exchange = [](const CommunicatorGroup&, bool) { return true; };
    simulation.step(circuit, exchange); // Warm up outside the timed region.
    const auto start = std::chrono::steady_clock::now();
    for (unsigned i = 0; i < steps; ++i) simulation.step(circuit, exchange);
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    if (circuit.size() != count || simulation.powered_count() != count) {
        std::cerr << "Benchmark circuit did not produce its expected state\n";
        return 1;
    }
    std::cout << "{\"workload\":\"" << workload << "\",\"cells\":" << count << ",\"steps\":" << steps
              << ",\"total_ms\":" << ms << ",\"ms_per_step\":" << ms / steps << "}\n";
}
