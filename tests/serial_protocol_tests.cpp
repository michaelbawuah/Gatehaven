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
