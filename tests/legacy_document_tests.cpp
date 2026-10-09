#include "test.hpp"
#include <algorithm>
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

TEST("legacy export uses reference identifiers and translates signed coordinates") {
    Circuit circuit; circuit.set({-2, 7}, Element::source); circuit.set({0, 8}, Element::signal, 3);
    std::ostringstream output; CHECK(write_legacy_document(output, circuit));
    CHECK(output.str() == fixture(3, 2, std::string("\x10\0\0\0\0\x0f", 6)));
    std::istringstream input(output.str()); const auto loaded = read_legacy_document(input);
    CHECK(loaded && loaded->at({0, 0}) == Element::source && loaded->saved_state({2, 1}) == 3);
    Circuit huge; huge.set({std::numeric_limits<Coordinate>::min(), 0}, Element::wire);
    huge.set({std::numeric_limits<Coordinate>::max(), 0}, Element::wire);
    std::ostringstream rejected; CHECK(!write_legacy_document(rejected, huge)); CHECK(rejected.str().empty());
    Circuit empty; std::ostringstream zero; CHECK(write_legacy_document(zero, empty)); CHECK(zero.str() == fixture(0, 0));
}
TEST("legacy serialization round trips every occupied type and state") {
    Circuit circuit;
    for (unsigned type = 1; type < 14; ++type) for (unsigned state = 0; state < 4; ++state)
        circuit.set({static_cast<Coordinate>(type - 1), static_cast<Coordinate>(state)}, legacy_elements[type], static_cast<std::uint8_t>(state));
    std::ostringstream output; CHECK(write_legacy_document(output, circuit));
    std::istringstream input(output.str()); CHECK(read_legacy_document(input).value() == circuit);
}

TEST("legacy block IO retains sparse holes and states across chunk boundaries") {
    Circuit circuit;
    for (Coordinate x : {0, 65535, 65536, 131071, 131074}) circuit.set({x, 0}, Element::nor_gate, 3);
    std::ostringstream output; CHECK(write_legacy_document(output, circuit));
    CHECK(output.str().size() == 16 + 131075);
    std::istringstream input(output.str()); CHECK(read_legacy_document(input).value() == circuit);
    auto bytes = output.str(); bytes[16 + 65536] = static_cast<char>(255);
    std::istringstream bad(bytes); const auto result = read_legacy_document(bad);
    CHECK(!result && result.error().message.find("65552") != std::string::npos);
}

namespace {
class LimitedOutput : public std::streambuf {
public:
    explicit LimitedOutput(std::size_t budget) : budget_(budget) {}
protected:
    std::streamsize xsputn(const char*, std::streamsize amount) override {
        const auto written = std::min(budget_, static_cast<std::size_t>(amount));
        budget_ -= written; return static_cast<std::streamsize>(written);
    }
    int_type overflow(int_type value) override {
        if (traits_type::eq_int_type(value, traits_type::eof())) return traits_type::not_eof(value);
        if (!budget_) return traits_type::eof();
        --budget_; return value;
    }
private:
    std::size_t budget_;
};
}
TEST("legacy writes report short header body and chunk failures") {
    Circuit circuit; circuit.set({0, 0}, Element::source); circuit.set({70000, 0}, Element::wire);
    for (std::size_t budget : {0U, 4U, 15U, 16U, 100U, 65552U}) {
        LimitedOutput storage(budget); std::ostream output(&storage);
        CHECK(!write_legacy_document(output, circuit));
    }
}
TEST("legacy dimensions use signed limits and configurable dense budgets") {
    Circuit circuit; circuit.set({0, 0}, Element::wire); circuit.set({999, 999}, Element::wire);
    CHECK(legacy_layout(circuit, {.max_legacy_area = 1'000'000})->area() == 1'000'000);
    CHECK(!legacy_layout(circuit, {.max_legacy_area = 999'999}));
    CHECK(!legacy_layout(circuit, {.max_cells = 1}));
    std::istringstream empty_width(fixture(0, 500)); CHECK(read_legacy_document(empty_width)->empty());
    auto bytes = fixture(0, 0); bytes[0] = 'X';
    std::istringstream bad_magic(bytes); CHECK(!read_legacy_document(bad_magic));
}
