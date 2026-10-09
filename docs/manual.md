# Gatehaven manual

Build a circuit by choosing a component in the sidebar and drawing on the grid.
The starter has two powered branches feeding an AND gate. Space starts or pauses
the simulation; Right advances one tick when no selection is active. R resets
power and time. Ctrl+Space sets the speed, initially five ticks per second.

## Draw and navigate

Click a tool with the mouse button you want to assign. The colored marks identify
left, right, middle, X1, X2, and touch. Digits 1–0 select a left-button pencil.
Hold E and click a cell to sample its pencil, or empty space to sample the eraser.
Drag with Pan to move the camera; scroll zooms around the pointer. F frames the
whole circuit. These actions do not edit your circuit.
Plus/minus zoom without a mouse wheel; Home also frames the circuit. A panner
double-click centers the clicked cell, while a drag moves the existing view.

Clicking a gate, relay, or communicator with its own pencil places a Signal input.
Ordinary strokes snap to a horizontal or vertical line. Hold Shift and click to
start a polyline. Further clicks add snapped segments; Backspace removes the last
segment. Double-click, Enter, or click after releasing Shift to finish. Escape
cancels the preview. The whole polyline is one undo action. Insulated-wire turns
use conductive wire so electricity can turn the corner.

Ctrl+D previews a duplicate of the selection without replacing any shared
clipboard. Ctrl+I inverts the selection among occupied cells. A successful paste
selects the newly placed cells, ready to move or transform.

## Select, move, and copy

Q assigns the selector to the left button. Drag a rectangle; hold Shift to add or
Alt to remove cells. Double-click follows an electrical net: insulated crossings
keep their axes separate and Signal/control boundaries stop traversal. Starting
on a crossing selects both axes. Triple-click follows all occupied neighboring
cells, including control connections. Selected cells are blue, then red after a
move or transform. Only selected cells are edited; holes are preserved.

Arrows move one cell, or four with Ctrl. H/V flip and brackets rotate. D or Delete
erases. Ctrl+A selects all. Ctrl+Z/Y undoes/redoes edits. Ctrl+C/X/V copies, cuts,
or pastes through default clipboard 0; add Shift to choose any of ten shared
slots. Other running Gatehaven windows see the same clipboards. A paste preview
is stable while another window copies, and its transforms do not change that
window's clipboard. Clipboard contents are session data, not permanent saves.

## Save and learn

Ctrl+S saves; Ctrl+Shift+S or Shift-click Save chooses another path. New and Open
start separate windows. `.ghv` files contain circuit structure and never contain
paths to user files. Returning to the saved revision with Undo clears the unsaved
indicator. Save your circuits before closing the last window.

B toggles context hints, off by default. F2 shows shortcuts. F1 opens this manual.
F3 opens six editable [circuit lessons](../samples/README.md) in separate windows.
F5/F6/F7 choose Screen/File In/File Out. Use I to interact with them; see the
[communicator and serial protocol guide](communicators.md) before connecting files.
Command can replace Ctrl on macOS. Touch-generated pointer events have a separate
tool binding; full multitouch navigation and hardware acceptance remain in progress.

See [simulation rules](architecture.md) and [verification](verification.md) for
the implementation's tested behavior and current release limits.
