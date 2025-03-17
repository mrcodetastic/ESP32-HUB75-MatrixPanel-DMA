// Modified from: https://github.com/wilson3682/My-HUB75-ESP32-Practice-Files/tree/main 
// by MrCodetastic

/***************************************************************************************
 * This sketch does a couple of things, it uses GFX_Lite's GFX_Layer to independently  
 * draw the background and the text onto separate layers. (memory used for each)
 * Then these are stacked on top of each other with some opacity, and drawn directly
 * to the dma_display via a callback (layer_draw_callback).
 *
 * These layers are offscreen pixel buffers that use  memory of their own
 * (approx 3 bytes for each pixel).
 * 
 * By using this approach, we don't really need to use DMA double buffering though.
 *
 ***************************************************************************************/

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

// You need to have: https://github.com/mrcodetastic/GFX_Lite
#include <GFX_Layer.hpp>

//------------------------------------------------------------------------------------------------------------------

// Configure for your panel(s) as appropriate!
#define PANEL_WIDTH   64
#define PANEL_HEIGHT  32  	// Panel height of 64 will required PIN_E to be defined.
#define CHAIN_LENGTH  1 	// Number of chained panels, if just a single panel, obviously set to 1


//------------------------------------------------------------------------------------------------------------------

/*
#define RL1 18
#define GL1 17
#define BL1 16
#define RL2 15
#define GL2 7
#define BL2 6
#define CH_A 4
#define CH_B 10
#define CH_C 14
#define CH_D 21
#define CH_E 5 // assign to any available pin if using two panels or 64x64 panels with 1/32 scan
#define CLK 47
#define LAT 48
#define OE  38
*/


//------------------------------------------------------------------------------------------------------------------

MatrixPanel_I2S_DMA *dma_display = nullptr;

//------------------------------------------------------------------------------------------------------------------

//====================== Variables For scrolling Text=====================================================
unsigned long isAnimationDue;
int delayBetweeenAnimations = 18;             // Smaller == faster
int textXPosition = PANEL_WIDTH * CHAIN_LENGTH;  // Will start off screen
int textYPosition = PANEL_HEIGHT / 2 - 7;        // center of screen - 8 (half of the text height)
//====================== Variables For scrolling Text=====================================================


// Pointers to this variable will be passed into getTextBounds,
// they will be updated from inside the method
int16_t xOne, yOne;
uint16_t w, h;


//------------------------------------------------------------------------------------------------------------------
// The layers don't draw to hardware directly, they use a callback function.
// You could be smart here and additionally draw to the VirtualMatrixPanel_T class...
void layer_draw_callback(int16_t x, int16_t y, uint8_t r_data, uint8_t g_data, uint8_t b_data) {

       dma_display->drawPixelRGB888(x,y,r_data,g_data,b_data);

}

// Global GFX_Layer object
GFX_Layer gfx_layer_bg(PANEL_WIDTH*CHAIN_LENGTH, PANEL_HEIGHT, layer_draw_callback); // background
GFX_Layer gfx_layer_fg(PANEL_WIDTH*CHAIN_LENGTH, PANEL_HEIGHT, layer_draw_callback); // foreground

GFX_LayerCompositor gfx_compositor(layer_draw_callback);

//------------------------------------------------------------------------------------------------------------------

void setup() {

  Serial.begin(115200);

  //Custom pin mapping for all pins
  HUB75_I2S_CFG::i2s_pins _pins={RL1, GL1, BL1, RL2, GL2, BL2, CH_A, CH_B, CH_C, CH_D, CH_E, LAT, OE, CLK};
  HUB75_I2S_CFG mxconfig(
                          PANEL_WIDTH,    // width
                          PANEL_HEIGHT,   // height
                          CHAIN_LENGTH   // chain length
                        // ,_pins           // pin mapping
        // ,HUB75_I2S_CFG::FM6126A         // driver chip
  );

  mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_20M;

  // If you are using a 64x64 matrix you need to pass a value for the E pin
  // The trinity connects GPIO 18 to E.
  // This can be commented out for any smaller displays (but should work fine with it)
  //mxconfig.gpio.e = -1;

  // May or may not be needed depending on your matrix
  // Example of what needing it looks like:
  // https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA/issues/134#issuecomment-866367216
  //mxconfig.clkphase = false;

  // Some matrix panels use different ICs for driving them and some of them have strange quirks.
  // If the display is not working right, try this.
  //mxconfig.driver = HUB75_I2S_CFG::FM6126A;

  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->setBrightness8(120);  //0-255
  dma_display->clearScreen();

  // Clear the layers
  gfx_layer_fg.clear();
  gfx_layer_fg.setTextWrap(false);

  gfx_layer_bg.clear();

}

