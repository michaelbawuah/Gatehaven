#pragma once
#include "gatehaven/simulation.hpp"
#include <expected>
#include <ostream>
#include <string>
namespace gatehaven {
[[nodiscard]] std::expected<void, std::string> export_svg(std::ostream& out, const Circuit& circuit, const Simulation* state = nullptr);
}
