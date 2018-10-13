// Copyright 2017 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "esp_heap_caps.h"
#include "anim.h"
#include "val2pwm.h"
#include "esp32_i2s_parallel.h"
#include "CircularBuffer.h"



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

#define matrixHeight            32
#define matrixWidth             64
#define matrixRowsInParallel    2


#define ESP32_NUM_FRAME_BUFFERS           2 // from SmartMatrixMultiPlexedRefresESP32.h
#define ESP32_OE_OFF_CLKS_AFTER_LATCH     1
#define ESP32_I2S_CLOCK_SPEED (20000000UL)


#define COLOR_DEPTH 24                  // known working: 24, 48 - If the sketch uses type `rgb24` directly, COLOR_DEPTH must be 24
const uint8_t kMatrixWidth = 64;        // known working: 32, 64, 96, 128
const uint8_t kMatrixHeight = 32;       // known working: 16, 32, 48, 64
const uint8_t kRefreshDepth = 24;       // known working: 24, 36, 48
const uint8_t kDmaBufferRows = 2;       // known working: 2-4, use 2 to save memory, more to keep from dropping frames and automatically lowering refresh rate

//This is the bit depth, per RGB subpixel, of the data that is sent to the display.
//The effective bit depth (in computer pixel terms) is less because of the PWM correction. With
//a bitplane count of 7, you should be able to reproduce an 16-bit image more or less faithfully, though.
#define BITPLANE_CNT 7

// LSBMSB_TRANSITION_BIT defines the color bit that is refreshed once per frame, with the same brightness as the bits above it
// when LSBMSB_TRANSITION_BIT is non-zero, all color bits below it will be be refreshed only once, with fractional brightness, saving RAM and speeding up refresh
// LSBMSB_TRANSITION_BIT must be < BITPLANE_CNT
#define LSBMSB_TRANSITION_BIT   1

//64*32 RGB leds, 2 pixels per 16-bit value...
#define BITPLANE_SZ (matrixWidth*matrixHeight/matrixRowsInParallel)

// From MatrixHardware_ESP32_V0
// ADDX is output directly using GPIO
#define CLKS_DURING_LATCH   0 
#define MATRIX_I2S_MODE I2S_PARALLEL_BITS_16
#define MATRIX_DATA_STORAGE_TYPE uint16_t

//#define CLKS_DURING_LATCH   4
//#define MATRIX_I2S_MODE I2S_PARALLEL_BITS_8
//#define MATRIX_DATA_STORAGE_TYPE uint8_t



//Upper half RGB
#define BIT_R1  (1<<0)   
#define BIT_G1  (1<<1)   
#define BIT_B1  (1<<2)   
//Lower half RGB
#define BIT_R2  (1<<3)   
#define BIT_G2  (1<<4)   
#define BIT_B2  (1<<5)   

// Control Signals
#define BIT_LAT (1<<6) 
#define BIT_OE  (1<<7)  

#define BIT_A (1<<8)    
#define BIT_B (1<<9)    
#define BIT_C (1<<10)   
#define BIT_D (1<<11)   
#define BIT_E (1<<12)   

// Pin Definitions
/*
#define R1_PIN  2
#define G1_PIN  15
#define B1_PIN  4
#define R2_PIN  16
#define G2_PIN  27
#define B2_PIN  17

#define A_PIN   5
#define B_PIN   18
#define C_PIN   19
#define D_PIN   21
#define LAT_PIN 26
#define OE_PIN  25

#define CLK_PIN 22
*/

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
#define LAT_PIN 4
#define OE_PIN  15

#define CLK_PIN 16


#define MATRIX_PANEL_HEIGHT 32
#define MATRIX_STACK_HEIGHT (matrixHeight / MATRIX_PANEL_HEIGHT)

#define PIXELS_PER_LATCH    ((matrixWidth * matrixHeight) / MATRIX_PANEL_HEIGHT) // = 64
#define ROW_PAIR_OFFSET 16

#define COLOR_CHANNELS_PER_PIXEL        3
#define LATCHES_PER_ROW (kRefreshDepth/COLOR_CHANNELS_PER_PIXEL)
#define COLOR_DEPTH_BITS (kRefreshDepth/COLOR_CHANNELS_PER_PIXEL)
#define ROWS_PER_FRAME 16




