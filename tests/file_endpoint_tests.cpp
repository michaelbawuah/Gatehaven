#include "test.hpp"
#include "gatehaven/file_endpoints.hpp"
#include "gatehaven/file_io.hpp"
#include "gatehaven/simulation.hpp"
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
    CHECK(!endpoints.choose_input(input.id, root));
    CHECK(!endpoints.choose_output(output.id, root));
    CHECK(endpoints.bound_files() == 2); // Failed choices retain the existing streams.
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

TEST("real simulated signal frames write every byte and acknowledge on the return wire") {
    const auto path = std::filesystem::temp_directory_path() / ("gatehaven-loop-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup { std::filesystem::path p; ~Cleanup() { std::filesystem::remove(p); } } cleanup{path};
    Circuit circuit;
    circuit.set({-2, 0}, Element::wire); circuit.set({-1, 0}, Element::signal);
    circuit.set({0, 0}, Element::file_output); circuit.set({1, 0}, Element::wire);
    FileEndpoints endpoints; CHECK(endpoints.choose_output({0, 0}, path));
    Simulation simulation;
    const auto step = [&](bool bit) {
        circuit.set({-3, 0}, bit ? Element::source : Element::empty);
        simulation.step(circuit, [&](const CommunicatorGroup& group, bool sent) { return endpoints.exchange(group, sent); });
        return simulation.powered({1, 0});
    };
    std::string expected;
    for (unsigned byte = 0; byte < 256; ++byte) {
        for (auto bit : serial_reply(0, static_cast<std::uint8_t>(byte), 8)) CHECK(!step(bit != 0));
        CHECK(!step(false)); // The last data bit reaches the port one tick after the Signal.
        expected.push_back(static_cast<char>(byte));
        CHECK(read_bounded_file(path, 256).value() == expected); // Flush precedes acknowledgement.
        CHECK(step(false)); CHECK(!step(false)); CHECK(!step(false));
        CHECK(!simulation.powered({-1, 0})); // Return power cannot backfeed the request Signal.
    }
}

TEST("merged ports use the latest binding and split ports retain their own stream positions") {
    const auto root = std::filesystem::temp_directory_path() / ("gatehaven-merge-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    struct Cleanup { std::filesystem::path p; ~Cleanup() { std::filesystem::remove_all(p); } } cleanup{root};
    CHECK(replace_file(root / "first", "AB")); CHECK(replace_file(root / "second", "XY"));
    FileEndpoints endpoints;
    const CommunicatorGroup left{{0, 0}, Element::file_input, {{0, 0}}};
    const CommunicatorGroup right{{2, 0}, Element::file_input, {{2, 0}}};
    const CommunicatorGroup merged{{0, 0}, Element::file_input, {{0, 0}, {1, 0}, {2, 0}}};
    CHECK(endpoints.choose_input(left.id, root / "first"));
    CHECK(endpoints.choose_input(right.id, root / "second"));
    CHECK(!endpoints.exchange(left, false)); CHECK(!endpoints.exchange(right, false));
    const auto read = [&](const CommunicatorGroup& group, std::uint8_t expected) {
        for (auto bit : serial_reply(0)) CHECK(!endpoints.exchange(group, bit != 0));
        std::vector<std::uint8_t> reply;
        for (unsigned i = 0; i < 11; ++i) reply.push_back(endpoints.exchange(group, false) ? 1 : 0);
        CHECK(reply == serial_reply(0, expected, 8));
    };
    read(merged, 'X');
    read(left, 'A'); read(right, 'Y');
    endpoints.reset_protocols(); read(left, 'B'); // Reset does not rewind the chosen file.
    Circuit remaining; remaining.set(left.id, Element::file_input); endpoints.prune(remaining);
    CHECK(endpoints.bound_files() == 1);
}
