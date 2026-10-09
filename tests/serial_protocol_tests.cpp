#include "test.hpp"
#include "gatehaven/serial_protocol.hpp"
using namespace gatehaven;
TEST("serial decoder accepts idle zeros and byte payloads least-significant bit first") {
    for (unsigned byte = 0; byte < 256; ++byte) {
        RequestDecoder decoder(true);
        CHECK(!decoder.push(false)); CHECK(!decoder.push(false));
        const auto bits = serial_reply(0, static_cast<std::uint8_t>(byte), 8);
        std::optional<SerialRequest> request;
        for (auto bit : bits) request = decoder.push(bit != 0);
        CHECK(request && request->command == 0 && request->byte == byte);
    }
}
TEST("serial command bits retain their documented order and reset drops partial frames") {
    RequestDecoder decoder(false);
    for (std::uint8_t command = 0; command < 4; ++command) {
        std::optional<SerialRequest> request;
        for (auto bit : serial_reply(command)) request = decoder.push(bit != 0);
        CHECK(request && request->command == command);
    }
    CHECK(!decoder.push(true)); decoder.reset(); CHECK(!decoder.push(false));
}
TEST("input byte requests wait at EOF and resume without resetting the circuit") {
    InputProtocol input;
    std::optional<std::uint8_t> next;
    const InputProtocol::Read read = [&]() -> std::expected<std::optional<std::uint8_t>, std::string> { auto result = next; next.reset(); return result; };
    const InputProtocol::More more = [&]() -> std::expected<bool, std::string> { return next.has_value(); };
    for (auto bit : serial_reply(0)) CHECK(!input.step(bit != 0, read, more));
    for (unsigned i = 0; i < 20; ++i) CHECK(!input.step(false, read, more));
    CHECK(input.pending() == 1);
    next = 0xA5; CHECK(!input.step(false, read, more));
    std::vector<std::uint8_t> reply;
    for (unsigned i = 0; i < 11; ++i) reply.push_back(input.step(false, read, more) ? 1 : 0);
    CHECK(reply == serial_reply(0, 0xA5, 8)); CHECK(input.pending() == 0);
}
TEST("output acknowledges only successful writes and retries failed bytes in order") {
    OutputProtocol output; bool ready = false; std::vector<std::uint8_t> bytes;
    const OutputProtocol::Write write = [&](std::uint8_t byte) -> std::expected<void, std::string> {
        if (!ready) return std::unexpected("unavailable"); bytes.push_back(byte); return {};
    };
    for (auto bit : serial_reply(0, 0xE3, 8)) CHECK(!output.step(bit != 0, write));
    CHECK(!output.error().empty() && output.pending() == 1 && bytes.empty());
    for (unsigned i = 0; i < 8; ++i) CHECK(!output.step(false, write));
    ready = true; output.retry(); CHECK(!output.step(false, write));
    CHECK(bytes == std::vector<std::uint8_t>{0xE3});
    std::vector<std::uint8_t> ack;
    for (unsigned i = 0; i < 3; ++i) ack.push_back(output.step(false, write) ? 1 : 0);
    CHECK(ack == serial_reply(0));
}
