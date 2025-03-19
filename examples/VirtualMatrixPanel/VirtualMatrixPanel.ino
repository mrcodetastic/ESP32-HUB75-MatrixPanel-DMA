/**
 * @file VirtualMatrixPanel.ino
 * @brief Example of using the VirtualMatrixPanel_T template class.
 *
 * The VirtualMatrixPanel_T class can be used for two purposes:
 * 
 *   1) Create a much larger display out of a number of physical LED panels 
 *      chained in a Serpentine or Zig-Zag manner; 
 *    
 *   2) Provide a way to deal with weird individual physical panels that do not have a 
 *      simple linear X, Y pixel mapping. For example, 1/4 scan panels, or outdoor panels.
 * 
 *   1) and 2) can be combined and utilsied together.
 * 
 *   There are FOUR examples contained within this library. What example gets built depends
 *   on the value of the "#define EXAMPLE_NUMBER X" value. Where X = Example number.
 *   
 *   Example 1:  STANDARD 1/2 Scan (i.e. 1/16, 1/32) LED matrix panels, 64x32 pixels each,
 *               in a grid of 2x2 panels, chained in a Serpentine manner. 
 * 
 *   Example 2:  Non-Standard 1/4 Scan (i.e. Four-Scan 1/8) outdoor LED matrix panels, 64x32 pixels each,
 *               in a grid of 2x2 panels, chained in a Serpentine manner. 
 * 
 *   Example 3:  A single non-standard 1/4 Scan (i.e. Four-Scan 1/8) outdoor LED matrix panel, 64x32 pixels.
 *
 *   Example 4:  Having your own panel pixel mapping logic of use only to a specific panel that isn't supported.
 *               In this case we re-use this to map an individual pixel in a weird way.
 */

 #include <Arduino.h>
 #include <ESP32-HUB75-VirtualMatrixPanel_T.hpp>
 
 // Select example to compile!
 #define EXAMPLE_NUMBER 1
 //#define EXAMPLE_NUMBER 2
 //#define EXAMPLE_NUMBER 3
 //#define EXAMPLE_NUMBER 4 // Custom Special Effects example!
 
 /**
  * Configuration of the LED matrix panels number and individual pixel resolution.
  **/
 #define PANEL_RES_X     64     // Number of pixels wide of each INDIVIDUAL panel module. 
 #define PANEL_RES_Y     32     // Number of pixels tall of each INDIVIDUAL panel module.
 
 #define VDISP_NUM_ROWS      2 // Number of rows of individual LED panels 
 #define VDISP_NUM_COLS      2 // Number of individual LED panels per row
 
 #define PANEL_CHAIN_LEN     (VDISP_NUM_ROWS*VDISP_NUM_COLS)  // Don't change
 
 
 /**
  * Configuration of the approach used to chain all the individual panels together.
  * Refer to the documentation or check the enum 'PANEL_CHAIN_TYPE' in VirtualMatrixPanel_T.hpp for options.
  **/
 #define PANEL_CHAIN_TYPE CHAIN_TOP_RIGHT_DOWN
 
 /**
  * Optional config for the per-panel pixel mapping, for non-standard panels.
  * i.e. 1/4 scan panels, or outdoor panels. They're a pain in the a-- and all
  *      have their own weird pixel mapping that is not linear.
  * 
  * This is used for Examples 2 and 3.
  * 
  **/
 #define PANEL_SCAN_TYPE  FOUR_SCAN_32PX_HIGH
 
 /**
  * Mandatory declaration of the dma_display. DO NOT CHANGE
  **/
 MatrixPanel_I2S_DMA *dma_display = nullptr;


 // ------------------------------------------------------------------------------------------------------------
 
 
 /**
  * Template instantiation for the VirtualMatrixPanel_T class, depending on use-case.
  **/
 #if EXAMPLE_NUMBER == 1

  // --- Example 1: STANDARD 1/2 Scan ---
  
  // Declare a pointer to the specific instantiation:
  VirtualMatrixPanel_T<PANEL_CHAIN_TYPE>* virtualDisp = nullptr;
  
 #endif
 
  
 #if EXAMPLE_NUMBER == 2 

  // --- Example 2: Non-Standard 1/4 Scan (Four-Scan 1/8) ---
 
  // Use an existing library user-contributed Scan Type pixel mapping
  using MyScanTypeMapping = ScanTypeMapping<PANEL_SCAN_TYPE>;
  
  // Create a pointer to the specific instantiation of the VirtualMatrixPanel_T class
  VirtualMatrixPanel_T<PANEL_CHAIN_TYPE, MyScanTypeMapping>* virtualDisp = nullptr;
  
 #endif
 
 #if EXAMPLE_NUMBER == 3

 // --- Example 3: Single non-standard 1/4 Scan (Four-Scan 1/8) ---
 
  // Use an existing library user-contributed Scan Type pixel mapping
  using MyScanTypeMapping = ScanTypeMapping<PANEL_SCAN_TYPE>;
  
  // Create a pointer to the specific instantiation of the VirtualMatrixPanel_T class
  VirtualMatrixPanel_T<CHAIN_NONE, MyScanTypeMapping>* virtualDisp = nullptr;
  
 #endif
 
 
 // Bonus non-existnat example. Create your own per-panel custom pixel mapping!
 #if EXAMPLE_NUMBER == 4
  
  // --- Custom Scan–Type Pixel Mapping ---
  // This is not what you would use this for, but in any case it 
  // makes a flipped mirror image
  struct CustomMirrorScanTypeMapping {

    static VirtualCoords apply(VirtualCoords coords, int vy, int pb) {

      // coords are the input coords for adjusting

      int width  = PANEL_RES_X;
      int height = PANEL_RES_Y;	

      // Flip / Mirror x 
      coords.x = PANEL_RES_X - coords.x - 1;   
    //  coords.y = PANEL_RES_Y - coords.y - 1;   
      
      return coords;

    }

  };

    // Create a pointer to the specific instantiation of the VirtualMatrixPanel_T class
  VirtualMatrixPanel_T<CHAIN_NONE, CustomMirrorScanTypeMapping>* virtualDisp = nullptr;
  
 #endif 
 
