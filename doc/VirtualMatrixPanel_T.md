# `VirtualMatrixPanel_T` Guide & Coordinate Mapping Manual

[`VirtualMatrixPanel_T`](../src/ESP32-HUB75-VirtualMatrixPanel_T.hpp) maps logical `(x, y)` drawing coordinates onto:
1. **Multi-panel grids** wired in serpentine or zig-zag ribbon chains (`PANEL_CHAIN_TYPE`).
2. **Non-two-scan panels** (`1/4`, `1/5`, `1/6`, `1/8`, `1/10`, `1/16` 4-scan and `1/2` / `1/4` 8-scan modules) whose driver shift chains interleave multiple physical rows per DMA row.
3. **Runtime rotation** (`setRotation(0..3)`) and **compile-time integer scaling** (`ScaleFactor`).

> [!NOTE]
> **Upgrading from older versions?**
> - Legacy `VirtualMatrixPanel` (`ESP32-VirtualMatrixPanel-I2S-DMA.h`) remains unchanged for older sketches.
> - `VirtualMatrixPanel_T` now takes `MatrixPanel_I2S_DMA&` in its constructor, replaces `ScanTypeMapping<PANEL_SCAN_TYPE>` + `setPixelBase()` with parameterized policies (`FourScanMapping<PxBase, Flags>`, `EightScanMapping<PxBase, Flags>`, `RuntimeFourScanMapping`), and provides `makeDMAConfig()` to eliminate manual `2*W × H/2` DMA arithmetic errors.

---

## 1. Quick Start

### Single or Chained Standard Two-Scan Panels (`1/8` 16px, `1/16` 32px, `1/32` 64px)
```cpp
#include <ESP32-HUB75-VirtualMatrixPanel_T.hpp>

using VirtualDisplay = VirtualMatrixPanel_T<CHAIN_TOP_RIGHT_DOWN>;

auto mxconfig = VirtualDisplay::makeDMAConfig(64, 32, 2, 2); // 2x2 grid of 64x32
auto *dma_display = new MatrixPanel_I2S_DMA(mxconfig);
dma_display->begin();

// mxconfig already carries (64, 32, 2, 2) — no need to repeat any dimensions!
auto *virtualDisp = new VirtualDisplay(*dma_display, mxconfig);
virtualDisp->drawDisplayTest();
```

### Non-Two-Scan Panel (e.g., 64×32 `1/8`-Scan Outdoor, `pxbase = 16`)
```cpp
#include <ESP32-HUB75-VirtualMatrixPanel_T.hpp>

using PanelScan      = FourScanMapping<16>;
using VirtualDisplay = VirtualMatrixPanel_T<CHAIN_NONE, PanelScan>;

// Automatically configures DMA as 128x16 (2*W x H/2) and carries 1x1 64x32 metadata
auto mxconfig = VirtualDisplay::makeDMAConfig(64, 32);
mxconfig.clkphase = false;

auto *dma_display = new MatrixPanel_I2S_DMA(mxconfig);
dma_display->begin();

auto *virtualDisp = new VirtualDisplay(*dma_display, mxconfig); // or just (*dma_display)
```

---

## 2. Pre-Mapping Hardware Triage Checklist (Do This First!)

Roughly a third of reported "mapping" issues are actually **row-decoder**, **clock-phase**, or **unsupported S-PWM chip** issues. Check the ICs on the back of your PCB before tuning `FourScanMapping`:

