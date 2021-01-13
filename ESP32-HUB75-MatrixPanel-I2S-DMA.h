#ifndef _ESP32_RGB_64_32_MATRIX_PANEL_I2S_DMA
#define _ESP32_RGB_64_32_MATRIX_PANEL_I2S_DMA

/***************************************************************************************/
/* COMPILE-TIME OPTIONS - CONFIGURE AS DESIRED                                         */
/***************************************************************************************/
/* Enable serial debugging of the library, to see how memory is allocated etc. */
//#define SERIAL_DEBUG 1

/* Use GFX_Root (https://github.com/mrfaptastic/GFX_Root) instead of 
 * Adafruit_GFX library. No real benefit unless you don't want Bus_IO & Wire.h library dependencies. 
 */
//#define USE_GFX_ROOT 1


/* Physical / Chained HUB75(s) RGB pixel WIDTH and HEIGHT. 
 *
 * This library has been tested with a 64x32 and 64x64 RGB panels. 
 * If you want to chain two or more of these horizontally to make a 128x32 panel
 * you can do so with the cable and then set the CHAIN_LENGTH to '2'.
 *
 * Also, if you use a 64x64 panel, then set the MATRIX_HEIGHT to '64' and an E_PIN; it will work!
 *
 * All of this is memory permitting of course (dependant on your sketch etc.) ...
 *
 */
#ifndef MATRIX_WIDTH
 #define MATRIX_WIDTH                64   // Single panel of 64 pixel width
#endif

#ifndef MATRIX_HEIGHT
 #define MATRIX_HEIGHT               32   // CHANGE THIS VALUE to 64 IF USING 64px HIGH panel(s) with E PIN
#endif

#ifndef CHAIN_LENGTH
 #define CHAIN_LENGTH                1   // Number of modules chained together, i.e. 4 panels chained result in virtualmatrix 64x4=256 px long
#endif

/* ESP32 Default Pin definition. You can change this, but best if you keep it as is and provide custom pin mappings 
 * as part of the begin(...) function.
 */
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
#define E_PIN_DEFAULT   -1 // IMPORTANT: Change to a valid pin if using a 64x64px panel.
          
#define LAT_PIN_DEFAULT 4
#define OE_PIN_DEFAULT  15
#define CLK_PIN_DEFAULT 16

// Interesting Fact: We end up using a uint16_t to send data in parallel to the HUB75... but 
//                   given we only map to 14 physical output wires/bits, we waste 2 bits.

/***************************************************************************************/
/* Do not change definitions below unless you pretty sure you know what you are doing! */
#ifndef ESP32_I2S_CLOCK_SPEED
  #define ESP32_I2S_CLOCK_SPEED       (10000000UL)            // @ 10Mhz
#endif

// RGB Panel Constants / Calculated Values
#ifndef MATRIX_ROWS_IN_PARALLEL
 #define MATRIX_ROWS_IN_PARALLEL     2
#endif

// 8bit per RGB color = 24 bit/per pixel,
// might be reduced to save RAM but it corrupts colors badly (curently broken) 
#ifndef PIXEL_COLOR_DEPTH_BITS
 #define PIXEL_COLOR_DEPTH_BITS      8
#endif

#define COLOR_CHANNELS_PER_PIXEL     3

/***************************************************************************************/
/* Definitions below shold NOT be ever changed without rewriting libraly logic         */
#define ESP32_I2S_DMA_MODE          I2S_PARALLEL_BITS_16    // Pump 16 bits out in parallel
#define ESP32_I2S_DMA_STORAGE_TYPE  uint16_t                // one uint16_t at a time.
#define CLKS_DURING_LATCH            0   // Not (yet) used. 

// Panel Upper half RGB (numbering according to order in DMA gpio_bus configuration)
#define BITS_RGB1_OFFSET 0 // Start point of RGB_X1 bits
#define BIT_R1  (1<<0)   
#define BIT_G1  (1<<1)   
#define BIT_B1  (1<<2)   

// Panel Lower half RGB
#define BITS_RGB2_OFFSET 3 // Start point of RGB_X2 bits
#define BIT_R2  (1<<3)   
#define BIT_G2  (1<<4)   
#define BIT_B2  (1<<5)   

// Panel Control Signals
#define BIT_LAT (1<<6) 
#define BIT_OE  (1<<7)  