// note: sizeof(data) must be multiple of 32 bits, as DMA linked list buffer address pointer must be word-aligned.
struct rowBitStruct {
    MATRIX_DATA_STORAGE_TYPE data[((matrixWidth * matrixHeight) / 32) + CLKS_DURING_LATCH];
};

struct rowDataStruct {
    rowBitStruct rowbits[COLOR_DEPTH_BITS];
};

struct frameStruct {
    rowDataStruct rowdata[ROWS_PER_FRAME];
};




//Get a pixel from the image at pix, assuming the image is a 64x32 8R8G8B image
//Returns it as an uint32 with the lower 24 bits containing the RGB values.
static uint32_t getpixel(const unsigned char *pix, int x, int y) {
    const unsigned char *p=pix+((x+y*64)*3);
    return (p[0]<<16)|(p[1]<<8)|(p[2]);
}

int brightness=28; //Change to set the global brightness of the display, range 1-matrixWidth
                   //Warning when set too high: Do not look into LEDs with remaining eye.


                   

// pixel data is organized from LSB to MSB sequentially by row, from row 0 to row matrixHeight/matrixRowsInParallel (two rows of pixels are refreshed in parallel)
frameStruct *matrixUpdateFrames;

// other variables
uint8_t lsbMsbTransitionBit;

CircularBuffer dmaBuffer;
uint16_t refreshRate; 


