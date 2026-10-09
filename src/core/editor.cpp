#include "gatehaven/editor.hpp"

#include <algorithm>
#include <cstdlib>
#include <set>
#include <utility>

namespace gatehaven {

std::expected<std::vector<Cell>, std::string> pencil_line(Point from, Point to, Element element,
                                                        std::size_t max_length) {
    if (static_cast<std::size_t>(element) >= element_names.size()) return std::unexpected("Invalid pencil element");
    const auto dx = static_cast<std::int64_t>(to.x) - from.x;
    const auto dy = static_cast<std::int64_t>(to.y) - from.y;
    const bool horizontal = std::abs(dx) >= std::abs(dy);
    const auto distance = horizontal ? dx : dy;
    const auto count = static_cast<std::uint64_t>(std::abs(distance)) + 1;
    if (count > max_length) return std::unexpected("Stroke is too long");
    const std::int64_t step = distance < 0 ? -1 : 1;
    std::vector<Cell> edits;
    edits.reserve(static_cast<std::size_t>(count));
    for (std::uint64_t i = 0; i < count; ++i) {
        const auto delta = step * static_cast<std::int64_t>(i);
        edits.push_back({*translated(from, horizontal ? delta : 0, horizontal ? 0 : delta), element});
    }
    return edits;
}

std::expected<bool, std::string> History::apply(Circuit& circuit, std::span<const Cell> edits) {
    last_changes_.clear();
    for (const auto& edit : edits) {
        if (static_cast<std::size_t>(edit.element) >= element_names.size()) {
            return std::unexpected("Invalid element in edit");
        }
    }
    std::vector<Cell> final(edits.begin(), edits.end());
    // Stable order preserves the last requested value at overlapping coordinates.
    std::stable_sort(final.begin(), final.end(), [](const Cell& a, const Cell& b) { return a.position < b.position; });
    Command command;
    command.reserve(std::min(final.size(), max_changes_));
    for (std::size_t i = 0; i < final.size(); ++i) {
        if (i + 1 < final.size() && final[i].position == final[i + 1].position) continue;
        const auto& [point, element] = final[i];
        const auto before = circuit.at(point);
        if (before != element) command.push_back({point, before, element});
        if (command.size() > max_changes_) return std::unexpected("Edit exceeds undo history limit");
    }
    if (command.empty()) return false;
    if (command.size() > max_changes_) return std::unexpected("Edit exceeds undo history limit");
    command.before_revision = revision_;
    command.after_revision = next_revision_++;
    for (const auto& delta : command) {
        circuit.set(delta.point, delta.after);
        last_changes_.push_back(delta.point);
    }
    for (const auto& stale : redo_) stored_changes_ -= stale.size();
    redo_.clear();
    stored_changes_ += command.size();
    revision_ = command.after_revision;
    undo_.push_back(std::move(command));
    while (stored_changes_ > max_changes_ && undo_.size() > 1) {
        stored_changes_ -= undo_.front().size();
        undo_.pop_front();
    }
    return true;
}

bool History::undo(Circuit& circuit) {
    last_changes_.clear();
    if (undo_.empty()) return false;
    auto command = std::move(undo_.back());
    revision_ = command.before_revision;
    undo_.pop_back();
    for (const auto& delta : command) {
        circuit.set(delta.point, delta.before);
        last_changes_.push_back(delta.point);
    }
    redo_.push_back(std::move(command));
    return true;
}

bool History::redo(Circuit& circuit) {
    last_changes_.clear();
    if (redo_.empty()) return false;
    auto command = std::move(redo_.back());
    revision_ = command.after_revision;
    redo_.pop_back();
    for (const auto& delta : command) {
        circuit.set(delta.point, delta.after);
        last_changes_.push_back(delta.point);
    }
    undo_.push_back(std::move(command));
    return true;
}

void History::clear() {
    undo_.clear();
    redo_.clear();
    last_changes_.clear();
    stored_changes_ = 0;
    revision_ = saved_revision_ = 0;
    next_revision_ = 1;
}

Stamp capture(const Circuit& circuit, Bounds region) {
    if (region.min.x > region.max.x || region.min.y > region.max.y) return {};
    Stamp stamp{static_cast<std::int64_t>(region.max.x) - region.min.x + 1,
                static_cast<std::int64_t>(region.max.y) - region.min.y + 1, {}};
    circuit.visit(region, [&](const Cell& cell) {
        stamp.cells.push_back({static_cast<std::int64_t>(cell.position.x) - region.min.x,
                               static_cast<std::int64_t>(cell.position.y) - region.min.y, cell.element});
    });
    return stamp;
}

void Stamp::rotate_clockwise() {
    for (auto& cell : cells) {
        const auto old_x = cell.x;
        cell.x = height - 1 - cell.y;
        cell.y = old_x;
    }
    std::swap(width, height);
}

void Stamp::flip_horizontal() {
    for (auto& cell : cells) cell.x = width - 1 - cell.x;
}

void Stamp::flip_vertical() {
    for (auto& cell : cells) cell.y = height - 1 - cell.y;
}

std::expected<std::vector<Cell>, std::string> paste(const Stamp& stamp, Point origin) {
    constexpr std::int64_t extent = 4294967296LL;
    if (stamp.width < 0 || stamp.height < 0 || stamp.width > extent || stamp.height > extent ||
        stamp.cells.size() > 1'000'000 || ((stamp.width == 0 || stamp.height == 0) &&
        (stamp.width != 0 || stamp.height != 0 || !stamp.cells.empty()))) return std::unexpected("Invalid stamp dimensions");
    if (stamp.width != 0 && !translated(origin, stamp.width - 1, stamp.height - 1)) return std::unexpected("Pasted selection border exceeds coordinate limits");
    std::vector<Cell> edits;
    edits.reserve(stamp.cells.size());
    std::set<Point> seen;
    for (const auto& cell : stamp.cells) {
        if (cell.x < 0 || cell.y < 0 || cell.x >= stamp.width || cell.y >= stamp.height || cell.element == Element::empty ||
            static_cast<std::size_t>(cell.element) >= element_names.size()) return std::unexpected("Invalid stamp cell");
        const auto point = translated(origin, cell.x, cell.y);
        if (!point) return std::unexpected("Pasted circuit would exceed coordinate limits");
        if (!seen.insert(*point).second) return std::unexpected("Duplicate stamp coordinate");
        edits.push_back({*point, cell.element});
    }
    return edits;
}

std::expected<SelectionEdit, std::string> move_region(const Circuit& circuit, Bounds region,
                                                     std::int64_t dx, std::int64_t dy) {
    if (region.min.x > region.max.x || region.min.y > region.max.y) return std::unexpected("Invalid selection bounds");
    const auto first = translated(region.min, dx, dy);
    const auto last = translated(region.max, dx, dy);
    if (!first || !last) return std::unexpected("Selection would exceed coordinate limits");
    SelectionEdit result{{*first, *last}, {}};
    const auto cells = circuit.cells_in(region);
    result.edits.reserve(cells.size() * 2);
    // Clear the complete source before placing the destination. History keeps
    // the last edit at an overlapping coordinate and restores overwritten cells.
    for (const auto& cell : cells) result.edits.push_back({cell.position, Element::empty});
    for (const auto& cell : cells) result.edits.push_back({*translated(cell.position, dx, dy), cell.element});
    return result;
}

} // namespace gatehaven
