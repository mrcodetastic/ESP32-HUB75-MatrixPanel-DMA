#ifndef _ESP32_RGB_64_32_MATRIX_PANEL_I2S_DMA
#define _ESP32_RGB_64_32_MATRIX_PANEL_I2S_DMA
/***************************************************************************************/
/* Core ESP32 hardware / idf includes!                                                 */
#include <vector>
#include <memory>
#include <esp_err.h>
#include <esp_log.h>
#include "esp_attr.h"
#include "esp_heap_caps.h"

// #include <Arduino.h>
#include "platforms/platform_detect.hpp"

#ifdef USE_GFX_LITE
  // Slimmed version of Adafruit GFX + FastLED: https://github.com/mrcodetastic/GFX_Lite
  #include "GFX_Lite.h" 
#elif !defined NO_GFX
  #include "Adafruit_GFX.h" // Adafruit class with all the other stuff
#endif

/*******************************************************************************************
 * COMPILE-TIME OPTIONS - MUST BE PROVIDED as part of PlatformIO project build_flags.      *
 * Changing the values just here won't work - as defines needs to persist beyond the scope *
 * of just this file.                                                                      *
 *******************************************************************************************/
/* Do NOT build additional methods optimized for fast drawing,
 * i.e. Adafruits drawFastHLine, drawFastVLine, etc...                         */
// #define NO_FAST_FUNCTIONS

// #define NO_CIE1931

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
#define MATRIX_WIDTH 64 // Single panel of 64 pixel width
#endif

#ifndef MATRIX_HEIGHT
#define MATRIX_HEIGHT 32 // CHANGE THIS VALUE to 64 IF USING 64px HIGH panel(s) with E PIN
#endif

#ifndef CHAIN_LENGTH
#define CHAIN_LENGTH 1 // Number of modules chained together, i.e. 4 panels chained result in virtualmatrix 64x4=256 px long
#endif

// Interesting Fact: We end up using a uint16_t to send data in parallel to the HUB75... but
//                   given we only map to 14 physical output wires/bits, we waste 2 bits.

/***************************************************************************************/
/* Do not change definitions below unless you pretty sure you know what you are doing! */

// keeping a check sine it was possibe to set it previously
#ifdef MATRIX_ROWS_IN_PARALLEL
#pragma message "You are not supposed to set MATRIX_ROWS_IN_PARALLEL. Setting it back to default."
#undef MATRIX_ROWS_IN_PARALLEL
#endif
#define MATRIX_ROWS_IN_PARALLEL 2

// 8bit per RGB color = 24 bit/per pixel,
// can be extended to offer deeper colors, or
// might be reduced to save DMA RAM
#ifdef PIXEL_COLOUR_DEPTH_BITS
#define PIXEL_COLOR_DEPTH_BITS PIXEL_COLOUR_DEPTH_BITS
#endif

// support backwarts compatibility
#ifdef PIXEL_COLOR_DEPTH_BITS
#define PIXEL_COLOR_DEPTH_BITS_DEFAULT PIXEL_COLOR_DEPTH_BITS
#else
#define PIXEL_COLOR_DEPTH_BITS_DEFAULT 8
#endif

#define PIXEL_COLOR_DEPTH_BITS_MAX 12

/***************************************************************************************/
/* Definitions below should NOT be ever changed without rewriting library logic         */
#define ESP32_I2S_DMA_STORAGE_TYPE uint16_t // DMA output of one uint16_t at a time.
#define CLKS_DURING_LATCH 0                 // Not (yet) used.

// Panel Upper half RGB (numbering according to order in DMA gpio_bus configuration)
#define BITS_RGB1_OFFSET 0 // Start point of RGB_X1 bits
#define BIT_R1 (1 << 0)
#define BIT_G1 (1 << 1)
#define BIT_B1 (1 << 2)

// Panel Lower half RGB
#define BITS_RGB2_OFFSET 3 // Start point of RGB_X2 bits
#define BIT_R2 (1 << 3)
#define BIT_G2 (1 << 4)
#define BIT_B2 (1 << 5)

// Panel Control Signals
#define BIT_LAT (1 << 6)
#define BIT_OE (1 << 7)

// Panel GPIO Pin Addresses (A, B, C, D etc..)
#define BITS_ADDR_OFFSET 8 // Start point of address bits
#define BIT_A (1 << 8)
#define BIT_B (1 << 9)
#define BIT_C (1 << 10)
#define BIT_D (1 << 11)
#define BIT_E (1 << 12)

// BitMasks are pre-computed based on the above #define's for performance.
#define BITMASK_RGB1_CLEAR (0b1111111111111000)  // inverted bitmask for R1G1B1 bit in pixel vector
#define BITMASK_RGB2_CLEAR (0b1111111111000111)  // inverted bitmask for R2G2B2 bit in pixel vector
#define BITMASK_RGB12_CLEAR (0b1111111111000000) // inverted bitmask for R1G1B1R2G2B2 bit in pixel vector
#define BITMASK_CTRL_CLEAR (0b1110000000111111)  // inverted bitmask for control bits ABCDE,LAT,OE in pixel vector
#define BITMASK_OE_CLEAR (0b1111111101111111)    // inverted bitmask for control bit OE in pixel vector

