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

The `share/gatehaven` folder contains lessons, manuals, Gatehaven's MIT `LICENSE`,
and dependency notices.
The Mac app also keeps these resources inside its bundle so moving the `.app`
preserves them. Windows archives include the redistributable compiler runtime.
Keep `LICENSE` and the separate dependency notices with any redistributed binaries.

On Mac, keep one clearly named copy in your Applications folder and launch that
copy. Older apps in build or test folders remain separate applications. If the
interface still shows text tiles after updating, save your work, quit that old
instance, and launch the newly installed app. Check the exact copy's revision:

```sh
"/Applications/gatehaven.app/Contents/MacOS/gatehaven" --build-info
```

Use the actual install path and app name if you placed it in your user
Applications folder or named it `Gatehaven.app`. Compare `Source revision` with
the candidate record; the visible version alone may match across preview builds.

`cmake --install build/package --prefix /chosen/location --config Release`
stages the same contents without an archive. Linux installs include desktop/MIME
metadata and a vector icon; update the desktop and MIME caches using your
distribution's normal installation process. The executable must be on `PATH`
for the desktop launcher. macOS bundles declare `.ghv` and import the `.ccsb` document type.

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
The macOS test also copies the bundle to a path containing spaces and non-ASCII
text, then checks desktop launch, bundled lessons/notices, and offline help from
another working directory. Linux extraction and macOS checks do not install into
the CI host's system directories.

Separate Linux x64 and ARM64 jobs install the actual DEB into a newly created
Ubuntu 24.04 container. They launch the app/CLI, verify the MIME cache created by
the package hooks, reinstall the same version, remove, and purge. User circuit
and settings fixtures must survive. The report records the package hash and the
resolved container image ID/digest. Run the same check with Docker:

```sh
python tools/check_deb_container.py build/app build/deb-lifecycle.json
```

This uses a disposable container, not a full desktop VM. Reinstalling the same
version does not establish upgrades from an earlier release. Native dialogs,
file-manager integration and physical hardware still need manual acceptance.

Every installer has a SHA-256 sidecar and is included in its architecture's
Actions artifact. DMGs remain unsigned and unnotarized; no build step bypasses
Gatekeeper. Clean-machine installation/removal, automatic associations, signing,
broader Linux distribution coverage remain release gates.

## Windows installer behavior

The unsigned NSIS preview installs the app, compiler runtime, lessons, notices,
and offline guides. It requires administrator approval and adds Start Menu
shortcuts plus `.ghv` and `.ccsb` Open With choices. It preserves the current default file
handler and does not change PATH. Uninstall removes only its own association;
saved circuits, preferences, shared sessions, and recovery data are retained.

Windows CI installs into a temporary path, launches both executables, checks the
quoted file-open command, reinstalls, and uninstalls. It checks that user circuit
fixtures and unknown files in the install directory survive, existing default
handler values remain unchanged, and Open With entries are recreated then
removed appropriately. This uses the disposable CI machine;
interactive clean-machine and signing acceptance still remain.

The [signing preparation guide](signing.md) covers credentialed Windows ZIP and
macOS DMG workflows. The ordinary CI pipeline never presents its unsigned previews
as signed releases.

## Linux AppImage

Linux x86-64 and ARM64 preview jobs produce an AppImage in addition to DEB/TGZ.
The image bundles Gatehaven, its C++ runtime libraries, manuals, examples, and
required dependency notices. The tool and executable runtime are pinned by
SHA-256 in `packaging/linux/appimage-tools.json`; cached downloads are rechecked.

```sh
python tools/build_appimage.py build/app build/app/Gatehaven-x86_64.AppImage
python tests/appimage_test.py build/app/Gatehaven-x86_64.AppImage
chmod +x Gatehaven-x86_64.AppImage
./Gatehaven-x86_64.AppImage
./Gatehaven-x86_64.AppImage --cli check circuit.ccsb
```

The build baseline requires glibc 2.39 or newer. Real use also requires a working
X11 or Wayland session and the system's native file-dialog services. This is not
an assertion of support for every Linux distribution. The extraction test runs
without FUSE, checks relocation and the exact SHA-256, and exercises both desktop
and CLI paths. Distribution/device acceptance is tracked separately.

Both `.ghv` and `.ccsb` are advertised on Linux/macOS and registered as Open With
choices by the Windows installer. Installers preserve the user's existing default
application. AppImage desktop integration depends on the user's desktop tooling.

The AppImage JSON sidecar includes the exact artifact hash, pinned assembly tools,
required GLIBC symbol floor, and the build information read from both packaged
executables. These metadata are checked before publication. They establish
artifact identity, not a signature or clean-machine support claim.
