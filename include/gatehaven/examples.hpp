#pragma once

#include "gatehaven/circuit.hpp"

namespace gatehaven {
[[nodiscard]] Circuit starter_circuit();
[[nodiscard]] Circuit oscillator_circuit();
[[nodiscard]] Circuit screen_switch_circuit();
[[nodiscard]] Circuit relay_demo_circuit(bool negative);
[[nodiscard]] Circuit gate_gallery_circuit();
inline constexpr std::array<std::string_view, 6> example_names{"starter", "oscillator", "screen-switch", "positive-relay", "negative-relay", "gate-gallery"};
[[nodiscard]] std::optional<Circuit> make_example(std::string_view name);
} // namespace gatehaven
