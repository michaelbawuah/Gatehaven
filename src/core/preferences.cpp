#include "gatehaven/preferences.hpp"
#include <sstream>

namespace gatehaven {
std::string encode_preferences(const Preferences& preferences) {
    std::ostringstream out;
    out << "GATEHAVEN-PREFERENCES 1\n" << preferences.speed << ' ' << preferences.beginner << '\n';
    for (const auto& binding : preferences.bindings) {
        out << static_cast<unsigned>(binding.kind) << ' ' << name(binding.element) << '\n';
    }
    return out.str();
}
std::expected<Preferences, std::string> decode_preferences(std::string_view text) {
    if (text.size() > 4096) return std::unexpected("Preferences exceed size limit");
    std::istringstream in{std::string(text)};
    std::string magic; unsigned version{}, beginner{};
    Preferences result;
    if (!(in >> magic >> version >> result.speed >> beginner) || magic != "GATEHAVEN-PREFERENCES" || version != 1 ||
        result.speed < 1 || result.speed > 1000 || beginner > 1) return std::unexpected("Invalid preferences header");
    result.beginner = beginner != 0;
    for (auto& binding : result.bindings) {
        unsigned kind{}; std::string element;
        if (!(in >> kind >> element) || kind > static_cast<unsigned>(ToolKind::interactor)) return std::unexpected("Invalid tool binding");
        const auto parsed = parse_element(element);
        if (!parsed || (kind == 0 && *parsed == Element::empty)) return std::unexpected("Invalid pencil element");
        binding = {static_cast<ToolKind>(kind), *parsed};
    }
    std::string trailing;
    if (in >> trailing) return std::unexpected("Unexpected preferences data");
    return result;
}
}
