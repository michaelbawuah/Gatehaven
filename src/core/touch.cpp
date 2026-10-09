#include "gatehaven/touch.hpp"
#include <cmath>

namespace gatehaven {
namespace {
bool valid(TouchPoint point) { return std::isfinite(point.x) && std::isfinite(point.y); }
std::pair<TouchPoint, double> geometry(const std::map<TouchId, TouchPoint>& contacts) {
    const auto a = contacts.begin()->second;
    const auto b = std::next(contacts.begin())->second;
    return {{(a.x + b.x) / 2, (a.y + b.y) / 2}, std::hypot(a.x - b.x, a.y - b.y)};
}
}
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
    if (navigation_ && contacts_.size() == 2) {
        const auto before = geometry(contacts_); found->second = point;
        const auto after = geometry(contacts_);
        // Coincident contacts may pan but cannot define a useful zoom ratio.
        const double zoom = before.second >= 2 && after.second >= 2 ? after.second / before.second : 1;
        return {TouchAction::navigate, before.first, after.first, zoom};
    }
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
