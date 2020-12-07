/**
 * Experimental layer class to do play with pixel in an off-screen buffer before painting to the DMA 
 *
 * Requires FastLED
 *
 * Faptastic 2020
 **/

#ifndef DISPLAY_MATRIX_LAYER
#define DISPLAY_MATRIX_LAYER


/* Use GFX_Root (https://github.com/mrfaptastic/GFX_Root) instead of 
 * Adafruit_GFX library. No real benefit unless you don't want Bus_IO & Wire.h library dependencies. 
 */
#define USE_GFX_ROOT 1


#ifdef USE_GFX_ROOT
	#include "GFX.h" // Adafruit GFX core class -> https://github.com/mrfaptastic/GFX_Root
#else	
	#include "Adafruit_GFX.h" // Adafruit class with all the other stuff
#endif	

#include <FastLed.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

/* 
 * Set the width and height of the layer buffers. This should match exactly that of your output display, or virtual display.
 */
#define LAYER_WIDTH 	  64
#define LAYER_HEIGHT  	32


#define HALF_WHITE_COLOUR 0x8410
#define BLACK_BACKGROUND_PIXEL_COLOUR CRGB(0,0,0)

enum textPosition { TOP, MIDDLE, BOTTOM };


/* To help with direct pixel referencing by width and height */
struct layerPixels {
	CRGB data[LAYER_HEIGHT][LAYER_WIDTH]; 
};

#ifdef USE_GFX_ROOT
class Layer : public GFX
#else
class Layer : public Adafruit_GFX
#endif
// class Layer : public GFX // use GFX Root for now
{
	public:

		// Static allocation of memory for layer
		//CRGB pixels[LAYER_WIDTH][LAYER_HEIGHT] = {{0}};

		Layer(MatrixPanel_I2S_DMA &disp) : GFX (LAYER_WIDTH, LAYER_HEIGHT) {
			matrix = &disp;
		}

		inline void init()
		{
				// https://stackoverflow.com/questions/5914422/proper-way-to-initialize-c-structs
				pixels = new layerPixels();

				//pixels = (layerPixels *) malloc(sizeof(layerPixels));
			//	pixel = (CRGB *) &pixels[0];
				//Serial.printf("Allocated %d bytes of memory for standard CRGB (24bit) layer.\r\n", NUM_PIXELS*sizeof(CRGB));
				Serial.printf("Allocated %d bytes of memory for layerPixels.\r\n", sizeof(layerPixels));

		} // end Layer

		void drawPixel(int16_t x, int16_t y, uint16_t color);   		// overwrite adafruit implementation
		void drawPixel(int16_t x, int16_t y, int r, int g, int b);		// Layer implementation		
		void drawPixel(int16_t x, int16_t y, CRGB color);				// Layer implementation		

		// Font Stuff
		//https://forum.arduino.cc/index.php?topic=642749.0
		void drawCentreText(const char *buf, textPosition textPos = BOTTOM, const GFXfont *f = NULL, CRGB color = 0x8410, int yadjust = 0); // 128,128,128 RGB @ bottom row by default
	

		void dim(byte value);
		void clear();
	    void display();  //	flush to display / LED matrix

		// override the color of all pixels that aren't the transparent color
		void overridePixelColor(int r, int g, int b);

		inline uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
					return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
		}

		inline void setTransparency(bool t) { transparency_enabled = t; }

		// Effects
		void moveX(int delta);
		void autoCenterX();		
		void moveY(int delta);

		// For layer composition - accessed publically
		CRGB 		transparency_colour 	= BLACK_BACKGROUND_PIXEL_COLOUR;
		bool		transparency_enabled 	= true;
		layerPixels *pixels;

		// Release Memory
		~Layer(void); 

	private:
		MatrixPanel_I2S_DMA *matrix = NULL;		
};


/* Merge FastLED layers into a super layer and display. */
namespace LayerCompositor
{
        void Stack(MatrixPanel_I2S_DMA &disp, const Layer &_bgLayer, const Layer &_fgLayer, bool writeToBgLayer = false);
        void Siloette(MatrixPanel_I2S_DMA &disp, const Layer &_bgLayer, const Layer &_fgLayer);		
		    void Blend(MatrixPanel_I2S_DMA &disp, const Layer &_bgLayer, const Layer &_fgLayer, uint8_t ratio = 127);
}

#endif
