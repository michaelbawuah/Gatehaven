#pragma once
#include "gatehaven/circuit.hpp"

namespace gatehaven {
struct CommunicatorGroup {
    Point id;
    Element element;
    std::vector<Point> cells;
};
[[nodiscard]] std::vector<CommunicatorGroup> communicator_groups(const Circuit& circuit);
}
