## Aurora Demo
* This demonstrates a combination of the following libraries:
  - "ESP32-HUB75-MatrixPanel-DMA" to send pixel data to the physical panels in combination with its in-built "VirtualMatrix" class to create a virtual display of chained panels, so the graphical effects of the Aurora demonstrations can be shown over a 'bigger' grid of physical panels acting as one big display.
  - "GFX_Lite" to provide a simple graphics library for drawing on the virtual display.
  - Note: GFX_Lite is a fork of AdaFruitGFX and FastLED library combined together, with a focus on simplicity and ease of use.

 ## Instructions
  * Use the serial input to advance through the patterns, or to toggle auto advance.
  * Sending 'n' will advance to the next pattern, 'p' will go to the previous pattern. Sending 'a' will toggle auto advance on and off.
