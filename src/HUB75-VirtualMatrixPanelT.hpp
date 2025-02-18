/**
 * @file VirtualMatrixPanelT.hpp
 * @brief Templated Virtual Matrix Panel class for HUB75 displays.
 *
 * This header defines the VirtualMatrixPanelT template class which maps virtual pixel
 * coordinates to physical LED coordinates. It supports compile‐time configuration for:
 *   - Panel chain type (PANEL_CHAIN_TYPE)
 *   - Scan type mapping (via a policy class, default is y)
 *   - A compile‐time scale factor (each virtual pixel is drawn as a block)
 *
 * Runtime rotation is supported via setRotation(). Depending on the build options,
 * the class conditionally inherits from Adafruit_GFX, GFX_Lite, or stands alone.
 *
 * @tparam ChainType       Compile‐time panel chain configuration.
 * @tparam ScanType        Policy type implementing a static apply() function for mapping.
 *                         Default is DefaultScanType<STANDARD_TWO_SCAN>.
 * @tparam ScaleFactor     Compile‐time zoom factor (each virtual pixel becomes a
 *                         ScaleFactor x ScaleFactor block).
 *
 * @note The enum PANEL_SCAN_TYPE replaces the former PANEL_SCAN_RATE.
 */
 


#ifndef VIRTUAL_MATRIX_PANEL_TEMPLATE_H
#define VIRTUAL_MATRIX_PANEL_TEMPLATE_H

//#include <cstdint>
#include "ESP32-HUB75-MatrixPanel-I2S-DMA.h"

#ifdef USE_GFX_LITE
  #include "GFX_Lite.h"
  #include <Fonts/FreeSansBold12pt7b.h>
#elif !defined(NO_GFX)
  #include "Adafruit_GFX.h"
  #include <Fonts/FreeSansBold12pt7b.h>
#endif

// ----------------------------------------------------------------------
// Data structures and enums

/**
 * @brief Structure holding virtual/physical coordinate mapping.
 */
struct VirtualCoords {
    int16_t x;
    int16_t y;
    int16_t virt_row; // chain of panels row (optional)
    int16_t virt_col; // chain of panels col (optional)
    VirtualCoords() : x(0), y(0), virt_row(0), virt_col(0) {}
};

/**
 * @brief Panel scan types.
 *
 * Defines the different scanning modes.
 */
enum PANEL_SCAN_TYPE {
    STANDARD_TWO_SCAN,
    FOUR_SCAN_32PX_HIGH,   ///< Four-scan mode, 32-pixel high panels.
    FOUR_SCAN_16PX_HIGH,   ///< Four-scan mode, 16-pixel high panels.
    FOUR_SCAN_64PX_HIGH,   ///< Four-scan mode, 64-pixel high panels.
    FOUR_SCAN_40PX_HIGH    ///< Four-scan mode, 40-pixel high panels.
};

/**
 * @brief Panel chain types.
 *
 * Defines the physical chain configuration for multiple panels.
 */
enum PANEL_CHAIN_TYPE {
    CHAIN_NONE,                 ///< No chaining.
    CHAIN_TOP_LEFT_DOWN,        ///< Chain starting top-left, going down.
    CHAIN_TOP_RIGHT_DOWN,       ///< Chain starting top-right, going down.
    CHAIN_BOTTOM_LEFT_UP,       ///< Chain starting bottom-left, going up.
    CHAIN_BOTTOM_RIGHT_UP,      ///< Chain starting bottom-right, going up.
    CHAIN_TOP_LEFT_DOWN_ZZ,     ///< Zigzag chain starting top-left.
    CHAIN_TOP_RIGHT_DOWN_ZZ,    ///< Zigzag chain starting top-right.
    CHAIN_BOTTOM_RIGHT_UP_ZZ,   ///< Zigzag chain starting bottom-right.
    CHAIN_BOTTOM_LEFT_UP_ZZ     ///< Zigzag chain starting bottom-left.
};

