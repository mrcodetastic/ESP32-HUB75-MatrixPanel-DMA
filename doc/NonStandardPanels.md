# Non-Standard HUB75 Panel Compatibility Reference (Non-Exhaustive)

This is a **community-verified starting point** of non-two-scan and quirky HUB75 panels reported across library issues and discussions, expressed using the refactored [`VirtualMatrixPanel_T`](./VirtualMatrixPanel_T.md) policies.

> [!WARNING]
> **Batch & Factory Variance Notice:** Identical PCB dimensions (`64x32 8S`, `80x40 10S`, `32x16 4S`), silkscreen markings, and driver ICs from different factories or production batches frequently use different `pxbase`, swapped blocks, or zigzag wiring. Treat entries below as **starting points**—if pixels still scramble, run [`examples/Pixel_Mapping_Test`](../examples/Pixel_Mapping_Test/Pixel_Mapping_Test.ino) using the [4-step guide](./VirtualMatrixPanel_T.md#3-finding-a-panels-mapping-in-4-steps-updated-discussion-622-guide).

---

## 1. Confirmed Four-Scan Panels (`2*W × H/2` DMA via `FourScanMapping`)

With `VirtualMatrixPanel_T::makeDMAConfig(PANEL_RES_X, PANEL_RES_Y, NUM_ROWS, NUM_COLS)`, the `2*W × H/2` DMA size is computed automatically.

| Panel Size | Scan Label | Example Drivers / Mux ICs | Policy Configuration | Extra `HUB75_I2S_CFG` Settings | Source Issues / PRs |
|---|---|---|---|---|---|
| **32×16** | `1/4` (`4S`) | `ICN2037` / `FM6124` / `DP5125` + `138` | `FourScanMapping<>` (`pxbase=32`) | Check `clkphase` | `#204`, `#864` |
| **32×16** | `1/4` (`4S`) | `ICN2037BP` / `SM16208` / `FM6124D` + `138` | `FourScanMapping<8>` or `FourScanMapping<8, FOUR_SCAN_SWAP_BLOCKS>` | `clkphase = false` on `SM16208` | `#622`, `#645`, `#680`, `#799`, `#864`, `#904`, `#930` |
| **32×16** | `1/4` (`4S`) | `SM16206S` / `MP7128GF` (no `138`/`595`) | `FourScanMapping<8>` | `line_decoder = TYPE_DIRECT`, `clkphase = false` | `#905` |
| **32×16** | `1/4` (`4S`) | `SM16208SJ` + `74HC138` (ZigZag8) | `FourScanMapping<8, FOUR_SCAN_REVERSE_RIGHT>` or `FourScanMapping<8, FOUR_SCAN_ZIGZAG_SM16208>` | Check `clkphase = false` | `#622` (Part 3, Reply 4) |
| **32×16** | `1/4` (`4S`) | `SMD3535` (P8) | `FourScanMapping<4, FOUR_SCAN_XOR_SEGMENTS>` | — | `#941` |
| **32×16** | `1/4` (`4S`) | `SM16208SJ` / `RHLP10-3535-16x32-4S` | `FourScanMapping<1>` | — | `#677`, `#875` |
| **32×32** | `1/8` (`8S`) | `ICN2037BP` + `SM5166PC` / `MP5020GP` | `FourScanMapping<16>` or `FourScanMapping<>` | `ICN2038S` / `SHIFTREG` | `#154`, `#578`, `#618`, `#927` |
| **32×32** | `1/8` (`8S`) | `DP5020B` + `DP74HC138D` | `FourScanMapping<0, FOUR_SCAN_SWAP_BLOCKS>` (`pxbase=32`) | — | `#840` |
| **40×20** | `1/5` (`5S`) | `DP5125D` / `FM6047` + `RUC7258D` | `FourScanP8_40x20` (`FourScanMapping<4, FOUR_SCAN_SWAP_BLOCKS \| FOUR_SCAN_XOR_SEGMENTS>`) | Check `clkphase` | `#637` (`ffkarine`), `#836` A/C, PR `#959` |
| **40×20** | `1/5` (`5S`) | `FM6047` + `RUC7258D` (ZigZag8) | `FourScanMapping<8, FOUR_SCAN_ZIGZAG_SM16208>` | — | `#637` (`mjmokhtar`), `#836` B |
| **48×24** | `1/6` (`6S`) | `DP5135` + `DP32020` | `FourScanMapping<>` (`pxbase=48`) | `line_decoder = TYPE595` (`SM5368`) | `#836` D |
| **64×32** | `1/8` (`8S`) | `ICN2037BP` / `SM16208` / `MBI5124` + `138`/`5166` | `FourScanMapping<>`, `FourScanMapping<32>`, or `FourScanMapping<16>` | `clkphase = false` on `SM16208` / chains | `#154`, `#204`, `#580`, `#618`, `#624`, `#711`, `#733`, `#737`, `#866` (`TYPE595`), `#949`, `#966` |
| **64×32** | `1/8` (`8S`) | `HRLOP-P5-1921` (`A/B/C` only) | `FourScanMapping<4>` | — | `#698`, PR `#959` |
| **64×32** | `1/8` (`8S`) | `SM16208SJ` + `SM5368PS` | `FourScanMapping<4, FOUR_SCAN_SWAP_BLOCKS \| FOUR_SCAN_XOR_SEGMENTS>` | `line_decoder = TYPE595`, `driver = DP3246` if needed | `#781` |
| **64×32** | `1/8` (`8S`) | `ICN2037BP+RUC7258E` / `FM6124HJ+595` (`YS-P5`) | `FourScanMapping<1>` | `line_decoder = TYPE595` on `YS-P5` | `#730`, `#955`, Disc. `#898` |
| **80×40** | `1/10` (`10S`) | `ICN2037` / `GKGD` / `SM5166` / `RUC7258` | `FourScanMapping<>`, `FourScanMapping<16>`, or `FourScanMapping<1>` | `line_decoder = TYPE595` if 595 mux | `#627`, `#702`, `#720`, `#871`, `#908`, `#943` |
| **80×40** | `1/10` (`10S`) | `SM16208` / `ICN2037` (ZigZag8) | `FourScan80x40_ZigZag8` (`FourScanMapping<8, FOUR_SCAN_ZIGZAG_SM16208>`) | `clkphase = false` | `#453`, `#720`, PR `#964` |
| **80×40** | `1/10` (`10S`) | `ICN2038` / `ICN2037BP` + `ICND2018` | `FourScan80x40_HFarcan` (`FourScanMapping<16, FOUR_SCAN_XOR_SEGMENTS>`) | `i2sspeed = HZ_20M` if blinking | `#759` |
| **80×40** | `1/10` (`10S`) | `DP5125`/`DP5135` + `DP2020`/`RUL5158C`/`SM5368` | `FourScanMapping<4, FOUR_SCAN_SWAP_BLOCKS>` (or `+ FOUR_SCAN_XOR_SEGMENTS`) | `line_decoder = TYPE595`, `clkphase = false` | `#754`, `#813`, `#828`, `#942` |
| **64×64** | `1/16` (`16S`) | `ICN2037BP` / `SM16208SJ` + `RUC7258D` | `FourScan64pxBitSwapMapping<>` | `clkphase = false` | `#345`, `#468` |
| **64×64** | `1/16` (`16S`) | `MBI5124` (P3 indoor `16S`, four 16-row blocks) | `FourScan64_16S` (`FourScanMapping<>`) | `driver = MBI5124`, `clkphase = false` | PR `#976` |
| **128×64** | `1/16` (`16S`) | `DP5125DP` + `D32019B` | `FourScanMapping<1, FOUR_SCAN_SWAP_BLOCKS>` | — | `#825` |

---

## 2. Confirmed Eight-Scan Panels (`4*W × H/4` DMA via `EightScanMapping`)

Eight-scan panels illuminate 8 physical rows per refresh step and fold DMA geometry to `4*W × H/4` via `EightScanMapping::makeDMAConfig(...)`.

| Panel Size | Scan Label | Example Drivers / Mux ICs | Policy Configuration | Extra `HUB75_I2S_CFG` Settings | Source |
|---|---|---|---|---|---|
| **32×16** | `1/2` (`2S`, 8-scan) | `SM16208SJ` + `4953` MOSFETs (direct mux) | `EightScan32x16_819` (`EightScanMapping<8, EIGHT_SCAN_REVERSE_EVEN_SLOTS \| EIGHT_SCAN_INVERT_ROW_IN_BAND>`) | `line_decoder = TYPE_DIRECT`, `clkphase = false` | Disc. `#819`, `#850` |
| **32×16** | `1/2` (`2S`, 8-scan) | `SM16208SJ` (direct mux, inverted band order) | `EightScan32x16_SM16208` (`EightScanMapping<8, EIGHT_SCAN_REVERSE_BANDS \| EIGHT_SCAN_REVERSE_ODD_SLOTS \| EIGHT_SCAN_INVERT_ROW_IN_BAND>`) | `line_decoder = TYPE_DIRECT`, `clkphase = false` | Disc. `#909`, `#967` |

---

## 3. Unusual / Custom-Stride Modules (Sketch-Local `ScanPolicy`)

| Panel Size | Quirk | Solution | Source |
|---|---|---|---|
| **32×8** | Single RGB lane (`R0/G0/B0` only, `SM16106SC` badge) | DMA `32x16`; reverse X per 8px slice and fold `y &= 7` in custom `ScanPolicy` | `#488` |
| **104×52** | `1/26`-scan module skipping dummy columns every ~16th clock | Configure padded DMA width (`110` or `112`) and offset X in custom `ScanPolicy` | `#608`, `#719` |
| **78×78** | `1/26`-scan `SM5368`/`RU5958` shift-register mux | `line_decoder = TYPE595` + custom width/column alignment policy | `#925` |

---

## 4. Known Incompatible / Non-Mapping Hardware

Do **not** try to fix the following hardware with `VirtualMatrixPanel_T` coordinate mapping—the incompatibility is electrical/protocol level:

- **S-PWM / Self-Refresh Framebuffer Driver ICs:** `ICND1065`, `ICND2153`, `DP3364`, `SM16169SH`, `SM16380`, `LS9930`, `LS9936`, `RUL6024`, `MBI5051/5052/5053`, `MBI6024`, `HX6158SP`, `SUM2033`, `ICND2055CP`.
- **PWM Chips Covered by Dedicated Forks Only:** `ICN2053`, `FM6353` (see the `ESP32-HUB75-MatrixPanel-DMA-ICN2053` fork linked in Discussion `#324`).
- **Unsupported LED Driver (`SM1620B`):** Incompatible latch/shift protocol (`#416`).
- **Panels with 3 or 4 Parallel RGB Sets (HUB75-3RGB / HUB75-4RGB, e.g., `R1..R4`, `#344`, `#938`, Disc. `#945`):** The ESP32 parallel DMA bus drives 2 RGB channels (`RGB1` + `RGB2`) only.
- **HUB12 Monochrome / Single-Color Sign Modules (`#379`, `#701`):** Different electrical interface (not HUB75).
- **Single-Scan Static/Full-Scan Modules Requiring Vertical Tiling (`scan == panel_res_y`, e.g., `64x32 1/32` `#540`, `#914`):** A single panel can sometimes work by doubling `PANEL_RES_Y` (`32 -> 64`), but hardware row wiring prevents standard `VirtualMatrixPanel_T` chaining without panel hardware changes.
