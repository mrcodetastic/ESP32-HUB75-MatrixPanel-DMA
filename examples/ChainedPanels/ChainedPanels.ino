/******************************************************************************
    -----------
    Steps to use
    -----------

    1) In the sketch (i.e. this example):
    
    - Set values for NUM_ROWS, NUM_COLS, PANEL_RES_X, PANEL_RES_Y, PANEL_CHAIN. 
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
      https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA/issues/26

      YouTube: https://www.youtube.com/brianlough
      Tindie: https://www.tindie.com/stores/brianlough/
      Twitter: https://twitter.com/witnessmenow

    * Galaxy-Man for the kind donation of panels make/test that this is possible:
      https://github.com/Galaxy-Man

*****************************************************************************/


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
#define PANEL_CHAIN NUM_ROWS*NUM_COLS    // total number of panels chained one to another

// Change this to your needs, for details on VirtualPanel pls read the PDF!
#define SERPENT true
#define TOPDOWN false

// library includes
#include <ESP32-VirtualMatrixPanel-I2S-DMA.h>

// placeholder for the matrix object
MatrixPanel_I2S_DMA *dma_display = nullptr;

// placeholder for the virtual display object
VirtualMatrixPanel  *virtualDisp = nullptr;


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


  /******************************************************************************
   * Create physical DMA panel class AND virtual (chained) display class.   
   ******************************************************************************/

  /*
    The configuration for MatrixPanel_I2S_DMA object is held in HUB75_I2S_CFG structure,
    All options has it's predefined default values. So we can create a new structure and redefine only the options we need

	Please refer to the '2_PatternPlasma.ino' example for detailed example of how to use the MatrixPanel_I2S_DMA configuration
  */

  HUB75_I2S_CFG mxconfig(
                PANEL_RES_X,   // module width
                PANEL_RES_Y,   // module height
                PANEL_CHAIN    // chain length
  );

  //mxconfig.driver = HUB75_I2S_CFG::FM6126A;     // in case that we use panels based on FM6126A chip, we can set it here before creating MatrixPanel_I2S_DMA object

  // Sanity checks
  if (NUM_ROWS <= 1) {
    Serial.println(F("There is no reason to use the VirtualDisplay class for a single horizontal chain and row!"));
  }

  // OK, now we can create our matrix object
  dma_display = new MatrixPanel_I2S_DMA(mxconfig);

  // let's adjust default brightness to about 75%
  dma_display->setBrightness8(192);    // range is 0-255, 0 - 0%, 255 - 100%

  // Allocate memory and start DMA display
  if( not dma_display->begin() )
      Serial.println("****** !KABOOM! I2S memory allocation failed ***********");

  // create VirtualDisplay object based on our newly created dma_display object
  virtualDisp = new VirtualMatrixPanel((*dma_display), NUM_ROWS, NUM_COLS, PANEL_RES_X, PANEL_RES_Y, SERPENT, TOPDOWN);

  // So far so good, so continue
  virtualDisp->fillScreen(virtualDisp->color444(0, 0, 0));
  virtualDisp->drawDisplayTest(); // draw text numbering on each screen to check connectivity

  delay(3000);

  Serial.println("Chain of 64x32 panels for this example:");
  Serial.println("+--------+---------+");
  Serial.println("|   4    |    3    |");
  Serial.println("|        |         |");
  Serial.println("+--------+---------+");
  Serial.println("|   1    |    2    |");
  Serial.println("| (ESP)  |         |");
  Serial.println("+--------+---------+");

   virtualDisp->setFont(&FreeSansBold12pt7b);
   virtualDisp->setTextColor(virtualDisp->color565(0, 0, 255));
   virtualDisp->setTextSize(2); 
   virtualDisp->setCursor(10, virtualDisp->height()-20); 
   
   // Red text inside red rect (2 pix in from edge)
   virtualDisp->print("1234");
   virtualDisp->drawRect(1,1, virtualDisp->width()-2, virtualDisp->height()-2, virtualDisp->color565(255,0,0));

   // White line from top left to bottom right
   virtualDisp->drawLine(0,0, virtualDisp->width()-1, virtualDisp->height()-1, virtualDisp->color565(255,255,255));
}

void loop() {
  

} // end loop
