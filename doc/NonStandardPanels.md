# Non-Standard & Incompatible HUB75 Panels

## Community-Verified Compatible Panels

The unified table of community-verified **two-scan**, **four-scan**, **eight-scan**, **single-scan**, and **custom-stride** HUB75 panels (along with their `VirtualMatrixPanel_T` policies and `HUB75_I2S_CFG` settings) has moved to:

- **[`doc/VirtualMatrixPanel_T.md` — Section 6: Community-Verified Panel Configurations](./VirtualMatrixPanel_T.md#6-community-verified-panel-configurations)**

If your panel isn't listed—or behaves differently due to factory/batch variance—follow the [4-step mapping guide](./VirtualMatrixPanel_T.md#3-finding-a-panels-mapping-in-4-steps-updated-discussion-622-guide) using [`examples/Pixel_Mapping_Test`](../examples/Pixel_Mapping_Test/Pixel_Mapping_Test.ino).

---

<a id="4-known-incompatible--non-mapping-hardware"></a>
## Known Incompatible / Non-Mapping Hardware

Do **not** try to fix the following hardware with `VirtualMatrixPanel_T` coordinate mapping—the incompatibility is electrical/protocol level:

- **S-PWM / Self-Refresh Framebuffer Driver ICs:** `ICND1065`, `ICND2153`, `DP3364`, `SM16169SH`, `SM16380`, `LS9930`, `LS9936`, `RUL6024`, `MBI5051/5052/5053`, `MBI6024`, `HX6158SP`, `SUM2033`, `ICND2055CP`, `ICND2046AP`.
- **PWM Chips Covered by Dedicated Forks Only:** `ICN2053`, `FM6353` (see the `ESP32-HUB75-MatrixPanel-DMA-ICN2053` fork linked in Discussion `#324`).
- **Unsupported LED Driver (`SM1620B`):** Incompatible latch/shift protocol.
- **Panels with 3 or 4 Parallel RGB Sets (HUB75-3RGB / HUB75-4RGB, e.g., `R1..R4`):** The ESP32 parallel DMA bus drives 2 RGB channels (`RGB1` + `RGB2`) only.
- **HUB12 Monochrome / Single-Color Sign Modules:** Different electrical interface (not HUB75).
- **Single-Scan Static/Full-Scan Modules Requiring Vertical Tiling (`scan == panel_res_y`, e.g., `64x32 1/32S`):** A single panel can sometimes work by doubling `PANEL_RES_Y` (`32 -> 64`) and drawing within `y = 0..31`, but hardware row wiring prevents standard `VirtualMatrixPanel_T` chaining without panel hardware changes.
