# AI Mobile E-Ink Reader

Desktop simulator for an ESP32 three-color E-Ink reader UI.

The simulator renders a 400 x 300 black/white/red frame with readable Chinese
UI text. It is the host-side prototype for the final device interface.

## Build

```bash
make reader_sim
```

## Test

```bash
make test
```

## Run

```bash
./reader_sim
```

Controls:

- `w`: up
- `s`: down
- `h` or Enter: home/select
- `p`: power/back
- `q`: quit

The latest simulated E-Ink frame is written to `out/frame.ppm`.

## Chinese Font Asset

The simulator uses the tracked bitmap font asset:

```text
assets/fonts/sim_zh16.h
```

This keeps runtime output deterministic and avoids depending on system fonts
when `reader_sim` runs.

To regenerate the asset on macOS:

```bash
python3 tools/generate_font.py
```

The generator uses `/System/Library/Fonts/PingFang.ttc` and Pillow. The
generated asset should be committed after regeneration.
