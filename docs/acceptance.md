# Test a desktop build

The acceptance collector runs isolated checks, records the exact executable hash
and build revision, and creates a local browser checklist. It never marks a
hands-on check passed for you. Use a fresh output folder for each machine and
candidate. Python 3.10 or newer is required for the collector; the game itself
does not require Python.

## From a checkout

On a Mac, after building the desktop application:

```sh
python3 tools/acceptance.py build/app/gatehaven.app build/my-mac-check
open build/my-mac-check/report.html
open build/app/gatehaven.app
```

On Windows, use the installed `bin/gatehaven.exe` or
`build/app/Release/gatehaven.exe` as the first argument. On Linux, use
`build/app/gatehaven` or the installed executable. The output folder's parent
must exist. The app must be allowed to run under the operating system's ordinary
protections; collection does not remove quarantine, change trust settings, or
work around a blocked launch. An unsigned preview is not a signed release.

## From an installed package

The collector ships alongside the offline guides. For a Mac app in Applications:

```sh
python3 /Applications/gatehaven.app/Contents/Resources/acceptance/acceptance.py \
  /Applications/gatehaven.app ./gatehaven-mac-check
open ./gatehaven-mac-check/report.html
```

Windows/Linux archives keep the script and its HTML template in
`share/gatehaven/acceptance/`. Keep those two files together. Pass the installed
application, an installation folder, or a Mac `.app`; save reports outside the
application bundle. A moved Mac app includes everything the collector needs.

Optionally add `--package path/to/download.zip --expected-sha256 EXPECTED_HASH`
to associate the original download with the report and check its hash before
running. Use the checksum belonging to that exact download. Without this option,
the collector still hashes the executable but does not verify a package checksum.

## What runs automatically

- Build information and JSON diagnostics, with a consistency check between them.
- Desktop event tests in a native window using the available renderer.
- Installed-manual lookup.
- Static/pan/zoom measurements with fixed visible work and 10,000 off-screen cells.

Test modes use isolated temporary sessions and do not load your circuits,
preferences, clipboards or recovery data. Brief test windows may appear. The
benchmark can be canceled with Escape or its close button; cancellation is
recorded as a failed automatic check. `--skip-benchmark` omits timing collection
and records that omission. Failures remain in `evidence.json` and in the report.

`--headless` explicitly uses SDL's dummy/software path for CI. A normal collection
rejects a dummy or off-screen driver. X11/Wayland virtual desktops and remote
sessions still require human disclosure; a native backend name is not proof of
a physical display. The report always leaves final release acceptance false.

## Complete and save observations

Open `report.html`, describe your machine and test setup, and use the disposable
`practice-circuit-é.ghv` in the same folder. Work through the twelve cards in the
installed game. Each starts **Pending**. Record observations for failed, blocked
or inapplicable checks; the page requires those explanations before export.
For example, a MacBook without a touchscreen can record touch as inapplicable,
while touchscreen acceptance remains open for the product as a whole.

Choose **Save results as JSON** before closing. The page does not upload anything
or save your edits into the original HTML. To resume, reopen the same report and
import the downloaded results. It rejects another executable's results or another
collection and never replaces the automatic evidence with imported claims.

Share the saved JSON with the person reviewing acceptance. It contains build
hashes, OS/build details, captured command output and your notes; output can
include local paths. The twelve cards cover the [release checklist](release-checklist.md),
but one machine's results do not sign off every supported target or the application
license and distribution decisions.

## Standalone diagnostics and rendering measurements

```sh
gatehaven --diagnostics
gatehaven --benchmark-display 10000 60
gatehaven --benchmark-render 10000 60
```

Diagnostics report the video driver, renderer, SDL version, window/pixel/output
dimensions, display scale, pixel density, refresh rate where available, VSync,
source revision and Debug/Release configuration. Unknown optional values are
JSON `null`. No monitor names, usernames or circuit contents are collected by
the diagnostics command.

`--benchmark-display` uses a visible native session and the chosen SDL renderer;
it can still be a software renderer. `--benchmark-render` requests the hidden
software path used by CI. Compare actual backend, pixel dimensions, build
configuration and effective VSync on the same machine. The measurements include
render submission and presentation; they are not GPU-fenced execution times or
physical input-to-photon latency. There is no universal frame-time pass threshold.
