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
Command can replace Ctrl on macOS. Native touch uses its own tool binding; see the gesture controls below.

See [simulation rules](architecture.md) and [verification](verification.md) for
the implementation's tested behavior and current release limits.

## Touch navigation

One finger uses the yellow Touch binding. Tap a sidebar tool to assign it to
touch. Put a second finger on the canvas to cancel an unfinished stroke and
start pan/pinch navigation. Moving both fingers pans; changing their separation
zooms around the moving midpoint. Lift both fingers before drawing again.
A third finger suspends navigation until two remain.

Double-tap with the Panner to center a cell. Double/triple-tap with Select to
choose an electrical net or physical circuit. Long holds and drags do not count
as repeated taps. Focus loss cancels unfinished touches and releases screen
interaction. Physical touchscreen and per-monitor scaling acceptance is still
required; automated SDL events cover the input and coordinate conversion paths.

## Recover unsaved work

Gatehaven keeps a recovery snapshot after two idle seconds of editing, or after
thirty seconds of continuous changes. Leaving the window also checkpoints a
modified circuit. These snapshots are separate from your saved circuit file.

After an unexpected shutdown, the next ordinary launch offers abandoned work.
Press F4 to open recovery later. Use Up/Down and Enter, or click a dated entry.
Escape keeps snapshots for later. Live windows never appear in this list.
Save your current circuit or use New before restoring another one.

A restored circuit opens paused and unsaved. Save it to choose its destination.
Recovery preserves components only: file-port choices and simulation state are
not recovered. Successful saves and an orderly close clear that window's
snapshot. A failed recovery write leaves the previous snapshot intact and shows
an error in the status bar. Recovery cannot guarantee the very latest edit after
sudden power loss; use Save for work you want to keep.

## Keyboard access to controls

Tab enters the toolbar and cycles through every toolbar, component, and tool
control. Shift+Tab moves backward; arrow keys move focus once it is visible.
Enter or Space activates the outlined control. Selecting a tool returns to the
canvas. Shift+Enter on a component or tool assigns the Touch binding. Escape
returns focus to the canvas without activating anything.

F2 help, the recovery chooser, clipboard chooser, and speed dialog consume
keyboard input while open. Simulation pauses behind these overlays and resumes
its previous play state when they close. Keyboard navigation is implemented;
screen-reader integration and a full accessibility audit remain release work.

## Files changed in another window

Saving an already opened file checks whether its contents changed on disk.
Gatehaven asks before replacing another window's saved work. Cancel keeps your
current edits; Ctrl+Shift+S saves them under a different name. A removed file
also requires confirmation before recreating it. Files are written beside the
destination and replaced atomically, so readers see a complete document.

This check reduces accidental overwrites; it is not a lock on other programs.
An external writer can still change a file between the check and replacement.

In the recovery chooser, Page Up/Page Down move five entries and Home/End jump
to the newest/oldest entry. The counter shows your position in the full list.
Delete asks for confirmation: Enter permanently removes only that abandoned
snapshot; Escape keeps it. Saved circuit files and live windows are unaffected.

## Offline help

Packaged builds include browser-readable guides. F1 opens the installed manual,
including the file-port protocol and simulation notes, without an internet
connection. Browser zoom, text selection, search, and printing work normally.
A development build without generated guides falls back to the repository copy.
