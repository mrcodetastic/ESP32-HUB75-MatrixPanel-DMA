#ifndef _ESP32_RGB_64_32_MATRIX_PANEL_I2S_DMA
#define _ESP32_RGB_64_32_MATRIX_PANEL_I2S_DMA

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "esp_heap_caps.h"
#include "esp32_i2s_parallel.h"

#include "Adafruit_GFX.h"

/*

    This is example code to driver a p3(2121)64*32 -style RGB LED display. These types of displays do not have memory and need to be refreshed
    continuously. The display has 2 RGB inputs, 4 inputs to select the active line, a pixel clock input, a latch enable input and an output-enable
    input. The display can be seen as 2 64x16 displays consisting of the upper half and the lower half of the display. Each half has a separate 
    RGB pixel input, the rest of the inputs are shared.

    Each display half can only show one line of RGB pixels at a time: to do this, the RGB data for the line is input by setting the RGB input pins
    to the desired value for the first pixel, giving the display a clock pulse, setting the RGB input pins to the desired value for the second pixel,
    giving a clock pulse, etc. Do this 64 times to clock in an entire row. The pixels will not be displayed yet: until the latch input is made high, 
    the display will still send out the previously clocked in line. Pulsing the latch input high will replace the displayed data with the data just 
    clocked in.

    The 4 line select inputs select where the currently active line is displayed: when provided with a binary number (0-15), the latched pixel data
    will immediately appear on this line. Note: While clocking in data for a line, the *previous* line is still displayed, and these lines should
    be set to the value to reflect the position the *previous* line is supposed to be on.

    Finally, the screen has an OE input, which is used to disable the LEDs when latching new data and changing the state of the line select inputs:
    doing so hides any artifacts that appear at this time. The OE line is also used to dim the display by only turning it on for a limited time every
    line.

    All in all, an image can be displayed by 'scanning' the display, say, 100 times per second. The slowness of the human eye hides the fact that
    only one line is showed at a time, and the display looks like every pixel is driven at the same time.

    Now, the RGB inputs for these types of displays are digital, meaning each red, green and blue subpixel can only be on or off. This leads to a
    color palette of 8 pixels, not enough to display nice pictures. To get around this, we use binary code modulation.

    Binary code modulation is somewhat like PWM, but easier to implement in our case. First, we define the time we would refresh the display without
    binary code modulation as the 'frame time'. For, say, a four-bit binary code modulation, the frame time is divided into 15 ticks of equal length.

    We also define 4 subframes (0 to 3), defining which LEDs are on and which LEDs are off during that subframe. (Subframes are the same as a 
    normal frame in non-binary-coded-modulation mode, but are showed faster.)  From our (non-monochrome) input image, we take the (8-bit: bit 7 
    to bit 0) RGB pixel values. If the pixel values have bit 7 set, we turn the corresponding LED on in subframe 3. If they have bit 6 set,
    we turn on the corresponding LED in subframe 2, if bit 5 is set subframe 1, if bit 4 is set in subframe 0.

    Now, in order to (on average within a frame) turn a LED on for the time specified in the pixel value in the input data, we need to weigh the
    subframes. We have 15 pixels: if we show subframe 3 for 8 of them, subframe 2 for 4 of them, subframe 1 for 2 of them and subframe 1 for 1 of
    them, this 'automatically' happens. (We also distribute the subframes evenly over the ticks, which reduces flicker.)

    In this code, we use the I2S peripheral in parallel mode to achieve this. Essentially, first we allocate memory for all subframes. This memory
    contains a sequence of all the signals (2xRGB, line select, latch enable, output enable) that need to be sent to the display for that subframe.
    Then we ask the I2S-parallel driver to set up a DMA chain so the subframes are sent out in a sequence that satisfies the requirement that
    subframe x has to be sent out for (2^x) ticks. Finally, we fill the subframes with image data.

    We use a frontbuffer/backbuffer technique here to make sure the display is refreshed in one go and drawing artifacts do not reach the display.
    In practice, for small displays this is not really necessarily.
    
*/

/***************************************************************************************/
/* Serial Debugging Output on or off                                                   */

#define SERIAL_DEBUG_OUTPUT 1

