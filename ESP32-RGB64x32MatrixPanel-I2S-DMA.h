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

	Finally, the binary code modulated intensity of a LED does not correspond to the intensity as seen by human eyes. To correct for that, a
	luminance correction is used. See val2pwm.c for more info.

	Note: Because every subframe contains one bit of grayscale information, they are also referred to as 'bitplanes' by the code below.

*/

/***************************************************************************************/
/* ESP32 Pin Definition. You can change this, but best if you keep it as is...         */

#define R1_PIN  25
#define G1_PIN  26
#define B1_PIN  27
#define R2_PIN  14
#define G2_PIN  12
#define B2_PIN  13

#define A_PIN   23
#define B_PIN   22
#define C_PIN   5
#define D_PIN   17
#define E_PIN   -1

#define LAT_PIN 4
#define OE_PIN  15

#define CLK_PIN 16

/***************************************************************************************/
/* HUB75 RGB Panel definitions and DMA Config. It's best you don't change any of this. */

#define MATRIX_HEIGHT			    32
#define MATRIX_WIDTH			    64
#define MATRIX_ROWS_IN_PARALLEL   	2

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
#define ROWS_PER_FRAME      (MATRIX_HEIGHT/MATRIX_ROWS_IN_PARALLEL) //  = 2

/***************************************************************************************/
/* You really don't want to change this stuff                                          */

#define CLKS_DURING_LATCH   0  // ADDX is output directly using GPIO
#define MATRIX_I2S_MODE I2S_PARALLEL_BITS_16
#define MATRIX_DATA_STORAGE_TYPE uint16_t

#define ESP32_NUM_FRAME_BUFFERS           2 
#define ESP32_OE_OFF_CLKS_AFTER_LATCH     1
#define ESP32_I2S_CLOCK_SPEED (20000000UL)
#define COLOR_DEPTH 24     

/***************************************************************************************/            

// note: sizeof(data) must be multiple of 32 bits, as ESP32 DMA linked list buffer address pointer must be word-aligned.
struct rowBitStruct {
    MATRIX_DATA_STORAGE_TYPE data[((MATRIX_WIDTH * MATRIX_HEIGHT) / 32) + CLKS_DURING_LATCH]; 
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
    RGB64x32MatrixPanel_I2S_DMA(bool _doubleBuffer = false) // doublebuffer always enabled, option makes no difference
      : Adafruit_GFX(MATRIX_WIDTH, MATRIX_HEIGHT), doubleBuffer(_doubleBuffer) {
      allocateDMAbuffers();
	  
		backbuf_id = 0;
		brightness = 64; // default to max brightness, wear sunglasses when looking directly at panel.
		
    }
	
    void begin(void)
    {
      configureDMA(); //DMA and I2S configuration and setup

      flushDMAbuffer();
	  swapBuffer();
	  flushDMAbuffer();
	  swapBuffer();
    }
 
	
    // Draw pixels
    virtual void drawPixel(int16_t x, int16_t y, uint16_t color); // adafruit implementation
    inline void drawPixelRGB565(int16_t x, int16_t y, uint16_t color);
    inline void drawPixelRGB888(int16_t x, int16_t y, uint8_t r, uint8_t g, uint8_t b);
	inline void drawPixelRGB24(int16_t x, int16_t y, rgb_24 color);
	
	// TODO: Draw a frame! Oooh.
	//void writeRGB24Frame2DMABuffer(rgb_24 *framedata, int16_t frame_width, int16_t frame_height);

	
    
    // Color 444 is a 4 bit scale, so 0 to 15, color 565 takes a 0-255 bit value, so scale up by 255/15 (i.e. 17)!
    uint16_t color444(uint8_t r, uint8_t g, uint8_t b) { return color565(r*17,g*17,b*17); }

    // Converts RGB888 to RGB565
    uint16_t color565(uint8_t r, uint8_t g, uint8_t b); // This is what is used by Adafruit GFX!

    void swapBuffer() {
      backbuf_id ^=1;
    }

    void setBrightness(int _brightness)
    {
	  // Change to set the brightness of the display, range of 1 to matrixWidth (i.e. 1 - 64)
      brightness = _brightness;
    }


   // ------- PRIVATE -------
  private:
  
    void allocateDMAbuffers() 
  	{
  		matrixUpdateFrames = (frameStruct *)heap_caps_malloc(sizeof(frameStruct) * ESP32_NUM_FRAME_BUFFERS, MALLOC_CAP_DMA);
  		Serial.printf("Allocating Refresh Buffer:\r\nDMA Memory Available: %d bytes total, %d bytes largest free block: \r\n", heap_caps_get_free_size(MALLOC_CAP_DMA), heap_caps_get_largest_free_block(MALLOC_CAP_DMA));
      
  	} // end initMatrixDMABuffer()
	
	void flushDMAbuffer()
	{
		 Serial.printf("Flushing buffer %d", backbuf_id);
		  // Need to wipe the contents of the matrix buffers or weird things happen.
		  for (int y=0;y<MATRIX_HEIGHT; y++)
			for (int x=0;x<MATRIX_WIDTH; x++)
			  updateMatrixDMABuffer( x, y, 0, 0, 0);
	}

    void configureDMA(); // Get everything setup. Refer to the .c file

    
    // Paint a pixel to the DMA buffer directly
    void updateMatrixDMABuffer(int16_t x, int16_t y, uint8_t red, uint8_t green, uint8_t blue);
  
  	
  	// Internal variables
  	bool dma_configuration_success;
    bool doubleBuffer;
  	
  	// Pixel data is organized from LSB to MSB sequentially by row, from row 0 to row matrixHeight/matrixRowsInParallel (two rows of pixels are refreshed in parallel)
  	frameStruct *matrixUpdateFrames;
  
  	int  lsbMsbTransitionBit;
  	int  refreshRate; 
  	
  	int  backbuf_id; // which buffer is the DMA backbuffer, as in, which one is not active so we can write to it
    int  brightness;

}; // end Class header

/***************************************************************************************/   

inline void RGB64x32MatrixPanel_I2S_DMA::drawPixel(int16_t x, int16_t y, uint16_t color) 
{
  drawPixelRGB565( x, y, color);
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


  


#endif
