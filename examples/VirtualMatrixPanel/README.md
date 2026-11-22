# The `VirtualMatrixPanel_T` Class

`VirtualMatrixPanel_T` remaps logical virtual `(x, y)` coordinates onto physical LED panel DMA buffers for two use cases (which can be combined freely):

1. **Multi-panel grids** chained in serpentine or zig-zag layouts (e.g., 2×2 of 64×32 panels as a 128×64 display).
2. **Non-standard scan panels** (`1/4`, `1/5`, `1/6`, `1/8`, `1/10`, `1/16` 4-scan, and `1/2` / `1/4` 8-scan panels) whose internal shift chains fold multiple physical rows into each DMA row.

> See [`doc/VirtualMatrixPanel_T.md`](../../doc/VirtualMatrixPanel_T.md) for the full API reference, wiring/IC triage checklist, and step-by-step discovery guide, and [`doc/NonStandardPanels.md`](../../doc/NonStandardPanels.md) for community-tested panel settings.

---

## 1. Chaining Standard Two-Scan Panels

For standard two-scan panels (`1/16` 64×32, `1/32` 64×64):

```cpp
#include <ESP32-HUB75-VirtualMatrixPanel_T.hpp>

using VirtualDisplay = VirtualMatrixPanel_T<CHAIN_TOP_RIGHT_DOWN>;

auto mxconfig = VirtualDisplay::makeDMAConfig(64, 32, 2, 2);
auto *dma_display = new MatrixPanel_I2S_DMA(mxconfig);
dma_display->begin();

auto *virtualDisp = new VirtualDisplay(*dma_display, mxconfig);
```

Refer to [`doc/VirtualMatrixPanel.pdf`](../../doc/VirtualMatrixPanel.pdf) for visual diagrams of the `CHAIN_*` physical wiring layouts.

---

## 2. Non-Standard Four-Scan & Eight-Scan Panels

Four-scan (`2*W × H/2`) and eight-scan (`4*W × H/4`) panels differ in segment width (`PxBase`) and wiring order flags (`FOUR_SCAN_FLAGS` / `EIGHT_SCAN_FLAGS`). Use `FourScanMapping<PxBase, Flags>` or `EightScanMapping<PxBase, Flags>` and let `makeDMAConfig()` derive the folded DMA size automatically:

```cpp
// Example: 64x32 1/8-scan outdoor panel with 16px segment width
using MyScan = FourScanMapping<16>;
using VirtualDisplay = VirtualMatrixPanel_T<CHAIN_NONE, MyScan>;

auto mxconfig = VirtualDisplay::makeDMAConfig(64, 32); // 128x16 automatically!
auto *dma_display = new MatrixPanel_I2S_DMA(mxconfig);
dma_display->begin();

auto *virtualDisp = new VirtualDisplay(*dma_display, mxconfig);
```

Common built-in policies and flags:
- `FourScanMapping<PxBase, Flags, PanelHeight>`: handles `16px`, `20px`, `24px`, `32px`, `40px`, `48px`, `64px` heights automatically; combine `FOUR_SCAN_SWAP_BLOCKS`, `FOUR_SCAN_XOR_SEGMENTS`, `FOUR_SCAN_REVERSE_LEFT`, `FOUR_SCAN_REVERSE_RIGHT` (or `FOUR_SCAN_ZIGZAG_SM16208`).
- `FourScan64_16S`: 64×64 1/16-scan ("16S") modules (`EXAMPLE_NUMBER 5` in `VirtualMatrixPanel.ino`, see [`doc/FourScan_64x64_16S.md`](../../doc/FourScan_64x64_16S.md)).
- `FourScan80x40_ZigZag8`: 80×40 1/10-scan SM16208 "ZIGZAG8" modules (`EXAMPLE_NUMBER 6` in `VirtualMatrixPanel.ino`, see [`doc/Panel_80x40_4Scan_ZigZag8.md`](../../doc/Panel_80x40_4Scan_ZigZag8.md)).
- `FourScan64pxBitSwapMapping<PxBase, Flags>`: 64×64 outdoor modules with 8-row bit-swapped blocks (e.g., `ICN2037BP`, issue `#345`).
- `EightScanMapping<PxBase, Flags>`: `4*W × H/4` modules (e.g., `32x16 1/2` scan).
- `RuntimeFourScanMapping` / `RuntimeEightScanMapping`: runtime-configurable variants used by [`Pixel_Mapping_Test`](../Pixel_Mapping_Test/).
- Custom policies: inherit `ScanPolicyTraits<DmaWidthMult, DmaHeightDiv>` and implement `VirtualCoords map(VirtualCoords, const PanelGeometry&) const`.