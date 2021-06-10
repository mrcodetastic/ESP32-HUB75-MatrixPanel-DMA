#ifndef _ESP32_RGB_64_32_MATRIX_PANEL_I2S_DMA
#define _ESP32_RGB_64_32_MATRIX_PANEL_I2S_DMA

/***************************************************************************************/
/* COMPILE-TIME OPTIONS - Provide as part of PlatformIO project build_flags.           */
/***************************************************************************************/
/* Enable serial debugging of the library, to see how memory is allocated etc. */
//#define SERIAL_DEBUG 1


/*
 * Do NOT build additional methods optimized for fast drawing,
 * i.e. Adafruits drawFastHLine, drawFastVLine, etc...
 */
//#define NO_FAST_FUNCTIONS

/* Use GFX_Root (https://github.com/mrfaptastic/GFX_Root) instead of Adafruit_GFX library.
 * > Removes Bus_IO & Wire.h library dependencies. 
 * > Provides 24bpp (CRGB) colour support for  Adafruit_GFX functions like drawCircle etc.
 * > Requires FastLED.h
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

// RGB Panel Constants / Calculated Values
#ifndef MATRIX_ROWS_IN_PARALLEL
 #define MATRIX_ROWS_IN_PARALLEL     2
#endif

// 8bit per RGB color = 24 bit/per pixel,
// might be reduced to save DMA RAM
#ifndef PIXEL_COLOR_DEPTH_BITS
 #define PIXEL_COLOR_DEPTH_BITS      8
#endif

#define COLOR_CHANNELS_PER_PIXEL     3


/***************************************************************************************/
/* Library Includes!                                                                   */
#include <vector>
#include <memory>
#include "esp_heap_caps.h"
#include "esp32_i2s_parallel_v2.h"

#ifdef USE_GFX_ROOT
	#include <FastLED.h>    
	#include "GFX.h" // Adafruit GFX core class -> https://github.com/mrfaptastic/GFX_Root	
#elif !defined NO_GFX
    #include "Adafruit_GFX.h" // Adafruit class with all the other stuff
#endif



/***************************************************************************************/
/* Definitions below should NOT be ever changed without rewriting library logic         */
#define ESP32_I2S_DMA_MODE          I2S_PARALLEL_WIDTH_16    // From esp32_i2s_parallel_v2.h = 16 bits in parallel
#define ESP32_I2S_DMA_STORAGE_TYPE  uint16_t                // DMA output of one uint16_t at a time.
#define CLKS_DURING_LATCH            0                      // Not (yet) used. 

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

// BitMasks are pre-computed based on the above #define's for performance.
#define BITMASK_RGB1_CLEAR   (0b1111111111111000)    // inverted bitmask for R1G1B1 bit in pixel vector
#define BITMASK_RGB2_CLEAR   (0b1111111111000111)    // inverted bitmask for R2G2B2 bit in pixel vector
#define BITMASK_RGB12_CLEAR  (0b1111111111000000)    // inverted bitmask for R1G1B1R2G2B2 bit in pixel vector
#define BITMASK_CTRL_CLEAR   (0b1110000000111111)    // inverted bitmask for control bits ABCDE,LAT,OE in pixel vector
#define BITMASK_OE_CLEAR     (0b1111111101111111)    // inverted bitmask for control bit OE in pixel vector

// How many clock cycles to blank OE before/after LAT signal change, default is 1 clock
#define DEFAULT_LAT_BLANKING  1
// Max clock cycles to blank OE before/after LAT signal change
#define MAX_LAT_BLANKING  4

/***************************************************************************************/
// Check compile-time only options
#if PIXEL_COLOR_DEPTH_BITS > 8
    #error "Pixel color depth bits cannot be greater than 8."
#elif PIXEL_COLOR_DEPTH_BITS < 2 
    #error "Pixel color depth bits cannot be less than 2."
#endif

/* This library is designed to take an 8 bit / 1 byt value (0-255) for each R G B colour sub-pixel. 
 * The PIXEL_COLOR_DEPTH_BITS should always be '8' as a result.
 * However, if the library is to be used with lower colour depth (i.e. 6 bit colour), then we need to ensure the 8-bit value passed to the colour masking
 * is adjusted accordingly to ensure the LSB's are shifted left to MSB, by the difference. Otherwise the colours will be all screwed up.
 */
