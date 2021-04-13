// Example sketch which shows how to display a 64x32 animated GIF image stored in FLASH memory
// on a 64x32 LED matrix
//
// Credits: https://github.com/bitbank2/AnimatedGIF/tree/master/examples/ESP32_LEDMatrix_I2S
// 

/* INSTRUCTIONS
 *
 * 1. First Run the 'ESP32 Sketch Data Upload Tool' in Arduino from the 'Tools' Menu.
 *    - If you don't know what this is or see it as an option, then read this:
 *      https://github.com/me-no-dev/arduino-esp32fs-plugin 
 *    - This tool will upload the contents of the data/ directory in the sketch folder onto
 *      the ESP32 itself.
 *
 * 2. You can drop any animated GIF you want in there, but keep it to the resolution of the 
 *    MATRIX you're displaying to. To resize a gif, use this online website: https://ezgif.com/
 *
 * 3. Have fun.
 */

#define FILESYSTEM SPIFFS
#include <SPIFFS.h>
#include <AnimatedGIF.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

// ----------------------------

/* 
 * Below is an is the 'legacy' way of initialising the MatrixPanel_I2S_DMA class.
 * i.e. MATRIX_WIDTH and MATRIX_HEIGHT are modified by compile-time directives.
 * By default the library assumes a single 64x32 pixel panel is connected.
 *
 * Refer to the example '2_PatternPlasma' on the new / correct way to setup this library
 * for different resolutions / panel chain lengths within the sketch 'setup()'.
 * 
 */
MatrixPanel_I2S_DMA dma_display;


AnimatedGIF gif;
File f;
int x_offset, y_offset;



// Draw a line of image directly on the LED Matrix
void GIFDraw(GIFDRAW *pDraw)
{
    uint8_t *s;
    uint16_t *d, *usPalette, usTemp[320];
    int x, y, iWidth;

  iWidth = pDraw->iWidth;
  if (iWidth > MATRIX_WIDTH)
      iWidth = MATRIX_WIDTH;

    usPalette = pDraw->pPalette;
    y = pDraw->iY + pDraw->y; // current line
    
    s = pDraw->pPixels;
    if (pDraw->ucDisposalMethod == 2) // restore to background color
    {
      for (x=0; x<iWidth; x++)
      {
        if (s[x] == pDraw->ucTransparent)
           s[x] = pDraw->ucBackground;
      }
      pDraw->ucHasTransparency = 0;
    }
    // Apply the new pixels to the main image
    if (pDraw->ucHasTransparency) // if transparency used
    {
      uint8_t *pEnd, c, ucTransparent = pDraw->ucTransparent;
      int x, iCount;
      pEnd = s + pDraw->iWidth;
      x = 0;
      iCount = 0; // count non-transparent pixels
      while(x < pDraw->iWidth)
      {
        c = ucTransparent-1;
        d = usTemp;
        while (c != ucTransparent && s < pEnd)
        {
          c = *s++;
          if (c == ucTransparent) // done, stop
          {
            s--; // back up to treat it like transparent
          }
          else // opaque
          {
             *d++ = usPalette[c];
             iCount++;
          }
        } // while looking for opaque pixels
        if (iCount) // any opaque pixels?
        {
          for(int xOffset = 0; xOffset < iCount; xOffset++ ){
            dma_display.drawPixel(x + xOffset, y, usTemp[xOffset]); // 565 Color Format
          }
          x += iCount;
          iCount = 0;
        }
        // no, look for a run of transparent pixels
        c = ucTransparent;
        while (c == ucTransparent && s < pEnd)
        {
          c = *s++;
          if (c == ucTransparent)
             iCount++;
          else
             s--; 
        }
        if (iCount)
        {
          x += iCount; // skip these
          iCount = 0;
        }
      }
    }
    else // does not have transparency
    {
      s = pDraw->pPixels;
      // Translate the 8-bit pixels through the RGB565 palette (already byte reversed)
      for (x=0; x<pDraw->iWidth; x++)
      {
        dma_display.drawPixel(x, y, usPalette[*s++]); // color 565
      }
    }
} /* GIFDraw() */


void * GIFOpenFile(const char *fname, int32_t *pSize)
{
  f = FILESYSTEM.open(fname);
  if (f)
  {
    *pSize = f.size();
    return (void *)&f;
  }
  return NULL;
} /* GIFOpenFile() */

void GIFCloseFile(void *pHandle)
{
  File *f = static_cast<File *>(pHandle);
  if (f != NULL)
     f->close();
} /* GIFCloseFile() */

int32_t GIFReadFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen)
{
    int32_t iBytesRead;
    iBytesRead = iLen;
    File *f = static_cast<File *>(pFile->fHandle);
    // Note: If you read a file all the way to the last byte, seek() stops working
    if ((pFile->iSize - pFile->iPos) < iLen)
       iBytesRead = pFile->iSize - pFile->iPos - 1; // <-- ugly work-around
    if (iBytesRead <= 0)
       return 0;
    iBytesRead = (int32_t)f->read(pBuf, iBytesRead);
    pFile->iPos = f->position();
    return iBytesRead;
} /* GIFReadFile() */

int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition)
{ 
  int i = micros();
  File *f = static_cast<File *>(pFile->fHandle);
  f->seek(iPosition);
  pFile->iPos = (int32_t)f->position();
  i = micros() - i;
//  Serial.printf("Seek time = %d us\n", i);
  return pFile->iPos;
} /* GIFSeekFile() */

unsigned long start_tick = 0;

void ShowGIF(char *name)
{
  start_tick = millis();
   
  if (gif.open(name, GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw))
  {
    x_offset = (MATRIX_WIDTH - gif.getCanvasWidth())/2;
    if (x_offset < 0) x_offset = 0;
    y_offset = (MATRIX_HEIGHT - gif.getCanvasHeight())/2;
    if (y_offset < 0) y_offset = 0;
    Serial.printf("Successfully opened GIF; Canvas size = %d x %d\n", gif.getCanvasWidth(), gif.getCanvasHeight());
    Serial.flush();
    while (gif.playFrame(true, NULL))
    {      
      if ( (millis() - start_tick) > 8000) { // we'll get bored after about 8 seconds of the same looping gif
        break;
      }
    }
    gif.close();
  }

} /* ShowGIF() */



/************************* Arduino Sketch Setup and Loop() *******************************/
void setup() {

  Serial.begin(115200);
  Serial.println("Starting AnimatedGIFs Sketch");

  // Start filesystem
  Serial.println(" * Loading SPIFFS");
  if(!SPIFFS.begin()){
        Serial.println("SPIFFS Mount Failed");
  }
  
  dma_display.begin();
  
  /* all other pixel drawing functions can only be called after .begin() */
  dma_display.fillScreen(dma_display.color565(0, 0, 0));
  gif.begin(LITTLE_ENDIAN_PIXELS);

}

void loop() {
char *szDir = "/gifs"; // play all GIFs in this directory on the SD card
char fname[256];
File root, temp;

   while (1) // run forever
   {
      root = FILESYSTEM.open(szDir);
      if (root)
      {
         temp = root.openNextFile();
            while (temp)
            {
              if (!temp.isDirectory()) // play it
              {
                strcpy(fname, temp.name());

                Serial.printf("Playing %s\n", temp.name());
                Serial.flush();
                ShowGIF((char *)temp.name());
             
              }
              temp.close();
              temp = root.openNextFile();
            }
         root.close();
      } // root
      delay(4000); // pause before restarting
   } // while
}
