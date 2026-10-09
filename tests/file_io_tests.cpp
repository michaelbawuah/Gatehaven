#include "test.hpp"
#include "gatehaven/file_io.hpp"
#include <chrono>
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