/***************************************************************************************/
/* HUB75 RGB pixel WIDTH and HEIGHT. 
 *
 * This library has only been tested with a 64 pixel (wide) and 32 (high) RGB panel. 
 * Theoretically, if you want to chain two of these horizontally to make a 128x32 panel
 * you can do so with the cable and then set the MATRIX_WIDTH to '128'.
 *
 * Also, if you use a 64x64 panel, then set the MATRIX_HEIGHT to '64'; it will work!
 *
 * All of this is memory permitting of course (dependant on your sketch etc.) ...
 *
 */
#ifndef MATRIX_HEIGHT
#define MATRIX_HEIGHT               32 //64
#endif

#ifndef MATRIX_WIDTH
#define MATRIX_WIDTH                64
#endif

#ifndef MATRIX_ROWS_IN_PARALLEL
#define MATRIX_ROWS_IN_PARALLEL     2
#endif

/***************************************************************************************/
/* ESP32 Pin Definition. You can change this, but best if you keep it as is...         */

#define R1_PIN_DEFAULT  25
#define G1_PIN_DEFAULT  26
#define B1_PIN_DEFAULT  27
#define R2_PIN_DEFAULT  14
#define G2_PIN_DEFAULT  12
#define B2_PIN_DEFAULT  13

#define A_PIN_DEFAULT   23
#define B_PIN_DEFAULT   19
#define C_PIN_DEFAULT   5
#define D_PIN_DEFAULT   17
#define E_PIN_DEFAULT   -1 // Change to a valid pin if using a 64 pixel row panel.
          
#define LAT_PIN_DEFAULT 4
#define OE_PIN_DEFAULT  15

#define CLK_PIN_DEFAULT 16

/***************************************************************************************/
/* Don't change this stuff unless you know what you are doing */

// Panel Upper half RGB (numbering according to order in DMA gpio_bus configuration)
#define BIT_R1  (1<<0)   
#define BIT_G1  (1<<1)   
#define BIT_B1  (1<<2)   

// Panel Lower half RGB
#define BIT_R2  (1<<3)   
#define BIT_G2  (1<<4)   
#define BIT_B2  (1<<5)   

// Panel Control Signals
#define BIT_LAT (1<<6) 
#define BIT_OE  (1<<7)  

// Panel GPIO Pin Addresses (A, B, C, D etc..)
#define BIT_A (1<<8)    
#define BIT_B (1<<9)    
#define BIT_C (1<<10)   
#define BIT_D (1<<11)   
#define BIT_E (1<<12)   

// RGB Panel Constants / Calculated Values
#define COLOR_CHANNELS_PER_PIXEL 3 
#define PIXELS_PER_LATCH    ((MATRIX_WIDTH * MATRIX_HEIGHT) / MATRIX_HEIGHT) // = 64
#define COLOR_DEPTH_BITS    (COLOR_DEPTH/COLOR_CHANNELS_PER_PIXEL)  //  = 8
#define ROWS_PER_FRAME      (MATRIX_HEIGHT/MATRIX_ROWS_IN_PARALLEL) //  = 16

/***************************************************************************************/
/* Keep this as is. Do not change.                                                     */
#define CLKS_DURING_LATCH 0
#define MATRIX_I2S_MODE I2S_PARALLEL_BITS_16
#define MATRIX_DATA_STORAGE_TYPE uint16_t

#define ESP32_NUM_FRAME_BUFFERS           2 
#define ESP32_I2S_CLOCK_SPEED (20000000UL)
#define COLOR_DEPTH 24     

/***************************************************************************************/            


// note: sizeof(data) must be multiple of 32 bits, as ESP32 DMA linked list buffer address pointer must be word-aligned.
struct rowBitStruct {
    MATRIX_DATA_STORAGE_TYPE data[((MATRIX_WIDTH * MATRIX_HEIGHT) / MATRIX_HEIGHT) + CLKS_DURING_LATCH]; 
    // this evaluates to just MATRIX_DATA_STORAGE_TYPE data[64] really; 
    // and array of 64 uint16_t's
};

struct rowColorDepthStruct {
    rowBitStruct rowbits[COLOR_DEPTH_BITS];
};

struct frameStruct {
    rowColorDepthStruct rowdata[ROWS_PER_FRAME];
};

typedef struct rgb_24 {
    rgb_24() : rgb_24(0,0,0) {}
    rgb_24(uint8_t r, uint8_t g, uint8_t b) {
        red = r; green = g; blue = b;
    }
    rgb_24& operator=(const rgb_24& col);

    uint8_t red;
    uint8_t green;
    uint8_t blue;
} rgb_24;


