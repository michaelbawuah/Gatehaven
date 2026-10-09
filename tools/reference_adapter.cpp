// A test-only bridge to an explicitly supplied external checkout.
// This file does not contain or distribute the reference implementation.
#include <cstring>
#include <stdexcept>
#include <fstream>
#include <iostream>
#include <chrono>
#include <type_traits>
#include "simulator.hpp"
#include "../tests/screen_pattern.hpp"
#include <set>

int main(int argc, char** argv) {
    if (argc != 4) { std::cerr << "reference-adapter state|profile FILE STEPS\n"; return 2; }
    try {
        const std::string mode(argv[1]);
        const bool screens = mode == "screen-state" || mode == "screen-profile";
        if (!screens && mode != "state" && mode != "profile") return 2;
        const auto steps = std::stoull(argv[3]);
        if (steps > 1000000) return 2;
        CanvasState circuit;
        std::ifstream input(argv[2], std::ios::binary);
        if (circuit.loadSave(input) != CanvasState::ReadResult::OK) return 1;
        Simulator engine;
        const auto begin = std::chrono::steady_clock::now();
        engine.compile(circuit);
        const auto compiled = std::chrono::steady_clock::now();
        struct Screen { int index, x, y; };
        std::vector<Screen> inputs;
        if (screens) {
            std::set<int> seen;
            for (int y = 0; y < circuit.height(); ++y) for (int x = 0; x < circuit.width(); ++x)
                if (const auto* cell = std::get_if<ScreenCommunicatorElement>(&circuit[{x, y}])) {
                    const auto index = cell->communicator->communicatorIndex;
                    if (seen.insert(index).second) inputs.push_back({index, x, y});
                }
        }
        const auto step_begin = std::chrono::steady_clock::now();
        for (unsigned long long tick = 0; tick < steps; ++tick) {
            for (const auto& screen : inputs) engine.sendCommunicatorEvent(screen.index, screen_pattern(tick, screen.x, screen.y));
            engine.step();
        }
        const auto finished = std::chrono::steady_clock::now();
        engine.takeSnapshot(circuit);
        if (mode == "profile" || mode == "screen-profile") {
            std::cout << "{\"compile_ms\":" << std::chrono::duration<double, std::milli>(compiled - begin).count()
                      << ",\"steps_ms\":" << std::chrono::duration<double, std::milli>(finished - step_begin).count() << "}\n";
            return 0;
        }
        std::cout << "x,y,element,powered,conductive\n";
        for (int y = 0; y < circuit.height(); ++y) for (int x = 0; x < circuit.width(); ++x) {
            const auto& cell = circuit[{x, y}];
            if (cell.index() == 0) continue;
            bool powered = false, conductive = false;
            std::visit([&](const auto& value) {
                using T = std::decay_t<decltype(value)>;
                if constexpr (!std::is_same_v<T, std::monostate>) powered = value.template getLogicLevel<>();
                if constexpr (std::is_base_of_v<Relay, T>) conductive = value.conductiveState;
            }, cell);
            std::cout << x << ',' << y << ',' << cell.index() << ',' << powered << ',' << conductive << '\n';
        }
        return std::cout ? 0 : 1;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
