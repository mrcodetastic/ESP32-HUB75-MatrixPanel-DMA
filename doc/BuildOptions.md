### Build Options and flags

This lib supports build-time defines used to set some of the basic key features.
For example build flags could be set using PlatformIO's  .ini file like this

```
[env]
framework = arduino
platform = espressif32
lib_deps =
    ESP32 HUB75 LED MATRIX PANEL DMA Display
build_flags =
    -DSERIAL_DEBUG
    -DNO_GFX
```

## Build flags

| Flag  | Description  | Note |
| :------------ |---------------|-----|
| **SERIAL_DEBUG** |Print out detailed information about memory allocations, DMA descriptors setup and color depth [BCM](http://www.batsocks.co.uk/readme/art_bcm_5.htm) |
| **USE_GFX_ROOT** | Use [lightweight](https://github.com/mrfaptastic/Adafruit_GFX_Lite) version of AdafuitGFX, without Adafruit BusIO extensions | You **must** install [Adafruit_GFX_Lite](https://github.com/mrfaptastic/Adafruit_GFX_Lite) library instead of original AdafruitGFX|
| **NO_GFX** | Build without AdafuitGFX API, only native methods supported based on manipulating DMA buffer. I.e. no methods of drawing circles/shapes, typing text or using fonts!!!    This might save some resources for applications using it's own internal graphics buffer or working solely with per-pixel manipulation.  |   Use this if you rely on FastLED, Neomatrix or any other API. For example [Aurora](/examples/AuroraDemo/) effects can work fine w/o AdafruitGFX. |
| **NO_FAST_FUNCTIONS** | Do not build auxiliary speed-optimized functions. Those are used to speed-up operations like drawing straight lines or rectangles. Otherwise lines/shapes are drawn using drawPixel() method. The trade-off for speed is RAM/code-size, take it or leave it ;)        | If you are not using AdafruitGFX than you probably do not need this eather|
|**NO_CIE1931**|Do not use LED brightness [compensation](https://ledshield.wordpress.com/2012/11/13/led-brightness-to-your-eye-gamma-correction-no/) described in [CIE 1931](https://en.wikipedia.org/wiki/CIE_1931_color_space). Normaly library would adjust every pixel's RGB888 so that liminance (or brighness control) for the corresponding LED's would apper 'linear' to the human's eye. I.e. a white dot with rgb(128,128,128) would seem to be at 50% brightness between rgb(0,0,0) and rgb(255,255,255). Normaly you would like to keep this enabled by default. Not only it makes brightness control "linear", it also makes colors more vivid, otherwise it looks brighter but 'bleached'.|You might want to turn it off in some special cases like: <ul><li>Using some other overlay lib for intermediate calculations that makes it's own compensation, like FastLED's [dimming functions](http://fastled.io/docs/3.1/group___dimming.html).<li>running at low color depth's - it **might** (or might not) look better in shadows, darker gradients w/o compensation, try it<li>you run for as bright output as possible, no matter what (make sure you have proper powering)<li>you run for speed/save resources at all costs</ul> |

## Build-time variables

| Flag  | Description  | Note |
| :------------ |---------------|-----|
| **PIXEL_COLOR_DEPTH_BITS=8** | Color depth per color per pixel in range 2-8. More bit's - more natural color. But on the other hand every additional bit:<ul><li>eats ~2.5 bits of DMA memoy per pixel<li>reduces matrix refresh rate in power of two due to nature of [BCM](http://www.batsocks.co.uk/readme/art_bcm_5.htm)</ul>  | Default is 8 bits per color per pixel, i.e. TrueColor 24 bit RGB. For higher resolutions, from 64x64 and above it is not possible to provide full 24 bits color without significant flickering OR reducing dynamic range in shadows. In that case using 5-6 bits at high res make very small difference to the humans eye actually. Refer to the [I2S memcalc](i2s_memcalc.md) for more details|