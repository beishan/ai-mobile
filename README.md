# AI Mobile E-Ink Reader

Desktop simulator for an ESP32 three-color E-Ink reader UI.

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
