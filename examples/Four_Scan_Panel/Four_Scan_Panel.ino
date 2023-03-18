/*************************************************************************
 * Description: 
 * 
 * The underlying implementation of the ESP32-HUB75-MatrixPanel-I2S-DMA only
 * supports output to HALF scan panels - which means outputting 
 * two lines at the same time, 16 or 32 rows apart if a 32px or 64px high panel
 * respectively. 
 * This cannot be changed at the DMA layer as it would require a messy and complex 
 * rebuild of the library's internals.
 *
 * However, it is possible to connect QUARTER (i.e. FOUR lines updated in parallel)
 * scan panels to this same library and
 * 'trick' the output to work correctly on these panels by way of adjusting the
 * pixel co-ordinates that are 'sent' to the ESP32-HUB75-MatrixPanel-I2S-DMA
 * library.
 * 
 **************************************************************************/
#include "ESP32-HUB75-MatrixPanel-I2S-DMA.h"

/* Use the Virtual Display class to re-map co-ordinates such that they draw 
 * correctly on a 32x16 1/8 Scan panel (or chain of such panels).
 */
#include "ESP32-VirtualMatrixPanel-I2S-DMA.h" 


  // Panel configuration
  #define PANEL_RES_X 64 // Number of pixels wide of each INDIVIDUAL panel module. 
  #define PANEL_RES_Y 32 // Number of pixels tall of each INDIVIDUAL panel module.
  
  
  #define NUM_ROWS 1 // Number of rows of chained INDIVIDUAL PANELS
  #define NUM_COLS 2 // Number of INDIVIDUAL PANELS per ROW
  
  // ^^^ NOTE: DEFAULT EXAMPLE SETUP IS FOR A CHAIN OF TWO x 1/8 SCAN PANELS
    
  // Change this to your needs, for details on VirtualPanel pls read the PDF!
  #define SERPENT true
  #define TOPDOWN false
  
  // placeholder for the matrix object
  MatrixPanel_I2S_DMA *dma_display = nullptr;

  // placeholder for the virtual display object
  VirtualMatrixPanel  *FourScanPanel = nullptr;
  
  /******************************************************************************
   * Setup!
   ******************************************************************************/
  void setup()
  {
    delay(250);
   
    Serial.begin(115200);
    Serial.println(""); Serial.println(""); Serial.println("");
    Serial.println("*****************************************************");
    Serial.println("*         1/8 Scan Panel Demonstration              *");
    Serial.println("*****************************************************");
  
/* 
     // 62x32 1/8 Scan Panels don't have a D and E pin!
     
     HUB75_I2S_CFG::i2s_pins _pins = {
      R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN, 
      A_PIN, B_PIN, C_PIN, D_PIN, E_PIN, 
      LAT_PIN, OE_PIN, CLK_PIN
     };
*/
    HUB75_I2S_CFG mxconfig(
                PANEL_RES_X*2,              // DO NOT CHANGE THIS
                PANEL_RES_Y/2,              // DO NOT CHANGE THIS
                NUM_ROWS*NUM_COLS           // DO NOT CHANGE THIS
                //,_pins            // Uncomment to enable custom pins
    );
    
    mxconfig.clkphase = false; // Change this if you see pixels showing up shifted wrongly by one column the left or right.
    
    //mxconfig.driver   = HUB75_I2S_CFG::FM6126A;     // in case that we use panels based on FM6126A chip, we can set it here before creating MatrixPanel_I2S_DMA object
  
    // OK, now we can create our matrix object
    dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  
    // let's adjust default brightness to about 75%
    dma_display->setBrightness8(96);    // range is 0-255, 0 - 0%, 255 - 100%
  
    // Allocate memory and start DMA display
    if( not dma_display->begin() )
      Serial.println("****** !KABOOM! I2S memory allocation failed ***********");

   
    dma_display->clearScreen();
    delay(500);
    
    // create FourScanPanellay object based on our newly created dma_display object
    FourScanPanel = new VirtualMatrixPanel((*dma_display), NUM_ROWS, NUM_COLS, PANEL_RES_X, PANEL_RES_Y);
    
	// THE IMPORTANT BIT BELOW!
    FourScanPanel->setPhysicalPanelScanRate(FOUR_SCAN_32PX_HIGH);
  }

  
  void loop() {

      // What the panel sees from the DMA engine!
      for (int i=PANEL_RES_X*2+10; i< PANEL_RES_X*(NUM_ROWS*NUM_COLS)*2; i++)
      {
        dma_display->drawLine(i, 0, i, 7, dma_display->color565(255, 0, 0)); // red
        delay(10);
      }
          
      dma_display->clearScreen();
      delay(1000);
/*
      // Try again using the pixel / dma memory remapper
      for (int i=PANEL_RES_X+5; i< (PANEL_RES_X*2)-1; i++)
      {
        FourScanPanel->drawLine(i, 0, i, 7, dma_display->color565(0, 0, 255)); // blue    
        delay(10);
      } 
*/

      // Try again using the pixel / dma memory remapper
      int offset = PANEL_RES_X*((NUM_ROWS*NUM_COLS)-1);
      for (int i=0; i< PANEL_RES_X; i++)
      {
        FourScanPanel->drawLine(i+offset, 0, i+offset, 7, dma_display->color565(0, 0, 255)); // blue
        FourScanPanel->drawLine(i+offset, 8, i+offset, 15, dma_display->color565(0, 128,0)); // g        
        FourScanPanel->drawLine(i+offset, 16, i+offset, 23, dma_display->color565(128, 0,0)); // red
        FourScanPanel->drawLine(i+offset, 24, i+offset, 31, dma_display->color565(0, 128, 128)); // blue        
        delay(10);
      } 

      delay(1000);


      // Print on each chained panel 1/8 module!
      // This only really works for a single horizontal chain
      for (int i = 0; i < NUM_ROWS*NUM_COLS; i++)
      {
        FourScanPanel->setTextColor(FourScanPanel->color565(255, 255, 255));
        FourScanPanel->setCursor(i*PANEL_RES_X+7, FourScanPanel->height()/3); 
      
        // Red text inside red rect (2 pix in from edge)
        FourScanPanel->print("Panel " + String(i+1));
        FourScanPanel->drawRect(1,1, FourScanPanel->width()-2, FourScanPanel->height()-2, FourScanPanel->color565(255,0,0));
      
        // White line from top left to bottom right
        FourScanPanel->drawLine(0,0, FourScanPanel->width()-1, FourScanPanel->height()-1, FourScanPanel->color565(255,255,255));
      }

      delay(2000);
      dma_display->clearScreen();
  
  } // end loop