// ----------------------------------------------------------------------
// Default Scan Rate Policy
/**
 * @brief Default policy for scan type mapping.
 *
 * This templated policy implements the static function apply() to remap
 * coordinates according to the panel scan type. It uses the panel's pixel base
 * to calculate offsets.
 *
 * @tparam Type The compile-time scan type (of type PANEL_SCAN_TYPE).
 */
template <PANEL_SCAN_TYPE ScanType>
struct ScanTypeMapping {
    static constexpr VirtualCoords apply(VirtualCoords coords, int virt_y, int panel_pixel_base) 
	{
        log_v("ScanTypeMapping: coords.x: %d, coords.y: %d, virt_y: %d, pixel_base: %d", coords.x, coords.y, virt_y, panel_pixel_base);

        if constexpr (ScanType == FOUR_SCAN_64PX_HIGH || ScanType == FOUR_SCAN_32PX_HIGH) 
		{
            int adjusted_y = virt_y;
            if constexpr (ScanType == FOUR_SCAN_64PX_HIGH) 
			{
                // As in the original code (with extra remapping for 64px high panels)
                if ((virt_y & 8) != ((virt_y & 16) >> 1))
                    adjusted_y = (((virt_y & 0b11000) ^ 0b11000) + (virt_y & 0b11100111));
            }

            if ((coords.y & 8) == 0) {
                coords.x += (((coords.x / panel_pixel_base) + 1) * panel_pixel_base);
			} else {
                coords.x += ((coords.x / panel_pixel_base) * panel_pixel_base);
			}
			
            coords.y = (adjusted_y >> 4) * 8 + (adjusted_y & 0b00000111);
        }
        else if constexpr (ScanType == FOUR_SCAN_16PX_HIGH) {
            if ((coords.y & 4) == 0) {
                coords.x += (((coords.x / panel_pixel_base) + 1) * panel_pixel_base);
			} else {
                coords.x += ((coords.x / panel_pixel_base) * panel_pixel_base);
			}
			
            coords.y = (coords.y >> 3) * 4 + (coords.y & 0b00000011);
        }
        else if constexpr (ScanType == FOUR_SCAN_40PX_HIGH) {
            
            if (((coords.y) / 10) % 2 == 0) {				
                coords.x += (((coords.x / panel_pixel_base) + 1) * panel_pixel_base);
			} else {
                coords.x += ((coords.x / panel_pixel_base) * panel_pixel_base);
			}
            coords.y = (coords.y / 20) * 10 + (coords.y % 10);
        }
        // For STANDARD_TWO_SCAN / NORMAL_ONE_SIXTEEN no remapping is done.
        return coords;
    }
};

// ----------------------------------------------------------------------
// VirtualMatrixPanelT Declaration
//
// Template parameters:
//   - ChainScanType: compile–time panel chain configuration.
//   - ScanTypeMapping: a policy type implementing a static "apply" function 
//                     (default is ScanTypeMapping<STANDARD_TWO_SCAN>).
//   - ScaleFactor: a compile–time zoom factor (must be >= 1).
#ifdef USE_GFX_LITE
template <PANEL_CHAIN_TYPE ChainScanType,
          class ScanTypeMapping = ScanTypeMapping<STANDARD_TWO_SCAN>,
          int ScaleFactor = 1>
class VirtualMatrixPanelT : public GFX {
public:
#elif !defined(NO_GFX)
template <PANEL_CHAIN_TYPE ChainScanType,
          class ScanTypeMapping = ScanTypeMapping<STANDARD_TWO_SCAN>,
          int ScaleFactor = 1>
class VirtualMatrixPanelT : public Adafruit_GFX {
public:
#else
template <PANEL_CHAIN_TYPE ChainScanType,
          class ScanTypeMapping = ScanTypeMapping<STANDARD_TWO_SCAN>,
          int ScaleFactor = 1>
class VirtualMatrixPanelT {
public:
#endif