/***************************************************************************************/   
class RGB64x32MatrixPanel_I2S_DMA : public Adafruit_GFX {
  // ------- PUBLIC -------
  public:
    RGB64x32MatrixPanel_I2S_DMA(bool _doubleBuffer = false) // Double buffer is disabled by default. Any change will display next active DMA buffer output (very quickly). NOTE: Not Implemented
      : Adafruit_GFX(MATRIX_WIDTH, MATRIX_HEIGHT), doubleBuffer(_doubleBuffer)  {
      
        backbuf_id = 0;
        brightness = 16; // If you get ghosting... reduce brightness level. 60 seems to be the limit before ghosting on a 64 pixel wide physical panel for some panels
        min_refresh_rate = 250; // Probably best to leave as is unless you want to experiment. Framerate has an impact on brightness and also power draw - voltage ripple.
        
    }

    // Painfully propagate the DMA pin configuration, or use compiler defaults
    void begin(int dma_r1_pin = R1_PIN_DEFAULT , int dma_g1_pin = G1_PIN_DEFAULT, int dma_b1_pin = B1_PIN_DEFAULT , int dma_r2_pin = R2_PIN_DEFAULT , int dma_g2_pin = G2_PIN_DEFAULT , int dma_b2_pin = B2_PIN_DEFAULT , int dma_a_pin  = A_PIN_DEFAULT  , int dma_b_pin = B_PIN_DEFAULT  , int dma_c_pin = C_PIN_DEFAULT , int dma_d_pin = D_PIN_DEFAULT  , int dma_e_pin = E_PIN_DEFAULT , int dma_lat_pin = LAT_PIN_DEFAULT, int dma_oe_pin = OE_PIN_DEFAULT , int dma_clk_pin = CLK_PIN_DEFAULT)
    {
        
     /* As DMA buffers are dynamically allocated, we must allocated in begin()
      * Ref: https://github.com/espressif/arduino-esp32/issues/831
      */
      allocateDMAbuffers(); 
        
// Change 'if' to '1' to enable, 0 to not include this Serial output in compiled program        
#if SERIAL_DEBUG_OUTPUT       
      Serial.printf("Using pin %d for the R1_PIN\n", dma_r1_pin);
      Serial.printf("Using pin %d for the G1_PIN\n", dma_g1_pin);
      Serial.printf("Using pin %d for the B1_PIN\n", dma_b1_pin);
      Serial.printf("Using pin %d for the R2_PIN\n", dma_r2_pin);
      Serial.printf("Using pin %d for the G2_PIN\n", dma_g2_pin);
      Serial.printf("Using pin %d for the B2_PIN\n", dma_b2_pin);
      Serial.printf("Using pin %d for the A_PIN\n", dma_a_pin);   
      Serial.printf("Using pin %d for the B_PIN\n", dma_b_pin);   
      Serial.printf("Using pin %d for the C_PIN\n", dma_c_pin);   
      Serial.printf("Using pin %d for the D_PIN\n", dma_d_pin);   
      Serial.printf("Using pin %d for the E_PIN\n", dma_e_pin); 
                     
      Serial.printf("Using pin %d for the LAT_PIN\n", dma_lat_pin);   
      Serial.printf("Using pin %d for the OE_PIN\n",  dma_oe_pin);    
      Serial.printf("Using pin %d for the CLK_PIN\n", dma_clk_pin); 
#endif    


      // Flush the DMA buffers prior to configuring DMA - Avoid visual artefacts on boot.
      clearScreen(); // Must fill the DMA buffer with the initial output bit sequence or the panel will display garbage
      flipDMABuffer(); // flip to backbuffer 1
      clearScreen(); // Must fill the DMA buffer with the initial output bit sequence or the panel will display garbage
      flipDMABuffer(); // backbuffer 0
      
      // Setup the ESP32 DMA Engine. Sprite_TM built this stuff.
      configureDMA(dma_r1_pin, dma_g1_pin, dma_b1_pin, dma_r2_pin, dma_g2_pin, dma_b2_pin, dma_a_pin,  dma_b_pin, dma_c_pin, dma_d_pin, dma_e_pin, dma_lat_pin,  dma_oe_pin,   dma_clk_pin ); //DMA and I2S configuration and setup

      showDMABuffer(); // show 0
      
    }
    