// Panel GPIO Pin Addresses (A, B, C, D etc..)
#define BITS_ADDR_OFFSET 8  // Start point of address bits
#define BIT_A (1<<8)    
#define BIT_B (1<<9)    
#define BIT_C (1<<10)   
#define BIT_D (1<<11)   
#define BIT_E (1<<12)   

#define BITMASK_RGB1_CLEAR   (0xfff8)    // inverted bitmask for R1G1B1 bit in pixel vector
#define BITMASK_RGB2_CLEAR   (0xffc7)    // inverted bitmask for R2G2B2 bit in pixel vector
#define BITMASK_RGB12_CLEAR  (0xffc0)    // inverted bitmask for R1G1B1R2G2B2 bit in pixel vector
#define BITMASK_CTRL_CLEAR   (0xe03f)    // inverted bitmask for control bits ABCDE,LAT,OE in pixel vector
#define BITMASK_OE_CLEAR     (0xff7f)    // inverted bitmask for control bit OE in pixel vector


/***************************************************************************************/
// Lib includes
#include <memory>
#include "esp_heap_caps.h"
#include "esp32_i2s_parallel.h"

#ifdef USE_GFX_ROOT
	#include "GFX.h" // Adafruit GFX core class -> https://github.com/mrfaptastic/GFX_Root
#else
	#include "Adafruit_GFX.h" // Adafruit class with all the other stuff
#endif

/***************************************************************************************/

/** @brief - Structure holds raw DMA data to drive TWO full rows of pixels spaning through all chained modules
 * Note: sizeof(data) must be multiple of 32 bits, as ESP32 DMA linked list buffer address pointer must be word-aligned
 */
struct rowBitStruct {
    const size_t width;
    const uint8_t color_depth;
    const bool double_buff;
    ESP32_I2S_DMA_STORAGE_TYPE *data;

    /** @brief - returns size (in bytes) of data vector holding _dpth bits of colors for a SINGLE buff
     * default - returns full data vector size for a SINGLE buff
     *
     */
    size_t size(uint8_t _dpth=0 ) { if (!_dpth) _dpth = color_depth; return width * _dpth * sizeof(ESP32_I2S_DMA_STORAGE_TYPE); };

    /** @brief - returns pointer to the row's data vector at _dpth color bit
     * default - returns pointer to the data vector's head
     */
    ESP32_I2S_DMA_STORAGE_TYPE* getDataPtr(const uint8_t _dpth=0, const bool buff_id=0) { return &(data[_dpth*width + buff_id*(width*color_depth)]); };

    // constructor - allocates DMA-capable memory to hold the struct data
    rowBitStruct(const size_t _cnt, const uint8_t _depth, const bool _dbuff) : width(_cnt), color_depth(_depth), double_buff(_dbuff) {
      data = (ESP32_I2S_DMA_STORAGE_TYPE *)heap_caps_malloc( size()+size()*double_buff, MALLOC_CAP_DMA);
    }
    ~rowBitStruct() { delete data;}
};


/* frameStruct
 * Note: A 'frameStruct' contains ALL the data for a full-frame (i.e. BOTH 2x16-row frames are
 *       are contained in parallel within the one uint16_t that is sent in parallel to the HUB75). 
 * 
 *       This structure isn't actually allocated in one memory block anymore, as the library now allocates
 *       memory per row (per rowColorDepthStruct) instead.
 */
struct frameStruct {
    uint8_t rows=0;    // number of rows held in current frame, not used actually, just to keep the idea of struct
    std::vector<std::shared_ptr<rowBitStruct> > rowBits;
};

typedef struct RGB24 {
    RGB24() : RGB24(0,0,0) {}
    RGB24(uint8_t r, uint8_t g, uint8_t b) {
        red = r; green = g; blue = b;
    }
    RGB24& operator=(const RGB24& col);

    uint8_t red;
    uint8_t green;
    uint8_t blue;
} RGB24;


