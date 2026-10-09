# Signing a preview

`tools/sign_preview.py` prepares Windows ZIPs and macOS DMGs from a CMake install
stage. It prints a plan by default. The native signing path is implemented and
its failure boundaries have automated tests; real certificate, timestamp-server,
Apple notarization, and end-user trust checks remain pending until credentials
are supplied. CI's ordinary packages remain unsigned development previews.

Start with a clean checkout of the exact candidate commit. Configure and build
from that checkout, pass its tests, then stage the static-SDL desktop install:

```sh
cmake --install build/app --config Release --prefix build/sign-stage
```

Use a fresh stage and a new output directory. The script checks target metadata,
required files, and the source revision; it cannot prove how an arbitrary binary
was compiled. Keep the original build logs and exact candidate CI evidence.

## Windows

Install the Windows SDK's SignTool and make it available on PATH, or pass its
absolute path with `--signtool`. Provision the signing certificate and private
key in the current user's Personal (`My`) certificate store using your chosen
certificate provider's supported process. Hardware-backed keys can require that
provider's signing middleware and interactive authentication.

```powershell
python tools/sign_preview.py windows build/sign-stage build/signed-windows --identity YOUR_40_HEX_CERTIFICATE_THUMBPRINT --timestamp-url YOUR_RFC3161_TIMESTAMP_URL
```

Replace both placeholders with your actual certificate thumbprint and your
provider's timestamp URL, review the plan, then repeat with `--execute`.
The script signs both executables using SHA-256 and an RFC 3161 timestamp. It
verifies all signatures and timestamps using the default Authenticode policy;
warnings fail the operation. Bundled runtime DLLs retain their own signatures
and must pass verification. It produces a ZIP, not a signed NSIS installer.

## macOS

Use Xcode command-line tools and a Developer ID Application identity already
installed in the signing keychain. Create a named `notarytool` keychain profile
through Apple's supported credential setup before running this tool. The script
takes only the profile name; no passwords or private-key files go in its flags.

```sh
python tools/sign_preview.py macos build/sign-stage build/signed-macos \
  --identity 'Developer ID Application: YOUR NAME (TEAMID)' \
  --notary-profile gatehaven-release
```

After replacing the identity and reviewing the plan, repeat with `--execute`.
The workflow signs the CLI and app with hardened runtime and secure timestamps,
submits the signed contents, requires `Accepted`, and staples/verifies the app.
It then builds and signs the DMG, obtains its own accepted notarization, staples
the DMG, and runs signature, Gatekeeper, and disk-image verification. Both
submission IDs are recorded. Network failures or rejection stop publication.
On timeout, use `xcrun notarytool history --keychain-profile gatehaven-release`
and the service's log commands to investigate the pending submission before
retrying. The tool does not alter Gatekeeper or quarantine protections.

## Outputs and acceptance

All work happens on a temporary copy. The original stage remains unchanged.
Only successful verification produces the new output directory, containing:

- The signed preview ZIP or signed/notarized DMG.
- A SHA-256 sidecar for those exact final bytes.
- `signing-evidence.json`, with input/signed-file hashes, original build metadata,
  verification command results, certificate identity and notarization receipts.

The build's `signed_release: false` field remains unchanged: it describes the
development build, not the later distribution operation. The separate signing
record identifies a `signed-preview` and leaves `release_acceptance_complete`
false. A signature does not replace the [release acceptance checks](release-checklist.md),
including downloaded-artifact trust on clean machines. Signing evidence can
contain local paths and the public certificate identity; review it before sharing.

Official references: [Microsoft SignTool](https://learn.microsoft.com/en-us/windows/win32/seccrypto/signtool),
[Apple notarization workflow](https://developer.apple.com/documentation/security/customizing-the-notarization-workflow),
[Apple signing and disk-image verification](https://developer.apple.com/library/archive/technotes/tn2206/).