    // TODO: Disable/Enable auto buffer flipping (useful for lots of drawPixel usage)...

    // Draw pixels
    virtual void drawPixel(int16_t x, int16_t y, uint16_t color);   // overwrite adafruit implementation
    virtual void fillScreen(uint16_t color);                        // overwrite adafruit implementation
            void clearScreen() { fillScreen(0); } 
    void drawPixelRGB565(int16_t x, int16_t y, uint16_t color);
    void drawPixelRGB888(int16_t x, int16_t y, uint8_t r, uint8_t g, uint8_t b);
    void drawPixelRGB24(int16_t x, int16_t y, rgb_24 color);
    void drawIcon (int *ico, int16_t x, int16_t y, int16_t cols, int16_t rows);
    
    // Color 444 is a 4 bit scale, so 0 to 15, color 565 takes a 0-255 bit value, so scale up by 255/15 (i.e. 17)!
    uint16_t color444(uint8_t r, uint8_t g, uint8_t b) { return color565(r*17,g*17,b*17); }

    // Converts RGB888 to RGB565
    uint16_t color565(uint8_t r, uint8_t g, uint8_t b); // This is what is used by Adafruit GFX!
	
    // Converts RGB333 to RGB565
    uint16_t Color333(uint8_t r, uint8_t g, uint8_t b); // This is what is used by Adafruit GFX! Not sure why they have a capital 'C' for this particular function.

    inline void flipDMABuffer() 
    {
        // Flip to other buffer as the backbuffer. i.e. Graphic changes happen to this buffer (but aren't displayed until showDMABuffer())
        backbuf_id ^=1; 
        
#if SERIAL_DEBUG_OUTPUT     
        Serial.printf("Set back buffer to: %d\n", backbuf_id);
#endif      
    }
    
    inline void showDMABuffer()
    {
        i2s_parallel_flip_to_buffer(&I2S1, backbuf_id);
    }
    
    
    inline void setPanelBrightness(int b)
    {
      // Change to set the brightness of the display, range of 1 to matrixWidth (i.e. 1 - 64)
        brightness = b;
    }
    
    inline void setMinRefreshRate(int rr)
    {
        min_refresh_rate = rr;
    }   
            
    
   // ------- PRIVATE -------
  private:
  
    void allocateDMAbuffers() 
    {
        matrixUpdateFrames = (frameStruct *)heap_caps_malloc(sizeof(frameStruct) * ESP32_NUM_FRAME_BUFFERS, MALLOC_CAP_DMA);
        Serial.printf("Allocating DMA Refresh Buffer...\r\nTotal DMA Memory available: %d bytes total. Largest free block: %d bytes\r\n", heap_caps_get_free_size(MALLOC_CAP_DMA), heap_caps_get_largest_free_block(MALLOC_CAP_DMA));
      
    } // end initMatrixDMABuffer()
    
    void configureDMA(int r1_pin, int  g1_pin, int  b1_pin, int  r2_pin, int  g2_pin, int  b2_pin, int  a_pin, int   b_pin, int  c_pin, int  d_pin, int  e_pin, int  lat_pin, int   oe_pin, int clk_pin); // Get everything setup. Refer to the .c file

    
    // Update a specific pixel in the DMA buffer to a colour 
    void updateMatrixDMABuffer(int16_t x, int16_t y, uint8_t red, uint8_t green, uint8_t blue);
   
    // Update the entire DMA buffer (aka. The RGB Panel) a certain colour (wipe the screen basically)
    void updateMatrixDMABuffer(uint8_t red, uint8_t green, uint8_t blue);
        
    // Pixel data is organized from LSB to MSB sequentially by row, from row 0 to row matrixHeight/matrixRowsInParallel (two rows of pixels are refreshed in parallel)
    frameStruct *matrixUpdateFrames;
    
    // Setup
    bool dma_configuration_success;
        
    // Internal variables
    bool doubleBuffer;  // Do we use double buffer mode? Your project code will have to manually flip between both.
    int  backbuf_id;    // If using double buffer, which one is NOT active (ie. being displayed) to write too?
    
    int  lsbMsbTransitionBit;
    int  refreshRate;   
    int  brightness;
    int  min_refresh_rate;
    

}; // end Class header

