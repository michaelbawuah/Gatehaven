#include "test.hpp"
#include "gatehaven/document.hpp"

#include <chrono>
#include <fstream>
#include <sstream>

using namespace gatehaven;

TEST("native documents round trip every element in deterministic order") {
    Circuit c;
    for (std::size_t i = 1; i < element_names.size(); ++i) {
        c.set({-static_cast<Coordinate>(i), static_cast<Coordinate>(i)}, static_cast<Element>(i));
    }
    std::ostringstream bytes;
    CHECK(write_document(bytes, c));
    std::istringstream input(bytes.str());
    const auto loaded = read_document(input);
    CHECK(loaded);
    CHECK(*loaded == c);
    std::ostringstream again;
    CHECK(write_document(again, *loaded));
    CHECK(bytes.str() == again.str());
}

TEST("parser rejects malformed or ambiguous documents") {
    for (const auto text : {"", "GATEHAVEN 3\n", "GATEHAVEN 1\n0 0 potato\n",
                            "GATEHAVEN 1\n0 0 empty\n", "GATEHAVEN 1\n0 0 wire extra\n",
                            "GATEHAVEN 1\n0 0 wire\n0 0 source\n",
                            "GATEHAVEN 1\n2147483648 0 wire\n",
                            "GATEHAVEN 1\n0x1 0 wire\n", "GATEHAVEN 1\n0 wire\n"}) {
        std::istringstream input(text);
        CHECK(!read_document(input));
    }
}

TEST("parser accepts comments CRLF and final lines without a newline") {
    std::istringstream input("GATEHAVEN 1\r\n# hello\r\n\r\n-2 7 source");
    const auto loaded = read_document(input);
    CHECK(loaded);
    CHECK(loaded->at({-2, 7}) == Element::source);
}

TEST("document limits reject oversized input before building an unbounded circuit") {
    std::istringstream cells("GATEHAVEN 1\n0 0 wire\n1 0 wire\n");
    CHECK(!read_document(cells, {.max_cells = 1}));
    std::istringstream long_line("GATEHAVEN 1\n" + std::string(200, ' ') + "\n");
    CHECK(!read_document(long_line));
    std::istringstream comments("GATEHAVEN 1\n# one\n# two\n");
    CHECK(!read_document(comments, {.max_lines = 2}));
}

TEST("saving replaces existing documents and removes temporary files") {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto dir = std::filesystem::temp_directory_path() / ("gatehaven-test-" + std::to_string(stamp));
    std::filesystem::create_directory(dir);
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() { std::error_code error; std::filesystem::remove_all(path, error); }
    } cleanup{dir};
    const auto path = dir / "circuit.ghv";
    Circuit c;
    c.set({2, 3}, Element::source);
    CHECK(save_document(path, c));
    c.set({4, 5}, Element::and_gate);
    CHECK(save_document(path, c));
    const auto loaded = load_document(path);
    CHECK(loaded && *loaded == c);
    CHECK(std::distance(std::filesystem::directory_iterator(dir), std::filesystem::directory_iterator{}) == 1);
    CHECK(!save_document(dir / "missing" / "circuit.ghv", c));
    CHECK(!load_document(dir / "missing.ghv"));
}

TEST("line limits count the mandatory document header") {
    std::istringstream zero("GATEHAVEN 1\n"); CHECK(!read_document(zero, {.max_lines = 0}));
    std::istringstream one("GATEHAVEN 1\n"); CHECK(read_document(one, {.max_lines = 1}));
    std::istringstream two("GATEHAVEN 1\n# comment\n"); CHECK(!read_document(two, {.max_lines = 1}));
}

TEST("document loader rejects directories before opening an input stream") {
    const auto result = load_document(std::filesystem::temp_directory_path());
    CHECK(!result && result.error().line == 0);
    CHECK(result.error().message.find("regular file") != std::string::npos);
}

TEST("UTF-8 text editor BOMs are accepted only before the document header") {
    std::istringstream input("\xEF\xBB\xBF" "GATEHAVEN 1\r\n0 0 source\r\n");
    CHECK(read_document(input)->at({0, 0}) == Element::source);
    std::istringstream bad("GATEHAVEN 1\n\xEF\xBB\xBF" "0 0 source\n");
    CHECK(!read_document(bad));
}

TEST("native revision two preserves state and rejects ambiguous state columns") {
    Circuit circuit; circuit.set({-3, 9}, Element::nand_gate, 3);
    std::ostringstream bytes; CHECK(write_document(bytes, circuit));
    CHECK(bytes.str() == "GATEHAVEN 2\n-3 9 nand 3\n");
    std::istringstream input(bytes.str()); CHECK(read_document(input).value() == circuit);
    for (const auto text : {"GATEHAVEN 2\n0 0 wire\n", "GATEHAVEN 2\n0 0 wire 4\n",
                            "GATEHAVEN 2\n0 0 wire -1\n", "GATEHAVEN 1\n0 0 wire 0\n",
                            "GATEHAVEN 2\n0 0 wire 0 extra\n"}) {
        std::istringstream invalid(text); CHECK(!read_document(invalid));
    }
}
