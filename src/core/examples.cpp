#include "gatehaven/examples.hpp"

namespace gatehaven {

Circuit starter_circuit() {
    Circuit c;
    // Two isolated inputs drive an AND gate in the center of the canvas.
    c.set({-7, -2}, Element::source);
    c.set({-7, 2}, Element::source);
    for (Coordinate x = -6; x <= 0; ++x) {
        c.set({x, -2}, Element::wire);
        c.set({x, 2}, Element::wire);
    }
    c.set({0, -1}, Element::signal);
    c.set({0, 1}, Element::signal);
    c.set({0, 0}, Element::and_gate);
    for (Coordinate x = 1; x <= 8; ++x) c.set({x, 0}, Element::wire);
    // A disconnected crossing demonstration below the main example.
    c.set({-7, 7}, Element::source);
    for (Coordinate x = -6; x <= 8; ++x) c.set({x, 7}, Element::crossing);
    for (Coordinate y = 4; y <= 10; ++y) c.set({0, y}, Element::crossing);
    return c;
}

Circuit oscillator_circuit() {
    Circuit c;
    c.set({0, 0}, Element::nor_gate);
    c.set({0, 1}, Element::signal);
    c.set({0, 2}, Element::wire);
    c.set({1, 2}, Element::wire);
    c.set({2, 2}, Element::wire);
    c.set({2, 1}, Element::wire);
    c.set({2, 0}, Element::wire);
    c.set({1, 0}, Element::wire);
    return c;
}

} // namespace gatehaven