    // Constructor: pass the underlying MatrixPanel_I2S_DMA display,
    // virtual module dimensions, and physical panel resolution.
    // (Chain type is chosen at compile time.)
    VirtualMatrixPanelT(uint8_t _vmodule_rows,
                        uint8_t _vmodule_cols,
                        uint8_t _panel_res_x,
                        uint8_t _panel_res_y)
#ifdef USE_GFX_LITE
      : GFX(_vmodule_cols * _panel_res_x, _vmodule_rows * _panel_res_y),
#elif !defined(NO_GFX)
      : Adafruit_GFX(_vmodule_cols * _panel_res_x, _vmodule_rows * _panel_res_y),
#endif
        panel_res_x(_panel_res_x),
        panel_res_y(_panel_res_y),
        panel_pixel_base(_panel_res_x), // default pixel base is panel_res_x
        vmodule_rows(_vmodule_rows),
        vmodule_cols(_vmodule_cols),
        virtual_res_x(_vmodule_cols * _panel_res_x),
        virtual_res_y(_vmodule_rows * _panel_res_y),
        dma_res_x(_panel_res_x * _vmodule_rows * _vmodule_cols - 1),
        _virtual_res_x(virtual_res_x),
        _virtual_res_y(virtual_res_y),
        _rotate(0)
    {
        // Initialize with an invalid coordinate.
        coords.x = coords.y = -1;
    }

    // ------------------------------------------------------------------
    // Drawing methods
    inline void drawPixel(int16_t x, int16_t y, uint16_t color) {
        if constexpr (ScaleFactor > 1) 
		{
            for (int dx = 0; dx < ScaleFactor; dx++) {
                for (int dy = 0; dy < ScaleFactor; dy++) {
                    //irtualCoords v = getCoords(x * ScaleFactor + dx, y * ScaleFactor + dy);
					// display->drawPixel(v.x, v.y, color);
					calcCoords(x * ScaleFactor + dx, y * ScaleFactor + dy);
					display->drawPixel(coords.x, coords.y, color);
					
                }
            }
        } else {
            //VirtualCoords v = getCoords(x, y);
            //display->drawPixel(v.x, v.y, color);
			
			calcCoords(x , y);
			display->drawPixel(coords.x, coords.y, color);			
        }

        log_v("x: %d, y: %d -> coords.x: %d, coords.y: %d", x, y, coords.x, coords.y);
    }

    inline void fillScreen(uint16_t color) {
        display->fillScreen(color);
    }

    inline void fillScreenRGB888(uint8_t r, uint8_t g, uint8_t b) {
        display->fillScreenRGB888(r, g, b);
    }

    inline void drawPixelRGB888(int16_t x, int16_t y, uint8_t r, uint8_t g, uint8_t b) {
        //VirtualCoords v = getCoords(x, y);
        //display->drawPixelRGB888(v.x, v.y, r, g, b);
		
		calcCoords(x , y);
		display->drawPixelRGB888(coords.x, coords.y, r, g, b);	
    }

#ifdef USE_GFX_LITE
    inline void drawPixel(int16_t x, int16_t y, CRGB color) {
        //VirtualCoords v = getCoords(x, y);
        //display->drawPixel(v.x, v.y, color);
		
		calcCoords(x , y);
		display->drawPixel(coords.x, coords.y, color);
		
    }

    inline void fillScreen(CRGB color) {
        display->fillScreen(color);
    }
#endif

#ifndef NO_GFX
    inline void drawDisplayTest() {
        display->setFont(&FreeSansBold12pt7b);
        display->setTextColor(display->color565(255, 255, 0));
        display->setTextSize(1);
        for (int panel = 0; panel < vmodule_cols * vmodule_rows; panel++) {
            int top_left_x = (panel == 0) ? 0 : (panel * panel_res_x);
            display->drawRect(top_left_x, 0, panel_res_x, panel_res_y, display->color565(0, 255, 0));
            display->setCursor((panel * panel_res_x) + 2, panel_res_y - 4);
            display->print((vmodule_cols * vmodule_rows) - panel);
        }
    }
#endif