#if PIXEL_COLOR_DEPTH_BITS != 8
static constexpr uint8_t const MASK_OFFSET = 8-PIXEL_COLOR_DEPTH_BITS;
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

    /** @brief - returns size of row of data vectorfor a SINGLE buff
     * size (in bytes) of a vector holding full DMA data for a row of pixels with _dpth color bits
     * a SINGLE buffer only size is accounted, when using double buffers it actually takes twice as much space
     * but returned size is for a half of double-buffer
     * 
     * default - returns full data vector size for a SINGLE buff
     *
     */
    size_t size(uint8_t _dpth=0 ) { if (!_dpth) _dpth = color_depth; return width * _dpth * sizeof(ESP32_I2S_DMA_STORAGE_TYPE); };

    /** @brief - returns pointer to the row's data vector begining at pixel[0] for _dpth color bit
     * default - returns pointer to the data vector's head
     * NOTE: this call might be very slow in loops. Due to poor instruction caching in esp32 it might be required a reread from flash 
     * every loop cycle, better use inlined #define instead in such cases
     */
    ESP32_I2S_DMA_STORAGE_TYPE* getDataPtr(const uint8_t _dpth=0, const bool buff_id=0) { return &(data[_dpth*width + buff_id*(width*color_depth)]); };

    // constructor - allocates DMA-capable memory to hold the struct data
    rowBitStruct(const size_t _width, const uint8_t _depth, const bool _dbuff) : width(_width), color_depth(_depth), double_buff(_dbuff) {
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

/***************************************************************************************/   
//C/p'ed from https://ledshield.wordpress.com/2012/11/13/led-brightness-to-your-eye-gamma-correction-no/
// Example calculator: https://gist.github.com/mathiasvr/19ce1d7b6caeab230934080ae1f1380e
// need to make sure this would end up in RAM for fastest access
#ifndef NO_CIE1931
static const uint8_t DRAM_ATTR lumConvTab[]={ 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 7, 7, 7, 7, 8, 8, 8, 8, 9, 9, 9, 10, 10, 10, 11, 11, 11, 12, 12, 12, 13, 13, 13, 14, 14, 14, 15, 15, 16, 16, 17, 17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 23, 24, 24, 25, 25, 26, 27, 27, 28, 28, 29, 30, 30, 31, 31, 32, 33, 33, 34, 35, 35, 36, 37, 38, 38, 39, 40, 41, 41, 42, 43, 44, 45, 45, 46, 47, 48, 49, 50, 51, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 73, 74, 75, 76, 77, 78, 80, 81, 82, 83, 84, 86, 87, 88, 90, 91, 92, 93, 95, 96, 98, 99, 100, 102, 103, 105, 106, 107, 109, 110, 112, 113, 115, 116, 118, 120, 121, 123, 124, 126, 128, 129, 131, 133, 134, 136, 138, 139, 141, 143, 145, 146, 148, 150, 152, 154, 156, 157, 159, 161, 163, 165, 167, 169, 171, 173, 175, 177, 179, 181, 183, 185, 187, 189, 192, 194, 196, 198, 200, 203, 205, 207, 209, 212, 214, 216, 218, 221, 223, 226, 228, 230, 233, 235, 238, 240, 243, 245, 248, 250, 253, 255, 255};
#endif

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
  enum shift_driver {SHIFTREG=0, FM6124, FM6126A, ICN2038S, MBI5124};

  /**
   * I2S clock speed selector
   */
  enum clk_speed {HZ_10M=10000000, HZ_20M=20000000};

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
  // How many clock cycles to blank OE before/after LAT signal change, default is 1 clock
  uint8_t latch_blanking;
  
  /**
   *  I2S clock phase
   *  0 - data lines are clocked with negative edge
   *  Clk  /¯\_/¯\_/
   *  LAT  __/¯¯¯\__
   *  EO   ¯¯¯¯¯¯\___
   *
   *  1 - data lines are clocked with positive edge (default now as of 10 June 2021)
   *  https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA/issues/130
   *  Clk  \_/¯\_/¯\
   *  LAT  __/¯¯¯\__
   *  EO   ¯¯¯¯¯¯\__
   *
   */
  bool clkphase;

  // Minimum refresh / scan rate needs to be configured on start due to LSBMSB_TRANSITION_BIT calculation in allocateDMAmemory()
  uint8_t min_refresh_rate;

  // struct constructor
  HUB75_I2S_CFG (
    uint16_t _w = MATRIX_WIDTH,
    uint16_t _h = MATRIX_HEIGHT,
    uint16_t _chain = CHAIN_LENGTH,
    i2s_pins _pinmap = {
      R1_PIN_DEFAULT, G1_PIN_DEFAULT, B1_PIN_DEFAULT, R2_PIN_DEFAULT, G2_PIN_DEFAULT, B2_PIN_DEFAULT,
      A_PIN_DEFAULT, B_PIN_DEFAULT, C_PIN_DEFAULT, D_PIN_DEFAULT, E_PIN_DEFAULT,
      LAT_PIN_DEFAULT, OE_PIN_DEFAULT, CLK_PIN_DEFAULT },
    shift_driver _drv = SHIFTREG,
    bool _dbuff = false,
    clk_speed _i2sspeed = HZ_10M,
    uint8_t _latblk = 1,
    bool _clockphase = true,
    uint8_t _min_refresh_rate = 85
  ) : mx_width(_w),
      mx_height(_h),
      chain_length(_chain),
      gpio(_pinmap),
      driver(_drv), i2sspeed(_i2sspeed),
      double_buff(_dbuff),
      latch_blanking(_latblk),
      clkphase(_clockphase),
      min_refresh_rate (_min_refresh_rate) {}
}; // end of structure HUB75_I2S_CFG



