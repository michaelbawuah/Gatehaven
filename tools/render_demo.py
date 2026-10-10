"""Render a 50-second film using the real editor and a deterministic input script.

Requires a Linux build of the optional gatehaven-demo target and FFmpeg with
libx264/drawtext. No network, account, live preferences, or user circuits are used.
"""
import argparse
import json
import math
import os
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("capture", type=Path)
parser.add_argument("output", type=Path)
args = parser.parse_args()
out = args.output.resolve()
out.mkdir(parents=True, exist_ok=True)
events = []
cursor = (440, 160)


def at(seconds, command):
    events.append((round(seconds * 30), len(events), command))


def move(seconds, x, y, duration=0.4):
    global cursor
    start = cursor
    frames = max(1, round(duration * 30))
    for n in range(1, frames + 1):
        phase = n / frames
        eased = phase * phase * (3 - 2 * phase)
        px = start[0] + (x - start[0]) * eased
        py = start[1] + (y - start[1]) * eased
        at(seconds - duration + n / 30, f"move {px:.3f} {py:.3f}")
    cursor = (x, y)


def click(seconds, x, y, button=1):
    move(seconds, x, y)
    at(seconds, f"down {button}")
    at(seconds + 0.1, f"up {button}")


def cell(x, y):
    return 760 + x * 64, 430 + y * 64


def draw(seconds, begin, end, duration=0.65):
    move(seconds, *cell(*begin))
    at(seconds, "down 1")
    move(seconds + duration, *cell(*end), duration=duration)
    at(seconds + duration, "up 1")


def component(seconds, index):
    click(seconds, 110, 159 + index * 28)


component(2.0, 2)
click(2.7, *cell(-4, -2))
click(3.5, *cell(-4, 2))
component(4.3, 0)
draw(5.0, (-3, -2), (0, -2))
draw(6.2, (-3, 2), (0, 2))
component(7.4, 3)
click(8.1, *cell(0, -1))
click(8.8, *cell(0, 1))
component(9.5, 4)
click(10.2, *cell(0, 0))
component(10.9, 0)
draw(11.5, (1, 0), (4, 0))
component(12.7, 10)
click(13.4, *cell(5, 0))
click(14.2, 320, 42)  # Run.
move(14.7, 1120, 575)
at(15.2, "on")
at(15.3, "screenshot gatehaven-circuit.bmp")
click(16.0, *cell(-4, 2), button=3)  # Remove one powered input.
at(17.0, "off")
click(18.0, 578, 42)  # Undo.
at(19.0, "on")
click(20.0, 660, 42)  # Redo.
at(20.8, "off")
click(21.4, 578, 42)  # Restore both inputs.
at(22.1, "on")
click(22.4, 320, 42)  # Pause.
click(23.2, 1231, 726)  # Zoom in.
click(24.3, 1231, 726)
move(24.9, 1100, 570)
at(25.0, "screenshot gatehaven-zoom.bmp")
click(26.1, 1089, 726)  # Zoom out.
click(27.1, 1158, 726)  # Reset zoom.
click(28.4, 418, 42)  # Step.
click(29.3, 418, 42)
click(30.2, 418, 42)
click(31.4, 910, 80)  # Contrast on.
move(32.0, 1100, 600)
at(32.3, "screenshot gatehaven-contrast.bmp")
click(33.8, 910, 80)  # Contrast off.
click(35.1, 820, 42)  # Save via the real save path; native picker omitted.
at(35.5, "saved")
move(35.9, 1100, 600)
click(37.4, 738, 42)  # Reopen the saved document.
move(38.1, 1100, 600)
click(39.7, 1060, 80)  # Open the real lessons menu.
at(40.8, "screenshot gatehaven-lessons.bmp")
at(43.5, "key Escape")
click(44.3, 320, 42)
move(45.0, 1120, 600)

timeline = out / "timeline.txt"
timeline.write_text("".join(f"{frame} {command}\n" for frame, _, command in sorted(events)), encoding="utf-8")
chapters = [
    (0, 2, "Build circuits. Bring logic to life."),
    (2, 7.2, "Place power sources. Draw your connections."),
    (7.2, 14, "Add a gate and an output screen."),
    (14, 17.6, "Both inputs on. Remove one and watch the output change."),
    (17.6, 22.7, "Experiment freely with undo and redo."),
    (22.7, 28, "Zoom in, zoom out, and return to 100%."),
    (28, 31, "Pause the circuit. Follow it one tick at a time."),
    (31, 34.7, "Switch to high contrast with one click."),
    (34.7, 39.3, "Save your circuit. Reopen it and keep building."),
    (39.3, 43.8, "Six built-in lessons help you explore."),
    (43.8, 50, "Gatehaven  |  C++23 + SDL3  |  Open source"),
]
font = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
filters = [
    "scale=1440:900:flags=lanczos",
    "pad=1920:1080:240:76:color=0x0c252b",
    f"drawtext=fontfile={font}:expansion=none:text=GATEHAVEN:x=240:y=25:fontsize=24:fontcolor=0xe7f5f0",
    f"drawtext=fontfile={font}:expansion=none:text='DEVELOPMENT PREVIEW':x=1362:y=28:fontsize=16:fontcolor=0x9bc8bd",
]
for start, end, caption in chapters:
    filters.append(f"drawtext=fontfile={font}:expansion=none:text='{caption}':x=(w-text_w)/2:y=1010:fontsize=26:fontcolor=white:enable='gte(t,{start})*lt(t,{end})'")
filters.append("drawbox=x=240:y=984:w=1440:h=3:color=0x257a70:t=fill")
video = out / "Gatehaven-Gameplay-Demo.mp4"
command = ["ffmpeg", "-hide_banner", "-loglevel", "warning", "-y", "-f", "rawvideo", "-pixel_format", "rgb24",
           "-video_size", "1280x800", "-framerate", "30", "-i", "pipe:0", "-vf", ",".join(filters),
           "-an", "-c:v", "libx264", "-preset", "fast", "-crf", "19", "-pix_fmt", "yuv420p",
           "-movflags", "+faststart", str(video)]
with (out / "capture.log").open("w", encoding="utf-8") as log:
    capture = subprocess.Popen([str(args.capture.resolve()), str(timeline), str(out)], stdout=subprocess.PIPE,
                               stderr=log, env=dict(os.environ, SDL_VIDEODRIVER="dummy"))
    encoder = subprocess.Popen(command, stdin=capture.stdout)
    capture.stdout.close()
    encode_status = encoder.wait()
    capture_status = capture.wait()
if encode_status or capture_status:
    raise SystemExit(f"Capture failed ({capture_status}); encoder ({encode_status}). See {out / 'capture.log'}")
for source in out.glob("gatehaven-*.bmp"):
    subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-y", "-i", str(source),
                    "-frames:v", "1", str(source.with_suffix(".png"))], check=True)
subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-y", "-ss", "15.4", "-i", str(video),
                "-frames:v", "1", str(out / "Gatehaven-Demo-Poster.png")], check=True)
probe = json.loads(subprocess.check_output(["ffprobe", "-v", "error", "-show_streams", "-show_format", "-of", "json", str(video)]))
stream = probe["streams"][0]
assert stream["width"] == 1920 and stream["height"] == 1080 and stream["nb_frames"] == "1500"
assert math.isclose(float(probe["format"]["duration"]), 50, abs_tol=0.05)
print(json.dumps({"video": str(video), "seconds": 50, "frames": 1500, "bytes": video.stat().st_size,
                  "evidence": json.loads((out / "capture-evidence.json").read_text())}, indent=2))