/***************************************************************************************/   
// Used by val2PWM
//C/p'ed from https://ledshield.wordpress.com/2012/11/13/led-brightness-to-your-eye-gamma-correction-no/
// Example calculator: https://gist.github.com/mathiasvr/19ce1d7b6caeab230934080ae1f1380e
static const uint8_t lumConvTab[]={ 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 7, 7, 7, 7, 8, 8, 8, 8, 9, 9, 9, 10, 10, 10, 11, 11, 11, 12, 12, 12, 13, 13, 13, 14, 14, 14, 15, 15, 16, 16, 17, 17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 23, 24, 24, 25, 25, 26, 27, 27, 28, 28, 29, 30, 30, 31, 31, 32, 33, 33, 34, 35, 35, 36, 37, 38, 38, 39, 40, 41, 41, 42, 43, 44, 45, 45, 46, 47, 48, 49, 50, 51, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 73, 74, 75, 76, 77, 78, 80, 81, 82, 83, 84, 86, 87, 88, 90, 91, 92, 93, 95, 96, 98, 99, 100, 102, 103, 105, 106, 107, 109, 110, 112, 113, 115, 116, 118, 120, 121, 123, 124, 126, 128, 129, 131, 133, 134, 136, 138, 139, 141, 143, 145, 146, 148, 150, 152, 154, 156, 157, 159, 161, 163, 165, 167, 169, 171, 173, 175, 177, 179, 181, 183, 185, 187, 189, 192, 194, 196, 198, 200, 203, 205, 207, 209, 212, 214, 216, 218, 221, 223, 226, 228, 230, 233, 235, 238, 240, 243, 245, 248, 250, 253, 255, 255};


/** @brief - configuration values for HUB75_I2S driver
 *  This structure holds configuration vars that are used as
 *  an initialization values when creating an instance of MatrixPanel_I2S_DMA object.
 *  All params have it's default values.
 */
struct  HUB75_I2S_CFG {

  /**
   * Enumeration of hardware-specific chips
   * used to drive matrix modules
   */
  enum shift_driver {SHIFT=0, FM6124, FM6126A, ICN2038S};

  /**
   * I2S clock speed selector
   */
  enum clk_speed {HZ_10M=10000000, HZ_13340K=13340000, HZ_16M=16000000, HZ_20M=20000000, HZ_26670K=26670000};

  // Structure Variables

  // physical width of a single matrix panel module (in pixels, usually it is 64 ;) )
  uint16_t mx_width;
  // physical height of a single matrix panel module (in pixels, usually amost always it is either 32 or 64)
  uint16_t mx_height;
  // number of chained panels regardless of the topology, default 1 - a single matrix module
  uint16_t chain_length;
  /**
   * GPIO pins mapping
   */
  struct i2s_pins{
    int8_t r1, g1, b1, r2, g2, b2, a, b, c, d, e, lat, oe, clk;
  } gpio;

  // Matrix driver chip type - default is a plain shift register
  shift_driver driver;
  // I2S clock speed
  clk_speed i2sspeed;
  // use DMA double buffer (twice as much RAM required)
  bool double_buff;

  // struct constructor
  HUB75_I2S_CFG (
    uint16_t _w = MATRIX_WIDTH,
    uint16_t _h = MATRIX_HEIGHT,
    uint16_t _chain = CHAIN_LENGTH,
    i2s_pins _pinmap = {
      R1_PIN_DEFAULT, G1_PIN_DEFAULT, B1_PIN_DEFAULT, R2_PIN_DEFAULT, G2_PIN_DEFAULT, B2_PIN_DEFAULT,
      A_PIN_DEFAULT, B_PIN_DEFAULT, C_PIN_DEFAULT, D_PIN_DEFAULT, E_PIN_DEFAULT,
      LAT_PIN_DEFAULT, OE_PIN_DEFAULT, CLK_PIN_DEFAULT },
    shift_driver _drv = SHIFT,
    bool _dbuff = false,
    clk_speed _i2sspeed = HZ_10M
  ) : mx_width(_w),
      mx_height(_h),
      chain_length(_chain),
      gpio(_pinmap),
      driver(_drv), i2sspeed(_i2sspeed),
      double_buff(_dbuff) {}

}; // end of structure HUB75_I2S_CFG



/***************************************************************************************/   
#ifdef USE_GFX_ROOT
class MatrixPanel_I2S_DMA : public GFX {
#else
class MatrixPanel_I2S_DMA : public Adafruit_GFX {	
#endif

  // ------- PUBLIC -------
  public:
    
