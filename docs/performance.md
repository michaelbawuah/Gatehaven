# Simulation measurements

Measured on 9 October 2026 with GCC 13.3.0, CMake 4.4.4, Linux x86-64, and
Release optimization on a shared host without CPU pinning. Each workload uses
10,000 occupied cells, one startup tick, and thirty timed ticks. Values below
are medians from five independent processes. [Raw runs](../bench/results-2026-10-09-indexed.json)
retain timings and work counters.

| Workload | Startup ms | Warm ms/tick |
| --- | ---: | ---: |
| wire-chain | 1.687 | 0.001930 |
| wire-grid | 2.104 | 0.003077 |
| gates | 1.681 | 0.003142 |
| screens | 6.231 | 0.110320 |
| pulsed-screens | 6.487 | 0.097623 |
| feedback | 1.795 | 0.050207 |

Startup includes graph compilation and the first propagation. Chain, grid, and
isolated gates settle after their second tick: their warm averages include one
propagation followed by skipped work. These values are **not** the cost of
propagating every tick. Screens, pulsed screens, and feedback loops keep running
all thirty measured propagations. Pulsed screens change external input every
tick; feedback loops alternate their powered state.

The previous map-based engine measured median warm times of 4.814 ms for chains,
6.173 ms for grids, 5.385 ms for gates, and 11.562 ms for screens in the same
shared environment. These are useful historical observations, not a controlled
hardware comparison or evidence of parity with another application.

```sh
cmake --preset release
cmake --build --preset release
./build/release/gatehaven-bench 10000 30 pulsed-screens
./build/release/gatehaven-bench 10000 30 feedback
./build/release/gatehaven-cli profile samples/oscillator.ghv 10000
```

Benchmarks bound requests to 2–1,000,000 cells and 1–10,000 ticks. The CLI profiler
includes startup in total time and reports topology builds, real propagations,
and settled ticks. CI verifies all six workload invariants without enforcing
wall-clock thresholds. Differential tests, coordinate limits, copy/reset/edit
behavior, and external exchanges remain correctness gates. Rendering, hardware
touch, native dialogs, file I/O throughput, and external performance parity are
separate measurements still required for final acceptance.

## Editor work in the 0.4 candidate

Rectangular and sparse captures avoid temporary cell vectors. History coalesces
edits in a contiguous stable sort while preserving the last value at overlaps.
Pencil previews clip before allocating; polyline clicks validate lengths without
building every cell. Polyline previews only materialize visible segments.

Paste previews build an index once per copied or transformed stamp. Drawing then
queries visible rows and columns, rather than recreating the entire paste on
every frame. Actual commits still validate every cell and remain one undo action.
These changes need measurements from the editor workload benchmark; they do not
establish performance parity with an external application.
