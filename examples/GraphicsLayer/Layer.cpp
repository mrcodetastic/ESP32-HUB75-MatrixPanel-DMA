/**
 * Experimental layer class to do play with pixel in an off-screen buffer before painting to the DMA 
 *
 * Requires FastLED
 *
 * Faptastic 2020
 **/

#include "Layer.h"

// For adafruit
void Layer::drawPixel(int16_t x, int16_t y, uint16_t color) {

	// 565 color conversion
	uint8_t r = ((((color >> 11)  & 0x1F) * 527) + 23) >> 6;
	uint8_t g = ((((color >> 5)   & 0x3F) * 259) + 33) >> 6;
	uint8_t b = (((color & 0x1F)  * 527) + 23) >> 6;

	drawPixel(x, y, CRGB(r,g,b));
}

void Layer::drawPixel(int16_t x, int16_t y, int r, int g, int b) {
	drawPixel(x, y, CRGB(r,g,b));
}

void Layer::drawPixel(int16_t x, int16_t y, CRGB color) {

	if( x >= LAYER_WIDTH 	|| x < 0) return; // 0;
	if( y >= LAYER_HEIGHT 	|| y < 0) return; // 0;
	
	pixels->data[y][x] = color;
}

/**
 * Dim all the pixels in the display.
 */
void Layer::dim(byte value)  {   

		// nscale8 max value is 255, or it'll flip back to 0 
		// (documentation is wrong when it says x/256), it's actually x/255
		for (int y = 0; y < LAYER_HEIGHT; y++) {
			for (int x = 0; x < LAYER_WIDTH; x++) {
				pixels->data[y][x].nscale8(value);
		}}
}

void Layer::clear() { 

	memset(pixels, BLACK_BACKGROUND_PIXEL_COLOUR, sizeof(layerPixels) );
}

/**
 * Send the layer to the display device.
 */
void Layer::display() { 

	CRGB _pixel = 0 ;
	for (int y = 0; y < LAYER_HEIGHT; y++) {
		for (int x = 0; x < LAYER_WIDTH; x++)
		{
			//_pixel = pixel[XY(x, y)];
			_pixel = pixels->data[y][x];

			matrix->drawPixelRGB888( x, y, _pixel.r, _pixel.g, _pixel.b);

			/*
			if ( !transparency_enabled ){
				matrix->drawPixelRGB888( x, y, _pixel.r, _pixel.g, _pixel.b);
			} else {
				if (_pixel != transparency_colour) {
					matrix->drawPixelRGB888( x, y, _pixel.r, _pixel.g, _pixel.b);
				}
			}
			*/
		} // end loop to copy fast led to the dma matrix
	}

} // display

void Layer::overridePixelColor(int r, int g, int b) {
	CRGB _pixel = 0 ;
	for (int y = 0; y < LAYER_HEIGHT; y++) {
		for (int x = 0; x < LAYER_WIDTH; x++)
		{
			//_pixel = pixel[XY(x, y)];
			_pixel = pixels->data[y][x];
			
			if (_pixel != transparency_colour) {
				matrix->drawPixelRGB888( x, y, _pixel.r, _pixel.g, _pixel.b);
			}

		} // end loop to copy fast led to the dma matrix
	}
}


