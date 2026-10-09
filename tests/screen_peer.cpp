#include "screen_pattern.hpp"
#include "gatehaven/file_io.hpp"
#include "gatehaven/file_endpoints.hpp"
#include "gatehaven/simulation.hpp"
#include "gatehaven/version.hpp"
#include <chrono>
#include <iostream>

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--build-info") { std::cout << gatehaven::build_information(); return 0; }
        if (argc != 4) return 2;
        const std::string_view mode(argv[1]);
        if (mode != "state" && mode != "profile") return 2;
        const auto steps = std::stoull(argv[3]);
        if (steps > 1000000) return 2;
        auto loaded = gatehaven::load_document(argv[2]);
        if (!loaded) throw std::runtime_error(loaded.error().message);
        const auto& circuit = *loaded;
        gatehaven::Simulation simulation;
        const auto begin = std::chrono::steady_clock::now();
        simulation.initialize(circuit);
        const auto compiled = std::chrono::steady_clock::now();
        for (std::uint64_t tick = 0; tick < steps; ++tick)
            simulation.step(circuit, [&](const gatehaven::CommunicatorGroup& group, bool) {
                return group.element == gatehaven::Element::screen && screen_pattern(tick, group.id.x, group.id.y);
            });
        const auto finished = std::chrono::steady_clock::now();
        if (mode == "profile") {
            std::cout << "{\"compile_ms\":" << std::chrono::duration<double, std::milli>(compiled - begin).count()
                      << ",\"steps_ms\":" << std::chrono::duration<double, std::milli>(finished - compiled).count() << "}\n";
        } else {
            std::cout << "x,y,element,powered,conductive\n";
            simulation.visit_state([&](gatehaven::Point point, gatehaven::Power power, bool, bool) {
                std::cout << point.x << ',' << point.y << ',' << gatehaven::name(power.element) << ','
                          << (power.ports != 0) << ',' << simulation.conductive(point) << '\n';
            });
        }
        return std::cout ? 0 : 1;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
