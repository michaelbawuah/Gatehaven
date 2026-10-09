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
}