// default value is in definition
void Layer::drawCentreText(const char *buf, textPosition textPos, const GFXfont *f, CRGB color, int yadjust) 
{
			int16_t x1, y1;
			uint16_t w, h;

			setTextWrap(false);
			
			if (f) {          // Font struct pointer passed in?
					setFont((GFXfont *)f);
			} else { // NULL passed.  Current font struct defined?
					setFont(); // use default
			}

			// getTextBounds isn't correct for variable width fonts
			getTextBounds(buf, 0, 0, &x1, &y1, &w, &h); //calc width of new string

			//Serial.printf("The width of the text is %d pixels, the height is %d pixels.\n", w,h);

			/*

			 From: https://learn.adafruit.com/adafruit-gfx-graphics-library/using-fonts
		
			 For example, whereas the cursor position when printing with the classic font identified 
			 the top-left corner of the character cell, with new fonts the cursor position indicates the baseline — 
			 the bottom-most row — of subsequent text. Characters may vary in size and width, and don’t 
			 necessarily begin at the exact cursor column (as in below, this character starts one pixel 
			 left of the cursor, but others may be on or to the right of it).
			 */
			
			if (!f) { 
				if (textPos == TOP) {
					setCursor((LAYER_WIDTH - w) / 2, 0); // top 
				} else if (textPos == BOTTOM) {
					setCursor((LAYER_WIDTH - w) / 2, LAYER_HEIGHT - h);
				} else { // middle
					setCursor((LAYER_WIDTH - w) / 2, (LAYER_HEIGHT - h) / 2); // top 
				}
			}
			else // custom font
			/* As we can't reliable know what is the actual FIRST and last 'lit' pixel, we need to check what was printed to the layer.*/
			{				
				int wstart = 0;

/*
				if (w > 42) wstart = (LAYER_WIDTH - w) / 2;
				else wstart = (LAYER_WIDTH - w) / 2;
*/
        wstart = (LAYER_WIDTH - w) / 2;

				if (textPos == TOP) {
					setCursor(wstart, h+yadjust); // top 
				} else if (textPos == BOTTOM) {
					setCursor(wstart+1, (LAYER_HEIGHT-1)+yadjust);
				} else { // middle
					setCursor( wstart, ((LAYER_HEIGHT/2) + (h/2)) + yadjust);
				}	
				
				//Serial.printf("Layer: x1: %d, y1: %d, w: %d, h: %d.\n", x1, y1, w, h);			
			}

		//	setCursor(0,16);
			setTextColor(this->color565(color.r, color.g, color.b)); // Need to confirm from FastLed CRGB to adafruit 565
			print(buf);

} // end drawCentreText


  // Move the contents of the screen left (-ve) or right (+ve)
  void Layer::moveX(int offset) 
  {
		if(offset > 0) { // move right
		//	Sprintln("Moving right");

			for(int x = LAYER_WIDTH - 1; x >= 0; x--){ // 63 to 0
				for(int y = 0; y < LAYER_HEIGHT; y++){ // 0 to 31
					if (x - offset >= 0) 
					{
						//  Serial.printf("setting y %d x %d to y %d x %d\n", y, x, y, x-offset);
						pixels->data[y][x] = pixels->data[y][x-offset];
					}
					else {
						pixels->data[y][x] = BLACK_BACKGROUND_PIXEL_COLOUR;            
					}
				}    
			}
		} else { // move left

		//	Sprintln("Moving Left");
			for(int x = 0; x <=LAYER_WIDTH - 1; x++){
				for(int y = 0; y < LAYER_HEIGHT; y++){
					if ( x > (LAYER_WIDTH-1)+offset )
					{
						pixels->data[y][x] = BLACK_BACKGROUND_PIXEL_COLOUR;                    
						//Serial.println("eh?");
					}
					else
					{
						pixels->data[y][x] = pixels->data[y][x-offset]; 
						//	Serial.println("eh?");
					}
				}    
			}         
		} 
  } 
  
/**
 * Centre the contents of the layer based on the leftmost and rightmost pixels.
 * Useful if you want to make sure text / graphics IS in the centre of the display.
 */
  void Layer::autoCenterX()
  {
	  	int leftmost_x = 0, rightmost_x = 0, adjusted_leftmost_x = 0;

		// Find leftmost
		for(int x = 0; x < LAYER_WIDTH; x++) { 
			for(int y = 0; y < LAYER_HEIGHT; y++) {
				if (pixels->data[y][x] != BLACK_BACKGROUND_PIXEL_COLOUR)
				{
					leftmost_x = x;
					//Serial.printf("Left most x pixel is %d\n", leftmost_x);
					goto rightmost;
				}
			}
		}

		rightmost:
		for(int x = LAYER_WIDTH-1; x >= 0; x--) { 
			for(int y = 0; y < LAYER_HEIGHT; y++) {
				if (pixels->data[y][x] != BLACK_BACKGROUND_PIXEL_COLOUR)
				{
					rightmost_x = x+1;
					//Serial.printf("Right most x pixel is %d\n", rightmost_x);					
					goto centreit;
				}
			}
		}

		centreit:
			adjusted_leftmost_x = ( LAYER_WIDTH - (rightmost_x - leftmost_x))/2;
			//Serial.printf("Adjusted: %d, Moving x coords by %d pixels.\n", adjusted_leftmost_x, adjusted_leftmost_x-leftmost_x);
			moveX(adjusted_leftmost_x-leftmost_x);
  } // end autoCentreX
  
  void Layer::moveY(int delta)
  {
	// Not implemented
  }


