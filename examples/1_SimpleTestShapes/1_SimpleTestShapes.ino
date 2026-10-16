
// Example sketch which shows how to display some patterns
// on a 64x32 LED matrix
//

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

#define PANEL_RES_X 64      // Number of pixels wide of each INDIVIDUAL panel module. 
#define PANEL_RES_Y 32     // Number of pixels tall of each INDIVIDUAL panel module.
#define PANEL_CHAIN 1      // Total number of panels chained one to another
 
//MatrixPanel_I2S_DMA dma_display;
MatrixPanel_I2S_DMA *dma_display = nullptr;

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
// From: https://gist.github.com/davidegironi/3144efdc6d67e5df55438cc3cba613c8
uint16_t colorWheel(uint8_t pos) {
  if(pos < 85) {
    return dma_display->color565(pos * 3, 255 - pos * 3, 0);
  } else if(pos < 170) {
    pos -= 85;
    return dma_display->color565(255 - pos * 3, 0, pos * 3);
  } else {
    pos -= 170;
    return dma_display->color565(0, pos * 3, 255 - pos * 3);
  }
}

void drawText(int colorWheelOffset)
{
  // Draw text with a rotating colour.
  dma_display->setCursor(5, 0);    // start at top left, with 8 pixel of spacing
  static const char str[] = "ESP32 DMA";
  for (size_t w = 0; w < sizeof(str) - 1; ++w) {
    dma_display->setTextColor(colorWheel((w * 32U) + colorWheelOffset));
    dma_display->print(str[w]);
  }

  dma_display->println();
  dma_display->print(" ");
  for (size_t w = 9; w < 18; ++w) {
    dma_display->setTextColor(colorWheel((w * 32U) + colorWheelOffset));
    dma_display->print("*");
  }
  
  dma_display->println();

  dma_display->setTextColor(dma_display->color565(255, 255, 255));
  dma_display->println("LED MATRIX!");

  // print each letter with a fixed rainbow color
  dma_display->setTextColor(dma_display->color565(0, 136, 255));
  dma_display->print('3');
  dma_display->setTextColor(dma_display->color565(255, 68, 0));
  dma_display->print('2');
  dma_display->setTextColor(dma_display->color565(255, 255, 0));
  dma_display->print('x');
  dma_display->setTextColor(dma_display->color565(136, 255, 0));
  dma_display->print('6');
  dma_display->setTextColor(dma_display->color565(136, 0, 255));
  dma_display->print('4');

  // Jump a half character
  dma_display->setCursor(34, 24);
  dma_display->setTextColor(dma_display->color565(0, 255, 255));
  dma_display->print("*");
  dma_display->setTextColor(dma_display->color565(255, 0, 0));
  dma_display->print('R');
  dma_display->setTextColor(dma_display->color565(0, 255, 0));
  dma_display->print('G');
  dma_display->setTextColor(dma_display->color565(0, 0, 255));
  dma_display->print("B");
  dma_display->setTextColor(dma_display->color565(255, 0, 136));
  dma_display->println("*");

}


void setup() {

  // Module configuration
  HUB75_I2S_CFG mxconfig(
    PANEL_RES_X,   // module width
    PANEL_RES_Y,   // module height
    PANEL_CHAIN    // Chain length
  );

  //mxconfig.gpio.e = 18;
  //mxconfig.clkphase = false;
  //mxconfig.driver = HUB75_I2S_CFG::FM6126A;

  // Display Setup
  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  if (dma_display == nullptr || !dma_display->begin()) {
    // Do not continue into loop() with an unavailable display.
    delete dma_display;
    dma_display = nullptr;
    while (true) {
      delay(1000);
    }
  }

  dma_display->setBrightness8(90); //0-255
  dma_display->clearScreen();

  // The text layout is fixed; configure it once rather than every frame.
  dma_display->setTextSize(1);     // size 1 == 8 pixels high
  dma_display->setTextWrap(false); // positions are laid out explicitly

  dma_display->fillScreen(dma_display->color565(255, 255, 255));
  
  // fix the screen with green
  dma_display->fillRect(0, 0, dma_display->width(), dma_display->height(), dma_display->color565(0, 255, 0));
  delay(500);

  // draw a box in yellow
  dma_display->drawRect(0, 0, dma_display->width(), dma_display->height(), dma_display->color565(255, 255, 0));
  delay(500);

  // draw an 'X' in red
  dma_display->drawLine(0, 0, dma_display->width()-1, dma_display->height()-1, dma_display->color565(255, 0, 0));
  dma_display->drawLine(dma_display->width()-1, 0, 0, dma_display->height()-1, dma_display->color565(255, 0, 0));
  delay(500);

  // draw a blue circle
  dma_display->drawCircle(10, 10, 10, dma_display->color565(0, 0, 255));
  delay(500);

  // fill a violet circle
  dma_display->fillCircle(40, 21, 10, dma_display->color565(255, 0, 255));
  delay(500);

  // fill the screen with 'black'
  dma_display->fillScreen(dma_display->color565(0, 0, 0));

}

uint8_t wheelval = 0;
void loop() {

    // animate by going through the colour wheel for the first two lines
    drawText(wheelval);
    wheelval +=1;

    delay(20);
}
