/*********************************************************************
 * AnimatedGif LED Matrix Panel example where the GIFs are 
 * stored on a SD card connected to the ESP32 using the
 * standard GPIO pins used for SD card acces via. SPI.
 *
 * Put the gifs into a directory called 'gifs' (case sensitive) on
 * a FAT32 formatted SDcard.
 ********************************************************************/
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h> 
#include <AnimatedGIF.h>

/********************************************************************
 * Pin mapping below is for LOLIN D32 (ESP 32)
 *
 * Default pin mapping used by this library is NOT compatable with the use of the 
 * ESP32-Arduino 'SD' card library (there is overlap). As such, some of the pins 
 * used for the HUB75 panel need to be shifted.
 * 
 * 'SD' card library requires GPIO 23, 18 and 19 
 *  https://github.com/espressif/arduino-esp32/tree/master/libraries/SD
 * 
 */

/*
 * Connect the SD card to the following pins:
 *
 * SD Card | ESP32
 *    D2       -
 *    D3       SS
 *    CMD      MOSI
 *    VSS      GND
 *    VDD      3.3V
 *    CLK      SCK
 *    VSS      GND
 *    D0       MISO
 *    D1       -
 */

/**** SD Card GPIO mappings ****/
#define SS_PIN          5
//#define MOSI_PIN        23
//#define MISO_PIN        19
//#define CLK_PIN         18


/**** HUB75 GPIO mapping ****/
// GPIO 34+ are on the ESP32 are input only!!
// https://randomnerdtutorials.com/esp32-pinout-reference-gpios/

#define A_PIN   33  // remap esp32 library default from 23 to 33
#define B_PIN   32  // remap esp32 library default from 19 to 32
#define C_PIN   22   // remap esp32 library defaultfrom 5 to 22

//#define R1_PIN  25  // library default for the esp32, unchanged
//#define G1_PIN  26  // library default for the esp32, unchanged
//#define B1_PIN  27  // library default for the esp32, unchanged
//#define R2_PIN  14  // library default for the esp32, unchanged
//#define G2_PIN  12  // library default for the esp32, unchanged
//#define B2_PIN  13  // library default for the esp32, unchanged
//#define D_PIN   17  // library default for the esp32, unchanged
//#define E_PIN   -1 // IMPORTANT: Change to a valid pin if using a 64x64px panel.
            
//#define LAT_PIN 4  // library default for the esp32, unchanged
//#define OE_PIN  15  // library default for the esp32, unchanged
//#define CLK_PIN 16  // library default for the esp32, unchanged

/***************************************************************
 * HUB 75 LED DMA Matrix Panel Configuration
 **************************************************************/
#define PANEL_RES_X 64      // Number of pixels wide of each INDIVIDUAL panel module. 
#define PANEL_RES_Y 32     // Number of pixels tall of each INDIVIDUAL panel module.
#define PANEL_CHAIN 1      // Total number of panels chained one to another

/**************************************************************/

AnimatedGIF gif;
MatrixPanel_I2S_DMA *dma_display = nullptr;

static int totalFiles = 0; // GIF files count

static File FSGifFile; // temp gif file holder
static File GifRootFolder; // directory listing

std::vector<std::string> GifFiles; // GIF files path

const int maxGifDuration    = 30000; // ms, max GIF duration

#include "gif_functions.hpp"
#include "sdcard_functions.hpp"
 

/**************************************************************/
void draw_test_patterns();
int gifPlay( const char* gifPath )
{ // 0=infinite

  if( ! gif.open( gifPath, GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw ) ) {
    log_n("Could not open gif %s", gifPath );
  }

  Serial.print("Playing: "); Serial.println(gifPath);

  int frameDelay = 0; // store delay for the last frame
  int then = 0; // store overall delay

  while (gif.playFrame(true, &frameDelay)) {

    then += frameDelay;
    if( then > maxGifDuration ) { // avoid being trapped in infinite GIF's
      //log_w("Broke the GIF loop, max duration exceeded");
      break;
    }
  }

  gif.close();

  return then;
}


