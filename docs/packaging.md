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

These builds are development previews. Signing, notarization, Windows file
registration, installers, clean-machine checks, and physical desktop acceptance
remain release work. No step in this build disables operating-system protections.
