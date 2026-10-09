#include "gatehaven/serial_protocol.hpp"
#include <stdexcept>

namespace gatehaven {
std::optional<SerialRequest> RequestDecoder::push(bool bit) {
    if (!active_) {
        if (bit) { active_ = true; phase_ = 0; request_ = {}; }
        return std::nullopt;
    }
    if (phase_ < 2) request_.command = static_cast<std::uint8_t>((request_.command << 1) | (bit ? 1 : 0));
    else if (bit) request_.byte = static_cast<std::uint8_t>(request_.byte | (1U << (phase_ - 2)));
    ++phase_;
    if (phase_ == (payload_ ? 10U : 2U)) { active_ = false; return request_; }
    return std::nullopt;
}
std::vector<std::uint8_t> serial_reply(std::uint8_t command, std::uint8_t data, unsigned width) {
    if (command > 3 || (width != 0 && width != 1 && width != 8)) throw std::invalid_argument("Invalid serial reply");
    std::vector<std::uint8_t> result{1, static_cast<std::uint8_t>((command >> 1) & 1), static_cast<std::uint8_t>(command & 1)};
    for (unsigned bit = 0; bit < width; ++bit) result.push_back(static_cast<std::uint8_t>((data >> bit) & 1));
    return result;
}
bool InputProtocol::step(bool request_bit, const Read& read, const More& more) {
    bool receiving = false;
    if (!reply_.empty()) { receiving = reply_.front() != 0; reply_.pop_front(); }
    if (const auto request = decoder_.push(request_bit); request && request->command < 2) {
        if (requests_.size() >= 4096) error_ = "Input request queue is full";
        else requests_.push_back(*request);
    }
    if (!error_.empty() || !reply_.empty() || requests_.empty()) return receiving;
    const auto command = requests_.front().command;
    std::vector<std::uint8_t> bits;
    if (command == 0) {
        const auto byte = read();
        if (!byte) { error_ = byte.error(); return receiving; }
        if (!*byte) return receiving; // EOF waits for a newly chosen input file.
        bits = serial_reply(0, **byte, 8);
    } else {
        const auto available = more();
        if (!available) { error_ = available.error(); return receiving; }
        bits = serial_reply(1, *available ? std::uint8_t{1} : std::uint8_t{0}, 1);
    }
    requests_.pop_front(); reply_.insert(reply_.end(), bits.begin(), bits.end());
    return receiving;
}
void InputProtocol::reset() { decoder_.reset(); requests_.clear(); reply_.clear(); error_.clear(); }
}
