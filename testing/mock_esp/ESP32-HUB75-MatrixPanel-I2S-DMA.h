#ifndef _ESP32_RGB_64_32_MATRIX_PANEL_I2S_DMA
#define _ESP32_RGB_64_32_MATRIX_PANEL_I2S_DMA

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

struct CRGB {
	uint8_t r = 0;
	uint8_t g = 0;
	uint8_t b = 0;
	CRGB() = default;
	CRGB(uint8_t rr, uint8_t gg, uint8_t bb) : r(rr), g(gg), b(bb) {}
};

namespace mock_esp_state {
inline int& error_count() {
	static int c = 0;
	return c;
}
inline int& warn_count() {
	static int c = 0;
	return c;
}
inline void reset_logs() {
	error_count() = 0;
	warn_count()  = 0;
}
} // namespace mock_esp_state

#ifndef ESP_LOGE
#define ESP_LOGE(tag, fmt, ...) do { (void)(tag); mock_esp_state::error_count()++; } while (0)
#endif
#ifndef ESP_LOGW
#define ESP_LOGW(tag, fmt, ...) do { (void)(tag); mock_esp_state::warn_count()++; } while (0)
#endif
#ifndef ESP_LOGI
#define ESP_LOGI(tag, fmt, ...) do { (void)(tag); } while (0)
#endif

#if defined(USE_GFX_LITE)
  #include "GFX_Lite.h"
#elif !defined(NO_GFX)
  #include "Adafruit_GFX.h"
#endif

#ifndef RGB_PANEL_WIDTH
#define RGB_PANEL_WIDTH 64
#endif
#ifndef RGB_PANEL_HEIGHT
#define RGB_PANEL_HEIGHT 32
#endif
#ifndef CHAIN_LENGTH
#define CHAIN_LENGTH 1
#endif

struct HUB75_I2S_CFG {
	enum shift_driver {
		SHIFTREG = 0,
		FM6124,
		FM6126A,
		ICN2038S,
		MBI5124,
		DP3246
	};
	enum line_driver {
		TYPE138 = 0,
		TYPE595,
		TYPE_DIRECT,
		SM5266P,
		SM5368 = TYPE595
	};
	enum clk_speed {
		HZ_8M  = 8000000,
		HZ_10M = 10000000,
		HZ_15M = 16000000,
		HZ_16M = 16000000,
		HZ_20M = 20000000
	};
	struct i2s_pins {
		int8_t r1, g1, b1, r2, g2, b2, a, b, c, d, e, lat, oe, clk;
	};

	uint16_t mx_width;
	uint16_t mx_height;
	uint16_t chain_length;
	i2s_pins gpio;
	shift_driver driver;
	line_driver line_decoder;
	bool double_buff;
	clk_speed i2sspeed;
	uint8_t latch_blanking;
	bool clkphase;
	uint16_t min_refresh_rate;

	HUB75_I2S_CFG(
		uint16_t _w = RGB_PANEL_WIDTH,
		uint16_t _h = RGB_PANEL_HEIGHT,
		uint16_t _chain = CHAIN_LENGTH,
		i2s_pins _pinmap = {25, 26, 27, 14, 12, 13, 23, 19, 5, 17, -1, 4, 15, 16},
		shift_driver _drv = SHIFTREG,
		line_driver _line_drv = TYPE138,
		bool _dbuff = false,
		clk_speed _i2sspeed = HZ_10M,
		uint8_t _latblk = 0,
		bool _clockphase = true,
		uint16_t _min_refresh_rate = 60,
		uint8_t /*_pixel_color_depth_bits*/ = 8)
	  : mx_width(_w),
		mx_height(_h),
		chain_length(_chain),
		gpio(_pinmap),
		driver(_drv),
		line_decoder(_line_drv),
		double_buff(_dbuff),
		i2sspeed(_i2sspeed),
		latch_blanking(_latblk ? _latblk : 4),
		clkphase(_clockphase),
		min_refresh_rate(_min_refresh_rate) {}
};

struct DrawnPixel {
	int16_t x;
	int16_t y;
	uint16_t color565;
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t kind; // 0 = 565, 1 = RGB888, 2 = CRGB
};

class MatrixPanel_I2S_DMA {
public:
	explicit MatrixPanel_I2S_DMA(const HUB75_I2S_CFG& cfg = HUB75_I2S_CFG()) : m_cfg(cfg) {}

	bool begin() { return true; }
	void setBrightness8(uint8_t) {}
	void clearScreen() { drawn.clear(); }
	void flipDMABuffer() {}

	const HUB75_I2S_CFG& getCfg() const noexcept { return m_cfg; }

	void drawPixel(int16_t x, int16_t y, uint16_t color) {
		drawn.push_back({x, y, color, 0, 0, 0, 0});
	}
	void fillScreen(uint16_t) {}

	void drawPixelRGB888(int16_t x, int16_t y, uint8_t r, uint8_t g, uint8_t b) {
		drawn.push_back({x, y, color565(r, g, b), r, g, b, 1});
	}
	void fillScreenRGB888(uint8_t, uint8_t, uint8_t) {}

	void drawPixel(int16_t x, int16_t y, CRGB c) {
		drawn.push_back({x, y, color565(c.r, c.g, c.b), c.r, c.g, c.b, 2});
	}
	void fillScreen(CRGB) {}

	void drawRect(int16_t, int16_t, int16_t, int16_t, uint16_t) {}
	void setTextColor(uint16_t) {}
	void setTextSize(uint8_t) {}
	void setFont(const void* = nullptr) {}
	void setCursor(int16_t, int16_t) {}
	void print(int) {}
	void print(const char*) {}

	static uint16_t color444(uint8_t r, uint8_t g, uint8_t b) {
		return color565(static_cast<uint8_t>(r * 17),
		                static_cast<uint8_t>(g * 17),
		                static_cast<uint8_t>(b * 17));
	}
	static uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
		return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
	}

	std::vector<DrawnPixel> drawn;

private:
	HUB75_I2S_CFG m_cfg;
};

#endif // _ESP32_RGB_64_32_MATRIX_PANEL_I2S_DMA