    /**
     * MatrixPanel_I2S_DMA 
     * 
     * @param  {bool} _double_buffer : Double buffer is disabled by default. Enable only if you know what you're doing. Manual switching required with flipDMABuffer() and showDMABuffer()
     *        
     */
    MatrixPanel_I2S_DMA(const HUB75_I2S_CFG& opts)
#ifdef USE_GFX_ROOT	
      : GFX(opts.mx_width*opts.chain_length, opts.mx_height), m_cfg(opts)  {
#else
      : Adafruit_GFX(opts.mx_width*opts.chain_length, opts.mx_height), m_cfg(opts) {
#endif		  
    }

    /* Propagate the DMA pin configuration, allocate DMA buffs and start data ouput, initialy blank */
    bool begin(){

      // Change 'if' to '1' to enable, 0 to not include this Serial output in compiled program        
      #if SERIAL_DEBUG       
            Serial.printf_P(PSTR("Using pin %d for the R1_PIN\n"), m_cfg.gpio.r1);
            Serial.printf_P(PSTR("Using pin %d for the G1_PIN\n"), m_cfg.gpio.g1);
            Serial.printf_P(PSTR("Using pin %d for the B1_PIN\n"), m_cfg.gpio.b1);
            Serial.printf_P(PSTR("Using pin %d for the R2_PIN\n"), m_cfg.gpio.r2);
            Serial.printf_P(PSTR("Using pin %d for the G2_PIN\n"), m_cfg.gpio.g2);
            Serial.printf_P(PSTR("Using pin %d for the B2_PIN\n"), m_cfg.gpio.b2);
            Serial.printf_P(PSTR("Using pin %d for the A_PIN\n"), m_cfg.gpio.a);
            Serial.printf_P(PSTR("Using pin %d for the B_PIN\n"), m_cfg.gpio.b);
            Serial.printf_P(PSTR("Using pin %d for the C_PIN\n"), m_cfg.gpio.c);
            Serial.printf_P(PSTR("Using pin %d for the D_PIN\n"), m_cfg.gpio.d);
            Serial.printf_P(PSTR("Using pin %d for the E_PIN\n"), m_cfg.gpio.e);
            Serial.printf_P(PSTR("Using pin %d for the LAT_PIN\n"), m_cfg.gpio.lat);
            Serial.printf_P(PSTR("Using pin %d for the OE_PIN\n"),  m_cfg.gpio.oe);
            Serial.printf_P(PSTR("Using pin %d for the CLK_PIN\n"), m_cfg.gpio.clk);
      #endif   

      // initialize some sppecific panel drivers
      if (m_cfg.driver)
        shiftDriver(m_cfg);

     /* As DMA buffers are dynamically allocated, we must allocated in begin()
      * Ref: https://github.com/espressif/arduino-esp32/issues/831
      */
      if ( !allocateDMAmemory() ) {  return false; } // couldn't even get the basic ram required.
        

      // Flush the DMA buffers prior to configuring DMA - Avoid visual artefacts on boot.
      clearScreen(); // Must fill the DMA buffer with the initial output bit sequence or the panel will display garbage

      // Setup the ESP32 DMA Engine. Sprite_TM built this stuff.
      configureDMA(m_cfg); //DMA and I2S configuration and setup

      showDMABuffer(); // show backbuf_id of 0

      #if SERIAL_DEBUG 
        if (!initialized)    
              Serial.println(F("MatrixPanel_I2S_DMA::begin() failed."));
      #endif      

      return initialized;

    }
    
    // TODO: Disable/Enable auto buffer flipping (useful for lots of drawPixel usage)...

    // Draw pixels
    virtual void drawPixel(int16_t x, int16_t y, uint16_t color);   // overwrite adafruit implementation
    virtual void fillScreen(uint16_t color);                        // overwrite adafruit implementation
            void clearScreen();
  	void fillScreenRGB888(uint8_t r, uint8_t g, uint8_t b);
    void drawPixelRGB565(int16_t x, int16_t y, uint16_t color);
    void drawPixelRGB888(int16_t x, int16_t y, uint8_t r, uint8_t g, uint8_t b);
    void drawPixelRGB24(int16_t x, int16_t y, RGB24 color);
    void drawIcon (int *ico, int16_t x, int16_t y, int16_t cols, int16_t rows);
    
    // Color 444 is a 4 bit scale, so 0 to 15, color 565 takes a 0-255 bit value, so scale up by 255/15 (i.e. 17)!
    uint16_t color444(uint8_t r, uint8_t g, uint8_t b) { return color565(r*17,g*17,b*17); }

    // Converts RGB888 to RGB565
    uint16_t color565(uint8_t r, uint8_t g, uint8_t b); // This is what is used by Adafruit GFX!
    
    // Converts RGB333 to RGB565
    uint16_t color333(uint8_t r, uint8_t g, uint8_t b); // This is what is used by Adafruit GFX! Not sure why they have a capital 'C' for this particular function.

    inline void flipDMABuffer() 
    {         
      if ( !m_cfg.double_buff) return;
        
        // Flip to other buffer as the backbuffer. i.e. Graphic changes happen to this buffer (but aren't displayed until showDMABuffer())
        back_buffer_id ^= 1; 
        
        #if SERIAL_DEBUG     
                Serial.printf_P(PSTR("Set back buffer to: %d\n"), back_buffer_id);
        #endif      

        // Wait before we allow any writing to the buffer. Stop flicker.
        while(!i2s_parallel_is_previous_buffer_free()) {}       
    }
    
    inline void showDMABuffer()
    {
      
        if (!m_cfg.double_buff) return;

        #if SERIAL_DEBUG     
                Serial.printf_P(PSTR("Showtime for buffer: %d\n"), back_buffer_id);
        #endif      
      
        i2s_parallel_flip_to_buffer(&I2S1, back_buffer_id);

        // Wait before we allow any writing to the buffer. Stop flicker.
        while(!i2s_parallel_is_previous_buffer_free()) {}               
    }
    
    inline void setPanelBrightness(int b)
    {
      // Change to set the brightness of the display, range of 1 to matrixWidth (i.e. 1 - 64)
        brightness = b;
        if (!initialized)
          return;

        brtCtrlOE(b);
        if (m_cfg.double_buff)
                brtCtrlOE(b, 1);
    }

    /**
     * this is just a wrapper to control brightness
     * with an 8-bit value (0-255), very popular in FastLED-based sketches :)
     * @param uint8_t b - 8-bit brightness value
     */
    void setBrightness8(const uint8_t b)
    {
      setPanelBrightness(b * PIXELS_PER_ROW / 256);
    }

    inline void setMinRefreshRate(int rr)
    {
        min_refresh_rate = rr;
    } 

  int  calculated_refresh_rate  = 0;         

   // ------- PRIVATE -------
  private:

    // Matrix i2s settings
    HUB75_I2S_CFG m_cfg;

    /* ESP32-HUB75-MatrixPanel-I2S-DMA functioning constants
     * we can't change those once object instance initialized it's DMA structs
     */
    const uint8_t rpf = m_cfg.mx_height / MATRIX_ROWS_IN_PARALLEL;   // RPF - rows per frame, either 16 or 32 depending on matrix module
    const uint16_t PIXELS_PER_ROW = m_cfg.mx_width * m_cfg.chain_length;   // number of pixels in a single row of all chained matrix modules (WIDTH of a combined matrix chain)

    // Other private variables
    bool initialized          = false;
    int  back_buffer_id       = 0;                       // If using double buffer, which one is NOT active (ie. being displayed) to write too?
    int  brightness           = 32;                      // If you get ghosting... reduce brightness level. 60 seems to be the limit before ghosting on a 64 pixel wide physical panel for some panels.
    int  min_refresh_rate     = 99;                      // Probably best to leave as is unless you want to experiment. Framerate has an impact on brightness and also power draw - voltage ripple.
    int  lsbMsbTransitionBit  = 0;                       // For possible color depth calculations

    // *** DMA FRAMEBUFFER structures

    // ESP 32 DMA Linked List descriptor
    int desccount = 0;
    lldesc_t * dmadesc_a = {0}; 
    lldesc_t * dmadesc_b = {0};

    /* Pixel data is organized from LSB to MSB sequentially by row, from row 0 to row matrixHeight/matrixRowsInParallel 
     * (two rows of pixels are refreshed in parallel) 
     * Memory is allocated (malloc'd) by the row, and not in one massive chunk, for flexibility.
     * The whole DMA framebuffer is just a vector of pointers to structs with ESP32_I2S_DMA_STORAGE_TYPE arrays
     * Since it's dimensions is unknown prior to class initialization, we just decrale it here as empty struct and will do all allocations later.
     * Refer to rowBitStruct to get the idea of it's internal structure
     */
    frameStruct dma_buff;


// ********** Private methods

    /* Calculate the memory available for DMA use, do some other stuff, and allocate accordingly */
    bool allocateDMAmemory();

    /* Setup the DMA Link List chain and initiate the ESP32 DMA engine */
    void configureDMA(const HUB75_I2S_CFG& opts);
   
    /* Update a specific pixel in the DMA buffer to a colour */
    void updateMatrixDMABuffer(int16_t x, int16_t y, uint8_t red, uint8_t green, uint8_t blue);
   
    /* Update the entire DMA buffer (aka. The RGB Panel) a certain colour (wipe the screen basically) */
    void updateMatrixDMABuffer(uint8_t red, uint8_t green, uint8_t blue);       

    /**
     * pre-init procedures for specific drivers
     * 
     */
    void shiftDriver(const HUB75_I2S_CFG& opts);

    /**
     * @brief - clears and reinitializes color/control data in DMA buffs
     * When allocated, DMA buffs might be dirtry, so we need to blank it and initialize ABCDE,LAT,OE control bits.
     * Those control bits are constants during the entire DMA sweep and never changed when updating just pixel color data
     * so we could set it once on DMA buffs initialization and forget. 
     * This effectively clears buffers to blank BLACK and makes it ready to display output.
     * (Brightness control via OE bit manipulation is another case)
     */
    void clearFrameBuffer(bool _buff_id = 0);


    void brtCtrlOE(const int brt, const bool _buff_id=0);

}; // end Class header

/***************************************************************************************/   
// https://stackoverflow.com/questions/5057021/why-are-c-inline-functions-in-the-header
/* 2. functions declared in the header must be marked inline because otherwise, every translation unit which includes the header will contain a definition of the function, and the linker will complain about multiple definitions (a violation of the One Definition Rule). The inline keyword suppresses this, allowing multiple translation units to contain (identical) definitions. */
inline void MatrixPanel_I2S_DMA::drawPixel(int16_t x, int16_t y, uint16_t color) // adafruit virtual void override
{
  drawPixelRGB565( x, y, color);
} 

inline void MatrixPanel_I2S_DMA::fillScreen(uint16_t color)  // adafruit virtual void override
{
  uint8_t r = ((((color >> 11) & 0x1F) * 527) + 23) >> 6;
  uint8_t g = ((((color >> 5) & 0x3F) * 259) + 33) >> 6;
  uint8_t b = (((color & 0x1F) * 527) + 23) >> 6;
  
  updateMatrixDMABuffer(r, g, b); // the RGB only (no pixel coordinate) version of 'updateMatrixDMABuffer'
} 

inline void MatrixPanel_I2S_DMA::fillScreenRGB888(uint8_t r, uint8_t g,uint8_t b)  // adafruit virtual void override
{
  updateMatrixDMABuffer(r, g, b);
} 

// For adafruit
inline void MatrixPanel_I2S_DMA::drawPixelRGB565(int16_t x, int16_t y, uint16_t color) 
{
  uint8_t r = ((((color >> 11) & 0x1F) * 527) + 23) >> 6;
  uint8_t g = ((((color >> 5) & 0x3F) * 259) + 33) >> 6;
  uint8_t b = (((color & 0x1F) * 527) + 23) >> 6;
  
  updateMatrixDMABuffer( x, y, r, g, b);
}

inline void MatrixPanel_I2S_DMA::drawPixelRGB888(int16_t x, int16_t y, uint8_t r, uint8_t g,uint8_t b) 
{
  updateMatrixDMABuffer( x, y, r, g, b);
}

inline void MatrixPanel_I2S_DMA::drawPixelRGB24(int16_t x, int16_t y, RGB24 color) 
{
  updateMatrixDMABuffer( x, y, color.red, color.green, color.blue);
}

// Pass 8-bit (each) R,G,B, get back 16-bit packed color
//https://github.com/squix78/ILI9341Buffer/blob/master/ILI9341_SPI.cpp
inline uint16_t MatrixPanel_I2S_DMA::color565(uint8_t r, uint8_t g, uint8_t b) {

/*
  Serial.printf("Got r value of %d\n", r);
  Serial.printf("Got g value of %d\n", g);
  Serial.printf("Got b value of %d\n", b);
  */
  
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// Promote 3/3/3 RGB to Adafruit_GFX 5/6/5 RRRrrGGGgggBBBbb
inline uint16_t MatrixPanel_I2S_DMA::color333(uint8_t r, uint8_t g, uint8_t b) { 

    return ((r & 0x7) << 13) | ((r & 0x6) << 10) | ((g & 0x7) << 8) | ((g & 0x7) << 5) | ((b & 0x7) << 2) | ((b & 0x6) >> 1);
    
}


inline void MatrixPanel_I2S_DMA::drawIcon (int *ico, int16_t x, int16_t y, int16_t cols, int16_t rows) {
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
  
  MatrixPanel_I2S_DMA matrix;

  matrix.drawIcon (half_sun, 0,0,10,5);
*/

  int i, j;
  for (i = 0; i < rows; i++) {
    for (j = 0; j < cols; j++) {
      drawPixelRGB565 (x + j, y + i, ico[i * cols + j]);
    }
  }  
}


#endif
