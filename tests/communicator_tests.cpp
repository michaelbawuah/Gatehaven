#include "test.hpp"
#include "gatehaven/communicator.hpp"
#include "gatehaven/simulation.hpp"
using namespace gatehaven;
TEST("communicator groups join only adjacent cells of the same type") {
    Circuit c;
    c.set({0, 0}, Element::screen); c.set({1, 0}, Element::screen);
    c.set({2, 0}, Element::file_input); c.set({3, 0}, Element::screen);
    const auto groups = communicator_groups(c);
    CHECK(groups.size() == 3); CHECK(groups[0].cells.size() == 2);
    CHECK(groups[0].id == Point{0, 0}); CHECK(groups[1].element == Element::file_input);
    c.set({1, 0}, Element::empty);
    CHECK(communicator_groups(c)[0].cells.size() == 1);
}
