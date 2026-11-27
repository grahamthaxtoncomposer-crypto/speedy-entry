# F--k you, Piano Roll!

A note-effect plugin for Bitwig (CLAP, plus VST3) that lets you write music on a
staff with a classic speedy keyboard workflow: arrow keys place the pitch,
number keys enter the note. It plays the notes as MIDI into the instrument after
it on the track, in time with Bitwig.

Inspired by the keyboard entry workflow of a discontinued notation program. It
is an independent re-implementation and contains no code, artwork or fonts from
that product. Music symbols use the free Bravura font (SIL Open Font Licence,
see `resources/fonts/OFL.txt`).

## What works in this first version

- Single staff or grand staff, any clef on any staff, clef changes at a measure
  or before any note
- Speedy-style entry: number keys 1-8 (64th to double whole), arrows for pitch,
  `.` dot, `+`/`-` half steps, `9` respell, `*` show/hide accidental, `=`/`T` tie,
  `Enter` add chord note, `Backspace` note/rest, `Delete`, `/` or `B` beam
  break/join, `L` stem flip, `;` grace note, `[` `]` measures, Shift+Up/Down
  staff, Cmd+3 triplet, Cmd+1 any tuplet ratio, Cmd+Z / Cmd+Y undo/redo
- Measures fill like the classic entry frame: an overfull note is split and
  tied across the barline and the cursor jumps to the next measure
- Staccato (Option+S), tenuto (Option+T), accent (Option+A), slurs
  (Option+9 start, Option+0 end) — all change playback, Classical-style
  defaults, adjustable in Settings
- Right-click a measure (or Option+M): time and key signatures, clefs,
  barlines, repeats, 1st/2nd endings, pickup measure, insert/delete measures,
  grand staff, MIDI channel per staff
- Scroll view and page view, zoom, light and dark mode
- Plays in sync with Bitwig (tempo, position, loop), or with its own
  play / stop / rewind and tempo when sync is off
- Clips view: a launcher column with launch quantize, loop or one-shot clips,
  a "song" clip that follows Bitwig's timeline, and drag-out of any clip as a
  MIDI file onto a Bitwig track
- **Keys** button lists every command; the key map is a text file you can edit

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
  save/load. No JUCE; unit tested in `tests/`.
- `plugin/` — JUCE plugin: processor (MIDI out, transport sync, launcher),
  staff view, clip view, editor window.
- `tools/RenderCheck.cpp` — draws a sample score to PNG for checking notation
  without a DAW.
