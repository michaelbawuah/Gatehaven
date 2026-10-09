#include "gatehaven/document.hpp"
#include "gatehaven/digest.hpp"
#include "gatehaven/examples.hpp"
#include "gatehaven/simulation.hpp"
#include "gatehaven/statistics.hpp"
#include "gatehaven/svg_export.hpp"
#include "gatehaven/file_io.hpp"
#include "gatehaven/version.hpp"
#include "gatehaven/paths.hpp"
#include "gatehaven/resources.hpp"
#include "gatehaven/process.hpp"

#include <charconv>
#include <chrono>
#include <iostream>
#include <string_view>
#include <sstream>

static int run_cli(int argc, char** argv) {
    using namespace gatehaven;
    if (argc == 2 && std::string_view(argv[1]) == "--version") {
        std::cout << "Gatehaven " << version << " (C++23)\n";
        return 0;
    }
    if (argc == 2 && std::string_view(argv[1]) == "--manual-path") {
        const auto executable = current_executable();
        const auto manual = executable ? manual_path(*executable) : std::nullopt;
        if (!manual) { std::cerr << "Bundled manual was not found\n"; return 1; }
        std::cout << path_utf8(*manual) << '\n'; return std::cout ? 0 : 1;
    }
    if (argc == 2 && std::string_view(argv[1]) == "--build-info") {
        std::cout << "Gatehaven " << version << "\nCompiler: " << compiler_id << ' ' << compiler_version
                  << "\nTarget: " << target_system << ' ' << target_processor << "\nLanguage: C++23\n";
        return std::cout ? 0 : 1;
    }
    if (argc == 2 && std::string_view(argv[1]) == "examples") {
        for (const auto name : example_names) std::cout << name << '\n';
        return 0;
    }
    if ((argc == 3 || argc == 4) && std::string_view(argv[1]) == "example") {
        const auto circuit = make_example(argc == 3 ? "starter" : argv[2]);
        if (!circuit) { std::cerr << "Unknown example; use gatehaven-cli examples\n"; return 2; }
        const auto saved = save_document(utf8_path(argv[argc - 1]), *circuit);
        if (!saved) { std::cerr << saved.error().message << '\n'; return 1; }
        std::cout << "Saved example to " << argv[argc - 1] << '\n';
        return 0;
    }
    if (argc == 4 && (std::string_view(argv[1]) == "normalize" || std::string_view(argv[1]) == "convert")) {
        const auto circuit = load_document(utf8_path(argv[2]));
        if (!circuit) { std::cerr << describe_error(circuit.error()) << '\n'; return 1; }
        const auto saved = save_document(utf8_path(argv[3]), *circuit);
        if (!saved) { std::cerr << saved.error().message << '\n'; return 1; }
        return 0;
    }
    if (argc == 4 && std::string_view(argv[1]) == "svg") {
        const auto circuit = load_document(utf8_path(argv[2]));
        if (!circuit) { std::cerr << circuit.error().message << '\n'; return 1; }
        std::ostringstream svg;
        const auto rendered = export_svg(svg, *circuit);
        if (!rendered) { std::cerr << rendered.error() << '\n'; return 1; }
        const auto written = replace_file(utf8_path(argv[3]), svg.str());
        if (!written) { std::cerr << written.error() << '\n'; return 1; }
        return 0;
    }
    const std::string_view command = argc >= 2 ? argv[1] : "";
    const bool help = command == "--help" || command == "-h";
    if ((command != "check" && command != "run" && command != "stats" && command != "trace" && command != "profile" && command != "digest" && command != "state") || argc < 3 || argc > 4 ||
        ((command == "check" || command == "stats") && argc != 3)) {
        auto& output = help ? std::cout : std::cerr;
        output << "Gatehaven CLI (.ghv and .ccsb input)\n  gatehaven-cli check FILE.ghv\n"
                     "  gatehaven-cli run FILE.ghv [STEPS]\n"
                     "  gatehaven-cli stats FILE.ghv\n"
                     "  gatehaven-cli trace FILE.ghv [STEPS]\n"
                     "  gatehaven-cli profile FILE.ghv [STEPS]\n"
                     "  gatehaven-cli digest FILE.ghv [STEPS]\n"
                     "  gatehaven-cli state FILE [STEPS]\n"
                     "  gatehaven-cli svg FILE.ghv OUTPUT.svg\n"
                     "  gatehaven-cli normalize FILE OUTPUT.ghv\n"
                     "  gatehaven-cli convert INPUT OUTPUT.ghv|OUTPUT.ccsb\n"
                     "  gatehaven-cli example FILE.ghv\n"
                     "  gatehaven-cli examples\n  gatehaven-cli example NAME FILE.ghv\n"
                     "  gatehaven-cli --version\n  gatehaven-cli --build-info\n  gatehaven-cli --manual-path\n";
        return help && argc == 2 ? 0 : 2;
    }
    std::uint64_t steps = 10;
    if (argc == 4) {
        const std::string_view text(argv[3]);
        const auto result = std::from_chars(text.data(), text.data() + text.size(), steps);
        if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || steps > 1'000'000) {
            std::cerr << "Steps must be an integer from 0 through 1000000\n";
            return 2;
        }
    }
    const auto circuit = load_document(utf8_path(argv[2]));
    if (!circuit) {
        std::cerr << describe_error(circuit.error()) << '\n';
        return 1;
    }
    if (command == "stats") { write_statistics(std::cout, statistics(*circuit)); return 0; }
    if (command == "state") {
        Simulation simulation; simulation.initialize(*circuit);
        for (std::uint64_t i = 0; i < steps; ++i) simulation.step(*circuit);
        std::cout << "x,y,element,powered,conductive\n";
        simulation.visit_state([&](Point point, Power power, bool, bool) {
            std::cout << point.x << ',' << point.y << ',' << name(power.element) << ','
                      << (power.ports != 0) << ',' << simulation.conductive(point) << '\n';
        });
        return std::cout ? 0 : 1;
    }
    if (command == "digest") {
        Simulation simulation; simulation.initialize(*circuit);
        for (std::uint64_t i = 0; i < steps; ++i) simulation.step(*circuit);
        std::cout << "{\"schema\":1,\"ticks\":" << simulation.ticks() << ",\"digest\":\""
                  << state_digest(*circuit, simulation) << "\"}\n";
        return std::cout ? 0 : 1;
    }
    if (command == "profile") {
        Simulation simulation;
        const auto start = std::chrono::steady_clock::now();
        simulation.initialize(*circuit);
        const auto compiled = std::chrono::steady_clock::now();
        for (std::uint64_t i = 0; i < steps; ++i) simulation.step(*circuit);
        const auto finished = std::chrono::steady_clock::now();
        const double elapsed = std::chrono::duration<double, std::milli>(finished - start).count();
        const double compile_ms = std::chrono::duration<double, std::milli>(compiled - start).count();
        const double steps_ms = std::chrono::duration<double, std::milli>(finished - compiled).count();
        const auto& metrics = simulation.metrics();
        std::cout << "{\"cells\":" << circuit->size() << ",\"ticks\":" << simulation.ticks()
                  << ",\"powered\":" << simulation.powered_count() << ",\"total_ms\":" << elapsed
                  << ",\"compile_ms\":" << compile_ms << ",\"steps_ms\":" << steps_ms
                  << ",\"topology_builds\":" << metrics.topology_builds << ",\"propagations\":" << metrics.propagations
                  << ",\"frontier_visits\":" << metrics.frontier_visits
                  << ",\"settled_ticks\":" << metrics.settled_ticks << "}\n";
        return std::cout ? 0 : 1;
    }
    if (command == "trace") {
        constexpr std::uint64_t max_trace_rows = 5'000'000;
        if (!circuit->empty() && steps > max_trace_rows / circuit->size()) {
            std::cerr << "Trace exceeds 5000000 rows; reduce the tick count or circuit size\n"; return 2;
        }
        Simulation simulation; simulation.initialize(*circuit);
        std::cout << "tick,x,y,element,ports,sending\n";
        for (std::uint64_t i = 0; i < steps; ++i) {
            simulation.step(*circuit);
            simulation.visit_state([&](Point point, Power power, bool sending, bool) {
                std::cout << simulation.ticks() << ',' << point.x << ',' << point.y << ',' << name(power.element)
                          << ',' << static_cast<unsigned>(power.ports) << ',' << sending << '\n';
            });
        }
        return 0;
    }
    std::cout << "Valid document: " << circuit->size() << " cells\n";
    if (command == "check") return 0;
    Simulation simulation; simulation.initialize(*circuit);
    for (std::uint64_t i = 0; i < steps; ++i) simulation.step(*circuit);
    std::cout << "Ticks: " << simulation.ticks() << "\nPowered cells: " << simulation.powered_count() << '\n';
    simulation.visit_state([&](Point point, Power power, bool, bool) {
        std::cout << point.x << ' ' << point.y << ' ' << name(power.element) << ' '
                  << static_cast<unsigned>(power.ports) << '\n';
    });
    return 0;
}

#ifdef _WIN32
int wmain(int argc, wchar_t** arguments) {
    try {
        std::vector<std::string> strings; strings.reserve(static_cast<std::size_t>(argc));
        for (int i = 0; i < argc; ++i) strings.push_back(gatehaven::path_utf8(std::filesystem::path(arguments[i])));
        std::vector<char*> argv; for (auto& argument : strings) argv.push_back(argument.data());
        return run_cli(argc, argv.data());
    } catch (const std::exception& error) { std::cerr << "Gatehaven: " << error.what() << '\n'; return 1; }
}
#else
int main(int argc, char** argv) {
    try { return run_cli(argc, argv); }
    catch (const std::exception& error) { std::cerr << "Gatehaven: " << error.what() << '\n'; return 1; }
}
#endif
