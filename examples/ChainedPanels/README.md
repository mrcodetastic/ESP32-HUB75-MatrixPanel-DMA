    ## Chained Panels example - Chaining individual LED matrix panels to make a larger panel ##

    This is the PatternPlasma Demo adopted for use with multiple
    displays arranged in a non standard order

    ### What is a non standard order? ###
    
    When you connect multiple panels together, the library treats the
    multiple panels as one big panel arranged horizontally. Arranging
    the displays like this would be a standard order.

    [ 4 ][ 3 ][ 2 ][ 1 ]  (ESP32 is connected to 1)

    If you wanted to arrange the displays vertically, or in rows and 
    columns this example might be able to help.

    [ 4 ][ 3 ]
    [ 2 ][ 1 ]

    It creates a virtual screen that you draw to in the same way you would
    the matrix, but it will look after mapping it back to the displays.

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

#### Contributor ####
    Written by Brian Lough
    YouTube: https://www.youtube.com/brianlough
    Tindie: https://www.tindie.com/stores/brianlough/
    Twitter: https://twitter.com/witnessmenow

    Parts:
    ESP32 D1 Mini * - https://s.click.aliexpress.com/e/_dSi824B
    ESP32 I2S Matrix Shield (From my Tindie) = https://www.tindie.com/products/brianlough/esp32-i2s-matrix-shield/

 *  * = Affilate

    If you find what I do useful and would like to support me,
    please consider becoming a sponsor on Github
    https://github.com/sponsors/witnessmenow/
