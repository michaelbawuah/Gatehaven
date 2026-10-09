#include "gatehaven/selection.hpp"
#include "gatehaven/stamp_preview.hpp"
#include <charconv>
#include <chrono>
#include <iostream>

int main(int argc, char** argv) {
    using namespace gatehaven;
    unsigned count = 100000;
    if (argc > 2) return 2;
    if (argc == 2) {
        const std::string_view text(argv[1]);
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), count);
        if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) return 2;
    }
    if (count < 100 || count > 1000000) return 2;
    using Clock = std::chrono::steady_clock;
    const auto milliseconds = [](auto from) { return std::chrono::duration<double, std::milli>(Clock::now() - from).count(); };
    const auto stroke = pencil_line({0, 0}, {static_cast<Coordinate>(count - 1), 0}, Element::wire);
    Circuit circuit; History history;
    auto start = Clock::now();
    if (!history.apply(circuit, *stroke)) return 1;
    const auto edit_ms = milliseconds(start);
    start = Clock::now(); const auto stamp = capture(circuit, *circuit.bounds());
    const auto capture_ms = milliseconds(start);
    start = Clock::now(); StampPreview preview; preview.reset(stamp);
    const auto index_ms = milliseconds(start);
    start = Clock::now(); std::size_t visited = 0;
    for (unsigned i = 0; i < 1000; ++i) preview.visit({0, 0}, {{10, -10}, {19, 10}}, [&](Cell) { ++visited; });
    const auto preview_ms = milliseconds(start);
    start = Clock::now();
    if (!history.undo(circuit) || !circuit.empty() || !history.redo(circuit) || circuit.size() != count || visited != 10000) return 1;
    const auto history_ms = milliseconds(start);
    std::cout << "{\"cells\":" << count << ",\"edit_ms\":" << edit_ms << ",\"capture_ms\":" << capture_ms
              << ",\"index_ms\":" << index_ms << ",\"preview_1000_ms\":" << preview_ms
              << ",\"preview_cells\":" << visited << ",\"undo_redo_ms\":" << history_ms << "}\n";
    return std::cout ? 0 : 1;
}
