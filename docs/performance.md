# Simulation measurements

The first tables below are historical pre-0.5 measurements. The component-engine
and external comparison sections describe the 0.5 candidate.

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

### Measured editor workload, 9 October 2026

Five fresh Release processes on the same shared Linux x64 host (GCC 13.3.0),
each using 100,000 cells. Median times from `results-2026-10-09-editor.json`:

| Operation | Median milliseconds |
| --- | ---: |
| Apply one 100,000-cell stroke | 29.8930 |
| Capture the circuit | 4.5219 |
| Build paste-preview index | 11.7727 |
| 1,000 clipped preview queries, ten cells each | 0.0819 |
| Undo then redo the stroke | 33.3263 |

Every run verifies exactly 10,000 preview visits and full undo/redo restoration.
Preview queries exclude index construction and SDL drawing; these are component
measurements, not an end-to-end frame-rate claim. Concurrent host load varied.
No earlier editor benchmark or external application baseline was recorded, so
these results establish a reproducible starting point rather than a speedup ratio.

## Component propagation

The engine now groups fixed wire connectivity into electrical components.
Crossings retain two terminals, and relays remain independently switched
vertices. Tick work visits controls and energized component/relay edges; full
per-cell state is expanded only when rendering or exporting observations.
`frontier_visits` therefore counts energized graph vertices in this version,
not individual grid cells. Historical counts use the earlier unit.

## 0.5 external headless comparison

Seven fresh processes per engine and sample, alternating engine order, each with
10,000 timed ticks. Both use GNU 13.3.0, C++23, `-O3 -DNDEBUG` on the same Linux
x86-64 shared host. Compilation and ticks are measured separately, excluding file
parsing, snapshots, and output formatting. Every final state is compared before
timing. [Raw runs](../bench/reference-results-2026-10-09.json) include source-tree
identity, input hashes, compiler, work counters, and adapter identity.

| Sample | Reference compile ms | Gatehaven compile ms | Reference 10k ticks ms | Gatehaven 10k ticks ms |
| --- | ---: | ---: | ---: | ---: |
| clock.ccsb | 0.050656 | 0.012999 | 1.526730 | 0.470497 |
| clock_minimal.ccsb | 0.021622 | 0.007882 | 1.353080 | 0.359882 |
| double dabble with inputs.ccsb | 19.371000 | 5.686340 | 274.610000 | 0.103516 |
| duplicator.ccsb | 0.131908 | 0.032859 | 1.409420 | 0.076325 |
| propagate.ccsb | 0.201553 | 0.076896 | 2.006070 | 0.076294 |
| readfile_multi.ccsb | 4.191820 | 1.325350 | 16.830400 | 0.081582 |

All six local medians are lower for both compilation and ticks. The largest tick
reductions come from suspended work after the circuits settle with their file
ports disconnected. They do not measure full recomputation on every tick.
Live endpoint callbacks wake the engine and always execute when present.

The reference revision is pinned in [the comparison procedure](reference-validation.md).
Shared-host timings can vary, so CI uploads measurements without a wall-clock
pass/fail threshold. Local `--require-no-regression` can enforce the measured
scenario. Rendering, continuously changing external inputs, attached streams,
and physical UI latency remain necessary before claiming overall parity.
