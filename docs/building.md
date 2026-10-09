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

The core has no SDL dependency. Python 3 builds the offline manuals and drives
integration tests. `GATEHAVEN_BUILD_TESTS=OFF` skips test targets; Python remains
required for generated guides. CMake applies high warning levels and treats
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
`829a65d769d935c4852f8159e964312c0957260a` (release-3.4.18). The Linux package enables
X11 and Wayland. All three use a software renderer and dummy video driver
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
`build/core/compile_commands.json`. Targeted Clang 18 static analysis runs in CI for storage, topology, component
compilation, simulation, legacy decoding, and statistics.

## Packaging

See [portable builds](packaging.md) for CPack archives and installed-product tests.
Successful desktop CI jobs upload native preview archives, SHA-256 checksums, and
screenshots as Actions artifacts retained for 30 days. These are unsigned
development builds; signing and clean-machine release verification remain open.

## Architecture and recovery checks

The CI matrix includes Linux x86-64 and ARM64, macOS Apple Silicon and Intel,
and Windows x64/ARM64 for both core and desktop builds. A configured target is not a
passing target: inspect the Actions run for the revision you intend to use.
Signing, native dialog interaction, and physical-device acceptance require
separate checks outside the dummy-driver matrix.

Core tests include native process-kill recovery checks in addition to clipboard
concurrency. Benchmark builds add six workload checks. Installed-product tests
exercise ten distinct interface states, Unicode document paths through the CLI,
and the actual desktop executable from a directory containing spaces.

Run `gatehaven --help` or `gatehaven-cli --help` without opening a graphical
window. `gatehaven-cli profile FILE.ghv STEPS` emits JSON simulation metrics.

## Windows ARM64

CI also uses the native `windows-11-arm` runner. Both SDL and Gatehaven configure
with `-A ARM64` to avoid accidentally producing x64 binaries. The ARM64 job runs
the same core, process, desktop, and installed-product checks and publishes an
ARM64 ZIP. The NSIS installer preview currently targets Windows x64 only.

## External compatibility checks

The production targets never link the reference engine. An isolated CI job builds
a test-only adapter from the pinned external checkout and publishes round-trip,
behavior, and headless timing reports. See [the reproducible commands](reference-validation.md).
Linux desktop packages use Ubuntu 24.04 as a fixed glibc 2.39 baseline; AppImage
assembly rejects a dependency requiring newer GLIBC symbols.

Clean checkout builds record the source revision in `--build-info` and package
metadata. CMake watches ordinary and detached Git references for revision changes;
source archives without Git metadata report `unknown`. This field identifies a
checkout, not a proof that an uncommitted working tree is clean. Release builds
must use an unmodified checkout and their exact Actions run.