/***************************************************************************************/   
#ifdef USE_GFX_ROOT
class MatrixPanel_I2S_DMA : public GFX {
#elif !defined NO_GFX
class MatrixPanel_I2S_DMA : public Adafruit_GFX {
#else
class MatrixPanel_I2S_DMA {
#endif

  // ------- PUBLIC -------
  public:

    /**
     * MatrixPanel_I2S_DMA
     *
     * default predefined values are used for matrix configuraton
     *
     */
    MatrixPanel_I2S_DMA()
#ifdef USE_GFX_ROOT
      : GFX(MATRIX_WIDTH, MATRIX_HEIGHT)
#elif !defined NO_GFX
      : Adafruit_GFX(MATRIX_WIDTH, MATRIX_HEIGHT)
#endif
    {}

    /**
     * MatrixPanel_I2S_DMA 
     * 
     * @param  {HUB75_I2S_CFG} opts : structure with matrix configuration
     *        
     */
    MatrixPanel_I2S_DMA(const HUB75_I2S_CFG& opts) :
#ifdef USE_GFX_ROOT 
      GFX(opts.mx_width*opts.chain_length, opts.mx_height),
#elif !defined NO_GFX
      Adafruit_GFX(opts.mx_width*opts.chain_length, opts.mx_height),
#endif        
      m_cfg(opts) {}

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

      // initialize some specific panel drivers
      if (m_cfg.driver)
        shiftDriver(m_cfg);
          

     /* As DMA buffers are dynamically allocated, we must allocated in begin()
      * Ref: https://github.com/espressif/arduino-esp32/issues/831
      */
      if ( !allocateDMAmemory() ) {  return false; } // couldn't even get the basic ram required.
        

      // Flush the DMA buffers prior to configuring DMA - Avoid visual artefacts on boot.
      resetbuffers(); // Must fill the DMA buffer with the initial output bit sequence or the panel will display garbage

      // Setup the ESP32 DMA Engine. Sprite_TM built this stuff.
      configureDMA(m_cfg); //DMA and I2S configuration and setup

      showDMABuffer(); // show backbuf_id of 0

      #if SERIAL_DEBUG 
        if (!initialized)    
              Serial.println(F("MatrixPanel_I2S_DMA::begin() failed."));
      #endif      

      return initialized;

    }

    /*
     *  overload for compatibility
     */
    bool begin(int r1, int g1 = G1_PIN_DEFAULT, int b1 = B1_PIN_DEFAULT, int r2 = R2_PIN_DEFAULT, int g2 = G2_PIN_DEFAULT, int b2 = B2_PIN_DEFAULT, int a  = A_PIN_DEFAULT, int b = B_PIN_DEFAULT, int c = C_PIN_DEFAULT, int d = D_PIN_DEFAULT, int e = E_PIN_DEFAULT, int lat = LAT_PIN_DEFAULT, int oe = OE_PIN_DEFAULT, int clk = CLK_PIN_DEFAULT);

    // TODO: Disable/Enable auto buffer flipping (useful for lots of drawPixel usage)...

    // Adafruit's BASIC DRAW API (565 colour format)
    virtual void drawPixel(int16_t x, int16_t y, uint16_t color);   // overwrite adafruit implementation
    virtual void fillScreen(uint16_t color);                        // overwrite adafruit implementation

