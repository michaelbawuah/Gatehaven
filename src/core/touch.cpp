#include "gatehaven/touch.hpp"
#include <cmath>

namespace gatehaven {
namespace { bool valid(TouchPoint point) { return std::isfinite(point.x) && std::isfinite(point.y); } }
TouchUpdate TouchGesture::down(TouchId id, TouchPoint point) {
    if (!valid(point) || contacts_.contains(id) || contacts_.size() >= 16) return {};
    contacts_.emplace(id, point);
    if (contacts_.size() == 1) { navigation_ = false; return {TouchAction::begin, point, point}; }
    navigation_ = true;
    return {TouchAction::cancel, point, point};
}
TouchUpdate TouchGesture::move(TouchId id, TouchPoint point) {
    const auto found = contacts_.find(id);
    if (found == contacts_.end() || !valid(point)) return {};
    const auto before = found->second; found->second = point;
    return navigation_ ? TouchUpdate{} : TouchUpdate{TouchAction::move, before, point};
}
TouchUpdate TouchGesture::up(TouchId id, TouchPoint point) {
    const auto found = contacts_.find(id);
    if (found == contacts_.end()) return {};
    const auto before = found->second; if (!valid(point)) point = before;
    contacts_.erase(found);
    const auto action = navigation_ ? TouchAction::none : TouchAction::end;
    if (contacts_.empty()) navigation_ = false;
    return {action, before, point};
}
}
