#pragma once
#include "gatehaven/simulation.hpp"
#include <string>

namespace gatehaven {
// Read-only, plain text suitable for native dialogs and assistive technology.
[[nodiscard]] std::string describe_cell(const Circuit& circuit, const Simulation& simulation, Point point);
}
