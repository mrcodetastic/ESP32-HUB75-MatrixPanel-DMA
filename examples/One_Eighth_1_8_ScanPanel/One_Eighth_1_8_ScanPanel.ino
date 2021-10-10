/*************************************************************************
 * Description: 
 * 
 * The underlying implementation of the ESP32-HUB75-MatrixPanel-I2S-DMA only
 * supports output to 1/16 or 1/32 scan panels - which means outputting 
 * two lines at the same time, 16 or 32 rows apart. This cannot be changed
 * at the DMA layer as it would require a messy and complex rebuild of the 
 * library's DMA internals.
 *
 * However, it is possible to connect 1/8 scan panels to this same library and
 * 'trick' the output to work correctly on these panels by way of adjusting the
 * pixel co-ordinates that are 'sent' to the ESP32-HUB75-MatrixPanel-I2S-DMA
 * library (in this example, it is the 'dmaOutput' class).
 * 
 * Supports chaining of multiple 1/8 panels...
 *
 **************************************************************************/
   
  // Panel configuration
  #define PANEL_RES_X 64 // Number of pixels wide of each INDIVIDUAL panel module. 
  #define PANEL_RES_Y 32 // Number of pixels tall of each INDIVIDUAL panel module.
  
  #define NUM_ROWS 1 // Number of rows of chained INDIVIDUAL PANELS
  #define NUM_COLS 1 // Number of INDIVIDUAL PANELS per ROW
  #define PANEL_CHAIN NUM_ROWS*NUM_COLS    // total number of panels chained one to another
  
  // Change this to your needs, for details on VirtualPanel pls read the PDF!
  #define SERPENT true
  #define TOPDOWN false
  
  // GPIO Configuration
  #define R1_PIN  2
  #define G1_PIN  15
  #define B1_PIN  4
  #define R2_PIN  16
  #define G2_PIN  27
  #define B2_PIN  17
  
  #define A_PIN   5
  #define B_PIN   18
  #define C_PIN   19
  #define D_PIN   -1 // Connected to GND on panel (21 if exist)
  #define E_PIN   -1 // Connected to GND on panel
  
  #define LAT_PIN 26
  #define OE_PIN  25
  #define CLK_PIN 22 
  
  #include "OneEighthScanMatrixPanel.h" // Virtual Display to re-map co-ordinates such that they draw correctly on a32x16 1/4 Scan panel 
  
  // placeholder for the matrix object
  MatrixPanel_I2S_DMA *dma_display   = nullptr;
  
  // placeholder for the virtual display object
  OneEighthMatrixPanel  *virtualDisp = nullptr;
  
  /******************************************************************************
   * Setup!
   ******************************************************************************/
  void setup() {
    
    delay(2000);
    Serial.begin(115200);
    Serial.println(""); Serial.println(""); Serial.println("");
    Serial.println("*****************************************************");
    Serial.println("*         1/8 Scan Panel Demonstration              *");
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
  				PANEL_RES_X*2,   	// DO NOT CHANGE THIS
  				PANEL_RES_Y/2,   	// DO NOT CHANGE THIS
  				PANEL_CHAIN    		// DO NOT CHANGE THIS
  	);
  
  	//mxconfig.driver = HUB75_I2S_CFG::FM6126A;     // in case that we use panels based on FM6126A chip, we can set it here before creating MatrixPanel_I2S_DMA object
  
  	// OK, now we can create our matrix object
  	dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  
  	// let's adjust default brightness to about 75%
  	dma_display->setBrightness8(192);    // range is 0-255, 0 - 0%, 255 - 100%
  
  	// Allocate memory and start DMA display
  	if( not dma_display->begin() )
  	  Serial.println("****** !KABOOM! I2S memory allocation failed ***********");
  
  	// create VirtualDisplay object based on our newly created dma_display object
  	virtualDisp = new OneEighthMatrixPanel((*dma_display), NUM_ROWS, NUM_COLS, PANEL_RES_X, PANEL_RES_Y, SERPENT, TOPDOWN);
  
  	virtualDisp->setTextColor(virtualDisp->color565(0, 0, 255));
  	virtualDisp->setCursor(0, virtualDisp->height()/2); 
  
  	// Red text inside red rect (2 pix in from edge)
  	virtualDisp->print("  1234");
  	virtualDisp->drawRect(1,1, virtualDisp->width()-2, virtualDisp->height()-2, virtualDisp->color565(255,0,0));
  
  	// White line from top left to bottom right
  	virtualDisp->drawLine(0,0, virtualDisp->width()-1, virtualDisp->height()-1, virtualDisp->color565(255,255,255));
  }
  
  
  void loop() {
    
  
  } // end loop