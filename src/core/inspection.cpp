#include "gatehaven/inspection.hpp"
#include "gatehaven/connectivity.hpp"

namespace gatehaven {
std::string describe_cell(const Circuit& circuit, const Simulation& simulation, Point point) {
    const auto element = circuit.at(point);
    const auto level = [](bool on) { return on ? "ON" : "OFF"; };
    std::string text = "Cell (" + std::to_string(point.x) + ", " + std::to_string(point.y) + ")\n";
    text += "Component: " + std::string(name(element)) + "\n";
    if (element == Element::empty) return text + "No component here.";
    text += "Powered: " + std::string(level(simulation.powered(point))) + "\n";
    if (element == Element::crossing) {
        text += "Horizontal: " + std::string(level((simulation.ports(point) & 10U) != 0)) + "\n";
        text += "Vertical: " + std::string(level((simulation.ports(point) & 5U) != 0)) + "\n";
    }
    if (is_relay(element)) text += "Conducting: " + std::string(level(simulation.conductive(point))) + "\n";
    if (receives_signal(element)) {
        const auto state = circuit.saved_state(point);
        const std::string kind = is_relay(element) ? "conductivity" : "output";
        text += "Stored " + kind + ": " + level((state & 2U) != 0) + "\n";
        text += "Reset " + kind + ": " + level((state & 1U) != 0) + "\n";
    }
    if (is_communicator(element)) {
        text += "Transmitting: " + std::string(level(simulation.sent(point))) + "\n";
        text += "Receiving: " + std::string(level(simulation.received(point))) + "\n";
    }
    return text + "Tick: " + std::to_string(simulation.ticks());
}
}
