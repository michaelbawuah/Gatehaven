#include "test.hpp"
#include "gatehaven/file_endpoints.hpp"
#include "gatehaven/file_io.hpp"
#include <chrono>
using namespace gatehaven;
TEST("file endpoints read chosen files write flushed bytes and drop deleted bindings") {
    const auto root = std::filesystem::temp_directory_path() / ("gatehaven-endpoints-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    struct Cleanup { std::filesystem::path p; ~Cleanup() { std::filesystem::remove_all(p); } } cleanup{root};
    CHECK(replace_file(root / "in", "Z"));
    FileEndpoints endpoints;
    const CommunicatorGroup input{{0, 0}, Element::file_input, {{0, 0}}};
    const CommunicatorGroup output{{2, 0}, Element::file_output, {{2, 0}}};
    CHECK(endpoints.choose_input(input.id, root / "in"));
    CHECK(endpoints.choose_output(output.id, root / "out"));
    for (auto bit : serial_reply(0)) CHECK(!endpoints.exchange(input, bit != 0));
    std::vector<std::uint8_t> reply;
    for (unsigned i = 0; i < 11; ++i) reply.push_back(endpoints.exchange(input, false) ? 1 : 0);
    CHECK(reply == serial_reply(0, 'Z', 8));
    for (auto bit : serial_reply(0, 'K', 8)) CHECK(!endpoints.exchange(output, bit != 0));
    CHECK(read_bounded_file(root / "out", 10).value() == "K");
    CHECK(endpoints.exchange(output, false));
    CHECK(endpoints.bound_files() == 2);
    Circuit empty; endpoints.prune(empty); CHECK(endpoints.bound_files() == 0);
}
TEST("screen interaction is grouped and reset releases every held cell") {
    FileEndpoints endpoints;
    const CommunicatorGroup screen{{0, 0}, Element::screen, {{0, 0}, {1, 0}}};
    CHECK(!endpoints.exchange(screen, true));
    endpoints.hold_screen({1, 0}); CHECK(endpoints.exchange(screen, false));
    endpoints.reset_protocols(); CHECK(!endpoints.exchange(screen, true));
    endpoints.hold_screen({1, 0}); Circuit empty; endpoints.prune(empty);
    CHECK(!endpoints.exchange(screen, false));
}
TEST("pending input reads survive reloading a file without replaying bytes") {
    const auto root = std::filesystem::temp_directory_path() / ("gatehaven-reload-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    struct Cleanup { std::filesystem::path p; ~Cleanup() { std::filesystem::remove_all(p); } } cleanup{root};
    FileEndpoints endpoints;
    const CommunicatorGroup input{{0, 0}, Element::file_input, {{0, 0}}};
    for (auto bit : serial_reply(0)) CHECK(!endpoints.exchange(input, bit != 0));
    for (unsigned i = 0; i < 5; ++i) CHECK(!endpoints.exchange(input, false));
    CHECK(replace_file(root / "chosen", std::string(1, static_cast<char>(0xFF))));
    CHECK(endpoints.choose_input(input.id, root / "chosen"));
    CHECK(!endpoints.exchange(input, false));
    std::vector<std::uint8_t> reply;
    for (unsigned i = 0; i < 11; ++i) reply.push_back(endpoints.exchange(input, false) ? 1 : 0);
    CHECK(reply == serial_reply(0, 255, 8));
    for (unsigned i = 0; i < 5; ++i) CHECK(!endpoints.exchange(input, false));
}
