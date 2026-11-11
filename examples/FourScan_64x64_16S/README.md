# 64x64 1/16-scan ("16S") panels

This example drives a single 64×64 P3-class "16S" panel (1/16 scan, four
16-row blocks, MBI5124-compatible drivers) with the `VirtualMatrixPanel_T`
class and the built-in `FOUR_SCAN_64PX_HIGH_16S` scan type.

## Wiring / configuration

The DMA engine is configured for **128×32**, not 64×64:

```cpp
HUB75_I2S_CFG mxconfig(PANEL_RES_X * 2, PANEL_RES_Y / 2, 1);
```

That is the same "two rows in parallel" trick used by the other four-scan
types: each DMA row carries two physical panel rows, one in the left half
and one in the right half of the 128-pixel row.

If you have one of these panels and it shows four scrambled vertical strips
with `FOUR_SCAN_64PX_HIGH`, use `FOUR_SCAN_64PX_HIGH_16S` instead. They are
different panel families (8-row blocks vs. 16-row blocks).

## Test pattern

- green border and 8-pixel grid
- corner blocks: top-left red, top-right green, bottom-left blue,
  bottom-right yellow
- both diagonals in white
- "16S" label

If the corners land in the right places and the diagonals are straight, the
mapping is correct.

See [`doc/FourScan_64x64_16S.md`](../../doc/FourScan_64x64_16S.md) for the
mapping table and the derivation.
