# HUB75 RGB LED matrix library utilizing ESP32 DMA Engine

__[BUILD OPTIONS](/doc/BuildOptions.md) | [EXAMPLES](/examples/README.md)__ | [![PlatformIO CI](https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-DMA/actions/workflows/pio_build.yml/badge.svg)](https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-DMA/actions/workflows/pio_build.yml)

  **Table of Content**

- [Introduction](#introduction)
  * [Features](#features)
  * [Supported ESP32](#esp32-supported)
  * [Supported Panels](#panels-supported)
  * [Panel driver chips known to be working well](#panel-driver-chips-known-to-be-working-well)
  * [Panels Not Supported](#panels-not-supported)
  * [Update for 16x32 Panels](#update-for-16x32-panels)
  * [Cool uses of this library](#cool-uses-of-this-library)
- [Getting Started](#getting-started)
  * [1. Library Installation](#1-library-installation)
  * [2. Wiring ESP32 with the LED Matrix Panel](#2-wiring-esp32-with-the-led-matrix-panel)
  * [3. Run a Test Sketch](#3-run-a-test-sketch)
- [More Information](#more-information)
  * [Build-time options](#doc/BuildOptions.md)
  * [Memory constraints](#memory-constraints)
  * [Can I use with a larger panel (i.e. 64x64px square panel)?](#can-i-use-with-a-larger-panel-ie-64x64px-square-panel)
  * [Can I chain panels?](#can-i-chain-panels)
  * [Panel Brightness](#panel-brightness)
  * [Latch blanking](#latch-blanking)
  * [Power, Power and Power!](#power--power-and-power)
  * [Inspiration](#inspiration)
 - [Thank you!](#thank-you)

## Introduction
This ESP32 Arduino/IDF library for HUB75 / HUB75E connector type 64x32 RGB LED 1/16 Scan OR 64x64 RGB LED 1/32 Scan LED Matrix Panel, utilities the DMA functionality provided by the ESP32's 'LCD Mode'.

### Features
-  **Low CPU overhead** - once initialized pixel data is pumped to the matrix inputs via DMA engine directly from memory
-  **Fast** - updating pixel data involves only bit-wise logic over DMA buffer memory, no pins manipulation or blocking IO
-  **Full screen BCM** - library utilizes [binary-code modulation](http://www.batsocks.co.uk/readme/art_bcm_5.htm) to render pixel color depth / brightness over the entire matrix
-  **Variable color depth** - up to TrueColor 24 bits output is possible depending on matrix size/refresh rate required
-  **CIE 1931** luminance [correction](https://ledshield.wordpress.com/2012/11/13/led-brightness-to-your-eye-gamma-correction-no/) (aka natural LED dimming)
-  **Adafruit GFX API** - library could be build with AdafruitGFX, simplified GFX or without GFX API at all

If you wanna ask "*...OK, OK, than what's the price for those features?*" I'll tell you - "[memory](/doc/memcalc.md), you pay it all by precious MCU's memory for DMA buffer". 

Please use the ['Memory Calculator'](/doc/memcalc.md) to see what is actually achievable with a typical ESP32.

![Memory Calculator](doc/memcalc.jpg)



## ESP32 Supported
This library supports the:
* Original ESP32 - That being the ESP-WROOM-32 module with ESP32‑D0WDQ6 chip from 2017. This MCU has 520kB of SRAM which is much more than all the recent 'reboots' of the ESP32 such as the S2, S3 etc.
* ESP32-S2; and
* ESP32-S3

RISC-V ESP32's (like the C3) are not, and will never be supported  as they do not have parallel DMA output required for this library.

## Panels Supported
* 64x32 (width x height) pixel 1/16 Scan LED Matrix 'Indoor' Panel, such as this [typical RGB panel available for purchase](https://www.aliexpress.com/item/256-128mm-64-32-pixels-1-16-Scan-Indoor-3in1-SMD2121-RGB-full-color-P4-led/32810362851.html).
* 64x64 pixel 1/32 Scan LED Matrix 'Indoor' Panel.
* 32x16 pixel 1/4 Scan LED Matrix 'Indoor' Panel using an ingenious workaround as demonstrated in the 32x16_1_4_ScanPanel example.
* 126x64 [SM5266P](https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA/issues/164) 1/32 Scan Panel 

Ones interested in internals of such matrices could find [this article](https://www.sparkfun.com/news/2650) useful.

Due to the high-speed optimized nature of this library, only specific panels are supported. Please do not raise issues with respect to panels not supported on the list below.

## Panel driver chips known to be working well

* ICND2012
* [RUC7258](http://www.ruichips.com/en/products.html?cateid=17496)
* FM6126A AKA ICN2038S, [FM6124](https://datasheet4u.com/datasheet-pdf/FINEMADELECTRONICS/FM6124/pdf.php?id=1309677) (Refer to [PatternPlasma](/examples/2_PatternPlasma) example on how to use.)
* SM5266P 

## Panels Not Supported
* 1/8 Scan LED Matrix Panels are not supported.
* RUL5358 / SHIFTREG_ABC_BIN_DE based panels are not supported.
* ICN2053 / FM6353 based panels - Refer to [this library](https://github.com/LAutour/ESP32-HUB75-MatrixPanel-DMA-ICN2053), which is a fork of this library ( [discussion link](https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-DMA/discussions/324)).
* Any other panel not listed above.

Please use an [alternative library](https://github.com/2dom/PxMatrix) if you bought one of these.

## Cool uses of this library
There are a number of great looking LED graphical display projects which leverage this library, these include:
* [128x64 Morph Clock](https://github.com/bogd/esp32-morphing-clock)
* [FFT Audio Visualisation](https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA/discussions/149)
* [Clock, GIF Animator and Audio Visualiser](https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA/discussions/153)
* [Aurora Audio Visualiser](https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA/discussions/188)
* [Big Visualisation](https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA/discussions/155)
* [Clockwise](https://jnthas.github.io/clockwise/)

# Getting Started
## 1. Library Installation

* Dependency: You will need to install Adafruit_GFX from the "Library > Manage Libraries" menu.
* Download and unzip this repository into your Arduino/libraries folder (or better still, use the Arduino 'add library from .zip' option.
* Library also tested to work fine with PlatformIO, install into your PlatformIO projects' lib/ folder as appropriate. Or just add it into [platformio.ini](/doc/BuildOptions.md) [lib_depth](https://docs.platformio.org/en/latest/projectconf/section_env_library.html#lib-deps) section.

## 2. Wiring the ESP32/ESP32-S2/ESP32-S3 to an LED Matrix Panel

Refer to the '*default-pins.hpp' file within the [applicable platforms folder](https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-DMA/tree/master/src/platforms).

```
If you want to change the GPIO mapping at runtime, simply provide the wanted pin mapping as part of the class initialization structure. For example, in your sketch have something like the following:

// Change these to whatever suits
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
#define E_PIN -1 // required for 1/32 scan panels, like 64x64. Any available pin would do, i.e. IO32
#define LAT_PIN 4
#define OE_PIN 15
#define CLK_PIN 16

HUB75_I2S_CFG::i2s_pins _pins={R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN, A_PIN, B_PIN, C_PIN, D_PIN, E_PIN, LAT_PIN, OE_PIN, CLK_PIN};
HUB75_I2S_CFG mxconfig(
	64, // Module width
	32, // Module height
	2, // chain length
	_pins, // pin mapping
);
dma_display = new MatrixPanel_I2S_DMA(mxconfig);
```

Various people have created PCBs for which one can simply connect an ESP32 to a PCB, and then the PCB to the HUB75 connector, such as:

* Brian Lough's [ESP32 I2S Matrix Shield](http://blough.ie/i2smat/)
* Charles Hallard's [WeMos Matrix Shield](https://github.com/hallard/WeMos-Matrix-Shield-DMA)
* Bogdan Sass's [Morph Clock Shield](https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA/discussions/110#discussioncomment-861152)

Please contact or order these products from the respective authors.

## 3. Run a Test Sketch
Below is a bare minimum sketch to draw a single white dot in the top left. You must call begin() before you call ANY pixel-drawing (fonts, lines, colours etc.) function of the MatrixPanel_I2S_DMA class.

Once this is working, refer to the [PIO Test Patterns](/examples/PIO_TestPatterns) example. This sketch draws simple colors/lines/gradients over the entire matrix and it could help to troubleshoot various issues with ghosting, flickering, etc...
>Note: Requires the use of [PlatformIO](https://platformio.org/), which you should probably use if you aren't already. 
# More Information
## Build-time options
Although Arduino IDE does not [seem](https://github.com/arduino/Arduino/issues/421) to offer any way of specifying compile-time options for external libs there are other IDE's (like [PlatformIO](https://platformio.org/)/[Eclipse](https://www.eclipse.org/ide/)) that could use that. Check [Build Options](doc/BuildOptions.md) document for reference.

## Memory constraints
If you are going to use large/combined panels make sure to check for [memory constraints](/doc/i2s_memcalc.md).

NOTE: You can use PSRAM to expand the amount of memory available only on the ESP32-S3 and with Octal SPI-RAM (ESP32 S3 N8R8 variant). However, due to bandwidth limitations, the maximum output frequency is 10Mhz, which will limit the real world number of panels that can be chained without the refresh rate being impacted.

For all other ESP32 variants (like the most popular ‘original’ ESP32), the hardware [only allows DMA transfer from *internal* SRAM](https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA/issues/55), so you will be limited to the 200KB or so of usable SRAM of the ESP32 regardless of how many megabytes of PSRAM you may have connected.

## Can I use with a larger panel (i.e. 64x64px square panel)?
If you want to use with a 64x64 pixel panel (typically a HUB75*E* panel) you MUST configure a valid *E_PIN* to your ESP32 and connect it to the E pin of the HUB75 panel! Hence the 'E' in 'HUB75E'

## Can I chain panels?
Yes!

[Horizontal](https://user-images.githubusercontent.com/12006953/122657476-cd358d00-d15b-11eb-9c6c-99b61378c56a.mp4)

For example: If you want to chain two of these horizontally to make a 128x32 panel you can do so by connecting the panels in series using the HUB75 ribbon cable. Than you must provide proper configuration structure to the class constructor letting it know that you use "one long virtual matrix chain". Refer to [Pattern Plasma](/examples/2_PatternPlasma/) example for all the details about configuration setup.

Finally, if you wanted to chain 4 x (64x32px) panels to make 128x64px display (essentially a 2x2 grid of 64x32 LED Matrix modules), a little more magic will be required. Refer to the [Chained Panels](examples/ChainedPanels/) example.

Resolutions beyond 128x64 are more likely to result in crashes due to [memory](/doc/i2s_memcalc.md) constraints etc. You are on your own after this point - PLEASE do not raise issues about this, the library can't magically defeat the SRAM memory constraints of the ESP32.

![ezgif com-video-to-gif](https://user-images.githubusercontent.com/12006953/89837358-b64c0480-db60-11ea-870d-4b6482068a3b.gif)

  ## Panel Brightness

By default you should not need to change / set the brightness setting as the default value (16) is sufficient for most purposes. Brightness can be changed by calling `setPanelBrightness(int XX)` or `setBrightness8(uint8_t XX)`.  

The value to pass `setPanelBrightness()` must be less than MATRIX_CHAIN_WIDTH in pixels. For example for a single 64x32 LED Matrix Module, a value must be less than 64. For 3 modules 64x32 it must be less than 192. However, if you set the brightness too high, you may experience ghosting. 

Also you may use method `setBrightness8(x)`, where x is a uint8_t value between 0-255. Library will recalculate required brightness level depending on matrix width (mostly useful with FastLED-based sketches).

Example:
```
void setup() {
Serial.begin(115200);
	dma_display->begin(); // setup the LED matrix
    dma_display->setBrightness8(90); //0-255
    dma_display->clearScreen();	
}
```

Summary: setPanelBrightness(xx) value can be any number from 0 (display off) to MATRIX_WIDTH-1. So if you are chaining multiple 64x32 panels, then this value may actually be > 64 (or you will have a dim display). Changing the brightness will have a huge impact on power usage.

![It's better in real life](image.jpg)


## Latch blanking
If you are facing issues with image ghosting when pixels has clones with horizontal offset, than you try to change Latch blanking value. Latch blanking controls for how many clock pulses matrix output is disabled via EO signal before/after toggling LAT signal. It hides row bits transitioning and different panels may require longer times for proper operation. Default value is 1 clock before/after LAT row transition. This could be controlled with `MatrixPanel_I2S_DMA::setLatBlanking(uint8_t v)`. v could be between 1 to 4, default is 1, larger values won't give any benefit other than reducing brightness.

An example:
```
dma_display->setLatBlanking(2);
```

## Power, Power and Power!
Having a good power supply is CRITICAL, and it is highly recommended, for chains of LED Panels to have a 1000-2000uf capacitor soldered to the back of each LED Panel across the [GND and VCC pins](https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA/issues/39#issuecomment-720780463), otherwise you WILL run into issues with 'flashy' graphics whereby a large amount of LEDs are turned on and off in succession (due to current/power draw peaks and troughs).

 - Refer to this guide written for the [rpi-rgb-led-matrix library](https://github.com/hzeller/rpi-rgb-led-matrix/blob/master/wiring.md#a-word-about-power) for an explanation.
- Refer to this [example](https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA/issues/39#issuecomment-722691127) issue of what can go wrong with a poor power supply.


- Refer to [this comment](https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA/issues/35#issuecomment-726419862) in regards to certain panels not playing nice with voltages, and a 3.3volt signal that the ESP32 GPIO can only provide.

## Inspiration
This project was inspired by:
* 'SmartMatrix': https://github.com/pixelmatix/SmartMatrix/tree/teensylc
* Sprite_TM's demo implementation here: https://www.esp32.com/viewtopic.php?f=17&t=3188

# Thank you!
* [Brian Lough](https://www.tindie.com/stores/brianlough/) ([youtube link](https://www.youtube.com/c/brianlough)) for providing code contributions, hardware and suggestions
* [Vortigont](https://github.com/vortigont) for his game changing code contributions and performance optimisations
* [Galaxy Man](https://github.com/Galaxy-Man) for donation of 1/16 scan panels to support the implemenation of led matrix panel chaining (virtual display) support
* [Pipimaxi](https://github.com/Pipimaxi) for the donation of a ESP32-S2 and [Radu](https://github.com/juniorradu) for the donation of an ESP32-S3 to enable support for ESP32 S2/S3's to be tested and implemented.
* [Mark Donners](https://github.com/donnersm) ('The Electronic Engineer' on [youtube](https://www.youtube.com/watch?v=bQ7c9Vlhyp0&t=118s)) for the donation of a 1/8 scan panel to build and test working support of these led matrix panels! 
* [PaintYourDragon](https://github.com/PaintYourDragon) for the DMA logic for the ESP32-S3.
* And lots of others, let me know if I've missed you.
