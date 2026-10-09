#include "gatehaven/file_endpoints.hpp"
#include "gatehaven/file_io.hpp"
#include "gatehaven/version.hpp"
#include <charconv>
#include <chrono>
#include <iostream>

namespace {
using namespace gatehaven;
std::size_t count(const char* text) {
    std::size_t value{}; const std::string_view input(text);
    const auto [end, error] = std::from_chars(input.data(), input.data() + input.size(), value);
    if (error != std::errc{} || end != input.data() + input.size()) throw std::invalid_argument("invalid count");
    return value;
}
struct Directory {
    std::filesystem::path path;
    ~Directory() { std::error_code error; std::filesystem::remove_all(path, error); }
};
}
int main(int argc, char** argv) {
    try {
        if (argc != 4 && argc != 5) return 2;
        const auto cells = count(argv[1]), bytes = count(argv[2]), repeats = count(argv[3]);
        const bool cached = argc == 4;
        if (!cells || cells > 100000 || !bytes || bytes > 65536 || !repeats || repeats > 20 ||
            (!cached && std::string_view(argv[4]) != "uncached")) return 2;
        Directory directory{std::filesystem::temp_directory_path() /
            ("gatehaven-io-bench-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))};
        if (!std::filesystem::create_directory(directory.path)) throw std::runtime_error("cannot create benchmark directory");
        std::string payload;
        for (std::size_t i = 0; i < bytes; ++i) payload.push_back(static_cast<char>(i % 256));
        if (!replace_file(directory.path / "input", payload)) throw std::runtime_error("cannot write benchmark fixture");
        CommunicatorGroup in{{0, 0}, Element::file_input, {}}, out{{0, 2}, Element::file_output, {}};
        Circuit circuit;
        for (std::size_t i = 0; i < cells; ++i) {
            const auto x = static_cast<Coordinate>(i);
            in.cells.push_back({x, 0}); out.cells.push_back({x, 2});
            circuit.set({x, 0}, Element::file_input); circuit.set({x, 2}, Element::file_output);
        }
        std::cout << "{\"schema\":1,\"source_revision\":\"" << source_revision << "\",\"group_cells\":" << cells
                  << ",\"bytes\":" << bytes << ",\"cached\":" << (cached ? "true" : "false") << ",\"runs_ms\":[";
        for (std::size_t repeat = 0; repeat < repeats; ++repeat) {
            FileEndpoints endpoints;
            if (!endpoints.choose_input(in.id, directory.path / "input") || !endpoints.choose_output(out.id, directory.path / "output"))
                throw std::runtime_error("cannot attach benchmark streams");
            std::vector<bool> input_bits, output_bits;
            input_bits.reserve(bytes * 11 + 32); output_bits.reserve(bytes * 11 + 32);
            const auto start = std::chrono::steady_clock::now();
            for (std::size_t tick = 0; tick < bytes * 11 + 32; ++tick) {
                const auto phase = tick % 11, byte = (tick / 11) % 256;
                const bool active = tick < bytes * 11;
                const bool request = active && phase == 0;
                const bool write = active && (phase == 0 || (phase >= 3 && ((byte >> (phase - 3)) & 1U)));
                endpoints.prune(circuit);
                input_bits.push_back(cached ? endpoints.exchange(in, request, circuit.revision()) : endpoints.exchange(in, request));
                output_bits.push_back(cached ? endpoints.exchange(out, write, circuit.revision()) : endpoints.exchange(out, write));
                if (!endpoints.last_error().empty()) throw std::runtime_error(endpoints.last_error());
            }
            const auto finish = std::chrono::steady_clock::now();
            const auto saved = read_bounded_file(directory.path / "output", bytes);
            if (!saved || *saved != payload) throw std::runtime_error("benchmark output mismatch");
            std::vector<std::uint8_t> received;
            std::size_t acknowledgements = 0;
            for (std::size_t i = 0; i < input_bits.size();) {
                if (!input_bits[i]) { ++i; continue; }
                if (i + 11 > input_bits.size() || input_bits[i + 1] || input_bits[i + 2]) throw std::runtime_error("bad input frame");
                std::uint8_t value = 0;
                for (unsigned bit = 0; bit < 8; ++bit) if (input_bits[i + 3 + bit]) value = static_cast<std::uint8_t>(value | (1U << bit));
                received.push_back(value); i += 11;
            }
            for (std::size_t i = 0; i < output_bits.size();) {
                if (!output_bits[i]) { ++i; continue; }
                if (i + 3 > output_bits.size() || output_bits[i + 1] || output_bits[i + 2]) throw std::runtime_error("bad acknowledgement");
                ++acknowledgements; i += 3;
            }
            if (received.size() != bytes || acknowledgements != bytes) throw std::runtime_error("reply count mismatch");
            for (std::size_t i = 0; i < bytes; ++i) if (received[i] != i % 256) throw std::runtime_error("input byte mismatch");
            if (repeat) std::cout << ',';
            std::cout << std::chrono::duration<double, std::milli>(finish - start).count();
        }
        std::cout << "],\"verified_bytes\":true}\n";
        return std::cout ? 0 : 1;
    } catch (const std::invalid_argument& error) { std::cerr << error.what() << '\n'; return 2; }
      catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