    inline void clearScreen() { display->clearScreen(); }

    inline uint16_t color444(uint8_t r, uint8_t g, uint8_t b) { return display->color444(r, g, b); }
    inline uint16_t color565(uint8_t r, uint8_t g, uint8_t b) { return display->color565(r, g, b); }

    inline void flipDMABuffer() { display->flipDMABuffer(); }

    // ------------------------------------------------------------------
    // Rotation (runtime)
    inline void setRotation(uint8_t rotate) {
        if (rotate < 4)
            _rotate = rotate;
#ifdef NO_GFX
        // When NO_GFX is defined, update _virtual_res_x/_virtual_res_y as needed.
#else
        uint8_t rotation = (rotate & 3);
        switch (rotation) {
            case 0:
            case 2:
                _virtual_res_x = virtual_res_x;
                _virtual_res_y = virtual_res_y;
                _width = virtual_res_x;
                _height = virtual_res_y;
                break;
            case 1:
            case 3:
                _virtual_res_x = virtual_res_y;
                _virtual_res_y = virtual_res_x;
                _width = virtual_res_y;
                _height = virtual_res_x;
                break;
        }
#endif
    }

    // ------------------------------------------------------------------
    // Panel scan–type configuration (runtime adjustment of pixel base)
    inline void setPixelBase(uint8_t pixel_base) {
        panel_pixel_base = pixel_base;
    }