// How many clock cycles to blank OE before/after LAT signal change, default is 2 clocks
#define DEFAULT_LAT_BLANKING 2

// Max clock cycles to blank OE before/after LAT signal change
#define MAX_LAT_BLANKING 4

/***************************************************************************************/

/** @brief - Structure holds raw DMA data to drive TWO full rows of pixels spanning through all chained modules
 * Note: sizeof(data) must be multiple of 32 bits, as ESP32 DMA linked list buffer address pointer must be word-aligned
 */
struct rowBitStruct
/**
 * @struct rowBitStruct
 * @brief Structure to hold row data for HUB75 matrix panel DMA operations
 * 
 * @var width
 * Width of the row in pixels
 * 
 * @var colour_depth
 * Number of color depths (i.e. copies of each row for each colur bitmask)
 * 
 * @var data
 * Pointer to DMA storage type array holding pixel data for the row
 * 
 * @note Memory allocation differs based on target platform and configuration:
 * - For ESP32-S3 with SPIRAM: Allocates aligned memory in SPIRAM
 * - For other configurations: Allocates DMA-capable internal memory
 */
{
  const size_t width;
  const uint8_t colour_depth;
  //const bool double_buff;
  ESP32_I2S_DMA_STORAGE_TYPE *data;

  /** @brief Returns size (in bytes) of a colour depth row data array
   * 
   * @param single_color_depth 
   *        - if true, returns size for a single color depth layer
   *        - if false, returns total size for all color depth layers for a row.
   * 
   * @returns size_t - Size in bytes required for DMA buffer allocation
   * 
   */
  size_t getColorDepthSize(bool single_color_depth)
  {
    int _cdepth = (single_color_depth) ? 1:colour_depth;
    return width * _cdepth * sizeof(ESP32_I2S_DMA_STORAGE_TYPE);
  };


  /** @brief
   * Returns pointer to the row's data vector beginning at pixel[0] for _dpth colour bit
   * 
   * NOTE: this call might be very slow in loops. Due to poor instruction caching in esp32 it might be required a reread from flash
   * every loop cycle, better use inlined #define instead in such cases
   */
  inline ESP32_I2S_DMA_STORAGE_TYPE *getDataPtr(const uint8_t _dpth = 0) { return &(data[_dpth * width]); };

  // constructor - allocates DMA-capable memory to hold the struct data
  //rowBitStruct(const size_t _width, const uint8_t _depth, const bool _dbuff) : width(_width), colour_depth(_depth), double_buff(_dbuff)
  rowBitStruct(const size_t _width, const uint8_t _depth) : width(_width), colour_depth(_depth)
  {

    // #if defined(SPIRAM_FRAMEBUFFER) && defined (CONFIG_IDF_TARGET_ESP32S3)
#if defined(SPIRAM_DMA_BUFFER)

    // data = (ESP32_I2S_DMA_STORAGE_TYPE *)heap_caps_aligned_alloc(64, size()+size()*double_buff, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    // No longer have double buffer in the same struct - have a different struct
    data = (ESP32_I2S_DMA_STORAGE_TYPE *)heap_caps_aligned_alloc(64, getColorDepthSize(false), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
    // data = (ESP32_I2S_DMA_STORAGE_TYPE *)heap_caps_malloc( size()+size()*double_buff, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);

    // No longer have double buffer in the same struct - have a different struct
    data = (ESP32_I2S_DMA_STORAGE_TYPE *)heap_caps_malloc(getColorDepthSize(false), MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);

#endif
  }
  ~rowBitStruct() { delete data; }
};

/* frameStruct
 * Note: A 'frameStruct' contains ALL the data for a full-frame (i.e. BOTH 2x16-row frames are
 *       are contained in parallel within the one uint16_t that is sent in parallel to the HUB75).
 *
 *       This structure isn't actually allocated in one memory block anymore, as the library now allocates
 *       memory per row (per rowBits) instead.
 */
struct frameStruct
{
  uint8_t rows = 0; // number of rows held in current frame, not used actually, just to keep the idea of struct
  std::vector<std::shared_ptr<rowBitStruct>> rowBits;
};

/***************************************************************************************/
#ifndef NO_CIE1931
// C/p'ed from https://ledshield.wordpress.com/2012/11/13/led-brightness-to-your-eye-gamma-correction-no/
// 16 bit resolution of https://gist.github.com/mathiasvr/19ce1d7b6caeab230934080ae1f1380e
static const uint16_t lumConvTab[] = {
0,   28,   57,   85,  114,  142,  171,  199,  228,  256,  285,  313,  342,  370,  399,  427,
456,  484,  513,  541,  570,  598,  627,  658,  689,  721,  755,  789,  825,  861,  899,  937,
977, 1018, 1060, 1103, 1147, 1192, 1239, 1287, 1336, 1386, 1437, 1490, 1544, 1599, 1656, 1714,
1773, 1834, 1896, 1959, 2024, 2090, 2157, 2226, 2297, 2369, 2442, 2517, 2593, 2671, 2751, 2832,
2914, 2999, 3085, 3172, 3261, 3352, 3444, 3538, 3634, 3732, 3831, 3932, 4035, 4139, 4245, 4354,
4464, 4575, 4689, 4804, 4922, 5041, 5162, 5285, 5410, 5537, 5666, 5797, 5930, 6065, 6202, 6341,
6482, 6626, 6771, 6918, 7068, 7220, 7373, 7529, 7687, 7848, 8010, 8175, 8342, 8512, 8683, 8857,
9033, 9212, 9393, 9576, 9762, 9949, 10140, 10333, 10528, 10725, 10926, 11128, 11333, 11541, 11751, 11963,
12179, 12396, 12617, 12840, 13065, 13293, 13524, 13757, 13993, 14232, 14474, 14718, 14965, 15215, 15467, 15722,
15980, 16241, 16505, 16771, 17041, 17313, 17588, 17866, 18147, 18431, 18717, 19007, 19300, 19596, 19894, 20196,
20501, 20809, 21119, 21433, 21750, 22071, 22394, 22720, 23050, 23383, 23719, 24058, 24400, 24746, 25095, 25447,
25802, 26161, 26523, 26888, 27257, 27629, 28004, 28383, 28765, 29151, 29540, 29932, 30328, 30728, 31131, 31537,
31947, 32360, 32777, 33198, 33622, 34050, 34481, 34916, 35355, 35797, 36243, 36693, 37146, 37603, 38064, 38529,
38997, 39469, 39945, 40425, 40908, 41396, 41887, 42382, 42881, 43384, 43891, 44401, 44916, 45435, 45957, 46484,
47015, 47549, 48088, 48631, 49178, 49728, 50283, 50843, 51406, 51973, 52545, 53120, 53700, 54284, 54873, 55465,
56062, 56663, 57269, 57878, 58492, 59111, 59733, 60360, 60992, 61627, 62268, 62912, 63561, 64215, 64873, 65535,
};
#endif

/** @brief - configuration values for HUB75_I2S driver
 *  This structure holds configuration vars that are used as
 *  an initialization values when creating an instance of MatrixPanel_I2S_DMA object.
 *  All params have it's default values.
 */
struct HUB75_I2S_CFG
{

  /**
   * Enumeration of hardware-specific chips
   * used to drive matrix modules
   */
  enum shift_driver
  {
    SHIFTREG = 0,
    FM6124,
    FM6126A,
    ICN2038S,
    MBI5124,
    SM5266P,
    DP3246_SM5368
  };

  /**
   * I2S clock speed selector
   */
  enum clk_speed
  {
    HZ_8M = 8000000,
    HZ_10M = 8000000,
    HZ_15M = 16000000, // for compatability
    HZ_16M = 16000000,
    HZ_20M = 20000000 // for compatability  
  };

  //
  // Members must be in order of declaration or it breaks Arduino compiling due to strict checking.
  //

  // physical width of a single matrix panel module (in pixels, usually it is 64 ;) )
  uint16_t mx_width;

  // physical height of a single matrix panel module (in pixels, usually almost always it is either 32 or 64)
  uint16_t mx_height;

  // number of chained panels regardless of the topology, default 1 - a single matrix module
  uint16_t chain_length;

  // GPIO Mapping
  struct i2s_pins
  {
    int8_t r1, g1, b1, r2, g2, b2, a, b, c, d, e, lat, oe, clk;
  } gpio;

  // Matrix driver chip type - default is a plain shift register
  shift_driver driver;

  // use DMA double buffer (twice as much RAM required)
  bool double_buff;

  // I2S clock speed
  clk_speed i2sspeed;

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
  // Set this to '1' to get all colour depths displayed with correct BCM time weighting.
  uint8_t min_refresh_rate;

  // struct constructor
  HUB75_I2S_CFG(
      uint16_t _w = MATRIX_WIDTH,
      uint16_t _h = MATRIX_HEIGHT,
      uint16_t _chain = CHAIN_LENGTH,
      i2s_pins _pinmap = {
          R1_PIN_DEFAULT, G1_PIN_DEFAULT, B1_PIN_DEFAULT, R2_PIN_DEFAULT, G2_PIN_DEFAULT, B2_PIN_DEFAULT,
          A_PIN_DEFAULT, B_PIN_DEFAULT, C_PIN_DEFAULT, D_PIN_DEFAULT, E_PIN_DEFAULT,
          LAT_PIN_DEFAULT, OE_PIN_DEFAULT, CLK_PIN_DEFAULT},
      shift_driver _drv = SHIFTREG, 
      bool _dbuff = false, 
      clk_speed _i2sspeed = HZ_8M,
      uint8_t _latblk = DEFAULT_LAT_BLANKING, // Anything > 1 seems to cause artefacts on ICS panels
      bool _clockphase = true, 
      uint16_t _min_refresh_rate = 60, 
      uint8_t _pixel_color_depth_bits = PIXEL_COLOR_DEPTH_BITS_DEFAULT) 
      : mx_width(_w), mx_height(_h), chain_length(_chain), gpio(_pinmap), driver(_drv), double_buff(_dbuff), i2sspeed(_i2sspeed), latch_blanking(_latblk), clkphase(_clockphase), min_refresh_rate(_min_refresh_rate)
  {
    setPixelColorDepthBits(_pixel_color_depth_bits);
  }

  // pixel_color_depth_bits must be between 12 and 2, and mask_offset needs to be calculated accordently
  // so they have to be private with getter (and setter)
  void setPixelColorDepthBits(uint8_t _pixel_color_depth_bits)
  {
    if (_pixel_color_depth_bits > PIXEL_COLOR_DEPTH_BITS_MAX || _pixel_color_depth_bits < 2)
    {

      if (_pixel_color_depth_bits > PIXEL_COLOR_DEPTH_BITS_MAX)
      {
        pixel_color_depth_bits = PIXEL_COLOR_DEPTH_BITS_MAX;
      }
      else
      {
        pixel_color_depth_bits = 2;
      }
      ESP_LOGW("HUB75_I2S_CFG", "Invalid pixel_color_depth_bits (%d): 2 <= pixel_color_depth_bits <= %d, choosing nearest valid %d", _pixel_color_depth_bits, PIXEL_COLOR_DEPTH_BITS_MAX, pixel_color_depth_bits);
    }
    else
    {
      pixel_color_depth_bits = _pixel_color_depth_bits;
    }
  }

  uint8_t getPixelColorDepthBits() const
  {
    return pixel_color_depth_bits;
  }

private:
  // these were priviously handeld as defines (PIXEL_COLOR_DEPTH_BITS, MASK_OFFSET)
  // to make it changable after compilation, it is now part of the config
  uint8_t pixel_color_depth_bits;
}; // end of structure HUB75_I2S_CFG

/***************************************************************************************/
#ifdef USE_GFX_LITE
// Slimmed version of Adafruit GFX + FastLED: https://github.com/mrcodetastic/GFX_Lite
class MatrixPanel_I2S_DMA : public GFX
{
#elif !defined NO_GFX
class MatrixPanel_I2S_DMA : public Adafruit_GFX
{
#else
class MatrixPanel_I2S_DMA
{
#endif

  // ------- PUBLIC -------
public:
  /**
   * MatrixPanel_I2S_DMA
   *
   * default predefined values are used for matrix configuration
   *
   */
  MatrixPanel_I2S_DMA()
#ifdef USE_GFX_LITE
      : GFX(MATRIX_WIDTH, MATRIX_HEIGHT)
#elif !defined NO_GFX
      : Adafruit_GFX(MATRIX_WIDTH, MATRIX_HEIGHT)
#endif
  {
  }

  /**
   * MatrixPanel_I2S_DMA
   *
   * @param  {HUB75_I2S_CFG} opts : structure with matrix configuration
   *
   */
  MatrixPanel_I2S_DMA(const HUB75_I2S_CFG &opts)
#ifdef USE_GFX_LITE
      : GFX(opts.mx_width * opts.chain_length, opts.mx_height)
#elif !defined NO_GFX
      : Adafruit_GFX(opts.mx_width * opts.chain_length, opts.mx_height)
#endif
  {
    setCfg(opts);
  }

  /* Propagate the DMA pin configuration, allocate DMA buffs and start data output, initially blank */
  bool begin()
  {

    if (initialized)
      return true; // we don't do this twice or more!
      
    if (!config_set)
      return false;

    ESP_LOGI("begin()", "Using GPIO %d for R1_PIN", m_cfg.gpio.r1);
    ESP_LOGI("begin()", "Using GPIO %d for G1_PIN", m_cfg.gpio.g1);
    ESP_LOGI("begin()", "Using GPIO %d for B1_PIN", m_cfg.gpio.b1);
    ESP_LOGI("begin()", "Using GPIO %d for R2_PIN", m_cfg.gpio.r2);
    ESP_LOGI("begin()", "Using GPIO %d for G2_PIN", m_cfg.gpio.g2);
    ESP_LOGI("begin()", "Using GPIO %d for B2_PIN", m_cfg.gpio.b2);
    ESP_LOGI("begin()", "Using GPIO %d for A_PIN", m_cfg.gpio.a);
    ESP_LOGI("begin()", "Using GPIO %d for B_PIN", m_cfg.gpio.b);
    ESP_LOGI("begin()", "Using GPIO %d for C_PIN", m_cfg.gpio.c);
    ESP_LOGI("begin()", "Using GPIO %d for D_PIN", m_cfg.gpio.d);
    ESP_LOGI("begin()", "Using GPIO %d for E_PIN", m_cfg.gpio.e);
    ESP_LOGI("begin()", "Using GPIO %d for LAT_PIN", m_cfg.gpio.lat);
    ESP_LOGI("begin()", "Using GPIO %d for OE_PIN", m_cfg.gpio.oe);
    ESP_LOGI("begin()", "Using GPIO %d for CLK_PIN", m_cfg.gpio.clk);

    // initialize some specific panel drivers
    if (m_cfg.driver)
      shiftDriver(m_cfg);

#if defined(SPIRAM_DMA_BUFFER)
    // Trick library into dropping colour depth slightly when using PSRAM.
    m_cfg.i2sspeed = HUB75_I2S_CFG::HZ_8M;
#endif
	
    ESP_LOGI("begin()", "HUB75 effective display resolution of width: %dpx height: %dpx.", m_cfg.mx_width * m_cfg.chain_length, m_cfg.mx_height);
	
	if (m_cfg.mx_height % 2 != 0) {
		ESP_LOGE("begin()", "Error: m_cfg.mx_height must be an even number!");
		return false;
	}

    /* As DMA buffers are dynamically allocated, we must allocated in begin()
     * Ref: https://github.com/espressif/arduino-esp32/issues/831
     */
    if (!setupDMA(m_cfg))
    {
      return false;
    } // couldn't even get the basic ram required.
	
    if (!initialized)
    {
      ESP_LOGE("being()", "MatrixPanel_I2S_DMA::begin() failed!");
    }

    // Flush the DMA buffers prior to configuring DMA - Avoid visual artefacts on boot.
    resetbuffers(); // Must fill the DMA buffer with the initial output bit sequence or the panel will display garbage
    ESP_LOGV("being()", "Completed resetbuffers()");	

	flipDMABuffer(); // display back buffer 0, draw to 1, ignored if double buffering isn't enabled.		
    ESP_LOGV("being()", "Completed flipDMABuffer()");		

	// Start output output
	dma_bus.init();
    ESP_LOGV("being()", "Completed dma_bus.init()");	
	
	dma_bus.dma_transfer_start();
    ESP_LOGV("being()", "Completed dma_bus.dma_transfer_start()");		

    return initialized;
  }

  // Obj destructor
  virtual ~MatrixPanel_I2S_DMA()
  {
    dma_bus.release();
  }

  /*
   *  overload for compatibility
   */
  bool begin(int r1, int g1 = G1_PIN_DEFAULT, int b1 = B1_PIN_DEFAULT, int r2 = R2_PIN_DEFAULT, int g2 = G2_PIN_DEFAULT, int b2 = B2_PIN_DEFAULT, int a = A_PIN_DEFAULT, int b = B_PIN_DEFAULT, int c = C_PIN_DEFAULT, int d = D_PIN_DEFAULT, int e = E_PIN_DEFAULT, int lat = LAT_PIN_DEFAULT, int oe = OE_PIN_DEFAULT, int clk = CLK_PIN_DEFAULT);
  bool begin(const HUB75_I2S_CFG &cfg);

  // Adafruit's BASIC DRAW API (565 colour format)
  virtual void drawPixel(int16_t x, int16_t y, uint16_t color); // overwrite adafruit implementation
  virtual void fillScreen(uint16_t color);                      // overwrite adafruit implementation

  /**
   * A wrapper to fill whatever selected DMA buffer / screen with black
   */
  inline void clearScreen() { updateMatrixDMABuffer(0, 0, 0); };

#ifndef NO_FAST_FUNCTIONS
  /**
   * @brief - override Adafruit's FastVLine
   * this works faster than multiple consecutive pixel by pixel drawPixel() call
   */
  virtual void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color)
  {
    uint8_t r, g, b;
    color565to888(color, r, g, b);
    
    int16_t w = 1;
    transform(x, y, w, h);
    if (h > w)
      vlineDMA(x, y, h, r, g, b);
    else
      hlineDMA(x, y, w, r, g, b);

  }
  // rgb888 overload
  virtual inline void drawFastVLine(int16_t x, int16_t y, int16_t h, uint8_t r, uint8_t g, uint8_t b)
  {
    int16_t w = 1;
    transform(x, y, w, h);
    if (h > w)
      vlineDMA(x, y, h, r, g, b);
    else
      hlineDMA(x, y, w, r, g, b);
  };

  /**
   * @brief - override Adafruit's FastHLine
   * this works faster than multiple consecutive pixel by pixel drawPixel() call
   */
  virtual void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color)
  {
    uint8_t r, g, b;
    color565to888(color, r, g, b);

    int16_t h = 1;
    transform(x, y, w, h);
    if (h > w)
      vlineDMA(x, y, h, r, g, b);
    else
      hlineDMA(x, y, w, r, g, b);

  }
  // rgb888 overload
  virtual inline void drawFastHLine(int16_t x, int16_t y, int16_t w, uint8_t r, uint8_t g, uint8_t b)
  {
    int16_t h = 1;
    transform(x, y, w, h);
    if (h > w)
      vlineDMA(x, y, h, r, g, b);
    else
      hlineDMA(x, y, w, r, g, b);
  };

  /**
   * @brief - override Adafruit's fillRect
   * this works much faster than multiple consecutive per-pixel drawPixel() calls
   */
  virtual void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
  {
    uint8_t r, g, b;
    color565to888(color, r, g, b);
    
    transform(x, y, w, h);
    fillRectDMA(x, y, w, h, r, g, b);
    
  }
  // rgb888 overload
  virtual inline void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t r, uint8_t g, uint8_t b)
  {
    
    transform(x, y, w, h);
    fillRectDMA(x, y, w, h, r, g, b);
    
  }
#endif

  void fillScreenRGB888(uint8_t r, uint8_t g, uint8_t b);
  void drawPixelRGB888(int16_t x, int16_t y, uint8_t r, uint8_t g, uint8_t b);

#ifdef USE_GFX_LITE
  // 24bpp FASTLED CRGB colour struct support
  void fillScreen(CRGB color);
  void drawPixel(int16_t x, int16_t y, CRGB color);
#endif

#ifdef NO_GFX
    inline int16_t width() const { return m_cfg.mx_width * m_cfg.chain_length; }
    inline int16_t height() const { return m_cfg.mx_height; }
#endif

  void drawIcon(int *ico, int16_t x, int16_t y, int16_t cols, int16_t rows);

  // Colour 444 is a 4 bit scale, so 0 to 15, colour 565 takes a 0-255 bit value, so scale up by 255/15 (i.e. 17)!
  static uint16_t color444(uint8_t r, uint8_t g, uint8_t b) { return color565(r * 17, g * 17, b * 17); }
  static uint16_t color565(uint8_t r, uint8_t g, uint8_t b); // This is what is used by Adafruit GFX!

  /**
   * @brief - convert RGB565 to RGB888
   * @param uint16_t colour - RGB565 input colour
   * @param uint8_t &r, &g, &b - refs to variables where converted colors would be emplaced
   */
  static void color565to888(const uint16_t color, uint8_t &r, uint8_t &g, uint8_t &b);

  inline void flipDMABuffer()
  {
    if (!m_cfg.double_buff)
    {
      return;
    }
	
    dma_bus.flip_dma_output_buffer(back_buffer_id);
	
	//back_buffer_id ^= 1;
	back_buffer_id = back_buffer_id^1;
    fb = &frame_buffer[back_buffer_id];	
	

	
  }

  /**
   * @param uint8_t b - 8-bit brightness value
   */
  void setBrightness(const uint8_t b)
  {
    if (!initialized)
    {
      ESP_LOGI("setBrightness()", "Tried to set output brightness before begin()");
      return;
    }

    brightness = b;
    setBrightnessOE(b, 0);

    if (m_cfg.double_buff)
    {
      setBrightnessOE(b, 1);
    }
  }

  /**
   * @param uint8_t b - 8-bit brightness value
   */
  void setPanelBrightness(const uint8_t b)
  {
    setBrightness(b);
  }

  /**
   * this is just a wrapper to control brightness
   * with an 8-bit value (0-255), very popular in FastLED-based sketches :)
   * @param uint8_t b - 8-bit brightness value
   */
  void setBrightness8(const uint8_t b)
  {
    setBrightness(b);
    // setPanelBrightness(b * PIXELS_PER_ROW / 256);
  }

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
  const HUB75_I2S_CFG &getCfg() const { return m_cfg; };

  inline bool setCfg(const HUB75_I2S_CFG &cfg)
  {
    if (initialized)
      return false;

    m_cfg = cfg;
    PIXELS_PER_ROW = m_cfg.mx_width * m_cfg.chain_length;
    ROWS_PER_FRAME = m_cfg.mx_height / MATRIX_ROWS_IN_PARALLEL;
    MASK_OFFSET = 16 - m_cfg.getPixelColorDepthBits();

    config_set = true;
    return true;
  }

  /**
   * Stop the ESP32 DMA Engine. Screen will forever be black until next ESP reboot.
   */
  void stopDMAoutput()
  {
    resetbuffers();
    // i2s_parallel_stop_dma(ESP32_I2S_DEVICE);
    dma_bus.dma_transfer_stop();
  }

  // ------- PROTECTED -------
  // those might be useful for child classes, like VirtualMatrixPanel
protected:
  /**
   * @brief - clears and reinitializes colour/control data in DMA buffs
   * When allocated, DMA buffs might be dirty, so we need to blank it and initialize ABCDE,LAT,OE control bits.
   * Those control bits are constants during the entire DMA sweep and never changed when updating just pixel colour data
   * so we could set it once on DMA buffs initialization and forget.
   * This effectively clears buffers to blank BLACK and makes it ready to display output.
   * (Brightness control via OE bit manipulation is another case)
   */
  void clearFrameBuffer(bool _buff_id);

  /* Update a specific pixel in the DMA buffer to a colour */
  void updateMatrixDMABuffer(uint16_t x, uint16_t y, uint8_t red, uint8_t green, uint8_t blue);

  /* Update the entire DMA buffer (aka. The RGB Panel) a certain colour (wipe the screen basically) */
  void updateMatrixDMABuffer(uint8_t red, uint8_t green, uint8_t blue);

  /**
   * wipes DMA buffer(s) and reset all colour/service bits
   */
  inline void resetbuffers()
  {
    clearFrameBuffer(0);        
    setBrightnessOE(brightness, 0); 

    if (m_cfg.double_buff) {
		
      clearFrameBuffer(1);        
      setBrightnessOE(brightness, 1);

    }
  }

#ifndef NO_FAST_FUNCTIONS
  /**
   * @brief - update DMA buff drawing horizontal line at specified coordinates
   * @param x_ccord - line start coordinate x
   * @param y_ccord - line start coordinate y
   * @param l - line length
   * @param r,g,b, - RGB888 colour
   */
  void hlineDMA(int16_t x_coord, int16_t y_coord, int16_t l, uint8_t red, uint8_t green, uint8_t blue);

  /**
   * @brief - update DMA buff drawing horizontal line at specified coordinates
   * @param x_ccord - line start coordinate x
   * @param y_ccord - line start coordinate y
   * @param l - line length
   * @param r,g,b, - RGB888 colour
   */
  void vlineDMA(int16_t x_coord, int16_t y_coord, int16_t l, uint8_t red, uint8_t green, uint8_t blue);

  /**
   * @brief - update DMA buff drawing a rectangular at specified coordinates
   * uses Fast H/V line draw internally, works faster than multiple consecutive pixel by pixel calls to updateMatrixDMABuffer()
   * @param int16_t x, int16_t y - coordinates of a top-left corner
   * @param int16_t w, int16_t h - width and height of a rectangular, min is 1 px
   * @param uint8_t r - RGB888 colour
   * @param uint8_t g - RGB888 colour
   * @param uint8_t b - RGB888 colour
   */
  void fillRectDMA(int16_t x_coord, int16_t y_coord, int16_t w, int16_t h, uint8_t r, uint8_t g, uint8_t b);
#endif

  // ------- PRIVATE -------
private:

  /* Setup the DMA Link List chain and configure the ESP32 DMA + I2S or LCD peripheral */
  bool setupDMA(const HUB75_I2S_CFG &opts);

  /**
   * pre-init procedures for specific drivers
   *
   */
  void shiftDriver(const HUB75_I2S_CFG &opts);

  /**
   * @brief - FM6124-family chips initialization routine
   */
  void fm6124init(const HUB75_I2S_CFG &_cfg);

  /**
   * @brief - DP3246-family chips initialization routine
   */
  void dp3246init(const HUB75_I2S_CFG& _cfg);

  /**
   * @brief - reset OE bits in DMA buffer in a way to control brightness
   * @param brt - brightness level from 0 to row_width
   * @param _buff_id - buffer id to control
   */
  // void brtCtrlOE(int brt, const bool _buff_id=0);

  /**
   * @brief - reset OE bits in DMA buffer in a way to control brightness
   * @param brt - brightness level from 0 to row_width
   * @param _buff_id - buffer id to control
   */
  void setBrightnessOE(uint8_t brt, const int _buff_id = 0);

  /**
   * @brief - transforms coordinates according to orientation
   * @param x - x position origin
   * @param y - y position origin
   * @param w - rectangular width
   * @param h - rectangular height
   */
  void transform(int16_t &x, int16_t &y, int16_t &w, int16_t &h)
  {
#ifndef NO_GFX
    int16_t t;
    switch (rotation)
    {
    case 1:
      t = _height - 1 - y - (h - 1);
      y = x;
      x = t;
      t = h;
      h = w;
      w = t;
      return;
    case 2:
      x = _width - 1 - x - (w - 1);
      y = _height - 1 - y - (h - 1);
      return;
    case 3:
      t = y;
      y = _width - 1 - x - (w - 1);
      x = t;
      t = h;
      h = w;
      w = t;
      return;
    }
#endif
  };

public:
  /**
   * Contains the resulting refresh rate (scan rate) that will be achieved
   * based on the i2sspeed, colour depth and min_refresh_rate requested.
   */
  int calculated_refresh_rate = 0;

protected:
  Bus_Parallel16 dma_bus;

private:

  // Matrix i2s settings
  HUB75_I2S_CFG m_cfg;

  /* Pixel data is organized from LSB to MSB sequentially by row, from row 0 to row matrixHeight/matrixRowsInParallel
   * (two rows of pixels are refreshed in parallel)
   * Memory is allocated (malloc'd) by the row, and not in one massive chunk, for flexibility.
   * The whole DMA framebuffer is just a vector of pointers to structs with ESP32_I2S_DMA_STORAGE_TYPE arrays
   * Since it's dimensions is unknown prior to class initialization, we just declare it here as empty struct and will do all allocations later.
   * Refer to rowBitStruct to get the idea of it's internal structure
   */
  frameStruct frame_buffer[2];
  frameStruct *fb; // What framebuffer we are writing pixel changes to? (pointer to either frame_buffer[0] or frame_buffer[1] basically ) used within updateMatrixDMABuffer(...)

  volatile int back_buffer_id = 0;      // If using double buffer, which one is NOT active (ie. being displayed) to write too?
  int brightness = 128;        // If you get ghosting... reduce brightness level. ((60/64)*255) seems to be the limit before ghosting on a 64 pixel wide physical panel for some panels.
  int lsbMsbTransitionBit = 0; // For colour depth calculations

  /* ESP32-HUB75-MatrixPanel-I2S-DMA functioning constants
   * we should not those once object instance initialized it's DMA structs
   * they weree const, but this lead to bugs, when the default constructor was called.
   * So now they could be changed, but shouldn't. Maybe put a cpp lock around it, so it can't be changed after initialisation
   */
  uint16_t PIXELS_PER_ROW = m_cfg.mx_width * m_cfg.chain_length;      // number of pixels in a single row of all chained matrix modules (WIDTH of a combined matrix chain)
  uint8_t ROWS_PER_FRAME = m_cfg.mx_height / MATRIX_ROWS_IN_PARALLEL; // RPF - rows per frame, either 16 or 32 depending on matrix module
  uint8_t MASK_OFFSET = 16 - m_cfg.getPixelColorDepthBits();

  // Other private variables
  bool initialized = false;
  bool config_set = false;

}; // end Class header

