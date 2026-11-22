# Four-Scan & Eight-Scan Pixel Mapping Test and Tune (`VirtualMatrixPanel_T`)

This diagnostic sketch helps you discover the exact coordinate mapping parameters for non-two-scan panels (`1/4`, `1/5`, `1/6`, `1/8`, `1/10`, `1/16` 4-scan and `1/2` / `1/4` 8-scan modules) using `VirtualMatrixPanel_T` — without editing any library headers or subclassing.

> Check the **Pre-Mapping Chipset Checklist** in [`doc/VirtualMatrixPanel_T.md`](../../doc/VirtualMatrixPanel_T.md) first! If you see multiple rows lit simultaneously (`xx--xx`), you likely need `mxconfig.line_decoder = HUB75_I2S_CFG::TYPE595` (for `SM5368`/`RUL5158`/`TC7558`/`DP32020A`) or `TYPE_DIRECT` (for MOSFET-only panels without a decoder chip) before adjusting mapping math.

---

## Step-by-Step Guide (Works via `#define` OR Live Serial Monitor @ 115200)

### 1. Start with a single panel (`NUM_ROWS = 1, NUM_COLS = 1`)
Set `PANEL_RES_X`, `PANEL_RES_Y`, and `PANEL_PARALLEL_ROWS` (`4` for most panels; `8` for 8-scan panels like `32x16 1/2`) and upload.

If rows fill cleanly left-to-right from top to bottom right away, no custom flags are needed:
`FourScanMapping<>`.

### 2. Find `pxbase` (if rows jump across horizontal segments)
If each row draws in short segments of `4`, `8`, `16`, or `32` pixels (or `1` pixel at a time), that segment width is `pxbase`.
- Set `PANEL_PX_BASE` (or type `b 16`, `b 8`, `b 4`, or `b 1` in the Serial Monitor).
- If alternating segments swap top/bottom halves across the width, toggle **`FOUR_SCAN_XOR_SEGMENTS`** (`x` over Serial).

### 3. Fix band order (if upper & lower blocks are swapped)
Once segments form solid horizontal bars, if the lower band fills before the upper band (for example, rows `4–7` fill above `0–3`), enable **`FOUR_SCAN_SWAP_BLOCKS`** (`s` over Serial).

### 4. Fix reversed segment fill direction (`ZIGZAG`)
If every other band draws from right to left instead of left to right inside each `pxbase` block:
- Enable **`FOUR_SCAN_REVERSE_LEFT`** (`l` over Serial) or **`FOUR_SCAN_REVERSE_RIGHT`** (`r` over Serial) depending on which pass fills backwards.

### 5. Print and copy your final C++ mapping
Send `p` in the Serial Monitor to print the exact `FourScanMapping<PxBase, Flags>` (or `EightScanMapping<...>`) declaration to paste into your final sketch (along with `VirtualDisplay::makeDMAConfig(...)`).

If a panel still behaves unusually (e.g., `104x52` column skipping or `32x8` single-lane badges), open a GitHub issue with chip markings, dimensions, and a short video of this sketch running!