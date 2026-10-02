# Phat Booty Bass

A VST3 bass synth with a built-in groove generator. Hold a key and it plays a swung,
syncopated 16th-note bassline locked to your DAW's tempo, in the key you're holding.
Roll the dice for a new riff, then drag it straight onto your timeline as MIDI.

## Features

- **Groove generator:** seeded patterns (0–999) shaped by Density, Funk (syncopation),
  Swing, Gate, Octave Pops, Slides and Ghost Notes. Six scales, 1/2/4-bar patterns.
  Turning the knobs adds or removes notes without reshuffling the riff.
- **Host sync:** follows the host's tempo and bar position while the transport plays,
  and keeps its own clock when it's stopped. Change keys mid-groove to transpose.
- **Mono bass voice:** PolyBLEP saw/square, sine sub, 24 dB resonant low-pass with an
  envelope, 303-style accents and slides, drive and glide. Turn Groove off for a plain
  mono synth.
- **Custom UI:** live pattern grid with a playhead, and a dice button for new seeds.
- **Riffs out as MIDI:**
  - *DRAG MIDI* handle: drag the current riff onto your DAW timeline as a `.mid` clip.
    Clicking it instead saves the file to `~/.local/share/PhatBooty/riffs/`.
  - *MIDI Out* bus: every played note is also sent out as MIDI, so the host can record
    a whole jam.

## Building

You need CMake ≥ 3.25, a C++17 compiler and the
[VST 3 SDK](https://github.com/steinbergmedia/vst3sdk) (tested with 3.8.1), cloned
with submodules **next to this repo**:

```
vstthreez/
├── vst3sdk/      git clone --recursive https://github.com/steinbergmedia/vst3sdk.git
└── phatbooty/    this repo
```

To use an SDK somewhere else, pass `-Dvst3sdk_SOURCE_DIR=/path/to/vst3sdk`.

### Linux dependencies (Debian/Ubuntu)

```
sudo apt install cmake ninja-build g++ libx11-dev libxcb-util-dev libxcb-cursor-dev \
  libxcb-keysyms1-dev libxcb-xkb-dev libxkbcommon-x11-dev libcairo2-dev \
  libpango1.0-dev libfreetype-dev libfontconfig1-dev libgtkmm-3.0-dev \
  libsqlite3-dev wayland-protocols
```

### Build

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The build runs Steinberg's validator on the plug-in. On Linux it also symlinks
`build/VST3/Release/phatbooty.vst3` into `~/.vst3`, so after rebuilding you only need
to re-scan plug-ins in your DAW.

macOS and Windows builds should work through the same CMake setup, but so far it has
only been tested on Linux.

## Using it (REAPER example)

1. Put **Phat Booty Bass** on a track, arm it and hold a low note (MIDI 28–40 sounds best).
2. Press play: the groove locks to the project tempo. Click the die for a new riff.
3. Drag **DRAG MIDI** onto the arrange view to drop the riff as a MIDI item.
4. To record a performance as MIDI, set the track's record mode to
   *Record: output (MIDI)*.

## Tools

- **Offline renderer:** plays the engine without a host and writes a WAV (and
  optionally the pattern as MIDI). It prints the step grid, which is handy for tuning
  the generator:

  ```
  build/bin/Release/phatbooty_render out.wav [bars] [bpm] [note] [Param=value ...] [mid=riff.mid]
  build/bin/Release/phatbooty_render out.wav 4 104 28 Seed=7 Funk=90 Slides=40
  ```

- **UI preview:** `build/bin/Release/editorhost build/VST3/Release/phatbooty.vst3`
  opens the editor without a DAW. Under Xwayland this SDK sample host can fail with
  `_XEMBED_INFO does not exist` until some app has registered the XEMBED atoms.

## Source layout

| File | What it does |
| --- | --- |
| `source/engine.h` | Groove generator, sequencer and synth voice (no VST dependencies) |
| `source/params.h` | Parameter table shared by the plug-in, UI and renderer |
| `source/processor.cpp` | VST3 audio processor: events, transport, MIDI out |
| `source/controller.cpp` | VST3 edit controller, editor, dice and riff export |
| `source/views.cpp` | Custom VSTGUI views: pattern grid, dice, drag handle |
| `source/midifile.h` | Pattern → Standard MIDI File |
| `source/filedrag_x11.cpp` | XDND drag source (VSTGUI has none on Linux) |
| `resource/phatbooty.uidesc` | Editor layout |
| `tools/render.cpp` | Offline renderer |

## License

MIT, see [LICENSE](LICENSE). The VST 3 SDK is MIT-licensed by Steinberg; VST is a
trademark of Steinberg Media Technologies GmbH.
