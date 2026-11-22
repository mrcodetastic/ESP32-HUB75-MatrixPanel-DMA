# 64x64 1/16-scan ("16S") panels

This example drives a single 64×64 P3-class "16S" panel (1/16 scan, four 16-row blocks, MBI5124-compatible drivers) with [`VirtualMatrixPanel_T`](../../src/ESP32-HUB75-VirtualMatrixPanel_T.hpp) and the built-in `FourScan64_16S` policy (alias for `FourScanMapping<0, FOUR_SCAN_NONE, 64>` / `FourScanMapping<>`).

## Wiring / configuration

`VirtualDisplay::makeDMAConfig(PANEL_RES_X, PANEL_RES_Y)` automatically configures the DMA surface as **128×32** (`2*W × H/2`), not `64×64`:

```cpp
using PanelScan = FourScan64_16S;
using VirtualDisplay = VirtualMatrixPanel_T<CHAIN_NONE, PanelScan>;

HUB75_I2S_CFG mxconfig = VirtualDisplay::makeDMAConfig(PANEL_RES_X, PANEL_RES_Y);
```

Each DMA row carries two physical panel rows, one in the left half and one in the right half of the 128-pixel DMA row.

If you have one of these panels and it shows four scrambled vertical strips with `FourScan64_16S`, switch the scan policy to `FourScan64pxBitSwapMapping<>` instead (for 8-row bit-swapped panels such as `ICN2037BP`, issue `#345`).

## Test pattern

- green border and 8-pixel grid
- corner blocks: top-left red, top-right green, bottom-left blue, bottom-right yellow
- both diagonals in white
- "16S" label

If the corners land in the right places and the diagonals are straight, the mapping is correct.

See [`doc/FourScan_64x64_16S.md`](../../doc/FourScan_64x64_16S.md) for the mapping table and derivation.
