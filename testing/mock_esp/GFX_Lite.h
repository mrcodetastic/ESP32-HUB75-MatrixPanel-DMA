#pragma once
#include <cstdint>

class GFX {
public:
	GFX(int16_t w, int16_t h) : WIDTH(w), HEIGHT(h), _width(w), _height(h), rotation(0) {}
	virtual ~GFX() = default;

	virtual void drawPixel(int16_t x, int16_t y, uint16_t color) = 0;
	virtual void fillScreen(uint16_t color) { fillRect(0, 0, _width, _height, color); }
	virtual void setRotation(uint8_t r) {
		rotation = r & 3;
		if (rotation == 1 || rotation == 3) {
			_width  = HEIGHT;
			_height = WIDTH;
		} else {
			_width  = WIDTH;
			_height = HEIGHT;
		}
	}
	virtual void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
		for (int16_t i = 0; i < h; i++) drawPixel(x, y + i, color);
	}
	virtual void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
		for (int16_t i = 0; i < w; i++) drawPixel(x + i, y, color);
	}
	virtual void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
		for (int16_t dy = 0; dy < h; dy++) drawFastHLine(x, y + dy, w, color);
	}
	void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) {
		drawFastHLine(x, y, w, c);
		drawFastHLine(x, y + h - 1, w, c);
		drawFastVLine(x, y, h, c);
		drawFastVLine(x + w - 1, y, h, c);
	}
	void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t c) {
		drawPixel(x0, y0, c);
		drawPixel(x1, y1, c);
	}
	void setTextColor(uint16_t) {}
	void setTextSize(uint8_t) {}
	void setCursor(int16_t, int16_t) {}
	void print(int) {}
	void print(const char*) {}

	int16_t width() const { return _width; }
	int16_t height() const { return _height; }
	uint8_t getRotation() const { return rotation; }

protected:
	int16_t WIDTH;
	int16_t HEIGHT;
	int16_t _width;
	int16_t _height;
	uint8_t rotation;
};
