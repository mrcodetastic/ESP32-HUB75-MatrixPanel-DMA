/******************************************************************************
    -----------
    Steps to use
    -----------

    1) In ESP32-RGB64x32MatrixPanel-I2S-DMA.h:

    - Set the MATRIX_HEIGHT to be the y resolution of the physical chained 
      panels in a line (if the panels are 32 x 16, set it to be 16)
    - Set the MATRIX_WIDTH to be the sum of the x resolution of all the physical 
      chained panels (i.e. If you have 4 x (32px w x 16px h) panels, 32x4 = 128) 

    2) In the sketch (i.e. this example):
    
    - Set values for NUM_ROWS, NUM_COLS, PANEL_RES_X, PANEL_RES_Y. 
      There are comments beside them explaining what they are in more detail.
    - Other than where the matrix is defined and matrix.begin in the setup, you 
      should now be using the virtual display for everything (drawing pixels, writing text etc).
       You can do a find and replace of all calls if it's an existing sketch
      (just make sure you don't replace the definition and the matrix.begin)
    - If the sketch makes use of MATRIX_HEIGHT or MATRIX_WIDTH, these will need to be 
      replaced with the width and height of your virtual screen. 
      Either make new defines and use that, or you can use virtualDisp.width() or .height()

    Thanks to:

    * Brian Lough for the original example as raised in this issue:
      https://github.com/mrfaptastic/ESP32-RGB64x32MatrixPanel-I2S-DMA/issues/26

      YouTube: https://www.youtube.com/brianlough
      Tindie: https://www.tindie.com/stores/brianlough/
      Twitter: https://twitter.com/witnessmenow

    * Galaxy-Man for the kind donation of panels make/test that this is possible:
      https://github.com/Galaxy-Man

*****************************************************************************/

//#define USE_CUSTOM_PINS // uncomment to use custom pins, then provide below

#define A_PIN  26
#define B_PIN  4
#define C_PIN  27
#define D_PIN  2
#define E_PIN  21 

#define R1_PIN   5
#define R2_PIN  19
#define G1_PIN  17
#define G2_PIN  16
#define B1_PIN  18
#define B2_PIN  25

#define CLK_PIN  14
#define LAT_PIN  15
#define OE_PIN  13
 

 /******************************************************************************
  * VIRTUAL DISPLAY / MATRIX PANEL CHAINING CONFIGURATION
  * 
  * Note 1: If chaining from the top right to the left, and then S curving down
  *         then serpentine_chain = true and top_down_chain = true
  *         (these being the last two parameters of the virtualDisp(...) constructor.
  * 
  * Note 2: If chaining starts from the bottom up, then top_down_chain = false.
  * 
  * Note 3: By default, this library has serpentine_chain = true, that is, every 
  *         second row has the panels 'upside down' (rotated 180), so the output
  *         pin of the row above is right above the input connector of the next 
  *         row.
  
  Example 1 panel chaining:
  +-----------------+-----------------+-------------------+
  | 64x32px PANEL 3 | 64x32px PANEL 2 | 64x32px PANEL 1   |
  |        ------------   <--------   | ------------xx    |
  | [OUT]  |   [IN] | [OUT]      [IN] | [OUT]    [ESP IN] |
  +--------|--------+-----------------+-------------------+
  | 64x32px|PANEL 4 | 64x32px PANEL 5 | 64x32px PANEL 6   |
  |       \|/   ---------->           |  ----->           |
  | [IN]      [OUT] | [IN]      [OUT] | [IN]      [OUT]   |
  +-----------------+-----------------+-------------------+

  Example 1 configuration:
    
    #define PANEL_RES_X 64 // Number of pixels wide of each INDIVIDUAL panel module. 
    #define PANEL_RES_Y 32 // Number of pixels tall of each INDIVIDUAL panel module.

    #define NUM_ROWS 2 // Number of rows of chained INDIVIDUAL PANELS
    #define NUM_COLS 3 // Number of INDIVIDUAL PANELS per ROW

    virtualDisp(dma_display, NUM_ROWS, NUM_COLS, PANEL_RES_X, PANEL_RES_Y, true, true);

    = 192x64 px virtual display, with the top left of panel 3 being pixel co-ord (0,0)

  ==========================================================

  Example 2 panel chaining:

  +-------------------+
  | 64x32px PANEL 1   |
  | ----------------- |
  | [OUT]    [ESP IN] |
  +-------------------+
  | 64x32px PANEL 2   |
  |                   |
  | [IN]      [OUT]   |
  +-------------------+
  | 64x32px PANEL 3   |
  |                   |
  | [OUT]      [IN]   |
  +-------------------+
  | 64x32px PANEL 4   |
  |                   |
  | [IN]      [OUT]   |
  +-------------------+    

  Example 2 configuration:
    
    #define PANEL_RES_X 64 // Number of pixels wide of each INDIVIDUAL panel module. 
    #define PANEL_RES_Y 32 // Number of pixels tall of each INDIVIDUAL panel module.

    #define NUM_ROWS 4 // Number of rows of chained INDIVIDUAL PANELS
    #define NUM_COLS 1 // Number of INDIVIDUAL PANELS per ROW

    virtualDisp(dma_display, NUM_ROWS, NUM_COLS, PANEL_RES_X, PANEL_RES_Y, true, true);

    virtualDisp(dma_display, NUM_ROWS, NUM_COLS, PANEL_RES_X, PANEL_RES_Y, true, true);     

    = 128x64 px virtual display, with the top left of panel 1 being pixel co-ord (0,0)

  ==========================================================

  Example 3 panel chaining (bottom up):

  +-----------------+-----------------+
  | 64x32px PANEL 4 | 64x32px PANEL 3 | 
  |             <----------           | 
  | [OUT]      [IN] | [OUT]      [in] | 
  +-----------------+-----------------+
  | 64x32px PANEL 1 | 64x32px PANEL 2 | 
  |             ---------->           | 
  | [ESP IN]  [OUT] | [IN]      [OUT] | 
  +-----------------+-----------------+

  Example 1 configuration:
    
    #define PANEL_RES_X 64 // Number of pixels wide of each INDIVIDUAL panel module. 
    #define PANEL_RES_Y 32 // Number of pixels tall of each INDIVIDUAL panel module.

    #define NUM_ROWS 2 // Number of rows of chained INDIVIDUAL PANELS
    #define NUM_COLS 2 // Number of INDIVIDUAL PANELS per ROW

    virtualDisp(dma_display, NUM_ROWS, NUM_COLS, PANEL_RES_X, PANEL_RES_Y, true, false);

    = 128x64 px virtual display, with the top left of panel 4 being pixel co-ord (0,0)  

*/




