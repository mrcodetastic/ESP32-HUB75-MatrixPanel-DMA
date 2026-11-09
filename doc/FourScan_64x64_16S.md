# 64×64 1/16-scan ("16S") 4-scan panel

This document covers 64×64 HUB75 panels marketed as **"16S"** (1/16 scan),
typically P3 indoor modules with MBI5124-compatible driver ICs.

They are *four-scan* panels: each address step drives four physical rows,
but the HUB75 connector only has two RGB lanes. The library therefore feeds
them as a **2·W × H/2** DMA surface and folds the logical coordinates:

```cpp
HUB75_I2S_CFG mxconfig(128, 32, 1);   // for one 64x64 panel
```

## Mapping

The panel is wired as four 16-row blocks. Two rows of the same block are
driven in parallel, one in each half of the 128-pixel DMA row:

| logical rows | block | DMA x half | DMA rows |
|--------------|:-----:|:----------:|:--------:|
| 0 – 15       | 0     | right      | 0 – 15   |
| 16 – 31      | 1     | left       | 0 – 15   |
| 32 – 47      | 2     | right      | 16 – 31  |
| 48 – 63      | 3     | left       | 16 – 31  |

In code (`ScanTypeMapping<FOUR_SCAN_64PX_HIGH_16S>`):

```cpp
const int block = y >> 4;                          // 0..3
if ((block & 1) == 0) {
    x += ((x / W) + 1) * W;                        // right half
} else {
    x += (x / W) * W;                              // left half
}
y = ((block >> 1) << 4) + (y & 0x0F);              // fold 64 rows into 32
```

For a single panel `x < W`, so the offsets are `+W` and `+0`. The
`(x / W)` terms make the same mapping work for chained panels: each panel
owns a 2·W-wide DMA chunk.

## Why not `FOUR_SCAN_64PX_HIGH`?

`FOUR_SCAN_64PX_HIGH` is for a *different* 64-pixel-high family (for example
ICN2037BP outdoor panels, see issue
[#345](https://github.com/mrcodetastic/ESP32-HUB75-MatrixPanel-DMA/issues/345)).
It folds the rows in **8-row** blocks. Using it on a 16S panel produces four
scrambled vertical strips. If that is what you see, switch to
`FOUR_SCAN_64PX_HIGH_16S`.

## Verification

The mapping above is the same arithmetic as the generic ("any other height")
branch in the `Pixel_Mapping_Test` example for a 64-pixel-high panel. That
branch is known to work on a 64×64 P3 16S module with MBI5124 drivers on an
Adafruit MatrixPortal ESP32-S3, and a host-side comparison of the two
formulas over all 4096 logical pixels (plus a two-panel 128×64 chain) gives
zero mismatches. The test pattern in
[`examples/FourScan_64x64_16S`](../examples/FourScan_64x64_16S) makes it easy
to confirm on a new panel: straight diagonals, correct corner colours, and
text at all four edges.

The legacy `VirtualMatrixPanel` class also accepts
`setPhysicalPanelScanRate(FOUR_SCAN_64PX_HIGH_16S)`.
