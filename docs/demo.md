# Gameplay demo

The portfolio film shows a real Gatehaven editing session driven by scripted
mouse input. It is 50 seconds long, 1920 × 1080, 30 fps, H.264, and silent with
on-screen captions. The finished MP4, poster, and screenshots belong with the
preview's release assets.

| Time | Demonstration |
| --- | --- |
| 0–14 s | Place two power sources, draw wires, add signal inputs, an AND gate, and an output screen |
| 14–23 s | Run; remove an input; undo and redo while the output changes |
| 23–28 s | Zoom in, zoom out, and reset to 100% |
| 28–35 s | Pause, single-step, and switch high contrast on and off |
| 35–39 s | Save and reopen the circuit |
| 39–50 s | Show the six built-in lessons and return to the running circuit |

## Reproduce on Linux

Use the normal desktop prerequisites and a static SDL build from
[building](building.md). The capture executable is an optional build target;
it is excluded from ordinary builds and never installed or packaged.
FFmpeg must provide `libx264`, `drawtext`, and `ffprobe`; the caption font is
DejaVu Sans at `/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf`.

```sh
cmake -S . -B build/app -DGATEHAVEN_BUILD_APP=ON \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/path/to/static-sdl3
cmake --build build/app --target gatehaven-demo --parallel 2
python3 tools/render_demo.py build/app/gatehaven-demo build/demo
```

The harness compiles the existing editor translation unit with its normal entry
point renamed, then drives the same event, update, and render methods. It uses
an isolated clipboard session and the SDL software renderer. Existing circuits,
preferences, app windows, and release binaries are not touched. The cursor,
click highlights, captions, and outer frame are capture overlays.

The save dialog is supplied a disposable destination through the editor's
existing picker callback. Reopen uses the real document loader in the capture
window; native dialogs and additional OS windows are omitted from the film.
The capture asserts powered/unpowered AND output, saved circuit equality, and
successful reopen. `capture-evidence.json` records the source revision and
checks; `timeline.txt` records all scripted events. The encoder checks frame
count, duration, and dimensions.

This demonstrates implemented behavior. Scripted input and software rendering
do not establish physical input latency, native dialog behavior, accessibility,
or completion of the [manual acceptance checklist](acceptance.md).