#define PANEL_RES_X 64 // Number of pixels wide of each INDIVIDUAL panel module. 
#define PANEL_RES_Y 32 // Number of pixels tall of each INDIVIDUAL panel module.

#define NUM_ROWS 2 // Number of rows of chained INDIVIDUAL PANELS
#define NUM_COLS 2 // Number of INDIVIDUAL PANELS per ROW


/******************************************************************************
 * Create physical DMA panel class AND virtual (chained) display class.   
 ******************************************************************************/
#include <ESP32-VirtualMatrixPanel-I2S-DMA.h>
RGB64x32MatrixPanel_I2S_DMA dma_display;
VirtualMatrixPanel          virtualDisp(dma_display, NUM_ROWS, NUM_COLS, PANEL_RES_X, PANEL_RES_Y, true);


/******************************************************************************
 * Setup!
 ******************************************************************************/
void setup() {
  
  delay(2000);
  Serial.begin(115200);
  Serial.println(""); Serial.println(""); Serial.println("");
  Serial.println("*****************************************************");
  Serial.println(" HELLO !");
  Serial.println("*****************************************************");

#ifdef USE_CUSTOM_PINS
  dma_display.begin(R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN, A_PIN, B_PIN, C_PIN, D_PIN, E_PIN, LAT_PIN, OE_PIN, CLK_PIN );  // setup the LED matrix
#else
  dma_display.begin();
#endif



  // Sanity checks
  if (NUM_ROWS <= 1) {
    Serial.println(F("There is no reason to use the VirtualDisplay class for a single horizontal chain and row!"));
  }

  if (dma_display.width() != NUM_ROWS*NUM_COLS*PANEL_RES_X )
  {
    Serial.println(F("\r\nERROR: MATRIX_WIDTH and/or MATRIX_HEIGHT in 'ESP32-RGB64x32MatrixPanel-I2S-DMA.h'\r\nis not configured correctly for the requested VirtualMatrixPanel dimensions!\r\n"));
    Serial.printf("WIDTH according dma_display is %d, but should be %d. Is your NUM_ROWS and NUM_COLS correct?\r\n", dma_display.width(), NUM_ROWS*NUM_COLS*PANEL_RES_X);
    return;
  }
  
  // So far so good, so continue
  virtualDisp.fillScreen(virtualDisp.color444(0, 0, 0));
  virtualDisp.drawDisplayTest(); // draw text numbering on each screen to check connectivity

  delay(3000);

  Serial.println("Chain of 64x32 panels for this example:");
  Serial.println("+--------+---------+");
  Serial.println("|   4    |    3    |");
  Serial.println("|        |         |");
  Serial.println("+--------+---------+");
  Serial.println("|   1    |    2    |");
  Serial.println("| (ESP)  |         |");
  Serial.println("+--------+---------+");

   virtualDisp.setFont(&FreeSansBold12pt7b);
   virtualDisp.setTextColor(virtualDisp.color565(0, 0, 255));
   virtualDisp.setTextSize(2); 
   virtualDisp.setCursor(10, virtualDisp.height()-20); 
   
   // Red text inside red rect (2 pix in from edge)
   virtualDisp.print("1234") ;  
   virtualDisp.drawRect(1,1, virtualDisp.width()-2, virtualDisp.height()-2, virtualDisp.color565(255,0,0));

   // White line from top left to bottom right
   virtualDisp.drawLine(0,0, virtualDisp.width()-1, virtualDisp.height()-1, virtualDisp.color565(255,255,255));



}

void loop() {
  

} // end loop