void setup() {    

  Serial.begin(115200);
   // cbInit(&dmaBuffer, ESP32_NUM_FRAME_BUFFERS);

    printf("Starting SmartMatrix DMA Mallocs\r\n");

    matrixUpdateFrames = (frameStruct *)heap_caps_malloc(sizeof(frameStruct) * ESP32_NUM_FRAME_BUFFERS, MALLOC_CAP_DMA);
    assert("can't allocate SmartMatrix frameStructs");

    printf("Allocating refresh buffer:\r\nDMA Memory Available: %d bytes total, %d bytes largest free block: \r\n", heap_caps_get_free_size(MALLOC_CAP_DMA), heap_caps_get_largest_free_block(MALLOC_CAP_DMA));


  //  setupTimer();

    // calculate the lowest LSBMSB_TRANSITION_BIT value that will fit in memory
    int numDescriptorsPerRow;
    lsbMsbTransitionBit = 0;
    while(1) {
        numDescriptorsPerRow = 1;
        for(int i=lsbMsbTransitionBit + 1; i<COLOR_DEPTH_BITS; i++) {
            numDescriptorsPerRow += 1<<(i - lsbMsbTransitionBit - 1);
        }

        int ramrequired = numDescriptorsPerRow * ROWS_PER_FRAME * ESP32_NUM_FRAME_BUFFERS * sizeof(lldesc_t);
        int largestblockfree = heap_caps_get_largest_free_block(MALLOC_CAP_DMA);

        printf("lsbMsbTransitionBit of %d requires %d RAM, %d available, leaving %d free: \r\n", lsbMsbTransitionBit, ramrequired, largestblockfree, largestblockfree - ramrequired);

        if(ramrequired < (largestblockfree))
            break;
            
        if(lsbMsbTransitionBit < COLOR_DEPTH_BITS - 1)
            lsbMsbTransitionBit++;
        else
            break;
    }

    if(numDescriptorsPerRow * ROWS_PER_FRAME * ESP32_NUM_FRAME_BUFFERS * sizeof(lldesc_t) > heap_caps_get_largest_free_block(MALLOC_CAP_DMA)){
        assert("not enough RAM for SmartMatrix descriptors");
        printf("not enough RAM for SmartMatrix descriptors\r\n");
        return;
    }

    printf("Raised lsbMsbTransitionBit to %d/%d to fit in RAM\r\n", lsbMsbTransitionBit, COLOR_DEPTH_BITS - 1);

    // calculate the lowest LSBMSB_TRANSITION_BIT value that will fit in memory that will meet or exceed the configured refresh rate
    while(1) {
        int psPerClock = 1000000000000UL/ESP32_I2S_CLOCK_SPEED;
        int nsPerLatch = ((PIXELS_PER_LATCH + CLKS_DURING_LATCH) * psPerClock) / 1000;
        //printf("ns per latch: %d: \r\n", nsPerLatch);        

        // add time to shift out LSBs + LSB-MSB transition bit - this ignores fractions...
        int nsPerRow = COLOR_DEPTH_BITS * nsPerLatch;

        // add time to shift out MSBs
        for(int i=lsbMsbTransitionBit + 1; i<COLOR_DEPTH_BITS; i++)
            nsPerRow += (1<<(i - lsbMsbTransitionBit - 1)) * (COLOR_DEPTH_BITS - i) * nsPerLatch;

        //printf("nsPerRow: %d: \r\n", nsPerRow);        

        int nsPerFrame = nsPerRow * ROWS_PER_FRAME;
        //printf("nsPerFrame: %d: \r\n", nsPerFrame);        

        int actualRefreshRate = 1000000000UL/(nsPerFrame);

        refreshRate = actualRefreshRate;

        printf("lsbMsbTransitionBit of %d gives %d Hz refresh: \r\n", lsbMsbTransitionBit, actualRefreshRate);        

        if(lsbMsbTransitionBit < COLOR_DEPTH_BITS - 1)
            lsbMsbTransitionBit++;
        else
            break;
    }

    printf("Raised lsbMsbTransitionBit to %d/%d to meet minimum refresh rate\r\n", lsbMsbTransitionBit, COLOR_DEPTH_BITS - 1);

    // TODO: completely fill buffer with data before enabling DMA - can't do this now, lsbMsbTransition bit isn't set in the calc class - also this call will probably have no effect as matrixCalcDivider will skip the first call
    //matrixCalcCallback();

    // lsbMsbTransition Bit is now finalized - redo descriptor count in case it changed to hit min refresh rate
    numDescriptorsPerRow = 1;
    for(int i=lsbMsbTransitionBit + 1; i<COLOR_DEPTH_BITS; i++) {
        numDescriptorsPerRow += 1<<(i - lsbMsbTransitionBit - 1);
    }

    printf("Descriptors for lsbMsbTransitionBit %d/%d with %d rows require %d bytes of DMA RAM\r\n", lsbMsbTransitionBit, COLOR_DEPTH_BITS - 1, ROWS_PER_FRAME, 2 * numDescriptorsPerRow * ROWS_PER_FRAME * sizeof(lldesc_t));

    // malloc the DMA linked list descriptors that i2s_parallel will need
    int desccount = numDescriptorsPerRow * ROWS_PER_FRAME;
    lldesc_t * dmadesc_a = (lldesc_t *)heap_caps_malloc(desccount * sizeof(lldesc_t), MALLOC_CAP_DMA);
    assert("Can't allocate descriptor buffer a");
    if(!dmadesc_a) {
        printf("can't malloc");
        return;
    }
    lldesc_t * dmadesc_b = (lldesc_t *)heap_caps_malloc(desccount * sizeof(lldesc_t), MALLOC_CAP_DMA);
    assert("Can't allocate descriptor buffer b");
    if(!dmadesc_b) {
        printf("can't malloc");
        return;
    }

    printf("SmartMatrix Mallocs Complete\r\n");
    printf("Heap Memory Available: %d bytes total, %d bytes largest free block: \r\n", heap_caps_get_free_size(0), heap_caps_get_largest_free_block(0));
    printf("8-bit Accessible Memory Available: %d bytes total, %d bytes largest free block: \r\n", heap_caps_get_free_size(MALLOC_CAP_8BIT), heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    printf("32-bit Memory Available: %d bytes total, %d bytes largest free block: \r\n", heap_caps_get_free_size(MALLOC_CAP_32BIT), heap_caps_get_largest_free_block(MALLOC_CAP_32BIT));
    printf("DMA Memory Available: %d bytes total, %d bytes largest free block: \r\n", heap_caps_get_free_size(MALLOC_CAP_DMA), heap_caps_get_largest_free_block(MALLOC_CAP_DMA));

    lldesc_t *prevdmadesca = 0;
    lldesc_t *prevdmadescb = 0;
    int currentDescOffset = 0;

    // fill DMA linked lists for both frames
    for(int j=0; j<ROWS_PER_FRAME; j++) {
        // first set of data is LSB through MSB, single pass - all color bits are displayed once, which takes care of everything below and inlcluding LSBMSB_TRANSITION_BIT
        // TODO: size must be less than DMA_MAX - worst case for SmartMatrix Library: 16-bpp with 256 pixels per row would exceed this, need to break into two
        link_dma_desc(&dmadesc_a[currentDescOffset], prevdmadesca, &(matrixUpdateFrames[0].rowdata[j].rowbits[0].data), sizeof(rowBitStruct) * COLOR_DEPTH_BITS);
        prevdmadesca = &dmadesc_a[currentDescOffset];
        link_dma_desc(&dmadesc_b[currentDescOffset], prevdmadescb, &(matrixUpdateFrames[1].rowdata[j].rowbits[0].data), sizeof(rowBitStruct) * COLOR_DEPTH_BITS);
        prevdmadescb = &dmadesc_b[currentDescOffset];
        currentDescOffset++;
        //printf("row %d: \r\n", j);

        for(int i=lsbMsbTransitionBit + 1; i<COLOR_DEPTH_BITS; i++) {
            // binary time division setup: we need 2 of bit (LSBMSB_TRANSITION_BIT + 1) four of (LSBMSB_TRANSITION_BIT + 2), etc
            // because we sweep through to MSB each time, it divides the number of times we have to sweep in half (saving linked list RAM)
            // we need 2^(i - LSBMSB_TRANSITION_BIT - 1) == 1 << (i - LSBMSB_TRANSITION_BIT - 1) passes from i to MSB
            //printf("buffer %d: repeat %d times, size: %d, from %d - %d\r\n", nextBufdescIndex, 1<<(i - LSBMSB_TRANSITION_BIT - 1), (COLOR_DEPTH_BITS - i), i, COLOR_DEPTH_BITS-1);
            for(int k=0; k < 1<<(i - lsbMsbTransitionBit - 1); k++) {
                link_dma_desc(&dmadesc_a[currentDescOffset], prevdmadesca, &(matrixUpdateFrames[0].rowdata[j].rowbits[i].data), sizeof(rowBitStruct) * (COLOR_DEPTH_BITS - i));
                prevdmadesca = &dmadesc_a[currentDescOffset];
                link_dma_desc(&dmadesc_b[currentDescOffset], prevdmadescb, &(matrixUpdateFrames[1].rowdata[j].rowbits[i].data), sizeof(rowBitStruct) * (COLOR_DEPTH_BITS - i));
                prevdmadescb = &dmadesc_b[currentDescOffset];

                currentDescOffset++;
                //printf("i %d, j %d, k %d\r\n", i, j, k);
            }
        }
    }

    //End markers
    dmadesc_a[desccount-1].eof = 1;
    dmadesc_b[desccount-1].eof = 1;
    dmadesc_a[desccount-1].qe.stqe_next=(lldesc_t*)&dmadesc_a[0];
    dmadesc_b[desccount-1].qe.stqe_next=(lldesc_t*)&dmadesc_b[0];

    //printf("\n");

    i2s_parallel_config_t cfg={
        .gpio_bus={R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN, LAT_PIN, OE_PIN, A_PIN, B_PIN, C_PIN, D_PIN, -1, -1, -1, -1},
        .gpio_clk=CLK_PIN,
        .clkspeed_hz=ESP32_I2S_CLOCK_SPEED, //ESP32_I2S_CLOCK_SPEED,  // formula used is 80000000L/(cfg->clkspeed_hz + 1), must result in >=2.  Acceptable values 26.67MHz, 20MHz, 16MHz, 13.34MHz...
        .bits=MATRIX_I2S_MODE, //MATRIX_I2S_MODE,
        .bufa=0,
        .bufb=0,
        desccount,
        desccount,
        dmadesc_a,
        dmadesc_b
    };

    //Setup I2S
    i2s_parallel_setup_without_malloc(&I2S1, &cfg);

    printf("I2S setup done.\n");


          /*
    tempRow0Ptr = malloc(sizeof(rgb24) * PIXELS_PER_LATCH);
    tempRow1Ptr = malloc(sizeof(rgb24) * PIXELS_PER_LATCH);
        */

}

struct rgb24;

typedef struct rgb24 {
    rgb24() : rgb24(0,0,0) {}
    rgb24(uint8_t r, uint8_t g, uint8_t b) {
        red = r; green = g; blue = b;
    }
    rgb24& operator=(const rgb24& col);

    uint8_t red;
    uint8_t green;
    uint8_t blue;
} rgb24;

/*
#if defined(ESP32)
    // use buffers malloc'd previously
    rgb24 * tempRow0 = (rgb24*)tempRow0Ptr;
    rgb24 * tempRow1 = (rgb24*)tempRow1Ptr;
#else
    // static to avoid putting large buffer on the stack
    static rgb24 tempRow0[PIXELS_PER_LATCH];
    static rgb24 tempRow1[PIXELS_PER_LATCH];
#endif

*/

void loop() {
    static int apos=0; //which frame in the animation we're on
    static int backbuf_id=0; //which buffer is the backbuffer, as in, which one is not active so we can write to it
    unsigned char currentRow;
 
    printf("\r\nStarting SmartMatrix Mallocs\r\n");
    printf("Heap Memory Available: %d bytes total, %d bytes largest free block: \r\n", heap_caps_get_free_size(0), heap_caps_get_largest_free_block(0));
    printf("8-bit Accessible Memory Available: %d bytes total, %d bytes largest free block: \r\n", heap_caps_get_free_size(MALLOC_CAP_8BIT), heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    printf("32-bit Memory Available: %d bytes total, %d bytes largest free block: \r\n", heap_caps_get_free_size(MALLOC_CAP_32BIT), heap_caps_get_largest_free_block(MALLOC_CAP_32BIT));
    printf("DMA Memory Available: %d bytes total, %d bytes largest free block: \r\n", heap_caps_get_free_size(MALLOC_CAP_DMA), heap_caps_get_largest_free_block(MALLOC_CAP_DMA));
      
  //  tempRow0Ptr = malloc(sizeof(rgb24) * PIXELS_PER_LATCH);
  //  tempRow1Ptr = malloc(sizeof(rgb24) * PIXELS_PER_LATCH);
  
        
    while(1) {
        //Fill bitplanes with the data for the current image
        const uint8_t *pix=&anim[apos*64*32*3];     //pixel data for this animation frame

        
        for (unsigned int y=0; y<matrixHeight/matrixRowsInParallel; y++) // half height - 16 iterations
        {
          currentRow = y;
      /*
          // use buffers malloc'd previously
          rgb24 * tempRow0 = (rgb24*)tempRow0Ptr;
          rgb24 * tempRow1 = (rgb24*)tempRow1Ptr;
            
          // clear buffer to prevent garbage data showing through transparent layers
          memset(tempRow0, 0x00, sizeof(rgb24) * PIXELS_PER_LATCH);
          memset(tempRow1, 0x00, sizeof(rgb24) * PIXELS_PER_LATCH);
        */        
          for(int j=0; j<COLOR_DEPTH_BITS; j++)  // color depth - 8 iterations
          {
              int maskoffset = 0;
              /*
              if(COLOR_DEPTH_BITS == 12)   // 36-bit color
                  maskoffset = 4;
              else if (COLOR_DEPTH_BITS == 16) // 48-bit color
                  maskoffset = 0;
              else if (COLOR_DEPTH_BITS == 8)  // 24-bit color
                  maskoffset = 0;
              */
              uint16_t mask = (1 << (j + maskoffset));

              // SmartMatrix3<refreshDepth, matrixWidth, matrixHeight, panelType, optionFlags>::rowBitStruct *p=&(frameBuffer->rowdata[currentRow].rowbits[j]); //bitplane location to write to              
              //MATRIX_DATA_STORAGE_TYPE *p=matrixUpdateFrames[backbuf_id].rowdata[y].rowbits[pl].data; //matrixUpdateFrames
              rowBitStruct *p=&matrixUpdateFrames[backbuf_id].rowdata[currentRow].rowbits[j]; //matrixUpdateFrames location to write to
 
              int i=0;
              while(i < PIXELS_PER_LATCH) // row pixels (64) iterations
              {
                
                   // parse through matrixWith block of pixels, from left to right, or right to left, depending on C_SHAPE_STACKING options
                  for(int k=0; k < matrixWidth; k++) // row pixel width 64 iterations
                  { 
                      int v=0;
      
      #if (CLKS_DURING_LATCH == 0)
                      // if there is no latch to hold address, output ADDX lines directly to GPIO and latch data at end of cycle
                      int gpioRowAddress = currentRow;
                      
                      // normally output current rows ADDX, special case for LSB, output previous row's ADDX (as previous row is being displayed for one latch cycle)
                      if(j == 0)
                          gpioRowAddress = currentRow-1;
      
                      if (gpioRowAddress & 0x01) v|=BIT_A;
                      if (gpioRowAddress & 0x02) v|=BIT_B;
                      if (gpioRowAddress & 0x04) v|=BIT_C;
                      if (gpioRowAddress & 0x08) v|=BIT_D;
                    //  if (gpioRowAddress & 0x10) v|=BIT_E;
      
                      // need to disable OE after latch to hide row transition
                      if((i+k) == 0) v|=BIT_OE;
      
                      // drive latch while shifting out last bit of RGB data
                      if((i+k) == PIXELS_PER_LATCH-1) v|=BIT_LAT;
      #endif
            
                      // turn off OE after brightness value is reached when displaying MSBs
                      // MSBs always output normal brightness
                      // LSB (!j) outputs normal brightness as MSB from previous row is being displayed
                      if((j > lsbMsbTransitionBit || !j) && ((i+k) >= brightness)) v|=BIT_OE;
      
                      // special case for the bits *after* LSB through (lsbMsbTransitionBit) - OE is output after data is shifted, so need to set OE to fractional brightness
                      if(j && j <= lsbMsbTransitionBit) {
                          // divide brightness in half for each bit below lsbMsbTransitionBit
                          int lsbBrightness = brightness >> (lsbMsbTransitionBit - j + 1);
                          if((i+k) >= lsbBrightness) v|=BIT_OE;
                      }
      
                      // need to turn off OE one clock before latch, otherwise can get ghosting
                      if((i+k)==PIXELS_PER_LATCH-1) v|=BIT_OE;

    
                    
                    //c2 = {0,0,0};
                    
                    int c1, c2; // 32 bit int
#if 1
                     /*
                    //uint32_t testpixel = 0xFFFFFFFF;
                    uint32_t testpixel = 0x7F7F7F7F;
                    //uint32_t testpixel = 0x80808080;
                    
                    if((31 - i) == y)
                        c1=testpixel;
                    else
                        c1 = 0x00;
                    if((31 - i) == y+16)
                        c2=testpixel;
                    else
                        c2 = 0x00;

                    c1 = 0xFFFFFFFF;
                    */

                    c1=getpixel(pix, k, y);
                    c2=getpixel(pix, k, y+(matrixHeight/2));
                    
                    if (c1 & (mask<<16)) v|=BIT_R1;
                    if (c1 & (mask<<8)) v|=BIT_G1;
                    if (c1 & (mask<<0)) v|=BIT_B1;
                    if (c2 & (mask<<16)) v|=BIT_R2;
                    if ( c2 & (mask<<8)) v|=BIT_G2;
                    if (c2 & (mask<<0)) v|=BIT_B2;
                                         
#else                           

                    struct rgb24 c1( 255,0,0);
                    struct rgb24 c2 = { 0,0,255 };
                    
     
                      if (c1.red & mask)
                          v|=BIT_R1;
                      if (c1.green & mask)
                          v|=BIT_G1;
                      if (c1.blue & mask)
                          v|=BIT_B1;
                      if (c2.red & mask)
                          v|=BIT_R2;
                      if (c2.green & mask) 
                          v|=BIT_G2;
                      if (c2.blue & mask)
                          v|=BIT_B2;

#endif               

                    /*
                    // 8 bit parallel mode
                    //Save the calculated value to the bitplane memory in 16-bit reversed order to account for I2S Tx FIFO mode1 ordering
                    if(k%4 == 0){
                        p->data[(i+k)+2] = v;
                    } else if(k%4 == 1) {
                        p->data[(i+k)+2] = v;
                    } else if(k%4 == 2) {
                        p->data[(i+k)-2] = v;
                    } else { //if(k%4 == 3)
                        p->data[(i+k)-2] = v;
                    }
                    */
        
                    // 16 bit parallel mode
                    //Save the calculated value to the bitplane memory in reverse order to account for I2S Tx FIFO mode1 ordering
                    if(k%2){
                        p->data[(i+k)-1] = v;
                    } else {
                        p->data[(i+k)+1] = v;
                    } // end reordering

                  } // end for matrixwidth
      
                  i += matrixWidth;
                  
              } // end pixels per latch loop (64)
      
          } // color depth loop (8)

                 // printf("Processing row %d \n", y)           ;
                  //delay(50);

          
      } // half matrix height (16)

        
        //Show our work!
        i2s_parallel_flip_to_buffer(&I2S1, backbuf_id);
        backbuf_id^=1;
        //Bitplanes are updated, new image shows now.
        vTaskDelay(100 / portTICK_PERIOD_MS); //animation has an 100ms interval

        if (true) {
            //show next frame of Nyancat animation
            apos++;
            if (apos>=12) apos=0;
        } else {
            //show Lena
            apos=12;
        }
    } // end while(1)
} // end loop
