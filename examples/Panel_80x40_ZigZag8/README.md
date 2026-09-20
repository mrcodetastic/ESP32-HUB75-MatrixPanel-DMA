# 80x40 ZIGZAG8 panel on the vanilla library (no PR required)

Drives a common "outdoor module" style **80x40, 4-scan (1/10 scan) HUB75 panel** built around **SM16208** driver ICs, using only the unmodified library as published — no branch, no PR merge, no edits to library source.

The panel:

| Property             | Value                                                                 |
| -------------------- | --------------------------------------------------------------------- |
| Logical resolution   | 80 x 40                                                               |
| Scan                 | 1/10 (A–D, no E), 4 rows lit per address                              |
| Data lanes           | 2 (RGB1 rows 0–19, RGB2 rows 20–39)                                   |
| Shift chain per lane | 160 clocks, two logical rows interleaved per lane                     |
| Quirk                | column order mirrored inside every 8-LED block (asymmetric per phase) |

See [`doc/Panel_80x40_4Scan_ZigZag8.md`](../../doc/Panel_80x40_4Scan_ZigZag8.md) for the panel's data ordering (with diagrams) and the full write-up of the custom scan-type technique used here.

## The technique

`VirtualMatrixPanel_T`'s scan-type template parameter is a **type**, not a fixed enum selection. The class only ever calls `ScanTypeMapping::apply(coords, panel_pixel_base)`, so any struct in your sketch with a matching static `apply()` can be passed directly as that
template argument. This sketch defines the ZIGZAG8 remap locally as `SM16208ZigZagMapping` instead of adding a scan type to the library:

```cpp
VirtualMatrixPanel_T<CHAIN_NONE, SM16208ZigZagMapping, 1> *panel = nullptr;
```

**Trade-offs vs. upstreaming the mapping into the library:** the mapping is private to your sketch (not discoverable by others with the same panel) and
leans on `VirtualCoords` / `apply()` / `panel_pixel_base` as an implicit contract rather than a documented, tested public API.

## Panel-specific configuration — don't skip this

1. **Configure the DMA as 160x20, not 80x40.** The driver ICs fold two logical rows into one 160-clock shift register per lane, so the mapping targets a folded 160x20 DMA surface. An 80x40 config silently drops every mapped `x >= 80` and never lights the second RGB lane (rows 20–39 stay dark).
2. **Call `panel->setPixelBase(8)` before drawing.** The 8-pixel segment width is a property of this panel's driver ICs, not of the remap logic. The default pixel base (panel width, 80) will scramble the image.
3. **`mxconfig.clkphase = false;`** — the SM16208 ICs latch data on the inverted shift clock.

## What the example shows

`loop()` runs three tests in sequence:

1. **Sequential pixel fill (red)** — lights all 80x40 logical pixels in address order. With a correct mapping the fill advances plainly left-to-right, top-to-bottom (the remap absorbs the panel's zigzag wiring). Segments lighting out of order or mirrored within 8-pixel blocks means `setPixelBase()` is wrong or missing.
2. **Colour bars** — red/green/blue/yellow horizontal bands, one per 10-row band; a dark band 3 or 4 exposes a dead RGB2 lane, misplaced bands expose a wrong row fold.
3. **Border, corner markers and text** — proves the logical corners (0,0) and (79,39) land where expected.

## Project layout (PlatformIO)

```
├── platformio.ini          # esp32 env; library deps incl. this repo via ../..
├── src/main.cpp            # the example sketch (Arduino API)
└── README.md
```

## Building

```bash
cd examples/Panel_80x40_ZigZag8
pio run            # build
pio run -t upload  # flash
pio device monitor # serial monitor @ 115200
```

Or open the folder in the PlatformIO IDE (VS Code / Atom) — it auto-detects `platformio.ini`.

- **Board:** `platformio.ini` targets `wemos_d1_mini32` (a generic ESP32 dev board), matching the repo's other PlatformIO example. Change `board` for your hardware; adjust GPIO pins via `mxconfig.gpio` in `setup()` if they differ from the library defaults.
- The source is plain Arduino code: to build it with the Arduino IDE instead, copy `src/main.cpp` to `Panel_80x40_ZigZag8.ino` in a folder of the same name and install the DMA library and Adafruit GFX/Lite through the Library Manager.
