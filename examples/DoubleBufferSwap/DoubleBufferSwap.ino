#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>


/* 
 * Below is an is the 'legacy' way of initialising the MatrixPanel_I2S_DMA class.
 * i.e. MATRIX_WIDTH and MATRIX_HEIGHT are modified by compile-time directives.
 * By default the library assumes a single 64x32 pixel panel is connected.
 *
 * Refer to the example '2_PatternPlasma' on the new / correct way to setup this library
 * for different resolutions / panel chain lengths within the sketch 'setup()'.
 * 
 */

MatrixPanel_I2S_DMA *display = nullptr;

const byte row0 = 2+0*10;
const byte row1 = 2+1*10;
const byte row2 = 2+2*10;


void setup() 
{
 
  // put your setup code here, to run once:
  delay(1000); Serial.begin(115200); delay(200);

  Serial.println("...Starting Display");
  
  HUB75_I2S_CFG mxconfig;
  mxconfig.double_buff = true; // Turn of double buffer

  // OK, now we can create our matrix object
  display = new MatrixPanel_I2S_DMA(mxconfig);  
  
  
  display->begin();  // setup display with pins as per defined in the library
  display->setTextColor(display->color565(255, 255, 255));  

  
  // Buffer 0 test
  display->fillScreen(display->color565(128, 0, 0));  
  display->setCursor(3, row0);
  display->print(F("Buffer 0"));
  display->setCursor(3, row1);
  display->print(F(" Buffer 0"));
  Serial.println("Wrote to to Buffer 0");
  display->showDMABuffer(); 
  delay(1500);
  
  // Buffer 1 test
  display->flipDMABuffer();
  display->fillScreen(display->color565(0, 128, 0)); // shouldn't see this
  display->setCursor(3, row0);
  display->print(F("Buffer 1"));
  display->setCursor(3, row2);
  display->print(F(" Buffer 1"));
 
  Serial.println("Wrote to to Buffer 1");
  display->showDMABuffer();  
  delay(1500);

}

void loop() {
  
  // Flip the back buffer
  display->flipDMABuffer();

  // Write: Set bottow row to black
  for (int y=20;y<MATRIX_HEIGHT; y++)
  for (int x=0;x<MATRIX_WIDTH; x++)
  {
    display->drawPixelRGB888( x, y, 0, 0, 0);
  }

  // Write:  Set bottom row to blue (this is what should show)
  for (int y=20;y<MATRIX_HEIGHT; y++)
  for (int x=0;x<MATRIX_WIDTH; x++)
  {
    display->drawPixelRGB888( x, y, 0, 0, 64);
  }

  // Now show this back buffer
  display->showDMABuffer();  
  delay(1000);

  // Flip back buffer
  display->flipDMABuffer();   

  // Show this buffer
  display->showDMABuffer();   
  delay(1000); 

}