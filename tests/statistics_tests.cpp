#include "test.hpp"
#include "gatehaven/statistics.hpp"
using namespace gatehaven;
TEST("circuit statistics distinguish occupied cells from unrepresentably large bounding areas") {
    Circuit c;
    CHECK(statistics(c).area == 0);
    c.set({std::numeric_limits<Coordinate>::min(), std::numeric_limits<Coordinate>::min()}, Element::wire);
    c.set({std::numeric_limits<Coordinate>::max(), std::numeric_limits<Coordinate>::max()}, Element::source);
    const auto result = statistics(c);
    CHECK(result.cells == 2 && result.width == 4294967296ULL && !result.area);
    CHECK(result.elements[static_cast<std::size_t>(Element::source)] == 1);
}
