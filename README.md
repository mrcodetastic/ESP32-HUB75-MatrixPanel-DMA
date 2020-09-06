# HUB75 LED matrix library for the ESP32, utilising DMA

This ESP32 Arduino library for an 64x32 RGB LED (HUB 75 type) 1/16 Scan LED Matrix Panel, utilises the DMA functionality provided by the ESP32's I2S 'LCD Mode' which basically means that pixel data is sent straight from memory, via the DMA controller, to the relevant LED Matrix GPIO pins with little CPU overhead.

As a result, this library can theoretically provide ~16-24 bit colour, at various brightness levels without noticeable flicker.

## Panels Supported
62x32 pixel 1/16 Scan LED Matrix 'Indoor' Panel, such as this [typical RGB panel available for purchase](https://www.aliexpress.com/item/256-128mm-64-32-pixels-1-16-Scan-Indoor-3in1-SMD2121-RGB-full-color-P4-led/32810362851.html). 

## Panels Not Supported
* 1/8 Scan LED Matrix Panels are not supported, please use an alternative library if you bought one of these.
* Chinese junk panels based on FMXXXX chipsets. This library does not support these panels. FM6126 panels based on [this untested example](/examples/FM6126Panel) could work however.

# Installation

* Dependency: You will need to install Adafruit_GFX from the "Library > Manage Libraries" menu.
* Download and unzip this repository into your Arduino/libraries folder (or better still, use the Arduino 'add library from .zip' option.
* Library also tested to work fine with PlatformIO, install into your PlatformIO projects' lib/ folder as appropriate.

# Wiring ESP32 with the LED Matrix Panel

By default the pin mapping is as follows (defaults defined in ESP32-RGB64x32MatrixPanel-I2S-DMA.h).

```
 HUB 75 PANEL              ESP 32 PIN
+-----------+   
|  R1   G1  |    R1  -> IO25      G1  -> IO26
|  B1   GND |    B1  -> IO27
|  R2   G2  |    R2  -> IO14      G2  -> IO12
|  B2   GND |    B2  -> IO13
|   A   B   |    A   -> IO23      B   -> IO19
|   C   D   |    C   -> IO 5      D   -> IO17
| CLK   LAT |    CLK -> IO16      LAT -> IO 4
|  OE   GND |    OE  -> IO15      GND -> ESP32 GND
+-----------+
```

However, if you want to change this, simply provide the wanted pin mapping as part of the display.begin() call. For example, in your sketch have something like the following:

```
// Change these to whatever suits
#define R1_PIN  25
#define G1_PIN  26
#define B1_PIN  27
#define R2_PIN  14
#define G2_PIN  12
#define B2_PIN  13

#define A_PIN   23
#define B_PIN   22 
#define C_PIN   5
#define D_PIN   17
#define E_PIN   -1
          
#define LAT_PIN 4
#define OE_PIN  15

#define CLK_PIN 16


display.begin(R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN, A_PIN, B_PIN, C_PIN, D_PIN, E_PIN, LAT_PIN, OE_PIN, CLK_PIN );  // setup the LED matrix
```

The panel must be powered by 5V AC adapter with enough current capacity. (Current varies due to how many LED are turned on at the same time. To drive all the LEDs, you need 5V4A adapter.)

# How to Use

Below is a bare minimum sketch to draw a single white dot in the top left. You must call .begin() before you call ANY pixel-drawing (fonts, lines, colours etc.) function of the RGB64x32MatrixPanel_I2S_DMA class.

No .begin() before other functions = Crash
```
#include <ESP32-RGB64x32MatrixPanel-I2S-DMA.h>
RGB64x32MatrixPanel_I2S_DMA matrix;

void setup()
{ 
  // MUST DO THIS FIRST!
  matrix.begin();  // Use default pins supplied within ESP32-RGB64x32MatrixPanel-I2S-DMA.h
  // matrix.begin(R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN, A_PIN, B_PIN, C_PIN, D_PIN, E_PIN, LAT_PIN, OE_PIN, CLK_PIN );  // or custom pins

  // Draw a single white pixel
  matrix.drawPixel(0,0, matrix.color565(255,255,255)); // can do this after .begin() only
  
}

void loop()
{ }

```


## Can I use with a larger panel (i.e. 64x64px square panel)?
If you want to use with a 64x64 pixel panel (typically a HUB75*E* panel) you MUST configure a valid *E_PIN* to your ESP32 and connect it to the E pin of the HUB75 panel! Hence the 'E' in 'HUB75E'

## Can I chain panels?

Yes. If you want to chain two of these horizontally to make a 128x32 panel you can easily by setting the MATRIX_WIDTH to '128' and connecting the panels in series using the HUB75 ribbon cable.

Similarly, if you wanted to chain 4 panels to make a 256x32 px horizontal panel, you can easily by setting the MATRIX_WIDTH to '256' and connecting the panels in series using the HUB75 ribbon cable.

Finally, if you wanted to chain 4 x (64x32px) panels to make 128x64px display (essentially a 2x2 grid of 64x32 LED Matrix modules), a little more magic will be required. Refer to the [Chained Panels](examples/ChainedPanels/) example.

Resolutions beyond 128x128 are likely to result in crashes due to memory constraints etc. You're on your own at this point.

![ezgif com-video-to-gif](https://user-images.githubusercontent.com/12006953/89837358-b64c0480-db60-11ea-870d-4b6482068a3b.gif)


## Panel Brightness

By default you should not need to change / set the brightness setting as the default value (16) is sufficent for most purposes. Brightness can be changed by calling `setPanelBrightness(XX)` and then `clearScreen()`.

The value to pass 'setPanelBrightness' must be a value less than MATRIX_WIDTH. For example for a single 64x32 LED Matrix Module, a value less than 64. However, if you set the brightness too high, you may experience ghosting. 

Example:

```
void setup() {
    Serial.begin(115200);
    matrix.begin(R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN, A_PIN, B_PIN, C_PIN, D_PIN, E_PIN, LAT_PIN, OE_PIN, CLK_PIN );  // setup the LED matrix
    matrix.setPanelBrightness(16); // Set the brightness. 32 or lower ideal for a single 64x32 LED Matrix Panel.
    matrix.clearScreen(); // You must clear the screen after changing brightness level for it to take effect.

}

```
Summary: setPanelBrightness(xx) value can be any number from 0 (display off) to MATRIX_WIDTH-1. So if you are chaining multiple 64x32 panels, then this value may actually be > 64 (or you will have a dim display). Changing the brightness will have a huge impact on power usage.

![It's better in real life](image.jpg)

## Inspiration

* 'SmartMatrix' project code: https://github.com/pixelmatix/SmartMatrix/tree/teensylc

* Sprite_TM's demo implementation here: https://www.esp32.com/viewtopic.php?f=17&t=3188

