#include "gatehaven/examples.hpp"

namespace gatehaven {

std::optional<Circuit> make_example(std::string_view name) {
    if (name == "starter") return starter_circuit();
    if (name == "oscillator") return oscillator_circuit();
    if (name == "screen-switch") return screen_switch_circuit();
    if (name == "positive-relay") return relay_demo_circuit(false);
    if (name == "negative-relay") return relay_demo_circuit(true);
    if (name == "gate-gallery") return gate_gallery_circuit();
    return std::nullopt;
}

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

Circuit screen_switch_circuit() {
    Circuit c;
    c.set({0, 0}, Element::screen);
    for (Coordinate x = 1; x < 4; ++x) c.set({x, 0}, Element::wire);
    c.set({4, 0}, Element::signal); c.set({5, 0}, Element::and_gate);
    for (Coordinate x = 6; x < 9; ++x) c.set({x, 0}, Element::wire);
    c.set({9, 0}, Element::signal); c.set({10, 0}, Element::screen);
    return c;
}

Circuit relay_demo_circuit(bool negative) {
    Circuit c;
    c.set({-4, 0}, Element::source);
    for (Coordinate x = -3; x <= 3; ++x) if (x != 0) c.set({x, 0}, Element::wire);
    c.set({0, 0}, negative ? Element::negative_relay : Element::positive_relay);
    c.set({4, 0}, Element::signal); c.set({5, 0}, Element::screen);
    c.set({0, 1}, Element::signal); c.set({0, 2}, Element::wire);
    c.set({0, 3}, Element::wire); c.set({0, 4}, Element::screen);
    return c;
}

Circuit gate_gallery_circuit() {
    Circuit c;
    constexpr std::array gates{Element::and_gate, Element::or_gate, Element::nand_gate, Element::nor_gate};
    for (std::size_t i = 0; i < gates.size(); ++i) {
        const auto x = static_cast<Coordinate>(i) * 10;
        c.set({x, 0}, gates[i]);
        for (const Coordinate side : {Coordinate{-1}, Coordinate{1}}) {
            c.set({x + side, 0}, Element::signal); c.set({x + side * 2, 0}, Element::wire);
            c.set({x + side * 3, 0}, Element::screen);
        }
        c.set({x, -1}, Element::wire); c.set({x, -2}, Element::signal); c.set({x, -3}, Element::screen);
    }
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
