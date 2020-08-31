/* -------------------------- Class Initialisation -------------------------- */
#include <ESP32-RGB64x32MatrixPanel-I2S-DMA.h>
RGB64x32MatrixPanel_I2S_DMA matrix;

#include "TPM2.h" // https://github.com/rstephan/TPM2

#define NO_OF_LEDS MATRIX_WIDTH*MATRIX_HEIGHT
bool led_off = true;
uint8_t buffer[NO_OF_LEDS * 3];
// uint8_t buffer[NO_OF_LEDS * 3]; tpm2 lite
TPM2 tpm2Driver(&Serial, buffer, sizeof(buffer));


void CallbackRx(uint8_t* data, uint16_t len)
{
  uint16_t pixel;
  
  for (int y = 0; y < MATRIX_HEIGHT; y++)
  {
      for (int x = 0; x < MATRIX_WIDTH; x++)
      {
          pixel = (y * MATRIX_WIDTH) + x;
          pixel *= 3;
          
          matrix.drawPixelRGB888(x, y, data[pixel], data[pixel+1], data[pixel+2]);

          //matrix.drawPixelRGB565(x, y, (((uint16_t)data[pixel+1] << 8) | data[pixel])); // tpm2 light

      } // end x
  } // end row
  
}



void setup()
{
  // Setup serial interface
  Serial.begin(1250000);
  delay(250);
  
  tpm2Driver.registerRxData(CallbackRx);

  matrix.begin();  // setup the LED matrix
}

void loop()
{
    tpm2Driver.update();
}