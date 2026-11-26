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
   Chips such as `ICND1065`, `ICND2153`, `DP3364`, `SM16169SH`, `SM16380`, `LS9930`, `LS9936`, `RUL6024`, `SUM2033`, `HX6158SP`, `MBI5051/5052/5053`, `ICN2053`, and `FM6353` do **not** work with standard DMA shift-register timing. Check [`doc/NonStandardPanels.md`](./NonStandardPanels.md#4-known-incompatible--non-mapping-hardware).

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
- `VirtualMatrixPanel_T(*dma[, scan_policy])` / `VirtualMatrixPanel_T(*dma, rows, cols[, scan_policy])` — shorthand when `mxconfig` is not passed; derives `panel_res_x`/`panel_res_y` directly from `dma.getCfg()`.
- `VirtualMatrixPanel_T(*dma, rows, cols, panel_res_x, panel_res_y[, scan_policy])` — explicit overload overriding all four dimensions (useful when DMA width is padded for skipped columns). **Note parameter order:** `rows, cols` come **before** `panel_res_x, panel_res_y` (matching legacy `VirtualMatrixPanel`, opposite to `makeDMAConfig(panel_res_x, panel_res_y, rows, cols)`).
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
  static HUB75_I2S_CFG makeDMAConfig(uint16_t /*w*/, uint16_t h, uint16_t chain = 1) {
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
// Construct with explicit logical panel size since DMA width (110) differs from logical width (104):
// auto *virtualDisp = new VirtualMatrixPanel_T<CHAIN_NONE, ColumnSkip16Mapping>(*dma_display, 1, 1, 104, 52);
```

> [!TIP]
> Always use `geom.panel_res_x` and `geom.panel_res_y` (rather than hardcoded constants) in custom `map()` implementations so your policy automatically works across multi-panel serpentine/zig-zag chains.

---

## 6. Community-Verified Panel Configurations

This is a **non-exhaustive starting point** of community-verified HUB75 panels and their `VirtualMatrixPanel_T` / `HUB75_I2S_CFG` settings.

> [!WARNING]
> **Batch & Factory Variance Notice:** Identical PCB dimensions (`64x32 8S`, `80x40 10S`, `32x16 4S`), silkscreen markings, and driver ICs from different factories or production batches frequently use different `pxbase`, swapped blocks, or zigzag wiring. Treat entries below as **starting points**—if pixels still scramble, run [`examples/Pixel_Mapping_Test`](../examples/Pixel_Mapping_Test/Pixel_Mapping_Test.ino) using the [4-step guide](#3-finding-a-panels-mapping-in-4-steps-updated-discussion-622-guide).
>
> For panels and driver ICs known to be **electrically incompatible** (e.g., S-PWM chips, HUB12, 3/4-RGB panels), see [`doc/NonStandardPanels.md`](./NonStandardPanels.md#4-known-incompatible--non-mapping-hardware).

### Standard Two-Scan Panels (`TwoScanMapping`, `1*W × 1*H` DMA)

Standard two-scan modules do not need 4-scan folding, but often require specific `line_decoder`, `driver`, `clkphase`, `gpio.e`, or wiring notes:

| Panel Size | Scan | Example PCB Models / Drivers & Mux ICs | `VirtualMatrixPanel_T` Policy | Extra `mxconfig` & Notes |
| --- | --- | --- | --- | --- |
| **32×16** | `1/8S` | Standard indoor; `P10-2727-8S-32X16-A` (`DP5125D` + `RUC7258D`) | `TwoScanMapping` | Standard two-scan; for `P10-2727-8S-32X16-A`: `i2sspeed = HZ_8M`, `latch_blanking = 4`, `clkphase = false` |
| **32×32** | `1/16S` | `MBI5124` + `ICND2018` mux | `TwoScanMapping` | `driver = MBI5124`, `line_decoder = TYPE595` |
| **64×32** | `1/16S` | `P5(2121)-3264-16S-M5` (`RUL6024` + `RUC7258D`) | `TwoScanMapping` | `driver = SHIFTREG`, `clkphase = false`, `latch_blanking = 1` |
| **64×32** (`2×2` grid) | `1/16S` | Chained `2×2` grid of standard `64×32` panels | `VirtualMatrixPanel_T<CHAIN_TOP_RIGHT_DOWN, TwoScanMapping>` | `makeDMAConfig(64, 32, 2, 2)`, `clkphase = false` |
| **64×64** | `1/32S` | `P3-6464-32S-V2.0` (`ICN2037BP` + `RUC7258D`), `DP5125D` + `VB5628` | `TwoScanMapping` | HUB75E pin 12 mislabeled `GND` on PCB is `D`; connect `D` & `E`, `clkphase = false` |
| **64×64** | `1/32S` | `SM16208SJ` + `SM5166PS` | `TwoScanMapping` | `i2sspeed = HZ_8M`, `latch_blanking = 4`, `clkphase = false`, `gpio.e` |
| **64×64** | `1/32S` | `FM6126A` | `TwoScanMapping` | `driver = FM6126A`, `clkphase = false`, `gpio.e` |
| **64×64** | `1/32S` | Q3 / P3 (`DP5125D` + `DP32020A` / `SM5368PF`) | `TwoScanMapping` | `line_decoder = TYPE595`, `clkphase = false`, `gpio.e` |
| **80×40** | `1/20S` | `P4-320X160-2121-20S-PV1` (`DP5125F` + `DP32019B`) | `TwoScanMapping` | Connect `gpio.e` (`>16` row addresses), `clkphase = false` |
| **80×40** | `1/20S` | P4 indoor (`FM6124D` + `CNCC29`/`TC7262`, or `ICN2038S` + `SM5166P`/`ICN2012`) | `TwoScanMapping` | `driver = FM6124` or `ICN2038S`, connect `gpio.e`, `clkphase = false` |
| **80×40** | `1/20S` | `DP5125D` + `3018SDP` | `TwoScanMapping` | `line_decoder = TYPE595`, `clkphase = false`, `gpio.e` |
| **128×64** | `1/32S` | `K716-128x64-32S-V3.3` (`SM16208SF` + `SM5166PS`) | `TwoScanMapping` | `driver = SHIFTREG`, `gpio.e`, `clkphase = false` |
| **128×64** | `1/32S` | `PH2-128*64-32S-V1.0` (`FM6126A` + `D5958SS`) | `TwoScanMapping` | `driver = FM6126A`, `line_decoder = TYPE595`, `latch_blanking = 3`, `clkphase = false`, `gpio.e` |
| **128×64** | `1/32S` | `ICN2037BP` + `RUL5158C` (`A/B/C` mux) | `TwoScanMapping` | `line_decoder = TYPE595`, `clkphase = false`, `gpio.e` |

### Four-Scan Panels (`FourScanMapping` / `FourScan64pxBitSwapMapping`, `2*W × H/2` DMA)

With `VirtualDisplay::makeDMAConfig(PANEL_RES_X, PANEL_RES_Y, NUM_ROWS, NUM_COLS)`, the `2*W × H/2` DMA geometry is computed automatically.

| Panel Size | Scan | Example PCB Models / Drivers & Mux ICs | `VirtualMatrixPanel_T` Policy | Extra `mxconfig` & Notes |
| --- | --- | --- | --- | --- |
| **32×16** | `1/4S` | `ICN2037` / `FM6124` / `DP5125` + `74HC138` | `FourScanMapping<>` (`pxbase = 32`) | Check `clkphase = false` |
| **32×16** | `1/4S` | `P10(3535)32x16-4S`, SzLightall `P10 320x160 1/4 scan V1.1`, `ICN2037BP` / `SM16208` / `FM6124D` / `DP5125D` + `74HC138` | `FourScanMapping<8>` or `FourScanMapping<8, FOUR_SCAN_SWAP_BLOCKS>` | `clkphase = false` (also works in `2×2` grids via `CHAIN_TOP_RIGHT_DOWN` + `makeDMAConfig(32, 16, 2, 2)`) |
| **32×16** | `1/4S` | P8 `SM16206S` / `TC5020AP` + `MP7128GF` / `DP4953` (no `138`/`595`) | `FourScanMapping<8>` | `line_decoder = TYPE_DIRECT`, `clkphase = false` |
| **32×16** | `1/4S` | `SM16208SJ` + `74HC138` (ZigZag8) | `FourScanMapping<8, FOUR_SCAN_REVERSE_RIGHT>` or `FourScanMapping<8, FOUR_SCAN_ZIGZAG_SM16208>` | Check `clkphase = false` |
| **32×16** | `1/4S` | P8 `SMD3535` | `FourScanMapping<4, FOUR_SCAN_XOR_SEGMENTS>` | — |
| **32×16** | `1/4S` | `RHLP10-3535-16x32-4S`, `SM16208SJ` + `DP74HC138B` + `MW245B` + `MW4953F` | `FourScanMapping<1>` | `clkphase = false` (supports `1×2` chains via `makeDMAConfig(32, 16, 1, 2)`) |
| **32×32** | `1/8S` | `TC5020AP`, `ICN2037BP` + `SM5166PC` / `MP5020GP` | `FourScanMapping<>` or `FourScanMapping<16>` | `driver = ICN2038S` or `SHIFTREG`, `clkphase = false` |
| **32×32** | `1/8S` | `P6(3535) 32x32-8S-V1.6` (`ICN5020B`) | `FourScanMapping<16, FOUR_SCAN_SWAP_BLOCKS>` | `clkphase = false` |
| **32×32** | `1/8S` | `P5-3232-3528-8S-A2` (`DP5020B` + `DP74HC138D`) | `FourScanMapping<0, FOUR_SCAN_SWAP_BLOCKS>` (`pxbase = 32`) | `clkphase = false` |
| **40×20** | `1/5S` | P8 `DP5125D` / `FM6047` + `RUC7258D` (4-px checkerboard) | `FourScanP8_40x20` (`FourScanMapping<4, FOUR_SCAN_SWAP_BLOCKS \| FOUR_SCAN_XOR_SEGMENTS>`) | `clkphase = false` |
| **40×20** | `1/5S` | `P8(3535)-20X40-5S-V1.2` (`FM6047` + `RUC7258D`, ZigZag8) | `FourScanMapping<8, FOUR_SCAN_ZIGZAG_SM16208>` | `clkphase = false` |
| **48×24** | `1/6S` | QiangLi P6.67 (`DP5135` + `DP32020`) | `FourScanMapping<>` (`pxbase = 48`) | `line_decoder = TYPE595`, `clkphase = false` |
| **64×32** | `1/8S` | `P4-6432-8S-V2.0`, `P4-1921-64x32-8s-s1`, `ICN2037BP` / `SM16208` + `RUC7258D` / `SM5166PC` | `FourScanMapping<>` (`pxbase = 64`) | `clkphase = false` (fixes `74HC245` skew on `1×2`, `2×1`, and `2×2` chains) |
| **64×32** | `1/8S` | P5 Outdoor (`ICN2038S` + `ICN2012`), `SM16208SJ` + `RUL5158C`, Q5 `DP5125D` + `DP32020A` | `FourScanMapping<32>` | `clkphase = false`; set `driver = ICN2038S` or `line_decoder = TYPE595` per PCB |
| **64×32** | `1/8S` | `P5-08S-1921` (`MBI5124` + `SM5166`), generic `pxbase = 16` | `FourScanMapping<16>` | `driver = MBI5124` (if MBI5124), `clkphase = false` |
| **64×32** | `1/8S` | `P5-1921-64*32-8S-S2` / `HRLOP-P5-1921/V2.0` (`SM16208S`/`ICN2038S` + `ICN2012`, `A/B/C` only) | `FourScanMapping<4>` | `clkphase = false` |
| **64×32** | `1/8S` | P5 `SM16208SJ` + `SM5368PS` | `FourScanMapping<4, FOUR_SCAN_SWAP_BLOCKS \| FOUR_SCAN_XOR_SEGMENTS>` | `line_decoder = TYPE595`, `clkphase = false` (`driver = DP3246` if needed) |
| **64×32** | `1/8S` | `YS-P5-320X160(PRO)-8S-1921-GYGY-V1` (`FM6124HJ` + 595), `ICN2037BP` + `RUC7258E` | `FourScanMapping<1>` | `driver = FM6124` & `line_decoder = TYPE595` on `YS-P5`, `clkphase = false` |
| **80×40** | `1/10S` | P4 `SM16208` + `TC7559` / `SM5368`, `ICN2037` / `GKGD` / `SM5166` / `RUC7258` | `FourScanMapping<>`, `FourScanMapping<16>`, or `FourScanMapping<1>` | `line_decoder = TYPE595` if 595 mux, `clkphase = false` |
| **80×40** | `1/10S` | `GKGD P4-8040-10S-V2.3` (`ICN2037` + `ICN2012`), `ICN2038S`, `ICN2037BP` + `RUC7258D` | `FourScanMapping<16>` | `driver = FM6124` / `ICN2038S` / `SHIFTREG`, `latch_blanking = 4`, `clkphase = false` |
| **80×40** | `1/10S` | `P4-80*40-10S-V2.0` (`SM16208` / `ICN2037`, ZigZag8) | `FourScan80x40_ZigZag8` (`FourScanMapping<8, FOUR_SCAN_ZIGZAG_SM16208>`) | `clkphase = false` |
| **80×40** | `1/10S` | `ICN2038` / `ICN2037BP` + `ICND2018` | `FourScan80x40_HFarcan` (`FourScanMapping<16, FOUR_SCAN_XOR_SEGMENTS>`) | `i2sspeed = HZ_20M` if blinking |
| **80×40** | `1/10S` | P4 `DP5125D` / `DP5135` / `SM16208` + `DP32020A` / `DP2020` / `RUL5158C` / `SM5368` | `FourScanMapping<4, FOUR_SCAN_SWAP_BLOCKS>`, `FourScanMapping<4, FOUR_SCAN_XOR_SEGMENTS>`, or both flags | `line_decoder = TYPE595`, `clkphase = false` |
| **64×64** | `1/16S` | Outdoor `ICN2037BP` / `SM16208SJ` + `RUC7258D` (8-row bit-swapped, single or `2×3` grid) | `FourScan64pxBitSwapMapping<>` | `clkphase = false` (chains with `CHAIN_TOP_RIGHT_DOWN`) |
| **64×64** | `1/16S` | P3 indoor `16S` (`MBI5124`, four 16-row blocks) | `FourScan64_16S` (`FourScanMapping<0, FOUR_SCAN_NONE, 64>`) | `driver = MBI5124`, `clkphase = false` (see [`doc/FourScan_64x64_16S.md`](./FourScan_64x64_16S.md)) |
| **128×64** | `1/16S` | `DP5125DP` + `D32019B` | `FourScanMapping<1, FOUR_SCAN_SWAP_BLOCKS>` | — |

### Eight-Scan Panels (`EightScanMapping`, `4*W × H/4` DMA)

| Panel Size | Scan | Example PCB Models / Drivers & Mux ICs | `VirtualMatrixPanel_T` Policy | Extra `mxconfig` & Notes |
| --- | --- | --- | --- | --- |
| **32×16** | `1/2S` *(sometimes labeled `1/4` or `1/8`)* | `SM16208SJ` / `SM16208S` + `4953` direct MOSFET mux | `EightScan32x16_819` or `EightScan32x16_SM16208` | `line_decoder = TYPE_DIRECT`, `clkphase = false`, `latch_blanking = 4` |
| **32×32** | `1/4S` | `P6(3535)-32X32-4S-72SB-22F` (`MBI5124GP`) | Custom `ScanPolicyTraits<4, 4>` (`4*W × H/4` DMA) | `clkphase = false`, sketch-local band/slot policy |

### Single-Scan (`scan == panel_res_y`) & Custom-Stride Panels

| Panel Size | Type / Quirk | Example PCB Models / Drivers | `VirtualMatrixPanel_T` Policy | Extra `mxconfig` & Notes |
| --- | --- | --- | --- | --- |
| **48×16** | Single-scan (`1/16S`) | P3 `MBI5124` | `TwoScanMapping` with `makeDMAConfig(48, 32)` | Double DMA height (`32`), `driver = MBI5124`; draw within `y = 0..15` |
| **64×32** | Single-scan (`1/32S`) | P4 / `EICEG-P2.5-64X32-32s-v1.3` (`FM6124`) | `TwoScanMapping` with `makeDMAConfig(64, 64)` | Double DMA height (`64`) + connect `gpio.e` (+ `driver = FM6124` if needed); draw within `y = 0..31` |
| **32×8** | Single RGB lane (`R0/G0/B0` only) | `SM16106SC` badge | Custom `ScanPolicy` (DMA `32×16`) | Reverse X per 8px slice and fold `y &= 7` |
| **78×78** | `1/26S` 3-row folding | `SM5368PS` / `RU5958DSP` mux | Custom 3-row folding `ScanPolicy` (`makeDMAConfig(78, 78)`) | `line_decoder = TYPE595`, `latch_blanking = 4`, `clkphase = false` |
| **104×52** | `1/26S` dummy column skip | `P2.5-10452-26S-V2.0` (`ICN2037BP`, `110`/`112` clocks) | Custom `ScanPolicyTraits<1, 1>` (see [Section 5](#5-writing-a-custom-scanpolicy-for-one-off-quirky-panels)) | Custom `makeDMAConfig` width `110` or `112`; construct `(*dma_display, 1, 1, 104, 52)` + `gpio.e` |
| **64×32** | Horizontal mirroring | Any standard `64×32` | Custom `ScanPolicyTraits<1, 1>` | Sketch-local policy returning `geom.virtual_res_x - 1 - coords.x` |