    /**
     * A wrapper to fill the entire Screen with black
     * if double buffering is used, than only back buffer is cleared
     */
    inline void clearScreen(){ updateMatrixDMABuffer(0,0,0); };

#ifndef NO_FAST_FUNCTIONS
    /**
     * @brief - override Adafruit's FastVLine
     * this works faster than multiple consecutive pixel by pixel drawPixel() call
     */
    virtual void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color){
      uint8_t r, g, b;
      color565to888(color, r, g, b);
      vlineDMA(x, y, h, r, g, b);
    }
    // rgb888 overload
    virtual inline void drawFastVLine(int16_t x, int16_t y, int16_t h, uint8_t r, uint8_t g, uint8_t b){ vlineDMA(x, y, h, r, g, b); };

    /**
     * @brief - override Adafruit's FastHLine
     * this works faster than multiple consecutive pixel by pixel drawPixel() call
     */
    virtual void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color){
      uint8_t r, g, b;
      color565to888(color, r, g, b);
      hlineDMA(x, y, w, r, g, b);
    }
    // rgb888 overload
    virtual inline void drawFastHLine(int16_t x, int16_t y, int16_t w, uint8_t r, uint8_t g, uint8_t b){ hlineDMA(x, y, w, r, g, b); };

    /**
     * @brief - override Adafruit's fillRect
     * this works much faster than mulltiple consecutive per-pixel drawPixel() calls
     */
    virtual void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color){
      uint8_t r, g, b;
      color565to888(color, r, g, b);
      fillRectDMA(x, y, w, h, r, g, b);
    }
    // rgb888 overload
    virtual inline void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t r, uint8_t g, uint8_t b){fillRectDMA(x, y, w, h, r, g, b);}
#endif

    void fillScreenRGB888(uint8_t r, uint8_t g, uint8_t b);
    void drawPixelRGB888(int16_t x, int16_t y, uint8_t r, uint8_t g, uint8_t b);
	
#ifdef USE_GFX_ROOT
	// 24bpp FASTLED CRGB colour struct support
	void fillScreen(CRGB color);
    void drawPixel(int16_t x, int16_t y, CRGB color);
