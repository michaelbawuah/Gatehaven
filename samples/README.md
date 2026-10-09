# Circuit lessons

Press **F3** in the desktop to open a lesson in its own window. Use **Space** to
run, **Right** to advance one tick with no selection, and **I** to hold a screen
input. Releasing the input turns it off. A screen's brightness shows its outgoing
signal, so an input screen can drive a wire while remaining dark.

| Lesson | Experiment |
| --- | --- |
| `starter` | Run the two powered inputs through an AND gate; remove an input source and observe the next tick. |
| `oscillator` | Step repeatedly and watch the feedback circuit alternate. |
| `screen-switch` | Hold the left screen while running. Watch the Signal and gate introduce tick delays before the right screen lights. |
| `positive-relay` | Hold the lower screen to open the horizontal power path; release it to close the path. |
| `negative-relay` | Repeat the same experiment: the horizontal path is enabled when the lower control is off. |
| `gate-gallery` | Left to right: AND, OR, NAND, NOR. Hold either side input; inspect the output above each gate. Duplicate a source to drive both inputs together. |

Each `.ghv` here is editable and contains only circuit cells. These are original
Gatehaven teaching circuits; they use no external assets. The factories that
generate them have regression tests for their expected behavior.

```sh
gatehaven --demo=screen-switch
gatehaven-cli examples
gatehaven-cli example gate-gallery my-gallery.ghv
gatehaven-cli trace samples/oscillator.ghv 8
gatehaven-cli svg samples/gate-gallery.ghv gallery.svg
```

File communicators need a circuit that implements their serial protocol; see
[the protocol guide](../docs/communicators.md). Opening a saved circuit never
opens arbitrary files on your computer.
