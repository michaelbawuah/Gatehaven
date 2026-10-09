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

## Active-input and desktop measurements, 9 October 2026

The active comparison injects the same deterministic screen holds into both
engines, verifies their final states, and alternates seven fresh-process runs of
10,000 ticks for every sample. GCC 13.3.0 Release builds on the same Linux x86-64
host produced these medians in milliseconds:

| Sample | Reference compile | Gatehaven compile | Reference ticks | Gatehaven ticks |
| --- | ---: | ---: | ---: | ---: |
| clock | 0.047141 | 0.010786 | 1.530380 | 0.421544 |
| clock_minimal | 0.020601 | 0.006259 | 1.453540 | 0.292740 |
| double dabble with inputs | 20.012700 | 5.660480 | 392.603000 | 368.299000 |
| duplicator | 0.127732 | 0.031277 | 1.468970 | 0.260011 |
| propagate | 0.198168 | 0.078358 | 3.750940 | 1.595390 |
| readfile_multi | 3.894570 | 1.324350 | 30.770200 | 25.382700 |

All six medians improved in this workload. File ports in these engine timings
are disconnected. See [raw runs](../bench/reference-active-results-2026-10-09.json)
for input hashes, correctness, host, toolchain and candidate source tree.

The separate [live endpoint measurement](../bench/endpoint-results-2026-10-09.json)
uses two 10,000-cell file groups and transfers 512 bytes in each direction,
checking every byte and acknowledgement. Five alternating runs measured median
routing-plus-stream time of **0.575154 ms cached** and **444.718 ms uncached**.
This large synthetic group isolates the cost of repeated membership scans; it
is not a general disk speedup or an external-engine comparison.

The actual desktop renderer now reports static, pan and zoom frame timings and
visible-cell counts. The culling test adds 20,000 off-screen cells and verifies
that rendered cell counts stay unchanged. The local 100,000-extra-cell Debug
experiment measured roughly 13–15% lower median frame time after batching text
rectangles. Its before/after help BMP files are byte-identical. This is a software
renderer experiment, not physical input-to-photon latency. CI records fresh
Release measurements on all six native targets.

```sh
python bench/compare_reference.py build/reference-adapter build/release/gatehaven_screen_peer /path/to/samples --screens
python bench/measure_endpoints.py build/release/gatehaven-io-bench
SDL_VIDEODRIVER=dummy build/app/gatehaven --benchmark-render 100000 60
```

[Raw render frames](../bench/rendering-results-2026-10-09.json) retain the
source trees, frame distributions, build type and identical-image hashes. Native
GPU rendering, high-DPI displays and physical interaction still need acceptance.
