#include "gatehaven/communicator.hpp"
#include <set>
#include <algorithm>

namespace gatehaven {
std::vector<CommunicatorGroup> communicator_groups(const Circuit& circuit) {
    std::set<Point> visited;
    std::vector<CommunicatorGroup> result;
    for (const auto& cell : circuit.cells()) {
        if (!is_communicator(cell.element) || !visited.insert(cell.position).second) continue;
        CommunicatorGroup group{cell.position, cell.element, {cell.position}};
        for (std::size_t i = 0; i < group.cells.size(); ++i) {
            for (const auto direction : directions) {
                const auto next = neighbor(group.cells[i], direction);
                if (next && circuit.at(*next) == cell.element && visited.insert(*next).second) group.cells.push_back(*next);
            }
        }
        std::sort(group.cells.begin(), group.cells.end());
        result.push_back(std::move(group));
    }
    return result;
}
}
