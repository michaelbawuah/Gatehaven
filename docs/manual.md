# Gatehaven manual

Build a circuit by choosing a component in the sidebar and drawing on the grid.
The starter has two powered branches feeding an AND gate. Space starts or pauses
the simulation; Right advances one tick when no selection is active. R restores
stored reset levels and resets time. Ctrl+Space sets the speed, initially five ticks per second.

Use **Run**, **Step**, **Reset**, and **Fit view** in the top toolbar. **Examples**
opens the circuit lessons; **Quick guide** groups the main shortcuts by task.
The editor uses a light workspace, smooth proportional text, component icons,
and an outlined keyboard focus. The minimum window is 1120 × 700 to keep the
controls readable; the default is 1280 × 800. Resizing preserves the canvas ratio.
Hover over a component or canvas cell to see its large image and a short
description at the bottom of the sidebar. New circuits start with 64-pixel cells;
the zoom percentage resets to this larger size. Fit view still shows the whole
circuit, which can reduce the size of cells in large examples.

## Draw and navigate

Click a tool with the mouse button you want to assign. The badges identify
L (left), R (right), M (middle), X1, X2, and T (touch). A plus means more than one
input uses that tool. The left-button tool also has a filled selected row.
Tab to a tool and press F8 to hear/read its complete binding description.
Digits 1–0 select a left-button pencil; their shortcut appears next to the badge.
Hold E and click a cell to sample its pencil, or empty space to sample the eraser.
Drag with Pan to move the camera. Scroll or pinch a Mac trackpad to zoom around
the pointer; the circuit stays anchored there throughout a pinch. Zoom ranges
from 12.5% to 800%, with gradual scrolling and support for fractional wheel input.
The lower-right minus and plus buttons zoom around the canvas center; click the
percentage to reset to 100%. Plus/minus keys also zoom, including Ctrl/Command
plus/minus; Ctrl/Command+0 resets to 100%. Hold a zoom key to repeat.
F or Home frames the whole circuit. A panner double-click centers the clicked
cell, while a drag moves the existing view. Navigation does not edit the circuit.

Placed components use the same circuit symbols as the sidebar: AND has a rounded
right edge, OR has curved sides, and NAND/NOR add a hollow inversion circle.
Gates turn toward a connected conducting neighbor, or away from a Signal input
when there is no output neighbor. This orientation only affects the drawing;
inputs and outputs still work on any side. Orange terminals identify Signal
controls. Relay contacts show their current conducting state, and screen centers
light up when the circuit sends power. Source and file ports also have distinct
symbols, including when zoomed in.

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
start separate windows. Save `.ghv` to keep signed canvas coordinates or `.ccsb`
for legacy interchange. The legacy file's origin is its occupied top-left corner;
wide sparse rectangles can exceed its dense-area limit. Both formats preserve
component levels and never contain paths to user files. Returning to the saved revision with Undo clears the unsaved
indicator. Save your circuits before closing the last window.

If you omit the extension, Gatehaven adds `.ghv` or `.ccsb` according to the chosen
format. If that completed filename already exists, confirm replacement or cancel
and choose a different name. Canceling or failing Save As keeps the circuit open
and preserves its previous save destination.

B toggles context hints, off by default. F2 shows shortcuts. F1 opens this manual.
F3 opens six editable [circuit lessons](../samples/README.md) in separate windows.
F5/F6/F7 choose Screen/File In/File Out. Use I to interact with them; see the
[communicator and serial protocol guide](communicators.md) before connecting files.
Command can replace Ctrl on macOS. Native touch uses its own tool binding; see the gesture controls below.

See [simulation rules](architecture.md) and [verification](verification.md) for
the implementation's tested behavior and current release limits.

## Touch navigation

On a touchscreen, one finger uses the Touch binding. Tap a sidebar tool to assign it to
touch. Put a second finger on the canvas to cancel an unfinished stroke and
start pan/pinch navigation. Moving both fingers pans; changing their separation
zooms around the moving midpoint. Lift both fingers before drawing again.
A third finger suspends navigation until two remain.

Mac trackpad pinches use native gesture events. Trackpad finger positions are
not treated as touchscreen drawing coordinates; ordinary trackpad scrolling
zooms just like a mouse wheel.

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
Recovery preserves components and their saved/reset levels. File-port choices,
protocol queues, tick count, viewport, and undo history are not recovered. Successful saves and an orderly close clear that window's
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

The status strip names the focused control. F8 opens its description, shortcut,
and current mouse/touch bindings in a text dialog. F12 describes the current
window, unsaved state, simulation, chosen file-connection count, and focused cell
or control. These dialogs pause elapsed-time accumulation while open.

F11 toggles high contrast. The choice is saved with your tool bindings and speed;
older preferences still load with the standard colors. Powered wires become
thicker, with each crossing axis shown independently, so power has a shape cue
as well as a color cue. F8 also reports each axis in words. This mode improves
legibility; it does not establish full screen-reader or accessibility compliance.

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

### Saved and reset levels

Opening a circuit establishes tick-zero power immediately. Sources and saved gate
outputs power connected wires before the first Step. Reset uses the circuit's
stored reset levels; Step then advances every gate/relay from the preceding tick.
Editing while paused updates wire connectivity immediately.

## Inspect a cell

Point at a cell and press F8 for a native text dialog. Crossings report horizontal
and vertical power separately. Relays distinguish whether they conduct from
whether they are powered. Gates and ports show stored and reset levels, and
communicators report transmitting/receiving. Inspection does not advance time
or edit the circuit. The bottom status strip also shows crossing axes and relay
conductivity. Native dialog accessibility still needs platform screen-reader QA.

## Keyboard canvas

F9 enters a cell cursor at the pointer, or the center of the view. Arrows navigate
one cell; Ctrl+arrows move four. The camera follows, and navigation never advances
the simulation. Digits or Tab choose tools; Enter applies the left-button tool at
the cursor. Q then Enter selects that cell for copy, delete, or transformation.
Ctrl+V then Enter places a copied circuit. Hold Enter with the Interactor to press
a screen; release Enter to release it. F10 steps while the cursor is active.
F8 opens the text inspector. Escape, F9, or a pointer click leaves cursor mode.
The native dialog and offline manual provide readable text, but the custom
canvas does not yet expose a complete screen-reader accessibility tree.

## Simulation rate under load

The speed control accepts 1–1,000 ticks per second. Each desktop frame runs the
steps due from elapsed time, with a six-millisecond work budget between ticks so
input and drawing can continue. If the circuit or file system is too slow, the
actual rate decreases; simulation steps still execute in order. A single slow
step or file operation can exceed that budget. Pausing clears pending time, and a
long suspension never creates an unbounded catch-up queue.

## Report a problem or try a release candidate

The optional [desktop check](acceptance.md) collects build/display details and
opens an offline checklist for your observations. Its practice circuit is
disposable, and its automatic test modes leave your saved work and preferences
alone. Save the checklist's results before closing the browser page.
