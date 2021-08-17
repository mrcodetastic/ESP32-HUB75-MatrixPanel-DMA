#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include "Dhole_weather_icons32px.h"

/*--------------------- DEBUG  -------------------------*/
#define Sprintln(a) (Serial.println(a))
#define SprintlnDEC(a, x) (Serial.println(a, x))

#define Sprint(a) (Serial.print(a))
#define SprintDEC(a, x) (Serial.print(a, x))


/*--------------------- RGB DISPLAY PINS -------------------------*/
#define R1_PIN 25
#define G1_PIN 26
#define B1_PIN 27
#define R2_PIN 14
#define G2_PIN 12
#define B2_PIN 13
#define A_PIN 23
#define B_PIN 19 // Changed from library default
#define C_PIN 5
#define D_PIN 17
#define E_PIN -1
#define LAT_PIN 4
#define OE_PIN 15
#define CLK_PIN 16


/*--------------------- MATRIX LILBRARY CONFIG -------------------------*/
#define PANEL_RES_X 64      // Number of pixels wide of each INDIVIDUAL panel module. 
#define PANEL_RES_Y 32     // Number of pixels tall of each INDIVIDUAL panel module.
#define PANEL_CHAIN 1      // Total number of panels chained one to another
 
MatrixPanel_I2S_DMA *dma_display = nullptr;

// Module configuration
HUB75_I2S_CFG mxconfig(
	PANEL_RES_X,   // module width
	PANEL_RES_Y,   // module height
	PANEL_CHAIN    // Chain length
);

/*
//Another way of creating config structure
//Custom pin mapping for all pins
HUB75_I2S_CFG::i2s_pins _pins={R1, G1, BL1, R2, G2, BL2, CH_A, CH_B, CH_C, CH_D, CH_E, LAT, OE, CLK};
HUB75_I2S_CFG mxconfig(
						64,   // width
						64,   // height
						 4,   // chain length
					 _pins,   // pin mapping
  HUB75_I2S_CFG::FM6126A      // driver chip
);

*/


//mxconfig.gpio.e = -1; // Assign a pin if you have a 64x64 panel
//mxconfig.clkphase = false; // Change this if you have issues with ghosting.
//mxconfig.driver = HUB75_I2S_CFG::FM6126A; // Change this according to your pane.


/*
 * Wifi Logo, generated with the following steps: 
 *
 * Python and Paint.Net needs to be installed.
 *
 * 1. SAVE BITMAP AS 1BIT COLOUR in paint.net
 * 2. Run: bmp2hex.py -i -x <BITMAP FILE>
 * 3. Copy paste output into sketch.
 * 
 */
 
const char wifi_image1bit[] PROGMEM   =  {
 0x00,0x00,0x00,0xf8,0x1f,0x00,0x00,0x00,0x00,0x00,0x80,0xff,0xff,0x01,0x00,
 0x00,0x00,0x00,0xf0,0xff,0xff,0x07,0x00,0x00,0x00,0x00,0xfc,0xff,0xff,0x1f,
 0x00,0x00,0x00,0x00,0xfe,0x07,0xe0,0x7f,0x00,0x00,0x00,0x80,0xff,0x00,0x00,
 0xff,0x01,0x00,0x00,0xc0,0x1f,0x00,0x00,0xf8,0x03,0x00,0x00,0xe0,0x0f,0x00,
 0x00,0xf0,0x07,0x00,0x00,0xf0,0x03,0xf0,0x0f,0xc0,0x0f,0x00,0x00,0xe0,0x01,
 0xff,0xff,0x80,0x07,0x00,0x00,0xc0,0xc0,0xff,0xff,0x03,0x03,0x00,0x00,0x00,
 0xe0,0xff,0xff,0x07,0x00,0x00,0x00,0x00,0xf8,0x0f,0xf0,0x1f,0x00,0x00,0x00,
 0x00,0xfc,0x01,0x80,0x3f,0x00,0x00,0x00,0x00,0x7c,0x00,0x00,0x3e,0x00,0x00,
 0x00,0x00,0x38,0x00,0x00,0x1c,0x00,0x00,0x00,0x00,0x10,0xe0,0x07,0x08,0x00,
 0x00,0x00,0x00,0x00,0xfc,0x3f,0x00,0x00,0x00,0x00,0x00,0x00,0xfe,0x7f,0x00,
 0x00,0x00,0x00,0x00,0x00,0xff,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x1f,0xf8,
 0x00,0x00,0x00,0x00,0x00,0x00,0x06,0x60,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x01,0x00,0x00,0x00,0x00,0x00,
 0x00,0xc0,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0xe0,0x07,0x00,0x00,0x00,0x00,
 0x00,0x00,0xe0,0x07,0x00,0x00,0x00,0x00,0x00,0x00,0xc0,0x03,0x00,0x00,0x00,
 0x00,0x00,0x00,0x80,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00  };


void drawXbm565(int x, int y, int width, int height, const char *xbm, uint16_t color = 0xffff) 
{
  if (width % 8 != 0) {
      width =  ((width / 8) + 1) * 8;
  }
    for (int i = 0; i < width * height / 8; i++ ) {
      unsigned char charColumn = pgm_read_byte(xbm + i);
      for (int j = 0; j < 8; j++) {
        int targetX = (i * 8 + j) % width + x;
        int targetY = (8 * i / (width)) + y;
        if (bitRead(charColumn, j)) {
          dma_display->drawPixel(targetX, targetY, color);
        }
      }
    }
}

/* Bitmaps */
int current_icon = 0;
static int num_icons = 22;

static char icon_name[22][30] = { 
"cloud_moon_bits",
"cloud_sun_bits",
"clouds_bits",
"cloud_wind_moon_bits",
"cloud_wind_sun_bits",
"cloud_wind_bits",
"cloud_bits",
"lightning_bits",
"moon_bits",
"rain0_sun_bits",
"rain0_bits",
"rain1_moon_bits",
"rain1_sun_bits",
"rain1_bits",
"rain2_bits",
"rain_lightning_bits",
"rain_snow_bits",
"snow_moon_bits",
"snow_sun_bits",
"snow_bits",
"sun_bits",
"wind_bits" };

static char *icon_bits[22] = { cloud_moon_bits,
cloud_sun_bits,
clouds_bits,
cloud_wind_moon_bits,
cloud_wind_sun_bits,
cloud_wind_bits,
cloud_bits,
lightning_bits,
moon_bits,
rain0_sun_bits,
rain0_bits,
rain1_moon_bits,
rain1_sun_bits,
rain1_bits,
rain2_bits,
rain_lightning_bits,
rain_snow_bits,
snow_moon_bits,
snow_sun_bits,
snow_bits,
sun_bits,
wind_bits};



void setup() {

  // put your setup code here, to run once:
  delay(1000); Serial.begin(115200); delay(200);


  /************** DISPLAY **************/
  Sprintln("...Starting Display");
  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->setBrightness8(90); //0-255
  dma_display->clearScreen();
  
  dma_display->fillScreen(dma_display->color444(0, 1, 0));  

  // Fade a Red Wifi Logo In
  for (int r=0; r < 255; r++ )
  {
    drawXbm565(0,0,64,32, wifi_image1bit, dma_display->color565(r,0,0));  
    delay(10);
  }

  delay(2000);
  dma_display->clearScreen();
}


void loop() {

  // Loop through Weather Icons
  Serial.print("Showing icon ");
  Serial.println(icon_name[current_icon]);
  drawXbm565(0,0, 32, 32, icon_bits[current_icon]);

  current_icon = (current_icon  +1 ) % num_icons;
  delay(2000);
  dma_display->clearScreen();
  
}