#pragma once

#include "gatehaven/circuit.hpp"

#include <deque>
#include <expected>
#include <span>
#include <string>

namespace gatehaven {

[[nodiscard]] std::expected<std::vector<Cell>, std::string> pencil_line(
    Point from, Point to, Element element, std::size_t max_length = 1'000'000);

class History {
public:
    explicit History(std::size_t max_changes = 1'000'000) : max_changes_(max_changes) {}
    [[nodiscard]] std::expected<bool, std::string> apply(Circuit& circuit, std::span<const Cell> edits);
    bool undo(Circuit& circuit);
    bool redo(Circuit& circuit);
    void clear();
    [[nodiscard]] bool can_undo() const noexcept { return !undo_.empty(); }
    [[nodiscard]] bool can_redo() const noexcept { return !redo_.empty(); }
    [[nodiscard]] std::span<const Point> last_changes() const noexcept { return last_changes_; }

private:
    struct Delta { Point point; Element before; Element after; };
    using Command = std::vector<Delta>;
    std::deque<Command> undo_;
    std::deque<Command> redo_;
    std::vector<Point> last_changes_;
    std::size_t max_changes_;
    std::size_t stored_changes_{};
};

struct StampCell {
    std::int64_t x;
    std::int64_t y;
    Element element;
    bool operator==(const StampCell&) const = default;
};
struct Stamp {
    std::int64_t width{};
    std::int64_t height{};
    std::vector<StampCell> cells;
    void rotate_clockwise();
    void flip_horizontal();
    void flip_vertical();
    bool operator==(const Stamp&) const = default;
};

[[nodiscard]] Stamp capture(const Circuit& circuit, Bounds region);
[[nodiscard]] std::expected<std::vector<Cell>, std::string> paste(const Stamp& stamp, Point origin);

struct SelectionEdit { Bounds region; std::vector<Cell> edits; };
[[nodiscard]] std::expected<SelectionEdit, std::string> move_region(
    const Circuit& circuit, Bounds region, std::int64_t dx, std::int64_t dy);

} // namespace gatehaven