Layer::~Layer(void)
{
  free(pixels);
}
	



/* Merge FastLED layers into a super layer and display. Definition */
namespace LayerCompositor
{
	 	/*
		 * Display the foreground pixels if they're not the background/transparent color.
		 * If not, then fill with whatever is in the background.
		 * 
		 * writeToBg = write the result back to the _bgLayer, and not directly to the output device!
		 * 			   -> no need to do a subsequent bgLayer.display() otherwise.
		 */
        void Stack(MatrixPanel_I2S_DMA &disp, const Layer &_bgLayer, const Layer &_fgLayer, bool writeBackToBg)
		{
				for (int y = 0; y < LAYER_HEIGHT; y++) {
					for (int x = 0; x < LAYER_WIDTH; x++)
					{
						//https://www.educative.io/edpresso/how-to-resolve-the-expression-must-have-class-type-error-in-cpp
						if (_fgLayer.pixels->data[y][x] == _fgLayer.transparency_colour) // foreground is transparent, show the _bgLayer colors
						{
							if (writeBackToBg) // write the foreground to the background layer... perhaps so we can do stuff later with the _fgLayer.
								_bgLayer.pixels->data[y][x]	= _bgLayer.pixels->data[y][x];
							else
								disp.drawPixelRGB888(x,y, _bgLayer.pixels->data[y][x].r, _bgLayer.pixels->data[y][x].g, _bgLayer.pixels->data[y][x].b );

						} // if the foreground is NOT transparent, then print whatever is the bg
						else
						{
							if (writeBackToBg) // write the foreground to the background layer... perhaps so we can do stuff later with the _fgLayer.
								_bgLayer.pixels->data[y][x]	= _fgLayer.pixels->data[y][x];
							else
								disp.drawPixelRGB888(x,y, _fgLayer.pixels->data[y][x].r, _fgLayer.pixels->data[y][x].g, _fgLayer.pixels->data[y][x].b );
						}

					} // end x loop
				} // end y loop
		}  // end stack


	 	/*
		 * Where the foreground pixels are not the background/transparent color, populate with 
		 * whatever is in the background.
		 */
        void Siloette(MatrixPanel_I2S_DMA &disp, const Layer &_bgLayer, const Layer &_fgLayer)
		{

				for (int y = 0; y < LAYER_HEIGHT; y++) {
					for (int x = 0; x < LAYER_WIDTH; x++)
					{
						//https://www.educative.io/edpresso/how-to-resolve-the-expression-must-have-class-type-error-in-cpp
						if (_fgLayer.pixels->data[y][x] != _fgLayer.transparency_colour)
						{
							disp.drawPixelRGB888(x,y, _bgLayer.pixels->data[y][x].r, _bgLayer.pixels->data[y][x].g, _bgLayer.pixels->data[y][x].b );
						} // if the foreground is transparent, then print whatever is the bg
						else
						{
							disp.drawPixelRGB888(x,y, 0,0,0);
						}

					} // end x loop
				} // end y loop
		}  // end stack		





		void Blend(MatrixPanel_I2S_DMA &disp, const Layer &_bgLayer, const Layer &_fgLayer, uint8_t ratio)
		{
			CRGB _pixel = 0 ;

			for (int y = 0; y < LAYER_HEIGHT; y++) 
			{
				for (int x = 0; x < LAYER_WIDTH; x++)
				{
						
						_pixel = _bgLayer.pixels->data[y][x];
					
						// (set ratio to 127 for a constant 50% / 50% blend)
						//_pixel = blend(_bgLayer.pixels->data[y][x], _fgLayer.pixels->data[y][x], ratio);

						// Blend with background if foreground pixel isn't clear/transparent
						if (_fgLayer.pixels->data[y][x] != _fgLayer.transparency_colour)
						{
							_pixel = blend(_bgLayer.pixels->data[y][x], _fgLayer.pixels->data[y][x], ratio);
						} // if the foreground is transparent, then print whatever is the bg
						
						
						// https://gist.github.com/StefanPetrick/0c0d54d0f35ea9cca983
						disp.drawPixelRGB888(x,y, _pixel.r, _pixel.g, _pixel.b );

				} // end x loop
			} // end y loop



		} // end blend
}
