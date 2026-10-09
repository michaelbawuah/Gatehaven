# Gatehaven

**Build circuits. Bring logic to life.**

[![Build and test](https://github.com/michaelbawuah/Gatehaven/actions/workflows/ci.yml/badge.svg)](https://github.com/michaelbawuah/Gatehaven/actions/workflows/ci.yml)

Gatehaven is a digital logic sandbox built in C++23. Connect wires, combine
logic gates, and step through a circuit to see how its signals change.

![Gatehaven desktop prototype](docs/images/desktop-prototype.png)

**Status: early desktop prototype, not a finished release.** Windows, macOS,
and Linux are the intended platforms. See [verification notes](docs/verification.md)
for exactly what has been tested.

## Try the core

A C++23 compiler and CMake 3.28 or later are required. The simulation core has no
external dependencies. Tests also require Python 3 for the cross-process checks.
The initial local build uses GCC 13.3.0 and CMake 4.4.4.

```sh
git clone https://github.com/michaelbawuah/Gatehaven.git
cd Gatehaven
cmake -S . -B build/core -DCMAKE_BUILD_TYPE=Debug
cmake --build build/core --config Debug
ctest --test-dir build/core -C Debug --output-on-failure
```

On Linux and macOS:

```sh
./build/core/gatehaven-cli check samples/starter.ghv
./build/core/gatehaven-cli run samples/oscillator.ghv 10
```

With Visual Studio's multi-configuration generator, executables are under
`build/core/Debug/` and have an `.exe` suffix.

## Play the desktop prototype

The application uses SDL **3.4.18**, pinned in `vcpkg.json`. Set up vcpkg as
described in [the build guide](docs/building.md), then run:

```sh
cmake --preset desktop
cmake --build --preset desktop
ctest --preset desktop
```

Launch `build/desktop/gatehaven` on Linux, `build/desktop/Debug/gatehaven.exe`
with Visual Studio, or `build/desktop/gatehaven.app` on macOS. Generator-specific
configuration subdirectories may apply. The starter circuit opens automatically.

| Action | Input |
| --- | --- |
| Choose a component | Sidebar or digits 1 through 0 |
| Bind a tool | Click its sidebar name with the button to bind |
| Draw / erase (default bindings) | Left / right mouse drag |
| Pan / zoom (default bindings) | Middle mouse drag / scroll wheel |
| Sample a component tool | Hold E and click with the button to bind |
| Play or pause / single step | Space / Right arrow when no selection is active |
| Set simulation speed | Ctrl+Space or the speed button; 1–1,000 ticks/s |
| Reset / frame circuit | R / F |
| Select a region | Q, then drag |
| Move a selection | Arrow keys; hold Ctrl to move four cells |
| Copy / cut / paste | Ctrl+C / Ctrl+X / Ctrl+V |
| Copy / cut / paste with a chosen shared clipboard | Ctrl+Shift+C / Ctrl+Shift+X / Ctrl+Shift+V, then 0–9 |
| Undo / redo | Ctrl+Z / Ctrl+Y |
| Rotate / flip selection or clipboard | [ and ] / H and V |
| Save / Save As / Open / New | Ctrl+S / Ctrl+Shift+S / Ctrl+O / Ctrl+N |
| Show keyboard help | B |

Command can replace Ctrl on macOS. Undo records one drawing stroke as one edit.
Gate and relay pencils place a Signal when clicked on an existing cell of the
same type. Native documents use `.ghv`; `.ccsb` import is still planned.

New and Open launch separate Gatehaven instances and preserve the current
document. All running instances share ten session clipboards. Ordinary copy,
cut, and paste use slot 0, which follows the last clipboard used. The clipboard
chooser selects a particular slot; an active paste preview stays unchanged if
another window updates it. Closing the last instance clears clipboard contents.
After an abrupt shutdown of every instance, the next launch clears abandoned data.

The binding markers identify left (red), right (blue), middle (green), X1 (cyan),
X2 (magenta), and touch (yellow). Button bindings are kept for the current window;
saved preferences and full multitouch navigation are still planned.

## Under the hood

- A sparse circuit grid indexes occupied rows and columns for viewport queries.
- Two-phase simulation reads gate inputs from the previous tick before
  propagating power. Crossing wires keep horizontal and vertical channels separate.
- The simulation library is independent of SDL and runs headlessly in tests.
- Documents are validated before loading. Saves replace the destination only
  after a temporary file has been written and closed successfully.
- Desktop controls have an automated smoke test using SDL's event queue.
- Shared clipboard tests run separate processes, exchange data concurrently,
  terminate peers, and verify session cleanup and recovery.

[Architecture](docs/architecture.md) · [Roadmap](docs/roadmap.md) ·
[Document format](docs/document-format.md) · [Shared clipboards](docs/shared-clipboards.md) ·
[Verification](docs/verification.md)

The application source does not yet have a selected public license.
External library notices are retained in `third_party/` and must accompany
redistributed dependency code.