void printTextRainbowCentered(int colorWheelOffset, const char *text, int yPos) {
  gfx_layer_fg.setTextSize(1);  // size 1 == 8 pixels high

  // Calculate the width of the text in pixels
  int textWidth = strlen(text) * 6;  // Assuming 6 pixels per character for size 1

  // Center the text horizontally
  int xPos = (gfx_layer_fg.width() - textWidth) / 2;

  gfx_layer_fg.setCursor(xPos, yPos);  // Set cursor position for centered text

  // Draw text with a rotating color
  for (uint8_t w = 0; w < strlen(text); w++) {
    gfx_layer_fg.setTextColor(colorWheel((w * 32) + colorWheelOffset));
    gfx_layer_fg.print(text[w]);
  }
}


// Code taken from: https://github.com/witnessmenow/ESP32-Trinity/blob/master/examples/BuildingBlocks/Text/ScrollingText/ScrollingText.ino
//
void scrollText(int colorWheelOffset, const char *text) {

  const char *str = text;
  byte offSet = 25;
  unsigned long now = millis();
  if (now > isAnimationDue) 
  {

    gfx_layer_fg.setTextSize(2);  // size 2 == 16 pixels high

    isAnimationDue = now + delayBetweeenAnimations;
    textXPosition -= 1;

    // Checking is the very right of the text off screen to the left
    gfx_layer_fg.getTextBounds(str, textXPosition, textYPosition, &xOne, &yOne, &w, &h);
    if (textXPosition + w <= 0) {
      textXPosition = gfx_layer_fg.width() + offSet;
    }

    gfx_layer_fg.setCursor(textXPosition, textYPosition);

    // Clear the area of text to be drawn to
    gfx_layer_fg.drawRect(1, textYPosition, gfx_layer_fg.width() - 1, 14, gfx_layer_fg.color565(0, 0, 0));
    gfx_layer_fg.fillRect(1, textYPosition, gfx_layer_fg.width() - 1, 14, gfx_layer_fg.color565(0, 0, 0));

    uint8_t w = 0;
    for (w = 0; w < strlen(str); w++) {
      //gfx_layer_fg.setTextColor(colorWheel((w * 32) + colorWheelOffset));
      gfx_layer_fg.setTextColor(gfx_layer_fg.color565(255, 255, 255));
      gfx_layer_fg.print(str[w]);
    }

  }
}


void drawTextCentered(int colorWheelOffset, const char *text, int yPos) {

  // draw text with a rotating colour
  gfx_layer_fg.setTextSize(1);       // size 1 == 8 pixels high
                                     // Calculate the width of the text in pixels
  int textWidth = strlen(text) * 6;  // Assuming 6 pixels per character for size 1

  // Center the text horizontally
  int xPos = (gfx_layer_fg.width() - textWidth) / 2;


  gfx_layer_fg.setCursor(xPos, yPos);  // start at top left, with 8 pixel of spacing
  uint8_t w = 0;
  //const char *str = "ESP32 DMA";
  const char *str = text;
  for (w = 0; w < strlen(str); w++) {
    gfx_layer_fg.setTextColor(colorWheel((w * 32) + colorWheelOffset));
    gfx_layer_fg.print(str[w]);
  }
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
// From: https://gist.github.com/davidegironi/3144efdc6d67e5df55438cc3cba613c8
uint16_t colorWheel(uint8_t pos) {
  if (pos < 85) {
    return dma_display->color565(pos * 3, 255 - pos * 3, 0);
  } else if (pos < 170) {
    pos -= 85;
    return dma_display->color565(255 - pos * 3, 0, pos * 3);
  } else {
    pos -= 170;
    return dma_display->color565(0, pos * 3, 255 - pos * 3);
  }
}

unsigned long last_increment = 0;
unsigned long increment_amt  = 0;
void updateBackground()
{

    CRGBPalette16 currentPalette = CloudColors_p;

    if ( (millis() - last_increment) > 20) {
        increment_amt++;
        last_increment = millis();
    } 

    for (int x = 0; x < gfx_layer_bg.width(); x++) {
        for (int y = 0; y < gfx_layer_bg.height(); y++) {

            int val = sin8(x + increment_amt);
            val += sin8(y + increment_amt);

            CRGB currentColor = ColorFromPalette(currentPalette, val); //, brightness, currentBlendType);

            // Set a pixel in the back layer
            gfx_layer_bg.setPixel(x, y, currentColor.r, currentColor.g, currentColor.b);

        }
    } 

     gfx_layer_bg.dim(192); // darken it a little
}

uint8_t wheelval = 0;
void loop() {

  updateBackground();

  scrollText(wheelval, "HAVE A WONDERFUL DAY!");    //Prints Scrolling text with a rainbow color
  printTextRainbowCentered(wheelval, "ENJOY IT", 24);  //Prints text X-Centered to chosen Y position with rainbow color

  //gfx_layer_fg.display();
  //gfx_layer_bg.display();

  gfx_compositor.Blend(gfx_layer_bg, gfx_layer_fg); // blend and immediately display

  wheelval += 1;

}
