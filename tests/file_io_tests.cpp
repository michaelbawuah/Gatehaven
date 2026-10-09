#include "test.hpp"
#include "gatehaven/file_io.hpp"
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
using namespace gatehaven;
TEST("bounded file reads and replacement preserve complete binary payloads") {
    const auto root = std::filesystem::temp_directory_path() / ("gatehaven-io-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    struct Cleanup { std::filesystem::path p; ~Cleanup() { std::filesystem::remove_all(p); } } cleanup{root};
    const auto path = root / "payload.bin";
    CHECK(replace_file(path, std::string("a\0b", 3)));
    CHECK(read_bounded_file(path, 3).value() == std::string("a\0b", 3));
    CHECK(!read_bounded_file(path, 2));
    CHECK(replace_file(path, "second")); CHECK(read_bounded_file(path, 10).value() == "second");
    CHECK(!replace_file(root / "missing" / "file", "bad"));
    CHECK(read_bounded_file(path, 10).value() == "second");
    CHECK(!replace_file(root, "cannot replace directory"));
}

TEST("concurrent replacements always leave one complete file and no temporary debris") {
    const auto root = std::filesystem::temp_directory_path() / ("gatehaven-atomic-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    struct Cleanup { std::filesystem::path p; ~Cleanup() { std::error_code error; std::filesystem::remove_all(p, error); } } cleanup{root};
    const auto path = root / "shared.bin";
    const std::string a(8192, 'a'), b(4096, 'b'); std::atomic<bool> success{true};
    std::mutex error_mutex; std::string failure;
    auto writer = [&](const std::string& bytes) {
        for (unsigned i = 0; i < 12; ++i) {
            const auto result = replace_file(path, bytes);
            if (!result) { success = false; const std::lock_guard guard(error_mutex); failure = result.error(); }
        }
    };
    std::thread first(writer, std::cref(a)), second(writer, std::cref(b)); first.join(); second.join();
    if (!success) throw std::runtime_error("Concurrent replacement failed: " + failure);
    const auto bytes = read_bounded_file(path, 8192); CHECK(bytes && (*bytes == a || *bytes == b));
    CHECK(std::distance(std::filesystem::directory_iterator(root), std::filesystem::directory_iterator{}) == 1);
    CHECK(!replace_file(root, "reject"));
    CHECK(std::distance(std::filesystem::directory_iterator(root), std::filesystem::directory_iterator{}) == 1);
    CHECK(replace_file(path, "")); CHECK(read_bounded_file(path, 0).value().empty());
}

TEST("fingerprints detect same-size changes even when modification times match") {
    const auto root = std::filesystem::temp_directory_path() / ("gatehaven-fingerprint-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    struct Cleanup { std::filesystem::path p; ~Cleanup() { std::error_code e; std::filesystem::remove_all(p, e); } } cleanup{root};
    const auto path = root / "circuit.ghv";
    CHECK(fingerprint_file(path).value() == std::nullopt);
    CHECK(replace_file(path, "first")); const auto first = fingerprint_file(path); CHECK(first && *first);
    CHECK(replace_file(path, "other")); std::filesystem::last_write_time(path, (**first).modified);
    const auto other = fingerprint_file(path); CHECK(other && *other && *other != *first);
    CHECK(!fingerprint_file(path, 4)); CHECK(!fingerprint_file(root));
    CHECK(replace_file(path, "")); CHECK(fingerprint_file(path, 0)->value().bytes == 0);
}