#endif 

    void drawIcon (int *ico, int16_t x, int16_t y, int16_t cols, int16_t rows);
    
    // Color 444 is a 4 bit scale, so 0 to 15, color 565 takes a 0-255 bit value, so scale up by 255/15 (i.e. 17)!
    static uint16_t color444(uint8_t r, uint8_t g, uint8_t b) { return color565(r*17,g*17,b*17); }

    // Converts RGB888 to RGB565
    static uint16_t color565(uint8_t r, uint8_t g, uint8_t b); // This is what is used by Adafruit GFX!
    
    // Converts RGB333 to RGB565
    static uint16_t color333(uint8_t r, uint8_t g, uint8_t b); // This is what is used by Adafruit GFX! Not sure why they have a capital 'C' for this particular function.

    /**
     * @brief - convert RGB565 to RGB888
     * @param uint16_t color - RGB565 input color
     * @param uint8_t &r, &g, &b - refs to variables where converted colors would be emplaced
     */
    static void color565to888(const uint16_t color, uint8_t &r, uint8_t &g, uint8_t &b);


    inline void flipDMABuffer() 
    {         
      if ( !m_cfg.double_buff) return;
        
        // Flip to other buffer as the backbuffer. i.e. Graphic changes happen to this buffer (but aren't displayed until showDMABuffer())
        back_buffer_id ^= 1; 
        
        #if SERIAL_DEBUG     
                Serial.printf_P(PSTR("Set back buffer to: %d\n"), back_buffer_id);
        #endif      

        // Wait before we allow any writing to the buffer. Stop flicker.
        while(!i2s_parallel_is_previous_buffer_free()) { delay(1); }       
    }
    
    inline void showDMABuffer()
    {
      
        if (!m_cfg.double_buff) return;

        #if SERIAL_DEBUG     
                Serial.printf_P(PSTR("Showtime for buffer: %d\n"), back_buffer_id);
        #endif      
      
        i2s_parallel_flip_to_buffer(I2S_NUM_1, back_buffer_id);

        // Wait before we allow any writing to the buffer. Stop flicker.
        while(!i2s_parallel_is_previous_buffer_free()) { delay(1); }               
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

    /**
     * Contains the resulting refresh rate (scan rate) that will be achieved
     * based on the i2sspeed, colour depth and min_refresh_rate requested.
     */
    int calculated_refresh_rate  = 0;         

    /**
     * @brief - Sets how many clock cycles to blank OE before/after LAT signal change
     * @param uint8_t pulses - clocks before/after OE
     * default is DEFAULT_LAT_BLANKING
     * Max is MAX_LAT_BLANKING
     * @returns - new value for m_cfg.latch_blanking
     */
    uint8_t setLatBlanking(uint8_t pulses);

    /**
     * Get a class configuration struct
     * 
     */
    const HUB75_I2S_CFG& getCfg() const {return m_cfg;};
    
    
    /** 
     * Stop the ESP32 DMA Engine. Screen will forever be black until next ESP reboot.
     */
    void stopDMAoutput() {  
        resetbuffers();
        i2s_parallel_stop_dma(I2S_NUM_1);
    } 
    


  // ------- PROTECTED -------
  // those might be useful for child classes, like VirtualMatrixPanel
  protected:

    /**
     * @brief - clears and reinitializes color/control data in DMA buffs
     * When allocated, DMA buffs might be dirtry, so we need to blank it and initialize ABCDE,LAT,OE control bits.
     * Those control bits are constants during the entire DMA sweep and never changed when updating just pixel color data
     * so we could set it once on DMA buffs initialization and forget. 
     * This effectively clears buffers to blank BLACK and makes it ready to display output.
     * (Brightness control via OE bit manipulation is another case)
     */
    void clearFrameBuffer(bool _buff_id = 0);

    /* Update a specific pixel in the DMA buffer to a colour */
    void updateMatrixDMABuffer(int16_t x, int16_t y, uint8_t red, uint8_t green, uint8_t blue);
   
    /* Update the entire DMA buffer (aka. The RGB Panel) a certain colour (wipe the screen basically) */
    void updateMatrixDMABuffer(uint8_t red, uint8_t green, uint8_t blue);       

    /**
     * wipes DMA buffer(s) and reset all color/service bits
     */
    inline void resetbuffers(){
      clearFrameBuffer();
      brtCtrlOE(brightness);
      if (m_cfg.double_buff){
        clearFrameBuffer(1); 
        brtCtrlOE(brightness, 1);
      }
    }


#ifndef NO_FAST_FUNCTIONS
    /**
     * @brief - update DMA buff drawing horizontal line at specified coordinates
     * @param x_ccord - line start coordinate x
     * @param y_ccord - line start coordinate y
     * @param l - line length
     * @param r,g,b, - RGB888 color
     */
    void hlineDMA(int16_t x_coord, int16_t y_coord, int16_t l, uint8_t red, uint8_t green, uint8_t blue);

    /**
     * @brief - update DMA buff drawing horizontal line at specified coordinates
     * @param x_ccord - line start coordinate x
     * @param y_ccord - line start coordinate y
     * @param l - line length
     * @param r,g,b, - RGB888 color
     */
    void vlineDMA(int16_t x_coord, int16_t y_coord, int16_t l, uint8_t red, uint8_t green, uint8_t blue);

    /**
     * @brief - update DMA buff drawing a rectangular at specified coordinates
     * uses Fast H/V line draw internally, works faster than mulltiple consecutive pixel by pixel calls to updateMatrixDMABuffer()
     * @param int16_t x, int16_t y - coordinates of a top-left corner
     * @param int16_t w, int16_t h - width and height of a rectangular, min is 1 px
     * @param uint8_t r - RGB888 color
     * @param uint8_t g - RGB888 color
     * @param uint8_t b - RGB888 color
     */
    void fillRectDMA(int16_t x_coord, int16_t y_coord, int16_t w, int16_t h, uint8_t r, uint8_t g, uint8_t b);
#endif

   // ------- PRIVATE -------
  private:

    // Matrix i2s settings
    HUB75_I2S_CFG m_cfg;

    /* ESP32-HUB75-MatrixPanel-I2S-DMA functioning constants
     * we can't change those once object instance initialized it's DMA structs
     */
    const uint8_t   ROWS_PER_FRAME = m_cfg.mx_height / MATRIX_ROWS_IN_PARALLEL;   // RPF - rows per frame, either 16 or 32 depending on matrix module
    const uint16_t  PIXELS_PER_ROW = m_cfg.mx_width * m_cfg.chain_length;   // number of pixels in a single row of all chained matrix modules (WIDTH of a combined matrix chain)

    // Other private variables
    bool initialized          = false;
    int  back_buffer_id       = 0;                       // If using double buffer, which one is NOT active (ie. being displayed) to write too?
    int  brightness           = 32;                      // If you get ghosting... reduce brightness level. 60 seems to be the limit before ghosting on a 64 pixel wide physical panel for some panels.
    int  lsbMsbTransitionBit  = 0;                       // For colour depth calculations
    

    // *** DMA FRAMEBUFFER structures

    // ESP 32 DMA Linked List descriptor
    int desccount        = 0;
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


    /* Calculate the memory available for DMA use, do some other stuff, and allocate accordingly */
    bool allocateDMAmemory();

    /* Setup the DMA Link List chain and initiate the ESP32 DMA engine */
    void configureDMA(const HUB75_I2S_CFG& opts);

    /**
     * pre-init procedures for specific drivers
     * 
     */
    void shiftDriver(const HUB75_I2S_CFG& opts);

    /**
     * @brief - FM6124-family chips initialization routine
     */
    void fm6124init(const HUB75_I2S_CFG& _cfg);

    /**
     * @brief - reset OE bits in DMA buffer in a way to control brightness
     * @param brt - brightness level from 0 to row_width
     * @param _buff_id - buffer id to control
     */
    void brtCtrlOE(int brt, const bool _buff_id=0);


}; // end Class header