void setup()
{
    Serial.begin(115200);

    // **************************** Setup SD Card access via SPI ****************************
    if(!SD.begin(SS_PIN)){
        //  bool begin(uint8_t ssPin=SS, SPIClass &spi=SPI, uint32_t frequency=4000000, const char * mountpoint="/sd", uint8_t max_files=5, bool format_if_empty=false);      
        Serial.println("Card Mount Failed");
        return;
    }
    uint8_t cardType = SD.cardType();

    if(cardType == CARD_NONE){
        Serial.println("No SD card attached");
        return;
    }

    Serial.print("SD Card Type: ");
    if(cardType == CARD_MMC){
        Serial.println("MMC");
    } else if(cardType == CARD_SD){
        Serial.println("SDSC");
    } else if(cardType == CARD_SDHC){
        Serial.println("SDHC");
    } else {
        Serial.println("UNKNOWN");
    }

    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("SD Card Size: %lluMB\n", cardSize);

    //listDir(SD, "/", 1, false);

    Serial.printf("Total space: %lluMB\n", SD.totalBytes() / (1024 * 1024));
    Serial.printf("Used space: %lluMB\n", SD.usedBytes() / (1024 * 1024));



    // **************************** Setup DMA Matrix ****************************
    HUB75_I2S_CFG mxconfig(
      PANEL_RES_X,   // module width
      PANEL_RES_Y,   // module height
      PANEL_CHAIN    // Chain length
    );

    // Need to remap these HUB75 DMA pins because the SPI SDCard is using them.
    // Otherwise the SD Card will not work.
    mxconfig.gpio.a = A_PIN;
    mxconfig.gpio.b = B_PIN;
    mxconfig.gpio.c = C_PIN;
 //   mxconfig.gpio.d = D_PIN;    

    //mxconfig.clkphase = false;
    //mxconfig.driver = HUB75_I2S_CFG::FM6126A;

    // Display Setup
    dma_display = new MatrixPanel_I2S_DMA(mxconfig);

    // Allocate memory and start DMA display
    if( not dma_display->begin() )
        Serial.println("****** !KABOOM! HUB75 memory allocation failed ***********");
 
    dma_display->setBrightness8(128); //0-255
    dma_display->clearScreen();


    // **************************** Setup Sketch ****************************
    Serial.println("Starting AnimatedGIFs Sketch");

    // SD CARD STOPS WORKING WITH DMA DISPLAY ENABLED>...

    File root = SD.open("/gifs");
    if(!root){
        Serial.println("Failed to open directory");
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(!file.isDirectory())
        {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("  SIZE: ");
            Serial.println(file.size());

            std::string filename = "/gifs/" + std::string(file.name());
            Serial.println(filename.c_str());
            
            GifFiles.push_back( filename );
         //   Serial.println("Adding to gif list:" + String(filename));
            totalFiles++;
    
        }
        file = root.openNextFile();
    }

    file.close();
    Serial.printf("Found %d GIFs to play.", totalFiles);
    //totalFiles = getGifInventory("/gifs");



  // This is important - Set the right endianness.
  gif.begin(LITTLE_ENDIAN_PIXELS);

}

void loop(){

      // Iterate over a vector using range based for loop
    for(auto & elem : GifFiles)
    {
        gifPlay( elem.c_str() );
        gif.reset();
        delay(500);
    }

}

void draw_test_patterns()
{
 // fix the screen with green
  dma_display->fillRect(0, 0, dma_display->width(), dma_display->height(), dma_display->color444(0, 15, 0));
  delay(500);

  // draw a box in yellow
  dma_display->drawRect(0, 0, dma_display->width(), dma_display->height(), dma_display->color444(15, 15, 0));
  delay(500);

  // draw an 'X' in red
  dma_display->drawLine(0, 0, dma_display->width()-1, dma_display->height()-1, dma_display->color444(15, 0, 0));
  dma_display->drawLine(dma_display->width()-1, 0, 0, dma_display->height()-1, dma_display->color444(15, 0, 0));
  delay(500);

  // draw a blue circle
  dma_display->drawCircle(10, 10, 10, dma_display->color444(0, 0, 15));
  delay(500);

  // fill a violet circle
  dma_display->fillCircle(40, 21, 10, dma_display->color444(15, 0, 15));
  delay(500);
  delay(1000);

}
