/*************************************************************************
 *             IMPORANT PLEASE READ THE INFORMATION BELOW!
 * 
 * This example implements a 'pixel buffer' which is essentally an 
 * off-screen copy of what is intended to be sent to output (LED panels)
 *
 * This essentially means DOUBLE THE AMOUNT OF MEMORY is required to 
 * to store the off-screen image/pixel/display buffer WITH a similar 
 * amount of memory used for the DMA output buffer for the physical panels.
 *
 * This means the practical resolution you will be able to output with the
 * ESP32 will be CUT IN HALF. Do not try to run huge chains of 
 * LED Matrix Panels using this buffer, you will run out of memory.
 *
 * Please DO NOT raise issues @ github about running out of memory,
 * we can't do anything about it. It's an ESP32, not a Raspberry Pi!
 *
 *************************************************************************/ 

/* Use the FastLED_Pixel_Buffer class to handle panel chaining
 * (it's based on the VirtualMatrixPanel class) AND also create an 
 * off-screen CRGB FastLED pixel buffer.
 */
#include "FastLED_Pixel_Buffer.h" 

  // Panel configuration
  #define PANEL_RES_X 64 // Number of pixels wide of each INDIVIDUAL panel module. 
  #define PANEL_RES_Y 32 // Number of pixels tall of each INDIVIDUAL panel module.
   
  #define NUM_ROWS 1 // Number of rows of chained INDIVIDUAL PANELS
  #define NUM_COLS 2 // Number of INDIVIDUAL PANELS per ROW
      
  // Change this to your needs, for details please read the PDF in 
  // the 'ChainedPanels'example folder!
  #define SERPENT true
  #define TOPDOWN false
  
  // placeholder for the matrix object
  MatrixPanel_I2S_DMA *dma_display = nullptr;

  // placeholder for the virtual display object
  VirtualMatrixPanel_FastLED_Pixel_Buffer  *FastLED_Pixel_Buff = nullptr;
  
  /******************************************************************************
   * Setup!
   ******************************************************************************/
  void setup()
  {
    delay(250);
   
    Serial.begin(115200);
    Serial.println(""); Serial.println(""); Serial.println("");
    Serial.println("*****************************************************");
    Serial.println("*        FastLED Pixel BufferDemonstration          *");
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
                PANEL_RES_X,              // DO NOT CHANGE THIS
                PANEL_RES_Y,              // DO NOT CHANGE THIS
                NUM_ROWS*NUM_COLS           // DO NOT CHANGE THIS
                //,_pins            // Uncomment to enable custom pins
    );
    
    mxconfig.clkphase = false; // Change this if you see pixels showing up shifted wrongly by one column the left or right.    
    //mxconfig.driver   = HUB75_I2S_CFG::FM6126A;     // in case that we use panels based on FM6126A chip, we can set it here before creating MatrixPanel_I2S_DMA object
	
	// Do NOT use mxconfig.double_buffer when using this pixel buffer. 
  
    // OK, now we can create our matrix object
    dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  
    // let's adjust default physical panel brightness to about 75%
    dma_display->setBrightness8(96);    // range is 0-255, 0 - 0%, 255 - 100%
  
    // Allocate memory and start DMA electrical output to physical panels
    if( not dma_display->begin() )
      Serial.println("****** !KABOOM! I2S memory allocation failed ***********");
   
    dma_display->clearScreen();
    delay(500);
    
    // NOW, create the 'Virtual Matrix Panel' class with a FastLED Pixel Buffer! Pass it a dma_display hardware library pointer to use.
    FastLED_Pixel_Buff = new VirtualMatrixPanel_FastLED_Pixel_Buffer((*dma_display), NUM_ROWS, NUM_COLS, PANEL_RES_X, PANEL_RES_Y, SERPENT, TOPDOWN);

   if( not FastLED_Pixel_Buff->allocateMemory() )
      Serial.println("****** !KABOOM! Unable to find enough memory for the FastLED pixel buffer! ***********");
	  
  }
  
   
 // Borrowed from the SimpleTextShapes example.
 uint16_t colorWheel(uint8_t pos) {
   if(pos < 85) {
     return dma_display->color565(pos * 3, 255 - pos * 3, 0);
   } else if(pos < 170) {
     pos -= 85;
     return dma_display->color565(255 - pos * 3, 0, pos * 3);
   } else {
     pos -= 170;
     return dma_display->color565(0, pos * 3, 255 - pos * 3);
   }
 }
 
 /* A crap demonstration of using the pixel buffer.
  * 1) Draw text at an incrementing (going down) y coordinate
  * 2) Move down a pixel row
  * 3) Draw the text again, fade the 'old' pixels. Using the pixel buffer to update all pixels on screen.
  * 4) 'show' (send) the pixel buffer to the DMA output.
  * 5) LOOP
  */   
  
  uint8_t y_coord     = 0;
  uint8_t wheel = 0;
  
  void loop() 
  {
      // draw text with a rotating colour
      FastLED_Pixel_Buff->dimAll(200); // Dim all pixels by 250/255
	  
      FastLED_Pixel_Buff->setTextSize(1);     // size 1 == 8 pixels high
      FastLED_Pixel_Buff->setTextWrap(false); // Don't wrap at end of line - will do ourselves
      
      FastLED_Pixel_Buff->setCursor(FastLED_Pixel_Buff->width()/4, y_coord);    // start at top left, with 8 pixel of spacing     
      FastLED_Pixel_Buff->setTextColor(colorWheel(wheel++));
      
      FastLED_Pixel_Buff->print("MythicalForce");
      
      FastLED_Pixel_Buff->show();  // IMPORTANT -> SEND Pixel Buffer to DMA / Panel Output!
      
      y_coord++;
      
      if ( y_coord >= FastLED_Pixel_Buff->height())
              y_coord = 0;

      delay(35);
      
  } // end loop
