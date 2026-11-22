# HUB75 RGB LED Matrix Panel Library Using ESP32 DMA

__[BUILD OPTIONS](/doc/BuildOptions.md) | [VIRTUAL PANEL & SCAN GUIDE](/doc/VirtualMatrixPanel_T.md) | [NON-STANDARD PANELS](/doc/NonStandardPanels.md) | [EXAMPLES](/examples/README.md)__ | [![ESP-IDF CI](https://github.com/mrcodetastic/ESP32-HUB75-MatrixPanel-DMA/actions/workflows/esp-idf-with-gfx.yml/badge.svg)](https://github.com/mrcodetastic/ESP32-HUB75-MatrixPanel-DMA/actions/workflows/esp-idf-with-gfx.yml) [![PlatformIO CI](https://github.com/mrcodetastic/ESP32-HUB75-MatrixPanel-DMA/actions/workflows/pio_arduino_ci.yml/badge.svg)](https://github.com/mrcodetastic/ESP32-HUB75-MatrixPanel-DMA/actions/workflows/pio_arduino_ci.yml)

## Table of Contents

- [Introduction](#introduction)
  * [Features](#features)
- [Hardware Compatibility](#hardware-compatibility)
  * [Supported ESP32 Variants](#supported-esp32-variants)
  * [A quick introduction to LED Matrix Panel 'Scan' types](#a-quick-introduction-to-led-matrix-panel-scan-types)
  * [Tested Driver Chips](#tested-driver-chips)
  * [Unsupported Panels & Driver Chips](#unsupported-panels--driver-chips)
  * [Memory Requirements](#memory-requirements)
  * [Important Hardware Notes](#important-hardware-notes)
- [Getting Started](#getting-started)
  * [1. Library Installation](#1-library-installation)
  * [2. Wiring the ESP32 to an LED Matrix Panel](#2-wiring-the-esp32-to-an-led-matrix-panel)
  * [3. Running a Test Sketch](#3-running-a-test-sketch)
- [Usage & Configuration](#usage--configuration)
  * [Chaining Multiple Panels & Non-Standard Scan Panels (`VirtualMatrixPanel_T`)](#chaining-multiple-panels--non-standard-scan-panels-virtualmatrixpanel_t)
  * [Line Decoder Selection](#line-decoder-selection)
  * [Adjusting Panel Brightness](#adjusting-panel-brightness)
  * [Build-Time Options](#build-time-options)
  * [Latch Blanking](#latch-blanking)
  * [Clock Phase](#clock-phase)
  * [Power Requirements](#power-requirements)
- [Community & Acknowledgments](#community--acknowledgments)
  * [Inspiration](#inspiration)
  * [Projects Using This Library](#projects-using-this-library)
  * [Thanks & Acknowledgments](#thanks--acknowledgments)
  * [Support & Donations](#support--donations)
  * [Music](#music)

---

# Introduction

* An Arduino and ESP-IDF (4.x / 5.x / 6.x) library for HUB75 and HUB75E RGB LED panels using ESP32 hardware DMA.
* Works out of the box with standard HUB75 panels where two rows are updated at the same time (referred to as "two scan" panels in this document).
* Supports non-two-scan panels (`1/4`, `1/5`, `1/6`, `1/8`, `1/10`, `1/16` 4-scan and `1/2` / `1/4` 8-scan modules) via [`VirtualMatrixPanel_T`](/doc/VirtualMatrixPanel_T.md). See the [VirtualMatrixPanel example](/examples/VirtualMatrixPanel/), [Pixel_Mapping_Test](/examples/Pixel_Mapping_Test/), and the [Non-Standard Panel Compatibility List](/doc/NonStandardPanels.md).
* Uses hardware DMA engines (**I2S LCD Mode** on ESP32/ESP32-S2, **LCD_CAM + GDMA** on ESP32-S3, and **PARLIO + GDMA** on ESP32-P4/ESP32-S31) for high-speed output with low CPU usage.

## Features

- **Low CPU usage** — Pixel data streams directly using hardware DMA without using CPU resources.
- **Fast performance** — Fast bitwise operations on the DMA buffer avoid pin toggling or blocking calls.
- **Full-screen BCM** — Uses [binary-code modulation](http://www.batsocks.co.uk/readme/art_bcm_5.htm) across the entire matrix for smooth color depth and brightness.
- **Variable color depth** — Supports up to 24-bit TrueColor depending on panel size and refresh rate.
- **Natural dimming** — Includes [CIE 1931 luminance correction](https://ledshield.wordpress.com/2012/11/13/led-brightness-to-your-eye-gamma-correction-no/) for human eye brightness perception.
- **Adafruit GFX compatible** — Works with Adafruit GFX, a simplified GFX implementation (`GFX_Lite`), or standalone without any GFX library (`NO_GFX`).

---

# Hardware Compatibility

> **Important Buying Advice:**
> Before buying an LED matrix panel, ask the seller to confirm that the panel is **NOT** an "S-PWM" chip and uses standard shift-register driver chips such as the FM6124, ICN2037, SM16208, or DP5125D.

New panel driver chips are released frequently. Always check with the seller before purchasing.

## Supported ESP32 Variants

| Variant | Architecture | Hardware DMA Engine | Status |
| :--- | :--- | :--- | :--- |
| **ESP32** (Original, e.g. ESP-WROOM-32) | Xtensa LX6 | I2S Parallel LCD Mode | Supported |
| **ESP32-S2** | Xtensa LX7 | I2S Parallel LCD Mode | Supported |
| **ESP32-S3** | Xtensa LX7 | LCD_CAM + GDMA | Supported (supports Octal PSRAM via `SPIRAM_DMA_BUFFER`) |
| **ESP32-P4** | RISC-V | PARLIO TX + GDMA | Supported |
| **ESP32-S31** | RISC-V | PARLIO TX + GDMA | Experimental (pending hardware bring-up) |

*Note: **ESP32-C2, ESP32-C3, ESP32-C5, ESP32-C6, and ESP32-H2** are **not supported** — they either lack a parallel LCD/PARLIO DMA engine or, in the case of C6/H2, PARLIO cannot control continuous transaction lengths in a hardware loop.*

## A quick introduction to LED Matrix Panel 'Scan' types

* **"Two scan" panels** (two rows updated in parallel):
  * 64x32 indoor panels (1/16 scan rate).
  * 64x64 indoor panels (1/32 scan rate).

* **"Four scan" panels** (four rows updated in parallel — `2*W x H/2` DMA via `FourScanMapping` in [`VirtualMatrixPanel_T`](/doc/VirtualMatrixPanel_T.md)):
  * 32x16 (`1/4`), 40x20 (`1/5`), 48x24 (`1/6`), 32x32 / 64x32 (`1/8`), and 80x40 (`1/10`) panels, including swapped-band, checkerboard-XOR, and ZIGZAG8 variants via `FourScanMapping<PxBase, Flags>` (see [`Pixel_Mapping_Test`](/examples/Pixel_Mapping_Test/) and [Non-Standard Panel List](/doc/NonStandardPanels.md)).
  * 80x40 `1/10`-scan SM16208 ZIGZAG8 panels using `FourScan80x40_ZigZag8`. See [80x40 4-scan (ZIGZAG8)](/doc/Panel_80x40_4Scan_ZigZag8.md).
  * 64x64 `1/16`-scan ("16S") panels folded in four 16-row blocks using `FourScan64_16S` (or `FourScan64pxBitSwapMapping` for 8-row bit-swapped outdoor panels). See [64x64 16S](/doc/FourScan_64x64_16S.md).

* **"Eight scan" panels** (eight rows updated in parallel — `4*W x H/4` DMA via `EightScanMapping` in [`VirtualMatrixPanel_T`](/doc/VirtualMatrixPanel_T.md)):
  * Ultra-low-scan modules such as `32x16 1/2`-scan (`EightScan32x16_819`, `EightScan32x16_SM16208`).

For technical background on matrix panel hardware, read [this SparkFun article](https://www.sparkfun.com/news/2650).

![Panel Scan Types](doc/ScanRateGraphic.jpg)

## Tested Driver Chips

The following driver chips work well with this library:
* Standard shift-register drivers (`SHIFTREG`): `ICN2037`/`ICN2037BP`, `SM16208`, `DP5125D`/`DP5125F`, `TC5020AP`, `ICND2012`, [`RUC7258`](http://www.ruichips.com/en/products.html?cateid=17496)
* `FM6126A`, `ICN2038S`, [`FM6124`](https://datasheet4u.com/datasheet-pdf/FINEMADELECTRONICS/FM6124/pdf.php?id=1309677) (See the [PatternPlasma](/examples/2_PatternPlasma) example)
* `MBI5124`
* `SM5266P` (`mxconfig.line_decoder = HUB75_I2S_CFG::SM5266P`)
* `DP3246` with `SM5368` / `DP32020A` shift-register row decoders (`mxconfig.driver = HUB75_I2S_CFG::DP3246; mxconfig.line_decoder = HUB75_I2S_CFG::TYPE595;`)

## Unsupported Panels & Driver Chips

The following panels and driver chips are known to be **NOT supported** (see the full list in [Non-Standard Panels](/doc/NonStandardPanels.md#4-known-incompatible--non-mapping-hardware)):
* **S-PWM or PWM chipset-based panels** (e.g., `ICND1065`, `ICND2153`, `DP3364`, `DP3356`, `DP3264S`, `SM16169SH`, `SM16380`, `LS9930`/`LS9931`/`LS9936`, `MBI5051`/`5052`/`5053`/`6024`, `HX6158SP`, `ICND2055CP`). These panels contain their own internal framebuffers and PWM counters, which are incompatible with this library.
* [SM1620B](https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-DMA/issues/416)
* Panels using `RUL5358` or `SHIFTREG_ABC_BIN_DE`.
* Panels using `ICN2053` or `FM6353`. Use [this library fork](https://github.com/LAutour/ESP32-HUB75-MatrixPanel-DMA-ICN2053) instead (see [discussion](https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-DMA/discussions/324)).

If your panel is not supported, consider using the [PxMatrix library](https://github.com/2dom/PxMatrix).

## Memory Requirements

DMA buffers use the microcontroller's internal SRAM memory. Check the [Memory Calculator](/doc/memcalc.md) to calculate expected SRAM usage.

![Memory Calculator](doc/memcalc.jpg)

* **ESP32-S3:** You can store the DMA buffer in external PSRAM **only if using Octal SPI PSRAM** (e.g., ESP32-S3-N8R8 / N16R8) by defining `-DSPIRAM_DMA_BUFFER`. Do not use Quad-SPI (Q-SPI) PSRAM because it is too slow. See [Build Options](/doc/BuildOptions.md) to enable PSRAM.
* **ESP32-P4:** Features 768 KB internal SRAM and PARLIO + GDMA cache synchronization.
* **Standard ESP32 & ESP32-S2:** You must use **internal SRAM**. You are limited to available internal RAM (~200 KB free memory), regardless of attached external PSRAM.

## Important Hardware Notes

* High-frequency DMA signals may cause Wi-Fi interference on some board designs.
* The **Adafruit MatrixPortal S3** can experience Wi-Fi radio issues with this library. It is not recommended if you need Wi-Fi. See [discussion](https://github.com/mrcodetastic/ESP32-HUB75-MatrixPanel-DMA/discussions/258#discussioncomment-12274566).

---

# Getting Started

## 1. Library Installation

* **Arduino IDE:** Open **Library Manager**, install **Adafruit_GFX**, then search for and install **ESP32 HUB75 LED MATRIX PANEL DMA Display**.
* **PlatformIO:** Add the library to the `lib/` folder or add it to `lib_deps` inside `platformio.ini`. See [Build Options](/doc/BuildOptions.md) and [PlatformIO lib_deps](https://docs.platformio.org/en/latest/projectconf/section_env_library.html#lib-deps).
* **ESP-IDF (4.x / 5.x / 6.x):** Add via [`idf_component.yml`](/idf_component.yml) (`idf.py add-dependency`) or clone into your project's `components/` directory. See the [ESP-IDF examples](/examples/esp-idf/).

## 2. Wiring the ESP32 to an LED Matrix Panel

Check default pin maps in `default-pins.hpp` under the [platforms folder](https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-DMA/tree/master/src/platforms).

You can customize pin assignments in code:

```cpp
// Change these pins as needed
#define R1_PIN 25
#define G1_PIN 26
#define B1_PIN 27
#define R2_PIN 14
#define G2_PIN 12
#define B2_PIN 13
#define A_PIN 23
#define B_PIN 19
#define C_PIN 5
#define D_PIN 17
#define E_PIN -1 // Required for 1/32 scan panels (e.g., 64x64px). Use any open pin, e.g., IO32
#define LAT_PIN 4
#define OE_PIN 15
#define CLK_PIN 16

HUB75_I2S_CFG::i2s_pins _pins={R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN, A_PIN, B_PIN, C_PIN, D_PIN, E_PIN, LAT_PIN, OE_PIN, CLK_PIN};
HUB75_I2S_CFG mxconfig(
	64, // Module width
	32, // Module height
	2, // Chain length
	_pins // Pin mapping
);
dma_display = new MatrixPanel_I2S_DMA(mxconfig);
```

> **Grounding:** Connect at least one Ground (GND) pin on the HUB75 connector to an ESP32 GND pin to prevent visual artifacts.

### Using Adapter Boards & Shields

Custom adapter PCBs make wiring easier:
* Brian Lough's [ESP32 I2S Matrix Shield](https://github.com/rorosaurus/esp32-hub75-driver)
* Charles Hallard's [WeMos Matrix Shield](https://github.com/hallard/WeMos-Matrix-Shield-DMA)
* Bogdan Sass's [Morph Clock Shield](https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA/discussions/110#discussioncomment-861152)
* [Matouch 1.28" ToolSet RGB LED Matrix](https://www.makerfabs.com/matouch-1-28-toolset-rgb-led-matrix.html)

### Configuring Off-the-Shelf Boards (e.g., Adafruit MatrixPortal)

Find your board's pin numbers in the Adafruit Protomatter examples (e.g., [doublebuffer_scrolltext.ino](https://github.com/adafruit/Adafruit_Protomatter/blob/master/examples/doublebuffer_scrolltext/doublebuffer_scrolltext.ino)) and update your `#define` values.

**Example for MatrixPortal S3:**

Protomatter definition:
```cpp
uint8_t rgbPins[]  = {42, 41, 40, 38, 39, 37};
uint8_t addrPins[] = {45, 36, 48, 35, 21};
uint8_t clockPin   = 2;
uint8_t latchPin   = 47;
uint8_t oePin      = 14;
```

Library config:
```cpp
#define R1_PIN 42
#define G1_PIN 41
#define B1_PIN 40
#define R2_PIN 38
#define G2_PIN 39
#define B2_PIN 37
#define A_PIN  45
#define B_PIN  36
#define C_PIN  48
#define D_PIN  35
#define E_PIN  21 
#define LAT_PIN 47
#define OE_PIN  14
#define CLK_PIN 2

HUB75_I2S_CFG::i2s_pins _pins={R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN, A_PIN, B_PIN, C_PIN, D_PIN, E_PIN, LAT_PIN, OE_PIN, CLK_PIN};

HUB75_I2S_CFG mxconfig(
  PANEL_RES_X,   // Module width
  PANEL_RES_Y,   // Module height
  PANEL_CHAIN,   // Chain length
  _pins          // Pin mapping
);
```

### Using 64x64 Square Panels (HUB75E)

For 64x64 panels (HUB75E), you **must** assign a valid `E_PIN` and connect it to the E pin on the panel header.

## 3. Running a Test Sketch

Call `begin()` before drawing shapes, lines, or text.

Minimal example:
```cpp
void setup() {
    dma_display->begin();
    dma_display->drawPixel(0, 0, dma_display->color565(255, 255, 255)); // Draw a white dot at top-left
}

void loop() {}
```

After verifying basic operation, test with the [PIO Test Patterns](/examples/PIO_TestPatterns) example. This sketch displays test patterns to help identify flickering, ghosting, or pin mapping issues.

---

# Usage & Configuration

## Chaining Multiple Panels & Non-Standard Scan Panels (`VirtualMatrixPanel_T`)

Yes, panels can be connected together in sequence using HUB75 ribbon cables.

* **Horizontal Chaining:** Connect panels end-to-end (e.g., two 64x32 panels for a 128x32 display). Update the chain length setting in `mxconfig`. See [Pattern Plasma](/examples/2_PatternPlasma/).
* **Grid Layouts & Non-Standard Scan Panels:** Use [`VirtualMatrixPanel_T`](/doc/VirtualMatrixPanel_T.md) to map multi-row serpentine/zig-zag grids (e.g., four 64x32 panels in a 2x2 grid for a 128x64 display) or non-two-scan outdoor panels (`1/4`, `1/5`, `1/6`, `1/8`, `1/10`, `1/16`, `1/2`). `VirtualDisplay::makeDMAConfig()` automatically computes the required DMA buffer dimensions.

**Example 1 — 2×2 Grid of Standard Two-Scan 64×32 Panels (128×64 display):**
```cpp
#include <ESP32-HUB75-VirtualMatrixPanel_T.hpp>

using VirtualDisplay = VirtualMatrixPanel_T<CHAIN_TOP_RIGHT_DOWN>;

auto mxconfig = VirtualDisplay::makeDMAConfig(64, 32, 2, 2); // 2 rows x 2 cols of 64x32
auto *dma_display = new MatrixPanel_I2S_DMA(mxconfig);
dma_display->begin();

auto *virtualDisp = new VirtualDisplay(*dma_display, mxconfig); // Reads grid & panel size from mxconfig
virtualDisp->drawDisplayTest();
```

**Example 2 — Non-Standard 4-Scan Panel (e.g., 64×32 1/8-scan outdoor panel with `pxbase = 16`):**
```cpp
#include <ESP32-HUB75-VirtualMatrixPanel_T.hpp>

using PanelScan      = FourScanMapping<16>; // 0 (default) uses panel_res_x
using VirtualDisplay = VirtualMatrixPanel_T<CHAIN_NONE, PanelScan>;

auto mxconfig = VirtualDisplay::makeDMAConfig(64, 32); // Auto-folds to 128x16 DMA
mxconfig.clkphase = false;

auto *dma_display = new MatrixPanel_I2S_DMA(mxconfig);
dma_display->begin();

auto *virtualDisp = new VirtualDisplay(*dma_display, mxconfig); // or (*dma_display) for 1x1 panels
virtualDisp->drawDisplayTest();
```

See the [`VirtualMatrixPanel_T` Guide](/doc/VirtualMatrixPanel_T.md), [Non-Standard Panel List](/doc/NonStandardPanels.md), [`VirtualMatrixPanel` example](/examples/VirtualMatrixPanel/), [`Pixel_Mapping_Test`](/examples/Pixel_Mapping_Test/), and [AuroraDemo](/examples/AuroraDemo/).

Resolutions above 128x64 may cause memory errors due to ESP32 internal SRAM limits unless using Octal PSRAM on an ESP32-S3 or an ESP32-P4.

![ezgif com-video-to-gif](https://user-images.githubusercontent.com/12006953/89837358-b64c0480-db60-11ea-870d-4b6482068a3b.gif)

## Line Decoder Selection

If multiple duplicate rows light up simultaneously, your panel may use a non-standard row multiplexer instead of the default `74HC138` binary decoder (`TYPE138`):
```cpp
mxconfig.line_decoder = HUB75_I2S_CFG::TYPE138;     // Default: 74HC138, RUC7258, ICN2012, SM5166
mxconfig.line_decoder = HUB75_I2S_CFG::TYPE595;     // Shift-register row mux (SM5368, DP32020A, RUL5158, ICND2018)
mxconfig.line_decoder = HUB75_I2S_CFG::TYPE_DIRECT; // Direct A/B/C/D MOSFET switching (no 138/595 IC)
mxconfig.line_decoder = HUB75_I2S_CFG::SM5266P;     // SM5266P row decoder
```

## Adjusting Panel Brightness

Default brightness is set to 128 (50%). Change brightness using `setPanelBrightness(val)` or `setBrightness8(val)`.

* Value range: `0` (off) to `255` (full brightness).

Example:
```cpp
void setup() {
    Serial.begin(115200);
    dma_display->begin();
    dma_display->setBrightness8(192); // Set brightness (~75%)
    dma_display->clearScreen();	
}
```

![Brightness Samples](https://user-images.githubusercontent.com/55933003/211192894-f90311f5-b6fe-4665-bf26-2f363bb36047.png)

## Build-Time Options

For IDEs supporting compile-time flags (such as [PlatformIO](https://platformio.org/) or [Eclipse](https://www.eclipse.org/ide/)), view the [Build Options Document](/doc/BuildOptions.md).

## Latch Blanking

If pixels repeat horizontally (ghosting), adjust Latch Blanking using `mxconfig.latch_blanking` or `setLatBlanking(uint8_t v)`.

Latch blanking disables display output via OE during LAT pin transitions to hide row switching artifacts.

* Range: `1` to `8` (default `0` auto-calculates `4` for `<= 10 MHz` or `8` for `> 10 MHz`). Higher values reduce overall display brightness.

Example:
```cpp
dma_display->setLatBlanking(4);
```

## Clock Phase

If pixels appear shifted by 1 pixel or show ghosting (especially on chained outdoor panels), change the clock edge mode. Some panels update on the rising clock edge; others update on the falling clock edge.

Default behavior for `SHIFTREG` is positive clock edge (`true`). To switch to negative clock edge, set:
```cpp
mxconfig.clkphase = false;
```
*(Note: `MBI5124` and `DP3246` automatically force `clkphase = true`, while `FM6124`, `FM6126A`, and `ICN2038S` automatically set `clkphase = false`.)*
See the [Simple Test Shapes Example](/examples/1_SimpleTestShapes/1_SimpleTestShapes.ino).

## Power Requirements

A stable power supply is **critical**:
1. Solder a 1000 µF–2000 µF capacitor across the **VCC and GND pins** on the back of each panel to stabilize power spikes.
2. ESP32 outputs 3.3V signals. Some panels require 5V logic inputs. Use level shifters if panel inputs do not recognize 3.3V signals reliably.

Useful resources:
* [Raspberry Pi Matrix Power Guide](https://github.com/hzeller/rpi-rgb-led-matrix/blob/master/wiring.md#a-word-about-power)
* [Power Supply Issue Discussion](https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA/issues/39#issuecomment-722691127)
* [3.3V Logic Level Discussion](https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA/issues/35#issuecomment-726419862)

---

# Community & Acknowledgments

## Inspiration

This project was inspired by:
* [SmartMatrix](https://github.com/pixelmatix/SmartMatrix/tree/teensylc)
* [Sprite_TM's ESP32 Demo](https://www.esp32.com/viewtopic.php?f=17&t=3188)

## Projects Using This Library

* [128x64 Morph Clock](https://github.com/bogd/esp32-morphing-clock)
* [FFT Audio Visualization](https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA/discussions/149)
* [Clock, GIF Animator and Audio Visualizer](https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA/discussions/153)
* [Aurora Audio Visualizer](https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA/discussions/188)
* [Big Visualization](https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA/discussions/155)
* [Clockwise](https://jnthas.github.io/clockwise/)
* [ZeDMD](https://github.com/PPUC/ZeDMD)
* [MatrixCOS](https://github.com/mklossde/MatrixCOS)

## Thanks & Acknowledgments

* [Brian Lough](https://www.tindie.com/stores/brianlough/) ([YouTube](https://www.youtube.com/c/brianlough)) for code contributions, testing hardware, and suggestions.
* [Vortigont](https://github.com/vortigont) for performance optimizations and major code contributions.
* [board707](https://github.com/board707) for extensive `VirtualMatrixPanel` non-standard scan mappings, `TYPE_DIRECT` support, and community troubleshooting.
* [Galaxy Man](https://github.com/Galaxy-Man) for donating 1/16 scan panels to help implement virtual display chaining.
* [Pipimaxi](https://github.com/Pipimaxi) for donating an ESP32-S2 module, and [Radu](https://github.com/juniorradu) for donating an ESP32-S3 module.
* [Mark Donners](https://github.com/donnersm) ([YouTube](https://www.youtube.com/watch?v=bQ7c9Vlhyp0&t=118s)) for donating a 1/8 scan panel.
* [PaintYourDragon](https://github.com/PaintYourDragon) for ESP32-S3 DMA logic.

## Support & Donations

This library is developed as a free open-source personal project. To support ongoing development or help fund hardware panel purchases, see [this discussion thread](https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-DMA/discussions/349) or visit [mrcodetastic on GitHub](https://github.com/mrcodetastic/).

## Music

An AI-generated song about this library:
[Listen on Suno](https://suno.com/song/183aa807-9fb6-410c-b0e9-0ea945232950) 😊

![It's better in real life](image.jpg)