    // ------------------------------------------------------------------
    // calcCoords() maps a virtual (x,y) coordinate to a physical coordinate.
    //VirtualCoords getCoords(int16_t virt_x, int16_t virt_y) {
	void calcCoords(int16_t virt_x, int16_t virt_y) {
		
#ifdef NO_GFX
        if (virt_x < 0 || virt_x >= _virtual_res_x || virt_y < 0 || virt_y >= _virtual_res_y) {
#else
        if (virt_x < 0 || virt_x >= _width || virt_y < 0 || virt_y >= _height) {
#endif
            coords.x = coords.y = -1;
			return;
            //return coords;
        }

        //log_d("calcCoords pre-chain: virt_x: %d, virt_y: %d", virt_x, virt_y);

        // --- Runtime rotation ---
        switch (_rotate) {
            case 1: {
                int16_t temp = virt_x;
                virt_x = virt_y;
                virt_y = virtual_res_y - 1 - temp;
                break;
            }
            case 2: {
                virt_x = virtual_res_x - 1 - virt_x;
                virt_y = virtual_res_y - 1 - virt_y;
                break;
            }
            case 3: {
                int16_t temp = virt_x;
                virt_x = virtual_res_x - 1 - virt_y;
                virt_y = temp;
                break;
            }
            default:
                break;
        }

        // --- Chain mapping ---
        int row = virt_y / panel_res_y; // 0-indexed row in the virtual module
        if constexpr (ChainScanType == CHAIN_TOP_RIGHT_DOWN) {
            if ((row & 1) == 1) {
                coords.x = dma_res_x - virt_x - (row * virtual_res_x);
                coords.y = panel_res_y - 1 - (virt_y % panel_res_y);
            } else {
                coords.x = ((vmodule_rows - (row + 1)) * virtual_res_x) + virt_x;
                coords.y = (virt_y % panel_res_y);
            }
        }
        else if constexpr (ChainScanType == CHAIN_TOP_RIGHT_DOWN_ZZ) {
            coords.x = ((vmodule_rows - (row + 1)) * virtual_res_x) + virt_x;
            coords.y = (virt_y % panel_res_y);
        }
        else if constexpr (ChainScanType == CHAIN_TOP_LEFT_DOWN) {
            if ((row & 1) == 0) {
                coords.x = dma_res_x - virt_x - (row * virtual_res_x);
                coords.y = panel_res_y - 1 - (virt_y % panel_res_y);
            } else {
                coords.x = ((vmodule_rows - (row + 1)) * virtual_res_x) + virt_x;
                coords.y = (virt_y % panel_res_y);
            }
        }
        else if constexpr (ChainScanType == CHAIN_TOP_LEFT_DOWN_ZZ) {
            coords.x = ((vmodule_rows - (row + 1)) * virtual_res_x) + virt_x;
            coords.y = (virt_y % panel_res_y);
        }
        else if constexpr (ChainScanType == CHAIN_BOTTOM_LEFT_UP) {
            row = vmodule_rows - row - 1;
            if ((row & 1) == 1) {
                coords.x = ((vmodule_rows - (row + 1)) * virtual_res_x) + virt_x;
                coords.y = (virt_y % panel_res_y);
            } else {
                coords.x = dma_res_x - (row * virtual_res_x) - virt_x;
                coords.y = panel_res_y - 1 - (virt_y % panel_res_y);
            }
        }
        else if constexpr (ChainScanType == CHAIN_BOTTOM_LEFT_UP_ZZ) {
            row = vmodule_rows - row - 1;
            coords.x = ((vmodule_rows - (row + 1)) * virtual_res_x) + virt_x;
            coords.y = (virt_y % panel_res_y);
        }
        else if constexpr (ChainScanType == CHAIN_BOTTOM_RIGHT_UP) {
            row = vmodule_rows - row - 1;
            if ((row & 1) == 0) {
                coords.x = ((vmodule_rows - (row + 1)) * virtual_res_x) + virt_x;
                coords.y = (virt_y % panel_res_y);
            } else {
                coords.x = dma_res_x - (row * virtual_res_x) - virt_x;
                coords.y = panel_res_y - 1 - (virt_y % panel_res_y);
            }
        }
        else if constexpr (ChainScanType == CHAIN_BOTTOM_RIGHT_UP_ZZ) {
            row = vmodule_rows - row - 1;
            coords.x = ((vmodule_rows - (row + 1)) * virtual_res_x) + virt_x;
            coords.y = (virt_y % panel_res_y);
        }
        else { // CHAIN_NONE (default)
            coords.x = virt_x;
            coords.y = virt_y;
        }

        //log_d("calcCoords post-chain: virt_x: %d, virt_y: %d", virt_x, virt_y);    

        // --- Apply scan–type mapping ---
        coords = ScanTypeMapping::apply(coords, virt_y, panel_pixel_base);

        //return coords;
    }

#ifdef NO_GFX
    inline uint16_t width()  const { return _virtual_res_x; }
    inline uint16_t height() const { return _virtual_res_y; }
#endif

    // ------------------------------------------------------------------
    // Data members (public for compatibility)
    VirtualCoords coords;
	
    uint8_t panel_res_x;       // physical panel resolution X
    uint8_t panel_res_y;       // physical panel resolution Y
    uint8_t panel_pixel_base; // used for scan–type mapping

    inline void setDisplay(MatrixPanel_I2S_DMA &disp) {
        display = &disp;
    }

private:
    MatrixPanel_I2S_DMA *display;
    // Note: panel_chain_type is now fixed via the compile–time template parameter 'ChainScanType'.
    uint16_t virtual_res_x;    // virtual display width (combination of panels)
    uint16_t virtual_res_y;    // virtual display height (combination of panels)
    uint16_t _virtual_res_x;   // width adjusted by current rotation
    uint16_t _virtual_res_y;   // height adjusted by current rotation
    uint8_t  vmodule_rows;    // virtual module rows
    uint8_t  vmodule_cols;    // virtual module columns
    uint16_t dma_res_x;        // width as seen by the DMA engine

    int _rotate;             // runtime rotation (0 to 3)
};

#endif  // VIRTUAL_MATRIX_PANEL_TEMPLATE_H