1. **Rule out unsupported S-PWM / embedded-PWM ICs:**
   Chips such as `ICND1065`, `ICND2153`, `DP3364`, `SM16169SH`, `SM16380`, `LS9930`, `LS9936`, `RUL6024`, `SUM2033`, `HX6158SP`, `MBI5051/5052/5053`, `ICN2053`, and `FM6353` do **not** work with standard DMA shift-register timing. Check [`doc/NonStandardPanels.md`](./NonStandardPanels.md#known-incompatible--non-mapping-hardware).

2. **Configure row address multiplexer (`mxconfig.line_decoder`):**
   - `74HC138` / `RUC7258` / `SM5166` (standard binary 3-to-8 decoder): leave default `HUB75_I2S_CFG::TYPE138`.
   - `SM5368` / `RUL5158` / `TC7558` / `DP32020A` / `DP2020` / `RU5958` (shift-register row decoder, often labeled `A/B/C` with no `D/E` even on `1/8` or `1/10` panels):
     ```cpp
     mxconfig.line_decoder = HUB75_I2S_CFG::TYPE595; // alias: SM5368
     ```
     *Symptom if omitted:* Multiple duplicate rows light simultaneously (`xx--xx`) across the panel.
   - Direct MOSFET switch (`4953` / `MP7128` with **no** `138` or `595` decoder IC):
     ```cpp
     mxconfig.line_decoder = HUB75_I2S_CFG::TYPE_DIRECT;
     ```
   - `SM5266P` shift-register mux:
     ```cpp
     mxconfig.line_decoder = HUB75_I2S_CFG::SM5266P;
     ```

3. **Check shift-clock polarity (`mxconfig.clkphase`):**
   `SM16208`, `MBI5124`, and many chained outdoor panels latch on the inverted clock edge. If pixels shift by 1 column, ghost, or corrupt starting on panel #2 in a chain, set:
   ```cpp
   mxconfig.clkphase = false;
   ```

4. **Check driver init (`mxconfig.driver`):**
   `FM6124`, `FM6126A`, `ICN2038S`, `MBI5124`, `DP3246` (only if the panel stays blank or latches weirdly with default `SHIFTREG`).

---

## 3. Finding a Panel's Mapping in 4 Steps (Updated Discussion #622 Guide)

Once every pixel in a single-panel slow fill lights **1 crisp LED at a time** (not 2–4 rows together), flash [`examples/Pixel_Mapping_Test/Pixel_Mapping_Test.ino`](../examples/Pixel_Mapping_Test/Pixel_Mapping_Test.ino) on **1 panel** (`NUM_ROWS = 1, NUM_COLS = 1`):

### Step 1: Pick DMA Fold Factor (`PANEL_PARALLEL_ROWS`)
- Most non-two-scan modules are **4-scan** (`2*W × H/2` DMA): `1/4` (`32x16`), `1/5` (`40x20`), `1/6` (`48x24`), `1/8` (`32x32` or `64x32`), `1/10` (`80x40`), `1/16` (`64x64 16S` or `128x64`). Start with `PANEL_PARALLEL_ROWS = 4`.
- Some ultra-low-scan modules (`1/2` on `32x16`, `1/4` on `32x32`) light 8 rows per step (**8-scan**, `4*W × H/4` DMA). Use `PANEL_PARALLEL_ROWS = 8` with `EightScanMapping`.
- `makeDMAConfig()` computes the folded DMA size automatically.

### Step 2: Find `pxbase` (and Parity `XOR`)
Watch the red pixel fill on a single panel:
- If rows jump back and forth in horizontal strips of `N` columns (`N` is typically `PANEL_RES_X`, `32`, `16`, `8`, `4`, or `1`), that `N` is your **`pxbase`**.
- Adjust `PANEL_PX_BASE` (or type `b 16`, `b 8`, `b 4`, `b 1` in the Serial Monitor at `115200` baud).
- If segments alternate between the top band and bottom band across the width of the panel, toggle **`FOUR_SCAN_XOR_SEGMENTS`** (`x` over Serial).

*(64×64 outdoor `ICN2037BP` note: if `64x64` shows scrambled 8-row strips even with `pxbase = 64`, toggle `6` / `PANEL_BITSWAP_64PX = true` to select `FourScan64pxBitSwapMapping` instead of `FourScanMapping`.)*

### Step 3: Fix Swapped Upper/Lower Bands (`SWAP_BLOCKS`)
When solid horizontal lines form, check whether the lower band inside a half fills *above* the upper band (e.g., rows `4–7` appear above `0–3`):
- If bands are inverted, enable **`FOUR_SCAN_SWAP_BLOCKS`** (`s` over Serial).

### Step 4: Fix Backward Segments (`ZIGZAG`) & Scale to Chained Grids
If one pass fills left-to-right and the other fills right-to-left within each `pxbase`-wide block:
- Enable **`FOUR_SCAN_REVERSE_LEFT`** (`l`) or **`FOUR_SCAN_REVERSE_RIGHT`** (`r`) depending on which half moves backwards.
- Press **`p`** in the Serial Monitor to print the exact `FourScanMapping<PxBase, Flags>` type for your real sketch.
- To chain multiple panels into a grid, replace `CHAIN_NONE` with your wiring layout from [`VirtualMatrixPanel.pdf`](./VirtualMatrixPanel.pdf) (`CHAIN_TOP_RIGHT_DOWN`, `CHAIN_BOTTOM_LEFT_UP_ZZ`, etc.) and pass `(PANEL_RES_X, PANEL_RES_Y, NUM_ROWS, NUM_COLS)` to `makeDMAConfig(...)` alongside `new VirtualDisplay(*dma_display, mxconfig)`. Remember: **draw through `virtualDisp->`, never `dma_display->`** once configured.

---

## 4. API Reference

### Class Template Signature
```cpp
template <PANEL_CHAIN_TYPE ChainScanType = CHAIN_NONE,
          class ScanPolicy = TwoScanMapping,
          int ScaleFactor = 1>
class VirtualMatrixPanel_T;
```

- **`ChainScanType`**: Compile-time chaining mode (`CHAIN_NONE`, `CHAIN_TOP_LEFT_DOWN`, `CHAIN_TOP_RIGHT_DOWN`, `CHAIN_BOTTOM_LEFT_UP`, `CHAIN_BOTTOM_RIGHT_UP`, `*_ZZ`, or `CHAIN_RUNTIME` with `setChainType(...)`).
- **`ScanPolicy`**: Stateless or stateful policy type matching `ScanPolicyTraits` (default `TwoScanMapping`). Zero runtime memory overhead for stateless policies.
- **`ScaleFactor`**: Compile-time integer magnification (`>= 1`). Logical `width()` and `height()` return `(grid_pixels / ScaleFactor)`. Applies across `drawPixel`, `drawPixelRGB888`, `drawPixel(CRGB)`, and `drawDisplayTest()`.

### Built-In Policies & Presets

| Policy / Alias | DMA Surface | Purpose |
|---|---|---|
| `TwoScanMapping` | `1*W × 1*H` | Default linear mapping (`1/8`, `1/16` on `32px`, `1/32` on `64px`). |
| `FourScanMapping<PxBase = 0, Flags = FOUR_SCAN_NONE, PanelHeight = 0>` | `2*W × H/2` | Parameterized 4-scan policy. Auto-uses `panel_res_x` when `PxBase == 0` and `panel_res_y` when `PanelHeight == 0`. |
| `FourScan64pxBitSwapMapping<PxBase = 0, Flags = FOUR_SCAN_NONE>` | `2*W × H/2` | 64px-high 8-row bit-swapped variant (`ICN2037BP`, issue `#345`). |
| `EightScanMapping<PxBase = 0, Flags = EIGHT_SCAN_NONE, PanelHeight = 0>` | `4*W × H/4` | Parameterized 8-scan policy (`1/2` on `16px`, `1/4` on `32px`). |
| `RuntimeFourScanMapping` / `RuntimeEightScanMapping` | `2*W×H/2` / `4*W×H/4` | Runtime-mutable `FourScanConfig` / `EightScanConfig` via `scanPolicy()`. |
| `FourScan16px<...>` / `FourScan32px<...>` / `FourScan40px<...>` | `2*W × H/2` | Explicit-height aliases for `FourScanMapping`. |
| `FourScan64_16S` | `2*W × H/2` | `64x64` `1/16S` four 16-row blocks (`MBI5124`). |
| `FourScanP8_40x20` | `2*W × H/2` | Preset for `40x20` P8 `1/5`-scan (`PxBase=4`, `SWAP_BLOCKS \| XOR_SEGMENTS`). |
| `FourScan80x40_ZigZag8` | `2*W × H/2` | Preset for `80x40` `SM16208` ZIGZAG8 (`PxBase=8`, `FOUR_SCAN_ZIGZAG_SM16208`). |
| `FourScan80x40_HFarcan` | `2*W × H/2` | Preset for `80x40` `pxbase=16` checkerboard (`XOR_SEGMENTS`, issue `#759`). |
| `EightScan32x16_819` / `EightScan32x16_SM16208` | `4*W × H/4` | Tested `32x16 1/2`-scan 8-scan presets (Discussions `#819`, `#909`). |

### Key `FOUR_SCAN_FLAGS` & `EIGHT_SCAN_FLAGS`

- **`FOUR_SCAN_SWAP_BLOCKS`**: Swaps DMA half slots (`block 0 -> left +0`, `block 1 -> right +pxbase`).
- **`FOUR_SCAN_XOR_SEGMENTS`**: Alternates half parity across odd `pxbase` column segments across the panel width.
- **`FOUR_SCAN_REVERSE_LEFT`** (`FOUR_SCAN_ZIGZAG_LEFT`): Reverses X inside segments mapped to the left DMA slot.
- **`FOUR_SCAN_REVERSE_RIGHT`** (`FOUR_SCAN_ZIGZAG_RIGHT`): Reverses X inside segments mapped to the right DMA slot.
- **`FOUR_SCAN_ZIGZAG_SM16208`**: Alias for `FOUR_SCAN_SWAP_BLOCKS | FOUR_SCAN_REVERSE_LEFT`.
- **`EIGHT_SCAN_REVERSE_BANDS`**: Reverses the 4-band slot order (`3 - band`) inside each lane.
- **`EIGHT_SCAN_REVERSE_EVEN_SLOTS`** / **`EIGHT_SCAN_REVERSE_ODD_SLOTS`**: Reverses intra-segment X on slots `(0, 2)` or `(1, 3)`.
- **`EIGHT_SCAN_INVERT_ROW_IN_BAND`**: Reverses row order inside each `H/8` band (e.g., swaps row 0/1 inside `1/2`-scan `32x16` modules).

### Constructors & Key Methods on `VirtualMatrixPanel_T`
- `makeDMAConfig(panel_res_x, panel_res_y, rows = 1, cols = 1[, pinmap])` — returns a `VirtualDMAConfig` (derived from `HUB75_I2S_CFG`, pre-folded to DMA geometry and retaining logical module/grid sizes).
- `VirtualMatrixPanel_T(*dma, mxconfig[, scan_policy])` — **recommended:** uses `VirtualDMAConfig` from `auto mxconfig = VirtualDisplay::makeDMAConfig(...)` without repeating any numbers. (If `mxconfig` was explicitly typed as plain `HUB75_I2S_CFG`, derives `panel_res_x`/`panel_res_y` automatically and assumes `1 × cfg.chain_length`; pass `(*dma, mxconfig, rows, cols)` for 2D grids.)
- `VirtualMatrixPanel_T(*dma[, scan_policy])` / `VirtualMatrixPanel_T(*dma, rows, cols[, scan_policy])` — shorthand deriving `panel_res_x`/`panel_res_y` directly from `dma.getCfg()`.
- `VirtualMatrixPanel_T(*dma, rows, cols, panel_res_x, panel_res_y[, scan_policy])` — explicit constructor overriding all four dimensions (useful when DMA width is padded for skipped columns).
- `isDMAConfigValid()` — `true` if the attached `MatrixPanel_I2S_DMA` framebuffer matches the policy's DMA multipliers (mismatches also log `ESP_LOGE` on construction).
- `getCoords(int16_t x, int16_t y) const` — pure stateless coordinate transform returning `VirtualCoords{x, y}` (`{-1, -1}` if out of bounds).
- `setRotation(uint8_t)` / `getRotation()` — runtime 0°/90°/180°/270° rotation, supported under `Adafruit_GFX`, `USE_GFX_LITE`, and `NO_GFX`.
- `scanPolicy()` — access/mutate runtime policy configuration (`RuntimeFourScanMapping`, etc.).
- `setChainType(PANEL_CHAIN_TYPE)` — updates runtime chain order when `ChainScanType == CHAIN_RUNTIME`.

---

## 5. Writing a Custom `ScanPolicy` (For One-Off Quirky Panels)

Panels with skipped dummy outputs (`104x52`, issues `#608` / `#719`), single-RGB-lane badges (`32x8 SM16106SC`, issue `#488`), or unusual strides can define a sketch-local policy without touching the library:

```cpp
// Example: 104x52 linear panel whose shift register has dummy columns every 16th clock (#719)
struct ColumnSkip16Mapping : ScanPolicyTraits<1, 1> {
  static HUB75_I2S_CFG makeDMAConfig(uint16_t w, uint16_t h, uint16_t chain = 1) {
    // Panel expects 110 DMA clocks per row for 104 physical columns
    return HUB75_I2S_CFG(110, h, chain);
  }

  constexpr VirtualCoords map(VirtualCoords coords, const PanelGeometry &geom) const noexcept {
    const int panel_idx = coords.x / geom.panel_res_x;
    const int local_x   = coords.x % geom.panel_res_x;
    const int skipped   = (local_x + 6) / 15; // hardware-specific dummy column skip
    coords.x = static_cast<int16_t>(panel_idx * 110 + local_x + skipped);
    return coords;
  }
};
```

> [!TIP]
> Always use `geom.panel_res_x` and `geom.panel_res_y` (rather than hardcoded constants) in custom `map()` implementations so your policy automatically works across multi-panel serpentine/zig-zag chains.
