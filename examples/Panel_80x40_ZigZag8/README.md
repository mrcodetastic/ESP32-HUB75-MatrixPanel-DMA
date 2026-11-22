# 80x40 ZIGZAG8 panel example (`VirtualMatrixPanel_T`)

Drives a common "outdoor module" style **80x40, 4-scan (1/10 scan) HUB75 panel** built around **SM16208** driver ICs.

The panel:

| Property             | Value                                                                 |
| -------------------- | --------------------------------------------------------------------- |
| Logical resolution   | 80 x 40                                                               |
| Scan                 | 1/10 (A–D, no E), 4 rows lit per address                              |
| Data lanes           | 2 (RGB1 rows 0–19, RGB2 rows 20–39)                                   |
| Shift chain per lane | 160 clocks, two logical rows interleaved per lane                     |
| Quirk                | column order mirrored inside every 8-LED block (asymmetric per phase) |

See [`doc/Panel_80x40_4Scan_ZigZag8.md`](../../doc/Panel_80x40_4Scan_ZigZag8.md) for the panel's data ordering diagrams and hardware explanation.

## How it works with the refactored `VirtualMatrixPanel_T`

The refactored [`VirtualMatrixPanel_T`](../../src/ESP32-HUB75-VirtualMatrixPanel_T.hpp) supports this panel natively with [`FourScan80x40_ZigZag8`](../../src/ESP32-HUB75-VirtualMatrixPanel_T.hpp) (an alias for `FourScanMapping<8, FOUR_SCAN_ZIGZAG_SM16208, 40>`):

```cpp
using PanelScan      = FourScan80x40_ZigZag8;
using VirtualDisplay = VirtualMatrixPanel_T<CHAIN_NONE, PanelScan>;

HUB75_I2S_CFG mxconfig = VirtualDisplay::makeDMAConfig(80, 40); // auto-sizes to 160x20!
mxconfig.clkphase = false;
```

To inspect or adapt the equivalent custom `ScanPolicyTraits<2, 2>` policy implementation (multi-panel aware via `PanelGeometry`), set `#define USE_CUSTOM_POLICY 1` in `src/main.cpp`.

## Panel-specific notes

1. **`VirtualDisplay::makeDMAConfig(80, 40)` handles the `160x20` fold automatically** — you no longer have to manually multiply/divide DMA width/height or call `setPixelBase(8)`.
2. **`mxconfig.clkphase = false;`** — the SM16208 ICs latch data on the inverted shift clock.

## What the example shows

`loop()` runs three tests in sequence:

1. **Sequential pixel fill (red)** — lights all 80x40 logical pixels in address order.
2. **Colour bars** — red/green/blue/yellow horizontal bands, one per 10-row band.
3. **Border, corner markers and text** — proves the logical corners `(0, 0)` and `(79, 39)` land where expected.

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

- **Board:** `platformio.ini` targets `wemos_d1_mini32` (generic ESP32 dev board). Adjust `board` and `mxconfig.gpio` for your wiring.
- For the Arduino IDE, copy `src/main.cpp` to `Panel_80x40_ZigZag8.ino` in a folder of the same name.
