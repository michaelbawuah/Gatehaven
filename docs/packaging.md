# Portable builds

Gatehaven can produce an unsigned development archive with CPack. Use a **static**
SDL 3.4.18 build (`SDL_SHARED=OFF`, `SDL_STATIC=ON`) so the archive does not depend
on an SDL installation. The operating system's normal system libraries are still
required. Linux binaries target the distribution and architecture used to build
them; they are not universal Linux packages.

```sh
cmake -S . -B build/package -DGATEHAVEN_BUILD_APP=ON \
  -DGATEHAVEN_BUILD_PACKAGES=ON -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/path/to/static-sdl3
cmake --build build/package --config Release
ctest --test-dir build/package -C Release --output-on-failure
cmake --build build/package --config Release --target package
```

The build directory receives a ZIP on Windows/macOS or `.tar.gz` on Linux and a
SHA-256 checksum file. Extract the whole archive before launching:

| Platform | Application | Command-line utility |
| --- | --- | --- |
| Windows | `bin/gatehaven.exe` | `bin/gatehaven-cli.exe` |
| macOS | `gatehaven.app` | `bin/gatehaven-cli` |
| Linux | `bin/gatehaven` | `bin/gatehaven-cli` |

The `share/gatehaven` folder contains lessons, manuals, and dependency notices.
The Mac app also keeps these resources inside its bundle so moving the `.app`
preserves them. Windows archives include the redistributable compiler runtime.
Keep dependency notices with any redistributed binaries.

`cmake --install build/package --prefix /chosen/location --config Release`
stages the same contents without an archive. Linux installs include desktop/MIME
metadata and a vector icon; update the desktop and MIME caches using your
distribution's normal installation process. The executable must be on `PATH`
for the desktop launcher. macOS bundles declare the `.ghv` document type.

These builds are development previews. Signing, notarization, final installer acceptance, clean-machine checks, and physical desktop acceptance
remain release work. No step in this build disables operating-system protections.

## Optional native installer previews

Add `-DGATEHAVEN_BUILD_INSTALLERS=ON` with `GATEHAVEN_BUILD_PACKAGES=ON` to
produce a Debian `.deb` on Linux, a `.dmg` on macOS, or an NSIS `.exe`
on Windows alongside the archive. These options require platform packaging
tools: `dpkg-deb`/`dpkg-shlibdeps` on Debian-based Linux, `hdiutil` on macOS,
and NSIS 3.13 on Windows.

DEBs install under `/usr`, declare the runtime dependencies discovered from
built binaries, and include launcher/MIME resources. Install only the package
for your architecture and a compatible distribution. CI extracts the package
into a temporary directory, checks metadata/resources, and executes the CLI.
DMGs are verified, mounted read-only, inspected, and unmounted by the test.
These checks do not install into the CI host's system directories.

Every installer has a SHA-256 sidecar and is included in its architecture's
Actions artifact. DMGs remain unsigned and unnotarized; no build step bypasses
Gatekeeper. Clean-machine installation/removal, automatic associations, signing,
broader Linux distribution coverage remain release gates.

## Windows installer behavior

The unsigned NSIS preview installs the app, compiler runtime, lessons, notices,
and offline guides. It requires administrator approval and adds Start Menu
shortcuts plus a `.ghv` Open With choice. It preserves the current default file
handler and does not change PATH. Uninstall removes only its own association;
saved circuits, preferences, shared sessions, and recovery data are retained.

Windows CI installs into a temporary path, launches both executables, checks the
quoted file-open command, and uninstalls. This uses the disposable CI machine;
interactive clean-machine and signing acceptance still remain.
