#include "test.hpp"
#include "reference_simulation.hpp"
#include <random>
using namespace gatehaven;
TEST("simulation matches the frozen reference across seeded mixed circuits and edits") {
    std::mt19937 random(20261009);
    for (unsigned trial = 0; trial < 60; ++trial) {
        Circuit circuit;
        for (Coordinate y = -4; y <= 4; ++y) for (Coordinate x = -6; x <= 6; ++x) {
            if (random() % 3 != 0) circuit.set({x, y}, static_cast<Element>(1 + random() % 13));
        }
        Simulation fast; reference::ReferenceSimulation slow;
        for (unsigned tick = 0; tick < 24; ++tick) {
            if (tick % 5 == 0) {
                const Point point{static_cast<Coordinate>(random() % 13) - 6, static_cast<Coordinate>(random() % 9) - 4};
                circuit.set(point, static_cast<Element>(random() % 14));
                const std::array changed{point}; fast.invalidate(changed); slow.invalidate(changed);
            }
            const Simulation::Exchange exchange = [tick](const CommunicatorGroup& group, bool sending) {
                return ((static_cast<std::uint32_t>(group.id.x) ^ tick) & 1U) != 0 && !sending;
            };
            fast.step(circuit, exchange); slow.step(circuit, exchange);
            CHECK(fast.snapshot() == slow.snapshot());
            CHECK(fast.powered_count() == slow.powered_count());
            for (const auto& cell : circuit.cells()) CHECK(fast.sent(cell.position) == slow.sent(cell.position));
        }
    }
}