/***************************************************************************************/   
// https://stackoverflow.com/questions/5057021/why-are-c-inline-functions-in-the-header
/* 2. functions declared in the header must be marked inline because otherwise, every translation unit which includes the header will contain a definition of the function, and the linker will complain about multiple definitions (a violation of the One Definition Rule). The inline keyword suppresses this, allowing multiple translation units to contain (identical) definitions. */
inline void RGB64x32MatrixPanel_I2S_DMA::drawPixel(int16_t x, int16_t y, uint16_t color) // adafruit virtual void override
{
  drawPixelRGB565( x, y, color);
} 



inline void RGB64x32MatrixPanel_I2S_DMA::drawIcon (int *ico, int16_t x, int16_t y, int16_t cols, int16_t rows) {
/*  drawIcon draws a C style bitmap.  
//  Example 10x5px bitmap of a yellow sun 
//
  int half_sun [50] = {
      0x0000, 0x0000, 0x0000, 0xffe0, 0x0000, 0x0000, 0xffe0, 0x0000, 0x0000, 0x0000,
      0x0000, 0xffe0, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xffe0, 0x0000,
      0x0000, 0x0000, 0x0000, 0xffe0, 0xffe0, 0xffe0, 0xffe0, 0x0000, 0x0000, 0x0000,
      0xffe0, 0x0000, 0xffe0, 0xffe0, 0xffe0, 0xffe0, 0xffe0, 0xffe0, 0x0000, 0xffe0,
      0x0000, 0x0000, 0xffe0, 0xffe0, 0xffe0, 0xffe0, 0xffe0, 0xffe0, 0x0000, 0x0000,
  };
  
  RGB64x32MatrixPanel_I2S_DMA matrix;

  matrix.drawIcon (half_sun, 0,0,10,5);
*/

  int i, j;
  for (i = 0; i < rows; i++) {
    for (j = 0; j < cols; j++) {
      drawPixelRGB565 (x + j, y + i, ico[i * cols + j]);
    }
  }  
}




inline void RGB64x32MatrixPanel_I2S_DMA::fillScreen(uint16_t color)  // adafruit virtual void override
{
  uint8_t r = ((((color >> 11) & 0x1F) * 527) + 23) >> 6;
  uint8_t g = ((((color >> 5) & 0x3F) * 259) + 33) >> 6;
  uint8_t b = (((color & 0x1F) * 527) + 23) >> 6;
  
  updateMatrixDMABuffer(r, g, b); // the RGB only (no pixel coordinate) version of 'updateMatrixDMABuffer'
} 

// For adafruit
inline void RGB64x32MatrixPanel_I2S_DMA::drawPixelRGB565(int16_t x, int16_t y, uint16_t color) 
{
  uint8_t r = ((((color >> 11) & 0x1F) * 527) + 23) >> 6;
  uint8_t g = ((((color >> 5) & 0x3F) * 259) + 33) >> 6;
  uint8_t b = (((color & 0x1F) * 527) + 23) >> 6;
  
  updateMatrixDMABuffer( x, y, r, g, b);
}

inline void RGB64x32MatrixPanel_I2S_DMA::drawPixelRGB888(int16_t x, int16_t y, uint8_t r, uint8_t g,uint8_t b) 
{
  updateMatrixDMABuffer( x, y, r, g, b);
}

inline void RGB64x32MatrixPanel_I2S_DMA::drawPixelRGB24(int16_t x, int16_t y, rgb_24 color) 
{
  updateMatrixDMABuffer( x, y, color.red, color.green, color.blue);
}


// Pass 8-bit (each) R,G,B, get back 16-bit packed color
//https://github.com/squix78/ILI9341Buffer/blob/master/ILI9341_SPI.cpp
inline uint16_t RGB64x32MatrixPanel_I2S_DMA::color565(uint8_t r, uint8_t g, uint8_t b) {

/*
  Serial.printf("Got r value of %d\n", r);
  Serial.printf("Got g value of %d\n", g);
  Serial.printf("Got b value of %d\n", b);
  */
  
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}


// Promote 3/3/3 RGB to Adafruit_GFX 5/6/5 RRRrrGGGgggBBBbb
inline uint16_t RGB64x32MatrixPanel_I2S_DMA::Color333(uint8_t r, uint8_t g, uint8_t b) { 

	return ((r & 0x7) << 13) | ((r & 0x6) << 10) | ((g & 0x7) << 8) | ((g & 0x7) << 5) | ((b & 0x7) << 2) | ((b & 0x6) >> 1);
	
}

#endif
