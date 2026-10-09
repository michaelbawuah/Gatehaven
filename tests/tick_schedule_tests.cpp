#include "test.hpp"
#include "gatehaven/tick_schedule.hpp"
#include <limits>
using namespace gatehaven;
TEST("tick scheduling supports a thousand steps per second at ordinary frame rates") {
    TickSchedule schedule;
    unsigned ticks = 0;
    for (unsigned frame = 0; frame < 60; ++frame) ticks += schedule.due(1.0 / 60.0, 1000);
    CHECK(ticks == 1000);
    schedule.reset(); ticks = 0;
    for (unsigned frame = 0; frame < 144; ++frame) ticks += schedule.due(1.0 / 144.0, 5);
    CHECK(ticks == 5);
}
TEST("tick scheduling bounds suspension catch-up and clears fractional time on pause") {
    TickSchedule schedule;
    CHECK(schedule.due(0.1, 5) == 0);
    schedule.reset(); CHECK(schedule.due(0.1, 5) == 0);
    CHECK(schedule.due(-1, 5) == 0);
    CHECK(schedule.due(std::numeric_limits<double>::infinity(), 5) == 0);
    CHECK(schedule.due(std::numeric_limits<double>::quiet_NaN(), 5) == 0);
    CHECK(schedule.due(0.1, 5) == 1);
    CHECK(schedule.due(100, 1000) == 250);
    CHECK(schedule.due(0, 1000) == 0); // no stale debt after a stalled frame
    CHECK(schedule.due(1, 0) == 0); CHECK(schedule.due(1, 1001) == 0);
}