/***************************************************************************************/
// https://stackoverflow.com/questions/5057021/why-are-c-inline-functions-in-the-header
/* 2. functions declared in the header must be marked inline because otherwise, every translation unit which includes the header will contain a definition of the function, and the linker will complain about multiple definitions (a violation of the One Definition Rule). The inline keyword suppresses this, allowing multiple translation units to contain (identical) definitions. */

/**
 * @brief - convert RGB565 to RGB888
 * @param uint16_t colour - RGB565 input colour
 * @param uint8_t &r, &g, &b - refs to variables where converted colours would be emplaced
 */
inline void MatrixPanel_I2S_DMA::color565to888(const uint16_t color, uint8_t &r, uint8_t &g, uint8_t &b)
{
  r = (color >> 8) & 0xf8;
  g = (color >> 3) & 0xfc;
  b = (color << 3);
  r |= r >> 5;
  g |= g >> 6;
  b |= b >> 5;
}

inline void MatrixPanel_I2S_DMA::drawPixel(int16_t x, int16_t y, uint16_t color) // adafruit virtual void override
{
  uint8_t r, g, b;
  color565to888(color, r, g, b);

  int16_t w = 1, h = 1;
  transform(x, y, w, h);
  updateMatrixDMABuffer(x, y, r, g, b);
}

