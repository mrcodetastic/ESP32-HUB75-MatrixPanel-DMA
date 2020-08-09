## Chained Panels example - Chaining individual LED matrix panels to make a larger panel ##

This is the PatternPlasma Demo adopted for use with multiple
displays arranged in a non standard order

### What is a non standard order? ###

When you connect multiple panels together, the library treats the
multiple panels as one big panel arranged horizontally. Arranging
the displays like this would be a standard order.

Non-standard order is essentially the creation of a non-horizontal only 'virtual' display that you can draw to in the same way you would
the matrix, but with VirtualDisplay library looking after the pixel mapping to the physical chained panels.

![Nothing better than a PowerPoint slide to explain](VirtualDisplayGraphic.png)

### Steps to Use ###

1) In ESP32-RGB64x32MatrixPanel-I2S-DMA.h:

- Set the MATRIX_HEIGHT to be the y resolution of the physical chained panels in a line (if the panels are 32 x 16, set it to be 16)
- Set the MATRIX_WIDTH to be the sum of the x resolution of all the physical chained panels (i.e. If you have 4 x (32px w x 16px h) panels, 32x4 = 128) 

2) In the sketch:

- Set values for NUM_ROWS, NUM_COLS, PANEL_RES_X, PANEL_RES_Y. There are comments beside them
explaining what they are in more detail.
- Other than where the matrix is defined and matrix.begin in the setup, you should now be using the virtual display
for everything (drawing pixels, writing text etc). You can do a find and replace of all calls if it's an existing sketch
(just make sure you don't replace the definition and the matrix.begin)
- If the sketch makes use of MATRIX_HEIGHT or MATRIX_WIDTH, these will need to be replaced with the width and height
of your virtual screen. Either make new defines and use that, or you can use virtualDisp.width() or .height()

#### Thanks to ####
* Brian Lough for the Virtual to Real pixel co-ordinate code.

YouTube: https://www.youtube.com/brianlough

Tindie: https://www.tindie.com/stores/brianlough/

Twitter: https://twitter.com/witnessmenow

* Galaxy-Man for the donation of hardware for testing.
