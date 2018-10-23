# Adafruit_GFX RGB64x32MatrixPanel library for the ESP32, utilising I2S DMA.

This ESP32 Arduino library for an RGB LED (HUB 75 type) Matrix Panel, utilises the DMA functionality provided by the ESP32's I2S 'LCD Mode' which basically means that pixel data can be sent straight from memory, via the DMA controller, to the relevant LED Matrix GPIO pins with little CPU overhead.

As a result, this library can theoretically provide ~20 bit colour, at various brightness levels without resulting in noticeable flicker.

![It's better in real life](image.jpg)

# Installation

You need to install [Adafruit_GFX_Library](https://github.com/adafruit/Adafruit-GFX-Library) from the "Library > Manage Libraries" menu.

## Patching GPIO to avoid eratta of ESP32

You should patch the `tools/sdk/ld/esp32.peripherals.ld` to avoid random pixel noise on the display as following:

```
/* PROVIDE ( GPIO = 0x3ff44000 ); */
PROVIDE ( GPIO = 0x60004000 );
```

Please refer section 3.3. in https://www.espressif.com/sites/default/files/documentation/eco_and_workarounds_for_bugs_in_esp32_en.pdf for more details.

# Wiring ESP32 with the LED Matrix Panel

If you wish to change this, you will need to adjust in the header file.

```
+-----------+   Panel - ESP32 pins
|  R1   G1  |    R1   - IO25      G1   - IO26
|  B1   GND |    B1   - IO27
|  R2   G2  |    R2   - IO14      G2   - IO12
|  B2   GND |    B2   - IO13
|   A   B   |    A    - IO23      B    - IO22
|   C   D   |    C    - IO 5      D    - IO17
| CLK   LAT |    CLK  - IO16      LAT  - IO 4
|  OE   GND |    OE   - IO15
+-----------+
```

The panel must be powered by 5V AC adapter with enough current capacity. (Current varies due to how many LED are turned on at the same time. To drive all the LEDs, you need 5V4A adapter.)


A [typical RGB panel available for purchase](https://www.aliexpress.com/item/256-128mm-64-32-pixels-1-16-Scan-Indoor-3in1-SMD2121-RGB-full-color-P4-led/32810362851.html). This is of the larger P4 (4mm spacing between pixels) type. 

# Usage

Refer to the excellent PxMatrix based library for how to wire one of these panels up (in fact, it's probably best to first get your panel working with this library first to be sure): https://github.com/2dom/PxMatrix

You'll need to adjust the pin configuration in this library of course.

# Notes

Seems to work well and drive a RGB panel with decent colours and little flicker. Even better that it uses Direct Memory Access :-)

# Credits

* 'SmartMatrix' project code: https://github.com/pixelmatix/SmartMatrix/tree/teensylc

* Sprite_TM's demo implementation here: https://www.esp32.com/viewtopic.php?f=17&t=3188

