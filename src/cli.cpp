#include "gatehaven/document.hpp"
#include "gatehaven/examples.hpp"
#include "gatehaven/simulation.hpp"
#include "gatehaven/statistics.hpp"

#include <charconv>
#include <iostream>
#include <string_view>

int main(int argc, char** argv) {
    using namespace gatehaven;
    if (argc == 2 && std::string_view(argv[1]) == "--version") {
        std::cout << "Gatehaven 0.1.0-dev (C++23)\n";
        return 0;
    }
    if (argc == 3 && std::string_view(argv[1]) == "example") {
        const auto saved = save_document(argv[2], starter_circuit());
        if (!saved) { std::cerr << saved.error().message << '\n'; return 1; }
        std::cout << "Saved starter circuit to " << argv[2] << '\n';
        return 0;
    }
    const std::string_view command = argc >= 2 ? argv[1] : "";
    if ((command != "check" && command != "run" && command != "stats") || argc < 3 || argc > 4 ||
        ((command == "check" || command == "stats") && argc != 3)) {
        std::cerr << "Gatehaven CLI\n  gatehaven-cli check FILE.ghv\n"
                     "  gatehaven-cli run FILE.ghv [STEPS]\n"
                     "  gatehaven-cli stats FILE.ghv\n"
                     "  gatehaven-cli example FILE.ghv\n"
                     "  gatehaven-cli --version\n";
        return 2;
    }
    std::uint64_t steps = 10;
    if (argc == 4) {
        const std::string_view text(argv[3]);
        const auto result = std::from_chars(text.data(), text.data() + text.size(), steps);
        if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || steps > 1'000'000) {
            std::cerr << "Steps must be an integer from 0 through 1000000\n";
            return 2;
        }
    }
    const auto circuit = load_document(argv[2]);
    if (!circuit) {
        std::cerr << "Line " << circuit.error().line << ": " << circuit.error().message << '\n';
        return 1;
    }
    if (command == "stats") { write_statistics(std::cout, statistics(*circuit)); return 0; }
    std::cout << "Valid document: " << circuit->size() << " cells\n";
    if (command == "check") return 0;
    Simulation simulation;
    for (std::uint64_t i = 0; i < steps; ++i) simulation.step(*circuit);
    std::cout << "Ticks: " << simulation.ticks() << "\nPowered cells: " << simulation.powered_count() << '\n';
    for (const auto& [point, power] : simulation.snapshot()) {
        std::cout << point.x << ' ' << point.y << ' ' << name(power.element) << ' '
                  << static_cast<unsigned>(power.ports) << '\n';
    }
    return 0;
}
