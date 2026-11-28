# F--k you, Piano Roll!

A note-effect plugin for Bitwig (CLAP, plus VST3) that lets you write music on a
staff with a classic speedy keyboard workflow: arrow keys place the pitch,
number keys enter the note. It plays the notes as MIDI into the instrument after
it on the track, in time with Bitwig.

Inspired by the keyboard entry workflow of a discontinued notation program. It
is an independent re-implementation and contains no code, artwork or fonts from
that product. Music symbols use the free Bravura font (SIL Open Font Licence,
see `resources/fonts/OFL.txt`).

## What it does

- Single staff or grand staff, any clef on any staff, clef changes at a measure
  or before any note. Staves move apart to make room for very high or low notes
  when you leave the entry frame.
- **Grand staff** button (Cmd+Shift+G) splits a single staff at middle C
  (middle C goes on the top staff); turning it off brings the notes back up.
  **Move to staff** buttons (Cmd+Shift+Up / Down) move a note, or the
  selection, to the other staff. **Clef** button: treble, alto, tenor, bass at
  the insertion bar (Cmd+Option+Shift+1-4).
- A note that doesn't fit in the measure opens a choice window: tie it over
  the barline, push the music along, change the time signature, leave it too
  full, or cancel (keys 1-4, Esc), with "do this every time" (also in Settings).
- Delete keys: forward delete turns a note into a rest; back delete removes it
  and closes the gap. Enter adds the crossbar's pitch to the note under the bar.
- Every command has a button: palettes for Notes, Rests, Pitch, Ties & beams,
  Articulations, Dynamics, Select and Move. Tooltips show the keys.
- Speedy-style entry with the Mac keys: number keys 1-8 (64th to double whole),
  arrows for pitch, `.` dot, `+`/`-` half steps, `9` respell, `*` accidental,
  `=`/`T` tie, Option+number tuplets (Option+1 any ratio), Option+Shift+number
  rests, Enter chord note, `;` grace, `` ` `` slashed grace, `/` beams, `L` stem,
  Shift+Up/Down staff, `[` `]` measures, Esc leaves and re-enters the frame,
  Shift+I insert mode
- Four layers (voices): `'` switches layer 1/2, Cmd+Option+1-4, or the 1-4
  buttons. Other layers are greyed while you type.
- Speedy and Select tools. Select by dragging (snaps to the grid: free, 128th
  to double whole, or whole measures), Shift+arrows, Cmd+A. Cmd+C / Cmd+X /
  Cmd+V copy, cut and paste, also between clips.
- Dynamics pppp to ffff (Option+D or the measure menu), each a fixed velocity
- Velocity lane under the staff, like Bitwig's: drag dots to change velocity;
  selecting notes selects their dots
- Staccato, tenuto, accent, slurs (`(` and `)`), all change playback,
  Classical-style defaults, adjustable in Settings
- Hear notes: each note plays once, short, as you enter it
- Clips in a left-to-right row of small numbered slots like Bitwig's launcher
  (double-click or click the name at the top to rename), New / Duplicate /
  Loop / Delete buttons, a song clip that follows Bitwig's timeline, drag-out
  as MIDI
- Tempo and meter follow Bitwig, each switchable; own transport and BPM when
  the tempo is not synced
- Takes the Bitwig track's colour (or your own pick); light and dark modes
- Export MusicXML (.musicxml or compressed .mxl)
- **Commands** button (Cmd+K) lists every command; the key map is a text file
  you can edit

The full key map is in `core/src/KeyMap.cpp` (`KeyMap::defaultText`).

## Building

Requires CMake 3.22+ and a C++17 compiler. JUCE and clap-juce-extensions are
downloaded automatically.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target StaffPaper_CLAP StaffPaper_VST3 core_tests
./build/core_tests
```

The GitHub Actions workflow builds signed (ad hoc) Apple Silicon bundles on
every push; download them from the run's artifacts. See `docs/INSTALL-mac.txt`.

The plugin's name is set once, in `PLUGIN_DISPLAY_NAME` in `CMakeLists.txt`.

## Layout

- `core/` — notation model, entry editor, key map, playback scheduler,
  save/load, MusicXML export. No JUCE; unit tested in `tests/`.
- `plugin/` — JUCE plugin: processor (MIDI out, transport sync, launcher),
  staff view, clip view, editor window.
- `tools/RenderCheck.cpp` — draws a sample score to PNG for checking notation
  without a DAW.
