#include "active_io_scenarios.hpp"
#include "gatehaven/file_endpoints.hpp"

struct Peer {
    gatehaven::FileEndpoints endpoints;
    const gatehaven::CommunicatorGroup in{{0, 0}, gatehaven::Element::file_input, {{0, 0}}};
    const gatehaven::CommunicatorGroup out{{2, 0}, gatehaven::Element::file_output, {{2, 0}}};
    void choose_input(const std::filesystem::path& path) { active_io::require(endpoints.choose_input(in.id, path).has_value(), "input choice failed"); }
    void choose_output(const std::filesystem::path& path) { active_io::require(endpoints.choose_output(out.id, path).has_value(), "output choice failed"); }
    bool input(bool bit) { return endpoints.exchange(in, bit); }
    bool output(bool bit) { return endpoints.exchange(out, bit); }
};
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    try { return active_io::run<Peer>(argv[1]); }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
