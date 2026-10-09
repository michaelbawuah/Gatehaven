# Architecture

## Boundaries

- `include/gatehaven` and `src/core`: circuits, simulation, documents, and editing.
  The core has no windowing, graphics, networking, or operating-system dependency.
- `src/app`: native SDL3 presentation and input handling.
- `tests`: executable checks of circuit rules and document/editor behavior.
- `bench`: repeatable workloads with explicit circuit sizes and step counts.
- `samples`: circuits authored for Gatehaven.

## Decisions

Use C++23 with compiler extensions disabled. Use RAII for resources and
`std::expected` for operations that can fail during normal use. Use fixed-width
integer simulation state and ordered traversal so outcomes do not depend on hash
iteration or platform scheduling.

Store occupied cells only. Empty space should consume no per-cell memory. Start
with a straightforward ordered sparse representation and establish correctness
before adding chunking or incremental connectivity caches.

Simulation advances in two phases: evaluate gate/relay controls from the previous
step, then propagate power across the resulting conductive networks. Crossing
wires have separate horizontal and vertical channels. Signals mark gate inputs;
direct adjacency does not make a gate output feed its own input.

Keep application mutation and simulation on the main thread initially. This
avoids racing editor changes against simulation snapshots. Worker threads will
need an explicit ownership model and measured benefit before being introduced.

Native documents contain circuit structure, not machine-dependent pointers or
running simulation state. Loading must either produce a complete validated
document or leave the current circuit unchanged.

## Dependency policy

Pin external library versions. Keep their license texts with redistributed
binaries. Do not add another project's source files, sample circuits, icons,
fonts, screenshots, or documentation to this repository.
