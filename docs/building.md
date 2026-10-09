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

The core has no SDL dependency. Test builds require Python 3, which drives the
cross-process clipboard tests. Set `GATEHAVEN_BUILD_TESTS=OFF` for a build without
that test dependency. CMake applies high warning levels and treats
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

Desktop CI builds on Linux, macOS, and Windows use exactly SDL commit
`829a65d769d935c4852f8159e964312c0957260a` (release-3.4.18). The Linux job disables
real display backends. All three use a software renderer and dummy video driver
for automation; these runs do not establish physical display or native dialog QA.

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

The `gatehaven_snapshot` build target writes `gatehaven.bmp` in the build folder
and resolves the executable correctly for Windows configurations and macOS app
bundles. Set `SDL_VIDEODRIVER=dummy` for headless use. `gatehaven --new` opens an
empty circuit; a document path opens that file in the newly started instance.

Use `clang-format` with the repository configuration and `clang-tidy` with
`build/core/compile_commands.json`. Static-analysis CI is a remaining release gate.

## Packaging

There are no signed installers or release binaries yet. A distributable desktop
package must include dependency license texts, required shared libraries, native
file associations, and platform-specific clean-machine verification.
