#include "test.hpp"
#include "gatehaven/editor.hpp"
#include "gatehaven/simulation.hpp"
#include <algorithm>
#include <random>
using namespace gatehaven;
TEST("randomized edit transactions undo to the exact original circuit and counts") {
    std::mt19937 random(84721);
    Circuit c; History history;
    for (unsigned round = 0; round < 100; ++round) {
        const auto before = c;
        std::vector<Cell> edits;
        for (unsigned i = 0; i < 25; ++i) edits.push_back({{static_cast<Coordinate>(random() % 17) - 8,
            static_cast<Coordinate>(random() % 17) - 8}, static_cast<Element>(random() % element_names.size())});
        const auto applied = history.apply(c, edits); CHECK(applied);
        const auto after = c;
        if (*applied) { CHECK(history.undo(c) && c == before); CHECK(history.redo(c) && c == after); }
        std::size_t sum = 0;
        for (std::size_t i = 1; i < element_names.size(); ++i) sum += c.count(static_cast<Element>(i));
        CHECK(sum == c.size());
    }
}
TEST("randomized circuits produce identical snapshots in reverse insertion order") {
    std::mt19937 random(95413);
    for (unsigned trial = 0; trial < 40; ++trial) {
        Circuit first;
        for (unsigned i = 0; i < 100; ++i) first.set({static_cast<Coordinate>(random() % 13) - 6,
            static_cast<Coordinate>(random() % 13) - 6}, static_cast<Element>(1 + random() % (element_names.size() - 1)));
        const auto cells = first.cells(); Circuit reverse;
        for (auto it = cells.rbegin(); it != cells.rend(); ++it) reverse.set(it->position, it->element);
        Simulation a, b;
        for (unsigned tick = 0; tick < 8; ++tick) { a.step(first); b.step(reverse); CHECK(a.snapshot() == b.snapshot()); }
    }
}
