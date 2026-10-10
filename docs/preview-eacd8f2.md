# 0.5 preview readiness — 10 October 2026

Candidate: `eacd8f27469b380d8115dd539c99615af179d274`.
Status: **verified unsigned development preview; final release acceptance is open**.
This records that exact candidate. Later documentation commits do not change its
binary identity. The [machine-readable record](preview-eacd8f2.json) contains
job IDs, artifact digests, package hashes and outstanding manual scenarios.

## Verified builds

[CI run 38030730887](https://github.com/michaelbawuah/Gatehaven/actions/runs/38030730887)
passed all 18 jobs. They cover core and desktop builds on six targets,
sanitizers, static analysis, simulation benchmarks, external compatibility,
and fresh Ubuntu container install/reinstall/remove/purge checks on both
architectures.

For a preview, open that run and choose the matching artifact under **Artifacts**:

| Machine | Artifact | Included preview formats |
| --- | --- | --- |
| Mac with Apple Silicon | `Gatehaven-macOS-ARM64` | ZIP, DMG |
| Mac with Intel CPU | `Gatehaven-macOS-X64` | ZIP, DMG |
| Windows x64 | `Gatehaven-Windows-X64` | ZIP, NSIS installer |
| Windows ARM64 | `Gatehaven-Windows-ARM64` | ZIP |
| Linux x86-64 | `Gatehaven-Linux-X64` | TGZ, DEB, AppImage |
| Linux ARM64 | `Gatehaven-Linux-ARM64` | TGZ, DEB, AppImage |

Actions downloads require a GitHub session and expire; these artifacts currently
expire on 9 November 2026. They are temporary testing downloads. Extract the outer
Actions archive and verify the chosen package against its included `.sha256`
file. The Actions artifact digest covers the outer download, not the inner ZIP,
DMG, installer or AppImage. See [packaging](packaging.md) for platform limits.

## Mac workstation check

An Apple Silicon Mac running Darwin 25.6.0 built this candidate with AppleClang
21.0.0.21000101, CMake 4.4.4 and static SDL 3.4.18. All **17** package-enabled
CTest suites passed. CPack produced a ZIP and DMG; the DMG installer test passed
integrity, read-only mounting, document types, relocation to a path with spaces
and an accented character, desktop self-test, bundled lessons, notices and help.
This was a developer workstation check, not a clean-machine installation.

The following checksums identify these local packages in `build/mac-controls/`.
CI downloads have their own hashes; do not use these values to verify CI builds.

| Package | Bytes | SHA-256 |
| --- | ---: | --- |
| `Gatehaven-0.5.0-Darwin-arm64.zip` | 3,231,725 | `7c582ed281cc3281a8a0fb848e527bb8d9a200f1967f3df4521ee6edb62d8ae4` |
| `Gatehaven-0.5.0-Darwin-arm64.dmg` | 3,141,272 | `0707b38fe4b5c1bc7a79faea8ec485a122f8d553cf54e2afbe6f4fa58c1c1c96` |

Both checksum sidecars matched; the ZIP's CRC check passed. Its desktop binary
matches the staged app and the working app installed in the user's Applications
folder, with SHA-256
`e4a7a7b0345ad50bab488103e9456ddc1d37f2afd30fdda2927d7fc0bcb4cdb9`.

The existing native acceptance collection for that same binary passed all five
automatic checks using Cocoa/Metal at 1280×800 logical and 2560×1600 output pixels.
The original collection did not bind a package checksum; the ZIP comparison above
was performed afterward. Its report is at
`build/mac-controls/native-evidence-final/report.html` in the Mac checkout.
These SDL event tests do not prove physical shortcut routing or screen-reader
accessibility.

The user confirmed the updated Mac app works after the reported gate-symbol
problem. The earlier text-only tiles came from an older app copy. This resolves
that reported issue; all twelve detailed manual cards remain pending until
their individual observations are recorded and exported.

## Remaining work

| Work | Next action |
| --- | --- |
| Hands-on acceptance | Complete the [offline report](acceptance.md), including default Mac shortcuts, file dialogs, recovery and a 20-minute circuit session. Record unavailable hardware explicitly. |
| Other devices and clean installations | Test intended Windows/Linux/Mac targets, physical display/input/accessibility behavior, and installation/update/removal with user data preserved. |
| Mac signing | Configure a Developer ID Application identity and notary keychain profile, then run the [signing workflow](signing.md) on the verified stage. This Mac reported zero valid code-signing identities. |
| Windows signing | Provide an appropriate signing identity and timestamp service, then verify the signed download on a clean Windows machine. |
| Release decisions | Choose the application license, supported target versions, and release/update policy; retain dependency notices. |

No signed artifact or public GitHub Release was published in this preparation
step. Signing creates new artifacts and hashes that need their own acceptance
record. Use the full [release checklist](release-checklist.md) for sign-off.
