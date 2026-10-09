#pragma once
#include <cstdint>
#include <deque>
#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace gatehaven {
struct SerialRequest { std::uint8_t command{}; std::uint8_t byte{}; };
class RequestDecoder {
public:
    explicit RequestDecoder(bool payload) : payload_(payload) {}
    [[nodiscard]] std::optional<SerialRequest> push(bool bit);
    void reset() { active_ = false; phase_ = 0; request_ = {}; }
private:
    bool payload_;
    bool active_{};
    unsigned phase_{};
    SerialRequest request_;
};
[[nodiscard]] std::vector<std::uint8_t> serial_reply(std::uint8_t command, std::uint8_t data = 0, unsigned width = 0);

class InputProtocol {
public:
    using Read = std::function<std::expected<std::optional<std::uint8_t>, std::string>()>;
    using More = std::function<std::expected<bool, std::string>()>;
    [[nodiscard]] bool step(bool request_bit, const Read& read, const More& more);
    void reset();
    void retry() { error_.clear(); }
    [[nodiscard]] const std::string& error() const { return error_; }
    [[nodiscard]] std::size_t pending() const { return requests_.size(); }
private:
    RequestDecoder decoder_{false};
    std::deque<SerialRequest> requests_;
    std::deque<std::uint8_t> reply_;
    std::string error_;
};
class OutputProtocol {
public:
    using Write = std::function<std::expected<void, std::string>(std::uint8_t)>;
    [[nodiscard]] bool step(bool request_bit, const Write& write);
    void reset();
    void retry() { error_.clear(); }
    [[nodiscard]] const std::string& error() const { return error_; }
    [[nodiscard]] std::size_t pending() const { return requests_.size(); }
private:
    RequestDecoder decoder_{true};
    std::deque<SerialRequest> requests_;
    std::deque<std::uint8_t> reply_;
    std::string error_;
};
}