inline void MatrixPanel_I2S_DMA::fillScreen(uint16_t color) // adafruit virtual void override
{
  uint8_t r, g, b;
  color565to888(color, r, g, b);

  updateMatrixDMABuffer(r, g, b); // RGB only (no pixel coordinate) version of 'updateMatrixDMABuffer'
}

inline void MatrixPanel_I2S_DMA::drawPixelRGB888(int16_t x, int16_t y, uint8_t r, uint8_t g, uint8_t b)
{
  int16_t w = 1, h = 1;
  transform(x, y, w, h);
  updateMatrixDMABuffer(x, y, r, g, b);
}

inline void MatrixPanel_I2S_DMA::fillScreenRGB888(uint8_t r, uint8_t g, uint8_t b)
{
  updateMatrixDMABuffer(r, g, b); // RGB only (no pixel coordinate) version of 'updateMatrixDMABuffer'
}

#ifdef USE_GFX_LITE
// Support for CRGB values provided via FastLED
inline void MatrixPanel_I2S_DMA::drawPixel(int16_t x, int16_t y, CRGB color)
{
  int16_t w = 1, h = 1;
  transform(x, y, w, h);
  updateMatrixDMABuffer(x, y, color.red, color.green, color.blue);
}

inline void MatrixPanel_I2S_DMA::fillScreen(CRGB color)
{
  updateMatrixDMABuffer(color.red, color.green, color.blue);
}
#endif