/***************************************************************************************/   
// https://stackoverflow.com/questions/5057021/why-are-c-inline-functions-in-the-header
/* 2. functions declared in the header must be marked inline because otherwise, every translation unit which includes the header will contain a definition of the function, and the linker will complain about multiple definitions (a violation of the One Definition Rule). The inline keyword suppresses this, allowing multiple translation units to contain (identical) definitions. */

/**
 * @brief - convert RGB565 to RGB888
 * @param uint16_t color - RGB565 input color
 * @param uint8_t &r, &g, &b - refs to variables where converted colours would be emplaced
 */
inline void MatrixPanel_I2S_DMA::color565to888(const uint16_t color, uint8_t &r, uint8_t &g, uint8_t &b){
  r = ((((color >> 11) & 0x1F) * 527) + 23) >> 6;
  g = ((((color >> 5) & 0x3F) * 259) + 33) >> 6;
  b = (((color & 0x1F) * 527) + 23) >> 6;
}

inline void MatrixPanel_I2S_DMA::drawPixel(int16_t x, int16_t y, uint16_t color) // adafruit virtual void override
{
  uint8_t r,g,b;
  color565to888(color,r,g,b);
  
  updateMatrixDMABuffer( x, y, r, g, b);
} 

inline void MatrixPanel_I2S_DMA::fillScreen(uint16_t color)  // adafruit virtual void override
{
  uint8_t r,g,b;
  color565to888(color,r,g,b);
  
  updateMatrixDMABuffer(r, g, b); // RGB only (no pixel coordinate) version of 'updateMatrixDMABuffer'
} 

inline void MatrixPanel_I2S_DMA::drawPixelRGB888(int16_t x, int16_t y, uint8_t r, uint8_t g,uint8_t b) 
{
  updateMatrixDMABuffer( x, y, r, g, b);
}

inline void MatrixPanel_I2S_DMA::fillScreenRGB888(uint8_t r, uint8_t g,uint8_t b)
{
  updateMatrixDMABuffer(r, g, b); // RGB only (no pixel coordinate) version of 'updateMatrixDMABuffer'
} 

#ifdef USE_GFX_ROOT
// Support for CRGB values provided via FastLED
inline void MatrixPanel_I2S_DMA::drawPixel(int16_t x, int16_t y, CRGB color) 
{
  updateMatrixDMABuffer( x, y, color.red, color.green, color.blue);
}

inline void MatrixPanel_I2S_DMA::fillScreen(CRGB color) 
{
  updateMatrixDMABuffer(color.red, color.green, color.blue);
}
#endif


// Pass 8-bit (each) R,G,B, get back 16-bit packed color
//https://github.com/squix78/ILI9341Buffer/blob/master/ILI9341_SPI.cpp
inline uint16_t MatrixPanel_I2S_DMA::color565(uint8_t r, uint8_t g, uint8_t b) {
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
      drawPixel (x + j, y + i, (uint16_t) ico[i * cols + j]);
    }
  }  
}


#endif
