#include "gatehaven/clipboard_session.hpp"

#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    const std::string argument(argv[1]);
    auto session = gatehaven::ClipboardSession::join(std::filesystem::path(std::u8string(argument.begin(), argument.end())));
    if (!session) { std::cerr << session.error() << '\n'; return 1; }
    std::cout << "READY" << std::endl;
    std::string command;
    while (std::cin >> command) {
        if (command == "QUIT") break;
        unsigned slot{};
        if (!(std::cin >> slot)) return 2;
        if (command == "SET") {
            std::int64_t value{};
            if (!(std::cin >> value)) return 2;
            const gatehaven::Stamp stamp{value + 1, 1, {{value, 0, gatehaven::Element::source}}};
            const auto written = (*session)->write(slot, stamp);
            std::cout << (written ? "OK" : "ERROR " + written.error()) << std::endl;
        } else if (command == "GET") {
            const auto stamp = (*session)->read(slot);
            if (!stamp) std::cout << "ERROR " << stamp.error() << std::endl;
            else if (stamp->cells.empty()) std::cout << "EMPTY" << std::endl;
            else std::cout << stamp->cells.front().x << std::endl;
        } else return 2;
    }
    return 0;
}
