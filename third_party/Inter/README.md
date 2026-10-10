# Inter 4.1

Unmodified variable font and license from https://github.com/rsms/inter/tree/v4.1.

- `InterVariable.ttf`: SHA-256 `4989b125924991b90d05b2d16e0e388c48f7d5bb8b30539bbf9c755278d0ccaf`
- `LICENSE.txt`: SIL Open Font License 1.1; retained in every installed package.

`tools/bake_ui_font.py` (Pillow 12.3.0, FreeType 2.14.3) generates regular and semibold alpha atlases at 64 px.
The checked-in atlas is embedded in the executable, so rendering works offline
and does not depend on fonts installed on the player's computer. The source font
and the generated atlas remain under the font's license.
