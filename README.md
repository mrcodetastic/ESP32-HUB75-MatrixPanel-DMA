# FOUR_SCAN_20PX_HIGH: correct mapping for 40x20 P8 1/5-scan panels

Proposed change to
[ESP32-HUB75-MatrixPanel-DMA](https://github.com/mrcodetastic/ESP32-HUB75-MatrixPanel-DMA)
(against v3.0.14).

## Summary

`FOUR_SCAN_20PX_HIGH` in `ESP32-HUB75-VirtualMatrixPanel_T.hpp` used the generic
four-scan fold. That formula does not drive 40x20 outdoor P8 modules with 1/5
scan — output is scrambled: solid blocks look plausible but text is unreadable
and diagonals break apart.

This replaces it with a mapping measured on real hardware.

## Contents

| Path | What it is |
| --- | --- |
| `FOUR_SCAN_20PX_HIGH.patch` | Unified diff, apply with `git apply` |
| `src/ESP32-HUB75-VirtualMatrixPanel_T.hpp` | The complete modified header |
| `examples/VirtualMatrixPanel_P8_40x20/` | Verification sketch |

To apply:

```sh
cd ESP32-HUB75-MatrixPanel-DMA
git apply /path/to/FOUR_SCAN_20PX_HIGH.patch
```

Or just copy `src/ESP32-HUB75-VirtualMatrixPanel_T.hpp` over the original.

## The change

```diff
-        if (((coords.y) / 10) % 2 == 0) {
-            coords.x += (((coords.x / panel_pixel_base) + 1) * panel_pixel_base);
-        } else {
-            coords.x += ((coords.x / panel_pixel_base) * panel_pixel_base);
-        }
-        coords.y = (coords.y / 20) * 10 + (coords.y % 10);
+        const int panel = coords.x / panel_pixel_base;
+        const int col   = coords.x % panel_pixel_base;
+        const int row   = coords.y;
+
+        const int is_lower = (row / 5) % 2;
+
+        const int u     = is_lower ? col : col + 4;
+        const int dma_x = 16 * (u / 8) + (u % 8) + (is_lower ? 4 : -4);
+
+        coords.x = panel * (panel_pixel_base * 2) + dma_x;
+        coords.y = (row % 5) + 5 * (row / 10);
```

## How the panel actually scans

Derived by lighting known pixels in the DMA buffer and recording where each one
appeared physically.

A single DMA row drives **two physical rows five apart**. With DMA rows `k` and
`k+5` active at one scan address, four physical rows are lit at once — which is
what 1/5 scan on a 20px-high panel means.

Along that DMA row, pixels are handed alternately to the two rows in **segments
of 4 and 8 columns**, and each row fills its own columns left to right:

| DMA columns | Segments (A = upper row, B = lower) |
| --- | --- |
| 0–19 | A4, B8, A8 |
| 20–39 | B8, A8, B4 |
| 40–59 | B4, A8, B8 |
| 60–79 | A8, B8, A4 |

The sequence is **palindromic**: DMA column `x` and column `79-x` always feed the
same physical row. Inverting it, each row takes its columns in groups of 8 that
advance 16 DMA columns at a time, the two rows of the pair offset four columns
in opposite directions — which is the closed form above.

## Verification

`examples/VirtualMatrixPanel_P8_40x20/` cycles five tests. Each targets a
failure the others cannot see:

1. **Squares with diagonals** — squares make the diagonals exactly 45°, so each
   holds one pixel per row. Non-square triangles produce doubled pixels through
   normal rasterisation, which is easy to mistake for a mapping fault. Corners
   must land at `(12,2)-(27,17)` and `(52,2)-(67,17)`.
2. **5x5 checkerboard** — the scan segments are 4 and 8 columns wide, so 5px
   blocks straddle segment boundaries. 10px blocks hide errors: a pixel
   displaced within a same-coloured block is invisible.
3. **Alignment lines** — verticals check column mapping; horizontals at rows
   3/4/5/6 check row *order* across the scan boundary. Verticals cover every row
   at once and are blind to a row permutation.
4. **Mixed-height text** — tall glyphs cross three scan-band boundaries. Per-glyph
   colours make a stray pixel traceable to the letter it came from.
5. **Border and centre cross** — the extreme rows and columns, untouched by the
   other tests.

Confirmed on two chained modules.

## Notes for review

- **This is a behaviour change**, not an addition. Sketches using
  `FOUR_SCAN_20PX_HIGH` with a panel that suited the old generic fold will
  change. If upstream prefers, this can instead be a new enum value
  (e.g. `FOUR_SCAN_20PX_HIGH_P8_40X20`) with the original left untouched.
- The mapping was verified against these panels only. Whether all 40x20 1/5-scan
  modules share this wiring is unknown — panels sold under the same description
  are known to differ.
- `panel_pixel_base` is the module width (40 by default) and is used to split the
  chain; the `*2` reflects the doubled electrical width four-scan panels need.
- Host sketches must configure the DMA driver with the electrical geometry:
  `HUB75_I2S_CFG(PANEL_RES_X * 2, PANEL_RES_Y / 2, chain_length)`.
