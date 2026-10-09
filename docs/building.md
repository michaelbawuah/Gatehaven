# Building

## Core

Gatehaven requires C++23 and CMake 3.28 or newer. CMake 4.4.4 is the pinned CI
version. The initial local toolchain is GCC 13.3.0 on Linux; this is the available
local compiler, not a claim that it is the latest GCC release.

```sh
cmake --preset core
cmake --build --preset core
ctest --preset core
```

The core has no SDL dependency. CMake applies high warning levels and treats
project warnings as errors. C++ extensions are disabled.

## Desktop with vcpkg

Clone vcpkg outside the Gatehaven directory and pin its revision:

```sh
git clone https://github.com/microsoft/vcpkg.git ../vcpkg
git -C ../vcpkg checkout 0699a19d0c6386247ce50d4dedbb8217d484d536
```

On Linux and macOS:

```sh
../vcpkg/bootstrap-vcpkg.sh -disableMetrics
export VCPKG_ROOT="$(cd ../vcpkg && pwd)"
cmake --preset desktop
cmake --build --preset desktop
ctest --preset desktop
```

On Windows, use a Developer PowerShell with the Visual Studio C++ desktop
workload installed:

```powershell
..\vcpkg\bootstrap-vcpkg.bat -disableMetrics
$env:VCPKG_ROOT = (Resolve-Path ..\vcpkg).Path
cmake --preset desktop
cmake --build --preset desktop
ctest --preset desktop
```

The manifest pins SDL3 3.4.18. Linux desktop builds need the development packages
for the enabled X11/Wayland backends; follow vcpkg's missing-system-package
diagnostics for the distribution. File dialogs may require working desktop
portals and D-Bus. These dependencies are distinct from the core test suite.

If SDL3 3.4.18 is already installed, a package manager is unnecessary for that
local configuration:

```sh
cmake -S . -B build/installed-sdl -DGATEHAVEN_BUILD_APP=ON -DCMAKE_PREFIX_PATH=/path/to/sdl3
cmake --build build/installed-sdl --config Debug
```

The software-only Linux CI job builds exactly SDL commit
`829a65d769d935c4852f8159e964312c0957260a` (release-3.4.18), disabling real display
backends for its dummy-driver smoke test. It does not exercise X11 or Wayland.

## Tests and tools

```sh
cmake --preset sanitize
cmake --build --preset sanitize
ctest --preset sanitize

cmake --preset release
cmake --build --preset release
./build/release/gatehaven-bench 10000 20
```

The sanitizer preset enables AddressSanitizer and UndefinedBehaviorSanitizer on
non-MSVC compilers. The desktop self-test can run without a display:

```sh
SDL_VIDEODRIVER=dummy ./build/desktop/gatehaven --self-test
SDL_VIDEODRIVER=dummy ./build/desktop/gatehaven --snapshot preview.bmp
```

Use `clang-format` with the repository configuration and `clang-tidy` with
`build/core/compile_commands.json`. Static-analysis CI is a remaining release gate.

## Packaging

There are no signed installers or release binaries yet. A distributable desktop
package must include dependency license texts, required shared libraries, native
file associations, and platform-specific clean-machine verification.
