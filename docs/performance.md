# Simulation measurements

Measured locally on 9 October 2026 with GCC 13.3.0, CMake 4.4.4, Linux x86-64,
and a Release (`-O3 -DNDEBUG`) build. This is a shared execution environment,
without CPU pinning or a dedicated benchmark host.

Each row uses 10,000 occupied cells, one untimed warm-up, and 30 timed ticks per
process. Five independent processes were measured for each workload. Every run
checks that the occupied and powered counts equal the requested cell count.

| Workload | Median ms/tick | Minimum | Maximum |
| --- | ---: | ---: | ---: |
| Wire chain | 4.814 | 4.080 | 5.784 |
| Wire grid, 100 columns | 6.173 | 5.571 | 6.699 |
| Isolated NOR gates | 5.385 | 5.270 | 7.134 |
| Isolated screens, each receiving 1 | 11.562 | 9.909 | 13.066 |

The screen workload exercises 10,000 independent group exchanges per tick;
it deliberately differs from a large contiguous screen group. These results
identify group discovery and repeated map construction as candidates for
profiling. They do not measure rendering, native file I/O, huge coordinate
extents, mixed interactive edits, or performance parity with another application.

Earlier single wire-chain runs in this batch measured 5.674 ms/tick before the
passive-cell/communicator fast paths and 4.004 ms/tick afterward. The repeated
measurements above show why those two samples alone are insufficient to claim
a stable percentage improvement.

```sh
cmake --preset release
cmake --build --preset release
./build/release/gatehaven-bench 10000 30 wire-chain
./build/release/gatehaven-bench 10000 30 wire-grid
./build/release/gatehaven-bench 10000 30 gates
./build/release/gatehaven-bench 10000 30 screens
```

The executable emits one JSON record per run. Arguments are bounded to 2–1,000,000
cells and 1–10,000 steps. CI checks all four workloads for correctness; it does
not enforce wall-clock thresholds on shared runners.

Next performance work should profile group discovery, cache unchanged topology,
and measure mixed circuits at larger sizes against a repeated baseline. Keep
deterministic tick results and editing invalidation tests as acceptance gates.
