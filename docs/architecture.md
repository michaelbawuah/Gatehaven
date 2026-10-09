# Architecture

## Boundaries

- `include/gatehaven` and `src/core`: circuits, simulation, documents, and editing.
  The core has no windowing, graphics, networking, or operating-system dependency.
- `src/app`: native SDL3 presentation and input handling.
- `src/platform`: process identity, OS-backed clipboard session locks, atomic
  file replacement, and explicitly chosen communicator streams.
- `tests`: executable checks of circuit rules and document/editor behavior.
- `bench`: repeatable workloads with explicit circuit sizes and step counts.
- `samples`: circuits authored for Gatehaven.

## Decisions

Use C++23 with compiler extensions disabled. Use RAII for resources and
`std::expected` for operations that can fail during normal use. Use fixed-width
integer simulation state and ordered traversal so outcomes do not depend on hash
iteration or platform scheduling.

The circuit maintains per-element counts with every mutation. Queries for
communicators can skip the grid entirely when none exist. Passive simulation
cells skip control-input scans. The simulator still rebuilds connectivity each
tick; incremental propagation is future performance work.

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

New/Open start the resolved executable using SDL's argument-array process API;
paths never pass through a command shell. Clipboard session members keep a shared
OS file lock for their lifetime. A separate transaction lock serializes joins,
leaves, and payload replacement. The first member clears data left by an earlier
crash; the last normal member clears the session. See [the protocol](shared-clipboards.md).

UI copy/paste reads and writes a bounded stamp format. Cut removes cells only
after a successful write. Paste takes a local snapshot so transforms and incoming
copies from other windows cannot change an in-progress placement. Selection moves
clear the source and place the destination within one undo transaction.

Selections track occupied points and a frame separately: sparse holes do not
overwrite unrelated cells, while empty rectangle borders survive copy and
transform. Polylines stage their snapped segments before one transactional
commit. History revisions identify the saved state across undo, redo, and
branches instead of treating every input event as a permanent dirty flag.

Communicator groups exchange bits through an injected callback, keeping streams
and dialogs out of the simulation core. Protocol decoders and bounded ordered
queues are pure core objects; the platform layer owns stream lifetimes and
flushes output before acknowledging. Group geometry changes reset protocol
framing. See [communicator behavior](communicators.md) for file replacement,
merge/split rules, and reset semantics.

Preferences use a bounded versioned text format and atomic replacement. A failed
load leaves defaults intact. Each window keeps its own bindings and writes on
normal exit; the last successful save supplies defaults for future windows.

## Dependency policy

Pin external library versions. Keep their license texts with redistributed
binaries. Do not add another project's source files, sample circuits, icons,
fonts, screenshots, or documentation to this repository.
