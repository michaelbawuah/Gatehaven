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

A C++23 compiler and CMake 3.28 or later are required. The core has no external
dependencies. The initial local build uses GCC 13.3.0 and CMake 4.4.4.

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
| Draw / erase | Left / right mouse drag |
| Pan / zoom | Middle mouse drag / scroll wheel |
| Play or pause / single step | Space / Right arrow |
| Reset / frame circuit | R / F |
| Select a region | Q, then drag |
| Copy / cut / paste | Ctrl+C / Ctrl+X / Ctrl+V |
| Undo / redo | Ctrl+Z / Ctrl+Y |
| Rotate / flip selection or clipboard | [ and ] / H and V |
| Choose one of ten local clipboards | Ctrl+0 through Ctrl+9 |
| Save / Save As / Open / New | Ctrl+S / Ctrl+Shift+S / Ctrl+O / Ctrl+N |
| Show keyboard help | B |

Command can replace Ctrl on macOS. Undo records one drawing stroke as one edit.
Gate and relay pencils place a Signal when clicked on an existing cell of the
same type. Native documents use `.ghv`; `.ccsb` import is still planned.

## Under the hood

- A sparse circuit grid indexes occupied rows and columns for viewport queries.
- Two-phase simulation reads gate inputs from the previous tick before
  propagating power. Crossing wires keep horizontal and vertical channels separate.
- The simulation library is independent of SDL and runs headlessly in tests.
- Documents are validated before loading. Saves replace the destination only
  after a temporary file has been written and closed successfully.
- Desktop controls have an automated smoke test using SDL's event queue.

[Architecture](docs/architecture.md) · [Roadmap](docs/roadmap.md) ·
[Document format](docs/document-format.md) · [Verification](docs/verification.md)

The application source does not yet have a selected public license.
External library notices are retained in `third_party/` and must accompany
redistributed dependency code.
