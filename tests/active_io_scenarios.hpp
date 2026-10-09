#pragma once
// Independently authored protocol driver shared by the two test-only peers.
// No application code depends on the external implementation.
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace active_io {
inline std::vector<bool> frame(unsigned command, unsigned data = 0, unsigned width = 0) {
    std::vector<bool> bits{true, (command & 2U) != 0, (command & 1U) != 0};
    for (unsigned i = 0; i < width; ++i) bits.push_back((data & (1U << i)) != 0);
    return bits;
}
inline void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
class Replies {
public:
    explicit Replies(bool input) : input_(input) {}
    void push(bool bit) {
        if (bits_.empty() && !bit) return;
        bits_.push_back(bit);
        if (bits_.size() < 3) return;
        const unsigned command = (bits_[1] ? 2U : 0U) | (bits_[2] ? 1U : 0U);
        require(command < 2, "invalid reply command");
        const std::size_t length = input_ ? (command == 0 ? 11 : 4) : 3;
        if (bits_.size() != length) return;
        unsigned value = 0;
        for (std::size_t i = 3; i < length; ++i) if (bits_[i]) value |= 1U << (i - 3);
        messages.emplace_back(command, value); bits_.clear();
    }
    std::vector<std::pair<unsigned, unsigned>> messages;
private:
    bool input_;
    std::vector<bool> bits_;
};
inline void write_bytes(const std::filesystem::path& path, const std::string& bytes) {
    std::ofstream file(path, std::ios::binary); file.write(bytes.data(), static_cast<std::streamsize>(bytes.size())); file.close();
    require(static_cast<bool>(file), "fixture write failed");
}
inline std::string read_bytes(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    require(static_cast<bool>(file), "output file missing");
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
template<class Peer> int run(const std::filesystem::path& directory) {
    std::filesystem::create_directories(directory);
    std::string bytes;
    for (unsigned i = 0; i < 256; ++i) bytes.push_back(static_cast<char>(i));
    const auto input = directory / "all-bytes.bin", reload = directory / "reload.bin", output = directory / "written.bin";
    write_bytes(input, bytes); write_bytes(reload, "Z");
    Peer peer;
    peer.choose_input(input); peer.choose_output(output);
    Replies in(true), out(false);
    std::uint64_t ticks = 0;
    auto tick = [&](bool request_in, bool request_out) {
        in.push(peer.input(request_in)); out.push(peer.output(request_out)); ++ticks;
        if ((ticks & 255U) == 0) std::this_thread::yield();
    };
    auto wait = [&](std::size_t inputs, std::size_t outputs) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (in.messages.size() < inputs || out.messages.size() < outputs) {
            tick(false, false);
            if ((ticks & 1023U) == 0) require(std::chrono::steady_clock::now() < deadline, "reply deadline exceeded");
        }
        require(in.messages.size() == inputs && out.messages.size() == outputs, "unexpected extra reply");
    };
    // Pipelined input requests preserve order; all byte values cross real streams.
    for (unsigned i = 0; i < 256; ++i) for (bool bit : frame(0)) tick(bit, false);
    wait(256, 0);
    for (unsigned i = 0; i < 256; ++i) require(in.messages[i] == std::pair{0U, i}, "input byte/order mismatch");
    for (unsigned i = 0; i < 256; ++i) for (bool bit : frame(0, i, 8)) tick(false, bit);
    wait(256, 256);
    for (const auto& reply : out.messages) require(reply == std::pair{0U, 0U}, "output acknowledgement mismatch");
    require(read_bytes(output) == bytes, "acknowledged output bytes differ");
    for (bool bit : frame(1)) tick(bit, false);
    wait(257, 256);
    require(in.messages.back() == std::pair{1U, 0U}, "EOF query mismatch");
    // A blocked byte request and a later query must resume in order on file reload.
    for (bool bit : frame(0)) tick(bit, false);
    for (bool bit : frame(1)) tick(bit, false);
    for (unsigned i = 0; i < 1000; ++i) tick(false, false);
    require(in.messages.size() == 257, "EOF read did not block its later query");
    peer.choose_input(reload); wait(259, 256);
    require(in.messages[257] == std::pair{0U, static_cast<unsigned>('Z')}, "reload lost blocked request");
    require(in.messages[258] == std::pair{1U, 0U}, "reload reply ordering mismatch");
    std::cout << "{\"schema\":1,\"passed\":true,\"input_bytes\":257,\"output_bytes\":256,"
                 "\"acknowledgements\":256,\"ordered_reload\":true,\"ticks\":" << ticks << "}\n";
    return 0;
}
}
