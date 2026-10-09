#include "test.hpp"
#include "gatehaven/recovery.hpp"
#include "gatehaven/file_io.hpp"
#include <chrono>
using namespace gatehaven;
namespace {
struct RecoveryDirectory {
    std::filesystem::path path = std::filesystem::temp_directory_path() / ("gatehaven-recovery-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ~RecoveryDirectory() { std::error_code error; std::filesystem::remove_all(path, error); }
};
}
TEST("recovery ignores live windows and takes ownership of abandoned snapshots") {
    RecoveryDirectory directory;
    auto writer = RecoveryStore::open(directory.path).value(); auto reader = RecoveryStore::open(directory.path).value();
    Circuit circuit; circuit.set({-3, 4}, Element::file_input); circuit.set({5, -6}, Element::source);
    CHECK(writer->write(circuit)); CHECK(reader->scan()->empty());
    const auto identity = writer->id(); CHECK(!reader->restore(identity)); CHECK(!reader->remove(identity));
    writer.reset();
    const auto entries = reader->scan(); CHECK(entries && entries->size() == 1 && entries->front().id == identity);
    const auto restored = reader->restore(identity); CHECK(restored && *restored == circuit);
    CHECK(reader->scan()->empty());
    const auto second_identity = reader->id(); reader.reset();
    auto last = RecoveryStore::open(directory.path).value();
    CHECK(last->restore(second_identity).value() == circuit); CHECK(last->discard());
    last.reset();
    CHECK(RecoveryStore::open(directory.path).value()->scan()->empty());
}

TEST("failed and malformed restores preserve the last recoverable copy") {
    RecoveryDirectory directory;
    auto writer = RecoveryStore::open(directory.path).value(); Circuit circuit; circuit.set({0, 0}, Element::source);
    CHECK(writer->write(circuit)); const auto identity = writer->id(); writer.reset();
    auto reader = RecoveryStore::open(directory.path).value();
    std::filesystem::create_directory(directory.path / (reader->id() + ".ghv"));
    CHECK(!reader->restore(identity)); CHECK(reader->scan()->size() == 1);
    std::filesystem::remove(directory.path / (reader->id() + ".ghv"));
    CHECK(!reader->restore("../outside")); CHECK(!reader->remove("session-../../outside"));
    CHECK(replace_file(directory.path / (identity + ".ghv"), "malformed"));
    CHECK(!reader->restore(identity)); CHECK(reader->scan()->size() == 1);
    CHECK(reader->remove(identity)); CHECK(reader->scan()->empty());
}
