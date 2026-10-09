#include "gatehaven/document.hpp"
#include "gatehaven/detail/replace_path.hpp"

#include <charconv>
#include <chrono>
#include <fstream>
#include <sstream>
#include <system_error>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace gatehaven {
namespace {

std::expected<bool, DocumentError> read_line(std::istream& input, std::string& line,
                                           std::size_t number, std::size_t limit) {
    line.clear();
    char ch{};
    while (input.get(ch)) {
        if (ch == '\n') break;
        if (line.size() >= limit) return std::unexpected(DocumentError{number, "Line is too long"});
        line.push_back(ch);
    }
    if (input.bad() || (input.fail() && !input.eof())) {
        return std::unexpected(DocumentError{number, "Could not read document"});
    }
    if (!line.empty() && line.back() == '\r') line.pop_back();
    return !input.eof() || !line.empty();
}

std::optional<Coordinate> coordinate(const std::string& text) {
    Coordinate result{};
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) return std::nullopt;
    return result;
}

struct RemoveTemporary {
    std::filesystem::path path;
    ~RemoveTemporary() {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
    }
};

} // namespace

std::expected<Circuit, DocumentError> read_document(std::istream& input, DocumentLimits limits) {
    if (limits.max_lines == 0) return std::unexpected(DocumentError{1, "Document header exceeds line limit"});
    Circuit circuit;
    std::string line;
    auto result = read_line(input, line, 1, limits.max_line_bytes);
    if (!result) return std::unexpected(result.error());
    if (!*result || line != "GATEHAVEN 1") {
        return std::unexpected(DocumentError{1, "Expected GATEHAVEN 1 document header"});
    }
    for (std::size_t number = 2;; ++number) {
        result = read_line(input, line, number, limits.max_line_bytes);
        if (!result) return std::unexpected(result.error());
        if (!*result) break;
        if (number > limits.max_lines) return std::unexpected(DocumentError{number, "Too many lines"});
        const auto start = line.find_first_not_of(" \t");
        if (start == std::string::npos || line[start] == '#') continue;
        std::istringstream fields(line);
        std::string x_text, y_text, element_text, extra;
        if (!(fields >> x_text >> y_text >> element_text) || (fields >> extra)) {
            return std::unexpected(DocumentError{number, "Expected x y element"});
        }
        const auto x = coordinate(x_text);
        const auto y = coordinate(y_text);
        const auto element = parse_element(element_text);
        if (!x || !y) return std::unexpected(DocumentError{number, "Coordinate is invalid or out of range"});
        if (!element || *element == Element::empty) {
            return std::unexpected(DocumentError{number, "Unknown or empty element"});
        }
        if (circuit.at({*x, *y}) != Element::empty) {
            return std::unexpected(DocumentError{number, "Duplicate cell coordinate"});
        }
        if (circuit.size() >= limits.max_cells) {
            return std::unexpected(DocumentError{number, "Document exceeds cell limit"});
        }
        circuit.set({*x, *y}, *element);
    }
    return circuit;
}

std::expected<void, DocumentError> write_document(std::ostream& output, const Circuit& circuit) {
    output << "GATEHAVEN 1\n";
    circuit.visit({{std::numeric_limits<Coordinate>::min(), std::numeric_limits<Coordinate>::min()},
                   {std::numeric_limits<Coordinate>::max(), std::numeric_limits<Coordinate>::max()}}, [&](const Cell& cell) {
        // to_chars through to_string is independent of the stream's number locale.
        output << std::to_string(cell.position.x) << ' ' << std::to_string(cell.position.y)
               << ' ' << name(cell.element) << '\n';
    });
    if (!output) return std::unexpected(DocumentError{0, "Could not write document"});
    return {};
}

std::expected<Circuit, DocumentError> load_document(const std::filesystem::path& path,
                                                  DocumentLimits limits) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return std::unexpected(DocumentError{0, "Could not open document"});
    return read_document(input, limits);
}

std::expected<void, DocumentError> save_document(const std::filesystem::path& path,
                                                const Circuit& circuit) {
    // Write beside the target so replacement stays on the same filesystem.
    // noreplace prevents collisions from truncating another temporary file.
    const auto stamp = detail::temporary_token();
    for (unsigned attempt = 0; attempt < 32; ++attempt) {
        auto temporary = path;
        temporary += ".part-" + stamp + "-" + std::to_string(attempt);
        std::ofstream output(temporary, std::ios::binary | std::ios::out | std::ios::noreplace);
        if (!output) continue;
        RemoveTemporary cleanup{temporary};
        const auto written = write_document(output, circuit);
        output.flush();
        const bool flushed = static_cast<bool>(output);
        output.close();
        if (!written) return std::unexpected(written.error());
        if (!flushed || output.fail()) {
            return std::unexpected(DocumentError{0, "Could not finish writing document"});
        }
        if (const auto error = detail::replace_path(temporary, path)) {
            return std::unexpected(DocumentError{0, "Could not replace document: " + error.message()});
        }
        return {};
    }
    return std::unexpected(DocumentError{0, "Could not create a temporary file beside the destination"});
}

} // namespace gatehaven
