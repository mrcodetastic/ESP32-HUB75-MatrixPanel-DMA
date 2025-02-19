# The 'VirtualMatrixPanel_T' class
The `VirtualMatrixPanel_T` is used to perform pixel re-mapping in order to support the following use-cases that can be used together:
1. To create a larger display based on a chain of individual physical panels connected electrically in a Serpentine or Zig-Zag manner.
2. To provide support for physical panels with non-standard (i.e. Not a 1/2 scan panel) pixel mapping approaches. This is often seen with 1/4 scan outdoor panels.

## 1. Chaining individual LED matrix panels to make a larger virtual display ##

This is the PatternPlasma Demo adopted for use with multiple LED Matrix Panel displays arranged in a non standard order (i.e. a grid) to make a bigger display.

![334894846_975082690567510_1362796919784291270_n](https://user-images.githubusercontent.com/89576620/224304944-94fe3483-d3cc-4aba-be0a-40b33ff901dc.jpg)

### What do we mean by 'non standard order'? ###

When you link / chain multiple panels together, the ESP32-HUB75-MatrixPanel-I2S-DMA library treats as one wide horizontal panel. This would be a 'standard' (default) order.

Non-standard order is essentially the creation of a non-horizontal-only display that you can draw to in the same way you would any other display, with VirtualDisplay library looking after the pixel mapping to the physical chained panels.

For example: You bought four (4) 64x32px panels, and wanted to use them to create a 128x64pixel display. You would use the VirtualMatrixPanel class.

[Refer to this document](https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-DMA/blob/master/doc/VirtualMatrixPanel.pdf) for an explanation and refer to this example on how to use.


### Steps to Use ###

1. [Refer to this document](https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-DMA/blob/master/doc/VirtualMatrixPanel.pdf) for an explanation and refer to this example on how to use.

2. Read the `VirtualMatrixPanel.ino` code

## 2. Using this library with 1/4 Scan Panels (Four Scan)

This library does not natively support 'Four Scan' 64x32 1/8 or 32x16 1/4 scan panels such [as this](https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA/issues/154) by default.

### Solution
Read the `VirtualMatrixPanel.ino` code. 

The VirtualMatrixPanel_T class provides a way to additionally remap pixel for each individual panel by way of the `ScanTypeMapping` class.

You can create your own custom per-panel pixel mapping class as well should you wish.

```cpp
// --- Example 3: Single non-standard 1/4 Scan (Four-Scan 1/8) ---

// Use an existing library user-contributed Scan Type pixel mapping
using MyScanTypeMapping = ScanTypeMapping<FOUR_SCAN_32PX_HIGH>;

// Create a pointer to the specific instantiation of the VirtualMatrixPanel_T class
VirtualMatrixPanel_T<CHAIN_NONE, MyScanTypeMapping>* virtualDisp = nullptr;
```

The library has these user-contributed additions, but given the variety of panels on the market, your success with any of these may vary.

```cpp
    FOUR_SCAN_32PX_HIGH,   ///< Four-scan mode, 32-pixel high panels.
    FOUR_SCAN_16PX_HIGH,   ///< Four-scan mode, 16-pixel high panels.
    FOUR_SCAN_64PX_HIGH,   ///< Four-scan mode, 64-pixel high panels.
    FOUR_SCAN_40PX_HIGH    ///< Four-scan mode, 40-pixel high panels.
```