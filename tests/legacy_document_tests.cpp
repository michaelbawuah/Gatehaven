#include "test.hpp"
#include "gatehaven/legacy_document.hpp"
#include <sstream>
using namespace gatehaven;
namespace {
std::string fixture(std::uint32_t width, std::uint32_t height, std::string cells = {}) {
    std::string bytes = "CCPG";
    for (const auto value : {0U, width, height}) for (unsigned i = 0; i < 4; ++i)
        bytes.push_back(static_cast<char>((value >> (i * 8)) & 255));
    return bytes + cells;
}
}
TEST("legacy reader maps every element and preserves both level bits") {
    std::string data;
    for (unsigned kind = 0; kind < 14; ++kind) for (unsigned state = 0; state < 4; ++state)
        data.push_back(static_cast<char>((kind << 2) | state));
    std::istringstream stream(fixture(14, 4, data));
    const auto circuit = read_legacy_document(stream); CHECK(circuit && circuit->size() == 52);
    for (unsigned i = 4; i < 56; ++i) {
        const Point point{static_cast<Coordinate>(i % 14), static_cast<Coordinate>(i / 14)};
        CHECK(circuit->at(point) == legacy_elements[i / 4]);
        CHECK(circuit->saved_state(point) == i % 4);
    }
}
TEST("legacy parser fails every truncated prefix without partial results") {
    const auto bytes = fixture(3, 2, std::string(6, '\x04'));
    for (std::size_t length = 0; length < bytes.size(); ++length) {
        std::istringstream input(bytes.substr(0, length)); CHECK(!read_legacy_document(input));
    }
    std::istringstream extended(bytes + "ignored extension"); CHECK(read_legacy_document(extended));
}
TEST("legacy dimensions version unknown elements and resource budgets are checked") {
    for (const auto& bytes : {fixture(0xffffffffU, 1), fixture(1, 0x80000000U), fixture(50000, 50000),
                              fixture(1, 1, std::string(1, '\xff'))}) {
        std::istringstream input(bytes); CHECK(!read_legacy_document(input));
    }
    auto bad_version = fixture(0, 0); bad_version[4] = 1;
    std::istringstream version(bad_version); CHECK(!read_legacy_document(version));
    std::istringstream area(fixture(100, 100)); CHECK(!read_legacy_document(area, {.max_legacy_area = 99}));
    std::istringstream cells(fixture(2, 1, "\x04\x04")); CHECK(!read_legacy_document(cells, {.max_cells = 1}));
    std::istringstream empty(fixture(0, 0)); CHECK(read_legacy_document(empty)->empty());
}
