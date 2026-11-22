# 64×64 1/16-scan ("16S") 4-scan panel

This document covers 64×64 HUB75 panels marketed as **"16S"** (1/16 scan),
typically P3 indoor modules with MBI5124-compatible driver ICs.

They are *four-scan* panels: each address step drives four physical rows,
but the HUB75 connector only has two RGB lanes. The library therefore feeds
them as a **2·W × H/2** DMA surface and folds the logical coordinates. With the
refactored `VirtualMatrixPanel_T`, [`FourScan64_16S`](../src/ESP32-HUB75-VirtualMatrixPanel_T.hpp)
(alias for `FourScanMapping<0, FOUR_SCAN_NONE, 64>` / `FourScanMapping<>`) computes
that folded **128×32** `HUB75_I2S_CFG` automatically via `makeDMAConfig()`:

```cpp
using PanelScan      = FourScan64_16S;
using VirtualDisplay = VirtualMatrixPanel_T<CHAIN_NONE, PanelScan>;

auto mxconfig = VirtualDisplay::makeDMAConfig(64, 64); // 128x32 automatically
mxconfig.clkphase = false;
mxconfig.driver   = HUB75_I2S_CFG::MBI5124;

auto *dma = new MatrixPanel_I2S_DMA(mxconfig);
dma->begin();
auto *panel = new VirtualDisplay(*dma, mxconfig);
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

For arbitrary `W` and `H = 64` (`half_h = 32`, `q_h = 16`):

```cpp
const int block = y / 16;                          // 0..3
x += ((block & 1) == 0 ? ((x / W) + 1) : (x / W)) * W;
y  = (y / 32) * 16 + (y % 16);                     // fold 64 rows into 32
```

For a single panel `x < W`, so the offsets are `+W` and `+0`. The `(x / W)` terms
make the same mapping work for chained panels: each panel owns a `2·W`-wide
DMA chunk.

## Why not `FourScan64pxBitSwapMapping`?

[`FourScan64pxBitSwapMapping`](../src/ESP32-HUB75-VirtualMatrixPanel_T.hpp)
(legacy `FOUR_SCAN_64PX_HIGH`) is for a *different* 64-pixel-high family (for
example `ICN2037BP` outdoor panels, see issue
[#345](https://github.com/mrcodetastic/ESP32-HUB75-MatrixPanel-DMA/issues/345)).
It folds rows in **8-row** bit-swapped blocks. Using it on a `16S` panel produces
four scrambled vertical strips. If that is what you see, use `FourScan64_16S`
instead (and vice versa).

## Verification

The mapping above is verified in [`testing/test_virtual_matrix_t.cpp`](../testing/test_virtual_matrix_t.cpp)
across single and multi-panel chains against the reference formula in
[`testing/four_scan_64x64_16s.cpp`](../testing/four_scan_64x64_16s.cpp) with zero
mismatches, and confirmed on hardware in [`examples/FourScan_64x64_16S`](../examples/FourScan_64x64_16S).

The legacy `VirtualMatrixPanel` class also accepts
`setPhysicalPanelScanRate(FOUR_SCAN_64PX_HIGH_16S)`.
