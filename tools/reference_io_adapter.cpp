// Test-only public-API bridge to a separately supplied, pinned checkout.
#include "../tests/active_io_scenarios.hpp"
#include "fileinputcommunicator.hpp"
#include "fileoutputcommunicator.hpp"
struct Peer {
    FileInputCommunicator in{};
    FileOutputCommunicator out{};
    Peer() { in.reset(); out.reset(); }
    void choose_input(const std::filesystem::path& path) { in.setFile(path.string().c_str()); }
    void choose_output(const std::filesystem::path& path) { out.setFile(path.string().c_str()); }
    bool input(bool bit) { in.transmit(bit); return in.receive(); }
    bool output(bool bit) { out.transmit(bit); return out.receive(); }
};
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    try { return active_io::run<Peer>(argv[1]); }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