// Pass 8-bit (each) R,G,B, get back 16-bit packed colour
// https://github.com/squix78/ILI9341Buffer/blob/master/ILI9341_SPI.cpp
inline uint16_t MatrixPanel_I2S_DMA::color565(uint8_t r, uint8_t g, uint8_t b)
{
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

#endif

// Credits: Louis Beaudoin <https://github.com/pixelmatix/SmartMatrix/tree/teensylc>
// and Sprite_TM:           https://www.esp32.com/viewtopic.php?f=17&t=3188 and https://www.esp32.com/viewtopic.php?f=13&t=3256

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
    doing so hides any artefacts that appear at this time. The OE line is also used to dim the display by only turning it on for a limited time every
    line.

    All in all, an image can be displayed by 'scanning' the display, say, 100 times per second. The slowness of the human eye hides the fact that
    only one line is showed at a time, and the display looks like every pixel is driven at the same time.

    Now, the RGB inputs for these types of displays are digital, meaning each red, green and blue subpixel can only be on or off. This leads to a
    colour palette of 8 pixels, not enough to display nice pictures. To get around this, we use binary code modulation.

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

    We use a front buffer/back buffer technique here to make sure the display is refreshed in one go and drawing artefacts do not reach the display.
    In practice, for small displays this is not really necessarily.

*/
