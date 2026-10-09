#pragma once

#include "gatehaven/circuit.hpp"

namespace gatehaven {
[[nodiscard]] Circuit starter_circuit();
[[nodiscard]] Circuit oscillator_circuit();
[[nodiscard]] Circuit screen_switch_circuit();
[[nodiscard]] Circuit relay_demo_circuit(bool negative);
} // namespace gatehaven