// ------------------------------------------------------------------------------------------------------------

 void setup() 
 {
   Serial.begin(115200);
   delay(2000);

/*

    #define RL1 18
    #define GL1 17
    #define BL1 16
    #define RL2 15
    #define GL2 7
    #define BL2 6
    #define CH_A 4
    #define CH_B 10
    #define CH_C 14
    #define CH_D 21
    #define CH_E 5 // assign to any available pin if using two panels or 64x64 panels with 1/32 scan
    #define CLK 47
    #define LAT 48
    #define OE  38

  // HUB75_I2S_CFG::i2s_pins _pins={RL1, GL1, BL1, RL2, GL2, BL2, CH_A, CH_B, CH_C, CH_D, CH_E, LAT, OE, CLK};

*/  

   #if EXAMPLE_NUMBER == 1
     // A grid of normal (i.e. supported out of the box) 1/16, 1/32 (two scan) panels

     // Standard panel type natively supported by this library (Example 1, 4)
     HUB75_I2S_CFG mxconfig(
       PANEL_RES_X,   
       PANEL_RES_Y,  
       PANEL_CHAIN_LEN
       //, _pins
     );

   #endif
 
   #if EXAMPLE_NUMBER == 2 
   // A grid of 1/4 scan panels. This panel type is not supported 'out of the box' and require specific  
   // ScanTypeMapping (pixel mapping) within the panel itself.

     /**
      * HACK ALERT!
      * For 1/4 scan panels (namely outdoor panels), electrically the pixels are connected in a chain that is
      * twice the physical panel's pixel width, and half the pixel height. As such, we need to configure
      * the underlying DMA library to match the same. Then we use the VirtualMatrixPanel_T class to map the
      * physical pixels to the virtual pixels.
     */
     HUB75_I2S_CFG mxconfig(
       PANEL_RES_X*2,              // DO NOT CHANGE THIS
       PANEL_RES_Y/2,              // DO NOT CHANGE THIS
       PANEL_CHAIN_LEN
       //, _pins       
     );
 
   #endif

   #if EXAMPLE_NUMBER == 3
   // A single 1/4 scan panel. This panel type is not supported 'out of the box' and require specific  
   // ScanTypeMapping (pixel mapping) within the panel itself.

     /**
      * HACK ALERT!
      * For 1/4 scan panels (namely outdoor panels), electrically the pixels are connected in a chain that is
      * twice the physical panel's pixel width, and half the pixel height. As such, we need to configure
      * the underlying DMA library to match the same. Then we use the VirtualMatrixPanel_T class to map the
      * physical pixels to the virtual pixels.
     */
   HUB75_I2S_CFG mxconfig(
     PANEL_RES_X*2,              // DO NOT CHANGE THIS
     PANEL_RES_Y/2,              // DO NOT CHANGE THIS
     1 // A Single panel
       //, _pins     
   );
   #endif

   #if EXAMPLE_NUMBER == 4
   // A single normal scan panel, but we're using a custom CustomScanTypeMapping in the 'wrong' 
   // way to demonstrate how it can be used to create custom physical LED Matrix panel mapping.

   HUB75_I2S_CFG::i2s_pins _pins={RL1, GL1, BL1, RL2, GL2, BL2, CH_A, CH_B, CH_C, CH_D, CH_E, LAT, OE, CLK};
     // Standard panel type natively supported by this library (Example 1, 4)
     HUB75_I2S_CFG mxconfig(
       PANEL_RES_X,   
       PANEL_RES_Y,  
       1          
     //   , _pins
     );
 
   #endif
 
   mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_10M;
   mxconfig.clkphase = false;
   //mxconfig.driver = HUB75_I2S_CFG::FM6126A;
 
   
   /**
    * Setup physical DMA LED display output.
    */
   dma_display = new MatrixPanel_I2S_DMA(mxconfig);
   dma_display->begin();
   dma_display->setBrightness8(128); //0-255
   dma_display->clearScreen();
 
   /**
    * Setup the VirtualMatrixPanel_T class to map the virtual pixels to the physical pixels.
    */
 
   #if EXAMPLE_NUMBER == 1
     virtualDisp = new VirtualMatrixPanel_T<PANEL_CHAIN_TYPE>(VDISP_NUM_ROWS, VDISP_NUM_COLS, PANEL_RES_X, PANEL_RES_Y);
   #elif EXAMPLE_NUMBER == 2
     virtualDisp = new VirtualMatrixPanel_T<PANEL_CHAIN_TYPE, MyScanTypeMapping>(VDISP_NUM_ROWS, VDISP_NUM_COLS, PANEL_RES_X, PANEL_RES_Y);
   #elif EXAMPLE_NUMBER == 3   
     virtualDisp = new VirtualMatrixPanel_T<CHAIN_NONE, MyScanTypeMapping>(1, 1, PANEL_RES_X, PANEL_RES_Y); // Single 1/4 scan panel
   #elif EXAMPLE_NUMBER == 4   
     virtualDisp = new VirtualMatrixPanel_T<CHAIN_NONE, CustomMirrorScanTypeMapping>(1, 1, PANEL_RES_X, PANEL_RES_Y); // Single 1/4 scan panel	 
   #endif  
 
   // Pass a reference to the DMA display to the VirtualMatrixPanel_T class
   virtualDisp->setDisplay(*dma_display);
  
   for (int y = 0; y < virtualDisp->height(); y++) {
     for (int x = 0; x < virtualDisp->width(); x++) {
 
       uint16_t color = virtualDisp->color565(96, 0, 0); // red
 
       if (x == 0)   color = virtualDisp->color565(0, 255, 0); // g
       if (x == (virtualDisp->width()-1)) color = virtualDisp->color565(0, 0, 255); // b
 
       virtualDisp->drawPixel(x, y, color);
       delay(1);
     }
   }

   virtualDisp->drawLine(virtualDisp->width() - 1, virtualDisp->height() - 1, 0, 0, virtualDisp->color565(255, 255, 255));
   
   virtualDisp->print("Virtual Matrix Panel");

   delay(3000);
   virtualDisp->clearScreen();
   virtualDisp->drawDisplayTest(); // re draw text numbering on each screen to check connectivity   

 }

 // ------------------------------------------------------------------------------------------------------------
 
 
 void loop() {
 
   // Do nothing here.

 }