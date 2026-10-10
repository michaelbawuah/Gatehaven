# Gatehaven 0.5 Preview 1

Status: **prepared unsigned development preview; public release upload pending**.
Planned tag: `v0.5.0-preview.1`.
Candidate: `02d810feccd9836fa0a07aab2757d91cf5ef0b30`.

This candidate includes MIT licensing for Gatehaven's original code and
documentation, with the full license and dependency notices inside the packages.
It also includes the accessible contrast and speed controls, gate artwork, and
native zoom test synchronization. The executable version is `0.5.0-dev`.

[CI run 38036451768](https://github.com/michaelbawuah/Gatehaven/actions/runs/38036451768)
passed all 18 jobs. Six native artifacts were downloaded and checked against
GitHub's artifact digests. All 13 enclosed application packages matched their
SHA-256 sidecars. Every portable archive's metadata identifies this exact
candidate; its MIT license matches the repository, including the copy within
each Mac app bundle. Linux AppImages require glibc 2.39 or newer and a working
X11 or Wayland desktop; they are not universal Linux binaries.

Until public release upload is complete, the native artifacts remain available
from that CI run to signed-in GitHub users. They expire on 9 November 2026.

| Computer | Artifact | Easiest portable package |
| --- | --- | --- |
| Apple Silicon Mac | `Gatehaven-macOS-ARM64` | `Gatehaven-0.5.0-Darwin-arm64.zip` |
| Intel Mac | `Gatehaven-macOS-X64` | `Gatehaven-0.5.0-Darwin-x86_64.zip` |
| Windows x64 | `Gatehaven-Windows-X64` | `Gatehaven-0.5.0-Windows-AMD64.zip` |
| Windows ARM64 | `Gatehaven-Windows-ARM64` | `Gatehaven-0.5.0-Windows-ARM64.zip` |
| Linux x86-64 | `Gatehaven-Linux-X64` | `Gatehaven-X64.AppImage` |
| Linux ARM64 | `Gatehaven-Linux-ARM64` | `Gatehaven-ARM64.AppImage` |

Mac DMGs, a Windows x64 installer, Linux tarballs and Debian packages are also
prepared. The release manifest records every package name, size and checksum.
See [packaging](packaging.md) for installation paths and platform limits.

The [50-second gameplay demo](demo.md) uses the candidate's unchanged editor and
simulation source through a separate capture harness. It shows real circuit
editing, output changes, undo/redo, zoom, contrast, save/reopen and lessons.
It is a scripted demonstration, not a manual acceptance record.

Signing, Mac notarization, clean-machine desktop checks, physical input and
screen-reader testing remain open. The earlier Mac workstation checks are
recorded separately for [their exact candidate](preview-eacd8f2.md); they are
not relabeled as hands-on results for this build. All unperformed manual cards
remain pending. See the [release checklist](release-checklist.md).
